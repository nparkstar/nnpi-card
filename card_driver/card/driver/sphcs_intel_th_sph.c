/********************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

/**
 * @file sph_types.h
 *
 * @brief Header file defining sph hwtrace types
 *
 * This header file defines common types used in the sph hwtrace interface library.
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/scatterlist.h>
#include <linux/sort.h>
#include <linux/spinlock.h>
#include <linux/dma-buf.h>
#include <linux/intel_th.h>

#include "sph_hwtrace_protocol.h"
#include "sphcs_hwtrace.h"
#include "sphcs_intel_th.h"
#include "sphcs_cs.h"
#include "sph_log.h"
#include "sph_debug.h"

#define HWTRACING_POOL_PAGE_COUNT ((uint32_t)(256))
#define HWTRACING_MIN_PAGE_COUNT ((uint32_t)(128))

struct msu_buffer_driver g_msu = {"sph_hwtrace",
	NULL,
	intel_th_assign_mode,
	intel_th_unassign,
	intel_th_alloc_window,
	intel_th_free_window,
	intel_th_activate,
	intel_th_deactivate,
	intel_th_window_ready};


//cleanup memory pool for hw tracing.

void npk_page_pool_cleanup(void)
{
	struct sphcs_hwtrace_data *hw_tracing = &g_the_sphcs->hw_tracing;
	int i;

	for (i = 0; i < SPHCS_HWTRACING_MAX_POOL_LENGTH; i++) {
		struct sphcs_hwtrace_mem_pool *pages_pool = &hw_tracing->mem_pool[i];

		if (!pages_pool->pages)
			continue;

		__free_pages(pages_pool->pages, get_order(hw_tracing->nr_pool_pages));

		memset(pages_pool, 0x0, sizeof(struct sphcs_hwtrace_mem_pool));
	}

	hw_tracing->nr_pool_pages = 0;

}

//allocating memory pool used for hwtracing when driver is loaded.

int npk_page_pool_alloc(size_t page_count)
{
	struct sphcs_hwtrace_data *hw_tracing = &g_the_sphcs->hw_tracing;
	int ret = 0;
	int i;

	hw_tracing->nr_pool_pages = page_count;


	for (i = 0; i < SPHCS_HWTRACING_MAX_POOL_LENGTH; i++) {
		struct sphcs_hwtrace_mem_pool *pages_pool = &hw_tracing->mem_pool[i];

		memset(pages_pool, 0x0, sizeof(struct sphcs_hwtrace_mem_pool));

		pages_pool->pages = alloc_pages(GFP_DMA32, get_order(hw_tracing->nr_pool_pages * PAGE_SIZE));
		if (unlikely(pages_pool->pages == NULL)) {
			ret = -ENOMEM;
			goto pool_cleanup;
		}
	}

	return 0;

pool_cleanup:
	npk_page_pool_cleanup();

	return ret;
}

//driver will use a multi window trace mode in NPK.

void sphcs_assign_intel_th_mode(int *mode)
{
	*mode = MSC_MODE_MULTI;
}


int sphcs_init_th_driver(void)
{
	struct sphcs_hwtrace_data *hw_tracing = &g_the_sphcs->hw_tracing;
	int ret = 0;
	size_t alloc_size = HWTRACING_POOL_PAGE_COUNT;

	hw_tracing->hwtrace_status = SPHCS_HWTRACE_NOT_SUPPORTED;

	//try to allocate page pool
	do {
		ret = npk_page_pool_alloc(alloc_size);
		if (ret)
			alloc_size--;

	} while ( ret != 0 && alloc_size > HWTRACING_MIN_PAGE_COUNT);

	if (alloc_size < HWTRACING_MIN_PAGE_COUNT) {
		sph_log_err(HWTRACE_LOG, "unable to allocate pool for npk resources - err %d", ret);
		goto err;
	}

	hw_tracing->cmd_wq = create_singlethread_workqueue("sphcs_inf_wq");
	if (!hw_tracing->cmd_wq) {
		sph_log_err(START_UP_LOG, "Failed to initialize hwtrace commands workqueue");
		goto pool_cleanup;
	}
	//driver will register to trace service from intel_th driver.

	g_msu.owner = THIS_MODULE;

	ret = intel_th_msu_buffer_register(&g_msu);
	if (ret) {
		sph_log_err(HWTRACE_LOG, "unable to register intel_th service - err %d", ret);
		goto wq_cleanup;
	}

	hw_tracing->hwtrace_status = SPHCS_HWTRACE_REGISTERED;

	init_waitqueue_head(&hw_tracing->waitq);
	spin_lock_init(&hw_tracing->lock_irq);
	INIT_LIST_HEAD(&(hw_tracing->dma_stream_list));
	hw_tracing->host_resource_count = 0;
	return ret;

wq_cleanup:
	destroy_workqueue(hw_tracing->cmd_wq);

pool_cleanup:
	npk_page_pool_cleanup();

err:
	return ret;

}

void sphcs_deinit_th_driver(void)
{
	struct sphcs_hwtrace_data *hw_tracing = &g_the_sphcs->hw_tracing;

	npk_page_pool_cleanup();

	destroy_workqueue(hw_tracing->cmd_wq);

	intel_th_msu_buffer_unregister(&g_msu);
}


void sphcs_intel_th_window_unlock(struct device *dev, struct sg_table *sgt)
{
	intel_th_msc_window_unlock(dev, sgt);
}




