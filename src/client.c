#include <stdio.h>
#include <errno.h>
#include "../include/parser.h"
#include "../include/queue.h"

int main(int argc, char* argv[]){

	queue* cmdList = parser(argc,argv);

	if(!cmdList && errno){
		perror("command parsing");
		return 1;
	}

	//cmdList contiene tutti i comandi che deve eseguire il client
	
	printf("nargs %d\n",cmdList->ndata);
	return 0;
}