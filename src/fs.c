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




void clientExit(int fd){
	fprintf(stdout,"client %d disconnesso\n",fd);
	close(fd);
	return;
}


fsT* create_fileStorage(char* configPath, char* delim){
	
	/*
		per prima cosa effettuo il parsing del il file di configurazione 
	*/
	fsT* storage = parseConfig(configPath,delim);
	if(!storage)
		return NULL;


	// alloco hash table
	ec(storage->ht = icl_hash_create(storage->maxFileNum,NULL,NULL), NULL,"hash create",return NULL)
	
	storage->currCapacity = 0;
	storage->currFileNum = 0;
	ec(storage->filesQueue = createQueue(freeFile,cmpFile),NULL,"create queue",return NULL)

	ec_n(pthread_mutex_init(&(storage->smux),NULL),0,"pthread mutex init storage",return NULL)

	return storage;	
}


int open_file(fsT* storage, int fdClient, char* pathname, int flags){


	short int create = IS_O_CREATE_SET(flags), lock = IS_O_LOCK_SET(flags);

	// lock dello storage
	LOCK(&(storage->smux));

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	
	if(fPtr){ // se il file è già nel file storage
		if(create){
			UNLOCK(&(storage->smux));
			return FILE_EXISTS; // bisogna aprirlo senza O_CREATE
		}
		if(lock){
			// LOCK
			
		}	
		
		// metto fdClient in coda ai fd che hanno aperto il file

		LOCK(&(fPtr->ordering))
		LOCK(&(fPtr->mux))

		UNLOCK(&(storage->smux));

		while(fPtr->activeReaders || fPtr->activeWriters)
       		WAIT(&(fPtr->go),&(fPtr->mux))
    
		fPtr->activeWriters++;

		UNLOCK(&(fPtr->ordering))
		UNLOCK(&(fPtr->mux))
		


		// if(enqueue(fPtr->fdopen,CAST_FD(fdClient)) != 0){
		// 	UNLOCK(&(storage->smux));
		// 	return SERVER_ERROR;
		// }
		
		LOCK(&(fPtr->mux))

		fPtr->activeWriters--;
		SIGNAL(&(fPtr->go))

		UNLOCK(&(fPtr->mux))

		return SUCCESS;

	}
	// il file deve essere creato
	if(!create){
		UNLOCK(&(storage->smux));
		return FILE_NOT_EXISTS; // bisogna aprirlo con O_CREATE
	}

	

	// creo file (vuoto) sullo storage
	if(store_insert(storage,fdClient,pathname,lock) != 0){
		UNLOCK(&(storage->smux));
		return SERVER_ERROR;
	}


	UNLOCK(&(storage->smux));
	
	return SUCCESS;
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
	
	// se il client ha aperto il file rimuovo il suo fd
	if(removeFromQueue(fPtr->fdopen,CAST_FD(fdClient),0) == 1){
		UNLOCK(&(fPtr->ordering))
		UNLOCK(&(fPtr->mux))
        return NOT_OPENED;
	}

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return SUCCESS;
}


int write_append_file(fsT* storage,int fdClient,char* pathname, size_t size, void* content, queue* ejected){

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
		if(store_remove(storage,fPtr,1) != 0){
			return SERVER_ERROR;
		}
		UNLOCK(&(storage->smux))
		return FILE_TOO_LARGE;
	}


	 

	while(size + storage->currCapacity > storage->maxCapacity){
		
		fT* ef = eject_file(storage->filesQueue,fPtr->pathname);
		if(!ef){
			UNLOCK(&(storage->smux))
			return FILE_TOO_LARGE;
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


	if(!ret){

		// in ogni caso devo appendere al file
		fPtr->content = (void*) realloc(fPtr->content,fPtr->size + size);
		if(!(fPtr->content)){
			UNLOCK(&(storage->smux))
			return SERVER_ERROR;
		}

		memcpy(fPtr->content+fPtr->size,content,size);
		fPtr->size += size;
		storage->currCapacity += size;
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

		// leggo file

		// copio size e content 

		*size = fPtr->size;
		
		chk_null(*content = (void*)malloc(*size),SERVER_ERROR)
		memcpy(*content,fPtr->content,*size);

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
	
	while(n && curr){
		
		fPtr = (fT*)(curr->data);

		
		// faccio copia del file
		chk_null(ef = fileCopy(fPtr),SERVER_ERROR)
		
		// lo inserisco in coda ai file da espellere (inviare)
		if(enqueue(ejected,ef) != 0){
			// UNLOCK(&(storage->smux))
			return SERVER_ERROR;
		}
		curr = curr->next;
		n--;
	}

	UNLOCK(&(storage->smux))

	return SUCCESS;

}



int remove_file(fsT* storage, int fdClient, char* pathname){
	
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
    
	fPtr->activeWriters++;

	UNLOCK(&(fPtr->ordering))
	UNLOCK(&(fPtr->mux))

	
	// il client che richiede la remove deve aver prima lockato il file
	if(fPtr->fdlock != fdClient){
		//LOCK(&(fPtr->mux))
		fPtr->activeWriters--;
		//SIGNAL(&(fPtr->go))

		//UNLOCK(&(fPtr->ordering))
		//UNLOCK(&(fPtr->mux))
		UNLOCK(&(storage->smux))
		return NOT_LOCKED;
	}

	// rimuovo il file

	if(store_remove(storage,fPtr,1) != 0){
		UNLOCK(&(storage->smux));
		return SERVER_ERROR;
	}
	
	UNLOCK(&(storage->smux));

	return SUCCESS;
		
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
	
	// se un altro client ha lockato il file
	if(fPtr->fdlock != 0 && fPtr->fdlock != fdClient){
		LOCK(&(fPtr->mux))

		fPtr->activeWriters--;
		SIGNAL(&(fPtr->go))
	
		UNLOCK(&(fPtr->mux))
		return LOCKED;
	}

	// altrimenti lock

	fPtr->fdlock = fdClient;

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return SUCCESS;
		
}

int unlock_file(fsT* storage, int fdClient, char* pathname){
	
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
	
	// se un altro client ha lockato il file
	if(fPtr->fdlock != fdClient){
		LOCK(&(fPtr->mux))

		fPtr->activeWriters--;
		SIGNAL(&(fPtr->go))
		
		UNLOCK(&(fPtr->mux))
		return NOT_LOCKED;
	}

	// altrimenti reset lock

	fPtr->fdlock = 0;

	LOCK(&(fPtr->mux))

	fPtr->activeWriters--;

	SIGNAL(&(fPtr->go))

	UNLOCK(&(fPtr->mux))

	return SUCCESS;
		
}




int store_insert(fsT* storage, int fdClient, char* pathname,int lock){

	// controllo che ci sia spazio a sufficienza 		
	if((storage->currFileNum+1) > storage->maxFileNum){
		// non importa che il file sia vuoto o meno
		fT* ef = dequeue(storage->filesQueue);
		chk_null_op(ef,free(pathname),SERVER_ERROR)

		// rimuovo il file dallo storage senza inviarlo al client
		// printf("rimosso: %s\n",ef->pathname);
		// rimuovo dalla hash table
		chk_neg1_op(icl_hash_delete(storage->ht,ef->pathname,free,freeFile),free(pathname),SERVER_ERROR)
	}
	
	fT* fPtr = NULL;
	chk_null_op(fPtr = malloc(sizeof(fT)),free(pathname),SERVER_ERROR)
		
	ec_n(pthread_mutex_init(&(fPtr->mux),NULL),0,"file mux init",return SERVER_ERROR)
	ec_n(pthread_mutex_init(&(fPtr->ordering),NULL),0,"file ordering init",return SERVER_ERROR)
	
	ec_n(pthread_cond_init(&(fPtr->go),NULL),0,"file ordering init",return SERVER_ERROR)

	fPtr->activeReaders = 0;
	fPtr->activeWriters  = 0;
	fPtr->pathname = pathname;
	fPtr->size = 0;
	fPtr->content = NULL;
	
	fPtr->fdlock = fdClient;
	fPtr->fdwrite = fdClient;

	if(!(fPtr->fdopen = createQueue(free,NULL))){
		free(pathname);
		free(fPtr);
	}

	// il file, oltre ad essere creato, viene anche aperto

	if(enqueue(fPtr->fdopen,CAST_FD(fdClient)) != 0){
			UNLOCK(&(storage->smux));
			return SERVER_ERROR;
	}

	// se il flag O_LOCK e' settato allora viene anche lockato da fdClient

	if(lock){
		fPtr->fdlock = fdClient;
	}

	if(icl_hash_insert(storage->ht,pathname,fPtr) == NULL || enqueue(storage->filesQueue,fPtr) != 0){
		free(pathname);
		free(fPtr);
		return SERVER_ERROR;

	}
	

	storage->currFileNum++;

	return 0;
}








int store_remove(fsT* storage, fT* fPtr, int freeData){
	char* rmpath = fPtr->pathname;
	// rimuovo dalla coda di rimpiazzamento
	if(removeFromQueue(storage->filesQueue,fPtr,freeData) != 0)
		return -1; 

	// rimuovo dalla hash table

	if(freeData){
		if(icl_hash_delete(storage->ht,rmpath,free,NULL) == -1)
			return -1; 
	}else{
		if(icl_hash_delete(storage->ht,rmpath,NULL,NULL) == -1)
			return -1;
	}

	storage->currFileNum--;

	return 0; // file rimosso con successo
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
		
	memcpy(cp->content,fPtr->content,cp->size);

	return cp;
}


fT* eject_file(queue* fq,char* pathname){
	
	if(isQueueEmpty(fq))
		return NULL;

	data* curr = fq->head;
	
	fT* tmp = NULL; 

	// scorro la coda di files fin quando non ne trovo uno non vuoto
	while(curr){
		tmp = curr->data;
		// skippo i file vuoti e il file che voglio appendere
		if(tmp->size > 0 && strcmp(tmp->pathname,pathname) != 0)
			break;
		
		curr = curr->next;
	}

	if(curr){
		return tmp;
	}
	// se sono tutti vuoti ritorno NULL
	return NULL;
	// return fq->head->data;
		
}




void freeFile(void* fptr){
	fT* fPtr = fptr;
	if(fPtr){
		free(fPtr->content);
		//free(fPtr->pathname);
		free(fPtr);
	}
	return;
}




int cmpFile(void* f1, void* f2){
	return (strncmp(((fT*)f1)->pathname, ((fT *)f2)->pathname, UNIX_PATH_MAX) == 0) ? 1 : 0;
}
