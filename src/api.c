#include <stdio.h>
#include <string.h> //
#include "../include/utils.h"
#include "../include/api.h"

//rb = response buffer r = intResponse
#define RESPONSE_FROM_SERVER(rb,r) \
	int ret = readn(FD_CLIENT,rb,1);\
	if(ret == -1){\
		free(rb);\
		return -1;\
	}\
	if(ret == 0){\
		free(rb);\
		errno = ECONNRESET;\
		return -1;\
	}\
	r = rb[0] - '0';
	//r = rb[0] == '\0' ? rb[0] : rb[0] - '0'; 


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

	// char* path = realpath(pathname,NULL);
	// ec(path,NULL,"pathname",return -1);

	// openFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)1Byte(flags)



	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + sizeof(char) +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)

	snprintf(req,reqLen,"%d%4d%s%d",OPEN_FILE,(int)strlen(pathname),pathname,flags);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		//perror("writen");
		free(req);
		return -1;
	}

	free(req);
	
	char* resBuf = calloc(1,1);
	chk_null(resBuf,-1)
	
	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);
	
	PRINTER("OPEN FILE",pathname,response)

	//free(path);

	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}


int closeFile(const char* pathname){

	// char* path = realpath(pathname,NULL);
	// ec(path,NULL,"pathname",return -1);

	// closeFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)



	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d%s",CLOSE_FILE,(int)strlen(pathname),pathname);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		//perror("writen");
		free(req);
		return -1;
	}

	free(req);
	
	char* resBuf = calloc(1,1);
	chk_null(resBuf,-1)

	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);
	
	PRINTER("CLOSE FILE",pathname,response)

	// free(path);

	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}





int writeFile(const char* pathname, const char* dirname){
	
	struct stat info;
	if(stat(pathname, &info) != 0)
		return -1;
	
   

	long fsize = info.st_size;

	if(fsize == 0){
		//non faccio nemmeno richiesta al server
		PRINTER("WRITE FILE",pathname,EMPTY_FILE)

		if(PRINTS)
			fprintf(stdout,"Scritti: %ld bytes\n",fsize);

		return EMPTY_FILE;
	}
	//printf("info sizeeee %ld\n",fsize);

	FILE* fPtr = fopen(pathname,"r");
	chk_null(fPtr,-1)
	


	//leggo il file su un buffer

	void* buf = malloc(fsize);
	chk_null(buf,-1)
	

	if(fread(buf,1,fsize,fPtr) != fsize){
		//fprintf(stderr,"errore lettura file\n");
		fclose(fPtr);
		free(buf);
		return -1;
	}
	fclose(fPtr);
	
	

	// writeFile:1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)MAX_FILESIZE_LENByte(dimensione file)dimensione_fileByte(file vero e proprio)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + MAX_FILESIZE_LEN + fsize +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)
	

	snprintf(req,reqLen,"%d%4d%s%010ld",WRITE_FILE,(int)strlen(pathname),pathname,fsize); // fsize 10 char max
	
	memcpy((req + strlen(req)),buf,fsize);

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		//perror("writen");
		free(req);
		return -1;
	}
	
	free(req);


	free(buf);
	
	char* resBuf = calloc(1,1);
	chk_null(resBuf,-1)
	

	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);

	PRINTER("WRITE FILE",pathname,response)

	if(PRINTS)
		fprintf(stdout,"Scritti: %ld bytes\n",fsize);



	if(response != SUCCESS)
		return -1;

	return 0;

}


int removeFile(const char* pathname){
	// char* path = realpath(pathname,NULL);
	// ec(path,NULL,"pathname",return -1);

	// removeFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)



	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)

	snprintf(req,reqLen,"%d%4d%s",REMOVE_FILE,(int)strlen(pathname),pathname);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		//perror("writen");
		free(req);
		return -1;
	}

	free(req);
	
	char* resBuf = calloc(1,1);
	chk_null(resBuf,-1)
	
	long response;

	RESPONSE_FROM_SERVER(resBuf,response)
	
	free(resBuf);
	
	PRINTER("REMOVE FILE",pathname,response)

	//free(path);

	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}



int readFile(const char* pathname, void** buf, size_t* size){
	// char* path = realpath(pathname,NULL);
	// ec(path,NULL,"pathname",return -1);

	// readFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)



	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d%s",READ_FILE,(int)strlen(pathname),pathname);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	

	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		//perror("writen");
		free(req);
		return -1;
	}

	free(req);
	
	
	char* resBuf = calloc(1,1);
	chk_null(resBuf,-1)

	long response;

	RESPONSE_FROM_SERVER(resBuf,response)

	free(resBuf);
	
	if(response != SUCCESS && response != EMPTY_FILE){
		PRINTER("READ FILE",pathname,response)
		return -1;
	}

	
	long fileSize;
	void* content;

	if(response == SUCCESS){

		// mi aspetto: size del file letto, file letto

		//prima read di 10byte per size
		
		char* sizeBuf = calloc(MAX_FILESIZE_LEN+1,1);
		chk_null(sizeBuf,-1)
		
		
		int ret = 0;

		ret = readn(FD_CLIENT,sizeBuf,MAX_FILESIZE_LEN);

		if(ret == 0){
			errno = ECONNRESET;
			free(sizeBuf);
			return -1;
		}
		if(ret == -1){
			free(sizeBuf);
			return -1;
		}
		
		
		fileSize = atol(sizeBuf);

		free(sizeBuf);
		// alloco spazio per leggere il file

		content = malloc(fileSize);
		chk_null(content,-1)

		ret = readn(FD_CLIENT,content,fileSize);

		if(ret == 0){
			errno = ECONNRESET;
			free(sizeBuf);
			return -1;
		}
		if(ret == -1){
			free(sizeBuf);
			return -1;
		}






	}else{ //file empty
		fileSize = 0;
		content = NULL;
	}


	*size = fileSize;
	*buf = content;

	PRINTER("READ FILE",pathname,response)

	if(PRINTS)
		fprintf(stdout,"Letti: %ld bytes\n",*size);
	
	
	return 0;


}


// int readNFiles(int N, const char* dirname){
// 	char* dir = realpath(dirname,NULL);
// 	ec(dir,NULL,"realpath",return -1)

// 	// readNFile:	1Byte(operazione)4Byte(N file da leggere)


// 	int reqLen = 
// }