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


#define chk_api(a,op)\
	do{\
		if((a) == -1 && errno != EEXIST && errno != ENOENT && errno != EFBIG && errno != ENOSPC && errno != EACCES){\
			if(errno && errno != ECONNRESET && errno != EPIPE && errno != EBADF)\
				perror(#a);\
			op\
			return -1;\
		}\
	}while(0);

int main(int argc, char* argv[]){

	parseT* parseResult = parser(argc,argv);
	ec(parseResult,NULL,"command parsing",return 1)
	
	if(parseResult->PRINT_ENABLE == 1)
		PRINTS = 1;


	if(parseResult->argList->ndata == 0){
		destroyClientParsing(parseResult);
		return 1;
	}

	//come prima cosa bisogna connettersi al server

	struct timespec abstime = {0,time(NULL)+OC_ABS_TIME};

	struct timespec reqTime = {(parseResult->REQ_TIME)/1000,((parseResult->REQ_TIME) % 1000)*1000000};


	if(openConnection(parseResult->SOCKNAME,OC_RETRY_TIME,abstime) == -1){
		perror("open connection");

		destroyClientParsing(parseResult);
		return 1;
	}
	
	queue* args = parseResult->argList;
	optT *op,*nextOp;

	char* dirname = NULL;

	int finish = 0;

	// scorro ed eseguo lista operazioni
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
						if((dirname = strndup(nextOp->arg,UNIX_PATH_MAX)) == NULL){
							finish = 1;
							continue;
						}
						removeFromHead(args,1); //rimuovo il -D
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
						if((dirname = strndup(nextOp->arg,UNIX_PATH_MAX)) == NULL){
							finish = 1;
							continue;
						}
						removeFromHead(args,1); //rimuovo il -D
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
						removeFromHead(args,1); //rimuovo il -d
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
						removeFromHead(args,1); //rimuovo il -d
					}
				}
				if(readHandler(op->opt,op->arg,dirname,&reqTime) != 0)
					finish = 1;
			
			}
			break;

			case 'l':
				if(lockUnlockHandler(op->opt,op->arg,&reqTime) != 0)
					finish = 1;
			break;

			case 'u':
				if(lockUnlockHandler(op->opt,op->arg,&reqTime) != 0)
					finish = 1;
			break;

			case 'c':
				if(removeHandler(op->arg,&reqTime) != 0)
					finish = 1;
			break;

			default:;
		}
		
		freeOp(op);
		if(dirname)
			free(dirname);
		
	}
	
	

		
	ec(closeConnection(parseResult->SOCKNAME),-1,"close connection",destroyClientParsing(parseResult);return 1)
	
	
	destroyClientParsing(parseResult);

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
			return -1;
		}
		char* pathname = realpath(path,NULL);
		if(!pathname){
			perror("realpath");
			return -1;
		}
		if(!isDirectory(pathname)){
			if(PRINTS)
				fprintf(stderr,"%s non e' una directory\n",pathname);
			free(pathname);
			return -1;
		}

		char* nfile;
		if((nfile = strtok_r(NULL,"n=",&save))){
			if(isNumber(nfile,(long*)&n) != 0){
				free(pathname);
				errno = EINVAL;
				perror("n=");
				return -1;
			}
		}
		
		int limited = 0;
		if(n > 0)
			limited = -1;

		
		int ret = recursiveVisit(pathname,&n,limited,dirname);
		free(pathname);

		if(ret == -1){
			return -1;
		}
		nanosleep(reqTime,NULL);
		
		
	}else{ // opt = W
		char* pathname = NULL;
		size_t size = 0;

		path = strtok_r(args,",",&save);

		while(path){
			pathname = realpath(path,NULL);
			if(!pathname){
				perror("realpath");
				return -1;
			}
			//controllo che sia effettivamente un file regolare
			if(!isRegularFile(pathname)){
				fprintf(stderr,"%s non e' un file regolare\n",pathname);
				free(pathname);
				return -1;
			}

			if(openFile(pathname,O_CREATE | O_LOCK) == 0){
				//adesso bisogna inviare al server il contenuto del file
				chk_api(writeFile(pathname,dirname),free(pathname);)
					
				chk_api(closeFile(pathname),free(pathname);)					
			}else{
		
				if(errno == EEXIST){

					errno = 0;
					// provo ad aprire il file senza flag e a fare la append
					chk_api(openFile(pathname,0),free(pathname);)
					
					void* content = NULL;
					ec_n(readFileFromDisk(pathname,&content,&size),0,"readFileFromDisk",return -1)

					chk_api(appendToFile(pathname,content,size,dirname),if(content){free(content);}free(pathname);)
						
					if(content) free(content);

					chk_api(closeFile(pathname),free(pathname);)
						
				}else{
					if(errno != EEXIST && errno != ENOENT && errno != EFBIG && errno != ENOSPC && errno != EACCES){

						if(errno && errno != ECONNRESET && errno != EPIPE && errno != EBADF){
							perror("openFile");
						}
						free(pathname);
						return -1;
					}
				}
				
			} 
			
			free(pathname);
			path = strtok_r(NULL,",",&save);
			nanosleep(reqTime,NULL);
		}
		
	}

	return 0;
}


int recursiveVisit(char* pathname,long* n,int limited,char* dirname){
	
	DIR *d;
	
    //tento di aprire la directory 
	ec(d=opendir(pathname),NULL,"opendir",return -1)

    
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
				chk_neg1(recursiveVisit(absoluteName,n,limited,dirname),-1) //chiamata ricorsiva
						

			}else if(file->d_type == DT_REG){
                if(openFile(absoluteName,O_CREATE | O_LOCK) == 0){ //file creato sul server
					//adesso bisogna inviare al server il contenuto del file
					chk_api(writeFile(absoluteName,dirname),;)
			
					if(limited)	(*n)--;
						
					chk_api(closeFile(absoluteName),;)
											
				}else{
	
					if(errno == EEXIST){
						
						errno = 0;
						void* content = NULL;
						size_t size = 0;
						// provo ad aprire il file senza flag e a fare la append
						chk_api(openFile(absoluteName,0),;)
						
						ec_n(readFileFromDisk(absoluteName,&content,&size),0,"readFileFromDisk",return -1)

						chk_api(appendToFile(absoluteName,content,size,dirname),free(content);)

						free(content);

						if(limited) (*n)--;
							
						chk_api(closeFile(absoluteName),;)
							
					}else{
						if(errno != EEXIST && errno != ENOENT && errno != EFBIG && errno != ENOSPC && errno != EACCES){

							if(errno && errno != ECONNRESET && errno != EPIPE && errno != EBADF){
								perror("openFile");
							}
							closedir(d);
							return -1;
						}
					}
				} 
			}
		}
    }

    closedir(d);

	return 0;
}



int readHandler(char opt,char* args,char* dirname,struct timespec* reqTime){

	char* save;
	
	if(opt == 'r'){
		
		char* path;
		path = strtok_r(args,",",&save);

		while(path){
			// openFile senza flags
			chk_api(openFile(path,0),;)//file gia' creato sul server
		
			//adesso bisogna leggere dal server il file
			void* buf = NULL;
			size_t size = 0;
			chk_api(readFile(path,&buf,&size),;)
			if(dirname){
				if(writeFileOnDisk(dirname,path,buf,size) != 0){
					return -1;
				}
			}
			if(buf)	free(buf);


			chk_api(closeFile(path),;)		
		

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
					return -1;
				}
			}
		}
		
		
		chk_api(readNFiles(n,dirname),;)
		
		nanosleep(reqTime,NULL);
	}

	return 0;
}

int lockUnlockHandler(char opt,char* args,struct timespec* reqTime){
	char* save;

	char* path;
	path = strtok_r(args,",",&save);

	while(path){
		if(opt == 'l'){
			chk_api(lockFile(path),;)
				
		}else{ // unlock
			chk_api(unlockFile(path),;) 
		}
		
	
		path = strtok_r(NULL,",",&save);
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
		chk_api(removeFile(path),;)
			

		path = strtok_r(NULL,",",&save);
		nanosleep(reqTime,NULL);
	}
		
	return 0;
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

    return -1;
}


