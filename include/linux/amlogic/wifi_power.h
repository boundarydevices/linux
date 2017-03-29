/*
 * include/linux/amlogic/wifi_power.h
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

#ifndef __PLAT_MESON_WIFI_POWER_H
#define __PLAT_MESON_WIFI_POWER_H

struct wifi_power_platform_data {
	int power_gpio;
	int power_gpio2;
	int (*set_power)(int val);
	int (*set_reset)(int val);
	int (*set_carddetect)(int val);
	void * (*mem_prealloc)(int section, unsigned long size);
	int (*get_mac_addr)(unsigned char *buf);
	void * (*get_country_code)(char *ccode);
	void (*usb_set_power)(int val);
};

int wifi_power_control(int power_up);
extern void sdio_reinit(void);

#endif /*__PLAT_MESON_WIFI_POWER_H*/
