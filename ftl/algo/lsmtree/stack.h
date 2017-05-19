#ifndef __S_HEADER__
#define __S_HEADER__
#include"utils.h"
struct Entry;

typedef struct Stack{
	struct Entry **content;
	int count;
	int size;
}Stack;

Stack* stack_init(Stack* q, int size);
struct Entry* pop(Stack *q);
bool push(Stack *q, struct Entry *);
void stack_free(Stack *q);
#endif
