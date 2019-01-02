/*
 * include/linux/amlogic/media/camera/aml_cam_info.h
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

#ifndef __AML_CAM_DEV__
#define __AML_CAM_DEV__
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/camera/flashlight.h>

#define FRONT_CAM	0
#define BACK_CAM	1

enum resolution_size {
	SIZE_NULL = 0, SIZE_176X144,	/* 4:3 */
	SIZE_320X240,	/* 4:3 */
	SIZE_352X288,   /* 4:3 */
	SIZE_640X480,	/* 0.3M	4:3 */
	SIZE_720X405,	/* 0.3M	16:9 */
	SIZE_800X600,	/* 0.5M	4:3 */
	SIZE_960X540,	/* 0.5M	16:9 */
	SIZE_1024X576,	/* 0.6M	16:9 */
	SIZE_960X720,	/* 0.7M	4:3 */
	SIZE_1024X768,	/* 0.8M	4:3 */
	SIZE_1280X720,	/* 0.9M	16:9 */
	SIZE_1152X864,	/* 1M	4:3 */
	SIZE_1366X768,	/* 1M	16:9 */
	SIZE_1280X960,	/* 1.2M	4:3 */
	SIZE_1280X1024,	/* 1.3M	16:9. */
	SIZE_1400X1050,	/* 1.5M	4:3 */
	SIZE_1600X900,	/* 1.5M	16:9 */
	SIZE_1600X1200,	/* 2M	4:3 */
	SIZE_1920X1080,	/* 2M	16:9 */
	SIZE_1792X1344,	/* 2.4M	4:3 */
	SIZE_2048X1152,	/* 2.4M	16:9 */
	SIZE_2048X1536,	/* 3.2M	4:3 */
	SIZE_2304X1728,	/* 4M	4:3 */
	SIZE_2560X1440,	/* 4M	16:9 */
	SIZE_2592X1944,	/* 5M	4:3 */
	SIZE_3072X1728,	/* 5M	16:9 */
	SIZE_2816X2112,	/* 6M	4:3 */
	SIZE_3264X1836, /* 6m    16:9 */
	SIZE_3072X2304,	/* 7M	4:3 */
	SIZE_3200X2400,	/* 7.5M	4:3 */
	SIZE_3264X2448,	/* 8M	4:3 */
	SIZE_3840X2160,	/* 8M	16:9 */
	SIZE_3456X2592,	/* 9M	4:3 */
	SIZE_3600X2700,	/* 9.5M	4:3 */
	SIZE_4096X2304,	/* 9.5M	16:9 */
	SIZE_3672X2754,	/* 10M	4:3 */
	SIZE_3840X2880,	/* 11M	4:3 */
	SIZE_4000X3000,	/* 12M	4:3 */
	SIZE_4608X2592,	/* 12M	16:9 */
	SIZE_4096X3072,	/* 12.5M	4:3 */
	SIZE_4800X3200,	/* 15M	4:3 */
	SIZE_5120X2880,	/* 15M	16:9 */
	SIZE_5120X3840,	/* 20M	4:3 */
	SIZE_6400X4800,	/* 30M	4:3 */
};

typedef int (*aml_cam_probe_fun_t)(struct i2c_adapter *);

struct aml_cam_info_s {
	struct list_head info_entry;
	const char *name;
	unsigned int i2c_bus_num;
	unsigned int pwdn_act;
	unsigned int front_back; /* front is 0, back is 1 */
	unsigned int m_flip;
	unsigned int v_flip;
	unsigned int flash;
	unsigned int auto_focus;
	unsigned int i2c_addr;
	const char *motor_driver;
	const char *resolution;
	const char *version;
	unsigned int mclk;
	unsigned int flash_support;
	unsigned int flash_ctrl_level;
	unsigned int torch_support;
	unsigned int torch_ctrl_level;
	unsigned int vcm_mode;
	unsigned int spread_spectrum;
	unsigned int vdin_path;
	unsigned int bt_path_count;
	enum bt_path_e bt_path;
	enum cam_interface_e interface;
	enum clk_channel_e clk_channel;
	/* gpio_t pwdn_pin; */
	/* gpio_t rst_pin; */
	/* gpio_t flash_ctrl_pin; */
	/* gpio_t torch_ctrl_pin; */
	unsigned int pwdn_pin;
	unsigned int rst_pin;
	unsigned int flash_ctrl_pin;
	unsigned int torch_ctrl_pin;
	enum resolution_size max_cap_size;
	enum tvin_color_fmt_e bayer_fmt;
	const char *config;
	struct pinctrl *camera_pin_ctrl;
};
/* aml_cam_info_t */

struct aml_camera_i2c_fig_s {
	unsigned short addr;
	unsigned char val;
};

struct aml_camera_i2c_fig0_s {
	unsigned short addr;
	unsigned short val;
};

struct aml_camera_i2c_fig1_s {
	unsigned char addr;
	unsigned char val;
};

extern void aml_cam_init(struct aml_cam_info_s *cam_dev);
extern void aml_cam_uninit(struct aml_cam_info_s *cam_dev);
extern void aml_cam_flash(struct aml_cam_info_s *cam_dev, int is_on);
extern void aml_cam_torch(struct aml_cam_info_s *cam_dev, int is_on);
extern int aml_cam_info_reg(struct aml_cam_info_s *cam_info);
extern int aml_cam_info_unreg(struct aml_cam_info_s *cam_info);

#endif /* __AML_CAM_DEV__ */
