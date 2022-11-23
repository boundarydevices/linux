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
#include "hdmi_rx2_edid.h"
#include "hdmi_rx21_phy.h"

const char *cRxHdcpStatus[] = {
	"UnAuthenticated",
	"Computations",
	"WaitforDownstream",
	"WaitVReady",
	"Authenticated",
};

const char *rx_state_str[] = {
	"no_signal",
	"ckdt",
	"analog",
	"digital",
	"timing",
	"stable",
	"rx_state_max",
};

void
Linux_Time(unsigned long *prTime)
{
	*prTime = jiffies;
}

bool
Linux_DeltaTime(unsigned long *u4OverTime, unsigned long *prStartT,
	unsigned long *prCurrentT)
{
	unsigned long u4DeltaTime;

	u4DeltaTime = *prStartT + (*u4OverTime) * HZ / 1000;
	if (time_after(*prCurrentT, u4DeltaTime))
		return TRUE;
	else
		return FALSE;
}

bool hdmi2_is_rpt(struct MTK_HDMI *myhdmi)
{
	if ((myhdmi->hdmi_mode_setting == HDMI_RPT_DGI_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_DRAM_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_PIXEL_MODE) ||
		(myhdmi->hdmi_mode_setting == HDMI_RPT_TMDS_MODE))
		return TRUE;
	else
		return FALSE;
}

bool hdmi2_is_dgi_mode(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdmi_mode_setting == HDMI_RPT_DGI_MODE)
		return TRUE;
	else
		return FALSE;
}

bool hdmi2_is_dram_mode(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdmi_mode_setting == HDMI_RPT_DRAM_MODE)
		return TRUE;
	else
		return FALSE;
}

bool hdmi2_is_pixel_mode(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdmi_mode_setting == HDMI_RPT_PIXEL_MODE)
		return TRUE;
	else
		return FALSE;
}

bool hdmi2_is_tmds_mode(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdmi_mode_setting == HDMI_RPT_TMDS_MODE)
		return TRUE;
	else
		return FALSE;
}

void txapi_rpt_mode(struct MTK_HDMI *myhdmi,
	enum hdmi_mode_setting mode)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s, 0x%p\n", __func__, TXP);
		return;
	}
	if (TXP->rpt_mode == NULL) {
		RX_DEF_LOG("[RX]no rpt_mode=0x%p\n", TXP->rpt_mode);
		return;
	}
	TXP->rpt_mode(myhdmi->txdev, mode);
#endif
}

bool txapi_get_edid(struct MTK_HDMI *myhdmi,
		u8 *p,
		u32 num,
		u32 len)
{
#ifndef HDMIRX_RPT_EN
	return FALSE;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return FALSE;
	}
	if (TXP->get_edid == NULL)
		return FALSE;

	if (TXP->get_edid(myhdmi->txdev, p, num, len) == 0)
		return TRUE;
	return FALSE;
#endif
}

void txapi_hdcp_start(struct MTK_HDMI *myhdmi,
		bool hdcp_on)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->hdcp_start == NULL)
		return;

	TXP->hdcp_start(myhdmi->txdev, hdcp_on);
#endif
}

bool txapi_hdcp_ver(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return FALSE;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return FALSE;
	}
	if (TXP->hdcp_ver == NULL)
		return FALSE;

	return TXP->hdcp_ver(myhdmi->txdev);
#endif
}

void txapi_hdmi_enable(struct MTK_HDMI *myhdmi, bool en)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->hdmi_enable == NULL)
		return;

	TXP->hdmi_enable(myhdmi->txdev, en);
#endif
}

bool txapi_is_plugin(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return FALSE;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return FALSE;
	}
	if (TXP->is_plugin == NULL)
		return FALSE;

	return TXP->is_plugin(myhdmi->txdev);
#endif
}

u32 txapi_get_tx_port(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return HDMI_PLUG_OUT;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return FALSE;
	}
	if (TXP->get_port == NULL)
		return FALSE;

	return TXP->get_port(myhdmi->txdev);
#endif
}

void txapi_signal_off(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->signal_off == NULL)
		return;

	TXP->signal_off(myhdmi->txdev);
#endif
}

void txapi_send_pkt(struct MTK_HDMI *myhdmi,
	enum packet_type type,
	bool enable,
	u8 *pkt_data,
	struct HdmiInfo *pkt_p)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->send_pkt == NULL)
		return;

	TXP->send_pkt(myhdmi->txdev, type, enable, pkt_data, pkt_p);
#endif
}

void txapi_cfg_video(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s, 0x%p\n", __func__, TXP);
		return;
	}
	if (TXP->cfg_video == NULL) {
		RX_DEF_LOG("[RX]no cfg_video=0x%p\n", TXP->cfg_video);
		return;
	}

	TXP->cfg_video(myhdmi->txdev, &myhdmi->vid_para);
#endif
}

void txapi_cfg_audio(struct MTK_HDMI *myhdmi, u8 aud)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->cfg_audio == NULL)
		return;

	TXP->cfg_audio(myhdmi->txdev, aud);
#endif
}

void txapi_vrr_emp(struct MTK_HDMI *myhdmi)
{
#ifndef HDMIRX_RPT_EN
	return;
#else
	if (TXP == NULL) {
		RX_DEF_LOG("[RX]no %s\n", __func__);
		return;
	}
	if (TXP->send_vrr == NULL)
		return;

	TXP->send_vrr(myhdmi->txdev, &myhdmi->vrr_emp);
#endif
}

void
hdmi2_set_unstb_start(struct MTK_HDMI *myhdmi)
{
	myhdmi->unstb_start = jiffies;
}

bool
hdmi2_is_unstb_timeout(struct MTK_HDMI *myhdmi, unsigned long delay_ms)
{
	unsigned long u4DeltaTime;

	u4DeltaTime = myhdmi->unstb_start + (delay_ms)*HZ / 1000;
	if (time_after(jiffies, u4DeltaTime))
		return TRUE;
	else
		return FALSE;
}

void
hdmi2_set_reset_start_time(struct MTK_HDMI *myhdmi)
{
	myhdmi->reset_timeout_start = jiffies;
}

bool
hdmi2_is_reset_timeout(struct MTK_HDMI *myhdmi, unsigned long delay_ms)
{
	unsigned long u4DeltaTime;

	u4DeltaTime = myhdmi->reset_timeout_start + (delay_ms)*HZ / 1000;
	if (time_after(jiffies, u4DeltaTime))
		return TRUE;
	else
		return FALSE;
}

void
hdmi2_set_digital_reset_start_time(struct MTK_HDMI *myhdmi)
{
	myhdmi->wait_scdt_timeout_start = jiffies;
}

bool
hdmi2_is_digital_reset_timeout(struct MTK_HDMI *myhdmi, unsigned long delay_ms)
{
	unsigned long u4DeltaTime;

	u4DeltaTime = myhdmi->wait_scdt_timeout_start + (delay_ms)*HZ / 1000;
	if (time_after(jiffies, u4DeltaTime))
		return TRUE;
	else
		return FALSE;
}

void
hdmi2_set_hpd_low(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch)
{
	hdmi2com_hdcp2x_req(myhdmi, 0);
	hdmi2com_set_hpd_low(myhdmi, u1HDMICurrSwitch);
}

void
hdmi2_set_hpd_high(struct MTK_HDMI *myhdmi, u8 u1HDMICurrSwitch)
{
	hdmi2com_is_hpd_high(myhdmi, u1HDMICurrSwitch);
}

static u8
hdmi2_get_port_5v_level(struct MTK_HDMI *myhdmi, u8 u1Switch)
{
	u8 u15VDetect = FALSE;

	if (myhdmi->my_debug == 1)
		return TRUE;

	switch (u1Switch) {
	case HDMI_SWITCH_1:
		u15VDetect = hdmi2com_get_port1_5v_level(myhdmi);
		break;
	case HDMI_SWITCH_2:
		u15VDetect = hdmi2com_get_port2_5v_level(myhdmi);
		break;
	case HDMI_SWITCH_3:
		u15VDetect = TRUE;
		break;
	case HDMI_SWITCH_4:
		u15VDetect = TRUE;
		break;
	default:
		u15VDetect = TRUE;
		break;
	}

	return u15VDetect;
}

static void
hdmi2_handle_port_5v_changed(struct MTK_HDMI *myhdmi)
{
}

u8
hdmi2_is_un_plug_in(struct MTK_HDMI *myhdmi)
{
	return myhdmi->UnplugFlag;
}

void
hdmi2_un_plug_in_clr(struct MTK_HDMI *myhdmi)
{
	myhdmi->UnplugFlag = 0;
}

void
hdmi2_set_un_plug_in(struct MTK_HDMI *myhdmi)
{
	myhdmi->UnplugFlag = 1;
}

u8
hdmi2_is_sync_lose(struct MTK_HDMI *myhdmi)
{
	return myhdmi->HDMIScdtLose;
}

void
hdmi2_set_sync_lose(struct MTK_HDMI *myhdmi, u8 u1Val)
{
	myhdmi->HDMIScdtLose = u1Val;
}

bool
hdmi2_is_420_mode(struct MTK_HDMI *myhdmi)
{
	return hdmi2com_is_420_mode(myhdmi);
}

void
hdmi2_reset_digital(struct MTK_HDMI *myhdmi)
{
	hdmi2com_dig_phy_rst1(myhdmi);
	hdmi2com_deep_rst1(myhdmi);
	hdmi2com_hdcp_reset(myhdmi);
	hdmi2com_info_status_clr(myhdmi);
	hdmi2_stable_status_clr(myhdmi);
}

void
hdmi2_set_hpd_low_timer(struct MTK_HDMI *myhdmi, u8 u1TimerCnt)
{
	myhdmi->HDMIHPDTimerCnt = u1TimerCnt;
}

void
hdmi2_tmds_term_ctrl(struct MTK_HDMI *myhdmi, bool bOnOff)
{
	vHDMI21PhyTermCtrl(myhdmi, bOnOff);
}

void
hdmi2_rx_init(struct MTK_HDMI *myhdmi)
{
	hdmi2com_rx_init(myhdmi);
}

void
hdmi2_rx_var_init(struct MTK_HDMI *myhdmi)
{
	myhdmi->hdcpstatus = 0;
	myhdmi->hdcpinit = 0;

	myhdmi->pdtclk_count = 0;
	myhdmi->scdc_reset = 0;
	myhdmi->reset_wait_time = HDMI_RESET_TIMEOUT;
	myhdmi->timing_unstable_time = 80;
	myhdmi->wait_data_stable_count = 30;
	myhdmi->timing_stable_count = 0;
	myhdmi->reset_timeout_start = 0;
	myhdmi->wait_scdt_timeout_start = 0;
	myhdmi->timeout_start = 0;

	myhdmi->h_total = 0;
	myhdmi->v_total = 0;

	myhdmi->is_420_cfg = 0xff;
	myhdmi->is_dp_info = 0xff;
	myhdmi->is_40x_mode = FALSE;
	myhdmi->dp_cfg = 0;
	myhdmi->is_av_mute = FALSE;
	myhdmi->is_timing_chg = FALSE;
	myhdmi->stable_count = 0;
	myhdmi->hdcp_stable_count = 0;

	myhdmi->p5v_status = 0;
	myhdmi->HDMIState = 0;
	myhdmi->vid_locked = 0;
	myhdmi->aud_locked = 0;
	myhdmi->hdcp_version = 0;

	myhdmi->aud_enable = 0;

	myhdmi->u1TxEdidReady = HDMI_PLUG_OUT;
	myhdmi->u1TxEdidReadyOld = HDMI_PLUG_IDLE;
	myhdmi->_fg2chPcmOnly = TRUE;

	myhdmi->video_notify = 0;
	myhdmi->audio_notify = 0;

	memset(&myhdmi->aud_caps, 0, sizeof(struct AUDIO_CAPS));
	memset(&myhdmi->aud_s, 0, sizeof(struct AUDIO_INFO));
}

void
hdmi2_hw_init(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s\n", __func__);
	hdmi2com_pad_init(myhdmi);
	hdmi2com_ddc_path_config(myhdmi, FALSE);
	vHDMI21PhyInit(myhdmi);
	hdmi2_rx_init(myhdmi);
	hdmi2com_path_config(myhdmi);
	hdmi2com_deep_color_auto_en(myhdmi, TRUE, 0);
	hdmi2com_infoframe_init(myhdmi);
	hdmi2_aud_pll_select(myhdmi, 1);
}

void
hdmi2_audio_mute(struct MTK_HDMI *myhdmi, bool bEn)
{
	hdmi2com_aud_mute_set(myhdmi, bEn);
}

bool
hdmi2_is_hdmi_mode(struct MTK_HDMI *myhdmi)
{
	return hdmi2com_is_hdmi_mode(myhdmi);
}

u8
hdmi2_get_deep_color_status(struct MTK_HDMI *myhdmi)
{
	return (u8)hdmi2com_get_deep_status(myhdmi);
}

void
hdmi2_video_mute(struct MTK_HDMI *myhdmi, u8 u1En)
{
	hdmi2com_vid_mute_set(myhdmi, u1En);
}

void
hdmi2_aud_pll_select(struct MTK_HDMI *myhdmi, u8 u1On)
{
	hdmi2com_aud_apll_sel(myhdmi, u1On);
}

void
hdmi2_aud_reset_for_verify(struct MTK_HDMI *myhdmi)
{
	u32 i, temp;

	hdmi2com_audio_pll_sel(myhdmi, 1);
	hdmi2com_aud_acr_rst(myhdmi);

	for (i = 0, temp = 0; i < 50; i++) {
		usleep_range(20000, 20050);
		if (hdmi2com_is_acr_lock(myhdmi) == TRUE) {
			temp++;
			if (temp > 3) {
				hdmi2com_aud_fifo_rst(myhdmi);
				usleep_range(20000, 20050);
				hdmi2com_aud_fifo_err_clr(myhdmi);
				RX_DEF_LOG("[RX]lock acr, %dms\n", i * 10);
				return;
			}
		} else {
			temp = 0;
		}
	}

	RX_DEF_LOG("[RX]can't lock acr\n");
}

void
hdmi2_audio_reset(struct MTK_HDMI *myhdmi)
{
	hdmi2com_audio_pll_sel(myhdmi, 1);
	hdmi2com_aud_acr_rst(myhdmi);
}

void
hdmi2_packet_data_init(struct MTK_HDMI *myhdmi)
{
	unsigned long LastestTime;

	Linux_Time(&LastestTime);

	memset(&(myhdmi->AviInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AviInfo.u4LastReceivedTime = LastestTime;
	myhdmi->AviInfo.u4timeout = 100;
	/* myhdmi->AviInfo.fgChanged = TRUE; */
	myhdmi->AviInfo.fgValid = TRUE;

	memset(&(myhdmi->SpdInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->SpdInfo.u4LastReceivedTime = LastestTime;
	myhdmi->SpdInfo.u4timeout = 2000;
	/* myhdmi->SpdInfo.fgChanged = TRUE; */
	myhdmi->SpdInfo.fgValid = TRUE;

	memset(&(myhdmi->AudInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AudInfo.u4LastReceivedTime = LastestTime;
	myhdmi->AudInfo.u4timeout = 100;
	/* myhdmi->AudInfo.fgChanged = TRUE; */
	myhdmi->AudInfo.fgValid = TRUE;

	memset(&(myhdmi->MpegInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->MpegInfo.u4LastReceivedTime = LastestTime;
	myhdmi->MpegInfo.u4timeout = 100;
	/* myhdmi->MpegInfo.fgChanged = TRUE; */
	myhdmi->MpegInfo.fgValid = TRUE;

	memset(&(myhdmi->UnrecInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->UnrecInfo.u4LastReceivedTime = LastestTime;
	myhdmi->UnrecInfo.u4timeout = 100;
	/* myhdmi->UnrecInfo.fgChanged = TRUE; */
	myhdmi->UnrecInfo.fgValid = TRUE;

	memset(&(myhdmi->AcpInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AcpInfo.u4LastReceivedTime = LastestTime;
	myhdmi->AcpInfo.u4timeout = 600;
	/* myhdmi->AcpInfo.fgChanged = TRUE; */
	myhdmi->AcpInfo.fgValid = TRUE;

	memset(&(myhdmi->VsiInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->VsiInfo.u4LastReceivedTime = LastestTime;
	myhdmi->VsiInfo.u4timeout = 100;
	/* myhdmi->VsiInfo.fgChanged = TRUE; */
	myhdmi->VsiInfo.fgValid = TRUE;

	memset(&(myhdmi->DVVSIInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->DVVSIInfo.u4LastReceivedTime = LastestTime;
	myhdmi->DVVSIInfo.u4timeout = 100;
	/* myhdmi->DVVSIInfo.fgChanged = TRUE; */
	myhdmi->DVVSIInfo.fgValid = TRUE;

	memset(&(myhdmi->Isrc0Info), 0, sizeof(struct HdmiInfo));
	myhdmi->Isrc0Info.u4LastReceivedTime = LastestTime;
	myhdmi->Isrc0Info.u4timeout = 300;
	/* myhdmi->Isrc0Info.fgChanged = TRUE; */
	myhdmi->Isrc0Info.fgValid = TRUE;

	memset(&(myhdmi->Isrc1Info), 0, sizeof(struct HdmiInfo));
	myhdmi->Isrc1Info.u4LastReceivedTime = LastestTime;
	myhdmi->Isrc1Info.u4timeout = 300;
	/* myhdmi->Isrc1Info.fgChanged = TRUE; */
	myhdmi->Isrc1Info.fgValid = TRUE;

	memset(&(myhdmi->GcpInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->GcpInfo.u4LastReceivedTime = LastestTime;
	myhdmi->GcpInfo.u4timeout = 600;
	/* myhdmi->GcpInfo.fgChanged = TRUE; */
	myhdmi->GcpInfo.fgValid = TRUE;

	memset(&(myhdmi->HfVsiInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->HfVsiInfo.u4LastReceivedTime = LastestTime;
	myhdmi->HfVsiInfo.u4timeout = 100;
	/* myhdmi->HfVsiInfo.fgChanged = TRUE; */
	myhdmi->HfVsiInfo.fgValid = TRUE;

	memset(&(myhdmi->Info86), 0, sizeof(struct HdmiInfo));
	myhdmi->Info86.u4LastReceivedTime = LastestTime;
	myhdmi->Info86.u4timeout = 300;
	/* myhdmi->Info86.fgChanged = TRUE; */
	myhdmi->Info86.fgValid = TRUE;

	memset(&(myhdmi->Info87), 0, sizeof(struct HdmiInfo));
	myhdmi->Info87.u4LastReceivedTime = LastestTime;
	myhdmi->Info87.u4timeout = 300;
	/* myhdmi->Info87.fgChanged = TRUE; */
	myhdmi->Info87.fgValid = TRUE;

	if (hdmi2_is_rpt(myhdmi))
		txapi_send_pkt(myhdmi,
			MAX_PACKET, 0, NULL, NULL);

	aud2_notify_acp_type_change(myhdmi, ACP_LOST_DISABLE);
	hdmi_rx_notify(myhdmi, HDMI_RX_ACP_PKT_CHANGE);
}

void
hdmi2_get_pkt(struct MTK_HDMI *myhdmi)
{
	unsigned long CurrTime;

	Linux_Time(&CurrTime);

	/***     HDMI_INFO_FRAME_ID_AVI     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_AVI)) {
		myhdmi->AviInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_AVI);
	} else {
		if (Linux_DeltaTime(&myhdmi->AviInfo.u4timeout,
			    &myhdmi->AviInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->AviInfo.fgValid == TRUE)
				myhdmi->AviInfo.fgChanged = TRUE;
			myhdmi->AviInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_AVI);
		}
	}
	if (myhdmi->AviInfo.fgChanged) {
		myhdmi->AviInfo.fgChanged = FALSE;
		/* hdmi2_show_infoframe(&(myhdmi->AviInfo)); */
		//if (hdmi2_is_pixel_mode(myhdmi))
		//	hdmi2_bypass_cs_convert(myhdmi);
		//else
		hdmi2_color_space_convert(myhdmi);
		hdmi2_avi_info_parser(myhdmi);
		hdmi2_video_setting(myhdmi);
		hdmi_rx_notify(myhdmi, HDMI_RX_AVI_INFO_CHANGE);
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				AVI_INFOFRAME,
				myhdmi->AviInfo.fgValid,
				&myhdmi->AviInfo.u1InfoData[0],
				&myhdmi->AviInfo);
	}

	/***     HDMI_INFO_FRAME_ID_SPD     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_SPD)) {
		myhdmi->SpdInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_SPD);
	} else {
		if (Linux_DeltaTime(&myhdmi->SpdInfo.u4timeout,
			    &myhdmi->SpdInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->SpdInfo.fgValid == TRUE)
				myhdmi->SpdInfo.fgChanged = TRUE;
			myhdmi->SpdInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_SPD);
		}
	}
	if (myhdmi->SpdInfo.fgChanged) {
		myhdmi->SpdInfo.fgChanged = FALSE;
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				SPD_INFOFRAME,
				myhdmi->SpdInfo.fgValid,
				&myhdmi->SpdInfo.u1InfoData[0],
				&myhdmi->SpdInfo);
	}

	/***     HDMI_INFO_FRAME_ID_AUD     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_AUD)) {
		myhdmi->AudInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_AUD);
	} else {
		if (Linux_DeltaTime(&myhdmi->AudInfo.u4timeout,
			    &myhdmi->AudInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->AudInfo.fgValid == TRUE)
				myhdmi->AudInfo.fgChanged = TRUE;
			myhdmi->AudInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_AUD);
		}
	}
	if (myhdmi->AudInfo.fgChanged) {
		myhdmi->AudInfo.fgChanged = FALSE;
		hdmi_rx_notify(myhdmi, HDMI_RX_AUD_INFO_CHANGE);
		//fixed to 2ch 48k PCM, not need to bypass audio info
	}

	/***     HDMI_INFO_FRAME_ID_ACP     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_ACP)) {
		myhdmi->AcpInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ACP);
	} else {
		if (Linux_DeltaTime(&myhdmi->AcpInfo.u4timeout,
			    &myhdmi->AcpInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->AcpInfo.fgValid == TRUE) {
				myhdmi->AcpInfo.fgChanged = TRUE;
				aud2_notify_acp_type_change(
					myhdmi, ACP_LOST_DISABLE);
				hdmi_rx_notify(myhdmi, HDMI_RX_ACP_PKT_CHANGE);
			}
			myhdmi->AcpInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_ACP);
		}
	}
	if (myhdmi->AcpInfo.fgChanged) {
		myhdmi->AcpInfo.fgChanged = FALSE;
		if (myhdmi->AcpInfo.fgValid) {
			aud2_notify_acp_type_change(
				myhdmi, myhdmi->AcpInfo.u1InfoData[1]);
			if (hdmi2_is_rpt(myhdmi))
				txapi_send_pkt(myhdmi,
					ACP_PACKET,
					myhdmi->AcpInfo.fgValid,
					&myhdmi->AcpInfo.u1InfoData[0],
					&myhdmi->AcpInfo);
		} else {
			hdmi_rx_notify(myhdmi, HDMI_RX_ACP_PKT_CHANGE);
		}
	}

	/***     HDMI_INFO_FRAME_ID_VSI     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_VSI)) {
		myhdmi->VsiInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_VSI);
	} else {
		if (Linux_DeltaTime(&myhdmi->VsiInfo.u4timeout,
			    &myhdmi->VsiInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->VsiInfo.fgValid == TRUE)
				myhdmi->VsiInfo.fgChanged = TRUE;
			myhdmi->VsiInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_VSI);
		}
	}
	if (myhdmi->VsiInfo.fgChanged) {
		myhdmi->VsiInfo.fgChanged = FALSE;
		hdmi2_vsi_info_parser(myhdmi);
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				VENDOR_INFOFRAME,
				myhdmi->VsiInfo.fgValid,
				&myhdmi->VsiInfo.u1InfoData[0],
				&myhdmi->VsiInfo);
	}

	/***     HDMI_INFO_FRAME_ID_HFVSI     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_HFVSI)) {
		myhdmi->HfVsiInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_HFVSI);
	} else {
		if (Linux_DeltaTime(&myhdmi->HfVsiInfo.u4timeout,
			    &myhdmi->HfVsiInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->HfVsiInfo.fgValid == TRUE)
				myhdmi->HfVsiInfo.fgChanged = TRUE;
			myhdmi->HfVsiInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_HFVSI);
		}
	}
	if (myhdmi->HfVsiInfo.fgChanged) {
		myhdmi->HfVsiInfo.fgChanged = FALSE;
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				HFVSI_PACKET,
				myhdmi->HfVsiInfo.fgValid,
				&myhdmi->HfVsiInfo.u1InfoData[0],
				&myhdmi->HfVsiInfo);
	}

	/***     HDMI_INFO_FRAME_ID_Dovi_VSI     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_DVVSI)) {
		myhdmi->DVVSIInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_DVVSI);
	} else {
		if (Linux_DeltaTime(&myhdmi->DVVSIInfo.u4timeout,
			    &myhdmi->DVVSIInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->DVVSIInfo.fgValid == TRUE)
				myhdmi->DVVSIInfo.fgChanged = TRUE;
			myhdmi->DVVSIInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_DVVSI);
		}
	}
	if (myhdmi->DVVSIInfo.fgChanged) {
		myhdmi->DVVSIInfo.fgChanged = FALSE;
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				DVVSI_PACKET,
				myhdmi->DVVSIInfo.fgValid,
				&myhdmi->DVVSIInfo.u1InfoData[0],
				&myhdmi->DVVSIInfo);
	}

	/***     HDMI_INFO_FRAME_ID_ISRC0     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_ISRC0)) {
		myhdmi->Isrc0Info.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ISRC0);
	} else {
		if (Linux_DeltaTime(&myhdmi->Isrc0Info.u4timeout,
			    &myhdmi->Isrc0Info.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->Isrc0Info.fgValid == TRUE)
				myhdmi->Isrc0Info.fgChanged = TRUE;
			myhdmi->Isrc0Info.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_ISRC0);
		}
	}
	if (myhdmi->Isrc0Info.fgChanged) {
		myhdmi->Isrc0Info.fgChanged = FALSE;
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				ISRC1_PACKET,
				myhdmi->Isrc0Info.fgValid,
				&myhdmi->Isrc0Info.u1InfoData[0],
				&myhdmi->Isrc0Info);
	}

	/***     HDMI_INFO_FRAME_ID_ISRC1     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_ISRC1)) {
		myhdmi->Isrc1Info.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ISRC1);
	} else {
		if (Linux_DeltaTime(&myhdmi->Isrc1Info.u4timeout,
			    &myhdmi->Isrc1Info.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->Isrc1Info.fgValid == TRUE)
				myhdmi->Isrc1Info.fgChanged = TRUE;
			myhdmi->Isrc1Info.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_ISRC1);
		}
	}
	if (myhdmi->Isrc1Info.fgChanged) {
		myhdmi->Isrc1Info.fgChanged = FALSE;
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				ISRC2_PACKET,
				myhdmi->Isrc1Info.fgValid,
				&myhdmi->Isrc1Info.u1InfoData[0],
				&myhdmi->Isrc1Info);
	}

	/***     HDMI_INFO_FRAME_ID_GCP     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_GCP)) {
		myhdmi->GcpInfo.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_GCP);
	} else {
		if (Linux_DeltaTime(&myhdmi->GcpInfo.u4timeout,
			    &myhdmi->GcpInfo.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->GcpInfo.fgValid == TRUE)
				myhdmi->GcpInfo.fgChanged = TRUE;
			myhdmi->GcpInfo.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_GCP);
		}
	}
	if (myhdmi->GcpInfo.fgChanged)
		myhdmi->GcpInfo.fgChanged = FALSE;

	/***     HDMI_INFO_FRAME_ID_86     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_86)) {
		myhdmi->Info86.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_86);
	} else {
		if (Linux_DeltaTime(&myhdmi->Info86.u4timeout,
			    &myhdmi->Info86.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->Info86.fgValid == TRUE)
				myhdmi->Info86.fgChanged = TRUE;
			myhdmi->Info86.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_86);
		}
	}
	if (myhdmi->Info86.fgChanged)
		myhdmi->Info86.fgChanged = FALSE;

	/***     HDMI_INFO_FRAME_ID_87     ***/
	if (hdmi2com_info_header(myhdmi, HDMI_INFO_FRAME_ID_87)) {
		myhdmi->Info87.u4LastReceivedTime = CurrTime;
		hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_87);
	} else {
		if (Linux_DeltaTime(&myhdmi->Info87.u4timeout,
			    &myhdmi->Info87.u4LastReceivedTime, &CurrTime)) {
			if (myhdmi->Info87.fgValid == TRUE)
				myhdmi->Info87.fgChanged = TRUE;
			myhdmi->Info87.fgValid = FALSE;
			hdmi2com_clear_info_header(myhdmi, HDMI_INFO_FRAME_ID_87);
		}
	}
	if (myhdmi->Info87.fgChanged) {
		myhdmi->Info87.fgChanged = FALSE;
		myhdmi->hdr10info.fgValid = myhdmi->Info87.fgValid;
		myhdmi->hdr10info.type = (u8)HDMI_INFO_FRAME_ID_87;
		memcpy(&myhdmi->hdr10info.u1InfoData[0],
			&myhdmi->Info87.u1InfoData[0], 32);
		hdmi_rx_notify(myhdmi, HDMI_RX_HDR_INFO_CHANGE);
		if (hdmi2_is_rpt(myhdmi))
			txapi_send_pkt(myhdmi,
				HDR87_PACKET,
				myhdmi->Info87.fgValid,
				&myhdmi->Info87.u1InfoData[0],
				&myhdmi->Info87);
	}
}

void io_get_hdr10_info(struct MTK_HDMI *myhdmi,
	struct hdr10InfoPkt *hdr10_info)
{
	memcpy(hdr10_info, &myhdmi->hdr10info, sizeof(struct hdr10InfoPkt));
}

void
hdmi2_get_infoframe(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]%s\n", __func__);

	memset(&(myhdmi->AviInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AviInfo.u1InfoID = HDMI_INFO_FRAME_ID_AVI;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_AVI);

	memset(&(myhdmi->SpdInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->SpdInfo.u1InfoID = HDMI_INFO_FRAME_ID_SPD;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_SPD);

	memset(&(myhdmi->AudInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AudInfo.u1InfoID = HDMI_INFO_FRAME_ID_AUD;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_AUD);

	memset(&(myhdmi->MpegInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->MpegInfo.u1InfoID = HDMI_INFO_FRAME_ID_MPEG;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_MPEG);

	memset(&(myhdmi->UnrecInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->UnrecInfo.u1InfoID = HDMI_INFO_FRAME_ID_UNREC;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_UNREC);

	memset(&(myhdmi->AcpInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->AcpInfo.u1InfoID = HDMI_INFO_FRAME_ID_ACP;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ACP);

	memset(&(myhdmi->VsiInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->VsiInfo.u1InfoID = HDMI_INFO_FRAME_ID_VSI;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_VSI);

	memset(&(myhdmi->DVVSIInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->DVVSIInfo.u1InfoID = HDMI_INFO_FRAME_ID_DVVSI;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_DVVSI);

	memset(&(myhdmi->MetaInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->MetaInfo.u1InfoID = HDMI_INFO_FRAME_ID_METADATA;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_METADATA);

	memset(&(myhdmi->Isrc0Info), 0, sizeof(struct HdmiInfo));
	myhdmi->Isrc0Info.u1InfoID = HDMI_INFO_FRAME_ID_ISRC0;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ISRC0);

	memset(&(myhdmi->Isrc1Info), 0, sizeof(struct HdmiInfo));
	myhdmi->Isrc1Info.u1InfoID = HDMI_INFO_FRAME_ID_ISRC1;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_ISRC1);

	memset(&(myhdmi->GcpInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->GcpInfo.u1InfoID = HDMI_INFO_FRAME_ID_GCP;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_GCP);

	memset(&(myhdmi->HfVsiInfo), 0, sizeof(struct HdmiInfo));
	myhdmi->HfVsiInfo.u1InfoID = HDMI_INFO_FRAME_ID_HFVSI;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_HFVSI);

	memset(&(myhdmi->Info86), 0, sizeof(struct HdmiInfo));
	myhdmi->Info86.u1InfoID = HDMI_INFO_FRAME_ID_86;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_86);

	memset(&(myhdmi->Info87), 0, sizeof(struct HdmiInfo));
	myhdmi->Info87.u1InfoID = HDMI_INFO_FRAME_ID_87;
	hdmi2com_info(myhdmi, HDMI_INFO_FRAME_ID_87);
}

void
hdmi2_stable_status_clr(struct MTK_HDMI *myhdmi)
{
	myhdmi->Hdcp1xState = HDMI_RX_HDCP1_STATUS_OFF;
	myhdmi->Hdcp2xState = HDMI_RX_HDCP2_STATUS_OFF;
}

void
hdmi2_phy_reset(struct MTK_HDMI *myhdmi, u8 resettype)
{
	if (resettype == HDMI_RST_ALL) {
		vHDMI21PhySetting(myhdmi);
		usleep_range(1000, 1050);
		hdmi2_reset_digital(myhdmi);
	}

	if (resettype == HDMI_RST_DIGITALPHY)
		hdmi2com_dig_phy_rst1(myhdmi);

	if (resettype == HDMI_RST_DEEPCOLOR)
		hdmi2com_deep_rst1(myhdmi);
}

void
hdmi2_sw_reset(struct MTK_HDMI *myhdmi)
{
	hdmi2com_sw_reset(myhdmi);
}

void
hdmi2_write_edid(struct MTK_HDMI *myhdmi, u8 *prData)
{
	RX_DEF_LOG("[RX]%s\n", __func__);

	hdmi2_set_hpd_low_timer(myhdmi, 0);
	hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);

	hdmi2com_write_edid_to_sram(myhdmi, 0, &prData[0]);
	hdmi2com_write_edid_to_sram(myhdmi, 1, &prData[0x80]);
	if (prData[0x7e] > 1) {
		hdmi2com_write_edid_to_sram(myhdmi, 2, &prData[0x100]);
		hdmi2com_write_edid_to_sram(myhdmi, 3, &prData[0x180]);
	}

	hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
}

void
hdmi2_edid_check_sum(struct MTK_HDMI *myhdmi, u8 *pbuf)
{
	u8 bCheckSum = 0;
	u32 u4Index;

	for (u4Index = 0; u4Index < 128; u4Index++) {
		bCheckSum = bCheckSum + pbuf[u4Index];
		if (u4Index == 126)
			RX_DEF_LOG("[RX]sum=%x\n", bCheckSum);
	}

	if (bCheckSum == 0)
		RX_DEF_LOG("[RX]check sum ok\n");
	else
		RX_DEF_LOG("[RX]check sum fail\n");
}

void
hdmi2_edid_chksum(u8 *pbuf)
{
	u8 bCheckSum = 0;
	u32 u4Index;

	for (u4Index = 0; u4Index < 127; u4Index++)
		bCheckSum = bCheckSum + pbuf[u4Index];
	pbuf[127] = 0x100 - bCheckSum;
}

void vBdModeChk(struct MTK_HDMI *myhdmi)
{
	u8 u1Cont;

	u1Cont = 15;
	if (myhdmi->_bHdmiRepeaterModeDelayCount < u1Cont) {
		myhdmi->_bHdmiRepeaterModeDelayCount++;
		return;
	}
	myhdmi->_bHdmiRepeaterModeDelayCount = u1Cont;

	if ((myhdmi->bHdmiRepeaterMode != myhdmi->bAppHdmiRepeaterMode) ||
		(myhdmi->bAppHDMICurrSwitch != myhdmi->bHDMICurrSwitch)) {
		RX_DEF_LOG("[RX]%s=%d, %s=%d\n",
			"bHdmiRepeaterMode",
			myhdmi->bHdmiRepeaterMode,
			"bAppHdmiRepeaterMode",
			myhdmi->bAppHdmiRepeaterMode);
		RX_DEF_LOG("[RX]_bHDMISwitch from %d TO %d\n",
			myhdmi->bHDMICurrSwitch,
			myhdmi->bAppHDMICurrSwitch);
		if (hdmi2_is_rpt(myhdmi))
			txapi_hdmi_enable(myhdmi, FALSE);

		if (myhdmi->bHdmiRepeaterMode != myhdmi->bAppHdmiRepeaterMode) {
			if (myhdmi->bAppHdmiRepeaterMode == HDMI_SOURCE_MODE) {
				myhdmi->bHdmiRepeaterMode
					= myhdmi->bAppHdmiRepeaterMode;
				hdmi2com_switch_port_sel(myhdmi,
					myhdmi->bAppHDMICurrSwitch);
				myhdmi->bHDMICurrSwitch = HDMI_SWITCH_INIT;
			} else {
				hdmi2com_switch_port_sel(myhdmi,
					myhdmi->bAppHDMICurrSwitch);
				myhdmi->bHdmiRepeaterMode
					= myhdmi->bAppHdmiRepeaterMode;
				myhdmi->bHDMICurrSwitch
					= myhdmi->bAppHDMICurrSwitch;
			}
		} else {
			myhdmi->bHDMICurrSwitch
				= myhdmi->bAppHDMICurrSwitch;
			hdmi2com_switch_port_sel(myhdmi,
				myhdmi->bHDMICurrSwitch);
		}
		hdmi2_set_state(myhdmi, rx_no_signal);
	}
}

void hdmi2_update_tx_port(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = txapi_get_tx_port(myhdmi);

	myhdmi->u1TxEdidReadyOld = HDMI_PLUG_IDLE;
	if (temp == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON) {
		RX_DEF_LOG("[RX]boot, tx is plug in\n");
		myhdmi->u1TxEdidReady = HDMI_PLUG_IN_AND_SINK_POWER_ON;
	} else if (temp == HDMI_STATE_HOT_PLUG_IN_ONLY) {
		RX_DEF_LOG("[RX]boot, tx is plug in only\n");
		myhdmi->u1TxEdidReady = HDMI_PLUG_IN_ONLY;
	} else {
		RX_DEF_LOG("[RX]boot, tx is plug out\n");
		myhdmi->u1TxEdidReady = HDMI_PLUG_OUT;
	}
}

void vEdidUpdataChk(struct MTK_HDMI *myhdmi)
{
	u8 bData;
	u32 u4Count;
	u8 edid_buf[128];

	bData = myhdmi->_fgHDMIRxBypassMode;
	if ((myhdmi->u1TxEdidReady != myhdmi->u1TxEdidReadyOld) ||
		(myhdmi->_bHdmiAudioOutputMode != bData) ||
		myhdmi->chg_edid) {
		hdmi2_set_hpd_low_timer(myhdmi, 0);
		hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
		hdmi2com_scdc_status_clr(myhdmi);

		RX_DEF_LOG("[RX]audio mode:new=%d,old=%d,chg_edid=%d\n",
			bData,
			myhdmi->_bHdmiAudioOutputMode,
			myhdmi->chg_edid);

		myhdmi->chg_edid = FALSE;
		myhdmi->u1TxEdidReadyOld = myhdmi->u1TxEdidReady;
		myhdmi->_bHdmiAudioOutputMode = bData;
		if (hdmi2_is_rpt(myhdmi))
			txapi_hdmi_enable(myhdmi, FALSE);

		if (myhdmi->u1TxEdidReady == HDMI_PLUG_OUT)	{
			RX_DEF_LOG("[RX] not connect TV, set default EDID\n");
			Default_Edid_BL0_BL1_Write(myhdmi);
			//hdmi2_hdcp_set_receiver(myhdmi);
		} else {
			//if (txapi_is_plugin(myhdmi))
			//	hdmi2_hdcp_set_repeater(myhdmi);
			//else
			//	hdmi2_hdcp_set_receiver(myhdmi);

			if (myhdmi->_bHdmiAudioOutputMode)
				vSetEdidUpdateMode(myhdmi, 1, 1);
			else
				vSetEdidUpdateMode(myhdmi, 1, 0);

			EdidProcessing(myhdmi);

			if (myhdmi->my_debug == 0x81) {
				memcpy(&edid_buf[0], &cts_4k_edid[0], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &cts_4k_edid[128], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&edid_buf[0]);
			}
			if (myhdmi->my_debug == 0x82) {
				memcpy(&edid_buf[0], &test_4_edid[0], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &test_4_edid[0x80], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &test_4_edid[0x100], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 2,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &test_4_edid[0x180], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 3,
					&edid_buf[0]);
			}
			if (myhdmi->my_debug == 0x83) {
				hdmi2_edid_chksum(&myhdmi->_bEdidData[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&myhdmi->_bEdidData[0]);
				hdmi2_edid_chksum(&myhdmi->_bEdidData[128]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&myhdmi->_bEdidData[128]);
			}
			if (myhdmi->my_debug == 0x84) {
				memcpy(&edid_buf[0], &pdYMH1800Edid[0], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &pdYMH1800Edid[128], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&edid_buf[0]);
			}
			if (myhdmi->my_debug == 0x85) {
				memcpy(&edid_buf[0], &vrr30to60[0], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &vrr30to60[128], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&edid_buf[0]);
			}
			if (myhdmi->my_debug == 0x86) {
				memcpy(&edid_buf[0], &vrr24to60[0], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 0,
					&edid_buf[0]);
				memcpy(&edid_buf[0], &vrr24to60[128], 128);
				hdmi2_edid_chksum(&edid_buf[0]);
				hdmi2com_write_edid_to_sram(myhdmi, 1,
					&edid_buf[0]);
			}
		}

		for (u4Count = 0; u4Count < 512; u4Count++) {
			if (u4Count < 256)
				myhdmi->u1HDMIINEDID[u4Count]
					= myhdmi->u1HDMIIN1EDID[u4Count];
			else {
				myhdmi->u1HDMIINEDID[u4Count]
					= myhdmi->u1HDMIIN2EDID[u4Count - 256];
				if (u4Count == 256) //  For PA address
					myhdmi->u1HDMIINEDID[u4Count]
						= myhdmi->_u1EDID0PAOFF + 0x80;
				if (u4Count == 257) //For PA High Byte
					myhdmi->u1HDMIINEDID[u4Count]
						= (myhdmi->_u2EDID0PA >> 8)
						& 0xFF;
				if (u4Count == 258) //For PA LOW Byte
					myhdmi->u1HDMIINEDID[u4Count]
						= myhdmi->_u2EDID0PA & 0xFF;
			}
		}

		hdmi_rx_notify(myhdmi, HDMI_RX_EDID_CHANGE);

		hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
	}
}

void
hdmi2_color_space_convert(struct MTK_HDMI *myhdmi)
{
	hdmi2com_set_rgb_to_ycbcr(myhdmi, 0);
	hdmi2com_set_420_to_444(myhdmi, 0);

	if (hdmi2com_input_color_space_type(myhdmi) == 0) {
		/* YUV */
		hdmi2com_set_output_format_level_color(myhdmi, 0, 0, 0);
		hdmi2com_set_input_format_level_color(myhdmi, 0, 0, 0);
		if (hdmi2_is_420_mode(myhdmi)) {
			hdmi2com_420_2x_clk_config(myhdmi, 1);
			hdmi2com_set_420_to_444(myhdmi, 1);
			RX_DET_LOG("[RX]YUV420->YUV444\n");
		} else if (hdmi2com_is_422_input(myhdmi)) {
			hdmi2com_420_2x_clk_config(myhdmi, 0);
			hdmi2com_set_422_to_444(myhdmi, 1);
			RX_DET_LOG("[RX]YUV422->YUV444\n");
		} else {
			hdmi2com_420_2x_clk_config(myhdmi, 0);
			RX_DET_LOG("[RX]YUV444->YUV444\n");
		}
	} else {
		/* RGB */
		hdmi2com_420_2x_clk_config(myhdmi, 0);
		hdmi2com_set_output_format_level_color(myhdmi, 1, 0, 0);
		hdmi2com_set_input_format_level_color(myhdmi, 1, 0, 0);
		RX_DET_LOG("[RX]RGB->RGB\n");
	}
}

void
hdmi2_bypass_cs_convert(struct MTK_HDMI *myhdmi)
{
	hdmi2com_set_rgb_to_ycbcr(myhdmi, 0);
	hdmi2com_set_420_to_444(myhdmi, 0);
	hdmi2com_420_2x_clk_config(myhdmi, 0);
	hdmi2com_set_output_format_level_color(myhdmi, 1, 0, 0);
	hdmi2com_set_input_format_level_color(myhdmi, 1, 0, 0);
	RX_DET_LOG("[RX]bypass CS convert\n");

	if (hdmi2com_input_color_space_type(myhdmi) == 0) {
		/* YUV */
		hdmi2com_set_output_format_level_color(myhdmi, 0, 0, 0);
		hdmi2com_set_input_format_level_color(myhdmi, 0, 0, 0);
		if (hdmi2_is_420_mode(myhdmi)) {
			hdmi2com_420_2x_clk_config(myhdmi, 1);
			hdmi2com_set_420_to_444(myhdmi, 1);
			RX_DET_LOG("[RX]YUV420->YUV444\n");
		}
	}
}

void
hdmi2_video_setting(struct MTK_HDMI *myhdmi)
{
	hdmi2com_set_color_ralated(myhdmi);
}

u8
hdmi2_get_hdmi_mode(struct MTK_HDMI *myhdmi)
{
	return myhdmi->HdmiMode;
}

u8
hdmi2_get_color_depth(struct MTK_HDMI *myhdmi)
{
	return hdmi2com_get_deep_status(myhdmi);
}

void
hdmi2_avi_info_parser(struct MTK_HDMI *myhdmi)
{
	u8 u1Colorimetry;
	u8 u1ExtColorrimetry;
	u8 u1ColorInfo;
	u8 u1Quant;
	u8 u1Yquant;

	if (hdmi2_get_hdmi_mode(myhdmi) == HDMI_RX_MODE_DVI) {
		myhdmi->HdmiClrSpc = 0;
		myhdmi->HdmiRange = 0;
		myhdmi->HdmiScan = 0;
		myhdmi->HdmiAspect = 0;
		myhdmi->HdmiAFD = 0;
		myhdmi->HdmiITC.fgItc = 0;
		myhdmi->HdmiITC.ItcContent = 0;
		return;
	}
	if (myhdmi->AviInfo.u1InfoID != HDMI_INFO_FRAME_ID_AVI) {
		myhdmi->HdmiClrSpc = 0;
		myhdmi->HdmiRange = 0;
		myhdmi->HdmiScan = 0;
		myhdmi->HdmiAspect = 0;
		myhdmi->HdmiAFD = 0;
		myhdmi->HdmiITC.fgItc = 0;
		myhdmi->HdmiITC.ItcContent = 0;
		return;
	}

	u1Colorimetry = (myhdmi->AviInfo.u1InfoData[5] & 0xC0) >> 6;
	u1ExtColorrimetry = (myhdmi->AviInfo.u1InfoData[6] & 0x70) >> 4;
	u1ColorInfo = (myhdmi->AviInfo.u1InfoData[4] & 0x60) |
		      (u1Colorimetry << 3) | u1ExtColorrimetry;
	u1Quant = (myhdmi->AviInfo.u1InfoData[6] & 0x06) >> 2;
	u1Yquant = (myhdmi->AviInfo.u1InfoData[8] & 0xc0) >> 6;

	switch (u1ColorInfo) {
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_RGB;
		break;
	case 0x1C:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_Adobe_RGB;
		break;
	case 0x1E:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_RGB_non_const_luminous;
		break;
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x29:
	case 0x2a:
	case 0x2b:
	case 0x2c:
	case 0x2d:
	case 0x2e:
	case 0x2f:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC422_601;
		break;
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC422_709;
		break;
	case 0x38:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC422_601;
		break;
	case 0x39:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC422_709;
		break;
	case 0x3A:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_sYCC422_601;
		break;
	case 0x3B:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_Adobe_YCC422_601;
		break;
	case 0x3D:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC422_const_luminous;
		break;
	case 0x3E:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC422_non_const_luminous;
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC444_601;
		break;
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC444_709;
		break;
	case 0x58:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC444_601;
		break;
	case 0x59:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC444_709;
		break;
	case 0x5A:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_sYCC444_601;
		break;
	case 0x5B:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_Adobe_YCC444_601;
		break;
	case 0x5D:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC444_const_luminous;
		break;
	case 0x5E:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC444_non_const_luminous;
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6a:
	case 0x6b:
	case 0x6c:
	case 0x6d:
	case 0x6e:
	case 0x6f:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC420_601;
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_YC420_709;
		break;
	case 0x78:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC420_601;
		break;
	case 0x79:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_XVYC420_709;
		break;
	case 0x7A:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_sYCC420_601;
		break;
	case 0x7B:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_Adobe_YCC420_601;
		break;
	case 0x7D:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC420_const_luminous;
		break;
	case 0x7E:
		myhdmi->HdmiClrSpc =
			HDMI_RX_CLRSPC_BT_2020_YCC420_non_const_luminous;
		break;
	default:
		myhdmi->HdmiClrSpc = HDMI_RX_CLRSPC_RGB;
		break;
	}

	if (u1Quant == 1)
		myhdmi->HdmiRange = HDMI_RX_RGB_LIMT;
	else if (u1Quant == 2)
		myhdmi->HdmiRange = HDMI_RX_RGB_FULL;

	if (u1Yquant == 1)
		myhdmi->HdmiRange = HDMI_RX_YCC_LIMT;
	else if (u1Yquant == 2)
		myhdmi->HdmiRange = HDMI_RX_YCC_FULL;
}

void
hdmi2_vsi_info_parser(struct MTK_HDMI *myhdmi)
{
	u8 u1IeeeCode[3] = {0};

	if (hdmi2_get_hdmi_mode(myhdmi) == HDMI_RX_MODE_DVI) {
		myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Str = 0;
		return;
	}

	if (myhdmi->VsiInfo.u1InfoID != HDMI_INFO_FRAME_ID_VSI) {
		myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Str = 0;
		return;
	}

	u1IeeeCode[0] = myhdmi->VsiInfo.u1InfoData[4];
	u1IeeeCode[1] = myhdmi->VsiInfo.u1InfoData[5];
	u1IeeeCode[2] = myhdmi->VsiInfo.u1InfoData[6];

	if (myhdmi->VsiInfo.fgValid == TRUE) {
		if ((u1IeeeCode[0] == 0x03) && (u1IeeeCode[1] == 0x0C) &&
			(u1IeeeCode[2] == 0x00)) {
			if ((myhdmi->VsiInfo.u1InfoData[7] >> 5) == 0x2) {
				myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 1;
				myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format =
					myhdmi->VsiInfo.u1InfoData[7] >> 5;
				myhdmi->Hdmi3DInfo.HDMI_3D_Str =
					myhdmi->VsiInfo.u1InfoData[8] >> 4;
				if (myhdmi->Hdmi3DInfo.HDMI_3D_Str ==
					HDMI_3D_SideBySideHalf) {
					myhdmi->Hdmi3DInfo.HDMI_3D_EXTDATA =
						myhdmi->VsiInfo.u1InfoData[9] >>
						4;
				} else {
					myhdmi->Hdmi3DInfo.HDMI_3D_EXTDATA = 0;
				}
			}
		} else if ((u1IeeeCode[0] == 0x1d) && (u1IeeeCode[1] == 0xa6) &&
			   (u1IeeeCode[2] == 0x7c)) {
			if ((myhdmi->VsiInfo.u1InfoData[7] & 0x01) == 0x01) {
				myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 1;
				switch ((myhdmi->VsiInfo.u1InfoData[7] >> 2)
					& 0x0f) {
				case 0x00:
					myhdmi->Hdmi3DInfo.HDMI_3D_Str =
						HDMI_3D_FramePacking;
					break;
				case 0x01:
					myhdmi->Hdmi3DInfo.HDMI_3D_Str =
						HDMI_3D_TopBottom;
					break;
				case 0x02:
					myhdmi->Hdmi3DInfo.HDMI_3D_Str =
						HDMI_3D_SideBySideHalf;
					break;
				default:
					break;
				}
				myhdmi->Hdmi3DInfo.HDMI_3D_EXTDATA = 0;
			}
		} else {
			myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 0;
			myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format = 0;
			myhdmi->Hdmi3DInfo.HDMI_3D_Str = 0;
		}
	} else {
		myhdmi->Hdmi3DInfo.HDMI_3D_Enable = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Video_Format = 0;
		myhdmi->Hdmi3DInfo.HDMI_3D_Str = 0;
	}
}

void
hdmi2_info_parser(struct MTK_HDMI *myhdmi)
{
	hdmi2_avi_info_parser(myhdmi);
	hdmi2_vsi_info_parser(myhdmi);
}

u8
hdmi2_get_hdcp1x_status(struct MTK_HDMI *myhdmi)
{
	return hdmi2com_hdcp1x_status(myhdmi);
}

u8
hdmi2_get_hdcp2x_status(struct MTK_HDMI *myhdmi)
{
	return hdmi2com_hdcp2x_status(myhdmi);
}

bool
hdmi2_is_debug(struct MTK_HDMI *myhdmi, u32 u4MessageType)
{
	if (myhdmi->debugtype & u4MessageType)
		return TRUE;
	else
		return FALSE;
}

void
hdmi2_check_5v_status(struct MTK_HDMI *myhdmi)
{
	u32 u4Pwr5VStatus = 0;

	u4Pwr5VStatus = hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1);
	if (u4Pwr5VStatus != myhdmi->p5v_status) {
		myhdmi->p5v_status = u4Pwr5VStatus;
		hdmi2_handle_port_5v_changed(myhdmi);
		RX_DEF_LOG("[RX]5v = 0x%x,%lums\n", myhdmi->p5v_status,
			jiffies);
		hdmi_rx_notify(myhdmi, HDMI_RX_PWR_5V_CHANGE);
	}
}

bool
hdmi2_get_5v_level(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1) == TRUE)
		return TRUE;
	return FALSE;
}

void
hdmi2_get_audio_info(struct MTK_HDMI *myhdmi, struct AUDIO_INFO *pv_get_info)
{
	aud2_get_audio_info(myhdmi, pv_get_info);
}

u8
hdmi2_timing_stb_check(struct MTK_HDMI *myhdmi)
{
	u8 temp;

	temp = 0;
	if (hdmi2com_is_ckdt_1(myhdmi))
		temp = temp | 1;
	if (hdmi2com_is_scdt_1(myhdmi))
		temp = temp | 2;
	if (hdmi2com_is_h_res_stb(myhdmi))
		temp = temp | 4;
	if (hdmi2com_is_v_res_stb(myhdmi))
		temp = temp | 8;

	return temp;
}

void hdmi2_rx_info(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->HDMIState < ARRAY_SIZE(rx_state_str))
		RX_DEF_LOG("[RX]%s,%lums\n",
			rx_state_str[myhdmi->HDMIState], jiffies);
	else
		RX_DEF_LOG("[RX]s:unknown\n");

	if (((u8)myhdmi->hdcp_state) < ARRAY_SIZE(cRxHdcpStatus))
		RX_DEF_LOG("[RX]HDCP STATUS = %s,%lums\n",
			cRxHdcpStatus[(u8)myhdmi->hdcp_state], jiffies);
}

/*** hdcp repeater ***/
void hdmi2_hdcp_set_receiver(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->force_hdcp_mode == 2) {
		RX_DEF_LOG("[RX][2]force RPT MODE,%lums\n", jiffies);
		hdmi2_hdcp_set_repeater(myhdmi);
		return;
	}
	hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 0);
	hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 0);
	hdmi2com_hdcp2_reset(myhdmi);
	myhdmi->hdcp_mode = HDCP_RECEIVER;
}

void hdmi2_hdcp_set_repeater(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->force_hdcp_mode == 1) {
		RX_DEF_LOG("[RX][HDCP]force RCV MODE,%lums\n", jiffies);
		hdmi2_hdcp_set_receiver(myhdmi);
		return;
	}
	hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 1);
	hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 1);
	hdmi2com_hdcp2_reset(myhdmi);
	myhdmi->hdcp_mode = HDCP_REPEATER;
}

void hdmi2_set_hdcp_mode(struct MTK_HDMI *myhdmi, u8 u1Mode)
{
	myhdmi->hdcp_mode = u1Mode;
	if (myhdmi->hdcp_mode == 1)
		hdmi2_hdcp_set_receiver(myhdmi);
	else if (myhdmi->hdcp_mode == 2)
		hdmi2_hdcp_set_repeater(myhdmi);
}

bool hdmi2_is_hdcp2x_done(struct MTK_HDMI *myhdmi)
{
	if ((hdmi2com_hdcp2x_state(myhdmi) == 40)
		|| (hdmi2com_hdcp2x_state(myhdmi) == 15)
		|| (hdmi2com_hdcp2x_state(myhdmi) == 21)
		|| (hdmi2com_hdcp2x_state(myhdmi) == 24)
		|| (hdmi2com_hdcp2x_state(myhdmi) == 27)
		|| (hdmi2com_hdcp2x_state(myhdmi) == 29))
		return FALSE;

	return TRUE;
}

bool hdmi2_is_upstream_need_auth(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->hdcp_state == UnAuthenticated)
		return FALSE;
	else
		return TRUE;
}

void hdmi2_set_hdcp_auth_done(struct MTK_HDMI *myhdmi,
	bool fgTxHdcpAuthDone)
{
	myhdmi->auth_done = fgTxHdcpAuthDone;

	if (fgTxHdcpAuthDone)
		RX_HDCP_LOG("TX Auth Done\n");
	else
		RX_HDCP_LOG("TX UnAuth\n");
}

bool hdmi2_is_tx_auth_done(struct MTK_HDMI *myhdmi)
{
	return myhdmi->auth_done;
}

void hdmi2_set_hdcp_state(struct MTK_HDMI *myhdmi,
	enum RxHdcpState bStatus)
{
	myhdmi->hdcp_state = bStatus;
	if (((u8)myhdmi->hdcp_state) < ARRAY_SIZE(cRxHdcpStatus))
		RX_HDCP_LOG("state: %s,%lums\n",
			cRxHdcpStatus[(u8)myhdmi->hdcp_state], jiffies);
}

void hdmi2_auth_start_init(struct MTK_HDMI *myhdmi)
{
	RX_HDCP_LOG("[rx]%s\n", __func__);
	hdmi2_set_hdcp_state(myhdmi, UnAuthenticated);
	hdmi2com_set_hdcp1_vready(myhdmi, FALSE);
}

void hdmi2_set_hdcp2_stream(struct MTK_HDMI *myhdmi,
	bool is_clear, u32 stream_type)
{
	if ((stream_type & 0xFF) == 0) {
		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
			RX_DEF_LOG("[RX]%d,stream=0x%x,un-mute.\n",
				is_clear, stream_type);
		myhdmi->is_stream_mute = FALSE;
		hdmi2_rx_av_mute(myhdmi, FALSE, FALSE);
	} else {
		if (myhdmi->rpt_data.is_hdcp2 == FALSE) {
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
				RX_DEF_LOG("[RX]%d,stream=0x%x,mute\n",
					is_clear, stream_type);
			myhdmi->is_stream_mute = TRUE;
			hdmi2_rx_av_mute(myhdmi, TRUE, FALSE);
		} else {
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
				RX_DEF_LOG("[RX]%d,stream=0x%x,but un-mute\n",
					is_clear, stream_type);
			myhdmi->is_stream_mute = FALSE;
			hdmi2_rx_av_mute(myhdmi, FALSE, FALSE);
		}
	}
}

void
hdmi2_hdcp1x_service(struct MTK_HDMI *myhdmi)
{
	u8 _bNewHDMIRxHDCPStatus;
	u32 temp;

	_bNewHDMIRxHDCPStatus = hdmi2com_check_hdcp1x_decrypt_on(myhdmi);
	if (myhdmi->hdcpstatus != _bNewHDMIRxHDCPStatus) {
		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
			RX_DEF_LOG("[RX]encryption status,old=%d,new=%d\n",
				myhdmi->hdcpstatus, _bNewHDMIRxHDCPStatus);
		myhdmi->hdcpstatus = _bNewHDMIRxHDCPStatus;

		if ((myhdmi->my_debug1 == 10) &&
			(hdmi2_get_state(myhdmi) == rx_is_stable) &&
			myhdmi->hdcpstatus) {
			hdmi2com_crc(myhdmi, 5);
			temp = myhdmi->crc0;
			myhdmi->crc0 = 0;
			usleep_range(50000, 50050);
			hdmi2com_crc(myhdmi, 5);
			if ((temp == myhdmi->crc0) && (temp != 0))
				RX_DEF_LOG("[RX][1]=>pass\n");
			else
				RX_DEF_LOG("[RX][1]=>fail\n");
		}
	}

	if (myhdmi->hdcp_mode == HDCP_RECEIVER) {
		if (hdmi2com_get_hdcp1x_rpt_mode(myhdmi, FALSE) == TRUE)
			hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 0);

		if (hdmi2com_is_hdcp1x_auth_start(myhdmi)) {
			myhdmi->hdcp_stable_count++;

			myhdmi->hdcp_version = 14;
			myhdmi->hdcp_flag = FALSE;
			hdmi_rx_notify(myhdmi, HDMI_RX_HDCP_VERSION);

			if (!(hdmi2_get_state(myhdmi) == rx_is_stable))
				RX_DEF_LOG("[RX]un-stb,hdcp err,%lums\n",
					jiffies);

			hdmi2com_get_aksv(myhdmi, myhdmi->AKSVShadow);
			hdmi2com_get_bksv(myhdmi, myhdmi->BKSVShadow);
			hdmi2com_get_an(myhdmi, myhdmi->AnShadow);

			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
				RX_DEF_LOG("[RX]receiver,Auth Start,%lums\n",
					jiffies);
				RX_DEF_LOG("[RX]AKSV[1~5]=%x,%x,%x,%x,%x\n",
					myhdmi->AKSVShadow[0],
					myhdmi->AKSVShadow[1],
					myhdmi->AKSVShadow[2],
					myhdmi->AKSVShadow[3],
					myhdmi->AKSVShadow[4]);
				RX_DEF_LOG("[RX]BKSV[1~5]=%x,%x,%x,%x,%x\n",
					myhdmi->BKSVShadow[0],
					myhdmi->BKSVShadow[1],
					myhdmi->BKSVShadow[2],
					myhdmi->BKSVShadow[3],
					myhdmi->BKSVShadow[4]);
				RX_DEF_LOG("[RX]AN[1~4]=%x,%x,%x,%x\n",
					myhdmi->AnShadow[0],
					myhdmi->AnShadow[1],
					myhdmi->AnShadow[2],
					myhdmi->AnShadow[3]);
				RX_DEF_LOG("[RX]AN[5~8]=%x,%x,%x,%x\n",
					myhdmi->AnShadow[4],
					myhdmi->AnShadow[5],
					myhdmi->AnShadow[6],
					myhdmi->AnShadow[7]);
			}
		}
		myhdmi->RiShadow = hdmi2com_get_ri(myhdmi);
		if (myhdmi->RiShadow != myhdmi->RiShadowOld) {
			myhdmi->RiShadowOld = myhdmi->RiShadow;
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP_RI))
				RX_DEF_LOG("[RX]Rx Ri =0x%x\n",
					myhdmi->RiShadow);
		}

		if (hdmi2com_is_hdcp1x_auth_done(myhdmi)) {
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
				if ((hdmi2com_hdcp1x_status(myhdmi) &
					    HDMI_RX_HDCP1_STATUS_AUTH_DONE) ==
					HDMI_RX_HDCP1_STATUS_AUTH_DONE)
					RX_DEF_LOG("[RX]CLR SUCCESS,%lums\n",
						jiffies);
				else
					RX_DEF_LOG(
						"[RX]CLR FAIL,%lums\n",
						jiffies);
			}
		}

		return;
	}

	if (hdmi2com_get_hdcp1x_rpt_mode(myhdmi, FALSE) == FALSE)
		hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 1);

	if (hdmi2com_is_hdcp1x_auth_start(myhdmi)) {
		if (!(hdmi2_get_state(myhdmi) == rx_is_stable))
			RX_DEF_LOG("[RX]un-stb,hdcp err,%lums\n", jiffies);

		myhdmi->hdcp_version = 14;
		myhdmi->hdcp_flag = FALSE;
		hdmi_rx_notify(myhdmi, HDMI_RX_HDCP_VERSION);

		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
			hdmi2com_get_aksv(myhdmi, myhdmi->AKSVShadow);
			hdmi2com_get_bksv(myhdmi, myhdmi->BKSVShadow);
			hdmi2com_get_an(myhdmi, myhdmi->AnShadow);

			RX_DEF_LOG("[RX]rpt mode,Auth Start,%lums\n",
				jiffies);
			RX_DEF_LOG("[RX]AKSV[1~5]=%x,%x,%x,%x,%x\n",
				myhdmi->AKSVShadow[0], myhdmi->AKSVShadow[1],
				myhdmi->AKSVShadow[2], myhdmi->AKSVShadow[3],
				myhdmi->AKSVShadow[4]);
			RX_DEF_LOG("[RX]BKSV[1~5]=%x,%x,%x,%x,%x\n",
				myhdmi->BKSVShadow[0], myhdmi->BKSVShadow[1],
				myhdmi->BKSVShadow[2], myhdmi->BKSVShadow[3],
				myhdmi->BKSVShadow[4]);
			RX_DEF_LOG("[RX]AN[0~3]=%x,%x,%x,%x\n",
				myhdmi->AnShadow[0], myhdmi->AnShadow[1],
				myhdmi->AnShadow[2], myhdmi->AnShadow[3]);
			RX_DEF_LOG("[RX]AN[4~7]=%x,%x,%x,%x\n",
				myhdmi->AnShadow[4], myhdmi->AnShadow[5],
				myhdmi->AnShadow[6], myhdmi->AnShadow[7]);
		}
		hdmi2_set_hdcp_state(myhdmi, Computations);
		hdmi2_set_hdcp_auth_done(myhdmi, FALSE);
		memset(&myhdmi->rpt_data, 0, sizeof(struct RxHDCPRptData));
		txapi_hdcp_start(myhdmi, 1);
		hdmi2_set_hdcp2_stream(myhdmi, TRUE, 0);
	}

	switch (myhdmi->hdcp_state) {
	case UnAuthenticated:
		break;
	case Computations:
		if (hdmi2com_is_hdcp1x_auth_done(myhdmi)) {
			myhdmi->RiShadow = hdmi2com_get_ri(myhdmi);
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP)) {
				myhdmi->RiShadowOld = myhdmi->RiShadow;
				RX_DEF_LOG("[RX]Rx Ri=0x%x\n",
					myhdmi->RiShadow);
				if ((hdmi2com_hdcp1x_status(myhdmi) &
					HDMI_RX_HDCP1_STATUS_AUTH_DONE) ==
					HDMI_RX_HDCP1_STATUS_AUTH_DONE)
					RX_DEF_LOG("[RX]CLR success,%lums\n",
						jiffies);
				else
					RX_DEF_LOG("[RX]CLEAR fail,%lums\n",
						jiffies);
			}
			hdmi2_set_hdcp_state(myhdmi, WaitforDownstream);
			myhdmi->WaitTxKsvListCount = 0;
		}
		break;
	case WaitforDownstream:
		if (myhdmi->WaitTxKsvListCount > HDMI2_WAIT_DOWNSTREAM_TIME) {
			RX_DEF_LOG("[RX]get KSV TO,%lums\n", jiffies);
			hdmi2_auth_start_init(myhdmi);
			break;
		}
		if (hdmi2_is_tx_auth_done(myhdmi)) {
			hdmi2com_set_hdcp1_rpt(myhdmi, &myhdmi->rpt_data);
			hdmi2_set_hdcp_state(myhdmi, WaitVReady);
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
				RX_DEF_LOG("[RX]get KSV,%lums\n", jiffies);
		}
		break;
	case WaitVReady:
		if (hdmi2com_is_hdcp1_vready(myhdmi)) {
			if (myhdmi->rpt_data.is_match) {
				hdmi2com_set_hdcp1_vready(myhdmi, TRUE);
				if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
					RX_DEF_LOG("[RX]v ready,%lums\n",
					jiffies);
			}
			hdmi2_set_hdcp_state(myhdmi, Authenticated);
		}
		break;
	case Authenticated:
		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP_RI)) {
			myhdmi->RiShadow = hdmi2com_get_ri(myhdmi);
			if (myhdmi->RiShadow != myhdmi->RiShadowOld) {
				myhdmi->RiShadowOld = myhdmi->RiShadow;
				RX_DEF_LOG("[RX]Rx Ri=0x%x\n",
					myhdmi->RiShadow);
			}
		}
		break;
	default:
		break;
	}
}

void hdmi2_hdcp2_int_proc(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	if (r_fld(RXHDCP2X_CMD_INTR1_MASK,
		REG_INTR2_SRC7)) {
		temp = hdmi2com_get_stream_id_type(myhdmi);
		if ((temp & 0xFFFF00) != 0x010000)
			return;
		hdmi2_set_hdcp2_stream(myhdmi, FALSE, temp & 0xFF);
	}
}

void
hdmi2_hdcp2x_service(struct MTK_HDMI *myhdmi)
{
	u8 _bNewHDMIRxHDCPStatus;
	u32 temp;

	_bNewHDMIRxHDCPStatus = hdmi2com_check_hdcp2x_decrypt_on(myhdmi);
	if (myhdmi->hdcpstatus != _bNewHDMIRxHDCPStatus) {
		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
			RX_DEF_LOG(
				"[RX][2]encryption status,old=%d,new=%d\n",
				myhdmi->hdcpstatus, _bNewHDMIRxHDCPStatus);
		myhdmi->hdcpstatus = _bNewHDMIRxHDCPStatus;

		if ((myhdmi->my_debug1 == 10) &&
			(hdmi2_get_state(myhdmi) == rx_is_stable) &&
			(hdmi2com_hdcp2x_state(myhdmi) == 0x2b) &&
			myhdmi->hdcpstatus) {
			hdmi2com_crc(myhdmi, 5);
			temp = myhdmi->crc0;
			myhdmi->crc0 = 0;
			usleep_range(50000, 50050);
			hdmi2com_crc(myhdmi, 5);
			if ((temp == myhdmi->crc0) && (temp != 0))
				RX_DEF_LOG("[RX][2]=>pass\n");
			else
				RX_DEF_LOG("[RX][2]=>fail\n");
		}
	}

	if (myhdmi->hdcp_mode == HDCP_RECEIVER) {
		if (hdmi2com_get_hdcp2x_rpt_mode(myhdmi, FALSE) == TRUE)
			hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 0);
		if (hdmi2com_is_hdcp2x_auth_start(myhdmi)) {
			hdmi2com_hdcp2x_req(myhdmi, 0);
			hdmi2com_clear_hdcp2x_accm(myhdmi);
			hdmi2com_is_hdcp2x_ecc_out_of_sync(myhdmi);

			myhdmi->hdcp_version = 22;
			myhdmi->hdcp_flag = FALSE;
			hdmi_rx_notify(myhdmi, HDMI_RX_HDCP_VERSION);

			myhdmi->hdcp_stable_count++;
			RX_DEF_LOG("[RX][2]receiver,ro_ake_sent_rcvd,%lums\n",
				jiffies);
		}
		hdmi2com_check_hdcp2x_irq(myhdmi);
		hdmi2com_hdcp2x_irq_clr(myhdmi);
		return;
	}

	if (hdmi2com_get_hdcp2x_rpt_mode(myhdmi, FALSE) == FALSE)
		hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 1);

	if (hdmi2com_is_hdcp2x_auth_start(myhdmi)) {
		hdmi2com_hdcp2x_req(myhdmi, 0);
		hdmi2com_clear_hdcp2x_accm(myhdmi);
		hdmi2com_is_hdcp2x_ecc_out_of_sync(myhdmi);

		myhdmi->hdcp_version = 22;
		myhdmi->hdcp_flag = FALSE;
		hdmi_rx_notify(myhdmi, HDMI_RX_HDCP_VERSION);

		if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
			RX_DEF_LOG("[RX][2]rpt,ro_ake_sent_rcvd,%lums\n",
				jiffies);
		hdmi2com_clear_receiver_id_setting(myhdmi);
		hdmi2_set_hdcp_state(myhdmi, WaitforDownstream);
		hdmi2_set_hdcp_auth_done(myhdmi, FALSE);
		memset(&myhdmi->rpt_data, 0, sizeof(struct RxHDCPRptData));
		txapi_hdcp_start(myhdmi, 1);
		hdmi2_set_hdcp2_stream(myhdmi, TRUE, 0);
	}

	switch (myhdmi->hdcp_state) {
	case UnAuthenticated:
		break;
	case WaitforDownstream:
		if (myhdmi->WaitTxKsvListCount >
			HDMI2_WAIT_DOWNSTREAM_TIME)	{
			if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
				RX_DEF_LOG("[RX][2]get KSV TO,%lums\n",
					jiffies);
			hdmi2_auth_start_init(myhdmi);
			break;
		}
		if (hdmi2_is_tx_auth_done(myhdmi)) {
			hdmi2com_set_hdcp2_rpt(myhdmi, &myhdmi->rpt_data);
			hdmi2_set_hdcp_state(myhdmi, Authenticated);
		}
		break;
	case Authenticated:
		break;
	default:
		break;
	}

	hdmi2com_check_hdcp2x_irq(myhdmi);
	hdmi2com_hdcp2x_irq_clr(myhdmi);
}

void
hdmi2_hdcp_service(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_is_ckdt_1(myhdmi) != myhdmi->fgCKDTDeteck) {
		if (myhdmi->fgCKDTDeteck) {
			hdmi2com_hdcp2_reset(myhdmi);
			hdmi2_auth_start_init(myhdmi);
		}
		myhdmi->fgCKDTDeteck = hdmi2com_is_ckdt_1(myhdmi);
	}

	if (!hdmi2com_is_scdt_1(myhdmi)) {
		if (myhdmi->hdcp_state != UnAuthenticated)
			if (hdmi2com_upstream_is_hdcp2x_device(myhdmi) == FALSE)
				hdmi2_auth_start_init(myhdmi);
		myhdmi->hdcpstatus = 0;
		return;
	}

	if (hdmi2com_upstream_is_hdcp2x_device(myhdmi))
		hdmi2_hdcp2x_service(myhdmi);
	else
		hdmi2_hdcp1x_service(myhdmi);
}

void
hdmi2_hdcp_status(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_upstream_is_hdcp2x_device(myhdmi)) {
		RX_DEF_LOG("[RX]upstream is hdcp2x device\n");
		if (hdmi2com_check_hdcp2x_decrypt_on(myhdmi))
			RX_DEF_LOG("[RX]upstream is hdcp2x decrpt on\n");
		else
			RX_DEF_LOG("[RX]upstream is hdcp2x decrpt off\n");
	} else {
		RX_DEF_LOG("[RX]upstream is hdcp1x device\n");
		if (hdmi2com_check_hdcp1x_decrypt_on(myhdmi))
			RX_DEF_LOG("[RX]upstream is hdcp1x decrpt on\n");
		else
			RX_DEF_LOG("[RX]upstream is hdcp1x decrpt off\n");
	}

	if (hdmi2com_is_hdcp2x_rpt_mode(myhdmi))
		RX_DEF_LOG("[RX]hdcp2 is in repeater mode\n");
	else
		RX_DEF_LOG("[RX]hdcp2 is in receiver mode\n");
	RX_DEF_LOG("[RX]hdcp2 state : 0x%x\n",
		hdmi2com_hdcp2x_state(myhdmi));

	hdmi2com_get_hdcp1x_rpt_mode(myhdmi, TRUE);
	hdmi2com_get_hdcp2x_rpt_mode(myhdmi, TRUE);
}

void
hdmi2_timer_handle(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_is_scrambing_by_scdc(myhdmi))
		myhdmi->ScdcInfo.u1ScramblingOn = 1;
	else
		myhdmi->ScdcInfo.u1ScramblingOn = 0;

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi)) {
		myhdmi->ScdcInfo.u1ClkRatio = 1;
		myhdmi->HdmiMode = HDMI_RX_MODE_HDMI2X;
	} else {
		myhdmi->ScdcInfo.u1ClkRatio = 0;
		myhdmi->HdmiMode = HDMI_RX_MODE_HDMI1X;
	}

	if (!hdmi2com_is_hdmi_mode(myhdmi))
		myhdmi->HdmiMode = HDMI_RX_MODE_DVI;

	if (myhdmi->HDMIHPDTimerCnt) {
		myhdmi->HDMIHPDTimerCnt--;
		if (myhdmi->HDMIHPDTimerCnt == HDMI2_Rsen_Period) {
			if ((hdmi2_get_state(myhdmi) != rx_no_signal) &&
			(myhdmi->portswitch != 0))
				hdmi2_tmds_term_ctrl(myhdmi, TRUE);
		}

		if (myhdmi->HDMIHPDTimerCnt == 0) {
			hdmi2_set_hpd_high(myhdmi, HDMI_SWITCH_1);
			RX_DEF_LOG("[RX]timer out, set HPD to 1\n");
		}
	}

	if ((myhdmi->HDMIState == rx_wait_timing_stable) ||
		(myhdmi->HDMIState == rx_is_stable)) {
		dvi2_timer_handle(myhdmi);
		aud2_timer_handle(myhdmi);
	}

	if (myhdmi->WaitTxKsvListCount < 255)
		myhdmi->WaitTxKsvListCount++;
}

#ifdef CC_SUPPORT_HDR10Plus
void
vHDMI2SetHdr10PlusVsiExist(struct MTK_HDMI *myhdmi,
	bool fgEmpExist)
{
	myhdmi->_rHdr10PlusVsiCtx.fgHdr10PlusVsiExist = fgEmpExist;
}

bool
vHDMI2GetHdr10PlusVSIExist(struct MTK_HDMI *myhdmi)
{
	return myhdmi->_rHdr10PlusVsiCtx.fgHdr10PlusVsiExist;
}

void
_vHDMI2Hdr10PlusVsifParseMd(struct MTK_HDMI *myhdmi,
	struct HDR10Plus_VSI_T *prHdmiHdr10PVsiCtx,
	const struct HdmiInfo *pHdr10PVsiInfo)
{
	u16 i;
	struct HDR10Plus_VSI_METADATA_T *prHdmiHdr10PVsiMd;

	RX_DEF_LOG("\n [RX]HDR10P VSI >>>>>>>\n");
	for (i = 0; i < 32; i++)
		RX_DEF_LOG("[RX]0x%X  ", pHdr10PVsiInfo->u1InfoData[i]);

	RX_DEF_LOG("\n [RX]HDR10P VSI <<<<<<<\n");
	prHdmiHdr10PVsiMd = &prHdmiHdr10PVsiCtx->rHdr10PlusVsiMetadata;
	memset(prHdmiHdr10PVsiMd, 0x00,
		sizeof(struct HDR10Plus_VSI_METADATA_T));
	prHdmiHdr10PVsiMd->u1ApplicationVersion =
		(pHdr10PVsiInfo->u1InfoData[7] & 0xC0) >> 6;
	prHdmiHdr10PVsiMd->u1TargetSystemDisplayMaximumLuminance =
		(pHdr10PVsiInfo->u1InfoData[7] & 0x3E) >> 1;
	prHdmiHdr10PVsiMd->u1AverageMaxrgb =
		pHdr10PVsiInfo->u1InfoData[8];
	memcpy(&prHdmiHdr10PVsiMd->au1DistributionValue[0],
		&pHdr10PVsiInfo->u1InfoData[9], 9);
	prHdmiHdr10PVsiMd->u1NumBezierCurveAnchrs =
		(pHdr10PVsiInfo->u1InfoData[18] & 0xF0) >> 4;
	prHdmiHdr10PVsiMd->u2KneePointX =
		((pHdr10PVsiInfo->u1InfoData[18] & 0x0F) << 6)+
		((pHdr10PVsiInfo->u1InfoData[19] & 0xFC) >> 2);
	prHdmiHdr10PVsiMd->u2KneePointY =
		((pHdr10PVsiInfo->u1InfoData[19] & 0x03) << 8) +
		(pHdr10PVsiInfo->u1InfoData[20]);
	memcpy(&prHdmiHdr10PVsiMd->u1BezierCurveAnchors[0],
		&pHdr10PVsiInfo->u1InfoData[21], 9);
	prHdmiHdr10PVsiMd->u1GraphicsOverlayFlag =
		(pHdr10PVsiInfo->u1InfoData[30] & 0x80) >> 7;
	prHdmiHdr10PVsiMd->u1NoDelayFlag =
		(pHdr10PVsiInfo->u1InfoData[30] & 0x40) >> 6;
	if (myhdmi->_rHdr10PlusVsiCtxBuff->u2CurrVSIMetadataIndex <
		HDR10P_MAX_METADATA_BUF) {
		memcpy(myhdmi->_rHdr10PlusVsiCtxBuff +
			myhdmi->_rHdr10PlusVsiCtxBuff->u2CurrVSIMetadataIndex,
			prHdmiHdr10PVsiCtx,
			sizeof(struct HDR10Plus_VSI_T));
		myhdmi->_rHdr10PlusVsiCtxBuff->u2CurrVSIMetadataIndex++;
	}
}

void
_vHDMI2ParseHdr10PlusVsiHandle(struct MTK_HDMI *myhdmi)
{
	const u8 *pIeeeCode;
	struct HdmiInfo _rHdr10PVsiInfo;
	u16 u2Length = 0;

	memset(&_rHdr10PVsiInfo, 0, sizeof(_rHdr10PVsiInfo));
	vHDMI2ComGetHDR10PlusVsiMetadata(myhdmi, &_rHdr10PVsiInfo);
	pIeeeCode = &_rHdr10PVsiInfo.u1InfoData[4];
	if ((pIeeeCode[0] == 0x8B) &&
		(pIeeeCode[1] == 0x84) &&
		(pIeeeCode[2] == 0x90)) {
		vHDMI2SetHdr10PlusVsiExist(myhdmi, TRUE);
		u2Length = r_reg_1b(HEADER_VER1_HEADER_BYTE2 + 3)
			& 0x1f;
		myhdmi->_rHdr10PlusVsiCtx.u2Length = u2Length;
		_vHDMI2Hdr10PlusVsifParseMd(myhdmi,
			&myhdmi->_rHdr10PlusVsiCtx,
			&_rHdr10PVsiInfo);
		/* _vHDMI2Hdr10PlusVsifNotifyToVDP(eHdmiPort); */
	} else {
		vHDMI2SetHdr10PlusVsiExist(myhdmi, FALSE);
	}
}

bool
fgHDMI2GetHdr10PlusVsiMetadataBuff(struct MTK_HDMI *myhdmi,
	u16 u2Index,
	struct HDR10Plus_VSI_T *prVsiMetadata)
{
	if ((prVsiMetadata == NULL) ||
		(u2Index >=
		myhdmi->_rHdr10PlusVsiCtxBuff->u2CurrVSIMetadataIndex))
		return FALSE;

	memcpy(prVsiMetadata,
		myhdmi->_rHdr10PlusVsiCtxBuff+u2Index,
		sizeof(struct HDR10Plus_VSI_T));
	return TRUE;
}

void
vHDMI2SetHdr10PlusEmpExist(struct MTK_HDMI *myhdmi,
	bool fgEmpExist)
{
	myhdmi->_rHdr10PlusEmpCtx.fgHdr10PlusEMPExist = fgEmpExist;
}

bool vHDMI2GetHdr10PlusEMPExist(struct MTK_HDMI *myhdmi)
{
	return myhdmi->_rHdr10PlusEmpCtx.fgHdr10PlusEMPExist;
}

static void
_vHDMI2DynamicHdrEmpClrInt(struct MTK_HDMI *myhdmi,
	u8 u1Fifo)
{
	vHDMI2ComDynamicMetaDataClrInt(myhdmi, u1Fifo);
}

bool
u4HDMI2UserDataRegITUT35ST209440(struct MTK_HDMI *myhdmi,
	struct HDMI_HDR_EMP_MD_T *prBs)
{
	u32 i, j, w;
	struct HDMI_HDR10PLUS_EMP_METADATA_T *pdata;
	bool u4Ret = TRUE;

	memset(&myhdmi->_rHdr10PlusEmpCtx.rHdr10PlusEmpMetadata, 0,
		sizeof(myhdmi->_rHdr10PlusEmpCtx.rHdr10PlusEmpMetadata));
	pdata = &myhdmi->_rHdr10PlusEmpCtx.rHdr10PlusEmpMetadata;

	RX_DEF_LOG("\n HDR10P EMP\n");
	for (i = 0; i < 66; i++)
		RX_DEF_LOG("0x%X  ", prBs->au1DyMdPKT[i]);

	//0xB5;byte 1
	pdata->uCountryCode = dwHDMI2ComGetBits(prBs, 8);
	//0x003C;byte 2\byte3
	pdata->u2ProviderCode = dwHDMI2ComGetBits(prBs, 16);
	//provider_oriented_code; byte 4\byte5
	pdata->u2OrientedCode = dwHDMI2ComGetBits(prBs, 16);

	//  0x0000              <---------->    Unspecified
	//  0x0001              <---------->    ST 2094-40
	//  0x0002-0x00FF       <---------->    Reserved
	if (pdata->u2OrientedCode != 0x01)
		return FALSE;

	//application identifier,byte6
	pdata->uIdentifier = dwHDMI2ComGetBits(prBs, 8);
	//need to check u4data is 4
	//application version,byte7
	pdata->uVersion = dwHDMI2ComGetBits(prBs, 8);
	//need to check u4data is 0

	pdata->uNumWindows = dwHDMI2ComGetBits(prBs, 2);//byte 8,

	//check num_windows < 3, in spec, for(w=1;;;),
	//need to fill w == 0
	for (w = 1; w < pdata->uNumWindows; w++) {
		pdata->u2WindowUpperLeftCornerX[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2WindowUpperLeftCornerY[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2WindowLowerRightCornerX[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2WindowLowerRightCornerY[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2CenterOfEllipseX[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2CenterOfEllipseY[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->uRotationAngle[w] =
			dwHDMI2ComGetBits(prBs, 8);
		pdata->u2SemiMajorAxisInternalEllipse[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2SemiMajorAxisExternalEllipse[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->u2SemiMinorAxisExternalEllipse[w] =
			dwHDMI2ComGetBits(prBs, 16);
		pdata->uOverlapProcessOption[w] =
			dwHDMI2ComGetBits(prBs, 1);
	}

	pdata->u4TargetSystemDisplayMaximumLuminance =
		dwHDMI2ComGetBits(prBs, 27);
	pdata->bTargetSystemDisplayActualPeakLuminanceFlag =
		dwHDMI2ComGetBits(prBs, 1);

	if (pdata->bTargetSystemDisplayActualPeakLuminanceFlag) {
		pdata->NumRowTargetSysDispActualPeakLum =
			dwHDMI2ComGetBits(prBs, 5);
		pdata->NumColTargetSysDispActualPeakLum =
			dwHDMI2ComGetBits(prBs, 5);
		for (i = 0;
			i < pdata->NumRowTargetSysDispActualPeakLum;
			i++)
			for (j = 0;
				j < pdata->NumColTargetSysDispActualPeakLum;
				j++)
				pdata->MasterDispyActualPeakLum[i][j] =
					dwHDMI2ComGetBits(prBs, 4);
	}

	for (w = 0; w < pdata->uNumWindows; w++) {
		for (i = 0; i < 3; i++)
			pdata->u4Maxscl[w][i] = dwHDMI2ComGetBits(prBs, 17);
		pdata->u4AverageMaxrgb[w] =
			dwHDMI2ComGetBits(prBs, 17);
		pdata->uNumDistributionMaxrgbPercentiles[w] =
			dwHDMI2ComGetBits(prBs, 4);
		for (i = 0; i < pdata->uNumDistributionMaxrgbPercentiles[w];
			i++) {
			pdata->uDistributionMaxrgbPercentages[w][i] =
				dwHDMI2ComGetBits(prBs, 7);
			pdata->u4DistributionMaxrgbPercentiles[w][i] =
				dwHDMI2ComGetBits(prBs, 17);
		}
		pdata->u2FractionBrightPixels[w] = dwHDMI2ComGetBits(prBs, 10);
	}

	pdata->bMasteringDisplayActualPeakLuminanceFlag =
		dwHDMI2ComGetBits(prBs, 1);
	if (pdata->bMasteringDisplayActualPeakLuminanceFlag) {
		pdata->NumRowMasterDispActualPeakLum =
			dwHDMI2ComGetBits(prBs, 5);
		pdata->NumColMasterDispActualPeakLum =
			dwHDMI2ComGetBits(prBs, 5);
		for (i = 0; i < pdata->NumRowMasterDispActualPeakLum;
			i++)
			for (j = 0; j < pdata->NumColMasterDispActualPeakLum;
				j++)
				pdata->MasterDispyActualPeakLum[i][j] =
					dwHDMI2ComGetBits(prBs, 4);
	}

	for (w = 0; w < pdata->uNumWindows; w++) {
		pdata->bToneMappingFlag[w] = dwHDMI2ComGetBits(prBs, 1);
		if (pdata->bToneMappingFlag[w]) {
			pdata->u2KneePointX[w] = dwHDMI2ComGetBits(prBs, 12);
			pdata->u2KneePointY[w] = dwHDMI2ComGetBits(prBs, 12);
			pdata->uNumBezierCurveAnchors[w] =
				dwHDMI2ComGetBits(prBs, 4);
			for (i = 0; i < pdata->uNumBezierCurveAnchors[w]; i++)
				pdata->u2BezierCurveAnchors[w][i] =
					dwHDMI2ComGetBits(prBs, 10);
		}
		pdata->bColorSaturationMappingFlag[w] =
			dwHDMI2ComGetBits(prBs, 1);
		//default is 1 according to the spec
		pdata->uColorSaturationWeight[w] = 1;
		if (pdata->bColorSaturationMappingFlag[w])
			pdata->uColorSaturationWeight[w] =
				dwHDMI2ComGetBits(prBs, 6);
	}

	return u4Ret;
}

void vHDMI2InitHdr10plusMetadataBuffer(void)
{
}

bool fgHDMI2GetHdr10PlusEmpMetadataBuff(struct MTK_HDMI *myhdmi,
	u16 u2Index, struct HDR10Plus_EMP_T *prEmpMetadata)
{
	if ((prEmpMetadata == NULL) ||
		(u2Index >= myhdmi->_rHdr10PlusEmpCtxBuff->u2CurrEMPMetaIndex))
		return FALSE;

	memcpy(prEmpMetadata,
		myhdmi->_rHdr10PlusEmpCtxBuff+u2Index,
		sizeof(struct HDR10Plus_EMP_T));
	return TRUE;
}

void vHDMI2HDR10PlusEmpHandler(struct MTK_HDMI *myhdmi,
	struct HDMI_HDR_EMP_MD_T *prDynamicHdrEMPktInfo)
{
	prDynamicHdrEMPktInfo->u1ByteIndex = 6;
	vHDMI2SetHdr10PlusEmpExist(myhdmi, TRUE);
	u4HDMI2UserDataRegITUT35ST209440(myhdmi, prDynamicHdrEMPktInfo);
	myhdmi->_rHdr10PlusEmpCtx.u2Length = prDynamicHdrEMPktInfo->u2Length;

	if (myhdmi->_rHdr10PlusEmpCtxBuff->u2CurrEMPMetaIndex <
		HDR10P_MAX_METADATA_BUF) {
		memcpy(myhdmi->_rHdr10PlusEmpCtxBuff +
			myhdmi->_rHdr10PlusEmpCtxBuff->u2CurrEMPMetaIndex,
			&myhdmi->_rHdr10PlusEmpCtx,
			sizeof(struct HDR10Plus_EMP_T));
		myhdmi->_rHdr10PlusEmpCtxBuff->u2CurrEMPMetaIndex++;
	}
	/* _vHDMI2HDR10PlusEmpNotifyToVDP(eHdmiPort); */
}
#endif

void
_vHDMI20HDREMPHandler(struct MTK_HDMI *myhdmi)
{
	u8 u1FifoLen = 0;
	u8 u1FifoIndex = 0;
	u32 u4IntStatus;

	if (myhdmi->my_debug2 == 11) {
		myhdmi->my_debug2 = 0;
		RX_DEF_LOG("[RX]emp irq\n");
	}

	memset(&myhdmi->emp, 0, sizeof(struct HDMI_HDR_EMP_MD_T));

#ifdef CC_SUPPORT_HDR10Plus
	vHDMI2SetHdr10PlusEmpExist(myhdmi, FALSE);
	myhdmi->_rHdr10PlusEmpCtx.u2Length = 0;
#endif

	u4IntStatus = r_reg(DYNAMIC_HDR_RO_STATUS);
	if (u4IntStatus & Fld2Msk32(DYNAMIC_HDR_FIFO0_READY))
		u1FifoIndex = 0;
	else if (u4IntStatus & Fld2Msk32(DYNAMIC_HDR_FIFO1_READY))
		u1FifoIndex = 1;
	else
		return;

	if (myhdmi->my_debug2 == 12)
		RX_DEF_LOG("[RX]emp 0x674=0x%x\n", u4IntStatus);

#ifdef CC_SUPPORT_HDR10Plus
	_vHDMI2DynamicHdrEmpClrInt(myhdmi, u1FifoIndex);
#endif

	w_fld(DYNAMIC_HDR_CTRL0, 0x00, DYNAMIC_HDR_FIFO_READ_ADDR);
	u1FifoLen = r_reg_1b(DYNAMIC_HDR_RO_STATUS + u1FifoIndex);

	if (myhdmi->my_debug2 == 12)
		RX_DEF_LOG("[RX]emp: 0x66C=0x%x\n", u1FifoLen);

	if (u1FifoLen > (DYNAMIC_METADATA_LEN / 28)) {
		RX_DEF_LOG("[RX]emp pkt too big(%d), ignore\n", u1FifoLen);
		return;
	}
	myhdmi->emp.pkt_len = u1FifoLen;

	vHDMI2ComGetDynamicMetadata(myhdmi, u1FifoLen, &myhdmi->emp);
	if (myhdmi->my_debug2 == 1) {
		if (u1FifoLen > 0) {
			myhdmi->my_debug2 = 0;
			RX_DEF_LOG("[RX]emp cpy, pkt len=%d\n", u1FifoLen);
			myhdmi->emp_test.pkt_len = myhdmi->emp.pkt_len;
			memcpy(&myhdmi->emp_test,
				&myhdmi->emp,
				sizeof(struct HDMI_HDR_EMP_MD_T));
		}
	}

	// switch Organiztion_ID[PB2]
	switch (myhdmi->emp.au1DyMdPKT[2]) {
	case 0x00: // DV VS-EMDS case
		break;
	case 0x01: // HDR10+ EMP case
		break;
	default:   // reserved organiztion ID, invalid case
		break;
	}

	if (myhdmi->my_debug2 == 12) {
		myhdmi->my_debug2 = 0;
		RX_DEF_LOG("[RX]emp end\n");
	}
}

void hdmi2_emp_vrr_irq(struct MTK_HDMI *myhdmi)
{
	hdmi2com_vrr_emp(myhdmi);
	if (myhdmi->vrr_emp.EmpData[1] & (1 << 24)) {
		myhdmi->vrr_en = TRUE;
		myhdmi->scdt = (bool)r_fld(TOP_MISC, SCDT);
		hdmi2com_set_isr_enable(myhdmi, TRUE,
			HDMI_RX_VSYNC_INT);
	} else {
		myhdmi->vrr_en = FALSE;
		hdmi2com_set_isr_enable(myhdmi, FALSE,
			HDMI_RX_VSYNC_INT);
	}
	txapi_vrr_emp(myhdmi);
}

u32 hdmi2_vrr_frame_rate(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	if (myhdmi->vrr_emp.fgValid == TRUE) {
		temp = myhdmi->vrr_emp.EmpData[2] & 0x0300;
		temp = temp + ((myhdmi->vrr_emp.EmpData[2] >> 16) & 0xff);
		return temp;
	}
	return 0;
}

bool
hdmi2_is_pdt_clk_changed(struct MTK_HDMI *myhdmi)
{
	u32 temp;

	temp = hdmi2com_xclk_of_pdt_clk(myhdmi);
	if (hdmi2com_value_diff(myhdmi->pdtclk_count, temp) <= HDMI2_CLK_DIFF)
		return FALSE;
	return TRUE;
}

bool
hdmi2_is_clk_40x_changed(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->is_40x_mode != hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		return TRUE;
	return FALSE;
}

void
hdmi2_updata_pdt_clk(struct MTK_HDMI *myhdmi)
{
	myhdmi->pdtclk_count = hdmi2com_xclk_of_pdt_clk(myhdmi);
}

void
hdmi2_updata_clk_40x_flag(struct MTK_HDMI *myhdmi)
{
	myhdmi->is_40x_mode = hdmi2com_is_tmds_clk_radio_40x(myhdmi);
}

u8
hdmi2_get_state(struct MTK_HDMI *myhdmi)
{
	return myhdmi->HDMIState;
}

void
hdmi2_set_video_mute(struct MTK_HDMI *myhdmi, bool mute, bool force)
{
	if (force) {
		if (mute) {
			myhdmi->is_av_mute = TRUE;
			hdmi2com_vid_mute_set_force(myhdmi, TRUE);
		} else {
			myhdmi->is_av_mute = FALSE;
			hdmi2com_vid_mute_set_force(myhdmi, FALSE);
		}
	} else {
		if (myhdmi->is_av_mute) {
			if (mute == FALSE) {
				myhdmi->is_av_mute = FALSE;
				hdmi2com_vid_mute_set_force(myhdmi, FALSE);
			}
		} else {
			if (mute) {
				myhdmi->is_av_mute = TRUE;
				hdmi2com_vid_mute_set_force(myhdmi, TRUE);
			}
		}
	}
}

void hdmi2_rx_av_mute(struct MTK_HDMI *myhdmi, bool mute, bool force)
{
	if (force) {
		if (mute) {
			myhdmi->is_stream_mute = TRUE;
			hdmi2com_vid_mute_set_force(myhdmi, TRUE);
		} else {
			myhdmi->is_stream_mute = FALSE;
			hdmi2com_vid_mute_set_force(myhdmi, FALSE);
		}
	} else {
		if (mute) {
			hdmi2com_vid_mute_set_force(myhdmi, TRUE);
		} else {
			if ((hdmi2com_is_av_mute(myhdmi) == 0) &&
				(myhdmi->is_stream_mute == FALSE))
				hdmi2com_vid_mute_set_force(myhdmi,
					FALSE);
		}
	}

	if (mute == FALSE) {
		if (hdmi2com_is_video_mute(myhdmi))
			RX_STB_LOG("%s=%d,%s=%d,%s=%d\n",
				"can't un-mute,force",
				force,
				"hdmi2com_is_av_mute",
				hdmi2com_is_av_mute(myhdmi),
				"is_stream_mute",
				myhdmi->is_stream_mute);
	}
}

void hdmi2_video_mute_proc(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_is_video_mute(myhdmi)) {
		if ((hdmi2com_is_av_mute(myhdmi) == 0) &&
			(myhdmi->is_stream_mute == FALSE))
			hdmi2_rx_av_mute(myhdmi, FALSE, FALSE);
	}
}

void
hdmi2_set_state(struct MTK_HDMI *myhdmi, u8 state)
{
	if ((state < rx_state_max) && (myhdmi->HDMIState < rx_state_max))
		RX_DEF_LOG("[RX]s:%s to %s,%lums\n",
			rx_state_str[myhdmi->HDMIState], rx_state_str[state],
			jiffies);
	else
		RX_DEF_LOG("[RX]s:unknown\n");

	myhdmi->HDMIState = state;

	switch (myhdmi->HDMIState) {
	case rx_no_signal:
		hdmi2com_set_isr_enable(myhdmi, FALSE, HDMI_RX_IRQ_ALL);
#ifdef CC_SUPPORT_HDR10Plus
		vHDMI2ComDynamicMetaDataEn(myhdmi, 0);
#endif
		myhdmi->HDMIPWR5V = 0;
		myhdmi->vid_locked = 0;
		myhdmi->aud_locked = 0;
		hdmi2_sw_reset(myhdmi);
		aud2_set_state(myhdmi, ASTATE_AudioOff);
		hdmi2com_info_status_clr(myhdmi);
		hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
		hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
		hdmi2_tmds_term_ctrl(myhdmi, FALSE);
		hdmi2com_ddc_path_config(myhdmi, FALSE);
		hdmi2com_hdcp_reset(myhdmi);
		hdmi2_set_video_mute(myhdmi, TRUE, TRUE);
		hdmi2_set_un_plug_in(myhdmi);
		if (hdmi2_is_rpt(myhdmi))
			txapi_signal_off(myhdmi);
		break;

	case rx_detect_ckdt:
		hdmi2com_set_isr_enable(myhdmi, FALSE, HDMI_RX_IRQ_ALL);
#ifdef CC_SUPPORT_HDR10Plus
		vHDMI2ComDynamicMetaDataEn(myhdmi, 0);
#endif
		if (hdmi2_is_rpt(myhdmi))
			txapi_signal_off(myhdmi);
		hdmi2_set_video_mute(myhdmi, TRUE, TRUE);
		hdmi2_set_sync_lose(myhdmi, 1);
		hdmi2_sw_reset(myhdmi);
		dvi2_set_switch(myhdmi, rx_detect_ckdt);
		hdmi2com_hdcp2x_req(myhdmi, 0);
		myhdmi->vid_locked = 0;
		myhdmi->aud_locked = 0;
		myhdmi->scdt = FALSE;
		myhdmi->scdt_clr_cnt = 0;
		myhdmi->vrr_en = FALSE;
		myhdmi->hdcp_version = 0;
		myhdmi->hdcp_cnt = 0;
		myhdmi->hdcp_flag = TRUE;
		break;

	case rx_reset_analog:
		hdmi2com_set_isr_enable(myhdmi, FALSE, HDMI_RX_IRQ_ALL);
		hdmi2_rx_av_mute(myhdmi, FALSE, TRUE);
#ifdef CC_SUPPORT_HDR10Plus
		vHDMI2ComDynamicMetaDataEn(myhdmi, 0);
#endif
		hdmi2_set_hdcp2_stream(myhdmi, TRUE, 0);
		if (hdmi2_is_rpt(myhdmi))
			txapi_signal_off(myhdmi);
		myhdmi->vid_locked = 0;
		myhdmi->aud_locked = 0;
		break;

	case rx_reset_digital:
		myhdmi->scdt = FALSE;
		myhdmi->scdt_clr_cnt = 0;
		myhdmi->vrr_en = FALSE;
		memset(&myhdmi->vrr_emp, 0, sizeof(struct HdmiVRR));
		myhdmi->vrr_emp.fgValid = FALSE;
		myhdmi->vrr_emp.cnt = 0;
		#ifdef HDMIRX_RPT_EN
		hdmi2com_set_isr_enable(myhdmi, TRUE,
			HDMI_RX_VRR_INT);
		#endif
		hdmi2_set_reset_start_time(myhdmi);
		break;

	case rx_is_stable:
		myhdmi->is_timing_chg = TRUE;
		myhdmi->stable_count++;

		myhdmi->h_unstb_count = 0;
		hdmi2_set_unstb_start(myhdmi);
		hdmi2com_clear_hdcp2x_accm(myhdmi);
		hdmi2com_is_hdcp2x_ecc_out_of_sync(myhdmi);

		hdmi2com_info_status_clr(myhdmi);
		hdmi2_packet_data_init(myhdmi);
#ifdef CC_SUPPORT_HDR10Plus
		//vHDMI2ComHDR10PlusVsiEn(myhdmi);
		vHDMI2ComDynamicMetaDataEn(myhdmi, 1);
		vHDMI2SetHdr10PlusEmpExist(myhdmi, FALSE);
		vHDMI2SetHdr10PlusVsiExist(myhdmi, FALSE);
#endif
		hdmi2_stable_status_clr(myhdmi);
		hdmi2com_av_mute_clr(myhdmi, 1);
		hdmi2com_av_mute_clr(myhdmi, 0);
		hdmi2_set_video_mute(myhdmi, FALSE, TRUE);
		break;

	default:
		break;
	}
}

void
hdmi2_state(struct MTK_HDMI *myhdmi)
{
	bool is_40x = FALSE;
	bool is_need_reset = FALSE;
	u32 pdtclk;
#ifndef HDMIRX_OPTEE_EN
	u8 edid_buf[128];
#endif
#ifdef HDMIRX_OPTEE_EN
	u32 key_status;
#endif

#ifdef HDMIRX_OPTEE_EN
	if (myhdmi->hdcpinit == 0) {
		//memcpy(&edid_buf[0], &cts_4k_edid[0], 128);
		//hdmi2_edid_chksum(&edid_buf[0]);
		//hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		//memcpy(&edid_buf[0], &cts_4k_edid[128], 128);
		//hdmi2_edid_chksum(&edid_buf[0]);
		//hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);

		myhdmi->hdcpinit++;

		myhdmi->reset_wait_time = HDMI_RESET_TIMEOUT;
		myhdmi->timing_unstable_time = 80;
		myhdmi->wait_data_stable_count = 30;
		myhdmi->scdc_reset = 0;

		return;
	} else if (myhdmi->hdcpinit == 1) {
		optee_key_status(myhdmi, &key_status);
		myhdmi->key_present = key_status;
		if ((key_status & 0x3) == 0x3) {
			hdmi2_set_hpd_low_timer(myhdmi, 0);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			RX_DEF_LOG("[RX]hdcp init, key present: 0x%x\n",
				myhdmi->key_present);
			hdcp2_load_hdcp_key(myhdmi);
			hdmi2com_hdcp_init(myhdmi);
		if (myhdmi->hdcp_mode == HDCP_REPEATER) {
			hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 1);
			hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 1);
		} else {
			hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 0);
			hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 0);
		}
			hdmi2com_hdcp_reset(myhdmi);
			hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
			myhdmi->hdcpinit++;
			return;
		}
	}
#else
	if (myhdmi->hdcpinit == 0) {
		RX_DEF_LOG("[RX]hdcp init, no optee\n");
		#if HDMIRX_YOCTO
		hdcp2_load_hdcp_key(myhdmi);
		hdmi2com_hdcp_init(myhdmi);
		hdmi2com_set_hdcp1x_rpt_mode(myhdmi, 0);
		hdmi2com_set_hdcp2x_rpt_mode(myhdmi, 0);
		hdmi2com_hdcp_reset(myhdmi);
		#endif
		memcpy(&edid_buf[0], &cts_4k_edid[0], 128);
		hdmi2_edid_chksum(&edid_buf[0]);
		hdmi2com_write_edid_to_sram(myhdmi, 0, &edid_buf[0]);
		memcpy(&edid_buf[0], &cts_4k_edid[128], 128);
		hdmi2_edid_chksum(&edid_buf[0]);
		hdmi2com_write_edid_to_sram(myhdmi, 1, &edid_buf[0]);

		myhdmi->hdcpinit++;

		myhdmi->reset_wait_time = HDMI_RESET_TIMEOUT;
		myhdmi->timing_unstable_time = 80;
		myhdmi->wait_data_stable_count = 30;
		myhdmi->scdc_reset = 0;

		aud2_enable(myhdmi, TRUE);
		return;
	}
#endif

	hdmi2_check_5v_status(myhdmi);
	vBdModeChk(myhdmi);
	vEdidUpdataChk(myhdmi);

	/***   clear scdc   ***/
	if ((hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1) == FALSE)
		/*||(hdmi2com_get_hpd_level(HDMI_SWITCH_1) == FALSE) */) {
		if (myhdmi->scdc_reset != 1) {
			hdmi2com_scdc_status_clr(myhdmi);
			myhdmi->scdc_reset = 1;
		}
	} else {
		myhdmi->scdc_reset = 0;
	}

	/**********************    backward     *************************/
	/***  hdmi rx mode or 5v change  ***/
	if (!(hdmi2_get_state(myhdmi) == rx_no_signal)) {
		if ((myhdmi->portswitch != HDMI_SWITCH_1) ||
			(hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1) ==
				FALSE))
			hdmi2_set_state(myhdmi, rx_no_signal);
	}

	/***   lost ckdt   ***/
	if ((hdmi2_get_state(myhdmi) == rx_reset_analog) ||
		(hdmi2_get_state(myhdmi) == rx_reset_digital) ||
		(hdmi2_get_state(myhdmi) == rx_wait_timing_stable) ||
		(hdmi2_get_state(myhdmi) == rx_is_stable)) {
		if ((hdmi2com_is_ckdt_1(myhdmi) == FALSE) ||
			(hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1) ==
				FALSE)) {
			RX_STB_LOG("[RX]lost ckdt or hpd,%d,%lums\n",
				hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1),
				jiffies);
			hdmi2_set_state(myhdmi, rx_detect_ckdt);
		}
	}

	/***   40x clk change   ***/
	if ((hdmi2_get_state(myhdmi) == rx_reset_digital) ||
		(hdmi2_get_state(myhdmi) == rx_wait_timing_stable) ||
		(hdmi2_get_state(myhdmi) == rx_is_stable)) {
		if (hdmi2_is_clk_40x_changed(myhdmi)) {
			RX_STB_LOG("[RX]40x chg,%lums\n", jiffies);
			hdmi2_set_state(myhdmi, rx_reset_analog);
		}
	}

	/***   pdtclk change   ***/
	if ((hdmi2_get_state(myhdmi) == rx_reset_digital) ||
		(hdmi2_get_state(myhdmi) == rx_wait_timing_stable) ||
		(hdmi2_get_state(myhdmi) == rx_is_stable)) {
		/* detect pdtclk changed by HW */
		if (hdmi2_is_pdt_clk_changed(myhdmi) ||
			hdmi2com_is_pdt_clk_changed(myhdmi)) {
			RX_STB_LOG("[RX]pdtclk change,%lums\n", jiffies);
			hdmi2_set_state(myhdmi, rx_reset_analog);
		}
	}

	/***   lost scdt   ***/
	if (hdmi2_get_state(myhdmi) == rx_is_stable) {
		if (hdmi2com_is_scdt_1(myhdmi) == FALSE) {
			RX_STB_LOG("[RX]lost scdt in stb,%lums\n", jiffies);
			hdmi2_set_state(myhdmi, rx_reset_analog);
		}
	}

	/***   reset timeout 250ms  ***/
	if ((hdmi2_get_state(myhdmi) == rx_reset_digital) ||
		(hdmi2_get_state(myhdmi) == rx_wait_timing_stable)) {
		if (hdmi2_is_reset_timeout(myhdmi, myhdmi->reset_wait_time) ==
			TRUE) {
			RX_STB_LOG("[RX]reset timeout,%lums\n", jiffies);
			hdmi2_set_state(myhdmi, rx_reset_analog);
		}
	}

	/**********************    forward     *************************/
	switch (myhdmi->HDMIState) {
	case rx_no_signal:
		if ((myhdmi->portswitch == HDMI_SWITCH_1) &&
			hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1)) {
			myhdmi->HDMIPWR5V = 1;
			hdmi2_set_hpd_low_timer(myhdmi, HDMI2_HPD_Period);
			hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			hdmi2com_ddc_path_config(myhdmi, TRUE);
			hdmi2_set_state(myhdmi, rx_detect_ckdt);
		} else {
			break;
		}

	case rx_detect_ckdt:
		/***   ckdt = 1 and pdtclk stable   */
		if ((hdmi2com_is_ckdt_1(myhdmi) == FALSE) ||
			(hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1) ==
				FALSE) ||
			(hdmi2com_is_pdt_clk_changed(myhdmi)))
			break;

		myhdmi->dp_cfg = 0;
		hdmi2_set_state(myhdmi, rx_reset_analog);

		if (hdmi2com_is_pdt_clk_changed(myhdmi))
			break;

	case rx_reset_analog:
		if (hdmi2com_is_pdt_clk_changed(myhdmi))
			break;

		/*   save pdtclk/40x information  */
		hdmi2_updata_pdt_clk(myhdmi);
		hdmi2_updata_clk_40x_flag(myhdmi);

		is_40x = hdmi2com_is_tmds_clk_radio_40x(myhdmi);
		pdtclk = hdmi2com_get_ref_clk(myhdmi);

		/* set 0x178[2] to 0 */
		hdmi2com_420_2x_clk_config(myhdmi, 0);
		hdmi2com_rst_div(myhdmi);

		/*   set hdmirx analog   */
		vHDMI21PhySetting(myhdmi);
		usleep_range(10000, 10050);
		vHDMI21PhyFIFOMACRST(myhdmi);

		if (hdmi2com_is_pdt_clk_changed(myhdmi)) {
			RX_DEF_LOG("[RX]pdt chg in phy setting\n");
			break;
		}

		RX_STB_LOG("[RX]pdtclk=%d/%x/%x,is_40x=%d,dp=%d,%lums\n",
			hdmi2com_get_ref_clk(myhdmi),
			hdmi2com_xclk_of_pdt_clk(myhdmi),
			myhdmi->pdtclk_count,
			is_40x,
			hdmi2_get_deep_color_status(myhdmi),
			jiffies);

		hdmi2_set_state(myhdmi, rx_reset_digital);

		if (hdmi2com_is_pdt_clk_changed(myhdmi))
			break;

	case rx_reset_digital:
		hdmi2com_dig_phy_rst1(myhdmi);
		hdmi2com_deep_rst1(myhdmi);
		usleep_range(1000, 1050);
		hdmi2com_vid_clk_changed_clr(myhdmi);
		hdmi2_set_digital_reset_start_time(myhdmi);
		hdmi2_set_state(myhdmi, rx_wait_timing_stable);
		break;

	case rx_wait_timing_stable:
		if (hdmi2_is_digital_reset_timeout(
			    myhdmi, myhdmi->timing_unstable_time)) {
			if (hdmi2com_is_scdt_1(myhdmi) == FALSE) {
				RX_STB_LOG("[RX]lost scdt,%lums\n", jiffies);
				hdmi2_set_state(myhdmi, rx_reset_analog);
				break;
			}

			if (hdmi2com_is_vid_clk_changed(myhdmi)) {
				if ((myhdmi->is_420_cfg ==
					    hdmi2com_is_420_clk_config(
						    myhdmi)) &&
					(myhdmi->is_dp_info ==
						hdmi2_get_deep_color_status(
							myhdmi))) {
					RX_STB_LOG(
						"[RX]pclk chg1,%lums\n",
						jiffies);
					hdmi2_set_state(
						myhdmi, rx_reset_analog);
				} else {
					myhdmi->is_420_cfg =
						hdmi2com_is_420_clk_config(
							myhdmi);
					myhdmi->is_dp_info =
						hdmi2_get_deep_color_status(
							myhdmi);
				}
			}

			if ((myhdmi->h_total != hdmi2com_h_total(myhdmi)) ||
				(myhdmi->v_total !=
					hdmi2com_v_total_even(myhdmi))) {
				myhdmi->h_total = hdmi2com_h_total(myhdmi);
				myhdmi->v_total = hdmi2com_v_total_even(myhdmi);
				myhdmi->timing_stable_count = 0;
				break;
			}

			if ((hdmi2_timing_stb_check(myhdmi) == 0x0f) &&
				(hdmi2com_is_h_unstable(myhdmi) == FALSE) &&
				(hdmi2com_is_v_unstable(myhdmi) == FALSE)) {
				myhdmi->timing_stable_count++;
				if (myhdmi->timing_stable_count >= HDMI_STABLE_TIME) {
					RX_DEF_LOG(
					"[RX]stb,%x,%x,%x,%x,%x,(%d,%d),%lums\n",
					r_reg(I_FDET_IRQ_STATUS),
					r_reg(RG_FDET_LINE_COUNT),
					r_reg(RG_FDET_HSYNC_HIGH_COUNT),
					r_reg(RG_FDET_HBACK_COUNT),
					r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
					myhdmi->timing_stable_count,
					HDMI_STABLE_TIME,
						jiffies);
					hdmi2_set_state(myhdmi, rx_is_stable);
				} else {
					RX_STB_LOG(
					"[RX]wait stb,%x,%x,%x,%x,%x,(%d,%d),%lums\n",
					r_reg(I_FDET_IRQ_STATUS),
					r_reg(RG_FDET_LINE_COUNT),
					r_reg(RG_FDET_HSYNC_HIGH_COUNT),
					r_reg(RG_FDET_HBACK_COUNT),
					r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
					myhdmi->timing_stable_count,
					HDMI_STABLE_TIME,
						jiffies);
					break;
				}

			} else {
				myhdmi->timing_stable_count = 0;
				break;
			}

		} else {
			myhdmi->h_total = hdmi2com_h_total(myhdmi);
			myhdmi->v_total = hdmi2com_v_total_even(myhdmi);
			myhdmi->is_420_cfg = hdmi2com_is_420_clk_config(myhdmi);
			myhdmi->is_dp_info = hdmi2_get_deep_color_status(myhdmi);
			myhdmi->timing_stable_count = 0;
			if (hdmi2_is_420_mode(myhdmi))
				hdmi2com_420_2x_clk_config(myhdmi, 1);
			else
				hdmi2com_420_2x_clk_config(myhdmi, 0);
			hdmi2com_is_h_unstable(myhdmi);
			hdmi2com_is_v_unstable(myhdmi);
			break;
		}

	case rx_is_stable:
		is_need_reset = FALSE;

		/*** check h toatl un-stable ***/
		if (hdmi2_is_unstb_timeout(myhdmi, 300)) {
			myhdmi->h_unstb_count = 0;
			hdmi2_set_unstb_start(myhdmi);
		} else {
			if (hdmi2com_is_v_unstable(myhdmi) ||
				(hdmi2com_is_h_res_stb(myhdmi) == FALSE) ||
				(hdmi2com_value_diff(myhdmi->h_total,
					 hdmi2com_h_total(myhdmi)) >= 5)) {
				myhdmi->h_unstb_count++;
				RX_STB_LOG(
				"[RX]h un-stb(%d)=%x,%x,%x,%x,%x,%x,%lums\n",
				myhdmi->h_unstb_count,
				myhdmi->h_total,
				r_reg(I_FDET_IRQ_STATUS),
				r_reg(RG_FDET_LINE_COUNT),
				r_reg(RG_FDET_HSYNC_HIGH_COUNT),
				r_reg(RG_FDET_HBACK_COUNT),
				r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
					jiffies);
			}
		}
		/*** reset if timing is not stable ***/
		if (hdmi2com_is_v_res_stb(myhdmi) == FALSE) {
			RX_STB_LOG(
			"[RX]v res un-stb=%x,%x,%x,%x,%x,%x,%lums\n",
				hdmi2_timing_stb_check(myhdmi),
			r_reg(I_FDET_IRQ_STATUS),
			r_reg(RG_FDET_LINE_COUNT),
			r_reg(RG_FDET_HSYNC_HIGH_COUNT),
			r_reg(RG_FDET_HBACK_COUNT),
			r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
				jiffies);
			is_need_reset = TRUE;
		} else if (hdmi2com_is_v_unstable(myhdmi) &&
			   (myhdmi->my_debug != 0xa1)) {
			RX_STB_LOG(
			"[RX]v un-stb,%x,%x,%x,%x,%x,%lums\n",
			r_reg(I_FDET_IRQ_STATUS),
			r_reg(RG_FDET_LINE_COUNT),
			r_reg(RG_FDET_HSYNC_HIGH_COUNT),
			r_reg(RG_FDET_HBACK_COUNT),
			r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
				jiffies);
			is_need_reset = TRUE;
		} else if (hdmi2com_value_diff(myhdmi->v_total,
				   hdmi2com_v_total_even(myhdmi)) >= 2) {
			RX_STB_LOG(
			"[RX]v total un-stb=%x,%x,%x,%x,%x,%x,%lums\n",
				myhdmi->v_total,
			r_reg(I_FDET_IRQ_STATUS),
			r_reg(RG_FDET_LINE_COUNT),
			r_reg(RG_FDET_HSYNC_HIGH_COUNT),
			r_reg(RG_FDET_HBACK_COUNT),
			r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN),
				jiffies);
			is_need_reset = TRUE;
		} else if ((myhdmi->h_unstb_count >= 3) &&
			   (myhdmi->my_debug != 5)) {
			RX_STB_LOG("[RX]h un-stb(%d),rst,%lums\n",
				myhdmi->h_unstb_count, jiffies);
			is_need_reset = TRUE;
		}
		/*** reset if pclk changed ***/
		if (hdmi2com_is_vid_clk_changed(myhdmi)) {
			if ((myhdmi->is_420_cfg ==
				    hdmi2com_is_420_clk_config(myhdmi)) &&
				(myhdmi->is_dp_info ==
					hdmi2_get_deep_color_status(myhdmi))) {
				RX_STB_LOG("[RX]pclk chg2,%lums\n", jiffies);
				hdmi2_set_state(myhdmi, rx_reset_analog);
			} else {
				myhdmi->is_420_cfg =
					hdmi2com_is_420_clk_config(myhdmi);
				myhdmi->is_dp_info =
					hdmi2_get_deep_color_status(myhdmi);
			}
		}

		/*** reset if ECC is error ***/
		if (hdmi2com_upstream_is_hdcp2x_device(myhdmi) &&
			(hdmi2com_is_hdcp2x_decrypt_on(myhdmi) == 1)) {
			if (hdmi2com_is_hdcp2x_ecc_out_of_sync(myhdmi)) {
				is_need_reset = TRUE;
				if (hdmi2_is_debug(myhdmi, HDMI_RX_DEBUG_HDCP))
					RX_DEF_LOG(
						"[RX][2]ECC err,%lums\n",
						jiffies);
			}
		}

		/*** reset ***/
		if (is_need_reset && (myhdmi->my_debug != 6)) {
			RX_DEF_LOG("[RX]un-stb reset,%lums\n", jiffies);
			if (hdmi2com_upstream_is_hdcp2x_device(myhdmi) &&
				(hdmi2com_is_hdcp2x_decrypt_on(myhdmi) == 1)) {
				hdmi2_set_hpd_low_timer(
					myhdmi, HDMI2_HPD_Period);
				hdmi2_set_hpd_low(myhdmi, HDMI_SWITCH_1);
			}
			hdmi2_set_state(myhdmi, rx_reset_analog);
		}

		hdmi2_get_pkt(myhdmi);
		hdmi2_video_mute_proc(myhdmi);
		break;

	default:
		break;
	}

	if (hdmi2com_is_pdt_clk_changed(myhdmi))
		RX_STB_LOG("[RX]pdtclk chg,%lums\n", jiffies);
	hdmi2com_vid_clk_changed_clr(myhdmi);
	hdmi2com_timing_changed_irq_clr(myhdmi);
}

void
hdmi2_get_pkt_info(struct MTK_HDMI *myhdmi,
	u32 Type, u8 *pbuf)
{
	switch (Type) {
	case HDMI_INFO_FRAME_ID_AUD:
		memcpy(pbuf, myhdmi->AudInfo.u1InfoData, 32);
		break;
	case HDMI_INFO_FRAME_ID_AVI:
		memcpy(pbuf, myhdmi->AviInfo.u1InfoData, 32);
		break;
	case HDMI_INFO_FRAME_ID_ACP:
		if ((aud2_get_acp_type(myhdmi) == ACP_TYPE_GENERAL_AUDIO) ||
			(aud2_get_acp_type(myhdmi) == ACP_TYPE_IEC60958) ||
			(aud2_get_acp_type(myhdmi) == ACP_TYPE_DVD_AUDIO) ||
			(aud2_get_acp_type(myhdmi) == ACP_TYPE_SACD))
			memcpy(pbuf, myhdmi->AcpInfo.u1InfoData, 32);
		else
			memset(pbuf, 0, sizeof(*pbuf));
		break;
	case HDMI_INFO_FRAME_ID_87:
		memcpy(pbuf, myhdmi->Info87.u1InfoData, 32);
		break;
	default:
		break;
	}
}

void
hdmi2_debug_enable(struct MTK_HDMI *myhdmi, u32 u4MessageType)
{
	myhdmi->debugtype |= u4MessageType;
}

void
hdmi2_debug_disable(struct MTK_HDMI *myhdmi, u32 u4MessageType)
{
	myhdmi->debugtype &= ~u4MessageType;
}

void
hdmi2_show_color_space(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_input_color_space_type(myhdmi) == 0x0) {
		/* YCbCr */
		if (hdmi2com_is_420_mode(myhdmi) == TRUE) {
			/* 4:4:4 */
			RX_DEF_LOG("[RX]Color Space = YCbCr420\n");
		} else if (hdmi2com_is_422_input(myhdmi) == 1) {
			RX_DEF_LOG("[RX]Color Space = YCbCr422\n");
		} else {
			RX_DEF_LOG("[RX]Color Space = YCbCr444\n");
		}
	} else {
		/* RGB */
		RX_DEF_LOG("[RX]Color Space = RGB\n");
	}
}

void
hdmi2_show_deep_color(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_deep_color_status(myhdmi) == (u8)RX_NON_DEEP)
		RX_DEF_LOG("[RX]DEEP COLOR = 24 BIT\n");
	else if (hdmi2_get_deep_color_status(myhdmi) ==
		 (u8)RX_30BITS_DEEP_COLOR)
		RX_DEF_LOG("[RX]DEEP COLOR = 30 BIT\n");
	else if (hdmi2_get_deep_color_status(myhdmi) ==
		 (u8)RX_36BITS_DEEP_COLOR)
		RX_DEF_LOG("[RX]DEEP COLOR = 36 BIT\n");
	else if (hdmi2_get_deep_color_status(myhdmi) ==
		 (u8)RX_48BITS_DEEP_COLOR)
		RX_DEF_LOG("[RX]DEEP COLOR = 48 BIT\n");
}

void
hdmi2_show_infoframe(struct MTK_HDMI *myhdmi, struct HdmiInfo *pt)
{
	if (pt->fgValid && pt->u1InfoData[0]) {
		if (pt->u1InfoID == HDMI_INFO_FRAME_ID_AVI)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_AVI)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_SPD)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_SPD)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_AUD)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_AUD)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_MPEG)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_MPEG)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_UNREC)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_UNREC)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_ACP)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_ACP)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_VSI)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_VSI)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_DVVSI)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_DVVSI)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_METADATA)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_METADATA)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_ISRC0)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_ISRC0)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_ISRC1)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_ISRC1)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_GCP)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_GCP)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_HFVSI)
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_HFVSI)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_86)
			RX_DEF_LOG(
				"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_86)\n",
				pt->u1InfoData[0]);
		else if (pt->u1InfoID == HDMI_INFO_FRAME_ID_87)
			RX_DEF_LOG(
				"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_ID_87)\n",
				pt->u1InfoData[0]);
		else
			RX_DEF_LOG(
			"[RX]TYPE = 0x%x(HDMI_INFO_FRAME_UNKNOWN)\n",
				pt->u1InfoData[0]);

		RX_DEF_LOG("[RX]Version = 0x%x\n", pt->u1InfoData[1]);
		RX_DEF_LOG("[RX]Length = 0x%x\n", pt->u1InfoData[2]);

		/* PB0 - PB10 */
		RX_DEF_LOG("[RX]PB 0~7 =%x,%x,%x,%x,%x,%x,%x,%x\n",
			pt->u1InfoData[3], pt->u1InfoData[4], pt->u1InfoData[5],
			pt->u1InfoData[6], pt->u1InfoData[7], pt->u1InfoData[8],
			pt->u1InfoData[9], pt->u1InfoData[10]);
		RX_DEF_LOG("[RX]PB 8~15 =%x,%x,%x,%x,%x,%x,%x,%x\n",
			pt->u1InfoData[11], pt->u1InfoData[12],
			pt->u1InfoData[13], pt->u1InfoData[14],
			pt->u1InfoData[15], pt->u1InfoData[16],
			pt->u1InfoData[17], pt->u1InfoData[18]);
		RX_DEF_LOG("[RX]PB 16~23 =%x,%x,%x,%x,%x,%x,%x,%x\n",
			pt->u1InfoData[19], pt->u1InfoData[20],
			pt->u1InfoData[21], pt->u1InfoData[22],
			pt->u1InfoData[23], pt->u1InfoData[24],
			pt->u1InfoData[25], pt->u1InfoData[26]);
		RX_DEF_LOG("[RX]PB 24~28 =%x,%x,%x,%x,%x\n",
			pt->u1InfoData[27], pt->u1InfoData[28],
			pt->u1InfoData[29], pt->u1InfoData[30],
			pt->u1InfoData[31]);
	}
}

void
hdmi2_show_status(struct MTK_HDMI *myhdmi)
{
	if (hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1) == FALSE)
		RX_DEF_LOG("[RX]no 5v\n");
	else
		RX_DEF_LOG("[RX]detected 5v\n");

	RX_DEF_LOG("[RX]HPD=%d\n",
		hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1));

	if (hdmi2com_is_scrambing_by_scdc(myhdmi))
		RX_DEF_LOG("[RX]scrambing enable\n");
	else
		RX_DEF_LOG("[RX]scrambing disable\n");

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		RX_DEF_LOG("[RX]clk radio is 1/40\n");
	else
		RX_DEF_LOG("[RX]clk radio is 1/10\n");

	RX_DEF_LOG("[RX]stable status:%x\n",
		hdmi2_timing_stb_check(myhdmi));
	RX_DEF_LOG("[RX]pdtclk=%d\n",
		hdmi2com_get_ref_clk(myhdmi));
	RX_DEF_LOG("[RX]0x8f4=%x,0x88c=%x,0x890=%x,0x894=%x,0x898=%x\n",
		r_reg(I_FDET_IRQ_STATUS),
		r_reg(RG_FDET_LINE_COUNT),
		r_reg(RG_FDET_HSYNC_HIGH_COUNT),
		r_reg(RG_FDET_HBACK_COUNT),
		r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN));

	if (hdmi2com_is_hdmi_mode(myhdmi))
		RX_DEF_LOG("[RX]hdmi mode\n");
	else
		RX_DEF_LOG("[RX]dvi mode\n");

	if (hdmi2com_is_pixel_rpt(myhdmi) == TRUE) {
		RX_DEF_LOG("[RX]is pixel repeat,");
		if (hdmi2com_get_pixel_rpt(myhdmi) == 1)
			RX_DEF_LOG("time is 2x\n");
		else if (hdmi2com_get_pixel_rpt(myhdmi) == 3)
			RX_DEF_LOG("time is 4x\n");
		else
			RX_DEF_LOG("time is unknown,%x\n",
				hdmi2com_get_pixel_rpt(myhdmi));
	} else {
		RX_DEF_LOG("[RX]is not pixel repeat\n");
	}

	RX_DEF_LOG("[RX]av-mute=%d\n", hdmi2com_is_av_mute(myhdmi));

	hdmi2_show_color_space(myhdmi);
	hdmi2_show_deep_color(myhdmi);
}

void
hdmi2_show_all_infoframe(struct MTK_HDMI *myhdmi)
{
	hdmi2_get_infoframe(myhdmi);

	hdmi2_show_infoframe(myhdmi, &(myhdmi->AviInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->SpdInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->AudInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->MpegInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->UnrecInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->AcpInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->VsiInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->DVVSIInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Isrc0Info));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Isrc1Info));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->GcpInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->HfVsiInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Info86));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Info87));
}

void
hdmi2_show_all_status(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]myhdmi->debugtype=%x\n", myhdmi->debugtype);
	RX_DEF_LOG("[RX]myhdmi->my_debug=%x\n", myhdmi->my_debug);
	RX_DEF_LOG("[RX]myhdmi->my_debug1=%d\n", myhdmi->my_debug1);
	RX_DEF_LOG("[RX]myhdmi->my_debug2=%d\n", myhdmi->my_debug2);
	RX_DEF_LOG("[RX]myhdmi->task_count=%d\n", myhdmi->task_count);
	RX_DEF_LOG("[RX]myhdmi->timer_count=%d\n", myhdmi->timer_count);
	RX_DEF_LOG("[RX]myhdmi->is_rx_task_disable=%d\n",
		myhdmi->is_rx_task_disable);
	RX_DEF_LOG("[RX]myhdmi->is_rx_init=%d\n",
		myhdmi->is_rx_init);
	RX_DEF_LOG("[RX]myhdmi->power_on=%d\n",
		myhdmi->power_on);
	RX_DEF_LOG("[RX]myhdmi->main_cmd=%d\n",
		myhdmi->main_cmd);
	RX_DEF_LOG("[RX]myhdmi->main_state=%d\n",
		myhdmi->main_state);
	RX_DEF_LOG("[RX]myhdmi->hdcpinit=%d\n",
		myhdmi->hdcpinit);
	RX_DEF_LOG("[RX]myhdmi->portswitch=%d\n",
		myhdmi->portswitch);
	RX_DEF_LOG("[RX]hdmi_mode_setting=%d\n",
		myhdmi->hdmi_mode_setting);
	RX_DEF_LOG("[RX]key_present=%d\n",
		myhdmi->key_present);
	RX_DEF_LOG("[RX]hdcp_mode=%d, %d\n",
		myhdmi->hdcp_mode,
		r_fld(RX_HDCP_DEBUG, REG_REPEATER));

	RX_DEF_LOG("[RX]myhdmi->hdcpstatus=%d/%d\n", myhdmi->hdcpstatus,
		hdmi2com_check_hdcp2x_decrypt_on(myhdmi));
	RX_DEF_LOG("[RX]myhdmi->reset_wait_time=%dms\n",
		myhdmi->reset_wait_time);
	RX_DEF_LOG("[RX]myhdmi->timing_unstable_time=%d\n",
		myhdmi->timing_unstable_time);
	RX_DEF_LOG("[RX]myhdmi->wait_data_stable_count=%d\n",
		myhdmi->wait_data_stable_count);
	RX_DEF_LOG("[RX]myhdmi->scdt=%d\n",
		myhdmi->scdt);
	RX_DEF_LOG("[RX]myhdmi->scdt_clr_cnt=%d\n",
		myhdmi->scdt_clr_cnt);
	RX_DEF_LOG("[RX]myhdmi->vrr_emp.fgValid=%d\n",
		myhdmi->vrr_emp.fgValid);
	RX_DEF_LOG("[RX]myhdmi->vrr_emp.cnt=%d\n",
		myhdmi->vrr_emp.cnt);
	RX_DEF_LOG("[RX]HPD=%d\n",
		hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1));
	RX_DEF_LOG("[RX]myhdmi->isr_mark=%d\n",
		myhdmi->isr_mark);
	RX_DEF_LOG("[RX]vrr_en=%d\n",
		myhdmi->vrr_en);

	if (myhdmi->HDMIState < rx_state_max)
		RX_DEF_LOG("[RX]state:%s\n",
			rx_state_str[myhdmi->HDMIState]);
	else
		RX_DEF_LOG("[RX]state:unknown\n");

	if (hdmi2_get_port_5v_level(myhdmi, HDMI_SWITCH_1) == FALSE) {
		RX_DEF_LOG("[RX]no 5v\n");
		return;
	}
	RX_DEF_LOG("[RX]detected 5v\n");

	if (hdmi2com_is_ckdt_1(myhdmi) == FALSE) {
		RX_DEF_LOG("[RX]can not detected ckdt signal\n");
		return;
	}
	RX_DEF_LOG("[RX]detected ckdt signal\n");

	if (hdmi2com_is_scrambing_by_scdc(myhdmi))
		RX_DEF_LOG("[RX]scrambing enable\n");
	else
		RX_DEF_LOG("[RX]scrambing disable\n");

	if (hdmi2com_is_tmds_clk_radio_40x(myhdmi))
		RX_DEF_LOG("[RX]clk radio is 1/40\n");
	else
		RX_DEF_LOG("[RX]clk radio is 1/10\n");

	RX_DEF_LOG("[RX]pdtclk=%d\n",
		hdmi2com_get_ref_clk(myhdmi));
	RX_DEF_LOG("[RX]stable status:%x\n",
		hdmi2_timing_stb_check(myhdmi));
	RX_DEF_LOG("[RX]0x8f4=%x,0x88c=%x,0x890=%x,0x894=%x,0x898=%x\n",
		r_reg(I_FDET_IRQ_STATUS),
		r_reg(RG_FDET_LINE_COUNT),
		r_reg(RG_FDET_HSYNC_HIGH_COUNT),
		r_reg(RG_FDET_HBACK_COUNT),
		r_reg(RG_FDET_VSYNC_HIGH_COUNT_EVEN));

	if (hdmi2com_is_scdt_1(myhdmi) == FALSE) {
		RX_DEF_LOG("[RX]can not detected scdt signal\n");
		RX_DEF_LOG("[RX]0x2ec60=%x,0x2e0c4=%x\n",
			r_reg(RG_RX_DEBUG_CLK_XPCNT),
			r_reg(DIG_PHY_CONTROL_2));
		return;
	}
	RX_DEF_LOG("[RX]detected scdt signal\n");

	if (hdmi2com_is_hdmi_mode(myhdmi))
		RX_DEF_LOG("[RX]hdmi mode\n");
	else
		RX_DEF_LOG("[RX]dvi mode\n");

	if (hdmi2com_is_pixel_rpt(myhdmi) == TRUE) {
		RX_DEF_LOG("[RX]is pixel repeat,");
		if (hdmi2com_get_pixel_rpt(myhdmi) == 1)
			RX_DEF_LOG("[RX]time is 2x\n");
		else if (hdmi2com_get_pixel_rpt(myhdmi) == 3)
			RX_DEF_LOG("[RX]time is 4x\n");
		else
			RX_DEF_LOG("[RX]time is unknown,%x\n",
				hdmi2com_get_pixel_rpt(myhdmi));
	} else {
		RX_DEF_LOG("[RX]is not pixel repeat\n");
	}

	hdmi2_show_color_space(myhdmi);
	hdmi2_show_deep_color(myhdmi);

	RX_DEF_LOG("[RX]hdmi2com_is_h_res_stb(myhdmi):%d\n",
		hdmi2com_is_h_res_stb(myhdmi));
	RX_DEF_LOG("[RX]hdmi2com_is_v_res_stb(myhdmi):%d\n",
		hdmi2com_is_v_res_stb(myhdmi));
	RX_DEF_LOG("[RX]HDMIPWR5V:%d\n", myhdmi->HDMIPWR5V);
	RX_DEF_LOG("[RX]HDMIScdtLose:%d\n", myhdmi->HDMIScdtLose);
	RX_DEF_LOG("[RX]_u4HTotal:%d\n", myhdmi->ResInfo._u4HTotal);
	RX_DEF_LOG("[RX]_u4VTotal:%d\n", myhdmi->ResInfo._u4VTotal);
	RX_DEF_LOG("[RX]_u4Width:%d\n", myhdmi->ResInfo._u4Width);
	RX_DEF_LOG("[RX]_u4Height:%d\n", myhdmi->ResInfo._u4Height);
	RX_DEF_LOG("[RX]HdmiMode:%d\n", myhdmi->HdmiMode);
	RX_DEF_LOG("[RX]HdmiDepth:%d\n", myhdmi->HdmiDepth);
	RX_DEF_LOG("[RX]HdmiClrSpc:%d\n", myhdmi->HdmiClrSpc);
	RX_DEF_LOG("[RX]HdmiRange:%d\n", myhdmi->HdmiRange);
	RX_DEF_LOG("[RX]HdmiScan:%d\n", myhdmi->HdmiScan);
	RX_DEF_LOG("[RX]HdmiAspect:%d\n", myhdmi->HdmiAspect);
	RX_DEF_LOG("[RX]HdmiAFD:%d\n", myhdmi->HdmiAFD);
	RX_DEF_LOG("[RX]fgItc:%d\n", myhdmi->HdmiITC.fgItc);
	RX_DEF_LOG("[RX]ItcContent:%d\n", myhdmi->HdmiITC.ItcContent);
	RX_DEF_LOG("[RX]HDMI_3D_Enable:%d\n",
		myhdmi->Hdmi3DInfo.HDMI_3D_Enable);
	RX_DEF_LOG("[RX]HDMI_3D_Str:%d\n", myhdmi->Hdmi3DInfo.HDMI_3D_Str);

	hdmi2_show_infoframe(myhdmi, &(myhdmi->AviInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->SpdInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->AudInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->MpegInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->UnrecInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->AcpInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->VsiInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->DVVSIInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Isrc0Info));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Isrc1Info));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->GcpInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->HfVsiInfo));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Info86));
	hdmi2_show_infoframe(myhdmi, &(myhdmi->Info87));
}

void hdmi2_show_emp(struct MTK_HDMI *myhdmi)
{
	u32 temp;
	u32 i, err_cnt;

	RX_DEF_LOG("[RX]pkt_len: %x\n", myhdmi->emp_test.pkt_len);

	if (myhdmi->emp_test.pkt_len > (DYNAMIC_METADATA_LEN / 28))
		myhdmi->emp_test.pkt_len = 1;

	err_cnt = 0;

	for (temp = 0; temp < myhdmi->emp_test.pkt_len; temp++) {
		RX_DEF_LOG("[RX]---pkt[%d]---\n", temp);
		RX_DEF_LOG("[RX]%02X %02X %02X %02X %02X %02X %02X\n",
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 0],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 1],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 2],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 3],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 4],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 5],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 6]);
		RX_DEF_LOG("[RX]%02X %02X %02X %02X %02X %02X %02X\n",
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 7],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 8],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 9],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 10],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 11],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 12],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 13]);
		RX_DEF_LOG("[RX]%02X %02X %02X %02X %02X %02X %02X\n",
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 14],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 15],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 16],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 17],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 18],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 19],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 20]);
		RX_DEF_LOG("[RX]%02X %02X %02X %02X %02X %02X %02X\n",
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 21],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 22],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 23],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 24],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 25],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 26],
			myhdmi->emp_test.au1DyMdPKT[(temp * 28) + 27]);

		for (i = 0; i < 28; i++) {
			if ((i + temp) !=
				myhdmi->emp_test.au1DyMdPKT[(temp * 28) + i])
				err_cnt++;
		}
	}

	RX_DEF_LOG("[RX] err conut=%d\n", err_cnt);
}

void io_get_dev_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_DEV_INFO *dev_info)
{
	dev_info->hdmirx5v = myhdmi->p5v_status;
	dev_info->hpd = hdmi2com_get_hpd_level(myhdmi, HDMI_SWITCH_1);
	dev_info->power_on = myhdmi->power_on;
	dev_info->vid_locked = myhdmi->vid_locked;
	dev_info->aud_locked = myhdmi->aud_locked;
	dev_info->hdcp_version = myhdmi->hdcp_version;

	if (myhdmi->my_debug == 24) {
		RX_DEF_LOG("[RX]hdmirx5v:%d\n", dev_info->hdmirx5v);
		RX_DEF_LOG("[RX]hpd:%d\n", dev_info->hpd);
		RX_DEF_LOG("[RX]power_on:%d\n", dev_info->power_on);
		RX_DEF_LOG("[RX]vid_locked:%d\n", dev_info->vid_locked);
		RX_DEF_LOG("[RX]aud_locked:%d\n", dev_info->aud_locked);
		RX_DEF_LOG("[RX]hdcp_version:%d\n", dev_info->hdcp_version);
	}
}
