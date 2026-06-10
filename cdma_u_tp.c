// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include "cdma_u_cmd.h"
#include "cdma_u_log.h"
#include "cdma_u_tp.h"

struct dma_tp *cdma_u_create_ctp(struct dma_context *ctx,
				 struct dma_tp_cfg *cfg)
{
	struct dma_tp *tp;

	if (!ctx || !cfg) {
		CDMA_LOG_ERR("create ctp parameter invalid\n");
		return NULL;
	}

	tp = (struct dma_tp *)calloc(1, sizeof(*tp));
	if (!tp) {
		CDMA_LOG_ERR("alloc ctp memory failed\n");
		return NULL;
	}

	if (cdma_cmd_create_ctp(ctx, tp, cfg))
		goto err_exec_cmd;

	return tp;

err_exec_cmd:
	free(tp);

	return NULL;
}

int cdma_u_delete_ctp(struct dma_tp *tp)
{
	int ret;

	if (!tp || !tp->dma_ctx) {
		CDMA_LOG_ERR("cdma delete tp parameter invalid\n");
		return -EINVAL;
	}

	ret = cdma_cmd_delete_ctp(tp);
	if (ret)
		CDMA_LOG_WARN("cdma delete tp cmd failed, ret = %d\n", ret);

	free(tp);

	return ret;
}
