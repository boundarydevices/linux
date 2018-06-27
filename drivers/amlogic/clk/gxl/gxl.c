/*
 * drivers/amlogic/clk/gxl/gxl.c
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

DEFINE_SPINLOCK(clk_lock);
struct clk **clks;
int clk_numbers;
static struct clk_onecell_data clk_data;
void __iomem *clk_base;
/* #undef pr_debug */
/* #define pr_debug pr_info */

static const struct pll_rate_table sys_pll_rate_table[] = {
	PLL_RATE(24000000, 56, 1, 2),
	PLL_RATE(48000000, 64, 1, 2),
	PLL_RATE(72000000, 72, 1, 2),
	PLL_RATE(96000000, 64, 1, 2),
	PLL_RATE(120000000, 80, 1, 2),
	PLL_RATE(144000000, 96, 1, 2),
	PLL_RATE(168000000, 56, 1, 1),
	PLL_RATE(192000000, 64, 1, 1),
	PLL_RATE(216000000, 72, 1, 1),
	PLL_RATE(240000000, 80, 1, 1),
	PLL_RATE(264000000, 88, 1, 1),
	PLL_RATE(288000000, 96, 1, 1),
	PLL_RATE(312000000, 52, 1, 2),
	PLL_RATE(336000000, 56, 1, 2),
	PLL_RATE(360000000, 60, 1, 2),
	PLL_RATE(384000000, 64, 1, 2),
	PLL_RATE(408000000, 68, 1, 2),
	PLL_RATE(432000000, 72, 1, 2),
	PLL_RATE(456000000, 76, 1, 2),
	PLL_RATE(480000000, 80, 1, 2),
	PLL_RATE(504000000, 84, 1, 2),
	PLL_RATE(528000000, 88, 1, 2),
	PLL_RATE(552000000, 92, 1, 2),
	PLL_RATE(576000000, 96, 1, 2),
	PLL_RATE(600000000, 50, 1, 1),
	PLL_RATE(624000000, 52, 1, 1),
	PLL_RATE(648000000, 54, 1, 1),
	PLL_RATE(672000000, 56, 1, 1),
	PLL_RATE(696000000, 58, 1, 1),
	PLL_RATE(720000000, 60, 1, 1),
	PLL_RATE(744000000, 62, 1, 1),
	PLL_RATE(768000000, 64, 1, 1),
	PLL_RATE(792000000, 66, 1, 1),
	PLL_RATE(816000000, 68, 1, 1),
	PLL_RATE(840000000, 70, 1, 1),
	PLL_RATE(864000000, 72, 1, 1),
	PLL_RATE(888000000, 74, 1, 1),
	PLL_RATE(912000000, 76, 1, 1),
	PLL_RATE(936000000, 78, 1, 1),
	PLL_RATE(960000000, 80, 1, 1),
	PLL_RATE(984000000, 82, 1, 1),
	PLL_RATE(1008000000, 84, 1, 1),
	PLL_RATE(1032000000, 86, 1, 1),
	PLL_RATE(1056000000, 88, 1, 1),
	PLL_RATE(1080000000, 90, 1, 1),
	PLL_RATE(1104000000, 92, 1, 1),
	PLL_RATE(1128000000, 94, 1, 1),
	PLL_RATE(1152000000, 96, 1, 1),
	PLL_RATE(1176000000, 98, 1, 1),
	PLL_RATE(1200000000, 50, 1, 0),
	PLL_RATE(1224000000, 51, 1, 0),
	PLL_RATE(1248000000, 52, 1, 0),
	PLL_RATE(1272000000, 53, 1, 0),
	PLL_RATE(1296000000, 54, 1, 0),
	PLL_RATE(1320000000, 55, 1, 0),
	PLL_RATE(1344000000, 56, 1, 0),
	PLL_RATE(1368000000, 57, 1, 0),
	PLL_RATE(1392000000, 58, 1, 0),
	PLL_RATE(1416000000, 59, 1, 0),
	PLL_RATE(1440000000, 60, 1, 0),
	PLL_RATE(1464000000, 61, 1, 0),
	PLL_RATE(1488000000, 62, 1, 0),
	PLL_RATE(1512000000, 63, 1, 0),
	PLL_RATE(1536000000, 64, 1, 0),
	PLL_RATE(1560000000, 65, 1, 0),
	PLL_RATE(1584000000, 66, 1, 0),
	PLL_RATE(1608000000, 67, 1, 0),
	PLL_RATE(1632000000, 68, 1, 0),
	PLL_RATE(1656000000, 68, 1, 0),
	PLL_RATE(1680000000, 68, 1, 0),
	PLL_RATE(1704000000, 68, 1, 0),
	PLL_RATE(1728000000, 69, 1, 0),
	PLL_RATE(1752000000, 69, 1, 0),
	PLL_RATE(1776000000, 69, 1, 0),
	PLL_RATE(1800000000, 69, 1, 0),
	PLL_RATE(1824000000, 70, 1, 0),
	PLL_RATE(1848000000, 70, 1, 0),
	PLL_RATE(1872000000, 70, 1, 0),
	PLL_RATE(1896000000, 70, 1, 0),
	PLL_RATE(1920000000, 71, 1, 0),
	PLL_RATE(1944000000, 71, 1, 0),
	PLL_RATE(1968000000, 71, 1, 0),
	PLL_RATE(1992000000, 71, 1, 0),
	PLL_RATE(2016000000, 72, 1, 0),
	PLL_RATE(2040000000, 72, 1, 0),
	PLL_RATE(2064000000, 72, 1, 0),
	PLL_RATE(2088000000, 72, 1, 0),
	PLL_RATE(2112000000, 73, 1, 0),
	{ /* sentinel */ },
};

/*GXL: gp0_pll: Fdco: 0.96G~1.632G Fdco = 24*(M+frac)/N
 *N: recommend is 1
 *gp0_clk_out = Fdco /Fod
 *od=0 Fod=1 od=1 Fod=2 od=2 Fod=4 od=3 Fod=4
 */
static const struct pll_rate_table gxl_gp0_pll_rate_table[] = {
	PLL_RATE(240000000, 40, 1, 2),
	PLL_RATE(246000000, 41, 1, 2),
	PLL_RATE(252000000, 42, 1, 2),
	PLL_RATE(258000000, 43, 1, 2),
	PLL_RATE(264000000, 44, 1, 2),
	PLL_RATE(270000000, 45, 1, 2),
	PLL_RATE(276000000, 46, 1, 2),
	PLL_RATE(282000000, 47, 1, 2),
	PLL_RATE(288000000, 48, 1, 2),
	PLL_RATE(294000000, 49, 1, 2),
	PLL_RATE(300000000, 50, 1, 2),
	PLL_RATE(306000000, 51, 1, 2),
	PLL_RATE(312000000, 52, 1, 2),
	PLL_RATE(318000000, 53, 1, 2),
	PLL_RATE(324000000, 54, 1, 2),
	PLL_RATE(330000000, 55, 1, 2),
	PLL_RATE(336000000, 56, 1, 2),
	PLL_RATE(342000000, 57, 1, 2),
	PLL_RATE(348000000, 58, 1, 2),
	PLL_RATE(354000000, 59, 1, 2),
	PLL_RATE(360000000, 60, 1, 2),
	PLL_RATE(366000000, 61, 1, 2),
	PLL_RATE(372000000, 62, 1, 2),
	PLL_RATE(378000000, 63, 1, 2),
	PLL_RATE(384000000, 64, 1, 2),
	PLL_RATE(390000000, 65, 1, 3),
	PLL_RATE(396000000, 66, 1, 3),
	PLL_RATE(402000000, 67, 1, 3),
	PLL_RATE(408000000, 68, 1, 3),
	PLL_RATE(480000000, 40, 1, 1),
	PLL_RATE(492000000, 41, 1, 1),
	PLL_RATE(504000000, 42, 1, 1),
	PLL_RATE(516000000, 43, 1, 1),
	PLL_RATE(528000000, 44, 1, 1),
	PLL_RATE(540000000, 45, 1, 1),
	PLL_RATE(552000000, 46, 1, 1),
	PLL_RATE(564000000, 47, 1, 1),
	PLL_RATE(576000000, 48, 1, 1),
	PLL_RATE(588000000, 49, 1, 1),
	PLL_RATE(600000000, 50, 1, 1),
	PLL_RATE(612000000, 51, 1, 1),
	PLL_RATE(624000000, 52, 1, 1),
	PLL_RATE(636000000, 53, 1, 1),
	PLL_RATE(648000000, 54, 1, 1),
	PLL_RATE(660000000, 55, 1, 1),
	PLL_RATE(672000000, 56, 1, 1),
	PLL_RATE(684000000, 57, 1, 1),
	PLL_RATE(696000000, 58, 1, 1),
	PLL_RATE(708000000, 59, 1, 1),
	PLL_RATE(720000000, 60, 1, 1),
	PLL_RATE(732000000, 61, 1, 1),
	PLL_RATE(744000000, 62, 1, 1),
	PLL_RATE(756000000, 63, 1, 1),
	PLL_RATE(768000000, 64, 1, 1),
	PLL_RATE(780000000, 65, 1, 1),
	PLL_RATE(792000000, 66, 1, 1),
	PLL_RATE(804000000, 67, 1, 1),
	PLL_RATE(816000000, 68, 1, 1),
	PLL_RATE(960000000, 40, 1, 0),
	PLL_RATE(984000000, 41, 1, 0),
	PLL_RATE(1008000000, 42, 1, 0),
	PLL_RATE(1032000000, 43, 1, 0),
	PLL_RATE(1056000000, 44, 1, 0),
	PLL_RATE(1080000000, 45, 1, 0),
	PLL_RATE(1104000000, 46, 1, 0),
	PLL_RATE(1128000000, 47, 1, 0),
	PLL_RATE(1152000000, 48, 1, 0),
	PLL_RATE(1176000000, 49, 1, 0),
	PLL_RATE(1200000000, 50, 1, 0),
	PLL_RATE(1224000000, 51, 1, 0),
	PLL_RATE(1248000000, 52, 1, 0),
	PLL_RATE(1272000000, 53, 1, 0),
	PLL_RATE(1296000000, 54, 1, 0),
	PLL_RATE(1320000000, 55, 1, 0),
	PLL_RATE(1344000000, 56, 1, 0),
	PLL_RATE(1368000000, 57, 1, 0),
	PLL_RATE(1392000000, 58, 1, 0),
	PLL_RATE(1416000000, 59, 1, 0),
	PLL_RATE(1440000000, 60, 1, 0),
	PLL_RATE(1464000000, 61, 1, 0),
	PLL_RATE(1488000000, 62, 1, 0),
	PLL_RATE(1512000000, 63, 1, 0),
	PLL_RATE(1536000000, 64, 1, 0),
	PLL_RATE(1560000000, 65, 1, 0),
	PLL_RATE(1584000000, 66, 1, 0),
	PLL_RATE(1608000000, 67, 1, 0),
	PLL_RATE(1632000000, 68, 1, 0),
	{ /* sentinel */ },
};

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

static struct meson_clk_pll gxl_fixed_pll = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_clk_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll gxl_hdmi_pll = {
	.m = {
		.reg_off = HHI_HDMI_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_HDMI_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.frac = {
		.reg_off = HHI_HDMI_PLL_CNTL2,
		.shift   = 0,
		.width   = 12,
	},
	.od = {
		.reg_off = HHI_HDMI_PLL_CNTL2,
		.shift   = 16,
		.width   = 2,
	},
	.od2 = {
		.reg_off = HHI_HDMI_PLL_CNTL2,
		.shift   = 22,
		.width   = 2,
	},
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll",
		.ops = &meson_clk_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll gxl_sys_pll = {
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
		.shift   = 10,
		.width   = 2,
	},
	.rate_table = sys_pll_rate_table,
	.rate_count = ARRAY_SIZE(sys_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll gxl_gp0_pll = {
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
	.rate_table = gxl_gp0_pll_rate_table,
	.rate_count = ARRAY_SIZE(gxl_gp0_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor gxl_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor gxl_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor gxl_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor gxl_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor gxl_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll gxl_mpll0 = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll gxl_mpll1 = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll gxl_mpll2 = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll gxl_mpll3 = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &meson_clk_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

/*
 * FIXME cpu clocks and the legacy composite clocks (e.g. clk81) are both PLL
 * post-dividers and should be modelled with their respective PLLs via the
 * forthcoming coordinated clock rates feature
 */
static u32 mux_table_cpu_p00[]	= { 0, 1, 2 };
static struct clk_mux gxl_cpu_fixedpll_p00 = {
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

static struct clk_divider gxl_cpu_fixedpll_p01 = {
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
static struct clk_mux gxl_cpu_fixedpll_p0 = {
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
static struct clk_mux gxl_cpu_fixedpll_p10 = {
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

static struct clk_divider gxl_cpu_fixedpll_p11 = {
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
static struct clk_mux gxl_cpu_fixedpll_p1 = {
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
static struct clk_mux gxl_cpu_fixedpll_p = {
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
static struct meson_clk_cpu gxl_cpu_clk = {
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

static struct clk_mux gxl_mpeg_clk_sel = {
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

static struct clk_divider gxl_mpeg_clk_div = {
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
static struct clk_gate gxl_clk81 = {
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

static struct clk_gate gxl_spicc = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gxl_spicc",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

/* Everything Else (EE) domain gates */
static MESON_GATE(gxl_ddr, HHI_GCLK_MPEG0, 0);
static MESON_GATE(gxl_dos, HHI_GCLK_MPEG0, 1);
static MESON_GATE(gxl_isa, HHI_GCLK_MPEG0, 5);
static MESON_GATE(gxl_pl301, HHI_GCLK_MPEG0, 6);
static MESON_GATE(gxl_periphs, HHI_GCLK_MPEG0, 7);
//static MESON_GATE(gxl_spicc, HHI_GCLK_MPEG0, 8);
static MESON_GATE(gxl_i2c, HHI_GCLK_MPEG0, 9);
static MESON_GATE(gxl_sana, HHI_GCLK_MPEG0, 10);
static MESON_GATE(gxl_smart_card, HHI_GCLK_MPEG0, 11);
static MESON_GATE(gxl_rng0, HHI_GCLK_MPEG0, 12);
static MESON_GATE(gxl_uart0, HHI_GCLK_MPEG0, 13);
static MESON_GATE(gxl_sdhc, HHI_GCLK_MPEG0, 14);
static MESON_GATE(gxl_stream, HHI_GCLK_MPEG0, 15);
static MESON_GATE(gxl_async_fifo, HHI_GCLK_MPEG0, 16);
static MESON_GATE(gxl_sdio, HHI_GCLK_MPEG0, 17);
static MESON_GATE(gxl_abuf, HHI_GCLK_MPEG0, 18);
static MESON_GATE(gxl_hiu_iface, HHI_GCLK_MPEG0, 19);
static MESON_GATE(gxl_bt656, HHI_GCLK_MPEG0, 22);
static MESON_GATE(gxl_assist_misc, HHI_GCLK_MPEG0, 23);
static MESON_GATE(gxl_emmc_a, HHI_GCLK_MPEG0, 24);
static MESON_GATE(gxl_emmc_b, HHI_GCLK_MPEG0, 25);
static MESON_GATE(gxl_emmc_c, HHI_GCLK_MPEG0, 26);
static MESON_GATE(gxl_dma, HHI_GCLK_MPEG0, 27);
static MESON_GATE(gxl_acodec, HHI_GCLK_MPEG0, 28);
static MESON_GATE(gxl_spi, HHI_GCLK_MPEG0, 30);

static MESON_GATE(gxl_pclk_tvfe, HHI_GCLK_MPEG1, 0);
static MESON_GATE(gxl_i2s_spdif, HHI_GCLK_MPEG1, 2);
static MESON_GATE(gxl_eth, HHI_GCLK_MPEG1, 3);
static MESON_GATE(gxl_demux, HHI_GCLK_MPEG1, 4);
static MESON_GATE(gxl_aiu_glue, HHI_GCLK_MPEG1, 6);
static MESON_GATE(gxl_iec958, HHI_GCLK_MPEG1, 7);
static MESON_GATE(gxl_i2s_out, HHI_GCLK_MPEG1, 8);
static MESON_GATE(gxl_amclk, HHI_GCLK_MPEG1, 9);
static MESON_GATE(gxl_aififo2, HHI_GCLK_MPEG1, 10);
static MESON_GATE(gxl_mixer, HHI_GCLK_MPEG1, 11);
static MESON_GATE(gxl_mixer_iface, HHI_GCLK_MPEG1, 12);
static MESON_GATE(gxl_adc, HHI_GCLK_MPEG1, 13);
static MESON_GATE(gxl_blkmv, HHI_GCLK_MPEG1, 14);
static MESON_GATE(gxl_aiu_top, HHI_GCLK_MPEG1, 15);
static MESON_GATE(gxl_uart1, HHI_GCLK_MPEG1, 16);
static MESON_GATE(gxl_g2d, HHI_GCLK_MPEG1, 20);
static MESON_GATE(gxl_usb0, HHI_GCLK_MPEG1, 21);
static MESON_GATE(gxl_usb1, HHI_GCLK_MPEG1, 22);
static MESON_GATE(gxl_reset, HHI_GCLK_MPEG1, 23);
static MESON_GATE(gxl_nand, HHI_GCLK_MPEG1, 24);
static MESON_GATE(gxl_dos_parser, HHI_GCLK_MPEG1, 25);
static MESON_GATE(gxl_usb_general, HHI_GCLK_MPEG1, 26);
static MESON_GATE(gxl_vdin1, HHI_GCLK_MPEG1, 28);
static MESON_GATE(gxl_ahb_arb0, HHI_GCLK_MPEG1, 29);
static MESON_GATE(gxl_efuse, HHI_GCLK_MPEG1, 30);
static MESON_GATE(gxl_boot_rom, HHI_GCLK_MPEG1, 31);

static MESON_GATE(gxl_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(gxl_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(gxl_hdcp22_pclk, HHI_GCLK_MPEG2, 3);
static MESON_GATE(gxl_hdmitx_pclk, HHI_GCLK_MPEG2, 4);
static MESON_GATE(gxl_pdm_pclk, HHI_GCLK_MPEG2, 5);
static MESON_GATE(gxl_bt656_pclk, HHI_GCLK_MPEG2, 6);
static MESON_GATE(gxl_usb1_to_ddr, HHI_GCLK_MPEG2, 8);
static MESON_GATE(gxl_usb0_to_ddr, HHI_GCLK_MPEG2, 9);
static MESON_GATE(gxl_aiu_pclk, HHI_GCLK_MPEG2, 10);
static MESON_GATE(gxl_mmc_pclk, HHI_GCLK_MPEG2, 11);
static MESON_GATE(gxl_dvin, HHI_GCLK_MPEG2, 12);
static MESON_GATE(gxl_uart2, HHI_GCLK_MPEG2, 15);
static MESON_GATE(gxl_saradc, HHI_GCLK_MPEG2, 22);
static MESON_GATE(gxl_vpu_intr, HHI_GCLK_MPEG2, 25);
static MESON_GATE(gxl_sec_ahb_ahb3_bridge, HHI_GCLK_MPEG2, 26);
static MESON_GATE(gxl_apb3_ao, HHI_GCLK_MPEG2, 27);
static MESON_GATE(gxl_mclk_tvfe, HHI_GCLK_MPEG2, 28);
static MESON_GATE(gxl_clk81_gic, HHI_GCLK_MPEG2, 30);

static MESON_GATE(gxl_vclk2_venci0, HHI_GCLK_OTHER, 1);
static MESON_GATE(gxl_vclk2_venci1, HHI_GCLK_OTHER, 2);
static MESON_GATE(gxl_vclk2_vencp0, HHI_GCLK_OTHER, 3);
static MESON_GATE(gxl_vclk2_vencp1, HHI_GCLK_OTHER, 4);
static MESON_GATE(gxl_vclk2_venct0, HHI_GCLK_OTHER, 5);
static MESON_GATE(gxl_vclk2_venct1, HHI_GCLK_OTHER, 6);
static MESON_GATE(gxl_vclk2_other, HHI_GCLK_OTHER, 7);
static MESON_GATE(gxl_vclk2_enci, HHI_GCLK_OTHER, 8);
static MESON_GATE(gxl_vclk2_encp, HHI_GCLK_OTHER, 9);
static MESON_GATE(gxl_dac_clk, HHI_GCLK_OTHER, 10);
static MESON_GATE(gxl_aoclk_gate, HHI_GCLK_OTHER, 14);
static MESON_GATE(gxl_iec958_gate, HHI_GCLK_OTHER, 16);
static MESON_GATE(gxl_enc480p, HHI_GCLK_OTHER, 20);
static MESON_GATE(gxl_rng1, HHI_GCLK_OTHER, 21);
static MESON_GATE(gxl_vclk2_enct, HHI_GCLK_OTHER, 22);
static MESON_GATE(gxl_vclk2_encl, HHI_GCLK_OTHER, 23);
static MESON_GATE(gxl_vclk2_venclmmc, HHI_GCLK_OTHER, 24);
static MESON_GATE(gxl_vclk2_vencl, HHI_GCLK_OTHER, 25);
static MESON_GATE(gxl_vclk2_other1, HHI_GCLK_OTHER, 26);
static MESON_GATE(gxl_edp, HHI_GCLK_OTHER, 31);

/* Always On (AO) domain gates */

static MESON_GATE(gxl_ao_media_cpu, HHI_GCLK_AO, 0);
static MESON_GATE(gxl_ao_ahb_sram, HHI_GCLK_AO, 1);
static MESON_GATE(gxl_ao_ahb_bus, HHI_GCLK_AO, 2);
static MESON_GATE(gxl_ao_iface, HHI_GCLK_AO, 3);
static MESON_GATE(gxl_ao_i2c, HHI_GCLK_AO, 4);

/* Array of all clocks provided by this provider */

static struct clk_hw *gxl_clk_hws[] = {
	[CLKID_SYS_PLL]		    = &gxl_sys_pll.hw,
	[CLKID_HDMI_PLL]	    = &gxl_hdmi_pll.hw,
	[CLKID_FIXED_PLL]	    = &gxl_fixed_pll.hw,
	[CLKID_FCLK_DIV2]	    = &gxl_fclk_div2.hw,
	[CLKID_FCLK_DIV3]	    = &gxl_fclk_div3.hw,
	[CLKID_FCLK_DIV4]	    = &gxl_fclk_div4.hw,
	[CLKID_FCLK_DIV5]	    = &gxl_fclk_div5.hw,
	[CLKID_FCLK_DIV7]	    = &gxl_fclk_div7.hw,
	[CLKID_GP0_PLL]		    = &gxl_gp0_pll.hw,
	[CLKID_MPEG_SEL]	    = &gxl_mpeg_clk_sel.hw,
	[CLKID_MPEG_DIV]	    = &gxl_mpeg_clk_div.hw,
	[CLKID_CLK81]		    = &gxl_clk81.hw,
	[CLKID_MPLL0]		    = &gxl_mpll0.hw,
	[CLKID_MPLL1]		    = &gxl_mpll1.hw,
	[CLKID_MPLL2]		    = &gxl_mpll2.hw,
	[CLKID_MPLL3]		    = &gxl_mpll3.hw,
	[CLKID_DDR]		    = &gxl_ddr.hw,
	[CLKID_DOS]		    = &gxl_dos.hw,
	[CLKID_ISA]		    = &gxl_isa.hw,
	[CLKID_PL301]		    = &gxl_pl301.hw,
	[CLKID_PERIPHS]		    = &gxl_periphs.hw,
	[CLKID_SPICC]		    = &gxl_spicc.hw,
	[CLKID_I2C]		    = &gxl_i2c.hw,
	[CLKID_SANA]		    = &gxl_sana.hw,
	[CLKID_SMART_CARD]	    = &gxl_smart_card.hw,
	[CLKID_RNG0]		    = &gxl_rng0.hw,
	[CLKID_UART0]		    = &gxl_uart0.hw,
	[CLKID_SDHC]		    = &gxl_sdhc.hw,
	[CLKID_STREAM]		    = &gxl_stream.hw,
	[CLKID_ASYNC_FIFO]	    = &gxl_async_fifo.hw,
	[CLKID_SDIO]		    = &gxl_sdio.hw,
	[CLKID_ABUF]		    = &gxl_abuf.hw,
	[CLKID_HIU_IFACE]	    = &gxl_hiu_iface.hw,
	[CLKID_BT656]	    = &gxl_bt656.hw,
	[CLKID_ASSIST_MISC]	    = &gxl_assist_misc.hw,
	[CLKID_SD_EMMC_A]	    = &gxl_emmc_a.hw,
	[CLKID_SD_EMMC_B]	    = &gxl_emmc_b.hw,
	[CLKID_SD_EMMC_C]	    = &gxl_emmc_c.hw,
	[CLKID_SD_EMMC_C]	    = &gxl_emmc_c.hw,
	[CLKID_DMA]	    = &gxl_dma.hw,
	[CLKID_ACODEC]	    = &gxl_acodec.hw,
	[CLKID_SPI]		    = &gxl_spi.hw,
	[CLKID_PCLK_TVFE]	    = &gxl_pclk_tvfe.hw,
	[CLKID_I2S_SPDIF]	    = &gxl_i2s_spdif.hw,
	[CLKID_ETH]		    = &gxl_eth.hw,
	[CLKID_DEMUX]		    = &gxl_demux.hw,
	[CLKID_AIU_GLUE]	    = &gxl_aiu_glue.hw,
	[CLKID_IEC958]		    = &gxl_iec958.hw,
	[CLKID_I2S_OUT]		    = &gxl_i2s_out.hw,
	[CLKID_AMCLK]		    = &gxl_amclk.hw,
	[CLKID_AIFIFO2]		    = &gxl_aififo2.hw,
	[CLKID_MIXER]		    = &gxl_mixer.hw,
	[CLKID_MIXER_IFACE]	    = &gxl_mixer_iface.hw,
	[CLKID_ADC]		    = &gxl_adc.hw,
	[CLKID_BLKMV]		    = &gxl_blkmv.hw,
	[CLKID_AIU_TOP]		    = &gxl_aiu_top.hw,
	[CLKID_UART1]		    = &gxl_uart1.hw,
	[CLKID_G2D]		    = &gxl_g2d.hw,
	[CLKID_USB0]		    = &gxl_usb0.hw,
	[CLKID_USB1]		    = &gxl_usb1.hw,
	[CLKID_RESET]		    = &gxl_reset.hw,
	[CLKID_NAND]		    = &gxl_nand.hw,
	[CLKID_DOS_PARSER]	    = &gxl_dos_parser.hw,
	[CLKID_USB_GENERAL]		    = &gxl_usb_general.hw,
	[CLKID_VDIN1]		    = &gxl_vdin1.hw,
	[CLKID_AHB_ARB0]	    = &gxl_ahb_arb0.hw,
	[CLKID_EFUSE]		    = &gxl_efuse.hw,
	[CLKID_BOOT_ROM]	    = &gxl_boot_rom.hw,
	[CLKID_AHB_DATA_BUS]	    = &gxl_ahb_data_bus.hw,
	[CLKID_AHB_CTRL_BUS]	    = &gxl_ahb_ctrl_bus.hw,
	[CLKID_HDCP22_PCLK]	    = &gxl_hdcp22_pclk.hw,
	[CLKID_HDMITX_PCLK]	    = &gxl_hdmitx_pclk.hw,
	[CLKID_PDM_PCLK]	    = &gxl_pdm_pclk.hw,
	[CLKID_BT656_PCLK1]	    = &gxl_bt656_pclk.hw,
	[CLKID_USB1_TO_DDR]	    = &gxl_usb1_to_ddr.hw,
	[CLKID_USB0_TO_DDR]	    = &gxl_usb0_to_ddr.hw,
	[CLKID_AIU_PCLK]	    = &gxl_aiu_pclk.hw,
	[CLKID_MMC_PCLK]	    = &gxl_mmc_pclk.hw,
	[CLKID_DVIN]		    = &gxl_dvin.hw,
	[CLKID_UART2]		    = &gxl_uart2.hw,
	[CLKID_SARADC]		    = &gxl_saradc.hw,
	[CLKID_VPU_INTR]	    = &gxl_vpu_intr.hw,
	[CLKID_SEC_AHB_AHB3_BRIDGE] = &gxl_sec_ahb_ahb3_bridge.hw,
	[CLKID_APB3_AO]	    = &gxl_apb3_ao.hw,
	[CLKID_MCLK_TVFE]	    = &gxl_mclk_tvfe.hw,
	[CLKID_CLK81_GIC]	    = &gxl_clk81_gic.hw,
	[CLKID_VCLK2_VENCI0]	    = &gxl_vclk2_venci0.hw,
	[CLKID_VCLK2_VENCI1]	    = &gxl_vclk2_venci1.hw,
	[CLKID_VCLK2_VENCP0]	    = &gxl_vclk2_vencp0.hw,
	[CLKID_VCLK2_VENCP1]	    = &gxl_vclk2_vencp1.hw,
	[CLKID_VCLK2_VENCT0]	    = &gxl_vclk2_venct0.hw,
	[CLKID_VCLK2_VENCT1]	    = &gxl_vclk2_venct1.hw,
	[CLKID_VCLK2_OTHER]	    = &gxl_vclk2_other.hw,
	[CLKID_VCLK2_ENCI]	    = &gxl_vclk2_enci.hw,
	[CLKID_VCLK2_ENCP]	    = &gxl_vclk2_encp.hw,
	[CLKID_DAC_CLK]		    = &gxl_dac_clk.hw,
	[CLKID_AOCLK_GATE]	    = &gxl_aoclk_gate.hw,
	[CLKID_IEC958_GATE]	    = &gxl_iec958_gate.hw,
	[CLKID_ENC480P]		    = &gxl_enc480p.hw,
	[CLKID_RNG1]		    = &gxl_rng1.hw,
	[CLKID_VCLK2_ENCT]	    = &gxl_vclk2_enct.hw,
	[CLKID_VCLK2_ENCL]	    = &gxl_vclk2_encl.hw,
	[CLKID_VCLK2_VENCLMMC]	    = &gxl_vclk2_venclmmc.hw,
	[CLKID_VCLK2_VENCL]	    = &gxl_vclk2_vencl.hw,
	[CLKID_VCLK2_OTHER1]	    = &gxl_vclk2_other1.hw,
	[CLKID_EDP]		    = &gxl_edp.hw,
	[CLKID_AO_MEDIA_CPU]	    = &gxl_ao_media_cpu.hw,
	[CLKID_AO_AHB_SRAM]	    = &gxl_ao_ahb_sram.hw,
	[CLKID_AO_AHB_BUS]	    = &gxl_ao_ahb_bus.hw,
	[CLKID_AO_IFACE]	    = &gxl_ao_iface.hw,
	[CLKID_AO_I2C]		    = &gxl_ao_i2c.hw,
	[CLKID_CPU_FCLK_P00]	= &gxl_cpu_fixedpll_p00.hw,
	[CLKID_CPU_FCLK_P01]	= &gxl_cpu_fixedpll_p01.hw,
	[CLKID_CPU_FCLK_P0]		= &gxl_cpu_fixedpll_p0.hw,
	[CLKID_CPU_FCLK_P10]	= &gxl_cpu_fixedpll_p10.hw,
	[CLKID_CPU_FCLK_P11]	= &gxl_cpu_fixedpll_p11.hw,
	[CLKID_CPU_FCLK_P1]		= &gxl_cpu_fixedpll_p1.hw,
	[CLKID_CPU_FCLK_P]		= &gxl_cpu_fixedpll_p.hw,
	[CLKID_CPU_CLK]		    = &gxl_cpu_clk.mux.hw,
};
/* Convenience tables to populate base addresses in .probe */

static struct meson_clk_pll *const gxl_clk_plls[] = {
	&gxl_fixed_pll,
	&gxl_hdmi_pll,
	&gxl_sys_pll,
	&gxl_gp0_pll,
};

static struct meson_clk_mpll *const gxl_clk_mplls[] = {
	&gxl_mpll0,
	&gxl_mpll1,
	&gxl_mpll2,
	&gxl_mpll3,
};

static struct clk_gate *gxl_clk_gates[] = {
	&gxl_clk81,
	&gxl_ddr,
	&gxl_dos,
	&gxl_isa,
	&gxl_pl301,
	&gxl_periphs,
	&gxl_spicc,
	&gxl_i2c,
	&gxl_sana,
	&gxl_smart_card,
	&gxl_rng0,
	&gxl_uart0,
	&gxl_sdhc,
	&gxl_stream,
	&gxl_async_fifo,
	&gxl_sdio,
	&gxl_abuf,
	&gxl_hiu_iface,
	&gxl_bt656,
	&gxl_assist_misc,
	&gxl_emmc_a,
	&gxl_emmc_b,
	&gxl_emmc_c,
	&gxl_dma,
	&gxl_acodec,
	&gxl_spi,
	&gxl_pclk_tvfe,
	&gxl_i2s_spdif,
	&gxl_eth,
	&gxl_demux,
	&gxl_aiu_glue,
	&gxl_iec958,
	&gxl_i2s_out,
	&gxl_amclk,
	&gxl_aififo2,
	&gxl_mixer,
	&gxl_mixer_iface,
	&gxl_adc,
	&gxl_blkmv,
	&gxl_aiu_top,
	&gxl_uart1,
	&gxl_g2d,
	&gxl_usb0,
	&gxl_usb1,
	&gxl_reset,
	&gxl_nand,
	&gxl_dos_parser,
	&gxl_usb_general,
	&gxl_vdin1,
	&gxl_ahb_arb0,
	&gxl_efuse,
	&gxl_boot_rom,
	&gxl_ahb_data_bus,
	&gxl_ahb_ctrl_bus,
	&gxl_hdcp22_pclk,
	&gxl_hdmitx_pclk,
	&gxl_pdm_pclk,
	&gxl_bt656_pclk,
	&gxl_usb1_to_ddr,
	&gxl_usb0_to_ddr,
	&gxl_aiu_pclk,
	&gxl_mmc_pclk,
	&gxl_dvin,
	&gxl_uart2,
	&gxl_saradc,
	&gxl_vpu_intr,
	&gxl_sec_ahb_ahb3_bridge,
	&gxl_apb3_ao,
	&gxl_mclk_tvfe,
	&gxl_clk81_gic,
	&gxl_vclk2_venci0,
	&gxl_vclk2_venci1,
	&gxl_vclk2_vencp0,
	&gxl_vclk2_vencp1,
	&gxl_vclk2_venct0,
	&gxl_vclk2_venct1,
	&gxl_vclk2_other,
	&gxl_vclk2_enci,
	&gxl_vclk2_encp,
	&gxl_dac_clk,
	&gxl_aoclk_gate,
	&gxl_iec958_gate,
	&gxl_enc480p,
	&gxl_rng1,
	&gxl_vclk2_enct,
	&gxl_vclk2_encl,
	&gxl_vclk2_venclmmc,
	&gxl_vclk2_vencl,
	&gxl_vclk2_other1,
	&gxl_edp,
	&gxl_ao_media_cpu,
	&gxl_ao_ahb_sram,
	&gxl_ao_ahb_bus,
	&gxl_ao_iface,
	&gxl_ao_i2c,
};

static void __init gxl_clkc_init(struct device_node *np)
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
	for (i = 0; i < ARRAY_SIZE(gxl_clk_plls); i++)
		gxl_clk_plls[i]->base = clk_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(gxl_clk_mplls); i++)
		gxl_clk_mplls[i]->base = clk_base;

	/* Populate the base address for CPU clk */
	gxl_cpu_clk.mux.reg = clk_base + (u64)gxl_cpu_clk.mux.reg;
	gxl_cpu_fixedpll_p00.reg = clk_base + (u64)gxl_cpu_fixedpll_p00.reg;
	gxl_cpu_fixedpll_p01.reg = clk_base + (u64)gxl_cpu_fixedpll_p01.reg;
	gxl_cpu_fixedpll_p10.reg = clk_base + (u64)gxl_cpu_fixedpll_p10.reg;
	gxl_cpu_fixedpll_p11.reg = clk_base + (u64)gxl_cpu_fixedpll_p11.reg;
	gxl_cpu_fixedpll_p0.reg = clk_base + (u64)gxl_cpu_fixedpll_p0.reg;
	gxl_cpu_fixedpll_p1.reg = clk_base + (u64)gxl_cpu_fixedpll_p1.reg;
	gxl_cpu_fixedpll_p.reg = clk_base + (u64)gxl_cpu_fixedpll_p.reg;

	/* Populate the base address for the MPEG clks */
	gxl_mpeg_clk_sel.reg = clk_base + (u64)gxl_mpeg_clk_sel.reg;
	gxl_mpeg_clk_div.reg = clk_base + (u64)gxl_mpeg_clk_div.reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(gxl_clk_gates); i++)
		gxl_clk_gates[i]->reg = clk_base +
			(u64)gxl_clk_gates[i]->reg;

	clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
	if (!clks) {
		/* pr_err("%s: alloc clks fail!", __func__); */
		/* return -ENOMEM; */
		return;
	}
	clk_numbers = NR_CLKS;
	/* pr_debug("%s: kzalloc clks ok!", __func__); */
	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;
	/*
	 * register all clks
	 */

	for (clkid = 0; clkid < OTHER_BASE; clkid++) {
		if (clkid == 1)
			continue;
		clks[clkid] = clk_register(NULL, gxl_clk_hws[clkid]);
		WARN_ON(IS_ERR(clks[clkid]));
	}

	amlogic_init_sdemmc();
	amlogic_init_gpu();
	amlogic_init_media();
	amlogic_init_misc();
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
	parent_hw = clk_hw_get_parent(&gxl_cpu_clk.mux.hw);
	parent_clk = parent_hw->clk;
	ret = clk_notifier_register(parent_clk, &gxl_cpu_clk.clk_nb);
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

CLK_OF_DECLARE(gxl, "amlogic,gxl-clkc", gxl_clkc_init);
