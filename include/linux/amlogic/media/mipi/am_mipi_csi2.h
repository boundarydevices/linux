/*
 * include/linux/amlogic/media/mipi/am_mipi_csi2.h
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

#ifndef __AM_MIPI_CSI2_H__
#define __AM_MIPI_CSI2_H__

#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#define CSI2_BUF_POOL_SIZE            6
#define CSI2_OUTPUT_BUF_POOL_SIZE     1

#define AM_CSI2_FLAG_NULL             0x00000000
#define AM_CSI2_FLAG_INITED           0x00000001
#define AM_CSI2_FLAG_DEV_READY        0x00000002
#define AM_CSI2_FLAG_STARTED          0x00000004

enum am_csi2_mode {
	AM_CSI2_ALL_MEM,
	AM_CSI2_VDIN,
};

struct am_csi2_pixel_fmt {
	char *name;
	u32  fourcc;
	int  depth;
};

struct am_csi2_camera_para {
	const char   *name;
	unsigned int output_pixel;
	unsigned int output_line;
	unsigned int active_pixel;
	unsigned int active_line;
	unsigned int frame_rate;
	unsigned int ui_val;
	unsigned int hs_freq;
	unsigned char clock_lane_mode;
	unsigned char mirror;
	unsigned int zoom;
	unsigned int angle;
	struct am_csi2_pixel_fmt *in_fmt;
	struct am_csi2_pixel_fmt *out_fmt;
};

struct am_csi2_client_config {
	enum am_csi2_mode mode;
	unsigned char lanes;
	unsigned char channel;
	int vdin_num;
	char name[32];
	void *pdev;
};

struct am_csi2_pdata {
	struct am_csi2_client_config *clients;
	int num_clients;
};

struct am_csi2_frame_s {
	unsigned int ddr_address;
	int index;
	unsigned int status;
	unsigned int w;
	unsigned int h;
	int read_cnt;
	unsigned int err;
};

struct am_csi2_output_s {
	void *vaddr;
	unsigned int output_pixel;
	unsigned int output_line;
	u32   fourcc;
	int    depth;
	unsigned int frame_size;
	unsigned char frame_available;
	unsigned int zoom;
	unsigned int angle;
	struct am_csi2_frame_s frame[CSI2_OUTPUT_BUF_POOL_SIZE];
};

struct am_csi2_input_s {
	unsigned int active_pixel;
	unsigned int active_line;
	u32   fourcc;
	int    depth;
	unsigned int frame_size;
	unsigned char frame_available;
	struct am_csi2_frame_s frame[CSI2_BUF_POOL_SIZE];
};

struct am_csi2_hw_s {
	unsigned char lanes;
	unsigned char channel;
	unsigned char mode;
	unsigned char clock_lane_mode;
	struct am_csi2_frame_s *frame;
	unsigned int active_pixel;
	unsigned int active_line;
	unsigned int frame_size;
	unsigned int ui_val;
	unsigned int hs_freq;
	unsigned int urgent;
};

struct am_csi2_s {
	char *name;
	enum am_csi2_mode mode;
	unsigned char lanes;
	unsigned char channel;
	int vdin_num;
	int id;
	struct platform_device *pdev;
	struct am_csi2_client_config *client;
	struct mutex lock;
#ifdef CONFIG_MEM_MIPI
	int irq;
#endif
	unsigned int pbufAddr;
	unsigned int decbuf_size;
	unsigned int frame_rate;
	unsigned int ui_val;
	unsigned int hs_freq;
	unsigned char clock_lane_mode;
	unsigned char mirror;
	unsigned int status;
	struct am_csi2_input_s input;
	struct am_csi2_output_s output;
	struct am_csi2_ops_s *ops;
};

struct am_csi2_ops_s {
	enum am_csi2_mode mode;
	struct am_csi2_pixel_fmt* (*getPixelFormat)(u32 fourcc, bool input);
	int (*init)(struct am_csi2_s *dev);
	int (*streamon)(struct am_csi2_s *dev);
	int (*streamoff)(struct am_csi2_s *dev);
	int (*fill)(struct am_csi2_s *dev);
	int (*uninit)(struct am_csi2_s *dev);
	void *privdata;
	int data_num;
};

extern int start_mipi_csi2_service(struct am_csi2_camera_para *para);
extern int stop_mipi_csi2_service(struct am_csi2_camera_para *para);
extern void am_mipi_csi2_init(struct csi_parm_s *info);
extern void am_mipi_csi2_uninit(void);
extern void init_am_mipi_csi2_clock(void);
extern void cal_csi_para(struct csi_parm_s *info);

#define MIPI_DEBUG
#ifdef MIPI_DEBUG
#define mipi_dbg(fmt, args...) printk(fmt, ## args)
#else
#define mipi_dbg(fmt, args...)
#endif
#define mipi_error(fmt, args...) printk(fmt, ## args)


#define CSI_ADPT_START_REG      CSI2_CLK_RESET
#define CSI_ADPT_END_REG        CSI2_PIC_SIZE_STAT

#define CSI_PHY_START_REG      MIPI_PHY_CTRL
#define CSI_PHY_END_REG        MIPI_PHY_MUX_CTRL1

#define CSI_HST_START_REG       CSI2_HOST_CSI2_RESETN
#define CSI_HST_END_REG         CSI2_HOST_MASK2

#endif
