// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "cdma_u_lib.h"
#include "cdma_u_log.h"
#include "cdma_u_device.h"

static int cdma_open_cdev(char *path)
{
	size_t len = strlen(CDMA_CDEV_PATH);
	char *real_path;
	int fd;

	real_path = realpath(path, NULL);
	if (real_path == NULL) {
		CDMA_LOG_ERR("cdma get realpath from %s failed.\n", path);
		return -EIO;
	}

	if (strlen(real_path) <= len) {
		CDMA_LOG_ERR("cdma real path is too short.\n");
		free(real_path);
		return -EIO;
	}

	if (strncmp(real_path, CDMA_CDEV_PATH, len) != 0) {
		CDMA_LOG_ERR("cdma real path do not have the same prefix.\n");
		free(real_path);
		return -EIO;
	}

	fd = open(real_path, O_RDWR);
	if (fd < 0)
		CDMA_LOG_ERR("cdma open %s failed, errno = %d.\n", real_path, errno);

	free(real_path);
	return fd;
}

static uint32_t cdma_get_devs_num(void)
{
	uint32_t num_devices = 0;
	struct dirent *dent;
	DIR *cdev_dir;

	cdev_dir = opendir(CDMA_CDEV_PATH);
	if (cdev_dir == NULL)
		return 0;

	while ((dent = readdir(cdev_dir)) != NULL) {
		if (dent->d_name[0] == '.')
			continue;
		CDMA_LOG_INFO("cdma get cdev: %s\n", dent->d_name);
		num_devices++;
	}

	if (closedir(cdev_dir) < 0)
		CDMA_LOG_ERR("failed to close dir: %s\n", CDMA_CDEV_PATH);

	return num_devices;
}

void cdma_free_device_list(struct dma_device *dev_list, uint32_t num_devices)
{
	uint32_t i;

	for (i = 0; i < num_devices; i++) {
		if (dev_list[i].fd > 0)
			close(dev_list[i].fd);
	}

	free(dev_list);
}

static bool cdma_find_seid_in_eus(struct dma_device *dev,
				  struct dev_eid *eid, struct eu_info *eu)
{
	struct eu_info *eus;
	uint8_t i;

	for (i = 0; i < dev->attr.eu_num; i++) {
		eus = &dev->attr.eus[i];
		if (memcmp(&eus->eid, eid, sizeof(*eid)) == 0) {
			memcpy(eu, eus, sizeof(*eus));
			return true;
		}
	}

	return false;
}

static void cdma_free_device_list_except_one(struct dma_device *dev_list,
					     uint32_t num_devices,
					     uint32_t idx)
{
	uint32_t i;

	for (i = 0; i < num_devices; i++) {
		if (dev_list[i].fd > 0 && i != idx)
			close(dev_list[i].fd);
	}

	free(dev_list);
}

static int cdma_query_device_info(struct dma_device *dev)
{
	struct cdma_cmd_query_device_attr_args info = {0};
	struct cdma_ioctl_hdr hdr;
	uint8_t eu_num;
	int ret, i;

	hdr.command = (uint32_t)CDMA_CMD_QUERY_DEV_INFO;
	hdr.args_len = (uint32_t)sizeof(info);
	hdr.args_addr = (uint64_t)&info;

	ret = ioctl(dev->fd, CDMA_SYNC, &hdr);
	if (ret) {
		CDMA_LOG_ERR("ioctl cdma query device info, ret = %d %d, cmd = %u.\n",
			     ret, errno, hdr.command);
		return ret;
	}

	eu_num = info.out.attr.eu_num;
	if (eu_num == 0 || eu_num > CDMA_MAX_EU_NUM) {
		CDMA_LOG_ERR("cdma eu num %u is invalid.\n", eu_num);
		return -EINVAL;
	}

	dev->attr.eu_num = eu_num;
	dev->attr.eid = info.out.attr.eid;
	memcpy(dev->attr.eus, info.out.attr.eus, eu_num * sizeof(struct eu_info));
	dev->attr.eu = info.out.attr.eus[0];
	dev->attr.dev_cap = info.out.attr.dev_cap;

	for (i = 0; i < eu_num; i++) {
		CDMA_LOG_INFO(
			"cdma query eus[%d], upi = 0x%x, eid = 0x%x, eid_idx = 0x%x.\n", i,
			dev->attr.eus[i].upi, dev->attr.eus[i].eid.dw0,
			dev->attr.eus[i].eid_idx);
	}

	return 0;
}

struct dma_device *cdma_get_device_list(uint32_t *num_devices)
{
	char cdev_path[CDMA_MAX_DEV_NAME_LEN] = {0};
	struct dma_device *dev_list;
	struct dirent *dent;
	uint32_t i = 0;
	DIR *cdev_dir;
	int ret;

	*num_devices = cdma_get_devs_num();
	if (*num_devices == 0)
		return NULL;

	cdev_dir = opendir(CDMA_CDEV_PATH);
	if (cdev_dir == NULL)
		return NULL;

	dev_list = (struct dma_device *)calloc(*num_devices, sizeof(*dev_list));
	if (dev_list == NULL) {
		CDMA_LOG_ERR("alloc dev_list failed.\n");
		goto out;
	}

	while ((dent = readdir(cdev_dir)) != NULL) {
		if (dent->d_name[0] == '.')
			continue;

		(void)snprintf(cdev_path, sizeof(cdev_path), "%s/%s", CDMA_CDEV_PATH,
			       dent->d_name);
		dev_list[i].fd = cdma_open_cdev(cdev_path);
		if (dev_list[i].fd < 0)
			continue;

		(void)strncpy(dev_list[i].name, dent->d_name,
			      CDMA_MAX_DEV_NAME_LEN - 1);
		ret = cdma_query_device_info(&dev_list[i]);
		if (ret) {
			close(dev_list[i].fd);
			dev_list[i].fd = -1;
			continue;
		}

		if (++i >= *num_devices)
			break;
	}

	if (i != *num_devices) {
		CDMA_LOG_WARN("%u cdma devices are online, but %u devices are ready.\n",
			      *num_devices, i);
		*num_devices = i;
	}

out:
	if (closedir(cdev_dir) < 0)
		CDMA_LOG_ERR("close dir %s\n failed", CDMA_CDEV_PATH);

	return dev_list;
}

struct dma_device *cdma_get_device_by_eid(struct dev_eid *eid)
{
	struct dma_device *dev_list = NULL;
	struct dma_device *ret_dev = NULL;
	uint32_t num_devices;
	struct eu_info eu;
	uint32_t i;

	dev_list = cdma_get_device_list(&num_devices);
	if (!dev_list)
		return NULL;

	for (i = 0; i < num_devices; i++) {
		if (cdma_find_seid_in_eus(&dev_list[i], eid, &eu)) {
			ret_dev = (struct dma_device *)calloc(1, sizeof(*dev_list));
			if (ret_dev == NULL)
				goto exit_without_dev;

			memcpy(ret_dev, &dev_list[i], sizeof(*ret_dev));
			memcpy(&ret_dev->attr.eu, &eu, sizeof(eu));
			cdma_free_device_list_except_one(dev_list, num_devices, i);
			return ret_dev;
		}
	}

exit_without_dev:
	cdma_free_device_list(dev_list, num_devices);
	return NULL;
}
