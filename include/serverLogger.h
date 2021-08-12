#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H


typedef struct{
	char* path;
	char* buffer;
	size_t* bufSize;
}loggerT;

// 8kb
#define LOGBUFFERSIZE 8192  


void* loggerFun(void* args);

int logEvent(char* logBuf,size_t* logBufSize);





#endif