/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_COMMON_H__
#define __CDMA_U_COMMON_H__

#include <unistd.h>
#include <stdatomic.h>
#include <arm_neon.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include "cdma_u_types.h"

#define CDMA_JFS_WQEBB 64

#define CDMA_HW_PAGE_SHIFT 12
#define CDMA_HW_PAGE_SIZE (1 << CDMA_HW_PAGE_SHIFT)
#define CDMA_BITS_PER_LONG 64
#define CDMA_BITS_PER_LONG_SHIFT 6
#define CDMA_SGE_SIZE 16
#define CDMA_DB_SIZE 64
#define CDMA_NORMAL_JETTY_TYPE ((int)(~0U >> 1))
#define CDMA_JFC_DB_OFFSET 0

#define MAP_INDEX_MASK 0xffffff
#define CDMA_GET_AE_FAIL 0x1000
#define CDMA_GET_AE_SUCCESS 0

#define likely(x)	 (__builtin_expect(!!(x), 1))
#define unlikely(x)   (__builtin_expect(!!(x), 0))

#if INT_MAX >= 9223372036854775807LL
#define builtin_ilog64_nz(v) \
	(((int)sizeof(uint32_t) * CHAR_BIT) - __builtin_clz(v))
#elif LONG_MAX >= 9223372036854775807LL
#define builtin_ilog64_nz(v) \
	(((int)sizeof(uint64_t) * CHAR_BIT) - __builtin_clzl(v))
#endif

#define ilog64(_v) ((uint64_t)builtin_ilog64_nz(_v)&((_v) == 0ULL ? 0ULL : 0xFFFFFFFFFFFFFFFFULL))

#define check_types_match(expr1, expr2)		\
	((typeof(expr1) *)0 != (typeof(expr2) *)0)

#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (CDMA_BITS_PER_LONG - 1 - (h))))

#ifndef container_of
#define container_off(containing_type, member)\
	offsetof(containing_type, member)
#define container_of(member_ptr, containing_type, member)	\
	((containing_type *)					\
	((char *)(member_ptr)					\
	- container_off(containing_type, member))		\
	+ (uint8_t)check_types_match(*(member_ptr), ((containing_type *)0)->member))
#endif

#define cdma_to_device_barrier() \
	do { \
		asm volatile("dsb ld" ::: "memory"); \
	} while (0)
#define cdma_from_device_barrier() \
	do { \
		asm volatile("dsb ld" ::: "memory"); \
	} while (0)

struct cdma_u_doorbell {
	uint32_t id;
	enum db_mmap_type type;
	void volatile *addr;
};

struct cdma_u_jetty_queue {
	/* Command queue */
	void *qbuf; /* Base virtual address of command buffer */
	uint32_t qbuf_size; /* Command buffer size */
	void *qbuf_curr; /* Virtual address to store the current command */
	uint32_t pi;
	uint32_t ci;
	uint32_t idx;
	struct cdma_u_doorbell db;
	/* Wqe or cqe base block */
	uint32_t baseblk_shift;
	uint32_t baseblk_cnt;
	uint32_t baseblk_mask;
	uintptr_t *wrid;
	pthread_spinlock_t lock;
	uint32_t max_inline_size;
	void volatile *dwqe_addr;

	uint32_t sqe_bb_cnt;
	uint32_t max_sge_num;
	bool flush_flag;
	bool lock_free;
	uint32_t cstm; /* sq ctrl flag */
};

struct cdma_u_buf {
	void *buf;
	uint32_t length;
};

struct cdma_u_db_page {
	struct cdma_u_db_page *prev, *next;
	struct cdma_u_buf buf;
	uint32_t num_db;
	uint32_t use_cnt;
	uintptr_t *bitmap;
	uint32_t bitmap_cnt;
};

struct cdma_u_context {
	struct dma_context dma_ctx;
	void *db_addr;
	uint32_t page_size;
	struct cdma_u_db_page *page;
	pthread_mutex_t page_mutex;
	struct cdma_u_doorbell db;
	uint8_t cqe_size;
	uint8_t dwqe_enable;
	int async_fd;
};

struct cdma_u_jfs {
	struct dma_jfs base;
	struct cdma_u_jetty_queue sq;
	uint32_t jfs_type;
};

struct cdma_u_jfc {
	dma_jfc_t			base;
	struct cdma_u_jetty_queue	cq;
	uint32_t			arm_sn;
	uint32_t			*sw_db;
	uint32_t			mode;
};

struct cdma_wqe_sge {
	uint32_t length;
	uint32_t token_id;
	uint64_t va;
};

static inline uint64_t roundup_pow_of_two(uint64_t n)
{
	return n == 1ULL ? 1ULL : 1ULL << ilog64(n - 1ULL);
}

static inline unsigned long align(unsigned long val, unsigned long align)
{
	return (val + align - 1UL) & ~(align - 1UL);
}

static inline void cdma_mmap_set_command(uint32_t command, off_t *offset)
{
	uint32_t offset_u = (uint32_t)*offset;

	offset_u |= (command & (uint32_t)MAP_COMMAND_MASK);
	*offset = (off_t)offset_u;
}

static inline uint32_t align_power2(uint32_t n)
{
	uint32_t res = 0;

	while ((1U << res) < n)
		res++;

	return res;
}

static inline void cdma_mmap_set_index(unsigned long index, off_t *offset)
{
	unsigned long offset_u = (unsigned long)*offset;

	offset_u |= ((index & (unsigned long)MAP_INDEX_MASK) << MAP_INDEX_SHIFT);
	*offset = (off_t)offset_u;
}

static inline off_t get_mmap_offset(uint32_t idx, int page_size, uint32_t cmd)
{
	off_t offset = 0;

	cdma_mmap_set_command(cmd, &offset);
	cdma_mmap_set_index(idx, &offset);

	return offset * page_size;
}

static inline void *cdma_update_ptr(uint8_t *ptr, uint32_t inc, uint8_t *qbuf,
				    uint8_t *qbuf_end)
{
	return ((ptr + inc) < qbuf_end) ?
		(ptr + inc) : (qbuf + (ptr + inc - qbuf_end));
}

static inline void mmio_memcpy_x64(uint64_t *dest, uint64_t *val)
{
	vst4q_u64(dest, vld4q_u64(val));
}

static inline struct cdma_u_context *to_cdma_u_ctx(struct dma_context *ctx)
{
	return container_of(ctx, struct cdma_u_context, dma_ctx);
}

static inline void cdma_u_write64(uint64_t *dest, uint64_t *val)
{
	atomic_store_explicit((_Atomic(uint64_t) *)(void *)dest,
			      (uint64_t)(*val), memory_order_relaxed);
}

static inline struct cdma_u_jfc *to_cdma_u_jfc(dma_jfc_t *jfc)
{
	return container_of(jfc, struct cdma_u_jfc, base);
}

static inline struct cdma_u_jfs *to_cdma_u_jfs(struct dma_jfs *jfs)
{
	return container_of(jfs, struct cdma_u_jfs, base);
}

static inline void cdma_u_free_buf(void *buf, uint32_t buf_size)
{
	(void)munmap(buf, buf_size);
}

static inline struct cdma_u_queue *to_cdma_u_queue(struct dma_queue *queue)
{
	return container_of(queue, struct cdma_u_queue, base);
}

void *cdma_u_alloc_buf(uint32_t buf_size);

int cdma_u_alloc_queue_buf(struct cdma_u_jetty_queue *q, uint32_t max_cnt,
			   uint32_t entry_size, uint32_t page_size,
			   bool wrid_en);

void cdma_u_free_queue_buf(struct cdma_u_jetty_queue *q);

#endif
