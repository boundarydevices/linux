/*
 * include/linux/amlogic/media/video_sink/video/video.h
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

#ifndef VIDEO_HEADER_
#define VIDEO_HEADER_
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA

int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
u32 RDMA_READ_REG(u32 adr);
int RDMA_SET_READ(u32 adr);
#endif

void try_free_keep_video(int flags);
void vh265_free_cmabuf(void);
void vh264_4k_free_cmabuf(void);
void vdec_free_cmabuf(void);

#endif
