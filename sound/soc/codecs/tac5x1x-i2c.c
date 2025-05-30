// SPDX-License-Identifier: GPL-2.0
//
// ALSA SoC Texas Instruments TAC5x1x Audio Codec
//
// Copyright (C) 2020 - 2025 Texas Instruments Incorporated
// https://www.ti.com
//
// TAC5x1x are a series of low-power and high performance mono or stereo
// audio codecs, as well as multiple inputs and outputs programmable in
// single-ended or fully differential configurations. Device supports both
// Microphone and Line In input on ADC Channel. DAC Output can be configured
// for either Line Out or Head Phone Load.
//
// Author: Kevin Lu <kevin-lu@ti.com>
// Author: Kokila Karuppusamy <kokila.karuppusamy@ti.com>
// Author: Niranjan H Y <niranjan.hy@ti.com>
//

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/module.h>

#include "tac5x1x.h"

static const struct i2c_device_id tac5x1x_id[] = {
	{ "taa5212", TAA5212 },
	{ "taa5412", TAA5412 },
	{ "tac5111", TAC5111 },
	{ "tac5112", TAC5112 },
	{ "tac5211", TAC5211 },
	{ "tac5212", TAC5212 },
	{ "tac5311", TAC5311 },
	{ "tac5312", TAC5312 },
	{ "tac5411", TAC5411 },
	{ "tac5412", TAC5412 },
	{ "tad5112", TAD5112 },
	{ "tad5212", TAD5212 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tac5x1x_id);

static int tac5x1x_i2c_probe(struct i2c_client *i2c)
{
	int ret;
	enum tac5x1x_type type;
	struct regmap *regmap;
	const struct regmap_config *config = &tac5x1x_regmap;

	regmap = devm_regmap_init_i2c(i2c, config);
	type = (uintptr_t)i2c_get_match_data(i2c);

	dev_info(&i2c->dev, "probing %s codec_type = %d\n",
		 i2c->name, type);

	ret = tac5x1x_probe(&i2c->dev, regmap, type);
	if (ret)
		dev_err(&i2c->dev, "probe failed");

	return ret;
}

static void tac5x1x_i2c_remove(struct i2c_client *client)
{
	tac5x1x_remove(&client->dev);
}

static struct i2c_driver tac5x1x_i2c_driver = {
	.driver = {
		.name		= "tac5x1x-codec",
		.of_match_table	= of_match_ptr(tac5x1x_of_match),
	},
	.probe		= tac5x1x_i2c_probe,
	.remove		= tac5x1x_i2c_remove,
	.id_table	= tac5x1x_id,
};

module_i2c_driver(tac5x1x_i2c_driver);

MODULE_DESCRIPTION("ASoC tac5x1x codec driver");
MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
