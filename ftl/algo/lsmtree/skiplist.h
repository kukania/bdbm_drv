#ifndef __SKIPLIST_HEADER
#define __SKIPLIST_HEADER
#define MAX_L 12
#include"utils.h"
#include"bdbm_drv.h"
#include"../page_ftl.h"
typedef struct snode{
	KEYT key;
	bdbm_phyaddr_t ppa;
	bdbm_llm_req_t lr;
	struct snode *list[MAX_L];
}snode;

typedef struct skiplist{
	int level;
	KEYT start,end;
	int size;
	snode *header;
	snode *tail;
}skiplist;

typedef struct skIterator{
	skiplist *mylist;
	snode *now;
} skIterator;

typedef struct keyset{
	KEYT key;
	bdbm_phyaddr_t ppa;
}keyset;
typedef struct sktable{
	keyset meta[KEYN];
}sktable;

snode *snode_init(snode*);
skiplist *skiplist_init(skiplist*);
snode *skiplist_find(skiplist*,KEYT);
snode *skiplist_insert(skiplist*,KEYT,bdbm_llm_req_t *lr,bool overwrite);
snode *skiplist_insert_node(skiplist*,snode*);
snode *skiplist_pop(skiplist *);

sktable *skiplist_read(int, int fd);
sktable *skiplist_meta_read(int, int fd);
keyset* skiplist_keyset_find(sktable *,KEYT key);
//sktable *skiplist_data_read(sktable*,int pbn, int fd);
//bool skiplist_keyset_read(keyset* ,char *,int fd);

void skiplist_sktable_free(sktable *);
int skiplist_write(skiplist*,int, bdbm_page_ftl_private_t *,bdbm_drv_info_t *,int fd);
int skiplist_meta_write(skiplist *,int,bdbm_page_ftl_private_t *,bdbm_drv_info_t *, int fd);
//int skiplist_data_write(skiplist *,int, int fd);
skiplist* skiplist_cut(skiplist *,int num);
void skiplist_ex_value_free(skiplist *list);
void skiplist_meta_free(skiplist *list);
void skiplist_free(skiplist *list);
skIterator* skiplist_getIterator(skiplist *list);
void skiplist_traversal(skiplist *data);
snode *sk_getNext(skIterator*);
#endif
