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
}fsT;


typedef struct{
	char* pathname;
	size_t size;

	void* content;		
}fT;






void clientExit(int fd);

fsT* create_fileStorage(char* configPath, char* delim);

int open_file(fsT* storage, int fd);

int write_file(fsT* storage,int fd);

int read_file(fsT* storage,int fd);

int read_n_file(fsT* storage,int fd);

int remove_file(fsT* storage, int fd);

void freeFile(void* fptr);

int cmpFile(void* f1, void* f2);




#endif