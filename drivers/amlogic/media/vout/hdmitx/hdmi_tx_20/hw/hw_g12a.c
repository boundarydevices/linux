/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_g12a.c
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
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_1		reg0[23]	GPIO1[21]
 * SCL		GPIOH_2		reg0[22]	GPIO1[22[
 * SDA		GPIOH_3		reg0[21]	GPIO1[23]
 */

#ifdef P_HHI_HDMI_PLL_CNTL
#undef P_HHI_HDMI_PLL_CNTL
#endif
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
#ifdef HHI_HDMI_PLL_CNTL_I
#undef HHI_HDMI_PLL_CNTL_I
#endif
#ifdef P_HHI_HDMI_PLL_CNTL_I
#undef P_HHI_HDMI_PLL_CNTL_I
#endif

#define P_HHI_HDMI_PLL_CNTL HHI_REG_ADDR(0xc8)
#define P_HHI_HDMI_PLL_CNTL0 P_HHI_HDMI_PLL_CNTL
#define P_HHI_HDMI_PLL_CNTL1 HHI_REG_ADDR(0xc9)
#define P_HHI_HDMI_PLL_CNTL2 HHI_REG_ADDR(0xca)
#define P_HHI_HDMI_PLL_CNTL3 HHI_REG_ADDR(0xcb)
#define P_HHI_HDMI_PLL_CNTL4 HHI_REG_ADDR(0xcc)
#define P_HHI_HDMI_PLL_CNTL5 HHI_REG_ADDR(0xcd)
#define P_HHI_HDMI_PLL_CNTL6 HHI_REG_ADDR(0xce)
#define P_HHI_HDMI_PLL_STS HHI_REG_ADDR(0xcf)

void set_g12a_hpll_clk_out(unsigned int frac_rate, unsigned int clk)
{
	switch (clk) {
	case 5940000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b3a04f7);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00010000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x2a29dc00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x65771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39272000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 5405400:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004e1);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00007333);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 3712500:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00049a);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00016000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 2970000:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b00047b);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00018000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	case 4324320:
		hd_write_reg(P_HHI_HDMI_PLL_CNTL0, 0x3b0004b4);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL1, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00000000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0a691c00);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x33771290);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x39270000);
		hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x50540000);
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_HHI_HDMI_PLL_CNTL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL0));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

void set_hpll_od1_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 16, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 16, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 16, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od2_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 18, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 18, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 18, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od3_g12a(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 20, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 20, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 2, 20, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

int hdmitx_hpd_hw_op_g12a(enum hpd_op cmd)
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
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_B, 1, 8, 4);
		hd_set_reg_bits(P_PREG_PAD_GPIO4_O, 1, 8, 1);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_B, 0, 8, 4);
		hd_set_reg_bits(P_PREG_PAD_GPIO4_O, 0, 8, 1);
		break;
	case HPD_READ_HPD_GPIO:
		ret = hdmitx_rd_reg(HDMITX_DWC_PHY_STAT0) & (1 << 1);
		break;
	default:
		break;
	}
	return ret;
}

