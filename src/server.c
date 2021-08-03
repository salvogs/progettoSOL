#define _GNU_SOURCE

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>     
#include <sys/socket.h>
#include <sys/un.h>/* struttura che rappresenta un indirizzo */
#include <unistd.h>
#include <ctype.h>
#include <sys/select.h>
#include "../include/utils.h"
#include "../include/configParser.h"
#include "../include/fs.h"
#include "../include/server.h"
#include "../include/queue.h"
#include "../include/comPrt.h"



#define BUFSIZE 2048
#define BUFPIPESIZE 4
#define DELIM_CONFIG_FILE ":"


pthread_mutex_t request_mux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


// coda in cui inserire i fd dei client che fanno richieste
queue* requestQueue;

// pipe che servirà ai thread worker per comunicare al master i fd_client da riascoltare
int pfd[2];



fsT* storage;

int masterFun(){

	int fd_skt, fd_num = 0,fd;
	// creazione endpoint
	ec(fd_skt=socket(AF_UNIX,SOCK_STREAM,0),-1,"server socket",return 1);
	// inizializzo campi necessari alla chiamata di bind
	(void) unlink(storage->SOCKNAME);
	struct sockaddr_un sa;
	strncpy(sa.sun_path, storage->SOCKNAME,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX; //famiglia indirizzo
	
	//assegno un indirizzo a un socket
	ec(bind(fd_skt,(struct sockaddr *)&sa,sizeof(sa)),-1,"server skt bind",return 1);
	
	//segnalo che il socket accetta connessioni
	ec(listen(fd_skt,SOMAXCONN),-1,"server listen",return 1);


	fd_set set,read_set;
	//mantengo il massimo indice di descrittore attivo in fd_num
	if(fd_skt > fd_num)
		fd_num = fd_skt;

	FD_ZERO(&set);
	FD_SET(fd_skt,&set);

	int fd_client;
	

	// char buf[BUFSIZE];

	

	FD_SET(pfd[0],&set);
	
	char bufPipe[BUFPIPESIZE];

	
	while(1){
		//usiamo la select per evitare che le read e le accept si blocchino
		
		//ad ogni iterazione set cambia, è opportuno farne una copia
		read_set = set;
		//vedo quali descrittori sono attivi fino a fd_num+1
		ec(select(fd_num+1,&read_set,NULL,NULL,NULL),-1,"select",return 1)

		
		//guardo tutti i fd della maschera di bit read_set
		for(fd = 0; fd <=fd_num; fd++){
			if(FD_ISSET(fd,&read_set)){
				//se è proprio fd_sdk faccio la accept che NON SI BLOCCHERÀ
				if(fd == fd_skt){
					ec(fd_client = accept(fd_skt,NULL,0),-1,"server accept",return 1);
					fprintf(stdout,"client connesso %d\n",fd_client);
					
					//nella mascheraa set metto a 1 il bit della fd_client(ora è attivo)
					FD_SET(fd_client,&set);
					//tengo sempre aggiornato il descrittore con indice massimo
					if(fd_client > fd_num)
						fd_num = fd_client;

				}else if(fd == pfd[0]){
					read(fd,bufPipe,BUFPIPESIZE);
					if((fd_client = atoi(bufPipe)) == 0)
						return 1;
					
					FD_SET(fd_client,&set);

					if(fd_client > fd_num)
						fd_num = fd_client;
				}else{
					//metto in coda il fd del client che ha mandato qualcosa (da gestire da un worker)
					LOCK(&request_mux)
					ec(enqueue(requestQueue,(void*)(long)fd),1,"enqueue",return 1)
					SIGNAL(&cond);
					UNLOCK(&request_mux);

					//fd non è più compito del master
					FD_CLR(fd,&set);
					//se era il max fd allora decremento il max
					if(fd == fd_num)
						fd_num--;
				}
			}
		}
	}
}

void* workerFun(){
	int ret,end = 0;
	
	char bufPipe[BUFPIPESIZE] = "";
	
	char reqBuf[OP_REQUEST_SIZE] = "";

	char resBuf[RESPONSE_CODE_SIZE+1] = "";

	queue* ejected = NULL;
	chk_null(ejected = createQueue(freeFile,cmpFile),NULL);

	while(!end){
		LOCK(&request_mux);
		while(isQueueEmpty(requestQueue))
			WAIT(&cond,&request_mux);
		fprintf(stdout,"sono il thread %ld\n", pthread_self());
		// printQueueInt(requestQueue);

		int fd = (long)dequeue(requestQueue);
		if(!isQueueEmpty(requestQueue))
			SIGNAL(&cond)
		UNLOCK(&request_mux);


		// if(fd == 1)
		// 	exit(EXIT_FAILURE);
		
		
		//memset(reqBuf,'\0',BUFSIZE);
		
		//leggo operazione
		
		ret = readn(fd,reqBuf,OP_REQUEST_SIZE);
		if(ret == 0){
			clientExit(fd);
			continue;
		}
		if(ret == -1){
			perror("op read");
			continue;
		}
		
	
		
		

		int op = reqBuf[0] - '0';
		// printf("operazione: %d\n dim: %s\n",op,buf+1);
		// printf("operazione: %d\n",op);
		
		//long reqLen;
		
	
		switch(op){
			case OPEN_FILE:{
				// openFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)
				// isNumber(buf+1,&reqLen); // reqLen = lunghezza pathname
				
			
				//printf("prima di openFile %d\n",storage->fileNum);
				char* pathname = NULL;
				// leggo il pathname del file da rimuovere
				if(getPathname(fd,&pathname) != 0){
					return NULL;
				}

				int flags = 0;

				//leggo i flags
				if((getFlags(fd, &flags)) != 0){
					free(pathname);
					return NULL;
				}

				ret = open_file(storage,fd,pathname,flags);				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("open file");
					}
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);
					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}

				//printf("dopo di openFile %d\n",storage->fileNum);
			
			}	
			
			break;
			
			case CLOSE_FILE:{
				
				char* pathname = NULL;
				// leggo il pathname del file da chiudere
				if(getPathname(fd,&pathname) != 0){
					return NULL;
				}
	
				ret = close_file(storage,fd,pathname);
				
				if(ret != -1){			
					if(ret == SERVER_ERROR){
						perror("close file");
					}

					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			case WRITE_FILE:
			case APPEND_TO_FILE:
			{
				char* pathname = NULL;
	
				// leggo il pathname del file da scrivere
				if((getPathname(fd,&pathname)) != 0){
					return NULL;
				}

				size_t size = 0;
				void* content = NULL;

				// leggo contentuto file
				if(getFile(fd,&size,&content) != 0){
					return NULL;
}


				ret = write_append_file(storage,fd,pathname,size,content,ejected); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR && errno){
						perror("write/append file");
					}	

					// mando i file espulsi
					while(ejected->ndata){
						fT* ef = dequeue(ejected);	
						if(sendFile(fd,ef->pathname,ef->size,ef->content) != 0){
							return NULL;
						}
						freeFile(ef);

					}
					
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			// case APPEND_TO_FILE:{
			// 	//ret = write_append_file(storage,fd,ejected); 
				
			// 	if(ret != -1){
			// 		if(ret == SERVER_ERROR){
			// 			perror("append to file");
			// 		}			
			// 		snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

			// 		// invio risposta al client
			// 		ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
			// 	}
				
			// }
			// break;

			case READ_FILE:{
				int ret = 0;
				char* pathname = NULL;
				// leggo il pathname del file da leggere
				if((ret = getPathname(fd,&pathname)) != 0){
					return NULL;
				}

				size_t size = 0;
				void* content = NULL;

				ret = read_file(storage,fd,pathname,&size,&content); 
				
				if(ret != -1 && ret != SUCCESS){		
					if(ret == SERVER_ERROR){
						perror("read file");
					}	
					
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta(errore) al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}else{
					// mando SENDING_FILE + size + file
	
					if(sendFile(fd,NULL,size,content) != 0){
						return NULL;
					}

					free(content);
				}
				
			}
			break;

			
			case REMOVE_FILE:{
				
				char* pathname = NULL;
				// leggo il pathname del file da rimuovere
				if(getPathname(fd,&pathname) != 0){
					return NULL;
				}

				ret = remove_file(storage,fd,pathname);
				
				if(ret != -1){			
					if(ret == SERVER_ERROR){
						perror("remove file");
					}

					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

		
			case READ_N_FILE:{

				int n = 0;
				if(getN(fd,&n) != 0){
					return NULL;
				}


				ret = read_n_file(storage,n,fd,ejected); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("read n file");
					}	

					// mando i file 
					while(ejected->ndata){
						fT* ef = dequeue(ejected);	
						if(sendFile(fd,ef->pathname,ef->size,ef->content) != 0){
							return NULL;
						}
						freeFile(ef);

					}

					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			case LOCK_FILE:{
				ret = lock_file(storage,fd); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("lock file");
					}			
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			case UNLOCK_FILE:{
				//ret = unlock_file(storage,fd); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("unlock file");
					}			
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			default:{
				ret = BAD_REQUEST;
				snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);
				// invio risposta al client
				ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL);
			}
		}
	
		sprintf(bufPipe,"%d",fd);
		write(pfd[1],bufPipe,4);
	}
	return NULL;
}

pthread_t* spawnThread(int n){
	pthread_t* tid = malloc(sizeof(pthread_t)*n);
	ec(tid,NULL,"malloc",return NULL)

	for(int i = 0; i < n; i++){
		CREA_THREAD(&tid[i],NULL,workerFun,NULL)
	}

	return tid;
}


int main(int argc, char* argv[]){
	//con l'avvio del server deve essere specificato il path del file di config
	if(argc == 1){
		fprintf(stderr,"usage:%s configpath\n",argv[0]);
		return 1;
	}
	
	
	if((storage = create_fileStorage(argv[1],DELIM_CONFIG_FILE)) == NULL)
		return 1;
	

	printf("MAXCAPACITY: %ld, MAXFILENUM: %ld"
		 "WORKERNUM %ld, SOCKNAME %s, LOGPATH %s\n"\
		 ,storage->maxCapacity,storage->maxFileNum,storage->workerNum,storage->SOCKNAME,storage->logPath);

	

	ec(requestQueue = createQueue(free,NULL),NULL,"creazione coda",return 1);
	
	ec(pipe(pfd),-1,"creazione pipe",return 1)
	



	// devo spawnare i thread worker sempre in attesa di servire client


	pthread_t *tid; //array dove mantenere tid dei worker
	tid = spawnThread(storage->workerNum);
	
	/*
		accetto richieste da parte dei client
		e le faccio servire dai thread worker
	*/
	
	return masterFun();

	// return 0;
}


int getPathname(int fd,char** pathname){
	// leggo lunghezza pathname
	char* _pathLen = calloc(PATHNAME_LEN+1,1);
	if(!_pathLen)
		return SERVER_ERROR;

	int ret = 0;

	ret = readn(fd,_pathLen,PATHNAME_LEN);
	if(ret == 0){
		clientExit(fd);
		free(_pathLen);
		return -1;
	}
	if(ret == -1){
		free(_pathLen);
		return SERVER_ERROR;
	}

	int pathLen = atoi(_pathLen);

	free(_pathLen);
	
	
	//leggo pathname 
	char* path = calloc(pathLen+1,1);
	chk_null(path,SERVER_ERROR)


	ret = readn(fd,path,pathLen);

	if(ret == 0){
		clientExit(fd);
		free(path);
		return -1;
	}
	if(ret == -1){
		free(path);
		return SERVER_ERROR;
	}	

	*pathname = path;
	
	return 0;
}





// mando al client il file
int sendFile(int fd,char* pathname, size_t size, void* content){

	long resLen = RESPONSE_CODE_SIZE+1;
	char* res = NULL;



	//long resLen = sizeof(char) + sizeof(int) + strlen(fPtr->pathname) + MAX_FILESIZE_LEN + fPtr->size +1;

	if(pathname){ // voglio mandare pathname + file
		resLen += PATHNAME_LEN + strlen(pathname);

		if(size == 0){ // se il file è vuoto mando solo il pathname
			res = calloc(resLen,1);
			chk_null(res,SERVER_ERROR);

			snprintf(res,resLen,"%d%4d%s",EMPTY_FILE,(int)strlen(pathname),pathname);
		}else{
			resLen +=  MAX_FILESIZE_LEN + size;
			res = calloc(resLen,1);
			chk_null(res,SERVER_ERROR);


			snprintf(res,resLen,"%d%4d%s%010ld",SENDING_FILE,(int)strlen(pathname),pathname,size);
			memcpy((res + strlen(res)),content,size);
		}

	}else{
		// mando solo il file

		resLen += MAX_FILESIZE_LEN + size;
		res = calloc(resLen,1);
		chk_null(res,SERVER_ERROR);

		snprintf(res,resLen,"%d%010ld",SENDING_FILE,size); // fsize 10 char max
		memcpy((res + strlen(res)),content,size);
	}
	
	if(writen(fd,res,resLen-1) == -1){
		perror("writen");
		free(res);
		return -1;
	}
	free(res);

	return 0;
}

int getN(int fd, int* n){

	// leggo numero file da leggere
	char* buf = calloc(sizeof(int)+1,1);
	chk_null(buf,SERVER_ERROR)
		
	int ret = 0;

	ret = readn(fd,buf,sizeof(int));
	if(ret == 0){
		clientExit(fd);
		free(buf);
		return -1;
	}
	if(ret == -1){
		free(buf);
		return SERVER_ERROR;
	}

	*n = atoi(buf);

	free(buf);

	return 0;
}

int getFlags(int fd, int *flags){
	int flags_ = 0;
	int ret = readn(fd,&flags_,FLAG_SIZE);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		errno = ECONNRESET;
		return -1;
	}
	*flags = flags_ - '0';

	return 0;
}



int getFile(int fd, size_t* size, void** content){

	char* fileSize = calloc(MAX_FILESIZE_LEN+1,1);
	chk_null(fileSize,SERVER_ERROR)
	//leggo dimensioneFile
	int ret = readn(fd,fileSize,MAX_FILESIZE_LEN);
	if(ret == 0){
		clientExit(fd);
		free(fileSize);
		return -1;
	}
	if(ret == -1){
		free(fileSize);
		return SERVER_ERROR;
	}
  
	size_t size_ = 0;	

	if(isNumber(fileSize,(long*)&size_) != 0){
		free(fileSize);
		return BAD_REQUEST;
	}
	free(fileSize);

	void* content_ = (void*) malloc(size_);
	chk_null(content_,SERVER_ERROR)

	//leggo contenuto file
	ret = readn(fd,content_,size_);

	if(ret == 0){
		errno = ECONNRESET;
		free(content_);
		return -1;
	}
	if(ret == -1){
		free(content_);
		return SERVER_ERROR;
	}


	*size = size_;
	*content = content_;

	return 0;
	
}
