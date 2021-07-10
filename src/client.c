#include <stdio.h>
#include <errno.h>
#include "../include/clientParser.h"
#include "../include/queue.h"
#include "../include/api.h"
#include "../include/utils.h"


//#include <string.h> //


#define OC_RETRY_TIME 1000 //ms
#define OC_ABS_TIME 10 //s


void printParseResult(parseT* parseResult){

	int tmpSize = parseResult->argList->ndata;
	data* tmpData = parseResult->argList->head;
	optT* tmpOp;
	while(tmpSize){
		tmpOp = tmpData->data;
		fprintf(stdout,"flag: %c arg:%s\n",tmpOp->opt, tmpOp->arg);
		tmpSize--;
		tmpData = tmpData->next;
	}

	fprintf(stdout,"STAMPE ABILITATE:%d\nSOCKETNAME:%s\nREQUEST TIME:%ld\n",parseResult->PRINT_ENABLE,parseResult->SOCKNAME,parseResult->REQ_TIME);

}


int main(int argc, char* argv[]){

	parseT* parseResult = parser(argc,argv);

	if(!parseResult){
		if(errno)
			perror("command parsing");

		return 1;
	}
	if(parseResult->PRINT_ENABLE == 1)
		PRINTS = 1;

	printParseResult(parseResult);

	//come prima cosa bisogna connettersi al server


	queue* args = parseResult->argList;
	optT *op,*nextOp;
	//ptr per scorrere la lista degli argomenti passati al client
	while(args->ndata){
		op = dequeue(args);
		switch(op->opt){
			case 'w':{
				nextOp = args->head->data;
				if(nextOp && nextOp->opt == 'D'){
					removeFromQueue(args); //rimuovo il -D
					fprintf(stdout,"scrivo %s e rimpiazzo su %s\n",op->arg,nextOp->arg);
				}else{
					fprintf(stdout,"scrivo %s\n",op->arg);
				}
			}
			break;

			case 'W':{
				nextOp = args->head->data;
				if(nextOp && nextOp->opt == 'D'){
					removeFromQueue(args); //rimuovo il -D
					fprintf(stdout,"scrivo %s e rimpiazzo su %s\n",op->arg,nextOp->arg);
				}else{
					fprintf(stdout,"scrivo %s\n",op->arg);
				}
			}

			break;

			case 'D':{
				fprintf(stderr,"l'opzione -D deve seguire -w/-W\n");
				return 1;
			}
			break;

			case 'r':{
				nextOp = args->head->data;
				if(nextOp && nextOp->opt == 'd'){
					removeFromQueue(args); //rimuovo il -D
					fprintf(stdout,"leggo %s su %s\n",op->arg,nextOp->arg);
				}else{
					fprintf(stdout,"leggo %s\n",op->arg);
				}
			}
			break;

			case 'd':
				fprintf(stderr,"l'opzione -d deve seguire -r/-R\n");
				return 1;
			break;

			case 'R':
				nextOp = args->head->data;
				if(nextOp && nextOp->opt == 'd'){
					removeFromQueue(args); //rimuovo il -D
					fprintf(stdout,"leggo %s file su %s\n",op->arg,nextOp->arg);
				}else{
					fprintf(stdout,"leggo %s file\n",op->arg);
				}
			break;

			case 'l':
				fprintf(stdout,"lock su %s\n",op->arg);
			break;

			case 'u':
				fprintf(stdout,"unlock su %s\n",op->arg);
			break;

			case 'c':
				fprintf(stdout,"rimuovo %s\n",op->arg);
			break;

			default:;
		}
	}
	
	

	struct timespec abstime = {0,time(NULL)+OC_ABS_TIME};

	

	if(openConnection(parseResult->SOCKNAME,OC_RETRY_TIME,abstime) == -1){
		if(errno == ENOENT)
			fprintf(stderr,"open connection: Socket non trovato\n");
		else
			perror("open connection");

		destroyClientParsing(parseResult);
		return 1;
	}

	openFile("bin/server",3);

	ec(closeConnection(parseResult->SOCKNAME),-1,"close connection",return 1);
	
	return 0;
}