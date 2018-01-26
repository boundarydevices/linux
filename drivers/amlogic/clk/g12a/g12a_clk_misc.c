/*
 * drivers/amlogic/clk/g12a/g12a_clk_misc.c
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

static const char * const ts_parent_names[] = { "xtal" };

static struct clk_divider g12a_ts_clk_div = {
	.reg = (void *)HHI_TS_CLK_CNTL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate g12a_ts_clk_gate = {
	.reg = (void *)HHI_TS_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "ts_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

void meson_g12a_misc_init(void)
{
	/* Populate base address for reg */
	pr_info("%s: register amlogic g12a misc clks\n", __func__);

	g12a_ts_clk_div.reg = clk_base + (u64)(g12a_ts_clk_div.reg);
	g12a_ts_clk_gate.reg = clk_base + (u64)(g12a_ts_clk_gate.reg);

	clks[CLKID_TS_COMP] = clk_register_composite(NULL,
		"ts_comp",
	    ts_parent_names, 1,
	    NULL,
	    NULL,
	    &g12a_ts_clk_div.hw,
	    &clk_divider_ops,
	   &g12a_ts_clk_gate.hw,
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_TS_COMP]))
		panic("%s: %d clk_register_composite ts_comp error\n",
			__func__, __LINE__);

	pr_info("%s: done.\n", __func__);
}
