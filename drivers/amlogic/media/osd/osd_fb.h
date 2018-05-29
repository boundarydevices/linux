/*
 * drivers/amlogic/media/osd/osd_fb.h
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

#ifndef _OSD_FB_H_
#define _OSD_FB_H_

/* Linux Headers */
#include <linux/list.h>
#include <linux/fb.h>
#include <linux/types.h>


/* Local Headers */
#include "osd.h"

#define OSD_COUNT (HW_OSD_COUNT)
#define INVALID_BPP_ITEM {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define FBIO_WAITFORVSYNC          _IOW('F', 0x20, __u32)
#define FBIO_WAITFORVSYNC_64       _IOW('F', 0x21, __u32)

struct osd_fb_dev_s {
	struct mutex lock;
	struct fb_info *fb_info;
	struct platform_device *dev;
	phys_addr_t fb_mem_paddr;
	void __iomem *fb_mem_vaddr;
	u32 fb_len;
	phys_addr_t fb_mem_afbc_paddr[OSD_MAX_BUF_NUM];
	void __iomem *fb_mem_afbc_vaddr[OSD_MAX_BUF_NUM];
	u32 fb_afbc_len[OSD_MAX_BUF_NUM];
	const struct color_bit_define_s *color;
	enum vmode_e vmode;
	struct osd_ctl_s osd_ctl;
	u32 order;
	u32 scale;
	u32 enable_3d;
	u32 preblend_enable;
	u32 enable_key_flag;
	u32 color_key;
	u32 fb_index;
	u32 open_count;
	bool dis_osd_mchange;
};

struct fb_dmabuf_export {
	__u32 buffer_idx;
	__u32 fd;
	__u32 flags;
};

#define OSD_INVALID_INFO 0xffffffff
#define OSD_FIRST_GROUP_START 1
#define OSD_SECOND_GROUP_START 4
#define OSD_END 7

extern phys_addr_t get_fb_rmem_paddr(int index);
extern void __iomem *get_fb_rmem_vaddr(int index);
extern size_t get_fb_rmem_size(int index);
extern int osd_blank(int blank_mode, struct fb_info *info);
extern struct osd_fb_dev_s *gp_fbdev_list[];
extern const struct color_bit_define_s default_color_format_array[];
#endif
