/*
 * include/linux/amlogic/media/vfm/video_common.h
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

#ifndef __INC_VIDEO_COMMON_H__
#define __INC_VIDEO_COMMON_H__

enum color_fmt_e {
	COLOR_FMT_RGB444 = 0,
	COLOR_FMT_YUV422,		/* 1 */
	COLOR_FMT_YUV444,		/* 2 */
	COLOR_FMT_YUYV422,		/* 3 */
	COLOR_FMT_YVYU422,		/* 4 */
	COLOR_FMT_UYVY422,		/* 5 */
	COLOR_FMT_VYUY422,		/* 6 */
	COLOR_FMT_NV12,		/* 7 */
	COLOR_FMT_NV21,		/* 8 */
	COLOR_FMT_BGGR,		/* 9  raw data */
	COLOR_FMT_RGGB,		/* 10 raw data */
	COLOR_FMT_GBRG,		/* 11 raw data */
	COLOR_FMT_GRBG,		/* 12 raw data */
	COLOR_FMT_MAX,
};


#endif
