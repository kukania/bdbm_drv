#include"queue.h"
#include<stdlib.h>

Queue *queue_init(Queue *q,int size){
	q->content=(struct Entry **)malloc(sizeof(struct Entry *)*size);
	q->front=q->rear=0;
	q->count=0;
	q->size=size;
	return q;
}

struct Entry *dequeue(Queue *q){
	if(q->count==0)
		return NULL;
	else{
		q->count--;
		struct Entry *res=q->content[q->front];
		q->content[q->front++]=NULL;
		q->front%=q->size;
		return res;
	}
}
bool enqueue(Queue *q, struct Entry *input){
	if(q->count==q->size)
		return false;
	else{
		q->count++;
		q->content[q->rear++]=input;
		q->rear%=q->size;
		return true;
	}
}

void queue_free(Queue *q){
	free(q->content);
	free(q);
}

#ifdef DEBUG
#include<stdio.h>
#include"bptree.h"

int main(){
	struct Entry *temp;
	int i=0;
	Queue *q=malloc(sizeof(Queue));

	queue_init(q,5);

	for(int i=1; i<=1000; i++){
		temp=malloc(sizeof(struct Entry));
		temp->pbn=i;
		enqueue(q,temp);
		if(q->count==5){
			for(int j=0; j<4; j++){
			Entry *temp2=dequeue(q);
			printf("%d\n",temp2->pbn);
			}
			sleep(1);
		}
	}
}
#endif
