#ifndef __LSM_HEADER__
#define __LSM_HEADER__
#include"bptree.h"
#include"skiplist.h"
#include"utils.h"
typedef struct buffer{
	skiplist *last;
	sktable *data;
	bloomfilter *filter;
	level *disk[LEVELN];
}buffer;
typedef struct table{
	int lev_addr[LEVELN];
}table;
typedef struct lsmtree{
	skiplist *memtree;
	buffer buf;
	table tlb;
	bloomfilter *filter;
	int fd;
}lsmtree;
lsmtree* init_lsm();
bool merge(int t,bdbm_page_ftl_private_t*,bdbm_drv_info_t *);
bool put(KEYT key, bdbm_llm_req_t* value,bdbm_page_ftl_private_t*,bdbm_drv_info_t *);
int get(KEYT key,bdbm_drv_info_t *,bdbm_llm_req_t *value);
void *lsm_free(lsmtree *);
lsmtree *LSM;
#endif
