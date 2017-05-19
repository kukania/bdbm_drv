#include"bptree.h"
#include"lsmtree.h"
#include"bloomfilter.h"
#include"skiplist.h"
#include"measure.h"
#include"hlm_reqs_pool.h"


#include<time.h>
#include<string.h>
#include<fcntl.h>
#include<sys/time.h>
static int filenumber=0;

int memtable_get_count;

int readbuffer_get_count;
int sstable_get_count;
struct timeval compaction_MAX;
struct MeasureTime mt;
int sstcheck;
int write_data(skiplist *data,
		bdbm_page_ftl_private_t *p,
		bdbm_drv_info_t *bdi
		){
	if(skiplist_write(data,filenumber,p,bdi,LSM->fd)!=-1)
		return filenumber++;
	else return -1;
}
lsmtree* init_lsm(lsmtree *res){
	measure_init(&mt);
	LSM->memtree=(skiplist*)malloc(sizeof(skiplist));
	LSM->memtree=skiplist_init(LSM->memtree);
	LSM->buf.data=NULL;
	for(int i=0;i<LEVELN; i++)LSM->buf.disk[i]=NULL;
	LSM->buf.last=malloc(sizeof(skiplist));
	skiplist_init(LSM->buf.last);
	LSM->filter=malloc(sizeof(bloomfilter));
	bloomfilter_init(LSM->filter);
	if(SEQUENCE)
		LSM->fd=open("skiplist_data.skip",O_RDWR|O_CREAT|O_TRUNC,0666);
	else
		LSM->fd=open("skiplist_data_r.skip",O_RDWR|O_CREAT|O_TRUNC,0666);
	if(LSM->fd==-1){
		printf("file open error!\n");
		return NULL;
	}
	return res;
}
void buffer_free(buffer *buf){
	skiplist_free(buf->last);
	free(buf->data);
	for(int i=0; i<LEVELN; i++){
		if(buf->disk[i]!=NULL) level_free(buf->disk[i]);
	}
}

void *lsm_free(lsmtree *input){
	skiplist_free(input->memtree);
	bloomfilter_free(input->filter);
	buffer_free(&input->buf);
	close(input->fd);
	free(input);
}
void *lsm_clear(lsmtree *input){
	skiplist_free(input->memtree);
	input->memtree=(skiplist*)malloc(sizeof(skiplist));
	input->memtree=skiplist_init(input->memtree);
	input->filter=malloc(sizeof(bloomfilter));
	bloomfilter_init(input->filter);
}
bool put(KEYT key, bdbm_llm_req_t* lr,
		bdbm_page_ftl_private_t *p, 
		bdbm_drv_info_t *bdi){
	while(1){
		if(LSM->memtree->size<KEYN){
			snode *temp;
			if(LSM->memtree->size!=0 &&(temp=skiplist_find(LSM->memtree,key))!=NULL){
				temp->lr=lr;
				return true;
			}
			skiplist_insert(LSM->memtree,key,lr,true);
			bloomfilter_set(LSM->filter,&key,sizeof(key));
			return true;
		}
		else{
			merge(0,p,bdi);
		}
	}
	return false;
}

int get(KEYT key,bdbm_drv_info_t *bdi,bdbm_llm_req_t *lr){
	sstcheck=0;
	struct timeval start,end,re;
	if(bloomfilter_get(LSM->filter,&key,sizeof(key))){
		snode* res=skiplist_find(LSM->memtree,key);
		if(res!=NULL){
			hlm_reqs_pool_relocate_kp(lr,0);
			if(bdi->ptr_llm_inf->make_req(bdi,lr)!=0){
				bdbm_error("make_req() failed in lsmtree/get");
				bdbm_bug_on(1);
				return 1;
			}
		}
	}
	else if(LSM->buf.data!=NULL){
		if(bloomfilter_get(LSM->buf.filter,&key,sizeof(key))){
			if((skiplist_keyset_find(LSM->buf.data,key))!=NULL){
			hlm_reqs_pool_relocate_kp(lr,0);
				if(bdi->ptr_llm_int->make_req(bdi,lr)!=0){
					bdbm_error("maek_req() failed in lsmtree/get");
					bdbm_bug_on(1);
					return 1;
				}
			}
		}
	}
	for(int i=0; i<LEVELN; i++){
		if(LSM->buf.disk[i]!=NULL){
			bpIterator* iter=level_find(LSM->buf.disk[i],key);
			Entry *temp=bp_getNext(iter);
			while(temp!=NULL){
				if(bloomfilter_get(temp->filter,&key,sizeof(key))){
					MS(&mt);

					MS(&mt);
					pmu_update_sw(bdi,lr);
					pmu_update_q(bdi,lr);
					sktable* temp_s=skiplist_meta_read(temp->pbn,LSM->fd);
					
					sstcheck++;
					/*read cnt++*/
					ME(&mt,"meta_read");

					if((skiplist_keyset_find(temp_s,key))!=NULL){
						if(LSM->buf.data!=NULL){
							free(LSM->buf.data);
						}
						LSM->buf.data=temp_s;
						LSM->buf.filter=temp->filter;
						sstable_get_count++;
						hlm_reqs_pool_relocate_kp(lp,0);
						if(bdi->ptr_llm_int->make_req(bdi,lr)!=0){
							bdbm_error("make_req() failed in lsmtree/get");
							bdbm_bug_on(1);
						}
						ME(&mt,"SST-succ");
						//printf("%s:%d sec and %.5f\n","sstcheck",sstcheck,0.0f);
						heap_free(iter);
						return 1;
					}
					MP(&mt);
					free(temp_s);
				}
				temp=bp_getNext(iter);
			}
		}
		else break;
	}

	snode *res=skiplist_find(LSM->buf.last,key);
	if(res!=NULL){
		if(bdi->ptr_llm_int->make_req(bdi,lr)!=0){
			bdbm_error("make_req() failed in lsmtree/get");
			bdbm_bug_on(1);
		}
		return 1;
	}
	return 0;
}
lsmtree* lsm_reset(lsmtree* input){
	LSM->memtree=(skiplist*)malloc(sizeof(skiplist));
	LSM->memtree=skiplist_init(LSM->memtree);
	LSM->filter=(bloomfilter*)malloc(sizeof(bloomfilter));
	bloomfilter_init(LSM->filter);
	return input;
}
bool compaction(level *src, level *des,bdbm_page_ftl_private_t* p){
	struct timeval start,end;
	gettimeofday(&start,NULL);
	skiplist *res=LSM->buf.last;
	//check for duplicated!
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
	for(int i=0; temp_[i]!=NULL; i++){
		temp=temp_[i];
		sktable* readS=skiplist_read(temp->pbn,LSM->fd);
		for(int j=0; j<KEYN; j++){
			skiplist_insert(res,readS->meta[j].key,NULL,true);
		}
		skiplist_sktable_free(readS);
		level_delete(src,temp);
	}
	skiplist *t;
	while((t=skiplist_cut(res,KEYN))!=NULL){
		bloomfilter	*filter;
		filter=(bloomfilter*)malloc(sizeof(bloomfilter));
		bloomfilter_init(filter);
		snode *r_iter=t->header->list[1];
		while(r_iter!=t->tail){
			bloomfilter_set(filter,&r_iter->key,sizeof(r_iter->key));
			r_iter=r_iter->list[1];
		}
		temp=make_entry(t->start,t->end,write_data(t,p),filter);
		level_insert(des,temp);
		skiplist_free(t);
	}

	printf("%s:%d sec and %.5f\n","compactionsize",4000-res->size,0.0f);
	gettimeofday(&end,NULL);
	struct timeval resT;
	timersub(&end,&start,&resT);
	return true;
}

bool merge(int t,bdbm_page_ftl_private_t *p){
	struct timeval start,end,resT;
	bool flag=false;
	while(1){
		if(t==0){
			if(LSM->buf.disk[0]==NULL){
				LSM->buf.disk[0]=(level*)malloc(sizeof(level));
				level_init(LSM->buf.disk[0],MUL);
			}
			if(LSM->buf.disk[0]->size<LSM->buf.disk[0]->m_size){
				Entry *temp=make_entry(LSM->memtree->start,LSM->memtree->end,write_data(LSM->memtree,p),LSM->filter);
				level_insert(LSM->buf.disk[0],temp);
				lsm_clear(LSM);
			}
			else{
				t++; continue;
			}
			if(flag){
				if(end.tv_usec>0 || end.tv_sec>0)
					compaction_MAX=resT;
			}
			return true;
		}
		else{
			if(LSM->buf.disk[t]==NULL){
				LSM->buf.disk[t]=(level*)malloc(sizeof(level));
				level_init(LSM->buf.disk[t],LSM->buf.disk[t-1]->size*MUL);
			}
			if(!flag)
				gettimeofday(&start,NULL);
			if(LSM->buf.disk[t]->size+COMPACTIONNUM<=LSM->buf.disk[t]->m_size){
				flag=true;
				MS(&mt);
				compaction(LSM->buf.disk[t-1],LSM->buf.disk[t],p);
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
//depcrated
void level_traversal(level* t){
	Node *temp=level_find_leafnode(t,0);

}
