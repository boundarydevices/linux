/*
 * drivers/amlogic/clk/axg/axg_clk_misc.c
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
#include <dt-bindings/clock/amlogic,axg-clkc.h>

#include "../clkc.h"
#include "axg.h"

static const char * const spicc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7" };

static struct clk_mux spicc_mux = {
	.reg = (void *)HHI_SPICC_HCLK_CNTL,
	.mask = 0x3,
	.shift = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc_mux",
		.ops = &clk_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = 4,
		.flags = (CLK_MUX_ROUND_CLOSEST),
	},
};

static struct clk_divider spicc_div = {
	.reg = (void *)HHI_SPICC_HCLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc_mux" },
		.num_parents = 1,
		.flags = (CLK_DIVIDER_ROUND_CLOSEST),
	},
};

static struct clk_gate spicc_gate = {
	.reg = (void *)HHI_SPICC_HCLK_CNTL,
	.bit_idx = 15,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc_div" },
		.num_parents = 1,
		.flags = 0,
	},
};

static struct clk_hw *misc_clk_hws[] = {
	[CLKID_SPICC_MUX - CLKID_SPICC_MUX]
		= &spicc_mux.hw,
	[CLKID_SPICC_DIV - CLKID_SPICC_MUX]
		= &spicc_div.hw,
	[CLKID_SPICC_GATE - CLKID_SPICC_MUX]
		= &spicc_gate.hw,
};

void axg_amlogic_init_misc(void)
{
	/* Populate base address for reg */
	pr_info("%s: register amlogic axg misc clks\n", __func__);

	spicc_mux.reg = clk_base + (unsigned long)(spicc_mux.reg);
	spicc_div.reg = clk_base + (unsigned long)(spicc_div.reg);
	spicc_gate.reg = clk_base + (unsigned long)(spicc_gate.reg);

	clks[CLKID_SPICC_COMP] = clk_register_composite(NULL,
		"spicc_comp",
	    spicc_parent_names, 4,
	    misc_clk_hws[CLKID_SPICC_MUX - CLKID_SPICC_MUX],
	    &clk_mux_ops,
	    misc_clk_hws[CLKID_SPICC_DIV - CLKID_SPICC_MUX],
	    &clk_divider_ops,
	    misc_clk_hws[CLKID_SPICC_GATE - CLKID_SPICC_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SPICC_COMP]))
		pr_err("%s: %d clk_register_composite spicc_comp error\n",
			__func__, __LINE__);

	pr_info("%s: register amlogic sdemmc clk\n", __func__);
}
