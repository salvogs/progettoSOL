#ifndef SERVER_H
#define SERVER_H



int clientExit(int fd);

pthread_t* spawnWorker(int n);

int masterFun();

void* workerFun();

int getPathname(int fd,char** pathname);

int sendResponseCode(int fd,int res);

int sendFile(int fd,char* pathname, size_t size, void* content);

int getN(int fd, int* n);

int getFlags(int fd, int *flags);

int getFile(int fd, size_t* size, void** content);

char* retToString(int ret);


#endif