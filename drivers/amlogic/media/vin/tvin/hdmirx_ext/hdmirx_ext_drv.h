/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_drv.h
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

#ifndef __HDMIRX_EXT_DRV_H__
#define __HDMIRX_EXT_DRV_H__

#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include "../tvin_frontend.h"

#define HDMIRX_EXT_DRV_VER "20170823"

#define HDMIRX_EXT_DRV_NAME "hdmirx_ext"
#define HDMIRX_EXT_DEV_NAME "hdmirx_ext"

extern int hdmirx_ext_debug_print;
#define RXEXTERR(fmt, args...)  pr_err("hdmirx_ext:error: " fmt, ## args)
#define RXEXTPR(fmt, args...)   pr_info("hdmirx_ext: " fmt, ## args)
#define RXEXTDBG(fmt, args...)  do { if (hdmirx_ext_debug_print) \
					RXEXTPR(fmt, ##args); \
				} while (0)

#define printf(fmt, args...)    pr_info("hdmirx_ext: " fmt, ## args)

#define RXEXT_PFUNC()    RXEXTPR("%s: line: %d\n", __func__, __LINE__)

struct hdmirx_ext_status_s {
	unsigned int  cable; /* whether the hdmi cable is inserted. */
	unsigned int  signal; /* whether the valid signal is got. */
	unsigned int  video_mode; /* format of the output video  */
	unsigned int  audio_sr; /* audio sample rate of the output audio */
};

struct video_timming_s {
	int h_active;
	int h_total;
	int hs_frontporch;
	int hs_width;
	int hs_backporch;
	int v_active;
	int v_total;
	int vs_frontporch;
	int vs_width;
	int vs_backporch;
	int mode;
	int hs_pol;
	int vs_pol;
};

struct hdmirx_ext_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int flag;
	int val_on;
	int val_off;
};

struct hdmirx_ext_hw_s {
	/* int i2c_addr_pull; */
	unsigned char i2c_addr;
	int i2c_bus_index;
	struct i2c_client *i2c_client;

	struct hdmirx_ext_gpio_s en_gpio;
	struct hdmirx_ext_gpio_s reset_gpio;

	struct pinctrl *pin;
	unsigned char pinmux_flag;

	/* hw operation */
	/* signal stauts */
	int (*get_cable_status)(void);
	int (*get_signal_status)(void);
	int (*get_input_port)(void);
	void (*set_input_port)(int port);

	/* signal timming */
	int (*get_video_timming)(struct video_timming_s *ptimming);

	/* hdmi/dvi mode */
	int (*is_hdmi_mode)(void);

	/* audio mode */
	int (*get_audio_sample_rate)(void);

	/* debug interface
	 * it should support the following format:
	 * read registers:  r device_address register_address
	 * write registers: w device_address register_address value
	 * dump registers:  dump device_address
	 *		     register_address_start register_address_end
	 * video timming:   vinfo
	 * cmd format help: help
	 */
	int (*debug)(char *buf);

	/* chip id and driver version */
	char* (*get_chip_version)(void);

	/* hardware init related */
	int (*init)(void);
	int (*enable)(void);
	void (*disable)(void);
};

struct hdmirx_ext_vdin_s {
	struct tvin_frontend_s frontend;
	struct vdin_parm_s     parm;
	unsigned char          vdin_sel;
	unsigned char          bt656_sel;
	unsigned char          started;
};

struct hdmirx_ext_drv_s {
	char name[20];
	int state; /* 1=on, 0=off */
	struct device *dev;
	struct cdev cdev;
	struct hdmirx_ext_status_s status;
	struct hdmirx_ext_hw_s hw;
	struct hdmirx_ext_vdin_s vdin;
	struct vdin_v4l2_ops_s *vops;

	/* 0 to disable from user
	 * 1 to enable, driver will trigger to vdin-stop
	 * 2 to enable, driver will trigger to vdin-start
	 * 3 to enable, driver will trigger to vdin-start/vdin-stop
	 * 4 to enable, driver will not trigger to vdin-start/vdin-stop
	 * 0xff to enable, and driver will NOT trigger signal-lost/vdin-stop,
	 *		signal-get/vdin-start
	 */
	unsigned int user_cmd;

	struct timer_list timer;
	int timer_cnt;
	void (*timer_func)(void);
	struct hrtimer hr_timer;
	long hrtimer_cnt;
	void (*hrtimer_func)(void);
};

/* according to the CEA-861-D */
enum SII9233_VIDEO_MODE_e {
	CEA_480P60	= 2,
	CEA_720P60	= 4,
	CEA_1080I60	= 5,
	CEA_480I60	= 6,

	CEA_1080P60	= 16,
	CEA_576P50	= 17,
	CEA_720P50	= 19,
	CEA_1080I50	= 20,
	CEA_576I50	= 21,

	CEA_1080P50	= 31,

	CEA_MAX = 60
};

extern struct hdmirx_ext_drv_s *hdmirx_ext_get_driver(void);
#if defined(CONFIG_AMLOGIC_MEDIA_TVIN_HDMI_EXT_SII9135)
extern void hdmirx_ext_register_hw_sii9135(struct hdmirx_ext_drv_s *hdev);
#endif

#endif
