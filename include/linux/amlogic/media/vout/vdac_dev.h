/*
 * include/linux/amlogic/media/vout/vdac_dev.h
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

#ifndef _VDAC_DEV_H_
#define _VDAC_DEV_H_

enum vdac_cpu_type {
	CPU_TYPE_GXTVBB = 0,
	CPU_TYPE_GX_L_M = 1,
	CPU_TYPE_TXL  = 2,
	CPU_TYPE_TXLX  = 3,
	CPU_TYPE_GXLX  = 4,
	CPU_TYPE_TXHD = 5,
	CPU_TYPE_G12AB = 6,
	CPU_TYPE_TL1 = 7,
};

struct meson_vdac_data {
	enum vdac_cpu_type cpu_id;
	const char *name;
};

extern void vdac_set_ctrl0_ctrl1(unsigned int ctrl0, unsigned int ctrl1);

#endif
