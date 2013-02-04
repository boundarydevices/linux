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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/usb/otg.h>
#include <linux/stmp_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/usb/mxs_phy.h>

#define DRIVER_NAME "mxs_phy"

#define HW_USBPHY_CTRL				0x30
#define HW_USBPHY_CTRL_SET			0x34
#define HW_USBPHY_CTRL_CLR			0x38

#define BM_USBPHY_CTRL_SFTRST			BIT(31)
#define BM_USBPHY_CTRL_CLKGATE			BIT(30)
#define BM_USBPHY_CTRL_ENAUTOSET_USBCLKS 	BIT(26)
#define BM_USBPHY_CTRL_ENAUTOCLR_USBCLKGATE	BIT(25)
#define BM_USBPHY_CTRL_ENVBUSCHG_WKUP		BIT(23)
#define BM_USBPHY_CTRL_ENIDCHG_WKUP		BIT(22)
#define BM_USBPHY_CTRL_ENDPDMCHG_WKUP		BIT(21)
#define BM_USBPHY_CTRL_ENAUTOCLR_PHY_PWD	BIT(20)
#define BM_USBPHY_CTRL_ENAUTOCLR_CLKGATE	BIT(19)
#define BM_USBPHY_CTRL_ENAUTO_PWRON_PLL		BIT(18)
#define BM_USBPHY_CTRL_ENUTMILEVEL3		BIT(15)
#define BM_USBPHY_CTRL_ENUTMILEVEL2		BIT(14)
#define BM_USBPHY_CTRL_ENHOSTDISCONDETECT	BIT(1)

struct mxs_phy {
	struct usb_phy phy;
	struct clk *clk;
	void __iomem *anatop_base_addr;
	bool need_disconnect_line_feature;
};

#define to_mxs_phy(p) container_of((p), struct mxs_phy, phy)

static void mxs_phy_hw_init(struct mxs_phy *mxs_phy)
{
	void __iomem *base = mxs_phy->phy.io_priv;

	stmp_reset_block(base + HW_USBPHY_CTRL);

	/* Power up the PHY */
	writel_relaxed(0, base + HW_USBPHY_PWD);

	/* enable FS/LS device */
	writel_relaxed(BM_USBPHY_CTRL_ENUTMILEVEL2 |
			BM_USBPHY_CTRL_ENUTMILEVEL3,
			base + HW_USBPHY_CTRL_SET);

	/* Enable wakeup logic */
	writel_relaxed(BM_USBPHY_CTRL_ENDPDMCHG_WKUP
			| BM_USBPHY_CTRL_ENVBUSCHG_WKUP
			| BM_USBPHY_CTRL_ENIDCHG_WKUP
			| BM_USBPHY_CTRL_ENAUTOSET_USBCLKS
			| BM_USBPHY_CTRL_ENAUTOCLR_PHY_PWD
			| BM_USBPHY_CTRL_ENAUTOCLR_CLKGATE
			| BM_USBPHY_CTRL_ENAUTOCLR_USBCLKGATE
			| BM_USBPHY_CTRL_ENAUTO_PWRON_PLL , base + HW_USBPHY_CTRL_SET);
}

static int mxs_phy_init(struct usb_phy *phy)
{
	struct mxs_phy *mxs_phy = to_mxs_phy(phy);

	clk_prepare_enable(mxs_phy->clk);
	mxs_phy_hw_init(mxs_phy);

	return 0;
}

static void mxs_phy_shutdown(struct usb_phy *phy)
{
	struct mxs_phy *mxs_phy = to_mxs_phy(phy);
	void __iomem *base = mxs_phy->phy.io_priv;

	/* Power off the PHY */
	writel_relaxed(0xffffffff, base + HW_USBPHY_PWD);

	/* Disable wakeup logic */
	writel_relaxed(BM_USBPHY_CTRL_ENDPDMCHG_WKUP
			| BM_USBPHY_CTRL_ENVBUSCHG_WKUP
			| BM_USBPHY_CTRL_ENIDCHG_WKUP
			| BM_USBPHY_CTRL_ENAUTOSET_USBCLKS
			| BM_USBPHY_CTRL_ENAUTOCLR_PHY_PWD
			| BM_USBPHY_CTRL_ENAUTOCLR_CLKGATE
			| BM_USBPHY_CTRL_ENAUTOCLR_USBCLKGATE
			| BM_USBPHY_CTRL_ENAUTO_PWRON_PLL , base + HW_USBPHY_CTRL_CLR);

	writel_relaxed(BM_USBPHY_CTRL_CLKGATE,
			base + HW_USBPHY_CTRL_SET);

	clk_disable_unprepare(mxs_phy->clk);
}

static int mxs_phy_suspend(struct usb_phy *x, int suspend)
{
	struct mxs_phy *mxs_phy = to_mxs_phy(x);

	if (suspend) {
		writel_relaxed(0xffffffff, x->io_priv + HW_USBPHY_PWD);
		writel_relaxed(BM_USBPHY_CTRL_CLKGATE,
			x->io_priv + HW_USBPHY_CTRL_SET);
		clk_disable_unprepare(mxs_phy->clk);
	} else {
		clk_prepare_enable(mxs_phy->clk);
		writel_relaxed(BM_USBPHY_CTRL_CLKGATE,
			x->io_priv + HW_USBPHY_CTRL_CLR);
		writel_relaxed(0, x->io_priv + HW_USBPHY_PWD);
	}

	return 0;
}

static int mxs_phy_on_connect(struct usb_phy *phy,
		enum usb_device_speed speed)
{
	dev_dbg(phy->dev, "%s speed device has connected\n",
		(speed == USB_SPEED_HIGH) ? "high" : "non-high");

	if (speed == USB_SPEED_HIGH)
		writel_relaxed(BM_USBPHY_CTRL_ENHOSTDISCONDETECT,
				phy->io_priv + HW_USBPHY_CTRL_SET);

	return 0;
}

static int mxs_phy_on_disconnect(struct usb_phy *phy,
		enum usb_device_speed speed)
{
	dev_dbg(phy->dev, "%s speed device has disconnected\n",
		(speed == USB_SPEED_HIGH) ? "high" : "non-high");

	if (speed == USB_SPEED_HIGH)
		writel_relaxed(BM_USBPHY_CTRL_ENHOSTDISCONDETECT,
				phy->io_priv + HW_USBPHY_CTRL_CLR);

	return 0;
}

void mxs_phy_disconnect_line(struct usb_phy *phy, bool enable)
{
	struct mxs_phy *mxs_phy = to_mxs_phy(phy);
	void __iomem *anatop_base_addr = mxs_phy->anatop_base_addr;
	void __iomem *phy_reg = phy->io_priv;

	if (!mxs_phy->need_disconnect_line_feature)
		return;

	if (enable) {
		dev_dbg(phy->dev,
			"Disconnect line between phy and controller\n");
		writel_relaxed(BM_USBPHY_DEBUG_CLKGATE,
			phy_reg + HW_USBPHY_DEBUG_CLR);
		writel_relaxed(BM_ANADIG_USB1_LOOPBACK_UTMI_DIG_TST1
			| BM_ANADIG_USB1_LOOPBACK_TSTI_TX_EN,
			anatop_base_addr + HW_ANADIG_USB1_LOOPBACK);
		/* Delay some time, and let Linestate be SE0 for controller */
		usleep_range(500, 1000);
	} else {
		dev_dbg(phy->dev, "Connect line between phy and controller\n");
		writel_relaxed(0x0, anatop_base_addr + HW_ANADIG_USB1_LOOPBACK);
		writel_relaxed(BM_USBPHY_DEBUG_CLKGATE,
				phy_reg + HW_USBPHY_DEBUG_SET);
	}
}
EXPORT_SYMBOL_GPL(mxs_phy_disconnect_line);

static int mxs_phy_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *base;
	struct clk *clk;
	struct mxs_phy *mxs_phy;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get device resources\n");
		return -ENOENT;
	}

	base = devm_request_and_ioremap(dev, res);
	if (!base)
		return -EBUSY;

	clk = devm_clk_get(dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(dev,
			"can't get the clock, err=%ld", PTR_ERR(clk));
		return PTR_ERR(clk);
	}

	mxs_phy = devm_kzalloc(dev, sizeof(*mxs_phy), GFP_KERNEL);
	if (!mxs_phy) {
		dev_err(dev, "Failed to allocate USB PHY structure!\n");
		return -ENOMEM;
	}

	mxs_phy->phy.io_priv		= base;
	mxs_phy->phy.dev		= &pdev->dev;
	mxs_phy->phy.label		= DRIVER_NAME;
	mxs_phy->phy.init		= mxs_phy_init;
	mxs_phy->phy.shutdown		= mxs_phy_shutdown;
	mxs_phy->phy.set_suspend	= mxs_phy_suspend;
	mxs_phy->phy.notify_connect	= mxs_phy_on_connect;
	mxs_phy->phy.notify_disconnect	= mxs_phy_on_disconnect;

	ATOMIC_INIT_NOTIFIER_HEAD(&mxs_phy->phy.notifier);

	mxs_phy->clk = clk;

	if (of_find_property(np, "usbphy_reg_at_anatop", NULL)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res)
			return -EBUSY;
		mxs_phy->anatop_base_addr = devm_ioremap(dev, res->start, resource_size(res));
		if (!mxs_phy->anatop_base_addr)
			return -EBUSY;
	}

	if (of_find_property(np, "usbphy_need_disconnect_line_feature", NULL))
		mxs_phy->need_disconnect_line_feature = true;

	dev_set_drvdata(dev, &mxs_phy->phy);

	return 0;
}

static int mxs_phy_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mxs_phy_dt_ids[] = {
	{ .compatible = "fsl,imx23-usbphy", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxs_phy_dt_ids);

static struct platform_driver mxs_phy_driver = {
	.probe = mxs_phy_probe,
	.remove = __devexit_p(mxs_phy_remove),
	.driver = {
		.name = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mxs_phy_dt_ids,
	 },
};

static int __init mxs_phy_module_init(void)
{
	return platform_driver_register(&mxs_phy_driver);
}
postcore_initcall(mxs_phy_module_init);

static void __exit mxs_phy_module_exit(void)
{
	platform_driver_unregister(&mxs_phy_driver);
}
module_exit(mxs_phy_module_exit);

MODULE_ALIAS("platform:mxs-usb-phy");
MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_AUTHOR("Richard Zhao <richard.zhao@freescale.com>");
MODULE_DESCRIPTION("Freescale MXS USB PHY driver");
MODULE_LICENSE("GPL");
