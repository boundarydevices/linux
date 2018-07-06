/*
 * drivers/amlogic/clk/txl/txl.c
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
#include <dt-bindings/clock/amlogic,txl-clkc.h>

#include "../clkc.h"
#include "txl.h"

static struct clk_onecell_data clk_data;

static struct meson_clk_pll txl_fixed_pll = {
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

static struct meson_clk_pll txl_sys_pll = {
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
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll txl_gp0_pll = {
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
	.rate_table = txl_gp0_pll_rate_table,
	.rate_count = ARRAY_SIZE(txl_gp0_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct meson_clk_pll txl_gp1_pll = {
	.m = {
		.reg_off = HHI_GP1_PLL_CNTL,
		.shift	 = 0,
		.width	 = 9,
	},
	.n = {
		.reg_off = HHI_GP1_PLL_CNTL,
		.shift	 = 9,
		.width	 = 5,
	},
	.od = {
		.reg_off = HHI_GP1_PLL_CNTL,
		.shift	 = 16,
		.width	 = 2,
	},
	.rate_table = txl_gp0_pll_rate_table,
	/* concern later ,write gp1? */
	.rate_count = ARRAY_SIZE(txl_gp0_pll_rate_table),
	/* concern later ,write gp1? */
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#if 0
static struct meson_clk_pll txl_adc_pll = {
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

/*txl fixed multiplier and divider clock*/
static struct clk_fixed_factor txl_fclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor txl_fclk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor txl_fclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor txl_fclk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor txl_fclk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "fixed_pll" },
		.num_parents = 1,
	},
};

static struct meson_clk_mpll txl_mpll0 = {
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

static struct meson_clk_mpll txl_mpll1 = {
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

static struct meson_clk_mpll txl_mpll2 = {
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

static struct meson_clk_mpll txl_mpll3 = {
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

static struct meson_clk_pll txl_hdmi_pll = {
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
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll",
		.ops = &meson_clk_pll_ro_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*
 * FIXME cpu clocks and the legacy composite clocks (e.g. clk81) are both PLL
 * post-dividers and should be modelled with their respective PLLs via the
 * forthcoming coordinated clock rates feature
 */
static u32 mux_table_cpu_p00[]	= { 0, 1, 2 };
static struct clk_mux txl_cpu_fixedpll_p00 = {
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

static struct clk_divider txl_cpu_fixedpll_p01 = {
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
static struct clk_mux txl_cpu_fixedpll_p0 = {
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
static struct clk_mux txl_cpu_fixedpll_p10 = {
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

static struct clk_divider txl_cpu_fixedpll_p11 = {
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
static struct clk_mux txl_cpu_fixedpll_p1 = {
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
static struct clk_mux txl_cpu_fixedpll_p = {
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
static struct meson_clk_cpu txl_cpu_clk = {
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

static struct clk_mux txl_mpeg_clk_sel = {
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

static struct clk_divider txl_mpeg_clk_div = {
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
static struct clk_gate txl_clk81 = {
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

static struct clk_gate txl_spicc_0 = {
	.reg = (void *)HHI_GCLK_MPEG0,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "txl_spicc_0",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "clk81" },
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};
/* Everything Else (EE) domain gates */

/*HHI_GCLK_MPEG0 bit 2,3,4,14,15,17,18,20,22,24,31 not use*/
static MESON_GATE(txl_ddr, HHI_GCLK_MPEG0, 0);
static MESON_GATE(txl_dos, HHI_GCLK_MPEG0, 1);
static MESON_GATE(txl_isa, HHI_GCLK_MPEG0, 5);
static MESON_GATE(txl_pl301, HHI_GCLK_MPEG0, 6);
static MESON_GATE(txl_periphs, HHI_GCLK_MPEG0, 7);
//static MESON_GATE(txl_spicc_0, HHI_GCLK_MPEG0, 8);
static MESON_GATE(txl_i2c, HHI_GCLK_MPEG0, 9);
static MESON_GATE(txl_sana, HHI_GCLK_MPEG0, 10);
static MESON_GATE(txl_smart_card, HHI_GCLK_MPEG0, 11);
static MESON_GATE(txl_rng0, HHI_GCLK_MPEG0, 12);
static MESON_GATE(txl_uart0, HHI_GCLK_MPEG0, 13);
static MESON_GATE(txl_async_fifo, HHI_GCLK_MPEG0, 16);
static MESON_GATE(txl_hiu_reg, HHI_GCLK_MPEG0, 19);
static MESON_GATE(txl_hdmirx_apb, HHI_GCLK_MPEG0, 21);
static MESON_GATE(txl_assist_misc, HHI_GCLK_MPEG0, 23);
static MESON_GATE(txl_emmc_b, HHI_GCLK_MPEG0, 25);
static MESON_GATE(txl_emmc_c, HHI_GCLK_MPEG0, 26);
static MESON_GATE(txl_dma, HHI_GCLK_MPEG0, 27);
static MESON_GATE(txl_acodec, HHI_GCLK_MPEG0, 28);
static MESON_GATE(txl_atv_demod, HHI_GCLK_MPEG0, 29);
static MESON_GATE(txl_spi, HHI_GCLK_MPEG0, 30);

/*HHI_GCLK_MPEG1 bit 1,5,14,17,18,19,24,27,28 not use*/
static MESON_GATE(txl_pclk_tvfe, HHI_GCLK_MPEG1, 0);
static MESON_GATE(txl_i2s_spdif, HHI_GCLK_MPEG1, 2);
static MESON_GATE(txl_eth_core, HHI_GCLK_MPEG1, 3);
static MESON_GATE(txl_demux, HHI_GCLK_MPEG1, 4);
static MESON_GATE(txl_aiu_glue, HHI_GCLK_MPEG1, 6);
static MESON_GATE(txl_iec958, HHI_GCLK_MPEG1, 7);
static MESON_GATE(txl_i2s_out, HHI_GCLK_MPEG1, 8);
static MESON_GATE(txl_amclk_measure, HHI_GCLK_MPEG1, 9);
static MESON_GATE(txl_aififo2, HHI_GCLK_MPEG1, 10);
static MESON_GATE(txl_mixer, HHI_GCLK_MPEG1, 11);
static MESON_GATE(txl_mixer_iface, HHI_GCLK_MPEG1, 12);
static MESON_GATE(txl_adc, HHI_GCLK_MPEG1, 13);
static MESON_GATE(txl_aiu_top, HHI_GCLK_MPEG1, 15);
static MESON_GATE(txl_uart1, HHI_GCLK_MPEG1, 16);
static MESON_GATE(txl_g2d, HHI_GCLK_MPEG1, 20);
static MESON_GATE(txl_usb0, HHI_GCLK_MPEG1, 21);
static MESON_GATE(txl_usb1, HHI_GCLK_MPEG1, 22);
static MESON_GATE(txl_reset, HHI_GCLK_MPEG1, 23);
static MESON_GATE(txl_dos_parser, HHI_GCLK_MPEG1, 25);
static MESON_GATE(txl_usb_general, HHI_GCLK_MPEG1, 26);
static MESON_GATE(txl_ahb_arb0, HHI_GCLK_MPEG1, 29);
static MESON_GATE(txl_efuse, HHI_GCLK_MPEG1, 30);
static MESON_GATE(txl_boot_rom, HHI_GCLK_MPEG1, 31);

/*HHI_GCLK_MPEG2 bit 0,3,4,5,6,7,9,14,16-2022-24,27,29,31 not use*/
static MESON_GATE(txl_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(txl_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(txl_usb1_to_ddr, HHI_GCLK_MPEG2, 8);
static MESON_GATE(txl_usb0_to_ddr, HHI_GCLK_MPEG2, 9);
static MESON_GATE(txl_aiu_pclk, HHI_GCLK_MPEG2, 10);
static MESON_GATE(txl_mmc_pclk, HHI_GCLK_MPEG2, 11);
static MESON_GATE(txl_hdmi_hdcp22, HHI_GCLK_MPEG2, 13);
static MESON_GATE(txl_uart2, HHI_GCLK_MPEG2, 15);
static MESON_GATE(txl_uart3, HHI_GCLK_MPEG2, 21);
static MESON_GATE(txl_vpu_intr, HHI_GCLK_MPEG2, 25);
static MESON_GATE(txl_sec_ahb_ahb3_bridge, HHI_GCLK_MPEG2, 26);
static MESON_GATE(txl_demod_tvfe, HHI_GCLK_MPEG2, 26);
static MESON_GATE(txl_gic, HHI_GCLK_MPEG2, 30);

/*HHI_GCLK_OTHER bit 0,11,12,12,15,17,18,19,27,28,29,30,31 not use*/
static MESON_GATE(txl_vclk2_venci0, HHI_GCLK_OTHER, 1);
static MESON_GATE(txl_vclk2_venci1, HHI_GCLK_OTHER, 2);
static MESON_GATE(txl_vclk2_vencp0, HHI_GCLK_OTHER, 3);
static MESON_GATE(txl_vclk2_vencp1, HHI_GCLK_OTHER, 4);
static MESON_GATE(txl_vclk2_venct0, HHI_GCLK_OTHER, 5);
static MESON_GATE(txl_vclk2_venct1, HHI_GCLK_OTHER, 6);
static MESON_GATE(txl_vclk2_other, HHI_GCLK_OTHER, 7);
static MESON_GATE(txl_vclk2_enci, HHI_GCLK_OTHER, 8);
static MESON_GATE(txl_vclk2_encp, HHI_GCLK_OTHER, 9);
static MESON_GATE(txl_dac_clk, HHI_GCLK_OTHER, 10);
static MESON_GATE(txl_aoclk_gate, HHI_GCLK_OTHER, 14);
static MESON_GATE(txl_iec958_gate, HHI_GCLK_OTHER, 16);
static MESON_GATE(txl_enc480p, HHI_GCLK_OTHER, 20);
static MESON_GATE(txl_rng1, HHI_GCLK_OTHER, 21);
static MESON_GATE(txl_vclk2_enct, HHI_GCLK_OTHER, 22);
static MESON_GATE(txl_vclk2_encl, HHI_GCLK_OTHER, 23);
static MESON_GATE(txl_vclk2_venclmmc, HHI_GCLK_OTHER, 24);
static MESON_GATE(txl_vclk2_vencl, HHI_GCLK_OTHER, 25);
static MESON_GATE(txl_vclk2_other1, HHI_GCLK_OTHER, 26);

/* end Everything Else (EE) domain gates */

/* Always On (AO) domain gates */

static MESON_GATE(txl_ao_media_cpu, HHI_GCLK_AO, 0);
static MESON_GATE(txl_ao_ahb_sram, HHI_GCLK_AO, 1);
static MESON_GATE(txl_ao_ahb_bus, HHI_GCLK_AO, 2);
static MESON_GATE(txl_ao_iface, HHI_GCLK_AO, 3);
static MESON_GATE(txl_ao_i2c, HHI_GCLK_AO, 4);

/* Array of all clocks provided by this provider */

static struct clk_hw *txl_clk_hws[] = {
	[CLKID_SYS_PLL]			= &txl_sys_pll.hw,
	[CLKID_FIXED_PLL]	    = &txl_fixed_pll.hw,
	[CLKID_GP0_PLL]		    = &txl_gp0_pll.hw,
	[CLKID_GP1_PLL]			= &txl_gp1_pll.hw,
	/*[CLKID_ADC_PLL]			= &txl_adc_pll.hw,*/
	[CLKID_FCLK_DIV2]	    = &txl_fclk_div2.hw,
	[CLKID_FCLK_DIV3]	    = &txl_fclk_div3.hw,
	[CLKID_FCLK_DIV4]	    = &txl_fclk_div4.hw,
	[CLKID_FCLK_DIV5]	    = &txl_fclk_div5.hw,
	[CLKID_FCLK_DIV7]	    = &txl_fclk_div7.hw,
	[CLKID_MPEG_SEL]	    = &txl_mpeg_clk_sel.hw,
	[CLKID_MPEG_DIV]	    = &txl_mpeg_clk_div.hw,
	[CLKID_CLK81]		    = &txl_clk81.hw,
	[CLKID_MPLL0]		    = &txl_mpll0.hw,
	[CLKID_MPLL1]		    = &txl_mpll1.hw,
	[CLKID_MPLL2]		    = &txl_mpll2.hw,
	[CLKID_MPLL3]		    = &txl_mpll3.hw,
	[CLKID_DDR]				= &txl_ddr.hw,
	/*HHI_GCLK_MPEG0 0*/
	[CLKID_DOS]				= &txl_dos.hw,
	/*HHI_GCLK_MPEG0 1*/
	[CLKID_ISA]				= &txl_isa.hw,
	/*HHI_GCLK_MPEG0 5*/
	[CLKID_PL301]		    = &txl_pl301.hw,
	/*HHI_GCLK_MPEG0 6*/
	[CLKID_PERIPHS]		    = &txl_periphs.hw,
	/*HHI_GCLK_MPEG0 7*/
	[CLKID_SPICC0]		    = &txl_spicc_0.hw,
	/*HHI_GCLK_MPEG0 8*/
	[CLKID_I2C]				= &txl_i2c.hw,
	/*HHI_GCLK_MPEG0 9*/
	[CLKID_SANA]			= &txl_sana.hw,
	/*HHI_GCLK_MPEG0 10*/
	[CLKID_SMART_CARD]		= &txl_smart_card.hw,
	/*HHI_GCLK_MPEG0 11*/
	[CLKID_RNG0]		    = &txl_rng0.hw,
	/*HHI_GCLK_MPEG0 12*/
	[CLKID_UART0]		    = &txl_uart0.hw,
	/*HHI_GCLK_MPEG0 13*/
	[CLKID_ASYNC_FIFO]		= &txl_async_fifo.hw,
	/*HHI_GCLK_MPEG0 16*/
	[CLKID_HIU_REG]			= &txl_hiu_reg.hw,
	/*HHI_GCLK_MPEG0 19*/
	[CLKID_HDMIRX_APB]			= &txl_hdmirx_apb.hw,
	/*HHI_GCLK_MPEG0 21*/
	[CLKID_ASSIST_MISC]	    = &txl_assist_misc.hw,
	/*HHI_GCLK_MPEG0 23*/
	[CLKID_SD_EMMC_B]	    = &txl_emmc_b.hw,
	/*HHI_GCLK_MPEG0 25*/
	[CLKID_SD_EMMC_C]	    = &txl_emmc_c.hw,
	/*HHI_GCLK_MPEG0 26*/
	[CLKID_DMA]				= &txl_dma.hw,
	/*HHI_GCLK_MPEG0 27*/
	[CLKID_ACODEC]			= &txl_acodec.hw,
	/*HHI_GCLK_MPEG0 28*/
	[CLKID_ATV_DEMOD]		= &txl_atv_demod.hw,
	/*HHI_GCLK_MPEG0 29*/
	[CLKID_SPI]				= &txl_spi.hw,
	/*HHI_GCLK_MPEG0 30*/

	[CLKID_PCLK_TVFE]		= &txl_pclk_tvfe.hw,
	/*HHI_GCLK_MPEG1 0*/
	[CLKID_I2S_SPDIF]		= &txl_i2s_spdif.hw,
	/*HHI_GCLK_MPEG1 2*/
	[CLKID_ETH_CORE]		= &txl_eth_core.hw,
	/*HHI_GCLK_MPEG1 3*/
	[CLKID_DEMUX]			= &txl_demux.hw,
	/*HHI_GCLK_MPEG1 4*/
	[CLKID_AIU_GLUE]		= &txl_aiu_glue.hw,
	/*HHI_GCLK_MPEG1 6*/
	[CLKID_IEC958]			= &txl_iec958.hw,
	/*HHI_GCLK_MPEG1 7*/
	[CLKID_I2S_OUT]			= &txl_i2s_out.hw,
	/*HHI_GCLK_MPEG1 8*/
	[CLKID_AMCLK_MEASURE]	= &txl_amclk_measure.hw,
	/*HHI_GCLK_MPEG1 9*/
	[CLKID_AIFIFO2]			= &txl_aififo2.hw,
	/*HHI_GCLK_MPEG1 10*/
	[CLKID_MIXER]			= &txl_mixer.hw,
	/*HHI_GCLK_MPEG1 11*/
	[CLKID_MIXER_IFACE]		= &txl_mixer_iface.hw,
	/*HHI_GCLK_MPEG1 12*/
	[CLKID_ADC]				= &txl_adc.hw,
	/*HHI_GCLK_MPEG1 13*/
	[CLKID_AIU_TOP]			= &txl_aiu_top.hw,
	/*HHI_GCLK_MPEG1 15*/
	[CLKID_UART1]			= &txl_uart1.hw,
	/*HHI_GCLK_MPEG1 16*/
	[CLKID_G2D]				= &txl_g2d.hw,
	/*HHI_GCLK_MPEG1 20*/
	[CLKID_RESET]		    = &txl_reset.hw,
	/*HHI_GCLK_MPEG1 21*/
	[CLKID_USB0]			= &txl_usb0.hw,
	/*HHI_GCLK_MPEG1 22*/
	[CLKID_USB1]			= &txl_usb1.hw,
	/*HHI_GCLK_MPEG1 23*/
	[CLKID_DOS_PARSER]		= &txl_dos_parser.hw,
	/*HHI_GCLK_MPEG1 25*/
	[CLKID_USB_GENERAL]		= &txl_usb_general.hw,
	/*HHI_GCLK_MPEG1 26*/
	[CLKID_AHB_ARB0]		= &txl_ahb_arb0.hw,
	/*HHI_GCLK_MPEG1 29*/
	[CLKID_EFUSE]			= &txl_efuse.hw,
	/*HHI_GCLK_MPEG1 30*/
	[CLKID_BOOT_ROM]		= &txl_boot_rom.hw,
	/*HHI_GCLK_MPEG1 31*/

	[CLKID_AHB_DATA_BUS]	= &txl_ahb_data_bus.hw,
	/*HHI_GCLK_MPEG2 1*/
	[CLKID_AHB_CTRL_BUS]	= &txl_ahb_ctrl_bus.hw,
	/*HHI_GCLK_MPEG2 2*/
	[CLKID_USB1_TO_DDR]		= &txl_usb1_to_ddr.hw,
	/*HHI_GCLK_MPEG2 8*/
	[CLKID_USB0_TO_DDR]		= &txl_usb0_to_ddr.hw,
	/*HHI_GCLK_MPEG2 9*/
	[CLKID_AIU_PCLK]		= &txl_aiu_pclk.hw,
	/*HHI_GCLK_MPEG2 10*/
	[CLKID_MMC_PCLK]		= &txl_mmc_pclk.hw,
	/*HHI_GCLK_MPEG2 11*/
	[CLKID_HDMI_HDCP22]		= &txl_hdmi_hdcp22.hw,
	/*HHI_GCLK_MPEG2 13*/
	[CLKID_UART2]			= &txl_uart2.hw,
	/*HHI_GCLK_MPEG2 15*/
	[CLKID_UART3]			= &txl_uart3.hw,
	/*HHI_GCLK_MPEG2 21*/
	[CLKID_VPU_INTR]		= &txl_vpu_intr.hw,
	/*HHI_GCLK_MPEG2 25*/
	[CLKID_SEC_AHB_AHB3_BRIDGE]	 = &txl_sec_ahb_ahb3_bridge.hw,
	/*HHI_GCLK_MPEG2 26*/
	[CLKID_DEMOD_TVFE]		= &txl_demod_tvfe.hw,
	/*HHI_GCLK_MPEG2 28*/
	[CLKID_GIC]				= &txl_gic.hw,
	/*HHI_GCLK_MPEG2 30*/

	[CLKID_VCLK2_VENCI0]	    = &txl_vclk2_venci0.hw,
	/*HHI_GCLK_OTHER 1*/
	[CLKID_VCLK2_VENCI1]	    = &txl_vclk2_venci1.hw,
	/*HHI_GCLK_OTHER 2*/
	[CLKID_VCLK2_VENCP0]	    = &txl_vclk2_vencp0.hw,
	/*HHI_GCLK_OTHER 3*/
	[CLKID_VCLK2_VENCP1]	    = &txl_vclk2_vencp1.hw,
	/*HHI_GCLK_OTHER 4*/
	[CLKID_VCLK2_VENCT0]	    = &txl_vclk2_venct0.hw,
	/*HHI_GCLK_OTHER 5*/
	[CLKID_VCLK2_VENCT1]	    = &txl_vclk2_venct1.hw,
	/*HHI_GCLK_OTHER 6*/
	[CLKID_VCLK2_OTHER]	    = &txl_vclk2_other.hw,
	/*HHI_GCLK_OTHER 7*/
	[CLKID_VCLK2_ENCI]	    = &txl_vclk2_enci.hw,
	/*HHI_GCLK_OTHER 8*/
	[CLKID_VCLK2_ENCP]	    = &txl_vclk2_encp.hw,
	/*HHI_GCLK_OTHER 9*/
	[CLKID_DAC_CLK]		    = &txl_dac_clk.hw,
	/*HHI_GCLK_OTHER 10*/
	[CLKID_AOCLK_GATE]	    = &txl_aoclk_gate.hw,
	/*HHI_GCLK_OTHER 14*/
	[CLKID_IEC958_GATE]	    = &txl_iec958_gate.hw,
	/*HHI_GCLK_OTHER 16*/
	[CLKID_ENC480P]		    = &txl_enc480p.hw,
	/*HHI_GCLK_OTHER 20*/
	[CLKID_RNG1]		    = &txl_rng1.hw,
	/*HHI_GCLK_OTHER 21*/
	[CLKID_VCLK2_ENCT]	    = &txl_vclk2_enct.hw,
	/*HHI_GCLK_OTHER 22*/
	[CLKID_VCLK2_ENCL]	    = &txl_vclk2_encl.hw,
	/*HHI_GCLK_OTHER 23*/
	[CLKID_VCLK2_VENCLMMC]	    = &txl_vclk2_venclmmc.hw,
	/*HHI_GCLK_OTHER 24*/
	[CLKID_VCLK2_VENCL]	    = &txl_vclk2_vencl.hw,
	/*HHI_GCLK_OTHER 25*/
	[CLKID_VCLK2_OTHER1]	    = &txl_vclk2_other1.hw,
	/*HHI_GCLK_OTHER 26*/

	[CLKID_AO_MEDIA_CPU]	= &txl_ao_media_cpu.hw,
	[CLKID_AO_AHB_SRAM]	    = &txl_ao_ahb_sram.hw,
	[CLKID_AO_AHB_BUS]	    = &txl_ao_ahb_bus.hw,
	[CLKID_AO_IFACE]	    = &txl_ao_iface.hw,
	[CLKID_AO_I2C]		    = &txl_ao_i2c.hw,
	[CLKID_CPU_FCLK_P00]	= &txl_cpu_fixedpll_p00.hw,
	[CLKID_CPU_FCLK_P01]	= &txl_cpu_fixedpll_p01.hw,
	[CLKID_CPU_FCLK_P0]		= &txl_cpu_fixedpll_p0.hw,
	[CLKID_CPU_FCLK_P10]	= &txl_cpu_fixedpll_p10.hw,
	[CLKID_CPU_FCLK_P11]	= &txl_cpu_fixedpll_p11.hw,
	[CLKID_CPU_FCLK_P1]		= &txl_cpu_fixedpll_p1.hw,
	[CLKID_CPU_FCLK_P]		= &txl_cpu_fixedpll_p.hw,
	[CLKID_CPU_CLK]		    = &txl_cpu_clk.mux.hw,
};
/* Convenience tables to populate base addresses in .probe */

static struct meson_clk_pll *const txl_clk_plls[] = {
	&txl_fixed_pll,
	&txl_sys_pll,
	&txl_gp0_pll,
	&txl_gp1_pll,
	/*&txl_adc_pll,*/
	&txl_hdmi_pll,
};

static struct meson_clk_mpll *const txl_clk_mplls[] = {
	&txl_mpll0,
	&txl_mpll1,
	&txl_mpll2,
	&txl_mpll3,
};

static struct clk_gate *txl_clk_gates[] = {
	&txl_clk81,
	&txl_ddr,
	&txl_dos,
	&txl_isa,
	&txl_pl301,
	&txl_periphs,
	&txl_spicc_0,
	&txl_i2c,
	&txl_sana,
	&txl_smart_card,
	&txl_rng0,
	&txl_uart0,
	&txl_async_fifo,
	&txl_hiu_reg,
	&txl_hdmirx_apb,
	&txl_assist_misc,
	&txl_emmc_b,
	&txl_emmc_c,
	&txl_dma,
	&txl_acodec,
	&txl_atv_demod,
	&txl_spi,
	&txl_pclk_tvfe,
	&txl_i2s_spdif,
	&txl_eth_core,
	&txl_demux,
	&txl_aiu_glue,
	&txl_iec958,
	&txl_i2s_out,
	&txl_amclk_measure,
	&txl_aififo2,
	&txl_mixer,
	&txl_mixer_iface,
	&txl_adc,
	&txl_aiu_top,
	&txl_uart1,
	&txl_usb0,
	&txl_usb1,
	&txl_g2d,
	&txl_reset,
	&txl_dos_parser,
	&txl_usb_general,
	&txl_ahb_arb0,
	&txl_efuse,
	&txl_boot_rom,
	&txl_ahb_data_bus,
	&txl_ahb_ctrl_bus,
	&txl_usb1_to_ddr,
	&txl_usb0_to_ddr,
	&txl_aiu_pclk,
	&txl_mmc_pclk,
	&txl_hdmi_hdcp22,
	&txl_uart2,
	&txl_uart3,
	&txl_vpu_intr,
	&txl_sec_ahb_ahb3_bridge,
	&txl_demod_tvfe,
	&txl_gic,
	&txl_vclk2_venci0,
	&txl_vclk2_venci1,
	&txl_vclk2_vencp0,
	&txl_vclk2_vencp1,
	&txl_vclk2_venct0,
	&txl_vclk2_venct1,
	&txl_vclk2_other,
	&txl_vclk2_enci,
	&txl_vclk2_encp,
	&txl_dac_clk,
	&txl_aoclk_gate,
	&txl_iec958_gate,
	&txl_enc480p,
	&txl_rng1,
	&txl_vclk2_enct,
	&txl_vclk2_encl,
	&txl_vclk2_venclmmc,
	&txl_vclk2_vencl,
	&txl_vclk2_other1,
	&txl_ao_media_cpu,
	&txl_ao_ahb_sram,
	&txl_ao_ahb_bus,
	&txl_ao_iface,
	&txl_ao_i2c,
};

static void __init txl_clkc_init(struct device_node *np)
{
	int ret, clkid, i;
	struct clk_hw *parent_hw;
	struct clk *parent_clk;

	/*  Generic clocks and PLLs */
	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		return;
	}

	/* Populate base address for PLLs */
	for (i = 0; i < ARRAY_SIZE(txl_clk_plls); i++)
	txl_clk_plls[i]->base = clk_base;

	/* Populate base address for MPLLs */
	for (i = 0; i < ARRAY_SIZE(txl_clk_mplls); i++)
	txl_clk_mplls[i]->base = clk_base;

	/* Populate the base address for CPU clk */
	txl_cpu_clk.mux.reg = clk_base
				+ (unsigned long)txl_cpu_clk.mux.reg;
	txl_cpu_fixedpll_p00.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p00.reg;
	txl_cpu_fixedpll_p01.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p01.reg;
	txl_cpu_fixedpll_p10.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p10.reg;
	txl_cpu_fixedpll_p11.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p11.reg;
	txl_cpu_fixedpll_p0.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p0.reg;
	txl_cpu_fixedpll_p1.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p1.reg;
	txl_cpu_fixedpll_p.reg = clk_base
				+ (unsigned long)txl_cpu_fixedpll_p.reg;

	/* Populate the base address for the MPEG clks */
	txl_mpeg_clk_sel.reg = clk_base + (unsigned long)txl_mpeg_clk_sel.reg;
	txl_mpeg_clk_div.reg = clk_base + (unsigned long)txl_mpeg_clk_div.reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(txl_clk_gates); i++)
		txl_clk_gates[i]->reg = clk_base +
			(unsigned long)txl_clk_gates[i]->reg;

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

	clk_data.clks = clks;
	clk_data.clk_num = NR_CLKS;

	/*register all clks*/
	for (clkid = 0; clkid < CLOCK_GATE; clkid++) {
		if (txl_clk_hws[clkid]) {
		clks[clkid] = clk_register(NULL, txl_clk_hws[clkid]);
		WARN_ON(IS_ERR(clks[clkid]));
		}
	}

	meson_txl_sdemmc_init();
	meson_txl_media_init();
	meson_init_gpu();

	parent_hw = clk_hw_get_parent(&txl_cpu_clk.mux.hw);
	parent_clk = parent_hw->clk;
	ret = clk_notifier_register(parent_clk, &txl_cpu_clk.clk_nb);
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

CLK_OF_DECLARE(txl, "amlogic,txl-clkc", txl_clkc_init);
