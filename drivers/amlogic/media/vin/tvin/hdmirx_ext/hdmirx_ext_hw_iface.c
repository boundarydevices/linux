/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_hw_iface.c
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

#include "hdmirx_ext_drv.h"
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/delay.h>

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* signal stauts */
int __hw_get_cable_status(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_cable_status))
		return hdrv->hw.get_cable_status();

	return 0;
}

int __hw_get_signal_status(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_signal_status))
		return hdrv->hw.get_signal_status();

	return 0;
}

int __hw_get_input_port(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_input_port))
		return hdrv->hw.get_input_port();

	return 0;
}

void __hw_set_input_port(int port)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.set_input_port))
		return hdrv->hw.set_input_port(port);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* signal timming */
int __hw_get_video_timming(struct video_timming_s *ptimming)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_video_timming))
		return hdrv->hw.get_video_timming(ptimming);

	return -1;
}

int __hw_get_video_mode(void)
{
	unsigned int h_active, h_total, v_active, v_total;
	unsigned int mode = 0;
	struct video_timming_s timming;

	if (__hw_get_video_timming(&timming) == -1) {
		RXEXTERR("%s: get video timming failed!\n", __func__);
		return CEA_MAX;
	}

	h_active = timming.h_active;
	h_total = timming.h_total;
	v_active = timming.v_active;
	v_total = timming.v_total;

	RXEXTPR("%s: pixel = %d x %d ( %d x %d )\n",
		__func__, h_active, v_active, h_total, v_total);

	if ((h_total == 2200) && (v_active == 1080))
		mode = CEA_1080P60;/* 1080p */
	else if ((h_total == 2640) && (v_active == 1080))
		mode = CEA_1080P50;/* 1080p50 */
	else if ((h_total == 2200) && (v_active == 540))
		mode = CEA_1080I60;/* 1080i */
	else if ((h_total == 2640) && (v_active == 540))
		mode = CEA_1080I50;/* 1080i50 */
	else if ((h_total == 1650) && (v_active == 720))
		mode = CEA_720P60;/* 720p */
	else if ((h_total == 1980) && (v_active == 720))
		mode = CEA_720P50;/* 720p50 */
	else if ((h_total == 864) && (v_active == 576))
		mode = CEA_576P50;/* 576p */
	else if ((h_total == 858) && (v_active == 480))
		mode = CEA_480P60;/* 480p */
	else if ((h_total == 864) && (v_active == 288))
		mode = CEA_576I50;/* 576i */
	else if ((h_total == 858) && (v_active == 240))
		mode = CEA_480I60;/* 480i */

	return mode;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hdmi/dvi mode */
int __hw_is_hdmi_mode(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.is_hdmi_mode))
		return hdrv->hw.is_hdmi_mode();

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* audio mode
 * audio sampling frequency:
 * 0x0 for 44.1 KHz
 * 0x1 for Not indicated
 * 0x2 for 48 KHz
 * 0x3 for 32 KHz
 * 0x4 for 22.05 KHz
 * 0x6 for 24 kHz
 * 0x8 for 88.2 kHz
 * 0x9 for 768 kHz (192*4)
 * 0xa for 96 kHz
 * 0xc for 176.4 kHz
 * 0xe for 192 kHz
 */
int __hw_get_audio_sample_rate(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_audio_sample_rate))
		return hdrv->hw.get_audio_sample_rate();

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug interface */
int __hw_debug(char *buf)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.debug))
		return hdrv->hw.debug(buf);

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* chip id and driver version */
char *__hw_get_chip_id(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.get_chip_version))
		return hdrv->hw.get_chip_version();

	return NULL;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware init related */
int __hw_init(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if ((hdrv) && (hdrv->hw.init))
		return hdrv->hw.init();

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware enable related */
int __hw_enable(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (!hdrv)
		return -1;

	if (hdrv->hw.enable)
		return hdrv->hw.enable();
	hdrv->state = 1;

	return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hardware disable related */
void __hw_disable(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (!hdrv)
		return;

	hdrv->state = 0;

	if (hdrv->hw.disable)
		hdrv->hw.disable();
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* hdmirx_ext utility */
void __hw_dump_video_timming(void)
{
	int height, width, h_total, v_total;
	int hs_fp, hs_width, hs_bp;
	int vs_fp, vs_width, vs_bp;
	struct video_timming_s timming;

	if (__hw_get_video_timming(&timming) == -1) {
		RXEXTERR("%s: get video timming failed!\n", __func__);
		return;
	}

	height		= timming.h_active;
	width		= timming.v_active;

	h_total		= timming.h_total;
	v_total		= timming.v_total;

	hs_fp		= timming.hs_frontporch;
	hs_width	= timming.hs_width;
	hs_bp		= timming.hs_backporch;

	vs_fp		= timming.vs_frontporch;
	vs_width	= timming.vs_width;
	vs_bp		= timming.vs_backporch;

	pr_info("hdmirx_ext video info:\n"
		"height*width active = %4d x %4d\n"
		"height*width total  = %4d x %4d\n"
		"h_sync = %4d, %4d, %4d\n"
		"v_sync = %4d, %4d, %4d\n",
		height, width, h_total, v_total,
		hs_fp, hs_width, hs_bp,
		vs_fp, vs_width, vs_bp);
}

