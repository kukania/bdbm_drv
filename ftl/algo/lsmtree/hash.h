#ifndef __HASH_H__
#define __HASH_H__
#include"utils.h"
typedef struct Hnode{
	int key;
	int pbn;
	int number;
}hnode;
typedef struct Hash{
	int size;
	hnode table[KEYN*TABLEFACTOR];
	char *value;
	int start;
	int end;
}Hash;

Hash *hash_init(Hash*);
void hash_free(Hash *);
bool hash_insert(Hash*,int,char*);
bool hash_find(Hash *,int,char*);
bool hash_meta_write(Hash *,int,int);
bool hash_data_write(Hash *,int, int);
bool hash_write(Hash *,int,int);
Hash *hash_read(int,int);
Hash *hash_meta_read(int,int);
bool hash_data_read(Hash *,int,int);
bool hash_hnode_read(hnode*,char*,int);
hnode *hash_hnode_find(Hash *,int);
#endif
