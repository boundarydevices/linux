/*
 * pcm186x.c - Texas Instruments PCM186x Universal Audio ADC - SPI
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
#include <linux/spi/spi.h>

#include "pcm186x.h"

static int pcm186x_spi_probe(struct spi_device *spi)
{
	const enum pcm186x_type type =
			 (enum pcm186x_type)spi_get_device_id(spi)->driver_data;
	int irq = spi->irq;
	struct regmap *regmap;

	dev_info(&spi->dev, "%s()\n", __func__);

	regmap = devm_regmap_init_spi(spi, &pcm186x_regmap);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return pcm186x_probe(&spi->dev, type, irq, regmap);
}

static int pcm186x_spi_remove(struct spi_device *spi)
{
	pcm186x_remove(&spi->dev);
	return 0;
}

static const struct spi_device_id pcm186x_spi_id[] = {
	{ " pcm1862", PCM1862 },
	{ " pcm1863", PCM1863 },
	{ " pcm1864", PCM1864 },
	{ " pcm1865", PCM1865 },
	{ }
};
MODULE_DEVICE_TABLE(spi, pcm186x_spi_id);

static const struct of_device_id pcm186x_of_match[] = {
	{ .compatible = "ti, pcm1862", },
	{ .compatible = "ti, pcm1863", },
	{ .compatible = "ti, pcm1864", },
	{ .compatible = "ti, pcm1865", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm186x_of_match);

static struct spi_driver pcm186x_spi_driver = {
	.probe		= pcm186x_spi_probe,
	.remove		= pcm186x_spi_remove,
	.id_table	= pcm186x_spi_id,
	.driver		= {
		.name	= "pcm186x",
		.of_match_table = pcm186x_of_match,
		.pm     = &pcm186x_pm_ops,
	},
};

module_spi_driver(pcm186x_spi_driver);

MODULE_DESCRIPTION("PCM186x Universal Audio ADC driver - SPI");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_LICENSE("GPL");
