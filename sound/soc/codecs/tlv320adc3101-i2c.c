// SPDX-License-Identifier: GPL-2.0
/*
 * linux/sound/soc/codecs/tlv320adc3101-i2c.c
 *
 * Copyright 2019 Baylibre
 *
 * Author: Nicolas Belin <nbelin@baylibre.com>
 *
 * Based on sound/soc/codecs/tlv320aic332x4.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "tlv320adc3101.h"

static int adc3101_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config;

	config = adc3101_regmap_config;
	config.reg_bits = 8;
	config.val_bits = 8;

	regmap = devm_regmap_init_i2c(i2c, &config);
	return adc3101_probe(&i2c->dev, regmap);
}

static int adc3101_i2c_remove(struct i2c_client *i2c)
{
	return adc3101_remove(&i2c->dev);
}

static const struct i2c_device_id adc3101_i2c_id[] = {
	{ "tlv320adc3101", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, adc3101_i2c_id);

static const struct of_device_id adc3101_of_id[] = {
	{ .compatible = "ti,tlv320adc3101", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, adc3101_of_id);

static struct i2c_driver adc3101_i2c_driver = {
	.driver = {
		.name = "tlv320adc3101",
		.of_match_table = adc3101_of_id,
	},
	.probe =    adc3101_i2c_probe,
	.remove =   adc3101_i2c_remove,
	.id_table = adc3101_i2c_id,
};

module_i2c_driver(adc3101_i2c_driver);

MODULE_DESCRIPTION("ASoC TLV320ADC3101 codec driver I2C");
MODULE_AUTHOR("Jeremy McDermond <nh6z@nh6z.net>");
MODULE_LICENSE("GPL");
