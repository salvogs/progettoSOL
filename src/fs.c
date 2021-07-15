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


int open_file(fsT* storage, int fd, long pathLen){
		
		//pathLen +1 per prendere anche i flags

		char* req = calloc(pathLen+1,1);
		ec(req,NULL,"calloc",return -1)

		if((readn(fd,req,pathLen+1)) == 0){
			fprintf(stdout,"client %d disconnesso\n",fd);
			free(req);
			return -1;
		}
		

		char* path = strndup(req,pathLen);
		if(!path){
			perror("strndup");
			free(req);
			return -1;
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
				fprintf(stderr,"flag O_CREATE non specificato\n");
				return BAD_REQUEST;
			}
			// LOCK
			if(storage->fileNum < storage->maxFileNum)
				storage->fileNum++;
			else{}

			if(!(fPtr = malloc(sizeof(fT))))
				return SERVER_ERROR;

			fPtr->pathname = malloc(strlen(path)+1);
			strncpy(fPtr->pathname,path,strlen(path)+1);
	
			fPtr->size = 0;
			fPtr->content = NULL;
			

			if(!icl_hash_insert(storage->ht,path,fPtr))
				return SERVER_ERROR;

		}
		
		
		return 0;
}


int write_file(fsT* storage,int fd,int pathLen){
	
	char* req = calloc(pathLen+8,1);
	ec(req,NULL,"calloc",return -1)

	//printf("pathlen: %d\n",pathLen);

	//leggo pathname e dimensioneFile
	if((readn(fd,req,pathLen+8)) == 0){
			fprintf(stdout,"client %d disconnesso\n",fd);
			free(req);
			return -1;
	}
  
	
	char* path = strndup(req,pathLen);
	if(!path){
		perror("strndup");
		free(req);
		return -1;
	}
		


	//printf("%s\n",path);
	char* fileSize = strndup(req+pathLen,sizeof(long));
	if(!fileSize){
		perror("strndup");
		free(req);
		free(path);
		return -1;
	}

	free(req);


	long fsize = 0;	


	if(isNumber(fileSize,&fsize) != 0){
		free(fileSize);
		free(path);
		return -1;
	}

	free(fileSize);

	fT* fPtr = icl_hash_find(storage->ht,path);
	if(!fPtr){
		//file non ancora creato
		free(path);
		return -1;
	}
	free(path);

	ec(fPtr->content = malloc(fsize),NULL,"malloc",return -1);
	
	

	if((readn(fd,fPtr->content,fsize)) == 0){
			fprintf(stdout,"client %d disconnesso\n",fd);
			return -1;
	}

	return 0;

}