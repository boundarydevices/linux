/*
 * drivers/amlogic/clk/g12b/g12b.c
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
#include <linux/delay.h>
#include <dt-bindings/clock/amlogic,g12a-clkc.h>

#include "../clkc.h"
#include "../g12a/g12a.h"

static struct meson_clk_pll g12b_sys_pll = {
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

static u32 mux_table_cpu_p[]	= { 0, 1, 2 };
/*static u32 mux_table_cpu_px[]   = { 0, 1 };*/

static struct meson_cpu_mux_divider g12b_cpu_fclk_p = {
	.reg = (void *)HHI_SYS_CPUB_CLK_CNTL,
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
		.name = "cpub_fixedpll_p",
		.ops = &meson_fclk_cpu_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div2",
			"fclk_div3"},
		.num_parents = 3,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

/*
 *static struct meson_clk_cpu g12b_cpu_clk = {
 *	.reg_off = HHI_SYS_CPUB_CLK_CNTL,
 *	.clk_nb.notifier_call = meson_clk_cpu_notifier_cb,
 *	.mux.reg = (void *)HHI_SYS_CPUB_CLK_CNTL,
 *	.mux.shift = 11,
 *	.mux.mask = 0x1,
 *	.mux.lock = &clk_lock,
 *	.mux.table = mux_table_cpu_px,
 *	.mux.hw.init = &(struct clk_init_data){
 *		.name = "cpub_clk",
 *		.ops = &meson_clk_cpu_ops,
 *		.parent_names = (const char *[]){ "cpub_fixedpll_p",
 *					"sys_pll"},
 *		.num_parents = 2,
 *		.flags = CLK_GET_RATE_NOCACHE,
 *	},
 *};
 */

static struct clk_mux g12b_cpu_clk = {
		.reg = (void *)HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x1,
		.shift = 11,
		.lock = &clk_lock,
		.hw.init = &(struct clk_init_data){
			.name = "cpub_clk",
			.ops = &clk_mux_ops,
			.parent_names = (const char *[]){ "cpub_fixedpll_p",
							 "sys_pll" },
			.num_parents = 2,
			.flags = CLK_GET_RATE_NOCACHE,
		},
};

static const char * const media_parent_names[] = { "xtal",
	"gp0_pll", "hifi_pll", "fclk_div2p5", "fclk_div3", "fclk_div4",
	"fclk_div5",  "fclk_div7"};

static const char * const media_parent_names_mipi[] = { "xtal",
	"gp0_pll", "mpll1", "mpll2", "fclk_div3", "fclk_div4",
	"fclk_div5",  "fclk_div7"};

static struct clk_mux cts_gdc_core_clk_mux = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_gdc_core_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_gdc_core_clk_div = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_gdc_core_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_gdc_core_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_gdc_core_clk_gate = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_gdc_core_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_gdc_core_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_gdc_axi_clk_mux = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_gdc_axi_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_gdc_axi_clk_div = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_gdc_axi_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_gdc_axi_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_gdc_axi_clk_gate = {
	.reg = (void *)HHI_APICALGDC_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_gdc_axi_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_gdc_axi_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_vipnanoq_core_clk_mux = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_core_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_vipnanoq_core_clk_div = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_core_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_core_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_vipnanoq_core_clk_gate = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vipnanoq_core_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_core_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_vipnanoq_axi_clk_mux = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_axi_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_vipnanoq_axi_clk_div = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_vipnanoq_axi_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_axi_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_vipnanoq_axi_clk_gate = {
	.reg = (void *)HHI_VIPNANOQ_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vipnanoq_axi_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_vipnanoq_axi_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};



static struct clk_mux cts_mipi_isp_clk_mux = {
	.reg = (void *)HHI_MIPI_ISP_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_isp_clk_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_mipi_isp_clk_div = {
	.reg = (void *)HHI_MIPI_ISP_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_isp_clk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_mipi_isp_clk_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_mipi_isp_clk_gate = {
	.reg = (void *)HHI_MIPI_ISP_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_mipi_isp_clk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_mipi_isp_clk_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_mipi_csi_phy_clk0_mux = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk0_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names_mipi,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_mipi_csi_phy_clk0_div = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_mipi_csi_phy_clk0_gate = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_mipi_csi_phy_clk0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_mipi_csi_phy_clk1_mux = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.mask = 0x7,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk1_mux",
		.ops = &clk_mux_ops,
		.parent_names = media_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider cts_mipi_csi_phy_clk1_div = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.shift = 16,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_csi_phy_clk1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate cts_mipi_csi_phy_clk1_gate = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "cts_mipi_csi_phy_clk1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "cts_mipi_csi_phy_clk1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux cts_mipi_sci_phy_mux = {
	.reg = (void *)HHI_MIPI_CSI_PHY_CLK_CNTL,
	.mask = 0x1,
	.shift = 31,
	.lock = &clk_lock,
	.flags = CLK_PARENT_ALTERNATE,
	.hw.init = &(struct clk_init_data){
		.name = "cts_mipi_sci_phy_mux",
		.ops = &meson_clk_mux_ops,
		.parent_names = (const char *[]) {
			"cts_mipi_csi_phy_clk0_composite",
			"cts_mipi_csi_phy_clk1_composite"
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll *const g12b_clk_plls[] = {
	&g12b_sys_pll,
};

static MESON_GATE(g12b_csi_dig, HHI_GCLK_MPEG1, 18);
static MESON_GATE(g12b_vipnanoq, HHI_GCLK_MPEG1, 19);

static MESON_GATE(g12b_gdc, HHI_GCLK_MPEG2, 16);
static MESON_GATE(g12b_mipi_isp, HHI_GCLK_MPEG2, 17);
static MESON_GATE(g12b_csi2_phy1, HHI_GCLK_MPEG2, 28);
static MESON_GATE(g12b_csi2_phy0, HHI_GCLK_MPEG2, 29);


static struct clk_gate *g12b_clk_gates[] = {
	&g12b_csi_dig,
	&g12b_vipnanoq,
	&g12b_gdc,
	&g12b_mipi_isp,
	&g12b_csi2_phy1,
	&g12b_csi2_phy0,
};

static struct clk_hw *g12b_clk_hws[] = {
	[CLKID_CPUB_FCLK_P - CLKID_G12B_ADD_BASE] = &g12b_cpu_fclk_p.hw,
	[CLKID_CPUB_CLK - CLKID_G12B_ADD_BASE] = &g12b_cpu_clk.hw,
	[CLKID_CSI_DIG - CLKID_G12B_ADD_BASE] = &g12b_csi_dig.hw,
	[CLKID_VIPNANOQ - CLKID_G12B_ADD_BASE] = &g12b_vipnanoq.hw,
	[CLKID_GDC - CLKID_G12B_ADD_BASE] = &g12b_gdc.hw,
	[CLKID_MIPI_ISP - CLKID_G12B_ADD_BASE] = &g12b_mipi_isp.hw,
	[CLKID_CSI2_PHY1 - CLKID_G12B_ADD_BASE] = &g12b_csi2_phy1.hw,
	[CLKID_CSI2_PHY0 - CLKID_G12B_ADD_BASE] = &g12b_csi2_phy0.hw,
};

static int g12b_cpub_clk_notifier_cb(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct clk_hw *cpu_clk_hw, *parent_clk_hw;
	struct clk *cpu_clk, *parent_clk;
	int ret;

	switch (event) {
	case PRE_RATE_CHANGE:
		parent_clk_hw = &g12b_cpu_fclk_p.hw;
		break;
	case POST_RATE_CHANGE:
		parent_clk_hw = &g12b_sys_pll.hw;
		break;
	default:
		return NOTIFY_DONE;
	}

	cpu_clk_hw = &g12b_cpu_clk.hw;
	cpu_clk = __clk_lookup(clk_hw_get_name(cpu_clk_hw));
	parent_clk = __clk_lookup(clk_hw_get_name(parent_clk_hw));

	ret = clk_set_parent(cpu_clk, parent_clk);
	if (ret)
		return notifier_from_errno(ret);

	udelay(80);

	return NOTIFY_OK;
}

static struct notifier_block g12b_cpub_nb_data = {
	.notifier_call = g12b_cpub_clk_notifier_cb,
};


static void __init g12b_clkc_init(struct device_node *np)
{
	int ret = 0, clkid, i;
	/*struct clk_hw *parent_hw;
	 *struct clk *parent_clk;
	 */

	/*	Generic clocks and PLLs */
	if (!clk_base)
		clk_base = of_iomap(np, 0);
	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		/* return -ENXIO; */
		return;
	}

	g12b_cpu_fclk_p.reg = clk_base
		+ (unsigned long)g12b_cpu_fclk_p.reg;
	g12b_cpu_clk.reg = clk_base
		+ (unsigned long)g12b_cpu_clk.reg;

	cts_gdc_core_clk_mux.reg = clk_base
		+ (unsigned long)(cts_gdc_core_clk_mux.reg);
	cts_gdc_core_clk_gate.reg = clk_base
		+ (unsigned long)(cts_gdc_core_clk_gate.reg);
	cts_gdc_core_clk_div.reg = clk_base
		+ (unsigned long)(cts_gdc_core_clk_div.reg);

	cts_gdc_axi_clk_mux.reg = clk_base
		+ (unsigned long)(cts_gdc_axi_clk_mux.reg);
	cts_gdc_axi_clk_gate.reg = clk_base
		+ (unsigned long)(cts_gdc_axi_clk_gate.reg);
	cts_gdc_axi_clk_div.reg = clk_base
		+ (unsigned long)(cts_gdc_axi_clk_div.reg);

	cts_vipnanoq_core_clk_mux.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_mux.reg);
	cts_vipnanoq_core_clk_gate.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_gate.reg);
	cts_vipnanoq_core_clk_div.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_core_clk_div.reg);
	cts_vipnanoq_axi_clk_mux.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_mux.reg);
	cts_vipnanoq_axi_clk_gate.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_gate.reg);
	cts_vipnanoq_axi_clk_div.reg = clk_base
		+ (unsigned long)(cts_vipnanoq_axi_clk_div.reg);


	cts_mipi_isp_clk_mux.reg = clk_base
		+ (unsigned long)(cts_mipi_isp_clk_mux.reg);
	cts_mipi_isp_clk_gate.reg = clk_base
		+ (unsigned long)(cts_mipi_isp_clk_gate.reg);
	cts_mipi_isp_clk_div.reg = clk_base
		+ (unsigned long)(cts_mipi_isp_clk_div.reg);

	cts_mipi_csi_phy_clk0_mux.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk0_mux.reg);
	cts_mipi_csi_phy_clk0_div.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk0_div.reg);
	cts_mipi_csi_phy_clk0_gate.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk0_gate.reg);
	cts_mipi_csi_phy_clk1_mux.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk1_mux.reg);
	cts_mipi_csi_phy_clk1_div.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk1_div.reg);
	cts_mipi_csi_phy_clk1_gate.reg = clk_base
		+ (unsigned long)(cts_mipi_csi_phy_clk1_gate.reg);
	cts_mipi_sci_phy_mux.reg = clk_base
		+ (unsigned long)(cts_mipi_sci_phy_mux.reg);


	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(g12b_clk_gates); i++)
		g12b_clk_gates[i]->reg = clk_base +
			(unsigned long)g12b_clk_gates[i]->reg;

	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(g12b_clk_plls); i++)
		g12b_clk_plls[i]->base = clk_base;

	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			pr_err("%s: alloc clks fail!", __func__);
			return;
		}
		clk_numbers = NR_CLKS;
	}

	/*
	 * register all clks
	 */
	clks[CLKID_SYS_PLL]	= clk_register(NULL, &g12b_sys_pll.hw);
	if (IS_ERR(clks[CLKID_SYS_PLL])) {
		pr_err("%s: failed to register %s\n", __func__,
			clk_hw_get_name(&g12b_sys_pll.hw));
		goto iounmap;
	}

	for (clkid = 0; clkid < ARRAY_SIZE(g12b_clk_hws); clkid++) {
		if (g12b_clk_hws[clkid]) {
			clks[clkid + CLKID_G12B_ADD_BASE]
				= clk_register(NULL, g12b_clk_hws[clkid]);
			if (IS_ERR(clks[clkid + CLKID_G12B_ADD_BASE])) {
				pr_err("%s: failed to register %s\n", __func__,
					clk_hw_get_name(g12b_clk_hws[clkid]));
				goto iounmap;
			}
		}
	}

	clks[CLKID_GDC_CORE_CLK_COMP] = clk_register_composite(NULL,
		"cts_gdc_core_clk_composite",
		media_parent_names, 8,
		&cts_gdc_core_clk_mux.hw,
		&clk_mux_ops,
		&cts_gdc_core_clk_div.hw,
		&clk_divider_ops,
		&cts_gdc_core_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_GDC_CORE_CLK_COMP]))
		panic("%s: %d register cts_gdc_core_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_GDC_AXI_CLK_COMP] = clk_register_composite(NULL,
		"cts_gdc_axi_clk_composite",
		media_parent_names, 8,
		&cts_gdc_axi_clk_mux.hw,
		&clk_mux_ops,
		&cts_gdc_axi_clk_div.hw,
		&clk_divider_ops,
		&cts_gdc_axi_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_GDC_AXI_CLK_COMP]))
		panic("%s: %d register cts_gdc_axi_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VNANOQ_CORE_CLK_COMP] = clk_register_composite(NULL,
		"cts_vipnanoq_core_clk_composite",
		media_parent_names, 8,
		&cts_vipnanoq_core_clk_mux.hw,
		&clk_mux_ops,
		&cts_vipnanoq_core_clk_div.hw,
		&clk_divider_ops,
		&cts_vipnanoq_core_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VNANOQ_CORE_CLK_COMP]))
		panic("%s: %d register cts_vipnanoq_core_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_VNANOQ_AXI_CLK_COMP] = clk_register_composite(NULL,
		"cts_vipnanoq_axi_clk_composite",
		media_parent_names, 8,
		&cts_vipnanoq_axi_clk_mux.hw,
		&clk_mux_ops,
		&cts_vipnanoq_axi_clk_div.hw,
		&clk_divider_ops,
		&cts_vipnanoq_axi_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VNANOQ_AXI_CLK_COMP]))
		panic("%s: %d register cts_vipnanoq_axi_clk_composite error\n",
			__func__, __LINE__);



	clks[CLKID_MIPI_ISP_CLK_COMP] = clk_register_composite(NULL,
		"cts_mipi_isp_clk_composite",
		media_parent_names, 8,
		&cts_mipi_isp_clk_mux.hw,
		&clk_mux_ops,
		&cts_mipi_isp_clk_div.hw,
		&clk_divider_ops,
		&cts_mipi_isp_clk_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_MIPI_ISP_CLK_COMP]))
		panic("%s: %d register cts_mipi_isp_clk_composite error\n",
			__func__, __LINE__);

	clks[CLKID_MIPI_CSI_PHY_CLK0_COMP] = clk_register_composite(NULL,
		"cts_mipi_csi_phy_clk0_composite",
		media_parent_names_mipi, 8,
		&cts_mipi_csi_phy_clk0_mux.hw,
		&clk_mux_ops,
		&cts_mipi_csi_phy_clk0_div.hw,
		&clk_divider_ops,
		&cts_mipi_csi_phy_clk0_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_MIPI_CSI_PHY_CLK0_COMP]))
		panic("%s: %d register cts_mipi_csi_phy_clk0_composite error\n",
			__func__, __LINE__);

	clks[CLKID_MIPI_CSI_PHY_CLK1_COMP] = clk_register_composite(NULL,
		"cts_mipi_csi_phy_clk1_composite",
		media_parent_names, 8,
		&cts_mipi_csi_phy_clk1_mux.hw,
		&clk_mux_ops,
		&cts_mipi_csi_phy_clk1_div.hw,
		&clk_divider_ops,
		&cts_mipi_csi_phy_clk1_gate.hw,
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_MIPI_CSI_PHY_CLK1_COMP]))
		panic("%s: %d register cts_mipi_csi_phy_clk1_composite error\n",
			__func__, __LINE__);

	clks[CLKID_MIPI_CSI_PHY_MUX] = clk_register(NULL,
		&cts_mipi_sci_phy_mux.hw);
	if (IS_ERR(clks[CLKID_MIPI_CSI_PHY_MUX]))
		panic("%s: %d clk_register %s error\n",
			__func__, __LINE__, cts_mipi_sci_phy_mux.hw.init->name);

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
	 /*parent_hw = clk_hw_get_parent(&g12b_cpu_clk.mux.hw);
	  *parent_clk = parent_hw->clk;
	  *ret = clk_notifier_register(parent_clk, &g12b_cpu_clk.clk_nb);
	  */
	ret = clk_notifier_register(g12b_sys_pll.hw.clk, &g12b_cpub_nb_data);
	if (ret) {
		pr_err("%s: failed to register clock notifier for cpu_clk\n",
				__func__);
		goto iounmap;
	}
	pr_debug("%s: cpu clk register notifier ok!", __func__);
	return;

iounmap:
	iounmap(clk_base);
	pr_info("%s: %d: ret: %d\n", __func__, __LINE__, ret);
	/* return; */
}

CLK_OF_DECLARE(g12b, "amlogic,g12b-clkc-2", g12b_clkc_init);


