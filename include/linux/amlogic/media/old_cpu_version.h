/*
 * include/linux/amlogic/media/old_cpu_version.h
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

#ifndef __PLAT_MESON_CPU_H_OLD
#define __PLAT_MESON_CPU_H_OLD

#include <linux/amlogic/cpu_version.h>

#define MESON_CPU_TYPE_MESON1		0x10
#define MESON_CPU_TYPE_MESON2		0x20
#define MESON_CPU_TYPE_MESON3		0x30
#define MESON_CPU_TYPE_MESON6		0x60
#define MESON_CPU_TYPE_MESON6TV		0x70
#define MESON_CPU_TYPE_MESON6TVD	0x75
#define MESON_CPU_TYPE_MESON8		0x80
#define MESON_CPU_TYPE_MESON8B		0x8B
#define MESON_CPU_TYPE_MESONG9TV	0x90

/*
 *	Read back value for P_ASSIST_HW_REV
 *
 *	Please note: M8M2 readback value same as M8 (0x19)
 *			     We changed it to 0x1D in software,
 *			     Please ALWAYS use get_meson_cpu_version()
 *			     to get the version of Meson CPU
 */
#define MESON_CPU_MAJOR_ID_M6		0x16
#define MESON_CPU_MAJOR_ID_M6TV		0x17
#define MESON_CPU_MAJOR_ID_M6TVL	0x18
#define MESON_CPU_MAJOR_ID_M8		0x19
#define MESON_CPU_MAJOR_ID_MTVD		0x1A
#define MESON_CPU_MAJOR_ID_M8B		0x1B
#define MESON_CPU_MAJOR_ID_MG9TV	0x1C
#define MESON_CPU_MAJOR_ID_M8M2		0x1D

static inline bool is_meson_m8_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8;
}

static inline bool is_meson_mtvd_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_MTVD;
}

static inline bool is_meson_m8m2_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8M2;
}

static inline bool is_meson_g9tv_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_MG9TV;
}


#endif
