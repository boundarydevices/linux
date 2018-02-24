/*
 * drivers/amlogic/media/common/ge2d/ge2dgen.h
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

#ifndef _GE2DGEN_H_
#define _GE2DGEN_H_

void ge2dgen_src(struct ge2d_context_s *wq,
		unsigned int canvas_addr,
		unsigned int format,
		unsigned int phy_addr,
		unsigned int stride);

void ge2dgen_post_release_src1buf(struct ge2d_context_s *wq,
	unsigned int buffer);

void ge2dgen_post_release_src1canvas(struct ge2d_context_s *wq);

void ge2dgen_post_release_src2buf(struct ge2d_context_s *wq,
	unsigned int buffer);

void ge2dgen_post_release_src2canvas(struct ge2d_context_s *wq);

void ge2dgen_src2(struct ge2d_context_s *wq,
		unsigned int canvas_addr,
		unsigned int format,
		unsigned int phy_addr,
		unsigned int stride);

void ge2dgen_src2_clip(struct ge2d_context_s *wq,
		       int x, int y, int w, int h);
void ge2dgen_antiflicker(struct ge2d_context_s *wq, unsigned long enable);
void ge2dgen_rendering_dir(struct ge2d_context_s *wq,
			   int src1_xrev,
			   int src1_yrev,
			   int dst_xrev,
			   int dst_yrev,
			   int dst_xy_swap);

void ge2dgen_dst(struct ge2d_context_s *wq,
		unsigned int canvas_addr,
		unsigned int format,
		unsigned int phy_addr,
		unsigned int stride);

void ge2dgen_src_clip(struct ge2d_context_s *wq,
		      int x, int y, int w, int h);

void ge2dgen_src_key(struct ge2d_context_s *wq,
		     int en, int key, int keymask, int keymode);

void ge2dgent_src_gbalpha(struct ge2d_context_s *wq,
			  unsigned char alpha1, unsigned char alpha2);

void ge2dgen_src_color(struct ge2d_context_s *wq,
		       unsigned int color);

void ge2dgent_rendering_dir(struct ge2d_context_s *wq,
			    int src_x_dir, int src_y_dir,
			    int dst_x_dir, int dst_y_dir);


void ge2dgen_dst_clip(struct ge2d_context_s *wq,
		      int x, int y, int w, int h, int mode);

void ge2dgent_src2_clip(struct ge2d_context_s *wq,
			int x, int y, int w, int h);

void ge2dgen_cb(struct ge2d_context_s *wq,
		int (*cmd_cb)(unsigned int), unsigned int param);

void ge2dgen_const_color(struct ge2d_context_s *wq,
	unsigned int color);

#endif

