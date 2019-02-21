/*
 * drivers/amlogic/clk/g12a/g12a_clk_sdemmc.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <dt-bindings/clock/amlogic,g12a-clkc.h>

#include "../clkc.h"
#include "g12a.h"

static const char * const sd_emmc_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div5", "fclk_div2p5", "mpll2", "mpll3", "gp0_pll" };

static struct clk_mux sd_emmc_p0_mux_A = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_mux_A",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sd_emmc_p0_div_A = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_div_A",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_mux_A" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate sd_emmc_p0_gate_A = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_p0_gate_A",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_div_A" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux sd_emmc_p0_mux_B = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_mux_B",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sd_emmc_p0_div_B = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_div_B",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_mux_B" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate sd_emmc_p0_gate_B = {
	.reg = (void *)HHI_SD_EMMC_CLK_CNTL,
	.bit_idx = 23,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_p0_gate_B",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_div_B" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux sd_emmc_p0_mux_C = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_mux_C",
		.ops = &clk_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider sd_emmc_p0_div_C = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_p0_div_C",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_mux_C" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate sd_emmc_p0_gate_C = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_p0_gate_C",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "sd_emmc_p0_div_C" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *sd_emmc_clk_hws[] = {
	[CLKID_SD_EMMC_A_P0_MUX - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_mux_A.hw,
	[CLKID_SD_EMMC_A_P0_DIV - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_div_A.hw,
	[CLKID_SD_EMMC_A_P0_GATE - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_gate_A.hw,
	[CLKID_SD_EMMC_B_P0_MUX - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_mux_B.hw,
	[CLKID_SD_EMMC_B_P0_DIV - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_div_B.hw,
	[CLKID_SD_EMMC_B_P0_GATE - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_gate_B.hw,
	[CLKID_SD_EMMC_C_P0_MUX - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_mux_C.hw,
	[CLKID_SD_EMMC_C_P0_DIV - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_div_C.hw,
	[CLKID_SD_EMMC_C_P0_GATE - CLKID_SD_EMMC_A_P0_MUX]
		= &sd_emmc_p0_gate_C.hw,
};


void meson_g12a_sdemmc_init(void)
{
	/* Populate base address for reg */
	pr_info("%s: register amlogic sdemmc clk\n", __func__);

	sd_emmc_p0_mux_A.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_mux_A.reg);
	sd_emmc_p0_div_A.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_div_A.reg);
	sd_emmc_p0_gate_A.reg = clk_base
				+ (unsigned long)(sd_emmc_p0_gate_A.reg);
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

	clks[CLKID_SD_EMMC_A_P0_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_A_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_P0_MUX - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_P0_DIV - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_A_P0_GATE - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SD_EMMC_A_P0_COMP]))
		pr_err("%s: %d clk_register_composite sd_emmc_p0_A_comp error\n",
			__func__, __LINE__);

	clks[CLKID_SD_EMMC_B_P0_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_B_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_P0_MUX - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_P0_DIV - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_B_P0_GATE - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SD_EMMC_B_P0_COMP]))
		pr_err("%s: %d clk_register_composite sd_emmc_p0_B_comp error\n",
			__func__, __LINE__);

	clks[CLKID_SD_EMMC_C_P0_COMP] = clk_register_composite(NULL,
		"sd_emmc_p0_C_comp",
	    sd_emmc_parent_names, 8,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_C_P0_MUX - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_mux_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_C_P0_DIV - CLKID_SD_EMMC_A_P0_MUX],
	    &clk_divider_ops,
	    sd_emmc_clk_hws[CLKID_SD_EMMC_C_P0_GATE - CLKID_SD_EMMC_A_P0_MUX],
	     &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SD_EMMC_C_P0_COMP]))
		pr_err("%s: %d clk_register_composite sd_emmc_p0_C_comp error\n",
			__func__, __LINE__);

	pr_info("%s: register amlogic sdemmc clk\n", __func__);
}
