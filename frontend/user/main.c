/*
/The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#if defined(KERNEL_MODE)
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/slab.h>

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#endif

#include "bdbm_drv.h"
#include "umemory.h"
#include "params.h"
#include "ftl_params.h"
#include "debug.h"
#include "userio.h"
#include "ufile.h"

#include "llm_noq.h"
#include "llm_mq.h"
#include "hlm_nobuf.h"
#include "hlm_buf.h"
#include "hlm_dftl.h"
#include "hlm_rsd.h"
#include "devices.h"
#include "pmu.h"

#include "algo/no_ftl.h"
#include "algo/block_ftl.h"
#include "algo/page_ftl.h"
#include "algo/dftl.h"

/* main data structure */
bdbm_drv_info_t* _bdi = NULL;

#define NUM_THREADS	20
/*#define NUM_THREADS	20*/
/*#define NUM_THREADS	10*/
/*#define NUM_THREADS	1*/

#include "bdbm_drv.h"
#include "uatomic64.h"

uint64_t read_cnt[21];
uint64_t  write_cnt[21];
static int w_cnt = 0;

void* host_thread_fn_read_tracefile (size_t offset, int size);
void* host_thread_fn_write_tracefile (size_t offset, int size, int is_sync);


void write_done (void* req)
{
	uint32_t j = 0;
	bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)req;
	bdbm_free (blkio_req->bi_bvec_ptr[0]);
	bdbm_free( blkio_req->user2);
    bdbm_free (blkio_req);


//	bdbm_sema_unlock ((bdbm_sema_t*)blkio_req->user2);

	/*
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	bdbm_free (blkio_req);
	*/

	//bdbm_msg ("write_done - 2");
}

/*void* host_thread_fn_write (void *data) */
void* host_thread_fn_write (int lpa) 
{
	int i = 0;
	uint32_t j = 0;
	int offset = lpa*8; /* sector (512B) */
	int size = 8; /* 512B * 8 * 32 = 128 KB */

	//bdbm_msg ("write_submit - 1");

	for (i = 0; i < 1; i++) {
		bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

		/* build blkio req */
		blkio_req->bi_rw = REQTYPE_WRITE;
		blkio_req->bi_offset = offset;
		blkio_req->bi_size = size;
		blkio_req->bi_bvec_cnt = size / 8;
		blkio_req->cb_done = write_done;
		blkio_req->user = (void*)blkio_req;
		blkio_req->user2 = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));

		bdbm_sema_init ((bdbm_sema_t*)blkio_req->user2);

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
			blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
			blkio_req->bi_bvec_ptr[j][0] = (uint8_t)w_cnt;
			blkio_req->bi_bvec_ptr[j][1] = (uint8_t)w_cnt+1;
			blkio_req->bi_bvec_ptr[j][2] = (uint8_t)w_cnt+2;;
		}

		/* send req to ftl */
		//bdbm_msg ("write_submit - 2");

		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);
		_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
		/*bdbm_msg (" .... waiting for ack from dm");*/
		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);

		//bdbm_msg ("write_submit - 3");

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
			bdbm_free (blkio_req->bi_bvec_ptr[j]);
		bdbm_free (blkio_req);

		/* increase offset */
		offset += size;
		w_cnt++;
	}

	//bdbm_msg ("write_submit - 4");

	/*pthread_exit (NULL);*/
	return NULL;
}

void read_done (void* req)
{
	uint32_t j = 0;
	bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)req;

#if 0
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
		if (blkio_req->bi_bvec_ptr[j][0] != (uint8_t)blkio_req->bi_offset) {
			bdbm_msg ("mismatch: %u %u", 
				blkio_req->bi_bvec_ptr[j][0], 
				(uint8_t)blkio_req->bi_offset);
		}
		if (blkio_req->bi_bvec_ptr[j][1] != (uint8_t)blkio_req->bi_size) {
			bdbm_msg ("mismatch: %u %u", 
				blkio_req->bi_bvec_ptr[j][1], 
				(uint8_t)blkio_req->bi_size);
		}
		if (blkio_req->bi_bvec_ptr[j][2] != (uint8_t)(blkio_req->bi_offset + blkio_req->bi_size)) {
			bdbm_msg ("mismatch: %u %u", 
				blkio_req->bi_bvec_ptr[j][2], 
				(uint8_t)(uint8_t)(blkio_req->bi_offset + blkio_req->bi_size));
		}
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	}
	bdbm_free (blkio_req);
#endif
	/*bdbm_msg ("read_done - 1");*/

	bdbm_sema_unlock ((bdbm_sema_t*)blkio_req->user2);

	/*
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	bdbm_free (blkio_req);
	*/

	//bdbm_msg ("read_done - 2");
}

/*void* host_thread_fn_read (void *data) */
void* host_thread_fn_read (int lpa) 
{
	int i = 0;
	uint32_t j = 0;
	int offset = lpa*8; /* sector (512B) */
	int size = 8; /* 512B * 8 * 32 = 128 KB */

	for (i = 0; i < 1; i++) {
		bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

		/*bdbm_msg ("read_submit - 1");*/

		/* build blkio req */
		blkio_req->bi_rw = REQTYPE_READ;
		blkio_req->bi_offset = offset;
		blkio_req->bi_size = size;
		blkio_req->bi_bvec_cnt = size / 8;
		blkio_req->cb_done = read_done;
		blkio_req->user = (void*)blkio_req;
		blkio_req->user2 = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));

		bdbm_sema_init ((bdbm_sema_t*)blkio_req->user2);

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
			blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
			if (blkio_req->bi_bvec_ptr[j] == NULL) {
				bdbm_msg ("bdbm_malloc () failed");
				exit (-1);
			}
		}

		/* send req to ftl */
		//bdbm_msg ("read_submit - 2");

		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);
		_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
		//bdbm_msg (" .... waiting for ack from dm");
		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);

		//bdbm_msg ("read_submit - 3");

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
			bdbm_free (blkio_req->bi_bvec_ptr[j]);
		bdbm_free (blkio_req);

		/* increase offset */
		offset += size;
	}

	/*pthread_exit (NULL);*/
	return NULL;
}

void erase_done (void* req)
{
	bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)req;
	bdbm_sema_unlock ((bdbm_sema_t*)blkio_req->user2);

	//bdbm_msg ("erase_done");
}

/*void* host_thread_fn_read (void *data) */
void* host_thread_fn_erase (int lpa) 
{
	int i = 0;
	uint32_t j = 0;
	int offset = lpa * 8; /* sector (512B) */
	int size = 8; /* 512B * 8 * 32 = 128 KB */

	for (i = 0; i < 1; i++) {
		bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

		/* build blkio req */
		blkio_req->bi_rw = REQTYPE_TRIM;
		blkio_req->bi_offset = offset;
		blkio_req->bi_size = size;
		blkio_req->bi_bvec_cnt = size / 8;
		blkio_req->cb_done = erase_done;
		blkio_req->user = (void*)blkio_req;
		blkio_req->user2 = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));

		bdbm_sema_init ((bdbm_sema_t*)blkio_req->user2);

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
			blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
			if (blkio_req->bi_bvec_ptr[j] == NULL) {
				bdbm_msg ("bdbm_malloc () failed");
				exit (-1);
			}
		}

		/* send req to ftl */
		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);
		_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
		bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);

		for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
			bdbm_free (blkio_req->bi_bvec_ptr[j]);
		bdbm_free (blkio_req);

		/* increase offset */
		offset += size;
	}

	/*pthread_exit (NULL);*/
	return NULL;
}



int main (int argc, char** argv)
{
	int loop_thread, i;

	pthread_t thread[NUM_THREADS];
	int thread_args[NUM_THREADS];
        int fd;
        FILE* fp;
        char tmps[64];
        char ops[16];
        float tmpf;
        int tmp;
		int sec,usec;
		long long time,elapsed_time; // time(ms)
        size_t offset, limit=0;
        int size;
		int nr;
		bdbm_stopwatch_t sw;

		//bdbm_msg ("[main] run ftlib... (%d)", sizeof (bdbm_llm_req_t));

        //bdbm_msg ("[user-main] initialize bdbm_drv");
        if ((_bdi = bdbm_drv_create ()) == NULL) {
                bdbm_error ("[kmain] bdbm_drv_create () failed");
                return -1;
        }

        if (bdbm_dm_init (_bdi) != 0) {
                bdbm_error ("[kmain] bdbm_dm_init () failed");
                return -1;
        }

        bdbm_drv_setup (_bdi, &_userio_inf, bdbm_dm_get_inf (_bdi));
        bdbm_drv_run (_bdi);

#if 0
        do {
                bdbm_msg ("[main] start writes");
                for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
                        thread_args[loop_thread] = loop_thread;
                        pthread_create (&thread[loop_thread], NULL,
                                        &host_thread_fn_write,
                                        (void*)&thread_args[loop_thread]);
                }

                bdbm_msg ("[main] wait for threads to end...");
                for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
                        pthread_join (thread[loop_thread], NULL);
                }

                bdbm_msg ("[main] start reads");
                for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
                        thread_args[loop_thread] = loop_thread;
                        pthread_create (&thread[loop_thread], NULL,
                                        &host_thread_fn_read,
                                        (void*)&thread_args[loop_thread]);
                }

                bdbm_msg ("[main] wait for threads to end...");
                for (loop_thread = 0; loop_thread < NUM_THREADS; loop_thread++) {
                        pthread_join (thread[loop_thread], NULL);
                }

        } while (0);
#endif

        /*host_thread_fn_write (NULL);*/

        /*
           for (i = 0; i < 256*64; i++) {
           host_thread_fn_write (i);
           host_thread_fn_read (i);
           }


           printf ("erasing....");
           for (i = 0; i < 64; i++) {
           host_thread_fn_erase (i);
           }
           */
        limit = _bdi->parm_dev.nr_channels * _bdi->parm_dev.nr_chips_per_channel *
                _bdi->parm_dev.nr_blocks_per_chip *
                _bdi->parm_dev.nr_pages_per_block *
                _bdi->parm_dev.page_main_size / 512;

        fd = open(argv[1], O_RDONLY);
        fp = fdopen(fd, "r");
		bdbm_stopwatch_start(&sw);

        while(!feof(fp)){

                fscanf(fp, "%d,%d %d %d %d.%d %d %[^ ] %[^ ] %zu + %d %[^\n]", \
                                &tmp, &tmp, &tmp, &tmp, &sec, &usec, &tmp, \
                                tmps, ops, &offset, &size, tmps);


				
                //printf("%s,%zu,%d, [%d]\n",  ops, offset, size, w_cnt);
                if(offset + size  >= limit){
				//	printf("offset >= limit\n");
                        continue;
				}

				/*
				time = sec*1000 + usec/(1000*1000);
				while(1) {
					elapsed_time = bdbm_stopwatch_get_elapsed_time_ms(&sw);
				//	printf("time [%lld], elapsed time [%lld]\n", time, elapsed_time);
					if(time <= elapsed_time )
						break;
					usleep(1000);
				}
				*/
				
                if(ops[0] == 'W'){
					if(ops[1] == 'S')
                        host_thread_fn_write_tracefile (offset, size,1);
					else
						host_thread_fn_write_tracefile(offset, size, 0);
                }
                else if(ops[0] == 'R'){
                        host_thread_fn_read_tracefile (offset, size);
                }

        }





        bdbm_msg ("[main] destroy bdbm_drv");
        bdbm_drv_close (_bdi);
        bdbm_dm_exit (_bdi);
        bdbm_drv_destroy (_bdi);

		/*
		nr = 1;
		for(nr=4; nr>=1; nr--) 
		printf("%d write: %lld\n", 4*nr, write_cnt[nr]);
		*/

	        bdbm_msg ("[main] done");

        return 0;
}

void* host_thread_fn_write_tracefile (size_t offset, int size, int is_sync) 
{
        int i = 0;
        uint32_t j = 0;
	
		//bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(_bdi);

        for (i = 0; i < 1; i++) {

                bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

				if(is_sync == 1)
					blkio_req->is_sync = 1;
				else
					blkio_req->is_sync = 0;
                /* build blkio req */
                blkio_req->bi_rw = REQTYPE_WRITE;
                blkio_req->bi_offset = offset;
                blkio_req->bi_size = size;
                blkio_req->bi_bvec_cnt = size / 8;
                blkio_req->cb_done = write_done;
                blkio_req->user = (void*)blkio_req;

               blkio_req->user2 = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));

				blkio_req->blk_number = w_cnt;
				blkio_req->bi_bvec_index = 0;
				atomic64_set(&blkio_req->reqs_done, 0);
                bdbm_sema_init ((bdbm_sema_t*)blkio_req->user2);

                blkio_req->bi_bvec_ptr[0] = (uint8_t*)bdbm_malloc (4096);
                for (j = 1; j < blkio_req->bi_bvec_cnt; j++) {
                        blkio_req->bi_bvec_ptr[j] = blkio_req->bi_bvec_ptr[0];
                }

//				bdbm_msg("host_inf->make_req start");
				//bdbm_msg("p->nr_host_reqs %llu [write_tracefile]",p->nr_host_reqs);


             // bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);
                _bdi->ptr_host_inf->make_req (_bdi, blkio_req);
			//	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);


//				bdbm_msg("host_inf->make_req done");

                offset += size;
                w_cnt++;
        }
//		bdbm_msg("last fn_write tracefile");

        return NULL;
}

void* host_thread_fn_read_tracefile (size_t offset, int size) 
{
        int i = 0;
        uint32_t j = 0;

        for (i = 0; i < 1; i++) {
                bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

                /* build blkio req */
                blkio_req->bi_rw = REQTYPE_READ;
                blkio_req->bi_offset = offset;
                blkio_req->bi_size = size;
                blkio_req->bi_bvec_cnt = size / 8;
                blkio_req->cb_done = write_done;  // why write_done?
                blkio_req->user = (void*)blkio_req;
                blkio_req->user2 = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));

                bdbm_sema_init ((bdbm_sema_t*)blkio_req->user2);

                blkio_req->bi_bvec_ptr[0] = (uint8_t*)bdbm_malloc (4096);
                for (j = 1; j < blkio_req->bi_bvec_cnt; j++) {
                        blkio_req->bi_bvec_ptr[j] = blkio_req->bi_bvec_ptr[0];
                }

//                bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);
                _bdi->ptr_host_inf->make_req (_bdi, blkio_req);
//                bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user2);

//                bdbm_free (blkio_req->bi_bvec_ptr[0]);
//              bdbm_free (blkio_req);

                offset += size;
                w_cnt++;
        }

        return NULL;
}
