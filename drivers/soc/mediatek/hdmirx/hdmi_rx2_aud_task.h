/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX_AUD_TASK_H_
#define _HDMI_RX_AUD_TASK_H_

#include "mtk_hdmirx.h"

bool aud2_get_ch_status(struct MTK_HDMI *myhdmi,
	struct RX_REG_AUDIO_CHSTS *RegChannelstatus,
	u8 audio_status);
void aud2_setup(struct MTK_HDMI *myhdmi);
void aud2_enable(struct MTK_HDMI *myhdmi, bool en);
void aud2_get_input_audio(struct MTK_HDMI *myhdmi,
	struct AUDIO_CAPS *pAudioCaps);
void aud2_mute(struct MTK_HDMI *myhdmi, bool bMute);
void aud2_get_input_ch_status(struct MTK_HDMI *myhdmi,
	struct AUDIO_CAPS *pAudioCaps);
void SetMUTE(struct MTK_HDMI *myhdmi,
	u8 AndMask, u8 OrMask);
void aud2_updata_audio_info(struct MTK_HDMI *myhdmi);
void aud2_main_task(struct MTK_HDMI *myhdmi);
void aud2_set_state(struct MTK_HDMI *myhdmi,
	enum AUDIO_STATE state);
void aud2_get_audio_info(struct MTK_HDMI *myhdmi,
	struct AUDIO_INFO *prAudIf);
u8 aud2_get_state(struct MTK_HDMI *myhdmi);
void aud2_notify_acp_type_change(struct MTK_HDMI *myhdmi,
	u8 u1ACPType);
void aud2_show_audio_status(struct MTK_HDMI *myhdmi);
void aud2_audio_out(struct MTK_HDMI *myhdmi,
	u32 aud_type, u32 aud_fmt,
	u32 aud_mclk, u32 aud_ch);
void aud2_show_status(struct MTK_HDMI *myhdmi);
void aud2_set_i2s_format(struct MTK_HDMI *myhdmi, bool fgHBR,
	enum HDMI_AUDIO_I2S_FMT_T i2s_format, bool fgBypass);
void aud2_timer_handle(struct MTK_HDMI *myhdmi);
void aud2_audio_mute(struct MTK_HDMI *myhdmi, bool mute);
bool aud2_is_lock(struct MTK_HDMI *myhdmi);
u8 aud2_get_acp_type(struct MTK_HDMI *myhdmi);
void io_get_aud_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_AUD_INFO *aud_info);

#endif
