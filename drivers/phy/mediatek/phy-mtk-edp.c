// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek Embedded DisplayPort PHY driver
 *
 * Copyright (C) 2025 MediaTek Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define PHYD_OFFSET					0x0000
#define PHYD_DIG_LAN0_OFFSET		0x1000
#define PHYD_DIG_LAN1_OFFSET		0x1100
#define PHYD_DIG_LAN2_OFFSET		0x1200
#define PHYD_DIG_LAN3_OFFSET		0x1300
#define PHYD_DIG_GLB_OFFSET			0x1400

#define IPMUX_CONTROL					(PHYD_DIG_GLB_OFFSET + 0x98)
#define EDPTX_DSI_PHYD_SEL_FLDMASK			1
#define EDPTX_DSI_PHYD_SEL_FLDMASK_POS		0
#define DP_PHY_DIG_TX_CTL_0				(PHYD_DIG_GLB_OFFSET + 0x44)
#define TX_LN_EN_FLDMASK					GENMASK(7, 4)
#define MTK_DP_PHY_DIG_PLL_CTL_1		(PHYD_DIG_GLB_OFFSET + 0x14)
#define TPLL_SSC_EN							BIT(8)
#define MTK_DP_PHY_DIG_BIT_RATE			(PHYD_DIG_GLB_OFFSET + 0x3C)
#define BIT_RATE_RBR						0
#define BIT_RATE_HBR						1
#define BIT_RATE_HBR2						2
#define BIT_RATE_HBR3						3
#define MTK_DP_PHY_DIG_SW_RST			(PHYD_DIG_GLB_OFFSET + 0x38)
#define DP_GLB_SW_RST_PHYD					BIT(0)
#define DP_GLB_SW_RST_PHYD_MASK				BIT(0)

#define DRIVING_FORCE						0x18
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK		GENMASK(2, 1)
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK_POS	1
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK			GENMASK(6, 5)
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK_POS		5
#define MTK_DP_LANE0_DRIVING_PARAM_3		(PHYD_OFFSET + 0x138)
#define MTK_DP_LANE1_DRIVING_PARAM_3		(PHYD_OFFSET + 0x238)
#define MTK_DP_LANE2_DRIVING_PARAM_3		(PHYD_OFFSET + 0x338)
#define MTK_DP_LANE3_DRIVING_PARAM_3		(PHYD_OFFSET + 0x438)
#define XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT		BIT(4)
#define XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT		(BIT(10) | BIT(12))
#define XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT		GENMASK(20, 19)
#define XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT		GENMASK(29, 29)
#define DRIVING_PARAM_3_DEFAULT	(XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT		GENMASK(4, 3)
#define XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT		GENMASK(12, 9)
#define XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT		(BIT(18) | BIT(21))
#define XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT		GENMASK(29, 29)
#define DRIVING_PARAM_4_DEFAULT	(XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT		(BIT(3) | BIT(5))
#define XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT		GENMASK(13, 12)
#define DRIVING_PARAM_5_DEFAULT	(XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT		0
#define XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT		GENMASK(10, 10)
#define XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT		GENMASK(19, 19)
#define XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT		GENMASK(28, 28)
#define DRIVING_PARAM_6_DEFAULT	(XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT		0
#define XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT		GENMASK(10, 9)
#define XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT		GENMASK(19, 18)
#define XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT		0
#define DRIVING_PARAM_7_DEFAULT	(XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT		GENMASK(3, 3)
#define XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT		0
#define DRIVING_PARAM_8_DEFAULT	(XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT)

struct mtk_edp_phy {
	struct regmap *regs;
};

enum {
	DPTX_SWING0 = 0x0,
	DPTX_SWING1 = 0x1,
	DPTX_SWING2 = 0x2,
	DPTX_SWING3 = 0x3,
};

enum {
	DPTX_PREEMPHASIS0 = 0x0,
	DPTX_PREEMPHASIS1 = 0x1,
	DPTX_PREEMPHASIS2 = 0x2,
	DPTX_PREEMPHASIS3 = 0x3,
};

enum {
	DPTX_LANE0 = 0x0,
	DPTX_LANE1 = 0x1,
	DPTX_LANE2 = 0x2,
	DPTX_LANE3 = 0x3,
	DPTX_LANE_MAX,
};

enum {
	DPTX_LANE_COUNT1 = 0x1,
	DPTX_LANE_COUNT2 = 0x2,
	DPTX_LANE_COUNT4 = 0x4,
};

static u32 mtk_phy_read(struct mtk_edp_phy *edp_phy, u32 offset)
{
	u32 read_val = 0;
	int ret;

	ret = regmap_read(edp_phy->regs, offset, &read_val);
	if (ret) {
		pr_info("Failed to read register 0x%x: %d\n", offset, ret);
		return 0;
	}

	return read_val;
}

static void mtk_dptx_phyd_reset_swing_pre(struct mtk_edp_phy *edp_phy)
{
	regmap_update_bits(edp_phy->regs, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(edp_phy->regs, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(edp_phy->regs, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(edp_phy->regs, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
}
static int mtk_edp_phy_init(struct phy *phy)
{
	struct mtk_edp_phy *edp_phy = phy_get_drvdata(phy);

	/* Set IPMUX_CONTROL to eDP Mode */
	regmap_update_bits(edp_phy->regs, IPMUX_CONTROL,
						0 << EDPTX_DSI_PHYD_SEL_FLDMASK_POS,
						EDPTX_DSI_PHYD_SEL_FLDMASK);
	regmap_update_bits(edp_phy->regs, PHYD_DIG_GLB_OFFSET + 0x10,
						BIT(0) | BIT(1) | BIT(2),
						BIT(0) | BIT(1) | BIT(2));

	return 0;
}
static int mtk_edp_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mtk_edp_phy *edp_phy = phy_get_drvdata(phy);
	u32 val = 0;

	if (opts->dp.set_rate) {
		switch (opts->dp.link_rate) {
		default:
			dev_err(&phy->dev,
				"Implementation error, unknown linkrate %x\n",
				opts->dp.link_rate);
			return -EINVAL;
		case 1620:
			val = BIT_RATE_RBR;
			break;
		case 2700:
			val = BIT_RATE_HBR;
			break;
		case 5400:
			val = BIT_RATE_HBR2;
			break;
		case 8100:
			val = BIT_RATE_HBR3;
			break;
		}
		regmap_write(edp_phy->regs, MTK_DP_PHY_DIG_BIT_RATE, val);
	}
	if (opts->dp.set_lanes) {
		for (val = 0; val < 4; val++) {
			regmap_update_bits(edp_phy->regs, DP_PHY_DIG_TX_CTL_0,
				((1 << (val + 1)) - 1) << 4, TX_LN_EN_FLDMASK);
		}
	}
	/* set swing and pre */
	if (opts->dp.set_voltages) {
		if (opts->dp.lanes >= DPTX_LANE_COUNT1) {
			regmap_update_bits(edp_phy->regs,
				PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
				EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
				opts->dp.voltage[DPTX_LANE0] << 1 |
				opts->dp.pre[DPTX_LANE0] << 3);
			if (opts->dp.lanes >= DPTX_LANE_COUNT2) {
				regmap_update_bits(edp_phy->regs,
					PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
					EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
					EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
					opts->dp.voltage[DPTX_LANE1] << 1 |
					opts->dp.pre[DPTX_LANE1] << 3);
				if (opts->dp.lanes == DPTX_LANE_COUNT4) {
					regmap_update_bits(edp_phy->regs,
						PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						opts->dp.voltage[DPTX_LANE2] << 1 |
						opts->dp.pre[DPTX_LANE2] << 3);
					regmap_update_bits(edp_phy->regs,
						PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						opts->dp.voltage[DPTX_LANE3] << 1 |
						opts->dp.pre[DPTX_LANE3] << 3);
				}
			}
		}
	}
	regmap_update_bits(edp_phy->regs, MTK_DP_PHY_DIG_PLL_CTL_1,
			   TPLL_SSC_EN, opts->dp.ssc ? 0 : TPLL_SSC_EN);

	return 0;
}

static int mtk_edp_phy_reset(struct phy *phy)
{
	struct mtk_edp_phy *edp_phy = phy_get_drvdata(phy);
	unsigned int val = 0;

	regmap_read(edp_phy->regs, DP_PHY_DIG_TX_CTL_0, &val);
	val = val & TX_LN_EN_FLDMASK;
	regmap_update_bits(edp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
			   0, DP_GLB_SW_RST_PHYD_MASK);
	usleep_range(50, 200);
	regmap_update_bits(edp_phy->regs, MTK_DP_PHY_DIG_SW_RST,
					   DP_GLB_SW_RST_PHYD, DP_GLB_SW_RST_PHYD_MASK);
	regmap_update_bits(edp_phy->regs, DP_PHY_DIG_TX_CTL_0, val << 4, TX_LN_EN_FLDMASK);
	mtk_dptx_phyd_reset_swing_pre(edp_phy);

	return 0;
}

static const struct phy_ops mtk_edp_phy_dev_ops = {
	.init = mtk_edp_phy_init,
	.configure = mtk_edp_phy_configure,
	.reset = mtk_edp_phy_reset,
	.owner = THIS_MODULE,
};

static int mtk_edp_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_edp_phy *edp_phy;
	struct phy *phy;
	struct regmap *regs;

	regs = *(struct regmap **)dev->platform_data;
	if (!regs)
		return dev_err_probe(dev, -EINVAL,
				     "No data passed, requires struct regmap**\n");

	edp_phy = devm_kzalloc(dev, sizeof(*edp_phy), GFP_KERNEL);
	if (!edp_phy)
		return -ENOMEM;
	edp_phy->regs = regs;
	phy = devm_phy_create(dev, NULL, &mtk_edp_phy_dev_ops);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy),
				     "Failed to create eDP PHY\n");

	phy_set_drvdata(phy, edp_phy);
	if (!dev->of_node)
		phy_create_lookup(phy, "edp", dev_name(dev));

	return 0;
}

static struct platform_driver mtk_edp_phy_driver = {
	.probe = mtk_edp_phy_probe,
	.driver = {
		.name = "mediatek-edp-phy",
	},
};
module_platform_driver(mtk_edp_phy_driver);

MODULE_AUTHOR("Jie-h.Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek Embedded DisplayPort PHY Driver");
MODULE_LICENSE("GPL");
