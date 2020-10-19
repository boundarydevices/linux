// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk-cdphy-rx-reg.h"

#define CSIxB_OFFSET		0x1000

struct mtk_mipi_dphy;

enum mtk_mipi_dphy_port_id {
	MTK_MIPI_PHY_PORT_0 = 0x0, /* 4D1C */
	MTK_MIPI_PHY_PORT_1, /* 4D1C */
	MTK_MIPI_PHY_PORT_2, /* 4D1C */
	MTK_MIPI_PHY_PORT_0A, /* 2D1C */
	MTK_MIPI_PHY_PORT_0B, /* 2D1C */
	MTK_MIPI_PHY_PORT_MAX_NUM
};

struct mtk_mipi_dphy_port {
	struct mtk_mipi_dphy *dev;
	enum mtk_mipi_dphy_port_id id;
	struct phy *phy;
	void __iomem *base;
	bool active;
	bool is_cdphy;
	bool is_4d1c;
};

struct mtk_mipi_dphy {
	struct device *dev;
	void __iomem *rx;
	struct mtk_mipi_dphy_port ports[MTK_MIPI_PHY_PORT_MAX_NUM];
	struct mutex lock;
};

static void mtk_mipi_phy_port_update(void __iomem *base,
				     u32 reg, u32 mask, u32 value)
{
	u32 val = readl(base + reg);
	val &= ~mask;
	val |= value;
	writel(val, base + reg);
}

#define MIPI_BITS(base, reg, field, val) \
	mtk_mipi_phy_port_update((base), reg, reg##_##field##_MASK, \
				 (val) << reg##_##field##_SHIFT)

static int mtk_mipi_phy_power_on(struct phy *phy)
{
	struct mtk_mipi_dphy_port *port = phy_get_drvdata(phy);
	struct mtk_mipi_dphy *priv = port->dev;
	void __iomem *pmipi_rx_base = port->dev->rx;
	void __iomem *pmipi_rx = port->base;
	int ret = 0;

	/* Ports CSI0 and CSI0A/B are mutually exclusive. */
	mutex_lock(&priv->lock);

	switch (port->id) {
	case MTK_MIPI_PHY_PORT_0:
		if (priv->ports[MTK_MIPI_PHY_PORT_0A].active ||
		    priv->ports[MTK_MIPI_PHY_PORT_0B].active)
			ret = -EBUSY;
		break;

	case MTK_MIPI_PHY_PORT_0A:
	case MTK_MIPI_PHY_PORT_0B:
		if (priv->ports[MTK_MIPI_PHY_PORT_0].active)
			ret = -EBUSY;
		break;

	default:
		break;
	}

	if (!ret)
		port->active = true;

	mutex_unlock(&priv->lock);

	if (ret < 0)
		return ret;

	/* Set analog phy mode to DPHY */
	if (port->is_cdphy)
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA, RG_CSI0A_CPHY_EN, 0);

	if (port->is_4d1c) {
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKSEL, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKSEL, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKMODE_EN, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKSEL, 1);
	} else {
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKSEL, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKMODE_EN, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKSEL, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKSEL, 0);
	}

	if (port->is_4d1c) {
		if (port->is_cdphy)
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
				  RG_CSI0A_CPHY_EN, 0);

		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L0_CKSEL, 1);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L1_CKSEL, 1);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKMODE_EN, 0);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_DPHY_L2_CKSEL, 1);
	}

	/* Byte clock invert */
	MIPI_BITS(pmipi_rx, MIPI_RX_ANAA8_CSIxA,
		  RG_CSIxA_CDPHY_L0_T0_BYTECK_INVERT, 1);
	MIPI_BITS(pmipi_rx, MIPI_RX_ANAA8_CSIxA,
		  RG_CSIxA_DPHY_L1_BYTECK_INVERT, 1);
	MIPI_BITS(pmipi_rx, MIPI_RX_ANAA8_CSIxA,
		  RG_CSIxA_CDPHY_L2_T1_BYTECK_INVERT, 1);

	if (port->is_4d1c) {
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANAA8_CSIxA,
			  RG_CSIxA_CDPHY_L0_T0_BYTECK_INVERT, 1);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANAA8_CSIxA,
			  RG_CSIxA_DPHY_L1_BYTECK_INVERT, 1);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANAA8_CSIxA,
			  RG_CSIxA_CDPHY_L2_T1_BYTECK_INVERT, 1);
	}

	/* Start ANA EQ tuning */
	if (port->is_cdphy) {
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI0A_L0_T0AB_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI0A_L0_T0AB_EQ_BW, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA1C_CSIxA,
			  RG_CSI0A_L1_T1AB_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA1C_CSIxA,
			  RG_CSI0A_L1_T1AB_EQ_BW, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA20_CSI0A,
			  RG_CSI0A_L2_T1BC_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA20_CSI0A,
			  RG_CSI0A_L2_T1BC_EQ_BW, 1);

		if (port->is_4d1c) {
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI0A_L0_T0AB_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI0A_L0_T0AB_EQ_BW, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA1C_CSIxA,
				  RG_CSI0A_L1_T1AB_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA1C_CSIxA,
				  RG_CSI0A_L1_T1AB_EQ_BW, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA20_CSI0A,
				  RG_CSI0A_L2_T1BC_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA20_CSI0A,
				  RG_CSI0A_L2_T1BC_EQ_BW, 1);
		}
	} else {
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI1A_L0_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI1A_L0_EQ_BW, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI1A_L1_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA18_CSIxA,
			  RG_CSI1A_L1_EQ_BW, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA1C_CSIxA,
			  RG_CSI1A_L2_EQ_IS, 1);
		MIPI_BITS(pmipi_rx, MIPI_RX_ANA1C_CSIxA,
			  RG_CSI1A_L2_EQ_BW, 1);

		if (port->is_4d1c) {
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI1A_L0_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI1A_L0_EQ_BW, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI1A_L1_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA18_CSIxA,
				  RG_CSI1A_L1_EQ_BW, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA1C_CSIxA,
				  RG_CSI1A_L2_EQ_IS, 1);
			MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA1C_CSIxA,
				  RG_CSI1A_L2_EQ_BW, 1);
		}
	}

	/* End ANA EQ tuning */
	writel(0x90, pmipi_rx_base + MIPI_RX_ANA40_CSIxA);
	MIPI_BITS(pmipi_rx, MIPI_RX_ANA24_CSIxA,
		  RG_CSIxA_RESERVE, 0x40);
	if (port->is_4d1c)
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA24_CSIxA,
			  RG_CSIxA_RESERVE, 0x40);
	MIPI_BITS(pmipi_rx, MIPI_RX_WRAPPER80_CSIxA,
		  CSR_CSI_RST_MODE, 0);
	if (port->is_4d1c)
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_WRAPPER80_CSIxA,
			  CSR_CSI_RST_MODE, 0);
	/* ANA power on */
	MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
		  RG_CSIxA_BG_CORE_EN, 1);
	if (port->is_4d1c)
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_BG_CORE_EN, 1);
	usleep_range(20, 40);
	MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
		  RG_CSIxA_BG_LPF_EN, 1);
	if (port->is_4d1c)
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_BG_LPF_EN, 1);

	return 0;
}

static int mtk_mipi_phy_power_off(struct phy *phy)
{
	struct mtk_mipi_dphy_port *port = phy_get_drvdata(phy);
	void __iomem *pmipi_rx = port->base;

	/* Disable MIPI BG. */
	MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
		  RG_CSIxA_BG_CORE_EN, 0);
	MIPI_BITS(pmipi_rx, MIPI_RX_ANA00_CSIxA,
		  RG_CSIxA_BG_LPF_EN, 0);

	if (port->is_4d1c) {
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_BG_CORE_EN, 0);
		MIPI_BITS(pmipi_rx + CSIxB_OFFSET, MIPI_RX_ANA00_CSIxA,
			  RG_CSIxA_BG_LPF_EN, 0);
	}

	mutex_lock(&port->dev->lock);
	port->active = false;
	mutex_unlock(&port->dev->lock);

	return 0;
}

static const struct phy_ops mtk_dphy_ops = {
	.power_on	= mtk_mipi_phy_power_on,
	.power_off	= mtk_mipi_phy_power_off,
	.owner		= THIS_MODULE,
};

static struct phy *mtk_mipi_dphy_xlate(struct device *dev,
				       struct of_phandle_args *args)
{
	struct mtk_mipi_dphy *priv = dev_get_drvdata(dev);

	if (args->args_count != 1)
		return ERR_PTR(-EINVAL);

	if (args->args[0] >= ARRAY_SIZE(priv->ports))
		return ERR_PTR(-ENODEV);

	return priv->ports[args->args[0]].phy;
}

static int mtk_mipi_dphy_probe(struct platform_device *pdev)
{
	static const unsigned int ports_offsets[] = {
		[MTK_MIPI_PHY_PORT_0] = 0,
		[MTK_MIPI_PHY_PORT_0A] = 0,
		[MTK_MIPI_PHY_PORT_0B] = 0x1000,
		[MTK_MIPI_PHY_PORT_1] = 0x2000,
		[MTK_MIPI_PHY_PORT_2] = 0x4000,
	};

	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct mtk_mipi_dphy *priv;
	struct resource *res;
	unsigned int i;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);
	priv->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->rx = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->rx))
		return PTR_ERR(priv->rx);

	for (i = 0; i < ARRAY_SIZE(priv->ports); ++i) {
		struct mtk_mipi_dphy_port *port = &priv->ports[i];
		struct phy *phy;

		port->dev = priv;
		port->id = i;
		port->base = priv->rx + ports_offsets[i];

		port->is_cdphy = i == MTK_MIPI_PHY_PORT_0A ||
				 i == MTK_MIPI_PHY_PORT_0B ||
				 i == MTK_MIPI_PHY_PORT_0;
		port->is_4d1c = i < MTK_MIPI_PHY_PORT_0A;

		phy = devm_phy_create(dev, NULL, &mtk_dphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			return PTR_ERR(phy);
		}

		port->phy = phy;
		phy_set_drvdata(phy, port);
	}

	mutex_init(&priv->lock);

	phy_provider = devm_of_phy_provider_register(dev, mtk_mipi_dphy_xlate);

	return 0;
}

static int mtk_mipi_dphy_remove(struct platform_device *pdev)
{
	struct mtk_mipi_dphy *priv = platform_get_drvdata(pdev);

	mutex_destroy(&priv->lock);

	return 0;
}

static const struct of_device_id mtk_mipi_dphy_of_match[] = {
	{.compatible = "mediatek,mt8183-mipi-dphy"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mipi_dphy_of_match);

static struct platform_driver mipi_dphy_pdrv = {
	.probe = mtk_mipi_dphy_probe,
	.remove = mtk_mipi_dphy_remove,
	.driver	= {
		.name	= "mtk-mipi-dphy",
		.of_match_table = of_match_ptr(mtk_mipi_dphy_of_match),
	},
};

module_platform_driver(mipi_dphy_pdrv);

MODULE_DESCRIPTION("MTK mipi dphy driver");
MODULE_AUTHOR("Louis Kuo <louis.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
