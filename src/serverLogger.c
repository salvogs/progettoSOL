#include <stdio.h>
#include <time.h>
#include "../include/utils.h"
#include "../include/serverLogger.h"



pthread_mutex_t lmux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lcond = PTHREAD_COND_INITIALIZER;

// buffer condiviso fra i vari thread per logging
char* logBuf = NULL;
size_t logBufSize = 0;
FILE* logFile = NULL;

int logCreate(char* logPath){
	
	logBuf = calloc(1,LOGBUFFERSIZE);
	chk_null(logBuf,-1)

	logBufSize = 0;
	(void) unlink(logPath);

	logFile = fopen(logPath,"w");
	chk_null(logFile,-1)
	

	return 0;
}


void* loggerFun(void* args){	

	while(1){
		LOCK(&lmux)
		
		while(logBufSize == 0)
			WAIT(&lcond,&lmux)
		// "messaggio" terminazione thread logger
		if(logBufSize == -1){
			UNLOCK(&lmux)
			return NULL;
		}
		ec(fwrite(logBuf,logBufSize,1,logFile),logBufSize,"log fwrite",exit(EXIT_FAILURE))

		//fflush(logFile);
		logBufSize = 0;
		memset(logBuf,'\0',LOGBUFFERSIZE);
		UNLOCK(&lmux)
	}
}


int logPrint(char* event, char* pathname, int fdClient, size_t bytes, char* ret){
	time_t t = time(NULL);
	
	char buf[LOGEVENTSIZE] = "";

	char strTime[20] = "";
	struct tm ltime;
	ec(localtime_r(&t,&ltime),NULL,"localtime_r",exit(EXIT_FAILURE));
	strftime(strTime,sizeof(strTime),"%F %T",&ltime);
	
	
	sprintf(buf+strlen(buf),"%s\ttid:%ld\t",strTime,pthread_self());

	
	if(fdClient){
		sprintf(buf+strlen(buf),"client:%d\t",fdClient);
	}

	sprintf(buf+strlen(buf),"op:%s\t",event);


	if(pathname){
		sprintf(buf+strlen(buf),"path:%s\t",pathname);
	}

	if(ret){
		sprintf(buf+strlen(buf),"%s\t",ret);
	}

	if(bytes){
		sprintf(buf+strlen(buf),"byte_processati:%ld\t",bytes);
	}
	

	buf[strlen(buf)] = '\n';

	LOCK(&lmux)

	
	strncat(logBuf,buf,strlen(buf));
	logBufSize += strlen(buf);


	SIGNAL(&(lcond));
	UNLOCK(&lmux)


	return 0;
}


int logDestroy(){
	LOCK(&(lmux))

	//controllo se c'e' ancora qualcosa da scrivere su disco 
	if(logBufSize)
		ec(fwrite(logBuf,logBufSize,1,logFile),logBufSize,"log fwrite",exit(EXIT_FAILURE))


	free(logBuf);
	fclose(logFile);

	// "messaggio" terminazione
	logBufSize = -1;
	SIGNAL(&(lcond))


	UNLOCK(&(lmux))
	return 0;
}