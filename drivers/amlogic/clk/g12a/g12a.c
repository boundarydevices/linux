/*
 * drivers/amlogic/clk/g12a/g12a.c
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

#include <linux/amlogic/cpu_version.h>
#include "../clkc.h"
#include "g12a.h"

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

static struct meson_clk_pll g12a_fixed_pll = {
	.m = {
		.reg_off = HHI_FIX_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_FIX_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_FIX_PLL_CNTL0,
		.shift   = 16,
		.width   = 2,
	},
	.frac = {
		.reg_off = HHI_FIX_PLL_CNTL1,
		.shift   = 0,
		.width   = 19,
	},
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_g12a_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll g12a_sys_pll = {
	.m = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = g12a_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_g12a_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll g12b_sys1_pll = {
	.m = {
		.reg_off = HHI_SYS1_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_SYS1_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_SYS1_PLL_CNTL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = g12a_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &meson_g12a_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll g12a_gp0_pll = {
	.m = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 16,
		.width   = 3,
	},
	.rate_table = g12a_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_g12a_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll g12a_hifi_pll = {
	.m = {
		.reg_off = HHI_HIFI_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_HIFI_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_HIFI_PLL_CNTL0,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = g12a_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_g12a_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll g12a_pcie_pll = {
	.m = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 16,
		.width   = 5,
	},

	.frac = {
		.reg_off = HHI_PCIE_PLL_CNTL1,
		.shift   = 0,
		.width   = 12,
	},
	.rate_table = g12a_pcie_pll_rate_table,
	.rate_count = ARRAY_SIZE(g12a_pcie_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &meson_g12a_pcie_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor g12a_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div2p5 = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll g12a_mpll0 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL1,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL1,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = HHI_MPLL_CNTL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &meson_g12a_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll g12a_mpll1 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL3,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL3,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = HHI_MPLL_CNTL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &meson_g12a_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll g12a_mpll2 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL5,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL5,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = HHI_MPLL_CNTL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &meson_g12a_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll g12a_mpll3 = {
	.sdm = {
		.reg_off = HHI_MPLL_CNTL7,
		.shift   = 0,
		.width   = 14,
	},
	.n2 = {
		.reg_off = HHI_MPLL_CNTL7,
		.shift   = 20,
		.width   = 9,
	},
	.sdm_en = 30,
	.en_dds = 31,
	.mpll_cntl0_reg = HHI_MPLL_CNTL0,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &meson_g12a_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static u32 mux_table_cpu_p[]	= { 0, 1, 2 };
static u32 mux_table_cpu_px[]   = { 0, 1 };
static struct meson_cpu_mux_divider g12a_cpu_fclk_p = {
	.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.cpu_fclk_p00 = {
		.mask = 0x3,
		.shift = 0,
		.width = 2,
	},
	.cpu_fclk_p0 = {
		.mask = 0x1,
		.shift = 2,
		.width = 1,
	},
	.cpu_fclk_p10 = {
		.mask = 0x3,
		.shift = 16,
		.width = 2,
	},
	.cpu_fclk_p1 = {
		.mask = 0x1,
		.shift = 18,
		.width = 1,
	},
	.cpu_fclk_p = {
		.mask = 0x1,
		.shift = 10,
		.width = 1,
	},
	.cpu_fclk_p01 = {
		.shift = 4,
		.width = 6,
	},
	.cpu_fclk_p11 = {
		.shift = 20,
		.width = 6,
	},
	.table = mux_table_cpu_p,
	.rate_table = fclk_pll_rate_table,
	.rate_count = ARRAY_SIZE(fclk_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_fixedpll_p",
		.ops = &meson_fclk_cpu_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
			"fclk_div3"},
		.num_parents = 3,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct meson_clk_cpu g12a_cpu_clk = {
	.reg_off = HHI_SYS_CPU_CLK_CNTL0,
	.clk_nb.notifier_call = meson_clk_cpu_notifier_cb,
	.mux.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mux.shift = 11,
	.mux.mask = 0x1,
	.mux.lock = &clk_lock,
	.mux.table = mux_table_cpu_px,
	.mux.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p", "sys_pll"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_cpu g12b_cpu_clk1 = {
	.reg_off = HHI_SYS_CPU_CLK_CNTL0,
	.clk_nb.notifier_call = meson_clk_cpu_notifier_cb,
	.mux.reg = (void *)HHI_SYS_CPU_CLK_CNTL0,
	.mux.shift = 11,
	.mux.mask = 0x1,
	.mux.lock = &clk_lock,
	.mux.table = mux_table_cpu_px,
	.mux.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &meson_clk_cpu_ops,
		.parent_names = (const char *[]){ "cpu_fixedpll_p", "sys1_pll"},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};



static u32 mux_table_clk81[] = { 6, 5, 7 };

static struct clk_mux g12a_mpeg_clk_sel = {
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
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_divider g12a_mpeg_clk_div = {
	.reg = (void *)HHI_MPEG_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "mpeg_clk_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* the mother of dragons^W gates */
static struct clk_gate g12a_clk81 = {
	.reg = (void *)HHI_MPEG_CLK_CNTL,
	.bit_idx = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "mpeg_clk_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

/* GPIO 24M */
static struct clk_gate g12a_24m = {
	.reg = (void *)HHI_XTAL_DIVN_CNTL,
	.bit_idx = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_24m",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

/* GPIO 12M */
static struct clk_divider g12a_12m_div = {
	.reg = (void *)HHI_XTAL_DIVN_CNTL,
	.shift = 10,
	.width = 1,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_12m_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate g12a_12m_gate = {
	.reg = (void *)HHI_XTAL_DIVN_CNTL,
	.bit_idx = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_12m_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "g12a_12m_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

static u32 mux_table_gen_clk[]	= { 0, 5, 6, 7, 20, 21, 22,
				23, 24, 25, 26, 27, 28, };
static const char * const gen_clk_parent_names[] = {
	"xtal", "gp0_pll", "gp1_pll", "hifi_pll", "fclk_div2", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "mpll0", "mpll1",
	"mpll2", "mpll3"
};

static struct clk_mux g12a_gen_clk_sel = {
	.reg = (void *)HHI_GEN_CLK_CNTL,
	.mask = 0x1f,
	.shift = 12,
	.table = mux_table_gen_clk,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_sel",
		.ops = &clk_mux_ops,
		.parent_names = gen_clk_parent_names,
		.num_parents = ARRAY_SIZE(gen_clk_parent_names),
	},
};

static struct clk_divider g12a_gen_clk_div = {
	.reg = (void *)HHI_GEN_CLK_CNTL,
	.shift = 0,
	.width = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gen_clk_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_gate g12a_gen_clk = {
	.reg = (void *)HHI_GEN_CLK_CNTL,
	.bit_idx = 11,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gen_clk_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};


/* Everything Else (EE) domain gates */
static struct clk_gate g12a_spicc_0 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_spicc_0",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

static struct clk_gate g12a_spicc_1 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 14,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_spicc_1",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};
static MESON_GATE(g12a_ddr, HHI_GCLK_MPEG0, 0);
static MESON_GATE(g12a_dos, HHI_GCLK_MPEG0, 1);
static MESON_GATE(g12a_alocker, HHI_GCLK_MPEG0, 2);
static MESON_GATE(g12a_mipi_dsi_host, HHI_GCLK_MPEG0, 3);
static MESON_GATE(g12a_eth_phy, HHI_GCLK_MPEG0, 4);
static MESON_GATE(g12a_isa, HHI_GCLK_MPEG0, 5);
static MESON_GATE(g12a_pl301, HHI_GCLK_MPEG0, 6);
static MESON_GATE(g12a_periphs, HHI_GCLK_MPEG0, 7);
//static MESON_GATE(g12a_spicc_0, HHI_GCLK_MPEG0, 8);
static MESON_GATE(g12a_i2c, HHI_GCLK_MPEG0, 9);
static MESON_GATE(g12a_sana, HHI_GCLK_MPEG0, 10);
static MESON_GATE(g12a_sd, HHI_GCLK_MPEG0, 11);
static MESON_GATE(g12a_rng0, HHI_GCLK_MPEG0, 12);
static MESON_GATE(g12a_uart0, HHI_GCLK_MPEG0, 13);
//static MESON_GATE(g12a_spicc_1, HHI_GCLK_MPEG0, 14);
static MESON_GATE(g12a_hiu_reg, HHI_GCLK_MPEG0, 19);
static MESON_GATE(g12a_mipi_dsi_phy, HHI_GCLK_MPEG0, 20);
static MESON_GATE(g12a_assist_misc, HHI_GCLK_MPEG0, 23);
static MESON_GATE(g12a_emmc_a, HHI_GCLK_MPEG0, 24);
static MESON_GATE(g12a_emmc_b, HHI_GCLK_MPEG0, 25);
static MESON_GATE(g12a_emmc_c, HHI_GCLK_MPEG0, 26);
static MESON_GATE(g12a_acodec, HHI_GCLK_MPEG0, 28);

static MESON_GATE(g12a_audio, HHI_GCLK_MPEG1, 0);
static MESON_GATE(g12a_eth_core, HHI_GCLK_MPEG1, 3);
static MESON_GATE(g12a_demux, HHI_GCLK_MPEG1, 4);
static MESON_GATE(g12a_aififo, HHI_GCLK_MPEG1, 11);
static MESON_GATE(g12a_adc, HHI_GCLK_MPEG1, 13);
static MESON_GATE(g12a_uart1, HHI_GCLK_MPEG1, 16);
static MESON_GATE(g12a_g2d, HHI_GCLK_MPEG1, 20);
static MESON_GATE(g12a_reset, HHI_GCLK_MPEG1, 23);
static MESON_GATE(g12a_pcie_comb, HHI_GCLK_MPEG1, 24);
static MESON_GATE(g12a_parser, HHI_GCLK_MPEG1, 25);
static MESON_GATE(g12a_usb_general, HHI_GCLK_MPEG1, 26);
static MESON_GATE(g12a_pcie_phy, HHI_GCLK_MPEG1, 27);
static MESON_GATE(g12a_ahb_arb0, HHI_GCLK_MPEG1, 29);

static MESON_GATE(g12a_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(g12a_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(g12a_htx_hdcp22, HHI_GCLK_MPEG2, 3);
static MESON_GATE(g12a_htx_pclk, HHI_GCLK_MPEG2, 4);
static MESON_GATE(g12a_bt656, HHI_GCLK_MPEG2, 6);
static MESON_GATE(g12a_usb1_to_ddr, HHI_GCLK_MPEG2, 8);
static MESON_GATE(g12a_mmc_pclk, HHI_GCLK_MPEG2, 11);
static MESON_GATE(g12a_uart2, HHI_GCLK_MPEG2, 15);
static MESON_GATE(g12a_vpu_intr, HHI_GCLK_MPEG2, 25);
static MESON_GATE(g12a_gic, HHI_GCLK_MPEG2, 30);

static MESON_GATE(g12a_vclk2_venci0, HHI_GCLK_OTHER, 1);
static MESON_GATE(g12a_vclk2_venci1, HHI_GCLK_OTHER, 2);
static MESON_GATE(g12a_vclk2_vencp0, HHI_GCLK_OTHER, 3);
static MESON_GATE(g12a_vclk2_vencp1, HHI_GCLK_OTHER, 4);
static MESON_GATE(g12a_vclk2_venct0, HHI_GCLK_OTHER, 5);
static MESON_GATE(g12a_vclk2_venct1, HHI_GCLK_OTHER, 6);
static MESON_GATE(g12a_vclk2_other, HHI_GCLK_OTHER, 7);
static MESON_GATE(g12a_vclk2_enci, HHI_GCLK_OTHER, 8);
static MESON_GATE(g12a_vclk2_encp, HHI_GCLK_OTHER, 9);
static MESON_GATE(g12a_dac_clk, HHI_GCLK_OTHER, 10);
static MESON_GATE(g12a_aoclk_gate, HHI_GCLK_OTHER, 14);
static MESON_GATE(g12a_iec958_gate, HHI_GCLK_OTHER, 16);
static MESON_GATE(g12a_enc480p, HHI_GCLK_OTHER, 20);
static MESON_GATE(g12a_rng1, HHI_GCLK_OTHER, 21);
static MESON_GATE(g12a_vclk2_enct, HHI_GCLK_OTHER, 22);
static MESON_GATE(g12a_vclk2_encl, HHI_GCLK_OTHER, 23);
static MESON_GATE(g12a_vclk2_venclmmc, HHI_GCLK_OTHER, 24);
static MESON_GATE(g12a_vclk2_vencl, HHI_GCLK_OTHER, 25);
static MESON_GATE(g12a_vclk2_other1, HHI_GCLK_OTHER, 26);
static MESON_GATE(g12a_efuse, HHI_GCLK_SP_MPEG, 1);

/* Array of all clocks provided by this provider */

static struct clk_hw *g12a_clk_hws[] = {
	//[CLKID_SYS_PLL]         = &g12a_sys_pll.hw,
	[CLKID_FIXED_PLL]       = &g12a_fixed_pll.hw,
	[CLKID_GP0_PLL]         = &g12a_gp0_pll.hw,
	[CLKID_HIFI_PLL]        = &g12a_hifi_pll.hw,
	[CLKID_PCIE_PLL]        = &g12a_pcie_pll.hw,
	[CLKID_FCLK_DIV2]       = &g12a_fclk_div2.hw,
	[CLKID_FCLK_DIV3]       = &g12a_fclk_div3.hw,
	[CLKID_FCLK_DIV4]       = &g12a_fclk_div4.hw,
	[CLKID_FCLK_DIV5]       = &g12a_fclk_div5.hw,
	[CLKID_FCLK_DIV7]       = &g12a_fclk_div7.hw,
	[CLKID_FCLK_DIV2P5]     = &g12a_fclk_div2p5.hw,
	[CLKID_MPEG_SEL]        = &g12a_mpeg_clk_sel.hw,
	[CLKID_MPEG_DIV]        = &g12a_mpeg_clk_div.hw,
	[CLKID_CLK81]           = &g12a_clk81.hw,

	[CLKID_MPLL0]           = &g12a_mpll0.hw,
	[CLKID_MPLL1]           = &g12a_mpll1.hw,
	[CLKID_MPLL2]           = &g12a_mpll2.hw,
	[CLKID_MPLL3]           = &g12a_mpll3.hw,

	[CLKID_DDR]             = &g12a_ddr.hw,
	[CLKID_DOS]             = &g12a_dos.hw,
	[CLKID_AUDIO_LOCKER]    = &g12a_alocker.hw,
	[CLKID_MIPI_DSI_HOST]   = &g12a_mipi_dsi_host.hw,
	[CLKID_ETH_PHY]         = &g12a_eth_phy.hw,
	[CLKID_ISA]             = &g12a_isa.hw,
	[CLKID_PL301]           = &g12a_pl301.hw,
	[CLKID_PERIPHS]         = &g12a_periphs.hw,
	[CLKID_SPICC0]          = &g12a_spicc_0.hw,
	[CLKID_I2C]             = &g12a_i2c.hw,
	[CLKID_SANA]            = &g12a_sana.hw,
	[CLKID_SD]              = &g12a_sd.hw,
	[CLKID_RNG0]            = &g12a_rng0.hw,
	[CLKID_UART0]           = &g12a_uart0.hw,
	[CLKID_SPICC1]          = &g12a_spicc_1.hw,
	[CLKID_HIU_REG]         = &g12a_hiu_reg.hw,
	[CLKID_MIPI_DSI_PHY]    = &g12a_mipi_dsi_phy.hw,
	[CLKID_ASSIST_MISC]     = &g12a_assist_misc.hw,
	[CLKID_SD_EMMC_A]       = &g12a_emmc_a.hw,
	[CLKID_SD_EMMC_B]       = &g12a_emmc_b.hw,
	[CLKID_SD_EMMC_C]       = &g12a_emmc_c.hw,
	[CLKID_ACODEC]          = &g12a_acodec.hw,
	[CLKID_AUDIO]           = &g12a_audio.hw,
	[CLKID_ETH_CORE]        = &g12a_eth_core.hw,
	[CLKID_DEMUX]           = &g12a_demux.hw,
	[CLKID_AIFIFO]          = &g12a_aififo.hw,
	[CLKID_ADC]             = &g12a_adc.hw,
	[CLKID_UART1]           = &g12a_uart1.hw,
	[CLKID_G2D]             = &g12a_g2d.hw,
	[CLKID_RESET]           = &g12a_reset.hw,
	[CLKID_PCIE_COMB]       = &g12a_pcie_comb.hw,
	[CLKID_DOS_PARSER]      = &g12a_parser.hw,
	[CLKID_USB_GENERAL]     = &g12a_usb_general.hw,
	[CLKID_PCIE_PHY]        = &g12a_pcie_phy.hw,
	[CLKID_AHB_ARB0]        = &g12a_ahb_arb0.hw,
	[CLKID_AHB_DATA_BUS]    = &g12a_ahb_data_bus.hw,
	[CLKID_AHB_CTRL_BUS]    = &g12a_ahb_ctrl_bus.hw,
	[CLKID_HTX_HDCP22]      = &g12a_htx_hdcp22.hw,
	[CLKID_HTX_PCLK]        = &g12a_htx_pclk.hw,
	[CLKID_BT656]           = &g12a_bt656.hw,
	[CLKID_USB1_TO_DDR]     = &g12a_usb1_to_ddr.hw,
	[CLKID_MMC_PCLK]        = &g12a_mmc_pclk.hw,
	[CLKID_UART2]           = &g12a_uart2.hw,
	[CLKID_VPU_INTR]        = &g12a_vpu_intr.hw,
	[CLKID_GIC]             = &g12a_gic.hw,
	[CLKID_VCLK2_VENCI0]    = &g12a_vclk2_venci0.hw,
	[CLKID_VCLK2_VENCI1]    = &g12a_vclk2_venci1.hw,
	[CLKID_VCLK2_VENCP0]    = &g12a_vclk2_vencp0.hw,
	[CLKID_VCLK2_VENCP1]    = &g12a_vclk2_vencp1.hw,
	[CLKID_VCLK2_VENCT0]    = &g12a_vclk2_venct0.hw,
	[CLKID_VCLK2_VENCT1]    = &g12a_vclk2_venct1.hw,
	[CLKID_VCLK2_OTHER]     = &g12a_vclk2_other.hw,
	[CLKID_VCLK2_ENCI]      = &g12a_vclk2_enci.hw,
	[CLKID_VCLK2_ENCP]      = &g12a_vclk2_encp.hw,
	[CLKID_DAC_CLK]         = &g12a_dac_clk.hw,
	[CLKID_AOCLK_GATE]      = &g12a_aoclk_gate.hw,
	[CLKID_IEC958_GATE]     = &g12a_iec958_gate.hw,
	[CLKID_ENC480P]         = &g12a_enc480p.hw,
	[CLKID_RNG1]            = &g12a_rng1.hw,
	[CLKID_VCLK2_ENCT]      = &g12a_vclk2_enct.hw,
	[CLKID_VCLK2_ENCL]      = &g12a_vclk2_encl.hw,
	[CLKID_VCLK2_VENCLMMC]  = &g12a_vclk2_venclmmc.hw,
	[CLKID_VCLK2_VENCL]     = &g12a_vclk2_vencl.hw,
	[CLKID_VCLK2_OTHER1]    = &g12a_vclk2_other1.hw,
	[CLKID_EFUSE]           = &g12a_efuse.hw,

	[CLKID_CPU_FCLK_P]      = &g12a_cpu_fclk_p.hw,
	[CLKID_CPU_CLK]         = &g12a_cpu_clk.mux.hw,

	[CLKID_PCIE_PLL]        = &g12a_pcie_pll.hw,
	[CLKID_24M]             = &g12a_24m.hw,
	[CLKID_12M_DIV]         = &g12a_12m_div.hw,
	[CLKID_12M_GATE]        = &g12a_12m_gate.hw,
	[CLKID_GEN_CLK_SEL]	= &g12a_gen_clk_sel.hw,
	[CLKID_GEN_CLK_DIV]	= &g12a_gen_clk_div.hw,
	[CLKID_GEN_CLK]		= &g12a_gen_clk.hw,

};
/* Convenience tables to populate base addresses in .probe */

static struct meson_clk_pll *const g12a_clk_plls[] = {
	&g12a_fixed_pll,
	&g12a_gp0_pll,
	&g12a_hifi_pll,
	&g12a_pcie_pll,
};

static struct meson_clk_mpll *const g12a_clk_mplls[] = {
	&g12a_mpll0,
	&g12a_mpll1,
	&g12a_mpll2,
	&g12a_mpll3,
};

static struct clk_gate *g12a_clk_gates[] = {
	&g12a_clk81,
	&g12a_ddr,
	&g12a_dos,
	&g12a_alocker,
	&g12a_mipi_dsi_host,
	&g12a_eth_phy,
	&g12a_isa,
	&g12a_pl301,
	&g12a_periphs,
	&g12a_spicc_0,
	&g12a_i2c,
	&g12a_sana,
	&g12a_sd,
	&g12a_rng0,
	&g12a_uart0,
	&g12a_spicc_1,
	&g12a_hiu_reg,
	&g12a_mipi_dsi_phy,
	&g12a_assist_misc,
	&g12a_emmc_a,
	&g12a_emmc_b,
	&g12a_emmc_c,
	&g12a_acodec,
	&g12a_audio,
	&g12a_eth_core,
	&g12a_demux,
	&g12a_aififo,
	&g12a_adc,
	&g12a_uart1,
	&g12a_g2d,
	&g12a_reset,
	&g12a_pcie_comb,
	&g12a_parser,
	&g12a_usb_general,
	&g12a_pcie_phy,
	&g12a_ahb_arb0,
	&g12a_ahb_data_bus,
	&g12a_ahb_ctrl_bus,
	&g12a_htx_hdcp22,
	&g12a_htx_pclk,
	&g12a_bt656,
	&g12a_usb1_to_ddr,
	&g12a_mmc_pclk,
	&g12a_uart2,
	&g12a_vpu_intr,
	&g12a_gic,
	&g12a_vclk2_venci0,
	&g12a_vclk2_venci1,
	&g12a_vclk2_vencp0,
	&g12a_vclk2_vencp1,
	&g12a_vclk2_venct0,
	&g12a_vclk2_venct1,
	&g12a_vclk2_other,
	&g12a_vclk2_enci,
	&g12a_vclk2_encp,
	&g12a_dac_clk,
	&g12a_aoclk_gate,
	&g12a_iec958_gate,
	&g12a_enc480p,
	&g12a_rng1,
	&g12a_vclk2_enct,
	&g12a_vclk2_encl,
	&g12a_vclk2_venclmmc,
	&g12a_vclk2_vencl,
	&g12a_vclk2_other1,
	&g12a_efuse,
	&g12a_24m,
	&g12a_12m_gate,
	&g12a_gen_clk,
};

static void __init g12a_clkc_init(struct device_node *np)
{
	int ret = 0, clkid, i;
	struct clk_hw *parent_hw;
	struct clk *parent_clk;

	meson_cpu_version_init();

	/*  Generic clocks and PLLs */
	if (!clk_base)
		clk_base = of_iomap(np, 0);

	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		return;
	}
	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(g12a_clk_plls); i++)
		g12a_clk_plls[i]->base = clk_base;

	if (is_meson_g12b_cpu())
		g12b_sys1_pll.base = clk_base;
	else
		g12a_sys_pll.base = clk_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(g12a_clk_mplls); i++)
		g12a_clk_mplls[i]->base = clk_base;

	/* Populate the base address for CPU clk */
	if (is_meson_g12b_cpu()) {
		g12a_clk_hws[CLKID_CPU_CLK] = &g12b_cpu_clk1.mux.hw;
		g12b_cpu_clk1.base = clk_base;
		g12b_cpu_clk1.mux.reg = clk_base
					+ (unsigned long)g12b_cpu_clk1.mux.reg;
	} else {
		g12a_cpu_clk.base = clk_base;
		g12a_cpu_clk.mux.reg = clk_base
					+ (unsigned long)g12a_cpu_clk.mux.reg;
	}

	g12a_cpu_fclk_p.reg = clk_base
					+ (unsigned long)g12a_cpu_fclk_p.reg;

	/* Populate the base address for the MPEG clks */
	g12a_mpeg_clk_sel.reg = clk_base
					+ (unsigned long)g12a_mpeg_clk_sel.reg;
	g12a_mpeg_clk_div.reg = clk_base
					+ (unsigned long)g12a_mpeg_clk_div.reg;

	g12a_12m_div.reg = clk_base
					+ (unsigned long)g12a_12m_div.reg;
	g12a_gen_clk_sel.reg = clk_base
					+ (unsigned long)g12a_gen_clk_sel.reg;
	g12a_gen_clk_div.reg = clk_base
					+ (unsigned long)g12a_gen_clk_div.reg;
	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(g12a_clk_gates); i++)
		g12a_clk_gates[i]->reg = clk_base +
			(unsigned long)g12a_clk_gates[i]->reg;

	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			/* pr_err("%s: alloc clks fail!", __func__); */
			/* return -ENOMEM; */
			return;
		}
		clk_numbers = NR_CLKS;
	}

	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;
	/*
	 * register all clks
	 */

	if (is_meson_g12b_cpu()) {
		clks[CLKID_SYS1_PLL] = clk_register(NULL, &g12b_sys1_pll.hw);
		if (IS_ERR(clks[CLKID_SYS1_PLL])) {
			pr_err("%s: failed to register %s\n", __func__,
				clk_hw_get_name(&g12b_sys1_pll.hw));
			goto iounmap;
		}
	} else
		g12a_clk_hws[CLKID_SYS_PLL] = &g12a_sys_pll.hw;

	for (clkid = 0; clkid < ARRAY_SIZE(g12a_clk_hws); clkid++) {
		if (g12a_clk_hws[clkid]) {
			clks[clkid] = clk_register(NULL, g12a_clk_hws[clkid]);
			if (IS_ERR(clks[clkid])) {
				pr_err("%s: failed to register %s\n", __func__,
					clk_hw_get_name(g12a_clk_hws[clkid]));
				goto iounmap;
			}
		}
	}

	meson_g12a_sdemmc_init();
	meson_g12a_gpu_init();
	meson_g12a_media_init();
	meson_g12a_misc_init();

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
	if (is_meson_g12b_cpu()) {
		parent_hw = clk_hw_get_parent(&g12b_cpu_clk1.mux.hw);
		parent_clk = parent_hw->clk;
		ret = clk_notifier_register(parent_clk, &g12b_cpu_clk1.clk_nb);
	} else {
		parent_hw = clk_hw_get_parent(&g12a_cpu_clk.mux.hw);
		parent_clk = parent_hw->clk;
		ret = clk_notifier_register(parent_clk, &g12a_cpu_clk.clk_nb);
	}

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

CLK_OF_DECLARE(g12a, "amlogic,g12a-clkc", g12a_clkc_init);
CLK_OF_DECLARE(g12b, "amlogic,g12b-clkc-1", g12a_clkc_init);

