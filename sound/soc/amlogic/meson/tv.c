/*
 * sound/soc/amlogic/meson/tv.c
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
#undef pr_fmt
#define pr_fmt(fmt) "snd_card_tv: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <asm/unaligned.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>
#ifdef CONFIG_AMLOGIC_AO_CEC
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_cec_20.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif

#include <linux/amlogic/media/sound/misc.h>

#include "i2s.h"
#include "audio_hw.h"
#include "tv.h"

#define DRV_NAME "aml_snd_card_tv"

#define AML_EQ_PARAM_LENGTH 100
#define AML_DRC_PARAM_LENGTH 12
#define AML_REG_BYTES 4

static u32 aml_EQ_table[AML_EQ_PARAM_LENGTH] = {
	/*channel 1 param*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef0*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef1*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef2*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef3*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef4*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef5*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef6*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef7*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef8*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef9*/
	/*channel 2 param*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef0*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef1*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef2*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef3*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef4*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef5*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef6*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef7*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef8*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef9*/
};

static u32 aml_DRC_table[AML_DRC_PARAM_LENGTH] = {
	0x0000111c, 0x00081bfc, 0x00001571,  /*drc_ae, drc_aa, drc_ad*/
	0x0380111c, 0x03881bfc, 0x03801571,  /*drc_ae_1m, drc_aa_1m, drc_ad_1m*/
	0x0,		0x0,	                 /*offset0, offset1*/
	0xcb000000, 0x0,	                 /*thd0, thd1*/
	0xa0000,	0x40000,                 /*k0, k1*/
};

static u32 aml_hw_resample_table[7][5] = {
	/*coef of 32K, fc = 9000, Q:0.55, G= 14.00, */
	{0x0137fd9a, 0x033fe4a2, 0x0029da1f, 0x001a66fb, 0x00075562},
	/*coef of 44.1K, fc = 14700, Q:0.55, G= 14.00, */
	{0x0106f9aa, 0x00b84366, 0x03cdcb2d, 0x00b84366, 0x0054c4d7},
	/*coef of 48K, fc = 15000, Q:0.60, G= 11.00, */
	{0x00ea25ae, 0x00afe01d, 0x03e0efb0, 0x00afe01d, 0x004b155e},
	/*coef of 88.2K, fc = 26000, Q:0.60, G= 4.00, */
	{0x009dc098, 0x000972c7, 0x000e7582, 0x00277b49, 0x000e2d97},
	/*coef of 96K, fc = 36000, Q:0.50, G= 4.00, */
	{0x0098178d, 0x008b0d0d, 0x00087862, 0x008b0d0d, 0x00208fef},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
};

/*IEC60958-1: 24~27bit defined sample rate*/
static int get_spdif_sample_rate_index(void)
{
	int index = 0;
	unsigned char value = aml_audin_read(AUDIN_SPDIF_CHNL_STS_A) >> 24;

	value = value & 0xf;

	switch (value) {
	case 0x4:/*22050*/
		index = 1;
		break;
	case 0x6:/*24000*/
		index = 2;
		break;
	case 0x3:/*32000*/
		index = 3;
		break;
	case 0x0:/*44100*/
		index = 4;
		break;
	case 0x2:/*48000*/
		index = 5;
		break;
	case 0x8:/*88200*/
		index = 6;
		break;
	case 0xa:/*96000*/
		index = 7;
		break;
	case 0xc:/*176400*/
		index = 8;
		break;
	case 0xe:/*192000*/
		index = 9;
		break;
	case 0x9:/*768000*/
		index = 10;
		break;
	default:
		pr_err("not indicated samplerate: %x\n", index);
		index = 0;
		break;
	}
	return index;
}

static const char *const audio_in_source_texts[] = {
	"LINEIN", "ATV", "HDMI", "SPDIFIN" };

static const struct soc_enum audio_in_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_in_source_texts),
			audio_in_source_texts);

static int aml_audio_get_in_source(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int value = audio_in_source;

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int aml_audio_set_in_source(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio =
			snd_soc_card_get_drvdata(card);
	struct aml_card_info *p_cardinfo = NULL;

	if (!p_aml_audio)
		return -EINVAL;
	p_cardinfo = p_aml_audio->cardinfo;

	if (p_cardinfo)
		p_cardinfo->set_audin_source(
			ucontrol->value.enumerated.item[0]);
	else
		pr_warn_once("check chipset supports to set audio in source\n");

	audio_in_source = ucontrol->value.enumerated.item[0];
	return 0;
}

static int aml_audio_get_spdifin_pao(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = bSpdifIN_PAO;
	return 0;
}


static int aml_audio_set_spdifin_pao(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	bSpdifIN_PAO = ucontrol->value.integer.value[0];
	return 0;
}
/* i2s audio format detect: LPCM or NONE-LPCM */
static const char *const i2s_audio_type_texts[] = {
	"LPCM", "NONE-LPCM", "UN-KNOWN"
};

static const struct soc_enum i2s_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(i2s_audio_type_texts),
			i2s_audio_type_texts);

static int aml_get_i2s_audio_type(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ch_status = 0;

	if ((aml_audin_read(AUDIN_DECODE_CONTROL_STATUS) >> 24) & 0x1) {
		ch_status = aml_audin_read(AUDIN_DECODE_CHANNEL_STATUS_A_0);
		if (ch_status & 2)
			ucontrol->value.enumerated.item[0] = 1;
		else
			ucontrol->value.enumerated.item[0] = 0;
	} else {
		ucontrol->value.enumerated.item[0] = 2;
	}
	return 0;
}

/* spdif in audio format detect: LPCM or NONE-LPCM */
struct spdif_audio_info {
	unsigned char aud_type;
	/*IEC61937 package presamble Pc value*/
	short pc;
	char *aud_type_str;
};
static const char *const spdif_audio_type_texts[] = {
	"LPCM",
	"AC3",
	"EAC3",
	"DTS",
	"DTS-HD",
	"TRUEHD",
	"PAUSE"
};
static const struct spdif_audio_info type_texts[] = {
	{0, 0, "LPCM"},
	{1, 0x1, "AC3"},
	{2, 0x15, "EAC3"},
	{3, 0xb, "DTS-I"},
	{3, 0x0c, "DTS-II"},
	{3, 0x0d, "DTS-III"},
	{3, 0x11, "DTS-IV"},
	{4, 0, "DTS-HD"},
	{5, 0x16, "TRUEHD"},
	{6, 0x103, "PAUSE"},
	{6, 0x003, "PAUSE"},
	{6, 0x100, "PAUSE"},
};
static const struct soc_enum spdif_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_audio_type_texts),
			spdif_audio_type_texts);

static int aml_get_spdif_audio_type(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio =
			snd_soc_card_get_drvdata(card);
	struct aml_card_info *p_cardinfo;
	int audio_type = 0;
	int i;
	int total_num = sizeof(type_texts)/sizeof(struct spdif_audio_info);
	int pc = aml_audin_read(AUDIN_SPDIF_NPCM_PCPD)>>16;

	pc = pc&0xfff;
	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			break;
		}
	}

	if (p_aml_audio)
		p_cardinfo = p_aml_audio->cardinfo;

	/* HDMI in,also check the hdmirx  fifo status*/
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	if (audio_in_source == 2) {
		int index = 0;

		update_spdifin_audio_type(audio_type);

		index = get_hdmi_sample_rate_index();
		if (p_aml_audio &&
			p_aml_audio->hdmi_sample_rate_index != index) {
			p_aml_audio->hdmi_sample_rate_index = index;
			p_aml_audio->spdif_sample_rate_index = 0;
			if (p_cardinfo && index > 0 && index <= 7 &&
				p_aml_audio->Hardware_resample_enable == 1) {
				p_cardinfo->set_resample_param(index - 1);
				pr_info("hdmi: set hw resample parmater table: %d\n",
					index - 1);
			}
		}
	}
#endif
	/*spdif in source*/
	if (audio_in_source == 3) {
		int index = get_spdif_sample_rate_index();

		if (p_aml_audio &&
			p_aml_audio->spdif_sample_rate_index != index) {
			p_aml_audio->spdif_sample_rate_index = index;
			p_aml_audio->hdmi_sample_rate_index = 0;
			if (p_cardinfo && index >= 3 && index <= 9 &&
				p_aml_audio->Hardware_resample_enable == 1) {
				p_cardinfo->set_resample_param(index - 3);
				pr_info("spdif: set hw resample parmater table: %d\n",
					index - 3);
			}
		}
	}
	ucontrol->value.enumerated.item[0] = audio_type;

	return 0;
}

static const char *const spdifin_in_samplerate[] = {
	"N/A",
	"22050",
	"24000",
	"32000",
	"44100",
	"48000",
	"88200",
	"96000",
	"176400",
	"192000",
	"768000",
};

static const struct soc_enum spdif_audio_samplerate_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdifin_in_samplerate),
			spdifin_in_samplerate);

static int aml_get_spdif_audio_samplerate(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = get_spdif_sample_rate_index();

	ucontrol->value.integer.value[0] = index;
	return 0;
}

/*Cnt_ctrl = mclk/fs_out-1 ; fest 256fs */
#define RESAMPLE_CNT_CONTROL 255

static int hardware_resample_enable(int input_sr)
{
	u16 Avg_cnt_init = 0;
	unsigned int clk_rate = clk81;

	Avg_cnt_init = (u16)(clk_rate * 4 / input_sr);
	pr_info("clk_rate = %u, input_sr = %d, Avg_cnt_init = %u\n",
		clk_rate, input_sr, Avg_cnt_init);

	aml_audin_write(AUD_RESAMPLE_CTRL0, (1 << 31));
	aml_audin_write(AUD_RESAMPLE_CTRL0, 0);
	aml_audin_write(AUD_RESAMPLE_CTRL0,
				(1 << 29)
				| (1 << 28)
				| (0 << 26)
				| (RESAMPLE_CNT_CONTROL << 16)
				| Avg_cnt_init);

	return 0;
}

static int hardware_resample_disable(void)
{
	aml_audin_write(AUD_RESAMPLE_CTRL0, 0);
	return 0;
}

static int set_hw_resample_param(int index)
{
	int i;

	for (i = 0; i < 5; i++) {
		aml_audin_write((AUD_RESAMPLE_COEF0 + i),
			aml_hw_resample_table[index][i]);
	}

	aml_audin_update_bits(AUD_RESAMPLE_CTRL2,
			1 << 25, 1 << 25);

	return 0;
}

static const char *const hardware_resample_texts[] = {
	"Disable",
	"Enable",
};

static const struct soc_enum hardware_resample_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hardware_resample_texts),
			hardware_resample_texts);

static int aml_get_hardware_resample(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio =
			snd_soc_card_get_drvdata(card);
	ucontrol->value.enumerated.item[0] =
			p_aml_audio->Hardware_resample_enable;
	return 0;
}

static int aml_set_hardware_resample(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio =
			snd_soc_card_get_drvdata(card);
	int index = ucontrol->value.enumerated.item[0];

	if (index == 0) {
		hardware_resample_disable();
		p_aml_audio->Hardware_resample_enable = 0;
	} else {
		hardware_resample_enable(48000);
		p_aml_audio->Hardware_resample_enable = 1;
	}
	return 0;
}

static const struct snd_soc_dapm_widget aml_asoc_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("LINEIN"),
	SND_SOC_DAPM_OUTPUT("LINEOUT"),
};

static const char * const audio_in_switch_texts[] = { "SPDIF_IN", "ARC_IN"};
static const struct soc_enum audio_in_switch_enum = SOC_ENUM_SINGLE(
		SND_SOC_NOPM, 0, ARRAY_SIZE(audio_in_switch_texts),
		audio_in_switch_texts);

static int aml_get_audio_in_switch(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);
	ucontrol->value.integer.value[0] = p_aml_audio->audio_in_GPIO;
	return 0;
}

static int aml_set_audio_in_switch(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	if (ucontrol->value.enumerated.item[0] == 0) {
		if (p_aml_audio->audio_in_GPIO_inv == 0) {
			gpiod_direction_output(p_aml_audio->source_switch,
					   GPIOF_OUT_INIT_HIGH);
		} else {
			gpiod_direction_output(p_aml_audio->source_switch,
					   GPIOF_OUT_INIT_LOW);
		}
		p_aml_audio->audio_in_GPIO = 0;
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		if (p_aml_audio->audio_in_GPIO_inv == 0) {
			gpiod_direction_output(p_aml_audio->source_switch,
					   GPIOF_OUT_INIT_LOW);
		} else {
			gpiod_direction_output(p_aml_audio->source_switch,
					   GPIOF_OUT_INIT_HIGH);
		}
		p_aml_audio->audio_in_GPIO = 1;
	}
	return 0;
}

static int init_EQ_DRC_module(void)
{
	aml_eqdrc_write(AED_TOP_CTL, (1 << 31)); /* fifo init */
	aml_eqdrc_write(AED_ED_CTL, 1); /* soft reset*/
	msleep(20);
	aml_eqdrc_write(AED_ED_CTL, 0); /* soft reset*/
	aml_eqdrc_write(AED_TOP_CTL, (0 << 1) /*i2s in sel*/
						| (1 << 0)); /*module enable*/
	aml_eqdrc_write(AED_NG_CTL, (3 << 30)); /* disable noise gate*/
	return 0;
}

static int set_internal_EQ_volume(
	unsigned int master_volume,
	unsigned int channel_1_volume,
	unsigned int channel_2_volume)
{
	aml_eqdrc_write(AED_EQ_VOLUME, (0 << 30) /* volume step: 0.125dB*/
			| (master_volume << 16) /* master volume: 0dB*/
			| (channel_1_volume << 8) /* channel 1 volume: 0dB*/
			| (channel_2_volume << 0) /* channel 2 volume: 0dB*/
			);
	/*fast attrack*/
	aml_eqdrc_write(AED_EQ_VOLUME_SLEW_CNT, 0x5);
	aml_eqdrc_write(AED_MUTE, 0);
	return 0;
}

#ifdef CONFIG_AMLOGIC_AO_CEC
/* call HDMI CEC API to enable arc audio */
static void aml_switch_arc_audio(bool enable)
{
	cec_enable_arc_pin(enable);
}
static int aml_get_arc_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);
	ucontrol->value.integer.value[0] = p_aml_audio->arc_enable;
	return 0;
}
static int aml_set_arc_audio(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_audio_private_data *p_aml_audio;
	bool enable = ucontrol->value.integer.value[0];

	p_aml_audio = snd_soc_card_get_drvdata(card);
	aml_switch_arc_audio(enable);
	p_aml_audio->arc_enable = enable;
	return 0;
}
#endif

#ifdef CONFIG_TVIN_VDIN
static const char *const av_audio_is_stable[] = {
	"false",
	"true"
};
static const struct soc_enum av_audio_status_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(av_audio_is_stable),
			av_audio_is_stable);
static int aml_get_av_audio_stable(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tvin_get_av_status();
	return 0;
}
#endif /* CONFIG_TVIN_VDIN */

static const struct snd_kcontrol_new av_controls[] = {
	SOC_ENUM_EXT("AudioIn Switch",
			 audio_in_switch_enum,
			 aml_get_audio_in_switch,
			 aml_set_audio_in_switch),
};

static const struct snd_kcontrol_new aml_tv_controls[] = {
	SOC_SINGLE_BOOL_EXT("SPDIFIN PAO",
		     0,
		     aml_audio_get_spdifin_pao,
		     aml_audio_set_spdifin_pao),
	SOC_ENUM_EXT("Audio In Source",
		     audio_in_source_enum,
		     aml_audio_get_in_source,
		     aml_audio_set_in_source),

	SOC_ENUM_EXT("I2SIN Audio Type",
		     i2s_audio_type_enum,
		     aml_get_i2s_audio_type,
		     NULL),

	SOC_ENUM_EXT("SPDIFIN Audio Type",
		     spdif_audio_type_enum,
		     aml_get_spdif_audio_type,
		     NULL),

	SOC_ENUM_EXT("SPDIFIN audio samplerate",
		     spdif_audio_samplerate_enum,
		     aml_get_spdif_audio_samplerate,
		     NULL),

	SOC_ENUM_EXT("Hardware resample enable",
		     hardware_resample_enum,
		     aml_get_hardware_resample,
		     aml_set_hardware_resample),
#ifdef CONFIG_AMLOGIC_AO_CEC
	SOC_SINGLE_BOOL_EXT("HDMI ARC Switch", 0,
				aml_get_arc_audio,
				aml_set_arc_audio),
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	SOC_ENUM_EXT("HDMIIN audio stable", hdmi_in_status_enum[0],
				aml_get_hdmiin_audio_stable,
				NULL),
	SOC_ENUM_EXT("HDMIIN audio samplerate", hdmi_in_status_enum[1],
				aml_get_hdmiin_audio_samplerate,
				NULL),
	SOC_ENUM_EXT("HDMIIN audio channels", hdmi_in_status_enum[2],
				aml_get_hdmiin_audio_channels,
				NULL),
	SOC_ENUM_EXT("HDMIIN audio format", hdmi_in_status_enum[3],
				aml_get_hdmiin_audio_format,
				NULL),
	SOC_SINGLE_BOOL_EXT("HDMI ATMOS EDID Switch", 0,
				aml_get_atmos_audio_edid,
				aml_set_atmos_audio_edid),
#endif
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
	SOC_ENUM_EXT("ATV audio stable", atv_audio_status_enum,
				aml_get_atv_audio_stable,
				NULL),
#endif
#ifdef CONFIG_TVIN_VDIN
	SOC_ENUM_EXT("AV audio stable", av_audio_status_enum,
				aml_get_av_audio_stable,
				NULL),
#endif
};

static int aml_get_eqdrc_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value =
			(((unsigned int)aml_eqdrc_read(reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int aml_set_eqdrc_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int reg_value = (unsigned int)aml_eqdrc_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	reg_value &= max;
	reg_value |= (value << shift);
	aml_eqdrc_write(reg, reg_value);

	return 0;
}

static int set_HW_resample_pause_thd(unsigned int thd)
{
	aml_audin_write(AUD_RESAMPLE_CTRL2,
			(1 << 24) /* enable HW_resample_pause*/
			| (thd << 11) /* set HW resample pause thd (sample)*/
			);
	return 0;
}

static int aml_get_audin_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value =
			(((unsigned int)aml_audin_read(reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int aml_set_audin_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int reg_value = (unsigned int)aml_audin_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	reg_value &= max;
	reg_value |= (value << shift);
	aml_audin_write(reg, reg_value);

	return 0;
}

static int set_aml_EQ_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	int bytes_num = params->max;
	int i = 0, reg_num = bytes_num / AML_REG_BYTES;
	u32 *value = (u32 *)ucontrol->value.bytes.data;

	for (i = 0; i < reg_num; i++, value++) {
		aml_EQ_table[i] = be32_to_cpu(*value);
		aml_eqdrc_write(AED_EQ_CH1_COEF00 + i, aml_EQ_table[i]);
	}
	return 0;
}

static int get_aml_EQ_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	int bytes_num = params->max;
	int i = 0, reg_num = bytes_num / AML_REG_BYTES;
	u32 *value = (u32 *)ucontrol->value.bytes.data;

	for (i = 0; i < reg_num; i++, value++) {
		aml_EQ_table[i] = (u32)aml_eqdrc_read(AED_EQ_CH1_COEF00 + i);
		*value = cpu_to_be32(aml_EQ_table[i]);
	}
	return 0;
}

static int set_aml_DRC_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	int bytes_num = params->max;
	int i = 0, reg_num = bytes_num / AML_REG_BYTES;
	u32 *value = (u32 *)ucontrol->value.bytes.data;

	for (i = 0; i < reg_num; i++, value++) {
		aml_DRC_table[i] = be32_to_cpu(*value);
		aml_eqdrc_write(AED_DRC_AE + i, aml_DRC_table[i]);
	}
	return 0;
}

static int get_aml_DRC_param(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	int bytes_num = params->max;
	int i = 0, reg_num = bytes_num / AML_REG_BYTES;
	u32 *value = (u32 *)ucontrol->value.bytes.data;

	for (i = 0; i < reg_num; i++, value++) {
		aml_DRC_table[i] = (u32)aml_eqdrc_read(AED_DRC_AE + i);
		*value = cpu_to_be32(aml_DRC_table[i]);
		/*pr_info("value = 0x%x\n", aml_DRC_table[i]);*/
	}
	return 0;
}

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new aml_EQ_DRC_controls[] = {
	SOC_SINGLE_EXT_TLV("EQ master volume",
			 AED_EQ_VOLUME, 16, 0x3FF, 1,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 mvol_tlv),

	SOC_SINGLE_EXT_TLV("EQ ch1 volume",
			 AED_EQ_VOLUME, 8, 0xFF, 1,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 chvol_tlv),

	SOC_SINGLE_EXT_TLV("EQ ch2 volume",
			 AED_EQ_VOLUME, 0, 0xFF, 1,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 chvol_tlv),

	SOC_SINGLE_EXT_TLV("EQ master volume mute",
			 AED_MUTE, 31, 0x1, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("EQ enable",
			 AED_EQ_EN, 0, 0x1, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("DRC enable",
			 AED_DRC_EN, 0, 0x1, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("NG enable",
			 AED_NG_CTL, 0, 0x1, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("NG noise thd",
			 AED_NG_THD0, 8, 0x7FFF, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("NG signal thd",
			 AED_NG_THD1, 8, 0x7FFF, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("NG counter thd",
			 AED_NG_CNT_THD, 0, 0xFFFF, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SND_SOC_BYTES_EXT("EQ table",
			 (AML_EQ_PARAM_LENGTH*AML_REG_BYTES),
			 get_aml_EQ_param,
			 set_aml_EQ_param),

	SND_SOC_BYTES_EXT("DRC table",
			 (AML_DRC_PARAM_LENGTH*AML_REG_BYTES),
			 get_aml_DRC_param,
			 set_aml_DRC_param),
};

static const struct snd_kcontrol_new aml_EQ_RESAMPLE_controls[] = {
	/*
	 * 0:multiply gain after eq
	 * 1:multiply gain after ng
	 */
	SOC_SINGLE_EXT_TLV("EQ Volume Pos",
			 AED_EQ_VOLUME, 28, 0x1, 0,
			 aml_get_eqdrc_reg, aml_set_eqdrc_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("Hw resample pause enable",
			 AUD_RESAMPLE_CTRL2, 24, 0x1, 0,
			 aml_get_audin_reg, aml_set_audin_reg,
			 NULL),

	SOC_SINGLE_EXT_TLV("Hw resample pause thd",
			 AUD_RESAMPLE_CTRL2, 11, 0x1FFF, 0,
			 aml_get_audin_reg, aml_set_audin_reg,
			 NULL),
};

static void aml_audio_start_timer(struct aml_audio_private_data *p_aml_audio,
				  unsigned long delay)
{
	p_aml_audio->timer.expires = jiffies + delay;
	p_aml_audio->timer.data = (unsigned long)p_aml_audio;
	p_aml_audio->detect_flag = -1;
	add_timer(&p_aml_audio->timer);
	p_aml_audio->timer_en = 1;
}

static void aml_audio_stop_timer(struct aml_audio_private_data *p_aml_audio)
{
	del_timer_sync(&p_aml_audio->timer);
	cancel_work_sync(&p_aml_audio->work);
	p_aml_audio->timer_en = 0;
	p_aml_audio->detect_flag = -1;
}

static int audio_hp_status;
static int aml_get_audio_hp_status(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = audio_hp_status;
	return 0;
}

static const char * const audio_hp_status_texts[] = {"Unpluged", "Pluged"};

static const struct soc_enum audio_hp_status_enum = SOC_ENUM_SINGLE(
			   SND_SOC_NOPM, 0, ARRAY_SIZE(audio_hp_status_texts),
			   audio_hp_status_texts);

static const struct snd_kcontrol_new hp_controls[] = {
	   SOC_ENUM_EXT("Hp Status",
			   audio_hp_status_enum,
			   aml_get_audio_hp_status,
			   NULL),
};

static int hp_det_adc_value(struct aml_audio_private_data *p_aml_audio)
{
	int ret, hp_value;
	int hp_val_sum = 0;
	int loop_num = 0;

	while (loop_num < 8) {
		hp_value = gpiod_get_value(p_aml_audio->hp_det_desc);
		if (hp_value < 0) {
			pr_info("hp detect get error adc value!\n");
			return -1;	/* continue; */
		}
		hp_val_sum += hp_value;
		loop_num++;
		msleep_interruptible(15);
	}
	hp_val_sum = hp_val_sum >> 3;

	if (p_aml_audio->hp_det_inv) {
		if (hp_val_sum > 0) {
			/* plug in */
			ret = 1;
		} else {
			/* unplug */
			ret = 0;
		}
	} else {
		if (hp_val_sum > 0) {
			/* unplug */
			ret = 0;
		} else {
			/* plug in */
			ret = 1;
		}
	}

	return ret;
}

static int aml_audio_hp_detect(struct aml_audio_private_data *p_aml_audio)
{
	int loop_num = 0;
	int ret;

	p_aml_audio->hp_det_status = false;

	while (loop_num < 3) {
		ret = hp_det_adc_value(p_aml_audio);
		if (p_aml_audio->hp_last_state != ret) {
			msleep_interruptible(50);
			if (ret < 0)
				ret = p_aml_audio->hp_last_state;
			else
				p_aml_audio->hp_last_state = ret;
		} else
			msleep_interruptible(50);

		loop_num = loop_num + 1;
	}

	return ret;
}

/*mute: 1, ummute: 0*/
static int aml_mute_unmute(struct snd_soc_card *card, int av_mute, int amp_mute)
{
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	if (!IS_ERR(p_aml_audio->av_mute_desc)) {
		if (p_aml_audio->av_mute_inv ^ av_mute) {
			gpiod_direction_output(
				p_aml_audio->av_mute_desc, GPIOF_OUT_INIT_LOW);
			pr_info("set av out GPIOF_OUT_INIT_LOW!\n");
		} else {
			gpiod_direction_output(
				p_aml_audio->av_mute_desc, GPIOF_OUT_INIT_HIGH);
			pr_info("set av out GPIOF_OUT_INIT_HIGH!\n");
		}
	}

	if (!IS_ERR(p_aml_audio->amp_mute_desc)) {
		if (p_aml_audio->amp_mute_inv ^ amp_mute) {
			gpiod_direction_output(
				p_aml_audio->amp_mute_desc, GPIOF_OUT_INIT_LOW);
			pr_info("set amp out GPIOF_OUT_INIT_LOW!\n");
		} else {
			gpiod_direction_output(
				p_aml_audio->amp_mute_desc,
				GPIOF_OUT_INIT_HIGH);
			pr_info("set amp out GPIOF_OUT_INIT_HIGH!\n");
		}
	}
	return 0;
}

static void aml_asoc_work_func(struct work_struct *work)
{
	struct aml_audio_private_data *p_aml_audio = NULL;
	struct snd_soc_card *card = NULL;
	int flag = -1;

	p_aml_audio = container_of(work, struct aml_audio_private_data, work);
	card = (struct snd_soc_card *)p_aml_audio->data;

	flag = aml_audio_hp_detect(p_aml_audio);

	if (p_aml_audio->detect_flag != flag) {
		p_aml_audio->detect_flag = flag;

		if (flag & 0x1) {
			pr_info("aml aduio hp pluged\n");
			audio_hp_status = 1;
			aml_mute_unmute(card, 0, 1);
		} else {
			pr_info("aml audio hp unpluged\n");
			audio_hp_status = 0;
			aml_mute_unmute(card, 1, 0);
		}

	}

	p_aml_audio->hp_det_status = true;
}

static void aml_asoc_timer_func(unsigned long data)
{
	struct aml_audio_private_data *p_aml_audio =
	    (struct aml_audio_private_data *)data;
	unsigned long delay = msecs_to_jiffies(150);

	if (p_aml_audio->hp_det_status &&
			!p_aml_audio->suspended) {
		schedule_work(&p_aml_audio->work);
	}
	mod_timer(&p_aml_audio->timer, jiffies + delay);
}

static int aml_suspend_pre(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);

	if (p_aml_audio->av_hs_switch) {
		/* stop timer */
		mutex_lock(&p_aml_audio->lock);
		p_aml_audio->suspended = true;
		if (p_aml_audio->timer_en)
			aml_audio_stop_timer(p_aml_audio);

		mutex_unlock(&p_aml_audio->lock);
	}

	aml_mute_unmute(card, 1, 1);
	return 0;
}

static int aml_suspend_post(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);
	return 0;
}

static int aml_resume_pre(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);
	return 0;
}

static int aml_resume_post(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);

	schedule_work(&p_aml_audio->init_work);

	return 0;
}

static int aml_asoc_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct aml_audio_private_data *p_aml_audio =
		snd_soc_card_get_drvdata(rtd->card);
	unsigned int fmt;
	int ret;

	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM;

	if (p_aml_audio
		&& p_aml_audio->cardinfo)
		fmt |= p_aml_audio->cardinfo->chipset_info.fmt_polarity;
	else
		fmt |= SND_SOC_DAIFMT_IB_NF;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);

	if (ret < 0) {
		pr_err("%s: set cpu dai fmt failed!\n", __func__);
		return ret;
	}

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("%s: set codec dai fmt failed!\n", __func__);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops aml_asoc_ops = {
	.hw_params	= aml_asoc_hw_params,
};

static int aml_asoc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->component.dapm;
	struct aml_audio_private_data *p_aml_audio;
	int ret = 0;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);

	ret = snd_soc_add_card_controls(codec->component.card,
					aml_tv_controls,
					ARRAY_SIZE(aml_tv_controls));

	/* Add specific widgets */
	snd_soc_dapm_new_controls(dapm, aml_asoc_dapm_widgets,
				  ARRAY_SIZE(aml_asoc_dapm_widgets));

	p_aml_audio->pin_ctl =
		devm_pinctrl_get_select(card->dev, "audio_i2s");
	if (IS_ERR(p_aml_audio->pin_ctl)) {
		pr_info("%s, aml_tv_pinmux_init error!\n", __func__);
		return 0;
	}

	/*read avmute pinmux from dts*/
	p_aml_audio->av_mute_desc =
		gpiod_get(card->dev,
				"mute_gpio",
				0);
	of_property_read_u32(card->dev->of_node, "av_mute_inv",
		&p_aml_audio->av_mute_inv);
	of_property_read_u32(card->dev->of_node, "sleep_time",
		&p_aml_audio->sleep_time);

	/*read amp mute pinmux from dts*/
	p_aml_audio->amp_mute_desc =
		gpiod_get(card->dev,
				"amp_mute_gpio",
				0);
	of_property_read_u32(card->dev->of_node, "amp_mute_inv",
		&p_aml_audio->amp_mute_inv);

	/*read headset pinmux from dts*/
	of_property_read_u32(card->dev->of_node, "av_hs_switch",
		&p_aml_audio->av_hs_switch);

	if (p_aml_audio->av_hs_switch) {
		/* headset dection gipo */
		p_aml_audio->hp_det_desc = gpiod_get(card->dev,
			"hp_det", GPIOD_IN);
		if (!IS_ERR(p_aml_audio->hp_det_desc))
			gpiod_direction_input(p_aml_audio->hp_det_desc);
		else
			pr_err("ASoC: hp_det-gpio failed\n");

		of_property_read_u32(card->dev->of_node,
			"hp_det_inv",
			&p_aml_audio->hp_det_inv);
		pr_info("hp_det_inv:%d, %s\n",
			p_aml_audio->hp_det_inv,
			p_aml_audio->hp_det_inv ?
			"hs pluged, HP_DET:1; hs unpluged, HS_DET:0"
			:
			"hs pluged, HP_DET:0; hs unpluged, HS_DET:1");

		p_aml_audio->hp_det_status = true;

		init_timer(&p_aml_audio->timer);
		p_aml_audio->timer.function = aml_asoc_timer_func;
		p_aml_audio->timer.data = (unsigned long)p_aml_audio;
		p_aml_audio->data = (void *)card;

		INIT_WORK(&p_aml_audio->work, aml_asoc_work_func);
		mutex_init(&p_aml_audio->lock);

		ret = snd_soc_add_card_controls(codec->component.card,
					hp_controls, ARRAY_SIZE(hp_controls));
	}

	/*It is used for switch of ARC_IN & SPDIF_IN */
	p_aml_audio->source_switch = gpiod_get(card->dev,
				"source_switch", GPIOF_OUT_INIT_HIGH);
	if (!IS_ERR(p_aml_audio->source_switch)) {
		of_property_read_u32(card->dev->of_node, "source_switch_inv",
				&p_aml_audio->audio_in_GPIO_inv);
		if (p_aml_audio->audio_in_GPIO_inv == 0) {
			gpiod_direction_output(p_aml_audio->source_switch,
				GPIOF_OUT_INIT_HIGH);
		} else {
			gpiod_direction_output(p_aml_audio->source_switch,
				GPIOF_OUT_INIT_LOW);
		}
		snd_soc_add_card_controls(card, av_controls,
					ARRAY_SIZE(av_controls));
	}

	return 0;
}

static void aml_tv_pinmux_init(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	if (!p_aml_audio->av_hs_switch) {
		if (p_aml_audio->sleep_time &&
				(!IS_ERR(p_aml_audio->av_mute_desc)))
			msleep(p_aml_audio->sleep_time);
		aml_mute_unmute(card, 0, 0);
		pr_info("av_mute_inv:%d, amp_mute_inv:%d, sleep %d ms\n",
			p_aml_audio->av_mute_inv, p_aml_audio->amp_mute_inv,
			p_aml_audio->sleep_time);
	} else {
		if (p_aml_audio->sleep_time &&
				(!IS_ERR(p_aml_audio->av_mute_desc)))
			msleep(p_aml_audio->sleep_time);
		pr_info("aml audio hs detect enable!\n");
		p_aml_audio->suspended = false;
		mutex_lock(&p_aml_audio->lock);
		if (!p_aml_audio->timer_en) {
			aml_audio_start_timer(p_aml_audio,
						  msecs_to_jiffies(100));
		}
		mutex_unlock(&p_aml_audio->lock);
	}
}

static int check_channel_mask(const char *str)
{
	int ret = -1;

	if (!strncmp(str, "i2s_0/1", 7))
		ret = 0;
	else if (!strncmp(str, "i2s_2/3", 7))
		ret = 1;
	else if (!strncmp(str, "i2s_4/5", 7))
		ret = 2;
	else if (!strncmp(str, "i2s_6/7", 7))
		ret = 3;
	return ret;
}

static void parse_speaker_channel_mask(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct device_node *np;
	const char *str;
	int ret;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	/* channel mask */
	np = of_get_child_by_name(card->dev->of_node,
			"Channel_Mask");
	if (np == NULL) {
		ret = -1;
		pr_err("error: failed to find channel mask node %s\n",
				"Channel_Mask");
		return;
	}

	/* ext Speaker mask*/
	ret = of_property_read_string(np, "Speaker0_Channel_Mask", &str);
	if (!ret) {
		ret = check_channel_mask(str);
		if (ret >= 0) {
			p_aml_audio->Speaker0_Channel_Mask = ret;
			aml_aiu_update_bits(AIU_I2S_OUT_CFG,
				0x3, p_aml_audio->Speaker0_Channel_Mask);
		}
	}
	ret = of_property_read_string(np, "Speaker1_Channel_Mask", &str);
	if (!ret) {
		ret = check_channel_mask(str);
		if (ret >= 0) {
			p_aml_audio->Speaker1_Channel_Mask = ret;
			aml_aiu_update_bits(AIU_I2S_OUT_CFG,
				0x3 << 2,
				p_aml_audio->Speaker1_Channel_Mask << 2);
		}
	}
	ret = of_property_read_string(np, "Speaker2_Channel_Mask", &str);
	if (!ret) {
		ret = check_channel_mask(str);
		if (ret >= 0) {
			p_aml_audio->Speaker2_Channel_Mask = ret;
			aml_aiu_update_bits(AIU_I2S_OUT_CFG,
				0x3 << 4,
				p_aml_audio->Speaker2_Channel_Mask << 4);
		}
	}
	ret = of_property_read_string(np, "Speaker3_Channel_Mask", &str);
	if (!ret) {
		ret = check_channel_mask(str);
		if (ret >= 0) {
			p_aml_audio->Speaker3_Channel_Mask = ret;
			aml_aiu_update_bits(AIU_I2S_OUT_CFG,
				0x3 << 6,
				p_aml_audio->Speaker3_Channel_Mask << 6);
		}
	}
}

static void parse_dac_channel_mask(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct device_node *np;
	const char *str;
	int ret;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	/* channel mask */
	np = of_get_child_by_name(card->dev->of_node,
			"Channel_Mask");
	if (np == NULL) {
		ret = -1;
		pr_err("error: failed to find channel mask node %s\n",
				"Channel_Mask");
		return;
	}

	/*Acodec DAC0 selects i2s source*/
	ret = of_property_read_string(np, "DAC0_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read DAC0_Channel_Mask\n");
		return;
	}
	ret = check_channel_mask(str);
	if (ret >= 0) {
		p_aml_audio->DAC0_Channel_Mask = ret;
		aml_aiu_update_bits(AIU_ACODEC_CTRL, 0x3,
				p_aml_audio->DAC0_Channel_Mask);
	}
	/*Acodec DAC1 selects i2s source*/
	ret = of_property_read_string(np, "DAC1_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read DAC1_Channel_Mask\n");
		return;
	}
	ret = check_channel_mask(str);
	if (ret >= 0) {
		p_aml_audio->DAC1_Channel_Mask = ret;
		aml_aiu_update_bits(AIU_ACODEC_CTRL, 0x3 << 8,
				p_aml_audio->DAC1_Channel_Mask << 8);
	}
}

static void parse_eqdrc_channel_mask(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct device_node *np;
	const char *str;
	int ret;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	/* channel mask */
	np = of_get_child_by_name(card->dev->of_node,
			"Channel_Mask");
	if (np == NULL) {
		ret = -1;
		pr_err("error: failed to find channel mask node %s\n",
				"Channel_Mask");
		return;
	}

	/*Hardware EQ and DRC can be muxed to i2s 2 channels*/
	ret = of_property_read_string(np, "EQ_DRC_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read EQ_DRC_Channel_Mask\n");
		return;
	}
	ret = check_channel_mask(str);
	if (ret >= 0) {
		p_aml_audio->EQ_DRC_Channel_Mask = ret;
		 /*i2s in sel*/
		aml_eqdrc_update_bits(AED_TOP_CTL, (0x7 << 1),
			(p_aml_audio->EQ_DRC_Channel_Mask << 1));
		aml_eqdrc_write(AED_ED_CTL, 1);
		/* disable noise gate*/
		aml_eqdrc_write(AED_NG_CTL, (3 << 30));
		aml_eqdrc_update_bits(AED_TOP_CTL, 1, 1);
	}

	/* If spdif is same source to i2s,
	 * it can be muxed to i2s 2 channels
	 */
	ret = of_property_read_string(np,
			"Spdif_samesource_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read Spdif_samesource_Channel_Mask\n");
		return;
	}
	ret = check_channel_mask(str);
	if (ret >= 0) {
		p_aml_audio->Spdif_samesource_Channel_Mask = ret;
		aml_aiu_update_bits(AIU_I2S_MISC, 0x7 << 5,
			p_aml_audio->Spdif_samesource_Channel_Mask << 5);
	}

}

/* spdif same source with i2s */
static void parse_samesource_channel_mask(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct device_node *np;
	const char *str;
	int ret;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	/* channel mask */
	np = of_get_child_by_name(card->dev->of_node,
			"Channel_Mask");
	if (np == NULL) {
		ret = -1;
		pr_err("error: failed to find channel mask node %s\n",
				"Channel_Mask");
		return;
	}

	/* If spdif is same source to i2s,
	 * it can be muxed to i2s 2 channels
	 */
	ret = of_property_read_string(np,
			"Spdif_samesource_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read Spdif_samesource_Channel_Mask\n");
		return;
	}
	ret = check_channel_mask(str);
	if (ret >= 0) {
		p_aml_audio->Spdif_samesource_Channel_Mask = ret;
		aml_aiu_update_bits(AIU_I2S_MISC, 0x7 << 5,
			p_aml_audio->Spdif_samesource_Channel_Mask << 5);
	}

}

static int aml_card_dai_parse_of(struct device *dev,
				 struct snd_soc_dai_link *dai_link,
				 int (*init)(
					 struct snd_soc_pcm_runtime *rtd),
				 struct device_node *cpu_node,
				 struct device_node *codec_node,
				 struct device_node *plat_node)
{
	int ret;

	/* get cpu dai->name */
	ret = snd_soc_of_get_dai_name(cpu_node, &dai_link->cpu_dai_name);
	if (ret < 0)
		goto parse_error;

	/* get codec dai->name */
	ret = snd_soc_of_get_dai_name(codec_node, &dai_link->codec_dai_name);
	if (ret < 0)
		goto parse_error;

	dai_link->name = dai_link->stream_name = dai_link->cpu_dai_name;
	dai_link->codec_of_node = of_parse_phandle(codec_node, "sound-dai", 0);
	dai_link->platform_of_node = plat_node;
	dai_link->init = init;

	return 0;

parse_error:
	return ret;
}

struct snd_soc_aux_dev tv_audio_aux_dev;
static struct snd_soc_codec_conf tv_audio_codec_conf[] = {
	{
		.name_prefix = "AMP",
	},
};
static struct codec_info codec_info_aux;

static int aml_aux_dev_parse_of_ext(struct snd_soc_card *card)
{
	struct device_node *audio_codec_node = card->dev->of_node;
	struct device_node *aux_node;
	int n, len;

	if (!of_find_property(audio_codec_node, "aux_dev", &len))
		return 0;		/* Ok to have no aux-devs */

	n = len / sizeof(__be32);
	if (n <= 0)
		return -EINVAL;

	pr_info("num of aux_dev:%d\n", n);

	if (n == 1) {
		const char *codec_name;

		aux_node = of_parse_phandle(audio_codec_node, "aux_dev", 0);
		if (aux_node == NULL) {
			pr_info("error: failed to find aux dev node\n");
			return -1;
		}

		if (of_property_read_string(
				aux_node,
				"codec_name",
				&codec_name)) {
			pr_info("no aux dev!\n");
			return -ENODEV;
		}
		pr_info("aux name = %s\n", codec_name);
		strlcpy(codec_info_aux.name, codec_name, I2C_NAME_SIZE);
		strlcpy(codec_info_aux.name_bus, codec_name, I2C_NAME_SIZE);

		tv_audio_aux_dev.name = codec_info_aux.name;
		tv_audio_aux_dev.codec_name = codec_info_aux.name_bus;
		tv_audio_codec_conf[0].dev_name = codec_info_aux.name_bus;

		card->aux_dev = &tv_audio_aux_dev;
		card->num_aux_devs = 1;
		card->codec_conf = tv_audio_codec_conf;
		card->num_configs = ARRAY_SIZE(tv_audio_codec_conf);

	} else
		pr_info("Cannot support multi aux-dev yet\n");

	return 0;
}

static int aml_card_dais_parse_of(struct snd_soc_card *card)
{
	struct device_node *np = card->dev->of_node;
	struct device_node *cpu_node, *codec_node, *plat_node;
	struct device *dev = card->dev;
	struct snd_soc_dai_link *dai_links;
	int num_dai_links, cpu_num, codec_num, plat_num;
	int i, ret;

	int (*init)(struct snd_soc_pcm_runtime *rtd);

	ret = of_count_phandle_with_args(np, "cpu_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no cpu_list errno: %d\n", ret);
		goto err;
	} else {
		cpu_num = ret;
	}
	ret = of_count_phandle_with_args(np, "codec_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no codec_list errno: %d\n", ret);
		goto err;
	} else {
		codec_num = ret;
	}
	ret = of_count_phandle_with_args(np, "plat_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no plat_list errno: %d\n", ret);
		goto err;
	} else {
		plat_num = ret;
	}
	if ((cpu_num == codec_num) && (cpu_num == plat_num)) {
		num_dai_links = cpu_num;
	} else {
		dev_err(dev,
			"AML sound card cpu_dai num, codec_dai num, platform num don't match: %d\n",
			ret);
		ret = -EINVAL;
		goto err;
	}

	dai_links =
		devm_kzalloc(dev,
			     num_dai_links * sizeof(struct snd_soc_dai_link),
			     GFP_KERNEL);
	if (!dai_links) {
		dev_err(dev, "Can't allocate snd_soc_dai_links\n");
		ret = -ENOMEM;
		goto err;
	}
	card->dai_link = dai_links;
	card->num_links = num_dai_links;
	for (i = 0; i < num_dai_links; i++) {
		init = NULL;
		/* CPU sub-node */
		cpu_node = of_parse_phandle(np, "cpu_list", i);
		if (cpu_node == NULL) {
			dev_err(dev, "parse aml sound card cpu list error\n");
			return -EINVAL;
		}
		/* CODEC sub-node */
		codec_node = of_parse_phandle(np, "codec_list", i);
		if (codec_node == NULL) {
			dev_err(dev, "parse aml sound card codec list error\n");
			return ret;
		}
		/* Platform sub-node */
		plat_node = of_parse_phandle(np, "plat_list", i);
		if (plat_node == NULL) {
			dev_err(dev,
				"parse aml sound card platform list error\n");
			return ret;
		}
		if (i == 0)
			init = aml_asoc_init;

		ret =
			aml_card_dai_parse_of(dev, &dai_links[i], init,
					      cpu_node,
					      codec_node, plat_node);

		if ((strstr(dai_links[i].name, "I2S") != NULL)
			&& (!ret))
			dai_links[i].ops = &aml_asoc_ops;
	}

err:
	return ret;
}

static void aml_init_work_func(struct work_struct *init_work)
{
	struct aml_audio_private_data *p_aml_audio = NULL;
	struct snd_soc_card *card = NULL;
	struct aml_card_info *p_aml_cardinfo = NULL;

	p_aml_audio = container_of(init_work,
				  struct aml_audio_private_data, init_work);
	card = (struct snd_soc_card *)p_aml_audio->data;

	/* chipset init */
	p_aml_cardinfo = p_aml_audio->cardinfo;
	if (p_aml_cardinfo)
		p_aml_cardinfo->chipset_init(card);

	/* controls */
	snd_soc_add_card_controls(card, aml_EQ_DRC_controls,
					ARRAY_SIZE(aml_EQ_DRC_controls));

	/* pinmux */
	aml_tv_pinmux_init(card);
}


int txl_chipset_init(struct snd_soc_card *card)
{
	/*EQ/DRC*/
	set_internal_EQ_volume(0xc0, 0x30, 0x30);
	init_EQ_DRC_module();

	/* channel mask */
	parse_speaker_channel_mask(card);
	/* dac mask */
	parse_dac_channel_mask(card);

	return 0;
}

int txlx_chipset_init(struct snd_soc_card *card)
{
	/*EQ/DRC*/
	set_internal_EQ_volume(0xc0, 0x30, 0x30);

	/* channel mask */
	parse_speaker_channel_mask(card);
	/* dac mask */
	parse_dac_channel_mask(card);
	/* eq/drc mask */
	parse_eqdrc_channel_mask(card);
	/* samesource mask */
	parse_samesource_channel_mask(card);

	return 0;
}

int txhd_chipset_init(struct snd_soc_card *card)
{
	/*EQ/DRC*/
	set_internal_EQ_volume(0xc0, 0x30, 0x30);
	set_HW_resample_pause_thd(128);

	/* special kcontrols */
	snd_soc_add_card_controls(card,
		aml_EQ_RESAMPLE_controls,
		ARRAY_SIZE(aml_EQ_RESAMPLE_controls));

	/* channel mask */
	parse_speaker_channel_mask(card);
	/* dac mask */
	parse_dac_channel_mask(card);
	/* eq/drc mask */
	parse_eqdrc_channel_mask(card);
	/* samesource mask */
	parse_samesource_channel_mask(card);

	return 0;
}

int gxtvbb_set_audin_source(int audin_src)
{
	if (audin_src == 0) {
		/* select external codec ADC as I2S source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3, 0);
	} else if (audin_src == 1) {
		/* select ATV output as I2S source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3, 1);
	} else if (audin_src == 2) {
		/* select HDMI-rx as Audio In source */
		/* [14:12]cntl_hdmirx_chsts_sel: */
		/* 0=Report chan1 status; 1=Report chan2 status */
		/* [11:8] cntl_hdmirx_chsts_en */
		/* [5:4] spdif_src_sel:*/
		/* 1=Select HDMIRX SPDIF output as AUDIN source */
		/* [1:0] i2sin_src_sel: */
		/* 2=Select HDMIRX I2S output as AUDIN source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3, 2);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4,
						1 << 4);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0xf << 8,
						0xf << 8);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x7 << 12,
						0);
	}  else if (audin_src == 3) {
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4, 0);
	}

	return 0;
}

int txl_set_audin_source(int audin_src)
{
	if (audin_src == 0) {
		/* select internal codec ADC in TXL as I2S source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3, 3);
	} else if (audin_src == 1) {
		/* select ATV output as I2S source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3, 1);
	} else if (audin_src == 2) {
		/* select HDMI-rx as Audio In source */
		/* [14:12]cntl_hdmirx_chsts_sel: */
		/* 0=Report chan1 status; 1=Report chan2 status */
		/* [11:8] cntl_hdmirx_chsts_en */
		/* [5:4] spdif_src_sel:*/
		/* 1=Select HDMIRX SPDIF output as AUDIN source */
		/* [1:0] i2sin_src_sel: */
		/* 2=Select HDMIRX I2S output as AUDIN source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3, 2);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4,
						1 << 4);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0xf << 8,
						0xf << 8);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x7 << 12,
						0);
	}  else if (audin_src == 3) {
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4, 0);
	}

	return 0;
}

int txlx_set_audin_source(int audin_src)
{
	if (audin_src == 0) {
		/* select internal codec ADC in TXLX as HDMI-i2s*/
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3 << 16,
					3 << 16);
	} else if (audin_src == 1) {
		/* select ATV output as I2S source */
	} else if (audin_src == 2) {
		/* select HDMI-rx as Audio In source */
		/* [14:12]cntl_hdmirx_chsts_sel: */
		/* 0=Report chan1 status; 1=Report chan2 status */
		/* [11:8] cntl_hdmirx_chsts_en */
		/* [5:4] spdif_src_sel:*/
		/* 1=Select HDMIRX SPDIF output as AUDIN source */
		/* [1:0] i2sin_src_sel: */
		/* 2=Select HDMIRX I2S output as AUDIN source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4,
						1 << 4);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0xf << 8,
						0xf << 8);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x7 << 12,
						0);
	}  else if (audin_src == 3) {
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4, 0);
	}

	return 0;
}

int txhd_set_audin_source(int audin_src)
{
	if (audin_src == 0) {
		/* select internal codec ADC in TXLX as HDMI-i2s*/
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 3 << 16,
					3 << 16);
	} else if (audin_src == 1) {
		/* select ATV output as I2S source */
	} else if (audin_src == 2) {
		/* select HDMI-rx as Audio In source */
		/* [14:12]cntl_hdmirx_chsts_sel: */
		/* 0=Report chan1 status; 1=Report chan2 status */
		/* [11:8] cntl_hdmirx_chsts_en */
		/* [5:4] spdif_src_sel:*/
		/* 1=Select HDMIRX SPDIF output as AUDIN source */
		/* [1:0] i2sin_src_sel: */
		/* 2=Select HDMIRX I2S output as AUDIN source */
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4,
						1 << 4);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0xf << 8,
						0xf << 8);
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x7 << 12,
						0);
	}  else if (audin_src == 3) {
		aml_audin_update_bits(AUDIN_SOURCE_SEL, 0x3 << 4, 0);
	}

	return 0;
}

int txl_set_resample_param(int index)
{
	set_hw_resample_param(index);

	return 0;
}

int txlx_set_resample_param(int index)
{
	set_hw_resample_param(index);

	return 0;
}

int txhd_set_resample_param(int index)
{
	set_hw_resample_param(index);

	return 0;
}

static struct aml_card_info tv_gxtvbb_info = {
	.chipset_info = {
		.is_tv_chipset     = true,
		.audin_clk_support = false,
		.fmt_polarity      = SND_SOC_DAIFMT_IB_NF,
	},
	.set_audin_source    = gxtvbb_set_audin_source,
};

static struct aml_card_info tv_txl_info = {
	.chipset_info = {
		.is_tv_chipset     = true,
		.audin_clk_support = false,
		.fmt_polarity      = SND_SOC_DAIFMT_NB_NF,
	},
	.chipset_init        = txl_chipset_init,
	.set_audin_source    = txl_set_audin_source,
	.set_resample_param  = txl_set_resample_param,
};

static struct aml_card_info tv_txlx_info = {
	.chipset_info = {
		.is_tv_chipset      = true,
		.audin_clk_support  = true,
		.audin_ext_support  = true,
		.audbuf_gate_rm     = true,
		.audin_lr_invert    = true,
		.fmt_polarity       = SND_SOC_DAIFMT_IB_NF,
	},
	.chipset_init        = txlx_chipset_init,
	.set_audin_source    = txlx_set_audin_source,
	.set_resample_param  = txlx_set_resample_param,
};

static struct aml_card_info tv_txhd_info = {
	.chipset_info = {
		.is_tv_chipset     = true,
		.audin_clk_support = true,
		.audin_ext_support = true,
		.audin_lr_invert   = true,
		.audbuf_gate_rm    = true,
		.split_complete    = true,
		.spdif_pao         = true,
		.spdifin_more_rate = true,
		.fmt_polarity      = SND_SOC_DAIFMT_IB_NF,
	},
	.chipset_init        = txhd_chipset_init,
	.set_audin_source    = txhd_set_audin_source,
	.set_resample_param  = txhd_set_resample_param,
};

static const struct of_device_id amlogic_audio_of_match[] = {
	{
		.compatible = "amlogic, gxtvbb-snd-tv",
		.data       = &tv_gxtvbb_info,
	},
	{
		.compatible = "amlogic, txl-snd-tv",
		.data       = &tv_txl_info,
	},
	{
		.compatible = "amlogic, txlx-snd-tv",
		.data       = &tv_txlx_info,
	},
	{
		.compatible = "amlogic, txhd-snd-tv",
		.data       = &tv_txhd_info,
	},
	{},
};

static int aml_tv_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card;
	struct aml_audio_private_data *p_aml_audio;
	const struct of_device_id *match;
	int ret;

	p_aml_audio =
		devm_kzalloc(dev, sizeof(struct aml_audio_private_data),
				 GFP_KERNEL);
	if (!p_aml_audio) {
		dev_err(&pdev->dev, "Can't allocate aml_audio_private_data\n");
		ret = -ENOMEM;
		goto err;
	}

	card = devm_kzalloc(dev, sizeof(struct snd_soc_card), GFP_KERNEL);
	if (!card) {
		/*dev_err(&pdev->dev, "Can't allocate snd_soc_card\n");*/
		ret = -ENOMEM;
		goto err;
	}

	match = of_match_device(
		of_match_ptr(amlogic_audio_of_match),
		&pdev->dev);

	if (match) {
		p_aml_audio->cardinfo = (struct aml_card_info *)match->data;
		aml_chipset_update_info(&p_aml_audio->cardinfo->chipset_info);
	} else
		pr_warn_once("failed get tv chipset info\n");

	snd_soc_card_set_drvdata(card, p_aml_audio);

	card->dev = dev;
	platform_set_drvdata(pdev, card);
	ret = snd_soc_of_parse_card_name(card, "aml_sound_card,name");
	if (ret < 0) {
		dev_err(dev, "no specific snd_soc_card name\n");
		goto err;
	}

	ret = aml_card_dais_parse_of(card);
	if (ret < 0) {
		dev_err(dev, "parse aml sound card dais error %d\n", ret);
		goto err;
	}
	aml_aux_dev_parse_of_ext(card);

	card->suspend_pre = aml_suspend_pre,
	card->suspend_post = aml_suspend_post,
	card->resume_pre = aml_resume_pre,
	card->resume_post = aml_resume_post,

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret < 0) {
		dev_err(dev, "register aml sound card error %d\n", ret);
		goto err;
	}

	p_aml_audio->data = (void *)card;
	INIT_WORK(&p_aml_audio->init_work, aml_init_work_func);
	schedule_work(&p_aml_audio->init_work);

	return 0;
err:
	dev_err(dev, "Can't probe snd_soc_card\n");
	return ret;
}

static void aml_tv_audio_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *card;

	card = platform_get_drvdata(pdev);
	aml_suspend_pre(card);
}

static struct platform_driver aml_tv_audio_driver = {
	.driver			= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_audio_of_match,
		.pm = &snd_soc_pm_ops,
	},
	.probe			= aml_tv_audio_probe,
	.shutdown		= aml_tv_audio_shutdown,
};

module_platform_driver(aml_tv_audio_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML_TV audio machine Asoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
