/*
 * drivers/amlogic/crypto/aml-dma.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <linux/of_platform.h>
#include "aml-crypto-dma.h"

int debug = 2;
#ifdef CRYPTO_DEBUG
module_param(debug, int, 0644);
#endif

void __iomem *cryptoreg;

struct meson_dma_data {
	uint32_t thread;
	uint32_t status;
};

struct meson_dma_data meson_gxl_data = {
	.thread = GXL_DMA_T0,
	.status = GXL_DMA_STS0,
};

struct meson_dma_data meson_txlx_data = {
	.thread = TXLX_DMA_T0,
	.status = TXLX_DMA_STS0,
};
#ifdef CONFIG_OF
static const struct of_device_id aml_dma_dt_match[] = {
	{	.compatible = "amlogic,aml_gxl_dma",
		.data = &meson_gxl_data,
	},
	{	.compatible = "amlogic,aml_txlx_dma",
		.data = &meson_txlx_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_dma_dt_match);
#else
#define aml_aes_dt_match NULL
#endif

static int aml_dma_probe(struct platform_device *pdev)
{
	struct aml_dma_dev *dma_dd;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource *res_base = 0;
	struct resource *res_irq = 0;
	const struct of_device_id *match;
	int err = -EPERM;
	const struct meson_dma_data *priv_data;

	dma_dd = kzalloc(sizeof(struct aml_dma_dev), GFP_KERNEL);
	if (dma_dd == NULL) {
		err = -ENOMEM;
		goto dma_err;
	}

	match = of_match_device(aml_dma_dt_match, &pdev->dev);
	if (!match)
		goto dma_err;
	priv_data = match->data;
	dma_dd->thread = priv_data->thread;
	dma_dd->status = priv_data->status;
	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(dev, "error to get normal IORESOURCE_MEM.\n");
		goto dma_err;
	} else {
		cryptoreg = ioremap(res_base->start,
				resource_size(res_base));
		if (!cryptoreg) {
			dev_err(dev, "failed to remap crypto reg\n");
			goto dma_err;
		}
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	dma_dd->irq = res_irq->start;
	dma_dd->dma_busy = 0;
	platform_set_drvdata(pdev, dma_dd);
	dev_info(dev, "Aml dma\n");

	err = of_platform_populate(np, NULL, NULL, dev);

	if (err != 0)
		iounmap(cryptoreg);

	return err;

dma_err:
	kfree(dma_dd);
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_dma_remove(struct platform_device *pdev)
{
	struct aml_dma_dev *dma_dd;

	dma_dd = platform_get_drvdata(pdev);
	if (!dma_dd)
		return -ENODEV;

	iounmap(cryptoreg);
	kfree(dma_dd);

	return 0;
}

static struct platform_driver aml_dma_driver = {
	.probe		= aml_dma_probe,
	.remove		= aml_dma_remove,
	.driver		= {
		.name	= "aml_dma",
		.owner	= THIS_MODULE,
		.of_match_table = aml_dma_dt_match,
	},
};

module_platform_driver(aml_dma_driver);

MODULE_DESCRIPTION("Aml crypto DMA support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
