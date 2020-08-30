// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#define PCIE_PHY_CKM_REG04		0x04
#define CKM_PT0_CKTX_SLEW_MASK		GENMASK(9, 8)
#define CKM_PT0_CKTX_SLEW(val)		((val << 8) & GENMASK(9, 8))
#define CKM_PT0_CKTX_OFFSET_MASK	GENMASK(11, 10)
#define CKM_PT0_CKTX_OFFSET(val)	((val << 10) & GENMASK(11, 10))
#define CKM_PT0_CKTX_AMP_MASK		GENMASK(15, 12)
#define CKM_PT0_CKTX_AMP(val)		((val << 12) & GENMASK(15, 12))

struct mtk_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *sif_base;
	void __iomem *ckm_base;
	struct clk *pipe_clk;
};

static int mtk_pcie_phy_power_on(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret;
	u32 val;

	pm_runtime_enable(pcie_phy->dev);
	pm_runtime_get_sync(pcie_phy->dev);

	val = readl(pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);
	val &= ~GENMASK(11, 10);
	val |= CKM_PT0_CKTX_OFFSET(0x2) | CKM_PT0_CKTX_AMP(0xf);
	writel(val, pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);

	if (pcie_phy->pipe_clk) {
		ret = clk_prepare_enable(pcie_phy->pipe_clk);
		if (ret) {
			dev_err(pcie_phy->dev, "failed to enable pipe_clk\n");
			return ret;
		}
	}

	return 0;
}

static int mtk_pcie_phy_power_off(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	if (pcie_phy->pipe_clk)
		clk_disable_unprepare(pcie_phy->pipe_clk);

	pm_runtime_put_sync(pcie_phy->dev);
	pm_runtime_disable(pcie_phy->dev);

	return 0;
}

static const struct phy_ops mtk_pcie_phy_ops = {
	.power_on	= mtk_pcie_phy_power_on,
	.power_off	= mtk_pcie_phy_power_off,
	.owner		= THIS_MODULE,
};

static int mtk_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct resource *reg_res;
	struct mtk_pcie_phy *pcie_phy;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	pcie_phy->dev = dev;

	pcie_phy->pipe_clk = devm_clk_get_optional(pcie_phy->dev, "pipe_clk");
	if (IS_ERR(pcie_phy->pipe_clk)) {
		dev_warn(dev, "failed to get pipe_clk\n");
		return PTR_ERR(pcie_phy->pipe_clk);
	}

	reg_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-ckm");
	pcie_phy->ckm_base = devm_ioremap_resource(dev, reg_res);
	if (IS_ERR(pcie_phy->ckm_base)) {
		dev_warn(dev, "failed to map pcie phy base\n");
		return PTR_ERR(pcie_phy->ckm_base);
	}

	platform_set_drvdata(pdev, pcie_phy);

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &mtk_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_err(dev, "failed to create pcie phy\n");
		return PTR_ERR(pcie_phy->phy);
	}

	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id mtk_pcie_phy_id_table[] = {
	{ .compatible = "mediatek,mt8192-pcie-phy" },
	{ .compatible = "mediatek,mt8195-pcie-phy" },
	{ },
};

static struct platform_driver mtk_pcie_phy_driver = {
	.probe	= mtk_pcie_phy_probe,
	.driver	= {
		.name	= "mtk-pcie-phy",
		.of_match_table = mtk_pcie_phy_id_table,
	},
};

module_platform_driver(mtk_pcie_phy_driver);
MODULE_LICENSE("GPL v2");
