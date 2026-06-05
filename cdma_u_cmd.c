// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <cdma_abi.h>
#include "cdma_u_log.h"
#include "cdma_u_cmd.h"
#include "cdma_u_jfs.h"
#include "cdma_u_common.h"

static int cdma_rw_check(struct dma_seg *rmt_seg, struct dma_seg *local_seg)
{
	if (rmt_seg->len == 0 || local_seg->len == 0) {
		CDMA_LOG_ERR("segment check invalid len.\n");
		return -EINVAL;
	}

	if (rmt_seg->sva == 0 || local_seg->sva == 0) {
		CDMA_LOG_ERR("segment check invalid address.\n");
		return -EINVAL;
	}

	return 0;
}

static inline void cdma_fill_comm_wr(dma_jfs_wr_t *wr, struct dma_queue *queue)
{
	struct cdma_u_queue *cdma_queue = to_cdma_u_queue(queue);

	wr->flag.bs.complete_enable = 1;
	wr->flag.bs.fence = 1;
	wr->user_ctx = queue->cfg.user_ctx;
	if (cdma_queue->cdma_tp)
		wr->tpn = cdma_queue->cdma_tp->tpn;
	wr->rmt_eid = queue->cfg.rmt_eid.dw0;
	wr->next = NULL;
}

static void cdma_fill_sge(dma_sge_t *rmt_sge, dma_sge_t *local_sge,
			  struct dma_seg *rmt_seg, struct dma_seg *local_seg)
{
	local_sge->addr = local_seg->sva;
	local_sge->len = local_seg->len;
	local_sge->seg = *local_seg;

	rmt_sge->addr = rmt_seg->sva;
	rmt_sge->len = rmt_seg->len;
	rmt_sge->seg = *rmt_seg;
}

dma_status cdma_cas(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg, uint64_t cmp, uint64_t swap)
{
	dma_jfs_wr_t wr = {.opcode = CDMA_WR_OPC_CAS};
	struct cdma_u_queue *cdma_queue;
	dma_sge_t rmt_sge, local_sge;
	dma_jfs_wr_t *bad_wr = NULL;
	dma_tp_cfg_t *cfg;
	int ret;

	if (cdma_rw_check(rmt_seg, local_seg)) {
		CDMA_LOG_ERR("cas param check failed.\n");
		return DMA_STATUS_INVAL;
	}

	cdma_fill_comm_wr(&wr, queue);

	cdma_fill_sge(&rmt_sge, &local_sge, rmt_seg, local_seg);

	wr.cas.src = &local_sge;
	wr.cas.dst = &rmt_sge;

	if (local_sge.len <= CDMA_ATOMIC_LEN_8) {
		wr.cas.cmp_data = cmp;
		wr.cas.swap_data = swap;
	} else {
		wr.cas.cmp_addr = cmp;
		wr.cas.swap_addr = swap;
	}

	cdma_queue = to_cdma_u_queue(queue);
	cfg = &cdma_queue->cdma_tp->cfg;

	ret = cdma_u_post_jfs_wr(cdma_queue->cdma_jfs, &wr, &bad_wr);
	if (ret) {
		CDMA_LOG_ERR("post jfs for cas failed, ret = %d, seid = %u, deid = %u.\n",
			     ret, cfg->seid, cfg->deid);
		return DMA_STATUS_INVAL;
	}

	return DMA_STATUS_OK;
}

dma_status cdma_faa(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg, uint64_t operand)
{
	dma_jfs_wr_t wr = {.opcode = CDMA_WR_OPC_FADD};
	struct cdma_u_queue *cdma_queue;
	dma_sge_t rmt_sge, local_sge;
	dma_jfs_wr_t *bad_wr = NULL;
	dma_tp_cfg_t *cfg;
	int ret;

	if (cdma_rw_check(rmt_seg, local_seg)) {
		CDMA_LOG_ERR("faa param check failed.\n");
		return DMA_STATUS_INVAL;
	}

	cdma_fill_comm_wr(&wr, queue);

	cdma_fill_sge(&rmt_sge, &local_sge, rmt_seg, local_seg);

	wr.faa.src = &local_sge;
	wr.faa.dst = &rmt_sge;
	wr.faa.operand = operand;

	cdma_queue = to_cdma_u_queue(queue);
	cfg = &cdma_queue->cdma_tp->cfg;

	ret = cdma_u_post_jfs_wr(cdma_queue->cdma_jfs, &wr, &bad_wr);
	if (ret) {
		CDMA_LOG_ERR("post jfs for faa failed, ret = %d, seid = %u, deid = %u.\n",
			     ret, cfg->seid, cfg->deid);
		return DMA_STATUS_INVAL;
	}

	return DMA_STATUS_OK;
}

dma_status cdma_write(struct dma_queue *queue, struct dma_seg *rmt_seg,
		      struct dma_seg *local_seg, struct dma_seg *notify_seg,
		      uint64_t notify_data)
{
	dma_jfs_wr_t wr = {.opcode = CDMA_WR_OPC_WRITE};
	struct cdma_u_queue *cdma_queue;
	dma_sge_t rmt_sge, local_sge;
	dma_jfs_wr_t *bad_wr = NULL;
	dma_tp_cfg_t *cfg;
	int ret;

	if (cdma_rw_check(rmt_seg, local_seg)) {
		CDMA_LOG_ERR("write param check failed.\n");
		return DMA_STATUS_INVAL;
	}

	if (notify_seg) {
		wr.opcode = CDMA_WR_OPC_WRITE_NOTIFY;
		wr.rw.notify_addr = notify_seg->sva;
		wr.rw.notify_data = notify_data;
		wr.rw.notify_tokenid = notify_seg->tid;
		wr.rw.notify_tokenvalue = notify_seg->token_value;
	}

	cdma_fill_comm_wr(&wr, queue);

	cdma_fill_sge(&rmt_sge, &local_sge, rmt_seg, local_seg);

	wr.rw.src.num_sge = 1;
	wr.rw.src.sge = &local_sge;

	wr.rw.dst.num_sge = 1;
	wr.rw.dst.sge = &rmt_sge;

	cdma_queue = to_cdma_u_queue(queue);
	cfg = &cdma_queue->cdma_tp->cfg;

	ret = cdma_u_post_jfs_wr(cdma_queue->cdma_jfs, &wr, &bad_wr);
	if (ret) {
		CDMA_LOG_ERR("post jfs for write failed, ret = %d, seid = %u, deid = %u.\n",
			     ret, cfg->seid, cfg->deid);
		return DMA_STATUS_INVAL;
	}

	return DMA_STATUS_OK;
}

dma_status cdma_read(struct dma_queue *queue, struct dma_seg *rmt_seg,
		     struct dma_seg *local_seg)
{
	dma_jfs_wr_t wr = {.opcode = CDMA_WR_OPC_READ};
	struct cdma_u_queue *cdma_queue;
	dma_sge_t rmt_sge, local_sge;
	dma_jfs_wr_t *bad_wr = NULL;
	dma_tp_cfg_t *cfg;
	int ret;

	if (cdma_rw_check(rmt_seg, local_seg)) {
		CDMA_LOG_ERR("read param check failed.\n");
		return DMA_STATUS_INVAL;
	}

	cdma_fill_comm_wr(&wr, queue);

	cdma_fill_sge(&rmt_sge, &local_sge, rmt_seg, local_seg);

	wr.rw.src.num_sge = 1;
	wr.rw.src.sge = &rmt_sge;

	wr.rw.dst.num_sge = 1;
	wr.rw.dst.sge = &local_sge;

	cdma_queue = to_cdma_u_queue(queue);
	cfg = &cdma_queue->cdma_tp->cfg;

	ret = cdma_u_post_jfs_wr(cdma_queue->cdma_jfs, &wr, &bad_wr);
	if (ret) {
		CDMA_LOG_ERR("post jfs for read failed, ret = %d, seid = %u, deid = %u.\n",
			     ret, cfg->seid, cfg->deid);
		return DMA_STATUS_INVAL;
	}

	return DMA_STATUS_OK;
}

int cdma_cmd_create_jfce(struct dma_context *ctx, dma_jfce_t *jfce)
{
	struct cdma_cmd_create_jfce_args arg = {0};
	struct cdma_ioctl_hdr hdr = {0};
	int ret;

	if (ctx == NULL || ctx->dma_dev == NULL || ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("create jfce cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_CREATE_JFCE;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl execute create jfce, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	jfce->fd = arg.out.fd;
	jfce->id = arg.out.id;

	return 0;
}

static inline void cdma_cmd_set_udrv_priv(struct cdma_cmd_udrv_priv *arg,
					  struct cdma_cmd_udrv_priv *udata)
{
	if (arg != NULL && udata != NULL) {
		*arg = *udata;
	}
}

int cdma_cmd_create_ctp(struct dma_context *ctx, struct dma_tp *ctp,
			struct dma_tp_cfg *cfg)
{
	struct cdma_cmd_create_ctp_args arg = {0};
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!ctx || !ctx->dma_dev || ctx->dma_dev->fd < 0 || !ctp || !cfg) {
		CDMA_LOG_ERR("create ctp cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_CREATE_CTP;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;

	arg.in.scna = cfg->scna;
	arg.in.dcna = cfg->dcna;
	arg.in.seid = cfg->seid;
	arg.in.deid = cfg->deid;
	arg.in.dma_tp = (uint64_t)(void *)ctp;
	arg.in.queue_id = cfg->queue_id;

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl execute create ctp, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	ctp->dma_ctx = ctx;
	ctp->cfg = *cfg;
	ctp->tpn = arg.out.tpn;
	ctp->handle = arg.out.handle;

	return 0;
}

int cdma_cmd_delete_ctp(struct dma_tp *ctp)
{
	struct cdma_cmd_delete_ctp_args arg = {0};
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!ctp || !ctp->dma_ctx || !ctp->dma_ctx->dma_dev ||
		ctp->dma_ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("cdma delete ctp cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_DELETE_CTP;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;
	arg.in.tpn = ctp->tpn;
	arg.in.handle = ctp->handle;
	arg.in.queue_id = ctp->cfg.queue_id;

	ret = ioctl(ctp->dma_ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl execute delete ctp, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	return 0;
}

static inline void cdma_fill_jfs(struct dma_jfs *jfs, struct dma_context *ctx,
				 struct dma_jfs_cfg *cfg,
				 struct cdma_cmd_create_jfs_args *arg)
{
	jfs->jfs_id = arg->out.id;
	jfs->jfs_cfg = *cfg;
	jfs->jfs_cfg.depth = arg->out.depth;
	jfs->jfs_cfg.max_sge = arg->out.max_sge;
	jfs->jfs_cfg.max_rsge = arg->out.max_rsge;
	jfs->dma_ctx = ctx;
	jfs->handle = arg->out.handle;
}

int cdma_u_cmd_create_jfs(struct dma_context *ctx, struct dma_jfs *jfs,
			  struct dma_jfs_cfg *cfg,
			  struct cdma_cmd_udrv_priv *udata)
{
	struct cdma_cmd_create_jfs_args arg = {0};
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!ctx || !ctx->dma_dev || ctx->dma_dev->fd < 0 || !jfs ||
		!cfg || !cfg->jfc) {
		CDMA_LOG_ERR("create jfs cmd parameter invalid.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_CREATE_JFS;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;

	arg.in.depth = cfg->depth;
	arg.in.flag = cfg->flag.value;
	arg.in.priority = cfg->priority;
	arg.in.max_sge = cfg->max_sge;
	arg.in.max_rsge = cfg->max_rsge;
	arg.in.rnr_retry = cfg->rnr_retry;
	arg.in.err_timeout = cfg->err_timeout;
	arg.in.jfc_id = cfg->jfc->jfc_id;
	arg.in.queue_id = cfg->queue_id;
	arg.in.rmt_eid = cfg->rmt_eid;
	arg.in.pld_token_id = cfg->pld_token_id;
	arg.in.tpn = cfg->tpn;
	arg.in.dma_jfs = (uint64_t)jfs;
	arg.in.eid_idx = cfg->eid_idx;
	arg.in.trans_mode = cfg->trans_mode;
	cdma_cmd_set_udrv_priv(&arg.udata, udata);

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl execute create jfs, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	cdma_fill_jfs(jfs, ctx, cfg, &arg);

	return 0;
}

int cdma_u_cmd_delete_jfs(struct dma_jfs *jfs)
{
	struct cdma_cmd_delete_jfs_args arg = {0};
	struct cdma_ioctl_hdr hdr;
	int ret;

	if (!jfs || !jfs->dma_ctx || !jfs->dma_ctx->dma_dev ||
		jfs->dma_ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("delete jfs cmd parameter invalid");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_DELETE_JFS;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;
	arg.in.jfs_id = jfs->jfs_id;
	arg.in.queue_id = jfs->jfs_cfg.queue_id;
	arg.in.handle = jfs->handle;

	ret = ioctl(jfs->dma_ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl execute delete jfs, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	return 0;
}
