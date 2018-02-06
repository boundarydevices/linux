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
#undef pr_fmt
#define pr_fmt(fmt) "g12a_clocks: " fmt

#include <dt-bindings/clock/amlogic,g12a-audio-clk.h>

#include "audio_clks.h"
#include "regs.h"

static spinlock_t aclk_lock;

static const char *const mclk_parent_names[] = {"mpll0", "mpll1",
	"mpll2", "mpll3", "hifi_pll", "fclk_div3", "fclk_div4", "gp0_pll"};

static const char *const audioclk_parent_names[] = {
	"mclk_a", "mclk_b", "mclk_c", "mclk_d", "mclk_e",
	"mclk_f", "i_slv_sclk_a", "i_slv_sclk_b", "i_slv_sclk_c",
	"i_slv_sclk_d", "i_slv_sclk_e", "i_slv_sclk_f", "i_slv_sclk_g",
	"i_slv_sclk_h", "i_slv_sclk_i", "i_slv_sclk_j"};

CLOCK_GATE(audio_ddr_arb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 0);
CLOCK_GATE(audio_pdm, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 1);
CLOCK_GATE(audio_tdmina, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 2);
CLOCK_GATE(audio_tdminb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 3);
CLOCK_GATE(audio_tdminc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 4);
CLOCK_GATE(audio_tdminlb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 5);
CLOCK_GATE(audio_tdmouta, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 6);
CLOCK_GATE(audio_tdmoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 7);
CLOCK_GATE(audio_tdmoutc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 8);
CLOCK_GATE(audio_frddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 9);
CLOCK_GATE(audio_frddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 10);
CLOCK_GATE(audio_frddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 11);
CLOCK_GATE(audio_toddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 12);
CLOCK_GATE(audio_toddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 13);
CLOCK_GATE(audio_toddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 14);
CLOCK_GATE(audio_loopback, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 15);
CLOCK_GATE(audio_spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 16);
CLOCK_GATE(audio_spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 17);
CLOCK_GATE(audio_resample, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 18);
CLOCK_GATE(audio_power_detect, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 19);
CLOCK_GATE(audio_toram, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 20);
CLOCK_GATE(audio_spdifoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 21);
CLOCK_GATE(audio_eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN), 22);

static struct clk_gate *g12a_audio_clk_gates[] = {
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
	&audio_toram,
	&audio_spdifoutb,
	&audio_eqdrc,
};

/* Array of all clocks provided by this provider */
static struct clk_hw *g12a_audio_clk_hws[] = {
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
	[CLKID_AUDIO_TORAM] = &audio_toram.hw,
	[CLKID_AUDIO_SPDIFOUTB] = &audio_spdifoutb.hw,
	[CLKID_AUDIO_EQDRC] = &audio_eqdrc.hw,
};

static int g12a_clk_gates_init(struct clk **clks, void __iomem *iobase)
{
	int clkid;

	if (ARRAY_SIZE(g12a_audio_clk_gates) != MCLK_BASE) {
		pr_err("check clk gates number\n");
		return -EINVAL;
	}

	for (clkid = 0; clkid < MCLK_BASE; clkid++) {
		g12a_audio_clk_gates[clkid]->reg = iobase;
		clks[clkid] = clk_register(NULL, g12a_audio_clk_hws[clkid]);
		WARN_ON(IS_ERR_OR_NULL(clks[clkid]));
	}

	return 0;
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
/* spdifout b*/
CLOCK_COM_MUX(spdifout_b,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifout_b, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 0, 10);
CLOCK_COM_GATE(spdifout_b, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 31);
/* audio locker_out */
CLOCK_COM_MUX(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 24);
CLOCK_COM_DIV(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 16, 8);
CLOCK_COM_GATE(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 31);
/* audio locker_in */
CLOCK_COM_MUX(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 8);
CLOCK_COM_DIV(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0, 8);
CLOCK_COM_GATE(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 15);
/* audio resample */
CLOCK_COM_MUX(resample, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLE_CTRL), 0xf, 24);
CLOCK_COM_DIV(resample, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLE_CTRL), 0, 8);
CLOCK_COM_GATE(resample, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLE_CTRL), 31);

static int g12a_clks_init(struct clk **clks, void __iomem *iobase)
{
	IOMAP_COM_CLK(mclk_a, iobase);
	clks[CLKID_AUDIO_MCLK_A] = REGISTER_CLK_COM(mclk_a);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_A]));

	IOMAP_COM_CLK(mclk_b, iobase);
	clks[CLKID_AUDIO_MCLK_B] = REGISTER_CLK_COM(mclk_b);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_B]));

	IOMAP_COM_CLK(mclk_c, iobase);
	clks[CLKID_AUDIO_MCLK_C] = REGISTER_CLK_COM(mclk_c);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_C]));

	IOMAP_COM_CLK(mclk_d, iobase);
	clks[CLKID_AUDIO_MCLK_D] = REGISTER_CLK_COM(mclk_d);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_D]));

	IOMAP_COM_CLK(mclk_e, iobase);
	clks[CLKID_AUDIO_MCLK_E] = REGISTER_CLK_COM(mclk_e);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_E]));

	IOMAP_COM_CLK(mclk_f, iobase);
	clks[CLKID_AUDIO_MCLK_F] = REGISTER_CLK_COM(mclk_f);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_F]));

	IOMAP_COM_CLK(spdifin, iobase);
	clks[CLKID_AUDIO_SPDIFIN_CTRL] = REGISTER_CLK_COM(spdifin);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFIN_CTRL]));

	IOMAP_COM_CLK(spdifout, iobase);
	clks[CLKID_AUDIO_SPDIFOUT_CTRL] = REGISTER_CLK_COM(spdifout);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFOUT_CTRL]));

	IOMAP_COM_CLK(pdmin0, iobase);
	clks[CLKID_AUDIO_PDMIN0] = REGISTER_CLK_COM(pdmin0);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_PDMIN0]));

	IOMAP_COM_CLK(pdmin1, iobase);
	clks[CLKID_AUDIO_PDMIN1] = REGISTER_CLK_COM(pdmin1);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_PDMIN1]));

	IOMAP_COM_CLK(spdifout_b, iobase);
	clks[CLKID_AUDIO_SPDIFOUTB_CTRL] = REGISTER_CLK_COM(spdifout_b);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFOUTB_CTRL]));

	IOMAP_COM_CLK(locker_out, iobase);
	clks[CLKID_AUDIO_LOCKER_OUT] = REGISTER_AUDIOCLK_COM(locker_out);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_OUT]));

	IOMAP_COM_CLK(locker_in, iobase);
	clks[CLKID_AUDIO_LOCKER_IN] = REGISTER_AUDIOCLK_COM(locker_in);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_IN]));

	IOMAP_COM_CLK(resample, iobase);
	clks[CLKID_AUDIO_RESAMPLE_CTRL] = REGISTER_AUDIOCLK_COM(resample);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_RESAMPLE_CTRL]));

	return 0;
}

struct audio_clk_init g12a_audio_clks_init = {
	.clk_num   = NUM_AUDIO_CLKS,
	.clk_gates = g12a_clk_gates_init,
	.clks      = g12a_clks_init,
};
