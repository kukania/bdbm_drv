#ifndef __BLOOMFILTER_H__
#define __BLOOMFILTER_H__

#include"utils.h"
#include <stdlib.h>
#include<stdio.h>
typedef struct bloomfilter {
	unsigned int  m; //size
	unsigned int  k; //hash function number;
	unsigned char *bit_vector;
}bloomfilter;

void
bloomfilter_init(struct bloomfilter *bloomfilter);

void
bloomfilter_set(struct bloomfilter *bloomfilter, const void *key, size_t len);

int
bloomfilter_get(struct bloomfilter *bloomfilter, const void *key, size_t len);

void
bloomfilter_free(bloomfilter *filetr);
#endif /* __BLOOMFILTER_H__ */
