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
#include <pthread.h>
#include "../include/utils.h"
#include "../include/configParser.h"
#include "../include/fs.h"
#include "../include/server.h"
#include "../include/queue.h"
#include "../include/comPrt.h"



#define CREA_THREAD(tid,param,fun,args) if(pthread_create(tid,param,fun,args) != 0){ \
	fprintf(stderr,"errore creazione thread\n"); \
	exit(EXIT_FAILURE);}



#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { printf("ERRORE FATALE lock\n"); exit(EXIT_FAILURE);}
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { printf("ERRORE FATALE unlock\n"); exit(EXIT_FAILURE);}
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { printf("ERRORE FATALE wait\n"); exit(EXIT_FAILURE);}
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       { printf("ERRORE FATALE signal\n"); exit(EXIT_FAILURE);}



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
			
				ret = open_file(storage,fd);				
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
			
			case WRITE_FILE:{
				ret = write_append_file(storage,fd,0); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("write file");
					}			
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			case APPEND_TO_FILE:{
				ret = write_append_file(storage,fd,1); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("append to file");
					}			
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			case READ_FILE:{
				ret = read_file(storage,fd); 
				
				if(ret != -1 && ret != SUCCESS){		
					if(ret == SERVER_ERROR){
						perror("read file");
					}	
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta(errore) al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			
			case REMOVE_FILE:{
				ret = remove_file(storage,fd);
				
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
				ret = read_n_file(storage,fd); 
				
				if(ret != -1){
					if(ret == SERVER_ERROR){
						perror("read n file");
					}			
					snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%d",ret);

					// invio risposta al client
					ec(writen(fd,resBuf,RESPONSE_CODE_SIZE),-1,"writen",return NULL)
				}
				
			}
			break;

			default:;
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


