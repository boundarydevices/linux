/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __LINUX_REG_PFUZE_H
#define __LINUX_REG_PFUZE_H

#define PFUZE_NUMREGS		128
#define PFUZE100_SW1A		0
#define PFUZE100_SW1C		1
#define PFUZE100_SW2		2
#define PFUZE100_SW3A		3
#define PFUZE100_SW3B		4
#define PFUZE100_SW4		5
#define PFUZE100_SWBST		6
#define PFUZE100_VSNVS		7
#define PFUZE100_VREFDDR	8
#define PFUZE100_VGEN1		9
#define PFUZE100_VGEN2		10
#define PFUZE100_VGEN3		11
#define PFUZE100_VGEN4		12
#define PFUZE100_VGEN5		13
#define PFUZE100_VGEN6		14


struct regulator_init_data;
struct pfuze_regulator_init_data {
	int id;
	struct regulator_init_data *init_data;
	struct device_node *node;
};
struct pfuze_regulator_platform_data {
	int num_regulators;
	struct pfuze_regulator_init_data *regulators;
};

#endif
