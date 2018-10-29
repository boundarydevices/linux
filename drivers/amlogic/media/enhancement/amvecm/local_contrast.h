/*
 * drivers/amlogic/media/enhancement/amvecm/amcm.h
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

#ifndef __AM_LC_H
#define __AM_LC_H


#include <linux/amlogic/media/vfm/vframe.h>

enum lc_mtx_sel_e {
	INP_MTX = 0x1,
	OUTP_MTX = 0x2,
	STAT_MTX = 0x4,
	MAX_MTX
};

enum lc_mtx_csc_e {
	LC_MTX_NULL = 0,
	LC_MTX_YUV709L_RGB = 0x1,
	LC_MTX_RGB_YUV709L = 0x2,
	LC_MTX_MAX
};

extern int amlc_debug;
extern int lc_en;
extern void lc_init(void);
extern void lc_process(struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en);
#endif

