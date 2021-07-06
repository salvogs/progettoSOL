#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/configParser.h"
#include "../include/utils.h"
#include "../include/server.h"
// #include <limits.h>
// #include <stdlib.h>

#define BUFSIZE 256


// per contare le occorrenze dei parametri
int mc = 0;
int mfn = 0;
int wn = 0;
int sp = 0;
int lp = 0;

void remplaceNewline(char* s, int len){
	for(int i = 0; i < len; i++){
		if(s[i] == '\n' || s[i] == '\r'){
			s[i] = '\0';
			break;
		}
	}
	return;
}

fsT* checkParsing(fsT* fsConfig){
	
	if(!mc){
		errno = EINVAL;
		perror("MAXCAPACITY");
	}

	if(!mfn){
		errno = EINVAL;
		perror("MAXFILENUM");
	}

	if(!wn){
		errno = EINVAL;
		perror("WORKERNUM");
	}

	if(!sp){
		errno = EINVAL;
		perror("SOCKETPATH");
	}
	
	if(!lp){
		errno = EINVAL;
		perror("LOGPATH");
	}

	if(errno){
		destroyConfiguration(fsConfig);
		return NULL;
	}
	
	return fsConfig;

}


fsT* parseConfig(char* configPath,char* delim){

	char* path = realpath(configPath,NULL);
	ec(path,NULL,"config path",return NULL)


	FILE* fPtr = fopen(path,"r");
	if(!fPtr)
		return NULL;
	free(path);

	char buf[256];
	char* token;

	fsT* fsConfig = malloc(sizeof(fsT));
	ec(fsConfig,NULL,"malloc fsConfig",return NULL)

	fsConfig->maxCapacity = 0;
	fsConfig->maxFileNum = 0;
	fsConfig->workerNum = 0;
	fsConfig->SOCKNAME = NULL;
	fsConfig->logPath = NULL;


	while(fgets(buf,BUFSIZE,fPtr) != NULL){
		if(buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
			continue;

		char* save = NULL;
		//printf("%s",buf);
		token = strtok_r(buf,delim,&save);
		if(!token){
			fprintf(stderr,"strtok_r failed\n");
			return NULL;
		}
		
		if(strncmp(token,"MAXCAPACITY",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->maxCapacity,1,MAXCAPACITY_ERR)
			mc++;
		}else if(strncmp(token,"MAXFILENUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->maxFileNum,1,MAXFILENUM_ERR)
			mfn++;
		}else if(strncmp(token,"WORKERNUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->workerNum,1,WORKERNUM_ERR)
			wn++;
		}else if(strncmp(token,"SOCKETPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(fsConfig->SOCKNAME,".sk",SOCKNAME_ERR)
			sp++;
		}else if(strncmp(token,"LOGPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(fsConfig->logPath,".txt",LOGPATH_ERR)
			lp++;
		}



	}

	fclose(fPtr);

	if(!checkParsing(fsConfig))
		return NULL;
	

	return fsConfig;

	

}

void destroyConfiguration(fsT* fsConfig){
	if(fsConfig->SOCKNAME)
		free(fsConfig->SOCKNAME);
	if(fsConfig->logPath)
		free(fsConfig->logPath);
	if(fsConfig)
		free(fsConfig);
}
