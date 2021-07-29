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
	

	return storage;	
}

int get_pathname(int fd,char** pathname){
	// leggo lunghezza pathname
	char* _pathLen = calloc(PATHNAME_LEN+1,1);
	if(!_pathLen)
		return SERVER_ERROR;

	int ret = 0;

	ret = readn(fd,_pathLen,PATHNAME_LEN);
	if(ret == 0){
		clientExit(fd);
		free(_pathLen);
		return -1;
	}
	if(ret == -1){
		free(_pathLen);
		return SERVER_ERROR;
	}

	int pathLen = atoi(_pathLen);

	free(_pathLen);
	
	
	//leggo pathname 
	char* path = calloc(pathLen+1,1);
	chk_null(path,SERVER_ERROR)


	ret = readn(fd,path,pathLen);

	if(ret == 0){
		clientExit(fd);
		free(path);
		return -1;
	}
	if(ret == -1){
		free(path);
		return SERVER_ERROR;
	}	

	*pathname = path;
	
	return 0;
}

int get_flags(int fd, int *flags){
	int flags_ = 0;
	int ret = readn(fd,&flags_,FLAG_SIZE);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		errno = ECONNRESET;
		return -1;
	}
	*flags = flags_ - '0';

	return 0;
}

int get_file(int fd, size_t* size, void** content){

	char* fileSize = calloc(MAX_FILESIZE_LEN+1,1);
	chk_null(fileSize,SERVER_ERROR)
	//leggo dimensioneFile
	int ret = readn(fd,fileSize,MAX_FILESIZE_LEN);
	if(ret == 0){
		clientExit(fd);
		free(fileSize);
		return -1;
	}
	if(ret == -1){
		free(fileSize);
		return SERVER_ERROR;
	}
  
	size_t size_ = 0;	

	if(isNumber(fileSize,(long*)&size_) != 0){
		free(fileSize);
		return BAD_REQUEST;
	}
	free(fileSize);

	void* content_ = (void*) malloc(size_);
	chk_null(content_,SERVER_ERROR)

	//leggo contenuto file
	ret = readn(fd,content_,size_);

	if(ret == 0){
		errno = ECONNRESET;
		free(content_);
		return -1;
	}
	if(ret == -1){
		free(content_);
		return SERVER_ERROR;
	}


	*size = size_;
	*content = content_;

	return 0;
	
}

int open_file(fsT* storage, int fd){

	int ret = 0;

	char* pathname = NULL;
	
	// leggo il pathname del file da aprire
	if((ret = get_pathname(fd,&pathname)) != 0){
		return ret;
	}

	int flags = 0;

	//leggo i flags
	if((ret = get_flags(fd, &flags)) != 0){
		free(pathname);
		return ret;
	}
	
	

	short int create = IS_O_CREATE_SET(flags), lock = IS_O_LOCK_SET(flags);


	fT* fPtr = icl_hash_find(storage->ht,pathname);

	if(fPtr){ // se il file è già nel file storage
		free(pathname);
		if(create){
			//fprintf(stderr,"file esistente\n");
				return FILE_EXISTS;
		}
		if(lock){
			// LOCK
		}	
	
	}else{ // il file deve essere creato
		if(!create){
			//fprintf(stderr,"flag O_CREATE non specificato\n");

			return FILE_NOT_EXISTS;
		}
		// LOCK
		if((storage->currFileNum+1) < storage->maxFileNum)
			storage->currFileNum++;
		else{

		}

		chk_null_op(fPtr = malloc(sizeof(fT)),free(pathname),SERVER_ERROR)
		
		fPtr->pathname = pathname;
		

		fPtr->size = 0;
		fPtr->content = NULL;
		

		chk_null_op(icl_hash_insert(storage->ht,pathname,fPtr),free(pathname),SERVER_ERROR)

		// inserisco in coda per politica rimpiazzamento
		if(enqueue(storage->filesQueue,fPtr) != 0){
			return SERVER_ERROR;
		}
	}
	
	return SUCCESS;
}


int write_append_file(fsT* storage,int fd, int mode){

	int ret = 0;
	char* pathname = NULL;
	
	// leggo il pathname del file da scrivere
	if((ret = get_pathname(fd,&pathname)) != 0){
		return ret;
	}

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	if(!fPtr){
		//file non ancora creato
		free(pathname);
		return FILE_NOT_EXISTS;
	}
	free(pathname);

	size_t size = 0;
	void* content = NULL;

	// leggo file
	if((ret = get_file(fd,&size,&content)) != 0){
		return ret;
	}

	if(mode == 0){ // se modalita' Write
		fPtr->size = size;
		fPtr->content = content;
	}else{ // modalita' append
	
		fPtr->content = (void*) realloc(fPtr->content,fPtr->size + size);
		chk_null_op(fPtr->content,free(content),SERVER_ERROR)

		memcpy(fPtr->content+fPtr->size,content,size);
		fPtr->size += size;
		free(content);
	}

	return SUCCESS;

}




int read_file(fsT* storage,int fd){

	int ret = 0;
	char* pathname = NULL;
	
	// leggo il pathname del file da leggere
	if((ret = get_pathname(fd,&pathname)) != 0){
		return ret;
	}

	fT* fPtr = icl_hash_find(storage->ht,pathname);
	
	if(fPtr == NULL){
		free(pathname);
		return FILE_NOT_EXISTS;
	}

	if(fPtr->size == 0){ //file vuoto
		free(pathname);
		return EMPTY_FILE;
	}

	free(pathname);
	
	// mando SENDING_FILE + size + file
	long resLen = sizeof(char) + MAX_FILESIZE_LEN + fPtr->size +1;
	char* res = calloc(resLen,1);
	if(!res)
		return SERVER_ERROR;
	

	snprintf(res,resLen,"%d%010ld",SENDING_FILE,fPtr->size); // fsize 10 char max
	
	memcpy((res + strlen(res)),fPtr->content,fPtr->size);
	
	
	if(writen(fd,res,resLen-1) == -1){
		perror("writen");
		free(res);
		return -1;
	}

	return SUCCESS;

}

int read_n_file(fsT* storage,int fd){

	// leggo numero file da leggere
	char* buf = calloc(sizeof(int)+1,1);
	chk_null(buf,SERVER_ERROR)
		
	int ret = 0;

	ret = readn(fd,buf,sizeof(int));
	if(ret == 0){
		clientExit(fd);
		free(buf);
		return -1;
	}
	if(ret == -1){
		free(buf);
		return SERVER_ERROR;
	}

	
	
	int n = atoi(buf);

	free(buf);

	if(n <= 0 || n >= storage->currFileNum)
		n = storage->currFileNum;

	data* curr = storage->filesQueue->head;
	fT* fPtr;
	
	long resLen = 0;
	char* res = NULL;

	while(n && curr){
		
		
		fPtr = (fT*)(curr->data);
		
		if(fPtr->size == 0){ //file vuoto
			// mando solo il pathname

			resLen = sizeof(char) + sizeof(int) + strlen(fPtr->pathname) + 1;
		
			res = calloc(resLen,1);
			chk_null(res,SERVER_ERROR);

			snprintf(res,resLen,"%d%4d%s",EMPTY_FILE,(int)strlen(fPtr->pathname),fPtr->pathname);	

		}else{
			resLen = sizeof(char) + sizeof(int) + strlen(fPtr->pathname) + MAX_FILESIZE_LEN + fPtr->size +1;
		
			res = calloc(resLen,1);
			chk_null(res,SERVER_ERROR);

			snprintf(res,resLen,"%d%4d%s%010ld",SENDING_FILE,(int)strlen(fPtr->pathname),fPtr->pathname,fPtr->size);
			memcpy((res + strlen(res)),fPtr->content,fPtr->size);
		}

		if(writen(fd,res,resLen-1) == -1){
				perror("writen");
				free(res);
				return -1;
			}
		free(res);
		curr = curr->next;
		n--;
	}


	return END_SENDING_FILE;

}



int remove_file(fsT* storage, int fd){
	
	char* pathname = NULL;
	
	// leggo il pathname del file da cancellare
	chk_neg1(get_pathname(fd,&pathname),-1)


	fT* fPtr = icl_hash_find(storage->ht,pathname);
	chk_null_op(fPtr,free(pathname),FILE_NOT_EXISTS);

	// rimuovo dalla coda di rimpiazzamento
	if(removeFromQueue(storage->filesQueue,fPtr) != 0)
		return FILE_NOT_EXISTS; 

	// rimuovo dalla hash table
	if(icl_hash_delete(storage->ht,pathname,free,NULL) == -1)
		return FILE_NOT_EXISTS; 
	

	free(pathname);

	

	storage->currFileNum--;
	

	return SUCCESS;
		
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
