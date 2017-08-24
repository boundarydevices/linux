/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/vdin_iface.c
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

/* #include <mach/am_regs.h> */
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include "hdmirx_ext_drv.h"
#include "../tvin_frontend.h"
#include "hdmirx_ext_hw_iface.h"
#include "hdmirx_ext_reg.h"
#include "vdin_iface.h"

static int __vdin_set_pinmux(int flag)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();

	if (flag) { /* enable pinmux */
		if (hdrv->hw.pinmux_flag == 0) {
			/* request pinmux */
			hdrv->hw.pin =
				devm_pinctrl_get_select_default(hdrv->dev);
			if (IS_ERR(hdrv->hw.pin))
				RXEXTERR("pinmux set error\n");
			else
				hdrv->hw.pinmux_flag = 1;
		} else {
			RXEXTERR("pinmux is already selected\n");
		}
	} else { /* disable pinmux */
		if (hdrv->hw.pinmux_flag) {
			hdrv->hw.pinmux_flag = 0;
			/* release pinmux */
			devm_pinctrl_put(hdrv->hw.pin);
		} else {
			RXEXTERR("pinmux is already released\n");
		}
	}

	return 0;
}

void hdmirx_ext_stop_vdin(void)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	unsigned char vdin_sel;

	if (hdrv->vdin.started == 0)
		return;

	vdin_sel = hdrv->vdin.vdin_sel;
	if (hdrv->vops) {
		RXEXTPR("%s: begin stop_tvin_service()\n", __func__);
		hdrv->vops->stop_tvin_service(vdin_sel);
		set_invert_top_bot(false);
		__vdin_set_pinmux(0);
		hdrv->vdin.started = 0;
	} else {
		RXEXTERR("%s: v4l2_ops is NULL\n", __func__);
	}

	RXEXTPR("%s: finish stop vdin\n", __func__);
}

void hdmirx_ext_start_vdin(int width, int height, int frame_rate,
		int field_flag)
{
	struct hdmirx_ext_drv_s *hdrv = hdmirx_ext_get_driver();
	struct vdin_parm_s para;
	unsigned char vdin_sel, bt656_sel;
	struct video_timming_s timming;
	int start_pix = 138, start_line_o = 22, start_line_e = 23;
	int h_total = 1728, v_total = 625;

	RXEXTPR("%s: width=%d, height=%d, frame_rate=%d, field_flag=%d\n",
		__func__, width, height, frame_rate, field_flag);

	if (hdrv->vdin.started)
		return;

	if (__hw_get_video_timming(&timming) == -1) {
		RXEXTERR("%s: get video timming failed!\n", __func__);
		return;
	}

	if ((width <= 0) || (height <= 0) || (frame_rate <= 0)) {
		RXEXTERR("%s: invalid size or frame_rate\n", __func__);
		return;
	}

	vdin_sel = hdrv->vdin.vdin_sel;
	bt656_sel = (hdrv->vdin.bt656_sel == 1) ? BT_PATH_GPIO : BT_PATH_GPIO_B;

	if (field_flag && (height <= 576)) {
		if ((width == 1440) && (height == 576)) {
			/* for rgb 576i signal, it's 720/864, not 1440/1728 */
			start_pix = 138;
			start_line_o = 22;
			start_line_e = 23;
			h_total = 1728;
			v_total = 625;
		} else if ((width == 1440) && (height == 480)) {
			/* for rgb 480i signal, it's 720/858, not 1440/1716 */
			start_pix = 114;
			start_line_o = 18;
			start_line_e = 19;
			h_total = 1716;
			v_total = 525;
		}
	}

	memset(&para, 0, sizeof(para));
	para.port  = TVIN_PORT_BT601_HDMI; /* TVIN_PORT_CAMERA; */
	para.bt_path = bt656_sel;
	para.frame_rate = frame_rate;
	para.h_active = width;
	para.v_active = height;
	if (field_flag) {
		if ((width == 1920) &&
			((height == 1080) || (height == 540))) {
			if (frame_rate == 60)
				para.fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ;
			else if (frame_rate == 50)
				para.fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A;
			/* para.v_active = 1080; */
		} else if ((width == 1440) &&
			((height == 576) || (height == 288))) {
			para.fmt = TVIN_SIG_FMT_HDMI_1440X576I_50HZ;
			/* para.v_active = 576; */
			set_invert_top_bot(true);
		}
	/*
	 *	else if( width == 720 &&  (height == 576 || height == 288)){
	 *		para.fmt = TVIN_SIG_FMT_HDMI_720X576I_50HZ;
	 *		para.v_active = 576;
	 *		set_invert_top_bot(true);
	 *	} else if( width == 720 &&  (height == 480 || height == 240)){
	 *		para.fmt = TVIN_SIG_FMT_HDMI_720X480I_60HZ;
	 *		para.v_active = 480;
	 *		set_invert_top_bot(true);
	 *	}
	 */
		else if ((width == 1440)  &&
			((height == 480) || (height == 240))) {
			para.fmt = TVIN_SIG_FMT_HDMI_1440X480I_60HZ;
			/* para.v_active = 480; */
			set_invert_top_bot(true);
		} else {
			para.fmt = TVIN_SIG_FMT_MAX+1;
			set_invert_top_bot(true);
		}
		para.scan_mode = TVIN_SCAN_MODE_INTERLACED;
		para.fid_check_cnt = timming.hs_width + timming.hs_backporch +
			timming.h_active;
	} else {
		if ((width == 1920) && (height == 1080))
			para.fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
		else if ((width == 1280) && (height == 720))
			para.fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
		else if (((width == 1440) || (width == 720)) && (height == 576))
			para.fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ;
		else if (((width == 1440) || (width == 720)) && (height == 480))
			para.fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
		else
			para.fmt = TVIN_SIG_FMT_MAX+1;
		/* para.fmt = TVIN_SIG_FMT_MAX; */
		para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		para.fid_check_cnt = 0;
	}
	para.hsync_phase = timming.hs_pol;
	para.vsync_phase = timming.vs_pol;
	para.hs_bp = timming.hs_backporch;
	para.vs_bp = timming.vs_backporch;
	para.cfmt = TVIN_YUV422;
	para.dfmt = TVIN_YUV422;
	para.skip_count =  0; /* skip_num */
	if (hdmirx_ext_debug_print) {
		pr_info("para frame_rate:   %d\n"
			"para h_active:     %d\n"
			"para v_active:     %d\n"
			"para hsync_phase:  %d\n"
			"para vsync_phase:  %d\n"
			"para hs_bp:        %d\n"
			"para vs_bp:        %d\n"
			"para scan_mode:    %d\n"
			"para fmt:          %d\n",
			para.frame_rate, para.h_active, para.v_active,
			para.hsync_phase, para.vsync_phase,
			para.hs_bp, para.vs_bp,
			para.scan_mode, para.fmt);
	}

	__vdin_set_pinmux(1);
	if (hdrv->vops) {
		RXEXTPR("%s: begin start_tvin_service()\n", __func__);
		hdrv->vops->start_tvin_service(vdin_sel, &para);
		hdrv->vdin.started = 1;
	} else {
		RXEXTERR("%s: v4l2_ops is NULL\n", __func__);
	}
	RXEXTPR("%s: finish start vdin\n", __func__);
}

void hdmirx_ext_start_vdin_mode(unsigned int mode)
{
	unsigned int height = 0, width = 0, frame_rate = 0, field_flag = 0;

	RXEXTPR("%s: start with mode = %d\n", __func__, mode);
	switch (mode) {
	case CEA_480I60:
		width = 1440;
		height = 480;
		frame_rate = 60;
		field_flag = 1;
		break;
	case CEA_480P60:
		width = 720;
		height = 480;
		frame_rate = 60;
		field_flag = 0;
		break;
	case CEA_576I50:
		width = 1440;
		height = 576;
		frame_rate = 50;
		field_flag = 1;
		break;
	case CEA_576P50:
		width = 720;
		height = 576;
		frame_rate = 50;
		field_flag = 0;
		break;
	case CEA_720P50:
		width = 1280;
		height = 720;
		frame_rate = 50;
		field_flag = 0;
		break;
	case CEA_720P60:
		width = 1280;
		height = 720;
		frame_rate = 60;
		field_flag = 0;
		break;
	case CEA_1080I60:
		width = 1920;
		height = 1080;
		frame_rate = 60;
		field_flag = 1;
		break;
	case CEA_1080P60:
		width = 1920;
		height = 1080;
		frame_rate = 60;
		field_flag = 0;
		break;
	case CEA_1080I50:
		width = 1920;
		height = 1080;
		frame_rate = 50;
		field_flag = 1;
		break;
	case CEA_1080P50:
		width = 1920;
		height = 1080;
		frame_rate = 50;
		field_flag = 0;
		break;
	default:
		RXEXTERR("%s: invalid video mode = %d!\n", __func__, mode);
		return;
	}

	hdmirx_ext_start_vdin(width, height, frame_rate, field_flag);
}
