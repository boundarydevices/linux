/*
 * include/linux/amlogic/media/video_sink/amvideocap.h
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

#ifndef __AMVIDEOCAP_HEADHER_
#define __AMVIDEOCAP_HEADHER_
#define AC_MAGIC  'V'
#include <linux/videodev2.h>

#define CAP_FLAG_AT_CURRENT		0
#define CAP_FLAG_AT_TIME_WINDOW	1
#define CAP_FLAG_AT_END			2

/*
 *format see linux/ge2d/ge2d.h
 *like:
 *GE2D_FORMAT_S24_RGB
 */

#define AMVIDEOCAP_IOW_SET_WANTFRAME_FORMAT	_IOW((AC_MAGIC), 0x01, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_WIDTH	_IOW((AC_MAGIC), 0x02, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_HEIGHT	_IOW((AC_MAGIC), 0x03, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_TIMESTAMP_MS _IOW((AC_MAGIC), 0x04, u64)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_WAIT_MAX_MS _IOW((AC_MAGIC), 0x05, u64)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_AT_FLAGS	_IOW((AC_MAGIC), 0x06, int)

#define AMVIDEOCAP_IOR_GET_FRAME_FORMAT	_IOR((AC_MAGIC), 0x10, int)
#define AMVIDEOCAP_IOR_GET_FRAME_WIDTH	_IOR((AC_MAGIC), 0x11, int)
#define AMVIDEOCAP_IOR_GET_FRAME_HEIGHT	_IOR((AC_MAGIC), 0x12, int)
#define AMVIDEOCAP_IOR_GET_FRAME_TIMESTAMP_MS	_IOR((AC_MAGIC), 0x13, int)

#define AMVIDEOCAP_IOR_GET_SRCFRAME_FORMAT	_IOR((AC_MAGIC), 0x20, int)
#define AMVIDEOCAP_IOR_GET_SRCFRAME_WIDTH	_IOR((AC_MAGIC), 0x21, int)
#define AMVIDEOCAP_IOR_GET_SRCFRAME_HEIGHT	_IOR((AC_MAGIC), 0x22, int)

#define AMVIDEOCAP_IOR_GET_STATE	_IOR((AC_MAGIC), 0x31, int)
#define AMVIDEOCAP_IOW_SET_START_CAPTURE	_IOW((AC_MAGIC), 0x32, int)
#define AMVIDEOCAP_IOW_SET_CANCEL_CAPTURE	_IOW((AC_MAGIC), 0x33, int)

#define AMVIDEOCAP_IOR_SET_SRC_X _IOR((AC_MAGIC), 0x40, int)
#define AMVIDEOCAP_IOR_SET_SRC_Y _IOR((AC_MAGIC), 0x41, int)
#define AMVIDEOCAP_IOR_SET_SRC_WIDTH _IOR((AC_MAGIC), 0x42, int)
#define AMVIDEOCAP_IOR_SET_SRC_HEIGHT _IOR((AC_MAGIC), 0x43, int)

enum amvideocap_state {
	AMVIDEOCAP_STATE_INIT = 0,
	AMVIDEOCAP_STATE_ON_CAPTURE = 200,
	AMVIDEOCAP_STATE_FINISHED_CAPTURE = 300,
	AMVIDEOCAP_STATE_ERROR = 0xffff,
};
#endif				/* __AMVIDEOCAP_HEADHER_ */
