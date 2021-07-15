#include <stdio.h>
#include <string.h> //
#include "../include/utils.h"
#include "../include/api.h"

//rb = response buffer r = intResponse
#define RESPONSE_FROM_SERVER(rb,r) \
	if(readn(FD_CLIENT,rb,1) == -1){\
		perror("readn");\
		return -1;\
	}\
	r = rb[0] - '0';


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
		
			PRINT(fprintf(stdout,"Impossibile stabilire una connessione con il socket. Riprovo tra %d msec\n",msec))
			
		}else{
			return -1;
		}
		
		nanosleep(&tsleep,NULL);
	}

	
	PRINT(fprintf(stdout,"Connessione con il socket stabilita.\n"))
	
	
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

	char* path = realpath(pathname,NULL);
	ec(path,NULL,"pathname",return -1);

	// openFile: 	1Byte(operazione)8Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)



	int reqLen = sizeof(char) + sizeof(long) + strlen(path) + sizeof(char) +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	ec(req,NULL,"calloc",return -1);

	snprintf(req,reqLen,"%d%8ld%s%d",OPEN_FILE,strlen(path),path,flags);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		perror("writen");
		free(req);
		return -1;
	}

	free(req);
	
	char* resBuf = calloc(1,1);
	ec(resBuf,NULL,"calloc",return -1);
	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);
	
	PRINTER("OPEN FILE",path,response)

	free(path);

	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}





int writeFile(const char* pathname, const char* dirname){
	
	struct stat info;
    ec_n(stat(pathname, &info),0,pathname,return -1)

	long fsize = info.st_size;

	//printf("info sizeeee %ld\n",fsize);

	FILE* fptr = fopen(pathname,"r");
	ec(fptr,NULL,"fopen",return -1)


	//leggo il file su un buffer

	void* buf = malloc(fsize);
	ec(buf,NULL,"malloc",return -1);

	if(fread(buf,1,fsize,fptr) != fsize){
		fprintf(stderr,"errore lettura file\n");
		fclose(fptr);
		free(buf);
		return -1;
	}
	fclose(fptr);
	// fseek(fptr, 0L, SEEK_END);
	// fsize = ftell(fptr);

	// printf("ftellllll %ld\n",fsize);

	// writeFile:1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)8Byte(dimensione file)dimensione_fileByte(file vero e proprio)

	int reqLen = sizeof(char) + sizeof(long) + strlen(pathname) + sizeof(long) + fsize +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	ec(req,NULL,"calloc",return -1);

	snprintf(req,reqLen,"%d%8ld%s%8ld",WRITE_FILE,strlen(pathname),pathname,fsize);

	memcpy((req + strlen(req)),buf,fsize);

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		perror("writen");
		free(req);
		return -1;
	}

	free(req);


	free(buf);
	
	char* resBuf = calloc(1,1);
	ec(resBuf,NULL,"calloc",return -1);

	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);

	PRINTER("WRITE FILE",pathname,response)

	if(response != SUCCESS)
		return -1;

	return 0;

}