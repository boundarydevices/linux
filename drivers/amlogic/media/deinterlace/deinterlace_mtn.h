/*
 * drivers/amlogic/media/deinterlace/deinterlace_mtn.h
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
#ifndef _DI_MTN_H
#define _DI_MTN_H
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
struct combing_param_s {
	unsigned int width;
	unsigned int height;
	unsigned int field_idx;
	enum vframe_source_type_e src_type;
	enum tvin_sig_fmt_e fmt;
	bool prog_flag;
};

struct combing_status_s {
	unsigned int frame_diff_avg;
	unsigned int cmb_row_num;
	unsigned int field_diff_rate;
	int like_pulldown22_flag;
	unsigned int cur_level;
};

struct combing_status_s *adpative_combing_config(unsigned int width,
	unsigned int height,
	enum vframe_source_type_e src_type, bool prog,
	enum tvin_sig_fmt_e fmt);
int adaptive_combing_fixing(
	struct combing_status_s *cmb_status,
	unsigned int field_diff, unsigned int frame_diff,
	int bit_mode);
void adpative_combing_exit(void);
extern void mtn_int_combing_glbmot(void);

#endif
