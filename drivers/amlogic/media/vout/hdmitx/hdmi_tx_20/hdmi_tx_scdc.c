/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_scdc.c
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

#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_ddc.h>

static struct timer_list scdc_tmds_cfg_timer;

static void tmds_config(unsigned long arg)
{
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)arg;

	/* TMDS 1/40 & Scramble */
	scdc_wr_sink(TMDS_CFG, hdev->para->tmds_clk_div40 ? 0x3 : 0);
}

void scdc_config(void *hdev)
{
	static int init_flag;

	if (!init_flag) {
		init_flag = 1;
		init_timer(&scdc_tmds_cfg_timer);
		scdc_tmds_cfg_timer.data = (ulong)hdev;
		scdc_tmds_cfg_timer.function = tmds_config;
		scdc_tmds_cfg_timer.expires = jiffies;
		add_timer(&scdc_tmds_cfg_timer);
		return;
	}
	mod_timer(&scdc_tmds_cfg_timer, jiffies);
}
