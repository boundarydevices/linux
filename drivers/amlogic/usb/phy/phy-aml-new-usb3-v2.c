/*
 * drivers/amlogic/usb/phy/phy-aml-new-usb3-v2.c
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
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include "phy-aml-new-usb-v2.h"

#define HOST_MODE	0
#define DEVICE_MODE	1

struct usb_aml_regs_v2 usb_new_aml_regs_v2;
struct amlogic_usb_v2	*g_phy_v2;

static void set_mode(unsigned long reg_addr, int mode);
BLOCKING_NOTIFIER_HEAD(aml_new_usb_v2_notifier_list);

int aml_new_usb_v2_register_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register
			(&aml_new_usb_v2_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(aml_new_usb_v2_register_notifier);

int aml_new_usb_v2_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister
			(&aml_new_usb_v2_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(aml_new_usb_v2_unregister_notifier);

static void aml_new_usb_notifier_call(unsigned long is_device_on)
{
	blocking_notifier_call_chain
			(&aml_new_usb_v2_notifier_list, is_device_on, NULL);
}
//EXPORT_SYMBOL(aml_new_usb_notifier_call);

static void set_usb_vbus_power
	(struct gpio_desc *usb_gd, int pin, char is_power_on)
{
	if (is_power_on)
		/*set vbus on by gpio*/
		gpiod_direction_output(usb_gd, is_power_on);
	else
		/*set vbus off by gpio first*/
		gpiod_direction_output(usb_gd, is_power_on);
}

static void amlogic_new_set_vbus_power
		(struct amlogic_usb_v2 *phy, char is_power_on)
{
	if (phy->vbus_power_pin != -1)
		set_usb_vbus_power(phy->usb_gpio_desc,
			phy->vbus_power_pin, is_power_on);
}

static int amlogic_new_usb3_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_new_usb3phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);

	phy->suspend_flag = 1;
}

void aml_new_usb_v2_init(void)
{
	union usb_r5_v2 r5 = {.d32 = 0};
	unsigned long reg_addr = ((unsigned long)
		usb_new_aml_regs_v2.usb_r_v2[0] - 0x80);

	r5.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[5]);
	if (r5.b.iddig_curr == 0) {
		amlogic_new_set_vbus_power(g_phy_v2, 1);
		aml_new_usb_notifier_call(0);
		set_mode(reg_addr, HOST_MODE);
	}
}
EXPORT_SYMBOL(aml_new_usb_v2_init);

static int amlogic_new_usb3_init(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	union usb_r1_v2 r1 = {.d32 = 0};
	union usb_r2_v2 r2 = {.d32 = 0};
	union usb_r3_v2 r3 = {.d32 = 0};
	union usb_r5_v2 r5 = {.d32 = 0};
	int i = 0;

	if (phy->suspend_flag) {
		phy->suspend_flag = 0;
		return 0;
	}

	for (i = 0; i < 6; i++) {
		usb_new_aml_regs_v2.usb_r_v2[i] = (void __iomem *)
			((unsigned long)phy->regs + 4*i);
	}

	r1.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[1]);
	r1.b.u3h_fladj_30mhz_reg = 0x26;
	writel(r1.d32, usb_new_aml_regs_v2.usb_r_v2[1]);

	r5.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[5]);
	r5.b.iddig_en0 = 1;
	r5.b.iddig_en1 = 1;
	r5.b.iddig_th = 255;
	writel(r5.d32, usb_new_aml_regs_v2.usb_r_v2[5]);

	/* config usb3 phy */
	if (phy->portnum > 0) {
		r3.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[3]);
		r3.b.p30_ssc_en = 1;
		r3.b.p30_ref_ssp_en = 1;
		writel(r3.d32, usb_new_aml_regs_v2.usb_r_v2[3]);
		udelay(2);
		r2.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[2]);
		r2.b.p30_pcs_tx_deemph_3p5db = 0x15;
		r2.b.p30_pcs_tx_deemph_6db = 0x20;
		writel(r2.d32, usb_new_aml_regs_v2.usb_r_v2[2]);
		udelay(2);
		r1.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[1]);
		r1.b.u3h_host_port_power_control_present = 1;
		r1.b.u3h_fladj_30mhz_reg = 0x26;
		writel(r1.d32, usb_new_aml_regs_v2.usb_r_v2[1]);
		udelay(2);
	}

	return 0;
}

static void set_mode(unsigned long reg_addr, int mode)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	struct usb_aml_regs_v2 usb_gxl_aml_regs;
	union u2p_r0_v2 reg0;
	union usb_r0_v2 r0 = {.d32 = 0};
	union usb_r4_v2 r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r_v2[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);

	usb_gxl_aml_regs.usb_r_v2[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_gxl_aml_regs.usb_r_v2[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_gxl_aml_regs.usb_r_v2[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);

	r0.d32 = readl(usb_gxl_aml_regs.usb_r_v2[0]);
	if (mode == DEVICE_MODE) {
		r0.b.u2d_act = 1;
		r0.b.u2d_ss_scaledown_mode = 0;
	} else
		r0.b.u2d_act = 0;
	writel(r0.d32, usb_gxl_aml_regs.usb_r_v2[0]);

	r4.d32 = readl(usb_gxl_aml_regs.usb_r_v2[4]);
	if (mode == DEVICE_MODE)
		r4.b.p21_SLEEPM0 = 0x1;
	else
		r4.b.p21_SLEEPM0 = 0x0;
	writel(r4.d32, usb_gxl_aml_regs.usb_r_v2[4]);

	reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
	if (mode == DEVICE_MODE) {
		reg0.b.host_device = 0;
		reg0.b.POR = 0;
	} else {
		reg0.b.host_device = 1;
		reg0.b.POR = 0;
	}
	writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);

	udelay(500);
}

static void amlogic_gxl_work(struct work_struct *work)
{
	struct amlogic_usb_v2 *phy =
		container_of(work, struct amlogic_usb_v2, work.work);
	union usb_r5_v2 r5 = {.d32 = 0};
	unsigned long reg_addr = ((unsigned long)phy->regs - 0x80);

	r5.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[5]);
	if (r5.b.iddig_curr == 0) {
		amlogic_new_set_vbus_power(phy, 1);
		aml_new_usb_notifier_call(0);
		set_mode(reg_addr, HOST_MODE);
	} else {
		set_mode(reg_addr, DEVICE_MODE);
		aml_new_usb_notifier_call(1);
		amlogic_new_set_vbus_power(phy, 0);
	}
	r5.b.usb_iddig_irq = 0;
	writel(r5.d32, usb_new_aml_regs_v2.usb_r_v2[5]);
}

static irqreturn_t amlogic_botg_detect_irq(int irq, void *dev)
{
	struct amlogic_usb_v2 *phy = (struct amlogic_usb_v2 *)dev;
	union usb_r5_v2 r5 = {.d32 = 0};

	r5.d32 = readl(usb_new_aml_regs_v2.usb_r_v2[5]);
	r5.b.usb_iddig_irq = 0;
	writel(r5.d32, usb_new_aml_regs_v2.usb_r_v2[5]);

	schedule_delayed_work(&phy->work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static bool device_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	if (!device)
		return false;

	status = of_get_property(device, "status", &statlen);
	if (status == NULL)
		return true;

	if (statlen > 0) {
		if (!strcmp(status, "okay") || !strcmp(status, "ok"))
			return true;
	}

	return false;
}

static int amlogic_new_usb3_v2_probe(struct platform_device *pdev)
{
	struct amlogic_usb_v2			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	void __iomem *phy3_base;
	unsigned int phy3_mem;
	unsigned int phy3_mem_size = 0;
	const char *gpio_name = NULL;
	struct gpio_desc *usb_gd = NULL;
	const void *prop;
	int portnum = 0;
	int irq;
	int retval;
	int gpio_vbus_power_pin = -1;
	int otg = 0;
	int ret;
	struct device_node *tsi_pci;

	gpio_name = of_get_property(dev->of_node, "gpio-vbus-power", NULL);
	if (gpio_name) {
		gpio_vbus_power_pin = 1;
		usb_gd = gpiod_get_index(&pdev->dev,
				 NULL, 0, GPIOD_OUT_LOW);
		if (IS_ERR(usb_gd))
			return -1;
	}

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	tsi_pci = of_find_node_by_type(NULL, "pci");
	if (tsi_pci) {
		if (device_is_available(tsi_pci)) {
			dev_info(&pdev->dev,
				"pci-e driver probe, disable USB 3.0 function!!!\n");
			portnum = 0;
		}
	}

	prop = of_get_property(dev->of_node, "otg", NULL);
	if (prop)
		otg = of_read_ulong(prop, 1);

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	retval = of_property_read_u32(dev->of_node, "phy-reg", &phy3_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
				(dev->of_node, "phy-reg-size", &phy3_mem_size);
	if (retval < 0)
		return -EINVAL;

	phy3_base = devm_ioremap_nocache
				(&(pdev->dev), (resource_size_t)phy3_mem,
				(unsigned long)phy3_mem_size);
	if (!phy3_base)
		return -ENOMEM;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	if (otg) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			return -ENODEV;
		retval = request_irq(irq, amlogic_botg_detect_irq,
				IRQF_SHARED | IRQ_LEVEL,
				"amlogic_botg_detect", phy);

		if (retval) {
			dev_err(&pdev->dev, "request of irq%d failed\n", irq);
			retval = -EBUSY;
			return retval;
		}
	}

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->phy3_cfg	= phy3_base;
	phy->portnum      = portnum;
	phy->suspend_flag = 0;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy3";
	phy->phy.init		= amlogic_new_usb3_init;
	phy->phy.set_suspend	= amlogic_new_usb3_suspend;
	phy->phy.shutdown	= amlogic_new_usb3phy_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB3;
	phy->phy.flags		= AML_USB3_PHY_DISABLE;
	phy->vbus_power_pin = gpio_vbus_power_pin;
	phy->usb_gpio_desc = usb_gd;

	/* set the phy from pcie to usb3 */
	if (phy->portnum > 0) {
		writel((readl(phy->phy3_cfg) | (3<<5)), phy->phy3_cfg);
		udelay(100);

		phy->clk = devm_clk_get(dev, "pcie_refpll");
		if (IS_ERR(phy->clk)) {
			dev_err(dev, "Failed to get usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}

		ret = clk_prepare_enable(phy->clk);
		if (ret) {
			dev_err(dev, "Failed to enable usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}
		phy->phy.flags = AML_USB3_PHY_ENABLE;
	}

	INIT_DELAYED_WORK(&phy->work, amlogic_gxl_work);

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	g_phy_v2 = phy;

	return 0;
}

static int amlogic_new_usb3_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_usb3_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_usb3_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_new_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_usb3_runtime_suspend,
		amlogic_new_usb3_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_usb3_v2_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-usb3-v2" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_usb3_v2_id_table);
#endif

static struct platform_driver amlogic_new_usb3_v2_driver = {
	.probe		= amlogic_new_usb3_v2_probe,
	.remove		= amlogic_new_usb3_remove,
	.driver		= {
		.name	= "amlogic-new-usb3-v2",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_usb3_v2_id_table),
	},
};

module_platform_driver(amlogic_new_usb3_v2_driver);

MODULE_ALIAS("platform: amlogic_usb3_v2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 v2 phy driver");
MODULE_LICENSE("GPL v2");
