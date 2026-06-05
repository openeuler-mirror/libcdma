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
#include <sys/ioctl.h>
#include "cdma_u_common.h"
#include "cdma_u_log.h"
#include "cdma_u_db.h"
#include "cdma_u_context.h"
#include <cdma_abi.h>
#include "ummu_api.h"

static void cdma_u_init_context(struct cdma_u_context *u_ctx,
				struct cdma_create_context_args *info)
{
#define CDMA_JFC_DB_OFFSET 0
	u_ctx->page_size = sysconf(_SC_PAGESIZE);
	u_ctx->db.id = CDMA_JFC_DB_OFFSET;
	u_ctx->db.type = CDMA_MMAP_JFC_PAGE;
	u_ctx->cqe_size = info->out.cqe_size;
	u_ctx->dwqe_enable = info->out.dwqe_enable;
	u_ctx->async_fd = info->out.async_fd;
	u_ctx->dma_ctx.async_fd = info->out.async_fd;
}

static int cdma_create_kernel_context(struct cdma_u_context *u_ctx)
{
	struct cdma_create_context_args in_info = { 0 };
	struct cdma_ioctl_hdr hdr;
	int ret;

	hdr.command = (uint32_t)CDMA_CMD_CREATE_CTX;
	hdr.args_len = (uint32_t)sizeof(in_info);
	hdr.args_addr = (uint64_t)&in_info;

	ret = ioctl(u_ctx->dma_ctx.dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl cdma create kernel ctx, ret = %d %d, cmd = %u.\n",
			     ret, errno, hdr.command);
		return ret;
	}

	cdma_u_init_context(u_ctx, &in_info);

	return 0;
}

static void cdma_delete_kernel_context(struct cdma_u_context *u_ctx)
{
	struct cdma_ioctl_hdr hdr;
	int ret;

	// Kernelspace creates jfae, userspace releases
	if (u_ctx->async_fd >= 0) {
		(void)close(u_ctx->async_fd);
		u_ctx->async_fd = -1;
	}

	hdr.command = (uint32_t)CDMA_CMD_DELETE_CTX;
	hdr.args_len = 0;
	hdr.args_addr = 0;

	ret = ioctl(u_ctx->dma_ctx.dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret != 0) {
		CDMA_LOG_ERR("ioctl cdma delete kernel ctx, ret = %d %d, cmd = %u.\n",
			     ret, errno, hdr.command);
		return;
	}
}

struct dma_context *cdma_create_context(struct dma_device *dma_dev)
{
	struct ummu_tid_attr tid_attr = {.mode = MAPT_MODE_TABLE};
	struct cdma_u_context *u_ctx;
	struct dma_context *ctx;
	int ret;

	u_ctx = (struct cdma_u_context *)calloc(1, sizeof(*u_ctx));
	if (!u_ctx)
		return NULL;
	ctx = &u_ctx->dma_ctx;
	ctx->dma_dev = dma_dev;

	if (pthread_mutex_init(&u_ctx->page_mutex, NULL)) {
		CDMA_LOG_ERR("init page_mutex failed.\n");
		goto free_u_ctx;
	}

	ret = ummu_allocate_tid(&tid_attr, &ctx->tid);
	if (ret) {
		CDMA_LOG_ERR("ummu_allocate_tid failed, ret = %d.\n", ret);
		goto mutex_destroy;
	}

	ret = cdma_create_kernel_context(u_ctx);
	if (ret) {
		CDMA_LOG_ERR("cdma_create_kernel_context failed, ret = %d.\n", ret);
		goto free_tid;
	}

	if (cdma_u_alloc_db(&u_ctx->dma_ctx, &u_ctx->db)) {
		CDMA_LOG_ERR("alloc jfc db failed.\n");
		goto del_kernel_ctx;
	}

	return ctx;

del_kernel_ctx:
	cdma_delete_kernel_context(u_ctx);
free_tid:
	(void)ummu_free_tid(ctx->tid);
mutex_destroy:
	pthread_mutex_destroy(&u_ctx->page_mutex);
free_u_ctx:
	free(u_ctx);

	return NULL;
}

void cdma_delete_context(struct dma_context *ctx)
{
	struct cdma_u_context *u_ctx = to_cdma_u_ctx(ctx);

	cdma_u_free_db(ctx, &u_ctx->db);
	cdma_delete_kernel_context(u_ctx);
	(void)ummu_free_tid(ctx->tid);
	pthread_mutex_destroy(&u_ctx->page_mutex);
	free(u_ctx);
}
