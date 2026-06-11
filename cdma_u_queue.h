/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_QUEUE_H__
#define __CDMA_U_QUEUE_H__
#include <cdma_abi.h>
#include "cdma_u_types.h"
#include "cdma_u_log.h"
#include "cdma_u_jfc.h"
#include "cdma_u_jfs.h"
#include "cdma_u_tp.h"
#include "cdma_u_common.h"

struct dma_queue *cdma_alloc_queue(struct dma_context *ctx, struct queue_cfg *cfg);
void cdma_free_queue(struct dma_queue *queue);

#endif
