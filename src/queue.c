#include "../include/queue.h"


#include <stdio.h>
#include <stdlib.h>


int isQueueEmpty(queue* q){
	if(!q || q->ndata == 0)
		return 1;
	return 0;
}

int cmpInt(void* a, void* b){
	return ((long)a == (long)b);
}

queue* createQueue(void (*freeFun)(void*), int (*cmpFun)(void*,void*)){
	
	queue *q = (queue*) malloc(sizeof(queue));
	if(q == NULL)//malloc fallita
		return NULL;
	
	q->ndata = 0;
	//default free and cmp function
	q->freeFun = freeFun ? freeFun : free;
	q->cmpFun = cmpFun ? cmpFun : cmpInt;
	
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

/* pop dalla testa della coda
	il chiamante si occupera' di fare la free
return: NULL no elementi in coda
		
*/
void* dequeue(queue* q){
	if(isQueueEmpty(q))
		return NULL;

	void* elem = q->head->data;
	
	data* newHead = q->head->next;
	if(newHead == NULL){
		q->ndata--;
		free(q->head);
		q->head = NULL;
		q->tail = NULL;
		return elem;
	}

	newHead->prev = NULL;
	free(q->head);
	q->head = newHead;
	q->ndata--;

	return elem;
	
}
/* pop dalla testa della coda senza ritornare il dato

return: 1 no elementi in coda
		0 successo
*/
int removeFromHead(queue* q, int freeData){

	if(isQueueEmpty(q))
		return 1;
		
	data* newHead = q->head->next;
	if(newHead == NULL){
		q->ndata--;
		if(freeData){
			q->freeFun(q->head->data);
		}
		free(q->head);
		q->head = NULL;
		q->tail = NULL;
		return 0;
	}

	newHead->prev = NULL;

	if(freeData){
		q->freeFun(q->head->data);
	}
	free(q->head);
	
	q->head = newHead;
	q->ndata--;

	return 0;
}

// cerca elem nella coda e lo rimuove

int removeFromQueue(queue* q, void* elem, int freeData){
	if(isQueueEmpty(q))
		return 1;
	
	data *curr = q->head;
	while (curr && !(q->cmpFun(curr->data, elem)))
		curr = curr->next;

	if(!curr)
		return 1;


	//se elem e' la testa
	if(curr == q->head){
		return removeFromHead(q,freeData);
	}
	data* prev = curr->prev;
	data* succ = curr->next;

	//se elem e' la coda
	if(succ == NULL){
		prev->next = NULL;
		q->tail = prev;
	}else{
		prev->next = succ;
		succ->prev = prev;
	}

	if(freeData){
		q->freeFun(curr->data);
	}
	free(curr);
	q->ndata--;

	return 0;
}




void* findQueue(queue* q,void* elem){
	if(isQueueEmpty(q))
		return NULL;

	data* curr = q->head;

	while(curr){
		if(q->cmpFun(elem,curr->data))
			return curr->data; //trovato!
		curr = curr->next;
	}
	
	return NULL; 
}

void destroyData(queue* q, int freeData){
	
	data* tmp = q->head;
	
	while(!isQueueEmpty(q)){
		q->ndata--;
		q->head = q->head->next;
		if(freeData){
			q->freeFun(tmp->data);
		}
		free(tmp);
		tmp = q->head;
	}
}


void destroyQueue(queue* q, int freeData){
	
	if(!isQueueEmpty(q))
		destroyData(q,freeData);

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