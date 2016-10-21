/*
 * drivers/amlogic/efuse/efuse_hw64.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#endif
#include "efuse.h"
#ifdef CONFIG_ARM64
#include <linux/amlogic/efuse.h>
#endif
#include <linux/amlogic/secmon.h>

static long meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned int cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);

	register unsigned int x0 asm("x0");
	register unsigned int x1 asm("x1");
	register unsigned int x2 asm("x2");

	if (!sharemem_input_base || !sharemem_output_base)
		return -1;

	if (arg->cmd == EFUSE_HAL_API_READ)
		cmd = efuse_read_cmd;
	else
		cmd = efuse_write_cmd;
	offset = arg->offset;
	size = arg->size;
	sharemem_mutex_lock();
	if (arg->cmd == EFUSE_HAL_API_WRITE)
		memcpy((void *)sharemem_input_base,
			(const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	x0 = cmd;
	x1 = offset;
	x2 = size;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x0")
			__asmeq("%2", "x1")
			__asmeq("%3", "x2")
			"smc	#0\n"
		: "=r"(x0)
		: "r"(x0), "r"(x1), "r"(x2));
	ret = x0;
	*retcnt = x0;

	if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0))
		memcpy((void *)arg->buffer,
			(const void *)sharemem_output_base, ret);
	sharemem_mutex_unlock();

	if (!ret)
		return -1;
	else
		return 0;
}

int meson_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	int ret;

	if (!arg)
		return -1;

	set_cpus_allowed_ptr(current, cpumask_of(0));
	ret = meson_efuse_fn_smc(arg);
	set_cpus_allowed_ptr(current, cpu_all_mask);
	return ret;
}

ssize_t meson_trustzone_efuse_get_max(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	unsigned int cmd;

	register uint64_t x0 asm("x0");

	if (arg->cmd == EFUSE_HAL_API_USER_MAX) {
		cmd = efuse_get_max_cmd;

		asm __volatile__("" : : : "memory");
		x0 = cmd;

		do {
			asm volatile(
			    __asmeq("%0", "x0")
			    __asmeq("%1", "x0")
			    "smc    #0\n"
			    : "=r"(x0)
			    : "r"(x0));
		} while (0);
		ret = x0;

		if (!ret)
			return -1;
		else
			return ret;
	} else {
		pr_err("%s: cmd error!!!\n", __func__);
		return -1;
	}
}

ssize_t efuse_get_max(void)
{
	struct efuse_hal_api_arg arg;
	int ret;

	arg.cmd = EFUSE_HAL_API_USER_MAX;

	set_cpus_allowed_ptr(current, cpumask_of(0));
	ret = meson_trustzone_efuse_get_max(&arg);
	set_cpus_allowed_ptr(current, cpu_all_mask);

	if (ret == 0) {
		pr_info("ERROR: can not get efuse user max bytes!!!\n");
		return -1;
	} else
		return ret;
}

ssize_t _efuse_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned int retcnt;
	int ret;

	arg.cmd = EFUSE_HAL_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;
	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos += retcnt;
		return retcnt;
	}
	pr_err("%s:%s:%d: read error!!!\n",
			   __FILE__, __func__, __LINE__);
	return ret;
}

ssize_t _efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned int retcnt;
	int ret;

	arg.cmd = EFUSE_HAL_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;

	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = retcnt;
		return retcnt;
	}
	pr_err("%s:%s:%d: write error!!!\n",
			   __FILE__, __func__, __LINE__);
	return ret;
}

ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos)
{
	char data[EFUSE_BYTES];
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	memset(data, 0, count);

	pdata = data;
	pos = *ppos;
	ret = _efuse_read(pdata, count, (loff_t *)&pos);

	memcpy(buf, data, count);

	return ret;
}

ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos)
{
	char data[EFUSE_BYTES];
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	if (count == 0) {
		pr_info("data length: 0 is error!\n");
		return -1;
	}

	memset(data, 0, EFUSE_BYTES);

	memcpy(data, buf, count);
	pdata = data;
	pos = *ppos;

	ret = _efuse_write(pdata, count, (loff_t *)&pos);

	return ret;
}
