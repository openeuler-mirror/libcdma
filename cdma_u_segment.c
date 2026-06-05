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
#include "cdma_u_segment.h"
#include <cdma_abi.h>
#include "ummu_api.h"

static inline void cdma_fill_seg(struct dma_seg *seg, struct dma_seg_cfg *cfg)
{
	seg->len = cfg->len;
	seg->sva = cfg->addr;
	seg->token_value = cfg->token_value;
	seg->token_value_valid = cfg->token_value_valid;
}

static inline void cdma_fill_seg_attr(struct ummu_token_info *token_info,
				      struct ummu_seg_attr *seg_attr,
				      struct dma_seg_cfg *cfg)
{
#define CDMA_TOKEN_VALUE_INPUT 0
	token_info->input = CDMA_TOKEN_VALUE_INPUT;
	token_info->tokenVal = cfg->token_value;
	seg_attr->token = token_info;
	seg_attr->e_bit = UMMU_EBIT_OFF;
}

static int cdma_register_kernel_seg(struct dma_context *ctx,
				    struct dma_seg_cfg *cfg,
				    struct dma_seg *seg)
{
	struct cdma_cmd_register_seg_args in_info;
	struct cdma_ioctl_hdr hdr;
	int ret;

	in_info.in.addr = cfg->addr;
	in_info.in.len = cfg->len;

	hdr.command = (uint32_t)CDMA_CMD_REGISTER_SEG;
	hdr.args_len = sizeof(in_info);
	hdr.args_addr = (uint64_t)(&in_info);

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret) {
		CDMA_LOG_ERR("ioctl cdma register seg, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
		return ret;
	}

	seg->handle = in_info.out.handle;

	return 0;
}

struct dma_seg *cdma_register_seg(struct dma_context *ctx,
				  struct dma_seg_cfg *cfg)
{
	struct ummu_token_info token_info;
	struct ummu_seg_attr seg_attr;
	struct dma_seg *seg;
	int ret;

	seg = (struct dma_seg *)calloc(1, sizeof(*seg));
	if (!seg) {
		CDMA_LOG_ERR("cdma alloc seg failed.\n");
		return NULL;
	}

	cdma_fill_seg(seg, cfg);
	seg->tid = ctx->tid;

	cdma_fill_seg_attr(&token_info, &seg_attr, cfg);
	ret = ummu_grant(seg->tid, (void *)seg->sva, seg->len, MAPT_PERM_RW,
					 &seg_attr);
	if (ret) {
		CDMA_LOG_ERR("cdma grant seg failed, ret = %d.\n", ret);
		goto free_seg;
	}

	ret = cdma_register_kernel_seg(ctx, cfg, seg);
	if (ret)
		goto ungrant;

	return seg;

ungrant:
	(void)ummu_ungrant(seg->tid, (void *)seg->sva, seg->len);
free_seg:
	free(seg);

	return NULL;
}

static void cdma_unregister_kernel_seg(struct dma_context *ctx,
				       struct dma_seg *seg)
{
	struct cdma_cmd_unregister_seg_args in_info;
	struct cdma_ioctl_hdr hdr;
	int ret;

	in_info.in.handle = seg->handle;

	hdr.command = (uint32_t)CDMA_CMD_UNREGISTER_SEG;
	hdr.args_len = sizeof(struct cdma_cmd_unregister_seg_args);
	hdr.args_addr = (uint64_t)(&in_info);

	ret = ioctl(ctx->dma_dev->fd, CDMA_SYNC, &hdr);
	if (ret)
		CDMA_LOG_ERR("ioctl cdma unregister seg, ret = %d %d, cmd = %u.\n", ret,
			     errno, hdr.command);
}

void cdma_unregister_seg(struct dma_context *ctx, struct dma_seg *seg)
{
	int ret;

	cdma_unregister_kernel_seg(ctx, seg);

	ret = ummu_ungrant(seg->tid, (void *)seg->sva, seg->len);
	if (ret)
		CDMA_LOG_ERR("cdma ungrant seg failed, ret = %d.\n", ret);

	free(seg);
}

struct dma_seg *cdma_import_seg(struct dma_seg_cfg *cfg)
{
	struct dma_seg *seg;

	seg = (struct dma_seg *)calloc(1, sizeof(*seg));
	if (!seg) {
		CDMA_LOG_ERR("cdma alloc seg when import failed.\n");
		return NULL;
	}

	cdma_fill_seg(seg, cfg);
	seg->tid = cfg->tid;

	return seg;
}

void cdma_unimport_seg(struct dma_seg *seg)
{
	free(seg);
}
