/*
 * drivers/amlogic/media/deinterlace/pulldown_drv.h
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

#ifndef _DI_PULLDOWN_H
#define _DI_PULLDOWN_H
#include "film_mode_fmw/film_vof_soft.h"
#include "deinterlace_mtn.h"

#define MAX_VOF_WIN_NUM	4

enum pulldown_mode_e {
	PULL_DOWN_BLEND_0 = 0,/* buf1=dup[0] */
	PULL_DOWN_BLEND_2 = 1,/* buf1=dup[2] */
	PULL_DOWN_MTN	  = 2,/* mtn only */
	PULL_DOWN_BUF1	  = 3,/* do wave with dup[0] */
	PULL_DOWN_EI	  = 4,/* ei only */
	PULL_DOWN_NORMAL  = 5,/* normal di */
	PULL_DOWN_NORMAL_2  = 6,/* di */
};

struct pulldown_vof_win_s {
	unsigned short win_vs;
	unsigned short win_ve;
	enum pulldown_mode_e blend_mode;
};

struct pulldown_detected_s {
	enum pulldown_mode_e global_mode;
	struct pulldown_vof_win_s regs[4];
};

unsigned char pulldown_init(unsigned short width, unsigned short height);

unsigned int pulldown_detection(struct pulldown_detected_s *res,
	struct combing_status_s *cmb_sts, bool reverse, struct vframe_s *vf);

void pulldown_vof_win_vshift(struct pulldown_detected_s *wins,
	unsigned short v_offset);

void pd_device_files_add(struct device *dev);

void pd_device_files_del(struct device *dev);
#endif
