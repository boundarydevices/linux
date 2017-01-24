/*
 * drivers/amlogic/media/osd_ext/osd_hw_def.h
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

#ifndef _OSD_HW_DEF_H_
#define	_OSD_HW_DEF_H_

#include <linux/list.h>
#include "osd_hw.h"

static void osd1_update_color_mode(void);
static void osd1_update_enable(void);
static void osd1_update_color_key(void);
static void osd1_update_color_key_enable(void);
static void osd1_update_gbl_alpha(void);
static void osd1_update_order(void);
static void osd1_update_coef(void);
static void osd1_update_disp_geometry(void);
static void osd1_update_disp_freescale_enable(void);
static void osd1_update_disp_osd_rotate(void);
static void osd1_update_disp_scale_enable(void);
static void osd1_update_disp_3d_mode(void);

static void osd2_update_color_mode(void);
static void osd2_update_enable(void);
static void osd2_update_color_key(void);
static void osd2_update_color_key_enable(void);
static void osd2_update_gbl_alpha(void);
static void osd2_update_order(void);
static void osd2_update_coef(void);
static void osd2_update_disp_geometry(void);
static void osd2_update_disp_freescale_enable(void);
static void osd2_update_disp_osd_rotate(void);
static void osd2_update_disp_scale_enable(void);
static void osd2_update_disp_3d_mode(void);

static DEFINE_SPINLOCK(osd_ext_lock);
static struct hw_para_s osd_ext_hw;
static unsigned long lock_flags;
#ifdef FIQ_VSYNC
static unsigned long fiq_flag;
#endif
static update_func_t hw_func_array[HW_OSD_COUNT][HW_REG_INDEX_MAX] = {
	{
		osd1_update_color_mode,
		osd1_update_enable,
		osd1_update_color_key,
		osd1_update_color_key_enable,
		osd1_update_gbl_alpha,
		osd1_update_order,
		osd1_update_coef,
		osd1_update_disp_geometry,
		osd1_update_disp_scale_enable,
		osd1_update_disp_freescale_enable,
		NULL,
		osd1_update_disp_osd_rotate,
	},
	{
		osd2_update_color_mode,
		osd2_update_enable,
		osd2_update_color_key,
		osd2_update_color_key_enable,
		osd2_update_gbl_alpha,
		osd2_update_order,
		osd2_update_coef,
		osd2_update_disp_geometry,
		osd2_update_disp_scale_enable,
		osd2_update_disp_freescale_enable,
		NULL,
		osd2_update_disp_osd_rotate,
	},
};

#ifdef FIQ_VSYNC
#define add_to_update_list(osd_ext_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_ext_lock, lock_flags); \
		raw_local_save_flags(fiq_flag); \
		local_fiq_disable(); \
		osd_ext_hw.updated[osd_ext_idx] |= (1<<cmd_idx); \
		raw_local_irq_restore(fiq_flag); \
		spin_unlock_irqrestore(&osd_ext_lock, lock_flags); \
	} while (0)
#else
#define add_to_update_list(osd_ext_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_ext_lock, lock_flags); \
		osd_ext_hw.updated[osd_ext_idx] |= (1<<cmd_idx); \
		spin_unlock_irqrestore(&osd_ext_lock, lock_flags); \
	} while (0)
#endif

#define remove_from_update_list(osd_ext_idx, cmd_idx) \
	(osd_ext_hw.updated[osd_ext_idx] &= ~(1<<cmd_idx))
#endif
