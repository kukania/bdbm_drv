/*
The MIT License (MIT)

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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "bdbm_drv.h"
#include "debug.h"
#include "umemory.h"
#include "userio.h"
#include "params.h"

#include "utime.h"
#include "uthread.h"
#include "hlm_reqs_pool.h"

bdbm_host_inf_t _userio_inf = {
	.ptr_private = NULL,
	.open = userio_open,
	.close = userio_close,
	.make_req = userio_buf_make_req,
	.end_req = userio_end_req,
};

typedef struct {
	bdbm_sema_t count_lock;
	uint64_t nr_host_reqs;
	/*atomic_t nr_host_reqs;*/
	bdbm_sema_t host_lock;
	bdbm_hlm_reqs_pool_t* hlm_reqs_pool;
} bdbm_userio_private_t;

extern uint64_t write_cnt[21];


uint32_t userio_open (bdbm_drv_info_t* bdi)
{
	uint32_t ret;
	bdbm_userio_private_t* p;
	int mapping_unit_size;

	/* create a private data structure */
	if ((p = (bdbm_userio_private_t*)bdbm_malloc_atomic
			(sizeof (bdbm_userio_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc_atomic failed");
		return 1;
	}
	/*atomic_set (&p->nr_host_reqs, 0);*/
	p->nr_host_reqs = 0;
	bdbm_sema_init (&p->host_lock);
	bdbm_sema_init (&p->count_lock);

	bdbm_msg("open_nr_host %llu",p->nr_host_reqs);
	/* create hlm_reqs pool */
	if (bdi->parm_dev.nr_subpages_per_page == 1)
		mapping_unit_size = bdi->parm_dev.page_main_size;
	else
		mapping_unit_size = KERNEL_PAGE_SIZE;

	if ((p->hlm_reqs_pool = bdbm_hlm_reqs_pool_create (
			mapping_unit_size,	/* mapping unit */
			bdi->parm_dev.page_main_size	/* io unit */	
			)) == NULL) {
		bdbm_warning ("bdbm_hlm_reqs_pool_create () failed");
		return 1;
	}

	bdi->ptr_host_inf->ptr_private = (void*)p;

	return 0;
}

void userio_close (bdbm_drv_info_t* bdi)
{
	bdbm_userio_private_t* p = NULL;

	p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);

	/* wait for host reqs to finish */
	//bdbm_msg ("wait for host reqs to finish");
	for (;;) {
		/*if (atomic_read (&p->nr_host_reqs) == 0)*/
		/*break;*/
		bdbm_sema_lock (&p->count_lock);
		if (p->nr_host_reqs == 0)  {
			bdbm_sema_unlock (&p->count_lock);
			break;
		}
		bdbm_sema_unlock (&p->count_lock);

		bdbm_msg ("p->nr_host_reqs = %llu", p->nr_host_reqs--);
		bdbm_thread_msleep (1);
	}

	if (p->hlm_reqs_pool) {
		bdbm_hlm_reqs_pool_destroy (p->hlm_reqs_pool);
	}

	bdbm_sema_free (&p->host_lock);
	bdbm_sema_free (&p->count_lock);

	/* free private */
	bdbm_free_atomic (p);
}


/* TODO: it must be more general... */
void __host_check_ondemand_gc (bdbm_drv_info_t* bdi)
{
	bdbm_ftl_params* dp = BDBM_GET_DRIVER_PARAMS (bdi);
	bdbm_ftl_inf_t* ftl = (bdbm_ftl_inf_t*)BDBM_GET_FTL_INF(bdi);

	if (dp->mapping_type == MAPPING_POLICY_PAGE) {
		uint32_t loop;
		/* see if foreground GC is needed or not */
		for (loop = 0; loop < 10; loop++) {
			if (ftl->is_gc_needed != NULL && 
				ftl->is_gc_needed (bdi, 0)) {
				/* perform GC before sending requests */ 
			//	bdbm_msg ("[host_make_req] trigger GC");
				ftl->do_gc (bdi, 0);
			} else
				break;
		}
	} else {
		bdbm_msg("ondemand gc error");
		/* do nothing */
	}
}



void userio_buf_make_req(bdbm_drv_info_t* bdi, void* bio)
{

	bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);
	bdbm_blkio_req_t* br = (bdbm_blkio_req_t*)bio;
	static bdbm_hlm_req_t* hr = NULL;
	static bdbm_hlm_req_t* rhr = NULL;
	uint64_t req_size;
	uint64_t ret = 0; 
	uint32_t loop=0;
	//bdbm_msg("start buf_make_req");

	bdbm_sema_lock (&p->count_lock);
	p->nr_host_reqs++;
	bdbm_sema_unlock (&p->count_lock);

	if(rhr==NULL) {
		rhr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool);
	}
	if(hr==NULL) {
		hr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool); //hlmsize=0;
	}


	if(br->bi_rw == REQTYPE_READ) {
		req_size = br->bi_bvec_cnt;
		for(loop=0; loop<req_size; loop++) {
			ret = bdbm_hlm_reqs_pool_add(p->hlm_reqs_pool,rhr,br);
			br->bi_bvec_index +=ret;
			br->bi_offset += 8*ret;
			if(bdi->ptr_hlm_inf->make_req(bdi, rhr) !=0) {
				bdbm_error("'bdi->ptr_hlm_inf->make_req' failed");
			}
			rhr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool);
		}

	} else if(br->bi_rw == REQTYPE_WRITE) {
		while(1) 
		{
			__host_check_ondemand_gc(bdi);

			req_size = hr->nr_charged + (br->bi_bvec_cnt - br->bi_bvec_index);
			hr->page_size = bdi->ptr_ftl_inf->get_next_ppa(bdi); // 16K,12K,8K,4K...       80K support
			if(req_size <= hr->page_size) 
			{
//				bdbm_msg("req_size %lld", req_size);
				ret = bdbm_hlm_reqs_pool_add(p->hlm_reqs_pool, hr, br);
				br->bi_bvec_index +=ret;
				br->bi_offset += 8*ret;
				hr-> hlm_number = loop;
				if(req_size == hr->page_size) {
					if(bdi->ptr_hlm_inf->make_req(bdi, hr) !=0) {
						bdbm_error("'bdi->ptr_hlm_inf->make_req' failed");
					}
					hr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool);
				}
				break;
			}

			ret = bdbm_hlm_reqs_pool_add(p->hlm_reqs_pool, hr, br);
			br->bi_bvec_index +=ret;
			br->bi_offset += 8*ret;
			hr-> hlm_number = loop;
			if(bdi->ptr_hlm_inf->make_req(bdi, hr) !=0) {
				bdbm_error("'bdi->ptr_hlm_inf->make_req' failed");
			}
			hr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool);
		}
	
		if(hr->nr_charged !=0) {
			if(br->is_sync == 1) {
				if(bdi->ptr_hlm_inf->make_req(bdi,hr) !=0) {
					bdbm_error("'bdi->ptr_hlm_inf->make_req' failed");
				}
				hr = bdbm_hlm_reqs_pool_get_item(p->hlm_reqs_pool);
			}
		}

	}
//	bdbm_msg("end buf_make_req");
}

void userio_make_req (bdbm_drv_info_t* bdi, void *bio)
{
	bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);
	bdbm_blkio_req_t* br = (bdbm_blkio_req_t*)bio;
	static bdbm_hlm_req_t* hr = NULL;

	//bdbm_msg ("userio_make_req - begin");

	/* get a free hlm_req from the hlm_reqs_pool */
	if ((hr = bdbm_hlm_reqs_pool_get_item (p->hlm_reqs_pool)) == NULL) {
		bdbm_error ("bdbm_hlm_reqs_pool_alloc_item () failed");
		bdbm_bug_on (1);
		return;
	}

	/* build hlm_req with bio */
	if (bdbm_hlm_reqs_pool_build_req (p->hlm_reqs_pool, hr, br) != 0) {
		bdbm_error ("bdbm_hlm_reqs_pool_build_req () failed");
		bdbm_bug_on (1);
		return;
	}

	/* if success, increase # of host reqs */
	/*atomic_inc (&p->nr_host_reqs);*/
	bdbm_sema_lock (&p->count_lock);
	p->nr_host_reqs++;
	bdbm_sema_unlock (&p->count_lock);

	bdbm_sema_lock (&p->host_lock);

	/* NOTE: it would be possible that 'hlm_req' becomes NULL 
	 * if 'bdi->ptr_hlm_inf->make_req' is success. */
	if (bdi->ptr_hlm_inf->make_req (bdi, hr) != 0) {
		/* oops! something wrong */
		bdbm_error ("'bdi->ptr_hlm_inf->make_req' failed");

		/* cancel the request */
		/*atomic_dec (&p->nr_host_reqs);*/
		bdbm_sema_lock (&p->count_lock);
		p->nr_host_reqs--;
		bdbm_sema_unlock (&p->count_lock);
		bdbm_hlm_reqs_pool_free_item (p->hlm_reqs_pool, hr);
	}
	bdbm_sema_unlock (&p->host_lock);

	//bdbm_msg ("userio_make_req - done");
}

void userio_end_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* req)
{
	bdbm_userio_private_t* p = (bdbm_userio_private_t*)BDBM_HOST_PRIV(bdi);
	bdbm_blkio_req_t** r = (bdbm_blkio_req_t**)req->blkio_req;
	uint8_t i,j,nr_free;
	uint64_t cnt;
	nr_free = req->nr_blkio_req;

//	bdbm_msg ("--------userio_end_req----- nrfree%d--------------------------",nr_free);


//	bdbm_msg("nr_free : %d, '%d'th hlm of number [%d] blk",nr_free, req->hlm_number, r[0]->blk_number);
	/* free first blk, next hlm is ok? */
	for(i=0; i<nr_free; i++)
	{
	//	if(r[0]->blk_number > 70) bdbm_msg("hlm release called, number [%d]",r[0]->blk_number);
	
		for(j=0; j<req->nr_pages_blk[i]; j++) {
			atomic64_inc(&r[i]->reqs_done);
		}

		cnt =atomic64_read(&r[i]->reqs_done);
	//	if(cnt >200) bdbm_msg("blk_number [%d] , cnt [%lld]", r[i]->blk_number, cnt);

		if(atomic64_read(&r[i]->reqs_done) == r[i]->bi_bvec_cnt){
			if(r[i]->bi_bvec_cnt != r[i]->bi_bvec_index) {
				bdbm_error("bvec_cnt != bvec_index");
				return;
			}
			bdbm_sema_lock (&p->count_lock);
			p->nr_host_reqs--;
		//	bdbm_msg("reqs_done %d, bvec_cnt %d [end req]", cnt, r[i]->bi_bvec_cnt);
//			bdbm_msg("p->nr_host_reqs %llu [end req]",p->nr_host_reqs);
			bdbm_sema_unlock (&p->count_lock);

		//	bdbm_msg("-------------------- release blk_number [%d]-----------",r[i]->blk_number);
			r[i]->cb_done(r[i]);
		}


	}
	/* destroy hlm_req */
//	bdbm_msg("hlm free");
	
	atomic64_set(&req->nr_llm_reqs_done,0);
	bdbm_hlm_reqs_pool_free_item (p->hlm_reqs_pool, req);

	/* decreate # of reqs */
	/*atomic_dec (&p->nr_host_reqs);*/

	/* call call-back function */
	/*
	for(i=0; i<nr_free; i++)
	{
		if (r[i]->cb_done) {
			r[i]->cb_done (r[i]);
		}
	}
	*/
}


