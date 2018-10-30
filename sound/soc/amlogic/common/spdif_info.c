/*
 * sound/soc/amlogic/common/spdif_info.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#undef pr_fmt
#define pr_fmt(fmt) "spdif_info: " fmt

#include <linux/amlogic/media/sound/aout_notify.h>
#include <linux/amlogic/media/sound/spdif_info.h>

/*
 * 0 --  other formats except(DD,DD+,DTS)
 * 1 --  DTS
 * 2 --  DD
 * 3 -- DTS with 958 PCM RAW package mode
 * 4 -- DD+
 */
unsigned int IEC958_mode_codec;
EXPORT_SYMBOL(IEC958_mode_codec);

bool spdif_is_4x_clk(void)
{
	bool is_4x = false;

	if (IEC958_mode_codec == 4 || IEC958_mode_codec == 5 ||
		IEC958_mode_codec == 7 || IEC958_mode_codec == 8) {
		is_4x = true;
	}

	return is_4x;
}

void spdif_get_channel_status_info(
	struct iec958_chsts *chsts,
	unsigned int rate)
{
	int rate_bit = snd_pcm_rate_to_rate_bit(rate);

	if (rate_bit == SNDRV_PCM_RATE_KNOT) {
		pr_err("Unsupport sample rate\n");
		return;
	}

	if (IEC958_mode_codec && IEC958_mode_codec != 9) {
		if (IEC958_mode_codec == 1) {
			/* dts, use raw sync-word mode */
			pr_info("iec958 mode RAW\n");
		} else {
			/* ac3,use the same pcm mode as i2s */
		}

		chsts->chstat0_l = 0x1902;
		chsts->chstat0_r = 0x1902;
		if (IEC958_mode_codec == 4 || IEC958_mode_codec == 5) {
			/* DD+ */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = 0x300;
				chsts->chstat1_r = 0x300;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = 0xc00;
				chsts->chstat1_r = 0xc00;
			} else {
				chsts->chstat1_l = 0Xe00;
				chsts->chstat1_r = 0Xe00;
			}
		} else {
			/* DTS,DD */
			if (rate_bit == SNDRV_PCM_RATE_32000) {
				chsts->chstat1_l = 0x300;
				chsts->chstat1_r = 0x300;
			} else if (rate_bit == SNDRV_PCM_RATE_44100) {
				chsts->chstat1_l = 0;
				chsts->chstat1_r = 0;
			} else {
				chsts->chstat1_l = 0x200;
				chsts->chstat1_r = 0x200;
			}
		}
	} else {
		chsts->chstat0_l = 0x0100;
		chsts->chstat0_r = 0x0100;
		chsts->chstat1_l = 0x200;
		chsts->chstat1_r = 0x200;

		if (rate_bit == SNDRV_PCM_RATE_44100) {
			chsts->chstat1_l = 0;
			chsts->chstat1_r = 0;
		} else if (rate_bit == SNDRV_PCM_RATE_88200) {
			chsts->chstat1_l = 0x800;
			chsts->chstat1_r = 0x800;
		} else if (rate_bit == SNDRV_PCM_RATE_96000) {
			chsts->chstat1_l = 0xa00;
			chsts->chstat1_r = 0xa00;
		} else if (rate_bit == SNDRV_PCM_RATE_176400) {
			chsts->chstat1_l = 0xc00;
			chsts->chstat1_r = 0xc00;
		} else if (rate_bit == SNDRV_PCM_RATE_192000) {
			chsts->chstat1_l = 0xe00;
			chsts->chstat1_r = 0xe00;
		}
	}
	pr_info("rate: %d, channel status ch0_l:0x%x, ch0_r:0x%x, ch1_l:0x%x, ch1_r:0x%x\n",
		rate,
		chsts->chstat0_l,
		chsts->chstat0_r,
		chsts->chstat1_l,
		chsts->chstat1_r);
}


void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream)
{
	if (IEC958_mode_codec == 2) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_AC_3,
			substream);
	} else if (IEC958_mode_codec == 3) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS,
			substream);
	} else if (IEC958_mode_codec == 4) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS,
			substream);
	} else if (IEC958_mode_codec == 5) {
		aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS_HD,
			substream);
	} else if (IEC958_mode_codec == 7 ||
				IEC958_mode_codec == 8) {
		//aml_aiu_write(AIU_958_CHSTAT_L0, 0x1902);
		//aml_aiu_write(AIU_958_CHSTAT_L1, 0x900);
		//aml_aiu_write(AIU_958_CHSTAT_R0, 0x1902);
		//aml_aiu_write(AIU_958_CHSTAT_R1, 0x900);
		if (IEC958_mode_codec == 8)
			aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_DTS_HD_MA,
			substream);
		else
			aout_notifier_call_chain(
			AOUT_EVENT_RAWDATA_MAT_MLP,
			substream);
	} else {
			aout_notifier_call_chain(
				AOUT_EVENT_IEC_60958_PCM,
				substream);
	}
}

static const char *const spdif_format_texts[10] = {
	"2 CH PCM", "DTS RAW Mode", "Dolby Digital", "DTS",
	"DD+", "DTS-HD", "Multi-channel LPCM", "TrueHD", "DTS-HD MA",
	"HIGH SR Stereo LPCM"
};

const struct soc_enum spdif_format_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_format_texts),
			spdif_format_texts);

int spdif_format_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = IEC958_mode_codec;
	return 0;
}

int spdif_format_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.enumerated.item[0];

	if (index >= 10) {
		pr_err("bad parameter for spdif format set\n");
		return -1;
	}
	IEC958_mode_codec = index;
	return 0;
}
