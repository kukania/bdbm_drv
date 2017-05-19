#ifndef __Q_HEADER__
#define __Q_HEADER__
#include"utils.h"

struct Entry;
typedef struct Queue{
	struct Entry **content;
	int front,rear;
	int count;
	int size;
}Queue;

Queue* queue_init(Queue *q, int size);
struct Entry *dequeue(Queue *q);
bool enqueue(Queue *q,struct Entry *);
void queue_free(Queue *q);

#endif
