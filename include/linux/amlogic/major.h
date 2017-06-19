/*
 * include/linux/amlogic/major.h
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

#ifndef _LINUX_MAJOR_H
#define _LINUX_MAJOR_H
#include <linux/fs.h>

/* Amlogic extension */
#define AML_BASE		CHRDEV_MAJOR_HASH_SIZE
#define AMSTREAM_MAJOR		(0+(AML_BASE))
#define AUDIODSP_MAJOR		(2+(AML_BASE))
#define AMVIDEO_MAJOR		(9+(AML_BASE))
#define AMAUDIO_MAJOR		(11+(AML_BASE))
#define AMVIDEO2_MAJOR		(12+(AML_BASE))
#define AMAUDIO2_MAJOR		(13+(AML_BASE))
#define VFM_MAJOR               (14+(AML_BASE))
#define IONVIDEO_MAJOR		(15+(AML_BASE))
/*
 *#define UIO_MAJOR			4+(AML_BASE)
 *#define USB_DEV_EP_MAJOR	5+(AML_BASE)
 *#define TV_CONF_MAJOR		6+(AML_BASE)
 *#define HIDRAW_MAJOR		7+(AML_BASE)
 *#define HWJPEGDEC_MAJOR		8+(AML_BASE)
 *
 *#define AML_DEMOD_MAJOR		10+(AML_BASE)
 *#define TV2_CONF_MAJOR		13+(AML_BASE)
 *#define BLOCK_EXT_MAJOR		14+(AML_BASE)
 *#define SCSI_OSD_MAJOR		15+(AML_BASE)
 */
#endif
