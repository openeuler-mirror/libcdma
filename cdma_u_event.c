// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include "cdma_u_log.h"
#include "cdma_u_common.h"
#include "cdma_u_jfc.h"
#include "cdma_u_event.h"

static int cdma_cmd_get_async_event(struct cdma_u_context *u_ctx,
				    struct dma_aeqe *arg)
{
	struct cdma_cmd_async_event ret_aeqe;
	struct pollfd fds = { 0 };
	int ret;

	if (arg->duration < 0 || arg->duration > EVENT_WAIT_TIME) {
		CDMA_LOG_ERR("get async event arg error.\n");
		return -EINVAL;
	}

	fds.fd = u_ctx->async_fd;
	fds.events = POLLIN;

	ret = poll(&fds, EVENT_EVENT_NUM, arg->duration);
	if (ret <= 0) {
		CDMA_LOG_DEBUG("poll wait event, ret=%d.\n", ret);
		return ret;
	} else if (fds.revents == POLLIN) {
		ret = ioctl(u_ctx->async_fd, CDMA_CMD_GET_ASYNC_EVENT, &ret_aeqe);
		if (ret != 0) {
			CDMA_LOG_ERR("get async event ioctl, ret = %d %d.\n", ret, errno);
			return ret;
		}

		switch (ret_aeqe.event_type) {
		case DMA_AEQE_TYPE_JFS:
			arg->queue_id = ret_aeqe.event_data;
			break;
		case DMA_AEQE_TYPE_JFC:
			arg->queue_id = ret_aeqe.event_data;
			break;
		case DMA_AEQE_TYPE_INVALID:
			arg->queue_id = 0;
			break;
		default:
			CDMA_LOG_ERR("invalid ae event type.\n");
			return -EFAULT;
		}

		CDMA_LOG_INFO("get async event successfully, ae type: %u.\n",
			      ret_aeqe.event_type);
		arg->ctx = &u_ctx->dma_ctx;
		arg->event_type = ret_aeqe.event_type;
	} else if (fds.revents == POLLERR) {
		CDMA_LOG_ERR("poll wait event POLLERR.\n");
		return -EFAULT;
	}

	return EVENT_EVENT_NUM; // return event num;
}

int cdma_poll_queue(struct dma_queue *queue, uint32_t cr_cnt, struct dma_cr *cr)
{
	struct cdma_u_queue *cdma_queue = to_cdma_u_queue(queue);
	dma_tp_cfg_t *cfg = &cdma_queue->cdma_tp->cfg;
	int npolled;

	npolled = cdma_u_poll_jfc(cdma_queue->cdma_jfc, cr_cnt, cr);
	if (npolled > 0 && cr->status != DMA_CR_SUCCESS) {
		CDMA_LOG_ERR("poll jfc npolled = %d, seid = %u, deid = %u\n",
			npolled, cfg->seid, cfg->deid);
	}

	return npolled;
}

int cdma_wait_queue(struct dma_queue *queue, uint32_t cr_cnt, int timeout,
		    struct dma_cr *cr)
{
	struct cdma_u_queue *cdma_queue = to_cdma_u_queue(queue);
	int wait_cnt;
	int poll_cnt;

	wait_cnt = cdma_u_wait_jfc(cdma_queue->cdma_jfce, cr_cnt, timeout);
	if (wait_cnt < 0) {
		CDMA_LOG_ERR("wait jfc failed, ret = %d.\n", wait_cnt);
		return -EFAULT;
	}

	if (wait_cnt == 0)
		CDMA_LOG_WARN("wait jfc timeout.\n");

	poll_cnt = cdma_u_poll_jfc(cdma_queue->cdma_jfc, cr_cnt, cr);
	if (poll_cnt <= 0) {
		CDMA_LOG_ERR("poll jfc failed, ret = %d.\n", poll_cnt);
		return -EFAULT;
	}

	cdma_u_ack_jfc(cdma_queue->cdma_jfc, poll_cnt);
	if (cdma_u_rearm_jfc(cdma_queue->cdma_jfc, false) != DMA_STATUS_OK) {
		CDMA_LOG_ERR("rearm jfc failed.\n");
		return -EFAULT;
	}

	return poll_cnt;
}

int cdma_wait_ae(struct dma_context *ctx, struct dma_aeqe *arg)
{
	struct cdma_u_context *u_ctx = to_cdma_u_ctx(ctx);
	int ret;

	ret = cdma_cmd_get_async_event(u_ctx, arg);
	if (ret < 0)
		CDMA_LOG_ERR("cmd get async event failed, ret = %d.\n", ret);

	return ret;
}
