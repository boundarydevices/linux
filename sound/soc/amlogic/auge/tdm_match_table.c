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

struct tdm_chipinfo {
	/* device id */
	unsigned int id;

	/* lane max count */
	unsigned int lane_cnt;

	/* no eco, sclk_ws_inv for out */
	bool sclk_ws_inv;

	/* output en (oe) for pinmux */
	bool oe_fn;

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
	.oe_fn       = true,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo g12a_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
};

struct tdm_chipinfo tl1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.adc_fn      = true,
};

struct tdm_chipinfo tl1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.adc_fn      = true,
};

struct tdm_chipinfo tl1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.adc_fn      = true,
};

struct tdm_chipinfo sm1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX0,
};

struct tdm_chipinfo sm1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
};

struct tdm_chipinfo sm1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX1,
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
		.compatible = "amlogic, sm1-snd-tdma",
		.data		= &sm1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmb",
		.data		= &sm1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmc",
		.data		= &sm1_tdmc_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);
