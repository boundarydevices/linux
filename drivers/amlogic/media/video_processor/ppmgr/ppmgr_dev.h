/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_dev.h
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

#ifndef PPMGR_DEV_INCLUDE_H
#define PPMGR_DEV_INCLUDE_H
#include <linux/amlogic/media/vfm/vframe.h>
struct ppmgr_device_t {
	struct class *cla;
	struct device *dev;
	char name[20];
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	unsigned int buffer_start;
	unsigned int buffer_size;

	unsigned int angle;
	unsigned int orientation;
	unsigned int videoangle;

	int bypass;
	int disp_width;
	int disp_height;
	int canvas_width;
	int canvas_height;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	int ppscaler_flag;
	int scale_h_start;
	int scale_h_end;
	int scale_v_start;
	int scale_v_end;
#endif
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
	unsigned int ppmgr_3d_mode;
	unsigned int direction_3d;
	int viewmode;
	unsigned int scale_down;
#endif
	const struct vinfo_s *vinfo;
	int left;
	int top;
	int width;
	int height;
	int receiver;
	int receiver_format;
	int display_mode;
	int mirror_flag;
	int use_prot;
	int disable_prot;
	int started;
	int global_angle;
	int use_reserved;
	unsigned int tb_detect;
	unsigned int tb_detect_period;
	unsigned int tb_detect_buf_len;
	unsigned int tb_detect_init_mute;
	struct page *cma_pages;
	struct io_mapping *mapping;
	void  __iomem *vir_addr;
	struct platform_device *pdev;
	unsigned int ppmgr_debug;
};

struct ppmgr_dev_reg_s {
	unsigned long mem_start;
	unsigned long mem_end;
	struct device *cma_dev;
	struct dec_sysinfo *sys_info;
};

struct ppframe_s {
	struct vframe_s frame;
	int index;
	int angle;
	struct vframe_s *dec_frame;
};

#define to_ppframe(vf)	\
	container_of(vf, struct ppframe_s, frame)

extern struct ppmgr_device_t ppmgr_device;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_3D_PROCESS
extern void Reset3Dclear(void);
extern void Set3DProcessPara(unsigned int mode);
#endif

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h, int flag);

extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h, int flag);

#endif

extern int vf_ppmgr_get_states(struct vframe_states *states);

#endif /* PPMGR_DEV_INCLUDE_H. */
