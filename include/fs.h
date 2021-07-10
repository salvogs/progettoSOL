#ifndef FS_H
#define FS_H

#include "../include/icl_hash.h"
//struct che mantiene lo stato del file storage server

typedef struct fs{
	long maxCapacity;
	long maxFileNum;
	long workerNum;

	char* SOCKNAME;
	char* logPath;

	icl_hash_t* ht;

}fsT;


fsT* createFileStorage(char* configPath, char* delim);

int openFile(int fd, long pathLen);

#endif