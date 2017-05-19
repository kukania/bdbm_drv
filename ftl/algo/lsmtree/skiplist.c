#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<time.h>
#include<fcntl.h>
#include<unistd.h>
#include"utils.h"
#include"skiplist.h"
#include"bloomfilter.h"
extern MeasureTime mt;
snode *snode_init(snode *node){
	for(int i=0; i<MAX_L; i++)
		node->list[i]=NULL;
	return node;
}
skiplist *skiplist_init(skiplist *point){
	point->level=1;
	point->start=point->end=point->size=0;
	point->header=(snode*)malloc(sizeof(snode));
	point->tail=(snode*)malloc(sizeof(snode));
	point->tail->key=INT_MAX;

	for(int i=0; i<MAX_L; i++) point->header->list[i]=point->tail;
	return point;
}

snode *skiplist_find(skiplist *list, KEYT key){
	if(list->size==0) return NULL;
	int level=list->level;

	snode* temp=list->tail;
	for(int i=level; i>=1; i--){
		if(list->header->list[i]->key<key){
			temp=list->header->list[i];
			level=i;
			break;
		}
		else if(list->header->list[i]->key==key)
			return list->header->list[i];
	}
	while(temp!=list->tail){
		if(temp->key==key)
			return temp;
		else{
			while(level>1 && temp->list[level]->key>key){
				level--;
			}
			temp=temp->list[level];
			if(temp==NULL) return NULL;
			if(temp->key==key)
				return temp;
		}
	}
	return NULL;
}
static snode * skiplist_find_level(int key, int level, skiplist *list){
	snode *temp=list->header;
	int _level=level;
	while(temp->list[_level]->key<key){
		temp=temp->list[_level];
	}
	return temp;
}
static int getLevel(){
	int level=1;
	int temp=rand();
	while(temp%4==1){
		temp=rand();
		level++;
		if(level+1>=MAX_L) break;
	}
	return level;
}
snode *skiplist_insert_node(skiplist *list,snode *new_node){
	snode *check_temp;
	if((check_temp=skiplist_find(list,new_node->key))!=NULL){
		check_temp->lr=new_node->lr;
		return NULL;
	}
	int new_level=getLevel();
	if(new_level>list->level)
		list->level=new_level;
	for(int i=new_level; i>=1; i--){
		snode *temp=skiplist_find_level(new_node->key,i,list);
		new_node->list[i]=temp->list[i];
		temp->list[i]=new_node;
	}
	list->size++;
	if(list->size==1){
		list->start=list->end=new_node->key;
	}
	else if(list->start>new_node->key) list->start=new_node->key;
	else if(list->end<new_node->key)list->end=new_node->key;
	return new_node;
}
snode *skiplist_insert(skiplist *list,KEYT key, bdbm_llm_req_t *lr, bool flag){
	snode *new_node=NULL;
	new_node=skiplist_find(list,key);
	if(new_node!=NULL){
		if(flag) new_node->lr=lr;
		return new_node;
	}
	int new_level=getLevel();
	new_node=(snode*)malloc(sizeof(snode));
	new_node=snode_init(new_node);
	if(new_level>list->level)
		list->level=new_level;
	for(int i=new_level; i>=1; i--){
		snode *temp=skiplist_find_level(key,i,list);
		new_node->list[i]=temp->list[i];
		temp->list[i]=new_node;
	}
	new_node->key=key;
	memcpy(&new_node->lr,lr,sizeof(new_node->lr));

	list->size++;
	if(list->size==1){
		list->start=list->end=key;
	}
	else if(list->start>key) list->start=key;
	else if(list->end<key)list->end=key;
	return new_node;
}
/*
void skiplist_dump(skiplist *list){
	for(int i=list->level; i>=1; i--){
		printf("level dump - %d\n",i);
		snode *temp=list->header->list[i];
		while(temp!=list->tail){
			printf("%d ",temp->key);
			temp=temp->list[i];
		}
		printf("\n\n");
	}
}*/
void skiplist_ex_value_free(skiplist *list){
	free(list->header);
	free(list->tail);
	free(list);
}
void skiplist_meta_free(skiplist *list){
	snode *temp=list->header->list[1];
	snode *temp_n=temp->list[1];
	while(temp!=list->tail){
		free(temp);
		temp=temp_n;
		temp_n=(temp==list->tail?NULL:temp_n->list[1]);
	}
	skiplist_ex_value_free(list);
}
void skiplist_free(skiplist *list){
	snode *temp=list->header->list[1];
	snode *temp_n=temp->list[1];
	while(temp!=list->tail){
		free(temp);
		temp=temp_n;
		temp_n=(temp==list->tail?NULL:temp_n->list[1]);
	}
	skiplist_ex_value_free(list);
}

void skiplist_sktable_free(sktable *f){
	free(f);
}
sktable *skiplist_read(int pbn, int fd){
	sktable *res=skiplist_meta_read(pbn,fd);
	return res;
}
sktable *skiplist_meta_read(int pbn, int fd){
	sktable *res=malloc(sizeof(sktable));
	lseek(fd,SKIP_BLOCK*pbn,SEEK_SET);
	read(fd,res,SKIP_METAS);
	//count of read ++ need
	return res;
}
/*
sktable* skiplist_data_read(sktable *list, int pbn, int fd){
	lseek(fd,SKIP_BLOCK*pbn+SKIP_META,SEEK_SET);
	list->value=malloc(KEYN*PAGESIZE);
	read(fd,list->value,KEYN*PAGESIZE);
	return list;
}
*/
keyset *skiplist_keyset_find(sktable *t, KEYT key){
	int start=0, end=KEYN-1;
	int mid=(start+end)/2;
	while(1){
		if(start>end) return NULL;
		if(key==t->meta[mid].key)
			return &t->meta[mid];
		else if(key<t->meta[mid].key){
			end=mid-1;
			mid=(start+end)/2;
		}
		else if(key> t->meta[mid].key){
			start=mid+1;
			mid=(start+end)/2;
		}
	}
}
/*
bool skiplist_keyset_read(keyset* k,char *res,int fd){
	if(k==NULL)
		return false;
	else{
		lseek(fd,SKIP_BLOCK*k->addr+SKIP_META+PAGESIZE*k->number,SEEK_SET);
		if(read(fd,res,PAGESIZE)!=-1)
			return true;
		else 
			return false;
	}
}
*/
int skiplist_write(skiplist *data, 
		int key, 
		bdbm_page_ftl_private_t *p,
		bdbm_drv_info_t *bdi,
		int fd){
	skiplist_meta_write(data,key,p,bdi,fd);
	//skiplist_data_write(data,key,fd);
	return 0;
}
int skiplist_meta_write(skiplist *data,
		int key,
		bdbm_page_ftl_private_t *p,
		bdbm_drv_info_t *bdi,
		int fd){
	lseek(fd,SKIP_BLOCK*key,SEEK_SET);
	char *buf=malloc(SKIP_METAS);
	snode *temp=data->header->list[1];
	int len=0;
	
	bdbm_device_params_t *np=BDBM_GET_DEVICE_PARAMS(bdi);
	bdbm_abm_block_t *b=NULL;
	/*ppa setting*/
	uint64_t curr_channel=p->curr_puid % np->nr_channels;
	uint64_t curr_chip=p->curr_putid/np->nr_channels;
	
	while(temp!=data->tail){
		memcpy(&buf[len],temp,SKIP_META);
		len+=SKIP_META;
		//ppa setting needed in lr
		//phyaddr

		b=p->ac_bab[curr_channel * np->nr_chips_per_channel +curr_chip];
		temp->ppa.channel_no=b->channel_no;
		temp->ppa.chip_no=b->chip_no;
		temp->ppa.block_no=b->block_no;
		temp->ppa.page_no=p->curr_page_ofs;
		temp->ppa.subpage_no=p->curr_subpage_ofs;
		temp->ppa.punit_id=BDBM_GET_PUNIT_ID(bdi,temp->ppa);

		temp->lr->subpage_ofs=p->curr_subpage_ofs;
		temp->lr->nr_valid=1;

		if((p->curr_puid+1) == p->nr_punits){
			p->curr_puid=0;
			p->curr_subpage_ofs=0;
			p->curr_page_ofs++;
			
			if(p->curr_page_ofs == np->nr_pages_per_block){
				b->block_no++;
				p->curr_page_ofs=0;
			}
		}else{
			p->curr_puid++;
		}
		temp=temp->list[1];		
	}

	write(fd,buf,SKIP_METAS);
	//thread for lr write......;
	free(buf);
	return 0;
}
/*
int skiplist_data_write(skiplist *data,int key,int fd){
	lseek(fd,SKIP_BLOCK*(key)+SKIP_META,SEEK_SET);
	snode *temp=data->header->list[1];
	char t_data[PAGESIZE];
	memcpy(t_data,temp->value,PAGESIZE);
	int count=0;
	while(temp!=data->tail){
		temp->addr=key; temp->number=count++;
		if(write(fd,temp->value,PAGESIZE)==0){
			printf("write error!\n");
			return -1;
		}
		temp=temp->list[1];
	}
	return 0;
}
*/
skiplist *skiplist_cut(skiplist *list,int num){
	if(list->size<num) return NULL;
	skiplist *res=(skiplist*)malloc(sizeof(skiplist));
	res=skiplist_init(res);
	snode *h=res->header;
	snode *t=res->tail;
	res->start=INT_MAX;
	for(int i=0; i<num; i++){
		snode *temp=skiplist_pop(list);
		if(temp==list->tail) return NULL;
		res->start=temp->key>res->start?res->start:temp->key;
		res->end=temp->key>res->end?temp->key:res->end;
		h->list[1]=temp;
		temp->list[1]=t;
		h=temp;
	}
	res->size=num;
	return res;
}

skIterator *skiplist_getIterator(skiplist* list){
	skIterator *res=(skIterator*)malloc(sizeof(skIterator));
	res->now=list->header->list[1];
	res->mylist=list;
	return res;
}

snode *sk_getNext(skIterator* iter){
	if(iter->now==iter->mylist->tail) return NULL;
	else{
		snode *res=iter->now;
		iter->now=iter->now->list[1];
		return res;
	}
}
snode *skiplist_pop(skiplist *list){
	snode *res=list->header->list[1];
	for(int i=list->level; i>=1; i--){
		if(res->list[i]==NULL) continue;
		list->header->list[i]=res->list[i];
		res->list[i]=NULL;
		if(i==list->level && list->header->list[i]==list->tail){
			list->level--;
		}
	}
	list->size--;
	return res;
}
void skiplist_traversal(skiplist * data){
	snode *list=data->header->list[1];
	while(list!=data->tail){
		printf("%d\n",list->key);
		list=list->list[1];
	}
}
#ifdef DEBUG

int main(){
	skiplist * temp,*temp2;
	int fd=open("skiplist_data.skip",O_RDWR|O_CREAT|O_TRUNC,0666);
	if(fd==-1){ printf("file open error!\n"); return -1;}
	bdbm_page_ftl_private_t *p;
	for(int j=0; j<2; j++){
		temp=(skiplist*)malloc(sizeof(skiplist));
		skiplist_init(temp);
		for(int i=KEYN+j*KEYN; i>=1+j*KEYN; i--){
			snode *test=skiplist_insert(temp,i,NULL,true);
		}
		skiplist_write(temp,j,p,fd);
		skiplist_free(temp);
	}

	for(int j=0; j<2; j++){
		sktable *table=skiplist_meta_read(j,fd);
		for(int i=1+j*KEYN; i<KEYN+1+j*KEYN; i++){
			if((skiplist_keyset_find(table,i))!=NULL){
				printf("%d\n",i);
			}
			else{
				printf("error : %d\n");
			}
		}
		free(table);
	}
	printf("\n");
	close(fd);
}
#endif
