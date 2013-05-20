/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2012 Marek Vasut <marex@denx.de>
 * on behalf of DENX Software Engineering GmbH
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/usb/chipidea.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/usb/of.h>
#include <linux/io.h>

#include "ci.h"
#include "bits.h"
#include "ci13xxx_imx.h"
#include "usbmisc_imx.c"

#define pdev_to_phy(pdev) \
	((struct usb_phy *)platform_get_drvdata(pdev))
#define IOMUXC_IOMUXC_GPR1			0x00000004
#define USB_OTG_ID_SEL_BIT			(1<<13)

static const struct of_device_id ci13xxx_imx_dt_ids[] = {
	{ .compatible = "fsl,imx25-usb", .data = (void *)&imx25_usbmisc_ops },
	{ .compatible = "fsl,imx53-usb", .data = (void *)&imx53_usbmisc_ops },
	{ .compatible = "fsl,imx6q-usb", .data = (void *)&imx6q_usbmisc_ops },
	{ .compatible = "fsl,imx27-usb", .data = NULL },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ci13xxx_imx_dt_ids);

static int usbmisc_init(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct ci13xxx_imx_data *data = dev_get_drvdata(dev);
	int ret;
	const struct of_device_id *of_id =
			of_match_device(ci13xxx_imx_dt_ids, dev);

	if (of_id->data)
		data->usbmisc_ops = (struct usbmisc_ops *)of_id->data;
	else
		dev_dbg(dev, "no usbmisc_ops\n");

	ret = of_alias_get_id(np, "usb");
	if (ret < 0) {
		dev_dbg(dev, "failed to get alias id, errno %d\n", ret);
		data->index = -1;
	} else {
		data->index = ret;
	}

	if (of_find_property(np, "disable-over-current", NULL))
		data->disable_oc = 1;

	if (of_find_property(np, "external-vbus-divider", NULL))
		data->evdo = 1;

	return 0;
}

static int imx_set_wakeup(struct ci13xxx_imx_data *data , bool enable)
{
	int ret = 0;
	struct usbmisc_ops *usbmisc_ops = data->usbmisc_ops;
	if (usbmisc_ops && usbmisc_ops->set_wakeup) {
		enum ci_usb_wakeup_events wakeup_event =
			CI_USB_WAKEUP_EVENT_NONE;
		struct platform_device *plat_ci = data->ci_pdev;
		struct ci13xxx *ci = platform_get_drvdata(plat_ci);

		if (!enable)
			wakeup_event = CI_USB_WAKEUP_EVENT_NONE;
		else if (ci->is_otg)
			wakeup_event = CI_USB_WAKEUP_EVENT_OTG;
		else if (ci->roles[CI_ROLE_HOST])
			wakeup_event = CI_USB_WAKEUP_EVENT_HOST;
		else if (ci->roles[CI_ROLE_GADGET])
			wakeup_event = CI_USB_WAKEUP_EVENT_GADGET;

		ret = usbmisc_ops->set_wakeup(data, wakeup_event);
	}

	return ret;
}

bool ci_is_host_mode(struct ci13xxx *ci)
{
	return hw_read(ci, OP_USBMODE, USBMODE_CM) == USBMODE_CM;
}

static void usbphy_pre_suspend(struct ci13xxx *ci, struct usb_phy *phy)
{
	if (IS_ENABLED(CONFIG_USB_MXS_PHY)) {
		extern void mxs_phy_disconnect_line
			(struct usb_phy *phy, bool enable);
		if (!ci_is_host_mode(ci))
			mxs_phy_disconnect_line(phy, true);
	}
}

static void usbphy_post_resume(struct ci13xxx *ci, struct usb_phy *phy)
{
	if (IS_ENABLED(CONFIG_USB_MXS_PHY)) {
		extern void mxs_phy_disconnect_line
			(struct usb_phy *phy, bool enable);
		if (!ci_is_host_mode(ci))
			mxs_phy_disconnect_line(phy, false);
	}
}

static void usbphy_regulator_enable(struct usb_phy *phy, bool enable)
{
	/* This setting is only used at low power mode */
	if (IS_ENABLED(CONFIG_USB_MXS_PHY)) {
		extern void mxs_phy_enable_regulator
			(struct usb_phy *phy, bool enable);
		mxs_phy_enable_regulator(phy, enable);
	}
}

static int ci13xxx_otg_set_vbus(struct usb_otg *otg, bool enabled)
{
	struct ci13xxx	*ci = container_of(otg, struct ci13xxx, otg);
	struct regulator *reg_vbus = ci->reg_vbus;
	int ret;

	WARN_ON(!reg_vbus);

	if (reg_vbus) {
		if (enabled) {
			ret = regulator_enable(reg_vbus);
			if (ret) {
				dev_err(ci->dev,
				"Failed to enable vbus regulator, ret=%d\n",
				ret);
				return ret;
			}
		} else {
			ret = regulator_disable(reg_vbus);
			if (ret) {
				dev_err(ci->dev,
				"Failed to disable vbus regulator, ret=%d\n",
				ret);
				return ret;
			}
		}
	}

	return 0;
}

static int ci13xxx_imx_probe(struct platform_device *pdev)
{
	struct ci13xxx_imx_data *data;
	struct ci13xxx_platform_data *pdata;
	struct platform_device *plat_ci, *phy_pdev;
	struct ci13xxx	*ci;
	struct device_node *ci_np, *phy_np;
	struct resource *res;
	struct regulator *reg_vbus;
	struct pinctrl *pinctrl;
	struct regmap *iomuxc_gpr;
	u32 gpr1 = 0;
	int ret;

	ci_np = pdev->dev.of_node;
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to allocate CI13xxx-IMX pdata!\n");
		return -ENOMEM;
	}

	pdata->name = "ci13xxx_imx";
	pdata->capoffset = DEF_CAPOFFSET;
	pdata->flags = CI13XXX_REQUIRE_TRANSCEIVER |
		       CI13XXX_DISABLE_STREAMING |
		       CI13XXX_REGS_SHARED;

	pdata->phy_mode = of_usb_get_phy_mode(pdev->dev.of_node);
	pdata->dr_mode = of_usb_get_dr_mode(pdev->dev.of_node);

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Failed to allocate CI13xxx-IMX data!\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Can't get device resources 0!\n");
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_dbg(&pdev->dev,
			"Can't get resource, mx23/mx28 doesn't need it\n");
	} else {
		data->non_core_base_addr = devm_ioremap
			(&pdev->dev, res->start, resource_size(res));
		if (!data->non_core_base_addr) {
			dev_err(&pdev->dev,
				"remap non core register failed!\n");
			return -EBUSY;
		}
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl))
		dev_warn(&pdev->dev, "pinctrl get/select failed, err=%ld\n",
			PTR_ERR(pinctrl));

	data->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(data->clk)) {
		dev_err(&pdev->dev,
			"Failed to get clock, err=%ld\n", PTR_ERR(data->clk));
		return PTR_ERR(data->clk);
	}

	ret = clk_prepare_enable(data->clk);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to prepare or enable clock, err=%d\n", ret);
		return ret;
	}

	phy_np = of_parse_phandle(pdev->dev.of_node, "fsl,usbphy", 0);
	if (phy_np) {
		data->phy_np = phy_np;
		phy_pdev = of_find_device_by_node(phy_np);
		if (phy_pdev) {
			struct usb_phy *phy;
			phy = pdev_to_phy(phy_pdev);
			if (phy &&
			    try_module_get(phy_pdev->dev.driver->owner)) {
				usb_phy_init(phy);
				data->phy = phy;
			}
		}
	}

	reg_vbus = devm_regulator_get(&pdev->dev, "vbus");
	if (!IS_ERR(reg_vbus))
		data->reg_vbus = reg_vbus;
	else
		reg_vbus = NULL;

	pdata->phy = data->phy;

	if (!pdev->dev.dma_mask) {
		pdev->dev.dma_mask = devm_kzalloc(&pdev->dev,
				      sizeof(*pdev->dev.dma_mask), GFP_KERNEL);
		if (!pdev->dev.dma_mask) {
			ret = -ENOMEM;
			dev_err(&pdev->dev, "Failed to alloc dma_mask!\n");
			goto put_np;
		}
		*pdev->dev.dma_mask = DMA_BIT_MASK(32);
		dma_set_coherent_mask(&pdev->dev, *pdev->dev.dma_mask);
	}

	spin_lock_init(&data->lock);

	/* Some platform specific initializations */
	ret = usbmisc_init(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "usbmisc_init failed, ret=%d\n", ret);
		goto put_np;
	}

	if (data->usbmisc_ops && data->usbmisc_ops->init) {
		ret = data->usbmisc_ops->init(data);
		if (ret) {
			dev_err(&pdev->dev,
				"usbmisc_ops->init failed, ret=%d\n", ret);
			goto put_np;
		}
	}

	/* Any imx6 user who needs to change id select pin, do below */
	if (of_find_property(pdev->dev.of_node,
			"otg_id_pin_select_change", NULL)) {
		iomuxc_gpr = syscon_regmap_lookup_by_compatible
			("fsl,imx6q-iomuxc-gpr");
		if (!IS_ERR(iomuxc_gpr)) {
			/* Select USB ID pin at iomuxc grp1 */
			regmap_read(iomuxc_gpr, IOMUXC_IOMUXC_GPR1, &gpr1);
			regmap_write(iomuxc_gpr, IOMUXC_IOMUXC_GPR1,
					gpr1 | USB_OTG_ID_SEL_BIT);
		} else {
			ret = PTR_ERR(iomuxc_gpr);
			dev_warn(&pdev->dev,
				"failed to find imx6q-iomuxc-gpr regmap:%d\n",
				ret);
			goto put_np;
		}
	}

	plat_ci = ci13xxx_add_device(&pdev->dev,
				pdev->resource, pdev->num_resources,
				pdata);
	if (IS_ERR(plat_ci)) {
		ret = PTR_ERR(plat_ci);
		dev_err(&pdev->dev,
			"Can't register ci_hdrc platform device, err=%d\n",
			ret);
		goto put_np;
	}

	if (data->usbmisc_ops && data->usbmisc_ops->post) {
		ret = data->usbmisc_ops->post(data);
		if (ret) {
			dev_err(&pdev->dev,
				"usbmisc post failed, ret=%d\n", ret);
			goto put_np;
		}
	}

	data->ci_pdev = plat_ci;

	ci = platform_get_drvdata(plat_ci);

	if (!ci) {
		ret = -ENODEV;
		dev_err(&pdev->dev,
			"some wrong at ci core's initialization\n");
		goto put_np;
	}
	/*
	 * Internal vbus on/off policy
	 * - Always on for host only function
	 * - Always off for gadget only function
	 * - call otg.set_vbus to control on/off according usb role
	 */

	if (ci->roles[CI_ROLE_HOST] && !ci->roles[CI_ROLE_GADGET]
			&& reg_vbus) {
		ret = regulator_enable(reg_vbus);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to enable vbus regulator, ret=%d\n",
				ret);
			goto put_np;
		}
	} else if (ci->is_otg) {
		ci->otg.set_vbus = ci13xxx_otg_set_vbus;
		ci->reg_vbus = data->reg_vbus;
	}

	device_set_wakeup_capable(&pdev->dev, true);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

put_np:
	if (phy_np)
		of_node_put(phy_np);
	clk_disable_unprepare(data->clk);

	return ret;
}

static int ci13xxx_imx_remove(struct platform_device *pdev)
{
	struct ci13xxx_imx_data *data = platform_get_drvdata(pdev);
	struct platform_device *plat_ci;
	int ret = 0;

	plat_ci = data->ci_pdev;

	pm_runtime_disable(&pdev->dev);

	if (data->reg_vbus)
		regulator_disable(data->reg_vbus);

	ret = imx_set_wakeup(data, false);
	if (ret) {
		dev_err(&pdev->dev,
			"usbmisc set_wakeup failed, ret=%d\n", ret);
		return ret;
	}

	ci13xxx_remove_device(plat_ci);

	if (data->phy) {
		usb_phy_shutdown(data->phy);
		module_put(data->phy->dev->driver->owner);
	}

	of_node_put(data->phy_np);

	clk_disable_unprepare(data->clk);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int imx_controller_suspend(struct device *dev)
{
	struct ci13xxx_imx_data *data = dev_get_drvdata(dev);
	struct platform_device *plat_ci;
	struct ci13xxx *ci;
	int ret = 0;

	dev_dbg(dev, "at the beginning of %s\n", __func__);

	plat_ci = data->ci_pdev;
	ci = platform_get_drvdata(plat_ci);

	if (atomic_read(&ci->in_lpm))
		return 0;

	disable_irq(ci->irq);

	/* without usbphy_pre_suspend, the unexptected wakeup may occur */
	if (data->phy)
		usbphy_pre_suspend(ci, data->phy);

	ret = imx_set_wakeup(data, true);
	if (ret) {
		dev_err(dev,
			"usbmisc set_wakeup failed, ret=%d\n", ret);
		return ret;
	}

	hw_write(ci, OP_PORTSC, PORTSC_PHCD, PORTSC_PHCD);

	if (data->phy)
		usb_phy_set_suspend(data->phy, 1);

	clk_disable_unprepare(data->clk);

	atomic_set(&ci->in_lpm, 1);

	enable_irq(ci->irq);

	dev_dbg(dev, "at the end of %s\n", __func__);

	return ret;

}
static int imx_controller_resume(struct device *dev)
{
	int ret;
	struct ci13xxx_imx_data *data = dev_get_drvdata(dev);
	struct platform_device *plat_ci;
	struct ci13xxx *ci;
	struct usbmisc_ops *usbmisc_ops = data->usbmisc_ops;

	dev_dbg(dev, "at the beginning of %s\n", __func__);

	plat_ci = data->ci_pdev;
	ci = platform_get_drvdata(plat_ci);

	if (!atomic_read(&ci->in_lpm))
		return 0;

	ret = clk_prepare_enable(data->clk);
	if (ret) {
		dev_err(dev,
			"Failed to prepare or enable clock, err=%d\n", ret);
		return ret;
	}

	if (usbmisc_ops && usbmisc_ops->is_wakeup_intr) {
		if (usbmisc_ops->is_wakeup_intr(data) == 1) {
			dev_dbg(dev, "Wakeup interrupt occurs %s\n", __func__);
			/* Wait controller reflects PHY's status */
			mdelay(2);
		}
	}

	ret = imx_set_wakeup(data, false);
	if (ret) {
		dev_err(dev,
			"usbmisc set_wakeup failed, ret=%d\n", ret);
		return ret;
	}

	if (hw_read(ci, OP_PORTSC, PORTSC_PHCD)) {
		hw_write(ci, OP_PORTSC, PORTSC_PHCD, 0);
		/* Some clks sync between Controller and USB PHY */
		mdelay(1);
	}

	if (data->phy) {
		usb_phy_set_suspend(data->phy, 0);
		usbphy_post_resume(ci, data->phy);
	}

	atomic_set(&ci->in_lpm, 0);

	if (ci->wakeup_int) {
		ci->wakeup_int = false;
		pm_runtime_put(ci->dev);
		enable_irq(ci->irq);
	}

	dev_dbg(dev, "at the end of %s\n", __func__);

	return ret;
}

static int ci13xxx_imx_runtime_suspend(struct device *dev)
{
	int ret;
	dev_dbg(dev, "at %s\n", __func__);

	ret = imx_controller_suspend(dev);

	return 0;
}

static int ci13xxx_imx_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "at %s\n", __func__);

	pm_runtime_idle(dev);

	return 0;
}

static int ci13xxx_imx_runtime_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "at %s\n", __func__);
	ret = imx_controller_resume(dev);

	return ret;
}

static int ci13xxx_imx_suspend(struct device *dev)
{
	struct ci13xxx_imx_data *data = dev_get_drvdata(dev);
	struct platform_device *plat_ci;
	struct ci13xxx *ci;
	int ret = 0;

	dev_dbg(dev, "at %s\n", __func__);

	plat_ci = data->ci_pdev;
	ci = platform_get_drvdata(plat_ci);

	ret = imx_controller_suspend(dev);
	if (ret)
		return ret;

	if (device_may_wakeup(dev)) {
		enable_irq_wake(ci->irq);
		usbphy_regulator_enable(data->phy, true);
	}

	return ret;
}

static int ci13xxx_imx_resume(struct device *dev)
{
	struct ci13xxx_imx_data *data = dev_get_drvdata(dev);
	struct platform_device *plat_ci;
	struct ci13xxx *ci;

	dev_dbg(dev, "at %s\n", __func__);

	plat_ci = data->ci_pdev;
	ci = platform_get_drvdata(plat_ci);

	if (device_may_wakeup(dev)) {
		disable_irq_wake(ci->irq);
		usbphy_regulator_enable(data->phy, false);
	}

	return imx_controller_resume(dev);
}

static const struct dev_pm_ops ci13xxx_imx_pm_ops = {
	.suspend		= ci13xxx_imx_suspend,
	.runtime_suspend	= ci13xxx_imx_runtime_suspend,
	.resume			= ci13xxx_imx_resume,
	.runtime_resume		= ci13xxx_imx_runtime_resume,
	.runtime_idle		= ci13xxx_imx_runtime_idle,
};
#endif

static struct platform_driver ci13xxx_imx_driver = {
	.probe = ci13xxx_imx_probe,
	.remove = __devexit_p(ci13xxx_imx_remove),
	.driver = {
		.name = "imx_usb",
		.owner = THIS_MODULE,
		.of_match_table = ci13xxx_imx_dt_ids,
#ifdef CONFIG_PM
		.pm	= &ci13xxx_imx_pm_ops,
#endif
	 },
};

module_platform_driver(ci13xxx_imx_driver);

MODULE_ALIAS("platform:imx-usb");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CI13xxx i.MX USB binding");
MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_AUTHOR("Richard Zhao <richard.zhao@freescale.com>");
