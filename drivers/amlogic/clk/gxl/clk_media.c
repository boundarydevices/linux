/*
 * drivers/amlogic/clk/gxl/clk_media.c
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

const char *dec_parent_names[] = { "fclk_div4", "fclk_div3", "fclk_div5",
	"fclk_div7", "mpll1", "mpll2", "gp0_pll", "xtal"};

/* cts_vdec_clk */
static struct clk_mux vdec_p0_mux = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vdec_p0_div = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdec_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vdec_p0_gate = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdec_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vdec_p1_mux = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vdec_p1_div = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdec_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vdec_p1_gate = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdec_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux vdec_mux = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.mask = 0x1,
	.shift = 15,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vdec_p0_composite",
			"vdec_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *vdec_clk_hws[] = {
	[CLKID_VDEC_P0_MUX - CLKID_VDEC_P0_MUX]		= &vdec_p0_mux.hw,
	[CLKID_VDEC_P0_DIV - CLKID_VDEC_P0_MUX]		= &vdec_p0_div.hw,
	[CLKID_VDEC_P0_GATE - CLKID_VDEC_P0_MUX]	= &vdec_p0_gate.hw,
	[CLKID_VDEC_P1_MUX - CLKID_VDEC_P0_MUX]		= &vdec_p1_mux.hw,
	[CLKID_VDEC_P1_DIV - CLKID_VDEC_P0_MUX]		= &vdec_p1_div.hw,
	[CLKID_VDEC_P1_GATE - CLKID_VDEC_P0_MUX]	= &vdec_p1_gate.hw,
	[CLKID_VDEC_MUX - CLKID_VDEC_P0_MUX]		  = &vdec_mux.hw,
};

/* cts_hcodec_clk */
static struct clk_mux hcodec_p0_mux = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider hcodec_p0_div = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hcodec_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate hcodec_p0_gate = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hcodec_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux hcodec_p1_mux = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider hcodec_p1_div = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hcodec_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate hcodec_p1_gate = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hcodec_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux hcodec_mux = {
	.reg = (void *)HHI_VDEC3_CLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hcodec_p0_composite",
			"hcodec_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *hcodec_clk_hws[] = {
	[CLKID_HCODEC_P0_MUX - CLKID_HCODEC_P0_MUX]	= &hcodec_p0_mux.hw,
	[CLKID_HCODEC_P0_DIV - CLKID_HCODEC_P0_MUX]	= &hcodec_p0_div.hw,
	[CLKID_HCODEC_P0_GATE - CLKID_HCODEC_P0_MUX] = &hcodec_p0_gate.hw,
	[CLKID_HCODEC_P1_MUX - CLKID_HCODEC_P0_MUX]	= &hcodec_p1_mux.hw,
	[CLKID_HCODEC_P1_DIV - CLKID_HCODEC_P0_MUX]	= &hcodec_p1_div.hw,
	[CLKID_HCODEC_P1_GATE - CLKID_HCODEC_P0_MUX] = &hcodec_p1_gate.hw,
	[CLKID_HCODEC_MUX - CLKID_HCODEC_P0_MUX]	  = &hcodec_mux.hw,
};

/* cts_hevc_clk */
static struct clk_mux hevc_p0_mux = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider hevc_p0_div = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevc_p0_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate hevc_p0_gate = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevc_p0_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux hevc_p1_mux = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = dec_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider hevc_p1_div = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevc_p1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate hevc_p1_gate = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevc_p1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux hevc_mux = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hevc_p0_composite",
			"hevc_p1_composite"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_PARENT_ALTERNATE),
	},
};

static struct clk_hw *hevc_clk_hws[] = {
	[CLKID_HEVC_P0_MUX - CLKID_HEVC_P0_MUX]	= &hevc_p0_mux.hw,
	[CLKID_HEVC_P0_DIV - CLKID_HEVC_P0_MUX]	= &hevc_p0_div.hw,
	[CLKID_HEVC_P0_GATE - CLKID_HEVC_P0_MUX] = &hevc_p0_gate.hw,
	[CLKID_HEVC_P1_MUX - CLKID_HEVC_P0_MUX]	= &hevc_p1_mux.hw,
	[CLKID_HEVC_P1_DIV - CLKID_HEVC_P0_MUX]	= &hevc_p1_div.hw,
	[CLKID_HEVC_P1_GATE - CLKID_HEVC_P0_MUX] = &hevc_p1_gate.hw,
	[CLKID_HEVC_MUX - CLKID_HEVC_P0_MUX]	  = &hevc_mux.hw,
};

const char *vpu_parent_names[] = { "fclk_div4", "fclk_div3", "fclk_div5",
	"fclk_div7", "null", "null", "null",  "gp1_pll"};

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

/* cts_bt656_clk */
const char *bt656_parent_names[] = { "fclk_div2", "fclk_div3", "fclk_div5",
	"fclk_div7"};

static struct clk_mux bt656_clk1_mux = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "bt656_clk1_mux",
		.ops = &clk_mux_ops,
		.parent_names = bt656_parent_names,
		.num_parents = 4,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider bt656_clk1_div = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "bt656_clk1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "bt656_clk1_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate bt656_clk1_gate = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_clk1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "bt656_clk1_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *bt656_clk1_hws[] = {
	[CLKID_BT656_CLK1_MUX - CLKID_BT656_CLK1_MUX]  = &bt656_clk1_mux.hw,
	[CLKID_BT656_CLK1_DIV - CLKID_BT656_CLK1_MUX]  = &bt656_clk1_div.hw,
	[CLKID_BT656_CLK1_GATE - CLKID_BT656_CLK1_MUX] = &bt656_clk1_gate.hw,
};

void amlogic_init_media(void)
{
	/* cts_vdec_clk */
	vdec_p0_mux.reg = clk_base + (unsigned long)(vdec_p0_mux.reg);
	vdec_p0_div.reg = clk_base + (unsigned long)(vdec_p0_div.reg);
	vdec_p0_gate.reg = clk_base + (unsigned long)(vdec_p0_gate.reg);
	vdec_p1_mux.reg = clk_base + (unsigned long)(vdec_p1_mux.reg);
	vdec_p1_div.reg = clk_base + (unsigned long)(vdec_p1_div.reg);
	vdec_p1_gate.reg = clk_base + (unsigned long)(vdec_p1_gate.reg);
	vdec_mux.reg = clk_base + (unsigned long)(vdec_mux.reg);
	/* cts_hcodec_clk */
	hcodec_p0_mux.reg = clk_base + (unsigned long)(hcodec_p0_mux.reg);
	hcodec_p0_div.reg = clk_base + (unsigned long)(hcodec_p0_div.reg);
	hcodec_p0_gate.reg = clk_base + (unsigned long)(hcodec_p0_gate.reg);
	hcodec_p1_mux.reg = clk_base + (unsigned long)(hcodec_p1_mux.reg);
	hcodec_p1_div.reg = clk_base + (unsigned long)(hcodec_p1_div.reg);
	hcodec_p1_gate.reg = clk_base + (unsigned long)(hcodec_p1_gate.reg);
	hcodec_mux.reg = clk_base + (unsigned long)(hcodec_mux.reg);
	/* cts_hevc_clk */
	hevc_p0_mux.reg = clk_base + (unsigned long)(hevc_p0_mux.reg);
	hevc_p0_div.reg = clk_base + (unsigned long)(hevc_p0_div.reg);
	hevc_p0_gate.reg = clk_base + (unsigned long)(hevc_p0_gate.reg);
	hevc_p1_mux.reg = clk_base + (unsigned long)(hevc_p1_mux.reg);
	hevc_p1_div.reg = clk_base + (unsigned long)(hevc_p1_div.reg);
	hevc_p1_gate.reg = clk_base + (unsigned long)(hevc_p1_gate.reg);
	hevc_mux.reg = clk_base + (unsigned long)(hevc_mux.reg);
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
	/* cts_bt656_clk1 */
	bt656_clk1_mux.reg = clk_base + (unsigned long)(bt656_clk1_mux.reg);
	bt656_clk1_div.reg = clk_base + (unsigned long)(bt656_clk1_div.reg);
	bt656_clk1_gate.reg = clk_base + (unsigned long)(bt656_clk1_gate.reg);

	/* cts_vdec_clk */
	clks[CLKID_VDEC_P0_COMP] = clk_register_composite(NULL,
		"vdec_p0_composite",
		dec_parent_names, 8,
		vdec_clk_hws[CLKID_VDEC_P0_MUX - CLKID_VDEC_P0_MUX],
		&clk_mux_ops,
		vdec_clk_hws[CLKID_VDEC_P0_DIV - CLKID_VDEC_P0_MUX],
		&clk_divider_ops,
		vdec_clk_hws[CLKID_VDEC_P0_GATE - CLKID_VDEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDEC_P0_COMP]))
		pr_err("%s: %d clk_register_composite vdec_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VDEC_P1_COMP] = clk_register_composite(NULL,
		"vdec_p1_composite",
		dec_parent_names, 8,
		vdec_clk_hws[CLKID_VDEC_P1_MUX - CLKID_VDEC_P0_MUX],
		&clk_mux_ops,
		vdec_clk_hws[CLKID_VDEC_P1_DIV - CLKID_VDEC_P0_MUX],
		&clk_divider_ops,
		vdec_clk_hws[CLKID_VDEC_P1_GATE - CLKID_VDEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDEC_P1_COMP]))
		pr_err("%s: %d clk_register_composite vdec_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VDEC_MUX] = clk_register(NULL,
		vdec_clk_hws[CLKID_VDEC_MUX - CLKID_VDEC_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_VDEC_MUX]));

	/* cts_hcodec_clk */
	clks[CLKID_HCODEC_P0_COMP] = clk_register_composite(NULL,
		"hcodec_p0_composite",
		dec_parent_names, 8,
		hcodec_clk_hws[CLKID_HCODEC_P0_MUX - CLKID_HCODEC_P0_MUX],
		&clk_mux_ops,
		hcodec_clk_hws[CLKID_HCODEC_P0_DIV - CLKID_HCODEC_P0_MUX],
		&clk_divider_ops,
		hcodec_clk_hws[CLKID_HCODEC_P0_GATE - CLKID_HCODEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HCODEC_P0_COMP]))
		pr_err("%s: %d clk_register_composite hcodec_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_HCODEC_P1_COMP] = clk_register_composite(NULL,
		"hcodec_p1_composite",
		dec_parent_names, 8,
		hcodec_clk_hws[CLKID_HCODEC_P1_MUX - CLKID_HCODEC_P0_MUX],
		&clk_mux_ops,
		hcodec_clk_hws[CLKID_HCODEC_P1_DIV - CLKID_HCODEC_P0_MUX],
		&clk_divider_ops,
		hcodec_clk_hws[CLKID_HCODEC_P1_GATE - CLKID_HCODEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HCODEC_P1_COMP]))
		pr_err("%s: %d clk_register_composite hcodec_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_HCODEC_MUX] = clk_register(NULL,
		hcodec_clk_hws[CLKID_HCODEC_MUX - CLKID_HCODEC_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_HCODEC_MUX]));

	/* cts_hevc_clk */
	clks[CLKID_HEVC_P0_COMP] = clk_register_composite(NULL,
		"hevc_p0_composite",
		dec_parent_names, 8,
		hevc_clk_hws[CLKID_HEVC_P0_MUX - CLKID_HEVC_P0_MUX],
		&clk_mux_ops,
		hevc_clk_hws[CLKID_HEVC_P0_DIV - CLKID_HEVC_P0_MUX],
		&clk_divider_ops,
		hevc_clk_hws[CLKID_HEVC_P0_GATE - CLKID_HEVC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVC_P0_COMP]))
		pr_err("%s: %d clk_register_composite hevc_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_HEVC_P1_COMP] = clk_register_composite(NULL,
		"hevc_p1_composite",
		dec_parent_names, 8,
		hevc_clk_hws[CLKID_HEVC_P1_MUX - CLKID_HEVC_P0_MUX],
		&clk_mux_ops,
		hevc_clk_hws[CLKID_HEVC_P1_DIV - CLKID_HEVC_P0_MUX],
		&clk_divider_ops,
		hevc_clk_hws[CLKID_HEVC_P1_GATE - CLKID_HEVC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVC_P1_COMP]))
		pr_err("%s: %d clk_register_composite hevc_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_HEVC_MUX] = clk_register(NULL,
		hevc_clk_hws[CLKID_HEVC_MUX - CLKID_HEVC_P0_MUX]);
	WARN_ON(IS_ERR(clks[CLKID_HEVC_MUX]));

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

	/* cts_bt656_clk1 */
	clks[CLKID_BT656_CLK1_COMP] = clk_register_composite(NULL,
		"bt656_clk1_composite",
		bt656_parent_names, 4,
		bt656_clk1_hws[CLKID_BT656_CLK1_MUX - CLKID_BT656_CLK1_MUX],
		&clk_mux_ops,
		bt656_clk1_hws[CLKID_BT656_CLK1_DIV - CLKID_BT656_CLK1_MUX],
		&clk_divider_ops,
		bt656_clk1_hws[CLKID_BT656_CLK1_GATE - CLKID_BT656_CLK1_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_BT656_CLK1_COMP]))
		pr_err("%s: %d clk_register_composite bt656_clk1_composite error\n",
		__func__, __LINE__);

	pr_info("%s: register meson media clk\n", __func__);
}

