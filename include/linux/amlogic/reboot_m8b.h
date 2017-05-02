#ifndef __reboot_m8b_h_
#define __reboot_m8b_h_
/*
 * include/linux/amlogic/reboot_m8b.h
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

/*******************************************************************/
#define	MESON_CHARGING_REBOOT					0x0
#define	MESON_NORMAL_BOOT				0x01010101
#define	MESON_FACTORY_RESET_REBOOT		0x02020202
#define	MESON_UPDATE_REBOOT				0x03030303
#define	MESON_CRASH_REBOOT				0x04040404
#define	MESON_FACTORY_TEST_REBOOT		0x05050505
#define	MESON_SYSTEM_SWITCH_REBOOT		0x06060606
#define	MESON_SAFE_REBOOT				0x07070707
#define	MESON_LOCK_REBOOT				0x08080808
#define	MESON_USB_BURNER_REBOOT			0x09090909
#define MESON_UBOOT_SUSPEND				0x0b0b0b0b
#define	MESON_REBOOT_CLEAR				0xdeaddead
#endif
