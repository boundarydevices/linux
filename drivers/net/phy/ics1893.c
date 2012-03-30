/*
 * drivers/net/phy/ics1893.c
 *
 * Driver for ICS1893 PHY
 *
 * Author: Herbert Valerio Riedel
 *
 * Copyright (c) 2006 Herbert Valerio Riedel <hvr@gnu.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support added for SMSC LAN8187 and LAN8700 by steve.glendinning@smsc.com
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>

static int ics_config_init(struct phy_device *phydev)
{
	return 0;
}

#ifdef CONFIG_MDIO_ICS1893_FORCE10
#define FEATURES (PHY_BASIC_FEATURES) & \
	~(SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full)
#else
#define FEATURES (PHY_BASIC_FEATURES)
#endif

static struct phy_driver ics1893_driver = {
	.phy_id 	= 0x0015f450,
	.phy_id_mask	= 0xfffffff0,
	.name 		= "ICS1893",
	.features	= FEATURES,
	.flags		= 0,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.config_init	= ics_config_init,

	.suspend	= genphy_suspend,
	.resume		= genphy_resume,

	.driver		= { .owner = THIS_MODULE, }
};


static int __init ics_init(void)
{
	int ret;

	ret = phy_driver_register (&ics1893_driver);
	return ret;
}

static void __exit ics_exit(void)
{
	phy_driver_unregister (&ics1893_driver);
}

MODULE_DESCRIPTION("ICS1893 PHY driver");
MODULE_AUTHOR("Herbert Valerio Riedel");
MODULE_LICENSE("GPL");

module_init(ics_init);
module_exit(ics_exit);
