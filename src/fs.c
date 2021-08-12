#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fs.h"
#include "../include/utils.h"
#include "../include/configParser.h"



#define IS_O_CREATE_SET(flags) \
	(flags == 1 || flags == 3) ? 1 : 0
	

#define IS_O_LOCK_SET(flags) \
	(flags == 2 || flags == 3) ? 1 : 0





fsT* create_fileStorage(size_t maxCapacity, int maxFileNum){
	
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
		// if(lock){
		// 	// LOCK
			
		// }	
		
		

		LOCK(&(fPtr->ordering))
		LOCK(&(fPtr->mux))

		UNLOCK(&(storage->smux));

		while(fPtr->activeReaders || fPtr->activeWriters)
       		WAIT(&(fPtr->go),&(fPtr->mux))
    
		fPtr->activeWriters++;

		UNLOCK(&(fPtr->ordering))
		UNLOCK(&(fPtr->mux))
		
		
		// metto fdClient in coda ai fd che hanno aperto il file
		if(enqueue(fPtr->fdopen,CAST_FD(fdClient)) != 0){
			ret = SERVER_ERROR;
		}

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


int write_append_file(fsT* storage,int fdClient,char* pathname, size_t size, void* content, queue* ejected, queue* fdPending){

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


	 

	while(size + storage->currCapacity > storage->maxCapacity){
		
		fT* ef = eject_file(storage,fPtr->pathname,fdClient,1);
		if(!ef){
			UNLOCK(&(storage->smux))
			return FILE_TOO_LARGE;
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
	
		storage->currCapacity -= ef->size;
		
		
	}

	LOCK(&(fPtr->ordering))
	LOCK(&(fPtr->mux))

	// non posso eseguire scritture in contemporanea ad altri scrittori/lettori

	while(fPtr->activeReaders || fPtr->activeWriters)
		WAIT(&(fPtr->go),&(fPtr->mux))

	fPtr->activeWriters++; // 1

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))

	int ret = SUCCESS;

	if(fPtr->fdlock && fPtr->fdlock != fdClient){
		ret = LOCKED;
	}
	
	//  !ret --- ret == SUCCESS

	if(!ret && !findQueue(fPtr->fdopen,CAST_FD(fdClient))){
		ret = NOT_OPENED;
	}

	if(fPtr->fdwrite && fPtr->fdwrite != fdClient){
		ret = BAD_REQUEST;
	}else{
		fPtr->fdwrite = 0;
	}

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

	
	// permetto ad altri lettori/scrittori di avanzare 
	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))

	int ret = SUCCESS;

	if(fPtr->fdlock && fPtr->fdlock != fdClient){
		ret = LOCKED;
	}
	
	//  !ret --- ret == SUCCESS

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

int read_n_file(fsT* storage,int n,int fdClient, queue* ejected){

	
	LOCK(&(storage->smux))

	if(n <= 0 || n >= storage->currFileNum)
		n = storage->currFileNum;

	data* curr = storage->filesQueue->head;
	fT* fPtr = NULL, *ef = NULL;
	int ret = SUCCESS;

	while(!ret && n && curr){
		
		fPtr = (fT*)(curr->data);

		
		// faccio copia del file e lo inserisco in coda ai file da espellere (inviare)
		if(!(ef = fileCopy(fPtr)) || enqueue(ejected,ef) != 0)
			ret = SERVER_ERROR;
		
		curr = curr->next;
		n--;
	}

	UNLOCK(&(storage->smux))

	return ret;

}



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
    
	//fPtr->activeWriters++;

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








int store_remove(fsT* storage, fT* fPtr, int freeData){
	char* rmpath = fPtr->pathname;
	// rimuovo dalla coda di rimpiazzamento
	if(removeFromQueue(storage->filesQueue,fPtr,0) != 0)
		return -1; 

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

fT* fileCopy(fT* fPtr){
	fT* cp = NULL;

	chk_null(cp = malloc(sizeof(fT)),NULL)

	cp->size = fPtr->size;
	if(!(cp->pathname = strndup(fPtr->pathname,strlen(fPtr->pathname))) || !(cp->content = (void*)malloc(cp->size))){
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
	memcpy(cp->content,fPtr->content,cp->size);

	return cp;
}





fT* eject_file(fsT* storage, char* pathname, int fdClient, int chkEmpty){
	
	storage->tryEjectFile++;

	queue* fq = storage->filesQueue;

	if(isQueueEmpty(fq))
		return NULL;

	data* curr = fq->head;
	
	fT* tmp = NULL; 

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
			storage->ejectedFileNum++;
			return tmp;
		}		
		curr = curr->next;
	}

	
	return NULL; // attualmente non ho file da espellere

		
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
