/*
 * drivers/amlogic/clk/tl1/tl1_clk_sdemmc.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <dt-bindings/clock/amlogic,tl1-clkc.h>

#include "../clkc.h"
#include "tl1.h"

PNAME(sd_emmc_parent_names) = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div5", "fclk_div2p5", "mpll2", "mpll3", "gp0_pll" };
/*sd_emmc B*/
static MUX(sd_emmc_p0_mux_B, HHI_SD_EMMC_CLK_CNTL, 0x7, 25,
sd_emmc_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sd_emmc_p0_div_B, HHI_SD_EMMC_CLK_CNTL, 16, 7, "sd_emmc_p0_mux_B",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(sd_emmc_p0_gate_B, HHI_SD_EMMC_CLK_CNTL, 23, "sd_emmc_p0_div_B",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/*sd_emmc C*/
static MUX(sd_emmc_p0_mux_C, HHI_NAND_CLK_CNTL, 0x7, 9,
sd_emmc_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(sd_emmc_p0_div_C, HHI_NAND_CLK_CNTL, 0, 7, "sd_emmc_p0_mux_C",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(sd_emmc_p0_gate_C, HHI_NAND_CLK_CNTL, 7, "sd_emmc_p0_div_C",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

static struct meson_composite sdemmc_comp[] = {
	{CLKID_SD_EMMC_B_P0_COMP, "sd_emmc_p0_B_comp",
	sd_emmc_parent_names, ARRAY_SIZE(sd_emmc_parent_names),
	&sd_emmc_p0_mux_B.hw, &sd_emmc_p0_div_B.hw,
	&sd_emmc_p0_gate_B.hw, 0
	},/* sd_emmc_B */

	{CLKID_SD_EMMC_C_P0_COMP, "sd_emmc_p0_C_comp",
	sd_emmc_parent_names, ARRAY_SIZE(sd_emmc_parent_names),
	&sd_emmc_p0_mux_C.hw, &sd_emmc_p0_div_C.hw,
	&sd_emmc_p0_gate_C.hw, 0
	},/* sd_emmc_C */

	{},
};

void meson_tl1_sdemmc_init(void)
{
	int length = ARRAY_SIZE(sdemmc_comp);

	/* Populate base address for reg */
	sd_emmc_p0_mux_B.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_mux_B.reg);
	sd_emmc_p0_div_B.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_div_B.reg);
	sd_emmc_p0_gate_B.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_gate_B.reg);
	sd_emmc_p0_mux_C.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_mux_C.reg);
	sd_emmc_p0_div_C.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_div_C.reg);
	sd_emmc_p0_gate_C.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_gate_C.reg);

	meson_clk_register_composite(clks, sdemmc_comp, length - 1);
}
