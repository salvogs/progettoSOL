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
#include <signal.h>
#include "../include/utils.h"
#include "../include/configParser.h"
#include "../include/fs.h"
#include "../include/server.h"
#include "../include/queue.h"
#include "../include/comPrt.h"
#include "../include/serverLogger.h"


#define BUFSIZE 2048
#define DELIM_CONFIG_FILE ":"


pthread_mutex_t request_mux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;



// coda in cui inserire i fd dei client che fanno richieste
queue* requestQueue;

// pipe che servirà ai thread worker per comunicare al master i fd_client da riascoltare
int pfd[2];


char* sockname;
int workerNum;
char* logPath;

fsT* storage;


volatile sig_atomic_t noMoreClient = 0;
volatile sig_atomic_t noMoreRequest = 0;



int clientNum = 0;
int maxClientNum = 0;
pthread_mutex_t clientMux = PTHREAD_MUTEX_INITIALIZER;

void signalHandler(int signum){
	printf("ricevuto %d\n",signum);
	switch(signum){
		case SIGINT:
		case SIGQUIT:
			noMoreRequest = 1;
			break;
		case SIGHUP:
			noMoreClient = 1;
			break;

		default: 
			_exit(EXIT_FAILURE);
	}
	return;
}

char* retToString(int ret){
	char* toRet = NULL;
	switch(ret){
			case SUCCESS:
				toRet =  "SUCCESS";
			break;
			case EMPTY_FILE:
				toRet =  "EMPTY_FILE";
			break;
			case FILE_EXISTS:
				toRet =  "FILE_EXISTS";
			break;
			case FILE_NOT_EXISTS:
				toRet =  "FILE_NOT_EXISTS";
			break;
			case SERVER_ERROR:
				toRet =  "SERVER_ERROR";
			break;
			case BAD_REQUEST:
				toRet =  "BAD_REQUEST";
			break;
			case FILE_TOO_LARGE:
				toRet =  "FILE_TOO_LARGE";
			break;
			case LOCKED:
				toRet =  "LOCKED";
			break;
			case NOT_LOCKED:
				toRet =  "NOT_LOCKED";
			break;
			case NOT_OPENED:
				toRet =  "NOT_OPENED";
			break;
			default:;break;
		}
	
	return toRet; 
}

int main(int argc, char* argv[]){
	//con l'avvio del server deve essere specificato il path del file di config
	if(argc == 1){
		fprintf(stderr,"usage:%s configpath\n",argv[0]);
		return 1;
	}
	
	

	//installo i nuovi signal handler per SIGINT, SIGQUIT, SIGHUP 
	sigset_t set;
	struct sigaction sact;

	//maschero tutti i segnali finchè i gestori permanenti non sono installati
	ec(sigfillset(&set),-1,"sigfillset",return 1)
	ec(pthread_sigmask(SIG_SETMASK,&set,NULL),-1,"pthread_sigmask",return 1)
	memset(&sact,0,sizeof(sact));

	sact.sa_handler = signalHandler;
	ec(sigaction(SIGINT,&sact,NULL),-1,"sigaction",return 1)
	ec(sigaction(SIGQUIT,&sact,NULL),-1,"sigaction",return 1)
	ec(sigaction(SIGHUP,&sact,NULL),-1,"sigaction",return 1)


	//tolgo la maschera
	ec(sigemptyset(&set),-1,"sigemptyset",return 1)
	ec(pthread_sigmask(SIG_SETMASK,&set,NULL),-1,"pthread_sigmask",return 1)
	
	/*
		effettuo il parsing del il file di configurazione 
	*/
	parseT* config = parseConfig(argv[1],DELIM_CONFIG_FILE);
	chk_null(config,1);

	sockname = config->sockname;
	logPath = config->logPath;
	workerNum = config->workerNum;

	if((storage = create_fileStorage(config->maxCapacity,config->maxFileNum)) == NULL)
		return 1;
	

	printf("MAXCAPACITY: %ld, MAXFILENUM: %d "
		 "WORKERNUM: %d SOCKNAME: %s LOGPATH: %s\n"\
		 ,storage->maxCapacity,storage->maxFileNum,workerNum,sockname,logPath);


	ec(requestQueue = createQueue(NULL,NULL),NULL,"requestQueue create",return 1);
	
	ec(pipe(pfd),-1,"pipe create",return 1)
	

	pthread_t tidLogger;
	chk_neg1(logCreate(logPath),SERVER_ERROR);

	CREA_THREAD(&tidLogger,NULL,loggerFun,(void*)logPath)

	logPrint("==AVVIO SERVER==",NULL,0,0,NULL);

	// spawn thread worker sempre in attesa di servire client
	
	pthread_t* tid; //array dove mantenere tid dei worker
	tid = spawnWorker(workerNum);
	
	/*
		accetto richieste da parte dei client
		e le faccio servire dai thread worker
	*/
	// funzione eseguita dal thread main (dispatcher)
	chk_neg1(masterFun(),EXIT_FAILURE);


	logPrint("==ARRESTO SERVER==",NULL,0,0,NULL);
	/* mando messaggio di terminazione (NULL) ai workers
	(tanti quanti sono i workers) */

	for(int i = 0; i < workerNum; i++){
		LOCK(&request_mux)
		ec(enqueue(requestQueue,NULL),1,"enqueue",return 1)
		SIGNAL(&cond);
		UNLOCK(&request_mux);
	}
	// join workers
	for(int i = 0; i < workerNum; i++){
		pthread_join(tid[i],NULL);
		// logPrint("JOIN THREAD",NULL,tid[i],0,NULL);
	}

	
	
	destroyQueue(requestQueue,0);
	close(pfd[0]);
    close(pfd[1]);
	unlink(sockname);
	free(tid);
	destroyConfiguration(config);
	
	// stampo sunto operazioni
	print_final_info(storage,maxClientNum);

	logDestroy();
	
	// join thread logger
	pthread_join(tidLogger,NULL);

	destroy_fileStorage(storage);
	

	return 0;
}





int masterFun(){

	int fd_skt, fd_num = 0,fd;
	// creazione endpoint
	ec(fd_skt=socket(AF_UNIX,SOCK_STREAM,0),-1,"server socket",return 1);
	// inizializzo campi necessari alla chiamata di bind
	(void) unlink(sockname);
	struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
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
	

	FD_SET(pfd[0],&set);
	
	
	while(!noMoreRequest){
		//usiamo la select per evitare che le read e le accept si blocchino
		
		//ad ogni iterazione set cambia, è opportuno farne una copia
		read_set = set;
		//vedo quali descrittori sono attivi fino a fd_num+1
		if(select(fd_num+1,&read_set,NULL,NULL,NULL) == -1){
			if(errno != EINTR){
				perror("select");
				return 1;
			}
			// errno == EINTR (arrivato segnale)
			
			// SIGHUP (non accetto piu'nuove connessioni)
			if(noMoreClient){
				puts("no more client");
				FD_CLR(fd_skt,&set);

				//se era il max fd allora decremento il max
				if(fd_skt == fd_num)
					fd_num--;
				close(fd_skt);

				LOCK(&(clientMux))
				if(clientNum == 0){
					UNLOCK(&(clientMux))
					break;
				}
				UNLOCK(&(clientMux))
			}
			continue;
		}
		//guardo tutti i fd della maschera di bit read_set
		for(fd = 0; fd <=fd_num; fd++){
			if(FD_ISSET(fd,&read_set)){
				//se è proprio fd_sdk faccio la accept che NON SI BLOCCHERÀ
				if(fd == fd_skt){
					ec(fd_client = accept(fd_skt,NULL,0),-1,"server accept",return 1);
					//fprintf(stdout,"client connesso %d\n",fd_client);
					logPrint("CLIENT CONNESSO",NULL,fd_client,0,NULL);

					LOCK(&(clientMux))
					clientNum++;
					if(clientNum > maxClientNum)
						maxClientNum = clientNum;

					UNLOCK(&(clientMux))
					//nella mascheraa set metto a 1 il bit della fd_client(ora è attivo)
					FD_SET(fd_client,&set);
					//tengo sempre aggiornato il descrittore con indice massimo
					if(fd_client > fd_num)
						fd_num = fd_client;

				}else if(fd == pfd[0]){
					
					read(fd,&fd_client,sizeof(int));
					
					
					if(fd_client == 0) // ultimo client disconnesso
						return 0;
						// break; 
					
					FD_SET(fd_client,&set);

					if(fd_client > fd_num)
						fd_num = fd_client;
				}else{
					//metto in coda il fd del client che ha mandato qualcosa (da gestire da un worker)
					LOCK(&request_mux)
					ec(enqueue(requestQueue,CAST_FD(fd)),1,"enqueue",return 1)
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

	// dispatcher termina
	return 0;
}

void* workerFun(){
	int op = 0,ret = 0;
		
	char reqBuf[OP_REQUEST_SIZE] = "";

	char *pathname = NULL;

	queue* ejected = createQueue(freeFile,cmpFile);
	chk_null(ejected,NULL);
	queue* fdPending = createQueue(free,NULL);
	chk_null(fdPending,NULL);
	
	while(1){
		
		LOCK(&request_mux);
		while(isQueueEmpty(requestQueue))
			WAIT(&cond,&request_mux);
		//fprintf(stdout,"sono il thread %ld\n", pthread_self());
		// printQueueInt(requestQueue);

		void* fd_ = dequeue(requestQueue);
		if(!isQueueEmpty(requestQueue))
			SIGNAL(&cond)
		UNLOCK(&request_mux);

		if(fd_ == NULL) break; // messaggio terminazione

		int fd = (long)fd_;

			
		//memset(reqBuf,'\0',OP_REQUEST_SIZE);
		
		//leggo operazione
		
		ret = readn(fd,reqBuf,OP_REQUEST_SIZE);
		if(ret == 0){
			clientExit(fd);
			continue;
		}
		if(ret == -1){
			perror("op read");
			return NULL;
		}
		
		op = reqBuf[0] - '0';
		
	
		switch(op){
			case OPEN_FILE:{
				// openFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)
				die_on_se(getPathname(fd,&pathname))

				int flags = 0;

				//leggo i flags
				die_on_se(getFlags(fd, &flags))

				ret = open_file(storage,fd,pathname,flags,fdPending);				
				
				logPrint((IS_O_LOCK_SET(flags)) ? "OPEN-LOCK FILE" : "OPEN FILE",pathname,fd,0,retToString(ret));

				if(!flags || ret != SUCCESS)
					free(pathname);
				
			}	
			
			break;
			
			case CLOSE_FILE:{

				// leggo il pathname del file da chiudere
				die_on_se(getPathname(fd,&pathname))

				ret = close_file(storage,fd,pathname);
				
				logPrint("CLOSE FILE",pathname,fd,0,retToString(ret));
				
			}
			break;

			case WRITE_FILE:
			case APPEND_TO_FILE:
			{
				// leggo il pathname del file da scrivere
				die_on_se(getPathname(fd,&pathname))
					
				size_t size = 0;
				void* content = NULL;

				// leggo contentuto file
				die_on_se(getFile(fd,&size,&content))
			


				ret = write_append_file(storage,fd,pathname,size,content,ejected,fdPending); 
			
				free(content);


				// mando i file espulsi
				while(ejected->ndata){
					fT* ef = dequeue(ejected);	
					die_on_se(sendFile(fd,ef->pathname,ef->size,ef->content))

					free(ef->pathname);
					freeFile(ef);
				}
				
				(op == WRITE_FILE) ? logPrint("WRITE FILE",pathname,fd,size,retToString(ret)) : logPrint("APPEND TO FILE",pathname,fd,size,retToString(ret));
				
				
			}
			break;

			case READ_FILE:{
				// leggo il pathname del file da leggere
				die_on_se(getPathname(fd,&pathname))
					

				size_t size = 0;
				void* content = NULL;

				ret = read_file(storage,fd,pathname,&size,&content); 
				
				if(ret == SUCCESS){

					// mando SENDING_FILE + size + file

					if(sendFile(fd,NULL,size,content) != 0){
						return NULL;
					}

					free(content);
				}else{
					// invio risposta al client (errore)
					die_on_se(sendResponseCode(fd,ret))
				}
				logPrint("READ FILE",pathname,fd,size,retToString(ret));
			}
	
			break;

			
			case REMOVE_FILE:{
				
				// leggo il pathname del file da rimuovere
				die_on_se(getPathname(fd,&pathname))
					
				
				ret = remove_file(storage,fd,pathname,fdPending);
				logPrint("REMOVE FILE",pathname,fd,0,retToString(ret));
			}
			break;

		
			case READ_N_FILE:{

				int n = 0;
				die_on_se(getN(fd,&n))
				

				die_on_se(ret = read_n_file(storage,n,fd,ejected))
				
				int sizeSum = 0;
				// mando i file 
				while(ejected->ndata){
					fT* ef = dequeue(ejected);	
					if(sendFile(fd,ef->pathname,ef->size,ef->content) != 0){
						return NULL;
					}
					sizeSum += ef->size;
					free(ef->pathname);
					freeFile(ef);

				}
				logPrint("READ N FILE",pathname,fd,sizeSum,retToString(ret));
			}
			break;

			case LOCK_FILE:{
				// leggo il pathname del file da rimuovere
				die_on_se(getPathname(fd,&pathname))


				ret = lock_file(storage,fd,pathname); 
				// se ret == LOCKED lascio il client in attesa

				if(ret != LOCKED)
					die_on_se(sendResponseCode(fd,ret))
					
				logPrint("LOCK FILE",pathname,fd,0,retToString(ret));
				
			}
			break;

			case UNLOCK_FILE:{
				// leggo il pathname del file da rimuovere
				die_on_se(getPathname(fd,&pathname))
				
				int newFdLock = 0;

				die_on_se(ret = unlock_file(storage,fd,pathname,&newFdLock))
				
				// notifico eventuale client in attesa che adesso ha la lock sul file
				if(newFdLock){
					die_on_se(sendResponseCode(newFdLock,SUCCESS))
				}

				
				logPrint("UNLOCK FILE",pathname,fd,0,retToString(ret));
			}
			break;

			default:{
				// invio risposta al client
				die_on_se(sendResponseCode(fd,BAD_REQUEST))
			}
		}

		if(pathname && op != OPEN_FILE){
			free(pathname);
		}
		pathname = NULL;


		if(op != READ_FILE && op != LOCK_FILE){
			// invio risposta al client
			die_on_se(sendResponseCode(fd,ret))
		}

		// notifico i client che erano in attesa di acquisire lock
		while(fdPending->ndata){
			die_on_se(sendResponseCode((long)dequeue(fdPending),FILE_NOT_EXISTS))
		}

		// scrivo fd sulla pipe in comune col dispatcher
		ec(write(pfd[1],&fd,sizeof(int)),-1,"write pipe",exit(EXIT_FAILURE));
	}

	destroyQueue(ejected,1);
	destroyQueue(fdPending,0);

	return NULL;
}


int clientExit(int fd){

	queue* fdPending = createQueue(free,NULL);
	chk_null(fdPending,1);

	die_on_se(remove_client(storage,fd,fdPending));

	// notifico il client che erano in attesa di acquisire lock
	while(fdPending->ndata){
		die_on_se(sendResponseCode((long)dequeue(fdPending),FILE_NOT_EXISTS))
	}


	//fprintf(stdout,"client %d disconnesso\n",fd);
	logPrint("CLIENT DISCONNESSO",NULL,fd,0,NULL);
	LOCK(&(clientMux))
	clientNum--;

	close(fd);

	// se era stato inviato SIGHUP ed fd era l'ultimo client
	if(noMoreClient && !clientNum){ 
		// messaggio che fa capire al dispatcher che adesso puo' terminare
		int ret = 0; 
		ec(write(pfd[1],&ret,sizeof(int)),-1,"write pipe",exit(EXIT_FAILURE));
	}
	UNLOCK(&(clientMux))

	destroyQueue(fdPending,0);



	return 0;
}



pthread_t* spawnWorker(int n){
	pthread_t* tid = malloc(sizeof(pthread_t)*n);
	ec(tid,NULL,"malloc",return NULL)

	for(int i = 0; i < n; i++){
		CREA_THREAD(&tid[i],NULL,workerFun,NULL)
		// logPrint("SPAWN WORKER",NULL,tid[i],0,NULL);
	}

	return tid;
}





int getPathname(int fd,char** pathname){
	// leggo lunghezza pathname
	char* _pathLen = calloc(PATHNAME_LEN+1,1);
	chk_null(_pathLen,SERVER_ERROR)
	
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



int sendResponseCode(int fd,int res){
	
	char resBuf[RESPONSE_CODE_SIZE+1] = "";

	snprintf(resBuf,RESPONSE_CODE_SIZE+1,"%2d",res);

	if(writen(fd,&resBuf,RESPONSE_CODE_SIZE) == -1){
		if(errno && errno != EPIPE && errno != ECONNRESET && errno != EBADF){
			perror("response code writen");
			return SERVER_ERROR;
		}
	}
	
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

			snprintf(res,resLen,"%2d%4d%s",EMPTY_FILE,(int)strlen(pathname),pathname);
		}else{
			resLen +=  MAX_FILESIZE_LEN + size;
			res = calloc(resLen,1);
			chk_null(res,SERVER_ERROR);


			snprintf(res,resLen,"%2d%4d%s%010ld",SENDING_FILE,(int)strlen(pathname),pathname,size);
			memcpy((res + strlen(res)),content,size);
		}

	}else{
		// mando solo il file

		resLen += MAX_FILESIZE_LEN + size;
		res = calloc(resLen,1);
		chk_null(res,SERVER_ERROR);

		snprintf(res,resLen,"%2d%010ld",SENDING_FILE,size); // fsize 10 char max
		memcpy((res + strlen(res)),content,size);
	}
	if(writen(fd,res,resLen-1) == -1){
		if(errno && errno != EPIPE && errno != ECONNRESET && errno == EBADF){
			perror("send file writen");
			printf("errno %d\n",errno);
			free(res);
			return SERVER_ERROR;
		}
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
	void* content_ = NULL;

	if(isNumber(fileSize,(long*)&size_) != 0){
		free(fileSize);
		return BAD_REQUEST;
	}
	free(fileSize);

	if(size_){

		content_ = (void*) malloc(size_);
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
	}



	*size = size_;
	*content = content_;

	return 0;
	
}

