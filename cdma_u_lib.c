// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <cdma_abi.h>
#include "cdma_u_device.h"
#include "cdma_u_context.h"
#include "cdma_u_log.h"
#include "cdma_u_lib.h"
#include "cdma_u_cmd.h"
#include "cdma_u_event.h"
#include "cdma_u_queue.h"
#include "cdma_u_common.h"
#include "cdma_u_segment.h"

struct dma_device *dma_get_device_list(uint32_t *num_devices)
{
	if (!num_devices)
		return NULL;

	return cdma_get_device_list(num_devices);
}

void dma_free_device_list(struct dma_device *dev_list, uint32_t num_devices)
{
	/*
	 * num_devices can be 0, it means that no device has been obtained,
	 * but the dev_list memory should be free any way
	 */
	if (!dev_list)
		return;

	cdma_free_device_list(dev_list, num_devices);
}

struct dma_device *dma_get_device_by_eid(struct dev_eid *eid)
{
	if (!eid)
		return NULL;

	return cdma_get_device_by_eid(eid);
}

struct dma_context *dma_create_context(struct dma_device *dma_dev)
{
	struct dma_context *ctx;

	if (!dma_dev)
		return NULL;

	ctx = cdma_create_context(dma_dev);
	if (!ctx) {
		CDMA_LOG_ERR("create context failed.\n");
		return NULL;
	}

	return ctx;
}

void dma_delete_context(struct dma_context *ctx)
{
	if (!ctx)
		return;

	cdma_delete_context(ctx);
}

struct dma_queue *dma_alloc_queue(struct dma_context *ctx, struct queue_cfg *cfg)
{
	struct dma_queue *queue;

	if (!ctx || !cfg) {
		CDMA_LOG_ERR("alloc queue parameter is null.\n");
		return NULL;
	}

	queue = cdma_alloc_queue(ctx, cfg);
	if (!queue) {
		CDMA_LOG_ERR("alloc queue failed.\n");
		return NULL;
	}

	return queue;
}

void dma_free_queue(struct dma_queue *queue)
{
	if (!queue)
		return;

	cdma_free_queue(queue);
}

struct dma_seg *dma_register_seg(struct dma_context *ctx,
				 struct dma_seg_cfg *cfg)
{
	struct dma_seg *seg;

	if (!ctx || !cfg || cfg->addr == 0 || cfg->len == 0) {
		CDMA_LOG_ERR("register seg parameter is invalid.\n");
		return NULL;
	}

	seg = cdma_register_seg(ctx, cfg);
	if (!seg) {
		CDMA_LOG_ERR("register seg failed.\n");
		return NULL;
	}

	return seg;
}

void dma_unregister_seg(struct dma_context *ctx, struct dma_seg *seg)
{
	if (!ctx || !seg)
		return;

	cdma_unregister_seg(ctx, seg);
}

struct dma_seg *dma_import_seg(struct dma_seg_cfg *cfg)
{
	if (!cfg || cfg->addr == 0 || cfg->len == 0) {
		CDMA_LOG_ERR("import seg parameter is invalid.\n");
		return NULL;
	}

	return cdma_import_seg(cfg);
}

void dma_unimport_seg(struct dma_seg *seg)
{
	if (!seg)
		return;

	cdma_unimport_seg(seg);
}

dma_status dma_write(struct dma_queue *queue, struct dma_seg *rmt_seg,
		     struct dma_seg *local_seg)
{
	if (!queue || !rmt_seg || !local_seg)
		return DMA_STATUS_INVAL;

	return cdma_write(queue, rmt_seg, local_seg, NULL, 0);
}

dma_status dma_read(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg)
{
	if (!queue || !rmt_seg || !local_seg)
		return DMA_STATUS_INVAL;

	return cdma_read(queue, rmt_seg, local_seg);
}

dma_status dma_cas(struct dma_queue *queue, struct dma_seg *rmt_seg,
				   struct dma_seg *local_seg, uint64_t cmp, uint64_t swap)
{
	if (!queue || !rmt_seg || !local_seg)
		return DMA_STATUS_INVAL;

	return cdma_cas(queue, rmt_seg, local_seg, cmp, swap);
}

dma_status dma_faa(struct dma_queue *queue, struct dma_seg *rmt_seg,
		   struct dma_seg *local_seg, uint64_t add)
{
	if (!queue || !rmt_seg || !local_seg)
		return DMA_STATUS_INVAL;

	return cdma_faa(queue, rmt_seg, local_seg, add);
}

dma_status dma_write_with_notify(struct dma_queue *queue,
				 struct dma_seg *rmt_seg,
				 struct dma_seg *local_seg,
				 struct dma_seg *notify_seg,
				 uint64_t notify_data)
{
	if (!queue || !rmt_seg || !local_seg || !notify_seg)
		return DMA_STATUS_INVAL;

	return cdma_write(queue, rmt_seg, local_seg, notify_seg, notify_data);
}

int dma_poll_queue(struct dma_queue *queue, uint32_t cr_cnt, struct dma_cr *cr)
{
	if (!queue || !cr_cnt || !cr) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return -EINVAL;
	}

	return cdma_poll_queue(queue, cr_cnt, cr);
}

int dma_wait_queue(struct dma_queue *queue, uint32_t cr_cnt, int timeout,
				   struct dma_cr *cr)
{
	if (!queue || !queue->cfg.event_mode || !cr_cnt || !cr) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return -EINVAL;
	}

	return cdma_wait_queue(queue, cr_cnt, timeout, cr);
}

int dma_wait_ae(struct dma_context *ctx, struct dma_aeqe *aeqe)
{
	if (!ctx || !aeqe)
		return -EINVAL;

	return cdma_wait_ae(ctx, aeqe);
}
