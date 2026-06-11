/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_EVENT_H__
#define __CDMA_U_EVENT_H__
#include "cdma_u_lib.h"

#define EVENT_WAIT_TIME 10000
#define EVENT_EVENT_NUM 1

int cdma_poll_queue(struct dma_queue *queue, uint32_t cr_cnt, struct dma_cr *cr);

int cdma_wait_queue(struct dma_queue *queue, uint32_t cr_cnt, int timeout,
		    struct dma_cr *cr);

int cdma_wait_ae(struct dma_context *ctx, struct dma_aeqe *aeqe);

#endif
