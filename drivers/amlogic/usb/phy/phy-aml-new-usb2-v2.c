/*
 * drivers/amlogic/usb/phy/phy-aml-new-usb2-v2.c
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
#include <linux/amlogic/usb-v2.h>
#include "phy-aml-new-usb-v2.h"

struct amlogic_usb_v2	*g_phy2_v2;
#define TUNING_DISCONNECT_THRESHOLD 0x34

void set_usb_phy_host_tuning(int port, int default_val)
{
	void __iomem	*phy_reg_base;

	if (!g_phy2_v2)
		return;

	if (g_phy2_v2->phy_version == 1)
		return;

	if (port > g_phy2_v2->portnum)
		return;
	if (default_val == g_phy2_v2->phy_cfg_state[port])
		return;

	phy_reg_base = g_phy2_v2->phy_cfg[port];
	dev_info(g_phy2_v2->dev, "---%s port(%d) tuning for host cf(%pf)--\n",
		default_val ? "Recovery" : "Set",
		port, __builtin_return_address(0));
	if (!default_val) {
		writel(g_phy2_v2->pll_setting[6], phy_reg_base + 0x38);
		writel(g_phy2_v2->pll_setting[5], phy_reg_base + 0x34);
	} else {
		writel(0, phy_reg_base + 0x38);
		writel(g_phy2_v2->pll_setting[5], phy_reg_base + 0x34);
	}
	g_phy2_v2->phy_cfg_state[port] = default_val;
}

void set_usb_phy_device_tuning(int port, int default_val)
{
	void __iomem	*phy_reg_base;

	if (!g_phy2_v2)
		return;

	if (g_phy2_v2->phy_version == 1)
		return;

	if (port > g_phy2_v2->portnum)
		return;
	if (default_val == g_phy2_v2->phy_cfg_state[port])
		return;

	phy_reg_base = g_phy2_v2->phy_cfg[port];
	dev_info(g_phy2_v2->dev, "---%s port(%d) tuning for device cf(%pf)--\n",
		default_val ? "Recovery" : "Set",
		port, __builtin_return_address(0));
	if (!default_val) {
		writel(g_phy2_v2->pll_setting[7], phy_reg_base + 0x38);
		writel(g_phy2_v2->pll_setting[5], phy_reg_base + 0x34);
	} else {
		writel(0, phy_reg_base + 0x38);
		writel(g_phy2_v2->pll_setting[5], phy_reg_base + 0x34);
	}
	g_phy2_v2->phy_cfg_state[port] = default_val;
}


void set_usb_pll(struct amlogic_usb_v2 *phy, void __iomem	*reg)
{
	/* TO DO set usb  PLL */
	writel((0x30000000 | (phy->pll_setting[0])), reg + 0x40);
	writel(phy->pll_setting[1], reg + 0x44);
	writel(phy->pll_setting[2], reg + 0x48);
	udelay(100);
	writel((0x10000000 | (phy->pll_setting[0])), reg + 0x40);

	/* PHY Tune */
	writel(phy->pll_setting[3], reg + 0x50);
	writel(phy->pll_setting[4], reg + 0x10);
	/* Recovery analog status */
	writel(0, reg + 0x38);
	writel(phy->pll_setting[5], reg + 0x34);

	writel(TUNING_DISCONNECT_THRESHOLD, reg + 0xC);
}

static int amlogic_new_usb2_init(struct usb_phy *x)
{
	int i, j, cnt;

	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r0_v2 reg0;
	union u2p_r1_v2 reg1;
	u32 val;
	u32 temp = 0;
	u32 portnum = phy->portnum;

	while (portnum--)
		temp = temp | (1 << (16 + portnum));

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (0x21 * 4 - 0x8)));
	writel((val | temp), (void __iomem *)
		((unsigned long)phy->reset_regs + (0x21 * 4 - 0x8)));

	amlogic_new_usbphy_reset_v2(phy);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + i*PHY_REGISTER_SIZE
				+ 4 * j);
		}

		reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
		reg0.b.POR = 1;
		if (phy->suspend_flag == 0) {
			reg0.b.host_device = 1;
			if (i == 1) {
				reg0.b.IDPULLUP0 = 1;
				reg0.b.DRVVBUS0 = 1;
			}
		}

		writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
	}

	udelay(10);
	amlogic_new_usbphy_reset_phycfg_v2(phy, phy->portnum);
	udelay(50);

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 2; j++) {
			u2p_aml_regs.u2p_r_v2[j] = (void __iomem	*)
				((unsigned long)phy->regs + i*PHY_REGISTER_SIZE
				+ 4 * j);
		}
		/* ID DETECT: usb2_otg_aca_en set to 0 */
		/* usb2_otg_iddet_en set to 1 */
		writel(readl(phy->phy_cfg[i] + 0x54) & (~(1 << 2)),
			(phy->phy_cfg[i] + 0x54));

		reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
		cnt = 0;
		while (reg1.b.phy_rdy != 1) {
			reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
			/*we wait phy ready max 1ms, common is 100us*/
			if (cnt > 200)
				break;

			cnt++;
			udelay(5);
		}
	}

	/* step 7: pll setting */
	for (i = 0; i < phy->portnum; i++)
		set_usb_pll(phy, phy->phy_cfg[i]);

	return 0;
}

static int amlogic_new_usb2_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_new_usb2phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	u32 val;
	u32 temp = 0;
	u32 cnt = phy->portnum;

	while (cnt--)
		temp = temp | (1 << (16 + cnt));

	/* set usb phy to low power mode */
	val = readl((void __iomem		*)
		((unsigned long)phy->reset_regs + (0x21 * 4 - 0x8)));
	writel((val & (~temp)), (void __iomem	*)
		((unsigned long)phy->reset_regs + (0x21 * 4 - 0x8)));

	phy->suspend_flag = 1;
}

static int amlogic_new_usb2_probe(struct platform_device *pdev)
{
	struct amlogic_usb_v2			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	struct resource *reset_mem;
	struct resource *phy_cfg_mem[4];
	void __iomem	*phy_base;
	void __iomem	*reset_base = NULL;
	void __iomem	*phy_cfg_base[4];
	int portnum = 0;
	int phy_version = 0;
	const void *prop;
	int i = 0;
	int retval;
	u32 pll_setting[8];

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum) {
		dev_err(&pdev->dev, "This phy has no usb port\n");
		return -ENOMEM;
	}

	prop = of_get_property(dev->of_node, "version", NULL);
	if (prop)
		phy_version = of_read_ulong(prop, 1);
	else
		phy_version = 0;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (reset_mem) {
		reset_base = ioremap(reset_mem->start,
			resource_size(reset_mem));
		if (IS_ERR(reset_base))
			return PTR_ERR(reset_base);
	}

	for (i = 0; i < portnum; i++) {
		phy_cfg_mem[i] = platform_get_resource
					(pdev, IORESOURCE_MEM, 2 + i);
		if (phy_cfg_mem[i]) {
			phy_cfg_base[i] = ioremap(phy_cfg_mem[i]->start,
				resource_size(phy_cfg_mem[i]));
			if (IS_ERR(phy_cfg_base[i]))
				return PTR_ERR(phy_cfg_base[i]);
		}
	}

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-1", &(pll_setting[0]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-2", &(pll_setting[1]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-3", &(pll_setting[2]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-4", &(pll_setting[3]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-5", &(pll_setting[4]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
		"pll-setting-6", &(pll_setting[5]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
			"pll-setting-7", &(pll_setting[6]));
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32(dev->of_node,
			"pll-setting-8", &(pll_setting[7]));
	if (retval < 0)
		return -EINVAL;

	dev_info(&pdev->dev, "USB2 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->reset_regs = reset_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy2";
	phy->phy.init		= amlogic_new_usb2_init;
	phy->phy.set_suspend	= amlogic_new_usb2_suspend;
	phy->phy.shutdown	= amlogic_new_usb2phy_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB2;
	phy->pll_setting[0] = pll_setting[0];
	phy->pll_setting[1] = pll_setting[1];
	phy->pll_setting[2] = pll_setting[2];
	phy->pll_setting[3] = pll_setting[3];
	phy->pll_setting[4] = pll_setting[4];
	phy->pll_setting[5] = pll_setting[5];
	phy->pll_setting[6] = pll_setting[6];
	phy->pll_setting[7] = pll_setting[7];
	phy->suspend_flag = 0;
	phy->phy_version = phy_version;
	for (i = 0; i < portnum; i++) {
		phy->phy_cfg[i] = phy_cfg_base[i];
		/* set port default tuning state */
		phy->phy_cfg_state[i] = 1;
	}

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	g_phy2_v2 = phy;

	return 0;
}

static int amlogic_new_usb2_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_usb2_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_usb2_runtime_resume(struct device *dev)
{
	unsigned int ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_new_usb2_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_usb2_runtime_suspend,
		amlogic_new_usb2_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_usb2_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_usb2_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-usb2-v2" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_usb2_id_table);
#endif

static struct platform_driver amlogic_new_usb2_v2_driver = {
	.probe		= amlogic_new_usb2_probe,
	.remove		= amlogic_new_usb2_remove,
	.driver		= {
		.name	= "amlogic-new-usb2-v2",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_usb2_id_table),
	},
};

module_platform_driver(amlogic_new_usb2_v2_driver);

MODULE_ALIAS("platform: amlogic_usb2_v2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB2 v2 phy driver");
MODULE_LICENSE("GPL v2");
