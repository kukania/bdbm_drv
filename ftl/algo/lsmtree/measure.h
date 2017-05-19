#ifndef __MEASURE_H__
#define __MEASURE_H__
#include<sys/time.h>
typedef struct linktime{
	struct timeval start,end;
	struct linktime * next;
}linktime;

typedef struct MeasureTime{
	linktime *header;
	int cnt;
}MeasureTime;

void measure_init(MeasureTime *);
void measure_start(MeasureTime *);
void measure_pop(MeasureTime *);
void measure_end(MeasureTime *,char *);
void donothing(MeasureTime *t);
void donothing2(MeasureTime *t,char *a);
#endif
