/*
 * drivers/amlogic/clk/g12a/g12a_ao.c
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

/* #undef pr_debug */
/* #define pr_debug pr_info */
static void __iomem *ao_clk_base;

static struct clk_mux aoclk81 = {
	.reg = (void *)AO_RTI_PWR_CNTL_REG0,
	.mask = 0x1,
	.shift = 8,
	.flags = CLK_MUX_READ_ONLY,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "aoclk81",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "clk81", "ao_slow_clk" },
		.num_parents = 2,
		.flags = (CLK_SET_RATE_NO_REPARENT),
	},
};

/* sar_adc_clk */
static struct clk_mux g12a_saradc_mux = {
	.reg = (void *)AO_SAR_CLK,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_saradc_mux",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "aoclk81"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_divider g12a_saradc_div = {
	.reg = (void *)AO_SAR_CLK,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_saradc_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "g12a_saradc_mux" },
		.num_parents = 1,
		.flags = (CLK_DIVIDER_ROUND_CLOSEST),
	},
};

static struct clk_gate g12a_saradc_gate = {
	.reg = (void *)AO_SAR_CLK,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "g12a_saradc_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "g12a_saradc_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),
	},
};

/* Array of all clocks provided by this provider */

static struct clk_hw *g12a_ao_clk_hws[] = {
	[CLKID_AO_CLK81 - CLKID_AO_CLK81]
		= &aoclk81.hw,
	[CLKID_SARADC_MUX - CLKID_AO_CLK81]
		= &g12a_saradc_mux.hw,
	[CLKID_SARADC_DIV - CLKID_AO_CLK81]
		= &g12a_saradc_div.hw,
	[CLKID_SARADC_GATE - CLKID_AO_CLK81]
		= &g12a_saradc_gate.hw,
};

static void __init g12a_aoclkc_init(struct device_node *np)
{
	int clkid;

	/*  Generic clocks and PLLs */
	ao_clk_base = of_iomap(np, 0);
	if (!ao_clk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		/* return -ENXIO; */
		return;
	}
	/* pr_debug("%s: iomap clk_base ok!", __func__); */
	/* Populate the base address for ao clk */
	aoclk81.reg = ao_clk_base + (unsigned long)aoclk81.reg;
	g12a_saradc_mux.reg = ao_clk_base
					+ (unsigned long)g12a_saradc_mux.reg;
	g12a_saradc_div.reg = ao_clk_base
					+ (unsigned long)g12a_saradc_div.reg;
	g12a_saradc_gate.reg = ao_clk_base
					+ (unsigned long)g12a_saradc_gate.reg;

	if (!clks) {
		clks = kzalloc(NR_CLKS*sizeof(struct clk *), GFP_KERNEL);
		if (!clks) {
			/* pr_err("%s: alloc clks fail!", __func__); */
			/* return -ENOMEM; */
			return;
		}
		clk_numbers = NR_CLKS;
	}

	for (clkid = CLKID_AO_BASE; clkid < NR_CLKS; clkid++) {
		if (g12a_ao_clk_hws[clkid - CLKID_AO_BASE]) {
			clks[clkid] = clk_register(NULL,
			g12a_ao_clk_hws[clkid - CLKID_AO_BASE]);
			WARN_ON(IS_ERR(clks[clkid]));
		}
	}

	pr_info("%s: register ao clk ok!\n", __func__);
	/*of_clk_add_provider in ee clk g12a_clkc_init*/
}

CLK_OF_DECLARE(g12a, "amlogic,g12a-aoclkc", g12a_aoclkc_init);
CLK_OF_DECLARE(g12b, "amlogic,g12b-aoclkc", g12a_aoclkc_init);
CLK_OF_DECLARE(sm1, "amlogic,sm1-aoclkc", g12a_aoclkc_init);


