/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_SEGMENT_H__
#define __CDMA_U_SEGMENT_H__
#include "cdma_u_lib.h"

struct dma_seg *cdma_register_seg(struct dma_context *ctx, struct dma_seg_cfg *cfg);
void cdma_unregister_seg(struct dma_context *ctx, struct dma_seg *seg);
struct dma_seg *cdma_import_seg(struct dma_seg_cfg *cfg);
void cdma_unimport_seg(struct dma_seg *seg);

#endif
