/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_DEVICE_H__
#define __CDMA_U_DEVICE_H__
#include "cdma_u_lib.h"

#define CDMA_CDEV_PATH "/dev/cdma"

struct dma_device *cdma_get_device_list(uint32_t *num_devices);
void cdma_free_device_list(struct dma_device *dev_list, uint32_t num_devices);
struct dma_device *cdma_get_device_by_eid(struct dev_eid *eid);

#endif
