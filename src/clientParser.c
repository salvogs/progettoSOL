//#define _POSIX_C_SOURCE 200809L
// #define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "../include/queue.h"
#include "../include/clientParser.h"
#include "../include/utils.h"

void destroyClientParsing(parseT* parseResult){
	destroyQueue(parseResult->argList);
	if(parseResult->SOCKNAME)
		free(parseResult->SOCKNAME);

	free(parseResult);
}


int enqueueArg(queue* argQueue, char opt,char* arg){
	//alloco spazio per memorizzare coppia opzione-argomento
	optT* op = malloc(sizeof(optT));
	ec(op,NULL,"malloc",return 1)

	op->opt = opt;
	
	int argLen = strlen(arg)+1; 
	
	ec(op->arg = malloc(argLen),NULL,"malloc",return 1);
	strncpy(op->arg,arg,argLen);

	return enqueue(argQueue,op);

}

parseT* parser(int argc, char* argv[]){
	if(argc == 1){
		printf("nessun argomento\n");
		fprintf(stderr,"usage:%s\n" HELP_MESSAGE,argv[0]);
		return NULL;
	}
	

	//int p_flag = 0,f_flag = 0;

	parseT* parseResult = malloc(sizeof(parseT));
	ec(parseResult,NULL,"malloc",return NULL)
	parseResult->argList = NULL;
	parseResult->PRINT_ENABLE = 0;
	parseResult->REQ_TIME = 0;
	parseResult->SOCKNAME = NULL;

	

	queue* argQueue = createQueue(free,NULL);
	ec(argQueue,NULL,"creazione coda args",return NULL)
	
	parseResult->argList = argQueue;

	int ret = 0;
	char opt;
	while((opt = getopt(argc,argv,":hf::w:W:D:r:R::d:t::l:u:c:p")) != -1){
		switch(opt){
			case 'h':  
				fprintf(stdout,HELP_MESSAGE); 
				destroyQueue(argQueue); 
				return NULL; 
			break;
			
			case 'R':
				if(optind < argc && argv[optind][0] != '-')
					optarg = argv[optind];
					//ret = enqueueArg(argQueue,opt,optarg);
				if(optarg){
					ret = enqueueArg(argQueue,opt,optarg);
				}

			break;

			case 'p': 
				if(!(parseResult->PRINT_ENABLE)) 
					parseResult->PRINT_ENABLE = 1; 
			break;

			case 'f':
				if(!(parseResult->SOCKNAME)){
					if(optind < argc && argv[optind][0] != '-')
						optarg = argv[optind];
					//ret = enqueueArg(argQueue,opt,optarg);
					if(optarg){
						int snameLen = strlen(optarg)+1; 
						ec(parseResult->SOCKNAME = (char*)malloc(snameLen),NULL,"malloc",return NULL);
						strncpy(parseResult->SOCKNAME,optarg,snameLen);
					}
				}
			break;

			case 't':
				if(!(parseResult->REQ_TIME)){
					if(optind < argc && argv[optind][0] != '-')
						optarg = argv[optind];
					//ret = enqueueArg(argQueue,opt,optarg);
					if(optarg){
						if(isNumber(optarg,&parseResult->REQ_TIME) != 0){ // no numero/overflow
							fprintf(stderr,"-t deve essere seguito da un intero\n");
							return NULL;
						}
					}
				}

			break;
			

			case ':':
				fprintf(stderr,"l'opzione '-%c' richiede un argomento\n", optopt);
				return NULL;
			break;

			case '?':// restituito se getopt trova una opzione non riconosciuta
      			fprintf(stderr,"l'opzione '-%c' non e' gestita\n", optopt);
				return NULL;
    		break;
			default:
				//if(optarg[0] != '-')
					ret = enqueueArg(argQueue,opt,optarg);
			break;
		}

		if(ret == 1){
			return NULL;
		}
	}	

	if(!parseResult->SOCKNAME){
		fprintf(stderr,"l'opzione -f e' obbligatoria\n");
		return NULL;
	}
	//destroyQueue(argQueue);
	return parseResult;
}

















