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
#include <linux/bitops.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include "dwmac1000.h"
#include "dwmac_dma.h"
#endif
#include "stmmac_platform.h"

#define ETHMAC_SPEED_10	BIT(1)

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
/*if not g12a use genphy driver*/
/* if it's internal phy we will shutdown analog*/
static unsigned int is_internal_phy;
/* Ethernet register for G12A PHY */
#define ETH_PLL_CTL0 0x44
#define ETH_PLL_CTL1 0x48
#define ETH_PLL_CTL2 0x4C
#define ETH_PLL_CTL3 0x50
#define ETH_PLL_CTL4 0x54
#define ETH_PLL_CTL5 0x58
#define ETH_PLL_CTL6 0x5C
#define ETH_PLL_CTL7 0x60

#define ETH_PHY_CNTL0 0x80
#define ETH_PHY_CNTL1 0x84
#define ETH_PHY_CNTL2 0x88

#define	ETH_USE_EPHY BIT(5)
#define	ETH_EPHY_FROM_MAC BIT(6)

struct meson_dwmac_data {
	bool g12a_phy;
};
#endif

struct meson_dwmac {
	struct device	*dev;
	void __iomem	*reg;
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	const struct meson_dwmac_data *data;
#endif
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

/*these two store the define of wol in dts*/
extern unsigned int support_internal_phy_wol;
extern unsigned int support_external_phy_wol;
static unsigned int support_mac_wol;
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
	void __iomem *PREG_ETH_REG0 = NULL;
	void __iomem *PREG_ETH_REG1 = NULL;
	void __iomem *PREG_ETH_REG2 = NULL;
	void __iomem *PREG_ETH_REG3 = NULL;
	void __iomem *PREG_ETH_REG4 = NULL;

	/*map reg0 and reg 1 addr.*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get resource(%d)\n", __LINE__);
		return NULL;
	}

	addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(addr)) {
		dev_err(&pdev->dev, "Unable to map base (%d)\n", __LINE__);
		return NULL;
	}

	PREG_ETH_REG0 = addr;
	PREG_ETH_REG1 = addr + 4;
	pr_debug("REG0:REG1 = %p :%p\n", PREG_ETH_REG0, PREG_ETH_REG1);

	if (!of_property_read_u32(np, "internal_phy", &internal_phy)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (res) {
		addr = devm_ioremap_resource(dev, res);

		if (IS_ERR(addr)) {
			dev_err(&pdev->dev, "Unable to map %d\n", __LINE__);
			return NULL;
		}

		PREG_ETH_REG2 = addr;
		PREG_ETH_REG3 = addr + 4;
		PREG_ETH_REG4 = addr + 8;
		}
		if (internal_phy == 1) {
			pr_debug("internal phy\n");
			/*merge wol from 3.14 start*/
			if (of_property_read_u32(np,
						"wol",
						&support_internal_phy_wol))
				pr_debug("wol not set\n");
			else
				pr_debug("Ethernet :got wol %d .set it\n",
						support_internal_phy_wol);
			/*merge wol from 3.14 end*/

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

static int dwmac_meson_cfg_ctrl(void __iomem *base_addr)
{
	void __iomem *ETH_PHY_config_addr = base_addr;

	/*config phyid should between  a 0~0xffffffff*/
	/*please don't use 44000181, this has been used by internal phy*/
	writel(0x33000180, ETH_PHY_config_addr + ETH_PHY_CNTL0);

	/*use_phy_smi | use_phy_ip | co_clkin from eth_phy_top*/
	writel(0x260, ETH_PHY_config_addr + ETH_PHY_CNTL2);
	/*led signal is inverted*/
	writel(0x41054147, ETH_PHY_config_addr + ETH_PHY_CNTL1);
	writel(0x41014147, ETH_PHY_config_addr + ETH_PHY_CNTL1);
	writel(0x41054147, ETH_PHY_config_addr + ETH_PHY_CNTL1);
	/*wait phy to reset cause Power Up Reset need 5.2~2.6 ms*/
	mdelay(10);
	return 0;
}

static int dwmac_meson_cfg_pll(void __iomem *base_addr,
			       struct platform_device *pdev)
{
	void __iomem *ETH_PHY_config_addr = base_addr;
	u32 pll_val[3] = {0};

	of_property_read_u32_array(pdev->dev.of_node, "pll_val",
				   pll_val, sizeof(pll_val) / sizeof(u32));
	pr_info("wzh pll %x %x %x", pll_val[0], pll_val[1], pll_val[2]);

	writel(pll_val[0] | 0x30000000, ETH_PHY_config_addr + ETH_PLL_CTL0);
	writel(pll_val[1], ETH_PHY_config_addr + ETH_PLL_CTL1);
	writel(pll_val[2], ETH_PHY_config_addr + ETH_PLL_CTL2);
	writel(0x00000000, ETH_PHY_config_addr + ETH_PLL_CTL3);
	usleep_range(100, 200);
	writel(pll_val[0] | 0x10000000, ETH_PHY_config_addr + ETH_PLL_CTL0);
	return 0;
}

static int dwmac_meson_cfg_analog(void __iomem *base_addr,
				  struct platform_device *pdev)
{
	void __iomem *ETH_PHY_config_addr = base_addr;
	u32 analog_val[3] = {0};

	of_property_read_u32_array
				   (pdev->dev.of_node, "analog_val",
				   analog_val,
				   sizeof(analog_val) / sizeof(u32));
	pr_info("wzh analog %x %x %x", analog_val[0],
		analog_val[1], analog_val[2]);
	/*Analog*/
	writel(analog_val[0], ETH_PHY_config_addr + ETH_PLL_CTL5);
	writel(analog_val[1], ETH_PHY_config_addr + ETH_PLL_CTL6);
	writel(analog_val[2], ETH_PHY_config_addr + ETH_PLL_CTL7);

	return 0;
}

/*for newer then g12a use this dts architecture for dts*/
void __iomem *phy_analog_config_addr;
static void __iomem *g12a_network_interface_setup(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct pinctrl *pin_ctl;
	struct resource *res = NULL;
	u32 mc_val;
	void __iomem *addr = NULL;
	void __iomem *REG_ETH_reg0_addr = NULL;
	void __iomem *ETH_PHY_config_addr = NULL;
	u32 internal_phy = 0;
	is_internal_phy = 0;

	pr_debug("g12a_network_interface_setup\n");
	/*map PRG_ETH_REG */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "eth_cfg");
	if (!res) {
		dev_err(&pdev->dev, "Unable to get resource(%d)\n", __LINE__);
		return NULL;
	}

	addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(addr)) {
		dev_err(&pdev->dev, "Unable to map base (%d)\n", __LINE__);
		return NULL;
	}

	REG_ETH_reg0_addr = addr;
	pr_info(" REG0:Addr = %p\n", REG_ETH_reg0_addr);

	/*map ETH_PLL address*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "eth_pll");
	if (!res) {
		dev_err(&pdev->dev, "Unable to get resource(%d)\n", __LINE__);
		return NULL;
	}

	addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(addr)) {
		dev_err(&pdev->dev, "Unable to map clk base (%d)\n", __LINE__);
		return NULL;
	}

	ETH_PHY_config_addr = addr;
	phy_analog_config_addr = addr;
	/*PRG_ETH_REG0*/
	if (of_property_read_u32(np, "mc_val", &mc_val))
		pr_info("Miss mc_val for REG0\n");
	else
		writel(mc_val, REG_ETH_reg0_addr);

	/*read phy option*/
	if (of_property_read_u32(np, "internal_phy", &internal_phy) != 0) {
		pr_info("Dts miss internal_phy item\n");
		return REG_ETH_reg0_addr;
	}

	is_internal_phy = internal_phy;
	/* Config G12A internal PHY */
	if (internal_phy) {
		/*mac wol*/
		if (of_property_read_u32(np, "mac_wol",
					 &support_mac_wol))
			pr_info("MAC wol not set\n");
		else
			pr_info("MAC wol :got wol %d .set it\n",
				support_mac_wol);
		/*PLL*/
		dwmac_meson_cfg_pll(ETH_PHY_config_addr, pdev);
		dwmac_meson_cfg_analog(ETH_PHY_config_addr, pdev);
		dwmac_meson_cfg_ctrl(ETH_PHY_config_addr);
		pin_ctl = devm_pinctrl_get_select
			(&pdev->dev, "internal_eth_pins");
		return REG_ETH_reg0_addr;
	}

	/*config extern phy*/
	if (internal_phy == 0) {
		/* only exphy support wol since g12a*/
		/*we enable/disable wol with item in dts with "wol=<1>"*/
		if (of_property_read_u32(np, "wol",
						&support_external_phy_wol))
			pr_debug("exphy wol not set\n");
		else
			pr_debug("exphy Ethernet :got wol %d .set it\n",
						support_external_phy_wol);

		/*switch to extern phy*/
		writel(0x0, ETH_PHY_config_addr + ETH_PHY_CNTL2);
		pin_ctl = devm_pinctrl_get_select
			(&pdev->dev, "external_eth_pins");
		return REG_ETH_reg0_addr;
	}

	pr_info("should not happen\n");
	return REG_ETH_reg0_addr;
}

static int dwmac_meson_disable_analog(struct device *dev)
{
	writel(0x00000000, phy_analog_config_addr + 0x0);
	writel(0x003e0000, phy_analog_config_addr + 0x4);
	writel(0x12844008, phy_analog_config_addr + 0x8);
	writel(0x0800a40c, phy_analog_config_addr + 0xc);
	writel(0x00000000, phy_analog_config_addr + 0x10);
	writel(0x031d161c, phy_analog_config_addr + 0x14);
	writel(0x00001683, phy_analog_config_addr + 0x18);
	writel(0x09c0040a, phy_analog_config_addr + 0x44);
	return 0;
}

static int dwmac_meson_recover_analog(struct device *dev)
{
	pr_info("recover %p\n", phy_analog_config_addr);

	writel(0x19c0040a, phy_analog_config_addr + 0x44);
	writel(0x0, phy_analog_config_addr + 0x4);
	return 0;
}

static int meson6_dwmac_suspend(struct device *dev)
{
	int ret;
	/*shudown internal phy analog*/
	struct pinctrl *pin_ctrl;
	struct pinctrl_state *turnoff_tes = NULL;

	/*shudown internal phy analog*/
	pr_info("suspend inter = %d\n", is_internal_phy);
	if ((is_internal_phy) && (support_mac_wol == 0)) {
		/*turn off led*/
		pin_ctrl = devm_pinctrl_get(dev);
		if (IS_ERR_OR_NULL(pin_ctrl)) {
		/*led will not work*/
			pr_info("pinctrl is null\n");
		} else {
			turnoff_tes = pinctrl_lookup_state
					(pin_ctrl, "internal_gpio_pins");
			if (IS_ERR_OR_NULL(turnoff_tes))
				pr_info("Not support gpio low\n");
			else
				pinctrl_select_state(pin_ctrl, turnoff_tes);

			devm_pinctrl_put(pin_ctrl);
			pin_ctrl = NULL;
		}
		dwmac_meson_disable_analog(dev);
	}
	ret = stmmac_pltfr_suspend(dev);

	return ret;
}
static int meson6_dwmac_resume(struct device *dev)
{
	int ret;
	struct pinctrl *pin_ctrl;
	struct pinctrl_state *turnon_tes = NULL;
	pr_info("resuem inter = %d\n", is_internal_phy);
	if ((is_internal_phy) && (support_mac_wol == 0)) {
		pin_ctrl = devm_pinctrl_get(dev);
		if (IS_ERR_OR_NULL(pin_ctrl)) {
			pr_info("pinctrl is null\n");
		} else {
			turnon_tes = pinctrl_lookup_state
					(pin_ctrl, "internal_eth_pins");
			pinctrl_select_state(pin_ctrl, turnon_tes);
			devm_pinctrl_put(pin_ctrl);
			pin_ctrl = NULL;
		}
		dwmac_meson_recover_analog(dev);
	}
	ret = stmmac_pltfr_resume(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(meson6_dwmac_resume);

void meson6_dwmac_shutdown(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct pinctrl *pin_ctrl;
	struct pinctrl_state *turnoff_tes = NULL;

	/*shudown internal phy analog*/
	if (is_internal_phy) {
		pin_ctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR_OR_NULL(pin_ctrl)) {
			pr_info("pinctrl is null\n");
		} else {
			turnoff_tes = pinctrl_lookup_state
					(pin_ctrl, "internal_gpio_pins");
			if (IS_ERR_OR_NULL(turnoff_tes))
				pr_info("Not support gpio low\n");
			else
				pinctrl_select_state(pin_ctrl, turnoff_tes);

			//pinctrl_select_state(pin_ctrl, turnoff_tes);
			devm_pinctrl_put(pin_ctrl);
			pin_ctrl = NULL;
		}
		dwmac_meson_disable_analog(&pdev->dev);
	}
	//stmmac_release(ndev);
	//stmmac_pltfr_suspend(&pdev->dev);
	if (priv->phydev) {
		if (priv->phydev->drv->remove)
			priv->phydev->drv->remove(ndev->phydev);
		else{
			pr_info("gen_suspend\n");
			genphy_suspend(ndev->phydev);
		}
	}
	stmmac_pltfr_suspend(&pdev->dev);
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
	dwmac->data = (const struct meson_dwmac_data *)
			of_device_get_match_data(&pdev->dev);

	if (dwmac->data->g12a_phy)
		dwmac->reg = g12a_network_interface_setup(pdev);
	else
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
	if (support_mac_wol)
		device_init_wakeup(&pdev->dev, 1);
	return 0;

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
static const struct meson_dwmac_data gxbb_dwmac_data = {
	.g12a_phy	= false,
};

static const struct meson_dwmac_data g12a_dwmac_data = {
	.g12a_phy	= true,
};
#endif
static const struct of_device_id meson6_dwmac_match[] = {
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	{
		.compatible	= "amlogic, meson6-dwmac",
		.data		= &gxbb_dwmac_data,
	},
	{
		.compatible	= "amlogic, gxbb-eth-dwmac",
		.data		= &gxbb_dwmac_data,
	},
	{
		.compatible	= "amlogic, g12a-eth-dwmac",
		.data		= &g12a_dwmac_data,
	},
#else
	{ .compatible = "amlogic,meson6-dwmac" },
	{ .compatible = "amlogic, gxbb-eth-dwmac" },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, meson6_dwmac_match);

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
SIMPLE_DEV_PM_OPS(stmmac_pltfr_pm_ops, meson6_dwmac_suspend,
		  meson6_dwmac_resume);
#endif
static struct platform_driver meson6_dwmac_driver = {
	.probe  = meson6_dwmac_probe,
	.remove = stmmac_pltfr_remove,
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	.shutdown = meson6_dwmac_shutdown,
#endif
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
