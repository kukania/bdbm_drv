#include"hash.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<limits.h>
Hash* hash_init(Hash *input){
	input->size=0;
	input->start=INT_MAX;
	input->end=0;
	memset(&input->table,0,sizeof(input->table));
	input->value=malloc(PAGESIZE * KEYN);
	return input;
}
void hash_free(Hash *input){
	free(input->value);
	free(input);
}
int hashfunction(int key){
	key = ~key + (key << 15); // key = (key << 15) - key - 1;
	key = key ^ (key >> 12);
	key = key + (key << 2);
	key = key ^ (key >> 4);
	key = key * 2057; // key = (key + (key << 3)) + (key << 11);
	key = key ^ (key >> 16);
	return key%(KEYN*TABLEFACTOR);
}
bool hash_insert(Hash *des, int key, char *value){
	if(des->start>key) des->start=key;
	if(des->end<key) des->end=key;
	if(des->size==KEYN)
		return false;
	int tv=hashfunction(key);
	while(des->table[tv].key!=0){
		if(des->table[tv].key==key){
			memcpy(&des->value[des->table[tv].number*PAGESIZE],value,PAGESIZE);
			return true;
		}
		tv++;
		if(tv==KEYN*TABLEFACTOR) tv=0;
	}
	des->table[tv].key=key;
	des->table[tv].number=des->size;
	memcpy(&des->value[des->size*PAGESIZE],value,PAGESIZE);
	des->size++;
	return true;
}
bool hash_find(Hash *des, int key, char *res){
	memset(res,0,PAGESIZE);
	int tv=hashfunction(key);
	int cnt=0;
	while(des->table[tv].key!=key){
		if(des->table[tv].key==key){
			memcpy(res,&des->value[des->table[tv].number*PAGESIZE],PAGESIZE);
			return true;
		}
		else{
			tv++;
			if(tv==KEYN*TABLEFACTOR)
				tv=0;
		}
		cnt++;
		if(cnt==KEYN*TABLEFACTOR) return false;
	}
	if(des->table[tv].key==key){
		memcpy(res,&des->value[des->table[tv].number*PAGESIZE],PAGESIZE);
		return true;
	}
	return false;
}
hnode * hash_hnode_find(Hash *des,int key){
	int tv=hashfunction(key);
	int cnt=0;
	while(des->table[tv].key!=key){
		if(des->table[tv].key==key){
			return &des->table[tv];
		}
		else{
			tv++;
			if(tv==KEYN*TABLEFACTOR)
				tv=0;
		}
		cnt++;
		if(cnt==KEYN*TABLEFACTOR) return NULL;
	}
	if(des->table[tv].key==key){
		return &des->table[tv];
	}
}
bool hash_write(Hash *src, int pbn,int fd){
	if(!hash_meta_write(src,pbn,fd)) return false;
	if(!hash_data_write(src,pbn,fd)) return false;
	return true;
}
bool hash_meta_write(Hash *src,int key, int fd){
	lseek(fd,HASH_BLOCK*key,SEEK_SET);
	for(int i=0; i<TABLEFACTOR*KEYN; i++){
		if(src->table[i].key==0)
			continue;
		else
			src->table[i].pbn=key;
	}
	if(write(fd,src,HASH_META)==-1){
		printf("write meta error!\n");
		return false;
	}
	return true;
}

bool hash_data_write(Hash *src,int key, int fd){
	lseek(fd,HASH_BLOCK*key+HASH_META,SEEK_SET);
	if(write(fd,src->value,PAGESIZE*KEYN)==-1){
		printf("write data error!\n");
		return false;
	}
	return true;
}
Hash * hash_read(int pbn, int fd){
	Hash *res=hash_meta_read(pbn,fd);
	if(hash_data_read(res,pbn,fd))
		return res;
	hash_free(res);
	return NULL;
}
Hash *hash_meta_read(int pbn, int fd){
	lseek(fd,HASH_BLOCK*pbn,SEEK_SET);
	Hash *res=malloc(sizeof(Hash));
	read(fd,res,HASH_META);
	return res;
}
bool hash_data_read(Hash *res,int pbn, int fd){
	lseek(fd,HASH_BLOCK*pbn+HASH_META,SEEK_SET);
	res->value=malloc(KEYN*PAGESIZE);
	memset(res->value,0,KEYN*PAGESIZE);
	if(read(fd,res->value,KEYN*PAGESIZE)==-1){
		printf("read data error\n");
		return false;
	}
	return true;
}
bool hash_hnode_read(hnode *src, char *value, int fd){
	int pbn=src->pbn;
	int number=src->number;
	lseek(fd,HASH_BLOCK*pbn+HASH_META+number*PAGESIZE,SEEK_SET);
	if(read(fd,value,PAGESIZE)==-1)
		return false;
	return true;
}
#ifdef DEBUG
int main(){
	Hash *h;
	int fd=open("data/hash.hash",O_CREAT|O_TRUNC|O_RDWR,0666);
	if(fd==-1) printf("file open error!");
	char value[PAGESIZE];
	for(int j=0; j<2; j++){
		h=malloc(sizeof(Hash));
		hash_init(h);
		int t;
		for(int i=1+j*1000; i<=KEYN+j*1000;i++){
			memcpy(value,&i,sizeof(int));
			hash_insert(h,i,value);
		}
		hash_write(h,j,fd);
		hash_free(h);
	}
	fsync(fd);
	for(int j=0; j<2; j++){

		h=hash_read(j,fd);
		for(int i=1+j*1000; i<=KEYN+j*1000; i++){
			int temp;
			hnode * t=hash_hnode_find(h,i);
			hash_hnode_read(t,value,fd);
			memcpy(&temp,value,sizeof(int));
			if((t->number+1)%1000!=(t->key%1000))
				printf("error :%d\n",i);
		}
		hash_free(h);
	}
}
#endif
