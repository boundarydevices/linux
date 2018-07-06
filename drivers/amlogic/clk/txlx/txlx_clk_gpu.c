/*
 * drivers/amlogic/clk/txlx/txlx_clk_gpu.c
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
#include <dt-bindings/clock/amlogic,txlx-clkc.h>

#include "../clkc.h"
#include "txlx.h"

PNAME(gpus_parent_names) = { "xtal", "gp0_pll", "mpll2", "mpll1",
		"fclk_div7", "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(gpumux_parent_names) = { "gpu_p0_composite", "gpu_p1_composite"};

/* gpu p0 */
static MUX(gpu_p0_mux, HHI_MALI_CLK_CNTL, 0x7, 9, gpus_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(gpu_p0_div, HHI_MALI_CLK_CNTL, 0, 7, "gpu_p0_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(gpu_p0_gate, HHI_MALI_CLK_CNTL, 8, "gpu_p0_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
/* gpu p1 */
static MUX(gpu_p1_mux, HHI_MALI_CLK_CNTL, 0x7, 25, gpus_parent_names,
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(gpu_p1_div, HHI_MALI_CLK_CNTL, 16, 7, "gpu_p1_mux",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static GATE(gpu_p1_gate, HHI_MALI_CLK_CNTL, 24, "gpu_p1_div",
			CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

static MESON_MUX(gpu_mux, HHI_MALI_CLK_CNTL, 0x1, 31,
gpumux_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);

/* for init mux/div/gate clocks reg base*/
static struct clk_mux *txlx_gpu_clk_muxes[] = {
	&gpu_p0_mux,
	&gpu_p1_mux,
	&gpu_mux,
};

static struct clk_divider *txlx_gpu_clk_divs[] = {
	&gpu_p0_div,
	&gpu_p1_div,
};

static struct clk_gate *txlx_gpu_clk_gates[] = {
	&gpu_p0_gate,
	&gpu_p1_gate,
};

static struct meson_composite gpu_composite[] = {
	{CLKID_GPU_P0_COMP, "gpu_p0_composite",
	gpus_parent_names, ARRAY_SIZE(gpus_parent_names),
	&gpu_p0_mux.hw, &gpu_p0_div.hw,
	&gpu_p0_gate.hw, 0
	},/*gpu_p0*/

	{CLKID_GPU_P1_COMP, "gpu_p1_composite",
	gpus_parent_names, ARRAY_SIZE(gpus_parent_names),
	&gpu_p1_mux.hw, &gpu_p1_div.hw, &gpu_p1_gate.hw, 0
	},/*gpu_p1*/
};
void meson_init_gpu(void)
{
	int i, length;

	length = ARRAY_SIZE(gpu_composite);
	/* Populate base address for gpu muxes, divs,gates */
	for (i = 0; i < ARRAY_SIZE(txlx_gpu_clk_muxes); i++)
		txlx_gpu_clk_muxes[i]->reg = clk_base +
			(unsigned long)txlx_gpu_clk_muxes[i]->reg;
	for (i = 0; i < ARRAY_SIZE(txlx_gpu_clk_divs); i++)
		txlx_gpu_clk_divs[i]->reg = clk_base +
			(unsigned long)txlx_gpu_clk_divs[i]->reg;
	for (i = 0; i < ARRAY_SIZE(txlx_gpu_clk_gates); i++)
		txlx_gpu_clk_gates[i]->reg = clk_base +
			(unsigned long)txlx_gpu_clk_gates[i]->reg;

	meson_clk_register_composite(clks, gpu_composite, length);

	clks[CLKID_GPU_MUX] = clk_register(NULL, &gpu_mux.hw);
	WARN_ON(IS_ERR(clks[CLKID_GPU_MUX]));

	clk_set_parent(clks[CLKID_GPU_MUX], clks[CLKID_GPU_P0_COMP]);
	clk_prepare_enable(clks[CLKID_GPU_P0_COMP]);
	clk_prepare_enable(clks[CLKID_GPU_P1_COMP]);
	clk_set_rate(clks[CLKID_GPU_MUX], 400000000);
}
