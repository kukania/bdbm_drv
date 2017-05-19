All: archive
lsm : lsm_n
CC = gcc
INCLUDE := ../../../include
FTL :=../../../ftl
COMMON := ../../../common
DM_COMMON :=../../devices/common

INCLUDES = -I$(PWD)/$(INCLUDE) \
           -I$(PWD)/$(COMMON)/utils \
           -I$(PWD)/$(COMMON)/3rd \
           -I$(PWD)/$(FTL) \
           -I$(PWD)/$(FTL)/lsmtree \
           -I$(PWD)/$(DM_COMMON) \
													  
CFLAGS += -D USE_PMU\
		  -D USER_MODE\
		  -D USE_NEW_RMW\
		  -D CONFIG_ENABLE_MSG\
		  -D CONFIG_ENABLE_DEBUG\

lsm_n: lsmtree.c bloomfilter.c bptree.c hash.c utils.c queue.c utils.h heap.c measure.c skiplist.c #stack.c
		$(CC) $(INCLUDES) $(CFLAGS) $^ -o lsm

skip: skiplist.* 
		$(CC) $(INCLUDES) $(CFLAGS) -D DEBUG -g skiplist.c -o skip

