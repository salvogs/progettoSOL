#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fs.h"
#include "../include/utils.h"
#include "../include/configParser.h"




fsT* create_fileStorage(size_t maxCapacity, int maxFileNum, int evictionPolicy){
	
	fsT* storage = malloc(sizeof(fsT));
	chk_null(storage,NULL)

	storage->maxCapacity = maxCapacity;
	storage->maxFileNum = maxFileNum;


	// alloco hash table
	ec(storage->ht = icl_hash_create(storage->maxFileNum,NULL,NULL), NULL,"hash create",return NULL)
	
	storage->currCapacity = 0;
	storage->currFileNum = 0;
	
	// stats
	storage->maxCapacityStored = 0;
	storage->maxFileNumStored = 0;
	storage->ejectedFileNum = 0;
	storage->tryEjectFile = 0;
	storage->evictionPolicy = evictionPolicy;
	ec(storage->filesQueue = createQueue(NULL,cmpFile),NULL,"create queue",return NULL)

	ec_n(pthread_mutex_init(&(storage->smux),NULL),0,"pthread mutex init storage",return NULL)

	return storage;	
}


int destroy_fileStorage(fsT* storage){

	destroyQueue(storage->filesQueue,0);
	icl_hash_destroy(storage->ht,free,freeFile);
	free(storage);
	return 0;

}

/**
 * \brief gestisce richiesta apertura file
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta
 *  
 * \param pathname path del file 
 * 
 * \param flags O_CREATE/O_LOCK se si vuole creare e/o lockare file
 * 
 * \param fdPending coda client che erano in attesa sui file espulsi
 * 
 * \retval SUCCESS(0): successo
 * \retval SERVER_ERROR: errore
 * \retval FILE_EXISTS: file esiste e flag O_CREATE settato
 * \retval FILE_NOT_EXISTS: file non esiste e flag O_CREATE non settato
 * \retval LOCKED : file locato da un altro client e O_LOCK settato
 */
int open_file(fsT* storage, int fdClient, char* pathname, int flags, queue* fdPending){

	short int create = IS_O_CREATE_SET(flags), lock = IS_O_LOCK_SET(flags);

	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	int ret = SUCCESS;
	if(fPtr){ // se il file è già nel file storage
		if(create){
			UNLOCK(&(storage->smux));
			return FILE_EXISTS; // bisogna aprirlo senza O_CREATE
		}
		

		LOCK(&(fPtr->ordering))
		LOCK(&(fPtr->mux))

		UNLOCK(&(storage->smux));

		while(fPtr->activeReaders || fPtr->activeWriters)
       		WAIT(&(fPtr->go),&(fPtr->mux))
    
		fPtr->activeWriters++;

		if(storage->evictionPolicy != FIFO)
			update_access_info(fPtr,storage->evictionPolicy);

		UNLOCK(&(fPtr->ordering))
		UNLOCK(&(fPtr->mux))
		


		if(lock){
			if(fPtr->fdlock && fPtr->fdlock != fdClient)
				ret = LOCKED;
			else
				fPtr->fdlock = fdClient;
		}

		
		// metto fdClient in coda ai fd che hanno aperto il file
		if(!ret && enqueue(fPtr->fdopen,CAST_FD(fdClient)) != 0)
			ret = SERVER_ERROR;
		
	

		LOCK(&(fPtr->mux))

		fPtr->activeWriters--;
		SIGNAL(&(fPtr->go))

		UNLOCK(&(fPtr->mux))

		return ret;

	}
	// il file deve essere creato
	if(!create){
		ret = FILE_NOT_EXISTS; // bisogna aprirlo con O_CREATE
	}
	// creo file (vuoto) sullo storage
	if(!ret)
		ret = store_insert(storage,fdClient,pathname,lock,fdPending);

	
	UNLOCK(&(storage->smux));
	
	return ret;
}




/**
 * \brief gestisce richiesta chiusura file
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta
 *  
 * \param pathname path del file 
 * 
 * \retval SUCCESS(0): successo
 * \retval SERVER_ERROR: errore 
 * \retval FILE_NOT_EXISTS: il file non esiste
 * \retval NOT_OPENED: il file non è aperto da fdClient
 */
int close_file(fsT* storage, int fdClient, char* pathname){

	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		UNLOCK(&(storage->smux));
		return FILE_NOT_EXISTS;
	}
	
	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))
	UNLOCK(&(storage->smux))

	// aspetto mentre qualcuno legge/scrive

	while(fPtr->activeReaders || fPtr->activeWriters)
        WAIT(&(fPtr->go),&(fPtr->mux));
    
	fPtr->activeWriters++;

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))
	
	if(storage->evictionPolicy != FIFO)
		update_access_info(fPtr,storage->evictionPolicy);

	int ret = SUCCESS;
	// se il client ha aperto il file rimuovo il suo fd
	if(removeFromQueue(fPtr->fdopen,CAST_FD(fdClient),0) != 0){
        ret = NOT_OPENED;
	}

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return ret;
}




/**
 * \brief gestisce richiesta di scrittura di un file già esistente. In ogni caso si tratta di una append
 * 
 * \param storage puntatore allo store 
 * 
 * \param mode flag che se == 1 indica che si tratta di una append
 * 
 * \param fdClient client che ha fatto richiesta 
 * 
 * \param pathname path del file
 * 
 * \param size size file
 * 
 * \param content contenuto file
 * 
 * \param ejected file espulsi per fare spazio
 * 
 * \param fdPending client in attesa dei file espulsi da avvisare
 * \retval SUCCESS(0): se successo,
 * \retval FILE_NOT_EXISTS: se il file non esiste
 * \retval FILE_TOO_LARGE: file troppo grande per essere memorizzato
 * \retval STORE_FULL: store attualmente pieno
 * \retval LOCKED : il file è lockato da un altro client
 * \retval NOT_OPENED: il file non è aperto da fdClient
 * \retval BAD_REQUEST: fdClient non può effettuare la prima scrittura
 * \retval SERVER_ERROR: errore
 */
int write_append_file(fsT* storage, int mode, int fdClient,char* pathname, size_t size, void* content, queue* ejected, queue* fdPending){

	LOCK(&(storage->smux))

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		//file non ancora creato
		UNLOCK(&(storage->smux))
		return FILE_NOT_EXISTS;
	}
	
	

	if(size > storage->maxCapacity){
		/* anche rimuovendo tutti i file attualmente presenti 
		non si riuscirebbe a memorizzare quello inviato dal client
		*/
		UNLOCK(&(storage->smux))
		return FILE_TOO_LARGE;
	}


	 
	// libero spazio per memorizzare file
	while(size + storage->currCapacity > storage->maxCapacity){
		
		fT* ef = eject_file(storage,fPtr->pathname,fdClient,1);
		if(!ef){
			UNLOCK(&(storage->smux))
			return STORE_FULL;
		}
		
		// metto in coda i fd dei client da risvegliare 
		while(ef->fdpending->ndata){
			long fd = (long)dequeue(ef->fdpending);
			enqueue(fdPending,CAST_FD(fd));
		}


		// lo inserisco in coda ai file da espellere e lo tolgo dallo store senza fare la free
		if(enqueue(ejected,ef) != 0 || store_remove(storage,ef,0) != 0){
			UNLOCK(&(storage->smux))
			return SERVER_ERROR;
		}
	
			
		
	}

	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))

	// non posso eseguire scritture in contemporanea ad altri scrittori/lettori

	while(fPtr->activeReaders || fPtr->activeWriters)
		WAIT(&(fPtr->go),&(fPtr->mux))

	fPtr->activeWriters++; // 1

	if(storage->evictionPolicy != FIFO)
		update_access_info(fPtr,storage->evictionPolicy);

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))


	int ret = SUCCESS;

	if(fPtr->fdlock && fPtr->fdlock != fdClient){
		ret = LOCKED;
	}
	


	if(!ret && !findQueue(fPtr->fdopen,CAST_FD(fdClient))){
		ret = NOT_OPENED;
	}

	if(fPtr->fdwrite && fPtr->fdwrite != fdClient){
		ret = BAD_REQUEST;
	}else{
		fPtr->fdwrite = 0;
	}

	if(!ret && mode == APPEND_TO_FILE)	
		fPtr->modified = 1;

	if(!ret && size){ // modifico il contenuto solo se e' stato effettivamente ricevuto
	
		// in ogni caso devo appendere al file
		fPtr->content = (void*) realloc(fPtr->content,fPtr->size + size);
		if(fPtr->content){
			memcpy(fPtr->content+fPtr->size,content,size);
			fPtr->size += size;
			storage->currCapacity += size;

			if(storage->currCapacity > storage->maxCapacityStored)
				storage->maxCapacityStored = storage->currCapacity;

		}else{
			ret = SERVER_ERROR;
		}

	}
	
	LOCK(&(fPtr->mux))

	fPtr->activeWriters--; // 0
	
	// segnalo ad eventuali scrittori/lettori in attesa che possono proseguire
	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))


	UNLOCK(&(storage->smux))
	
	
	return ret;

}


/**
 * \brief gestisce richiesta di lettura file
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta 
 * 
 * \param pathname path del file
 * 
 * \param size puntatore dove memorizzare size file letto
 * 
 * \param content puntatore dove memorizzare contenuto file letto
 * 
 * \retval SUCCESS(0): successo o EMPTY_FILE se file vuoto
 * \retval FILE_NOT_EXISTS: il file non esiste
 * \retval LOCKED : il file è lockato da un altro client
 * \retval NOT_OPENED: il file non è aperto da fdClient
 * \retval SERVER_ERROR: errore
 *
 */
int read_file(fsT* storage,int fdClient, char* pathname, size_t* size, void** content){


	LOCK(&(storage->smux))

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		UNLOCK(&(storage->smux))
		return FILE_NOT_EXISTS;
	}

	// paradigma lettori/scrittori 

	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))

	UNLOCK(&(storage->smux))



	/* fin quando ci sono degli scrittori sul file 
		mi metto in attesa rilasciando la lock su MUX */

	while(fPtr->activeWriters){
		WAIT(&(fPtr->go),&(fPtr->mux))
	}

	fPtr->activeReaders++;

	
	if(storage->evictionPolicy != FIFO)
		update_access_info(fPtr,storage->evictionPolicy);

	// permetto ad altri lettori/scrittori di avanzare 
	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))

	int ret = SUCCESS;

	if(fPtr->fdlock && fPtr->fdlock != fdClient){
		ret = LOCKED;
	}
	
	

	if(!ret && !findQueue(fPtr->fdopen,CAST_FD(fdClient))){
		ret = NOT_OPENED;
	}
	
	if(!ret && fPtr->size == 0){ //file vuoto
		ret = EMPTY_FILE;
	}

	if(!ret){
		// copio size e content 

		*size = fPtr->size;
		
		if((*content = (void*)malloc(*size)))
			memcpy(*content,fPtr->content,*size);
		else
			ret = SERVER_ERROR;

	}

	LOCK(&(fPtr->mux))

	fPtr->activeReaders--;
	if(fPtr->activeReaders == 0)
		SIGNAL(&(fPtr->go))
	
	UNLOCK(&(fPtr->mux))


	return ret;

}




/**
 * \brief gestisce richiesta di leggere n file casuali
 * 
 * \param storage puntatore allo store 
 * 
 * \param n numero di file richiesti
 * 
 * \param fdClient client che ha fatto richiesta 
 * 
 * \param ejected coda di file letti
 * 
 * \retval numero di file letti: successo
 * \retval SERVER_ERROR: errore
 * \note la lock sullo store permette di leggere tutti i file in mutua esclusione
 *
 */
int read_n_file(fsT* storage,int n,int fdClient, queue* ejected){

	
	LOCK(&(storage->smux))

	if(n <= 0 || n >= storage->currFileNum)
		n = storage->currFileNum;

	data* curr = storage->filesQueue->head;
	fT* fPtr = NULL, *ef = NULL;
	int nsave = n;

	while(n && curr){
		
		fPtr = (fT*)(curr->data);

		if(storage->evictionPolicy != FIFO)
			update_access_info(fPtr,storage->evictionPolicy);
		
		// faccio copia del file e lo inserisco in coda ai file da espellere (inviare)
		if(!(ef = fileCopy(fPtr)) || enqueue(ejected,ef) != 0){
			UNLOCK(&(storage->smux))
			return SERVER_ERROR;
		}
		
		curr = curr->next;
		n--;
	}

	UNLOCK(&(storage->smux))

	return nsave - n;

}


/**
 * \brief gestisce richiesta rimozione file
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta
 *  
 * \param pathname path del file 
 * 
 * \param fdPending coda dove memorizzare i client che erano in attesa 
 * per poi notificarli che il file non esiste più 
 * 
 * \retval SUCCESS(0): successo
 * \retval SERVER_ERROR: errore
 * \retval FILE_NOT_EXISTS: il file non esiste
 * \retval NOT_LOCKED: il file non è lockato da fdClient
 */
int remove_file(fsT* storage, int fdClient, char* pathname, queue* fdPending){
	
	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		UNLOCK(&(storage->smux));
		return FILE_NOT_EXISTS;
	}

	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))
	
	/* non rilascio lo store perche' potrei 
		cancellare il file a qualche lettore/scrittore in attesa */
	//UNLOCK(&(storage->smux))


	// aspetto mentre qualcuno legge/scrive

	while(fPtr->activeReaders || fPtr->activeWriters)
        WAIT(&(fPtr->go),&(fPtr->mux));
    
	

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))

	int ret = SUCCESS;
	// il client che richiede la remove deve aver prima lockato il file
	if(fPtr->fdlock != fdClient){
		ret = NOT_LOCKED;
	}
	else{
		/* devo notificare eventuali client in attesa di acquisire la lock
			che il file non esiste */
		while(fPtr->fdpending->ndata){
			long fd = (long)dequeue(fPtr->fdpending);
			enqueue(fdPending,CAST_FD(fd));
		}
		

		// rimuovo il file

		if(store_remove(storage,fPtr,1) != 0){
			ret =  SERVER_ERROR;
		}
	}
	
	UNLOCK(&(storage->smux));

	return ret;
		
}


/**
 * \brief setta il flag O_LOCK al file
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta
 *  
 * \param pathname path del file 
 * 
 * \retval SUCCESS(0): successo o file già lockato da fdClient
 * \retval SERVER_ERROR: errore
 * \retval FILE_NOT_EXISTS: il file non esiste
 * \retval LOCKED : il file è lockato da un altro client
 */
int lock_file(fsT* storage, int fdClient, char* pathname){
	
	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		UNLOCK(&(storage->smux));
		return FILE_NOT_EXISTS;
	}
	
	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))
	UNLOCK(&(storage->smux));


	// aspetto mentre qualcuno legge/scrive

	while(fPtr->activeReaders || fPtr->activeWriters)
        WAIT(&(fPtr->go),&(fPtr->mux));
    
	fPtr->activeWriters++;

	if(storage->evictionPolicy != FIFO)
		update_access_info(fPtr,storage->evictionPolicy);

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))
	
	int ret = SUCCESS;
	/* se un altro client ha lockato il file aggiungo il fd del client
	 alla coda di fd che attendono di acquisire la lock su quel file */
	if(fPtr->fdlock != 0 && fPtr->fdlock != fdClient){
		if(enqueue(fPtr->fdpending,CAST_FD(fdClient)) != 0)
			ret = SERVER_ERROR;
		else
			ret = LOCKED; // il worker non mandera' alcuna risposta al client

	}else{
		// altrimenti lock
		fPtr->fdlock = fdClient;
	}

	

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return ret;
		
}


/**
 * \brief reimposta il flag O_LOCK al file a 0 o ad un client in attesa
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha fatto richiesta
 *  
 * \param pathname path del file 
 * 
 * \param newFdLock puntatore dove memorizzare l'eventuale nuovo owner della lock
 * 
 * \retval SUCCESS(0): successo 
 * \retval SERVER_ERROR: errore
 * \retval FILE_NOT_EXISTS: il file non esiste
 * \retval NOT_LOCKED: il file non è lockato da fdClient
 */
int unlock_file(fsT* storage, int fdClient, char* pathname, int *newFdLock){
	
	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		UNLOCK(&(storage->smux));
		return FILE_NOT_EXISTS;
	}
	
	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))
	UNLOCK(&(storage->smux));


	// aspetto mentre qualcuno legge/scrive

	while(fPtr->activeReaders || fPtr->activeWriters)
        WAIT(&(fPtr->go),&(fPtr->mux));
    
	fPtr->activeWriters++;

	if(storage->evictionPolicy != FIFO)
		update_access_info(fPtr,storage->evictionPolicy);


	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))
	
	int ret = SUCCESS;
	// se fdClient ha lockato il file
	if(fPtr->fdlock == fdClient){
		// controllo se qualcuno era in attesa di acquisire la lock
		if(fPtr->fdpending->ndata){
			*newFdLock = (long)dequeue(fPtr->fdpending);
			if(!(*newFdLock))
				ret = SERVER_ERROR;

			fPtr->fdlock = *newFdLock;
		}else{
			// reset lock
			fPtr->fdlock = 0;
		}
	}else{
		ret = NOT_LOCKED;
	}



	

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return ret;
		
}



/**
 * \brief inizializza e inserisce un file nello store
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client che ha inviato il file
 *  
 * \param pathname path del file da memorizzare
 * 
 * \param lock flag O_LOCK
 * 
 * \param fdPending coda dei file eventualmente espulsi
 *  per memorizzare quello inviato
 * \retval 0: successo
 * \retval SERVER_ERROR: errore
 * \retval STORE_FULL: attualmente non è possibile memorizzare il file
 * 
 * \note assume che lo store sia lockato dal chiamante e quindi di lavorare in mutua esclusione
 */
int store_insert(fsT* storage, int fdClient, char* pathname,int lock, queue* fdPending){

	// controllo che ci sia spazio a sufficienza 		
	if((storage->currFileNum+1) > storage->maxFileNum){
		// non importa che il file sia vuoto o meno
		fT* ef = eject_file(storage,NULL,fdClient,0);
		chk_null(ef,STORE_FULL)

		/* devo notificare eventuali client in attesa di acquisire la lock
			che il file non esiste */
		while(ef->fdpending->ndata){
			long fd = (long)dequeue(ef->fdpending);
			enqueue(fdPending,CAST_FD(fd));
		}
		

		// rimuovo il file dallo storage senza inviarlo (per il momento) al client
		chk_neg1(store_remove(storage,ef,1),SERVER_ERROR)
	}
	
	fT* fPtr = NULL;
	chk_null(fPtr = malloc(sizeof(fT)),SERVER_ERROR)
		
	ec_n(pthread_mutex_init(&(fPtr->mux),NULL),0,"file mux init",return SERVER_ERROR)
	ec_n(pthread_mutex_init(&(fPtr->ordering),NULL),0,"file ordering init",return SERVER_ERROR)
	
	ec_n(pthread_cond_init(&(fPtr->go),NULL),0,"file cond go init",return SERVER_ERROR)

	fPtr->activeReaders = 0;
	fPtr->activeWriters  = 0;
	fPtr->pathname = pathname;
	fPtr->size = 0;
	fPtr->content = NULL;
	fPtr->modified = 0;
	
	fPtr->accessCount = 0;

	struct timespec ret;
	ec(clock_gettime(CLOCK_MONOTONIC,&ret),-1,"clock_gettime",exit(EXIT_FAILURE));
	fPtr->accessTime = ret.tv_nsec;


	fPtr->fdlock = fdClient;
	fPtr->fdwrite = fdClient;

	if(!(fPtr->fdopen = createQueue(NULL,NULL)) || !(fPtr->fdpending = createQueue(NULL,NULL))){
		free(fPtr);
		return SERVER_ERROR;
	}

	// il file, oltre ad essere creato, viene anche aperto

	if(enqueue(fPtr->fdopen,CAST_FD(fdClient)) != 0){
		return SERVER_ERROR;
	}

	// se il flag O_LOCK e' settato allora viene anche lockato da fdClient

	if(lock){
		fPtr->fdlock = fdClient;
	}

	if(icl_hash_insert(storage->ht,pathname,fPtr) == NULL || enqueue(storage->filesQueue,fPtr) != 0){
		free(fPtr);
		return SERVER_ERROR;

	}
	

	storage->currFileNum++;
	if(storage->currFileNum > storage->maxFileNumStored)
		storage->maxFileNumStored = storage->currFileNum;

	return 0;
}


/**
 * \brief rimuove un file dallo store 
 * 
 * \param storage puntatore allo store 
 * 
 * \param fPtr file interessato
 *  
 * \param freeData se == 1 il file sara' anche deallocato
 * 
 * \retval SUCCESS(0): successo
 * \retval SERVER_ERROR: errore
 * 
 * \note assume che lo store sia lockato dal chiamante e quindi di lavorare in mutua esclusione
 */
int store_remove(fsT* storage, fT* fPtr, int freeData){
	char* rmpath = fPtr->pathname;
	// rimuovo dalla coda di rimpiazzamento
	if(removeFromQueue(storage->filesQueue,fPtr,0) != 0)
		return -1; 

	storage->currCapacity -= fPtr->size;
	// rimuovo dalla hash table
	if(freeData){
		if(icl_hash_delete(storage->ht,rmpath,free,freeFile) == -1)
			return -1; 
	}else{
		if(icl_hash_delete(storage->ht,rmpath,NULL,NULL) == -1)
			return -1;
	}

	storage->currFileNum--;

	return 0; // file rimosso con successo
}



/**
 * \brief chiude/unlocka tutti i file aperti/lockati da fdClient 
 * 
 * \param storage puntatore allo store 
 * 
 * \param fdClient client interessato
 *  
 * \param fdPending coda sulla quale inserire i client da notificare dopo avergli dato la lock
 * 
 * \retval SUCCESS(0):successo
 * \retval SERVER_ERROR: errore
 * 
 */
int remove_client(fsT* storage, int fdClient, queue* fdPending){
	
	// lock dello storage
	LOCK(&(storage->smux));

	int nfiles = storage->filesQueue->ndata;
	data* curr = storage->filesQueue->head;
	fT* fPtr = NULL;
	int ret = SUCCESS;
	int newFdLock = 0;

	while(nfiles){
		
		fPtr = curr->data;

		LOCK(&(fPtr->ordering))
		LOCK(&(fPtr->mux))
		
		
		// aspetto mentre qualcuno legge/scrive

		while(fPtr->activeReaders || fPtr->activeWriters)
			WAIT(&(fPtr->go),&(fPtr->mux));
		
		fPtr->activeWriters++;

		UNLOCK(&(fPtr->ordering))
		UNLOCK(&(fPtr->mux))

		if(fPtr->fdwrite == fdClient){
			// resetto il client che può effettuare la write
			fPtr->fdwrite = 0;
		}



		// rimuovo fdClient dai client che hanno aperto il file
		removeFromQueue(fPtr->fdopen,CAST_FD(fdClient),1);

		
		
		// se fdClient ha lockato il file lo rilascio
		if(fPtr->fdlock == fdClient){
			// controllo se qualcuno era in attesa di acquisire la lock
			if(fPtr->fdpending->ndata){
				if(!(newFdLock = (long)dequeue(fPtr->fdpending))){
					ret = SERVER_ERROR;
					break;
				}

				fPtr->fdlock = newFdLock;
				// metto in coda ai client "da avvisare"
				if(enqueue(fdPending,CAST_FD(newFdLock)) != 0){
					ret = SERVER_ERROR;
					break;
				}

			}else{
				// reset lock
				fPtr->fdlock = 0;
			}
		}else{
			// altrimenti, se presente, lo tolgo da quelli in attesa di acquisirla
			removeFromQueue(fPtr->fdpending,CAST_FD(fdClient),1);		
		}


		LOCK(&(fPtr->mux))

		fPtr->activeWriters--;

		SIGNAL(&(fPtr->go))

		UNLOCK(&(fPtr->mux))


		curr = curr->next;
		nfiles--;
	}
	UNLOCK(&(storage->smux));

	return ret;

}



// copia fPtr e restituisce il nuovo puntatore
fT* fileCopy(fT* fPtr){
	fT* cp = NULL;

	chk_null(cp = malloc(sizeof(fT)),NULL)
	cp->content = NULL;
	cp->pathname = NULL;


	cp->size = fPtr->size;

	if(cp->size){
		chk_null_op(cp->content = (void*)malloc(cp->size),free(cp),NULL)
	}

	if(!(cp->pathname = strndup(fPtr->pathname,strlen(fPtr->pathname)))){
		if(cp->pathname)
			free(cp->pathname);

		if(cp->content)
			free(cp->content);
	
		free(cp);
		return NULL;
	}

	// non copio la coda dei fd che hanno aperto il file e di quelli in attesa
	cp->fdopen = NULL;
	cp->fdpending = NULL;

	if(cp->size)
		memcpy(cp->content,fPtr->content,cp->size);
	
	
	
	cp->modified = fPtr->modified;

	return cp;
}




/**
 * Algoritmo di rimpiazzamento che scorre la coda di tutti i file 
 * attualmente presenti nel server e ritorna (se possibile) il primo 
 * che rispetta le condizioni
 * 
 * \param storage puntatore allo store 
 * 
 * \param pathname path del file che ha causato l'espulsione 
 * 
 * \param fdClient client che ha causato l'espulsione 
 * 
 * \param chkEmpty se == 1 l'algoritmo salta i file vuoti
 * 
 * \note viene saltato, se esiste, il file 'pathname' 
 * vengono saltati i file lockati da altri client
 * 
*/
fT* eject_file(fsT* storage, char* pathname, int fdClient, int chkEmpty){
	
	storage->tryEjectFile++;
	logPrint("==ALGORIMO RIMPIAZZAMENTO==",NULL,0,-1,NULL);


	queue* fq = storage->filesQueue;

	if(isQueueEmpty(fq))
		return NULL;

	data* curr = fq->head;
	fT* tmp = NULL, *toEject = NULL; 
	
	// scorro la lista dei file
	while(curr){
		tmp = curr->data;
		// skippo i file vuoti/lockati e il file che voglio scrivere
		LOCK(&(tmp->ordering))
		LOCK(&(tmp->mux))

		while(tmp->activeReaders || tmp->activeWriters)
			WAIT(&(tmp->go),&(tmp->mux))
		// mi serve solo fare la lock, 
		// avendo lo store lockato sono sicuro che nessuno sia in attesa

		UNLOCK(&(tmp->ordering))
		UNLOCK(&(tmp->mux))
		
		// chkEmpty == 1 ritorno il file solo se non e' vuoto

		if((tmp->fdlock == fdClient || !(tmp->fdlock)) && (pathname ? (strcmp(tmp->pathname,pathname) != 0) : 1) && (chkEmpty ? tmp->size : 1)){
			
			if(storage->evictionPolicy == LRU){
				if(!toEject)
					toEject = tmp;
				else if(tmp->accessTime < toEject->accessTime)
					toEject = tmp;
				
			}else if(storage->evictionPolicy == LFU){
				if(!toEject)
					toEject = tmp;
				if(tmp->accessCount < toEject->accessCount)
					toEject = tmp;
				
			}else{// FIFO
				storage->ejectedFileNum++;
				logPrint("ESPULSO",tmp->pathname,0,tmp->size,NULL);
				return tmp;
			}
			
		}
		curr = curr->next;
	}
	
	if(storage->evictionPolicy == FIFO)
		return NULL; // non ci sono attualmente file da espellere

	if(toEject){
		storage->ejectedFileNum++;
		logPrint("ESPULSO",toEject->pathname,0,toEject->size,NULL);
		return toEject;
	}
	return NULL;

		
}

void update_access_info(fT* fPtr, int ep){
	if(ep == LRU){
		struct timespec ret;
		ec(clock_gettime(CLOCK_MONOTONIC,&ret),-1,"clock_gettime",exit(EXIT_FAILURE));
		fPtr->accessTime = ret.tv_nsec;
	}
	else // LFU
		fPtr->accessCount++;

	return;
}

void print_final_info(fsT* storage,int maxClientNum){

	fprintf(stdout,"\033[0;32m---STATISTICHE FINALI---\033[0m\n");
	
	fprintf(stdout,"\033[0;34mMax file memorizzati:\033[0m %d\n",storage->maxFileNumStored);

	fprintf(stdout,"\033[0;34mMax Mbyte memorizzati:\033[0m %f\n",(float)(storage->maxCapacityStored)/1000000);
	fprintf(stdout,"\033[0;34mEsecuzioni dell'algoritmo di rimpiazzamento:\033[0m %d\n",storage->tryEjectFile);
	fprintf(stdout,"\033[0;34mFile espulsi:\033[0m %d\n",storage->ejectedFileNum);

	fprintf(stdout,"\033[0;34mFile presenti alla chiusura:\033[0m %d\n",storage->currFileNum);

	char buf[LOGEVENTSIZE] = "";
	snprintf(buf,LOGEVENTSIZE,"==STATS==MAXNFILE %d MAXMBYTE %f MAXCLIENT %d",storage->maxFileNumStored,(float)(storage->maxCapacityStored)/1000000,maxClientNum);
	logPrint(buf,NULL,0,-1,NULL);

	// stampo lista file (path) presenti alla chiusura
	data* curr = storage->filesQueue->head;

	while(curr){
		fprintf(stdout,"\033[0;31m%s\033[0m\n",((fT*)(curr->data))->pathname);
		curr = curr->next;
	}

	return;

}



void freeFile(void* fptr){
	fT* fPtr = (fT*) fptr;
	if(fPtr){
		free(fPtr->content);
		//if(fPtr->pathname) free(fPtr->pathname);
		destroyQueue(fPtr->fdopen,0);
		destroyQueue(fPtr->fdpending,0);
		free(fPtr);
	}
	return;
}


int cmpFile(void* f1, void* f2){
	return (strncmp(((fT*)f1)->pathname, ((fT *)f2)->pathname, UNIX_PATH_MAX) == 0) ? 1 : 0;
}
