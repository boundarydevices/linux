/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_hdcp.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/extcon.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "hw/common.h"
#include "hdmi_tx_hdcp.h"
/*
 * hdmi_tx_hdcp.c
 * version 1.1
 */


static int hdmi_authenticated;

unsigned int hdcp_get_downstream_ver(void)
{
	unsigned int ret = 14;

	struct hdmitx_dev *hdev = get_hdmitx_device();

	/* if TX don't have HDCP22 key, skip RX hdcp22 ver */
	if (hdev->HWOp.CntlDDC(hdev,
		DDC_HDCP_22_LSTORE, 0) == 0)
		if (hdcp_rd_hdcp22_ver())
			ret = 22;
		else
			ret = 14;
	else
		ret = 14;
	return ret;
}

/* Notic: the HDCP key setting has been moved to uboot
 * On MBX project, it is too late for HDCP get from
 * other devices
 */

/* verify ksv, 20 ones and 20 zeroes */
int hdcp_ksv_valid(unsigned char *dat)
{
	int i, j, one_num = 0;

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 8; j++) {
			if ((dat[i] >> j) & 0x1)
				one_num++;
		}
	}
	return one_num == 20;
}

static void _hdcp_do_work(struct work_struct *work)
{
	struct hdmitx_dev *hdev =
		container_of(work, struct hdmitx_dev, work_do_hdcp.work);

	if (hdev->hdcp_mode == 2) {
		/* hdev->HWOp.CntlMisc(hdev, MISC_HDCP_CLKDIS, 1); */
		/* schedule_delayed_work(&hdev->work_do_hdcp, HZ / 50); */
	} else
		hdev->HWOp.CntlMisc(hdev, MISC_HDCP_CLKDIS, 0);
}

void hdmitx_hdcp_do_work(struct hdmitx_dev *hdev)
{
	_hdcp_do_work(&hdev->work_do_hdcp.work);
}

static int hdmitx_hdcp_task(void *data)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)data;

	INIT_DELAYED_WORK(&hdev->work_do_hdcp, _hdcp_do_work);
	while (hdev->hpd_event != 0xff) {
		hdmi_authenticated = hdev->HWOp.CntlDDC(hdev,
			DDC_HDCP_GET_AUTH, 0);
		hdmitx_hdcp_status(hdmi_authenticated);
		msleep_interruptible(200);
	}

	return 0;
}

static int __init hdmitx_hdcp_init(void)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	pr_info(HDCP "hdmitx_hdcp_init\n");
	if (hdev->hdtx_dev == NULL) {
		pr_info(HDCP "exit for null device of hdmitx!\n");
		return -ENODEV;
	}

	hdev->task_hdcp = kthread_run(hdmitx_hdcp_task,	(void *)hdev,
		"kthread_hdcp");

	return 0;
}

static void __exit hdmitx_hdcp_exit(void)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (hdev)
		cancel_delayed_work_sync(&hdev->work_do_hdcp);
}


MODULE_PARM_DESC(hdmi_authenticated, "\n hdmi_authenticated\n");
module_param(hdmi_authenticated, int, 0444);

module_init(hdmitx_hdcp_init);
module_exit(hdmitx_hdcp_exit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX HDCP driver");
MODULE_LICENSE("GPL");

