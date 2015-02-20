/*
 * stbcfg01.h - Voltage regulator driver for the STBCFG01 driver
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_STBCFG01_H
#define __LINUX_MFD_STBCFG01_H

#include <linux/battery/sec_charging_common.h>
#include <linux/regulator/consumer.h>

struct max77823_regulator_data {
	int id;
	struct regulator_init_data *initdata;
};


/* MAX77683 regulator IDs */
enum max77823_regulators {
	MAX77823_ESAFEOUT1 = 0,
	MAX77823_ESAFEOUT2,

	MAX77823_CHARGER,

	MAX77823_REG_MAX,
};

struct max77823_platform_data {
	sec_battery_platform_data_t *charger_data;
	sec_battery_platform_data_t *fuelgauge_data;

	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	struct max77823_regulator_data *regulators;
	int num_regulators;
};

extern struct max77823_regulator_data max77823_regulators[];
extern int max77823_muic_set_safeout(int path);

#endif /*  __LINUX_MFD_MAX8998_H */
