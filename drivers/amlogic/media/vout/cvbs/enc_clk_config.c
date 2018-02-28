/*
 * drivers/amlogic/media/vout/cvbs/enc_clk_config.c
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

/* Linux Headers */
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/iomap.h>

/* Local Headers */
#include "cvbs_out.h"
#include "enc_clk_config.h"
#include "cvbs_out_reg.h"

static DEFINE_MUTEX(setclk_mutex);

static int hpll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = 2000;
	int ret = 0;

	do {
		udelay(50);
		pll_lock = cvbs_out_hiu_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (wait_loop == 0)
		ret = -1;
	return ret;
}

static void disable_hpll_clk_out(void)
{
	/* close cts_enci_clk & cts_vdac_clk */
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 4, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 0, 1);

	/* close vclk_div gate */
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 0, 19, 1);

	/* close vid_pll gate */
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);

	/* close hpll */
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A)
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0, 28, 1);
	else
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0, 30, 1);
}

void set_vmode_clk(void)
{
	int ret;

	pr_info("set_vmode_clk start\n");
	mutex_lock(&setclk_mutex);
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_GXTVBB) {
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x5800023d);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00404380);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x801da72c);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x71486980);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x00000e55);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4800023d);
		ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]: hdmi_pll lock failed\n");
		cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
		udelay(5);
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A) {
		pr_info("config g12a hpll\n");
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x1b01047b);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00018000);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x00000000);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x6a29dc00);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x65771290);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x39272000);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL7, 0x54540000);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x3b01047b);
		udelay(100);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x1b01047b);
		ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]:hdmi_pll lock failed\n");
	} else {
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4000027b);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x800cb300);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0xa6212844);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x0c4d000c);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x001fa729);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x01a31500);
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]: hdmi_pll lock failed\n");
	}

	/* divider: 1 */
	/* clk div */
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	/* Enable the final output clock */
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

	/* setup the XD divider value */
	cvbs_out_hiu_setb(HHI_VIID_CLK_DIV, (55 - 1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	/*before g12a set 4 and 0 all ok,after g12a must set 0*/
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_CLK_IN_SEL, 3);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);
	/* vclk: 27M */
	/* [15:12] encl_clk_sel, select vclk2_div1 */
	cvbs_out_hiu_setb(HHI_VID_CLK_DIV, 8, 28, 4);
	cvbs_out_hiu_setb(HHI_VIID_CLK_DIV, 8, 28, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	cvbs_out_hiu_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 0, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);

	mutex_unlock(&setclk_mutex);
	pr_info("set_vmode_clk DONE\n");
}

void disable_vmode_clk(void)
{
	disable_hpll_clk_out();
}
