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

static int pll_wait_lock(unsigned int reg, unsigned int lock_bit)
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

static void disable_vid1_clk_out(void)
{
	/* close cts_enci_clk & cts_vdac_clk */
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 4, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 0, 1);

	/* close vclk_div gate */
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 0, VCLK_DIV1_EN, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 0, VCLK_EN0, 1);

	/* close pll_div gate */
	cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
}

static void disable_vid2_clk_out(void)
{
	/* close cts_enci_clk & cts_vdac_clk */
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 4, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 0, 0, 1);

	/* close vclk2_div gate */
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_DIV1_EN, 1);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
}

static void cvbs_set_vid1_clk(unsigned int src_pll)
{
	int sel = 0;

	pr_info("%s\n", __func__);

	if (src_pll == 0) { /* hpll */
		/* divider: 1 */
		/* Disable the div output clock */
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
		/* Enable the final output clock */
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

		sel = 0;
	} else { /* gp0_pll */
		sel = 1;
	}

	/* xd: 55 */
	/* setup the XD divider value */
	cvbs_out_hiu_setb(HHI_VID_CLK_DIV, (55 - 1), VCLK_XD0, 8);
	udelay(5);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, sel, VCLK_CLK_IN_SEL, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 1, VCLK_EN0, 1);
	udelay(2);

	/* vclk: 27M */
	/* [31:28]=0 enci_clk_sel, select vclk_div1 */
	cvbs_out_hiu_setb(HHI_VID_CLK_DIV, 0, 28, 4);
	cvbs_out_hiu_setb(HHI_VIID_CLK_DIV, 0, 28, 4);
	/* release vclk_div_reset and enable vclk_div */
	cvbs_out_hiu_setb(HHI_VID_CLK_DIV, 1, VCLK_XD_EN, 2);
	udelay(5);

	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 1, VCLK_DIV1_EN, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 1, VCLK_SOFT_RST, 1);
	udelay(10);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL, 0, VCLK_SOFT_RST, 1);
	udelay(5);
}

static void cvbs_set_vid2_clk(unsigned int src_pll)
{
	int sel = 0;

	pr_info("%s\n", __func__);

	if (src_pll == 0) { /* hpll */
		/* divider: 1 */
		/* Disable the div output clock */
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
		/* Enable the final output clock */
		cvbs_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

		sel = 0;
	} else { /* gp0_pll */
		sel = 1;
	}

	/* xd: 55 */
	/* setup the XD divider value */
	cvbs_out_hiu_setb(HHI_VIID_CLK_DIV, (55 - 1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, sel, VCLK2_CLK_IN_SEL, 3);
	cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* vclk: 27M */
	/* [31:28]=8 enci_clk_sel, select vclk2_div1 */
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
}

void set_vmode_clk(void)
{
	int ret;

	pr_info("set_vmode_clk start\n");
	mutex_lock(&setclk_mutex);
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_GXTVBB) {
		pr_info("config gxtvbb hdmi pll\n");
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x5800023d);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00404380);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x801da72c);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x71486980);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x00000e55);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4800023d);
		ret = pll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]: hdmi_pll lock failed\n");
		cvbs_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
		udelay(5);
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
			cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
			cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x1) {
			pr_info("config g12a gp0_pll\n");
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL0, 0x180204f7);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL1, 0x00010000);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL2, 0x00000000);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL3, 0x6a28dc00);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL4, 0x65771290);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL5, 0x39272000);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL6, 0x56540000);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL0, 0x380204f7);
			udelay(100);
			cvbs_out_hiu_write(HHI_GP0_PLL_CNTL0, 0x180204f7);
			ret = pll_wait_lock(HHI_GP0_PLL_CNTL0, 31);
		} else {
			pr_info("config g12a hdmi_pll\n");
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x1a0504f7);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00010000);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x00000000);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x6a28dc00);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x65771290);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x39272000);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL7, 0x56540000);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x3a0504f7);
			udelay(100);
			cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x1a0504f7);
			ret = pll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		}
		if (ret)
			pr_info("[error]:hdmi_pll lock failed\n");
	} else {
		pr_info("config eqafter gxl hdmi pll\n");
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4000027b);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x800cb300);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0xa6212844);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x0c4d000c);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x001fa729);
		cvbs_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x01a31500);
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		ret = pll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]: hdmi_pll lock failed\n");
	}

	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x2)
			cvbs_set_vid1_clk(cvbs_clk_path & 0x1);
		else
			cvbs_set_vid2_clk(cvbs_clk_path & 0x1);
	} else {
		cvbs_set_vid2_clk(0);
	}

	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 0, 1);
	cvbs_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);

	mutex_unlock(&setclk_mutex);
	pr_info("set_vmode_clk DONE\n");
}

void disable_vmode_clk(void)
{
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x2)
			disable_vid1_clk_out();
		else
			disable_vid2_clk_out();

		/* disable pll */
		if (cvbs_clk_path & 0x1)
			cvbs_out_hiu_setb(HHI_GP0_PLL_CNTL0, 0, 28, 1);
		else
			cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0, 28, 1);
	} else {
		disable_vid2_clk_out();
		/* disable pll */
		cvbs_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0, 30, 1);
	}
}
