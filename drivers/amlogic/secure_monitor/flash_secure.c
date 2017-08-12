/*
 * drivers/amlogic/secure_monitor/flash_secure.c
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <asm/irqflags.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/meson-secure.h>
#include <asm/compiler.h>
#include "flash_secure.h"


#define SECURE_MONITOR_MODULE_NAME "secure_monitor"
#define SECURE_MONITOR_DRIVER_NAME  "secure_monitor"
#define SECURE_MONITOR_DEVICE_NAME  "secure_monitor"
#define SECURE_MONITOR_CLASS_NAME "secure_monitor"


static uint32_t sm_share_mem_addr;
static uint32_t sm_share_mem_size;

/*
 * SHARE MEM:
 * Communicate Head: 1KB
 * communicate Head lock mutex location: the last 4B of HEAD; inited by secureOS
 * Data: 128KB
 */
#define SHARE_MEM_HEAD_OFFSET 0x0
#define SHARE_MEM_DATA_OFFSET 0x400
#define SHARE_MEM_PHY_SIZE 0x20400
#define FLASH_BUF_SIZE 0x20000

#define SHARE_MEM_CMD_FREE	0x00000000
#define SHARE_MEM_CMD_WRITE	0x00000001

struct __NS_SHARE_MEM_HEAD {
	unsigned int cmdlock;
	unsigned int cmd;
	unsigned int state;
	/* can store input data position in NS_SHARE_MEM_CONTENT */
	unsigned int input;
	unsigned int inlen;
	unsigned int output;
	unsigned int outlen;
};

struct secure_monitor_arg {
	unsigned char *pfbuf;
	unsigned char *psbuf;
};
static struct secure_monitor_arg secure_monitor_buf;
static struct task_struct *secure_task;

static int secure_writer_monitor(void *arg);

static int secure_monitor_start(void)
{
	int ret = 0;

	sm_share_mem_addr = meson_secure_mem_flash_start();
	sm_share_mem_size = meson_secure_mem_flash_size();

	pr_info("%s:%d\n", __func__, __LINE__);
	pr_info("sm share mem start: 0x%x, size: 0x%x\r\n",
		sm_share_mem_addr, sm_share_mem_size);
	secure_monitor_buf.pfbuf = kmalloc(FLASH_BUF_SIZE, GFP_KERNEL);
	if (!secure_monitor_buf.pfbuf) {
		pr_err("nandbuf create fail!\n");
		ret = -ENOMEM;
		goto flash_monitor_probe_exit;
	}
	secure_monitor_buf.psbuf = ioremap_cached(
					sm_share_mem_addr,
					sm_share_mem_size);
	if (!secure_monitor_buf.psbuf) {
		pr_err("ioremap share memory fail\n");
		ret = -ENOMEM;
		goto flash_monitor_probe_exit1;
	}

	secure_task = kthread_run(secure_writer_monitor,
		(void *)(&secure_monitor_buf), "secure_flash");
	if (IS_ERR_OR_NULL(secure_task)) {
		pr_err("create secure task failed\n");
		ret = -ENODEV;
		goto flash_monitor_probe_exit2;
	}
	goto flash_monitor_probe_exit;

flash_monitor_probe_exit2:
	if (secure_monitor_buf.psbuf)
		iounmap(secure_monitor_buf.psbuf);
	secure_monitor_buf.psbuf = NULL;

flash_monitor_probe_exit1:
	kfree(secure_monitor_buf.pfbuf);
	secure_monitor_buf.pfbuf = NULL;

flash_monitor_probe_exit:
	return ret;
}


void write_to_flash(unsigned char *psrc, unsigned int size)
{
#ifdef CONFIG_AMLOGIC_M8B_NAND
	int error = 0;

	pr_info("%s %d save secure here\n", __func__, __LINE__);
	error = secure_storage_nand_write(psrc, size);
	if (error)
		pr_err("save secure failed\n");
	pr_info("///////////////////////////////////////save secure success//////////////////////////////////\n");
	return;
#endif

#ifdef CONFIG_SPI_NOR_SECURE_STORAGE
	unsigned char *secure_ptr = psrc;
	int error = 0;

	pr_info("%s %d save secure here\n", __func__, __LINE__);
	error = secure_storage_spi_write(secure_ptr, size);
	if (error)
		pr_err("save secure failed\n");
	pr_info("///////////////////////////////////////save secure success//////////////////////////////////\n");
	return;
#endif

#ifdef CONFIG_EMMC_SECURE_STORAGE
	unsigned char *secure_ptr = psrc;
	int error = 0;

	pr_info("%s %d save secure here\n", __func__, __LINE__);
	error = mmc_secure_storage_ops(secure_ptr, size, 1);
	if (error)
		pr_err("save secure failed\n");
	pr_info("///////////////////////////////////////save secure success//////////////////////////////////\n");
	return;
#endif
}


static int secure_writer_monitor(void *arg)
{
	struct secure_monitor_arg *parg = (struct secure_monitor_arg *)arg;
	unsigned char *pfbuf = parg->pfbuf;
	unsigned long flags;

	struct __NS_SHARE_MEM_HEAD *pshead = (struct __NS_SHARE_MEM_HEAD *)
		(parg->psbuf+SHARE_MEM_HEAD_OFFSET);
	unsigned char *psdata = parg->psbuf + SHARE_MEM_DATA_OFFSET;
	bool w_busy = false;

	while (1) {
		if (w_busy) {
			write_to_flash(pfbuf, FLASH_BUF_SIZE);
			w_busy = false;
		}

		flags = arch_local_irq_save();
		if (lock_mutex_try(&(pshead->cmdlock))) {
			if (pshead->cmd == SHARE_MEM_CMD_WRITE) {
				memcpy(pfbuf, psdata, FLASH_BUF_SIZE);
				pshead->cmd = SHARE_MEM_CMD_FREE;
				w_busy = true;
				pr_info("************kernel detect write flag*****\n");
			}
			unlock_mutex(&(pshead->cmdlock));
		}
		arch_local_irq_restore(flags);

		if (!w_busy) {
			if (kthread_should_stop())
				break;
			msleep(200);
		}
	}

	return 0;
}

static int __init secure_monitor_init(void)
{
	int ret =  -1;

	pr_info("%s:%d\n", __func__, __LINE__);
	if (meson_secure_enabled()) {
		ret = secure_monitor_start();
		if (ret != 0) {
			pr_err("fail to register sm drv err:%d\n", ret);
			return -ENODEV;
		}
		return ret;
	} else
		return 0;
}

static void __exit secure_monitor_exit(void)
{
	pr_info("**************flash_secure_remove start!\n");
	if (secure_task) {
		kthread_stop(secure_task);
		secure_task = NULL;
	}

	if (secure_monitor_buf.psbuf)
		iounmap(secure_monitor_buf.psbuf);
	secure_monitor_buf.psbuf = NULL;

	kfree(secure_monitor_buf.pfbuf);
	secure_monitor_buf.pfbuf = NULL;

	pr_info("**************flash_secure_remove end!\n");
}


module_init(secure_monitor_init);
module_exit(secure_monitor_exit);


MODULE_DESCRIPTION("AMLOGIC secure monitor driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yan Wang <yan.wang@amlogic.com>");
