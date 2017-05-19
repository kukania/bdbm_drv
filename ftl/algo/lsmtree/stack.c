#include"stack.h"
#include<stdlib.h>
Stack* stack_init(Stack* q, int size){
	q->content=(struct Entry**)malloc(sizeof(struct Entry *)*size);
	q->count=0;
	q->size=size;
	return q;
}
struct Entry * pop(Stack *s){
	if(s->count==0) return NULL;
	else{
		s->count--;
		return s->content[s->count];
	}
}
bool push(Stack *s,struct Entry* input){
	if(s->count==s->size) return false;
	else{
		s->content[s->count]=input;
		s->count++;
		return true;
	}
}

void stack_free(Stack *q){
	free(q->content);
	free(q);
}
#ifdef DEBUG
#include<stdio.h>
int main(){
	struct Entry *temp;
	int i=0;
	Stack *q=malloc(sizeof(Stack));
	stack_init(q,10);
	do{
		temp=malloc(sizeof(struct Entry));
		temp->pbn=i++;
	}while(push(s,temp));

	while((temp=pop(q))!=NULL){
		printf("%d\n",temp->pbn);
	}
}
#endif
