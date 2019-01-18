/*
 * include/linux/amlogic/cpu_version.h
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

/*
 * All of these api should be call after early_initcall stage
 */
#ifndef __PLAT_MESON_CPU_H
#define __PLAT_MESON_CPU_H

#define MESON_CPU_MAJOR_ID_M8B		0x1B
#define MESON_CPU_MAJOR_ID_GXBB		0x1F
#define MESON_CPU_MAJOR_ID_GXTVBB	0x20
#define MESON_CPU_MAJOR_ID_GXL		0x21
#define MESON_CPU_MAJOR_ID_GXM		0x22
#define MESON_CPU_MAJOR_ID_TXL		0x23
#define MESON_CPU_MAJOR_ID_TXLX		0x24
#define MESON_CPU_MAJOR_ID_AXG		0x25
#define MESON_CPU_MAJOR_ID_GXLX		0x26
#define MESON_CPU_MAJOR_ID_TXHD		0x27

#define MESON_CPU_MAJOR_ID_G12A		0x28
#define MESON_CPU_MAJOR_ID_G12B		0x29
#define MESON_CPU_MAJOR_ID_SM1		0x2B

#define MESON_CPU_MAJOR_ID_TL1		0x2E

#define MESON_CPU_VERSION_LVL_MAJOR	0
#define MESON_CPU_VERSION_LVL_MINOR	1
#define MESON_CPU_VERSION_LVL_PACK	2
#define MESON_CPU_VERSION_LVL_MISC	3
#define MESON_CPU_VERSION_LVL_MAX	MESON_CPU_VERSION_LVL_MISC

#define CHIPID_LEN 16
void cpuinfo_get_chipid(unsigned char *cid, unsigned int size);
int  meson_cpu_version_init(void);
#ifdef CONFIG_AMLOGIC_CPU_VERSION
int get_meson_cpu_version(int level);
int arch_big_cpu(int cpu);
#else
static inline int get_meson_cpu_version(int level)
{
	return -1;
}

static inline int arch_big_cpu(int cpu)
{
	return 0;
}
#endif

static inline int get_cpu_type(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR);
}

static inline u32 get_cpu_package(void)
{
	unsigned int pk;

	pk = get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0;
	return pk;
}

static inline bool package_id_is(unsigned int id)
{
	return get_cpu_package() == id;
}

static inline bool is_meson_m8b_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_M8B;
}

static inline bool is_meson_gxbb_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB;
}

static inline bool is_meson_gxtvbb_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB;
}

static inline bool is_meson_gxbb_package_905(void)
{
	return (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) &&
		(get_cpu_package() != 0x20);
}

static inline bool is_meson_gxbb_package_905m(void)
{
	return (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) &&
		(get_cpu_package() == 0x20);
}

static inline bool is_meson_gxl_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXL;
}

static inline bool is_meson_gxl_package_905D(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x0);
}
static inline bool is_meson_gxl_package_905X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x80);
}

static inline bool is_meson_gxl_package_905L(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xc0);
}

static inline bool is_meson_gxl_package_905M2(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xe0);
}

static inline bool is_meson_gxl_package_805X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x30);
}

static inline bool is_meson_gxl_package_805Y(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xb0);
}

static inline bool is_meson_gxm_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXM;
}

static inline bool is_meson_txl_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXL;
}

static inline bool is_meson_txl_package_950(void)
{
	return is_meson_txl_cpu() && package_id_is(0x20);
}

static inline bool is_meson_txlx_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXLX;
}

static inline bool is_meson_axg_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_AXG;
}

static inline bool is_meson_gxlx_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_GXLX;
}

static inline bool is_meson_txhd_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TXHD;
}

static inline bool is_meson_g12a_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_G12A;
}

static inline bool is_meson_g12b_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_G12B;
}

static inline bool is_meson_tl1_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_TL1;
}

static inline bool is_meson_sm1_cpu(void)
{
	return get_cpu_type() == MESON_CPU_MAJOR_ID_SM1;
}

static inline bool cpu_after_eq(unsigned int id)
{
	return get_cpu_type() >= id;
}

static inline bool is_meson_txlx_package_962X(void)
{
	return is_meson_txlx_cpu() && package_id_is(0x10);
}

static inline bool is_meson_txlx_package_962E(void)
{
	return is_meson_txlx_cpu() && package_id_is(0x20);
}

static inline bool is_meson_rev_a(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xA);
}

static inline bool is_meson_rev_b(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xB);
}

static inline bool is_meson_rev_c(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR) == 0xC);
}

#endif
