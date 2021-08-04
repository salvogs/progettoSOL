#ifndef FS_H
#define FS_H

#include <unistd.h>
#include "../include/comPrt.h"
#include "../include/icl_hash.h"
#include "../include/queue.h"
//struct che mantiene lo stato del file storage server

typedef struct{
	long maxCapacity;
	long maxFileNum;
	long workerNum;

	char* SOCKNAME;
	char* logPath;

	icl_hash_t* ht;
	long currCapacity;
	long currFileNum;

	queue* filesQueue;

	pthread_mutex_t smux;

}fsT;


typedef struct{
	char* pathname;
	size_t size;

	void* content;

	pthread_mutex_t mux;
	pthread_mutex_t ordering;	

	pthread_cond_t go;	
	int activeReaders;
	int activeWriters;


	//pthread_mutex_t muxfd;
	int fdlock;	
	int fdwrite;
	queue* fdopen;
}fT;



#define CAST_FD(fd) (void*)(long)fd


void clientExit(int fd);

fsT* create_fileStorage(char* configPath, char* delim);

int open_file(fsT* storage, int fdClient, char* pathname, int flags);

int close_file(fsT* storage, int fdClient, char* pathname);

int write_append_file(fsT* storage,int fdClient,char* pathname, size_t size, void* content, queue* ejected);

int read_file(fsT* storage,int fdClient, char* pathname, size_t* size, void** content);

int read_n_file(fsT* storage,int n,int fdClient, queue* ejected);

int remove_file(fsT* storage, int fdClient, char* pathname);

int lock_file(fsT* storage, int fdClient, char* pathname);

int unlock_file(fsT* storage, int fdClient, char* pathname);


void freeFile(void* fptr);

int cmpFile(void* f1, void* f2);



fT* eject_file(queue* fq, char* pathname);

fT* fileCopy(fT* fPtr);

int store_remove(fsT* storage, fT* fPtr, int freeFile);

int store_insert(fsT* storage, int fdClient, char* pathname,int lock);

#endif