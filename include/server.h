#ifndef SERVER_H
#define SERVER_H




int masterFun();

void* workerFun();


int getPathname(int fd,char** pathname);

int sendFile(int fd,char* pathname, size_t size, void* content);

int getN(int fd, int* n);
int getFlags(int fd, int *flags);

int getFile(int fd, size_t* size, void** content);

#endif