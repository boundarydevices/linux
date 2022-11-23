/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX2_DVI_H_
#define _HDMI_RX2_DVI_H_

#include "mtk_hdmirx.h"

extern const char *szRxResStr[];

void dvi2_init(struct MTK_HDMI *myhdmi);
u16 dvi2_get_input_width(struct MTK_HDMI *myhdmi);
u16 dvi2_get_input_height(struct MTK_HDMI *myhdmi);
void dvi2_set_mode_done(struct MTK_HDMI *myhdmi);
void dvi2_set_mode_change(struct MTK_HDMI *myhdmi);
u8 dvi2_get_refresh_rate(struct MTK_HDMI *myhdmi);
bool dvi2_is_video_timing(struct MTK_HDMI *myhdmi);
void dvi2_mode_detected(struct MTK_HDMI *myhdmi);
void dvi2_check_mode_change(struct MTK_HDMI *myhdmi);
bool dvi2_is_mode_change(struct MTK_HDMI *myhdmi);
u8 dvi2_is_3d_pkt_valid(struct MTK_HDMI *myhdmi);
u32 dvi2_get_3d_v_active(struct MTK_HDMI *myhdmi);
u32 dvi2_get_3d_h_active(struct MTK_HDMI *myhdmi);
u8 dvi2_get_3d_refresh_rate(struct MTK_HDMI *myhdmi);
u8 dvi2_get_interlaced(struct MTK_HDMI *myhdmi);
void dvi2_show_status(struct MTK_HDMI *myhdmi);
bool dvi2_is_hdmi_mode(struct MTK_HDMI *myhdmi);
void dvi2_show_timing_detail(struct MTK_HDMI *myhdmi);
u32 dvi2_get_pixel_clk_value(struct MTK_HDMI *myhdmi);
void dvi2_timer_handle(struct MTK_HDMI *myhdmi);
void dvi2_set_switch(struct MTK_HDMI *myhdmi, u8 sel);
bool dvi2_is_4k_resolution(struct MTK_HDMI *myhdmi);
void dvi2_get_timing_detail(struct MTK_HDMI *myhdmi, struct HDMIRX_TIMING_T *p);
void dvi2_var_init(struct MTK_HDMI *myhdmi);
void hdmi2_get_vid_para(struct MTK_HDMI *myhdmi);
#ifdef CC_SUPPORT_HDR10Plus
u8 bDvi2GetCurrentHdr10PlusStatus(struct MTK_HDMI *myhdmi);
#endif
void io_get_vid_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_VID_PARA *vid_para);

#endif
