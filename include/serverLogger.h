#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H


// 8kb
#define LOGBUFFERSIZE 8192  
#define LOGEVENTSIZE 512

void* loggerFun(void* args);
int logCreate(char* logPath);
int logPrint(char* event, char* pathname, int fdClient, size_t bytes, char* ret);
int logDestroy();





#endif