#ifndef FS_H
#define FS_H

#include "../include/comPrt.h"
#include "../include/icl_hash.h"
//struct che mantiene lo stato del file storage server

typedef struct{
	long maxCapacity;
	long maxFileNum;
	long workerNum;

	char* SOCKNAME;
	char* logPath;

	icl_hash_t* ht;
	long capacity;
	long fileNum;
}fsT;


typedef struct{
	char* pathname;
	size_t size;

	void* content;		
}fT;



fsT* create_fileStorage(char* configPath, char* delim);

int open_file(fsT* storage, int fd, long pathLen);

int write_file(fsT* storage,int fd,int pathLen);

#endif