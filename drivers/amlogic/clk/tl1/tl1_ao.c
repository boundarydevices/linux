/*
 * drivers/amlogic/clk/tl1/tl1_ao.c
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

#define MESON_AO_GATE_TL1(_name, _reg, _bit)			\
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

/* tl1 ao clock gates */
MESON_AO_GATE_TL1(tl1_ao_ahb_bus, AO_CLK_GATE0, 0);
MESON_AO_GATE_TL1(tl1_ao_ir, AO_CLK_GATE0, 1);
MESON_AO_GATE_TL1(tl1_ao_i2c_master, AO_CLK_GATE0, 2);
MESON_AO_GATE_TL1(tl1_ao_i2c_slave, AO_CLK_GATE0, 3);
MESON_AO_GATE_TL1(tl1_ao_uart1, AO_CLK_GATE0, 4);
MESON_AO_GATE_TL1(tl1_ao_prod_i2c, AO_CLK_GATE0, 5);
MESON_AO_GATE_TL1(tl1_ao_uart2, AO_CLK_GATE0, 6);
MESON_AO_GATE_TL1(tl1_ao_ir_blaster, AO_CLK_GATE0, 7);
MESON_AO_GATE_TL1(tl1_ao_sar_adc, AO_CLK_GATE0, 8);
/* FIXME for AO_CLK_GATE0_SP clocks */

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
static struct clk_mux tl1_saradc_mux = {
	.reg = (void *)AO_SAR_CLK,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tl1_saradc_mux",
		.ops = &clk_mux_ops,
		.parent_names = (const char *[]){ "xtal", "aoclk81"},
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_divider tl1_saradc_div = {
	.reg = (void *)AO_SAR_CLK,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tl1_saradc_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "tl1_saradc_mux" },
		.num_parents = 1,
		.flags = (CLK_DIVIDER_ROUND_CLOSEST),
	},
};

static struct clk_gate tl1_saradc_gate = {
	.reg = (void *)AO_SAR_CLK,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tl1_saradc_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "tl1_saradc_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
	},
};

/* Array of all clocks provided by this provider */
static struct clk_hw *tl1_ao_clk_hws[] = {
	[CLKID_AO_CLK81 - CLKID_AO_CLK81]
		= &aoclk81.hw,
	[CLKID_SARADC_MUX - CLKID_AO_CLK81]
		= &tl1_saradc_mux.hw,
	[CLKID_SARADC_DIV - CLKID_AO_CLK81]
		= &tl1_saradc_div.hw,
	[CLKID_SARADC_GATE - CLKID_AO_CLK81]
		= &tl1_saradc_gate.hw,
	[CLKID_AO_AHB_BUS - CLKID_AO_CLK81]
		= &tl1_ao_ahb_bus.hw,
	[CLKID_AO_IR - CLKID_AO_CLK81]
		= &tl1_ao_ir.hw,
	[CLKID_AO_I2C_MASTER - CLKID_AO_CLK81]
		= &tl1_ao_i2c_master.hw,
	[CLKID_AO_I2C_SLAVE - CLKID_AO_CLK81]
		= &tl1_ao_i2c_slave.hw,
	[CLKID_AO_UART1 - CLKID_AO_CLK81]
		= &tl1_ao_uart1.hw,
	[CLKID_AO_PROD_I2C - CLKID_AO_CLK81]
		= &tl1_ao_prod_i2c.hw,
	[CLKID_AO_UART2 - CLKID_AO_CLK81]
		= &tl1_ao_uart2.hw,
	[CLKID_AO_IR_BLASTER - CLKID_AO_CLK81]
		= &tl1_ao_ir_blaster.hw,
	[CLKID_AO_SAR_ADC - CLKID_AO_CLK81]
		= &tl1_ao_sar_adc.hw,
};

static struct clk_gate *tl1_ao_clk_gates[] = {
	&tl1_ao_ahb_bus,
	&tl1_ao_i2c_master,
	&tl1_ao_i2c_slave,
	&tl1_ao_uart1,
	&tl1_ao_prod_i2c,
	&tl1_ao_uart2,
	&tl1_ao_ir_blaster,
	&tl1_ao_sar_adc,
};

static int tl1_aoclkc_probe(struct platform_device *pdev)
{
	void __iomem *aoclk_base;
	int clkid, i;
	struct device *dev = &pdev->dev;

	/*  Generic clocks and PLLs */
	aoclk_base = of_iomap(dev->of_node, 0);
	if (!aoclk_base) {
		pr_err("%s: Unable to map clk base\n", __func__);
		return -ENXIO;
	}
	/* Populate the base address for ao clk */
	for (i = 0; i < ARRAY_SIZE(tl1_ao_clk_gates); i++)
		tl1_ao_clk_gates[i]->reg = aoclk_base +
			(unsigned long)tl1_ao_clk_gates[i]->reg;

	aoclk81.reg = aoclk_base + (unsigned long)aoclk81.reg;
	tl1_saradc_mux.reg = aoclk_base + (unsigned long)tl1_saradc_mux.reg;
	tl1_saradc_div.reg = aoclk_base + (unsigned long)tl1_saradc_div.reg;
	tl1_saradc_gate.reg = aoclk_base + (unsigned long)tl1_saradc_gate.reg;

	for (clkid = CLKID_AO_BASE; clkid < NR_CLKS; clkid++) {
		if (tl1_ao_clk_hws[clkid-CLKID_AO_BASE]) {
			clks[clkid] = clk_register(NULL,
			tl1_ao_clk_hws[clkid-CLKID_AO_BASE]);
			WARN_ON(IS_ERR(clks[clkid]));
		}
	}

	return 0;
}

static const struct of_device_id tl1_aoclkc_match_table[] = {
	{ .compatible = "amlogic,tl1-aoclkc" },
	{ },
};

static struct platform_driver tl1_aoclk_driver = {
	.probe		= tl1_aoclkc_probe,
	.driver		= {
		.name	= "tl1-aoclkc",
		.of_match_table = tl1_aoclkc_match_table,
	},
};

builtin_platform_driver(tl1_aoclk_driver);
