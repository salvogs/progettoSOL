#include "../include/queue.h"


#include <stdio.h>
#include <stdlib.h>


int isQueueEmpty(queue* q){
	if(q->ndata == 0)
		return 1;
	return 0;
}

queue* createQueue(void (*freeFun)(void*), int (*compare)(void*,void*)){
	queue *q = (queue*) malloc(sizeof(queue));
	if(q == NULL)//malloc fallita
		return NULL;
	
	q->ndata = 0;
	q->freeFun = freeFun;
	q->compare = compare;
	q->head = q->tail = NULL;


	return q;
}


int enqueue(queue* q, void* elem){
	//alloco spazio per nuovo elemento
	data *new = (data*) malloc(sizeof(data));
	if(new == NULL)
		return 1;

	new->data = elem;
	new->next = NULL;
	//se la coda è vuota inserisco in testa
	if(isQueueEmpty(q)){
		new->prev = NULL;
		q->head = q->tail = new;
		q->ndata++;
		return 0;
	}
	//altrimenti inserisco in coda

	new->prev = q->tail;
	q->tail->next = new;
	q->tail = new;
	q->ndata++;

	return 0;
}


void* dequeue(queue* q){
	if(isQueueEmpty(q))
		return NULL;

	void* elem = q->head->data;
	
	data* newHead = q->head->next;
	if(newHead == NULL){
		q->ndata--;
		free(q->head);
		return elem;
	}

	newHead->prev = NULL;
	free(q->head);
	q->head = newHead;
	q->ndata--;

	return elem;
	
}

int findQueue(queue* q,void* elem){
	if(isQueueEmpty(q))
		return 0;

	data* curr = q->head;

	while(curr){
		if(q->compare(elem,curr->data))
			return 1; //trovato!
		curr = curr->next;
	}
	
	return 0; 
}

void destroyData(queue* q){
	
	data* tmp = q->head;

	/*se quando e' stata creata la coda non e' stata
	definita alcuna freeFun*/
	if(q->freeFun == NULL){
		while(isQueueEmpty(q) != 1){
			q->ndata--;
			q->head = q->head->next;

			free(tmp);
			tmp = q->head;
		}
	}else{
		while(isQueueEmpty(q) != 1){
			q->ndata--;
			q->head = q->head->next;
			if(q->freeFun)
				q->freeFun(tmp->data);
			
			free(tmp);
			tmp = q->head;
		}


	}
	
	

	
}

void destroyQueue(queue* q){

	if(!isQueueEmpty(q))
		destroyData(q);

	free(q);
	return;
}


void printQueueInt(queue* q){
	data* printPtr = q->head;
	
	while(printPtr != NULL){
		fprintf(stdout,"%ld ", (long)printPtr->data);
		printPtr = printPtr->next;
	}


	return;
}