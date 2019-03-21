/*
 * drivers/amlogic/power/power_ctrl.c
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/amlogic/power_ctrl.h>

struct power_ctrl {
	void __iomem *sleep_addr;
	void __iomem *iso_addr;
	void __iomem *mempd0_addr;
	void __iomem *reset_addr_base;
	spinlock_t sleep_lock;
	spinlock_t iso_lock;
	spinlock_t mempd0_lock;
};

struct power_ctrl ctrl;
static bool probe_done;

int power_ctrl_sleep(bool power_on, unsigned int shift)
{
	unsigned int val;
	unsigned long flags;

	if (!probe_done)
		return -ENXIO;

	if (power_on) {
		spin_lock_irqsave(&ctrl.sleep_lock, flags);
		val = readl(ctrl.sleep_addr);
		val = val & (~(1 << shift));
		writel(val, ctrl.sleep_addr);
		spin_unlock_irqrestore(&ctrl.sleep_lock, flags);
	} else {
		spin_lock_irqsave(&ctrl.sleep_lock, flags);
		val = readl(ctrl.sleep_addr);
		val = val | (1 << shift);
		writel(val, ctrl.sleep_addr);
		spin_unlock_irqrestore(&ctrl.sleep_lock, flags);
	}
	return 0;
}
EXPORT_SYMBOL(power_ctrl_sleep);

int power_ctrl_iso(bool power_on, unsigned int shift)
{
	unsigned int val;
	unsigned long flags;

	if (!probe_done)
		return -ENXIO;

	if (power_on) {
		spin_lock_irqsave(&(ctrl.iso_lock), flags);
		val = readl(ctrl.iso_addr);
		val = val & (~(1 << shift));
		writel(val, ctrl.iso_addr);
		spin_unlock_irqrestore(&(ctrl.iso_lock), flags);
	} else {
		spin_lock_irqsave((&ctrl.iso_lock), flags);
		val = readl(ctrl.iso_addr);
		val = val | (1 << shift);
		writel(val, ctrl.iso_addr);
		spin_unlock_irqrestore(&(ctrl.iso_lock), flags);
	}
	return 0;
}
EXPORT_SYMBOL(power_ctrl_iso);

int power_ctrl_mempd0(bool power_on, unsigned int mask_val, unsigned int shift)
{
	unsigned int val;
	unsigned long flags;

	if (!probe_done)
		return -ENXIO;

	if (power_on) {
		spin_lock_irqsave(&(ctrl.mempd0_lock), flags);
		val = readl(ctrl.mempd0_addr);
		val = val & (~(mask_val << shift));
		writel(val, ctrl.mempd0_addr);
		spin_unlock_irqrestore(&(ctrl.mempd0_lock), flags);
	} else {
		spin_lock_irqsave((&ctrl.mempd0_lock), flags);
		val = readl(ctrl.mempd0_addr);
		val = val | (mask_val << shift);
		writel(val, ctrl.mempd0_addr);
		spin_unlock_irqrestore(&(ctrl.mempd0_lock), flags);
	}
	return 0;
}
EXPORT_SYMBOL(power_ctrl_mempd0);

static int amlogic_powerctrl_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Fail to get power ctrl memory res0\n");
		return -ENXIO;
	}

	ctrl.sleep_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl.sleep_addr))
		return PTR_ERR(ctrl.sleep_addr);

	ctrl.iso_addr = ctrl.sleep_addr + 4;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "Fail to get power ctrl memory res1\n");
		return -ENXIO;
	}
	ctrl.mempd0_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl.mempd0_addr))
		return PTR_ERR(ctrl.mempd0_addr);

	spin_lock_init(&ctrl.sleep_lock);
	spin_lock_init(&ctrl.iso_lock);
	spin_lock_init(&ctrl.mempd0_lock);

	probe_done = 1;
	return 0;
}

static int amlogic_powerctrl_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	devm_iounmap(dev, ctrl.sleep_addr);
	devm_iounmap(dev, ctrl.mempd0_addr);
	devm_iounmap(dev, ctrl.reset_addr_base);
	return 0;
}

static const struct of_device_id amlogic_powerctrl_match[] = {
	{ .compatible = "amlogic, sm1-powerctrl", },
	{}
};

static struct platform_driver amlogic_powerctrl_driver = {
	.driver = {
		.name = "amlogic, sm1-powerctrl",
		.of_match_table = amlogic_powerctrl_match,
	},
	.probe = amlogic_powerctrl_probe,
	.remove = amlogic_powerctrl_remove,
};

static int __init amlogic_powerctrl_init(void)
{
	return platform_driver_register(&amlogic_powerctrl_driver);
}
arch_initcall(amlogic_powerctrl_init);

MODULE_AUTHOR("shunzhou jiang<shunzhou.jiang@shunzhou.jiang.com");
MODULE_DESCRIPTION("SM1 power ctrl driver");
MODULE_LICENSE("GPL");
