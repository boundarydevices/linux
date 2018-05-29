/*
 * drivers/amlogic/usb/phy/phy-aml-new-usb-v2.c
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/iomap.h>
#include "phy-aml-new-usb-v2.h"

int amlogic_new_usbphy_reset_v2(struct amlogic_usb_v2 *phy)
{
	static int	init_count;
	int i = 0;

	if (!init_count) {
		init_count++;
		if (!phy->reset_regs)
			aml_cbus_update_bits(0x1102, 0x1<<2, 0x1<<2);
		else
			writel((readl(phy->reset_regs) | (0x1 << 2)),
				phy->reset_regs);
		for (i = 0; i < 1000; i++)
			udelay(500);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_new_usbphy_reset_v2);

int amlogic_new_usbphy_reset_phycfg_v2(struct amlogic_usb_v2 *phy, int cnt)
{
	if (phy->reset_regs)
		writel((readl(phy->reset_regs) | (1 << (16 + cnt))),
			phy->reset_regs);

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_new_usbphy_reset_phycfg_v2);
