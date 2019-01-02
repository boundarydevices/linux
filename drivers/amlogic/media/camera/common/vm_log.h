/*
 * drivers/amlogic/media/camera/common/vm_log.h
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

#ifndef VM_LOG_H
#define  VM_LOG_H

#define DEBUG
#ifdef DEBUG
#define  AMLOG   1
#define LOG_LEVEL_VAR amlog_level_ppmgr
#define LOG_MASK_VAR amlog_mask_ppmgr
#endif

#define	LOG_LEVEL_HIGH		0x00f
#define	LOG_LEVEL_1				0x001
#define	LOG_LEVEL_LOW			0x000

#define LOG_LEVEL_DESC \
"[0x00]LOW[0X01]LEVEL1[0xf]HIGH"

#define	LOG_MASK_INIT			0x001
#define	LOG_MASK_IOCTL			0x002
#define	LOG_MASK_HARDWARE		0x004
#define	LOG_MASK_CONFIG		0x008
#define	LOG_MASK_WORK			0x010
#define	LOG_MASK_DESC \
"[0x01]:INIT,[0x02]:IOCTL,[0x04]:HARDWARE,[0x08]LOG_MASK_CONFIG[0x10]LOG_MASK_WORK"

#endif

