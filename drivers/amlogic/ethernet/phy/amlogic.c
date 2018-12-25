/*
 * drivers/amlogic/ethernet/phy/amlogic.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/amlogic/scpi_protocol.h>

#define  SMI_ADDR_TSTWRITE    23

MODULE_DESCRIPTION("amlogic internal ethernet phy driver");
MODULE_AUTHOR("Yizhou Jiang");
MODULE_LICENSE("GPL");
void set_a3_config(struct phy_device *phydev)
{
	int value = 0;

	phy_write(phydev, 0x17, 0xa900);
	phy_write(phydev, 0x14, 0x4414);
	phy_write(phydev, 0x14, 0x8680);
	value = phy_read(phydev, 0x15);
}

void internal_wol_init(struct phy_device *phydev)
{
	int val;
	unsigned char *mac_addr;

	mac_addr = phydev->attached_dev->dev_addr;
	/*chose wol register bank*/
	val = phy_read(phydev, 0x14);
	val |= 0x800;
	val &= ~0x1000;
	phy_write(phydev, 0x14, val);/*write data to wol register bank*/
	/*write mac address*/
	phy_write(phydev, SMI_ADDR_TSTWRITE, mac_addr[5] | mac_addr[4] << 8);
	phy_write(phydev, 0x14, 0x4800 | 0x00);
	phy_write(phydev, SMI_ADDR_TSTWRITE, mac_addr[3] | mac_addr[2] << 8);
	phy_write(phydev, 0x14, 0x4800 | 0x01);
	phy_write(phydev, SMI_ADDR_TSTWRITE, mac_addr[1] | mac_addr[0] << 8);
	phy_write(phydev, 0x14, 0x4800 | 0x02);
	/*enable wol*/
	phy_write(phydev, SMI_ADDR_TSTWRITE, 0x9);
	phy_write(phydev, 0x14, 0x4800 | 0x03);
	/*enable interrupt*/
	val = phy_read(phydev, 0x1E);
	phy_write(phydev, 0x1E, val | 0xe00);
}

void internal_config(struct phy_device *phydev)
{
	int value;
	/*set reg27[12] = 1*/
	value = phy_read(phydev, 0x1b);
	phy_write(phydev, 0x1b, value | 0x1000);
	phy_write(phydev, 0x11, 0x0080);
	/*Enable Analog and DSP register Bank access by*/
	phy_write(phydev, 0x14, 0x0000);
	phy_write(phydev, 0x14, 0x0400);
	phy_write(phydev, 0x14, 0x0000);
	phy_write(phydev, 0x14, 0x0400);
	/*Write Analog register 23*/
	phy_write(phydev, 0x17, 0x8E0D);
	phy_write(phydev, 0x14, 0x4417);
	/*Enable fractional PLL*/
	phy_write(phydev, 0x17, 0x0005);
	phy_write(phydev, 0x14, 0x5C1B);
	/*Programme fraction FR_PLL_DIV1*/
	phy_write(phydev, 0x17, 0x029A);
	phy_write(phydev, 0x14, 0x5C1D);
	/*programme fraction FR_PLL_DiV1*/
	phy_write(phydev, 0x17, 0xAAAA);
	phy_write(phydev, 0x14, 0x5C1C);
	phy_write(phydev, 0x17, 0x000c);
	phy_write(phydev, 0x14, 0x4418);
	phy_write(phydev, 0x17, 0x1A0C);
	phy_write(phydev, 0x14, 0x4417); /* A6_CONFIG */
	phy_write(phydev, 0x17, 0x6400);
	phy_write(phydev, 0x14, 0x441A); /* A8_CONFIG */
	pr_info("internal phy init\n");
}
/*fetch tx_amp from uboot*/
#if 0
int myAtoi(char *str)
{
	int i = 0;
	int res = 0;

	for (i = 0; str[i] != '\0'; ++i)
		res = res*10 + str[i] - '0';
	return res;
}
#endif
unsigned int tx_amp;

module_param_named(tx_amp, tx_amp,
		uint, 0644);

static char tx_amp_str[5] = "0";
static int __init get_tx_amp(char *s)
{
	int ret = 0;

	if (s != NULL)
		sprintf(tx_amp_str, "%s", s);
	if (strcmp(tx_amp_str, "0") == 0)
		tx_amp = 0;
	else
		ret = kstrtouint(tx_amp_str, 0, &tx_amp);

	return 0;
}
__setup("tx_amp=", get_tx_amp);

void custom_internal_config(struct phy_device *phydev)
{
	unsigned int efuse_valid = 0;
	unsigned int env_valid = 0;
	unsigned int efuse_amp = 0;
	unsigned int setup_amp = 0;
	/*we will setup env tx_amp first to debug,
	 *if env tx_amp ==0 we will use the efuse
	 */
	efuse_amp = scpi_get_ethernet_calc();
	efuse_valid = (efuse_amp >> 3);
	env_valid = (tx_amp >> 7);
	if (env_valid || efuse_valid) {

		/*env valid use env tx_amp*/
		if (env_valid) {
			/*debug mode use env tx_amp*/
			setup_amp = tx_amp & (~0x80);
			pr_info("debug mode tx_amp = %d\n", setup_amp);
		} else {
			/* efuse is valid but env not*/
			setup_amp = efuse_amp;
			pr_info("use efuse tx_amp = %d\n", setup_amp);
		}
		/*Enable Analog and DSP register Bank access by*/
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x17, setup_amp);
		phy_write(phydev, 0x14, 0x4418);
		pr_info("set phy setup_amp = %d\n", setup_amp);
	} else {
		/*env not set, efuse not valid return*/
		pr_info("env not set, efuse also invalid\n");
	}
}
void reset_internal_phy(struct phy_device *phydev)
{
	int value;
	/*get value of bit 15:8*/
	/*if get 1, means power down reset or warm reset*/
	if (phydev->drv->features & 0xff00) {
		pr_info("power down and up\n");
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value | BMCR_PDOWN);
		msleep(50);
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value & ~BMCR_PDOWN);
		msleep(50);
	}

	phy_write(phydev, MII_BMCR, BMCR_RESET);
	msleep(50);
	internal_config(phydev);
	pr_info("reset phy\n");
}

unsigned int enable_wol_check = 1;
static int internal_phy_read_status(struct phy_device *phydev)
{
	int err;
	int reg31 = 0;
	int wol_reg12;
	int linkup = 0;
	int val;
	static int reg12_error_count;
	/* Update the link, but return if there was an error */
	/* Bit 15: READ*/
	/*Bit 14: Write*/
	/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
	/*Bit 10: Test Mode*/
	/*Bit 9:5: Read Address*/
	/*Bit 4:0: Write Address*/
	/*read wol bank reg12*/

	/*add enable/disable to stop the check for debug*/
	/*cause this part will involve many issue*/
	if (enable_wol_check) {
		val = ((1 << 15) | (1 << 11) | (1 << 10) | (12 << 5));
		phy_write(phydev, 0x14, val);
		wol_reg12 = phy_read(phydev, 0x15);
		if ((phydev->link) && (phydev->speed != SPEED_10)) {
			if ((wol_reg12 & 0x1000))
				reg12_error_count = 0;
			if (!(wol_reg12 & 0x1000)) {
				reg12_error_count++;
				pr_info("wol_reg12[12]==0, error\n");
			}
			if (reg12_error_count >=
					(phydev->drv->features & 0xff)) {
				reg12_error_count = 0;
				reset_internal_phy(phydev);
			}
		} else {
			reg12_error_count = 0;
		}
	}
	linkup = phydev->link;
	err = genphy_update_link(phydev);
	if (err)
		return err;

	phydev->lp_advertising = 0;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		/*read internal phy reg 0x1f*/
		reg31 = phy_read(phydev, 0x1f);
		/*bit 12 auto negotiation done*/
		if (reg31 & 0x1000) {
			phydev->pause = 0;
			phydev->asym_pause = 0;
			phydev->speed = SPEED_10;
			phydev->duplex = DUPLEX_HALF;
			/*bit 4:2 speed indication*/
			reg31 &= 0x1c;
			/*value 001: 10M/half*/
			if (reg31 == 0x4) {
				phydev->speed = SPEED_10;
				phydev->duplex = DUPLEX_HALF;
			}
			/*value 101: 10M/full*/
			if (reg31 == 0x14) {
				phydev->speed = SPEED_10;
				phydev->duplex = DUPLEX_FULL;
			}
			/*value 010: 100M/half*/
			if (reg31 == 0x8) {
				phydev->speed = SPEED_100;
				phydev->duplex = DUPLEX_HALF;
			}
			/*value 110: 100M/full*/
			if (reg31 == 0x18) {
				phydev->speed = SPEED_100;
				phydev->duplex = DUPLEX_FULL;
			}
		}
	} else {
		int bmcr = phy_read(phydev, MII_BMCR);

		if (bmcr < 0)
			return bmcr;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;

		if (bmcr & BMCR_SPEED1000)
			phydev->speed = SPEED_1000;
		else if (bmcr & BMCR_SPEED100)
			phydev->speed = SPEED_100;
		else
			phydev->speed = SPEED_10;

		phydev->pause = 0;
		phydev->asym_pause = 0;
	}
	/*every time link up, set a3 config*/
	if ((linkup == 0) && (phydev->link == 1)) {
		if (phydev->speed == SPEED_100)
			set_a3_config(phydev);
	}

	return 0;
}

static int internal_config_init(struct phy_device *phydev)
{
	internal_wol_init(phydev);
	internal_config(phydev);
	return genphy_config_init(phydev);
}

static int custom_internal_config_init(struct phy_device *phydev)
{
	custom_internal_config(phydev);
	return genphy_config_init(phydev);
}
unsigned int support_internal_phy_wol;
int internal_phy_suspend(struct phy_device *phydev)
{
	int value;
	int rc;

	/*disable link interrupt*/
	value = phy_read(phydev, 0x1E);
	phy_write(phydev, 0x1E, value & ~0x50);
	/*don't power off if wol is needed*/
	if (support_internal_phy_wol) {
		pr_info("don't power down phy\n");
		return 0;
	}

	rc = genphy_suspend(phydev);

	return rc;
}
static int internal_phy_resume(struct phy_device *phydev)
{
	int rc;

	rc = genphy_resume(phydev);
	phy_init_hw(phydev);
	return rc;
}

void internal_phy_remove(struct phy_device *phydev)
{
	int value;

	pr_info("internal phy shutdown\n");
	/*disable link interrupt*/
	value = phy_read(phydev, 0x1E);
	phy_write(phydev, 0x1E, value & ~0x50);

	value = 0;
	value = phy_read(phydev, 0x18);
	phy_write(phydev, 0x18, value | 0x1);
}
static struct phy_driver amlogic_internal_driver[] = {
{
	.phy_id	= 0x01814400,
	.name		= "amlogic internal phy",
	.phy_id_mask	= 0x0fffffff,
	.config_init	= internal_config_init,
	/*1 means power down reset, 0 means marm reset*/
	/*bit 0-7,value f:count_sec=15*/
	.features	= 0x10f,
	.config_aneg	= genphy_config_aneg,
	.read_status	= internal_phy_read_status,
	.suspend	= internal_phy_suspend,
	.resume		= internal_phy_resume,
	.remove		= internal_phy_remove,
}, {

	.phy_id	= 0x01803301,
	.name		= "custom internal phy",
	.phy_id_mask	= 0x0fffffff,
	.config_init	= custom_internal_config_init,
	/*1 means power down reset, 0 means marm reset*/
	/*bit 0-7,value f:count_sec=15*/
	.features	= 0x10f,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.remove		= internal_phy_remove,
} };

module_phy_driver(amlogic_internal_driver);

static struct mdio_device_id __maybe_unused amlogic_tbl[] = {
	{ 0x01814400, 0xfffffff0 },
	{ 0x01803301, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, amlogic_tbl);
