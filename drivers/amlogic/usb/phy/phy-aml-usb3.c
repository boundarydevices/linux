/*
 * drivers/amlogic/usb/phy/phy-aml-usb3.c
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-gxbbtv.h>
#include "phy-aml-usb.h"

struct usb_aml_regs_t usb_aml_regs;

static int amlogic_usb3_suspend(struct usb_phy *x, int suspend)
{
#if 0
	struct omap_usb *phy = phy_to_omapusb(x);
	int	val;
	int timeout = PLL_IDLE_TIME;

	if (suspend && !phy->is_suspended) {
		val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
		val |= PLL_IDLE;
		omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

		do {
			val = omap_usb_readl(phy->pll_ctrl_base, PLL_STATUS);
			if (val & PLL_TICOPWDN)
				break;
			udelay(1);
		} while (--timeout);

		omap_control_usb3_phy_power(phy->control_dev, 0);

		phy->is_suspended	= 1;
	} else if (!suspend && phy->is_suspended) {
		phy->is_suspended	= 0;

		val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
		val &= ~PLL_IDLE;
		omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

		do {
			val = omap_usb_readl(phy->pll_ctrl_base, PLL_STATUS);
			if (!(val & PLL_TICOPWDN))
				break;
			udelay(1);
		} while (--timeout);
	}
#endif
	return 0;
}

void cr_bus_addr(unsigned int addr)
{
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	usb_r2.b.p30_cr_data_in = addr;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	usb_r2.b.p30_cr_cap_addr = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);
	usb_r2.b.p30_cr_cap_addr = 1;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 0);

	usb_r2.b.p30_cr_cap_addr = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 1);
}

int cr_bus_read(unsigned int addr)
{
	int data;
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	cr_bus_addr(addr);

	usb_r2.b.p30_cr_read = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);
	usb_r2.b.p30_cr_read = 1;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 0);

	data = usb_r6.b.p30_cr_data_out;

	usb_r2.b.p30_cr_read = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 1);

	return data;
}

void cr_bus_write(unsigned int addr, unsigned int data)
{
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	cr_bus_addr(addr);

	usb_r2.b.p30_cr_data_in = data;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	usb_r2.b.p30_cr_cap_data = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);
	usb_r2.b.p30_cr_cap_data = 1;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 0);

	usb_r2.b.p30_cr_cap_data = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 1);

	usb_r2.b.p30_cr_write = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);
	usb_r2.b.p30_cr_write = 1;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 0);

	usb_r2.b.p30_cr_write = 0;
	writel(usb_r2.d32, usb_aml_regs.usb_r[2]);

	do {
		usb_r6.d32 = readl(usb_aml_regs.usb_r[6]);
	} while (usb_r6.b.p30_cr_ack == 1);
}

static int amlogic_usb3_init(struct usb_phy *x)
{
	struct amlogic_usb *phy = phy_to_amlusb(x);

	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r2_t r2 = {.d32 = 0};
	union usb_r3_t r3 = {.d32 = 0};
	int i;
	u32 data = 0;
	int j = 0;

	amlogic_usbphy_reset();

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 7; j++) {
			usb_aml_regs.usb_r[j] = (void __iomem *)
				((unsigned long)phy->regs + i*PHY_REGISTER_SIZE
				+ 4*j);
		}

		r3.d32 = (1<<13) | (0x68<<24);
		writel(r3.d32, usb_aml_regs.usb_r[3]);

		udelay(2);
		r0.d32 = readl(usb_aml_regs.usb_r[0]);
		r0.b.p30_phy_reset = 1;
		writel(r0.d32, usb_aml_regs.usb_r[0]);

		udelay(2);

		r0.b.p30_phy_reset = 0;
		r0.b.p30_tx_vboost_lvl = 0x4;
		writel(r0.d32, usb_aml_regs.usb_r[0]);

		/*
		* WORKAROUND: There is SSPHY suspend bug due to
		* which USB enumerates
		* in HS mode instead of SS mode. Workaround it by asserting
		* LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus mode
		*/
		data = cr_bus_read(0x102d);
		data |= (1 << 7);
		cr_bus_write(0x102D, data);

		data = cr_bus_read(0x1010);
		data &= ~0xff0;
		data |= 0x20;
		cr_bus_write(0x1010, data);
		/*
		* Fix RX Equalization setting as follows
		* LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
		* LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
		* LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
		* LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
		*/
		data = cr_bus_read(0x1006);
		data &= ~(1 << 6);
		data |= (1 << 7);
		data &= ~(0x7 << 8);
		data |= (0x3 << 8);
		data |= (0x1 << 11);
		cr_bus_write(0x1006, data);

		/*
		* Set EQ and TX launch amplitudes as follows
		* LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
		* LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
		* LANE0.TX_OVRD_DRV_LO.EN set to 1.
		*/
		data = cr_bus_read(0x1002);
		data &= ~0x3f80;
		data |= (0x16 << 7);
		data &= ~0x7f;
		data |= (0x7f | (1 << 14));
		cr_bus_write(0x1002, data);

		/*
		* MPLL_LOOP_CTL.PROP_CNTRL
		*/
		data = cr_bus_read(0x30);
		data &= ~(0xf << 4);
		data |= (0x8 << 4);
		cr_bus_write(0x30, data);

		/*
		* TX_FULL_SWING  to 127
		*/
		r1.d32 = readl(usb_aml_regs.usb_r[1]);
		r1.b.p30_pcs_tx_swing_full = 127;
		r1.b.u3h_fladj_30mhz_reg = 0x20;
		writel(r1.d32, usb_aml_regs.usb_r[1]);
		udelay(2);

		/*
		* TX_DEEMPH_3_5DB  to 22
		*/
		r2.d32 = readl(usb_aml_regs.usb_r[2]);
		r2.b.p30_pcs_tx_deemph_3p5db = 22;
		writel(r2.d32, usb_aml_regs.usb_r[2]);

		udelay(2);

		/*
		* LOS_BIAS  to 0x5
		* LOS_LEVEL to 0x9
		*/
		r3.d32 = readl(usb_aml_regs.usb_r[3]);
		r3.b.p30_los_bias = 0x5;
		r3.b.p30_los_level = 0x9;
		r3.b.p30_ssc_en = 1;
		r3.b.p30_ssc_range = 2;
		writel(r3.d32, usb_aml_regs.usb_r[3]);
	}
	return 0;
}

static int amlogic_usb3_probe(struct platform_device *pdev)
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
		dev_err(&pdev->dev, "unable to allocate memory for USB3 PHY\n");
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy2";
	phy->phy.init		= amlogic_usb3_init;
	phy->phy.set_suspend	= amlogic_usb3_suspend;
	phy->phy.type		= USB_PHY_TYPE_USB3;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_usb3_remove(struct platform_device *pdev)
{
#if 0
	struct omap_usb *phy = platform_get_drvdata(pdev);

	clk_unprepare(phy->wkupclk);
	clk_unprepare(phy->optclk);
	usb_remove_phy(&phy->phy);
	if (!pm_runtime_suspended(&pdev->dev))
		pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
#endif
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_usb3_runtime_suspend(struct device *dev)
{
#if 0
	struct platform_device	*pdev = to_platform_device(dev);
	struct omap_usb	*phy = platform_get_drvdata(pdev);

	clk_disable(phy->wkupclk);
	clk_disable(phy->optclk);
#endif
	return 0;
}

static int amlogic_usb3_runtime_resume(struct device *dev)
{
	u32 ret = 0;
#if 0
	struct platform_device	*pdev = to_platform_device(dev);
	struct omap_usb	*phy = platform_get_drvdata(pdev);

	ret = clk_enable(phy->optclk);
	if (ret) {
		dev_err(phy->dev, "Failed to enable optclk %d\n", ret);
		goto err1;
	}

	ret = clk_enable(phy->wkupclk);
	if (ret) {
		dev_err(phy->dev, "Failed to enable wkupclk %d\n", ret);
		goto err2;
	}

	return 0;

err2:
	clk_disable(phy->optclk);

err1:
#endif
	return ret;
}

static const struct dev_pm_ops amlogic_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_usb3_runtime_suspend,
		amlogic_usb3_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_usb3_id_table[] = {
	{ .compatible = "amlogic, amlogic-usb3" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_usb3_id_table);
#endif

static struct platform_driver amlogic_usb3_driver = {
	.probe		= amlogic_usb3_probe,
	.remove		= amlogic_usb3_remove,
	.driver		= {
		.name	= "amlogic-usb3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_usb3_id_table),
	},
};

module_platform_driver(amlogic_usb3_driver);

MODULE_ALIAS("platform: amlogic_usb3");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 phy driver");
MODULE_LICENSE("GPL v2");
