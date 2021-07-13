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





fsT* createFileStorage(char* configPath, char* delim){
	
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


int openFile(fsT* storage, int fd, long pathLen){
	
		int letti = 0;
		
		//pathLen +1 per prendere anche i flags

		char* req = calloc(pathLen+1,1);
		ec(req,NULL,"calloc",return -1)

		if((letti = readn(fd,req,pathLen+1)) == 0){
			fprintf(stdout,"client %d disconnesso\n",fd);
			return -1;
		}
		//printf("letti: %d\n",letti);

		char* path = strndup(req,pathLen);
		ec(path,NULL,"strndup",return -1)


		printf("%s\n",path);
		// printf("%s\n",req);


		// if((letti = readn(fd,req,1)) == 0){
		// 	fprintf(stdout,"client %d disconnesso\n",fd);
		// 	return -1;			
		// }

		int flags = req[pathLen] - '0';
		printf("flags:%d\n",flags);

		free(req);


		// for(int i = 0; i <= letti;i++){
		// 	printf("%c",buf[i]);
		// }
		//printf("\n");
		
		



		short int create = IS_O_CREATE_SET(flags), lock = IS_O_LOCK_SET(flags);

		


		
		fT* fPtr = icl_hash_find(storage->ht,path);

		if(fPtr){ // se il file è già nel file storage
			if(create){
				fprintf(stderr,"file esistente\n");
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