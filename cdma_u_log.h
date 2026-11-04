/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#ifndef __CDMA_U_LOG_H__
#define __CDMA_U_LOG_H__

#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

enum cdma_log_level {
	CDMA_VLOG_LEVEL_EMERG = 0,
	CDMA_VLOG_LEVEL_ALERT = 1,
	CDMA_VLOG_LEVEL_CRIT = 2,
	CDMA_VLOG_LEVEL_ERR = 3,
	CDMA_VLOG_LEVEL_WARNING = 4,
	CDMA_VLOG_LEVEL_NOTICE = 5,
	CDMA_VLOG_LEVEL_INFO = 6,
	CDMA_VLOG_LEVEL_DEBUG = 7,
	CDMA_VLOG_LEVEL_MAX = 8,
};

struct cdma_log_level_st {
	enum cdma_log_level level;
	pthread_mutex_t lock;
};

struct cdma_log_rl_state {
	_Atomic time_t window_start;
	_Atomic int count;
};

bool cdma_log_drop(enum cdma_log_level level);
bool cdma_log_ratelimit(struct cdma_log_rl_state *state);
void cdma_getenv_log_level(void);
void cdma_log(const char *function, int line, enum cdma_log_level level,
	      const char *format, ...) __attribute__((format(printf, 4, 5)));
enum cdma_log_level cdma_log_get_level(const char *level_string);

#define CDMA_LOG(l, ...) \
do { \
	if (!cdma_log_drop(CDMA_VLOG_LEVEL_##l)) { \
		cdma_log(__func__, __LINE__, CDMA_VLOG_LEVEL_##l, __VA_ARGS__); \
	} \
} while (0)

#define CDMA_LOG_INFO(...) CDMA_LOG(INFO, __VA_ARGS__)

#define CDMA_LOG_ERR(...) CDMA_LOG(ERR, __VA_ARGS__)

#define CDMA_LOG_WARN(...) CDMA_LOG(WARNING, __VA_ARGS__)

#define CDMA_LOG_DEBUG(...) CDMA_LOG(DEBUG, __VA_ARGS__)

/* ratelimit: max CDMA_LOG_RATE_BURST logs per CDMA_LOG_RATE_INTERVAL_SEC
 * seconds, first log is always printed.
 */
#define CDMA_LOG_RATE_BURST 5
#define CDMA_LOG_RATE_INTERVAL_SEC 5

#define CDMA_LOG_RL(l, name, ...) \
	do { \
		static struct cdma_log_rl_state name##_rl; \
		if (cdma_log_ratelimit(&name##_rl) && \
			!cdma_log_drop(CDMA_VLOG_LEVEL_##l)) { \
			cdma_log(__func__, __LINE__, CDMA_VLOG_LEVEL_##l, __VA_ARGS__); \
		} \
	} while (0)

#define CDMA_LOG_ERR_RL(name, ...) CDMA_LOG_RL(ERR, name, __VA_ARGS__)

#define CDMA_LOG_WARN_RL(name, ...) CDMA_LOG_RL(WARNING, name, __VA_ARGS__)

#endif
