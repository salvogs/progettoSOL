#include <stdio.h>
#include <time.h>
#include "../include/utils.h"
#include "../include/serverLogger.h"



pthread_mutex_t lmux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lcond = PTHREAD_COND_INITIALIZER;


void* loggerFun(void* args){

	LOCK(&lmux)

	loggerT* logArgs = (loggerT*)args;

	while(1){

		FILE* fPtr = fopen(logArgs->path,"a");
		chk_null(fPtr,NULL)

		

		while(*(logArgs->bufSize) == 0)
			WAIT(&lcond,&lmux)

		//fwrite(logArgs->buffer,*(logArgs->bufSize),1,fPtr);
		fprintf(fPtr,"%s",logArgs->buffer);
		fclose(fPtr);
		UNLOCK(&lmux)
	}
}


int logEvent(char* logBuf,size_t* logBufSize){
	time_t t = time(NULL);
	

    printf("%s\n",ctime(&t));
	LOCK(&lmux)
	
	// while(logBuf == )
	// 	WAIT(&lcond,&lmux)

	snprintf(logBuf+strlen(logBuf),strlen(ctime(&t)),"%s",ctime(&t));
	*logBufSize += strlen(ctime(&t));
	SIGNAL(&(lcond));
	UNLOCK(&lmux)


	return 0;
}