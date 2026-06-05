// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <string.h>
#include "cdma_u_db.h"
#include "cdma_u_log.h"
#include "cdma_u_cmd.h"
#include "cdma_u_log.h"
#include "cdma_u_jfc.h"
#include "cdma_u_jfs.h"

static int cdma_u_exec_create_jfs_cmd(struct dma_context *ctx,
				      struct cdma_u_jfs *jfs,
				      struct dma_jfs_cfg *cfg)
{
	struct cdma_create_jfs_ucmd cmd = {0};
	struct cdma_cmd_udrv_priv udata = {0};
	int ret;

	cmd.buf_addr = (uintptr_t)jfs->sq.qbuf;
	cmd.buf_len = jfs->sq.qbuf_size;
	cmd.jetty_addr = (uintptr_t)&jfs->sq;
	cmd.sqe_bb_cnt = jfs->sq.sqe_bb_cnt;
	cmd.jetty_type = jfs->jfs_type;
	cmd.queue_id = cfg->queue_id;
	cmd.non_pin = jfs->sq.cstm;
	cmd.tid = ctx->tid;
	cdma_u_set_udata(&udata, &cmd, sizeof(cmd), NULL, 0);

	ret = cdma_u_cmd_create_jfs(ctx, &jfs->base, cfg, &udata);
	if (ret)
		CDMA_LOG_ERR("cmd create jfs failed, ret = %d.\n", ret);

	return ret;
}

static inline void cdma_u_init_sq(struct cdma_u_jetty_queue *sq)
{
	sq->flush_flag = false;
	sq->pi = 0;
	sq->ci = 0;
}

static inline uint32_t cdma_sq_cal_wqebb_num(uint32_t sqe_ctl_len,
					     uint32_t sge_num,
					     uint32_t wqebb_size)
{
	return (sqe_ctl_len + (sge_num - 1) * CDMA_SGE_SIZE) / wqebb_size + 1;
}

static int cdma_u_create_sq(struct cdma_u_jetty_queue *sq,
			    struct dma_jfs_cfg *cfg)
{
	uint32_t wqebb_depth;
	uint32_t sqe_bb_cnt;

	if (pthread_spin_init(&sq->lock, PTHREAD_PROCESS_PRIVATE)) {
		CDMA_LOG_ERR("init lock failed.\n");
		return -EINVAL;
	}

	cdma_u_init_sq(sq);

	sqe_bb_cnt = cdma_sq_cal_wqebb_num(SQE_WRITE_NOTIFY_CTL_LEN,
					   cfg->max_sge, CDMA_JFS_WQEBB);
	if (sqe_bb_cnt > MAX_SQE_BB_NUM)
		sqe_bb_cnt = MAX_SQE_BB_NUM;
	sq->sqe_bb_cnt = sqe_bb_cnt;
	sq->max_sge_num = cfg->max_sge;
	wqebb_depth = roundup_pow_of_two(sqe_bb_cnt * cfg->depth);
	if (cdma_u_alloc_queue_buf(sq, wqebb_depth, CDMA_JFS_WQEBB,
							   CDMA_HW_PAGE_SIZE, true)) {
		CDMA_LOG_ERR("alloc jfs wqe buf failed.\n");
		(void)pthread_spin_destroy(&sq->lock);
		return -EINVAL;
	}

	return 0;
}

static int cdma_verify_jfs_param(struct dma_context *ctx,
				 struct dma_jfs_cfg *cfg)
{
	struct cdma_device_cap *caps = &ctx->dma_dev->attr.dev_cap;

	if (!cfg->depth || cfg->depth > caps->max_jfs_depth ||
		cfg->max_sge > caps->max_jfs_sge) {
		CDMA_LOG_ERR("jfs param is invalid, depth = %u, sge = %u, \
					 max_depth = %u, max_jfs_sge = %u.\n",
					 cfg->depth, cfg->max_sge, caps->max_jfs_depth,
					 caps->max_jfs_sge);
		return -EINVAL;
	}

	return 0;
}

static inline void cdma_u_delete_sq(struct cdma_u_jetty_queue *sq)
{
	cdma_u_free_queue_buf(sq);
	pthread_spin_destroy(&sq->lock);
}

struct dma_jfs *cdma_u_create_jfs(struct dma_context *ctx,
				  struct dma_jfs_cfg *cfg)
{
	struct cdma_u_jfs *jfs;
	int ret;

	if (!ctx || !cfg || !ctx->dma_dev) {
		CDMA_LOG_ERR("create jfs parameter invalid.\n");
		return NULL;
	}

	ret = cdma_verify_jfs_param(ctx, cfg);
	if (ret)
		return NULL;

	jfs = (struct cdma_u_jfs *)calloc(1, sizeof(*jfs));
	if (!jfs) {
		CDMA_LOG_ERR("alloc jfs memory failed.\n");
		return NULL;
	}

	if (cdma_u_create_sq(&jfs->sq, cfg)) {
		CDMA_LOG_ERR("create sq failed.\n");
		goto err_create_sq;
	}

	jfs->jfs_type = CDMA_NORMAL_JETTY_TYPE;
	if (cdma_u_exec_create_jfs_cmd(ctx, jfs, cfg))
		goto err_exec_cmd;

	jfs->sq.db.id = jfs->base.jfs_id;
	jfs->sq.db.type = CDMA_MMAP_JETTY_DSQE;
	jfs->sq.idx = jfs->base.jfs_id;

	if (cdma_u_alloc_db(ctx, &jfs->sq.db))
		goto err_alloc_db;

	jfs->sq.dwqe_addr = jfs->sq.db.addr;

	return &jfs->base;

err_alloc_db:
	cdma_u_cmd_delete_jfs(&jfs->base);
err_exec_cmd:
	cdma_u_delete_sq(&jfs->sq);
err_create_sq:
	free(jfs);

	return NULL;
}

int cdma_u_delete_jfs(struct dma_jfs *jfs)
{
	struct cdma_u_context *cdma_ctx;
	struct cdma_u_jfs *cdma_jfs;
	int ret;

	if (!jfs || !jfs->dma_ctx) {
		CDMA_LOG_ERR("dma parameter invalid in delete jfs.\n");
		return -EINVAL;
	}

	cdma_ctx = to_cdma_u_ctx(jfs->dma_ctx);
	cdma_jfs = to_cdma_u_jfs(jfs);
	if (!cdma_ctx || !cdma_jfs) {
		CDMA_LOG_ERR("user dma parameter invalid in delete jfs.\n");
		return -EINVAL;
	}

	ret = cdma_u_cmd_delete_jfs(jfs);
	if (ret)
		CDMA_LOG_WARN("cdma delete jfs cmd failed, ret = %d.\n", ret);

	if (!!jfs->jfs_cfg.jfc)
		cdma_u_clean_jfc((dma_jfc_t *)jfs->jfs_cfg.jfc, jfs->jfs_id);

	cdma_u_free_db(jfs->dma_ctx, &cdma_jfs->sq.db);
	cdma_u_delete_sq(&cdma_jfs->sq);
	free(cdma_jfs);

	return 0;
}

static uint8_t cdma_parse_jfs_opcode(dma_wr_opcode_t opcode)
{
	switch (opcode) {
	case CDMA_WR_OPC_READ:
		return CDMA_OPCODE_READ;
	case CDMA_WR_OPC_WRITE:
		return CDMA_OPCODE_WRITE;
	case CDMA_WR_OPC_WRITE_NOTIFY:
		return CDMA_OPCODE_WRITE_WITH_NOTIFY;
	case CDMA_WR_OPC_CAS:
		return CDMA_OPCODE_CAS;
	case CDMA_WR_OPC_FADD:
		return CDMA_OPCODE_FAA;
	case CDMA_WR_OPC_NOP:
		return CDMA_OPCODE_NOP;
	default:
		return CDMA_OPCODE_INVALID;
	}
}

static bool cdma_check_sge_num(uint8_t opcode, struct cdma_u_jetty_queue *sq,
			       dma_jfs_wr_t *wr)
{
	switch (opcode) {
	case CDMA_OPCODE_CAS:
	case CDMA_OPCODE_FAA:
		return sq->max_sge_num == 0;
	case CDMA_OPCODE_READ:
		return (wr->rw.dst.num_sge > CDMA_JFS_MAX_SGE_READ) ||
				(wr->rw.dst.num_sge > sq->max_sge_num);
	case CDMA_OPCODE_WRITE_WITH_NOTIFY:
		return (wr->rw.src.num_sge > CDMA_JFS_MAX_SGE_NOTIFY) ||
				(wr->rw.src.num_sge > sq->max_sge_num);
	default:
		return wr->rw.src.num_sge > sq->max_sge_num;
	}
}

static inline uint32_t cdma_get_ctl_len(uint8_t opcode)
{
	if (opcode == CDMA_WR_OPC_WRITE_NOTIFY)
		return SQE_WRITE_NOTIFY_CTL_LEN;

	return SQE_NORMAL_CTL_LEN;
}

static int cdma_fill_multi_sge(struct cdma_jfs_sqe_ctl *wqe_ctl, dma_jfs_wr_t *wr,
			       struct cdma_wqe_sge *sge)
{
	uint32_t sge_num = 0;
	dma_sge_t *sge_info;
	uint32_t num_sge;
	uint32_t i;

	sge_info = wr->rw.src.sge;
	num_sge = wr->rw.src.num_sge;

	for (i = 0; i < num_sge; i++) {
		if (unlikely(sge_info[i].len == 0))
			continue;
		sge->va = sge_info[i].addr;
		sge->length = sge_info[i].len;
		sge->token_id = sge_info[i].seg.tid;
		sge++;
		sge_num++;
	}

	wqe_ctl->sge_num = sge_num;

	return 0;
}

static int cdma_u_fill_write_sqe(struct cdma_jfs_sqe_ctl *wqe_ctl,
				 dma_jfs_wr_t *wr)

{
	struct cdma_token_info *token_info;
	struct cdma_wqe_sge *sge;
	dma_sge_t *sge_info;

	sge = (struct cdma_wqe_sge *)((char *)wqe_ctl + cdma_get_ctl_len(wr->opcode));
	if (cdma_fill_multi_sge(wqe_ctl, wr, sge)) {
		CDMA_LOG_ERR("cdma fill sw sge invalid.\n");
		return -EINVAL;
	}

	sge_info = wr->rw.dst.sge;

	wqe_ctl->toid = sge_info->seg.tid;
	wqe_ctl->token_en = sge_info->seg.token_value_valid;
	wqe_ctl->rmt_token_value = sge_info->seg.token_value;
	wqe_ctl->target_hint = wr->rw.target_hint;
	wqe_ctl->rmt_addr_l_or_tid =
		sge_info->addr & (uint32_t)SQE_CTL_RMA_ADDR_BIT;
	wqe_ctl->rmt_addr_h_or_tval =
		(sge_info->addr >> (uint32_t)SQE_CTL_RMA_ADDR_OFFSET) &
		(uint32_t)SQE_CTL_RMA_ADDR_BIT;

	if (wr->opcode == CDMA_WR_OPC_WRITE_NOTIFY) {
		token_info = (struct cdma_token_info *)((char *)wqe_ctl +
							SQE_NOTIFY_TOKEN_ID_FIELD);
		token_info->token_id = wr->rw.notify_tokenid;
		token_info->token_value = wr->rw.notify_tokenvalue;

		memcpy((char *)wqe_ctl + SQE_NOTIFY_ADDR_FIELD, &wr->rw.notify_addr,
		       sizeof(uint64_t));
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD, &wr->rw.notify_data,
		       sizeof(uint64_t));
	}

	return 0;
}

static int cdma_u_fill_read_sqe(struct cdma_jfs_sqe_ctl *wqe_ctl,
				dma_jfs_wr_t *wr)
{
	struct cdma_wqe_sge *sge;
	uint32_t sge_num = 0;
	dma_sge_t *sge_info;
	uint32_t i;

	sge = (struct cdma_wqe_sge *)(wqe_ctl + 1);
	sge_info = wr->rw.dst.sge;

	for (i = 0; i < wr->rw.dst.num_sge; i++) {
		if (unlikely(sge_info[i].len == 0))
			continue;
		sge->va = sge_info[i].addr;
		sge->length = sge_info[i].len;
		sge->token_id = sge_info[i].seg.tid;
		sge++;
		sge_num++;
	}

	sge_info = wr->rw.src.sge;

	wqe_ctl->sge_num = sge_num;
	wqe_ctl->toid = sge_info->seg.tid;
	wqe_ctl->token_en = sge_info->seg.token_value_valid;
	wqe_ctl->rmt_token_value = sge_info->seg.token_value;
	wqe_ctl->rmt_addr_l_or_tid =
		sge_info->addr & (uint32_t)SQE_CTL_RMA_ADDR_BIT;
	wqe_ctl->rmt_addr_h_or_tval =
		(sge_info->addr >> (uint32_t)SQE_CTL_RMA_ADDR_OFFSET) &
		(uint32_t)SQE_CTL_RMA_ADDR_BIT;

	return 0;
}

static inline bool cdma_check_atomic_len(uint32_t len, uint8_t opcode)
{
	if (opcode == CDMA_WR_OPC_CAS) {
		return len == CDMA_ATOMIC_LEN_4 || len == CDMA_ATOMIC_LEN_8 ||
			   len == CDMA_ATOMIC_LEN_16;
	} else if (opcode == CDMA_WR_OPC_FADD) {
		return len == CDMA_ATOMIC_LEN_4 || len == CDMA_ATOMIC_LEN_8;
	}

	return false;
}

static int cdma_u_fill_cas_sqe(struct cdma_jfs_sqe_ctl *wqe_ctl,
			       dma_jfs_wr_t *wr)
{
	struct cdma_wqe_sge *sge;
	dma_sge_t *sge_info;

	sge_info = wr->cas.src;
	if (!cdma_check_atomic_len(sge_info->len, wr->opcode)) {
		CDMA_LOG_ERR("cdma cas sge len invalid, len = %u.\n", sge_info->len);
		return -EINVAL;
	}

	sge = (struct cdma_wqe_sge *)(wqe_ctl + 1);
	sge->va = sge_info->addr;
	sge->length = sge_info->len;
	sge->token_id = sge_info->seg.tid;

	sge_info = wr->cas.dst;
	wqe_ctl->sge_num = CDMA_ATOMIC_SGE_NUM;
	wqe_ctl->toid = sge_info->seg.tid;
	wqe_ctl->token_en = sge_info->seg.token_value_valid;
	wqe_ctl->rmt_token_value = sge_info->seg.token_value;
	wqe_ctl->rmt_addr_l_or_tid =
		sge_info->addr & (uint32_t)SQE_CTL_RMA_ADDR_BIT;
	wqe_ctl->rmt_addr_h_or_tval =
		(sge_info->addr >> (uint32_t)SQE_CTL_RMA_ADDR_OFFSET) &
		(uint32_t)SQE_CTL_RMA_ADDR_BIT;

	if (sge->length <= CDMA_ATOMIC_LEN_8) {
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD, &wr->cas.swap_data,
		       sge->length);
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD + sge->length,
		       &wr->cas.cmp_data, sge->length);
	} else {
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD,
		       (char *)wr->cas.swap_addr, sge->length);
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD + sge->length,
		       (char *)wr->cas.cmp_addr, sge->length);
	}

	return 0;
}

static int cdma_u_fill_faa_sqe(struct cdma_jfs_sqe_ctl *wqe_ctl,
			       dma_jfs_wr_t *wr)
{
	struct cdma_wqe_sge *sge;
	dma_sge_t *sge_info;

	sge_info = wr->faa.src;
	if (!cdma_check_atomic_len(sge_info->len, wr->opcode)) {
		CDMA_LOG_ERR("cdma faa sge len invalid, len = %u.\n", sge_info->len);
		return -EINVAL;
	}

	sge = (struct cdma_wqe_sge *)(wqe_ctl + 1);
	sge->va = sge_info->addr;
	sge->length = sge_info->len;
	sge->token_id = sge_info->seg.tid;

	sge_info = wr->faa.dst;
	wqe_ctl->sge_num = CDMA_ATOMIC_SGE_NUM;
	wqe_ctl->toid = sge_info->seg.tid;
	wqe_ctl->token_en = sge_info->seg.token_value_valid;
	wqe_ctl->rmt_token_value = sge_info->seg.token_value;
	wqe_ctl->rmt_addr_l_or_tid =
		sge_info->addr & (uint32_t)SQE_CTL_RMA_ADDR_BIT;
	wqe_ctl->rmt_addr_h_or_tval =
		(sge_info->addr >> (uint32_t)SQE_CTL_RMA_ADDR_OFFSET) &
		(uint32_t)SQE_CTL_RMA_ADDR_BIT;

	if (sge->length <= CDMA_ATOMIC_LEN_8) {
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD, &wr->faa.operand,
		       sge->length);
	} else {
		memcpy((char *)wqe_ctl + SQE_ATOMIC_DATA_FIELD,
		       (void *)wr->faa.operand_addr, sge->length);
	}

	return 0;
}

static int cdma_fill_normal_sge(struct cdma_jfs_sqe_ctl *wqe_ctl,
				dma_jfs_wr_t *wr)
{
	switch (wr->opcode) {
	case CDMA_WR_OPC_WRITE:
	case CDMA_WR_OPC_WRITE_NOTIFY:
		return cdma_u_fill_write_sqe(wqe_ctl, wr);
	case CDMA_WR_OPC_READ:
		return cdma_u_fill_read_sqe(wqe_ctl, wr);
	case CDMA_WR_OPC_CAS:
		return cdma_u_fill_cas_sqe(wqe_ctl, wr);
	case CDMA_WR_OPC_FADD:
		return cdma_u_fill_faa_sqe(wqe_ctl, wr);
	default:
		CDMA_LOG_ERR("cdma wr opcode invalid, opcode = %u.\n",
						(uint8_t)wr->opcode);
		return -EINVAL;
	}
}

static int cdma_set_sqe(struct cdma_jfs_sqe_ctl *wqe_ctl,
			struct cdma_u_jetty_queue *sq, dma_jfs_wr_t *wr,
			uint8_t opcode)
{
	int ret;

	wqe_ctl->cqe = wr->flag.bs.complete_enable;
	wqe_ctl->owner = (sq->pi & sq->baseblk_cnt) == 0 ? 1 : 0;
	wqe_ctl->opcode = opcode;
	wqe_ctl->tpn = wr->tpn;
	wqe_ctl->place_odr = wr->flag.bs.place_order;
	wqe_ctl->fence = wr->flag.bs.fence;
	wqe_ctl->comp_order = wr->flag.bs.comp_order;
	wqe_ctl->se = wr->flag.bs.solicited_enable;
	wqe_ctl->inline_en = 0;
	memcpy(wqe_ctl->rmt_eid, &wr->rmt_eid, sizeof(wr->rmt_eid));

	ret = cdma_fill_normal_sge(wqe_ctl, wr);
	if (ret)
		CDMA_LOG_ERR("cdma fill normal sge failed, opcode = %u in wr, ret = %d.\n",
			     (uint8_t)wr->opcode, ret);

	return ret;
}

static inline uint32_t cdma_get_normal_sge_num(uint8_t opcode,
					       struct cdma_jfs_sqe_ctl *wqe_ctl)
{
	switch (opcode) {
	case CDMA_WR_OPC_CAS:
	case CDMA_WR_OPC_FADD:
		return CDMA_ATOMIC_SGE_NUM + 1;
	default:
		return wqe_ctl->sge_num;
	}
}

static uint32_t cdma_get_wqebb_cnt(dma_jfs_wr_t *wr,
				   struct cdma_jfs_sqe_ctl *wqe_ctl)
{
	uint32_t normal_sge_num;
	uint32_t sqe_ctl_len;
	uint32_t wqebb_cnt;

	if (wr->opcode == CDMA_WR_OPC_NOP)
		return NOP_WQEBB_CNT;

	sqe_ctl_len = cdma_get_ctl_len(wr->opcode);

	normal_sge_num = cdma_get_normal_sge_num(wr->opcode, wqe_ctl);
	wqebb_cnt =
		cdma_sq_cal_wqebb_num(sqe_ctl_len, normal_sge_num, CDMA_JFS_WQEBB);

	return wqebb_cnt;
}

static inline bool check_sq_overflow(struct cdma_u_jetty_queue *sq,
				     uint32_t wqebb_cnt)
{
	return (sq->pi - sq->ci + wqebb_cnt) > sq->baseblk_cnt;
}

static int cdma_copy_to_sq(struct cdma_u_jetty_queue *sq, uint32_t wqebb_cnt,
			   struct cdma_jfs_wqebb *tmp_sq)
{
	uint32_t remain = sq->baseblk_cnt - (sq->pi & sq->baseblk_mask);
	uint32_t tail_cnt;
	uint32_t head_cnt;

	if (check_sq_overflow(sq, wqebb_cnt))
		return -ENOMEM;

	tail_cnt = remain > wqebb_cnt ? wqebb_cnt : remain;
	head_cnt = wqebb_cnt - tail_cnt;

	memcpy(sq->qbuf_curr, tmp_sq, tail_cnt * sizeof(*tmp_sq));

	if (head_cnt)
		memcpy(sq->qbuf, tmp_sq + tail_cnt, head_cnt * sizeof(*tmp_sq));

	return 0;
}

static int cdma_post_one_wr(struct cdma_u_context *cdma_ctx,
			    struct cdma_u_jetty_queue *sq, dma_jfs_wr_t *wr,
			    struct cdma_jfs_sqe_ctl **dwqe_addr, uint8_t *dwqe_enable)
{
	struct cdma_jfs_wqebb tmp_sq[MAX_SQE_BB_NUM] = {0};
	uint32_t wqebb_cnt, i;
	uint8_t opcode;
	int ret;

	opcode = cdma_parse_jfs_opcode(wr->opcode);
	if (opcode == CDMA_OPCODE_INVALID) {
		CDMA_LOG_ERR("cdma jfs parse invalid opcode = %u.\n",
			     (uint8_t)wr->opcode);
		return -EINVAL;
	}

	if (cdma_check_sge_num(opcode, sq, wr)) {
		CDMA_LOG_ERR("cdma sge num invalid.\n");
		return -EINVAL;
	}

	ret = cdma_set_sqe((struct cdma_jfs_sqe_ctl *)tmp_sq, sq, wr, opcode);
	if (ret)
		return ret;

	wqebb_cnt = cdma_get_wqebb_cnt(wr, (struct cdma_jfs_sqe_ctl *)tmp_sq);
	if (wqebb_cnt == 1 && cdma_ctx->dwqe_enable)
		*dwqe_enable = 1;

	if (wqebb_cnt > MAX_SQE_BB_NUM)
		wqebb_cnt = MAX_SQE_BB_NUM;

	ret = cdma_copy_to_sq(sq, wqebb_cnt, tmp_sq);
	if (ret) {
		CDMA_LOG_ERR("cdma jfs sq overflow, ret = %d.\n", ret);
		return ret;
	}

	*dwqe_addr = (struct cdma_jfs_sqe_ctl *)sq->qbuf_curr;

	sq->qbuf_curr = cdma_update_ptr((uint8_t *)sq->qbuf_curr,
		wqebb_cnt << sq->baseblk_shift,
		(uint8_t *)sq->qbuf,
		(uint8_t *)sq->qbuf + sq->qbuf_size);

	for (i = 0; i < wqebb_cnt; i++)
		sq->wrid[(sq->pi + i) & sq->baseblk_mask] = wr->user_ctx;

	sq->pi += wqebb_cnt;

	return 0;
}

static void cdma_write_dsqe(struct cdma_u_jetty_queue *sq,
			    struct cdma_jfs_sqe_ctl *ctrl)
{
	ctrl->sqe_bb_idx = sq->pi;
	mmio_memcpy_x64((uint64_t *)sq->dwqe_addr, (uint64_t *)ctrl);
}

static void cdma_update_sq_db(struct cdma_u_jetty_queue *sq)
{
	uint32_t volatile *db_addr = (uint32_t volatile *)((char volatile *)sq->db.addr + CDMA_DOORBELL_OFFSET);
	*db_addr = sq->pi;
}

static int cdma_post_sq_wr(struct cdma_u_context *cdma_ctx,
			   struct cdma_u_jetty_queue *sq, dma_jfs_wr_t *wr,
			   dma_jfs_wr_t **bad_wr)
{
	struct cdma_jfs_sqe_ctl *dwqe_addr;
	uint8_t dwqe_enable = 0;
	dma_jfs_wr_t *it;
	int wr_cnt = 0;
	int ret = 0;

	(void)pthread_spin_lock(&sq->lock);

	for (it = wr; it != NULL; it = it->next) {
		ret = cdma_post_one_wr(cdma_ctx, sq, it, &dwqe_addr, &dwqe_enable);
		if (ret) {
			CDMA_LOG_ERR("cdma jfs post one wr failed, ret = %d.\n", ret);
			*bad_wr = it;
			goto out;
		}
		wr_cnt++;
	}

out:
	if (wr_cnt) {
		cdma_to_device_barrier();

		if (wr_cnt == 1 && dwqe_enable && (sq->pi - sq->ci == 1))
			cdma_write_dsqe(sq, dwqe_addr);
		else
			cdma_update_sq_db(sq);
	}

	(void)pthread_spin_unlock(&sq->lock);

	return ret;
}

int cdma_u_post_jfs_wr(struct dma_jfs *jfs, dma_jfs_wr_t *wr,
		       dma_jfs_wr_t **bad_wr)
{
	struct cdma_u_context *cdma_ctx;
	struct cdma_u_jfs *cdma_jfs;
	int ret;

	cdma_jfs = to_cdma_u_jfs(jfs);
	cdma_ctx = to_cdma_u_ctx(jfs->dma_ctx);

	ret = cdma_post_sq_wr(cdma_ctx, &cdma_jfs->sq, wr, bad_wr);
	if (ret)
		CDMA_LOG_ERR("cdma jfs post wr failed, ret = %d.\n", ret);

	return ret;
}
