/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/phy.h>
#include <linux/module.h>

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/netdevice.h>

#include <linux/amlogic/pm.h>
#endif
#define RTL821x_LCR		0x10
#define RTL821x_PHYSR		0x11
#define RTL821x_PHYSR_DUPLEX	0x2000
#define RTL821x_PHYSR_SPEED	0xc000
#define RTL821x_INER		0x12
#define RTL821x_INER_INIT	0x6400
#define RTL821x_INSR		0x13
#define RTL8211E_INER_LINK_STATUS 0x400

#define RTL8211F_INER_LINK_STATUS 0x0010
#define RTL8211F_INSR		0x1d
#define RTL8211F_PAGE_SELECT	0x1f
#define RTL8211F_TX_DELAY	0x100

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
unsigned int support_external_phy_wol;
#endif
static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xa43);
	err = phy_read(phydev, RTL8211F_INSR);
	/* restore to default page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	return (err < 0) ? err : 0;
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL821x_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211F_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 reg;

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	unsigned char *mac_addr = NULL;
#endif
	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd08);
	reg = phy_read(phydev, 0x11);

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		reg |= RTL8211F_TX_DELAY;
	else
		reg &= ~RTL8211F_TX_DELAY;

	phy_write(phydev, 0x11, reg);
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
	/*disable clk_out pin 35 set page 0x0a43 reg25.0 as 0*/
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0a43);
	reg = phy_read(phydev, 0x19);
	/*set reg25 bit0 as 0*/
	reg = phy_write(phydev, 0x19, reg & 0xfffe);
	/* switch to page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);
	/*reset phy to apply*/
	reg = phy_write(phydev, 0x0, 0x9200);
	/* config mac address for wol*/
	if ((phydev->attached_dev) && (support_external_phy_wol)) {
		mac_addr = phydev->attached_dev->dev_addr;
		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8c);
		phy_write(phydev, 0x10, mac_addr[0] | (mac_addr[1] << 8));
		phy_write(phydev, 0x11, mac_addr[2] | (mac_addr[3] << 8));
		phy_write(phydev, 0x12, mac_addr[4] | (mac_addr[5] << 8));
	} else {
		pr_debug("not set wol mac\n");
	}
#endif
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd04); /*set page 0xd04*/
	phy_write(phydev, RTL821x_LCR, 0XC171); /*led configuration*/

	/* restore to default page 0 */
	phy_write(phydev, RTL8211F_PAGE_SELECT, 0x0);

	return 0;
}

#ifdef CONFIG_AMLOGIC_ETH_PRIVE
int rtl8211f_suspend(struct phy_device *phydev)
{
	int value = 0;

	if (support_external_phy_wol) {
		mutex_lock(&phydev->lock);
		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8a);
		/*set magic packet for wol*/
		phy_write(phydev, 0x10, 0x1000);
		phy_write(phydev, 0x11, 0x9fff);
		/*pad isolation*/
		value = phy_read(phydev, 0x13);
		phy_write(phydev, 0x13, value | (0x1 << 15));
		/*pin 31 pull high*/
		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd40);
		value = phy_read(phydev, 0x16);
		phy_write(phydev, 0x16, value | (1 << 5));
		phy_write(phydev, RTL8211F_PAGE_SELECT, 0);

		mutex_unlock(&phydev->lock);
	} else {
		genphy_suspend(phydev);
	}
	return 0;
}

int rtl8211f_resume(struct phy_device *phydev)
{
	int value;

	if (support_external_phy_wol) {
		mutex_lock(&phydev->lock);

		phy_write(phydev, RTL8211F_PAGE_SELECT, 0xd8a);
		phy_write(phydev, 0x10, 0x0);
		/*reset wol*/
		value = phy_read(phydev, 0x11);
		phy_write(phydev, 0x11, value & ~(0x1 << 15));
		/*pad isolantion*/
		value = phy_read(phydev, 0x13);
		phy_write(phydev, 0x13, value & ~(0x1 << 15));

		phy_write(phydev, RTL8211F_PAGE_SELECT, 0);
		mutex_unlock(&phydev->lock);
	} else {
		genphy_resume(phydev);
	}
	pr_debug("%s %d\n", __func__, __LINE__);

	return 0;
}
#endif
static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
		.config_aneg    = &genphy_config_aneg,
		.read_status    = &genphy_read_status,
	}, {
		.phy_id		= 0x001cc912,
		.name		= "RTL8211B Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211b_config_intr,
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc915,
		.name		= "RTL8211E Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_aneg	= &genphy_config_aneg,
		.config_init	= &rtl8211f_config_init,
		.read_status	= &genphy_read_status,
		.ack_interrupt	= &rtl8211f_ack_interrupt,
		.config_intr	= &rtl8211f_config_intr,
#ifdef CONFIG_AMLOGIC_ETH_PRIVE
		.suspend	= rtl8211f_suspend,
		.resume		= rtl8211f_resume,
#else
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
#endif
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
