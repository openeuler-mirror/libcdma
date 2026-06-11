// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include "cdma_u_log.h"
#include "cdma_u_common.h"

void *cdma_u_alloc_buf(uint32_t buf_size)
{
	void *buf;
	int ret;

	buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		CDMA_LOG_ERR("mmap buf failed\n");
		return NULL;
	}

	ret = madvise(buf, buf_size, MADV_DONTFORK);
	if (ret) {
		(void)munmap(buf, buf_size);
		CDMA_LOG_ERR("buf madvise failed, ret = %d\n", ret);
		return NULL;
	}

	return buf;
}

int cdma_u_alloc_queue_buf(struct cdma_u_jetty_queue *q, uint32_t max_cnt,
			   uint32_t entry_size, uint32_t page_size,
			   bool wrid_en)
{
	uint32_t buf_shift;
	uint32_t entry_cnt;

	entry_cnt = roundup_pow_of_two(max_cnt);
	buf_shift = align_power2(roundup_pow_of_two(entry_cnt * entry_size));
	q->baseblk_shift = align_power2(roundup_pow_of_two(entry_size));
	q->qbuf_size = align((1U << buf_shift), page_size);
	q->baseblk_cnt = q->qbuf_size >> q->baseblk_shift;
	q->baseblk_mask = q->baseblk_cnt - 1;

	if (wrid_en) {
		q->wrid = (uintptr_t *)malloc(q->baseblk_cnt * sizeof(uint64_t));
		if (!q->wrid) {
			CDMA_LOG_ERR("alloc buffer for wrid failed\n");
			return -ENOMEM;
		}
	}

	q->qbuf = cdma_u_alloc_buf(q->qbuf_size);
	if (!q->qbuf) {
		CDMA_LOG_ERR("alloc queue buffer failed\n");
		goto err_alloc_buf;
	}

	q->qbuf_curr = q->qbuf;

	return 0;

err_alloc_buf:
	if (wrid_en) {
		free(q->wrid);
		q->wrid = NULL;
	}
	return -ENOMEM;
}

void cdma_u_free_queue_buf(struct cdma_u_jetty_queue *q)
{
	if (q->qbuf) {
		cdma_u_free_buf(q->qbuf, q->qbuf_size);
		q->qbuf = NULL;
	}

	if (q->wrid) {
		free(q->wrid);
		q->wrid = NULL;
	}
}
