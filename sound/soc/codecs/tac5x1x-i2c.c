// SPDX-License-Identifier: GPL-2.0
//
// ALSA SoC Texas Instruments TAC5x1x Audio Codec
//
// Copyright (C) 2024 - 2025 Texas Instruments Incorporated
// https://www.ti.com
//
// TAC5x1x are a series of low-power and high performance mono or stereo
// audio codecs, as well as multiple inputs and outputs programmable in
// single-ended or fully differential configurations. Device supports both
// Microphone and Line In input on ADC Channel. DAC Output can be configured
// for either Line Out or Head Phone Load. 
//
// Author: Kevin Lu <kevin-lu@ti.com>
//

#include <linux/version.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "tac5x1x.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
static int tac5x1x_i2c_probe(struct i2c_client *i2c)
{
	struct regmap *regmap;
	struct regmap_config *config = &tac5x1x_regmap;
	const struct i2c_device_id *id;

	regmap = devm_regmap_init_i2c(i2c, config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	id = i2c_match_id(tac5x1x_id, i2c);
	dev_set_drvdata(&i2c->dev, (void *)id->driver_data);

	dev_info(&i2c->dev, "## %s: %s codec_type = %ld\n", __func__,
		i2c->name, (kernel_ulong_t)dev_get_drvdata(&i2c->dev));

	return tac5x1x_probe(&i2c->dev, regmap);
}
#else
static int tac5x1x_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config *config = &tac5x1x_regmap;

	regmap = devm_regmap_init_i2c(i2c, config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	dev_set_drvdata(&i2c->dev, (void *)id->driver_data);

	dev_info(&i2c->dev, "## %s: %s codec_type = %ld\n", __func__,
		i2c->name, (kernel_ulong_t)dev_get_drvdata(&i2c->dev));

	return tac5x1x_probe(&i2c->dev, regmap);
}
#endif

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
MODULE_AUTHOR("Kevin Lu <kevin-lu@ti.com>");
MODULE_LICENSE("GPL v2");
