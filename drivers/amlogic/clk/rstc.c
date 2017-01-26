/*
 * drivers/amlogic/clk/rstc.c
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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

/*
 * Modules and submodules within the chip can be reset by disabling the
 * clock and enabling it again.
 * The modules in the AO (Always-On) domain are controlled by a different
 * register mapped in a different memory region accessed by the ao_base.
 *
 */
#define  BITS_PER_REG	32

struct meson_rstc {
	struct reset_controller_dev	rcdev;
	void __iomem *membase;
	spinlock_t			lock;
};

static int meson_rstc_assert(struct reset_controller_dev *rcdev,
			     unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_REG;
	int offset = id % BITS_PER_REG;
	void __iomem *rstc_mem;
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&rstc->lock, flags);

	rstc_mem = rstc->membase + (bank << 2);
	reg = readl(rstc_mem);
	writel(reg & ~BIT(offset), rstc_mem);
	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;
}

static int meson_rstc_deassert(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct meson_rstc *rstc = container_of(rcdev,
					       struct meson_rstc,
					       rcdev);
	int bank = id / BITS_PER_REG;
	int offset = id % BITS_PER_REG;
	void __iomem *rstc_mem;
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&rstc->lock, flags);

	rstc_mem = rstc->membase + (bank << 2);
	reg = readl(rstc_mem);
	writel(reg | BIT(offset), rstc_mem);
	spin_unlock_irqrestore(&rstc->lock, flags);

	return 0;

}

static int meson_rstc_reset(struct reset_controller_dev *rcdev,
							unsigned long id)
{
	int err;

	err = meson_rstc_assert(rcdev, id);
	if (err)
		return err;

	return meson_rstc_deassert(rcdev, id);
}

static struct reset_control_ops meson_rstc_ops = {
	.assert		= meson_rstc_assert,
	.deassert	= meson_rstc_deassert,
	.reset		= meson_rstc_reset,
};

static int meson_reset_probe(struct platform_device *pdev)
{
	struct meson_rstc *rstc;
	struct resource *res;
	int ret;

	rstc = devm_kzalloc(&pdev->dev, sizeof(*rstc), GFP_KERNEL);
	if (!rstc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rstc->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rstc->membase))
		return PTR_ERR(rstc->membase);

	spin_lock_init(&rstc->lock);

	rstc->rcdev.owner = THIS_MODULE;
	rstc->rcdev.nr_resets = resource_size(res) * BITS_PER_REG;
	rstc->rcdev.of_node = pdev->dev.of_node;
	rstc->rcdev.ops = &meson_rstc_ops;

	ret = reset_controller_register(&rstc->rcdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not register reset controller: %d\n",
		       __func__, ret);
	}

	dev_info(&pdev->dev, "%s: register reset controller ok,ret: %d\n",
		       __func__, ret);
	return ret;
}
static const struct of_device_id meson_reset_dt_ids[] = {
	 { .compatible = "amlogic,reset", },
	 { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, meson_reset_dt_ids);

static struct platform_driver meson_reset_driver = {
	.probe	= meson_reset_probe,
	.driver = {
		.name		= "meson_reset",
		.of_match_table	= meson_reset_dt_ids,
	},
};

module_platform_driver(meson_reset_driver);

MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_DESCRIPTION("Amlogic Meson Reset Controller driver");
MODULE_LICENSE("GPL");
