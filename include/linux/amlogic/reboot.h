/*
 * include/linux/amlogic/reboot.h
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
#define	MESON_COLD_REBOOT					0
#define	MESON_NORMAL_BOOT					1
#define	MESON_FACTORY_RESET_REBOOT			2
#define	MESON_UPDATE_REBOOT					3
#define	MESON_FASTBOOT_REBOOT				4
#define	MESON_UBOOT_SUSPEND					5
#define	MESON_HIBERNATE						6
#define	MESON_BOOTLOADER_REBOOT				7
#define	MESON_RPMBP_REBOOT					9
#define MESON_QUIESCENT_REBOOT					10
#define	MESON_CRASH_REBOOT					11
#define	MESON_KERNEL_PANIC					12
#define MESON_RECOVERY_QUIESCENT_REBOOT				14
