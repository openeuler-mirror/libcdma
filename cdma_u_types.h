/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_TYPES_H__
#define __CDMA_U_TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdatomic.h>
#include "cdma_u_lib.h"

#define DMA_EID_SIZE (16)

union dma_jfs_flag {
	struct {
		uint32_t error_suspend  : 1;  /* 0: error continue; 1: error suspend */
		uint32_t order_comp	: 1;  /* 0: not support; 1: support out-of-order completion */
		uint32_t reserved	: 30;
	} bs;
	uint32_t value;
};

typedef union dma_jfs_wr_flag {
	struct {
		uint32_t place_order : 2;	/* 0: There is no order with other WR
						 * 1: relax order
						 * 2: strong order
						 * 3: reserve */ /* see cdma_order_type_t */
		uint32_t comp_order : 1;	/* 0: There is no completion order with othwe WR.
						 * 1: Completion order with previous WR.
						 */
		uint32_t fence : 1;		/* 0: There is not fence.
						 * 1: Fence with previous read and atomic WR
						 */
		uint32_t solicited_enable : 1;	/* 0: There is not solicited.
						 * 1: solicited.
						 * It will trigger an event on remote side
						 */
		uint32_t complete_enable : 1;	/* 0: Do not notify local process after
						 * the task is complete.
						 * 1: Notify local process after
						 * the task is completed.
						 */
		uint32_t inline_flag : 1;	/* 0: not inline.
						 * 1: inline data.
						 */
		uint32_t reserved : 25;
	} bs;
	uint32_t value;
} dma_jfs_wr_flag_t;

typedef enum dma_wr_opcode {
	CDMA_WR_OPC_WRITE = 0x00,
	CDMA_WR_OPC_WRITE_NOTIFY = 0x02,
	CDMA_WR_OPC_READ = 0x10,
	CDMA_WR_OPC_CAS = 0x20,
	CDMA_WR_OPC_SWAP = 0x21,
	CDMA_WR_OPC_FADD = 0x22,
	CDMA_WR_OPC_NOP = 0x51,
	CDMA_WR_OPC_LAST
} dma_wr_opcode_t;

typedef struct dma_tp_cfg {
	uint32_t scna;
	uint32_t dcna;
	uint32_t seid;
	uint32_t deid;
	uint32_t queue_id;
} dma_tp_cfg_t;

typedef struct dma_tp {
	struct dma_context *dma_ctx;
	dma_tp_cfg_t cfg;
	uint32_t tpn;
	uint64_t handle;
} dma_tp_t;

typedef struct dma_sge {
	uint64_t addr;
	uint32_t len;
	struct dma_seg seg;
} dma_sge_t;

typedef struct dma_sg {
	dma_sge_t *sge;
	uint32_t num_sge;
} dma_sg_t;

typedef struct dma_rw_wr {
	dma_sg_t src;
	dma_sg_t dst;
	uint8_t target_hint;
	uint64_t notify_data;
	uint64_t notify_addr;
	uint32_t notify_tokenid;
	uint32_t notify_tokenvalue;
} dma_rw_wr_t;

typedef struct dma_cas_wr {
	dma_sge_t *dst;
	dma_sge_t *src;
	union {
		uint64_t cmp_data;
		uint64_t cmp_addr;
	};
	union {
		uint64_t swap_data;
		uint64_t swap_addr;
	};
} dma_cas_wr_t;

typedef struct dma_faa_wr {
	dma_sge_t *dst;
	dma_sge_t *src;
	union {
		uint64_t operand;
		uint64_t operand_addr;
	};
} dma_faa_wr_t;

typedef struct dma_jfs_wr {
	dma_wr_opcode_t opcode;
	dma_jfs_wr_flag_t flag;
	uint64_t user_ctx;
	uint32_t tpn;
	uint32_t rmt_eid;
	union {
		dma_rw_wr_t rw;
		dma_cas_wr_t cas;
		dma_faa_wr_t faa;
	};
	struct dma_jfs_wr *next;
} dma_jfs_wr_t;

typedef struct dma_jfce {
	struct dma_context	*dma_ctx;
	int			fd;
	int			id;
} dma_jfce_t;

typedef union dma_jfc_flag {
	struct {
		uint32_t    lock_free : 1;
		uint32_t    jfc_inline : 1;
		uint32_t    reserved : 30;
	} bs;
	uint32_t value;
} dma_jfc_flag_t;

typedef struct dma_jfc_cfg {
	uint32_t depth;
	dma_jfc_flag_t flag;
	uint32_t ceqn;
	dma_jfce_t *jfce;
	uint64_t user_ctx;
	uint32_t queue_id;
} dma_jfc_cfg_t;

typedef struct dma_jfc {
	struct dma_context	*dma_ctx;
	uint32_t		jfc_id;
	dma_jfc_cfg_t		jfc_cfg;
	uint64_t		handle;
	pthread_mutex_t		event_mutex;
	pthread_cond_t		event_cond;
	uint32_t		comp_events_acked;
	uint32_t		async_events_acked;
} dma_jfc_t;

typedef struct dma_jfs_cfg {
	uint32_t depth;
	union dma_jfs_flag flag;
	uint8_t priority;
	uint8_t max_sge;
	uint8_t max_rsge;
	uint8_t rnr_retry;
	uint8_t err_timeout;
	struct dma_jfc *jfc;
	uint32_t queue_id;
	uint32_t rmt_eid;
	uint32_t pld_token_id;
	uint32_t tpn;
	uint32_t eid_idx;
	uint32_t trans_mode;
} dma_jfs_cfg_t;

typedef struct dma_jfs {
	struct dma_context *dma_ctx;
	uint32_t jfs_id;
	struct dma_jfs_cfg jfs_cfg;
	uint64_t handle;
} dma_jfs_t;

struct dma_queue_cfg {
	struct dma_jfc_cfg cdma_jfc_cfg;
	struct dma_jfs_cfg cdma_jfs_cfg;
	struct dma_tp_cfg cdma_tp_cfg;
};

struct cdma_u_queue {
	struct dma_queue base;
	struct dma_jfs *cdma_jfs;
	struct dma_jfc *cdma_jfc;
	struct dma_tp *cdma_tp;
	struct dma_jfce *cdma_jfce;
	struct dma_queue_cfg queue_cfg;
	uint64_t handle;
};

#endif
