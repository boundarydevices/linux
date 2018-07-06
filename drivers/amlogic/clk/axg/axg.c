/*
 * drivers/amlogic/clk/axg/axg.c
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

static struct clk_onecell_data clk_data;
/* #undef pr_debug */
/* #define pr_debug pr_info */

static const struct clk_div_table cpu_div_table[] = {
	{ .val = 1, .div = 1 },
	{ .val = 2, .div = 2 },
	{ .val = 3, .div = 3 },
	{ .val = 2, .div = 4 },
	{ .val = 3, .div = 6 },
	{ .val = 4, .div = 8 },
	{ .val = 5, .div = 10 },
	{ .val = 6, .div = 12 },
	{ .val = 7, .div = 14 },
	{ .val = 8, .div = 16 },
	{ /* sentinel */ },
};

static struct meson_clk_pll axg_fixed_pll = {
	.m = {
		.reg_off = HHI_MPLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_MPLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_MPLL_CNTL,
		.shift   = 16,
		.width   = 2,
	},
	.frac = {
		.reg_off = HHI_MPLL_CNTL2,
		.shift   = 0,
		.width   = 12,
	},
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_axg_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll axg_sys_pll = {
	.m = {
		.reg_off = HHI_SYS_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_SYS_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_SYS_PLL_CNTL,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = sys_pll_rate_table,
	.rate_count = ARRAY_SIZE(sys_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_axg_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll axg_gp0_pll = {
	.m = {
		.reg_off = HHI_GP0_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_GP0_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_GP0_PLL_CNTL,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = axg_gp0_pll_rate_table,
	.rate_count = ARRAY_SIZE(axg_gp0_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_axg_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll axg_hifi_pll = {
	.m = {
		.reg_off = HHI_HIFI_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_HIFI_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_HIFI_PLL_CNTL,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = axg_gp0_pll_rate_table,
	.rate_count = ARRAY_SIZE(axg_gp0_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_axg_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor axg_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll axg_mpll0 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL7,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL7,
		.shift   = 16,
		.width   = 9,
	},
	.sdm_en = 15,
	.en_dds = 14,
	.top_misc_reg = HHI_PLL_TOP_MISC,
	.top_misc_bit = 0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll axg_mpll1 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL8,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL8,
		.shift   = 16,
		.width   = 9,
	},
	.sdm_en = 15,
	.en_dds = 14,
	.top_misc_reg = HHI_PLL_TOP_MISC,
	.top_misc_bit = 1,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll axg_mpll2 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL9,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL9,
		.shift   = 16,
		.width   = 9,
	},
	.sdm_en = 15,
	.en_dds = 14,
	.top_misc_reg = HHI_PLL_TOP_MISC,
	.top_misc_bit = 2,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll axg_mpll3 = {
	.sdm = {
		.reg_off = HHI_MPLL3_CNTL0,
		.shift   = 12,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL3_CNTL0,
		.shift   = 2,
		.width   = 9,
	},
	.sdm_en = 11,
	.en_dds = 0,
	.top_misc_reg = HHI_PLL_TOP_MISC,
	.top_misc_bit = 3,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_pll axg_pcie_pll = {
	.m = {
		.reg_off = HHI_PCIE_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_PCIE_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_PCIE_PLL_CNTL,
		.shift   = 16,
		.width   = 2,
	},
	.od2 = {
		.reg_off = HHI_PCIE_PLL_CNTL6,
		.shift   = 6,
		.width   = 2,
	},
	.frac = {
		.reg_off = HHI_PCIE_PLL_CNTL1,
		.shift   = 0,
		.width   = 12,
	},
	.rate_table = axg_pcie_pll_rate_table,
	.rate_count = ARRAY_SIZE(axg_pcie_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &meson_axg_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux axg_pcie_mux = {
	.reg = (void *)HHI_PCIE_PLL_CNTL6,
	.mask = 0x1,
	.shift = 2,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axg_pcie_mux",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "mpll3", "pcie_pll" },
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux axg_pcie_ref = {
	.reg = (void *)HHI_PCIE_PLL_CNTL6,
	.mask = 0x1,
	.shift = 1,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axg_pcie_ref",
		.ops = &clk_mux_ops,
		//.parent_names = (const char *[]){ "axg_pcie_input_gate",
		.parent_names = (const char *[]){ "NULL",
			"axg_pcie_mux" },
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate axg_pcie_cml_en0 = {
	.reg = (void *)HHI_PCIE_PLL_CNTL6,
	.bit_idx = 4,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axg_pcie_cml_en0",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "axg_pcie_ref" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate axg_pcie_cml_en1 = {
	.reg = (void *)HHI_PCIE_PLL_CNTL6,
	.bit_idx = 3,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axg_pcie_cml_en1",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "axg_pcie_ref" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate axg_mipi_enable_gate = {
	.reg = (void *)HHI_MIPI_CNTL0,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axg_mipi_enable_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "NULL" },
		.num_parents = 0,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate axg_mipi_bandgap_gate = {
	.reg = (void *)HHI_MIPI_CNTL0,
	.bit_idx = 26,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "axg_mipi_bandgap_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "NULL" },
		.num_parents = 0,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

/*
 * FIXME cpu clocks and the legacy composite clocks (e.g. clk81) are both PLL
 * post-dividers and should be modelled with their respective PLLs via the
 * forthcoming coordinated clock rates feature
 */
static u32 mux_table_cpu_p00[]	= { 0, 1, 2 };
static struct clk_mux axg_cpu_fixedpll_p00 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mask = 0x3,
	.shift = 0,
	.table = mux_table_cpu_p00,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p00",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
			"fclk_div3"},
		.num_parents = 3,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider axg_cpu_fixedpll_p01 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.shift = 4,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p01",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p00" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_cpu_p0[]	= { 0, 1 };
static struct clk_mux axg_cpu_fixedpll_p0 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mask = 0x1,
	.shift = 2,
	.table = mux_table_cpu_p0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p0",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p00",
			"cpu_fixedpll_p01"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
				  | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_cpu_p10[]	= { 0, 1, 2 };
static struct clk_mux axg_cpu_fixedpll_p10 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mask = 0x3,
	.shift = 16,
	.table = mux_table_cpu_p10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p10",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
			"fclk_div3"},
		.num_parents = 3,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider axg_cpu_fixedpll_p11 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.shift = 20,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p11",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p10" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_cpu_p1[]	= { 0, 1 };
static struct clk_mux axg_cpu_fixedpll_p1 = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mask = 0x1,
	.shift = 18,
	.table = mux_table_cpu_p1,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p1",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p10",
			"cpu_fixedpll_p11"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
				  | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_cpu_p[]	= { 0, 1 };
static struct clk_mux axg_cpu_fixedpll_p = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mask = 0x1,
	.shift = 10,
	.table = mux_table_cpu_p,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p0",
			"cpu_fixedpll_p1"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_cpu_clk[]	= { 0, 1 };
static struct meson_clk_cpu axg_cpu_clk = {
	.reg_off = HHI_SYS_CPU_CLK_CNTL0,
	.mux.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mux.shift = 11,
	.mux.mask = 0x1,
	.mux.lock = &clk_lock,
	.mux.table = mux_table_cpu_clk,
	.mux.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p", "sys_pll"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static u32 mux_table_clk81[]	= { 6, 5, 7 };

static struct clk_mux axg_mpeg_clk_sel = {
	.reg = (void *)HHI_MPEG_CLK_CNTL,
	.mask = 0x7,
	.shift = 12,
	.flags = CLK_MUX_READ_ONLY,
	.table = mux_table_clk81,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_sel",
		.ops = &clk_mux_ro_ops,
		/*
		 * FIXME bits 14:12 selects from 8 possible parents:
		 * xtal, 1'b0 (wtf), fclk_div7, mpll_clkout1, mpll_clkout2,
		 * fclk_div4, fclk_div3, fclk_div5
		 */
		.parent_names = (const char *[]){ "fclk_div3", "fclk_div4",
			"fclk_div5" },
		.num_parents = 3,
		.flags = (CLK_SET_RATE_NO_REPARENT | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider axg_mpeg_clk_div = {
	.reg = (void *)HHI_MPEG_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "mpeg_clk_sel" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),
	},
};

/* the mother of dragons^W gates */
static struct clk_gate axg_clk81 = {
	.reg = (void *)HHI_MPEG_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "mpeg_clk_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate axg_spicc_0 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axg_spicc_0",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

static struct clk_gate axg_spicc_1 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 15,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "axg_spicc_1",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

/* Everything Else (EE) domain gates */
static MESON_GATE(axg_ddr, HHI_GCLK_MPEG0, 0);
static MESON_GATE(axg_audio_locker, HHI_GCLK_MPEG0, 2);
static MESON_GATE(axg_mipi_dsi_host, HHI_GCLK_MPEG0, 3);
static MESON_GATE(axg_isa, HHI_GCLK_MPEG0, 5);
static MESON_GATE(axg_pl301, HHI_GCLK_MPEG0, 6);
static MESON_GATE(axg_periphs, HHI_GCLK_MPEG0, 7);
//static MESON_GATE(axg_spicc_0, HHI_GCLK_MPEG0, 8);
static MESON_GATE(axg_i2c, HHI_GCLK_MPEG0, 9);
static MESON_GATE(axg_rng0, HHI_GCLK_MPEG0, 12);
static MESON_GATE(axg_uart0, HHI_GCLK_MPEG0, 13);
static MESON_GATE(axg_mipi_dsi_phy, HHI_GCLK_MPEG0, 14);
//static MESON_GATE(axg_spicc_1, HHI_GCLK_MPEG0, 15);
static MESON_GATE(axg_pcie_a, HHI_GCLK_MPEG0, 16);
static MESON_GATE(axg_pcie_b, HHI_GCLK_MPEG0, 17);
static MESON_GATE(axg_hiu_reg, HHI_GCLK_MPEG0, 19);
static MESON_GATE(axg_assist_misc, HHI_GCLK_MPEG0, 23);
static MESON_GATE(axg_emmc_b, HHI_GCLK_MPEG0, 25);
static MESON_GATE(axg_emmc_c, HHI_GCLK_MPEG0, 26);
static MESON_GATE(axg_dma, HHI_GCLK_MPEG0, 27);
static MESON_GATE(axg_spi, HHI_GCLK_MPEG0, 30);

static MESON_GATE(axg_audio, HHI_GCLK_MPEG1, 0);
static MESON_GATE(axg_eth_core, HHI_GCLK_MPEG1, 3);
static MESON_GATE(axg_uart1, HHI_GCLK_MPEG1, 16);
static MESON_GATE(axg_g2d, HHI_GCLK_MPEG1, 20);
static MESON_GATE(axg_usb0, HHI_GCLK_MPEG1, 21);
static MESON_GATE(axg_usb1, HHI_GCLK_MPEG1, 22);
static MESON_GATE(axg_reset, HHI_GCLK_MPEG1, 23);
static MESON_GATE(axg_usb_general, HHI_GCLK_MPEG1, 26);
static MESON_GATE(axg_ahb_arb0, HHI_GCLK_MPEG1, 29);
static MESON_GATE(axg_efuse, HHI_GCLK_MPEG1, 30);
static MESON_GATE(axg_boot_rom, HHI_GCLK_MPEG1, 31);

static MESON_GATE(axg_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(axg_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(axg_usb1_to_ddr, HHI_GCLK_MPEG2, 8);
static MESON_GATE(axg_usb0_to_ddr, HHI_GCLK_MPEG2, 9);
static MESON_GATE(axg_mmc_pclk, HHI_GCLK_MPEG2, 11);
static MESON_GATE(axg_vpu_intr, HHI_GCLK_MPEG2, 25);
static MESON_GATE(axg_sec_ahb_ahb3_bridge, HHI_GCLK_MPEG2, 26);
static MESON_GATE(axg_gic, HHI_GCLK_MPEG2, 30);

/* Always On (AO) domain gates */

static MESON_GATE(axg_ao_media_cpu, HHI_GCLK_AO, 0);
static MESON_GATE(axg_ao_ahb_sram, HHI_GCLK_AO, 1);
static MESON_GATE(axg_ao_ahb_bus, HHI_GCLK_AO, 2);
static MESON_GATE(axg_ao_iface, HHI_GCLK_AO, 3);
static MESON_GATE(axg_ao_i2c, HHI_GCLK_AO, 4);

/* Array of all clocks provided by this provider */

static struct clk_hw *axg_clk_hws[] = {
	[CLKID_SYS_PLL]		    = &axg_sys_pll.hw,
	[CLKID_FIXED_PLL]	    = &axg_fixed_pll.hw,
	[CLKID_GP0_PLL]		    = &axg_gp0_pll.hw,
	[CLKID_HIFI_PLL]		    = &axg_hifi_pll.hw,
	[CLKID_FCLK_DIV2]	    = &axg_fclk_div2.hw,
	[CLKID_FCLK_DIV3]	    = &axg_fclk_div3.hw,
	[CLKID_FCLK_DIV4]	    = &axg_fclk_div4.hw,
	[CLKID_FCLK_DIV5]	    = &axg_fclk_div5.hw,
	[CLKID_FCLK_DIV7]	    = &axg_fclk_div7.hw,
	[CLKID_MPEG_SEL]	    = &axg_mpeg_clk_sel.hw,
	[CLKID_MPEG_DIV]	    = &axg_mpeg_clk_div.hw,
	[CLKID_CLK81]		    = &axg_clk81.hw,
	[CLKID_MPLL0]		    = &axg_mpll0.hw,
	[CLKID_MPLL1]		    = &axg_mpll1.hw,
	[CLKID_MPLL2]		    = &axg_mpll2.hw,
	[CLKID_MPLL3]		    = &axg_mpll3.hw,
	[CLKID_DDR]		    = &axg_ddr.hw,
	[CLKID_AUDIO_LOCKER]	= &axg_audio_locker.hw,
	[CLKID_MIPI_DSI_HOST]	= &axg_mipi_dsi_host.hw,
	[CLKID_ISA]		    = &axg_isa.hw,
	[CLKID_PL301]		    = &axg_pl301.hw,
	[CLKID_PERIPHS]		    = &axg_periphs.hw,
	[CLKID_SPICC0]		    = &axg_spicc_0.hw,
	[CLKID_I2C]		    = &axg_i2c.hw,
	[CLKID_RNG0]		    = &axg_rng0.hw,
	[CLKID_UART0]		    = &axg_uart0.hw,
	[CLKID_MIPI_DSI_PHY] = &axg_mipi_dsi_phy.hw,
	[CLKID_SPICC1]		    = &axg_spicc_1.hw,
	[CLKID_PCIE_A]		    = &axg_pcie_a.hw,
	[CLKID_PCIE_B]		    = &axg_pcie_b.hw,
	[CLKID_HIU_REG]	    = &axg_hiu_reg.hw,
	[CLKID_ASSIST_MISC]	    = &axg_assist_misc.hw,
	[CLKID_SD_EMMC_B]	    = &axg_emmc_b.hw,
	[CLKID_SD_EMMC_C]	    = &axg_emmc_c.hw,
	[CLKID_DMA]	    = &axg_dma.hw,
	[CLKID_SPI]		    = &axg_spi.hw,
	[CLKID_AUDIO]		    = &axg_audio.hw,
	[CLKID_ETH_CORE]		    = &axg_eth_core.hw,
	[CLKID_UART1]		    = &axg_uart1.hw,
	[CLKID_G2D]		    = &axg_g2d.hw,
	[CLKID_USB0]		    = &axg_usb0.hw,
	[CLKID_USB1]		    = &axg_usb1.hw,
	[CLKID_RESET]		    = &axg_reset.hw,
	[CLKID_USB_GENERAL]		    = &axg_usb_general.hw,
	[CLKID_AHB_ARB0]	    = &axg_ahb_arb0.hw,
	[CLKID_EFUSE]		    = &axg_efuse.hw,
	[CLKID_BOOT_ROM]	    = &axg_boot_rom.hw,
	[CLKID_AHB_DATA_BUS]	    = &axg_ahb_data_bus.hw,
	[CLKID_AHB_CTRL_BUS]	    = &axg_ahb_ctrl_bus.hw,
	[CLKID_USB1_TO_DDR]	    = &axg_usb1_to_ddr.hw,
	[CLKID_USB0_TO_DDR]	    = &axg_usb0_to_ddr.hw,
	[CLKID_MMC_PCLK]	    = &axg_mmc_pclk.hw,
	[CLKID_VPU_INTR]	    = &axg_vpu_intr.hw,
	[CLKID_SEC_AHB_AHB3_BRIDGE] = &axg_sec_ahb_ahb3_bridge.hw,
	[CLKID_GIC]	    = &axg_gic.hw,
	[CLKID_AO_MEDIA_CPU]	    = &axg_ao_media_cpu.hw,
	[CLKID_AO_AHB_SRAM]	    = &axg_ao_ahb_sram.hw,
	[CLKID_AO_AHB_BUS]	    = &axg_ao_ahb_bus.hw,
	[CLKID_AO_IFACE]	    = &axg_ao_iface.hw,
	[CLKID_AO_I2C]		    = &axg_ao_i2c.hw,
	[CLKID_CPU_FCLK_P00]	= &axg_cpu_fixedpll_p00.hw,
	[CLKID_CPU_FCLK_P01]	= &axg_cpu_fixedpll_p01.hw,
	[CLKID_CPU_FCLK_P0]		= &axg_cpu_fixedpll_p0.hw,
	[CLKID_CPU_FCLK_P10]	= &axg_cpu_fixedpll_p10.hw,
	[CLKID_CPU_FCLK_P11]	= &axg_cpu_fixedpll_p11.hw,
	[CLKID_CPU_FCLK_P1]		= &axg_cpu_fixedpll_p1.hw,
	[CLKID_CPU_FCLK_P]		= &axg_cpu_fixedpll_p.hw,
	[CLKID_CPU_CLK]		    = &axg_cpu_clk.mux.hw,
	[CLKID_PCIE_PLL]		= &axg_pcie_pll.hw,
	[CLKID_PCIE_MUX]			= &axg_pcie_mux.hw,
	[CLKID_PCIE_REF]			= &axg_pcie_ref.hw,
	[CLKID_PCIE_CML_EN0]	= &axg_pcie_cml_en0.hw,
	[CLKID_PCIE_CML_EN1]	= &axg_pcie_cml_en1.hw,
	[CLKID_MIPI_ENABLE_GATE]	= &axg_mipi_enable_gate.hw,
	[CLKID_MIPI_BANDGAP_GATE]	= &axg_mipi_bandgap_gate.hw,
};
/* Convenience tables to populate base addresses in .probe */

static struct meson_clk_pll *const axg_clk_plls[] = {
	&axg_fixed_pll,
	&axg_sys_pll,
	&axg_gp0_pll,
	&axg_hifi_pll,
	&axg_pcie_pll,
};

static struct meson_clk_mpll *const axg_clk_mplls[] = {
	&axg_mpll0,
	&axg_mpll1,
	&axg_mpll2,
	&axg_mpll3,
};

static struct clk_gate *axg_clk_gates[] = {
	&axg_clk81,
	&axg_ddr,
	&axg_audio_locker,
	&axg_mipi_dsi_host,
	&axg_isa,
	&axg_pl301,
	&axg_periphs,
	&axg_spicc_0,
	&axg_i2c,
	&axg_rng0,
	&axg_uart0,
	&axg_mipi_dsi_phy,
	&axg_spicc_1,
	&axg_pcie_a,
	&axg_pcie_b,
	&axg_hiu_reg,
	&axg_assist_misc,
	&axg_emmc_b,
	&axg_emmc_c,
	&axg_dma,
	&axg_spi,
	&axg_audio,
	&axg_eth_core,
	&axg_uart1,
	&axg_g2d,
	&axg_usb0,
	&axg_usb1,
	&axg_reset,
	&axg_usb_general,
	&axg_ahb_arb0,
	&axg_efuse,
	&axg_boot_rom,
	&axg_ahb_data_bus,
	&axg_ahb_ctrl_bus,
	&axg_usb1_to_ddr,
	&axg_usb0_to_ddr,
	&axg_mmc_pclk,
	&axg_vpu_intr,
	&axg_sec_ahb_ahb3_bridge,
	&axg_gic,
	&axg_ao_media_cpu,
	&axg_ao_ahb_sram,
	&axg_ao_ahb_bus,
	&axg_ao_iface,
	&axg_ao_i2c,
	&axg_mipi_enable_gate,
	&axg_mipi_bandgap_gate,
};

static void __init axg_clkc_init(struct device_node *np)
{
	int ret, clkid, i;
	struct clk_hw *parent_hw;
	struct clk *parent_clk;

	/*  Generic clocks and PLLs */
	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		/* return -ENXIO; */
		return;
	}
	/* pr_debug("%s: iomap clk_base ok!", __func__); */
	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(axg_clk_plls); i++)
		axg_clk_plls[i]->base = clk_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(axg_clk_mplls); i++)
		axg_clk_mplls[i]->base = clk_base;

	/* Populate the base address for CPU clk */
	axg_cpu_clk.mux.reg = clk_base
			+ (unsigned long)axg_cpu_clk.mux.reg;
	axg_cpu_fixedpll_p00.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p00.reg;
	axg_cpu_fixedpll_p01.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p01.reg;
	axg_cpu_fixedpll_p10.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p10.reg;
	axg_cpu_fixedpll_p11.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p11.reg;
	axg_cpu_fixedpll_p0.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p0.reg;
	axg_cpu_fixedpll_p1.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p1.reg;
	axg_cpu_fixedpll_p.reg = clk_base
			+ (unsigned long)axg_cpu_fixedpll_p.reg;

	/* Populate the base address for the MPEG clks */
	axg_mpeg_clk_sel.reg = clk_base
			+ (unsigned long)axg_mpeg_clk_sel.reg;
	axg_mpeg_clk_div.reg = clk_base
			+ (unsigned long)axg_mpeg_clk_div.reg;

	axg_pcie_mux.reg = clk_base
			+ (unsigned long)axg_pcie_mux.reg;
	axg_pcie_ref.reg = clk_base
			+ (unsigned long)axg_pcie_ref.reg;

	axg_pcie_cml_en0.reg = clk_base
			+ (unsigned long)axg_pcie_cml_en0.reg;
	axg_pcie_cml_en1.reg = clk_base
			+ (unsigned long)axg_pcie_cml_en1.reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(axg_clk_gates); i++)
		axg_clk_gates[i]->reg = clk_base +
			(unsigned long)axg_clk_gates[i]->reg;


	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			/* pr_err("%s: alloc clks fail!", __func__); */
			/* return -ENOMEM; */
			return;
		}
		clk_numbers = NR_CLKS;
	}

	if (NULL == clks) {
		pr_err("%s: error: not kzalloc clks in aoclk!", __func__);
		return;
	}

	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;
	/*
	 * register all clks
	 */

	for (clkid = 0; clkid < OTHER_BASE; clkid++) {
		if (axg_clk_hws[clkid]) {
		clks[clkid] = clk_register(NULL, axg_clk_hws[clkid]);
		WARN_ON(IS_ERR(clks[clkid]));
		}
	}

	clk_set_parent(clks[CLKID_PCIE_MUX], clks[CLKID_PCIE_PLL]);
	clk_set_parent(clks[CLKID_PCIE_REF], clks[CLKID_PCIE_MUX]);

	axg_amlogic_init_sdemmc();
	axg_amlogic_init_media();
	axg_amlogic_init_misc();
	pr_debug("%s: register all clk ok!", __func__);
	/*
	 * Register CPU clk notifier
	 *
	 * FIXME this is wrong for a lot of reasons. First, the muxes should be
	 * struct clk_hw objects. Second, we shouldn't program the muxes in
	 * notifier handlers. The tricky programming sequence will be handled
	 * by the forthcoming coordinated clock rates mechanism once that
	 * feature is released.
	 *
	 * Furthermore, looking up the parent this way is terrible. At some
	 * point we will stop allocating a default struct clk when registering
	 * a new clk_hw, and this hack will no longer work. Releasing the ccr
	 * feature before that time solves the problem :-)
	 */
	parent_hw = clk_hw_get_parent(&axg_cpu_clk.mux.hw);
	parent_clk = parent_hw->clk;
	ret = clk_notifier_register(parent_clk, &axg_cpu_clk.clk_nb);
	if (ret) {
		pr_err("%s: failed to register clock notifier for cpu_clk\n",
				__func__);
		goto iounmap;
	}
	pr_debug("%s: cpu clk register notifier ok!", __func__);

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
			&clk_data);
	if (ret < 0)
		pr_err("%s fail ret: %d\n", __func__, ret);
	else
		pr_info("%s initialization complete\n", __func__);
	return;

iounmap:
	iounmap(clk_base);
	pr_info("%s: %d: ret: %d\n", __func__, __LINE__, ret);
	/* return; */
}

CLK_OF_DECLARE(axg, "amlogic,axg-clkc", axg_clkc_init);


