#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#define MAXCAPACITY_ERR "MAXCAPACITY: deve essere un intero > 0\n"

#define MAXFILENUM_ERR "MAXFILENUM: deve essere un intero > 0\n"

#define WORKERNUM_ERR "WORKERNUM: deve essere un intero > 0\n"

#define SOCKNAME_ERR "SOCKNAME: deve essere un path terminato con .sk\n"

#define LOGPATH_ERR "LOGPATH: deve essere un path terminato con .txt\n"


#include "../include/server.h"
/*
	converte, se il campo 'p' non e' gia' stato settato,
	il parametro di configurazione in intero e controlla 
	che sia < di 'v'
*/

#define GET_INTEGER_PARAMETER(p,v,err_m) \
	if(p == 0){\
		token = strtok_r(NULL,delim,&save);\
		if(!token){\
			fprintf(stderr,err_m); \
			return NULL;\
		}\
		remplaceNewline(token,strlen(token));\
		if(isNumber(token,&(p)) == 0){\
			if(p < v){\
				fprintf(stderr,err_m); \
				return NULL;\
			}\
		}\
}


/*
	copia, se il campo 'p' non e' gia' stato settato,
	il parametro di configurazione nella giusta variabile
	e controlla che termini con ext	
*/

#define GET_STRING_PARAMETER(p,ext,err_m) \
	if(p == NULL){\
		token = strtok_r(NULL,delim,&save);\
		if(!token){\
			fprintf(stderr,err_m); \
			return NULL;\
		}\
		remplaceNewline(token,strlen(token));\
		if(!(p = malloc(strlen(token) + 1))){ \
			perror("malloc"); \
			return NULL;  \
		}\
		char* occ = strrchr(token,'.');\
		if(!occ || strncmp(occ,ext,strlen(occ)+1) != 0){\
			fprintf(stderr,err_m); \
			return NULL;\
		}\
		strncpy(p,token,strlen(token)+1);\
	}

		





void remplaceNewline(char* s, int len);
fsT* parseConfig(char* configPath,char* delim);
void destroyConfiguration(fsT* fsConfig);

#endif