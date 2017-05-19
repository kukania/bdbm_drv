#ifndef __LSM_HEADER__
#define __LSM_HEADER__
#include"bptree.h"
#include"skiplist.h"
#include"hash.h"
#include"utils.h"
typedef struct buffer{
	skiplist *last;
    Hash *data;
	bloomfilter *filter;
    level *disk[LEVELN];
}buffer;
typedef struct table{
    int lev_addr[LEVELN];
}table;
typedef struct lsmtree{
    Hash *memtree;
    buffer buf;
    table tlb;
	bloomfilter *filter;
	int fd;
}lsmtree;
lsmtree* init_lsm();
bool merge(int t);
bool put(KEYT key, char* value);
int get(KEYT key,char *);
void *lsm_free(lsmtree *);
lsmtree *LSM;
#endif
