/*
 * drivers/amlogic/clk/g12a/g12a_clk_media.c
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

/* cts_dsi_meas_clk */ /*MIPI_HOST*/
const char *g12a_meas_parent_names[] = { "xtal", "fclk_div4",
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
		.parent_names = g12a_meas_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *dsi_meas_clk_hws[] = {
	[CLKID_DSI_MEAS_MUX - CLKID_DSI_MEAS_MUX] = &dsi_meas_mux.hw,
	[CLKID_DSI_MEAS_DIV - CLKID_DSI_MEAS_MUX] = &dsi_meas_div.hw,
	[CLKID_DSI_MEAS_GATE - CLKID_DSI_MEAS_MUX] = &dsi_meas_gate.hw,
};

const char *g12a_dec_parent_names[] = { "fclk_div2p5", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "hifi_pll", "gp0_pll", "xtal"};

/* cts_vdec_clk */
static struct clk_mux vdec_p0_mux = {
	.reg = (void *)HHI_VDEC_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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

/* cts_hevcb_clk */
static struct clk_mux hevc_p0_mux = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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

/* cts_hevcf_clk */
static struct clk_mux hevcf_p0_mux = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider hevcf_p0_div = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcf_p0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate hevcf_p0_gate = {
	.reg = (void *)HHI_VDEC2_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcf_p0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux hevcf_p1_mux = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = g12a_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider hevcf_p1_div = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "hevcf_p1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate hevcf_p1_gate = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "hevcf_p1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux hevcf_mux = {
	.reg = (void *)HHI_VDEC4_CLK_CNTL,
	.mask = 0x1,
	.shift = 15,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "hevcf_p0_composite",
			"hevcf_p1_composite"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *hevcf_clk_hws[] = {
	[CLKID_HEVCF_P0_MUX - CLKID_HEVCF_P0_MUX]   = &hevcf_p0_mux.hw,
	[CLKID_HEVCF_P0_DIV - CLKID_HEVCF_P0_MUX]   = &hevcf_p0_div.hw,
	[CLKID_HEVCF_P0_GATE - CLKID_HEVCF_P0_MUX]  = &hevcf_p0_gate.hw,
	[CLKID_HEVCF_P1_MUX - CLKID_HEVCF_P0_MUX]   = &hevcf_p1_mux.hw,
	[CLKID_HEVCF_P1_DIV - CLKID_HEVCF_P0_MUX]   = &hevcf_p1_div.hw,
	[CLKID_HEVCF_P1_GATE - CLKID_HEVCF_P0_MUX]  = &hevcf_p1_gate.hw,
	[CLKID_HEVCF_MUX - CLKID_HEVCF_P0_MUX]      = &hevcf_mux.hw,
};

static const char * const vpu_parent_names[] = { "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "null", "null",
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *vpu_clk_hws[] = {
	[CLKID_VPU_P0_MUX - CLKID_VPU_P0_MUX]   = &vpu_p0_mux.hw,
	[CLKID_VPU_P0_DIV - CLKID_VPU_P0_MUX]   = &vpu_p0_div.hw,
	[CLKID_VPU_P0_GATE - CLKID_VPU_P0_MUX]  = &vpu_p0_gate.hw,
	[CLKID_VPU_P1_MUX - CLKID_VPU_P0_MUX]   = &vpu_p1_mux.hw,
	[CLKID_VPU_P1_DIV - CLKID_VPU_P0_MUX]   = &vpu_p1_div.hw,
	[CLKID_VPU_P1_GATE - CLKID_VPU_P0_MUX]  = &vpu_p1_gate.hw,
	[CLKID_VPU_MUX - CLKID_VPU_P0_MUX]      = &vpu_mux.hw,
};

static const char * const vapb_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "null",
	"mpll2",  "fclk_div2p5"};

/* cts_vapbclk */
static struct clk_mux vapb_p0_mux = {
	.reg = (void *)HHI_VAPBCLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = vapb_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.parent_names = vapb_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
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
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *vapb_clk_hws[] = {
	[CLKID_VAPB_P0_MUX - CLKID_VAPB_P0_MUX]   = &vapb_p0_mux.hw,
	[CLKID_VAPB_P0_DIV - CLKID_VAPB_P0_MUX]   = &vapb_p0_div.hw,
	[CLKID_VAPB_P0_GATE - CLKID_VAPB_P0_MUX]  = &vapb_p0_gate.hw,
	[CLKID_VAPB_P1_MUX - CLKID_VAPB_P0_MUX]   = &vapb_p1_mux.hw,
	[CLKID_VAPB_P1_DIV - CLKID_VAPB_P0_MUX]   = &vapb_p1_div.hw,
	[CLKID_VAPB_P1_GATE - CLKID_VAPB_P0_MUX]  = &vapb_p1_gate.hw,
	[CLKID_VAPB_MUX - CLKID_VAPB_P0_MUX]      = &vapb_mux.hw,
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
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const vpu_clkb_tmp_parent_names[] = { "vpu_mux",
	"fclk_div4", "fclk_div5", "fclk_div7"};

static struct clk_mux vpu_clkb_tmp_mux = {
	.reg = (void *)HHI_VPU_CLKB_CNTL,
	.mask = 0x3,
	.shift = 20,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_clkb_tmp_parent_names,
		.num_parents = ARRAY_SIZE(vpu_clkb_tmp_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider vpu_clkb_tmp_div = {
	.reg = (void *)HHI_VPU_CLKB_CNTL,
	.shift = 16,
	.width = 4,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate vpu_clkb_tmp_gate = {
	.reg = (void *)HHI_VPU_CLKB_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const vpu_clkb_parent_names[]
						= { "vpu_clkb_tmp_composite" };

static struct clk_divider vpu_clkb_div = {
	.reg = (void *)HHI_VPU_CLKB_CNTL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkb_tmp_composite" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate vpu_clkb_gate = {
	.reg = (void *)HHI_VPU_CLKB_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkb_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const vpu_clkc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "null", "null",
	"null",  "null"};

/* cts_clkc */
static struct clk_mux vpu_clkc_p0_mux = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_clkc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider vpu_clkc_p0_div = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkc_p0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate vpu_clkc_p0_gate = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkc_p0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux vpu_clkc_p1_mux = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1_mux",
		.ops = &clk_mux_ops,
		.parent_names = vpu_clkc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider vpu_clkc_p1_div = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vpu_clkc_p1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate vpu_clkc_p1_gate = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vpu_clkc_p1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux vpu_clkc_mux = {
	.reg = (void *)HHI_VPU_CLKC_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]){ "vpu_clkc_p0_composite",
			"vpu_clkc_p1_composite"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *vpu_clkc_hws[] = {
	[CLKID_VPU_CLKC_P0_MUX - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p0_mux.hw,
	[CLKID_VPU_CLKC_P0_DIV - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p0_div.hw,
	[CLKID_VPU_CLKC_P0_GATE - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p0_gate.hw,
	[CLKID_VPU_CLKC_P1_MUX - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p1_mux.hw,
	[CLKID_VPU_CLKC_P1_DIV - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p1_div.hw,
	[CLKID_VPU_CLKC_P1_GATE - CLKID_VPU_CLKC_P0_MUX] = &vpu_clkc_p1_gate.hw,
	[CLKID_VPU_CLKC_MUX - CLKID_VPU_CLKC_P0_MUX]      = &vpu_clkc_mux.hw,
};

/* cts_bt656 */
static const char * const bt656_parent_names[] = { "fclk_div2",
"fclk_div3", "fclk_div5", "fclk_div7" };

static struct clk_mux bt656_mux = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "bt656_mux",
		.ops = &clk_mux_ops,
		.parent_names = bt656_parent_names,
		.num_parents = ARRAY_SIZE(bt656_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider bt656_div = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "bt656_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "bt656_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate bt656_gate = {
	.reg = (void *)HHI_BT656_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "bt656_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_hw *bt656_clk_hws[] = {
	[0] = &bt656_mux.hw,
	[1] = &bt656_div.hw,
	[2] = &bt656_gate.hw,
};

void meson_g12a_media_init(void)
{
	/* cts_dsi_meas_clk */
	dsi_meas_mux.reg = clk_base + (unsigned long)(dsi_meas_mux.reg);
	dsi_meas_div.reg = clk_base + (unsigned long)(dsi_meas_div.reg);
	dsi_meas_gate.reg = clk_base + (unsigned long)(dsi_meas_gate.reg);

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

	/* cts_hevcf_clk */
	hevcf_p0_mux.reg = clk_base + (unsigned long)(hevcf_p0_mux.reg);
	hevcf_p0_div.reg = clk_base + (unsigned long)(hevcf_p0_div.reg);
	hevcf_p0_gate.reg = clk_base + (unsigned long)(hevcf_p0_gate.reg);
	hevcf_p1_mux.reg = clk_base + (unsigned long)(hevcf_p1_mux.reg);
	hevcf_p1_div.reg = clk_base + (unsigned long)(hevcf_p1_div.reg);
	hevcf_p1_gate.reg = clk_base + (unsigned long)(hevcf_p1_gate.reg);
	hevcf_mux.reg = clk_base + (unsigned long)(hevcf_mux.reg);

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

	/* vpu_clkb_tmp */
	vpu_clkb_tmp_mux.reg = clk_base + (unsigned long)(vpu_clkb_tmp_mux.reg);
	vpu_clkb_tmp_div.reg = clk_base + (unsigned long)(vpu_clkb_tmp_div.reg);
	vpu_clkb_tmp_gate.reg = clk_base
			+ (unsigned long)(vpu_clkb_tmp_gate.reg);

	vpu_clkb_div.reg = clk_base + (unsigned long)(vpu_clkb_div.reg);
	vpu_clkb_gate.reg = clk_base + (unsigned long)(vpu_clkb_gate.reg);

	/* cts_vpu_clkc */
	vpu_clkc_p0_mux.reg = clk_base + (unsigned long)(vpu_clkc_p0_mux.reg);
	vpu_clkc_p0_div.reg = clk_base + (unsigned long)(vpu_clkc_p0_div.reg);
	vpu_clkc_p0_gate.reg = clk_base + (unsigned long)(vpu_clkc_p0_gate.reg);
	vpu_clkc_p1_mux.reg = clk_base + (unsigned long)(vpu_clkc_p1_mux.reg);
	vpu_clkc_p1_div.reg = clk_base + (unsigned long)(vpu_clkc_p1_div.reg);
	vpu_clkc_p1_gate.reg = clk_base + (unsigned long)(vpu_clkc_p1_gate.reg);
	vpu_clkc_mux.reg = clk_base + (unsigned long)(vpu_clkc_mux.reg);

	/* bt656 clk */
	bt656_mux.reg = clk_base + (unsigned long)(bt656_mux.reg);
	bt656_div.reg = clk_base + (unsigned long)(bt656_div.reg);
	bt656_gate.reg = clk_base + (unsigned long)(bt656_gate.reg);

	clks[CLKID_BT656_COMP] = clk_register_composite(NULL,
		"bt656_composite",
		bt656_parent_names, ARRAY_SIZE(bt656_parent_names),
		bt656_clk_hws[0], &clk_mux_ops,
		bt656_clk_hws[1], &clk_divider_ops,
		bt656_clk_hws[2], &clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_BT656_COMP]))
		panic("%s: %d clk_register_composite bt656_composite error\n",
			__func__, __LINE__);


	clks[CLKID_DSI_MEAS_COMP] = clk_register_composite(NULL,
		"dsi_meas_composite",
		g12a_meas_parent_names, 8,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_MUX - CLKID_DSI_MEAS_MUX],
		&clk_mux_ops,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_DIV - CLKID_DSI_MEAS_MUX],
		&clk_divider_ops,
		dsi_meas_clk_hws[CLKID_DSI_MEAS_GATE - CLKID_DSI_MEAS_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_DSI_MEAS_COMP]))
		panic("%s: %d clk_register_composite dsi_meas_composite error\n",
			__func__, __LINE__);

	/* cts_vdec_clk */
	clks[CLKID_VDEC_P0_COMP] = clk_register_composite(NULL,
		"vdec_p0_composite",
		g12a_dec_parent_names, 8,
		vdec_clk_hws[CLKID_VDEC_P0_MUX - CLKID_VDEC_P0_MUX],
		&clk_mux_ops,
		vdec_clk_hws[CLKID_VDEC_P0_DIV - CLKID_VDEC_P0_MUX],
		&clk_divider_ops,
		vdec_clk_hws[CLKID_VDEC_P0_GATE - CLKID_VDEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDEC_P0_COMP]))
		panic("%s: %d clk_register_composite vdec_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VDEC_P1_COMP] = clk_register_composite(NULL,
		"vdec_p1_composite",
		g12a_dec_parent_names, 8,
		vdec_clk_hws[CLKID_VDEC_P1_MUX - CLKID_VDEC_P0_MUX],
		&clk_mux_ops,
		vdec_clk_hws[CLKID_VDEC_P1_DIV - CLKID_VDEC_P0_MUX],
		&clk_divider_ops,
		vdec_clk_hws[CLKID_VDEC_P1_GATE - CLKID_VDEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDEC_P1_COMP]))
		panic("%s: %d clk_register_composite vdec_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_VDEC_MUX] = clk_register(NULL,
		vdec_clk_hws[CLKID_VDEC_MUX - CLKID_VDEC_P0_MUX]);
	if (IS_ERR(clks[CLKID_VDEC_MUX]))
		panic("%s: %d clk_register vdec_mux error\n",
			__func__, __LINE__);

	/* cts_hcodec_clk */
	clks[CLKID_HCODEC_P0_COMP] = clk_register_composite(NULL,
		"hcodec_p0_composite",
		g12a_dec_parent_names, 8,
		hcodec_clk_hws[CLKID_HCODEC_P0_MUX - CLKID_HCODEC_P0_MUX],
		&clk_mux_ops,
		hcodec_clk_hws[CLKID_HCODEC_P0_DIV - CLKID_HCODEC_P0_MUX],
		&clk_divider_ops,
		hcodec_clk_hws[CLKID_HCODEC_P0_GATE - CLKID_HCODEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HCODEC_P0_COMP]))
		panic("%s: %d clk_register_composite hcodec_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_HCODEC_P1_COMP] = clk_register_composite(NULL,
		"hcodec_p1_composite",
		g12a_dec_parent_names, 8,
		hcodec_clk_hws[CLKID_HCODEC_P1_MUX - CLKID_HCODEC_P0_MUX],
		&clk_mux_ops,
		hcodec_clk_hws[CLKID_HCODEC_P1_DIV - CLKID_HCODEC_P0_MUX],
		&clk_divider_ops,
		hcodec_clk_hws[CLKID_HCODEC_P1_GATE - CLKID_HCODEC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HCODEC_P1_COMP]))
		panic("%s: %d clk_register_composite hcodec_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_HCODEC_MUX] = clk_register(NULL,
		hcodec_clk_hws[CLKID_HCODEC_MUX - CLKID_HCODEC_P0_MUX]);
	if (IS_ERR(clks[CLKID_HCODEC_MUX]))
		panic("%s: %d clk_register hcodec_mux error\n",
			__func__, __LINE__);

	/* cts_hevc_clk */
	clks[CLKID_HEVC_P0_COMP] = clk_register_composite(NULL,
		"hevc_p0_composite",
		g12a_dec_parent_names, 8,
		hevc_clk_hws[CLKID_HEVC_P0_MUX - CLKID_HEVC_P0_MUX],
		&clk_mux_ops,
		hevc_clk_hws[CLKID_HEVC_P0_DIV - CLKID_HEVC_P0_MUX],
		&clk_divider_ops,
		hevc_clk_hws[CLKID_HEVC_P0_GATE - CLKID_HEVC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVC_P0_COMP]))
		panic("%s: %d clk_register_composite hevc_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_HEVC_P1_COMP] = clk_register_composite(NULL,
		"hevc_p1_composite",
		g12a_dec_parent_names, 8,
		hevc_clk_hws[CLKID_HEVC_P1_MUX - CLKID_HEVC_P0_MUX],
		&clk_mux_ops,
		hevc_clk_hws[CLKID_HEVC_P1_DIV - CLKID_HEVC_P0_MUX],
		&clk_divider_ops,
		hevc_clk_hws[CLKID_HEVC_P1_GATE - CLKID_HEVC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVC_P1_COMP]))
		panic("%s: %d clk_register_composite hevc_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_HEVC_MUX] = clk_register(NULL,
		hevc_clk_hws[CLKID_HEVC_MUX - CLKID_HEVC_P0_MUX]);
	if (IS_ERR(clks[CLKID_HEVC_MUX]))
		panic("%s: %d clk_register hevc_mux error\n",
			__func__, __LINE__);

	/* cts_hevcf_clk */
	clks[CLKID_HEVCF_P0_COMP] = clk_register_composite(NULL,
		"hevcf_p0_composite",
		g12a_dec_parent_names, 8,
		hevcf_clk_hws[CLKID_HEVCF_P0_MUX - CLKID_HEVCF_P0_MUX],
		&clk_mux_ops,
		hevcf_clk_hws[CLKID_HEVCF_P0_DIV - CLKID_HEVCF_P0_MUX],
		&clk_divider_ops,
		hevcf_clk_hws[CLKID_HEVCF_P0_GATE - CLKID_HEVCF_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVCF_P0_COMP]))
		panic("%s: %d clk_register_composite hevcf_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_HEVCF_P1_COMP] = clk_register_composite(NULL,
		"hevcf_p1_composite",
		g12a_dec_parent_names, 8,
		hevcf_clk_hws[CLKID_HEVCF_P1_MUX - CLKID_HEVCF_P0_MUX],
		&clk_mux_ops,
		hevcf_clk_hws[CLKID_HEVCF_P1_DIV - CLKID_HEVCF_P0_MUX],
		&clk_divider_ops,
		hevcf_clk_hws[CLKID_HEVCF_P1_GATE - CLKID_HEVCF_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_HEVCF_P1_COMP]))
		panic("%s: %d clk_register_composite hevcf_p1_composite error\n",
		__func__, __LINE__);

	clks[CLKID_HEVCF_MUX] = clk_register(NULL,
		hevcf_clk_hws[CLKID_HEVCF_MUX - CLKID_HEVCF_P0_MUX]);
	if (IS_ERR(clks[CLKID_HEVCF_MUX]))
		panic("%s: %d clk_register hevcf_mux error\n",
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
		panic("%s: %d clk_register_composite vpu_p0_composite error\n",
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
		panic("%s: %d clk_register_composite vpu_p1_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VPU_MUX] = clk_register(NULL,
		vpu_clk_hws[CLKID_VPU_MUX - CLKID_VPU_P0_MUX]);
	if (IS_ERR(clks[CLKID_VPU_MUX]))
		panic("%s: %d clk_register vpu_mux error\n",
			__func__, __LINE__);

	/* cts_vapb_clk */
	clks[CLKID_VAPB_P0_COMP] = clk_register_composite(NULL,
		"vapb_p0_composite",
		vapb_parent_names, 8,
		vapb_clk_hws[CLKID_VAPB_P0_MUX - CLKID_VAPB_P0_MUX],
		&clk_mux_ops,
		vapb_clk_hws[CLKID_VAPB_P0_DIV - CLKID_VAPB_P0_MUX],
		&clk_divider_ops,
		vapb_clk_hws[CLKID_VAPB_P0_GATE - CLKID_VAPB_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VAPB_P0_COMP]))
		panic("%s: %d clk_register_composite vapb_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VAPB_P1_COMP] = clk_register_composite(NULL,
		"vapb_p1_composite",
		vapb_parent_names, 8,
		vapb_clk_hws[CLKID_VAPB_P1_MUX - CLKID_VAPB_P0_MUX],
		&clk_mux_ops,
		vapb_clk_hws[CLKID_VAPB_P1_DIV - CLKID_VAPB_P0_MUX],
		&clk_divider_ops,
		vapb_clk_hws[CLKID_VAPB_P1_GATE - CLKID_VAPB_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VAPB_P1_COMP]))
		panic("%s: %d clk_register_composite vapb_p1_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VAPB_MUX] = clk_register(NULL,
		vapb_clk_hws[CLKID_VAPB_MUX - CLKID_VAPB_P0_MUX]);
	if (IS_ERR(clks[CLKID_VAPB_MUX]))
		panic("%s: %d clk_register vapb_mux error\n",
			__func__, __LINE__);

	clks[CLKID_GE2D_GATE] = clk_register(NULL,
		&ge2d_gate.hw);
	if (IS_ERR(clks[CLKID_GE2D_GATE]))
		panic("%s: %d clk_register ge2d_gate error\n",
			__func__, __LINE__);

	clks[CLKID_VPU_CLKB_TMP_COMP] = clk_register_composite(NULL,
			"vpu_clkb_tmp_composite",
			vpu_clkb_tmp_parent_names, 4,
			&vpu_clkb_tmp_mux.hw,
			&clk_mux_ops,
			&vpu_clkb_tmp_div.hw,
			&clk_divider_ops,
			&vpu_clkb_tmp_gate.hw,
			&clk_gate_ops, 0);
		if (IS_ERR(clks[CLKID_VPU_CLKB_TMP_COMP]))
			panic("%s: %d clk_register_composite vpu_clkb_tmp_composite error\n",
				__func__, __LINE__);

	clks[CLKID_VPU_CLKB_COMP] = clk_register_composite(NULL,
			"vpu_clkb_composite",
			vpu_clkb_parent_names, 1,
			NULL,
			NULL,
			&vpu_clkb_div.hw,
			&clk_divider_ops,
			&vpu_clkb_gate.hw,
			&clk_gate_ops, 0);
		if (IS_ERR(clks[CLKID_VPU_CLKB_COMP]))
			panic("%s: %d clk_register_composite vpu_clkb_composite error\n",
				__func__, __LINE__);

	/* cts_vpu_clkc */
	clks[CLKID_VPU_CLKC_P0_COMP] = clk_register_composite(NULL,
		"vpu_clkc_p0_composite",
		vpu_clkc_parent_names, 8,
		vpu_clkc_hws[CLKID_VPU_CLKC_P0_MUX - CLKID_VPU_CLKC_P0_MUX],
		&clk_mux_ops,
		vpu_clkc_hws[CLKID_VPU_CLKC_P0_DIV - CLKID_VPU_CLKC_P0_MUX],
		&clk_divider_ops,
		vpu_clkc_hws[CLKID_VPU_CLKC_P0_GATE - CLKID_VPU_CLKC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VPU_CLKC_P0_COMP]))
		panic("%s: %d clk_register_composite vpu_clkc_p0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VPU_CLKC_P1_COMP] = clk_register_composite(NULL,
		"vpu_clkc_p1_composite",
		vpu_clkc_parent_names, 8,
		vpu_clkc_hws[CLKID_VPU_CLKC_P1_MUX - CLKID_VPU_CLKC_P0_MUX],
		&clk_mux_ops,
		vpu_clkc_hws[CLKID_VPU_CLKC_P1_DIV - CLKID_VPU_CLKC_P0_MUX],
		&clk_divider_ops,
		vpu_clkc_hws[CLKID_VPU_CLKC_P1_GATE - CLKID_VPU_CLKC_P0_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VPU_CLKC_P1_COMP]))
		panic("%s: %d clk_register_composite vpu_clkc_p1_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VPU_CLKC_MUX] = clk_register(NULL,
		vpu_clkc_hws[CLKID_VPU_CLKC_MUX - CLKID_VPU_CLKC_P0_MUX]);
	if (IS_ERR(clks[CLKID_VPU_CLKC_MUX]))
		panic("%s: %d clk_register vpu_clkc_mux error\n",
			__func__, __LINE__);
	pr_info("%s: register meson media clk\n", __func__);
}

