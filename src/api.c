#include "../include/utils.h"
#include "../include/api.h"
#include "../include/client.h"



char *realpath(const char *path, char *resolved_path);


int openConnection(const char* sockname, int msec, const struct timespec abstime){
		
	//creazione client socket
	ec(FD_CLIENT=socket(AF_UNIX,SOCK_STREAM,0),-1,"client socket",return -1);

	struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX; //famiglia indirizzo

	
	
	time_t t; 
	
	struct timespec tsleep = {msec/1000,(msec % 1000)*1000000}; //modulo per overflow campo .tv_nsec
	
	while(connect(FD_CLIENT,(struct sockaddr *)&sa,sizeof(sa)) == -1){
		t = time(NULL); //ritorna il tempo trascorso da 00:00:00 UTC, January 1, 1970

		if(errno == ENOENT){ //socket non esiste ancora
			if(t >= abstime.tv_nsec){
				errno = ETIME;
				return -1;
			}
		
			PRINT(fprintf(stdout,"==%d== Impossibile stabilire una connessione con il socket. Riprovo tra %d msec\n",getpid(),msec))
			
		}else{
			return -1;
		}
		
		nanosleep(&tsleep,NULL);
	}

	
	PRINT(fprintf(stdout,"==%d== Connessione con il socket stabilita.\n",getpid()))
	
	
	return 0;
	
}



int closeConnection(const char* sockname){
	if(!sockname){
		errno = EINVAL;
		return -1;
	}

	chk_neg1(close(FD_CLIENT),-1)

	PRINT(fprintf(stdout,"==%d== Connessione chiusa.\n",getpid()))
		
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
	
	int response = getResponseCode(FD_CLIENT);
	
	PRINTER("OPEN FILE",pathname,response)

	

	if(response != SUCCESS){
		if(response == FILE_EXISTS) errno = EADDRINUSE;
		return -1;
	}
	errno = 0;
	return 0;
	
}


int closeFile(const char* pathname){

	// closeFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)



	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d%s",CLOSE_FILE,(int)strlen(pathname),pathname);
		
		
	if(writen(FD_CLIENT,req,reqLen-1) == -1){
		free(req);
		return -1;
	}

	free(req);
	
	int response = getResponseCode(FD_CLIENT);
	
	PRINTER("CLOSE FILE",pathname,response)


	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}





int writeFile(const char* pathname, const char* dirname){
	 
	size_t fsize =0;
	void* content = NULL;

	chk_neg1(readFileFromDisk(pathname,&content,&fsize),-1)
	
	// writeFile:1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)MAX_FILESIZE_LENByte(dimensione file)dimensione_fileByte(file vero e proprio)
	int reqLen = 0;

	if(fsize == 0)// file vuoto
		reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + MAX_FILESIZE_LEN + 1; 
	else
		reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + MAX_FILESIZE_LEN + fsize + 1; 
	

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d%s%010ld",WRITE_FILE,(int)strlen(pathname),pathname,fsize);
		
	if(fsize){
		memcpy((req + strlen(req)),content,fsize);
		if(content) free(content);
	}

	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)



	
	int response = getResponseCode(FD_CLIENT);

	size_t bytes = 0;
	int readCounter = 0;

	while(response == SENDING_FILE){
		chk_neg1(getEjectedFile(response,dirname,&bytes,&readCounter),-1)
		
		response = getResponseCode(FD_CLIENT);	
	}
	
	
	
	PRINTER("WRITE FILE",pathname,response)
	if(response == SUCCESS){
		PRINT_WRITE(fsize)
		return 0;
	}
	
	return -1;

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	

	if(size == 0){
		//non faccio nemmeno richiesta al server
		PRINTER("APPEND TO FILE",pathname,EMPTY_FILE)

		
		PRINT_WRITE(size)

		return 0;
	}
	// appendToFile:1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)MAX_FILESIZE_LENByte(dimensione file)dimensione_fileByte(file vero e proprio)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + MAX_FILESIZE_LEN + size + 1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)
	

	snprintf(req,reqLen,"%d%4d%s%010ld",APPEND_TO_FILE,(int)strlen(pathname),pathname,size); // fsize 10 char max
	
	memcpy((req + strlen(req)),buf,size);

	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)
	
	
	int response = getResponseCode(FD_CLIENT);

	size_t bytes = 0;;
	int readCounter = 0;

	while(response == SENDING_FILE){
		chk_neg1(getEjectedFile(response,dirname,&bytes,&readCounter),-1)
		
		response = getResponseCode(FD_CLIENT);	
	}
	
	
	PRINTER("APPEND TO FILE",pathname,response)
	if(response == SUCCESS){
		PRINT_WRITE(size)
		return 0;
	}
	
	return -1;
	

}

int removeFile(const char* pathname){
	// removeFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)

	snprintf(req,reqLen,"%d%4d%s",REMOVE_FILE,(int)strlen(pathname),pathname);
	//printf("reqLen: %d\n req: %s\n",reqLen,req);
	
	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)
	

	int response = getResponseCode(FD_CLIENT);
		
	PRINTER("REMOVE FILE",pathname,response)


	if(response != SUCCESS){
		return -1;
	}

	return 0;
	
}


int readFile(const char* pathname, void** buf, size_t* size){
	// readFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d%s",READ_FILE,(int)strlen(pathname),pathname);
	
	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)

	int response = getResponseCode(FD_CLIENT);
	chk_neg1(response,-1)

	
	if(response != SUCCESS && response != SENDING_FILE && response != EMPTY_FILE){
		PRINTER("READ FILE",pathname,response)
		return -1;
	}

	size_t fileSize = 0;
	void* content = NULL;

	// se response == EMPTY_FILE size = 0


	if(response == SENDING_FILE){
		// mi aspetto: size del file letto, file letto
		if(getFile(&fileSize,&content,NULL) != 0)
			return -1;
	}

	*size = fileSize;
	*buf = content;
	
	PRINTER("READ FILE",pathname,SUCCESS)

	PRINT_READ(*size)
	
	
	return 0;


}


int readNFiles(int N, const char* dirname){

	// readNFile:	1Byte(operazione)4Byte(N file da leggere)

	int reqLen = sizeof(char) + sizeof(int) +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d",READ_N_FILE,N);
	
	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)
	
	
	int response = getResponseCode(FD_CLIENT);
	
	if(response != SUCCESS && response != EMPTY_FILE && response != SENDING_FILE){
		//PRINTER("READ FILE",pathname,response)
		
		return -1;
	}

	/* mi aspetto una sequenza di SENDING_FILE/EMPTY_FILE
	quando i file da leggere sono finiti response sara' END_SENDING_FILE
	*/

	size_t bytes = 0;
	int readCounter = 0;

	while(response == SENDING_FILE || response == EMPTY_FILE){
		chk_neg1(getEjectedFile(response,dirname,&bytes,&readCounter),-1)
		
		response = getResponseCode(FD_CLIENT);	
	}
	
	
	if(response == SUCCESS){
		PRINT(fprintf(stdout,"==%d== Letti: %d files e %ld bytes\n",getpid(),readCounter,bytes))
		return readCounter;
	}else{
		return -1;
	}


}



int sendRequest(int fd, void* req, int len){
	if(writen(FD_CLIENT,req,len) == -1){
		//perror("writen");
		free(req);
		return -1;
	}
	free(req);
	return 0;
}

int getResponseCode(int fd){
	char res[RESPONSE_CODE_SIZE+1] = "";

	int ret = readn(FD_CLIENT,&res,RESPONSE_CODE_SIZE);\
	
	if(ret == -1){\
		return -1;\
	}\
	if(ret == 0){\
		errno = ECONNRESET;\
		return -1;\
	}\
	// return res - '0';
	return atoi(res);
}


int getPathname(char** pathname){
	// leggo lunghezza pathname
	char* _pathLen = calloc(PATHNAME_LEN+1,1);
	if(!_pathLen)
		return SERVER_ERROR;

	int ret = 0;

	ret = readn(FD_CLIENT,_pathLen,PATHNAME_LEN);
	if(ret == 0){
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


	ret = readn(FD_CLIENT,path,pathLen);

	if(ret == 0){
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


int getFile(size_t* size,void** content, char** pathname){
	
	char* path;
	int ret = 0;
	//se pathname passato allora voglio leggere anche pathname
	if(pathname){
		// leggo lunghezza pathname
		if(getPathname(&path) != 0)
			return -1;
	}

	
	char* sizeBuf = calloc(MAX_FILESIZE_LEN+1,1);
	chk_null(sizeBuf,-1)
	
	int size_;
	void* content_;

	//prima read di 10byte per size

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
	
	
	size_ = atol(sizeBuf);

	free(sizeBuf);
	// alloco spazio per leggere il file

	content_ = malloc(size_);
	chk_null(content_,-1)

	//leggo contenuto file
	ret = readn(FD_CLIENT,content_,size_);

	if(ret == 0){
		errno = ECONNRESET;
		free(content_);
		return -1;
	}
	if(ret == -1){
		free(content_);
		return -1;
	}

	if(pathname) 
		*pathname = path;

	*size = size_;
	*content = content_;

	return 0;
}

int getEjectedFile(int response, const char* dirname, size_t* bytes, int* readCounter){
	void* content = NULL;
	char* pathname = NULL;
	size_t size = 0;

	if(response == EMPTY_FILE){
		if(getPathname(&pathname) != 0)
			return -1;
	}else{
		if(getFile(&size,&content,&pathname) != 0)
			return -1;
	}
	PRINT(fprintf(stdout,"==%d== RICEVUTO : %s : %ld bytes\n",getpid(),pathname,size))

	if(dirname){
		if(writeFileOnDisk(dirname,pathname,content,size) != 0){
			if(content) free(content);
			free(pathname);
			return -1;
		}
	}
	free(pathname);
	if(content) 
		free(content);
	
	*bytes += size;
	(*readCounter)++;

	return 0;
}


int lockFile(const char* pathname){
	// lockFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)

	snprintf(req,reqLen,"%d%4d%s",LOCK_FILE,(int)strlen(pathname),pathname);
	
	
	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)
	

	int response = getResponseCode(FD_CLIENT);
		
	PRINTER("LOCK FILE",pathname,response)


	if(response != SUCCESS){
		return -1;
	}

	return 0;
}

int unlockFile(const char* pathname){
	// unlockFile: 	1Byte(operazione)4Byte(lunghezza pathname)lunghezza_pathnameByte(pathname)

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname)+1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)

	snprintf(req,reqLen,"%d%4d%s",UNLOCK_FILE,(int)strlen(pathname),pathname);
	
	
	chk_neg1(sendRequest(FD_CLIENT,req,reqLen-1),-1)
	

	int response = getResponseCode(FD_CLIENT);
		
	PRINTER("UNLOCK FILE",pathname,response)


	if(response != SUCCESS){
		return -1;
	}

	return 0;
}