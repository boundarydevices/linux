/*
 * drivers/amlogic/clk/axg/axg_clk_media.c
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

/* cts_dsi_meas_clk */ /*MIPI_HOST*/
static const char * const meas_parent_names[] = { "xtal", "fclk_div4",
	"fclk_div3", "fclk_div5", "null", "null", "fclk_dvi2", "fclk_div7"};

/* cts_dsi_meas_clk */
static struct clk_mux dsi_meas_mux = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.mask = 0x7,
	.shift = 21,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas_mux",
		.ops = &clk_mux_ops,
		.parent_names = meas_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider dsi_meas_div = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.shift = 12,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "dsi_meas_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate dsi_meas_gate = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.bit_idx = 20,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "dsi_meas_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *dsi_meas_clk_hws[] = {
[CLKID_DSI_MEAS_MUX - CLKID_DSI_MEAS_MUX] = &dsi_meas_mux.hw,
[CLKID_DSI_MEAS_DIV - CLKID_DSI_MEAS_MUX] = &dsi_meas_div.hw,
[CLKID_DSI_MEAS_GATE - CLKID_DSI_MEAS_MUX] = &dsi_meas_gate.hw,
};

static const char * const vpu_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "null", "null",
	"null",  "null"};

/* cts_vpu_clk */
static struct clk_mux vpu_p0_mux = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vpu_p0_div = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vpu_p0_gate = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vpu_p1_mux = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vpu_p1_div = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vpu_p1_gate = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vpu_mux = {
	.reg = (void *)HHI_VPU_CLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vpu_p0_composite",
			"vpu_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *vpu_clk_hws[] = {
	[CLKID_VPU_P0_MUX - CLKID_VPU_P0_MUX]	= &vpu_p0_mux.hw,
	[CLKID_VPU_P0_DIV - CLKID_VPU_P0_MUX]	= &vpu_p0_div.hw,
	[CLKID_VPU_P0_GATE - CLKID_VPU_P0_MUX] = &vpu_p0_gate.hw,
	[CLKID_VPU_P1_MUX - CLKID_VPU_P0_MUX]	= &vpu_p1_mux.hw,
	[CLKID_VPU_P1_DIV - CLKID_VPU_P0_MUX]	= &vpu_p1_div.hw,
	[CLKID_VPU_P1_GATE - CLKID_VPU_P0_MUX] = &vpu_p1_gate.hw,
	[CLKID_VPU_MUX - CLKID_VPU_P0_MUX]	  = &vpu_mux.hw,
};

/* cts_vapbclk */
static struct clk_mux vapb_p0_mux = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vapb_p0_div = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vapb_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vapb_p0_gate = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vapb_p1_mux = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vapb_p1_div = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vapb_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vapb_p1_gate = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vapb_mux = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vapb_p0_composite",
			"vapb_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *vapb_clk_hws[] = {
	[CLKID_VPU_P0_MUX - CLKID_VPU_P0_MUX]	= &vapb_p0_mux.hw,
	[CLKID_VPU_P0_DIV - CLKID_VPU_P0_MUX]	= &vapb_p0_div.hw,
	[CLKID_VPU_P0_GATE - CLKID_VPU_P0_MUX] = &vapb_p0_gate.hw,
	[CLKID_VPU_P1_MUX - CLKID_VPU_P0_MUX]	= &vapb_p1_mux.hw,
	[CLKID_VPU_P1_DIV - CLKID_VPU_P0_MUX]	= &vapb_p1_div.hw,
	[CLKID_VPU_P1_GATE - CLKID_VPU_P0_MUX] = &vapb_p1_gate.hw,
	[CLKID_VPU_MUX - CLKID_VPU_P0_MUX]	  = &vapb_mux.hw,
};

static struct clk_gate ge2d_gate = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.bit_idx = 30,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vapb_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

void axg_amlogic_init_media(void)
{
	/* cts_dsi_meas_clk */
	dsi_meas_mux.reg = clk_base + (unsigned long)(dsi_meas_mux.reg);
	dsi_meas_div.reg = clk_base + (unsigned long)(dsi_meas_div.reg);
	dsi_meas_gate.reg = clk_base + (unsigned long)(dsi_meas_gate.reg);

	/* cts_vpu_clk */
	vpu_p0_mux.reg = clk_base + (unsigned long)(vpu_p0_mux.reg);
	vpu_p0_div.reg = clk_base + (unsigned long)(vpu_p0_div.reg);
	vpu_p0_gate.reg = clk_base + (unsigned long)(vpu_p0_gate.reg);
	vpu_p1_mux.reg = clk_base + (unsigned long)(vpu_p1_mux.reg);
	vpu_p1_div.reg = clk_base + (unsigned long)(vpu_p1_div.reg);
	vpu_p1_gate.reg = clk_base + (unsigned long)(vpu_p1_gate.reg);
	vpu_mux.reg = clk_base + (unsigned long)(vpu_mux.reg);
	/* cts_vapbclk */
	vapb_p0_mux.reg = clk_base + (unsigned long)(vapb_p0_mux.reg);
	vapb_p0_div.reg = clk_base + (unsigned long)(vapb_p0_div.reg);
	vapb_p0_gate.reg = clk_base + (unsigned long)(vapb_p0_gate.reg);
	vapb_p1_mux.reg = clk_base + (unsigned long)(vapb_p1_mux.reg);
	vapb_p1_div.reg = clk_base + (unsigned long)(vapb_p1_div.reg);
	vapb_p1_gate.reg = clk_base + (unsigned long)(vapb_p1_gate.reg);
	vapb_mux.reg = clk_base + (unsigned long)(vapb_mux.reg);
	/* cts_ge2d_clk */
	ge2d_gate.reg = clk_base + (unsigned long)(ge2d_gate.reg);

	clks[CLKID_DSI_MEAS_COMP] = clk_register_composite(NULL,
		"dsi_meas_composite",
		meas_parent_names, 6,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_MUX - CLKID_DSI_MEAS_MUX],
		&clk_mux_ops,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_DIV - CLKID_DSI_MEAS_MUX],
		&clk_divider_ops,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_GATE - CLKID_DSI_MEAS_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_DSI_MEAS_COMP]))
		pr_err("%s: %d clk_register_composite dsi_meas_composite error\n",
		__func__, __LINE__);

	/* cts_vpu_clk */
	clks[CLKID_VPU_P0_COMP] = clk_register_composite(NULL,
		"vpu_p0_composite",
		vpu_parent_names, 8,
		vpu_clk_hws[CLKID_VPU_P0_MUX - CLKID_VPU_P0_MUX],
		&clk_mux_ops,
		vpu_clk_hws[CLKID_VPU_P0_DIV - CLKID_VPU_P0_MUX],
		&clk_divider_ops,
		vpu_clk_hws[CLKID_VPU_P0_GATE - CLKID_VPU_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VPU_P0_COMP]))
		pr_err("%s: %d clk_register_composite vpu_p0_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VPU_P1_COMP] = clk_register_composite(NULL,
		"vpu_p1_composite",
		vpu_parent_names, 8,
		vpu_clk_hws[CLKID_VPU_P1_MUX - CLKID_VPU_P0_MUX],
		&clk_mux_ops,
		vpu_clk_hws[CLKID_VPU_P1_DIV - CLKID_VPU_P0_MUX],
		&clk_divider_ops,
		vpu_clk_hws[CLKID_VPU_P1_GATE - CLKID_VPU_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VPU_P1_COMP]))
		pr_err("%s: %d clk_register_composite vpu_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VPU_MUX] = clk_register(NULL,
		vpu_clk_hws[CLKID_VPU_MUX - CLKID_VPU_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_VPU_MUX]));
	clk_prepare_enable(clks[CLKID_VPU_MUX]);

	/* cts_vapb_clk */
	clks[CLKID_VAPB_P0_COMP] = clk_register_composite(NULL,
		"vapb_p0_composite",
		vpu_parent_names, 8,
		vapb_clk_hws[CLKID_VAPB_P0_MUX - CLKID_VAPB_P0_MUX],
		&clk_mux_ops,
		vapb_clk_hws[CLKID_VAPB_P0_DIV - CLKID_VAPB_P0_MUX],
		&clk_divider_ops,
		vapb_clk_hws[CLKID_VAPB_P0_GATE - CLKID_VAPB_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VAPB_P0_COMP]))
		pr_err("%s: %d clk_register_composite vapb_p0_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VAPB_P1_COMP] = clk_register_composite(NULL,
		"vapb_p1_composite",
		vpu_parent_names, 8,
		vapb_clk_hws[CLKID_VAPB_P1_MUX - CLKID_VAPB_P0_MUX],
		&clk_mux_ops,
		vapb_clk_hws[CLKID_VAPB_P1_DIV - CLKID_VAPB_P0_MUX],
		&clk_divider_ops,
		vapb_clk_hws[CLKID_VAPB_P1_GATE - CLKID_VAPB_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VAPB_P1_COMP]))
		pr_err("%s: %d clk_register_composite vapb_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VAPB_MUX] = clk_register(NULL,
		vapb_clk_hws[CLKID_VAPB_MUX - CLKID_VAPB_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_VAPB_MUX]));
	clk_prepare_enable(clks[CLKID_VAPB_MUX]);

	clks[CLKID_GE2D_GATE] = clk_register(NULL,
		&ge2d_gate.hw);
	WARN_ON(IS_ERR(clks[CLKID_GE2D_GATE]));

	pr_info("%s: register meson media clk\n", __func__);
}

