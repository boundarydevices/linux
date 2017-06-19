/*
 * drivers/amlogic/media/video_sink/amvideocap_priv.h
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

#ifndef AMVIDEOCAP_PRIV_HHH
#define AMVIDEOCAP_PRIV_HHH
#include <linux/amlogic/media/video_sink/amvideocap.h>
#include <linux/amlogic/media/vfm/vframe.h>

struct video_frame_info {
	int at_flags;
	int width;
	int height;
	int fmt;
	int width_aligned;
	int byte_per_pix;
	u64 timestamp_ms;
};

struct src_cap_rect {
	int x;
	int y;
	int width;
	int height;
};

struct amvideocap_private {
	int flags;
	int buf_size;
	unsigned long phyaddr;
	u8 *vaddr;
	enum amvideocap_state state;
	int sended_end_frame_cap_req;
	int wait_max_ms;

	struct video_frame_info src;
	struct video_frame_info want;
	struct video_frame_info out;
	struct src_cap_rect src_rect;
};

struct amvideocap_req_data {
	struct amvideocap_private *privdata;
};

struct amvideocap_req {
	int (*callback)(unsigned long data, struct vframe_s *vfput,
					int index);
	unsigned long data;
	int at_flags;		/*AT_ */
	u64 timestamp_ms;
};

s32 amvideocap_register_memory(unsigned char *phybufaddr, int phybufsize);

#endif
