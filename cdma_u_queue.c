// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <sys/ioctl.h>
#include "cdma_u_cmd.h"
#include "cdma_u_queue.h"

static int cdma_u_cmd_create_queue(struct dma_context *ctx,
				   struct cdma_u_queue *cdma_queue,
				   struct queue_cfg *cfg)
{
	struct cdma_cmd_create_queue_args arg = { 0 };
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!ctx->dma_dev || ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("create queue cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_CREATE_QUEUE;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;
	arg.in.queue_depth = cfg->queue_depth;
	arg.in.dcna = cfg->dcna;
	arg.in.priority = cfg->priority;
	arg.in.rmt_eid = cfg->rmt_eid.dw0;
	arg.in.user_ctx = cfg->user_ctx;
	arg.in.trans_mode = cfg->trans_mode;

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret) {
		CDMA_LOG_ERR("ioctl in create queue, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	cdma_queue->handle = arg.out.handle;
	cdma_queue->base.queue_id = arg.out.queue_id;

	return ret;
}

static int cdma_u_cmd_delete_queue(struct cdma_u_queue *cdma_queue)
{
	struct cdma_cmd_delete_queue_args arg = { 0 };
	struct dma_queue *queue = &cdma_queue->base;
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!queue->ctx || !queue->ctx->dma_dev || queue->ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("delete jfs cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_DELETE_QUEUE;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;
	arg.in.queue_id = queue->queue_id;
	arg.in.handle = cdma_queue->handle;

	ret = ioctl(queue->ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret)
		CDMA_LOG_ERR("ioctl in delete queue, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);

	return ret;
}

static void cdma_assemble_jfs_cfg(struct dma_jfs_cfg *jfs_cfg,
				  const struct dma_device *dev,
				  const struct queue_cfg *cfg,
				  const uint32_t queue_id)
{
	jfs_cfg->err_timeout = CDMA_TYPICAL_ERR_TIMEOUT;
	jfs_cfg->max_rsge = dev->attr.dev_cap.max_jfs_rsge;
	jfs_cfg->max_sge = dev->attr.dev_cap.max_jfs_sge;
	jfs_cfg->rnr_retry = CDMA_TYPICAL_RNR_RETRY;
	jfs_cfg->eid_idx = dev->attr.eu.eid_idx;
	jfs_cfg->depth = cfg->queue_depth;
	jfs_cfg->priority = cfg->priority;
	jfs_cfg->queue_id = queue_id;
	jfs_cfg->rmt_eid = cfg->rmt_eid.dw0;
	jfs_cfg->trans_mode = cfg->trans_mode;
}

static void cdma_assemble_tp_cfg(struct dma_tp_cfg *tp_cfg,
				 const struct dma_device *dev,
				 const struct queue_cfg *cfg,
				 const uint32_t queue_id)
{
	tp_cfg->seid = dev->attr.eu.eid.dw0;
	tp_cfg->deid = cfg->rmt_eid.dw0;
	tp_cfg->dcna = cfg->dcna;
	tp_cfg->queue_id = queue_id;
}

static void cdma_assemble_jfc_cfg(struct dma_jfc_cfg *jfc_cfg,
				  const struct queue_cfg *cfg,
				  const uint32_t queue_id)
{
	jfc_cfg->depth = cfg->queue_depth;
	jfc_cfg->user_ctx = cfg->user_ctx;
	jfc_cfg->queue_id = queue_id;
}

static int cdma_u_alloc_jfc_res(struct dma_context *ctx,
				struct cdma_u_queue *cdma_queue,
				struct queue_cfg *cfg,
				struct dma_jfc_cfg *jfc_cfg)
{
	struct dma_jfc *jfc;
	int ret = -ENOMEM;

	jfc_cfg->jfce = cdma_u_create_jfce(ctx);
	if (!jfc_cfg->jfce) {
		CDMA_LOG_ERR("cdma jfce create failed.\n");
		return ret;
	}

	jfc = cdma_u_create_jfc(ctx, jfc_cfg);
	if (!jfc) {
		CDMA_LOG_ERR("cdma jfc create failed.\n");
		goto delete_jfce;
	}

	if (cfg->event_mode && cdma_u_rearm_jfc(jfc, false) != DMA_STATUS_OK) {
		CDMA_LOG_ERR("cdma rearm jfc failed.\n");
		ret = -EFAULT;
		goto delete_jfc;
	}

	cdma_queue->cdma_jfce = jfc_cfg->jfce;
	cdma_queue->cdma_jfc = jfc;

	return 0;

delete_jfc:
	(void)cdma_u_delete_jfc(jfc);
delete_jfce:
	(void)cdma_u_delete_jfce(jfc_cfg->jfce);

	return ret;
}

static void cdma_u_free_jfc_res(struct cdma_u_queue *cdma_queue)
{
	(void)cdma_u_delete_jfc(cdma_queue->cdma_jfc);
	(void)cdma_u_delete_jfce(cdma_queue->cdma_jfce);
}

static int cdma_u_create_queue(struct dma_context *ctx, struct queue_cfg *cfg,
			       const uint32_t queue_id,
			       struct cdma_u_queue *cdma_queue)
{
	struct dma_jfc_cfg jfc_cfg = {0};
	struct dma_jfs_cfg jfs_cfg = {0};
	struct dma_tp_cfg tp_cfg = {0};
	int ret;

	if (!ctx || !cfg || !ctx->dma_dev) {
		CDMA_LOG_ERR("create queue parameter invalid.\n");
		return -EINVAL;
	}

	cdma_queue->base.ctx = ctx;
	cdma_queue->base.cfg = *cfg;

	cdma_assemble_jfs_cfg(&jfs_cfg, ctx->dma_dev, cfg, queue_id);
	cdma_assemble_tp_cfg(&tp_cfg, ctx->dma_dev, cfg, queue_id);
	cdma_assemble_jfc_cfg(&jfc_cfg, cfg, queue_id);

	ret = cdma_u_alloc_jfc_res(ctx, cdma_queue, cfg, &jfc_cfg);
	if (ret) {
		CDMA_LOG_ERR("cdma alloc jfc res, ret = %d, seid = %u, deid = %u.\n",
			     ret, tp_cfg.seid, tp_cfg.deid);
		return ret;
	}

	cdma_queue->cdma_tp = cdma_u_create_ctp(ctx, &tp_cfg);
	if (!cdma_queue->cdma_tp) {
		CDMA_LOG_ERR("cdma tp create failed, seid = %u, deid = %u.\n",
			     tp_cfg.seid, tp_cfg.deid);
		goto delete_jfc_res;
	}

	jfs_cfg.jfc = cdma_queue->cdma_jfc;
	jfs_cfg.tpn = cdma_queue->cdma_tp->tpn;

	cdma_queue->cdma_jfs = cdma_u_create_jfs(ctx, &jfs_cfg);
	if (!cdma_queue->cdma_jfs) {
		CDMA_LOG_ERR("cdma jfs create failed, seid = %u, deid = %u.\n",
			     tp_cfg.seid, tp_cfg.deid);
		goto delete_tp;
	}

	cdma_queue->queue_cfg.cdma_jfc_cfg = jfc_cfg;
	cdma_queue->queue_cfg.cdma_jfs_cfg = jfs_cfg;
	cdma_queue->queue_cfg.cdma_tp_cfg = tp_cfg;

	return 0;

delete_tp:
	(void)cdma_u_delete_ctp(cdma_queue->cdma_tp);
delete_jfc_res:
	cdma_u_free_jfc_res(cdma_queue);

	return -ENOMEM;
}

static void cdma_u_delete_queue(struct cdma_u_queue *cdma_queue)
{
	(void)cdma_u_delete_jfs(cdma_queue->cdma_jfs);
	(void)cdma_u_delete_ctp(cdma_queue->cdma_tp);
	cdma_u_free_jfc_res(cdma_queue);
}

struct dma_queue *cdma_alloc_queue(struct dma_context *ctx,
				   struct queue_cfg *cfg)
{
	struct cdma_u_queue *cdma_queue;
	int ret;

	cdma_queue = (struct cdma_u_queue *)calloc(1, sizeof(*cdma_queue));
	if (!cdma_queue) {
		CDMA_LOG_ERR("cdma queue alloc failed.\n");
		return NULL;
	}

	ret = cdma_u_cmd_create_queue(ctx, cdma_queue, cfg);
	if (ret) {
		CDMA_LOG_ERR("cdma queue create failed, ret = %d.\n", ret);
		free(cdma_queue);
		return NULL;
	}

	ret = cdma_u_create_queue(ctx, cfg, cdma_queue->handle, cdma_queue);
	if (ret) {
		CDMA_LOG_ERR("cdma create queue failed, ret = %d.\n", ret);
		(void)cdma_u_cmd_delete_queue(cdma_queue);
		free(cdma_queue);
		return NULL;
	}

	cdma_queue->base.cfg = *cfg;
	cdma_queue->base.ctx = ctx;

	return &cdma_queue->base;
}

void cdma_free_queue(struct dma_queue *queue)
{
	struct cdma_u_queue *cdma_queue;

	cdma_queue = to_cdma_u_queue(queue);

	cdma_u_delete_queue(cdma_queue);
	(void)cdma_u_cmd_delete_queue(cdma_queue);
	free(cdma_queue);
}
