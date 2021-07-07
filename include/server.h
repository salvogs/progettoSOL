#ifndef SERVER_H
#define SERVER_H


//struct che mantiene lo stato del server

typedef struct fs{
	long maxCapacity;
	long maxFileNum;
	long workerNum;

	char* SOCKNAME;
	char* logPath;
}fsT;




#endif