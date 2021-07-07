#ifndef QUEUE_H
#define QUEUE_H


#include <stdio.h>

typedef struct datatype{
	void* data;
	struct datatype *next;
	struct datatype *prev;
}data;

typedef struct _queue{
	size_t ndata;
	void (*freeFun)(void*);
	int (*compare)(void*,void*);
	data *head;
	data *tail;
}queue;


//ritorna 1 se la coda e' vuota 0 altrimenti
int isQueueEmpty(queue* q);

queue* createQueue(void (*freeFun)(void *), int (*compare)(void*,void*));

int enqueue(queue* q, void* data);

void* dequeue(queue* q);

int findQueue(queue* q,void* elem);

void destroyData(queue* q);

void destroyQueue(queue* q);

void printQueueInt(queue* q);

#endif
