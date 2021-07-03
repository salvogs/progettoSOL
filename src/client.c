#include <stdio.h>
#include <errno.h>
#include "../include/clientParser.h"
#include "../include/queue.h"
//#include "../include/api.h"

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
		puts("qui");
		if(errno){
			perror("command parsing");
		}
		return 1;
	}

	printParseResult(parseResult);

	//come prima cosa bisogna connettersi al server
	//ec(openConnection(parseResult->SOCKNAME,10,)


	

	
	return 0;
}