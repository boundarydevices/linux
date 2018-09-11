/*
 * drivers/amlogic/clk/tl1/tl1_clk_misc.c
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

/*ts clock */
PNAME(ts_parent_names) = { "xtal" };
static DIV(tl1_ts_div, HHI_TS_CLK_CNTL, 0, 7, "xtal",
			CLK_GET_RATE_NOCACHE);
static GATE(tl1_ts_gate, HHI_TS_CLK_CNTL, 8, "tl1_ts_clk_div",
			CLK_GET_RATE_NOCACHE);

/* spicc0 and spicc1 clocks */
static const char * const spicc_parent_names[] = { "xtal",
	"clk81", "fclk_div4", "fclk_div3", "fclk_div2", "fclk_div5",
	"fclk_div7", "gpo_pll"};

static struct clk_mux tl1_spicc0_mux = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.mask = 0x7,
	.shift = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_mux",
		.ops = &clk_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = ARRAY_SIZE(spicc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider tl1_spicc0_div = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.shift = 0,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc0_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate tl1_spicc0_gate = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.bit_idx = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc0_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux tl1_spicc1_mux = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.mask = 0x7,
	.shift = 23,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_mux",
		.ops = &clk_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = ARRAY_SIZE(spicc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_divider tl1_spicc1_div = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.shift = 16,
	.width = 6,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "spicc_p1_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "spicc_p1_mux" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate tl1_spicc1_gate = {
	.reg = (void *)HHI_SPICC_CLK_CNTL,
	.bit_idx = 22,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_p1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "spicc_p1_div" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_mux *tl1_misc_clk_muxes[] = {
	&tl1_spicc0_mux,
	&tl1_spicc1_mux,
};

static struct clk_divider *tl1_misc_clk_divs[] = {
	&tl1_spicc0_div,
	&tl1_spicc1_div,
	&tl1_ts_div,
};

static struct clk_gate *tl1_media_clk_gates[] = {
	&tl1_spicc0_gate,
	&tl1_spicc1_gate,
	&tl1_ts_gate,
};

static struct meson_composite misc_composite[] = {
	{CLKID_TS_CLK_COMP, "ts_clk_composite",
	ts_parent_names, ARRAY_SIZE(ts_parent_names),
	NULL, &tl1_ts_div.hw,
	&tl1_ts_gate.hw, 0
	},/* ts_clk */

	{CLKID_SPICC0_COMP, "spicc0_composite",
	spicc_parent_names, ARRAY_SIZE(spicc_parent_names),
	&tl1_spicc0_mux.hw, &tl1_spicc0_div.hw,
	&tl1_spicc0_gate.hw, 0
	},/* spicc0 */

	{CLKID_SPICC1_COMP, "spicc1_composite",
	spicc_parent_names, ARRAY_SIZE(spicc_parent_names),
	&tl1_spicc1_mux.hw, &tl1_spicc1_div.hw,
	&tl1_spicc1_gate.hw, 0
	},/* spicc1 */

	{},
};
void meson_tl1_misc_init(void)
{
	int i, length;

	/* Populate base address for misc muxes */
	for (i = 0; i < ARRAY_SIZE(tl1_misc_clk_muxes); i++)
		tl1_misc_clk_muxes[i]->reg = clk_base +
			(unsigned long)tl1_misc_clk_muxes[i]->reg;

	/* Populate base address for misc divs */
	for (i = 0; i < ARRAY_SIZE(tl1_misc_clk_divs); i++)
		tl1_misc_clk_divs[i]->reg = clk_base +
			(unsigned long)tl1_misc_clk_divs[i]->reg;

	/* Populate base address for misc gates */
	for (i = 0; i < ARRAY_SIZE(tl1_media_clk_gates); i++)
		tl1_media_clk_gates[i]->reg = clk_base +
			(unsigned long)tl1_media_clk_gates[i]->reg;

	length = ARRAY_SIZE(misc_composite);

	meson_clk_register_composite(clks, misc_composite, length - 1);
}
