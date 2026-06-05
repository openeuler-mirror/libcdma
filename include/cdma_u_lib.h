/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_LIB_H__
#define __CDMA_U_LIB_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <cdma_abi.h>

#define CDMA_VERSION "1.0.2"
/* cover the maximum file length(255) + path prefix */
#define CDMA_MAX_DEV_NAME_LEN 300

struct dma_device {
	char name[CDMA_MAX_DEV_NAME_LEN];
	int fd;
	struct cdma_device_attr attr;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

typedef enum {
	DMA_CR_OPC_SEND = 0x00,
	DMA_CR_OPC_SEND_WITH_IMM,
	DMA_CR_OPC_SEND_WITH_INV,
	DMA_CR_OPC_WRITE_WITH_IMM,
} dma_cr_opcode_t;

typedef union dma_cr_flag {
	struct {
		uint8_t s_r : 1;
		uint8_t jetty : 1;
		uint8_t suspend_done : 1;
		uint8_t flush_err_done : 1;
		uint8_t reserved : 4;
	} bs;
	uint8_t value;
} dma_cr_flag_t;

struct dma_cr {
	enum dma_cr_status	status;
	uint64_t		user_ctx;
	dma_cr_opcode_t		opcode;
	dma_cr_flag_t		flag;
	uint32_t		completion_len;
	uint32_t		local_id;
	uint32_t		remote_id;
	uint32_t		tpn;
	uint32_t		rsv_bitmap;
	uint32_t		rsvd[4];
};

struct queue_cfg {
	uint32_t queue_depth;
	uint8_t priority;
	uint64_t user_ctx;
	uint32_t dcna;
	struct dev_eid rmt_eid;
	bool event_mode; /* false: poll mode, true: interrupt mode. */
	uint32_t trans_mode;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

struct dma_queue {
	struct dma_context *ctx;
	struct queue_cfg cfg;
	uint32_t queue_id;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

struct dma_seg {
	uint64_t handle;
	uint64_t sva;
	uint64_t len;
	uint32_t tid; /* data valid only in bit 0-19 */
	uint32_t token_value;
	bool token_value_valid;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

struct dma_seg_cfg {
	uint64_t addr;
	uint64_t len;
	uint32_t token_value;
	uint32_t tid; /* used by import segment, data valid only in bit 0-19*/
	bool token_value_valid;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

struct dma_context {
	struct dma_device *dma_dev;
	uint32_t tid;
	int async_fd;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

struct dma_aeqe {
	struct dma_context *ctx;
	uint32_t queue_id;
	uint32_t event_type;
	int duration;
	uint32_t rsv_bitmap;
	uint32_t rsvd[4];
};

typedef enum {
	DMA_STATUS_OK,
	DMA_STATUS_INVAL,
} dma_status;

typedef enum {
	DMA_AEQE_TYPE_JFC,
	DMA_AEQE_TYPE_JFS,
	DMA_AEQE_TYPE_INVALID,
	DMA_AEQE_TYPE_UNKNOWN,
} dma_aeqe_type;

/**
 * dma_get_device_list - Get DMA device list
 * @num_devices: DMA device number;
 * Return: address of the first device in the list
 */
struct dma_device *dma_get_device_list(uint32_t *num_devices);

/**
 * dma_free_device_list - Free DMA device list
 * @dev_list: DMA device list;
 * @num_devices: DMA device number;
 * Return: NA
 */
void dma_free_device_list(struct dma_device *dev_list, uint32_t num_devices);

/**
 * dma_get_device_by_eid - Get the specified EID DMA device
 * @eid: device eid pointer;
 * Return: DMA device structure pointer
 */
struct dma_device *dma_get_device_by_eid(struct dev_eid *eid);

/**
 * dma_create_context - Create DMA context
 * @dma_dev: DMA device pointer;
 * Return: DMA context structure pointer
 */
struct dma_context *dma_create_context(struct dma_device *dma_dev);

/**
 * dma_delete_context - Delete DMA context
 * @ctx: DMA context pointer;
 * Return: NA
 */
void dma_delete_context(struct dma_context *ctx);

/**
 * dma_alloc_queue - Alloc DMA queue
 * @ctx: DMA context pointer;
 * @cfg: DMA queue configuration information pointer;
 * Return: DMA queue structure pointer
 */
struct dma_queue *dma_alloc_queue(struct dma_context *ctx,
				  struct queue_cfg *cfg);

/**
 * dma_free_queue - Free DMA queue
 * @queue: DMA queue pointer;
 * Return: NA
 */
void dma_free_queue(struct dma_queue *queue);

/**
 * dma_register_seg - Register local segment
 * @ctx: DMA context pointer;
 * @cfg: DMA segment configuration information pointer;
 * Return: DMA segment structure pointer
 */
struct dma_seg *dma_register_seg(struct dma_context *ctx,
				 struct dma_seg_cfg *cfg);

/**
 * dma_unregister_seg - Unregister local segment
 * @ctx: DMA context pointer;
 * @seg: DMA segment pointer;
 * Return: NA
 */
void dma_unregister_seg(struct dma_context *ctx, struct dma_seg *seg);

/**
 * dma_import_seg - Import the remote segment
 * @cfg: DMA segment configuration information pointer;
 * Return: DMA segment structure pointer
 */
struct dma_seg *dma_import_seg(struct dma_seg_cfg *cfg);

/**
 * dma_unimport_seg - Unimport the remote segment
 * @seg: DMA segment pointer;
 * Return: NA
 */
void dma_unimport_seg(struct dma_seg *seg);

/**
 * dma_write - DMA write operation
 * @queue: DMA queue pointer;
 * @rmt_seg: the remote segment pointer;
 * @local_seg: the local segment pointer;
 * Return: operation result, DMA_STATUS_OK on success
 */
dma_status dma_write(struct dma_queue *queue, struct dma_seg *rmt_seg,
		     struct dma_seg *local_seg);

/**
 * dma_read - DMA read operation
 * @queue: DMA queue pointer;
 * @rmt_seg: the remote segment pointer;
 * @local_seg: the local segment pointer;
 * Return: operation result, DMA_STATUS_OK on success
 */
dma_status dma_read(struct dma_queue *queue, struct dma_seg *rmt_seg,
		    struct dma_seg *local_seg);

/**
 * dma_cas - DMA CAS operation
 * @queue: DMA queue pointer;
 * @rmt_seg: the remote segment pointer;
 * @local_seg: the local segment pointer;
 * @cmp: compare data, length <= 8B: CMP value, length > 8B: data address;
 * @swap: swap data, length <= 8B: swap value, length > 8B: data address;
 * Return: operation result, DMA_STATUS_OK on success
 */
dma_status dma_cas(struct dma_queue *queue, struct dma_seg *rmt_seg,
		   struct dma_seg *local_seg, uint64_t cmp, uint64_t swap);

/**
 * dma_faa - DMA FAA operation
 * @queue: DMA queue pointer;
 * @rmt_seg: the remote segment pointer;
 * @local_seg: the local segment pointer;
 * @add: add data for faa operation;
 * Return: operation result, DMA_STATUS_OK on success
 */
dma_status dma_faa(struct dma_queue *queue, struct dma_seg *rmt_seg,
		   struct dma_seg *local_seg, uint64_t add);

/**
 * dma_write_with_notify - DMA write with notify operation
 * @queue: DMA queue pointer;
 * @rmt_seg: the remote segment pointer;
 * @local_seg: the local segment pointer;
 * @notify_seg: the segment pointer for notify;
 * @notify_data: data value used for notify;
 * Return: operation result, DMA_STATUS_OK on success
 */
dma_status dma_write_with_notify(struct dma_queue *queue,
				 struct dma_seg *rmt_seg,
				 struct dma_seg *local_seg,
				 struct dma_seg *notify_seg,
				 uint64_t notify_data);

/**
 * dma_poll_queue - DMA polling queue
 * @queue: DMA queue pointer;
 * @cr_cnt: number of completion record;
 * @cr: completion record pointer;
 * Return: polling operation results  >0 on success, others on failed
 */
int dma_poll_queue(struct dma_queue *queue, uint32_t cr_cnt, struct dma_cr *cr);

/**
 * dma_wait_queue - Wait for completion event
 * @queue: DMA queue pointer;
 * @cr_cnt: number of completion record;
 * @timeout: timeout time for waiting ;
 * @cr: completion record pointer;
 * Return: operation result, >0 on success, others on failed
 */
int dma_wait_queue(struct dma_queue *queue, uint32_t cr_cnt, int timeout,
		   struct dma_cr *cr);

/**
 * dma_wait_ae - Wait for asynchronous events
 * @ctx: DMA context pointer;
 * @aeqe: asynchronous event queue entity pointer;
 * Return: operation result, 0 on 0 event success, 1 on 1 event success, others
 * on failed
 */
int dma_wait_ae(struct dma_context *ctx, struct dma_aeqe *aeqe);

#endif
