#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "../include/queue.h"
#include "../include/parser.h"


int p_flag = 0;
int f_flag = 0;


// }
//-f -h -p solo 1 volta
//realpath converte path relativo in assoluto



int enqueueArg(queue* argQueue, char opt,char* arg){
	//alloco spazio per memorizzare coppia opzione-argomento
	opType* op = malloc(sizeof(opType));
	ec(op,NULL,"malloc",return 1)

	op->opt = opt;
	op->arg = arg;

	return enqueue(argQueue,op);

}

queue* parser(int argc, char* argv[]){
	if(argc == 1){
		printf("nessun argomento\n");
		fprintf(stderr,"usage:%s\n" HELP_MESSAGE,argv[0]);
		return NULL;
	}
	
	

	queue* argQueue = createQueue(free,NULL);
	ec(argQueue,NULL,"creazione coda args",return NULL)
	
	int ret = 0;
	char opt;
	while((opt = getopt(argc,argv,":hf:w:W:D:r:R::d:t:l:u:c:p")) != -1){
		switch(opt){
			case 'h':  
				fprintf(stdout,HELP_MESSAGE); 
				destroyQueue(argQueue); 
				return NULL; 
			break;
			
			case 'R':
				if(optind < argc && argv[optind][0] != '-'){
					optarg = argv[optind];
				}
				ret = enqueueArg(argQueue,opt,optarg);
			break;

			case 'p': 
				if(!p_flag) 
					p_flag = 1; 
			break;

			case 'f':
				if(!f_flag){
					f_flag = 1;
					// ec(sockname = strndup(optarg,UNIX_PATH_MAX),NULL,"malloc",return 1)
				}
			break;

			case ':':
				fprintf(stderr,"l'opzione '-%c' richiede un argomento\n", optopt);
				errno = EINVAL;
				return NULL;
			break;

			case '?':// restituito se getopt trova una opzione non riconosciuta
      			fprintf(stderr,"l'opzione '-%c' non e' gestita\n", optopt);
				errno = EINVAL;
				return NULL;
    		break;
			default:
				ret = enqueueArg(argQueue,opt,optarg);
			break;
		}

		if(ret == 1){
			return NULL;
		}
	}	

	int tmpSize = argQueue->ndata;
	data* tmpData = argQueue->head;
	opType* tmpOp;
	while(tmpSize){
		tmpOp = tmpData->data;
		fprintf(stdout,"flag: %c arg:%s\n",tmpOp->opt, tmpOp->arg);
		tmpSize--;
		tmpData = tmpData->next;
	}

	//destroyQueue(argQueue);
	return argQueue;
}

















