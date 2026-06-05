// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <sys/ioctl.h>
#include "cdma_u_cmd.h"
#include "cdma_u_db.h"
#include "cdma_u_log.h"
#include "cdma_u_jfc.h"

static int cdma_u_check_jfc_cfg(struct dma_context *ctx, dma_jfc_cfg_t *cfg)
{
	struct cdma_device_cap *cap;

	if (ctx == NULL || ctx->dma_dev == NULL || cfg == NULL ||
	    cfg->jfce == NULL) {
		CDMA_LOG_ERR("jfc ctx or cfg is null.\n");
		return -EINVAL;
	}

	cap = &ctx->dma_dev->attr.dev_cap;
	if (cfg->depth == 0 || cfg->depth > cap->max_jfc_depth) {
		CDMA_LOG_ERR("invalid jfc cfg depth = %u, cap depth = %u.\n",
			      cfg->depth, cap->max_jfc_depth);
		return -EINVAL;
	}

	if (cfg->ceqn >= cap->ceq_cnt) {
		CDMA_LOG_ERR("invalid cfg ceqn = %u, cap ceq_cnt = %u.\n",
			      cfg->ceqn, cap->ceq_cnt);
		return -EINVAL;
	}

	return 0;
}

static int cdma_u_alloc_jfc_buf(struct cdma_u_context *cdma_ctx,
				dma_jfc_cfg_t *cfg, struct cdma_u_jfc *jfc)
{
	uint32_t depth;
	int ret;

	depth = cfg->depth < CDMA_U_MIN_JFC_DEPTH ? CDMA_U_MIN_JFC_DEPTH :
			cfg->depth;
	ret = cdma_u_alloc_queue_buf(&jfc->cq, depth, cdma_ctx->cqe_size,
								 CDMA_HW_PAGE_SIZE, false);
	if (ret) {
		CDMA_LOG_ERR("alloc user jfc wqe buf failed, ret = %d.\n", ret);
		return ret;
	}

	jfc->sw_db = (uint32_t *)cdma_u_alloc_sw_db(cdma_ctx);
	if (!jfc->sw_db) {
		CDMA_LOG_ERR("alloc user jfc sw db failed.\n");
		cdma_u_free_queue_buf(&jfc->cq);
		return -ENOMEM;
	}

	return 0;
}

static void cdma_u_free_jfc_buf(struct cdma_u_context *cdma_ctx,
				struct cdma_u_jfc *cdma_jfc)
{
	cdma_u_free_sw_db(cdma_ctx, cdma_jfc->sw_db);
	cdma_u_free_queue_buf(&cdma_jfc->cq);
}

static inline void cdma_ack_event_init(pthread_mutex_t *mutex,
				       pthread_cond_t *cond,
				       uint32_t *events_acked)
{
	*events_acked = 0;
	(void)pthread_mutex_init(mutex, NULL);
	(void)pthread_cond_init(cond, NULL);
}

static void cdma_fill_jfc(dma_jfc_t *jfc, struct dma_context *ctx,
			  dma_jfc_cfg_t *cfg,
			  struct cdma_cmd_create_jfc_args *arg)
{
	jfc->jfc_id = arg->out.id;
	jfc->handle = arg->out.handle;
	jfc->jfc_cfg = *cfg;
	jfc->jfc_cfg.depth = arg->out.depth;
	jfc->dma_ctx = ctx;
	jfc->comp_events_acked = 0;
	cdma_ack_event_init(&jfc->event_mutex, &jfc->event_cond,
						&jfc->async_events_acked);
}

static int cdma_cmd_create_jfc(struct dma_context *ctx, struct cdma_u_jfc *jfc,
			       dma_jfc_cfg_t *cfg)
{
	struct cdma_cmd_create_jfc_args arg = { 0 };
	struct cdma_create_jfc_ucmd cmd = { 0 };
	struct cdma_ioctl_hdr hdr = { 0 };
	int ret;

	if (ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return -EINVAL;
	}

	cmd.buf_addr = (uintptr_t)jfc->cq.qbuf;
	cmd.db_addr = (uintptr_t)jfc->sw_db;
	cmd.buf_len = jfc->cq.qbuf_size;
	cmd.tid = ctx->tid;
	cdma_u_set_udata(&arg.udata, &cmd, sizeof(cmd), NULL, 0);

	hdr.command = (uint32_t)CDMA_CMD_CREATE_JFC;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;

	arg.in.depth = cfg->depth;
	arg.in.jfce_fd = cfg->jfce->fd;
	arg.in.jfce_id = cfg->jfce->id;
	arg.in.ceqn = cfg->ceqn;
	arg.in.queue_id = cfg->queue_id;

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl in create jfc, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	cdma_fill_jfc(&jfc->base, ctx, cfg, &arg);

	return 0;
}

static int cdma_cmd_delete_jfc(struct dma_jfc *jfc)
{
	struct cdma_cmd_delete_jfc_args arg = { 0 };
	struct cdma_ioctl_hdr hdr = { 0 };
	int ret;

	if (jfc->dma_ctx->dma_dev->fd < 0) {
		CDMA_LOG_ERR("invalid fd parameter.\n");
		return -EINVAL;
	}

	hdr.command = (uint32_t)CDMA_CMD_DELETE_JFC;
	hdr.args_len = (uint32_t)sizeof(arg);
	hdr.args_addr = (uint64_t)&arg;
	arg.in.handle = jfc->handle;
	arg.in.jfcn = jfc->jfc_id;
	arg.in.queue_id = jfc->jfc_cfg.queue_id;

	ret = ioctl(jfc->dma_ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl in cdma_delete_jfc, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	return 0;
}

static inline void *cdma_u_get_buf_entry(struct cdma_u_jetty_queue *cq,
					 uint32_t n)
{
	uint32_t cqe_index = n & cq->baseblk_mask;
	uint32_t cqe_size = 1U << cq->baseblk_shift;

	return (char *)cq->qbuf + (cqe_index * cqe_size);
}

static struct cdma_u_jfc_cqe *cdma_u_get_next_cqe(struct cdma_u_jfc *jfc,
						  uint32_t n)
{
	struct cdma_u_jfc_cqe *cqe;
	uint32_t valid_owner;

	cqe = (struct cdma_u_jfc_cqe *)cdma_u_get_buf_entry(&jfc->cq, n);
	valid_owner = (jfc->cq.ci >> align_power2(jfc->cq.baseblk_cnt)) &
		       CDMA_JFC_DB_VALID_OWNER_M;
	if (!(cqe->owner ^ valid_owner))
		return NULL;

	return cqe;
}

static struct cdma_u_jetty_queue *cdma_u_update_jetty_idx(struct cdma_u_jfc_cqe *cqe)
{
	struct cdma_u_jetty_queue *queue;

	queue = (struct cdma_u_jetty_queue *)((uint64_t)cqe->user_data[1] <<
			CDMA_ADDR_SHIFT | cqe->user_data[0]);
	if (!queue)
		return NULL;

	if (!!cqe->fd)
		return queue;

	queue->ci += (cqe->entry_idx - queue->ci) & queue->baseblk_mask;

	return queue;
}

static enum jfc_poll_state cdma_get_cr_status(uint8_t src_status,
					      uint8_t substatus,
					      enum dma_cr_status *cr_status)
{
struct cdma_cqe_status {
	bool is_valid;
	enum dma_cr_status cr_status;
};

	static const struct cdma_cqe_status
		map[CDMA_CQE_STATUS_NUM][CDMA_CQE_SUB_STATUS_NUM] = {
		{{true, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS}},
		{{true, DMA_CR_UNSUPPORTED_OPCODE_ERR},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS}},
		{{false, DMA_CR_SUCCESS},
		 {true, DMA_CR_LOC_LEN_ERR},
		 {true, DMA_CR_LOC_ACCESS_ERR},
		 {true, DMA_CR_REM_RESP_LEN_ERR},
		 {true, DMA_CR_LOC_DATA_POISON}},
		{{false, DMA_CR_SUCCESS},
		 {true, DMA_CR_REM_UNSUPPORTED_REQ_ERR},
		 {true, DMA_CR_REM_ACCESS_ABORT_ERR},
		 {false, DMA_CR_SUCCESS},
		 {true, DMA_CR_REM_DATA_POISON}},
		{{true, DMA_CR_RNR_RETRY_CNT_EXC_ERR},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS}},
		{{true, DMA_CR_ACK_TIMEOUT_ERR},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS}},
		{{true, DMA_CR_WR_FLUSH_ERR},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS},
		 {false, DMA_CR_SUCCESS}}};

	if ((src_status < CDMA_CQE_STATUS_NUM) &&
	    (substatus < CDMA_CQE_SUB_STATUS_NUM) &&
	    map[src_status][substatus].is_valid) {
		*cr_status = map[src_status][substatus].cr_status;
		return JFC_OK;
	}

	CDMA_LOG_ERR("cqe_status (%u) substatus (%u) is invalid.", src_status,
				 substatus);

	return JFC_POLL_ERR;
}

static enum jfc_poll_state cdma_u_update_flush_cr(struct cdma_u_jetty_queue *queue,
						  struct cdma_u_jfc_cqe *cqe,
						  struct dma_cr *cr)
{
	uint8_t flush_done;

	if (cdma_get_cr_status(cqe->status, cqe->substatus, &cr->status))
		return JFC_POLL_ERR;

	flush_done = cqe->fd;
	if (!!flush_done) {
		cr->status = DMA_CR_WR_FLUSH_ERR_DONE;
		queue->flush_flag = true;
	} else {
		queue->ci++;
	}

	return JFC_OK;
}

static enum jfc_poll_state cdma_parse_cqe_for_jfc(struct cdma_u_jfc_cqe *cqe,
						  struct dma_cr *cr)
{
	struct cdma_u_jetty_queue *queue;

	queue = cdma_u_update_jetty_idx(cqe);
	if (!queue) {
		CDMA_LOG_ERR("update jetty idx failed.");
		return JFC_POLL_ERR;
	}

	cr->flag.bs.s_r = cqe->s_r;
	cr->flag.bs.jetty = cqe->is_jetty;
	cr->completion_len = cqe->byte_cnt;
	cr->tpn = cqe->tpn;
	cr->local_id = (cqe->local_num_h << CDMA_SRC_IDX_SHIFT) | cqe->local_num_l;
	cr->remote_id = cqe->rmt_idx;
	cr->user_ctx = queue->wrid[queue->ci & queue->baseblk_mask];

	if (cqe->status) {
		CDMA_LOG_WARN("get sq %u cqe status abnormal, ci = %u,"
			      " pi = %u, status = %u, substatus = %u.\n", queue->idx,
			      queue->ci, queue->pi, cqe->status, cqe->substatus);
	}

	if (cdma_u_update_flush_cr(queue, cqe, cr))
		return JFC_POLL_ERR;

	return JFC_OK;
}

static enum jfc_poll_state cdma_u_poll_one(struct cdma_u_jfc *cdma_jfc,
					   struct dma_cr *cr)
{
	struct cdma_u_jfc_cqe *cqe;
	enum dma_cr_status status;

	cqe = cdma_u_get_next_cqe(cdma_jfc, cdma_jfc->cq.ci);
	if (!cqe)
		return JFC_EMPTY;

	++cdma_jfc->cq.ci;

	cdma_from_device_barrier();

	if (cdma_parse_cqe_for_jfc(cqe, cr))
		return JFC_POLL_ERR;

	status = cr->status;
	if (status == DMA_CR_WR_FLUSH_ERR_DONE ||
	    status == DMA_CR_WR_SUSPEND_DONE) {
		CDMA_LOG_INFO("poll cr flush/suspend done, jfc_id = %u, status = %u.\n",
			       cdma_jfc->base.jfc_id, status);
		return JFC_EMPTY;
	}

	return JFC_OK;
}

static int cdma_cmd_wait_jfc(int jfce_fd, uint32_t jfc_cnt, int time_out)
{
	struct cdma_cmd_jfce_wait_args arg = { 0 };
	int ret;

	arg.in.max_event_cnt = jfc_cnt;
	arg.in.time_out = time_out;

	ret = ioctl(jfce_fd, CDMA_CMD_WAIT_JFC, &arg);
	if (ret) {
		CDMA_LOG_ERR("wait jfc ioctl, ret = %d %d.\n", ret, errno);
		return -EFAULT;
	}

	return (int)arg.out.event_cnt;
}

static inline void cdma_ack_comp_event(pthread_mutex_t *mutex,
				       pthread_cond_t *cond,
				       uint32_t *events_acked,
				       uint32_t nevent)
{
	(void)pthread_mutex_lock(mutex);
	*events_acked = *events_acked + nevent;
	(void)pthread_cond_signal(cond);
	(void)pthread_mutex_unlock(mutex);
}

dma_jfce_t *cdma_u_create_jfce(struct dma_context *ctx)
{
	dma_jfce_t *jfce;
	int ret;

	jfce = (dma_jfce_t *)calloc(1, sizeof(*jfce));
	if (!jfce) {
		CDMA_LOG_ERR("alloc jfce failed.\n");
		return NULL;
	}

	jfce->dma_ctx = ctx;
	ret = cdma_cmd_create_jfce(ctx, jfce);
	if (ret) {
		CDMA_LOG_ERR("cmd create jfce failed, ret = %d.\n", ret);
		free(jfce);
		return NULL;
	}

	return jfce;
}

dma_status cdma_u_delete_jfce(dma_jfce_t *jfce)
{
	if (jfce == NULL || jfce->fd < 0) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return DMA_STATUS_INVAL;
	}

	(void)close(jfce->fd);
	free(jfce);

	return DMA_STATUS_OK;
}

dma_jfc_t *cdma_u_create_jfc(struct dma_context *ctx, dma_jfc_cfg_t *cfg)
{
	struct cdma_u_context *cdma_ctx;
	struct cdma_u_jfc *cdma_jfc;
	int ret;

	ret = cdma_u_check_jfc_cfg(ctx, cfg);
	if (ret)
		return NULL;

	cdma_ctx = to_cdma_u_ctx(ctx);

	cdma_jfc = (struct cdma_u_jfc *)calloc(1, sizeof(*cdma_jfc));
	if (!cdma_jfc) {
		CDMA_LOG_ERR("alloc user cdma jfc memory failed.\n");
		return NULL;
	}

	if (pthread_spin_init(&cdma_jfc->cq.lock, PTHREAD_PROCESS_PRIVATE)) {
		CDMA_LOG_ERR("init user cdma jfc spinlock failed.\n");
		goto err_init_lock;
	}

	ret = cdma_u_alloc_jfc_buf(cdma_ctx, cfg, cdma_jfc);
	if (ret) {
		CDMA_LOG_ERR("alloc jfc buf failed, ret = %d.\n", ret);
		goto err_alloc_jfc;
	}

	ret = cdma_cmd_create_jfc(ctx, cdma_jfc, cfg);
	if (ret) {
		CDMA_LOG_ERR("create jfc failed, ret = %d.\n", ret);
		goto err_create_jfc;
	}

	cdma_jfc->cq.idx = cdma_jfc->base.jfc_id;
	cdma_jfc->arm_sn = 1;

	return &cdma_jfc->base;

err_create_jfc:
	cdma_u_free_jfc_buf(cdma_ctx, cdma_jfc);
err_alloc_jfc:
	(void)pthread_spin_destroy(&cdma_jfc->cq.lock);
err_init_lock:
	free(cdma_jfc);
	return NULL;
}

dma_status cdma_u_delete_jfc(dma_jfc_t *jfc)
{
	struct cdma_u_context *cdma_ctx;
	struct cdma_u_jfc *cdma_jfc;

	if (jfc == NULL || jfc->dma_ctx == NULL) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return DMA_STATUS_INVAL;
	}

	cdma_ctx = to_cdma_u_ctx(jfc->dma_ctx);
	cdma_jfc = to_cdma_u_jfc(jfc);

	if (cdma_cmd_delete_jfc(jfc))
		CDMA_LOG_WARN("delete jfc cmd failed.\n");

	cdma_u_free_jfc_buf(cdma_ctx, cdma_jfc);

	(void)pthread_spin_destroy(&cdma_jfc->cq.lock);

	free(cdma_jfc);

	return DMA_STATUS_OK;
}

int cdma_u_poll_jfc(dma_jfc_t *jfc, uint32_t cr_cnt, struct dma_cr *cr)
{
	enum jfc_poll_state ret = JFC_OK;
	struct cdma_u_jfc *cdma_jfc;
	uint32_t npolled;

	if (jfc == NULL) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return -EINVAL;
	}

	cdma_jfc = to_cdma_u_jfc(jfc);
	(void)pthread_spin_lock(&cdma_jfc->cq.lock);

	for (npolled = 0; npolled < cr_cnt; ++npolled) {
		ret = cdma_u_poll_one(cdma_jfc, cr + npolled);
		if (ret != JFC_OK)
			break;
	}

	if (npolled)
		*cdma_jfc->sw_db = cdma_jfc->cq.ci & CDMA_JFC_DB_CI_IDX_M;

	(void)pthread_spin_unlock(&cdma_jfc->cq.lock);

	return ret == JFC_POLL_ERR ? -CDMA_INTER_ERR : (int)npolled;
}

int cdma_u_wait_jfc(dma_jfce_t *jfce, uint32_t jfc_cnt, int time_out)
{
	if (jfce == NULL || jfce->fd < 0 || jfc_cnt == 0) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return -1;
	}

	return cdma_cmd_wait_jfc(jfce->fd, jfc_cnt, time_out);
}

void cdma_u_ack_jfc(dma_jfc_t *jfc, uint32_t events)
{
	struct cdma_u_jfc *cdma_jfc;

	if (!jfc) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return;
	}

	cdma_jfc = to_cdma_u_jfc(jfc);

	cdma_jfc->arm_sn++;
	cdma_ack_comp_event(&jfc->event_mutex, &jfc->event_cond,
			    &jfc->comp_events_acked, events);
}

dma_status cdma_u_rearm_jfc(dma_jfc_t *jfc, bool solicited_only)
{
	struct cdma_u_context *cdma_ctx;
	struct cdma_u_jfc *cdma_jfc;
	struct cdma_jfc_db db;

	if (jfc == NULL || jfc->dma_ctx == NULL) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return DMA_STATUS_INVAL;
	}

	cdma_jfc = to_cdma_u_jfc(jfc);
	cdma_ctx = to_cdma_u_ctx(jfc->dma_ctx);

	db.ci = cdma_jfc->cq.ci & CDMA_JFC_DB_CI_IDX_M;
	db.notify = solicited_only;
	db.arm_sn = cdma_jfc->arm_sn;
	db.type = CDMA_CQ_ARM_DB;
	db.jfcn = cdma_jfc->cq.idx;

	cdma_u_write64((uint64_t *)((char volatile *)cdma_ctx->db.addr + CDMA_JFC_HW_DB_OFFSET),
				   (uint64_t *)&db);

	return DMA_STATUS_OK;
}

void cdma_u_clean_jfc(dma_jfc_t *jfc, uint32_t jetty_id)
{
	struct cdma_u_jetty_queue *cq;
	struct cdma_u_jfc *cdma_jfc;
	struct cdma_u_jfc_cqe *dest;
	struct cdma_u_jfc_cqe *cqe;
	uint32_t nfreed = 0;
	uint32_t local_id;
	uint8_t owner_bit;
	uint32_t pi;

	if (jfc == NULL) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return;
	}

	cdma_jfc = to_cdma_u_jfc(jfc);
	cq = &cdma_jfc->cq;
	(void)pthread_spin_lock(&cq->lock);

	for (pi = cq->ci; cdma_u_get_next_cqe(cdma_jfc, pi) != NULL; ++pi) {
		if (pi > cq->ci + cq->baseblk_cnt)
			break;
	}

	while (((int) --pi - (int)cq->ci) >= 0) {
		cqe = (struct cdma_u_jfc_cqe *)cdma_u_get_buf_entry(cq, pi);
		cdma_from_device_barrier();
		local_id = cqe->local_num_h << CDMA_SRC_IDX_SHIFT | cqe->local_num_l;
		if (local_id == jetty_id) {
			++nfreed;
		} else if (!!nfreed) {
			dest = (struct cdma_u_jfc_cqe *)cdma_u_get_buf_entry(cq, pi + nfreed);
			cdma_from_device_barrier();
			owner_bit = dest->owner;
			*dest = *cqe;
			dest->owner = owner_bit;
		}
	}

	if (!!nfreed) {
		cq->ci += nfreed;
		cdma_to_device_barrier();
		*cdma_jfc->sw_db = cq->ci & (uint32_t)CDMA_JFC_DB_CI_IDX_M;
	}

	(void)pthread_spin_unlock(&cq->lock);
}
