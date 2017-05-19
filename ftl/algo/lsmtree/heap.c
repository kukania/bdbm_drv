#include"heap.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
bool heap_function(Entry *a, Entry *b){
	return a->version>b->version;
}
bool heap_push(Heap *h,Entry *input){
	if(h->last > h->size)
		return false;
	else{
		h->body[h->last]=input;
		if(h->last==1){
			h->last++; return true;
		}
		int idx=h->last++;
		while(idx>1){
			if(heap_function(h->body[idx],h->body[idx/2])){
				Entry *temp=h->body[idx];
				h->body[idx]=h->body[idx/2];
				h->body[idx/2]=temp;
				idx/=2;
			}
			else
				break;
		}
		return true;
	}
}
Entry* heap_pop(Heap *h){
	Entry *res=h->body[1];
	int idx=1;
	if(h->last==1) return NULL;
	h->body[idx]=h->body[h->last-1];
	h->body[h->last-1]=NULL;
	while(idx<=h->size && h->body[idx]!=NULL && idx*2<h->last){
		Entry *a;
		if((h->body[2*idx]==NULL && 2*idx+1>=h->last) || (h->body[2*idx+1]==NULL && 2*idx>=h->last)) break;

		if(h->body[2*idx]==NULL && h->body[2*idx+1]!=NULL && 2*idx+1<h->last){
			if(!heap_function(h->body[idx],h->body[2*idx+1])){
				a=h->body[idx];
				h->body[idx]=h->body[2*idx+1];
				h->body[2*idx+1]=a;
				idx=2*idx+1;
			}
			else break;
		}
		else if(h->body[2*idx+1]==NULL && h->body[2*idx]!=NULL && 2*idx<h->last){
			if(!heap_function(h->body[idx],h->body[2*idx])){
				a=h->body[idx];
				h->body[idx]=h->body[2*idx];
				h->body[2*idx]=a;
				idx=2*idx;
			}
			else 
				break;
		}
		else if(h->body[2*idx]==NULL && h->body[2*idx+1]==NULL) break;
		else if(heap_function(h->body[2*idx],h->body[2*idx+1])){
			if(!heap_function(h->body[idx],h->body[2*idx])){
				a=h->body[idx];
				h->body[idx]=h->body[2*idx];
				h->body[2*idx]=a;
				idx=2*idx;
			}
			else 
				break;

		}
		else{
			if(!heap_function(h->body[idx],h->body[idx+1])){
				a=h->body[idx];
				h->body[idx]=h->body[2*idx+1];
				h->body[2*idx+1]=a;
				idx=2*idx+1;
			}
			else break;

		}
	}
	h->last--;
	return res;
}
void heap_init(Heap *h,int size){
	h->body=(Entry**)malloc(sizeof(Entry*)*size+1);
	memset(h->body,0,sizeof(Entry *)+1);
	h->size=size;
	h->last=1;
}
void heap_free(Heap *h){
	free(h->body);
	free(h);
}
#ifdef DEBUG
int main(){
	Heap *h=malloc(sizeof(Heap));
	heap_init(h,100);
	int i=0;
	Entry *entry;
	do{
		entry=malloc(sizeof(Entry));
		entry->version=i++;
	}while(heap_push(h,entry));

	while((entry=heap_pop(h))!=NULL){
		printf("%d\n",entry->version);
	}
	heap_free(h);
}
#endif
