/*
 * drivers/amlogic/media/osd/osd_hw_def.h
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

static void osd_update_color_mode(u32 index);
static void osd_update_enable(u32 index);
static void osd_update_color_key(u32 index);
static void osd_update_color_key_enable(u32 index);
static void osd_update_gbl_alpha(u32 index);
static void osd_update_order(u32 index);
static void osd_update_disp_geometry(u32 index);
static void osd_update_coef(u32 index);
static void osd_update_disp_freescale_enable(u32 index);
static void osd_update_disp_osd_reverse(u32 index);
static void osd_update_disp_osd_rotate(u32 index);
static void osd_update_disp_scale_enable(u32 index);
static void osd_update_disp_3d_mode(u32 index);
static void osd_update_fifo(u32 index);


LIST_HEAD(update_list);
static DEFINE_SPINLOCK(osd_lock);
static unsigned long lock_flags;
#ifdef FIQ_VSYNC
static unsigned long fiq_flag;
#endif
static update_func_t hw_func_array[HW_REG_INDEX_MAX] = {
		osd_update_color_mode,
		osd_update_enable,
		osd_update_color_key,
		osd_update_color_key_enable,
		osd_update_gbl_alpha,
		osd_update_order,
		osd_update_coef,
		osd_update_disp_geometry,
		osd_update_disp_scale_enable,
		osd_update_disp_freescale_enable,
		osd_update_disp_osd_reverse,
		osd_update_disp_osd_rotate,
		osd_update_fifo,
};


#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
#ifdef FIQ_VSYNC
#define add_to_update_list(osd_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_lock, lock_flags); \
		raw_local_save_flags(fiq_flag); \
		local_fiq_disable(); \
		if (osd_hw.hw_rdma_en || osd_hw.osd_use_latch[osd_idx]) \
			osd_hw.reg[cmd_idx].update_func(osd_idx); \
		else \
			osd_hw.updated[osd_idx] |= (1<<cmd_idx); \
		raw_local_irq_restore(fiq_flag); \
		spin_unlock_irqrestore(&osd_lock, lock_flags); \
	} while (0)
#else
#define add_to_update_list(osd_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_lock, lock_flags); \
		if (osd_hw.hw_rdma_en || osd_hw.osd_use_latch[osd_idx]) \
			osd_hw.reg[cmd_idx].update_func(osd_idx); \
		else \
			osd_hw.updated[osd_idx] |= (1<<cmd_idx); \
		spin_unlock_irqrestore(&osd_lock, lock_flags); \
	} while (0)
#endif
#else
#ifdef FIQ_VSYNC
#define add_to_update_list(osd_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_lock, lock_flags); \
		raw_local_save_flags(fiq_flag); \
		local_fiq_disable(); \
		osd_hw.updated[osd_idx] |= (1<<cmd_idx); \
		raw_local_irq_restore(fiq_flag); \
		spin_unlock_irqrestore(&osd_lock, lock_flags); \
	} while (0)
#else
#define add_to_update_list(osd_idx, cmd_idx) \
	do { \
		spin_lock_irqsave(&osd_lock, lock_flags); \
		osd_hw.updated[osd_idx] |= (1<<cmd_idx); \
		spin_unlock_irqrestore(&osd_lock, lock_flags); \
	} while (0)
#endif
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
#define remove_from_update_list(osd_idx, cmd_idx) \
	do { \
		if (!osd_hw.hw_rdma_en && \
			!osd_hw.osd_use_latch[osd_idx]) \
			(osd_hw.updated[osd_idx] &= ~(1<<cmd_idx)); \
	} while (0)
#else
#define remove_from_update_list(osd_idx, cmd_idx) \
	(osd_hw.updated[osd_idx] &= ~(1<<cmd_idx))
#endif
#endif
