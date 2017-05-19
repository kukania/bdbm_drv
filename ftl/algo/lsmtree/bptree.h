#ifndef __BP__HEADER__
#define __BP__HEADER__
#include"utils.h"
#include"queue.h"
#include"stack.h"
#include"heap.h"
struct Entry; struct Node;
typedef struct bloomfilter bloomfilter;
typedef struct Heap bpIterator;
typedef union Child{
	struct Entry *entry;
	struct Node *node;
}Child;

typedef struct Entry{
	KEYT key;
	KEYT version;
	KEYT end;
	KEYT pbn;
	bloomfilter *filter;
	struct Node *parent;
}Entry;

typedef struct Node{
	bool leaf;
	short count;
	KEYT separator[MAXC];
	Child children[MAXC+1];
	struct Node *parent;
}Node;

typedef struct level{
	Node *root;
	//Stack *stack;
	Queue *q;
	int size;
	int m_size;
	int depth;
	KEYT version;
}level;

Node *level_find_leafnode(level *lev, KEYT key);
Entry *make_entry(KEYT start, KEYT end,KEYT pbn,bloomfilter*);
Entry *level_entry_copy(Entry *);
level* level_init(level*,int size);
bpIterator *level_find(level*,KEYT key);
Node *level_insert(level*,Entry *);
Node *level_delete(level*,Entry *);
Entry *bp_getNext(bpIterator *);
Entry *level_getFirst(level *);
void level_free(level*);
#endif
