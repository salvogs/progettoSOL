#include <stdio.h>
#include <errno.h>
#include "../include/clientParser.h"
#include "../include/queue.h"
#include "../include/api.h"
#include "../include/utils.h"

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
		if(errno){
			perror("command parsing");
		}
		destroyClientParsing(parseResult);
		return 1;
	}
	if(parseResult->PRINT_ENABLE == 1)
		PRINTS = 1;

	printParseResult(parseResult);

	//come prima cosa bisogna connettersi al server
	

	struct timespec abstime = {0,time(NULL)+OC_ABS_TIME};

	if(openConnection(parseResult->SOCKNAME,OC_RETRY_TIME,abstime) == -1){
		if(errno == ENOENT)
			fprintf(stderr,"open connection: Socket non trovato\n");
		else
			perror("open connection");

		destroyClientParsing(parseResult);
		return 1;
	}

	
	return 0;
}