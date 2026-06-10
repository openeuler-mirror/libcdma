/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_CONTEXT_H__
#define __CDMA_U_CONTEXT_H__
#include "cdma_u_lib.h"

struct dma_context *cdma_create_context(struct dma_device *dma_dev);
void cdma_delete_context(struct dma_context *ctx);

#endif
