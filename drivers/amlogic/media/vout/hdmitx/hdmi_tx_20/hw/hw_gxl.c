/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_gxl.c
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

#include <linux/printk.h>
#include "common.h"
#include "mach_reg.h"

/*
 * From GXL chips, registers names changes.
 * Added new HHI_HDMI_PLL_CNTL1 but addessed as 0xc9.
 * It's different with HHI_HDMI_PLL_CNTL2 0xc9 in previouse chips.
 */
#ifdef P_HHI_HDMI_PLL_CNTL2
#undef P_HHI_HDMI_PLL_CNTL2
#endif
#ifdef P_HHI_HDMI_PLL_CNTL3
#undef P_HHI_HDMI_PLL_CNTL3
#endif
#ifdef P_HHI_HDMI_PLL_CNTL4
#undef P_HHI_HDMI_PLL_CNTL4
#endif
#ifdef P_HHI_HDMI_PLL_CNTL5
#undef P_HHI_HDMI_PLL_CNTL5
#endif
#ifdef P_HHI_HDMI_PLL_CNTL6
#undef P_HHI_HDMI_PLL_CNTL6
#endif

#define P_HHI_HDMI_PLL_CNTL1 HHI_REG_ADDR(0xc9)
#define P_HHI_HDMI_PLL_CNTL2 HHI_REG_ADDR(0xca)
#define P_HHI_HDMI_PLL_CNTL3 HHI_REG_ADDR(0xcb)
#define P_HHI_HDMI_PLL_CNTL4 HHI_REG_ADDR(0xcc)
#define P_HHI_HDMI_PLL_CNTL5 HHI_REG_ADDR(0xcd)

/*
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_0		reg6[31]	GPIO1[20]
 * SCL		GPIOH_2		reg6[29]	GPIO1[22[
 * SDA		GPIOH_1		reg6[30]	GPIO1[21]
 */

int hdmitx_hpd_hw_op_gxl(enum hpd_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 25, 1);
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = !!(hd_read_reg(P_PERIPHS_PIN_MUX_6) & (1 << 31));
		break;
	case HPD_MUX_HPD:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 20, 1);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 1, 31, 1);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 0, 31, 1);
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 20, 1);
		break;
	case HPD_READ_HPD_GPIO:
		ret = !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 20));
		break;
	default:
		pr_info("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio_gxl(void)
{
	return !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 20));
}

int hdmitx_ddc_hw_op_gxl(enum ddc_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
		hd_set_reg_bits(P_PAD_PULL_UP_EN_REG1, 0, 23, 2);
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 23, 2);
		break;
	case DDC_MUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 21, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 3, 29, 2);
		break;
	case DDC_UNMUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 21, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 0, 29, 2);
		break;
	default:
		pr_info("error ddc cmd %d\n", cmd);
	}
	return ret;
}

void set_gxl_hpll_clk_out(unsigned int frac_rate, unsigned int clk)
{
	switch (clk) {
	case 5940000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000027b);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb281);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb300);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0xc60f30e0);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 5405400:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x400002e1);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb0e6);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 4455000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x400002b9);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb1c2);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb280);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000029a);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb222);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb2c0);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3450000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000028f);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb300);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 3243240:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x40000287);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb08a);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4000027b);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb281);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb300);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	case 4324320:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x400002b4);
		if (frac_rate)
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb000);
		else
			hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x800cb0b8);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x860f30c4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0c8e0000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x001fa729);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x01a31500);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

void set_hpll_sspll_gxl(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_1920x1080p60_16x9:
	case HDMI_1920x1080p50_16x9:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x68b48c4, 0, 30);
		break;
	case HDMI_1280x720p60_16x9:
	case HDMI_1280x720p50_16x9:
	case HDMI_1920x1080i60_16x9:
	case HDMI_1920x1080i50_16x9:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x64348c4, 0, 30);
		break;
	default:
		break;
	}
}

void set_hpll_od1_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 21, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 21, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 21, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

void set_hpll_od2_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 23, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 23, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 23, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

void set_hpll_od3_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 19, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 19, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 19, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}
