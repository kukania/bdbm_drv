#include "bloomfilter.h"
#include <string.h>
#include <unistd.h>
#include<stdint.h>
#include<stdlib.h>

#define bit_set(v,n)    ((v)[(n) >> 3] |= (0x1 << (0x7 - ((n) & 0x7))))
#define bit_get(v,n)    ((v)[(n) >> 3] &  (0x1 << (0x7 - ((n) & 0x7))))
#define bit_clr(v,n)    ((v)[(n) >> 3] &=~(0x1 << (0x7 - ((n) & 0x7))))
int inthash(int key)
{
	key += ~(key << 15);
	key ^=  (key >> 10);
	key +=  (key << 3);
	key ^=  (key >> 6);
	key += ~(key << 11);
	key ^=  (key >> 16);
	return key;
}


	void
bloomfilter_init(struct bloomfilter *bloomfilter)
{
	bloomfilter->m = FILTERSIZE;
	bloomfilter->k = FILTERFUNC;
	bloomfilter->bit_vector=malloc(FILTERBIT);
	memset(bloomfilter->bit_vector, 0, bloomfilter->m>>3);
}

	void
bloomfilter_set(struct bloomfilter *bloomfilter, const void *key, size_t len)
{
	uint32_t i;
	uint32_t h;

	for (i = 0; i < bloomfilter->k; i++) {
		//murmur3_hash32(key, len, i, &h);
		memcpy(&h,key,sizeof(uint32_t));
		h=inthash(h|i);
		h %= bloomfilter->m;
		bit_set(bloomfilter->bit_vector, h);
	}
}

	int
bloomfilter_get(struct bloomfilter *bloomfilter, const void *key, size_t len)
{
	uint32_t i;
	uint32_t h;

	for (i = 0; i < bloomfilter->k; i++) {
		//murmur3_hash32(key, len, i, &h);
		memcpy(&h,key,sizeof(uint32_t));
		h=inthash(h|i);
		int temp=h;
		h %= bloomfilter->m;
		if (!bit_get(bloomfilter->bit_vector, h))
			return 0;
	}
	return 1;
}
void bloomfilter_free(bloomfilter *filter){
	free(filter->bit_vector);
	free(filter);
}
#ifdef DEBUG
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
bool temp[INPUTSIZE+1];
int main(){
	struct bloomfilter *bloomfilter;
	srand(time(NULL));
	bloomfilter=malloc(sizeof(struct bloomfilter)+2000/8);//byte
	bloomfilter_init(bloomfilter); //52*8 numbers
	for(int i=0; i<1000; i++){
		int t=rand()%INPUTSIZE+1;
		temp[t]=true;
		bloomfilter_set(bloomfilter,&t,sizeof(t));
	}
	int a=0,b=0,c=0;
	for(int i=1; i<=INPUTSIZE; i++){
		if(bloomfilter_get(bloomfilter,&i,sizeof(i))){
			if(temp[i]){
				b++;
			}
			else a++;
		}
		c++;
	}
	printf("%d %d %d\npf:%3.3f %3.3f\n",a,b,c,(float)(a)/(a+b+c),(float)(a+b)/(a+b+c));
}
#endif
