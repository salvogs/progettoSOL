#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include "../include/clientParser.h"
#include "../include/queue.h"
#include "../include/api.h"
#include "../include/utils.h"

//#include <string.h> //
char *realpath(const char *path, char *resolved_path);

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


int isPointDir(char *dir){
    if(strcmp(dir,".") && strcmp(dir,".."))
        return 0;

    return 1;
}
void recursiveVisit(char* pathname,long n){
	
	DIR *d;
    //tento di aprire la directory 
	ec(d=opendir(pathname),NULL,"opendir",return)

    
    //printf("Directory: %s\n",name);

    struct dirent *file;
    while((errno = 0, file = readdir(d))!=NULL && n > 0){
        int nameLen = strlen(pathname)+strlen(file->d_name)+2; //  '\0'+'\'
        char absoluteName[nameLen];
        //concatenazione directory e filename per ottenere un path assoluto
        strncpy(absoluteName,pathname,nameLen-1);
        strncat(absoluteName,"/",nameLen-1);
        strncat(absoluteName,file->d_name,nameLen-1);




        if(isPointDir(file->d_name) == 0){ //non è una point-dir
            if(file->d_type == DT_DIR){
                //if(n != 0)
					recursiveVisit(absoluteName,n); //chiamata ricorsiva
			}else{
                openFile(absoluteName,O_CREATE | O_LOCK);
				// n--;
			}
        }
    }
    

    if (errno != 0){
        perror("file/directory");
        return;
    }

    closedir(d);

	return;

}
int w_handler(char* args,char* dirname){

	char* save;
	char* path;
	int n = 10;

	if(!(path = strtok_r(args,",",&save))){
		errno = EINVAL;
		perror("w dirname");
		return 1;
	}
	char* pathname = realpath(path,NULL);
	if(!pathname){
		free(path);
		perror("realpath");
		return 1;
	}
	free(path);


	// if((nfile = strtok_r(NULL,"n=",&save))){
	// 	if(isNumber(nfile,(long*)&n) != 0){
	// 		errno = EINVAL;
	// 		perror("n=");
	// 		return 1;
	// 	}
	// }
	
	printf("scrivo dir: %s n file = %d\n",pathname,n);
	
	recursiveVisit(pathname,n);
   	// openFile(args,O_CREATE | O_LOCK);

	return 0;
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

	//printParseResult(parseResult);

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
	
	queue* args = parseResult->argList;
	optT *op,*nextOp;

	char* dirname = NULL;
	//ptr per scorrere la lista degli argomenti passati al client
	while(args->ndata){
		op = dequeue(args);
		switch(op->opt){
			case 'w':{
				nextOp = args->head->data;
				if(nextOp && nextOp->opt == 'D'){
					removeFromQueue(args); //rimuovo il -D
					dirname = nextOp->arg;
					//fprintf(stdout,"scrivo %s e rimpiazzo su %s\n",op->arg,dirname);
				}else{
					dirname = NULL;
					//fprintf(stdout,"scrivo %s\n",op->arg);
				}
				w_handler(op->arg,dirname);
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
	
	

	

	

	ec(closeConnection(parseResult->SOCKNAME),-1,"close connection",return 1);
	
	return 0;
}