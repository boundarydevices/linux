/*
 * include/linux/amlogic/media/utils/vformat.h
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

#ifndef VFORMAT_H
#define VFORMAT_H

enum vformat_e {
	VFORMAT_MPEG12 = 0,
	VFORMAT_MPEG4,
	VFORMAT_H264,
	VFORMAT_MJPEG,
	VFORMAT_REAL,
	VFORMAT_JPEG,
	VFORMAT_VC1,
	VFORMAT_AVS,
	VFORMAT_YUV,		/* Use SW decoder */
	VFORMAT_H264MVC,
	VFORMAT_H264_4K2K,
	VFORMAT_HEVC,
	VFORMAT_H264_ENC,
	VFORMAT_JPEG_ENC,
	VFORMAT_VP9,
	VFORMAT_AVS2,
	VFORMAT_MAX
};

#endif /* VFORMAT_H */
