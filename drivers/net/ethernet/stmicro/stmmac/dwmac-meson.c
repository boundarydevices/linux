/*
 * Amlogic Meson6 and Meson8 DWMAC glue layer
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/stmmac.h>
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
#include <linux/gpio/consumer.h>
#include "dwmac1000.h"
#include "dwmac_dma.h"
#endif
#include "stmmac_platform.h"

#define ETHMAC_SPEED_100	BIT(1)

struct meson_dwmac {
	struct device	*dev;
	void __iomem	*reg;
};

static void meson6_dwmac_fix_mac_speed(void *priv, unsigned int speed)
{
#ifdef CONFIG_AMLOGIC_ETH_PRIVE

#else
	struct meson_dwmac *dwmac = priv;
	unsigned int val;
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	return;
#endif
	val = readl(dwmac->reg);

	switch (speed) {
	case SPEED_10:
		val &= ~ETHMAC_SPEED_100;
		break;
	case SPEED_100:
		val |= ETHMAC_SPEED_100;
		break;
	}

	writel(val, dwmac->reg);
#endif
}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
#define ETH_REG2_REVERSED BIT(28)
#define INTERNAL_PHY_ID 0x110181
#define PHY_ENABLE  BIT(31)
#define USE_PHY_IP  BIT(30)
#define CLK_IN_EN   BIT(29)
#define USE_PHY_MDI BIT(26)
#define LED_POLARITY  BIT(23)
#define ETH_REG3_19_RESVERD (0x9 << 16)
#define CFG_PHY_ADDR (0x8 << 8)
#define CFG_MODE (0x7 << 4)
#define CFG_EN_HIGH BIT(3)
#define ETH_REG3_2_RESERVED 0x7
static void __iomem *network_interface_setup(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct gpio_desc *gdesc;
	struct gpio_desc *gdesc_z4;
	struct gpio_desc *gdesc_z5;
	struct pinctrl *pin_ctl;
	struct resource *res;
	u32 mc_val, cali_val, internal_phy;
	void __iomem *addr = NULL;
	void __iomem *PREG_ETH_REG0;
	void __iomem *PREG_ETH_REG1;
	void __iomem *PREG_ETH_REG2;
	void __iomem *PREG_ETH_REG3;
	void __iomem *PREG_ETH_REG4;

	/*map reg0 and reg 1 addr.*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	addr = devm_ioremap_resource(dev, res);
	PREG_ETH_REG0 = addr;
	PREG_ETH_REG1 = addr + 4;
	pr_debug("REG0:REG1 = %p :%p\n", PREG_ETH_REG0, PREG_ETH_REG1);

	if (!of_property_read_u32(np, "internal_phy", &internal_phy)) {
		res = NULL;
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (res) {
		addr = devm_ioremap_resource(dev, res);
		PREG_ETH_REG2 = addr;
		PREG_ETH_REG3 = addr + 4;
		PREG_ETH_REG4 = addr + 8;
		}
		if (internal_phy == 1) {
			pr_debug("internal phy\n");
			/* Get mec mode & ting value  set it in cbus2050 */
			if (of_property_read_u32(np, "mc_val_internal_phy",
						 &mc_val)) {
			} else {
				writel(mc_val, PREG_ETH_REG0);
			}
			if (res) {
			writel(ETH_REG2_REVERSED | INTERNAL_PHY_ID,
			       PREG_ETH_REG2);
			writel(PHY_ENABLE | USE_PHY_IP | CLK_IN_EN |
					USE_PHY_MDI | LED_POLARITY |
					ETH_REG3_19_RESVERD	| CFG_PHY_ADDR |
					CFG_MODE | CFG_EN_HIGH |
					ETH_REG3_2_RESERVED, PREG_ETH_REG3);
			}
			pin_ctl = devm_pinctrl_get_select
				(&pdev->dev, "internal_eth_pins");
		} else {
			/* Get mec mode & ting value  set it in cbus2050 */
			if (of_property_read_u32(np, "mc_val_external_phy",
						 &mc_val)) {
			} else {
				writel(mc_val, PREG_ETH_REG0);
			}
			if (!of_property_read_u32(np, "cali_val", &cali_val))
				writel(cali_val, PREG_ETH_REG1);
			if (res) {
			writel(ETH_REG2_REVERSED | INTERNAL_PHY_ID,
			       PREG_ETH_REG2);
			writel(CLK_IN_EN | ETH_REG3_19_RESVERD	|
					CFG_PHY_ADDR | CFG_MODE | CFG_EN_HIGH |
					ETH_REG3_2_RESERVED, PREG_ETH_REG3);
			}
			/* pull reset pin for resetting phy  */
			gdesc = gpiod_get(&pdev->dev, "rst_pin",
					  GPIOD_FLAGS_BIT_DIR_OUT);
			gdesc_z4 = gpiod_get(&pdev->dev, "GPIOZ4_pin",
					     GPIOD_FLAGS_BIT_DIR_OUT);
			gdesc_z5 = gpiod_get(&pdev->dev, "GPIOZ5_pin",
					     GPIOD_FLAGS_BIT_DIR_OUT);
			if (!IS_ERR(gdesc) && !IS_ERR(gdesc_z4)) {
				gpiod_direction_output(gdesc_z4, 0);
				gpiod_direction_output(gdesc_z5, 0);
				gpiod_direction_output(gdesc, 0);
				mdelay(20);
				gpiod_direction_output(gdesc, 1);
				mdelay(100);
				gpiod_put(gdesc_z4);
				gpiod_put(gdesc_z5);
				pr_debug("Ethernet: gpio reset ok\n");
			}
			pin_ctl = devm_pinctrl_get_select
				(&pdev->dev, "external_eth_pins");
		}
	} else {
		pin_ctl = devm_pinctrl_get_select(&pdev->dev, "eth_pins");
	}
	pr_debug("Ethernet: pinmux setup ok\n");
	return PREG_ETH_REG0;
}
#endif
static int meson6_dwmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct meson_dwmac *dwmac;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	dwmac = devm_kzalloc(&pdev->dev, sizeof(*dwmac), GFP_KERNEL);
	if (!dwmac) {
		ret = -ENOMEM;
		goto err_remove_config_dt;
	}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	dwmac->reg = network_interface_setup(pdev);
	/* Custom initialisation (if needed) */
	if (plat_dat->init) {
		ret = plat_dat->init(pdev, plat_dat->bsp_priv);
		if (ret)
			return ret;
	}
#else
	struct resource *res;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	dwmac->reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dwmac->reg)) {
		ret = PTR_ERR(dwmac->reg);
		goto err_remove_config_dt;
	}
#endif
	plat_dat->bsp_priv = dwmac;
	plat_dat->fix_mac_speed = meson6_dwmac_fix_mac_speed;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_remove_config_dt;

	return 0;

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static const struct of_device_id meson6_dwmac_match[] = {
	{ .compatible = "amlogic,meson6-dwmac" },
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	{ .compatible = "amlogic, gxbb-eth-dwmac" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, meson6_dwmac_match);

static struct platform_driver meson6_dwmac_driver = {
	.probe  = meson6_dwmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "meson6-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = meson6_dwmac_match,
	},
};
module_platform_driver(meson6_dwmac_driver);

MODULE_AUTHOR("Beniamino Galvani <b.galvani@gmail.com>");
MODULE_DESCRIPTION("Amlogic Meson6 and Meson8 DWMAC glue layer");
MODULE_LICENSE("GPL v2");
