#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fs.h"
#include "../include/utils.h"
#include "../include/configParser.h"



fsT* createFileStorage(char* configPath, char* delim){
	
	/*
		per prima cosa effettuo il parsing del il file di configurazione 
	*/
	fsT* storage = parseConfig(configPath,delim);
	if(!storage)
		return NULL;


	// alloco hash table
	ec(storage->ht = icl_hash_create(storage->maxFileNum,NULL,NULL), NULL,"hash create",return NULL)
	printf("dentroo %p\n",storage);

	return storage;	
}


int openFile(int fd, long pathLen){
	
		int letti = 0;
		
		//pathLen +1 per prendere anche i flags

		char* req = calloc(pathLen+1,1);
		ec(req,NULL,"calloc",return -1)

		if((letti = readn(fd,req,pathLen+1)) == 0){
			fprintf(stdout,"client %d disconnesso\n",fd);
			return -1;
		}
		printf("letti: %d\n",letti);

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
		
		
		writen(fd,"0",1);

		return 0;
}