/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hw_txlx.c
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

int hdmitx_hpd_hw_op_txlx(enum hpd_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 21, 1);
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = !!(hd_read_reg(P_PERIPHS_PIN_MUX_0) & (1 << 23));
		break;
	case HPD_MUX_HPD:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 21, 1);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_0, 1, 23, 1);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_0, 0, 23, 1);
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 21, 1);
		break;
	case HPD_READ_HPD_GPIO:
		ret = !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 21));
		break;
	default:
		pr_info("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio_txlx(void)
{
	return !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 21));
}

int hdmitx_ddc_hw_op_txlx(enum ddc_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
		hd_set_reg_bits(P_PAD_PULL_UP_EN_REG1, 0, 22, 2);
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 22, 2);
		break;
	case DDC_MUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 22, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_0, 3, 21, 2);
		break;
	case DDC_UNMUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 22, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_0, 0, 21, 2);
		break;
	default:
		pr_info("error ddc cmd %d\n", cmd);
	}
	return ret;
}
