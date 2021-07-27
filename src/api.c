#include <stdio.h>
#include <string.h> //
#include "../include/utils.h"
#include "../include/api.h"
#include "../include/client.h"

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

	
	
	time_t t; 
	
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

	int reqLen = sizeof(char) + sizeof(int) + strlen(pathname) + MAX_FILESIZE_LEN + fsize + 1; //+1 finale percheè snprintf include anche il \0

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

	
	
	size_t fileSize = 0;
	void* content = NULL;

	if(response == SUCCESS){

		// mi aspetto: size del file letto, file letto
		if(getFile(&fileSize,&content,NULL) != 0)
			return -1;
	}


	*size = fileSize;
	*buf = content;
	
	PRINTER("READ FILE",pathname,response)

	if(PRINTS)
		fprintf(stdout,"Letti: %ld bytes\n",*size);
	
	
	return 0;


}


int readNFiles(int N, const char* dirname){

	// readNFile:	1Byte(operazione)4Byte(N file da leggere)

	int reqLen = sizeof(char) + sizeof(int) +1; //+1 finale percheè snprintf include anche il \0

	char* req = calloc(reqLen,1);
	chk_null(req,-1)


	snprintf(req,reqLen,"%d%4d",READ_N_FILE,N);
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
	
	if(response != SUCCESS && response != EMPTY_FILE && response != FILE_LIST && response != END_FILE){
		//PRINTER("READ FILE",pathname,response)
		
		return -1;
	}

	/* mi aspetto una sequenza di SUCCESS-FILE
	quando i file da leggere sono finiti response sara' END
	*/
	


	size_t size = 0,bytes = 0;;
	void* content;
	char* pathname;
	int readCounter = 0;
	while(response == FILE_LIST){
			
		if(getFile(&size,&content,&pathname) != 0)
			return -1;

		PRINTER("READ N FILE",pathname,SUCCESS)
		bytes += size;
		readCounter++;

		if(dirname){
			writeOnDisk(dirname,pathname,content,size);
		}

		free(pathname);
		free(content);
		size = 0;
		

		resBuf = calloc(1,1);
		chk_null(resBuf,-1)

		RESPONSE_FROM_SERVER(resBuf,response)
		free(resBuf);
	}
	
	if(response == END_FILE){
		PRINT(fprintf(stdout,"Letti: %d files e %ld bytes\n",readCounter,bytes))
		return 0;
	}else{
		return -1;
	}


}



int getFile(size_t* size,void** content, char** pathname){
	
	char* path;
	int ret = 0;
	//se pathname passato allora voglio leggere anche paathname
	if(pathname){
		// leggo lunghezza pathname
		char* _pathLen = calloc(sizeof(int)+1,1);
		if(!_pathLen)
			return SERVER_ERROR;

		ret = readn(FD_CLIENT,_pathLen,sizeof(int));
		if(ret == 0){
			errno = ECONNRESET;
			free(_pathLen);
			return -1;
		}
		if(ret == -1){
			free(_pathLen);
			return SERVER_ERROR;
		}


		//  lunghezza pathname
		int pathLen = atoi(_pathLen);
		free(_pathLen);

		path = calloc(pathLen+1,1);
		chk_null(path,-1)
		
		//leggo pathname
		ret = readn(FD_CLIENT,path,pathLen);
		if(ret == 0){
			errno = ECONNRESET;
			free(path);
			return -1;
		}
		if(ret == -1){
			free(path);
			return -1;
		}
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




// int getEjectedFiles(int n,const char* dirname,size_t* bytes){
	
// 	size_t efSize;
	

// 	char* efSize_;

// 	void* efBuf;
// 	ejectedFileT* ef;
	
// 	int nfile = n,ret;

// 	while(n){

// 		// mi aspetto size ejectedFile, ejectedFile


// 		//leggo dimensione File
// 		efSize_ = calloc(MAX_FILESIZE_LEN,1);
// 		chk_null(efSize_,-1);
		
// 		ret = readn(FD_CLIENT,efSize_,MAX_FILESIZE_LEN);

// 		if(ret == 0){
// 			errno = ECONNRESET;
// 			free(efSize_);
// 			return -1;
// 		}
// 		if(ret == -1){
// 			free(efSize_);
// 			return -1;
// 		}
	
		
		
	


// 		if(isNumber(efSize_,(long*)&efSize) != 0){
// 			free(efSize_);
// 			return -1;
// 		}
		
// 		free(efSize_);

// 		// leggo ejectedFile

// 		efBuf = malloc(efSize);
// 		if(!efBuf){
// 			return -1;
// 		}
	

// 		ret = readn(FD_CLIENT,efBuf,efSize);
// 		if(ret == 0){
// 			errno = ECONNRESET;
// 			free(efBuf);
// 			return -1;
// 		}
// 		if(ret == -1){
// 			free(efBuf);
// 			return -1;
// 		}

// 		if(dirname){
// 			//writeOnDisk(dirname,path,content,fileSize);
// 		}

// 		ef = (ejectedFileT*) efBuf;


// 		*bytes += ef->size;

// 		fprintf(stdout,"letto %s di %ld bytes\n",ef->pathname,ef->size);
// 		n--;
// 	}

// 	return nfile - n;
// }

