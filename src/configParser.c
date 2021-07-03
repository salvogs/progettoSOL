#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/fsystem.h"
#include "../include/utils.h"
// #include <limits.h>
// #include <stdlib.h>

#define BUFSIZE 256

void remplaceNewline(char* token, int len){
	for(int i = 0; i < len; i++){
		if(iscntrl(token[i])){
			token[i] = '\0';
			break;
		}
	}
	return;
}

fsT* parseConfig(char* configPath,char* delim){

	FILE* fPtr = fopen(configPath,"r");
	ec(configPath,NULL,"config file",return NULL)
	
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
		if(buf[0] == '#' || iscntrl(buf[0]))
			continue;

		char* save = NULL;
		//printf("%s",buf);
		token = strtok_r(buf,delim,&save);
		
		
		if(strncmp(token,"MAXCAPACITY",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->maxCapacity,1,MAXCAPACITY_ERR)
		}else if(strncmp(token,"MAXFILENUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->maxFileNum,1,MAXFILENUM_ERR)
		}else if(strncmp(token,"WORKERNUM",strlen(token)) == 0){
			GET_INTEGER_PARAMETER(fsConfig->workerNum,1,WORKERNUM_ERR)
		}else if(strncmp(token,"SOCKETPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(fsConfig->SOCKNAME,".sk",SOCKNAME_ERR)
		}else if(strncmp(token,"LOGPATH",strlen(token)) == 0){
			GET_STRING_PARAMETER(fsConfig->logPath,".txt",LOGPATH_ERR)
		}



	}

	fclose(fPtr);
	return fsConfig;
}

void destroyConfiguration(fsT* fsConfig){
	free(fsConfig->SOCKNAME);
	free(fsConfig->logPath);

	free(fsConfig);
}

int main(int argc,char* argv[]){
	fsT* fsConfig = parseConfig("../config.txt",":");
	if(fsConfig)
		printf("MAXCAPACITY: %ld, MAXFILENUM: %ld"
		 "WORKERNUM %ld, SOCKNAME %s, LOGPATH %s\n"\
		 ,fsConfig->maxCapacity,fsConfig->maxFileNum,fsConfig->workerNum,fsConfig->SOCKNAME,fsConfig->logPath);

	destroyConfiguration(fsConfig);
	return 0;
}