/*
 * drivers/amlogic/clk/gxl/clk_misc.c
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

/* cts_vdin_meas_clk */
const char *meas_parent_names[] = { "xtal", "fclk_div4",
	"fclk_div3", "fclk_div5", "vid_pll_clk", "vid2_pll_clk"};

/* cts_vdin_meas_clk */
static struct clk_mux vdin_meas_mux = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.mask = 0x7,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_mux",
		.ops = &clk_mux_ops,
		.parent_names = meas_parent_names,
		.num_parents = 6,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider vdin_meas_div = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.shift = 0,
	.width = 7,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "vdin_meas_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate vdin_meas_gate = {
	.reg = (void *)HHI_VDIN_MEAS_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "vdin_meas_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *vdin_meas_clk_hws[] = {
[CLKID_VDIN_MEAS_MUX - CLKID_VDIN_MEAS_MUX] = &vdin_meas_mux.hw,
[CLKID_VDIN_MEAS_DIV - CLKID_VDIN_MEAS_MUX] = &vdin_meas_div.hw,
[CLKID_VDIN_MEAS_GATE - CLKID_VDIN_MEAS_MUX] = &vdin_meas_gate.hw,
};

/* cts_amclk */
const char *amclk_parent_names[] = { "ddr_pll_clk", "mpll0", "mpll1", "mpll2"};

static struct clk_mux amclk_mux = {
	.reg = (void *)HHI_AUD_CLK_CNTL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "amclk_mux",
		.ops = &clk_mux_ops,
		.parent_names = amclk_parent_names,
		.num_parents = 4,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider amclk_div = {
	.reg = (void *)HHI_AUD_CLK_CNTL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.flags = CLK_DIVIDER_ROUND_CLOSEST,
	.hw.init = &(struct clk_init_data){
		.name = "amclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "amclk_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate amclk_gate = {
	.reg = (void *)HHI_AUD_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "amclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "amclk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *amclk_hws[] = {
[CLKID_AMCLK_MUX - CLKID_AMCLK_MUX] = &amclk_mux.hw,
[CLKID_AMCLK_DIV - CLKID_AMCLK_MUX] = &amclk_div.hw,
[CLKID_AMCLK_GATE - CLKID_AMCLK_MUX] = &amclk_gate.hw,
};

/* cts_pdm */
const char *pdm_parent_names[] = { "amclk_composite",
	"mpll0", "mpll1", "mpll2"};

static struct clk_mux pdm_mux = {
	.reg = (void *)HHI_AUD_CLK_CNTL3,
	.mask = 0x3,
	.shift = 17,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pdm_mux",
		.ops = &clk_mux_ops,
		.parent_names = pdm_parent_names,
		.num_parents = 4,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider pdm_div = {
	.reg = (void *)HHI_AUD_CLK_CNTL3,
	.shift = 0,
	.width = 16,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pdm_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "pdm_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate pdm_gate = {
	.reg = (void *)HHI_AUD_CLK_CNTL3,
	.bit_idx = 16,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "pdm_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "pdm_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *pdm_hws[] = {
[CLKID_PDM_MUX - CLKID_PDM_MUX] = &pdm_mux.hw,
[CLKID_PDM_DIV - CLKID_PDM_MUX] = &pdm_div.hw,
[CLKID_PDM_GATE - CLKID_PDM_MUX] = &pdm_gate.hw,
};

/* cts_clk_i958 */
const char *i958_parent_names[] = { "NULL", "mpll0", "mpll1", "mpll2"};
const char *i958_ext_parent_names[] = {"amclk_composite", "i958_composite"};

static struct clk_mux i958_mux = {
	.reg = (void *)HHI_AUD_CLK_CNTL2,
	.mask = 0x3,
	.shift = 25,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "i958_mux",
		.ops = &clk_mux_ops,
		.parent_names = i958_parent_names,
		.num_parents = 4,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider i958_div = {
	.reg = (void *)HHI_AUD_CLK_CNTL2,
	.shift = 16,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "i958_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "i958_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate i958_gate = {
	.reg = (void *)HHI_AUD_CLK_CNTL2,
	.bit_idx = 24,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "i958_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "i958_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_mux i958_comp_spdif = {
	.reg = (void *)HHI_AUD_CLK_CNTL2,
	.mask = 0x1,
	.shift = 27,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "i958_comp_spdif",
		.ops = &clk_mux_ops,
		.parent_names = i958_ext_parent_names,
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *i958_hws[] = {
[CLKID_I958_MUX - CLKID_I958_MUX] = &i958_mux.hw,
[CLKID_I958_DIV - CLKID_I958_MUX] = &i958_div.hw,
[CLKID_I958_GATE - CLKID_I958_MUX] = &i958_gate.hw,
};

/* cts_pcm_mclk */
const char *pcm_mclk_parent_names[] = { "mpll0", "fclk_div4",
	"fclk_div3", "fclk_div5"};

static struct clk_mux pcm_mclk_mux = {
	.reg = (void *)HHI_PCM_CLK_CNTL,
	.mask = 0x3,
	.shift = 10,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcm_mclk_mux",
		.ops = &clk_mux_ops,
		.parent_names = pcm_mclk_parent_names,
		.num_parents = 4,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_divider pcm_mclk_div = {
	.reg = (void *)HHI_PCM_CLK_CNTL,
	.shift = 0,
	.width = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcm_mclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "pcm_mclk_mux" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate pcm_mclk_gate = {
	.reg = (void *)HHI_PCM_CLK_CNTL,
	.bit_idx = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "pcm_mclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "pcm_mclk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *pcm_mclk_hws[] = {
[CLKID_PCM_MCLK_MUX - CLKID_PCM_MCLK_MUX] = &pcm_mclk_mux.hw,
[CLKID_PCM_MCLK_DIV - CLKID_PCM_MCLK_MUX] = &pcm_mclk_div.hw,
[CLKID_PCM_MCLK_GATE - CLKID_PCM_MCLK_MUX] = &pcm_mclk_gate.hw,
};

/* cts_pcm_sclk */
static struct clk_divider pcm_sclk_div = {
	.reg = (void *)HHI_PCM_CLK_CNTL,
	.shift = 16,
	.width = 6,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcm_sclk_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "pcm_mclk_composite" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

static struct clk_gate pcm_sclk_gate = {
	.reg = (void *)HHI_PCM_CLK_CNTL,
	.bit_idx = 22,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data) {
		.name = "pcm_sclk_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "pcm_sclk_div" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	},
};

/* cts_sar_adc_clk */
const char *saradc_parent_names[] = { "xtal", "clk81" };
static struct clk_mux gxl_saradc_mux = {
	.reg = (void *)HHI_SAR_CLK_CNTL,
	.mask = 0x3,
	.shift = 9,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gxl_saradc_mux",
		.ops = &clk_mux_ops,
		.parent_names = saradc_parent_names,
		.num_parents = 2,
		.flags = (CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_divider gxl_saradc_div = {
	.reg = (void *)HHI_SAR_CLK_CNTL,
	.shift = 0,
	.width = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gxl_saradc_div",
		.ops = &clk_divider_ops,
		.parent_names = (const char *[]){ "gxl_saradc_mux" },
		.num_parents = 1,
		.flags = (CLK_DIVIDER_ROUND_CLOSEST),
	},
};

static struct clk_gate gxl_saradc_gate = {
	.reg = (void *)HHI_SAR_CLK_CNTL,
	.bit_idx = 8,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "gxl_saradc_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "gxl_saradc_div" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),
	},
};

static struct clk_hw *saradc_hws[] = {
[CLKID_SARADC_MUX - CLKID_SARADC_MUX] = &gxl_saradc_mux.hw,
[CLKID_SARADC_DIV - CLKID_SARADC_MUX] = &gxl_saradc_div.hw,
[CLKID_SARADC_GATE - CLKID_SARADC_MUX] = &gxl_saradc_gate.hw,
};

void amlogic_init_misc(void)
{
	/* cts_vdin_meas_clk */
	vdin_meas_mux.reg = clk_base + (unsigned long)(vdin_meas_mux.reg);
	vdin_meas_div.reg = clk_base + (unsigned long)(vdin_meas_div.reg);
	vdin_meas_gate.reg = clk_base + (unsigned long)(vdin_meas_gate.reg);

	/* cts_amclk */
	amclk_mux.reg = clk_base + (unsigned long)(amclk_mux.reg);
	amclk_div.reg = clk_base + (unsigned long)(amclk_div.reg);
	amclk_gate.reg = clk_base + (unsigned long)(amclk_gate.reg);

	/* cts_pdm */
	pdm_mux.reg = clk_base + (unsigned long)(pdm_mux.reg);
	pdm_div.reg = clk_base + (unsigned long)(pdm_div.reg);
	pdm_gate.reg = clk_base + (unsigned long)(pdm_gate.reg);

	/* cts_clk_i958 */
	i958_mux.reg = clk_base + (unsigned long)(i958_mux.reg);
	i958_div.reg = clk_base + (unsigned long)(i958_div.reg);
	i958_gate.reg = clk_base + (unsigned long)(i958_gate.reg);

	/*clk_i958 spdif*/
	i958_comp_spdif.reg = clk_base + (unsigned long)(i958_comp_spdif.reg);

	/* cts_pclk_mclk */
	pcm_mclk_mux.reg = clk_base + (unsigned long)(pcm_mclk_mux.reg);
	pcm_mclk_div.reg = clk_base + (unsigned long)(pcm_mclk_div.reg);
	pcm_mclk_gate.reg = clk_base + (unsigned long)(pcm_mclk_gate.reg);

	/* cts_pclk_sclk */
	pcm_sclk_div.reg = clk_base + (unsigned long)(pcm_sclk_div.reg);
	pcm_sclk_gate.reg = clk_base + (unsigned long)(pcm_sclk_gate.reg);

	/* cts_sar_adc_clk */
	gxl_saradc_mux.reg = clk_base + (unsigned long)(gxl_saradc_mux.reg);
	gxl_saradc_div.reg = clk_base + (unsigned long)(gxl_saradc_div.reg);
	gxl_saradc_gate.reg = clk_base + (unsigned long)(gxl_saradc_gate.reg);

	clks[CLKID_VDIN_MEAS_COMP] = clk_register_composite(NULL,
		"vdin_meas_composite",
		meas_parent_names, 6,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_MUX - CLKID_VDIN_MEAS_MUX],
		&clk_mux_ops,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_DIV - CLKID_VDIN_MEAS_MUX],
		&clk_divider_ops,
		vdin_meas_clk_hws[CLKID_VDIN_MEAS_GATE - CLKID_VDIN_MEAS_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_VDIN_MEAS_COMP]))
		pr_err("%s: %d clk_register_composite vdin_meas_composite error\n",
		__func__, __LINE__);

	clks[CLKID_AMCLK_COMP] = clk_register_composite(NULL,
		"amclk_composite",
		amclk_parent_names, 4,
		amclk_hws[CLKID_AMCLK_MUX - CLKID_AMCLK_MUX],
		&clk_mux_ops,
		amclk_hws[CLKID_AMCLK_DIV - CLKID_AMCLK_MUX],
		&clk_divider_ops,
		amclk_hws[CLKID_AMCLK_GATE - CLKID_AMCLK_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_AMCLK_COMP]))
		pr_err("%s: %d clk_register_composite amclk_composite error\n",
		__func__, __LINE__);

	clks[CLKID_PDM_COMP] = clk_register_composite(NULL,
		"pdm_composite",
		pdm_parent_names, 4,
		pdm_hws[CLKID_PDM_MUX - CLKID_PDM_MUX],
		&clk_mux_ops,
		pdm_hws[CLKID_PDM_DIV - CLKID_PDM_MUX],
		&clk_divider_ops,
		pdm_hws[CLKID_PDM_GATE - CLKID_PDM_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_PDM_COMP]))
		pr_err("%s: %d clk_register_composite pdm_composite error\n",
		__func__, __LINE__);

	clks[CLKID_I958_COMP] = clk_register_composite(NULL,
		"i958_composite",
		i958_parent_names, 4,
		i958_hws[CLKID_I958_MUX - CLKID_I958_MUX],
		&clk_mux_ops,
		i958_hws[CLKID_I958_DIV - CLKID_I958_MUX],
		&clk_divider_ops,
		i958_hws[CLKID_I958_GATE - CLKID_I958_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_I958_COMP]))
		pr_err("%s: %d clk_register_composite i958_composite error\n",
		__func__, __LINE__);

	clks[CLKID_PCM_MCLK_COMP] = clk_register_composite(NULL,
		"pcm_mclk_composite",
		pcm_mclk_parent_names, 4,
		pcm_mclk_hws[CLKID_PCM_MCLK_MUX - CLKID_PCM_MCLK_MUX],
		&clk_mux_ops,
		pcm_mclk_hws[CLKID_PCM_MCLK_DIV - CLKID_PCM_MCLK_MUX],
		&clk_divider_ops,
		pcm_mclk_hws[CLKID_PCM_MCLK_GATE - CLKID_PCM_MCLK_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_PCM_MCLK_COMP]))
		pr_err("%s: %d clk_register_composite pcm_mclk_composite error\n",
		__func__, __LINE__);

	clks[CLKID_SARADC_COMP] = clk_register_composite(NULL,
		"saradc_composite",
		saradc_parent_names, 2,
		saradc_hws[CLKID_SARADC_MUX - CLKID_SARADC_MUX],
		&clk_mux_ops,
		saradc_hws[CLKID_SARADC_DIV - CLKID_SARADC_MUX],
		&clk_divider_ops,
		saradc_hws[CLKID_SARADC_GATE - CLKID_SARADC_MUX],
		&clk_gate_ops, 0);
	if (IS_ERR(clks[CLKID_SARADC_COMP]))
		pr_err("%s: %d clk_register_composite saradc_composite error\n",
		__func__, __LINE__);

	clks[CLKID_PCM_SCLK_DIV] = clk_register(NULL, &pcm_sclk_div.hw);
	WARN_ON(IS_ERR(clks[CLKID_PCM_SCLK_DIV]));

	clks[CLKID_PCM_SCLK_GATE] = clk_register(NULL, &pcm_sclk_gate.hw);
	WARN_ON(IS_ERR(clks[CLKID_PCM_SCLK_GATE]));

	clks[CLKID_I958_COMP_SPDIF] = clk_register(NULL, &i958_comp_spdif.hw);
	WARN_ON(IS_ERR(clks[CLKID_I958_COMP_SPDIF]));

	pr_info("%s: register meson misc clk\n", __func__);
};

