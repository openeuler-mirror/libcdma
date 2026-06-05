// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <syslog.h>
#include "cdma_u_log.h"
#include "cdma_u_lib.h"

static struct cdma_log_level_st g_cdma_log_level = {
	CDMA_VLOG_LEVEL_INFO,
	PTHREAD_MUTEX_INITIALIZER
};

#define MAX_LOG_LEN 512
#define CDMA_LOG_TAG "CDMA_LOG_TAG"
#define CDMA_LOG_ENV_STR "CDMA_LOG_LEVEL"
#define CDMA_ENV_MAX_LEN 32

static inline void cdma_default_log_func(int level, char *message)
{
	syslog(level, "%s", message);
}

inline bool cdma_log_drop(enum cdma_log_level level)
{
	return level > g_cdma_log_level.level;
}

enum cdma_log_level cdma_log_get_level(const char *level_string)
{
	if (level_string == NULL)
		return CDMA_VLOG_LEVEL_MAX;

	if (strcasecmp(level_string, "fatal") == 0)
		return CDMA_VLOG_LEVEL_CRIT;

	if (strcasecmp(level_string, "error") == 0)
		return CDMA_VLOG_LEVEL_ERR;

	if (strcasecmp(level_string, "warning") == 0)
		return CDMA_VLOG_LEVEL_WARNING;

	if (strcasecmp(level_string, "info") == 0)
		return CDMA_VLOG_LEVEL_INFO;

	if (strcasecmp(level_string, "debug") == 0)
		return CDMA_VLOG_LEVEL_DEBUG;

	return CDMA_VLOG_LEVEL_MAX;
}

void cdma_getenv_log_level(void)
{
	char *level_str = getenv(CDMA_LOG_ENV_STR);
	if (level_str == NULL)
		return;

	if (strnlen(level_str, CDMA_ENV_MAX_LEN) >= CDMA_ENV_MAX_LEN) {
		CDMA_LOG_ERR("invalid parameter: log level str.");
		return;
	}

	enum cdma_log_level log_level = cdma_log_get_level(level_str);
	if (log_level != CDMA_VLOG_LEVEL_MAX) {
		pthread_mutex_lock(&g_cdma_log_level.lock);
		g_cdma_log_level.level = log_level;
		pthread_mutex_unlock(&g_cdma_log_level.lock);
	}
}

static __attribute__((format(printf, 4, 0)))
int cdma_vlog(const char *function, int line, enum cdma_log_level level,
	      const char *format, va_list va)
{
	char newformat[MAX_LOG_LEN + 1] = {0};
	char logmsg[MAX_LOG_LEN + 1] = {0};
	int ret;

	/* add log head info, "CDMA_LOG_TAG|function|[line]|format" */
	ret = snprintf(newformat, MAX_LOG_LEN, "%s|%s[%d]|%s", CDMA_LOG_TAG,
		       function, line, format);
	if (ret <= 0 || ret >= (int)sizeof(newformat))
		return ret;

	ret = vsnprintf(logmsg, MAX_LOG_LEN, newformat, va);
	if (ret == -1) {
		(void)printf("logmsg size exceeds MAX_LOG_LEN size :%d.\n",
			     MAX_LOG_LEN);
		return ret;
	}

	cdma_default_log_func((int)level, logmsg);

	return ret;
}

void cdma_log(const char *function, int line, enum cdma_log_level level,
	      const char *format, ...)
{
	va_list va;

	va_start(va, format);
	(void)cdma_vlog(function, line, level, format, va);
	va_end(va);
}

static __attribute__((constructor)) void cdma_log_init(void)
{
	cdma_getenv_log_level();
	CDMA_LOG_INFO("cdma version %s\n", CDMA_VERSION);
}

static __attribute__((destructor)) void cdma_log_uninit(void)
{
}
