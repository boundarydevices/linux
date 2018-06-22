/*
 * drivers/amlogic/media/vin/tvin/tvin_global.c
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

#include <linux/module.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "tvin_global.h"


const char *tvin_color_fmt_str(enum tvin_color_fmt_e color_fmt)
{
	switch (color_fmt) {
	case TVIN_RGB444:
		return "COLOR_FMT_RGB444";
	case TVIN_YUV422:
		return "COLOR_FMT_YUV422";
	case TVIN_YUV444:
		return "COLOR_FMT_YUV444";
	case TVIN_YUYV422:
		return "COLOR_FMT_TVIN_YUYV422";
	case TVIN_YVYU422:
		return "COLOR_FMT_TVIN_YVYU422";
	case TVIN_VYUY422:
		return "COLOR_FMT_TVIN_VYUY422";
	case TVIN_UYVY422:
		return "COLOR_FMT_TVIN_UYVY422";
	case TVIN_NV12:
		return "COLOR_FMT_TVIN_NV12";
	case TVIN_NV21:
		return "COLOR_FMT_TVIN_NV21";
	case TVIN_BGGR:
		return "COLOR_FMT_TVIN_BGGR";
	case TVIN_RGGB:
		return "COLOR_FMT_TVIN_RGGB";
	case TVIN_GBRG:
		return "COLOR_FMT_TVIN_GBRG";
	case TVIN_GRBG:
		return "COLOR_FMT_TVIN_GRBG";
	default:
		return "COLOR_FMT_NULL";
	}
}
EXPORT_SYMBOL(tvin_color_fmt_str);

const char *tvin_aspect_ratio_str(enum tvin_aspect_ratio_e aspect_ratio)
{
	switch (aspect_ratio) {
	case TVIN_ASPECT_1x1:
		return "TVIN_ASPECT_1x1";
	case TVIN_ASPECT_4x3_FULL:
		return "TVIN_ASPECT_4x3_FULL";
	case TVIN_ASPECT_14x9_FULL:
		return "TVIN_ASPECT_14x9_FULL";
	case TVIN_ASPECT_14x9_LB_CENTER:
		return "TVIN_ASPECT_14x9_LETTERBOX_CENTER";
	case TVIN_ASPECT_14x9_LB_TOP:
		return "TVIN_ASPECT_14x9_LETTERBOX_TOP";
	case TVIN_ASPECT_16x9_FULL:
		return "TVIN_ASPECT_16x9_FULL";
	case TVIN_ASPECT_16x9_LB_CENTER:
		return "TVIN_ASPECT_16x9_LETTERBOX_CENTER";
	case TVIN_ASPECT_16x9_LB_TOP:
		return "TVIN_ASPECT_16x9_LETTERBOX_TOP";
	default:
		return "TVIN_ASPECT_NULL";
	}
}
EXPORT_SYMBOL(tvin_aspect_ratio_str);

const char *tvin_port_str(enum tvin_port_e port)
{
	switch (port) {
	case TVIN_PORT_MPEG0:
		return "TVIN_PORT_MPEG0";
	case TVIN_PORT_BT656:
		return "TVIN_PORT_BT656";
	case TVIN_PORT_BT601:
		return "TVIN_PORT_BT601";
	case TVIN_PORT_CAMERA:
		return "TVIN_PORT_CAMERA";
	case TVIN_PORT_BT656_HDMI:
		return "TVIN_PORT_BT656_HDMI";
	case TVIN_PORT_BT601_HDMI:
		return "TVIN_PORT_BT601_HDMI";
	case TVIN_PORT_CVBS0:
		return "TVIN_PORT_CVBS0";
	case TVIN_PORT_CVBS1:
		return "TVIN_PORT_CVBS1";
	case TVIN_PORT_CVBS2:
		return "TVIN_PORT_CVBS2";
	case TVIN_PORT_CVBS3:
		return "TVIN_PORT_CVBS3";
	case TVIN_PORT_HDMI0:
		return "TVIN_PORT_HDMI0";
	case TVIN_PORT_HDMI1:
		return "TVIN_PORT_HDMI1";
	case TVIN_PORT_HDMI2:
		return "TVIN_PORT_HDMI2";
	case TVIN_PORT_HDMI3:
		return "TVIN_PORT_HDMI3";
	case TVIN_PORT_HDMI4:
		return "TVIN_PORT_HDMI4";
	case TVIN_PORT_HDMI5:
		return "TVIN_PORT_HDMI5";
	case TVIN_PORT_HDMI6:
		return "TVIN_PORT_HDMI6";
	case TVIN_PORT_HDMI7:
		return "TVIN_PORT_HDMI7";
	case TVIN_PORT_DVIN0:
		return "TVIN_PORT_DVIN0";
	case TVIN_PORT_VIU1:
		return "TVIN_PORT_VIU1";
	case TVIN_PORT_MIPI:
		return "TVIN_PORT_MIPI";
	case TVIN_PORT_ISP:
		return "TVIN_PORT_ISP";
	case TVIN_PORT_MAX:
		return "TVIN_PORT_MAX";
	default:
		return "TVIN_PORT_NULL";
	}
}
EXPORT_SYMBOL(tvin_port_str);

const char *tvin_sig_status_str(enum tvin_sig_status_e status)
{
	switch (status) {
	case TVIN_SIG_STATUS_NULL:
		return "TVIN_SIG_STATUS_NULL";
	case TVIN_SIG_STATUS_NOSIG:
		return "TVIN_SIG_STATUS_NOSIG";
	case TVIN_SIG_STATUS_UNSTABLE:
		return "TVIN_SIG_STATUS_UNSTABLE";
	case TVIN_SIG_STATUS_NOTSUP:
		return "TVIN_SIG_STATUS_NOTSUP";
	case TVIN_SIG_STATUS_STABLE:
		return "TVIN_SIG_STATUS_STABLE";
	default:
		return "TVIN_SIG_STATUS_NULL";
	}
}
EXPORT_SYMBOL(tvin_sig_status_str);

const char *tvin_trans_fmt_str(enum tvin_trans_fmt trans_fmt)
{
	switch (trans_fmt) {
	case TVIN_TFMT_2D:
		return "TVIN_TFMT_2D";
	case TVIN_TFMT_3D_LRH_OLOR:
		return "TVIN_TFMT_3D_LRH_OLOR";
	case TVIN_TFMT_3D_LRH_OLER:
		return "TVIN_TFMT_3D_LRH_OLER";
	case TVIN_TFMT_3D_LRH_ELOR:
		return "TVIN_TFMT_3D_LRH_ELOR";
	case TVIN_TFMT_3D_LRH_ELER:
		return "TVIN_TFMT_3D_LRH_ELER";
	case TVIN_TFMT_3D_TB:
		return "TVIN_TFMT_3D_TB";
	case TVIN_TFMT_3D_FP:
		return "TVIN_TFMT_3D_FP";
	case TVIN_TFMT_3D_FA:
		return "TVIN_TFMT_3D_FA";
	case TVIN_TFMT_3D_LA:
		return "TVIN_TFMT_3D_LA";
	case TVIN_TFMT_3D_LRF:
		return "TVIN_TFMT_3D_LRF";
	case TVIN_TFMT_3D_LD:
		return "TVIN_TFMT_3D_LD";
	case TVIN_TFMT_3D_LDGD:
		return "TVIN_TFMT_3D_LDGD";
	case TVIN_TFMT_3D_DET_TB:
		return "TVIN_TFMT_3D_DET_TB";
	case TVIN_TFMT_3D_DET_LR:
		return "TVIN_TFMT_3D_DET_LR";
	case TVIN_TFMT_3D_DET_INTERLACE:
		return "TVIN_TFMT_3D_DET_INTERLACE";
	case TVIN_TFMT_3D_DET_CHESSBOARD:
		return "TVIN_TFMT_3D_DET_CHESSBOARD";
	default:
		return "TVIN_TFMT_NULL";
	}
}
EXPORT_SYMBOL(tvin_trans_fmt_str);


MODULE_LICENSE("GPL");

