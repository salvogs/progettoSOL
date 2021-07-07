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
#include "../include/server.h"
#include "../include/queue.h"
#include "../include/icl_hash.h"


#define CREA_THREAD(tid,param,fun,args) if(pthread_create(tid,param,fun,args) != 0){ \
	printf("errore creazione thread\n"); \
	exit(EXIT_FAILURE);}



#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { printf("ERRORE FATALE lock\n"); exit(EXIT_FAILURE);}
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { printf("ERRORE FATALE unlock\n"); exit(EXIT_FAILURE);}
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { printf("ERRORE FATALE wait\n"); exit(EXIT_FAILURE);}
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       { printf("ERRORE FATALE signal\n"); exit(EXIT_FAILURE);}




#define UNIX_PATH_MAX 108 //lunghezza massima indirizzo
#define BUFSIZE 2048


fsT* fsConfig = NULL;

pthread_mutex_t request_mux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;


// coda in cui inserire i fd dei client che fanno richieste
queue* requestQueue;

// pipe che servirà ai thread worker per comunicare al master i fd_client da riascoltare
int pfd[2];





int masterFun(){

	// creazione hash table che ospitera' il file system
	icl_hash_t* ht = icl_hash_create(fsConfig->maxFileNum,hash_pjw,string_compare);
	ec(ht,NULL,"creazione hash table",return 1);
	

	
	int fd_skt, fd_num = 0,fd;
	// creazione endpoint
	ec(fd_skt=socket(AF_UNIX,SOCK_STREAM,0),-1,"server socket",return 1);
	// inizializzo campi necessari alla chiamata di bind
	(void) unlink(fsConfig->SOCKNAME);
	struct sockaddr_un sa;
	strncpy(sa.sun_path, fsConfig->SOCKNAME,UNIX_PATH_MAX);
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

	long fd_client;
	

	// char buf[BUFSIZE];

	

	FD_SET(pfd[0],&set);
	
	

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
					fprintf(stdout,"client connesso %ld\n",fd_client);

					//nella mascheraa set metto a 1 il bit della fd_client(ora è attivo)
					FD_SET(fd_client,&set);
					//tengo sempre aggiornato il descrittore con indice massimo
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
	int end = 0;
	while(!end){
		LOCK(&request_mux);
		printf("sono il thread %ld\n", pthread_self());
		while(isQueueEmpty(requestQueue))
			WAIT(&cond,&request_mux);
		printQueueInt(requestQueue);

		int fd = (long)dequeue(requestQueue);
		if(!isQueueEmpty(requestQueue))
			SIGNAL(&cond)
		UNLOCK(&request_mux);


		if(fd == 1)
			exit(EXIT_FAILURE);
		
		char buf[BUFSIZE];
		read(fd,buf,BUFSIZE);
		// upperString(buf,strlen(buf));
		write(fd,buf,strlen(buf));
		close(fd);
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
	
	/*
		per prima cosa leggo il file di configurazione 
		memorizzando i parametri in una struct 
	*/
	

	fsConfig = parseConfig(argv[1],":");
	if(!fsConfig)
		return 1;
	

	

	printf("MAXCAPACITY: %ld, MAXFILENUM: %ld"
		 "WORKERNUM %ld, SOCKNAME %s, LOGPATH %s\n"\
		 ,fsConfig->maxCapacity,fsConfig->maxFileNum,fsConfig->workerNum,fsConfig->SOCKNAME,fsConfig->logPath);

	

	ec(requestQueue = createQueue(free,NULL),NULL,"creazione coda",return 1);
	
	ec(pipe(pfd),-1,"creazione pipe",return 1)
	



	// devo spawnare i thread worker sempre in attesa di servire client


	pthread_t *tid; //array dove mantenere tid dei worker
	tid = spawnThread(fsConfig->workerNum);
	
	/*
		accetto richieste da parte dei client
		e le faccio servire dai thread worker
	*/
	
	return masterFun();

	// return 0;
}


