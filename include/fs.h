#ifndef FS_H
#define FS_H

#include <unistd.h>
#include "../include/comPrt.h"
#include "../include/icl_hash.h"
#include "../include/queue.h"
#include "../include/serverLogger.h"

#define FIFO 0
#define LRU 1
#define LFU 2

//struct che mantiene lo stato del file storage server
typedef struct{
	
	icl_hash_t* ht;

	size_t maxCapacity;
	size_t currCapacity;
	int maxFileNum;
	int currFileNum;

	queue* filesQueue;

	pthread_mutex_t smux;

	// stats

	size_t maxCapacityStored;
	int maxFileNumStored;
	int ejectedFileNum; // numero di file effettivamente espulsi
	int tryEjectFile; // numero volte che e' partito l'algoritmo di rimpiazzamento


	int evictionPolicy;

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


	
	int fdlock;	//O_LOCK = client che possiede la lock
	int fdwrite; //client che può effettuare la write
	queue* fdopen; //client che hanno il file aperto
	queue* fdpending; //client in attesa di ottenere la lock

	int modified; // DIRTY BIT che indica se il file è stato modificato dopo la prima write
 
	time_t accessTime; // per LRU
	int accessCount; // per LFU
}fT;

#define IS_O_CREATE_SET(flags) \
	(flags == 1 || flags == 3) ? 1 : 0
	

#define IS_O_LOCK_SET(flags) \
	(flags == 2 || flags == 3) ? 1 : 0


#define CAST_FD(fd) (void*)(long)fd


fsT* create_fileStorage(size_t maxCapacity, int maxFileNum, int evictionPolicy);

int destroy_fileStorage(fsT* storage);

int open_file(fsT* storage, int fdClient, char* pathname, int flags, queue* fdPending);

int close_file(fsT* storage, int fdClient, char* pathname);

int write_append_file(fsT* storage, int mode, int fdClient,char* pathname, size_t size, void* content, queue* ejected, queue* fdPending);

int read_file(fsT* storage,int fdClient, char* pathname, size_t* size, void** content);

int read_n_file(fsT* storage,int n,int fdClient, queue* ejected);

int remove_file(fsT* storage, int fdClient, char* pathname, queue* fdPending);

int lock_file(fsT* storage, int fdClient, char* pathname);

int unlock_file(fsT* storage, int fdClient, char* pathname, int *newFdLock);

int remove_client(fsT* storage, int fdClient, queue* fdPending);

void freeFile(void* fptr);

int cmpFile(void* f1, void* f2);



fT* eject_file(fsT* storage, char* pathname, int fdClient, int chkEmpty);

void print_final_info(fsT* storage,int maxClientNum);

fT* fileCopy(fT* fPtr);

int store_remove(fsT* storage, fT* fPtr, int freeFile);

int store_insert(fsT* storage, int fdClient, char* pathname,int lock, queue* fdPending);

void update_access_info(fT* fPtr, int ep);


#endif