/*
 * include/linux/amlogic/ramdump.h
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

#ifndef __RAMDUMP_H__
#define __RAMDUMP_H__

#define SET_REBOOT_REASON		0x82000049

extern int ramdump_disabled(void);
extern void ramdump_sync_data(void);

#endif /* __RAMDUMP_H__ */
