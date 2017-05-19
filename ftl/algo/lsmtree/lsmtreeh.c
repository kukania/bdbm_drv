#include"lsmtreeh.h"
#include"bloomfilter.h"
#include"measure.h"
#include<time.h>
#include<string.h>
#include<fcntl.h>
#include<sys/time.h>
static int filenumber=0;
extern MeasureTime mt;
extern int memtable_get_count;
extern int readbuffer_get_count;
extern int sstable_get_count;
extern struct timeval compaction_MAX;
int sstcheck;
int write_data(Hash *data){
	if(hash_write(data,filenumber,LSM->fd)!=-1)
		return filenumber++;
	else return -1;
}
lsmtree* init_lsm(lsmtree *res){
	measure_init(&mt);
	LSM->memtree=(Hash*)malloc(sizeof(Hash));
	LSM->memtree=hash_init(LSM->memtree);
	LSM->buf.data=NULL;
	for(int i=0;i<LEVELN; i++)LSM->buf.disk[i]=NULL;
	LSM->buf.last=malloc(sizeof(skiplist));
	skiplist_init(LSM->buf.last);
	LSM->filter=malloc(sizeof(bloomfilter));
	bloomfilter_init(LSM->filter);
	if(SEQUENCE)
		LSM->fd=open("data/hash_data.hash",O_RDWR|O_CREAT|O_TRUNC,0666);
	else
		LSM->fd=open("data/hash_data_r.hash",O_RDWR|O_CREAT|O_TRUNC,0666);
	if(LSM->fd==-1){
		printf("file open error!\n");
		return NULL;
	}
	return res;
}
void buffer_free(buffer *buf){
	skiplist_free(buf->last);
	for(int i=0; i<LEVELN; i++){
		if(buf->disk[i]!=NULL) level_free(buf->disk[i]);
	}
}

void *lsm_free(lsmtree *input){
	hash_free(input->memtree);
	bloomfilter_free(input->filter);
	buffer_free(&input->buf);
	close(input->fd);
	free(input);
}
void *lsm_clear(lsmtree *input){
	hash_free(input->memtree);
	input->memtree=(Hash*)malloc(sizeof(Hash));
	input->memtree=hash_init(input->memtree);
	input->filter=malloc(sizeof(bloomfilter));
	bloomfilter_init(input->filter);
}
bool put(KEYT key, char *value){
	while(1){
		if(LSM->memtree->size<KEYN){
			hash_insert(LSM->memtree,key,value);
			bloomfilter_set(LSM->filter,&key,sizeof(key));
			return true;
		}
		else{
			merge(0);
		}
	}
	return false;
}

int get(KEYT key,char *ret){
	sstcheck=0;
	memset(ret,0,PAGESIZE);
	if(bloomfilter_get(LSM->filter,&key,sizeof(key))){
		if(hash_find(LSM->memtree,key,ret)){
			memtable_get_count++;
			return 1;
		}
	}
	else if(LSM->buf.data!=NULL){
		if(bloomfilter_get(LSM->buf.filter,&key,sizeof(key))){
			hnode *res=hash_hnode_find(LSM->buf.data,key);
			if(res!=NULL){
				MS(&mt);
				MS(&mt);
				hash_hnode_read(res,ret,LSM->fd);
				ME(&mt,"node_read");
				readbuffer_get_count++;
				ME(&mt,"buf-succ");
				return 1;
			}
		}
	}
	MS(&mt);
	for(int i=0; i<LEVELN; i++){
		if(LSM->buf.disk[i]!=NULL){
			bpIterator* iter=level_find(LSM->buf.disk[i],key);
			Entry *temp=bp_getNext(iter);
			while(temp!=NULL){
				if(bloomfilter_get(temp->filter,&key,sizeof(key))){
					MS(&mt);
					
					MS(&mt);
					Hash *temp_s=hash_meta_read(temp->pbn,LSM->fd);
					ME(&mt,"meta_read");
					sstcheck++;

					hnode *res=hash_hnode_find(temp_s,key);
					if(res!=NULL){
						if(LSM->buf.data!=NULL){
							free(LSM->buf.data);
						}
						LSM->buf.data=temp_s;
						LSM->buf.filter=temp->filter;
						hash_hnode_read(res,ret,LSM->fd);
						sstable_get_count++;
						ME(&mt,"SST-succ");
						printf("%s:%d sec and %.5f\n","sstcheck",sstcheck,0.0f);
						MP(&mt);
						return 1;
					}
					ME(&mt,"SST-fail");
					free(temp_s);
				}
				temp=bp_getNext(iter);
			}
		}
		else break;
	}
	ME(&mt,"disk");

	snode *temp=skiplist_find(LSM->buf.last,key);
	if(temp!=NULL){
		memcpy(ret,temp->value,PAGESIZE);
		return 1;
	}
	return 0;
}
lsmtree* lsm_reset(lsmtree* input){
	LSM->memtree=(Hash*)malloc(sizeof(Hash));
	LSM->memtree=hash_init(LSM->memtree);
	LSM->filter=(bloomfilter*)malloc(sizeof(bloomfilter));
	bloomfilter_init(LSM->filter);
	return input;
} 
bool compaction(level *src, level *des){
	skiplist *res=LSM->buf.last;
	Entry *compactionSet[COMPACTIONNUM];
	Entry *temp_[COMPACTIONNUM+1];
	for(int i=0; i<COMPACTIONNUM; i++){
		compactionSet[i]=dequeue(src->q);
	}
	int k=0;
	bool lastFlag=true;
	for(int i=0; i<COMPACTIONNUM-1; i++){
		int start1,end1=compactionSet[i]->end;
		start1=compactionSet[i]->key;
		bool flag=true;
		for(int j=COMPACTIONNUM-1; j>i ; j--){
			int start2,end2=compactionSet[j]->end;
			start2=compactionSet[j]->key;
			if(!(start1>end2 || end1<start2)){ 
				flag=false;
			}
			if(lastFlag && j==COMPACTIONNUM-1){
				lastFlag=flag;
			}
			if(!flag) break;
		}	
		if(!flag) temp_[k++]=compactionSet[i];
		else{
			Entry * a=level_entry_copy(compactionSet[i]);
			level_delete(src,compactionSet[i]);
			level_insert(des,a);
		}
	}
	if(lastFlag){
		Entry * a=level_entry_copy(compactionSet[COMPACTIONNUM-1]);
		level_delete(src,compactionSet[COMPACTIONNUM-1]);
		level_insert(des,a);
	}
	else
		temp_[k++]=compactionSet[COMPACTIONNUM-1];

	temp_[k]=NULL;
	Entry *temp;
	char find_V[PAGESIZE];
	for(int i=0; temp_[i]!=NULL; i++){
		temp=temp_[i];
		Hash* readS=hash_read(temp->pbn,LSM->fd);
		for(int j=0; j<KEYN*TABLEFACTOR; j++){
			if(readS->table[j].key!=0){
				int ttt;
				hnode t=readS->table[j];
				hash_find(readS,t.key,find_V);
				memcpy(&ttt,find_V,sizeof(int));
				if(ttt!=t.key){
					printf("%d error!\n",t.key);
					exit(1);
				}
				skiplist_insert(res,t.key,find_V,true);
			}
		}
		hash_free(readS);
		level_delete(src,temp);
	}
	skiplist *t;
	while((t=skiplist_cut(res,KEYN))!=NULL){
		snode *t_header=t->header->list[1];
		Hash *temp_h=malloc(sizeof(Hash));
		hash_init(temp_h);
		bloomfilter *filter=malloc(sizeof(bloomfilter));
		bloomfilter_init(filter);
		while(t_header!=t->tail){
			bloomfilter_set(filter,&t_header->key,sizeof(t_header->key));
			hash_insert(temp_h,t_header->key,t_header->value);
			t_header=t_header->list[1];
		}
		temp=make_entry(temp_h->start,temp_h->end,write_data(temp_h),filter);
		level_insert(des,temp);
		hash_free(temp_h);
		skiplist_free(t);
	}
	return true;
}
bool merge(int t){
	struct timeval start,end,resT;
	MS(&mt);
	bool flag=false;
	while(1){
		if(t==0){
			if(LSM->buf.disk[0]==NULL){
				LSM->buf.disk[0]=(level*)malloc(sizeof(level));
				level_init(LSM->buf.disk[0],MUL);
			}
			if(LSM->buf.disk[0]->size<LSM->buf.disk[0]->m_size){
				MS(&mt);
				Entry *temp=make_entry(LSM->memtree->start,LSM->memtree->end,write_data(LSM->memtree),LSM->filter);
				ME(&mt,"write");
				MS(&mt);
				level_insert(LSM->buf.disk[0],temp);
				ME(&mt,"level_insert");
				lsm_clear(LSM);
			}
			else{
				t++; continue;
			}
			if(flag){
				gettimeofday(&end,NULL);
				timersub(&end,&start,&resT);
#ifdef PRINTOPTION
				printf("compaction time : %ld and %.3f\n",resT.tv_sec,((float)resT.tv_usec)/1000000);
#endif
				timersub(&resT,&compaction_MAX,&end);
				if(end.tv_usec>0 || end.tv_sec>0)
					compaction_MAX=resT;
			}
			ME(&mt,"merge");
			return true;
		}
		else{
			if(LSM->buf.disk[t]==NULL){
				LSM->buf.disk[t]=(level*)malloc(sizeof(level));
				level_init(LSM->buf.disk[t],LSM->buf.disk[t-1]->size*MUL);
			}
			if(!flag)
				gettimeofday(&start,NULL);
			if(LSM->buf.disk[t]->size+COMPACTIONNUM<LSM->buf.disk[t]->m_size){
				flag=true;
				MS(&mt);
				compaction(LSM->buf.disk[t-1],LSM->buf.disk[t]);
				ME(&mt,"compaction");
				t--;
			}
			else{
				t++;
			}
			continue;
		}
	}
	return false;
}

void level_traversal(level* t){
	Node *temp=level_find_leafnode(t,0);
	while(temp!=NULL){
		for(int i=0; i<temp->count; i++){
			Hash *r=hash_read(temp->children[i].entry->pbn,LSM->fd);
			//hash traversal needed
			printf("\n");
		}
		temp=temp->children[MAXC].node;
	}
}
