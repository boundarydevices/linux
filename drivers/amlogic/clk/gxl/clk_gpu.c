/*
 * drivers/amlogic/clk/gxl/clk_gpu.c
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

#include "../clkc.h"
#include "gxl.h"

const char *gpu_parent_names[] = { "xtal", "gp0_pll", "mpll2", "mpll1",
	"fclk_div7", "fclk_div4", "fclk_div3", "fclk_div5"};

static struct clk_mux gpu_p0_mux = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = gpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider gpu_p0_div = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gpu_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate gpu_p0_gate = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gpu_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux gpu_p1_mux = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = gpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider gpu_p1_div = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gpu_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate gpu_p1_gate = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gpu_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux gpu_mux = {
	.reg = (void *)HHI_MALI_CLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "gpu_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "gpu_p0_composite",
			"gpu_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *gpu_clk_hws[] = {
	[CLKID_GPU_P0_MUX - CLKID_GPU_P0_MUX]		    = &gpu_p0_mux.hw,
	[CLKID_GPU_P0_DIV - CLKID_GPU_P0_MUX]		    = &gpu_p0_div.hw,
	[CLKID_GPU_P0_GATE - CLKID_GPU_P0_MUX]		    = &gpu_p0_gate.hw,
	[CLKID_GPU_P1_MUX - CLKID_GPU_P0_MUX]		    = &gpu_p1_mux.hw,
	[CLKID_GPU_P1_DIV - CLKID_GPU_P0_MUX]		    = &gpu_p1_div.hw,
	[CLKID_GPU_P1_GATE - CLKID_GPU_P0_MUX]		    = &gpu_p1_gate.hw,
	[CLKID_GPU_MUX - CLKID_GPU_P0_MUX]		    = &gpu_mux.hw,
};

void amlogic_init_gpu(void)
{
	gpu_p0_mux.reg = clk_base + (unsigned long)(gpu_p0_mux.reg);
	gpu_p0_div.reg = clk_base + (unsigned long)(gpu_p0_div.reg);
	gpu_p0_gate.reg = clk_base + (unsigned long)(gpu_p0_gate.reg);
	gpu_p1_mux.reg = clk_base + (unsigned long)(gpu_p1_mux.reg);
	gpu_p1_div.reg = clk_base + (unsigned long)(gpu_p1_div.reg);
	gpu_p1_gate.reg = clk_base + (unsigned long)(gpu_p1_gate.reg);
	gpu_mux.reg = clk_base + (unsigned long)(gpu_mux.reg);

	clks[CLKID_GPU_P0_COMP] = clk_register_composite(NULL,
		"gpu_p0_composite",
		gpu_parent_names, 8,
		gpu_clk_hws[CLKID_GPU_P0_MUX - CLKID_GPU_P0_MUX],
		&clk_mux_ops,
		gpu_clk_hws[CLKID_GPU_P0_DIV - CLKID_GPU_P0_MUX],
		&clk_divider_ops,
		gpu_clk_hws[CLKID_GPU_P0_GATE - CLKID_GPU_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_GPU_P0_COMP]))
		pr_err("%s: %d clk_register_composite gpu_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_GPU_P1_COMP] = clk_register_composite(NULL,
		"gpu_p1_composite",
		gpu_parent_names, 8,
		gpu_clk_hws[CLKID_GPU_P1_MUX - CLKID_GPU_P0_MUX],
		&clk_mux_ops,
		gpu_clk_hws[CLKID_GPU_P1_DIV - CLKID_GPU_P0_MUX],
		&clk_divider_ops,
		gpu_clk_hws[CLKID_GPU_P1_GATE - CLKID_GPU_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_GPU_P1_COMP]))
		pr_err("%s: %d clk_register_composite gpu_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_GPU_MUX] = clk_register(NULL,
		gpu_clk_hws[CLKID_GPU_MUX - CLKID_GPU_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_GPU_MUX]));

	clk_set_parent(clks[CLKID_GPU_MUX], clks[CLKID_GPU_P0_COMP]);
	clk_prepare_enable(clks[CLKID_GPU_P0_COMP]);
	clk_prepare_enable(clks[CLKID_GPU_P1_COMP]);
	clk_set_rate(clks[CLKID_GPU_MUX], 400000000);

	pr_info("%s: register meson gpu clk\n", __func__);
}

