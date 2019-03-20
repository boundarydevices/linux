/*
 * sound/soc/amlogic/auge/pdm_match_table.c
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

static struct pdm_chipinfo g12a_pdm_chipinfo = {
	.mute_fn         = true,
	.truncate_data   = false,
};

static struct pdm_chipinfo tl1_pdm_chipinfo = {
	.mute_fn         = true,
	.truncate_data   = false,
};

static struct pdm_chipinfo sm1_pdm_chipinfo = {
	.mute_fn         = true,
	.truncate_data   = false,
	.train           = true,
};

static const struct of_device_id aml_pdm_device_id[] = {
	{
		.compatible = "amlogic, axg-snd-pdm",
	},
	{
		.compatible = "amlogic, g12a-snd-pdm",
		.data       = &g12a_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-pdm",
		.data       = &tl1_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-pdm",
		.data		= &sm1_pdm_chipinfo,
	},

	{}
};
MODULE_DEVICE_TABLE(of, aml_pdm_device_id);
