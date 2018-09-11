/*
 * drivers/amlogic/clk/tl1/tl1.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#include <dt-bindings/clock/amlogic,tl1-clkc.h>

#include "../clkc.h"
#include "tl1.h"

#define MESON_GATE_TL1(_name, _reg, _bit)			\
struct clk_gate _name = {					\
	.reg = (void __iomem *) _reg,				\
	.bit_idx = (_bit),					\
	.lock = &clk_lock,					\
	.hw.init = &(struct clk_init_data) {			\
		.name = #_name,					\
		.ops = &clk_gate_ops,				\
		.parent_names = (const char *[]){ "clk81" },	\
		.num_parents = 1,				\
		.flags = CLK_IGNORE_UNUSED,			\
	},							\
}


static struct clk_onecell_data clk_data;

static struct meson_clk_pll tl1_sys_pll = {
	.m = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_SYS_PLL_CNTL0,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = tl1_pll_rate_table,
	.rate_count = ARRAY_SIZE(tl1_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_tl1_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll tl1_gp0_pll = {
	.m = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_GP0_PLL_CNTL0,
		.shift   = 16,
		.width   = 2,
	},
	.rate_table = tl1_pll_rate_table,
	.rate_count = ARRAY_SIZE(tl1_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_tl1_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll tl1_gp1_pll = {
	.m = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift	 = 0,
		.width	 = 9,
	},
	.n = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift	 = 9,
		.width	 = 5,
	},
	.od = {
		.reg_off = HHI_GP1_PLL_CNTL0,
		.shift	 = 16,
		.width	 = 2,
	},
	.rate_table = tl1_pll_rate_table,
	.rate_count = ARRAY_SIZE(tl1_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_tl1_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll tl1_hifi_pll = {
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
	.rate_table = tl1_pll_rate_table,
	.rate_count = ARRAY_SIZE(tl1_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_tl1_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#if 0
static struct meson_clk_pll tl1_adc_pll = {
	.m = {
		.reg_off = HHI_ADC_PLL_CNTL,
		.shift   = 0,
		.width   = 9,
	},
	.n = {
		.reg_off = HHI_ADC_PLL_CNTL,
		.shift   = 9,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_ADC_PLL_CNTL,
		.shift   = 14,
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
#endif

static struct meson_clk_pll tl1_fixed_pll = {
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
		.shift	 = 0,
		.width	 = 19,
	},
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_tl1_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*tl1 fixed multiplier and divider clock*/
static struct clk_fixed_factor tl1_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div2p5 = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll tl1_mpll0 = {
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
		.ops = &meson_tl1_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll tl1_mpll1 = {
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
		.ops = &meson_tl1_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll tl1_mpll2 = {
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
		.ops = &meson_tl1_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll tl1_mpll3 = {
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
		.ops = &meson_tl1_mpll_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};
/*
 *static struct meson_clk_pll tl1_hdmi_pll = {
 *	.m = {
 *		.reg_off = HHI_HDMI_PLL_CNTL,
 *		.shift   = 0,
 *		.width   = 9,
 *	},
 *	.n = {
 *		.reg_off = HHI_HDMI_PLL_CNTL,
 *		.shift   = 9,
 *		.width   = 5,
 *	},
 *	.lock = &clk_lock,
 *	.hw.init = &(struct clk_init_data){
 *		.name = "hdmi_pll",
 *		.ops = &meson_clk_pll_ro_ops,
 *		.parent_names = (const char *[]){ "xtal" },
 *		.num_parents = 1,
 *		.flags = CLK_GET_RATE_NOCACHE,
 *	},
 * };
 */

/*
 * FIXME cpu clocks and the legacy composite clocks (e.g. clk81) are both PLL
 * post-dividers and should be modelled with their respective PLLs via the
 * forthcoming coordinated clock rates feature
 */
static u32 mux_table_cpu_p[]	= { 0, 1, 2 };
static u32 mux_table_cpu_px[]   = { 0, 1 };
static struct meson_cpu_mux_divider tl1_cpu_fclk_p = {
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

static struct meson_clk_cpu tl1_cpu_clk = {
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

static u32 mux_table_clk81[]	= { 6, 5, 7 };

static struct clk_mux tl1_mpeg_clk_sel = {
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

static struct clk_divider tl1_mpeg_clk_div = {
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
static struct clk_gate tl1_clk81 = {
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

/* Everything Else (EE) domain gates */
/* HHI_GCLK_MPEG0 26 bits valid */
static MESON_GATE_TL1(tl1_ddr, HHI_GCLK_MPEG0,		0);
static MESON_GATE_TL1(tl1_dos, HHI_GCLK_MPEG0,		1);
static MESON_GATE_TL1(tl1_eth_phy, HHI_GCLK_MPEG0,	4);
static MESON_GATE_TL1(tl1_isa, HHI_GCLK_MPEG0,		5);
static MESON_GATE_TL1(tl1_pl310, HHI_GCLK_MPEG0,	6);
static MESON_GATE_TL1(tl1_periphs, HHI_GCLK_MPEG0,	7);
static MESON_GATE_TL1(tl1_i2c, HHI_GCLK_MPEG0,		9);
static MESON_GATE_TL1(tl1_sana, HHI_GCLK_MPEG0,		10);
static MESON_GATE_TL1(tl1_smart_card, HHI_GCLK_MPEG0,	11);
static MESON_GATE_TL1(tl1_uart0, HHI_GCLK_MPEG0,	13);
static MESON_GATE_TL1(tl1_stream, HHI_GCLK_MPEG0,	15);
static MESON_GATE_TL1(tl1_async_fifo, HHI_GCLK_MPEG0,	16);
static MESON_GATE_TL1(tl1_tvfe, HHI_GCLK_MPEG0,		18);
static MESON_GATE_TL1(tl1_hiu_reg, HHI_GCLK_MPEG0,	19);
static MESON_GATE_TL1(tl1_hdmirx_pclk, HHI_GCLK_MPEG0,	21);
static MESON_GATE_TL1(tl1_atv_demod, HHI_GCLK_MPEG0,	22);
static MESON_GATE_TL1(tl1_assist_misc, HHI_GCLK_MPEG0,	23);
static MESON_GATE_TL1(tl1_emmc_b, HHI_GCLK_MPEG0,	25);
static MESON_GATE_TL1(tl1_emmc_c, HHI_GCLK_MPEG0,	26);
static MESON_GATE_TL1(tl1_adec, HHI_GCLK_MPEG0,		27);
static MESON_GATE_TL1(tl1_acodec, HHI_GCLK_MPEG0,	28);
static MESON_GATE_TL1(tl1_tcon, HHI_GCLK_MPEG0,		29);
static MESON_GATE_TL1(tl1_spi, HHI_GCLK_MPEG0,		30);
static MESON_GATE_TL1(tl1_dsp, HHI_GCLK_MPEG0,		31);
/* HHI_GCLK_MPEG1 11 bits valid */
static MESON_GATE_TL1(tl1_audio, HHI_GCLK_MPEG1,	0);
static MESON_GATE_TL1(tl1_eth_core, HHI_GCLK_MPEG1,	3);
static MESON_GATE_TL1(tl1_demux, HHI_GCLK_MPEG1,	4);
static MESON_GATE_TL1(tl1_aififo, HHI_GCLK_MPEG1,	11);
static MESON_GATE_TL1(tl1_adc, HHI_GCLK_MPEG1,		13);
static MESON_GATE_TL1(tl1_uart1, HHI_GCLK_MPEG1,	16);
static MESON_GATE_TL1(tl1_g2d, HHI_GCLK_MPEG1,		20);
static MESON_GATE_TL1(tl1_reset, HHI_GCLK_MPEG1,	23);
static MESON_GATE_TL1(tl1_u_parser, HHI_GCLK_MPEG1,	25);
static MESON_GATE_TL1(tl1_usb_general, HHI_GCLK_MPEG1,	26);
static MESON_GATE_TL1(tl1_ahb_arb0, HHI_GCLK_MPEG1,	29);
/* HHI_GCLK_MPEG2 11 bits valid */
static MESON_GATE_TL1(tl1_ahb_data_bus, HHI_GCLK_MPEG2,	1);
static MESON_GATE_TL1(tl1_ahb_ctrl_bus, HHI_GCLK_MPEG2,	2);
static MESON_GATE_TL1(tl1_bt656, HHI_GCLK_MPEG2,	6);
static MESON_GATE_TL1(tl1_usb1_to_ddr, HHI_GCLK_MPEG2,	8);
static MESON_GATE_TL1(tl1_mmc_pclk, HHI_GCLK_MPEG2,	11);
static MESON_GATE_TL1(tl1_hdcp22_pclk, HHI_GCLK_MPEG2,	13);
static MESON_GATE_TL1(tl1_uart2, HHI_GCLK_MPEG2,	15);
static MESON_GATE_TL1(tl1_ts, HHI_GCLK_MPEG2,		22);
static MESON_GATE_TL1(tl1_demod_comb, HHI_GCLK_MPEG2,	25);
static MESON_GATE_TL1(tl1_vpu_intr, HHI_GCLK_MPEG2,	28);
static MESON_GATE_TL1(tl1_gic, HHI_GCLK_MPEG2,		30);
/* HHI_GCLK_OTHER 17bits valid */
static MESON_GATE_TL1(tl1_vclk2_venci0, HHI_GCLK_OTHER,	1);
static MESON_GATE_TL1(tl1_vclk2_venci1, HHI_GCLK_OTHER,	2);
static MESON_GATE_TL1(tl1_vclk2_vencp0, HHI_GCLK_OTHER,	3);
static MESON_GATE_TL1(tl1_vclk2_vencp1, HHI_GCLK_OTHER,	4);
static MESON_GATE_TL1(tl1_vclk2_venct0, HHI_GCLK_OTHER,	5);
static MESON_GATE_TL1(tl1_vclk2_venct1, HHI_GCLK_OTHER,	6);
static MESON_GATE_TL1(tl1_vclk2_other, HHI_GCLK_OTHER,	7);
static MESON_GATE_TL1(tl1_vclk2_enci, HHI_GCLK_OTHER,	8);
static MESON_GATE_TL1(tl1_vclk2_encp, HHI_GCLK_OTHER,	9);
static MESON_GATE_TL1(tl1_dac_clk, HHI_GCLK_OTHER,	10);
static MESON_GATE_TL1(tl1_enc480p, HHI_GCLK_OTHER,	20);
static MESON_GATE_TL1(tl1_rng1, HHI_GCLK_OTHER,		21);
static MESON_GATE_TL1(tl1_vclk2_enct, HHI_GCLK_OTHER,	22);
static MESON_GATE_TL1(tl1_vclk2_encl, HHI_GCLK_OTHER,	23);
static MESON_GATE_TL1(tl1_vclk2_venclmmc, HHI_GCLK_OTHER, 24);
static MESON_GATE_TL1(tl1_vclk2_vencl, HHI_GCLK_OTHER,	25);
static MESON_GATE_TL1(tl1_vclk2_other1, HHI_GCLK_OTHER,	26);
/* end Everything Else (EE) domain gates */
/* Always On (AO) domain gates */
static MESON_GATE(tl1_dma, HHI_GCLK_AO,			0);
static MESON_GATE(tl1_efuse, HHI_GCLK_AO,		1);
static MESON_GATE(tl1_rom_boot, HHI_GCLK_AO,		2);
static MESON_GATE(tl1_reset_sec, HHI_GCLK_AO,		3);
static MESON_GATE(tl1_sec_ahb_apb3, HHI_GCLK_AO,	4);

static struct clk_gate tl1_spicc_0 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tl1_spicc_0",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

static struct clk_gate tl1_spicc_1 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 14,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tl1_spicc_1",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = 0,
	},
};

/* Array of all clocks provided by this provider */
static struct clk_hw *tl1_clk_hws[] = {
	[CLKID_SYS_PLL]		= &tl1_sys_pll.hw,
	[CLKID_FIXED_PLL]	= &tl1_fixed_pll.hw,
	[CLKID_GP0_PLL]		= &tl1_gp0_pll.hw,
	[CLKID_GP1_PLL]		= &tl1_gp1_pll.hw,
	[CLKID_HIFI_PLL]	= &tl1_hifi_pll.hw,
	/*[CLKID_ADC_PLL]	= &tl1_adc_pll.hw,*/
	[CLKID_FCLK_DIV2]	= &tl1_fclk_div2.hw,
	[CLKID_FCLK_DIV3]	= &tl1_fclk_div3.hw,
	[CLKID_FCLK_DIV4]	= &tl1_fclk_div4.hw,
	[CLKID_FCLK_DIV5]	= &tl1_fclk_div5.hw,
	[CLKID_FCLK_DIV7]	= &tl1_fclk_div7.hw,
	[CLKID_FCLK_DIV2P5]	= &tl1_fclk_div2p5.hw,
	[CLKID_MPEG_SEL]	= &tl1_mpeg_clk_sel.hw,
	[CLKID_MPEG_DIV]	= &tl1_mpeg_clk_div.hw,
	[CLKID_CLK81]		= &tl1_clk81.hw,
	[CLKID_MPLL0]		= &tl1_mpll0.hw,
	[CLKID_MPLL1]		= &tl1_mpll1.hw,
	[CLKID_MPLL2]		= &tl1_mpll2.hw,
	[CLKID_MPLL3]		= &tl1_mpll3.hw,
	[CLKID_DDR]		= &tl1_ddr.hw, /*MPEG0 0*/
	[CLKID_DOS]		= &tl1_dos.hw, /*MPEG0 1*/
	[CLKID_ETH_PHY]		= &tl1_eth_phy.hw, /*MPEG0 4*/
	[CLKID_ISA]		= &tl1_isa.hw, /*MPEG0 5*/
	[CLKID_PL310]		= &tl1_pl310.hw, /*MPEG0 6*/
	[CLKID_PERIPHS]		= &tl1_periphs.hw, /*MPEG0 7*/
	[CLKID_SPICC0]		= &tl1_spicc_0.hw, /*MPEG0 8*/
	[CLKID_I2C]		= &tl1_i2c.hw, /*MPEG0 9*/
	[CLKID_SANA]		= &tl1_sana.hw, /*MPEG0 10*/
	[CLKID_SMART_CARD]	= &tl1_smart_card.hw, /*MPEG0 11*/
	[CLKID_UART0]		= &tl1_uart0.hw, /*MPEG0 13*/
	[CLKID_SPICC1]		= &tl1_spicc_1.hw, /*MPEG0 14*/
	[CLKID_STREAM]		= &tl1_stream.hw, /*MPEG0 15*/
	[CLKID_ASYNC_FIFO]	= &tl1_async_fifo.hw,/*MPEG0 16*/
	[CLKID_TVFE]		= &tl1_tvfe.hw, /*MPEG0 18*/
	[CLKID_HIU_REG]		= &tl1_hiu_reg.hw, /*MPEG0 19*/
	[CLKID_HDMIRX_PCLK]	= &tl1_hdmirx_pclk.hw, /*MPEG0 21*/
	[CLKID_ATV_DEMOD]	= &tl1_atv_demod.hw, /*MPEG0 22*/
	[CLKID_ASSIST_MISC]	= &tl1_assist_misc.hw, /*MPEG0 23*/
	[CLKID_SD_EMMC_B]	= &tl1_emmc_b.hw, /*MPEG0 25*/
	[CLKID_SD_EMMC_C]	= &tl1_emmc_c.hw, /*MPEG0 26*/
	[CLKID_ADEC]		= &tl1_adec.hw, /*MPEG0 27*/
	[CLKID_ACODEC]		= &tl1_acodec.hw, /*MPEG0 28*/
	[CLKID_TCON]		= &tl1_tcon.hw, /*MPEG0 29*/
	[CLKID_SPI]		= &tl1_spi.hw, /*MPEG0 30*/
	[CLKID_DSP]		= &tl1_dsp.hw, /*MPEG0 31*/
	[CLKID_AUDIO]		= &tl1_audio.hw, /*MPEG1 0*/
	[CLKID_ETH_CORE]	= &tl1_eth_core.hw, /*MPEG1 3*/
	[CLKID_DEMUX]		= &tl1_demux.hw, /*MPEG1 4*/
	[CLKID_AIFIFO]		= &tl1_aififo.hw,/*MPEG1 11*/
	[CLKID_ADC]		= &tl1_adc.hw, /*MPEG1 13*/
	[CLKID_UART1]		= &tl1_uart1.hw, /*MPEG1 16*/
	[CLKID_G2D]		= &tl1_g2d.hw, /*MPEG1 20*/
	[CLKID_RESET]		= &tl1_reset.hw, /*MPEG1 23*/
	[CLKID_U_PARSER]	= &tl1_u_parser.hw, /*MPEG1 25*/
	[CLKID_USB_GENERAL]	= &tl1_usb_general.hw, /*MPEG1 26*/
	[CLKID_AHB_ARB0]	= &tl1_ahb_arb0.hw, /*MPEG1 29*/
	[CLKID_AHB_DATA_BUS]	= &tl1_ahb_data_bus.hw, /*MPEG2 1*/
	[CLKID_AHB_CTRL_BUS]	= &tl1_ahb_ctrl_bus.hw, /*MPEG2 2*/
	[CLKID_BT656]		= &tl1_bt656.hw, /*MPEG2 6*/
	[CLKID_USB1_TO_DDR]	= &tl1_usb1_to_ddr.hw, /*MPEG2 8*/
	[CLKID_MMC_PCLK]	= &tl1_mmc_pclk.hw, /*MPEG2 11*/
	[CLKID_HDCP22_PCLK]	= &tl1_hdcp22_pclk.hw, /*MPEG2 13*/
	[CLKID_UART2]		= &tl1_uart2.hw, /*MPEG2 15*/
	[CLKID_TS]		= &tl1_ts.hw, /*MPEG2 22*/
	[CLKID_VPU_INTR]	= &tl1_vpu_intr.hw, /*MPEG2 25*/
	[CLKID_DEMOD_COMB]	= &tl1_demod_comb.hw, /*MPEG2 28*/
	[CLKID_GIC]		= &tl1_gic.hw, /*MPEG2 30*/
	[CLKID_VCLK2_VENCI0]	= &tl1_vclk2_venci0.hw, /*OTHER 1*/
	[CLKID_VCLK2_VENCI1]	= &tl1_vclk2_venci1.hw, /*OTHER 2*/
	[CLKID_VCLK2_VENCP0]	= &tl1_vclk2_vencp0.hw, /*OTHER 3*/
	[CLKID_VCLK2_VENCP1]	= &tl1_vclk2_vencp1.hw, /*OTHER 4*/
	[CLKID_VCLK2_VENCT0]	= &tl1_vclk2_venct0.hw, /*OTHER 5*/
	[CLKID_VCLK2_VENCT1]	= &tl1_vclk2_venct1.hw, /*OTHER 6*/
	[CLKID_VCLK2_OTHER]	= &tl1_vclk2_other.hw, /*OTHER 7*/
	[CLKID_VCLK2_ENCI]	= &tl1_vclk2_enci.hw, /*OTHER 8*/
	[CLKID_VCLK2_ENCP]	= &tl1_vclk2_encp.hw, /*OTHER 9*/
	[CLKID_DAC_CLK]		= &tl1_dac_clk.hw, /*OTHER 10*/
	[CLKID_ENC480P]		= &tl1_enc480p.hw, /*OTHER 20*/
	[CLKID_RNG1]		= &tl1_rng1.hw, /*OTHER 21*/
	[CLKID_VCLK2_ENCT]	= &tl1_vclk2_enct.hw, /*OTHER 22*/
	[CLKID_VCLK2_ENCL]	= &tl1_vclk2_encl.hw, /*OTHER 23*/
	[CLKID_VCLK2_VENCLMMC]	= &tl1_vclk2_venclmmc.hw, /*OTHER 24*/
	[CLKID_VCLK2_VENCL]	= &tl1_vclk2_vencl.hw, /*OTHER 25*/
	[CLKID_VCLK2_OTHER1]	= &tl1_vclk2_other1.hw, /*OTHER 26*/
	[CLKID_DMA]		= &tl1_dma.hw, /*AO 0*/
	[CLKID_EFUSE]		= &tl1_efuse.hw, /*AO 1*/
	[CLKID_ROM_BOOT]	= &tl1_rom_boot.hw, /*AO 2*/
	[CLKID_RESET_SEC]	= &tl1_reset_sec.hw, /*AO 3*/
	[CLKID_SEC_AHB_APB3]	= &tl1_sec_ahb_apb3.hw, /*AO 4*/

	[CLKID_CPU_FCLK_P]      = &tl1_cpu_fclk_p.hw,
	[CLKID_CPU_CLK]         = &tl1_cpu_clk.mux.hw,
};
/* Convenience tables to populate base addresses in .probe */

static struct meson_clk_pll *const tl1_clk_plls[] = {
	&tl1_fixed_pll,
	&tl1_sys_pll,
	&tl1_gp0_pll,
	&tl1_gp1_pll,
	&tl1_hifi_pll,
	/*&tl1_adc_pll,*/
	/*&tl1_hdmi_pll,*/
};

static struct meson_clk_mpll *const tl1_clk_mplls[] = {
	&tl1_mpll0,
	&tl1_mpll1,
	&tl1_mpll2,
	&tl1_mpll3,
};

static struct clk_gate *tl1_clk_gates[] = {
	&tl1_clk81,
	&tl1_ddr,
	&tl1_dos,
	&tl1_eth_phy,
	&tl1_isa,
	&tl1_pl310,
	&tl1_periphs,
	&tl1_spicc_0,
	&tl1_i2c,
	&tl1_sana,
	&tl1_smart_card,
	&tl1_uart0,
	&tl1_spicc_1,
	&tl1_stream,
	&tl1_async_fifo,
	&tl1_tvfe,
	&tl1_hiu_reg,
	&tl1_hdmirx_pclk,
	&tl1_atv_demod,
	&tl1_assist_misc,
	&tl1_emmc_b,
	&tl1_emmc_c,
	&tl1_adec,
	&tl1_acodec,
	&tl1_tcon,
	&tl1_spi,
	&tl1_dsp,/*gate0 end*/
	&tl1_audio,
	&tl1_eth_core,
	&tl1_demux,
	&tl1_aififo,
	&tl1_adc,
	&tl1_uart1,
	&tl1_g2d,
	&tl1_reset,
	&tl1_u_parser,
	&tl1_usb_general,
	&tl1_ahb_arb0, /*gate1 end*/
	&tl1_ahb_data_bus,
	&tl1_ahb_ctrl_bus,
	&tl1_bt656,
	&tl1_usb1_to_ddr,
	&tl1_mmc_pclk,
	&tl1_hdcp22_pclk,
	&tl1_uart2,
	&tl1_ts,
	&tl1_vpu_intr,
	&tl1_demod_comb,
	&tl1_gic,/*gate2 end*/
	&tl1_vclk2_venci0,
	&tl1_vclk2_venci1,
	&tl1_vclk2_vencp0,
	&tl1_vclk2_vencp1,
	&tl1_vclk2_venct0,
	&tl1_vclk2_venct1,
	&tl1_vclk2_other,
	&tl1_vclk2_enci,
	&tl1_vclk2_encp,
	&tl1_dac_clk,
	&tl1_enc480p,
	&tl1_rng1,
	&tl1_vclk2_enct,
	&tl1_vclk2_encl,
	&tl1_vclk2_venclmmc,
	&tl1_vclk2_vencl,
	&tl1_vclk2_other1,/* other end */
	&tl1_dma,
	&tl1_efuse,
	&tl1_rom_boot,
	&tl1_reset_sec,
	&tl1_sec_ahb_apb3,
};

static void __init tl1_clkc_init(struct device_node *np)
{
	int clkid, i;
	int ret = 0;
	struct clk_hw *parent_hw;
	struct clk *parent_clk;

	/*  Generic clocks and PLLs */
	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		return;
	}

	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(tl1_clk_plls); i++)
	tl1_clk_plls[i]->base = clk_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(tl1_clk_mplls); i++)
	tl1_clk_mplls[i]->base = clk_base;

	/* Populate the base address for the MPEG clks */
	tl1_mpeg_clk_sel.reg = clk_base
			+ (unsigned long)tl1_mpeg_clk_sel.reg;
	tl1_mpeg_clk_div.reg = clk_base
			+ (unsigned long)tl1_mpeg_clk_div.reg;

	/* Populate the base address for CPU clk */
	tl1_cpu_fclk_p.reg = clk_base
			+ (unsigned long)tl1_cpu_fclk_p.reg;
	tl1_cpu_clk.base = clk_base;
	tl1_cpu_clk.mux.reg = clk_base
				+ (unsigned long)tl1_cpu_clk.mux.reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(tl1_clk_gates); i++)
		tl1_clk_gates[i]->reg = clk_base +
			(unsigned long)tl1_clk_gates[i]->reg;

	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			pr_err("%s: alloc clks fail!", __func__);
			return;
		}
	} else {
		pr_err("%s: error: not kzalloc clks in eeclk!", __func__);
		return;
	}
	clk_numbers = NR_CLKS;
	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;

	/*register all clks*/
	for (clkid = 0; clkid < CLOCK_GATE; clkid++) {
		if (tl1_clk_hws[clkid]) {
		clks[clkid] = clk_register(NULL, tl1_clk_hws[clkid]);
		WARN_ON(IS_ERR(clks[clkid]));
		}
	}

	meson_tl1_sdemmc_init();
	meson_tl1_media_init();
	meson_tl1_gpu_init();
	meson_tl1_misc_init();

	parent_hw = clk_hw_get_parent(&tl1_cpu_clk.mux.hw);
	parent_clk = parent_hw->clk;
	ret = clk_notifier_register(parent_clk, &tl1_cpu_clk.clk_nb);
	if (ret) {
		pr_err("%s: failed to register clock notifier for cpu_clk\n",
		__func__);
		goto iounmap;
	}

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
			&clk_data);
	if (ret < 0) {
		pr_err("%s fail ret: %d\n", __func__, ret);
		return;
	}

	return;

iounmap:
	iounmap(clk_base);
	pr_err("%s iomap failed ret: %d\n", __func__, ret);
}

CLK_OF_DECLARE(tl1, "amlogic,tl1-clkc", tl1_clkc_init);
