// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek LVDS PHY Driver
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Huijuan Xie <huijuan.xie@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <drm/drm_print.h>

#include "phy-mtk-lvds.h"

static void mtk_lvds_tx_mask(struct mtk_lvds_tx *lvds_tx, u32 offset, u32 mask, u32 data)
{
	u32 temp = readl(lvds_tx->regs + offset);

	writel((temp & ~mask) | (data & mask), lvds_tx->regs + offset);
}

static void mtk_lvds_tx_power_on_signal(struct phy *phy)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);
	u32 reg;

	writel(0x8053, lvds_tx->regs + VOPLL_ANA14);
	writel((0x38 << 24), lvds_tx->regs + VOPLL_ANA18);
	writel(RG_LVDSTX_BIASLPF_EN, lvds_tx->regs + LVDSTX_ANA0C);
	writel(0x3c | RG_LVDSTX_LDO1_EN, lvds_tx->regs + LVDSTX_ANA10);
	writel(0x20f87 | RG_LVDSTX_LDO_EN | RG_LVDSTX_BIAS_EN,
		lvds_tx->regs + LVDSTX_ANA04);

	writel(0x48e2, lvds_tx->regs + VOPLL_ANA1C);
	writel(0x07df | RG_LVDSTX_TSTPAD_EN | RG_LVDSTX_MPX_EN,
		lvds_tx->regs + LVDSTX_ANA08);

	writel(0x8051 | (1 << 2), lvds_tx->regs + VOPLL_ANA14);

	writel(0x48e2 | RG_LVDSTX_VPLL_SDM_PWR_ON, lvds_tx->regs + VOPLL_ANA1C);
	udelay(20);
	writel(0x8e2 | RG_LVDSTX_VPLL_SDM_PWR_ON, lvds_tx->regs + VOPLL_ANA1C);
	writel(0x8053 | (1 << 2), lvds_tx->regs + VOPLL_ANA14);
	udelay(20);

	pr_debug("Start Dump LVDS-AND\n");
	pr_debug("0x4=0x%x\n", readl(lvds_tx->regs + LVDSTX_ANA04));
	pr_debug("0x8=0x%x\n", readl(lvds_tx->regs + LVDSTX_ANA08));
	pr_debug("0xC=0x%x\n", readl(lvds_tx->regs + LVDSTX_ANA0C));
	pr_debug("0x10=0x%x\n", readl(lvds_tx->regs + LVDSTX_ANA10));
	pr_debug("0x18=0x%x\n", readl(lvds_tx->regs + VOPLL_ANA18));
	pr_debug("0x1c=0x%x\n", readl(lvds_tx->regs + VOPLL_ANA1C));
}

static int mtk_lvds_tx_power_on(struct phy *phy)
{
	mtk_lvds_tx_power_on_signal(phy);

	return 0;
}

static void mtk_lvds_tx_power_off_signal(struct phy *phy)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);
	u32 reg;

	reg = readl(lvds_tx->regs + VOPLL_ANA1C) &
		    (~(RG_LVDSTX_VPLL_SDM_PWR_ON | RG_LVDSTX_VPLL_SDM_ISO_EN));
	writel(reg, lvds_tx->regs + VOPLL_ANA1C);

	writel(0, lvds_tx->regs + VOPLL_ANA14);

	reg = readl(lvds_tx->regs + LVDSTX_ANA04) &
		    (~(RG_LVDSTX_BIAS_EN | RG_LVDSTX_LDO_EN));
	writel(reg, lvds_tx->regs + LVDSTX_ANA04);

	writel(0, lvds_tx->regs + LVDSTX_ANA08);
}

static int mtk_lvds_tx_power_off(struct phy *phy)
{
	mtk_lvds_tx_power_off_signal(phy);
	return 0;
}

static const struct phy_ops mtk_lvds_tx_ops = {
	.power_on = mtk_lvds_tx_power_on,
	.power_off = mtk_lvds_tx_power_off,
	.owner = THIS_MODULE,
};

static int mtk_lvds_tx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_lvds_tx *lvds_tx;
	struct resource *mem;
	struct phy *phy;
	struct phy_provider *phy_provider;
	int ret;

	lvds_tx = devm_kzalloc(dev, sizeof(*lvds_tx), GFP_KERNEL);
	if (lvds_tx == NULL)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lvds_tx->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(lvds_tx->regs)) {
		ret = PTR_ERR(lvds_tx->regs);
		dev_err(dev, "Failed to get lvds1 memory resource: %d\n", ret);
		return ret;
	}

	phy = devm_phy_create(dev, NULL, &mtk_lvds_tx_ops);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		dev_err(dev, "Failed to create lvds D-PHY: %d\n", ret);
		return ret;
	}
	phy_set_drvdata(phy, lvds_tx);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		ret = PTR_ERR(phy_provider);
		dev_err(dev, "Failed to phy_provider: %d\n", ret);
		return ret;
	}

	lvds_tx->dev = dev;

	return 0;
}

static int mtk_lvds_tx_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	return 0;
}

static const struct of_device_id mtk_lvds_tx_match[] = {
	{ .compatible = "mediatek,mt8365-lvds-tx" },
	{},
};

struct platform_driver mtk_lvds_tx_driver = {
	.probe = mtk_lvds_tx_probe,
	.remove = mtk_lvds_tx_remove,
	.driver = {
		.name = "mediatek-lvds-tx",
		.of_match_table = mtk_lvds_tx_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(mtk_lvds_tx_driver);

MODULE_DESCRIPTION("MediaTek LVDS PHY Driver");
MODULE_LICENSE("GPL v2");
