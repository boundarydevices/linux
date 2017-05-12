/*
 * pcm186x.c - Texas Instruments PCM186x Universal Audio ADC - I2C
 *
 * Copyright (C) 2015-2016 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Andreas Dannenberg <dannenberg@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "pcm186x.h"

static int pcm186x_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	const enum pcm186x_type type = (enum pcm186x_type)id->driver_data;
	int irq = i2c->irq;
	struct regmap *regmap;

	dev_info(&i2c->dev, "%s() i2c->addr=%d\n", __func__, i2c->addr);

	regmap = devm_regmap_init_i2c(i2c, &pcm186x_regmap);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return pcm186x_probe(&i2c->dev, type, irq, regmap);
}

static int pcm186x_i2c_remove(struct i2c_client *i2c)
{
	pcm186x_remove(&i2c->dev);
	return 0;
}

static const struct i2c_device_id pcm186x_i2c_id[] = {
	{ " pcm1862", PCM1862 },
	{ " pcm1863", PCM1863 },
	{ " pcm1864", PCM1864 },
	{ " pcm1865", PCM1865 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcm186x_i2c_id);

static const struct of_device_id pcm186x_of_match[] = {
	{ .compatible = "ti, pcm1862", },
	{ .compatible = "ti, pcm1863", },
	{ .compatible = "ti, pcm1864", },
	{ .compatible = "ti, pcm1865", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm186x_of_match);

static struct i2c_driver pcm186x_i2c_driver = {
	.probe		= pcm186x_i2c_probe,
	.remove		= pcm186x_i2c_remove,
	.id_table	= pcm186x_i2c_id,
	.driver		= {
		.name	= "pcm186x",
		.of_match_table = pcm186x_of_match,
		.pm     = &pcm186x_pm_ops,
	},
};

module_i2c_driver(pcm186x_i2c_driver);

MODULE_DESCRIPTION("PCM186x Universal Audio ADC driver - I2C");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_LICENSE("GPL");

