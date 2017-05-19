
#include"measure.h"
#include<stdio.h>
#include<stdlib.h>
#include<sys/time.h>
void donothing(MeasureTime *t){
}
void donothing2(MeasureTime *t,char *a){
}
void measure_init(MeasureTime *m){
	m->header=NULL;
	m->cnt=0;
}
void measure_start(MeasureTime *m){
	linktime *new_val=malloc(sizeof(linktime));
	if(m->header==NULL){
		m->header=new_val;
	}
	else{
		new_val->next=m->header;
		m->header=new_val;
	}
	gettimeofday(&new_val->start,NULL);
	return;
}
void measure_pop(MeasureTime *m){
	linktime *t=m->header;
	m->header=m->header->next;
	free(t);
	return;
}
void measure_end(MeasureTime *m,char *format){
	struct timeval res; linktime *t;
	gettimeofday(&m->header->end,NULL);
	timersub(&m->header->end,&m->header->start,&res);
	printf("%s:%ld sec and %.5f\n",format,res.tv_sec,(float)res.tv_usec/1000000);
	t=m->header;
	m->header=m->header->next;
	free(t);
	return;
}
#ifdef DEBUG
int main(){
	MeasureTime t;
	measure_init(&t);
	measure_start(&t);
	measure_end(&t);
}
#endif
