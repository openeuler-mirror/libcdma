// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include "cdma_u_log.h"
#include "cdma_u_db.h"

static uint64_t *cdma_bitmap_alloc(uint32_t n_bits, uint32_t *bitmap_cnt)
{
	uint64_t *bitmap;
	uint32_t i;

	*bitmap_cnt = align(n_bits, CDMA_BITS_PER_LONG) / CDMA_BITS_PER_LONG;
	bitmap = (uint64_t *)calloc(*bitmap_cnt, sizeof(uint64_t));
	if (!bitmap) {
		CDMA_LOG_ERR("calloc bitmap failed!\n");
		return NULL;
	}

	for (i = 0; i < *bitmap_cnt; ++i)
		bitmap[i] = ~(0ULL);

	return bitmap;
}

static inline void cdma_bitmap_free(uint64_t *bitmap)
{
	free(bitmap);
}

static struct cdma_u_db_page *cdma_add_db_page(struct cdma_u_context *ctx)
{
	struct cdma_u_db_page *db_page;
	uint32_t page_size;

	page_size = ctx->page_size;
	db_page = (struct cdma_u_db_page *)calloc(1, sizeof(*db_page));
	if (!db_page) {
		CDMA_LOG_ERR("calloc sw db db_page failed!\n");
		return NULL;
	}

	db_page->num_db = page_size / CDMA_DB_SIZE;
	db_page->use_cnt = 0;
	db_page->bitmap = cdma_bitmap_alloc(db_page->num_db, &db_page->bitmap_cnt);
	if (!db_page->bitmap)
		goto err_map;

	db_page->buf.length = page_size;
	db_page->buf.buf = cdma_u_alloc_buf(db_page->buf.length);
	if (!db_page->buf.buf)
		goto err_buf;

	db_page->prev = NULL;
	db_page->next = ctx->page;
	ctx->page = db_page;
	if (db_page->next)
		db_page->next->prev = db_page;

	return db_page;

err_buf:
	cdma_bitmap_free(db_page->bitmap);
err_map:
	free(db_page);

	return NULL;
}

static void cdma_clear_db_page(struct cdma_u_db_page *db_page)
{
	if (db_page->buf.buf)
		cdma_u_free_buf(db_page->buf.buf, db_page->buf.length);

	cdma_bitmap_free(db_page->bitmap);
	free(db_page);
}

static int cdma_bitmap_use_idx(uint64_t *bitmap, uint32_t bitmap_cnt,
			       uint32_t n_bits, uint32_t *idx)
{
	uint32_t bit_num;
	uint32_t i;

	for (i = 0; i < bitmap_cnt && bitmap[i] == 0; ++i)
		;
	if (i == bitmap_cnt) {
		CDMA_LOG_ERR("no available bitmap! bitmap_cnt = %u\n", bitmap_cnt);
		return -ENOMEM;
	}

	bit_num = ffsl(bitmap[i]);
	*idx = (i << CDMA_BITS_PER_LONG_SHIFT) + bit_num - 1;

	if (*idx >= n_bits) {
		CDMA_LOG_ERR("idx is larger than n_bits!.\n");
		return -ENOMEM;
	}

	bitmap[i] &= ~(1ULL << (bit_num - 1));

	return 0;
}

static void cdma_bitmap_free_idx(uint64_t *bitmap, uint32_t bitmap_cnt,
				 uint32_t idx)
{
	uint32_t bitmap_num;
	uint32_t bit_num;

	bitmap_num = idx >> CDMA_BITS_PER_LONG_SHIFT;
	if (bitmap_num >= bitmap_cnt)
		return;

	bit_num = idx % CDMA_BITS_PER_LONG;
	bitmap[bitmap_num] |= (1ULL << bit_num);
}

void *cdma_u_alloc_sw_db(struct cdma_u_context *ctx)
{
	struct cdma_u_db_page *db_page;
	uint32_t npos = 0;
	void *db = NULL;

	pthread_mutex_lock(&ctx->page_mutex);
	for (db_page = ctx->page; db_page != NULL; db_page = db_page->next) {
		if (db_page->use_cnt < db_page->num_db)
			goto found;
	}

	db_page = cdma_add_db_page(ctx);
	if (!db_page)
		goto out;

found:
	(void)cdma_bitmap_use_idx(db_page->bitmap, db_page->bitmap_cnt,
				  db_page->num_db, &npos);
	db = (char *)db_page->buf.buf + npos * CDMA_DB_SIZE;
	++db_page->use_cnt;
out:
	pthread_mutex_unlock(&ctx->page_mutex);

	return db;
}

void cdma_u_free_sw_db(struct cdma_u_context *ctx, uint32_t *db)
{
	struct cdma_u_db_page *db_page;
	uint32_t page_size;
	uint32_t npos;

	pthread_mutex_lock(&ctx->page_mutex);
	page_size = ctx->page_size;
	for (db_page = ctx->page; db_page != NULL; db_page = db_page->next) {
		if (((uintptr_t)db & (~((uintptr_t)page_size - 1))) ==
			(uintptr_t)(db_page->buf.buf))
			goto found;
	}

	goto out;

found:
	--db_page->use_cnt;
	if (!db_page->use_cnt) {
		if (db_page->prev)
			db_page->prev->next = db_page->next;
		else
			ctx->page = db_page->next;

		if (db_page->next)
			db_page->next->prev = db_page->prev;

		cdma_clear_db_page(db_page);
		goto out;
	}

	npos = ((uintptr_t)db - (uintptr_t)db_page->buf.buf) / CDMA_DB_SIZE;
	cdma_bitmap_free_idx(db_page->bitmap, db_page->bitmap_cnt, npos);

out:
	pthread_mutex_unlock(&ctx->page_mutex);
}

static uint32_t cdma_get_dsqe_db_offset(struct cdma_u_context *cdma_ctx,
					struct cdma_u_doorbell *db)
{
	if (db->type != CDMA_MMAP_JETTY_DSQE)
		return 0;

	return (db->id + 1) % (cdma_ctx->page_size / CDMA_HW_PAGE_SIZE) *
	       CDMA_HW_PAGE_SIZE;
}

int cdma_u_alloc_db(struct dma_context *ctx, struct cdma_u_doorbell *db)
{
	struct cdma_u_context *cdma_ctx = to_cdma_u_ctx(ctx);
	off_t offset;

	offset = get_mmap_offset(db->id, cdma_ctx->page_size, db->type);
	db->addr = mmap(NULL, cdma_ctx->page_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, ctx->dma_dev->fd, offset);
	if (db->addr == MAP_FAILED) {
		CDMA_LOG_ERR("mmap doorbell page failed, id = %u, type = %u.\n",
			      db->id, db->type);
		return -EINVAL;
	}

	db->addr += cdma_get_dsqe_db_offset(cdma_ctx, db);

	return 0;
}

void cdma_u_free_db(struct dma_context *ctx, struct cdma_u_doorbell *db)
{
	struct cdma_u_context *cdma_ctx;

	if (db->addr == MAP_FAILED || db->addr == NULL) {
		CDMA_LOG_ERR("invalid parameter.\n");
		return;
	}

	cdma_ctx = to_cdma_u_ctx(ctx);

	db->addr -= cdma_get_dsqe_db_offset(cdma_ctx, db);
	munmap((void *)db->addr, (size_t)cdma_ctx->page_size);
	db->addr = NULL;
}
