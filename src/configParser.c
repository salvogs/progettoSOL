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
int ep = 0;

void remplaceNewline(char* s, int len){
	for(int i = 0; i < len; i++){
		if(s[i] == '\n' || s[i] == '\r'){
			s[i] = '\0';
			break;
		}
	}
	return;
}

int checkParsing(parseT* config){
	
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

	if(!ep){
		errno = EINVAL;
		perror("EVICTIONPOLICY");
	}


	if(errno){
		destroyConfiguration(config);
		return 1;
	}
	
	return 0;

}


parseT* parseConfig(char* configPath,char* delim){

	char* path = realpath(configPath,NULL);
	ec(path,NULL,"config path",return NULL)


	FILE* fPtr = fopen(path,"r");
	if(!fPtr)
		return NULL;
	free(path);

	char buf[256];
	char* token;

	parseT* config = malloc(sizeof(parseT));
	ec(config,NULL,"malloc config",return NULL)

	config->maxCapacity = 0;
	config->maxFileNum = 0;
	config->workerNum = 0;
	config->evictionPolicy = 0;
	config->sockname = NULL;
	config->logPath = NULL;


	while(fgets(buf,BUFSIZE,fPtr) != NULL){
		if(buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
			continue;

		char* save = NULL;
		
		token = strtok_r(buf,delim,&save);
		if(!token){
			fprintf(stderr,"strtok_r failed\n");
			destroyConfiguration(config);
			return NULL;
		}
		
		if(strncmp(token,"MAXCAPACITY",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(config->maxCapacity,1,MAXCAPACITY_ERR)
			mc++;
		}else if(strncmp(token,"MAXFILENUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(config->maxFileNum,1,MAXFILENUM_ERR)
			mfn++;
		}else if(strncmp(token,"WORKERNUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(config->workerNum,1,WORKERNUM_ERR)
			wn++;
		}else if(strncmp(token,"SOCKETPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(config->sockname,".sk",SOCKNAME_ERR)
			sp++;
		}else if(strncmp(token,"LOGPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(config->logPath,".txt",LOGPATH_ERR)
			lp++;
		}else if(strncmp(token,"EVICTIONPOLICY",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(config->evictionPolicy,0,EVICTIONPOLICY_ERR)
			if(config->evictionPolicy != 0 && config->evictionPolicy != 1 && config->evictionPolicy != 2){
				fprintf(stderr,EVICTIONPOLICY_ERR);
				return NULL;
			}
			ep++;
		}



	}

	fclose(fPtr);

	if(checkParsing(config) != 0)
		return NULL;
	

	return config;

	

}

void destroyConfiguration(parseT* config){
	if(config->sockname)
		free(config->sockname);
	if(config->logPath)
		free(config->logPath);
	if(config)
		free(config);
}
