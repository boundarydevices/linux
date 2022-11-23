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

const char *AStateStr[7] = {
	"ASTATE_AudioOff",
	"ASTATE_RequestAudio",
	"ASTATE_ResetAudio",
	"ASTATE_WaitForReady",
	"ASTATE_AudioUnMute,",
	"ASTATE_AudioOn",
	"ASTATE_Reserved"
};

void
aud2_set_wait_start_time(struct MTK_HDMI *myhdmi)
{
	myhdmi->wait_audio_stable_start = jiffies;
}

bool
aud2_is_wait_aud_timeout(struct MTK_HDMI *myhdmi, unsigned long delay_ms)
{
	unsigned long u4DeltaTime;

	u4DeltaTime = myhdmi->wait_audio_stable_start + (delay_ms)*HZ / 1000;
	if (time_after(jiffies, u4DeltaTime))
		return TRUE;
	else
		return FALSE;
}

u32
aud2_get_err(struct MTK_HDMI *myhdmi)
{
	u32 u4AudErr = hdmi2com_get_aud_err(myhdmi);

	if (u4AudErr & HDMI_AUDIO_FIFO_UNDERRUN)
		RX_AUDIO_LOG("[RX]Error : Audio Fifo Under-run.\n");

	if (u4AudErr & HDMI_AUDIO_FIFO_OVERRUN)
		RX_AUDIO_LOG("[RX]Error : Audio Fifo Over-run.\n");

	if (u4AudErr & HDMI_AUDIO_FS_CHG)
		RX_AUDIO_LOG("[RX]Error : FS CHG Error.\n");

	return u4AudErr;
}

void
aud2_set_i2s_format(struct MTK_HDMI *myhdmi, bool fgHBR,
	enum HDMI_AUDIO_I2S_FMT_T i2s_format, bool fgBypass)
{
	if (fgHBR) {
		hdmi2com_set_aud_i2s_format(myhdmi, FORMAT_I2S, LRCK_CYC_32);
		myhdmi->aud_format = FORMAT_I2S;
	} else {
		if (fgBypass) {
			/* Set rx's format the same as tx's. */
			switch (i2s_format) {
			case HDMI_RJT_24BIT:
			case HDMI_RJT_16BIT:
				hdmi2com_set_aud_i2s_format(
					myhdmi, FORMAT_RJ, LRCK_CYC_32);
				myhdmi->aud_format = FORMAT_RJ;
				break;
			case HDMI_LJT_24BIT:
			case HDMI_LJT_16BIT:
				hdmi2com_set_aud_i2s_format(
					myhdmi, FORMAT_LJ, LRCK_CYC_32);
				myhdmi->aud_format = FORMAT_LJ;
				break;
			case HDMI_I2S_24BIT:
			case HDMI_I2S_16BIT:
				hdmi2com_set_aud_i2s_format(
					myhdmi, FORMAT_I2S, LRCK_CYC_32);
				myhdmi->aud_format = FORMAT_I2S;
				break;
			default:
				break;
			}
		} else {
			/* Set rx's format as default i2s. */
			hdmi2com_set_aud_i2s_format(
				myhdmi, FORMAT_I2S, LRCK_CYC_32);
			myhdmi->aud_format = FORMAT_I2S;
		}
	}
}

bool
aud2_get_ch_status(struct MTK_HDMI *myhdmi,
	struct RX_REG_AUDIO_CHSTS *RegChannelstatus, u8 audio_status)
{
	u8 uc;

	if ((audio_status & T_AUDIO_MASK) == T_AUDIO_OFF)
		return FALSE;

	uc = hdmi2com_get_aud_ch0(myhdmi);

	if ((audio_status & T_AUDIO_MASK) == T_AUDIO_HBR)
		RegChannelstatus->IsLPCM = 1;
	else if ((audio_status & T_AUDIO_MASK) == T_AUDIO_DSD)
		RegChannelstatus->IsLPCM = 0;
	else
		RegChannelstatus->IsLPCM = (uc & 0x02) >> 1;

	RegChannelstatus->CopyRight = (uc & 0x04) >> 2;
	RegChannelstatus->AdditionFormatInfo = (uc & 0x18) >> 3;
	RegChannelstatus->ChannelStatusMode = (uc & 0xE0) >> 5;
	RegChannelstatus->CategoryCode = hdmi2com_get_aud_ch1(myhdmi);
	RegChannelstatus->SourceNumber = hdmi2com_get_aud_ch2(myhdmi) & 0x0F;
	RegChannelstatus->ChannelNumber =
		(hdmi2com_get_aud_ch2(myhdmi) & 0xf0) >> 4;
	uc = hdmi2com_aud_fs(myhdmi);

	if (hdmi2com_is_hd_audio(myhdmi))
		RegChannelstatus->SamplingFreq = B_Fs_HBR;
	else
		RegChannelstatus->SamplingFreq = uc & 0x0F;

	RegChannelstatus->ClockAccuary = hdmi2com_get_aud_ch3(myhdmi) & 0x03;
	RegChannelstatus->WordLen = (hdmi2com_get_aud_ch4(myhdmi) & 0x0F);
	RegChannelstatus->OriginalSamplingFreq =
		(hdmi2com_get_aud_ch4(myhdmi) & 0xF0) >> 4;
	return TRUE;
}

void
aud2_get_audio_info(struct MTK_HDMI *myhdmi, struct AUDIO_INFO *prAudIf)
{
	prAudIf->u1HBRAudio = myhdmi->aud_s.u1HBRAudio;
	prAudIf->u1DSDAudio = myhdmi->aud_s.u1DSDAudio;
	prAudIf->u1RawSDAudio = myhdmi->aud_s.u1RawSDAudio;
	prAudIf->u1PCMMultiCh = myhdmi->aud_s.u1PCMMultiCh;

	if (myhdmi->aud_s.u1HBRAudio)
		myhdmi->aud_s.u1PCMMultiCh = 1;

	prAudIf->u1FsDec = myhdmi->aud_s.u1FsDec;
	prAudIf->u1I2sChanValid = myhdmi->aud_s.u1I2sChanValid;
	prAudIf->u1MCLKRatio = myhdmi->aud_s.u1MCLKRatio;

	prAudIf->u1I2sCh0Sel = hdmi2com_get_aud_ch0(myhdmi);
	prAudIf->u1I2sCh1Sel = hdmi2com_get_aud_ch1(myhdmi);
	prAudIf->u1I2sCh2Sel = hdmi2com_get_aud_ch2(myhdmi);
	prAudIf->u1I2sCh3Sel = hdmi2com_get_aud_ch3(myhdmi);
	prAudIf->u1I2sCh4Sel = hdmi2com_get_aud_ch4(myhdmi);
}

void
aud2_timer_handle(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->acr_lock_timer > 0)
		myhdmi->acr_lock_timer--;
}

bool
aud2_err_check(struct MTK_HDMI *myhdmi, struct AUDIO_CAPS *pAudioCaps_old,
	struct AUDIO_CAPS *pAudioCaps_new)
{
	bool fgError = FALSE;

	if (pAudioCaps_old->AudioFlag != pAudioCaps_new->AudioFlag) {
		RX_AUDIO_LOG("[RX]AudioFlag 0x%x(old) != 0x%x(new).\n",
			pAudioCaps_old->AudioFlag, pAudioCaps_new->AudioFlag);
		fgError = TRUE;
	}

	if (pAudioCaps_old->AudSrcEnable != pAudioCaps_new->AudSrcEnable) {
		RX_AUDIO_LOG("[RX]AudSrcEnable 0x%x(old) != 0x%x(new).\n",
			pAudioCaps_old->AudSrcEnable,
			pAudioCaps_new->AudSrcEnable);
		fgError = TRUE;
	}

	if (pAudioCaps_old->SampleFreq != pAudioCaps_new->SampleFreq) {
		RX_AUDIO_LOG("[RX]SampleFreq 0x%x(old) != 0x%x(new).\n",
			pAudioCaps_old->SampleFreq, pAudioCaps_new->SampleFreq);
		fgError = TRUE;
	}

	if (pAudioCaps_new->SampleFreq == B_Fs_UNKNOWN) {
		RX_AUDIO_LOG("[RX]Sample frequency not indicated.\n");
		fgError = TRUE;
	}

	return fgError;
}

void
aud2_check_acr(struct MTK_HDMI *myhdmi)
{
	if (hdmi2com_is_acr_lock(myhdmi)) {
		if (myhdmi->acr_lock_count >= 2) {
			hdmi2com_aud_fifo_rst(myhdmi);
			hdmi2com_get_aud_err(myhdmi);
			if ((aud2_get_err(myhdmi) &
				HDMIRX_INT_STATUS_CHK) == 0) {
				RX_DEF_LOG("[RX]acr lock,fifo ok,%dms\n",
					(ACR_LOCK_TIMEOUT -
					myhdmi->acr_lock_timer) *
					HDMI_RX_TIMER_TICKET);
				myhdmi->audio_on_delay =
					(20 / HDMI_RX_TIMER_TICKET);
				aud2_set_state(myhdmi, ASTATE_AudioUnMute);
				myhdmi->fifo_err_conut = 0;
			}
		}
		myhdmi->acr_lock_count++;
	} else if (myhdmi->acr_lock_count > 0)
		myhdmi->acr_lock_count = 0;
}

void
aud2_error_happen(struct MTK_HDMI *myhdmi)
{
	if (!hdmi2com_is_acr_lock(myhdmi)) {
		RX_AUDIO_LOG("[RX]acr un-lock\n");
	} else {
		if (aud2_get_err(myhdmi) & HDMIRX_INT_STATUS_CHK) {
			if (myhdmi->fifo_err_conut == 0) {
				RX_AUDIO_LOG("[RX]Afifo error\n");
				aud2_mute(myhdmi, TRUE);
				hdmi2com_aud_fifo_rst(myhdmi);
				hdmi2com_get_aud_err(myhdmi);
			}

			myhdmi->fifo_err_conut++;

			if (myhdmi->fifo_err_conut > 5) {
				RX_AUDIO_LOG("[RX]AFIFO Error\n");
				aud2_set_state(myhdmi, ASTATE_AudioOff);
				aud2_set_state(myhdmi, ASTATE_RequestAudio);
			}
		} else {
			if (myhdmi->fifo_err_conut != 0) {
				aud2_get_err(myhdmi);
				aud2_mute(myhdmi, FALSE);
				myhdmi->fifo_err_conut = 0;
			}
		}
	}
}

void
aud2_main_task(struct MTK_HDMI *myhdmi)
{
	struct AUDIO_CAPS CurAudioCaps;
	bool fgError = FALSE;
	bool fgChange = FALSE;

	if (myhdmi->aud_enable == 0)
		return;

	switch (myhdmi->aud_state) {
	case ASTATE_RequestAudio:
		aud2_setup(myhdmi);
		break;

	case ASTATE_WaitForReady:
		if (aud2_is_wait_aud_timeout(myhdmi, 50) == FALSE)
			break;

		aud2_get_input_audio(myhdmi, &CurAudioCaps);
		fgError = aud2_err_check(
			myhdmi, &myhdmi->aud_caps, &CurAudioCaps);

		if (myhdmi->acr_lock_timer == 0) {
			if (hdmi2com_is_acr_lock(myhdmi))
				RX_DEF_LOG("[RX]acr lock, but fifo err\n");
			else
				RX_DEF_LOG("[RX]acr unlock\n");
			aud2_set_state(myhdmi, ASTATE_RequestAudio);
		} else if (fgError == TRUE) {
			aud2_set_state(myhdmi, ASTATE_RequestAudio);
		} else {
			aud2_check_acr(myhdmi);
		}
		break;

	case ASTATE_AudioUnMute:
		if (myhdmi->audio_on_delay == 0)
			aud2_set_state(myhdmi, ASTATE_AudioOn);
		else
			myhdmi->audio_on_delay--;
		break;

	case ASTATE_AudioOn:
	aud2_get_input_audio(myhdmi, &CurAudioCaps);
	fgError = aud2_err_check(myhdmi, &myhdmi->aud_caps, &CurAudioCaps);

	if (fgError) {
		aud2_set_state(myhdmi, ASTATE_AudioOff);
		aud2_set_state(myhdmi, ASTATE_RequestAudio);
	} else {
		aud2_error_happen(myhdmi);

		if ((myhdmi->aud_caps.AudInf.info.SpeakerPlacement !=
			    CurAudioCaps.AudInf.info
				    .SpeakerPlacement) ||
			(myhdmi->aud_caps.AudInf.info.AudioCodingType !=
				CurAudioCaps.AudInf.info
					.AudioCodingType) ||
			(myhdmi->aud_caps.AudInf.info.SampleFreq !=
				CurAudioCaps.AudInf.info.SampleFreq)) {
			RX_AUDIO_LOG("[RX]InforFrame Change\n");
			RX_AUDIO_LOG("[RX]info.SpeakerPlacement =0x%x\n",
				CurAudioCaps.AudInf.info.SpeakerPlacement);
			RX_AUDIO_LOG("[RX]info.AudioCodingType =0x%x\n",
				CurAudioCaps.AudInf.info.AudioCodingType);
			RX_AUDIO_LOG("[RX]info.SampleFreq =0x%x\n",
				CurAudioCaps.AudInf.info.SampleFreq);
		}

		if (CurAudioCaps.AudChStat.IsLPCM !=
			myhdmi->aud_caps.AudChStat.IsLPCM) {
			RX_AUDIO_LOG(
				"[RX]ChStatBit1 0x%x(old) != 0x%x(new).\n",
				myhdmi->aud_caps.AudChStat.IsLPCM,
				CurAudioCaps.AudChStat.IsLPCM);
			fgChange = TRUE;
		}

		if ((CurAudioCaps.AudioFlag & B_HBRAUDIO) !=
			(myhdmi->aud_caps.AudioFlag & B_HBRAUDIO)) {
			RX_AUDIO_LOG(
				"[RX](AudioFlag & B_HBRAUDIO) chg\n");
			fgChange = TRUE;
		}
		if ((CurAudioCaps.AudioFlag & B_DSDAUDIO) !=
			(myhdmi->aud_caps.AudioFlag & B_DSDAUDIO)) {
			RX_AUDIO_LOG(
				"[RX](AudioFlag & B_DSDAUDIO) chg\n");
			fgChange = TRUE;
		}

		if (fgChange) {
			aud2_get_input_audio(myhdmi, &myhdmi->aud_caps);
			aud2_updata_audio_info(myhdmi);
			hdmi_rx_notify(myhdmi, HDMI_RX_AUD_INFO_CHANGE);
		}
	}
	break;

	default:
		break;
	}
}

void
aud2_get_input_ch_status(struct MTK_HDMI *myhdmi,
struct AUDIO_CAPS *pAudioCaps)
{
	if (!pAudioCaps)
		return;

	pAudioCaps->ChStat[0] = hdmi2com_get_aud_ch0(myhdmi);
	pAudioCaps->ChStat[1] = hdmi2com_get_aud_ch1(myhdmi);
	pAudioCaps->ChStat[2] = hdmi2com_get_aud_ch2(myhdmi);
	pAudioCaps->ChStat[3] = hdmi2com_get_aud_ch3(myhdmi);
	pAudioCaps->ChStat[4] = hdmi2com_get_aud_ch4(myhdmi);

	RX_DEF_LOG("[RX]ch[0]: 0x%x\n", pAudioCaps->ChStat[0]);
	RX_DEF_LOG("[RX]ch[1]: 0x%x\n", pAudioCaps->ChStat[1]);
	RX_DEF_LOG("[RX]ch[2]: 0x%x\n", pAudioCaps->ChStat[2]);
	RX_DEF_LOG("[RX]ch[3]: 0x%x\n", pAudioCaps->ChStat[3]);
	RX_DEF_LOG("[RX]ch[4]: 0x%x\n", pAudioCaps->ChStat[4]);
}

void
aud2_updata_audio_info(struct MTK_HDMI *myhdmi)
{
	RX_AUDIO_LOG("[RX]myhdmi->aud_caps.AudioFlag=0x%x\n",
		myhdmi->aud_caps.AudioFlag);
	RX_AUDIO_LOG("[RX]myhdmi->aud_caps.AudSrcEnable=0x%x\n",
		myhdmi->aud_caps.AudSrcEnable);
	RX_AUDIO_LOG("[RX]myhdmi->aud_caps.SampleFreq=0x%x\n",
		myhdmi->aud_caps.SampleFreq);

	if (myhdmi->aud_caps.AudioFlag & B_HBRAUDIO) {
		RX_AUDIO_LOG("[RX]B_CAP_HBR_AUDIO\n");
		myhdmi->aud_s.u1HBRAudio = 1;
	} else {
		RX_AUDIO_LOG("[RX]NON B_CAP_HBR_AUDIO\n");
		myhdmi->aud_s.u1HBRAudio = 0;
	}

	if (myhdmi->aud_caps.AudioFlag & B_DSDAUDIO) {
		RX_AUDIO_LOG("[RX]B_CAP_DSD_AUDIO\n");
		myhdmi->aud_s.u1DSDAudio = 1;
	} else {
		RX_AUDIO_LOG("[RX]NON B_CAP_DSD_AUDIO\n");
		myhdmi->aud_s.u1DSDAudio = 0;
	}

	if (myhdmi->aud_caps.AudioFlag & B_LAYOUT) {
		RX_AUDIO_LOG("[RX]B_LAYOUT = 1, Muti-Channel\n");
		myhdmi->aud_s.u1PCMMultiCh = 1;
	} else {
		RX_AUDIO_LOG("[RX]B_LAYOUT = 0, 2-Channel\n");
		myhdmi->aud_s.u1PCMMultiCh = 0;
	}

	if (myhdmi->aud_caps.AudioFlag & B_CAP_DSD_AUDIO) {
		RX_AUDIO_LOG("[RX]stream is DSD\n");
		myhdmi->aud_s.u1RawSDAudio = 0;
	} else {
		if (myhdmi->aud_caps.AudChStat.IsLPCM) {
			RX_AUDIO_LOG("[RX]stream is non-PCM\n");
			myhdmi->aud_s.u1RawSDAudio = 1;
		} else {
			RX_AUDIO_LOG("[RX]stream is PCM\n");
			myhdmi->aud_s.u1RawSDAudio = 0;
		}
	}

	myhdmi->aud_s.u1FsDec = myhdmi->aud_caps.SampleFreq;

	if (myhdmi->aud_caps.SampleFreq == 0)
		RX_AUDIO_LOG("[RX]FS is 44.1kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 2)
		RX_AUDIO_LOG("[RX]FS is 48kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 3)
		RX_AUDIO_LOG("[RX]FS is 32kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 8)
		RX_AUDIO_LOG("[RX]FS is 88.2kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 9)
		RX_AUDIO_LOG("[RX]FS is 768kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 0x0a)
		RX_AUDIO_LOG("[RX]FS is 96kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 0x0c)
		RX_AUDIO_LOG("[RX]FS is 176kHz\n");

	if (myhdmi->aud_caps.SampleFreq == 0x0E)
		RX_AUDIO_LOG("[RX]FS is 192kHz\n");

	myhdmi->aud_s.u1MCLKRatio = hdmi2com_get_aud_i2s_mclk(myhdmi);

	switch (myhdmi->aud_s.u1MCLKRatio) {
	case RX_MCLK_128FS:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 128FS\n");
		break;

	case RX_MCLK_256FS:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 256FS\n");
		break;

	case RX_MCLK_384FS:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 384FS\n");
		break;

	case RX_MCLK_512FS:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 512FS\n");
		break;

	case RX_MCLK_768FS:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 768FS\n");
		break;

	default:
		RX_AUDIO_LOG("[RX]Get Audio MCLK  is 128FS\n");
		break;
	}

	myhdmi->aud_s.u1I2sChanValid = myhdmi->aud_caps.AudSrcEnable;
	RX_AUDIO_LOG("[RX]I2S ch0%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x01) ? "" : " not");
	RX_AUDIO_LOG("[RX]I2S ch1%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x02) ? "" : " not");
	RX_AUDIO_LOG("[RX]I2S ch2%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x04) ? "" : " not");
	RX_AUDIO_LOG("[RX]I2S ch3%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x08) ? "" : " not");
}

void
aud2_mute(struct MTK_HDMI *myhdmi, bool bMute)
{
	if (bMute) {
		RX_AUDIO_LOG("[RX]Audio Mute\n");
		hdmi2_audio_mute(myhdmi, TRUE);
	} else {
		RX_AUDIO_LOG("[RX]Audio Un-Mute\n");
		hdmi2_audio_mute(myhdmi, FALSE);
	}
}

void
aud2_setup_path(struct MTK_HDMI *myhdmi)
{
	hdmi2com_audio_pll_config(myhdmi, TRUE);

	if (myhdmi->aud_caps.AudioFlag & B_CAP_HBR_AUDIO) {
		RX_AUDIO_LOG("[RX]aud2_setup, B_CAP_HBR_AUDIO\n");
		aud2_set_i2s_format(myhdmi, TRUE, HDMI_I2S_24BIT, FALSE);
		hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_128FS);
		hdmi2com_set_aud_valid_channel(myhdmi, 0xF0);
		hdmi2_audio_reset(myhdmi);
	} else if (myhdmi->aud_caps.AudioFlag & B_CAP_DSD_AUDIO) {
		RX_AUDIO_LOG("[RX]aud2_setup, B_CAP_DSD_AUDIO\n");
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_I2S_24BIT, FALSE);
		hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_512FS);
		hdmi2com_set_aud_valid_channel(myhdmi, 0xF0);
		hdmi2_audio_reset(myhdmi);
	} else {
		RX_AUDIO_LOG("[RX]aud2_setup, PCM or SPDIF\n");
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_I2S_24BIT, FALSE);
		hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_128FS);
		hdmi2com_set_aud_valid_channel(myhdmi,
			myhdmi->aud_caps.AudSrcEnable << 4);
		hdmi2_audio_reset(myhdmi);
	}
	aud2_set_state(myhdmi, ASTATE_WaitForReady);
}

/*
 *	   audio config step :\n
 *		step1 : enable audpll\n
 *		step2 : set hdmirx audio output\n
 *		step3 : reset acr\n
 *		step4 : set hdmitx or audioin input\n
 *		step5 : wait acr lock\n
 *		step6 : reset fifo and clear audio error when acr lock\n
 *	   note : before reset acr, audpll/Fs/Fm/NCTS are ready.
 */
void
aud2_setup(struct MTK_HDMI *myhdmi)
{
	aud2_get_input_audio(myhdmi, &myhdmi->aud_caps);

	if (myhdmi->aud_caps.AudioFlag & B_CAP_AUDIO_ON) {
		if (myhdmi->aud_caps.SampleFreq == B_Fs_UNKNOWN)
			RX_AUDIO_LOG("[RX]Audio Sampling Rate is Unknown.\n");
		else
			aud2_setup_path(myhdmi);
	}
}

void
aud2_enable(struct MTK_HDMI *myhdmi, bool en)
{
	if (en) {
		RX_AUDIO_LOG("[RX]aud2 Start\n");
		myhdmi->aud_enable = 1;
		aud2_set_state(myhdmi, ASTATE_RequestAudio);
	} else {
		RX_AUDIO_LOG("[RX]aud2 stop\n");
		aud2_set_state(myhdmi, ASTATE_AudioOff);
		myhdmi->aud_enable = 0;
	}
}

void
aud2_get_input_audio(struct MTK_HDMI *myhdmi,
struct AUDIO_CAPS *pAudioCaps)
{
	u32 temp;

	if (!pAudioCaps)
		return;

	pAudioCaps->SampleFreq = hdmi2com_aud_fs(myhdmi);
	pAudioCaps->AudioFlag =
		((hdmi2com_is_audio_packet(myhdmi) << B_CAP_AUDIO_ON_SFT) |
			(hdmi2com_is_hd_audio(myhdmi) << B_CAP_HBR_AUDIO_SFT) |
			(hdmi2com_is_dsd_audio(myhdmi) << B_CAP_DSD_AUDIO_SFT));

	if (pAudioCaps->AudioFlag & B_HBRAUDIO) {
		temp = hdmi2com_aud_fs_cal(myhdmi);
		if (temp == HDMI2_AUD_FS_44K)
			pAudioCaps->SampleFreq = B_Fs_176p4KHz;
		else if (temp == HDMI2_AUD_FS_88K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_3528K;
		else if (temp == HDMI2_AUD_FS_176K)
			pAudioCaps->SampleFreq = B_FS_7056KHz;
		else if (temp == HDMI2_AUD_FS_48K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_192K;
		else if (temp == HDMI2_AUD_FS_96K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_384K;
		else
			pAudioCaps->SampleFreq = B_Fs_HBR;
	}

	if (pAudioCaps->AudioFlag & B_DSDAUDIO)
		pAudioCaps->SampleFreq = B_Fs_44p1KHz;

	if (pAudioCaps->AudioFlag != 0)
		pAudioCaps->AudioFlag |= B_CAP_AUDIO_ON;

	pAudioCaps->AudioFlag |=
		(hdmi2com_is_multi_pcm_aud(myhdmi) << B_MULTICH_SFT);

	if ((pAudioCaps->AudioFlag & B_LAYOUT) == B_MULTICH)
		pAudioCaps->AudSrcEnable =
			hdmi2com_get_aud_valid_channel(myhdmi) & M_AUDIO_CH;
	else
		pAudioCaps->AudSrcEnable = B_AUDIO_SRC_VALID_0;

	hdmi2com_get_aud_infoframe(myhdmi, &(pAudioCaps->AudInf));
	aud2_get_ch_status(
		myhdmi, &(pAudioCaps->AudChStat), pAudioCaps->AudioFlag);
}

u8
aud2_get_acp_type(struct MTK_HDMI *myhdmi)
{
	return myhdmi->acp_type;
}

void
aud2_notify_acp_type_change(struct MTK_HDMI *myhdmi, u8 u1ACPType)
{
	if (u1ACPType == ACP_TYPE_GENERAL_AUDIO) {
		myhdmi->acp_type = ACP_TYPE_GENERAL_AUDIO;
		RX_AUDIO_LOG("[RX]ACP Type is General Audio.\n");
	} else if (u1ACPType == ACP_TYPE_IEC60958) {
		myhdmi->acp_type = ACP_TYPE_IEC60958;
		RX_AUDIO_LOG("[RX]ACP Type is IEC-89058.\n");
	} else if (u1ACPType == ACP_TYPE_DVD_AUDIO) {
		myhdmi->acp_type = ACP_TYPE_DVD_AUDIO;
		RX_AUDIO_LOG("[RX]ACP Type is DVD-Audio.\n");
	} else if (u1ACPType == ACP_TYPE_SACD) {
		myhdmi->acp_type = ACP_TYPE_SACD;
		RX_AUDIO_LOG("[RX]ACP Type is SACD.\n");
	} else {
		myhdmi->acp_type = ACP_TYPE_GENERAL_AUDIO;
		RX_AUDIO_LOG("[RX]ACP Type is Reserved.\n");
	}
}

void
aud2_set_state(struct MTK_HDMI *myhdmi, enum AUDIO_STATE state)
{
	if (myhdmi->aud_enable == 0)
		return;

	if ((((u8)myhdmi->aud_state) < ARRAY_SIZE(AStateStr)) &&
		(((u8)state) < ARRAY_SIZE(AStateStr)))
		RX_AUDIO_LOG("[RX]audio state from %s to %s\n",
			AStateStr[(u8)myhdmi->aud_state],
			AStateStr[(u8)state]);
	else
		RX_AUDIO_LOG("[RX]audio state unknown\n");

	myhdmi->aud_state = state;

	switch (myhdmi->aud_state) {
	case ASTATE_AudioOff:
		hdmi_rx_audio_notify(myhdmi, AUDIO_UNLOCK);
		aud2_mute(myhdmi, TRUE);
		myhdmi->acr_lock_timer = ACR_LOCK_TIMEOUT;
		myhdmi->acr_lock_count = 0;
		break;

	case ASTATE_WaitForReady:
		myhdmi->acr_lock_timer = ACR_LOCK_TIMEOUT;
		myhdmi->acr_lock_count = 0;
		aud2_set_wait_start_time(myhdmi);
		break;

	case ASTATE_AudioUnMute:
		aud2_mute(myhdmi, FALSE);
		break;

	case ASTATE_AudioOn:
		aud2_get_input_audio(myhdmi, &myhdmi->aud_caps);
		aud2_updata_audio_info(myhdmi);
		hdmi_rx_audio_notify(myhdmi, AUDIO_LOCK);
		break;

	default:
		break;
	}
}

u8
aud2_get_state(struct MTK_HDMI *myhdmi)
{
	return myhdmi->aud_state;
}

bool
aud2_is_lock(struct MTK_HDMI *myhdmi)
{
	if (myhdmi->aud_state == ASTATE_AudioOn)
		return TRUE;
	return FALSE;
}

void
aud2_show_audio_status(struct MTK_HDMI *myhdmi)
{
	RX_DEF_LOG("[RX]myhdmi->aud_state=%d\n", myhdmi->aud_state);
	RX_DEF_LOG("[RX]myhdmi->aud_enable=%d\n", myhdmi->aud_enable);

	RX_DEF_LOG("[RX]AudioFlag:\n");
	if (myhdmi->aud_caps.AudioFlag & B_CAP_AUDIO_ON)
		RX_DEF_LOG("[RX]AudioFlag+B_CAP_AUDIO_ON\n");
	if (myhdmi->aud_caps.AudioFlag & B_CAP_HBR_AUDIO)
		RX_DEF_LOG("[RX]AudioFlag+B_CAP_HBR_AUDIO\n");
	if (myhdmi->aud_caps.AudioFlag & B_CAP_DSD_AUDIO)
		RX_DEF_LOG("[RX]AudioFlag+B_CAP_DSD_AUDIO\n");
	if (myhdmi->aud_caps.AudioFlag & B_LAYOUT)
		RX_DEF_LOG("[RX]AudioFlag+B_LAYOUT\n");
	if (myhdmi->aud_caps.AudioFlag & B_MULTICH)
		RX_DEF_LOG("[RX]AudioFlag+B_MULTICH\n");
	if (myhdmi->aud_caps.AudioFlag & B_HBR_BY_SPDIF)
		RX_DEF_LOG("[RX]AudioFlag+B_HBR_BY_SPDIF\n");

	RX_DEF_LOG("[RX]I2S ch0%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x01) ? "" : " not");
	RX_DEF_LOG("[RX]I2S ch1%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x02) ? "" : " not");
	RX_DEF_LOG("[RX]I2S ch2%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x04) ? "" : " not");
	RX_DEF_LOG("[RX]I2S ch3%s Valid\n",
		(myhdmi->aud_caps.AudSrcEnable & 0x08) ? "" : " not");

	if (myhdmi->aud_caps.SampleFreq == B_Fs_UNKNOWN)
		RX_DEF_LOG("[RX]Fs is B_Fs_UNKNOWN\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_44p1KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_44p1KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_48KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_48KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_32KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_32KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_88p2KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_88p2KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_96KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_96KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_176p4KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_176p4KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_768KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_768KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_HBR)
		RX_DEF_LOG("[RX]Fs is B_Fs_HBR\n");
	else if (myhdmi->aud_caps.SampleFreq == B_FS_7056KHz)
		RX_DEF_LOG("[RX]Fs is B_FS_7056KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_Fs_192KHz)
		RX_DEF_LOG("[RX]Fs is B_Fs_192KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_FS_3528KHz)
		RX_DEF_LOG("[RX]Fs is B_FS_3528KHz\n");
	else if (myhdmi->aud_caps.SampleFreq == B_fs_384KHz)
		RX_DEF_LOG("[RX]Fs is B_fs_384KHz\n");

	aud2_get_input_ch_status(myhdmi, &myhdmi->aud_caps);
	RX_DEF_LOG("[RX]audio channel status: %x %x %x %x %x\n",
		myhdmi->aud_caps.ChStat[0], myhdmi->aud_caps.ChStat[1],
		myhdmi->aud_caps.ChStat[2], myhdmi->aud_caps.ChStat[3],
		myhdmi->aud_caps.ChStat[4]);

	RX_DEF_LOG("[RX]AudCh.ISLPCM=%x\n",
		myhdmi->aud_caps.AudChStat.IsLPCM);
	RX_DEF_LOG("[RX]AudCh.CopyRight=%x\n",
		myhdmi->aud_caps.AudChStat.CopyRight);
	RX_DEF_LOG("[RX]AudCh.AdditionFormatInfo=%x\n",
		myhdmi->aud_caps.AudChStat.AdditionFormatInfo);
	RX_DEF_LOG("[RX]AudCh.ChannelStatusMode=%x\n",
		myhdmi->aud_caps.AudChStat.ChannelStatusMode);
	RX_DEF_LOG("[RX]AudCh.CategoryCode=%x\n",
		myhdmi->aud_caps.AudChStat.CategoryCode);
	RX_DEF_LOG("[RX]AudCh.SourceNumber=%x\n",
		myhdmi->aud_caps.AudChStat.SourceNumber);
	RX_DEF_LOG("[RX]AudCh.ChannelNumber=%x\n",
		myhdmi->aud_caps.AudChStat.ChannelNumber);
	RX_DEF_LOG("[RX]AudCh.SamplingFreq=%x\n",
		myhdmi->aud_caps.AudChStat.SamplingFreq);
	RX_DEF_LOG("[RX]AudCh.ClockAccuary=%x\n",
		myhdmi->aud_caps.AudChStat.ClockAccuary);
	RX_DEF_LOG("[RX]AudCh.WordLen=%x\n",
		myhdmi->aud_caps.AudChStat.WordLen);
	RX_DEF_LOG("[RX]AudCh.OriginalSamplingFreq=%x\n",
		myhdmi->aud_caps.AudChStat.OriginalSamplingFreq);

	RX_DEF_LOG("[RX]AudInf.info.SpeakerPlacement=%x\n",
		myhdmi->aud_caps.AudInf.info.SpeakerPlacement);
}

void
aud2_show_status(struct MTK_HDMI *myhdmi)
{
	hdmi2com_show_audio_status(myhdmi);
	memset(&myhdmi->aud_caps, 0, sizeof(struct AUDIO_CAPS));
	aud2_get_input_audio(myhdmi, &myhdmi->aud_caps);
	aud2_get_input_ch_status(myhdmi, &myhdmi->aud_caps);
	aud2_show_audio_status(myhdmi);
}

void
aud2_audio_out(struct MTK_HDMI *myhdmi, u32 aud_type,
	u32 aud_fmt, u32 aud_mclk, u32 aud_ch)
{
	RX_DEF_LOG("[RX]aud_type=%x,aud_fmt=%x,aud_fs=%x,aud_ch=%x\n",
		aud_type, aud_fmt, aud_mclk, aud_ch);

	if (aud_type == 0) {
		RX_DEF_LOG("[RX]audio type is i2s/spdif\n");
		if (aud_fmt == 0)
			aud2_set_i2s_format(myhdmi, FALSE, HDMI_LJT_24BIT, 1);
		else if (aud_fmt == 1)
			aud2_set_i2s_format(myhdmi, FALSE, HDMI_RJT_24BIT, 1);
		else if (aud_fmt == 2)
			aud2_set_i2s_format(myhdmi, FALSE, HDMI_I2S_24BIT, 1);

		if (aud_mclk == 0)
			hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_128FS);
		else if (aud_mclk == 1)
			hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_256FS);
		else if (aud_mclk == 2)
			hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_384FS);
		else if (aud_mclk == 3)
			hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_512FS);

		hdmi2com_set_aud_valid_channel(myhdmi, aud_ch << 4);
	} else if (aud_type == 1) {
		RX_DEF_LOG("[RX]audio type is hbr\n");
		aud2_set_i2s_format(myhdmi, TRUE, HDMI_LJT_24BIT, 1);
		hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_128FS);
		hdmi2com_set_aud_valid_channel(myhdmi, 0xF0);
	} else if (aud_type == 2) {
		RX_DEF_LOG("[RX]audio type is dsd\n");
		aud2_set_i2s_format(myhdmi, FALSE, HDMI_LJT_24BIT, 1);
		hdmi2com_set_aud_i2s_mclk(myhdmi, MCLK_512FS);
		hdmi2com_set_aud_valid_channel(myhdmi, 0xF0);
	}
}

void
aud2_audio_mute(struct MTK_HDMI *myhdmi, bool mute)
{
	if (myhdmi->aud_caps.AudioFlag & B_CAP_DSD_AUDIO)
		hdmi2com_aud_mute_mode(myhdmi, TRUE);
	else
		hdmi2com_aud_mute_mode(myhdmi, FALSE);

	hdmi2_audio_mute(myhdmi, mute);
}

static void io_aud2_get_input_audio(struct MTK_HDMI *myhdmi,
struct IO_AUDIO_CAPS *pAudioCaps)
{
	u32 temp;
	u8 AudioFlag;

	if (!pAudioCaps)
		return;

	pAudioCaps->SampleFreq = hdmi2com_aud_fs(myhdmi);
	AudioFlag =
		((hdmi2com_is_audio_packet(myhdmi) << B_CAP_AUDIO_ON_SFT) |
		(hdmi2com_is_hd_audio(myhdmi) << B_CAP_HBR_AUDIO_SFT) |
		(hdmi2com_is_dsd_audio(myhdmi) << B_CAP_DSD_AUDIO_SFT));

	if (AudioFlag & B_HBRAUDIO) {
		temp = hdmi2com_aud_fs_cal(myhdmi);
		if (temp == HDMI2_AUD_FS_44K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_176K;
		else if (temp == HDMI2_AUD_FS_88K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_3528K;
		else if (temp == HDMI2_AUD_FS_176K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_7056K;
		else if (temp == HDMI2_AUD_FS_48K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_192K;
		else if (temp == HDMI2_AUD_FS_96K)
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_384K;
		else
			pAudioCaps->SampleFreq = HDMI2_AUD_FS_768K;
	}

	if (AudioFlag & B_DSDAUDIO)
		pAudioCaps->SampleFreq = HDMI2_AUD_FS_44K;

	if (AudioFlag != 0)
		AudioFlag |= B_CAP_AUDIO_ON;

	AudioFlag |=	(hdmi2com_is_multi_pcm_aud(myhdmi) << B_MULTICH_SFT);

	pAudioCaps->CHStatusData[0] = hdmi2com_get_aud_ch0(myhdmi);
	pAudioCaps->CHStatusData[1] = hdmi2com_get_aud_ch1(myhdmi);
	pAudioCaps->CHStatusData[2] = hdmi2com_get_aud_ch2(myhdmi);
	pAudioCaps->CHStatusData[3] = hdmi2com_get_aud_ch3(myhdmi);
	pAudioCaps->CHStatusData[4] = hdmi2com_get_aud_ch4(myhdmi);

	hdmi2com_get_aud_infoframe(myhdmi, &(pAudioCaps->AudInf));
	aud2_get_ch_status(myhdmi, &(pAudioCaps->AudChStat), AudioFlag);

	if (myhdmi->my_debug == 24) {
		RX_DEF_LOG("[RX]SampleFreq:%d\n", pAudioCaps->SampleFreq);
		RX_DEF_LOG("[RX]CHStatusData[0]:%d\n", pAudioCaps->CHStatusData[0]);
		RX_DEF_LOG("[RX]CHStatusData[1]:%d\n", pAudioCaps->CHStatusData[1]);
		RX_DEF_LOG("[RX]CHStatusData[2]:%d\n", pAudioCaps->CHStatusData[2]);
		RX_DEF_LOG("[RX]CHStatusData[3]:%d\n", pAudioCaps->CHStatusData[3]);
		RX_DEF_LOG("[RX]CHStatusData[4]:%d\n", pAudioCaps->CHStatusData[4]);
		RX_DEF_LOG("[RX]IsLPCM:%d\n",
			pAudioCaps->AudChStat.IsLPCM);
		RX_DEF_LOG("[RX]ChannelNumber:%d\n",
			pAudioCaps->AudChStat.ChannelNumber);
		RX_DEF_LOG("[RX]SamplingFreq:%d\n",
			pAudioCaps->AudChStat.SamplingFreq);
		RX_DEF_LOG("[RX]WordLen:%d\n",
			pAudioCaps->AudChStat.WordLen);
		RX_DEF_LOG("[RX]AudioChannelCount:%d\n",
			pAudioCaps->AudInf.info.AudioChannelCount);
		RX_DEF_LOG("[RX]SpeakerPlacement:%d\n",
			pAudioCaps->AudInf.info.SpeakerPlacement);
	}
}

static void io_aud2_get_audio_info(struct MTK_HDMI *myhdmi,
	struct IO_AUDIO_INFO *prAudIf)
{
	prAudIf->is_HBRAudio = FALSE;
	prAudIf->is_DSDAudio = FALSE;
	prAudIf->is_RawSDAudio = FALSE;
	prAudIf->is_PCMMultiCh = FALSE;
	if (myhdmi->aud_s.u1HBRAudio != 0)
		prAudIf->is_HBRAudio = TRUE;
	else if (myhdmi->aud_s.u1DSDAudio != 0)
		prAudIf->is_DSDAudio = TRUE;
	else if (myhdmi->aud_s.u1RawSDAudio != 0)
		prAudIf->is_RawSDAudio = TRUE;
	else
		prAudIf->is_PCMMultiCh = TRUE;
}

void io_get_aud_info(struct MTK_HDMI *myhdmi,
	struct HDMIRX_AUD_INFO *aud_info)
{
	io_aud2_get_input_audio(myhdmi, &aud_info->caps);
	io_aud2_get_audio_info(myhdmi, &aud_info->info);
}
