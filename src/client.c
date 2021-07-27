#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/clientParser.h"
#include "../include/client.h"
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


int isRegularFile(char* pathname){
	struct stat info;
    ec_n(stat(pathname, &info),0,pathname,return 0)
	return S_ISREG(info.st_mode);
}

int isDirectory(char* pathname){
	struct stat info;
    ec_n(stat(pathname, &info),0,pathname,return 0)
	return S_ISDIR(info.st_mode);
}

int isPointDir(char *dir){
    if(strcmp(dir,".") && strcmp(dir,".."))
        return 0;

    return 1;
}
int recursiveVisit(char* pathname,long* n,int limited,char* dirname){
	
	DIR *d;
	
    //tento di aprire la directory 
	ec(d=opendir(pathname),NULL,"opendir",return 1)

    
    //printf("Directory: %s\n",name);

    struct dirent *file;

    while((errno = 0, file = readdir(d))!=NULL && (!limited || (*n) > 0)){
		
        int nameLen = strlen(pathname)+strlen(file->d_name)+2; //  '\0'+'\'
        char absoluteName[nameLen];
        //concatenazione directory e filename per ottenere un path assoluto
        strncpy(absoluteName,pathname,nameLen-1);
        strncat(absoluteName,"/",nameLen-1);
        strncat(absoluteName,file->d_name,nameLen-1);




        if(isPointDir(file->d_name) == 0){ //non è una point-dir
            if(file->d_type == DT_DIR){
                //if(n != 0)
				recursiveVisit(absoluteName,n,limited,dirname); //chiamata ricorsiva
				if(errno){
					errno = 0;
					return 1;
				}
					

			}else if(file->d_type == DT_REG){
                if(openFile(absoluteName,O_CREATE | O_LOCK) == 0){ //file creato sul server
					//adesso bisogna inviare al server il contenuto del file
					if(writeFile(absoluteName,dirname) == 0){
						if(limited)
							(*n)--;
					}else if(errno){
						closedir(d);
						perror("writeFile");
						return 1;
					}	
					
				}else if(errno){
					closedir(d);
					perror("openFile");
					return 1;
				}
			}
        }
    }
    


    closedir(d);

	return 0;

}
int writeHandler(char opt,char* args,char* dirname,struct timespec* reqTime){

	char* save;
	char* path;

	
	if(opt == 'w'){
		long n = 0;
		if(!(path = strtok_r(args,",",&save))){
			errno = EINVAL;
			perror("dirname");
			return 1;
		}
		char* pathname = realpath(path,NULL);
		if(!pathname){
			free(path);
			perror("realpath");
			return 1;
		}
		if(!isDirectory(pathname)){
			if(PRINTS)
				fprintf(stderr,"%s non e' una directory\n",pathname);
			free(pathname);
			return 1;
		}

		char* nfile;
		if((nfile = strtok_r(NULL,"n=",&save))){
			if(isNumber(nfile,(long*)&n) != 0){
				free(pathname);
				errno = EINVAL;
				perror("n=");
				return 1;
			}
		}
		//free(path);

		//printf("scrivo dir: %s n file = %d\n",pathname,n);
		int limited = 0;
		if(n > 0)
			limited = 1;

		
		int ret = recursiveVisit(pathname,&n,limited,dirname);
		free(pathname);

		nanosleep(reqTime,NULL);
		if(ret == 1){
			return 1;
		}
		
		// openFile(args,O_CREATE | O_LOCK);
	}else{ // opt = W
		char* pathname;
		path = strtok_r(args,",",&save);

		while(path){
			pathname =  realpath(path,NULL);
			if(!pathname){
				free(pathname);
				perror("realpath");
				return 1;
			}
			//controllo che sia effettivamente un file regolare
			if(!isRegularFile(pathname)){
				free(pathname);
				return 1;
			}

			if(openFile(pathname,O_CREATE | O_LOCK) == 0){
					//adesso bisogna inviare al server il contenuto del file
					if(writeFile(pathname,dirname) == 0){

						//closefile
					}else if(errno){
						perror("writeFile");
						free(pathname);
						return 1;
					} 
					
			}else if(errno){
				perror("openFile");
				free(pathname);
				return 1;
			}
			
			free(pathname);
			path = strtok_r(NULL,",",&save);
			nanosleep(reqTime,NULL);
		}
		// free(args);
	}

	return 0;
}


int writeOnDisk(const char* dirname,const char* path, void* buf, size_t size){

	int newLen = strlen(dirname) + strlen(path)+1; // \0  
	char* newPath = calloc(newLen,1);
	ec(newPath,NULL,"calloc",return 1);

	strncpy(newPath,dirname,newLen-1);
    // strncat(newPath,"/",newLen-1);
    strncat(newPath,path,newLen-1);

	char* tmp = strndup(newPath,newLen);
	chk_null(tmp,1);

	for(int i = newLen-1;i >= 0;i--){
		if(tmp[i] == '/'){
			tmp[i] = '\0';
			i = -1;
		}
	}

	char* save = NULL,*token;
	
	char initialDir[UNIX_PATH_MAX];
	getcwd(initialDir,UNIX_PATH_MAX);
	
	token = strtok_r(tmp,"/",&save);

	while(token){
		if(mkdir(token,S_IRWXU) != 0 && errno != EEXIST){
			free(newPath);
			free(tmp);
			return 1;
		}
		chdir(token);
		token = strtok_r(NULL,"/",&save);
	}
	free(tmp);

	chdir(initialDir);

	FILE* fPtr = fopen(newPath,"w+");
	free(newPath);
	fwrite(buf,1,size,fPtr);

	fclose(fPtr);
	return 0;
}



int readHandler(char opt,char* args,char* dirname,struct timespec* reqTime){

	char* save;

	if(opt == 'r'){
		
		char* path;
		path = strtok_r(args,",",&save);

		while(path){
			// openFile senza flags
			if(openFile(path,0) == 0){ //file creato sul server
					//adesso bisogna leggere dal server il file
					void* buf;
					size_t size;
					if(readFile(path,&buf,&size) == 0){
						if(dirname){
							writeOnDisk(dirname,path,buf,size);
						}
						if(buf)
							free(buf);

					}else if(errno){
							perror("readFile");
							return 1;
					} 
					
			}else if(errno){
				perror("openFile");
				return 1;
			} 
			
			//free(pathname);
			path = strtok_r(NULL,",",&save);
			nanosleep(reqTime,NULL);
		}
	}else{ // opt = R
		char* nfile;
		long n = 0;
	
		if(strlen(args) > 0){
			if((nfile = strtok_r(args,"n=",&save))){
				if(isNumber(nfile,(long*)&n) != 0){
					errno = EINVAL;
					perror("n=");
					return 1;
				}
			}
		}
		
	
		// if(dirname){
		// 	dir = realpath(dirname,NULL);
		// 	if(!dir){
		// 		free(dir);
		// 		return 1;
		// 	}
		// }
		
		readNFiles(n,dirname);
		//free(dirname);
		nanosleep(reqTime,NULL);
	}

	return 0;
}





int removeHandler(char* args,struct timespec* reqTime){

	char* save;
	char* path;
	//char* pathname;

	path = strtok_r(args,",",&save);

	while(path){
		//pathname =  realpath(path,NULL);
		if(!path){
			free(path);
			perror("realpath");
			return 1;
		}
		
		if(removeFile(path) == 0){
			//sperror("removeFile");
		}else if(errno){
				perror("removeFile");
				return 1;
		}
		

		//free(path);

		path = strtok_r(NULL,",",&save);
		nanosleep(reqTime,NULL);
	}
		
	return 0;
}




int main(int argc, char* argv[]){

	parseT* parseResult = parser(argc,argv);
	ec(parseResult,NULL,"command parsing",return 1)
	
	if(parseResult->PRINT_ENABLE == 1)
		PRINTS = 1;

	//printParseResult(parseResult);

	if(parseResult->argList->ndata == 0){
		destroyClientParsing(parseResult);
		return 1;
	}

	//come prima cosa bisogna connettersi al server

	struct timespec abstime = {0,time(NULL)+OC_ABS_TIME};

	struct timespec reqTime = {(parseResult->REQ_TIME)/1000,((parseResult->REQ_TIME) % 1000)*1000000};


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

	int finish = 0;
	//ptr per scorrere la lista degli argomenti passati al client
	while(!finish && args->ndata){
		op = dequeue(args);
		nextOp = NULL;
		dirname = NULL;
		errno = 0;
		switch(op->opt){
			case 'w':{
				if(!isQueueEmpty(args)){
					nextOp = args->head->data;
					if(nextOp && nextOp->opt == 'D'){
						removeFromHead(args); //rimuovo il -D
						dirname = nextOp->arg;
					}
				}
				if(writeHandler(op->opt,op->arg,dirname,&reqTime) != 0)
					finish = 1;
			}
			break;

			case 'W':{
				if(!isQueueEmpty(args)){
					nextOp = args->head->data;
					if(nextOp && nextOp->opt == 'D'){
						removeFromHead(args); //rimuovo il -D
						dirname = nextOp->arg;
					}
				}
				if(writeHandler(op->opt,op->arg,dirname,&reqTime) != 0)
					finish = 1;
			}

			break;

			case 'D':{
				fprintf(stderr,"l'opzione -D deve seguire -w/-W\n");
				finish = 1;
			}
			break;

			case 'r':{
				if(!isQueueEmpty(args)){
					nextOp = args->head->data;
					if(nextOp && nextOp->opt == 'd'){
						if((dirname = strndup(nextOp->arg,UNIX_PATH_MAX)) == NULL){
							finish = 1;
							continue;
						}
						removeFromHead(args); //rimuovo il -D
					}
				}
				if(readHandler(op->opt,op->arg,dirname,&reqTime) != 0)
					finish = 1;

			}
			break;

			case 'd':
				fprintf(stderr,"l'opzione -d deve seguire -r/-R\n");
				finish = 1;
			break;

			case 'R':{
				if(!isQueueEmpty(args)){
					nextOp = args->head->data;
					if(nextOp && nextOp->opt == 'd'){
						if((dirname = strndup(nextOp->arg,UNIX_PATH_MAX)) == NULL){
							finish = 1;
							continue;
						}
						removeFromHead(args); //rimuovo il -D
					}
				}
				if(readHandler(op->opt,op->arg,dirname,&reqTime) != 0)
					finish = 1;
			
			}
			break;

			case 'l':
				fprintf(stdout,"lock su %s\n",op->arg);
			break;

			case 'u':
				fprintf(stdout,"unlock su %s\n",op->arg);
			break;

			case 'c':
				removeHandler(op->arg,&reqTime);
				//fprintf(stdout,"rimuovo %s\n",op->arg);
			break;

			default:;
		}
		
		freeOp(op);
		if(dirname)
			free(dirname);
		// free(op->arg);
		// free(op);
	}
	
	

	

	
	
	ec(closeConnection(parseResult->SOCKNAME),-1,"close connection",destroyClientParsing(parseResult);return 1)
	
	
	destroyClientParsing(parseResult);

	return 0;
}