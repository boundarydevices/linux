// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

#include "mtk-dphy-rx-reg.h"

struct mtk_mipi_dphy;

enum mtk_mipi_dphy_port_id {
	MTK_MIPI_PHY_PORT_0 = 0x0, /* only 4D1C supported */
	MTK_MIPI_PHY_PORT_MAX_NUM
};

struct mtk_mipi_dphy_port {
	struct mtk_mipi_dphy *dev;
	enum mtk_mipi_dphy_port_id id;
	struct phy *phy;
	void __iomem *base;
};

struct mtk_mipi_dphy {
	struct device *dev;
	void __iomem *rx;
	struct mtk_mipi_dphy_port ports[MTK_MIPI_PHY_PORT_MAX_NUM];
};

static u32 mtk_dphy_read(struct mtk_mipi_dphy_port *priv, u32 reg)
{
	return readl(priv->base + reg);
}

static void mtk_dphy_write(struct mtk_mipi_dphy_port *priv, u32 reg, u32 value)
{
	writel(value, priv->base + reg);
}


static int mtk_mipi_dphy_power_on(struct phy *phy)
{
	struct mtk_mipi_dphy_port *port = phy_get_drvdata(phy);

	/* enable clock lanes */
	mtk_dphy_write(port, MIPI_RX_ANA00_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA00_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA04_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA04_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA08_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA08_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA0C_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA0C_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA10_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA10_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA20_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA20_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA24_CSI,
		       1UL | mtk_dphy_read(port, MIPI_RX_ANA24_CSI));

	mtk_dphy_write(port, MIPI_RX_ANA4C_CSI,
		       0xFEFBEFBEU & mtk_dphy_read(port, MIPI_RX_ANA4C_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA50_CSI,
		       0xFEFBEFBEU & mtk_dphy_read(port, MIPI_RX_ANA50_CSI));

	/* clock lane and lane0-lane3 input select */
	mtk_dphy_write(port, MIPI_RX_ANA00_CSI,
		       8UL | mtk_dphy_read(port, MIPI_RX_ANA00_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA04_CSI,
		       8UL | mtk_dphy_read(port, MIPI_RX_ANA04_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA08_CSI,
		       8UL | mtk_dphy_read(port, MIPI_RX_ANA08_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA0C_CSI,
		       8UL | mtk_dphy_read(port, MIPI_RX_ANA0C_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA10_CSI,
		       8UL | mtk_dphy_read(port, MIPI_RX_ANA10_CSI));

	/* BG chopper clock and CSI BG enable */
	mtk_dphy_write(port, MIPI_RX_ANA24_CSI,
		       11UL | mtk_dphy_read(port, MIPI_RX_ANA24_CSI));
	mdelay(1);

	/* LDO core bias enable */
	mtk_dphy_write(port, MIPI_RX_ANA20_CSI,
		       0xFF030003U | mtk_dphy_read(port, MIPI_RX_ANA20_CSI));
	mdelay(1);

	return 0;
}

static int mtk_mipi_dphy_power_off(struct phy *phy)
{
	struct mtk_mipi_dphy_port *port = phy_get_drvdata(phy);

	mtk_dphy_write(port, MIPI_RX_ANA00_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA00_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA04_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA04_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA08_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA08_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA0C_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA0C_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA10_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA10_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA20_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA20_CSI));
	mtk_dphy_write(port, MIPI_RX_ANA24_CSI,
		       ~1UL & mtk_dphy_read(port, MIPI_RX_ANA24_CSI));

	return 0;
}

static const struct phy_ops mtk_dphy_ops = {
	.power_on	= mtk_mipi_dphy_power_on,
	.power_off	= mtk_mipi_dphy_power_off,
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

		phy = devm_phy_create(dev, NULL, &mtk_dphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			return PTR_ERR(phy);
		}

		port->phy = phy;
		phy_set_drvdata(phy, port);
	}

	phy_provider = devm_of_phy_provider_register(dev, mtk_mipi_dphy_xlate);

	return 0;
}

static int mtk_mipi_dphy_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_mipi_dphy_of_match[] = {
	{.compatible = "mediatek,mt8167-mipi-dphy"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mipi_dphy_of_match);

static struct platform_driver mipi_dphy_pdrv = {
	.probe = mtk_mipi_dphy_probe,
	.remove = mtk_mipi_dphy_remove,
	.driver	= {
		.name	= "mtk-mipi-csi-dphy",
		.of_match_table = of_match_ptr(mtk_mipi_dphy_of_match),
	},
};

module_platform_driver(mipi_dphy_pdrv);

MODULE_DESCRIPTION("MTK mipi phy driver");
MODULE_AUTHOR("Florian Sylvestre <fsylvestre@baylibre.com>");
MODULE_LICENSE("GPL v2");