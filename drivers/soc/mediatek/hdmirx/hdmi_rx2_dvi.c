// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>

#include "mtk_hdmirx.h"
#include "hdmi_rx2_hw.h"
#include "vga_table.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx2_hdcp.h"
#include "hdmi_rx2_ctrl.h"
#include "hdmi_rx2_dvi.h"
#include "hdmi_rx2_aud_task.h"
#include "hdmirx_drv.h"

const char *szRxResStr[] = {
	"MODE_NOSIGNAL = 0",	/* No signal */
	"MODE_525I_OVERSAMPLE = 1",			   /* SDTV */
	"MODE_625I_OVERSAMPLE",				   /*  */
	"MODE_480P_OVERSAMPLE",				   /* SDTV */
	"MODE_576P_OVERSAMPLE",
	"MODE_720p_50",		/* HDTV */
	"MODE_720p_60",					   /* HDTV */
	"MODE_1080i_48",				   /* HDTV */
	"MODE_1080i_50",				   /* HDTV */
	"MODE_1080i",					   /* HDTV */
	"MODE_1080p_24",				   /* HDTV */
	"MODE_1080p_25",
	"MODE_1080p_30",
	"MODE_1080p_50",	/* HDTV */
	"MODE_1080p_60",
	"MODE_525I",
	"MODE_625I",
	"MODE_480P",
	"MODE_576P",
	"MODE_720p_24",
	"MODE_720p_25",
	"MODE_720p_30",
	"MODE_240P",
	"MODE_540P",
	"MODE_288P",
	"MODE_480P_24",
	"MODE_480P_30",
	"MODE_576P_25",
	"MODE_HDMI_640_480P",
	"MODE_HDMI_720p_24",
	"MODE_3D_720p_50_FP",
	"MODE_3D_720p_60_FP",
	"MODE_3D_1080p_24_FP",
	"MODE_3D_1080I_60_FP",
	"MODE_3D_480p_60_FP",
	"MODE_3D_576p_50_FP",
	"MODE_3D_720p_24_FP",
	"MODE_3D_720p_30_FP",
	"MODE_3D_1080p_30_FP",
	"MODE_3D_480I_60_FP",
	"MODE_3D_576I_60_FP",
	"MODE_3D_1080I_50_FP",
	"MODE_3D_1080p_50_FP",
	"MODE_3D_1080p_60_FP",
	"MODE_3D_1650_750_60_FP",
	"MODE_3D_1650_1500_30_FP",
	"MODE_3D_640_480p_60_FP",
	"MODE_3D_1440_240p_60_FP",
	"MODE_3D_1440_288p_50_FP",
	"MODE_3D_1440_576p_50_FP",
	"MODE_3D_720p_25_FP",
	"MODE_3D_1080p_25_FP",
	"MODE_3D_1080I_1250TOTAL_50_FP",
	"MODE_3D_1080p_24_SBS_FULL",
	"MODE_3D_1080p_25_SBS_FULL",
	"MODE_3D_1080p_30_SBS_FULL",
	"MODE_3D_1080I_50_SBS_FULL",
	"MODE_3D_1080I_60_SBS_FULL",
	"MODE_3D_720p_24_SBS_FULL",
	"MODE_3D_720p_30_SBS_FULL",
	"MODE_3D_720p_50_SBS_FULL",
	"MODE_3D_720p_60_SBS_FULL",
	"MODE_3D_480p_60_SBS_FULL",
	"MODE_3D_576p_50_SBS_FULL",
	"MODE_3D_480I_60_SBS_FULL",
	"MODE_3D_576I_50_SBS_FULL",
	"MODE_3D_640_480p_60_SBS_FULL",
	"MODE_3D_640_480p_60_LA",
	"MODE_3D_240p_60_LA",
	"MODE_3D_288p_50_LA",
	"MODE_3D_480p_60_LA",
	"MODE_3D_576p_50_LA",
	"MODE_3D_720p_24_LA",
	"MODE_3D_720p_60_LA",
	"MODE_3D_720p_50_LA",
	"MODE_3D_1080p_24_LA",
	"MODE_3D_1080p_25_LA",
	"MODE_3D_1080p_30_LA",
	"MODE_3D_480I_60_FA",
	"MODE_3D_576I_50_FA",
	"MODE_3D_1080I_60_FA",
	"MODE_3D_1080I_50_FA",
	"MODE_3D_MASTER_1080I_60_FA",
	"MODE_3D_MASTER_1080I_50_FA",
	"MODE_3D_480I_60_SBS_HALF",
	"MODE_3D_576I_50_SBS_HALF",
	"MODE_3D_1080I_60_SBS_HALF",
	"MODE_3D_1080I_50_SBS_HALF",
	"MODE_1080i_50_VID39", /* HDTV */
	"MODE_1080P_30_2640H", /* HDTV */
	"MODE_240P_60_3432H",
	"MODE_576i_50_3456H_FP",
	"MODE_576P_50_1728H_FP",
	"MODE_480P_60_3432H",
	"MODE_2576P_60_3456H",
	"MODE_3D_1440_480p_60_FP",
	"MODE_3D_240p_263LINES",
	"MODE_3840_1080P_24",	/* 4k1k */
	"MODE_3840_1080P_25",			       /* 4k1k */
	"MODE_3840_1080P_30",			       /* 4k1k */
	"MODE_3840_2160P_15",			       /* 4k2k */
	"MODE_3840_2160P_24",			       /* 4k2k */
	"MODE_3840_2160P_25",			       /* 4k2k */
	"MODE_3840_2160P_30",			       /* 4k2k */
	"MODE_4096_2160P_24",			       /* 4k2k */
	"MODE_4096_2160P_25",			       /* 4k2k */
	"MODE_4096_2160P_30",			       /* 4k2k */
	"MODE_4096_2160P_50",
	"MODE_4096_2160P_60",
	"MODE_3840_2160P_50",
	"MODE_3840_2160P_60",
	"MODE_4096_2160P_50_420",
	"MODE_4096_2160P_60_420",
	"MODE_3840_2160P_50_420",
	"MODE_3840_2160P_60_420",
	"MODE_MAX"
};

const char *dvi_state_str[] = {
	"DVI_NO_SIGNAL",
	"DVI_CHK_MODECHG",
	"DVI_WAIT_STABLE",
	"DVI_SATE_MAX",
};

bool fgDvi2HdResolution(u32 bRes)
{
	if ((bRes == MODE_525I) ||
		(bRes == MODE_625I) ||
		(bRes == MODE_480P) ||
		(bRes == MODE_576P) ||
		(bRes == MODE_480P_OVERSAMPLE) ||
		(bRes == MODE_576P_OVERSAMPLE) ||
		(bRes == MODE_525I_OVERSAMPLE) ||
		(bRes == MODE_625I_OVERSAMPLE) ||
		(bRes == MODE_HDMI_640_480P))
		return FALSE;
	return TRUE;
}


#ifdef CC_SUPPORT_HDR10Plus
static enum E_DVI_HDR10P_STATUS
_eDvi2GetCurrentHdr10PlusStatus(struct MTK_HDMI *myhdmi)
{
	enum E_DVI_HDR10P_STATUS eDviHdr10PStatus = DVI_HDR10P_NONE;

	if (vHDMI2GetHdr10PlusEMPExist(myhdmi))
		eDviHdr10PStatus = DVI_HDR10P_EMP;
	else if (vHDMI2GetHdr10PlusVSIExist(myhdmi))
		eDviHdr10PStatus = DVI_HDR10P_VSI;
	else
		eDviHdr10PStatus = DVI_HDR10P_NONE;

	return eDviHdr10PStatus;
}

static bool
_fgDvi2ChkHdr10PlusChange(struct MTK_HDMI *myhdmi)
{
	enum E_DVI_HDR10P_STATUS eDviHdr10PStatus;

	eDviHdr10PStatus = _eDvi2GetCurrentHdr10PlusStatus(myhdmi);
	if (eDviHdr10PStatus != myhdmi->dvi_s._rHdr10PStatus)
		return TRUE;
	return FALSE;
}

u8
bDvi2GetCurrentHdr10PlusStatus(struct MTK_HDMI *myhdmi)
{
	return (u8)_eDvi2GetCurrentHdr10PlusStatus(myhdmi);
}
#endif

static bool
dvi2_check_not_support_timing(struct MTK_HDMI *myhdmi)
{
	u8 DviTiming = myhdmi->dvi_s.DviTiming;
	bool fgTimingNotSupport = FALSE;

	if (DviTiming != MODE_NOSIGNAL) {
		if (!dvi2_is_3d_pkt_valid(myhdmi)) {
			if (hdmi2com_is_interlace(myhdmi) &&
				(hdmi2com_frame_rate(myhdmi) > 80))
				fgTimingNotSupport = TRUE;
		}
	}
	if (dvi2_is_3d_pkt_valid(myhdmi)) {
		if ((myhdmi->dvi_s.DviWidth == 4096 ||
			    myhdmi->dvi_s.DviWidth == 3840) &&
			(myhdmi->dvi_s.DviHeight == 2160))
			fgTimingNotSupport = TRUE;
	}
	return fgTimingNotSupport;
}

u8
dvi2_3d_timing_search(struct MTK_HDMI *myhdmi, u8 u1OriSearch)
{
	u8 bSearch = u1OriSearch;

	if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_FramePacking) {
		if (bSearch == MODE_576P_OVERSAMPLE) {
			bSearch = MODE_3D_1440_576p_50_FP;
			RX_DET_LOG("[RX]3D FB: %d\n", bSearch);
			return bSearch;
		}
	} else if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_SideBySideFull) {
		if (bSearch == MODE_525I_OVERSAMPLE) {
			bSearch = MODE_3D_480I_60_SBS_FULL;
			RX_DET_LOG("[RX]3D SBS full: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_625I_OVERSAMPLE) {
			bSearch = MODE_3D_576I_50_SBS_FULL;
			RX_DET_LOG("[RX]3D SBS full: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_480P_OVERSAMPLE) {
			bSearch = MODE_3D_480p_60_SBS_FULL;
			RX_DET_LOG("[RX]3D SBS full: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_576P_OVERSAMPLE) {
			bSearch = MODE_3D_576p_50_SBS_FULL;
			RX_DET_LOG("[RX]3D SBS full: %d\n", bSearch);
			return bSearch;
		}
	} else if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_LineAlternative) {
		if (bSearch == MODE_480P_OVERSAMPLE) {
			bSearch = MODE_3D_240p_60_LA;
			RX_DET_LOG("[RX]3D LA: %d\n",	bSearch);
			return bSearch;
		} else if (bSearch == MODE_576P_OVERSAMPLE) {
			bSearch = MODE_3D_288p_50_LA;
			RX_DET_LOG("[RX]3D LA: %d\n",	bSearch);
			return bSearch;
		}
	} else if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_FieldAlternative) {
		if (bSearch == MODE_480P_OVERSAMPLE) {
			bSearch = MODE_3D_480I_60_FA;
			RX_DET_LOG("[RX]3D FA: %d\n",	bSearch);
			return bSearch;
		} else if (bSearch == MODE_576P_OVERSAMPLE) {
			bSearch = MODE_3D_576I_50_FA;
			RX_DET_LOG("[RX]3D FA: %d\n",	bSearch);
			return bSearch;
		} else if (bSearch == MODE_1080p_50) {
			bSearch = MODE_3D_1080I_50_FA;
			RX_DET_LOG("[RX]3D FA: %d\n",	bSearch);
			return bSearch;
		} else if (bSearch == MODE_1080p_60) {
			bSearch = MODE_3D_1080I_60_FA;
			RX_DET_LOG("[RX]3D FA: %d\n",	bSearch);
			return bSearch;
		}
	} else if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_SideBySideHalf) {
		if (bSearch == MODE_525I) {
			bSearch = MODE_3D_480I_60_SBS_HALF;
			RX_DET_LOG("[RX]3D SBS half: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_625I) {
			bSearch = MODE_3D_576I_50_SBS_HALF;
			RX_DET_LOG("[RX]3D SBS half: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_1080i) {
			bSearch = MODE_3D_1080I_60_SBS_HALF;
			RX_DET_LOG("[RX]3D SBS half: %d\n", bSearch);
			return bSearch;
		} else if (bSearch == MODE_1080i_50) {
			bSearch = MODE_3D_1080I_50_SBS_HALF;
			RX_DET_LOG("[RX]3D SBS half: %d\n", bSearch);
			return bSearch;
		}
	}

	RX_DET_LOG("[RX]3D timing table: %d\n", bSearch);
	return bSearch;
}

u8
dvi2_check_3d_format(struct MTK_HDMI *myhdmi)
{
	if ((myhdmi->dvi_s.DviTiming == MODE_3D_720p_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_24_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_480p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_576p_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_24_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_30_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_30_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_640_480p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1440_240p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1440_288p_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1440_576p_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_25_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_25_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1440_480p_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_240p_263LINES))
		return DVI_FP_progressive;

	if ((myhdmi->dvi_s.DviTiming == MODE_3D_1080I_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_480I_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_576I_60_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080I_50_FP) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080I_1250TOTAL_50_FP))
		return DVI_FP_interlace;

	if ((myhdmi->dvi_s.DviTiming == MODE_3D_1080p_30_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_25_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080p_24_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_24_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_30_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_50_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_720p_60_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_480p_60_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_576p_50_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_640_480p_60_SBS_FULL))
		return DVI_SBS_full_progressive;

	if ((myhdmi->dvi_s.DviTiming == MODE_3D_1080I_50_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080I_60_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_480I_60_SBS_FULL) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_576I_50_SBS_FULL))
		return DVI_SBS_full_interlace;

	if ((myhdmi->dvi_s.DviTiming == MODE_3D_480I_60_FA) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080I_60_FA) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_1080I_50_FA) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_MASTER_1080I_60_FA) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_MASTER_1080I_50_FA) ||
		(myhdmi->dvi_s.DviTiming == MODE_3D_576I_50_FA))
		return DVI_Field_Alternative;

	return 6;
}

void
dvi2_init(struct MTK_HDMI *myhdmi)
{
	myhdmi->dvi_s.DviTiming = MODE_WAIT;
}

bool
dvi2_is_mode_change(struct MTK_HDMI *myhdmi)
{
	return (bool)myhdmi->dvi_s.bDviModeChgFlg;
}

u16
dvi2_get_input_width(struct MTK_HDMI *myhdmi)
{
	u16 u2Hactive;

	if (dvi2_is_3d_pkt_valid(myhdmi)) {
		u2Hactive = dvi2_get_3d_h_active(myhdmi);
	} else {
		u2Hactive = hdmi2com_h_active(myhdmi);
		if (hdmi2_is_420_mode(myhdmi))
			u2Hactive = u2Hactive * 2;
	}

	return u2Hactive;
}

u16
dvi2_get_input_height(struct MTK_HDMI *myhdmi)
{
	u16 u2Height = 0;

	if (dvi2_is_3d_pkt_valid(myhdmi))
		return dvi2_get_3d_v_active(myhdmi);

	if (dvi2_is_video_timing(myhdmi))
		return (u16)(hdmi2com_v_active(myhdmi)
					<< hdmi2com_is_interlace(myhdmi));

	u2Height = (u16)hdmi2com_v_active(myhdmi);
	if ((myhdmi->dvi_s.DviTiming == MODE_DE_MODE) &&
		((u2Height == 517) || (u2Height == 518))) {
		u2Height = 517;
		return u2Height;
	}

	return u2Height;
}

bool
dvi2_is_hdmi_mode(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_hdmi_mode(myhdmi) == HDMI_RX_MODE_DVI)
		return FALSE;

	return TRUE;
}

u8
dvi2_get_refresh_rate(struct MTK_HDMI *myhdmi)
{
	u8 u1FrameRate;

	u1FrameRate = (u8)hdmi2com_frame_rate(myhdmi);

	if (dvi2_is_3d_pkt_valid(myhdmi))
		return dvi2_get_3d_refresh_rate(myhdmi);
	else
		return (u8)hdmi2com_frame_rate(myhdmi);
}

bool
dvi2_is_video_timing(struct MTK_HDMI *myhdmi)
{
	return (bool)fgIsVideoTiming(myhdmi->dvi_s.DviTiming);
}

void
dvi2_set_mode_change(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->dvi_s.bDviModeChgFlg)
		return;

	myhdmi->dvi_s.bDviModeChgFlg = 1;
}

u8
dvi2_is_3d_pkt_valid(struct MTK_HDMI *myhdmi)
{
	/* hdmi2_vsi_info_parser(myhdmi); */

	if (myhdmi->Hdmi3DInfo.HDMI_3D_Enable)
		return 1;

	if ((dvi2_check_3d_format(myhdmi) >= DVI_FP_progressive) &&
		(dvi2_check_3d_format(myhdmi) <= DVI_SBS_full_interlace))
		return 1;

	return 0;
}

u32
dvi2_get_3d_v_active(struct MTK_HDMI *myhdmi)
{
	u8 DviTiming = myhdmi->dvi_s.DviTiming;

	switch (myhdmi->Hdmi3DInfo.HDMI_3D_Str) {
	case HDMI_3D_FramePacking:
		if (DVI_FP_progressive == dvi2_check_3d_format(myhdmi)) {
			return (Get_VGAMODE_IPV_LEN(DviTiming) -
				Get_VGAMODE_IPV_STA(DviTiming)) >> 1;
		} else if (DVI_FP_interlace == dvi2_check_3d_format(myhdmi)) {
			return (Get_VGAMODE_IPV_LEN(DviTiming) -
				Get_VGAMODE_IPV_STA(DviTiming) * 3 - 2) >> 1;
		} else {
			return hdmi2com_v_active(myhdmi);
		}
		break;
	case HDMI_3D_FieldAlternative:
		if ((DviTiming == MODE_3D_480I_60_FA) ||
			(DviTiming == MODE_3D_1080I_60_FA) ||
			(DviTiming == MODE_3D_1080I_50_FA) ||
			(DviTiming == MODE_3D_MASTER_1080I_60_FA) ||
			(DviTiming == MODE_3D_MASTER_1080I_50_FA) ||
			(DviTiming == MODE_3D_576I_50_FA))
			return Get_VGAMODE_IPV_LEN(DviTiming);
		else if (DviTiming == MODE_DE_MODE)
			return hdmi2com_v_active(myhdmi);
		break;
	case HDMI_3D_SideBySideFull:
		if ((DVI_SBS_full_progressive ==
			dvi2_check_3d_format(myhdmi)) ||
			(DVI_SBS_full_interlace ==
			dvi2_check_3d_format(myhdmi))) {
			return Get_VGAMODE_IPV_LEN(DviTiming);
		}
		if (hdmi2com_is_interlace(myhdmi))
			return hdmi2com_v_active(myhdmi) << 1;
		else
			return hdmi2com_v_active(myhdmi);
		break;
	case HDMI_3D_LineAlternative:
		return hdmi2com_v_active(myhdmi);
	case HDMI_3D_SideBySideHalf:
	case HDMI_3D_TopBottom:
		if (hdmi2com_is_interlace(myhdmi)) {
			if (dvi2_is_video_timing(myhdmi))
				return Get_VGAMODE_IPV_LEN(DviTiming);
			else
				return hdmi2com_v_active(myhdmi);
		} else {
			return hdmi2com_v_active(myhdmi);
		}
		break;
	default:
		return hdmi2com_v_active(myhdmi);
	}

	return hdmi2com_v_active(myhdmi);
}

u32
dvi2_get_3d_h_active(struct MTK_HDMI *myhdmi)
{
	u8 DviTiming = myhdmi->dvi_s.DviTiming;

	switch (myhdmi->Hdmi3DInfo.HDMI_3D_Str) {
	case HDMI_3D_FramePacking:
	case HDMI_3D_FieldAlternative:
	case HDMI_3D_LineAlternative:
	case HDMI_3D_SideBySideFull:
	case HDMI_3D_SideBySideHalf:
	case HDMI_3D_TopBottom:
	default:
		if (dvi2_is_video_timing(myhdmi))
			return Get_VGAMODE_IPH_WID(DviTiming);
		else
			return hdmi2com_h_active(myhdmi);
	}
}

u8
dvi2_get_3d_refresh_rate(struct MTK_HDMI *myhdmi)
{
	u8 DviTiming = myhdmi->dvi_s.DviTiming;
	u8 DviVclk = myhdmi->dvi_s.DviVclk;

	switch (myhdmi->Hdmi3DInfo.HDMI_3D_Str) {
	case HDMI_3D_FramePacking:
		if (DviTiming == MODE_DE_MODE)
			return DviVclk * 2;
		else if (DVI_FP_interlace == dvi2_check_3d_format(myhdmi))
			return Get_VGAMODE_IVF(DviTiming) * 4;
		else
			return Get_VGAMODE_IVF(DviTiming) * 2;
		break;
	case HDMI_3D_FieldAlternative:
		if ((DviTiming == MODE_3D_480I_60_FA) ||
			(DviTiming == MODE_3D_1080I_60_FA) ||
			(DviTiming == MODE_3D_1080I_50_FA) ||
			(DviTiming == MODE_3D_MASTER_1080I_60_FA) ||
			(DviTiming == MODE_3D_MASTER_1080I_50_FA) ||
			(DviTiming == MODE_3D_576I_50_FA))
			return Get_VGAMODE_IVF(DviTiming) * 2;
		else if (DviTiming == MODE_DE_MODE)
			return DviVclk * 2;
		break;
	case HDMI_3D_SideBySideFull:
		if ((DVI_SBS_full_progressive ==
			dvi2_check_3d_format(myhdmi)) ||
			(DVI_SBS_full_interlace ==
			dvi2_check_3d_format(myhdmi)))
			return Get_VGAMODE_IVF(DviTiming);
		else
			return DviVclk;
		break;
	case HDMI_3D_LineAlternative:
	case HDMI_3D_SideBySideHalf:
	case HDMI_3D_TopBottom:
	default:
		break;
	}

	return DviVclk;
}

u8
dvi2_get_interlaced(struct MTK_HDMI *myhdmi)
{
	u8 DviTiming = myhdmi->dvi_s.DviTiming;

	if (DviTiming == MODE_DE_MODE)
		return 0;

	if ((DviTiming != MODE_NOSIGNAL) &&
	(DviTiming < MAX_TIMING_FORMAT)) {
		if (DVI_FP_interlace ==
		dvi2_check_3d_format(myhdmi))
			return 1;
		else if (DVI_Field_Alternative ==
		dvi2_check_3d_format(myhdmi))
			return 1;
		else
			return Get_VGAMODE_INTERLACE(DviTiming);
	} else {
		return 0;
	}
}

void
dvi2_wait_res_stable(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_state(myhdmi) == rx_is_stable) {
		myhdmi->dvi_s.DviHtotal = hdmi2com_h_total(myhdmi);
		myhdmi->dvi_s.DviVtotal = hdmi2com_v_total(myhdmi);
		myhdmi->dvi_s.DviWidth = hdmi2com_h_active(myhdmi);
		myhdmi->dvi_s.DviHeight = hdmi2com_v_active(myhdmi);
		myhdmi->dvi_s.DviPixClk = hdmi2com_pixel_clk(myhdmi);
		myhdmi->dvi_s.DviVclk = hdmi2com_frame_rate(myhdmi);
		myhdmi->dvi_s.DviHclk = hdmi2com_h_clk(myhdmi);
		myhdmi->dvi_s.Interlace = hdmi2com_is_interlace(myhdmi);

		myhdmi->is_detected = FALSE;
		hdmi_rx_video_notify(myhdmi, VSW_NFY_RES_CHGING);
		dvi2_init(myhdmi);
		dvi2_set_mode_change(myhdmi);
	}

	myhdmi->dvi_s.DviHtotal = hdmi2com_h_total(myhdmi);
	myhdmi->dvi_s.DviVtotal = hdmi2com_v_total(myhdmi);
}

void
dvi2_sync_color_related_change(struct MTK_HDMI *myhdmi)
{
	u8 _bHDMIScanInfo;
	u8 _bHDMIAspectRatio;
	u8 _bHDMIAFD;
	u8 _bCurHDMIHDCP1Status;
	u8 _bCurHDMIHDCP2Status;
	u8 _bHDMI422Input;
	u8 ITCFlag;
	u8 ITCContent;

	_bHDMIScanInfo = hdmi2com_get_scan_info(myhdmi);
	_bHDMIAspectRatio = hdmi2com_get_aspect_ratio(myhdmi);
	_bHDMIAFD = hdmi2com_get_afd_info(myhdmi);
	ITCFlag = hdmi2com_get_itc_flag(myhdmi);
	ITCContent = hdmi2com_get_itc_content(myhdmi);
	_bHDMI422Input = hdmi2com_is_422_input(myhdmi);
	_bCurHDMIHDCP1Status = (hdmi2_get_hdcp1x_status(myhdmi) & 0x5);
	_bCurHDMIHDCP2Status = (hdmi2_get_hdcp2x_status(myhdmi) & 0x81);

	myhdmi->HdmiScan = _bHDMIScanInfo;
	myhdmi->HdmiAspect = _bHDMIAspectRatio;
	myhdmi->HdmiAFD = _bHDMIAFD;
	myhdmi->HdmiITC.fgItc = ITCFlag;
	myhdmi->HdmiITC.ItcContent = ITCContent;
	myhdmi->Hdcp1xState = _bCurHDMIHDCP1Status;
	myhdmi->Hdcp2xState = _bCurHDMIHDCP2Status;
	myhdmi->Is422 = _bHDMI422Input;
}

void
dvi2_check_color_related_change(struct MTK_HDMI *myhdmi)
{
	u8 _bHDMIScanInfo;
	u8 _bHDMIAspectRatio;
	u8 _bHDMIAFD;
	u8 _bDetHDMIHDCP1Status;
	u8 _bDetHDMIHDCP2Status;
	u8 _bHDMI422Input;
	u8 ITCFlag;
	u8 ITCContent;

	_bHDMIScanInfo = myhdmi->HdmiScan;
	_bHDMIAspectRatio = myhdmi->HdmiAspect;
	_bHDMIAFD = myhdmi->HdmiAFD;
	ITCFlag = myhdmi->HdmiITC.fgItc;
	ITCContent = myhdmi->HdmiITC.ItcContent;
	_bDetHDMIHDCP1Status = myhdmi->Hdcp1xState;
	_bDetHDMIHDCP2Status = myhdmi->Hdcp2xState;
	_bHDMI422Input = myhdmi->Is422;

	if ((_bHDMIScanInfo != hdmi2com_get_scan_info(myhdmi)) ||
	(_bHDMIAspectRatio != hdmi2com_get_aspect_ratio(myhdmi)) ||
	(_bHDMIAFD != hdmi2com_get_afd_info(myhdmi)) ||
	(_bHDMI422Input != hdmi2com_is_422_input(myhdmi)) ||
	(ITCFlag != hdmi2com_get_itc_flag(myhdmi)) ||
	(ITCContent != hdmi2com_get_itc_content(myhdmi)) ||
	(_bDetHDMIHDCP1Status != (hdmi2_get_hdcp1x_status(myhdmi) & 0x5)) ||
	(_bDetHDMIHDCP2Status != (hdmi2_get_hdcp2x_status(myhdmi) & 0x81))) {
		if (myhdmi->dvi_s.InfoChgCnt++ > 3) {
			myhdmi->dvi_s.InfoChgCnt = 0;
			myhdmi->HdmiScan = hdmi2com_get_scan_info(myhdmi);
			myhdmi->HdmiAspect = hdmi2com_get_aspect_ratio(myhdmi);
			myhdmi->HdmiAFD = hdmi2com_get_afd_info(myhdmi);
			myhdmi->Hdcp1xState =
				(hdmi2_get_hdcp1x_status(myhdmi) & 0x5);
			myhdmi->Hdcp2xState =
				(hdmi2_get_hdcp2x_status(myhdmi) & 0x81);
			myhdmi->HdmiITC.fgItc = hdmi2com_get_itc_flag(myhdmi);
			myhdmi->HdmiITC.ItcContent =
				hdmi2com_get_itc_content(myhdmi);
			myhdmi->Is422 = hdmi2com_is_422_input(myhdmi);

			if ((_bHDMIAspectRatio !=
			hdmi2com_get_aspect_ratio(myhdmi)) ||
			(_bHDMIAFD != hdmi2com_get_afd_info(myhdmi)))
				hdmi_rx_video_notify(myhdmi,
					VSW_NFY_ASPECT_CHG);

			if (_bHDMI422Input != hdmi2com_is_422_input(myhdmi))
				hdmi_rx_video_notify(myhdmi,
					VSW_NFY_CS_CHG);
		}
	} else {
		myhdmi->dvi_s.InfoChgCnt = 0;
	}
}

void
dvi2_timer_handle(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_state(myhdmi) == rx_is_stable) {
		if (myhdmi->wait_support_timing < 255)
			myhdmi->wait_support_timing++;
	} else {
		myhdmi->wait_support_timing = 0;
	}
}

void
dvi2_set_switch(struct MTK_HDMI *myhdmi, u8 sel)
{
	switch (sel) {
	case rx_detect_ckdt:
		myhdmi->is_detected = TRUE;
		break;
	case rx_is_stable:
		myhdmi->wait_support_timing = 0;
		break;
	default:
		break;
	}
}

void
dvi2_check_mode_change(struct MTK_HDMI *myhdmi)
{
	u8 DviChkState;

	DviChkState = myhdmi->dvi_s.DviChkState;

	if (hdmi2_is_un_plug_in(myhdmi)) {
		hdmi2_un_plug_in_clr(myhdmi);
		RX_DET_LOG("[RX]unplug deteted\n");
		dvi2_init(myhdmi);
		DviChkState = DVI_NO_SIGNAL;
		myhdmi->dvi_s.DviTiming = MODE_NOSIGNAL;
		if (aud2_get_state(myhdmi) != ASTATE_AudioOff)
			aud2_set_state(myhdmi, ASTATE_AudioOff);
		hdmi_rx_video_notify(myhdmi, VSW_NFY_UNLOCK);
		dvi2_set_mode_change(myhdmi);
		dvi2_set_mode_done(myhdmi);
	}

	if (DviChkState != DVI_NO_SIGNAL) {
		if (hdmi2_get_state(myhdmi) != rx_is_stable) {
			RX_DET_LOG("[RX]timing un-stb\n");
			DviChkState = DVI_NO_SIGNAL;
			myhdmi->dvi_s.DviChkState = DviChkState;
			dvi2_set_mode_change(myhdmi);
			dvi2_init(myhdmi);
			myhdmi->dvi_s.DviTiming = MODE_NOSIGNAL;
			myhdmi->dvi_s.NoSigalCnt = 0;
			if (aud2_get_state(myhdmi) != ASTATE_AudioOff)
				aud2_set_state(myhdmi, ASTATE_AudioOff);
			hdmi_rx_video_notify(myhdmi, VSW_NFY_UNLOCK);
			dvi2_set_mode_done(myhdmi);
		}
	}

	if (myhdmi->dvi_state_old != DviChkState) {
		if (myhdmi->dvi_state_old >= DVI_SATE_MAX)
			myhdmi->dvi_state_old = DviChkState;

		if ((myhdmi->dvi_state_old < DVI_SATE_MAX) &&
			(DviChkState < DVI_SATE_MAX))
			RX_DET_LOG("[RX]dvi state from %s to %s,%lu\n",
				dvi_state_str[myhdmi->dvi_state_old],
				dvi_state_str[DviChkState], jiffies);
		else
			RX_DEF_LOG("[RX]unknown dvi state\n");

		myhdmi->dvi_state_old = DviChkState;
	}

	if (!((hdmi2_get_state(myhdmi) == rx_is_stable) ||
		    hdmi2_is_sync_lose(myhdmi)))
		return;

	switch (DviChkState) {
	case DVI_NO_SIGNAL:
		if (hdmi2_get_state(myhdmi) == rx_is_stable)
			DviChkState = DVI_WAIT_STABLE;
		else
			break;

	case DVI_WAIT_STABLE:
		if (myhdmi->dvi_s.HdmiMD != hdmi2com_is_hdmi_mode(myhdmi))
			myhdmi->dvi_s.HdmiMD = hdmi2com_is_hdmi_mode(myhdmi);
		dvi2_wait_res_stable(myhdmi);
		break;

	case DVI_CHK_MODECHG:
		if (hdmi2com_input_color_space_type(myhdmi) !=
			myhdmi->dvi_s.HDMICS) {
			hdmi2_set_video_mute(myhdmi, TRUE, TRUE);
			RX_DET_LOG("[RX]color space from %d to %d\n",
				myhdmi->dvi_s.HDMICS,
				hdmi2com_input_color_space_type(myhdmi));

			myhdmi->dvi_s.HDMICS =
				hdmi2com_input_color_space_type(myhdmi);
			DviChkState = DVI_WAIT_STABLE;
			dvi2_init(myhdmi);
			dvi2_set_mode_change(myhdmi);
		}

		dvi2_check_color_related_change(myhdmi);

		if (myhdmi->dvi_s.HdmiMD !=
		hdmi2com_is_hdmi_mode(myhdmi)) {
			hdmi_rx_video_notify(myhdmi, VSW_NFY_RES_CHGING);

			if (myhdmi->dvi_s.HdmiMD !=
			hdmi2com_is_hdmi_mode(myhdmi)) {
				myhdmi->dvi_s.HdmiMD =
					hdmi2com_is_hdmi_mode(myhdmi);
				RX_DET_LOG("[RX]HDMI mode change to %d\n",
					myhdmi->dvi_s.HdmiMD);
			}

			if (hdmi2_is_sync_lose(myhdmi)) {
				hdmi2_set_sync_lose(myhdmi, 0);
				RX_DET_LOG("[RX]sync lost\n");
			}

			DviChkState = DVI_WAIT_STABLE;
			dvi2_init(myhdmi);
			dvi2_set_mode_change(myhdmi);
		}

#ifdef CC_SUPPORT_HDR10Plus
		if (_fgDvi2ChkHdr10PlusChange(myhdmi)) {
			DviChkState = DVI_WAIT_STABLE;
			dvi2_init(myhdmi);
			dvi2_set_mode_change(myhdmi);
		}
#endif

		aud2_main_task(myhdmi);
		break;

	default:
		break;
	}

	myhdmi->dvi_s.DviChkState = DviChkState;
}

u8
dvi2_stb_timing_search(struct MTK_HDMI *myhdmi)
{
	u8 bSearch;
	u8 bSearchEnd;
	u8 DviVclk;
	u16 DviHclk;
	u16 DviVtotal;
	u16 DviHtotal;
	u8 _bInterlaced;
	u32 _wHeight, _wWidth;

	DviHtotal = myhdmi->dvi_s.DviHtotal;
	DviVtotal = myhdmi->dvi_s.DviVtotal;
	DviVclk = myhdmi->dvi_s.DviVclk;
	DviHclk = myhdmi->dvi_s.DviHclk;
	_bInterlaced = myhdmi->dvi_s.Interlace;
	_wHeight = myhdmi->dvi_s.DviHeight;
	_wWidth = myhdmi->dvi_s.DviWidth;
	RX_DET_LOG("[RX]%s>\n", __func__);
	RX_DET_LOG("[RX]>%d/%d/%d/%d/%d/%d/%d\n",
		DviHtotal,
		DviVtotal,
		DviVclk,
		DviHclk,
		_bInterlaced,
		_wHeight,
		_wWidth);

	if (DviVclk <= 30)
		_bInterlaced = 0;

	bSearch = HDTV_SEARCH_START;
	bSearchEnd = HDTV_SEARCH_END;

	do {
		if ((DviVclk >= (Get_VGAMODE_IVF(bSearch) - 2)) &&
			(DviVclk <= (Get_VGAMODE_IVF(bSearch) + 2)) &&
			(DviHclk >= (Get_VGAMODE_IHF(bSearch) - 17)) &&
			(DviHclk <= (Get_VGAMODE_IHF(bSearch) + 17))) {

			/* check interlaced */
			if (!dvi2_is_3d_pkt_valid(myhdmi) &&
			((Get_VGAMODE_INTERLACE(bSearch)) !=
			_bInterlaced)) {
				RX_DET_LOG(
				"[RX]Get_VGAMODE_INTERLACE(bSearch): %d\n",
				Get_VGAMODE_INTERLACE(bSearch));
				RX_DET_LOG(
				"[RX]fgHDMIinterlaced: %d,_bInterlaced: %d\n",
				hdmi2com_is_interlace(myhdmi), _bInterlaced);
				continue;
			}

		RX_DET_LOG("[RX]timing search\n");
		RX_DET_LOG("[RX]bSearch= %d, id=%d\n",
			bSearch, Get_VGAMODE_ID(bSearch));
		RX_DET_LOG("[RX]IVF=%d\n",
			 Get_VGAMODE_IVF(bSearch));
		RX_DET_LOG("[RX]IHF=%d\n",
			 Get_VGAMODE_IHF(bSearch));
		RX_DET_LOG("[RX]IHTOTAL=%d\n",
			 Get_VGAMODE_IHTOTAL(bSearch));
		RX_DET_LOG("[RX]IPH_WID=%d\n",
			 Get_VGAMODE_IPH_WID(bSearch));
		RX_DET_LOG("[RX]IPV_LEN=%d\n",
			Get_VGAMODE_IPV_LEN(bSearch));
		RX_DET_LOG("[RX]INTERLACE=%d\n",
			Get_VGAMODE_INTERLACE(bSearch));
		RX_DET_LOG("[RX]dvi2_is_3d_pkt_valid=%d,0x%x\n",
			dvi2_is_3d_pkt_valid(myhdmi),
			myhdmi->Hdmi3DInfo.HDMI_3D_Str);

		if ((DviHtotal <= (Get_VGAMODE_IHTOTAL(bSearch) - 40)) ||
			(DviHtotal >= (Get_VGAMODE_IHTOTAL(bSearch) + 40)))
			continue;

		if (dvi2_is_3d_pkt_valid(myhdmi) &&
			(myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_FieldAlternative) &&
			_bInterlaced) {
			_bInterlaced = 0;
		}

		if (((_wHeight << _bInterlaced) >
		(Get_VGAMODE_IPV_LEN(bSearch) - 3)) &&
		((_wHeight << _bInterlaced) <
		(Get_VGAMODE_IPV_LEN(bSearch) + 3))) {
			if (Get_VGAMODE_IPV_LEN(bSearch) == 2160) {
				if ((_wWidth >
				(Get_VGAMODE_IPH_WID(bSearch) - 3))	&&
				(_wWidth <
				(Get_VGAMODE_IPH_WID(bSearch) + 3)))
					return Get_VGAMODE_ID(bSearch);
				} else
					return Get_VGAMODE_ID(bSearch);
			}
		}
	} while (++bSearch <= bSearchEnd);

	return MODE_NOSUPPORT;
}

void
dvi2_set_mode_done(struct MTK_HDMI *myhdmi)
{

	if (!myhdmi->dvi_s.bDviModeChgFlg)
		return;

	myhdmi->dvi_s.HDMICS =
		hdmi2com_input_color_space_type(myhdmi);

	myhdmi->dvi_s.bDviModeChgFlg = 0;
}

void
dvi2_mode_detected(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	if (hdmi2_get_state(myhdmi) != rx_is_stable) {
		myhdmi->dvi_s.DviTiming = MODE_NOSIGNAL;
		myhdmi->dvi_s.DviChkState = DVI_WAIT_STABLE;
		myhdmi->is_detected = TRUE;
	} else {
		myhdmi->dvi_s.DviTiming = dvi2_stb_timing_search(myhdmi);
		RX_DEF_LOG("[RX]DviTiming=%d\n", myhdmi->dvi_s.DviTiming);

		if (myhdmi->dvi_s.DviTiming == MODE_720p_60) {
			if (myhdmi->dvi_s.DviHtotal >= 1660)
				myhdmi->dvi_s.DviTiming = MODE_DE_MODE;
		} else if (myhdmi->dvi_s.DviTiming == MODE_NOSUPPORT) {
			myhdmi->dvi_s.DviTiming = MODE_DE_MODE;
		}

		if (dvi2_check_not_support_timing(myhdmi))
			myhdmi->dvi_s.DviTiming = MODE_NOSUPPORT;

		if (((myhdmi->dvi_s.DviTiming == MODE_NOSUPPORT) ||
			(myhdmi->dvi_s.DviTiming == MODE_DE_MODE)) &&
			(myhdmi->wait_support_timing <
			HDMI_RX_DVI_WAIT_SPPORT_TIMING_COUNT)) {
			myhdmi->is_detected = TRUE;
			myhdmi->dvi_s.DviChkState = DVI_WAIT_STABLE;

		} else {
			if (myhdmi->dvi_s.DviTiming != MODE_NOSUPPORT)
				aud2_set_state(myhdmi, ASTATE_RequestAudio);

			hdmi2com_set_hv_sync_pol(myhdmi, TRUE);

			hdmi2_set_video_mute(myhdmi, FALSE, TRUE);

			hdmi_rx_video_notify(myhdmi, VSW_NFY_RES_CHG_DONE);

			dvi2_set_mode_done(myhdmi);
			dvi2_sync_color_related_change(myhdmi);

			if (myhdmi->dvi_s.DviTiming < MODE_MAX)
				RX_DEF_LOG("[RX]timing=%d,%s\n",
					myhdmi->dvi_s.DviTiming,
					szRxResStr[myhdmi->dvi_s.DviTiming]);
			else
				RX_DEF_LOG("[RX]err, timing=%d\n",
					myhdmi->dvi_s.DviTiming);

			RX_DEF_LOG(
				"[RX]ht=%d,vt=%d,vc=%d,w=%d,h=%d,%lums\n",
				myhdmi->dvi_s.DviHtotal,
				myhdmi->dvi_s.DviVtotal,
				myhdmi->dvi_s.DviVclk,
				myhdmi->dvi_s.DviWidth,
				myhdmi->dvi_s.DviHeight,
				jiffies);

			if (myhdmi->my_debug == 10) {
				hdmi2com_crc(myhdmi, 5);
				temp = myhdmi->crc0;
				myhdmi->crc0 = 0;
				usleep_range(50000, 50050);
				hdmi2com_crc(myhdmi, 5);
				if ((temp == myhdmi->crc0) && (temp != 0))
					RX_DEF_LOG("[RX]=>pass\n");
				else
					RX_DEF_LOG("[RX]=>fail\n");
			}

			myhdmi->is_detected = TRUE;

			myhdmi->dvi_s.DviChkState = DVI_CHK_MODECHG;
		}
	}
}

u8
dvi2_get_timing(struct MTK_HDMI *myhdmi)
{
	return myhdmi->dvi_s.DviTiming;
}

void hdmi2_get_vid_para(struct MTK_HDMI *myhdmi)
{
	/* color space */
	if (hdmi2com_input_color_space_type(myhdmi) == 0) {
		/* YUV */
		if (hdmi2_is_420_mode(myhdmi))
			myhdmi->vid_para.cs = CS_YUV420;
		else if (hdmi2com_is_422_input(myhdmi))
			myhdmi->vid_para.cs = CS_YUV422;
		else
			myhdmi->vid_para.cs = CS_YUV444;
	} else {
		/* RGB */
		myhdmi->vid_para.cs = CS_RGB;
	}

	/* deepcolor */
	myhdmi->vid_para.dp = hdmi2com_get_deep_status(myhdmi);

	if (myhdmi->my_debug == 20) {
		RX_DEF_LOG("[RX] VRR force 1080p\n");
		myhdmi->vid_para.vtotal = 1125;
		myhdmi->vid_para.htotal = 2200;
		myhdmi->vid_para.vactive = 1080;
		myhdmi->vid_para.hactive = 1920;
	} else if (myhdmi->my_debug == 21) {
		RX_DEF_LOG("[RX] VRR force 4k60\n");
		myhdmi->vid_para.vtotal = 2250;
		myhdmi->vid_para.htotal = 4400;
		myhdmi->vid_para.vactive = 2160;
		myhdmi->vid_para.hactive = 3840;
	} else {
		myhdmi->vid_para.vtotal = hdmi2com_v_total(myhdmi);
		myhdmi->vid_para.htotal = hdmi2com_h_total(myhdmi);
		myhdmi->vid_para.vactive = hdmi2com_v_active(myhdmi);
		myhdmi->vid_para.hactive = hdmi2com_h_active(myhdmi);
	}

	myhdmi->vid_para.pixclk = hdmi2com_pixel_clk(myhdmi) / 1000;
	myhdmi->vid_para.tmdsclk = hdmi2com_get_ref_clk(myhdmi);
	myhdmi->vid_para.is_40x = hdmi2com_is_tmds_clk_radio_40x(myhdmi);
	if (hdmi2com_is_interlace(myhdmi) == 0)
		myhdmi->vid_para.is_pscan = 1;
	else
		myhdmi->vid_para.is_pscan = 0;
	myhdmi->vid_para.frame_rate = hdmi2com_frame_rate(myhdmi);

	myhdmi->vid_para.hdmi_mode = hdmi2com_is_hdmi_mode(myhdmi);

	if (myhdmi->my_debug == 20)
		myhdmi->vid_para.id = MODE_1080p_60;
	else if (myhdmi->my_debug == 21)
		myhdmi->vid_para.id = MODE_3840_2160P_60;
	else
		myhdmi->vid_para.id = myhdmi->dvi_s.DviTiming;
}

void
dvi2_show_timing_detail(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]h total : %d\n", hdmi2com_h_total(myhdmi));
	RX_DEF_LOG("[RX]v total : %d\n", hdmi2com_v_total(myhdmi));
	RX_DEF_LOG("[RX]width : %d\n", hdmi2com_h_active(myhdmi));
	RX_DEF_LOG("[RX]height : %d\n", hdmi2com_v_active(myhdmi));
	RX_DEF_LOG("[RX]pixel clk : %d\n", hdmi2com_pixel_clk(myhdmi));
	RX_DEF_LOG("[RX]vs clk : %d\n", hdmi2com_frame_rate(myhdmi));
	RX_DEF_LOG("[RX]hs clk : %d\n", hdmi2com_h_clk(myhdmi));
	RX_DEF_LOG("[RX]p/l : %d\n", hdmi2com_is_interlace(myhdmi));
	RX_DEF_LOG("[RX]h_backporch : %d\n",
		hdmi2com_h_back_porch(myhdmi));
	RX_DEF_LOG("[RX]h_frontporch : %d\n",
		hdmi2com_h_front_porch(myhdmi));
	RX_DEF_LOG("[RX]v_backporch_odd : %d\n",
		hdmi2com_v_back_porch_odd(myhdmi));
	RX_DEF_LOG("[RX]v_frontporch_odd : %d\n",
		hdmi2com_v_front_porch_odd(myhdmi));
	RX_DEF_LOG("[RX]v_backporch_even : %d\n",
		hdmi2com_v_back_porch_even(myhdmi));
	RX_DEF_LOG("[RX]v_frontporch_even : %d\n",
		hdmi2com_v_front_porch_even(myhdmi));
}

void
dvi2_get_timing_detail(struct MTK_HDMI *myhdmi, struct HDMIRX_TIMING_T *p)
{
	if (hdmi2_is_420_mode(myhdmi)) {
		p->h_total = hdmi2com_h_total(myhdmi) * 2;
		p->h_active = hdmi2com_h_active(myhdmi) * 2;
	} else {
		p->h_total = hdmi2com_h_total(myhdmi);
		p->h_active = hdmi2com_h_active(myhdmi);
	}
	p->v_total = hdmi2com_v_total(myhdmi);
	p->v_active = hdmi2com_v_active(myhdmi);
	p->pixel_clk = hdmi2com_pixel_clk(myhdmi);
	p->hs_clk = hdmi2com_h_clk(myhdmi);
	p->vs_clk = hdmi2com_frame_rate(myhdmi);
	p->is_interlace = hdmi2com_is_interlace(myhdmi);
	p->mode_id = myhdmi->dvi_s.DviTiming;
	p->h_backporch =
		hdmi2com_h_back_porch(myhdmi);
	p->h_frontporch =
		hdmi2com_h_front_porch(myhdmi);
	p->v_backporch_odd =
		hdmi2com_v_back_porch_odd(myhdmi);
	p->v_frontporch_odd =
		hdmi2com_v_front_porch_odd(myhdmi);
	p->v_backporch_even =
		hdmi2com_v_back_porch_even(myhdmi);
	p->v_frontporch_even =
		hdmi2com_v_front_porch_even(myhdmi);
}

u32
dvi2_get_pixel_clk_value(struct MTK_HDMI *myhdmi)
{
	return myhdmi->dvi_s.DviPixClk / 1000000;
}

void
dvi2_var_init(struct MTK_HDMI *myhdmi)
{
	myhdmi->is_detected = TRUE;
	myhdmi->dvi_state_old = DVI_SATE_MAX;
	myhdmi->wait_support_timing = 0;
}

void
dvi2_show_status(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->dvi_s.DviChkState == DVI_NO_SIGNAL)
		RX_DEF_LOG("[RX]DviChkState = DVI_NO_SIGNAL\n");
	else if (myhdmi->dvi_s.DviChkState == DVI_CHK_MODECHG)
		RX_DEF_LOG("[RX]DviChkState = DVI_CHK_MODECHG\n");
	else if (myhdmi->dvi_s.DviChkState == DVI_WAIT_STABLE)
		RX_DEF_LOG("[RX]DviChkState = DVI_WAIT_STABLE\n");
	else
		RX_DEF_LOG("[RX]DviChkState = unknown\n");

	if (hdmi2com_is_scdt_1(myhdmi) == FALSE)
		return;

	if (myhdmi->dvi_s.DviChkState == DVI_CHK_MODECHG) {
		if (myhdmi->dvi_s.DviTiming < MODE_MAX)
			RX_DEF_LOG("[RX]DviTiming: %d,%s\n",
				myhdmi->dvi_s.DviTiming,
				szRxResStr[myhdmi->dvi_s.DviTiming]);
		else
			RX_DEF_LOG("[RX]unknown DviTiming: %d\n",
				myhdmi->dvi_s.DviTiming);

		if (dvi2_is_3d_pkt_valid(myhdmi)) {
			RX_DEF_LOG("[RX]3D signal\n");
			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_FramePacking)
				RX_DEF_LOG("[RX]HDMI_3D_FramePacking\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_FieldAlternative)
				RX_DEF_LOG(
					"[RX]HDMI_3D_FieldAlternative\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_LineAlternative)
				RX_DEF_LOG(
					"[RX]HDMI_3D_LineAlternative\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_SideBySideFull)
				RX_DEF_LOG("[RX]HDMI_3D_SideBySideFull\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_LDepth)
				RX_DEF_LOG("[RX]HDMI_3D_LDepth\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_LDepthGraph)
				RX_DEF_LOG("[RX]HDMI_3D_LDepthGraph\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str == HDMI_3D_TopBottom)
				RX_DEF_LOG("[RX]HDMI_3D_TopBottom\n");

			if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
				HDMI_3D_SideBySideHalf)
				RX_DEF_LOG("[RX]HDMI_3D_SideBySideHalf\n");

		} else {
			RX_DEF_LOG("[RX]2D signal\n");
		}

	} else {
		RX_DEF_LOG("[RX]don't care resolution info\n");
	}

	RX_DEF_LOG("[RX]DVI width: %d\n", dvi2_get_input_width(myhdmi));
	RX_DEF_LOG("[RX]DVI height: %d\n", dvi2_get_input_height(myhdmi));
	RX_DEF_LOG("[RX]DVI refresh rate: %d\n",
		dvi2_get_refresh_rate(myhdmi));
	RX_DEF_LOG("[RX]DVI interlace/progressive: %d\n",
		dvi2_get_interlaced(myhdmi));
	RX_DEF_LOG("[RX]pixel clock: %d\n", hdmi2com_pixel_clk(myhdmi));

	RX_DEF_LOG("[RX]DviVclk: %d\n", myhdmi->dvi_s.DviVclk);
	RX_DEF_LOG("[RX]DviHclk: %d\n", myhdmi->dvi_s.DviHclk);
	RX_DEF_LOG(
		"[RX]vsync frequency: %d\n", hdmi2com_frame_rate(myhdmi));
	RX_DEF_LOG("[RX]hsync frequency: %d\n", hdmi2com_h_clk(myhdmi));
	RX_DEF_LOG("[RX]HDMICS (1=RGB , 0 =YCBCR) : %d\n",
		myhdmi->dvi_s.HDMICS);
#ifdef CC_SUPPORT_HDR10Plus
	RX_DEF_LOG("[RX]HDR10Plus status: %u\n", myhdmi->dvi_s._rHdr10PStatus);
#endif
}

void io_get_vid_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_VID_PARA *vid_para)
{
	/* color space */
	if (hdmi2com_input_color_space_type(myhdmi) == 0) {
		/* YUV */
		if (hdmi2_is_420_mode(myhdmi))
			vid_para->cs = HDMI_CS_YUV420;
		else if (hdmi2com_is_422_input(myhdmi))
			vid_para->cs = HDMI_CS_YUV422;
		else
			vid_para->cs = HDMI_CS_YUV444;
	} else {
		/* RGB */
		vid_para->cs = HDMI_CS_RGB;
	}

	/* deepcolor */
	vid_para->dp = (enum HdmiRxDP)hdmi2com_get_deep_status(myhdmi);

	vid_para->vtotal = hdmi2com_v_total(myhdmi);
	vid_para->htotal = hdmi2com_h_total(myhdmi);
	vid_para->vactive = hdmi2com_v_active(myhdmi);
	vid_para->hactive = hdmi2com_h_active(myhdmi);

	vid_para->pixclk = hdmi2com_pixel_clk(myhdmi) / 1000;
	if (hdmi2com_is_interlace(myhdmi) == 0)
		vid_para->is_pscan = 1;
	else
		vid_para->is_pscan = 0;
	vid_para->frame_rate = hdmi2com_frame_rate(myhdmi);

	vid_para->hdmi_mode = hdmi2com_is_hdmi_mode(myhdmi);

	if (myhdmi->my_debug == 24) {
		RX_DEF_LOG("[RX]cs:%d\n", vid_para->cs);
		RX_DEF_LOG("[RX]dp:%d\n", vid_para->dp);
		RX_DEF_LOG("[RX]vtotal:%d\n", vid_para->vtotal);
		RX_DEF_LOG("[RX]htotal:%d\n", vid_para->htotal);
		RX_DEF_LOG("[RX]vactive:%d\n", vid_para->vactive);
		RX_DEF_LOG("[RX]hactive:%d\n", vid_para->hactive);
		RX_DEF_LOG("[RX]is_pscan:%d\n", vid_para->is_pscan);
		RX_DEF_LOG("[RX]frame_rate:%d\n", vid_para->frame_rate);
		RX_DEF_LOG("[RX]hdmi_mode:%d\n", vid_para->hdmi_mode);
	}
}

