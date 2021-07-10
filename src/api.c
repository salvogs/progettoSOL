#include <stdio.h>
#include <string.h> //
#include "../include/utils.h"
#include "../include/api.h"
#include "../include/comPrt.h"


char *realpath(const char *path, char *resolved_path);


int openConnection(const char* sockname, int msec, const struct timespec abstime){
		
	//creazione client socket
	ec(FD_CLIENT=socket(AF_UNIX,SOCK_STREAM,0),-1,"client socket",return -1);

	struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX; //famiglia indirizzo

	
	
	time_t t; //ritorna il tempo trascorso da 00:00:00 UTC, January 1, 1970
	
	struct timespec tsleep = {msec/1000,(msec % 1000)*1000000}; //modulo per overflow campo .tv_nsec
	
	while(connect(FD_CLIENT,(struct sockaddr *)&sa,sizeof(sa)) == -1){
		t = time(NULL); //ritorna il tempo trascorso da 00:00:00 UTC, January 1, 1970

		if(errno == ENOENT){ //socket non esiste ancora
			if(t >= abstime.tv_nsec){
				return -1;
			}
			if(PRINTS == 1)
				fprintf(stdout,"Impossibile stabilire una connessione con il socket. Riprovo tra %d msec\n",msec);
			
		}else{
			return -1;
		}
		
		nanosleep(&tsleep,NULL);
	}

	if(PRINTS == 1)
		fprintf(stdout,"Connessione con il socket stabilita.\n");
	
	
	return 0;
	
}



int closeConnection(const char* sockname){
	if(!sockname){
		errno = EINVAL;
		return -1;
	}

	if(close(FD_CLIENT) == -1)
		return -1;

	if(PRINTS == 1)
		fprintf(stdout,"Connessione chiusa.\n");
		
	return 0;
}


int openFile(const char* pathname, int flags){

	pathname = realpath(pathname,NULL);
	ec(pathname,NULL,"pathname",return -1);

	// openFile: 	1Byte(operazione)8Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)



	int reqLen = sizeof(char) + sizeof(long) + strlen(pathname) + sizeof(char) +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen+1,1);
	ec(req,NULL,"calloc",return -1);

	snprintf(req,reqLen,"%d%8ld%s%d",OPEN_FILE,strlen(pathname),pathname,flags);
	printf("reqLen: %d\n req: %s\n",reqLen,req);

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		perror("writen");
		free(req);
		return -1;
	}

	free(req);
	char* resBuf = calloc(1,1);
	ec(resBuf,NULL,"calloc",return -1);

	//leggo la risposta (da un byte)
	if(readn(FD_CLIENT,resBuf,1) == -1){
		perror("readn");
		free(req);
		return -1;
	}
	long response;

	if(isNumber(resBuf,&response) != 0){
		return -1;
	}

	if(response == APPOST){
		fprintf(stdout,"tt'appost\n");
		return 0;
	}else{
		fprintf(stderr,"AHIA!\n");
		return -1;
	}
	return 0;
	
}