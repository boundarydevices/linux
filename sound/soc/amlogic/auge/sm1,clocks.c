/*
 * sound/soc/amlogic/auge/sm1,clocks.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#define pr_fmt(fmt) "sm1_audio_clocks: " fmt

#include <dt-bindings/clock/amlogic,sm1-audio-clk.h>

#include "audio_clks.h"
#include "regs.h"

static spinlock_t aclk_lock;

static const char *const mclk_parent_names[] = {
	"mpll0", "mpll1", "mpll2", "mpll3", "hifi_pll",
	"fclk_div3", "fclk_div4", "fclk_div5"};

static const char *const audioclk_parent_names[] = {
	"mclk_a", "mclk_b", "mclk_c", "mclk_d", "mclk_e",
	"mclk_f", "i_slv_sclk_a", "i_slv_sclk_b", "i_slv_sclk_c",
	"i_slv_sclk_d", "i_slv_sclk_e", "i_slv_sclk_f", "i_slv_sclk_g",
	"i_slv_sclk_h", "i_slv_sclk_i", "i_slv_sclk_j"};

CLOCK_GATE(audio_ddr_arb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 0);
CLOCK_GATE(audio_pdm, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 1);
CLOCK_GATE(audio_tdmina, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 2);
CLOCK_GATE(audio_tdminb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 3);
CLOCK_GATE(audio_tdminc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 4);
CLOCK_GATE(audio_tdminlb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 5);
CLOCK_GATE(audio_tdmouta, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 6);
CLOCK_GATE(audio_tdmoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 7);
CLOCK_GATE(audio_tdmoutc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 8);
CLOCK_GATE(audio_frddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 9);
CLOCK_GATE(audio_frddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 10);
CLOCK_GATE(audio_frddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 11);
CLOCK_GATE(audio_toddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 12);
CLOCK_GATE(audio_toddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 13);
CLOCK_GATE(audio_toddrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 14);
CLOCK_GATE(audio_loopbacka, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 15);
CLOCK_GATE(audio_spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 16);
CLOCK_GATE(audio_spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 17);
CLOCK_GATE(audio_resamplea, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 18);
CLOCK_GATE(audio_reserved0, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 19);
CLOCK_GATE(audio_toram, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 20);
CLOCK_GATE(audio_spdifoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 21);
CLOCK_GATE(audio_eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 22);
CLOCK_GATE(audio_reserved1, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 23);
CLOCK_GATE(audio_reserved2, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 24);
CLOCK_GATE(audio_reserved3, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 25);
CLOCK_GATE(audio_resampleb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 26);
CLOCK_GATE(audio_tovad, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 27);
CLOCK_GATE(audio_audiolocker, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 28);
CLOCK_GATE(audio_spdifin_lb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 29);
CLOCK_GATE(audio_reserved4, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 30);
CLOCK_GATE(audio_reserved5, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 31);

CLOCK_GATE(audio_frddrd, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN1), 0);
CLOCK_GATE(audio_toddrd, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN1), 1);
CLOCK_GATE(audio_loopbackb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN1), 2);
CLOCK_GATE(audio_earc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN1), 6);

static struct clk_gate *sm1_audio_clk_gates[] = {
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
	&audio_loopbacka,
	&audio_spdifin,
	&audio_spdifout,
	&audio_resamplea,
	&audio_reserved0,
	&audio_toram,
	&audio_spdifoutb,
	&audio_eqdrc,
	&audio_reserved1,
	&audio_reserved2,
	&audio_reserved3,
	&audio_resampleb,
	&audio_tovad,
	&audio_audiolocker,
	&audio_spdifin_lb,
	&audio_reserved4,
	&audio_reserved5,

	&audio_frddrd,
	&audio_toddrd,
	&audio_loopbackb,
	&audio_earc,
};

/* Array of all clocks provided by this provider */
static struct clk_hw *sm1_audio_clk_hws[] = {
	[CLKID_AUDIO_GATE_DDR_ARB]     = &audio_ddr_arb.hw,
	[CLKID_AUDIO_GATE_PDM]         = &audio_pdm.hw,
	[CLKID_AUDIO_GATE_TDMINA]      = &audio_tdmina.hw,
	[CLKID_AUDIO_GATE_TDMINB]      = &audio_tdminb.hw,
	[CLKID_AUDIO_GATE_TDMINC]      = &audio_tdminc.hw,
	[CLKID_AUDIO_GATE_TDMINLB]     = &audio_tdminlb.hw,
	[CLKID_AUDIO_GATE_TDMOUTA]     = &audio_tdmouta.hw,
	[CLKID_AUDIO_GATE_TDMOUTB]     = &audio_tdmoutb.hw,
	[CLKID_AUDIO_GATE_TDMOUTC]     = &audio_tdmoutc.hw,
	[CLKID_AUDIO_GATE_FRDDRA]      = &audio_frddra.hw,
	[CLKID_AUDIO_GATE_FRDDRB]      = &audio_frddrb.hw,
	[CLKID_AUDIO_GATE_FRDDRC]      = &audio_frddrc.hw,
	[CLKID_AUDIO_GATE_TODDRA]      = &audio_toddra.hw,
	[CLKID_AUDIO_GATE_TODDRB]      = &audio_toddrb.hw,
	[CLKID_AUDIO_GATE_TODDRC]      = &audio_toddrc.hw,
	[CLKID_AUDIO_GATE_LOOPBACKA]   = &audio_loopbacka.hw,
	[CLKID_AUDIO_GATE_SPDIFIN]     = &audio_spdifin.hw,
	[CLKID_AUDIO_GATE_SPDIFOUT_A]  = &audio_spdifout.hw,
	[CLKID_AUDIO_GATE_RESAMPLEA]   = &audio_resamplea.hw,
	[CLKID_AUDIO_GATE_RESERVED0]   = &audio_reserved0.hw,
	[CLKID_AUDIO_GATE_TORAM]       = &audio_toram.hw,
	[CLKID_AUDIO_GATE_SPDIFOUT_B]  = &audio_spdifoutb.hw,
	[CLKID_AUDIO_GATE_EQDRC]       = &audio_eqdrc.hw,
	[CLKID_AUDIO_GATE_RESERVED1]   = &audio_reserved1.hw,
	[CLKID_AUDIO_GATE_RESERVED2]   = &audio_reserved2.hw,
	[CLKID_AUDIO_GATE_RESERVED3]   = &audio_reserved3.hw,
	[CLKID_AUDIO_GATE_RESAMPLEB]   = &audio_resampleb.hw,
	[CLKID_AUDIO_GATE_TOVAD]       = &audio_tovad.hw,
	[CLKID_AUDIO_GATE_AUDIOLOCKER] = &audio_audiolocker.hw,
	[CLKID_AUDIO_GATE_SPDIFIN_LB]  = &audio_spdifin_lb.hw,
	[CLKID_AUDIO_GATE_RESERVED4]   = &audio_reserved4.hw,
	[CLKID_AUDIO_GATE_RESERVED5]   = &audio_reserved5.hw,

	[CLKID_AUDIO_GATE_FRDDRD]      = &audio_frddrd.hw,
	[CLKID_AUDIO_GATE_TODDRD]      = &audio_toddrd.hw,
	[CLKID_AUDIO_GATE_LOOPBACKB]   = &audio_loopbackb.hw,
	[CLKID_AUDIO_GATE_EARCRX]      = &audio_earc.hw,
};

static int sm1_clk_gates_init(struct clk **clks, void __iomem *iobase)
{
	int clkid;

	if (ARRAY_SIZE(sm1_audio_clk_gates) != MCLK_BASE) {
		pr_err("check clk gates number\n");
		return -EINVAL;
	}

	for (clkid = 0; clkid < MCLK_BASE; clkid++) {
		sm1_audio_clk_gates[clkid]->reg = iobase;
		clks[clkid] = clk_register(NULL, sm1_audio_clk_hws[clkid]);
		WARN_ON(IS_ERR_OR_NULL(clks[clkid]));
	}

	return 0;
}

/* mclk_a */
CLOCK_COM_MUX(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 31);
/* mclk_b */
CLOCK_COM_MUX(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 31);
/* mclk_c */
CLOCK_COM_MUX(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 31);
/* mclk_d */
CLOCK_COM_MUX(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 31);
/* mclk_e */
CLOCK_COM_MUX(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_e, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_E_CTRL(1)), 31);
/* mclk_f */
CLOCK_COM_MUX(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_f, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_F_CTRL(1)), 31);
/* spdifin */
CLOCK_COM_MUX(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0, 8);
CLOCK_COM_GATE(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 31);
/* spdifout */
CLOCK_COM_MUX(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 0, 10);
CLOCK_COM_GATE(spdifout, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_CTRL), 31);
/* audio resample_a */
CLOCK_COM_MUX(resample_a,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 0xf, 24);
CLOCK_COM_DIV(resample_a, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 0, 8);
CLOCK_COM_GATE(resample_a, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 31);
/* audio locker_out */
CLOCK_COM_MUX(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 24);
CLOCK_COM_DIV(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 16, 8);
CLOCK_COM_GATE(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 31);
/* audio locker_in */
CLOCK_COM_MUX(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 8);
CLOCK_COM_DIV(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0, 8);
CLOCK_COM_GATE(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 15);
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
/* audio resample_b */
CLOCK_COM_MUX(resample_b,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEB_CTRL), 0xf, 24);
CLOCK_COM_DIV(resample_b, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEB_CTRL), 0, 8);
CLOCK_COM_GATE(resample_b, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEB_CTRL), 31);
/* spdifin_lb, div is a fake */
CLOCK_COM_MUX(spdifin_lb,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 0x1, 30);
CLOCK_COM_DIV(spdifin_lb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 0, 29);
CLOCK_COM_GATE(spdifin_lb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFOUT_B_CTRL), 31);
/* audio eqdrc  */
CLOCK_COM_MUX(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 0x7, 24);
CLOCK_COM_DIV(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 0, 16);
CLOCK_COM_GATE(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 31);
/* audio vad  */
CLOCK_COM_MUX(vad, AUD_ADDR_OFFSET(EE_AUDIO_VAD_CLK_CTRL), 0x7, 24);
CLOCK_COM_DIV(vad, AUD_ADDR_OFFSET(EE_AUDIO_VAD_CLK_CTRL), 0, 16);
CLOCK_COM_GATE(vad, AUD_ADDR_OFFSET(EE_AUDIO_VAD_CLK_CTRL), 31);
/* EARC TX CMDC */
CLOCK_COM_MUX(earctx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_CMDC_CLK_CTRL), 0x7, 24);
CLOCK_COM_DIV(earctx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_CMDC_CLK_CTRL), 0, 16);
CLOCK_COM_GATE(earctx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_CMDC_CLK_CTRL), 31);
/* EARC TX DMAC */
CLOCK_COM_MUX(earctx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_DMAC_CLK_CTRL), 0x7, 24);
CLOCK_COM_DIV(earctx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_DMAC_CLK_CTRL), 0, 16);
CLOCK_COM_GATE(earctx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCTX_DMAC_CLK_CTRL), 31);
/* EARC RX CMDC */
CLOCK_COM_MUX(earcrx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_CMDC_CLK_CTRL), 0x7, 24);
CLOCK_COM_DIV(earcrx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_CMDC_CLK_CTRL), 0, 16);
CLOCK_COM_GATE(earcrx_cmdc,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_CMDC_CLK_CTRL), 31);
/* EARC RX DMAC */
CLOCK_COM_MUX(earcrx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_DMAC_CLK_CTRL), 0x7, 24);
CLOCK_COM_DIV(earcrx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_DMAC_CLK_CTRL), 0, 16);
CLOCK_COM_GATE(earcrx_dmac,
	AUD_ADDR_OFFSET(EE_AUDIO_EARCRX_DMAC_CLK_CTRL), 31);

static int sm1_clks_init(struct clk **clks, void __iomem *iobase)
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
	clks[CLKID_AUDIO_SPDIFIN] = REGISTER_CLK_COM(spdifin);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFIN]));

	IOMAP_COM_CLK(spdifout, iobase);
	clks[CLKID_AUDIO_SPDIFOUT_A] = REGISTER_CLK_COM(spdifout);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFOUT_A]));

	IOMAP_COM_CLK(resample_a, iobase);
	clks[CLKID_AUDIO_RESAMPLE_A] = REGISTER_AUDIOCLK_COM(resample_a);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_RESAMPLE_A]));

	IOMAP_COM_CLK(locker_out, iobase);
	clks[CLKID_AUDIO_LOCKER_OUT] = REGISTER_AUDIOCLK_COM(locker_out);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_OUT]));

	IOMAP_COM_CLK(locker_in, iobase);
	clks[CLKID_AUDIO_LOCKER_IN] = REGISTER_AUDIOCLK_COM(locker_in);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_IN]));

	IOMAP_COM_CLK(pdmin0, iobase);
	clks[CLKID_AUDIO_PDMIN0] = REGISTER_CLK_COM(pdmin0);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_PDMIN0]));

	IOMAP_COM_CLK(pdmin1, iobase);
	clks[CLKID_AUDIO_PDMIN1] = REGISTER_CLK_COM(pdmin1);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_PDMIN1]));

	IOMAP_COM_CLK(spdifout_b, iobase);
	clks[CLKID_AUDIO_SPDIFOUT_B] = REGISTER_CLK_COM(spdifout_b);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFOUT_B]));

	IOMAP_COM_CLK(resample_b, iobase);
	clks[CLKID_AUDIO_RESAMPLE_B] = REGISTER_AUDIOCLK_COM(resample_b);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_RESAMPLE_B]));

	IOMAP_COM_CLK(spdifin_lb, iobase);
	clks[CLKID_AUDIO_SPDIFIN_LB] = REGISTER_CLK_COM(spdifin_lb);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFIN_LB]));

	IOMAP_COM_CLK(eqdrc, iobase);
	clks[CLKID_AUDIO_EQDRC] = REGISTER_CLK_COM(eqdrc);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_EQDRC]));

	IOMAP_COM_CLK(vad, iobase);
	clks[CLKID_AUDIO_VAD] = REGISTER_CLK_COM(vad);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_VAD]));

	IOMAP_COM_CLK(earctx_cmdc, iobase);
	clks[CLKID_EARCTX_CMDC] = REGISTER_CLK_COM(earctx_cmdc);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_EARCTX_CMDC]));

	IOMAP_COM_CLK(earctx_dmac, iobase);
	clks[CLKID_EARCTX_DMAC] = REGISTER_CLK_COM(earctx_dmac);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_EARCTX_DMAC]));

	IOMAP_COM_CLK(earcrx_cmdc, iobase);
	clks[CLKID_EARCRX_CMDC] = REGISTER_CLK_COM(earcrx_cmdc);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_EARCRX_CMDC]));

	IOMAP_COM_CLK(earcrx_dmac, iobase);
	clks[CLKID_EARCRX_DMAC] = REGISTER_CLK_COM(earcrx_dmac);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_EARCRX_DMAC]));

	return 0;
}

struct audio_clk_init sm1_audio_clks_init = {
	.clk_num   = NUM_AUDIO_CLKS,
	.clk_gates = sm1_clk_gates_init,
	.clks      = sm1_clks_init,
};
