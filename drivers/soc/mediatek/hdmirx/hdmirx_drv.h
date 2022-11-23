/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMIRX_DRV_H_
#define _HDMIRX_DRV_H_

#include "mtk_hdmirx.h"

void hdmi_rx_power_on(struct MTK_HDMI *myhdmi);
void hdmi_rx_power_off(struct MTK_HDMI *myhdmi);
void hdmirx_state_callback(
	struct MTK_HDMI *myhdmi, enum HDMIRX_NOTIFY_T notify);
void hdmi_rx_notify(struct MTK_HDMI *myhdmi, enum HDMIRX_NOTIFY_T notify);
void hdmi_rx_video_notify(struct MTK_HDMI *myhdmi, enum VSW_NFY_COND_T notify);
void hdmi_rx_audio_notify(struct MTK_HDMI *myhdmi, enum AUDIO_EVENT_T notify);
void hdmi_rx_get_info(struct MTK_HDMI *myhdmi, struct HDMIRX_INFO_T *p);
void hdmi_rx_get_aud_info(struct MTK_HDMI *myhdmi, struct HDMIRX_AUD_INFO_T *p);

#endif
