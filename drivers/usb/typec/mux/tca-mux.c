// SPDX-License-Identifier: GPL-2.0
/**
 * Driver for NXP TCA (Type-C Assist) USB mux control
 *
 * Copyright (c) 2023 NXP.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/usb/typec_mux.h>

#define TCA_CLK_RST			0x00
#define TCA_CLK_RST_SW			BIT(9)
#define TCA_CLK_RST_REF_CLK_EN		BIT(1)
#define TCA_CLK_RST_SUSPEND_CLK_EN	BIT(0)

#define TCA_INTR_EN			0x04
#define TCA_INTR_STS			0x08

#define TCA_GCFG			0x10
#define TCA_GCFG_ROLE_HSTDEV		BIT(4)
#define TCA_GCFG_OP_MODE		GENMASK(1, 0)
#define TCA_GCFG_OP_MODE_SYSMODE	0
#define TCA_GCFG_OP_MODE_SYNCMODE	1

#define TCA_TCPC			0x14
#define TCA_TCPC_VALID			BIT(4)
#define TCA_TCPC_LOW_POWER_EN		BIT(3)
#define TCA_TCPC_ORIENTATION_NORMAL	BIT(2)
#define TCA_TCPC_MUX_CONTRL		GENMASK(1, 0)
#define TCA_TCPC_MUX_CONTRL_NO_CONN	0
#define TCA_TCPC_MUX_CONTRL_USB_CONN	1

#define TCA_SYSMODE_CFG			0x18
#define TCA_SYSMODE_TCPC_DISABLE	BIT(3)
#define TCA_SYSMODE_TCPC_FLIP		BIT(2)

#define TCA_CTRLSYNCMODE_CFG0		0x20
#define TCA_CTRLSYNCMODE_CFG1           0x20

#define TCA_PSTATE			0x30
#define TCA_PSTATE_CM_STS		BIT(4)
#define TCA_PSTATE_TX_STS		BIT(3)
#define TCA_PSTATE_RX_PLL_STS		BIT(2)
#define TCA_PSTATE_PIPE0_POWER_DOWN	GENMASK(1, 0)

#define TCA_GEN_STATUS			0x34
#define TCA_GEN_DEV_POR			BIT(12)
#define TCA_GEN_REF_CLK_SEL		BIT(8)
#define TCA_GEN_TYPEC_FLIP_INVERT	BIT(4)
#define TCA_GEN_PHY_TYPEC_DISABLE	BIT(3)
#define TCA_GEN_PHY_TYPEC_FLIP		BIT(2)

#define TCA_VBUS_CTRL			0x40
#define TCA_VBUS_STATUS			0x44

#define TCA_INFO			0xFC

struct tca_mux {
	struct typec_switch_dev *sw;
	void __iomem *base;
	struct clk *clk;
	struct mutex mutex;
	enum typec_orientation orientation;
};

static int tca_mux_set(struct typec_switch_dev *sw,
			   enum typec_orientation orientation)
{
	struct tca_mux *tca = typec_switch_get_drvdata(sw);
	u32 val;
	int ret;

	if (tca->orientation == orientation)
		return 0;

	ret = clk_prepare_enable(tca->clk);
	if (ret)
		return ret;

	mutex_lock(&tca->mutex);

	val = readl(tca->base + TCA_SYSMODE_CFG);

	if (tca->orientation != TYPEC_ORIENTATION_NONE) {
		/* Disable TCA module */
		val |= TCA_SYSMODE_TCPC_DISABLE;
		writel(val, tca->base + TCA_SYSMODE_CFG);
		udelay(1);
	}

	tca->orientation = orientation;

	if (orientation == TYPEC_ORIENTATION_REVERSE)
		val |= TCA_SYSMODE_TCPC_FLIP;
	else if (orientation == TYPEC_ORIENTATION_NORMAL)
		val &= ~TCA_SYSMODE_TCPC_FLIP;
	else if (orientation == TYPEC_ORIENTATION_NONE)
		/* Keep at USB Safe State  */
		goto out;

	writel(val, tca->base + TCA_SYSMODE_CFG);
	udelay(1);

	/* Enable TCA module */
	val &= ~TCA_SYSMODE_TCPC_DISABLE;
	writel(val, tca->base + TCA_SYSMODE_CFG);

out:
	mutex_unlock(&tca->mutex);
	clk_disable_unprepare(tca->clk);

	return 0;
}

static int tca_mux_init(struct tca_mux *tca)
{
	int ret;
	u32 val;

	ret = clk_prepare_enable(tca->clk);
	if (ret)
		return ret;

	mutex_lock(&tca->mutex);

	/* reset XBar block */
	val = TCA_CLK_RST_SW;
	writel(val, tca->base + TCA_CLK_RST);

	val = FIELD_PREP(TCA_GCFG_OP_MODE, TCA_GCFG_OP_MODE_SYSMODE);
	writel(val, tca->base + TCA_GCFG);

	/* Put into USB Safe state */
	val |= TCA_SYSMODE_TCPC_DISABLE;
	writel(val, tca->base + TCA_SYSMODE_CFG);

	mutex_unlock(&tca->mutex);

	clk_disable_unprepare(tca->clk);

	return 0;
}

static int tca_mux_probe(struct platform_device *pdev)
{
	struct tca_mux *tca;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct typec_switch_desc sw_desc = { };

	tca = devm_kzalloc(dev, sizeof(*tca), GFP_KERNEL);
	if (!tca)
		return -ENOMEM;

	platform_set_drvdata(pdev, tca);
	mutex_init(&tca->mutex);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tca->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(tca->base))
		return PTR_ERR(tca->base);

	tca->clk = devm_clk_get(dev, "clk");
	if (IS_ERR(tca->clk)) {
		dev_err(dev, "Error get clk ret: %ld", PTR_ERR(tca->clk));
		return PTR_ERR(tca->clk);
	}

	tca_mux_init(tca);

	sw_desc.drvdata = tca;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = tca_mux_set;
	sw_desc.name = NULL;

	tca->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(tca->sw)) {
		dev_err(dev, "Error register typec switch: %ld", PTR_ERR(tca->sw));
		return PTR_ERR(tca->sw);
	}

	return 0;
}

static int tca_mux_remove(struct platform_device *pdev)
{
	struct tca_mux *tca = platform_get_drvdata(pdev);

	typec_switch_unregister(tca->sw);

	return 0;
}

static const struct of_device_id tca_mux_ids[] = {
	{ .compatible = "nxp,tca-mux" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, tca_mux_ids);

static struct platform_driver tca_mux_driver = {
	.probe		= tca_mux_probe,
	.remove		= tca_mux_remove,
	.driver		= {
		.name	= "nxp_tca_mux",
		.of_match_table = tca_mux_ids,
	},
};

module_platform_driver(tca_mux_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NXP TCA USB mux control driver");
