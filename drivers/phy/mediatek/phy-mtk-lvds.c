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

#include "phy-mtk-lvds.h"

static void mtk_lvds_tx_mask(void __iomem *base_addr, u32 offset, u32 data, u32 mask)
{
	u32 temp = readl(base_addr + offset);

	writel((temp & ~mask) | (data & mask), base_addr + offset);
}

void mtk_lvds_tx_set_timing(struct phy *phy, uint32_t htt, uint32_t vtt, uint32_t fps)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);
	struct mtk_lvds_tx_clk *clk;

	if (!lvds_tx || !lvds_tx->driver_data || !lvds_tx->driver_data->clk)
		return;

	clk = lvds_tx->driver_data->clk;

	clk->htt = htt;
	clk->vtt = vtt;
	clk->fps = fps;

	pr_debug("%s clk->htt = %u, clk->vtt = %u, fps = %u\n",
		__func__, clk->htt, clk->vtt, clk->fps);
}
EXPORT_SYMBOL(mtk_lvds_tx_set_timing);

static int mtk_lvds_tx_clk_config(struct mtk_lvds_tx_clk *clk)
{
	int i;

	if (!clk)
		return -EINVAL;

	if (clk->rg_lvdstx_pll_prediv > 2)
		clk->rg_lvdstx_pll_prediv = 2;

	clk->fref = clk->fin / (1 << clk->rg_lvdstx_pll_prediv);
	if (clk->fref < 20000000 || clk->fref > 30000000) {
		pr_err("%s() Error fref = %llu out of range\r\n", __func__, clk->fref);
		return -ERANGE;
	}

	clk->pixclk = (((uint64_t)clk->htt) * clk->vtt * clk->fps * 35 / 10);

	clk->fpll = clk->pixclk * (1 << clk->rg_lvdstx_vpll_txdiv2) *
		   (1 << clk->rg_lvdstx_vpll_txdiv1) * (1 << (clk->rg_lvdstx_vpll_txmuxdiv2 + 1));

	clk->fvco = clk->fpll *
		(clk->rg_lvdstx_pll_postdiv >= 4 ? 16 : 1 << (clk->rg_lvdstx_pll_postdiv));

	for (i = 0; i <= 4 && (clk->fvco < 1500000000 || clk->fvco > 3800000000); i++) {
		clk->rg_lvdstx_pll_postdiv = i;
		clk->fvco = clk->fpll *
			(clk->rg_lvdstx_pll_postdiv >= 4 ? 16 : 1 << (clk->rg_lvdstx_pll_postdiv));
	}

	if (clk->fvco < 1500000000U || clk->fvco > 3800000000U) {
		pr_err("%s() Error fvco = %llu out of range\r\n", __func__, clk->fvco);
		return -ERANGE;
	}

	clk->rg_lvdstx_pll_sdm_pcw =
		(uint32_t)((uint64_t)clk->fvco * 16777216ULL / (uint64_t)clk->fref);
	return 0;
}

static int mtk_lvds_tx_power_on_signal(struct phy *phy)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);

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

	return 0;
}

static int mtk_lvds_tx_power_on_bused_mipi_pad(struct phy *phy)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);
	struct mtk_lvds_tx_clk *clk;

	if (!lvds_tx || !lvds_tx->driver_data || !lvds_tx->driver_data->clk)
		return -EIO;

	clk = lvds_tx->driver_data->clk;
	if (mtk_lvds_tx_clk_config(clk) != 0)
		return -EIO;

	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x01 << 0), (0x01 << 0)); /* RG_LVDSTX_BIAS_EN */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x07 << 6), (0x0f << 6)); /* RG_LVDSTX_TVCM */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x00 << 10), (0x03 << 10)); /* RG_LVDSTX_5T1LDO_SEL */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x00 << 14), (0x03 << 14)); /* RG_LVDSTX_VREF_CLKDO_SEL */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x01 << 12), (0x03 << 12)); /* RG_LVDSTX_VREF_SLDO_SEL */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x02 << 16), (0x03 << 16)); /* RG_LVDSTX_VREF_V2L_SEL */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x11 << 1), (0x1F << 1)); /* RG_LVDSTX_INTR_CAL */

	if (lvds_tx->driver_data->bused_mipi_pad) {
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x0C, (0x01 << 8), (0x01 << 8)); /* RG_DSI_PAD_TIEL_SEL */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x334, (0x00 << 0), (0x01 << 0)); /* DSI_CKN_T0B_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x330, (0x00 << 0), (0x01 << 0)); /* DSI_CKP_T0A_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x234, (0x00 << 0), (0x01 << 0)); /* DSI_D0N_T0B_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x230, (0x00 << 0), (0x01 << 0)); /* DSI_D0P_T0A_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x434, (0x00 << 0), (0x01 << 0)); /* DSI_D1N_T0B_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x430, (0x00 << 0), (0x01 << 0)); /* DSI_D1P_T0A_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x134, (0x00 << 0), (0x01 << 0)); /* DSI_D2N_T0B_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x130, (0x00 << 0), (0x01 << 0)); /* DSI_D2P_T0A_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x534, (0x00 << 0), (0x01 << 0)); /* DSI_D3N_T0B_TIEL_EN */
		mtk_lvds_tx_mask(lvds_tx->mipi_tx_regs,
				 0x530, (0x00 << 0), (0x01 << 0)); /* DSI_D3P_T0A_TIEL_EN */
		udelay(10);
	}

	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x10, (0x01 << 3), (0x01 << 3)); /* RG_LVDSTX_PLL_SDM_FRA_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x1C, (clk->rg_lvdstx_pll_sdm_pcw),
			 (0xFFFFFFFF)); /* RG_LVDSTX_PLL_SDM_PCW[31:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x10, ((clk->rg_lvdstx_pll_prediv) << 5),
			 (0x03 << 5)); /* RG_LVDSTX_PLL_PREDIV[1:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x20, (clk->rg_lvdstx_pll_postdiv << 1),
			 (0x07 << 1)); /* RG_LVDSTX_PLL_POSDIV[2:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x10, (0x00 << 4), (0x01 << 4)); /* RG_LVDSTX_PLL_GLITCH_FREE_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x10, (0x00 << 0), (0x07 << 0)); /* RG_LVDSTX_PLL_BW[2:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x20, (0x00 << 9), (0x01 << 9)); /* RG_LVDSTX_PLL_DIG_RESETB[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x20, (0x00 << 4), (0x01 << 4)); /* RG_LVDSTX_PLL_DIV3_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x20, (0x01 << 0), (0x01 << 0)); /* RG_LVDSTX_PLL_EN[0:0] */
	udelay(20);

	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x2C, (0x1F << 5), (0x1F << 5)); /* RG_LVDSTX_SLDO_EN[4:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x01 << 26), (0x01 << 26)); /* RG_LVDSTX_5T1LDO_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x28, (0x01 << 30), (0x01 << 30)); /* RG_LVDSTX_CKLDO_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x38, (0x01 << 25), (0x01 << 25)); /* RG_LVDS_VPLL_POSDIV_CK_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x38, (clk->rg_lvdstx_vpll_txmuxdiv2 << 26),
			 (0x01 << 26)); /* RG_LVDS_VPLL_TXMUXDIV2_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x40, (0x01 << 0), (0x01 << 0)); /* RG_LVDS_VPLL_LVDS_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x38, (0x00 << 27), (0x03 << 27)); /* RG_LVDS_VPLL_TXDIV1[1:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x40, (0x00 << 5), (0x01 << 5)); /* RG_LVDS_VPLL_TXDIV5_EN[0:0] */
	udelay(1);

	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x38, (clk->rg_lvdstx_vpll_txdiv1 << 27),
			 (0x03 << 27)); /* RG_LVDS_VPLL_TXDIV1[1:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x40, (0x01 << 5), (0x01 << 5)); /* RG_LVDS_VPLL_TXDIV5_EN[0:0] */
	udelay(2);

	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x2C, (0x1F << 26), (0x1F << 26)); /* RG_LVDSTX_EXT_EN[4:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x0F << 13), (0x0F << 13)); /* RG_LVDSTX_TVO[3:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x01 << 10), (0x01 << 10)); /* RG_LVDSTX_DRV_HS_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x01 << 12), (0x01 << 12)); /* RG_LVDSTX_DRV_OPMODE_EN[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x38, (0x04 << 0), (0xFFFFF << 0)); /* RG_LVDSTX_REV[0:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x18 << 0), (0x1F << 0)); /* RG_LVDSTX_TERM[4:0] */
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x01 << 11), (0x01 << 11)); /* RG_LVDSTX_TERM_EN[0:0] */
	udelay(10);
	mtk_lvds_tx_mask(lvds_tx->regs,
			 0x30, (0x1F << 5), (0x1F << 5)); /* RG_LVDSTX_DRV_EN[4:0] */

	return 0;
}

static int mtk_lvds_tx_power_on(struct phy *phy)
{
	struct mtk_lvds_tx *lvds_tx = phy_get_drvdata(phy);

	if (lvds_tx->driver_data && lvds_tx->driver_data->bused_mipi_pad)
		return mtk_lvds_tx_power_on_bused_mipi_pad(phy);
	else
		return mtk_lvds_tx_power_on_signal(phy);
}

static int mtk_lvds_tx_power_off(struct phy *phy)
{
	return 0;
}

static const struct phy_ops mtk_lvds_tx_ops = {
	.power_on = mtk_lvds_tx_power_on,
	.power_off = mtk_lvds_tx_power_off,
	.owner = THIS_MODULE,
};

static struct mtk_lvds_tx_clk mt8189_lvds_clk_cfg = {
	.fin = 26000000,
	.fref = 0,
	.fvco = 0,
	.fpll = 0,
	.pixclk = 0,
	.rg_lvdstx_pll_prediv = 0,
	.rg_lvdstx_pll_sdm_pcw = 0,
	.rg_lvdstx_vpll_txdiv2 = 0,
	.rg_lvdstx_vpll_txdiv1 = 1,
	.rg_lvdstx_vpll_txmuxdiv2 = 0,
	.rg_lvdstx_pll_postdiv = 0,
	.htt = 0,
	.vtt = 0,
	.fps = 0,
};

static const struct mtk_lvds_phy_driver_data lvds_phy_data_mt8189 = {
	.bused_mipi_pad = 1,
	.clk = &mt8189_lvds_clk_cfg,
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
		dev_err(dev, "Failed to get lvds memory resource: %d\n", ret);
		return ret;
	}

	lvds_tx->driver_data = of_device_get_match_data(dev);

	if (lvds_tx->driver_data && lvds_tx->driver_data->bused_mipi_pad) {
		mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		lvds_tx->mipi_tx_regs = devm_ioremap_resource(dev, mem);
		if (IS_ERR(lvds_tx->mipi_tx_regs)) {
			ret = PTR_ERR(lvds_tx->mipi_tx_regs);
			dev_err(dev, "Failed to get lvds mipi_tx_regs resource: %d\n", ret);
			return ret;
		}
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
	{ .compatible = "mediatek,mt8189-lvds-tx", .data = &lvds_phy_data_mt8189 },
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
