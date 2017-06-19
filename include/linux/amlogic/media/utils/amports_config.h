/*
 * include/linux/amlogic/media/utils/amports_config.h
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

#ifndef AMPORTS_CONFIG_HHH
#define AMPORTS_CONFIG_HHH
#include <linux/kconfig.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>

/*
 *value seem:
 *arch\arm\plat-meson\include\plat\cpu.h
 */

#if 0
#if MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV
#define HAS_VPU_PROT  0
#define HAS_VDEC2     0
#define HAS_HEVC_VDEC 1
#define HAS_HDEC      1

#elif MESON_CPU_TYPE == MESON_CPU_TYPE_MESON8B
#define HAS_VPU_PROT  0
#define HAS_VDEC2     0
#define HAS_HEVC_VDEC 1
#define HAS_HDEC      1

#elif MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8
#define HAS_VPU_PROT  1
#define HAS_VDEC2     (IS_MESON_M8_CPU ? 1 : 0)
#define HAS_HEVC_VDEC (IS_MESON_M8_CPU ? 0 : 1)
#define HAS_HDEC      1

#elif MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD
#define HAS_VPU_PROT  0
#define HAS_VDEC2     1
#define HAS_HDEC      1
#define HAS_HEVC_VDEC 0

#else
#define HAS_VPU_PROT  0
#define HAS_VDEC2     0
#define HAS_HEVC_VDEC 0
#define HAS_HDEC      0

#endif

#ifndef CONFIG_AM_VDEC_H265
#undef HAS_HEVC_VDEC
#define HAS_HEVC_VDEC   0
#endif
#endif
#define HAS_VPU_PROT 0

/*
 *has vpu prot Later than m8;
 *except g9tv,mtvd,m8b.
 */
static inline bool has_Vpu_prot(void)
{
	if (is_meson_g9tv_cpu() || is_meson_mtvd_cpu() || is_meson_m8b_cpu())
		return 0;
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

/*
 *only mtvd,m8 has vdec2;
 *others all don't have it.
 */
static inline bool has_vdec2(void)
{
	if (is_meson_mtvd_cpu() || is_meson_m8_cpu())
		return 1;
	return 0;
}

static inline bool has_hevc_vdec(void)
{
/*
 *#ifndef CONFIG_AM_VDEC_H265
 *	return 0;
 *#endif
 */
	/*only tvd not have hevc,when later than m8 */
	if (is_meson_mtvd_cpu())
		return 0;
	else if (get_cpu_type() > MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

static inline bool has_hdec(void)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		return 1;
	return 0;
}

#endif /* AMPORTS_CONFIG_HHH */
