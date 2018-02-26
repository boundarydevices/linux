/*
 * drivers/amlogic/media/osd/osd_drm.h
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

#ifndef _OSD_DRM_H_
#define _OSD_DRM_H_
#include "osd.h"
struct osd_plane_map_s {
	u32 plane_index;
	u32 zorder;
	u32 phy_addr;
	u32 format;
	u32 enable;
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	int byte_stride;
	u32 background_w;
	u32 background_h;
	u32 premult_en;
	u32 afbc_en;
	u32 afbc_inter_format;
	u32 blend_mode;
	int  plane_alpha;
	u32 reserve;
};

int osd_drm_init(struct osd_device_data_s *osd_meson_dev);
void osd_drm_debugfs_add(
	struct dentry **plane_debugfs_dir,
	char *name,
	int osd_id);
void osd_drm_debugfs_init(void);
void osd_drm_debugfs_exit(void);

void osd_drm_plane_page_flip(struct osd_plane_map_s *plane_map);
void osd_drm_plane_enable_hw(u32 index, u32 enable);
void osd_drm_vsync_isr_handler(void);


#endif
