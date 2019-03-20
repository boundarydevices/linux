/*
 * sound/soc/amlogic/auge/spdif_match_table.c
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

#define SPDIF_A	0
#define SPDIF_B	1

struct spdif_chipinfo {
	unsigned int id;

	/* add ch_cnt to ch_num */
	bool chnum_en;
	/*
	 * axg, clear all irq bits
	 * after axg, such as g12a, clear each bits
	 * Reg_clr_interrupt[7:0] for each bit of irq_status[7:0];
	 */
	bool clr_irq_all_bits;
	/* no PaPb irq */
	bool irq_no_papb;
	/* reg_hold_start_en; 1: add delay to match TDM out when share buff; */
	bool hold_start;
	/* eq/drc */
	bool eq_drc_en;
	/* pc, pd interrupt is separated. */
	bool pcpd_separated;
	/* same source, spdif re-enable */
	bool same_src_spdif_reen;
};

struct spdif_chipinfo axg_spdif_chipinfo = {
	.id               = SPDIF_A,
	.irq_no_papb      = true,
	.clr_irq_all_bits = true,
	.pcpd_separated   = true,
};

struct spdif_chipinfo g12a_spdif_a_chipinfo = {
	.id             = SPDIF_A,
	.chnum_en       = true,
	.hold_start     = true,
	.eq_drc_en      = true,
	.pcpd_separated = true,
};

struct spdif_chipinfo g12a_spdif_b_chipinfo = {
	.id             = SPDIF_B,
	.chnum_en       = true,
	.hold_start     = true,
	.eq_drc_en      = true,
	.pcpd_separated = true,
};

struct spdif_chipinfo tl1_spdif_a_chipinfo = {
	.id           = SPDIF_A,
	.chnum_en     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

struct spdif_chipinfo tl1_spdif_b_chipinfo = {
	.id           = SPDIF_B,
	.chnum_en     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

struct spdif_chipinfo sm1_spdif_a_chipinfo = {
	.id           = SPDIF_A,
	.chnum_en     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

struct spdif_chipinfo sm1_spdif_b_chipinfo = {
	.id           = SPDIF_B,
	.chnum_en     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

static const struct of_device_id aml_spdif_device_id[] = {
	{
		.compatible = "amlogic, axg-snd-spdif",
		.data       = &axg_spdif_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-spdif-a",
		.data       = &g12a_spdif_a_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-spdif-b",
		.data       = &g12a_spdif_b_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-spdif-a",
		.data       = &tl1_spdif_a_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-spdif-b",
		.data       = &tl1_spdif_b_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-spdif-a",
		.data		= &sm1_spdif_a_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-spdif-b",
		.data		= &sm1_spdif_b_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_spdif_device_id);
