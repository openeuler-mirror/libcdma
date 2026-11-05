/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_JFS_H__
#define __CDMA_U_JFS_H__

#include "cdma_u_lib.h"
#include "cdma_u_common.h"

#define CDMA_SQE_RMT_EID_SIZE 4
#define SQE_WQEBB_VAL_CNT 16
#define SQE_CTL_RMA_ADDR_OFFSET 32
#define SQE_CTL_RMA_ADDR_BIT GENMASK(31, 0)

#define MAX_SQE_BB_NUM 4
#define CDMA_JFS_MAX_SGE_READ 6
#define CDMA_JFS_MAX_SGE_NOTIFY 11

#define SQE_NORMAL_CTL_LEN 48
#define SQE_WRITE_NOTIFY_CTL_LEN 80

#define CDMA_ATOMIC_LEN_4 4
#define CDMA_ATOMIC_LEN_8 8
#define CDMA_ATOMIC_LEN_16 16
#define CDMA_ATOMIC_SGE_NUM 1

#define SQE_NOTIFY_ADDR_FIELD 56
#define SQE_NOTIFY_TOKEN_ID_FIELD 48
#define SQE_ATOMIC_DATA_FIELD 64

struct cdma_jfs_sqe_ctl {
	uint32_t sqe_bb_idx : 16;
	uint32_t place_odr : 2;
	uint32_t comp_order : 1;
	uint32_t fence : 1;
	uint32_t se : 1;
	uint32_t cqe : 1;
	uint32_t inline_en : 1;
	uint32_t rsv : 5;
	uint32_t token_en : 1;
	uint32_t rmt_jetty_type : 2;
	uint32_t owner : 1;

	uint32_t target_hint : 8;
	uint32_t opcode : 8;
	uint32_t rsv1 : 6;
	uint32_t inline_msg_len : 10;
	uint32_t tpn : 24;
	uint32_t sge_num : 8;
	uint32_t toid : 20;
	uint32_t rsv2 : 12;
	uint32_t rmt_eid[CDMA_SQE_RMT_EID_SIZE];
	uint32_t rmt_token_value;
	uint32_t rsv3;
	uint32_t rmt_addr_l_or_tid;
	uint32_t rmt_addr_h_or_tval;
};

struct cdma_jfs_wqebb {
	uint32_t value[SQE_WQEBB_VAL_CNT];
};

struct cdma_token_info {
	uint32_t token_id : 20;
	uint32_t rsv : 12;
	uint32_t token_value;
};

enum cdma_jfs_opcode {
	CDMA_OPCODE_WRITE = 0x03,
	CDMA_OPCODE_WRITE_WITH_NOTIFY = 0x05,
	CDMA_OPCODE_READ,
	CDMA_OPCODE_CAS,
	CDMA_OPCODE_FAA = 0xb,
	CDMA_OPCODE_INVALID = 0x12,
};

struct dma_jfs *cdma_u_create_jfs(struct dma_context *ctx,
				  struct dma_jfs_cfg *cfg);
int cdma_u_delete_jfs(struct dma_jfs *jfs);

int cdma_u_post_jfs_wr(struct dma_jfs *jfs, dma_jfs_wr_t *wr,
		       dma_jfs_wr_t **bad_wr);

#endif
