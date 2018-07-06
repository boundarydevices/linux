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

static const char * const spicc_parent_names[] = { "xtal",
	"clk81", "fclk_div4", "fclk_div3", "", "fclk_div5",
	"fclk_div7", ""};

/* spicc clk */
static struct clk_mux g12a_spicc0_mux = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.mask = 0x7,
	.shift = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_mux",
		.ops = &clk_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider g12a_spicc0_div = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.shift = 0,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate g12a_spicc0_gate = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.bit_idx = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux g12a_spicc1_mux = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.mask = 0x7,
	.shift = 23,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_mux",
		.ops = &clk_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider g12a_spicc1_div = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.shift = 16,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc_p1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate g12a_spicc1_gate = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.bit_idx = 22,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc_p1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};


void meson_g12a_misc_init(void)
{
	/* Populate base address for reg */
	pr_info("%s: register amlogic g12a misc clks\n", __func__);

	g12a_ts_clk_div.reg = clk_base + (unsigned long)(g12a_ts_clk_div.reg);
	g12a_ts_clk_gate.reg = clk_base + (unsigned long)(g12a_ts_clk_gate.reg);

	g12a_spicc0_mux.reg = clk_base + (unsigned long)(g12a_spicc0_mux.reg);
	g12a_spicc0_div.reg = clk_base + (unsigned long)(g12a_spicc0_div.reg);
	g12a_spicc0_gate.reg = clk_base + (unsigned long)(g12a_spicc0_gate.reg);
	g12a_spicc1_mux.reg = clk_base + (unsigned long)(g12a_spicc1_mux.reg);
	g12a_spicc1_div.reg = clk_base + (unsigned long)(g12a_spicc1_div.reg);
	g12a_spicc1_gate.reg = clk_base + (unsigned long)(g12a_spicc1_gate.reg);

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

	clks[CLKID_SPICC0_COMP] = clk_register_composite(NULL,
		"spicc0_comp",
	    spicc_parent_names, 8,
	    &g12a_spicc0_mux.hw,
	    &clk_mux_ops,
	    &g12a_spicc0_div.hw,
	    &clk_divider_ops,
	    &g12a_spicc0_gate.hw,
	    &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SPICC0_COMP]))
		panic("%s: %d clk_register_composite spicc0_comp error\n",
			__func__, __LINE__);

	clks[CLKID_SPICC1_COMP] = clk_register_composite(NULL,
		"spicc1_comp",
		spicc_parent_names, 8,
		&g12a_spicc1_mux.hw,
		&clk_mux_ops,
		&g12a_spicc1_div.hw,
		&clk_divider_ops,
		&g12a_spicc1_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SPICC1_COMP]))
		panic("%s: %d clk_register_composite spicc1_comp error\n",
			__func__, __LINE__);

	pr_info("%s: done.\n", __func__);
}
