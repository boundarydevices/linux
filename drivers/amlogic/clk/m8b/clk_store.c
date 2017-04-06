/*
 * drivers/amlogic/clk/m8b/clk_store.c
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
#include <dt-bindings/clock/meson8b-clkc.h>

#include "clkc.h"
#include "meson8b.h"

/* cts_nand_core_clk */
const char *nand_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "xtal", "mpll1"};

static struct clk_mux nand_core_mux = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "nand_core_mux",
		.ops = &clk_mux_ops,
		.parent_names = nand_parent_names,
		.num_parents = 6,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider nand_core_div = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "nand_core_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "nand_core_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate nand_core_gate = {
	.reg = (void *)HHI_NAND_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "nand_core_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "nand_core_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *nand_core_clk_hws[] = {
[CLKID_NAND_CORE_MUX - CLKID_NAND_CORE_MUX] = &nand_core_mux.hw,
[CLKID_NAND_CORE_DIV - CLKID_NAND_CORE_MUX] = &nand_core_div.hw,
[CLKID_NAND_CORE_GATE - CLKID_NAND_CORE_MUX] = &nand_core_gate.hw,
};

void amlogic_init_store(void)
{
	/* Populate base address for reg */
	nand_core_mux.reg = clk_base + (u32)(nand_core_mux.reg);
	nand_core_div.reg = clk_base + (u32)(nand_core_div.reg);
	nand_core_gate.reg = clk_base + (u32)(nand_core_gate.reg);

	clks[CLKID_NAND_CORE_COMP] = clk_register_composite(NULL,
		"nand_core_comp",
		nand_parent_names, 6,
	    nand_core_clk_hws[CLKID_NAND_CORE_MUX - CLKID_NAND_CORE_MUX],
	    &clk_mux_ops,
	    nand_core_clk_hws[CLKID_NAND_CORE_DIV - CLKID_NAND_CORE_MUX],
	    &clk_divider_ops,
	    nand_core_clk_hws[CLKID_NAND_CORE_GATE - CLKID_NAND_CORE_MUX],
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_NAND_CORE_COMP]))
		pr_err("%s: %d clk_register_composite nand_core_comp error\n",
			__func__, __LINE__);

	pr_info("%s: register amlogic store clk\n", __func__);
}
