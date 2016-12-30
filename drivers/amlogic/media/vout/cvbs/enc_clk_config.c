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

/* Local Headers */
#include "enc_clk_config.h"
#include "cvbs_out_reg.h"
#include "cvbs_log.h"

static DEFINE_MUTEX(setclk_mutex);

static int hpll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = 200;
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
	cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0, 30, 1);
}

void set_vmode_clk(void)
{
	int ret;

	cvbs_log_info("set_vmode_clk start\n");
	mutex_lock(&setclk_mutex);
	if (is_meson_gxbb_cpu() ||
		is_meson_gxtvbb_cpu()) {
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x5800023d);
		if (is_meson_gxbb_cpu())
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00404e00);
		else
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
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXL)) {
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
	} else {
		cvbs_log_err("Set clk.cpu_type unsupport.\n");
		goto LAB_OUT;
	}
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
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 4, VCLK2_CLK_IN_SEL, 3);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);
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

LAB_OUT:
	mutex_unlock(&setclk_mutex);
	cvbs_log_info("set_vmode_clk DONE\n");
}

void disable_vmode_clk(void)
{
	disable_hpll_clk_out();
}
