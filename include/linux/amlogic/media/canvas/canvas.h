/*
 * include/linux/amlogic/media/canvas/canvas.h
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

#ifndef CANVAS_H
#define CANVAS_H

#include <linux/types.h>
#include <linux/kobject.h>

struct canvas_s {
	struct kobject kobj;
	u32 index;
	ulong addr;
	u32 width;
	u32 height;
	u32 wrap;
	u32 blkmode;
	u32 endian;
	u32 dataL;
	u32 dataH;
};

struct canvas_config_s {
	u32 phy_addr;
	u32 width;
	u32 height;
	u32 block_mode;
	u32 endian;
};

#define CANVAS_ADDR_NOWRAP      0x00
#define CANVAS_ADDR_WRAPX       0x01
#define CANVAS_ADDR_WRAPY       0x02
#define CANVAS_BLKMODE_MASK     3
#define CANVAS_BLKMODE_BIT      24
#define CANVAS_BLKMODE_LINEAR   0x00
#define CANVAS_BLKMODE_32X32    0x01
#define CANVAS_BLKMODE_64X32    0x02

#define PPMGR_CANVAS_INDEX 0x70
#define PPMGR_DOUBLE_CANVAS_INDEX 0x74	/*for double canvas use */
#define PPMGR_DEINTERLACE_BUF_CANVAS 0x7a	/*for progressive mjpeg use */

/*for progressive mjpeg (nv21 output)use*/
#define PPMGR_DEINTERLACE_BUF_NV21_CANVAS 0x7e

#define PPMGR2_MAX_CANVAS 16
#define PPMGR2_CANVAS_INDEX 0x70    /* 0x70-0x7f for PPMGR2 (IONVIDEO)/ */

/*
 *the following reserved canvas index value
 * should match the configurations defined
 * in canvas_mgr.c canvas_pool_config().
 */
#define AMVDEC_CANVAS_MAX1        0xbf
#define AMVDEC_CANVAS_MAX2        0x25
#define AMVDEC_CANVAS_START_INDEX 0x78

extern void canvas_config_config(u32 index, struct canvas_config_s *cfg);

extern void canvas_config(u32 index, ulong addr, u32 width, u32 height,
	u32 wrap, u32 blkmode);

void canvas_config_ex(u32 index, ulong addr, u32 width, u32 height, u32 wrap,
	u32 blkmode, u32 endian);

extern void canvas_read(u32 index, struct canvas_s *p);

extern void canvas_copy(unsigned int src, unsigned int dst);

extern void canvas_update_addr(u32 index, u32 addr);

extern unsigned int canvas_get_addr(u32 index);

#endif /* CANVAS_H */
