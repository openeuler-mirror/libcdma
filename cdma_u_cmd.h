/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_CMD_H__
#define __CDMA_U_CMD_H__
#include <stdio.h>
#include "cdma_u_lib.h"
#include "cdma_u_types.h"

static inline void cdma_u_set_udata(struct cdma_cmd_udrv_priv *udrv_data,
				    void *in_addr, uint32_t in_len,
				    void *out_addr, uint32_t out_len)
{
	udrv_data->in_addr = (uint64_t)in_addr;
	udrv_data->in_len = in_len;
	udrv_data->out_addr = (uint64_t)out_addr;
	udrv_data->out_len = out_len;
}

dma_status cdma_write(struct dma_queue *queue, struct dma_seg *rmt_seg,
		      struct dma_seg *local_seg, struct dma_seg *notify_seg,
		      uint64_t notify_data);
dma_status cdma_read(struct dma_queue *queue, struct dma_seg *rmt_seg,
		     struct dma_seg *local_seg);
dma_status cdma_cas(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg, uint64_t cmp, uint64_t swap);
dma_status cdma_faa(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg, uint64_t operand);
int cdma_cmd_create_jfce(struct dma_context *ctx, dma_jfce_t *jfce);
int cdma_cmd_create_ctp(struct dma_context *ctx, struct dma_tp *ctp,
			struct dma_tp_cfg *cfg);
int cdma_cmd_delete_ctp(struct dma_tp *ctp);
int cdma_u_cmd_create_jfs(struct dma_context *ctx, struct dma_jfs *jfs,
			  struct dma_jfs_cfg *cfg,
			  struct cdma_cmd_udrv_priv *udata);
int cdma_u_cmd_delete_jfs(struct dma_jfs *jfs);

#endif
