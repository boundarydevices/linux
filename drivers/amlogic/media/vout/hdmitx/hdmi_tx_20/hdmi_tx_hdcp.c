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
/* #include <mach/hdmi_tx_reg.h> */
#include "hdmi_tx_hdcp.h"
/*
 * hdmi_tx_hdcp.c
 * version 1.1
 */

/* android ics switch device */
static struct extcon_dev hdcp_dev = {
	.name = "hdcp",
};

/* For most cases, we don't use HDCP
 * If using HDCP, need add follow command in boot/init.rc and
 * recovery/boot/init.rc
 * write /sys/module/hdmitx/parameters/hdmi_output_force 0
 */
static int hdmi_output_force = 1;
static int hdmi_authenticated;
static int hdmi_hdcp_process = 1;

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

static struct timer_list hdcp_monitor_timer;
static void hdcp_monitor_func(unsigned long arg)
{
	/* static int hdcp_auth_flag = 0; */
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)hdcp_monitor_timer.data;

	if ((hdev->HWOp.Cntl) && (hdev->log & (HDMI_LOG_HDCP)))
		hdev->HWOp.Cntl(hdev, HDMITX_HDCP_MONITOR, 1);

	mod_timer(&hdcp_monitor_timer, jiffies + 2 * HZ);
}

static int hdmitx_hdcp_task(void *data)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)data;

	init_timer(&hdcp_monitor_timer);
	hdcp_monitor_timer.data = (ulong) data;
	hdcp_monitor_timer.function = hdcp_monitor_func;
	hdcp_monitor_timer.expires = jiffies + HZ;
	add_timer(&hdcp_monitor_timer);

	while (hdev->hpd_event != 0xff) {
		hdmi_authenticated = hdev->HWOp.CntlDDC(hdev,
			DDC_HDCP_GET_AUTH, 0);
		extcon_set_state(&hdcp_dev, 0, hdmi_authenticated); //TO_DO___49
		msleep_interruptible(200);
	}

	return 0;
}

static int __init hdmitx_hdcp_init(void)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	hdmi_print(IMP, SYS "hdmitx_hdcp_init\n");
	if (hdev->hdtx_dev == NULL) {
		hdmi_print(IMP, SYS "exit for null device of hdmitx!\n");
		return -ENODEV;
	}

	extcon_dev_register(&hdcp_dev);

	hdev->task_hdcp = kthread_run(hdmitx_hdcp_task,	(void *)hdev,
		"kthread_hdcp");

	return 0;
}

static void __exit hdmitx_hdcp_exit(void)
{
	extcon_dev_unregister(&hdcp_dev);
}


MODULE_PARM_DESC(hdmi_authenticated, "\n hdmi_authenticated\n");
module_param(hdmi_authenticated, int, 0444);

MODULE_PARM_DESC(hdmi_hdcp_process, "\n hdmi_hdcp_process\n");
module_param(hdmi_hdcp_process, int, 0664);

MODULE_PARM_DESC(hdmi_output_force, "\n hdmi_output_force\n");
module_param(hdmi_output_force, int, 0664);


module_init(hdmitx_hdcp_init);
module_exit(hdmitx_hdcp_exit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX HDCP driver");
MODULE_LICENSE("GPL");

