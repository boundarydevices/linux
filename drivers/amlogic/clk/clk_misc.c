/*
 * drivers/amlogic/clk/clk_misc.c
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
#include <dt-bindings/clock/amlogic,gxl-clkc.h>

#include "clkc.h"
#include "gxl.h"

const char *meas_parent_names[] = { "xtal", "fclk_div4",
	"fclk_div3", "fclk_div5", "vid_pll_clk", "vid2_pll_clk"};

/* cts_vdin_meas_clk */
static struct clk_mux vdin_meas_mux = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_mux",
		.ops = &clk_mux_ops,
		.parent_names = meas_parent_names,
		.num_parents = 6,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vdin_meas_div = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdin_meas_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vdin_meas_gate = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdin_meas_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *vdin_meas_clk_hws[] = {
[CLKID_VDIN_MEAS_MUX - CLKID_VDIN_MEAS_MUX] = &vdin_meas_mux.hw,
[CLKID_VDIN_MEAS_DIV - CLKID_VDIN_MEAS_MUX] = &vdin_meas_div.hw,
[CLKID_VDIN_MEAS_GATE - CLKID_VDIN_MEAS_MUX] = &vdin_meas_gate.hw,
};

void amlogic_init_misc(void)
{
	/* cts_vdin_meas_clk */
	vdin_meas_mux.reg = clk_base + (u64)(vdin_meas_mux.reg);
	vdin_meas_div.reg = clk_base + (u64)(vdin_meas_div.reg);
	vdin_meas_gate.reg = clk_base + (u64)(vdin_meas_gate.reg);

	clks[CLKID_VDIN_MEAS_COMP] = clk_register_composite(NULL,
		"vdin_meas_composite",
		meas_parent_names, 6,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_MUX - CLKID_VDIN_MEAS_MUX],
		&clk_mux_ops,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_DIV - CLKID_VDIN_MEAS_MUX],
		&clk_divider_ops,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_GATE - CLKID_VDIN_MEAS_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDIN_MEAS_COMP]))
		pr_err("%s: %d clk_register_composite vdin_meas_composite error\n",
		__func__, __LINE__);

	pr_info("%s: register meson misc clk\n", __func__);
};

