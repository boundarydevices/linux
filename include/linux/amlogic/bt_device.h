/*
 * include/linux/amlogic/bt_device.h
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

#ifndef __PLAT_MESON_BT_DEVICE_H
#define __PLAT_MESON_BT_DEVICE_H

struct bt_dev_data {
	int gpio_reset;
	int gpio_en;
	int power_low_level;
	int power_on_pin_OD;
};

#endif
