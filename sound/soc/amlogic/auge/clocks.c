/*
 * sound/soc/amlogic/auge/audio_clocks.c
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <dt-bindings/clock/amlogic,axg-audio-clk.h>

#include <linux/clk-provider.h>
#include "regs.h"

#define DRV_NAME "aml-audio-clocks"

spinlock_t aclk_lock;

#define CLOCK_GATE(_name, _reg, _bit) \
struct clk_gate _name = { \
	.reg = (void __iomem *)(_reg), \
	.bit_idx = (_bit), \
	.lock = &aclk_lock,	\
	.hw.init = &(struct clk_init_data) { \
		.name = #_name,	\
		.ops = &clk_gate_ops, \
		.parent_names = (const char *[]){ "clk81" }, \
		.num_parents = 1, \
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED), \
	}, \
}

static CLOCK_GATE(audio_ddr_arb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 0);
static CLOCK_GATE(audio_pdm, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 1);
static CLOCK_GATE(audio_tdmina, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 2);
static CLOCK_GATE(audio_tdminb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 3);
static CLOCK_GATE(audio_tdminc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 4);
static CLOCK_GATE(audio_tdminlb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 5);
static CLOCK_GATE(audio_tdmouta, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 6);
static CLOCK_GATE(audio_tdmoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 7);
static CLOCK_GATE(audio_tdmoutc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 8);
static CLOCK_GATE(audio_frddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 9);
static CLOCK_GATE(audio_frddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 10);
static CLOCK_GATE(audio_frddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 11);
static CLOCK_GATE(audio_toddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 12);
static CLOCK_GATE(audio_toddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 13);
static CLOCK_GATE(audio_toddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 14);
static CLOCK_GATE(audio_loopback, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 15);
static CLOCK_GATE(audio_spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 16);
static CLOCK_GATE(audio_spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 17);
static CLOCK_GATE(audio_resample, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 18);
static CLOCK_GATE(audio_power_detect,
		AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 19);

static struct clk_gate *audio_clk_gates[] = {
	&audio_ddr_arb,
	&audio_pdm,
	&audio_tdmina,
	&audio_tdminb,
	&audio_tdminc,
	&audio_tdminlb,
	&audio_tdmouta,
	&audio_tdmoutb,
	&audio_tdmoutc,
	&audio_frddra,
	&audio_frddrb,
	&audio_frddrc,
	&audio_toddra,
	&audio_toddrb,
	&audio_toddrc,
	&audio_loopback,
	&audio_spdifin,
	&audio_spdifout,
	&audio_resample,
	&audio_power_detect,
};

/* Array of all clocks provided by this provider */
static struct clk_hw *audio_clk_hws[] = {
	[CLKID_AUDIO_DDR_ARB] = &audio_ddr_arb.hw,
	[CLKID_AUDIO_PDM] = &audio_pdm.hw,
	[CLKID_AUDIO_TDMINA] = &audio_tdmina.hw,
	[CLKID_AUDIO_TDMINB] = &audio_tdminb.hw,
	[CLKID_AUDIO_TDMINC] = &audio_tdminc.hw,
	[CLKID_AUDIO_TDMINLB] = &audio_tdminlb.hw,
	[CLKID_AUDIO_TDMOUTA] = &audio_tdmouta.hw,
	[CLKID_AUDIO_TDMOUTB] = &audio_tdmoutb.hw,
	[CLKID_AUDIO_TDMOUTC] = &audio_tdmoutc.hw,
	[CLKID_AUDIO_FRDDRA] = &audio_frddra.hw,
	[CLKID_AUDIO_FRDDRB] = &audio_frddrb.hw,
	[CLKID_AUDIO_FRDDRC] = &audio_frddrc.hw,
	[CLKID_AUDIO_TODDRA] = &audio_toddra.hw,
	[CLKID_AUDIO_TODDRB] = &audio_toddrb.hw,
	[CLKID_AUDIO_TODDRC] = &audio_toddrc.hw,
	[CLKID_AUDIO_LOOPBACK] = &audio_loopback.hw,
	[CLKID_AUDIO_SPDIFIN] = &audio_spdifin.hw,
	[CLKID_AUDIO_SPDIFOUT] = &audio_spdifout.hw,
	[CLKID_AUDIO_RESAMPLE] = &audio_resample.hw,
	[CLKID_AUDIO_POWER_DETECT] = &audio_power_detect.hw,
};

const char *mclk_parent_names[] = {"mpll0", "mpll1", "mpll2", "mpll3",
	"hifi_pll", "fclk_div3", "fclk_div4", "gp0_pll"};

#define CLOCK_COM_MUX(_name, _reg, _mask, _shift) \
struct clk_mux _name##_mux = { \
	.reg = (void __iomem *)(_reg), \
	.mask = (_mask), \
	.shift = (_shift), \
	.lock = &aclk_lock,	\
}
#define CLOCK_COM_DIV(_name, _reg, _shift, _width) \
struct clk_divider _name##_div = { \
	.reg = (void __iomem *)(_reg), \
	.shift = (_shift), \
	.width = (_width), \
	.lock = &aclk_lock,	\
}
#define CLOCK_COM_GATE(_name, _reg, _bit) \
struct clk_gate _name##_gate = { \
	.reg = (void __iomem *)(_reg), \
	.bit_idx = (_bit), \
	.lock = &aclk_lock,	\
}

/* mclk_a */
CLOCK_COM_MUX(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL), 31);
/* mclk_b */
CLOCK_COM_MUX(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL), 31);
/* mclk_c */
CLOCK_COM_MUX(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL), 31);
/* mclk_d */
CLOCK_COM_MUX(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL), 31);
/* mclk_e */
CLOCK_COM_MUX(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL), 31);
/* mclk_f */
CLOCK_COM_MUX(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL), 31);
/* spdifin */
CLOCK_COM_MUX(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0, 8);
CLOCK_COM_GATE(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 31);
/* spdifout */
CLOCK_COM_MUX(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 0, 10);
CLOCK_COM_GATE(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 31);
/* pdmin0 */
CLOCK_COM_MUX(pdmin0, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL0), 0x7, 24);
CLOCK_COM_DIV(pdmin0, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL0), 0, 16);
CLOCK_COM_GATE(pdmin0, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL0), 31);
/* pdmin1 */
CLOCK_COM_MUX(pdmin1, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL1), 0x7, 24);
CLOCK_COM_DIV(pdmin1, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL1), 0, 16);
CLOCK_COM_GATE(pdmin1, AUD_ADDR_OFFSET(EE_AUDIO_CLK_PDMIN_CTRL1), 31);

#define IOMAP_COM_CLK(_name, _iobase) \
do { \
	_name##_mux.reg += (unsigned long)(iobase); \
	_name##_div.reg += (unsigned long)(iobase); \
	_name##_gate.reg += (unsigned long)(iobase); \
} while (0)

#define REGISTER_CLK_COM(_name) \
	clk_register_composite(NULL, #_name, \
			mclk_parent_names, ARRAY_SIZE(mclk_parent_names), \
			&_name##_mux.hw, &clk_mux_ops, \
			&_name##_div.hw, &clk_divider_ops, \
			&_name##_gate.hw, &clk_gate_ops, \
			CLK_SET_RATE_NO_REPARENT)

static int register_audio_clk(struct clk **clks, void __iomem *iobase)
{
	IOMAP_COM_CLK(mclk_a, iobase);
	clks[CLKID_AUDIO_MCLK_A] = REGISTER_CLK_COM(mclk_a);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_A]));

	IOMAP_COM_CLK(mclk_b, iobase);
	clks[CLKID_AUDIO_MCLK_B] = REGISTER_CLK_COM(mclk_b);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_B]));

	IOMAP_COM_CLK(mclk_c, iobase);
	clks[CLKID_AUDIO_MCLK_C] = REGISTER_CLK_COM(mclk_c);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_C]));

	IOMAP_COM_CLK(mclk_d, iobase);
	clks[CLKID_AUDIO_MCLK_D] = REGISTER_CLK_COM(mclk_d);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_D]));

	IOMAP_COM_CLK(mclk_e, iobase);
	clks[CLKID_AUDIO_MCLK_E] = REGISTER_CLK_COM(mclk_e);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_E]));

	IOMAP_COM_CLK(mclk_f, iobase);
	clks[CLKID_AUDIO_MCLK_F] = REGISTER_CLK_COM(mclk_f);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_MCLK_F]));

	IOMAP_COM_CLK(spdifin, iobase);
	clks[CLKID_AUDIO_SPDIFIN_CTRL] = REGISTER_CLK_COM(spdifin);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_SPDIFIN_CTRL]));

	IOMAP_COM_CLK(spdifout, iobase);
	clks[CLKID_AUDIO_SPDIFOUT_CTRL] = REGISTER_CLK_COM(spdifout);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_SPDIFOUT_CTRL]));

	IOMAP_COM_CLK(pdmin0, iobase);
	clks[CLKID_AUDIO_PDMIN0] = REGISTER_CLK_COM(pdmin0);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_PDMIN0]));

	IOMAP_COM_CLK(pdmin1, iobase);
	clks[CLKID_AUDIO_PDMIN1] = REGISTER_CLK_COM(pdmin1);
	WARN_ON(IS_ERR(clks[CLKID_AUDIO_PDMIN1]));

	return 0;
}

static int aml_audio_clocks_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct clk **clks;
	struct clk_onecell_data *clk_data;
	void __iomem *clk_base;
	int clkid, ret;

	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		dev_err(dev, "%s: Unable to map clk base\n", __func__);
		return -ENXIO;
	}

	clk_data = devm_kmalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clks = devm_kmalloc(dev, NUM_AUDIO_CLKS * sizeof(*clks), GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	/* register all audio clks */
	if (ARRAY_SIZE(audio_clk_gates) == MCLK_BASE
			&& ARRAY_SIZE(audio_clk_gates) == MCLK_BASE) {
		dev_info(dev, "%s register audio gate clks\n", __func__);
		for (clkid = 0; clkid < MCLK_BASE; clkid++) {
			audio_clk_gates[clkid]->reg += (unsigned long)clk_base;
			clks[clkid] = clk_register(NULL, audio_clk_hws[clkid]);
			WARN_ON(IS_ERR(clks[clkid]));
		}
	} else {
		return -EINVAL;
	}

	/* register composite audio clks */
	register_audio_clk(clks, clk_base);

	clk_data->clks = clks;
	clk_data->clk_num = NUM_AUDIO_CLKS;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
			clk_data);
	if (ret < 0)
		dev_err(dev, "%s fail ret: %d\n", __func__, ret);

	return 0;
}

static const struct of_device_id amlogic_audio_clocks_of_match[] = {
	{ .compatible = "amlogic, audio_clocks" },
	{},
};

static struct platform_driver aml_audio_clocks_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = amlogic_audio_clocks_of_match,
	},
	.probe = aml_audio_clocks_probe,
};
module_platform_driver(aml_audio_clocks_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio clocks ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, amlogic_audio_clocks_of_match);
