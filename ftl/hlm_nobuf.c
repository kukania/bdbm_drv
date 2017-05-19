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

#if defined(KERNEL_MODE)
#include <linux/module.h>
#include <linux/blkdev.h>

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdint.h>

#else
#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif

#include "debug.h"
#include "params.h"
#include "bdbm_drv.h"
#include "hlm_nobuf.h"
#include "hlm_reqs_pool.h"
#include "utime.h"
#include "umemory.h"

#include "algo/no_ftl.h"
#include "algo/block_ftl.h"
#include "algo/page_ftl.h"

#include "algo/lsmtree/lsmtree.h"

/* interface for hlm_nobuf */
bdbm_hlm_inf_t _hlm_nobuf_inf = {
	.ptr_private = NULL,
	.create = hlm_nobuf_create,
	.destroy = hlm_nobuf_destroy,
	.make_req = hlm_nobuf_make_req,
	.end_req = hlm_nobuf_end_req,
	/*.load = hlm_nobuf_load,*/
	/*.store = hlm_nobuf_store,*/
};

extern uint64_t read_cnt[21];
bdbm_hlm_req_t * hlm_buf_ptr;

/* data structures for hlm_nobuf */
typedef struct {
	bdbm_hlm_req_t tmp_hr;
} bdbm_hlm_nobuf_private_t;


/* functions for hlm_nobuf */
uint32_t hlm_nobuf_create (bdbm_drv_info_t* bdi)
{
	bdbm_hlm_nobuf_private_t* p;

	/* create private */
	if ((p = (bdbm_hlm_nobuf_private_t*)bdbm_malloc
			(sizeof(bdbm_hlm_nobuf_private_t))) == NULL) {
		bdbm_error ("bdbm_malloc failed");
		return 1;
	}

	/* keep the private structure */
	bdi->ptr_hlm_inf->ptr_private = (void*)p;
	return 0;
}

void hlm_nobuf_destroy (bdbm_drv_info_t* bdi)
{
	bdbm_hlm_nobuf_private_t* p = (bdbm_hlm_nobuf_private_t*)BDBM_HLM_PRIV(bdi);
	/* free priv */
	bdbm_free (p);
}

uint32_t __hlm_nobuf_make_trim_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* ptr_hlm_req)
{
	bdbm_ftl_inf_t* ftl = (bdbm_ftl_inf_t*)BDBM_GET_FTL_INF(bdi);
	uint64_t i;

	for (i = 0; i < ptr_hlm_req->len; i++) {
		ftl->invalidate_lpa (bdi, ptr_hlm_req->lpa + i, 1);
	}

	return 0;
}
uint32_t __hlm_nobuf_make_rw_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* hr)
{
	bdbm_device_params_t* np = BDBM_GET_DEVICE_PARAMS(bdi);
	bdbm_ftl_inf_t* ftl = BDBM_GET_FTL_INF(bdi);
	bdbm_llm_req_t* lr = NULL;
	uint64_t i = 0, j = 0, sp_ofs;
	bdbm_blkio_req_t* br =(bdbm_blkio_req_t*)hr->blkio_req[0]; //must be changed!

	//hr->llm_reqs[0].subpage_ofs = hr->subpage_ofs;

	/* perform mapping with the FTL */
	bdbm_hlm_for_each_llm_req (lr, hr, i) {
//		bdbm_msg("hlm_for_each_llm_req : %lld",i);
		/* (1) get the physical locations through the FTL */
		if (bdbm_is_normal (lr->req_type)) {
			/* handling normal I/O operations */
			if (bdbm_is_read (lr->req_type)) {
				if(get(lr->logaddr.lpa[0],bdi,lr)){
					lr->req_type=REQTYPE_READ_DUMMY;
					hlm_reqs_pool_relocate_kp (lr, 0);
					bdbm_msg("dummy READ");
				}else {
					return 1;
				}
			} else if (bdbm_is_write (lr->req_type) || bdbm_is_rmw(lr->req_type)) {
				if (ftl->get_free_ppa (bdi, lr->logaddr.lpa[0], &lr->phyaddr,lr) != 0) {
					bdbm_error ("`ftl->get_free_ppa' failed");
					goto fail;
				}
			} else {
				bdbm_error ("oops! invalid type (%x)", lr->req_type);
				bdbm_bug_on (1);
			}
		} else {
			bdbm_error ("oops! invalid type (%x)", lr->req_type);
			bdbm_bug_on (1);
		}

		/* (2) setup oob */
		for (j = 0; j < np->nr_subpages_per_page; j++) {
			((int64_t*)lr->foob.data)[j] = lr->logaddr.lpa[j];
		}
	}
	return 0;
fail:
	return 1;
	
}

/* TODO: it must be more general... */
void __hlm_nobuf_check_ondemand_gc (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* hr)
{
	bdbm_ftl_params* dp = BDBM_GET_DRIVER_PARAMS (bdi);
	bdbm_ftl_inf_t* ftl = (bdbm_ftl_inf_t*)BDBM_GET_FTL_INF(bdi);

	if (dp->mapping_type == MAPPING_POLICY_PAGE) {
		uint32_t loop;
		/* see if foreground GC is needed or not */
		for (loop = 0; loop < 10; loop++) {
			if (hr->req_type == REQTYPE_WRITE && 
				ftl->is_gc_needed != NULL && 
				ftl->is_gc_needed (bdi, 0)) {
				/* perform GC before sending requests */ 
				bdbm_msg ("[hlm_nobuf_make_req] trigger GC");
				ftl->do_gc (bdi, 0);
			} else
				break;
		}
	} else if (dp->mapping_type == MAPPING_POLICY_RSD ||
			   dp->mapping_type == MAPPING_POLICY_BLOCK) {
		/* perform mapping with the FTL */
		if (hr->req_type == REQTYPE_WRITE && ftl->is_gc_needed != NULL) {
			bdbm_llm_req_t* lr = NULL;
			uint64_t i = 0;
			bdbm_hlm_for_each_llm_req (lr, hr, i) {
				/* NOTE: segment-level ftl does not support fine-grain rmw */
				if (ftl->is_gc_needed (bdi, lr->logaddr.lpa[0])) {
					/* perform GC before sending requests */ 
					//bdbm_msg ("[hlm_nobuf_make_req] trigger GC");
					ftl->do_gc (bdi, lr->logaddr.lpa[0]);
				}
			}
		}
	} else {
		/* do nothing */
	}
}


uint32_t hlm_nobuf_make_req (bdbm_drv_info_t* bdi, bdbm_hlm_req_t* hr)
{
	uint32_t ret;
	bdbm_stopwatch_t sw;
	bdbm_stopwatch_start (&sw);
	/* is req_type correct? */
	bdbm_bug_on (!bdbm_is_normal (hr->req_type));

#if 0
	/* trigger gc if necessary */
	if (dp->mapping_type != MAPPING_POLICY_DFTL) {
		bdbm_ftl_inf_t* ftl = (bdbm_ftl_inf_t*)BDBM_GET_FTL_INF(bdi);
		/* see if foreground GC is needed or not */
		for (loop = 0; loop < 10; loop++) {
			if (hr->req_type == REQTYPE_WRITE && 
				ftl->is_gc_needed != NULL && 
				ftl->is_gc_needed (bdi, 0)) {
				/* perform GC before sending requests */ 
				bdbm_msg ("[hlm_nobuf_make_req] trigger GC");
				ftl->do_gc (bdi, 0);
			} else
				break;
		}
	}
#endif

//	bdbm_msg("hlm_nobuf_make_req");
	/* perform i/o */
	if (bdbm_is_trim (hr->req_type)) {
//		bdbm_msg("make_req call make_trim_req");
		if ((ret = __hlm_nobuf_make_trim_req (bdi, hr)) == 0) {
			/* call 'ptr_host_inf->end_req' directly */
			bdi->ptr_host_inf->end_req (bdi, hr);
			/* hr is now NULL */
		}
	} else {
		/* do we need to do garbage collection? */

	//	__hlm_nobuf_check_ondemand_gc (bdi, hr);   

	
		/*kukania : original request*/
		ret=__hlm_nobuf_make_rw_req (bdi, hr);
		if(bdbm_is_write(hr->req_type)){
			bdi->ptr_host_inf->end_req (bdi, hr);
			return ret;
		}
	}
	return ret;
}

void __hlm_nobuf_end_blkio_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* lr)
{
	bdbm_hlm_req_t* hr = (bdbm_hlm_req_t* )lr->ptr_hlm_req;
//	bdbm_blkio_req_* br= (bdbm_blkio_req_t*)hr->blkio_req[0];
	/* increase # of reqs finished */

//	bdbm_msg("hlm_end_req before atomic inc %lld", atomic64_read(&hr->nr_llm_reqs_done));
	atomic64_inc (&hr->nr_llm_reqs_done);
	lr->req_type |= REQTYPE_DONE;

//	bdbm_msg("hlm_get end req %d blk number[%d]", hr->hlm_number, br->blk_number);
	if (atomic64_read (&hr->nr_llm_reqs_done) == hr->nr_llm_reqs) {
		/* finish the host request */
		//bdbm_msg("call host_inf->end_req");
		bdi->ptr_host_inf->end_req (bdi, hr);
	} else {
		bdbm_error("nr_llm_reqs_done != nr_llm_reqs");
		bdbm_msg("nr_llm_reqs_done %lld, hr->nr_llm_reqs %lld", atomic64_read(&hr->nr_llm_reqs_done), hr->nr_llm_reqs);
	}
}

void __hlm_nobuf_end_gcio_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* lr)
{
	bdbm_hlm_req_gc_t* hr_gc = (bdbm_hlm_req_gc_t* )lr->ptr_hlm_req;

	atomic64_inc (&hr_gc->nr_llm_reqs_done);
	lr->req_type |= REQTYPE_DONE;

	/*
	if (atomic64_read (&hr_gc->nr_llm_reqs_done) == hr_gc->nr_llm_reqs) {
		bdbm_sema_unlock (&hr_gc->done);
	}
	*/
}

void hlm_nobuf_end_req (bdbm_drv_info_t* bdi, bdbm_llm_req_t* lr)
{
	if (bdbm_is_gc (lr->req_type)) {
		__hlm_nobuf_end_gcio_req (bdi, lr);
	} else {
		__hlm_nobuf_end_blkio_req (bdi, lr);
	}
}

