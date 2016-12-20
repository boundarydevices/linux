/*
 * drivers/amlogic/usb/phy/phy-aml-usb2.c
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/usb/phy_companion.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxbbtv.h>
#include "phy-aml-usb.h"

static int amlogic_usb2_init(struct usb_phy *x)
{
	int time_dly = 500;
	struct amlogic_usb *phy = phy_to_amlusb(x);
	int i;
	int j;

	struct u2p_aml_regs_t u2p_aml_regs;
	union u2p_r0_t reg0;

	amlogic_usbphy_reset();

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 3; j++) {
			u2p_aml_regs.u2p_r[j] = (void __iomem	*)
				((unsigned long)phy->regs + i*PHY_REGISTER_SIZE
				+ 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
		reg0.b.por = 1;
		reg0.b.dmpulldown = 1;
		reg0.b.dppulldown = 1;
		writel(reg0.d32, u2p_aml_regs.u2p_r[0]);

		udelay(time_dly);

		reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
		reg0.b.por = 0;
		writel(reg0.d32, u2p_aml_regs.u2p_r[0]);
	}

	return 0;
}

static int amlogic_usb2_suspend(struct usb_phy *x, int suspend)
{
#if 0
	u32 ret;
	struct omap_usb *phy = phy_to_omapusb(x);

	if (suspend && !phy->is_suspended) {
		omap_control_usb_phy_power(phy->control_dev, 0);
		pm_runtime_put_sync(phy->dev);
		phy->is_suspended = 1;
	} else if (!suspend && phy->is_suspended) {
		ret = pm_runtime_get_sync(phy->dev);
		if (ret < 0) {
			dev_err(phy->dev, "get_sync failed with err %d\n",
									ret);
			return ret;
		}
		omap_control_usb_phy_power(phy->control_dev, 1);
		phy->is_suspended = 0;
	}
#endif
	return 0;
}

static int amlogic_usb2_probe(struct platform_device *pdev)
{
	struct amlogic_usb			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	int portnum = 0;
	const void *prop;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum) {
		dev_err(&pdev->dev, "This phy has no usb port\n");
		return -ENOMEM;
	}

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);
	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(&pdev->dev, "unable to allocate memory for USB2 PHY\n");
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "USB2 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy2";
	phy->phy.init		= amlogic_usb2_init;
	phy->phy.set_suspend	= amlogic_usb2_suspend;
	phy->phy.type		= USB_PHY_TYPE_USB2;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_usb2_remove(struct platform_device *pdev)
{
#if 0
	struct omap_usb	*phy = platform_get_drvdata(pdev);

	clk_unprepare(phy->wkupclk);
	if (!IS_ERR(phy->optclk))
		clk_unprepare(phy->optclk);
	usb_remove_phy(&phy->phy);
#endif
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_usb2_runtime_suspend(struct device *dev)
{
#if 0
	struct platform_device	*pdev = to_platform_device(dev);
	struct omap_usb	*phy = platform_get_drvdata(pdev);

	clk_disable(phy->wkupclk);
	if (!IS_ERR(phy->optclk))
		clk_disable(phy->optclk);
#endif
	return 0;
}

static int amlogic_usb2_runtime_resume(struct device *dev)
{
	unsigned ret = 0;
#if 0
	struct platform_device	*pdev = to_platform_device(dev);
	struct omap_usb	*phy = platform_get_drvdata(pdev);

	ret = clk_enable(phy->wkupclk);
	if (ret < 0) {
		dev_err(phy->dev, "Failed to enable wkupclk %d\n", ret);
		goto err0;
	}

	if (!IS_ERR(phy->optclk)) {
		ret = clk_enable(phy->optclk);
		if (ret < 0) {
			dev_err(phy->dev, "Failed to enable optclk %d\n", ret);
			goto err1;
		}
	}

	return 0;

err1:
	clk_disable(phy->wkupclk);

err0:
#endif
	return ret;
}

static const struct dev_pm_ops amlogic_usb2_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_usb2_runtime_suspend,
		amlogic_usb2_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_usb2_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_usb2_id_table[] = {
	{ .compatible = "amlogic, amlogic-usb2" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_usb2_id_table);
#endif

static struct platform_driver amlogic_usb2_driver = {
	.probe		= amlogic_usb2_probe,
	.remove		= amlogic_usb2_remove,
	.driver		= {
		.name	= "amlogic-usb2",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_usb2_id_table),
	},
};

module_platform_driver(amlogic_usb2_driver);

MODULE_ALIAS("platform: amlogic_usb2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB2 phy driver");
MODULE_LICENSE("GPL v2");
