/*
 * drivers/amlogic/usb/phy/phy-aml-new-usb-v2.h
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

#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>

#define	phy_to_amlusb(x)	container_of((x), struct amlogic_usb_v2, phy)

extern int amlogic_new_usbphy_reset_v2(struct amlogic_usb_v2 *phy);
extern int amlogic_new_usbphy_reset_phycfg_v2
	(struct amlogic_usb_v2 *phy, int cnt);

