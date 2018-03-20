/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_clk.h
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

#ifndef __HW_ENC_CLK_CONFIG_H__
#define __HW_ENC_CLK_CONFIG_H__

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/vinfo.h>

#define VID_PLL_DIV_1      0
#define VID_PLL_DIV_2      1
#define VID_PLL_DIV_3      2
#define VID_PLL_DIV_3p5    3
#define VID_PLL_DIV_3p75   4
#define VID_PLL_DIV_4      5
#define VID_PLL_DIV_5      6
#define VID_PLL_DIV_6      7
#define VID_PLL_DIV_6p25   8
#define VID_PLL_DIV_7      9
#define VID_PLL_DIV_7p5    10
#define VID_PLL_DIV_12     11
#define VID_PLL_DIV_14     12
#define VID_PLL_DIV_15     13
#define VID_PLL_DIV_2p5    14
#define VID_PLL_DIV_3p25   15

#define GROUP_MAX	8
struct hw_enc_clk_val_group {
	enum hdmi_vic group[GROUP_MAX];
	unsigned int hpll_clk_out; /* Unit: kHz */
	unsigned int od1;
	unsigned int od2; /* HDMI_CLK_TODIG */
	unsigned int od3;
	unsigned int vid_pll_div;
	unsigned int vid_clk_div;
	unsigned int hdmi_tx_pixel_div;
	unsigned int encp_div;
	unsigned int enci_div;
};

void hdmitx_set_clk(struct hdmitx_dev *hdev);
void hdmitx_set_cts_sys_clk(struct hdmitx_dev *hdev);
void hdmitx_set_top_pclk(struct hdmitx_dev *hdev);
void hdmitx_set_hdcp_pclk(struct hdmitx_dev *hdev);
void hdmitx_set_cts_hdcp22_clk(struct hdmitx_dev *hdev);
void hdmitx_set_sys_clk(struct hdmitx_dev *hdev, unsigned char flag);
void hdmitx_set_vclk2_encp(struct hdmitx_dev *hdev);
void hdmitx_disable_vclk2_enci(struct hdmitx_dev *hdev);
void hdmitx_set_vclk2_enci(struct hdmitx_dev *hdev);


#endif

