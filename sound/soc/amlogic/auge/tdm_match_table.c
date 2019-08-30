/*
 * sound/soc/amlogic/auge/tdm_match_table.c
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

/* For OE function V1:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL0 & EE_AUDIO_TDMOUT_A_CTRL1
 */
#define OE_FUNCTION_V1 1
/* For OE function V2:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL2
 */
#define OE_FUNCTION_V2 2

struct tdm_chipinfo {
	/* device id */
	unsigned int id;

	/* lane max count */
	unsigned int lane_cnt;

	/* no eco, sclk_ws_inv for out */
	bool sclk_ws_inv;

	/* output en (oe) for pinmux */
	unsigned int oe_fn;

	/* clk pad */
	bool no_mclkpad_ctrl;

	/* same source */
	bool same_src_fn;

	/* same source, spdif re-enable */
	bool same_src_spdif_reen;

	/* ACODEC_ADC function */
	bool adc_fn;

	/* mclk pad offset */
	bool mclkpad_no_offset;

	/* offset config for SW_RESET in reg.h */
	int reset_reg_offset;

	/* async fifo */
	bool async_fifo;
};


struct tdm_chipinfo axg_tdma_chipinfo = {
	.id          = TDM_A,
	.no_mclkpad_ctrl = true,
};

struct tdm_chipinfo axg_tdmb_chipinfo = {
	.id          = TDM_B,
	.no_mclkpad_ctrl = true,
};

struct tdm_chipinfo axg_tdmc_chipinfo = {
	.id          = TDM_C,
	.no_mclkpad_ctrl = true,
};

struct tdm_chipinfo axg_tdminlb_chipinfo = {
	.id          = TDM_LB,
	.no_mclkpad_ctrl = true,
};

struct tdm_chipinfo g12a_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo g12a_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo g12a_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo g12a_tdminlb_chipinfo = {
	.id          = TDM_LB,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo tl1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tl1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tl1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tl1_tdminlb_chipinfo = {
	.id          = TDM_LB,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX0,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdminlb_chipinfo = {
	.id          = TDM_LB,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdminlb_chipinfo = {
	.id          = TDM_LB,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.async_fifo  = true,
};

static const struct of_device_id aml_tdm_device_id[] = {
	{
		.compatible = "amlogic, axg-snd-tdma",
		.data       = &axg_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmb",
		.data       = &axg_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmc",
		.data       = &axg_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmlb",
		.data       = &axg_tdminlb_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdma",
		.data       = &g12a_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmb",
		.data       = &g12a_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmc",
		.data       = &g12a_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmlb",
		.data       = &g12a_tdminlb_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdma",
		.data       = &tl1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmb",
		.data       = &tl1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmc",
		.data       = &tl1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmlb",
		.data       = &tl1_tdminlb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdma",
		.data       = &sm1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmb",
		.data       = &sm1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmc",
		.data       = &sm1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmlb",
		.data       = &sm1_tdminlb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdma",
		.data       = &tm2_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmb",
		.data       = &tm2_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmc",
		.data       = &tm2_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmlb",
		.data       = &tm2_tdminlb_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);
