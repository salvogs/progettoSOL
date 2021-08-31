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
	int (*cmpFun)(void*,void*);
	data *head;
	data *tail;
}queue;



int isQueueEmpty(queue* q);

queue* createQueue(void (*freeFun)(void *), int (*cmpFun)(void*,void*));

int enqueue(queue* q, void* data);

void* dequeue(queue* q);

int removeFromHead(queue* q, int freeData);

int removeFromQueue(queue* q, void* elem, int freeData);

void* findQueue(queue* q,void* elem);

void destroyData(queue* q, int freeData);

void destroyQueue(queue* q, int freeData);

void printQueueInt(queue* q);

#endif
