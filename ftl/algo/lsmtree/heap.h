#ifndef __HEAP_H__
#define __HEAP_H__

#include "utils.h"
#include"bptree.h"
typedef struct Entry Entry;
typedef struct Heap{
	Entry **body;
	int last;
	int size;
}Heap;
bool heap_function(Entry *, Entry *);
bool heap_push(Heap *,Entry *);
Entry *heap_pop(Heap *h);
void heap_init(Heap *,int);
void heap_free(Heap *);

#endif
