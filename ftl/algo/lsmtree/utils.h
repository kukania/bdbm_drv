#ifndef __UTIL_H__
#define __UTIL_H__
#include<sys/time.h>
#include<unistd.h>
#include"bdbm_drv.h"
#include"measure.h"
#define KEYT uint64_t
#define FILTERSIZE (1024*12)
#define FILTERFUNC 5
#define FILTERBIT ((1024*12)/8)

#define KEYN 1024
#define PAGESIZE 4096
#define MUL 24
#define LEVELN 5
#define INPUTSIZE 100000

#define TABLEFACTOR 2
#define HASH_BLOCK ((sizeof(bdbm_phyaddr_t)+sizeof(uint64_t))*1024)
#define HASH_META ((sizeof(bdbm_phyaddr_t)+sizeof(uint64_t))*1024)

#define STARTMERGE 0.7
#define ENDMERGE 0.5
#define COMPACTIONNUM 4
#define MAXC 10
#define SEQUENCE 0
#define READTEST
#define GETTEST

#define SNODE_SIZE (4096)
#define SKIP_BLOCK  ((sizeof(bdbm_phyaddr_t)+sizeof(uint64_t))*1024)
#define SKIP_METAS  ((sizeof(bdbm_phyaddr_t)+sizeof(uint64_t))*1024)
#define SKIP_META 	(sizeof(bdbm_phyaddr_t)+sizeof(uint64_t))


/*#ifndef NPRINTOPTION
#define MS(t) measure_start((t))
#define ME(t,s) measure_end((t),(s))
#define MP(t) measure_pop((t))
#else*/
#define MS(t) donothing(t)
#define ME(t,s) donothing2((t),(s))
#define MP(t) donothing((t))
//#endif
#ifndef BOOL
#define BOOL
typedef enum{false,true} bool;
#endif
#endif

