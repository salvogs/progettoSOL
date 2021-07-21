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
	
	storage->capacity = 0;
	storage->fileNum = 0;
	
	return storage;	
}


int open_file(fsT* storage, int fd){
		
	// leggo lunghezza pathname
	char* _pathLen = calloc(sizeof(int)+1,1);
	if(!_pathLen)
		return SERVER_ERROR;

	int ret = 0;

	ret = readn(fd,_pathLen,sizeof(int));
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
	//pathLen +1 per prendere anche i flags

	char* req = calloc(pathLen+1,1);
	if(!req)
		return SERVER_ERROR;


	ret = readn(fd,req,pathLen+1);

	if(ret == 0){
		clientExit(fd);
		free(req);
		return -1;
	}
	if(ret == -1){
		free(req);
		return SERVER_ERROR;
	}	

	char* path = strndup(req,pathLen);
	if(!path){
		//perror("strndup");
		free(req);
		return SERVER_ERROR;
	}


	int flags = req[pathLen] - '0';
	
	free(req);


	short int create = IS_O_CREATE_SET(flags), lock = IS_O_LOCK_SET(flags);


	fT* fPtr = icl_hash_find(storage->ht,path);

	if(fPtr){ // se il file è già nel file storage
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
		if(storage->fileNum < storage->maxFileNum)
			storage->fileNum++;
		else{}

		if(!(fPtr = malloc(sizeof(fT))))
			return SERVER_ERROR;

		fPtr->pathname = path;
		// malloc(strlen(path)+1);
		// strncpy(fPtr->pathname,path,strlen(path)+1);

		fPtr->size = 0;
		fPtr->content = NULL;
		

		if(!icl_hash_insert(storage->ht,path,fPtr))
			return SERVER_ERROR;

	}
	
	
	return SUCCESS;
}


int write_file(fsT* storage,int fd){

	// leggo lunghezza pathname
	char* _pathLen = calloc(sizeof(int)+1,1);
	if(!_pathLen)
		return SERVER_ERROR;
	
	
	int ret = 0;

	ret = readn(fd,_pathLen,sizeof(int));
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


	char* req = calloc(pathLen+MAX_FILESIZE_LEN,1);
	ec(req,NULL,"calloc",return -1)

	//printf("pathlen: %d\n",pathLen);

	//leggo pathname e dimensioneFile
	ret = readn(fd,req,pathLen+MAX_FILESIZE_LEN);
	if(ret == 0){
		clientExit(fd);
		free(req);
		return -1;
	}
	if(ret == -1){
		free(req);
		return SERVER_ERROR;
	}
  
	
	char* path = strndup(req,pathLen);
	if(!path){
		//perror("strndup");
		free(req);
		return SERVER_ERROR;
	}
		


	//printf("%s\n",path);
	char* fileSize = strndup(req+pathLen,MAX_FILESIZE_LEN);
	if(!fileSize){
		//perror("strndup");
		free(req);
		free(path);
		return SERVER_ERROR;
	}
	
	free(req);
	// printf("%s\n",fileSize);

	long fsize = 0;	


	if(isNumber(fileSize,&fsize) != 0){
		free(fileSize);
		free(path);
		return BAD_REQUEST;
	}
	// printf("%ld\n",fsize);
	free(fileSize);

	fT* fPtr = icl_hash_find(storage->ht,path);
	if(!fPtr){
		//file non ancora creato
		free(path);
		return FILE_NOT_EXISTS;
	}
	free(path);

	fPtr->content = malloc(fsize);
	if(!fPtr->content)
		return SERVER_ERROR;
	
	

	ret = readn(fd,fPtr->content,fsize);
	if(ret == 0){
		clientExit(fd);
		free(fPtr->content);
		return -1;
	}
	if(ret == -1){
		free(fPtr->content);
		return SERVER_ERROR;
	}

	fPtr->size = fsize;

	return SUCCESS;

}


int read_file(fsT* storage,int fd){

	// leggo lunghezza pathname
	char* _pathLen = calloc(sizeof(int),1);
	if(!_pathLen)
		return SERVER_ERROR;
	


	int ret = 0;

	ret = readn(fd,_pathLen,sizeof(int));
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

	char* req = calloc(pathLen,1);
	if(!req)
		return SERVER_ERROR;
	

	ret = readn(fd,req,pathLen);
	if(ret == 0){
		clientExit(fd);
		free(req);
		return -1;
	}
	if(ret == -1){
		free(req);
		return SERVER_ERROR;
	}

	

	char* path = strndup(req,pathLen);
	if(!path){
		//perror("strndup");
		free(req);
		return SERVER_ERROR;
	}		
	free(req);

	fT* fPtr = icl_hash_find(storage->ht,path);
	
	if(fPtr == NULL){
		free(path);
		return FILE_NOT_EXISTS;
	}

	if(fPtr->size == 0){ //file vuoto
		free(path);
		return EMPTY_FILE;
	}

	free(path);

	// mando SUCCESS + size + file
	long resLen = sizeof(char) + MAX_FILESIZE_LEN + fPtr->size +1;
	char* res = calloc(resLen,1);
	if(!res)
		return SERVER_ERROR;
	

	snprintf(res,resLen,"%d%010ld",SUCCESS,fPtr->size); // fsize 10 char max
	
	memcpy((res + strlen(res)),fPtr->content,fPtr->size);
	
	
	if(writen(fd,res,resLen-1) == -1){
		perror("writen");
		free(res);
		return -1;
	}

	return SUCCESS;

}




int remove_file(fsT* storage, int fd){
	
	// leggo lunghezza pathname
	char* _pathLen = calloc(sizeof(int)+1,1);
	if(!_pathLen)
		return SERVER_ERROR;
	

	int ret;

	ret = readn(fd,_pathLen,sizeof(int));

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


	char* req = calloc(pathLen,1);
	if(!req)
		return SERVER_ERROR;
	

	ret = readn(fd,req,pathLen);
	
	if(ret == 0){
		fprintf(stdout,"client %d disconnesso\n",fd);
		free(req);
		return -1;
	}
	if(ret == -1){
		free(req);
		return SERVER_ERROR;
	}




	char* path = strndup(req,pathLen);
	if(!path){
		//perror("strndup");
		free(req);
		return SERVER_ERROR;
	}		
	free(req);

	fT* fPtr = icl_hash_find(storage->ht,path);

	
	if(icl_hash_delete(storage->ht,path,NULL,NULL) == -1)
		return FILE_NOT_EXISTS; 
		
	free(fPtr->content);
	free(fPtr->pathname);

	free(fPtr);

	return SUCCESS;
		
}
