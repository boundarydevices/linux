/*
 * drivers/amlogic/media/osd/osd_prot.c
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

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/slab.h>

/* Local Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

/* Local Headers */
#include "osd_hw.h"
#include "osd_io.h"
#include "osd_reg.h"
#include "osd_prot.h"

int osd_set_prot(unsigned char   x_rev,
		 unsigned char   y_rev,
		 unsigned char   bytes_per_pixel,
		 unsigned char   conv_422to444,
		 unsigned char   little_endian,
		 unsigned int    hold_lines,
		 unsigned int    x_start,
		 unsigned int    x_end,
		 unsigned int    y_start,
		 unsigned int    y_end,
		 unsigned int    y_len_m1,
		 unsigned char   y_step,
		 unsigned char   pat_start_ptr,
		 unsigned char   pat_end_ptr,
		 unsigned long   pat_val,
		 unsigned int    canv_addr,
		 unsigned int    cid_val,
		 unsigned char   cid_mode,
		 unsigned char   cugt,
		 unsigned char   req_onoff_en,
		 unsigned int    req_on_max,
		 unsigned int    req_off_min,
		 unsigned char   osd_index,
		 unsigned char   on)
{
	unsigned long   data32;

	if (!on) {
		/* no one use prot1. */
		VSYNCOSD_CLR_MPEG_REG_MASK(VPU_PROT1_MMC_CTRL, 0xf << 12);
		VSYNCOSD_CLR_MPEG_REG_MASK(VPU_PROT1_CLK_GATE, 1 << 0);
		if (osd_index == OSD1) {
			/* switch back to little endian */
			VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0,
					1, 15, 1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_PROT_CTRL, 0);
		} else if (osd_index == OSD2) {
			/* switch back to little endian */
			VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0,
					1, 15, 1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD2_PROT_CTRL, 0);
		}
		return 0;
	}
	if (osd_index == OSD1) {
		/* bit[12..15] OSD1 OSD2 OSD3 OSD4 */
		VSYNCOSD_WR_MPEG_REG_BITS(VPU_PROT1_MMC_CTRL, 1, 12, 4);
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_PROT_CTRL, 1 << 15 | y_len_m1);
		/* before rotate set big endian */
		VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD1_BLK0_CFG_W0, 1 << 15);
	} else if (osd_index == OSD2) {
		VSYNCOSD_WR_MPEG_REG_BITS(VPU_PROT1_MMC_CTRL, 2, 12, 4);
		/* bit[12..15] OSD1 OSD2 OSD3 OSD4 */
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_PROT_CTRL, 1 << 15 | y_len_m1);
		/* before rotate set big endian */
		VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0, 1 << 15);
	}
	data32  = (x_end    << 16)  |
		  (x_start  << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_X_START_END,  data32);
	data32  = (y_end    << 16)  |
		  (y_start  << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_Y_START_END,  data32);
	data32  = (y_step   << 16)  |
		  (y_len_m1 << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_Y_LEN_STEP,   data32);
	data32  = (pat_start_ptr    << 4)   |
		  (pat_end_ptr      << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_RPT_LOOP,     data32);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_RPT_PAT,      pat_val);
	data32  = (cugt         << 20)  |
		  (cid_mode     << 16)  |
		  (cid_val      << 8)   |
		  (canv_addr    << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_DDR,          data32);
	data32  = (hold_lines       << 8)   |
		  (little_endian    << 7)   |
		  (conv_422to444    << 6)   |
		  (bytes_per_pixel  << 4)   |
		  (y_rev            << 3)   |
		  (x_rev            << 2)   |
		  (1                <<
		   0);
	/* [1:0] req_en: 0=Idle; 1=Rotate mode; 2=FIFO mode. */
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_GEN_CNTL, data32);
	data32  = (req_onoff_en << 31)  |
		  (req_off_min  << 16)  |
		  (req_on_max   << 0);
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_REQ_ONOFF, data32);
	/* Enable clock */
	VSYNCOSD_WR_MPEG_REG(VPU_PROT1_CLK_GATE, 1);
	return 0;
}
