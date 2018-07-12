/*
 * sound/soc/codecs/amlogic/aml_codec_t9015S.c
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

#include "aml_codec_t9015S.h"

struct aml_T9015S_audio_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;
};

static const struct reg_default t9015s_init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x34003CFF},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x50502929},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFBFB0000},
	{LINE_OUT_CONFIG, 0x00004444},
	{POWER_CONFIG, 0x00010000},
};

static int aml_T9015S_audio_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t9015s_init_list); i++)
		snd_soc_write(codec, t9015s_init_list[i].reg,
				t9015s_init_list[i].def);

	return 0;
}

static int aml_DAC_Gain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	u32 add = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_read(codec, add);
	u32 val1 = (val & (0x1 <<  DAC_GAIN_SEL_L)) >> DAC_GAIN_SEL_L;
	u32 val2 = (val & (0x1 <<  DAC_GAIN_SEL_H)) >> (DAC_GAIN_SEL_H - 1);

	val = val1 | val2;
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_Gain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	u32 add = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_read(codec, add);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_write(codec, add, val);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum DAC_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
			DAC_Gain_texts);

static const struct snd_kcontrol_new T9015S_audio_snd_controls[] = {
	/*PGA_IN Gain */
	SOC_DOUBLE_TLV("PGA IN Gain", ADC_VOL_CTR_PGA_IN_CONFIG,
		       PGAL_IN_GAIN, PGAR_IN_GAIN,
		       0x1f, 0, pga_in_tlv),

	/*ADC Digital Volume control */
	SOC_DOUBLE_TLV("ADC Digital Capture Volume", ADC_VOL_CTR_PGA_IN_CONFIG,
		       ADCL_VC, ADCR_VC,
		       0x7f, 0, adc_vol_tlv),

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume",
			   DAC_VOL_CTR_DAC_SOFT_MUTE,
			   DACL_VC, DACR_VC,
			   0xff, 0, dac_vol_tlv),

    /*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
			   DAC_Gain_enum,
			   aml_DAC_Gain_get_enum,
			   aml_DAC_Gain_set_enum),

};

/*pgain Left Channel Input */
static const char * const T9015S_pgain_left_txt[] = {
	"None", "AIL1", "AIL2", "AIL3", "AIL4"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_pgain_left_enum,
				  ADC_VOL_CTR_PGA_IN_CONFIG,
				  PGAL_IN_SEL, T9015S_pgain_left_txt);

static const struct snd_kcontrol_new pgain_ln_mux =
SOC_DAPM_ENUM("ROUTE_L", T9015S_pgain_left_enum);

/*pgain right Channel Input */
static const char * const T9015S_pgain_right_txt[] = {
	"None", "AIR1", "AIR2", "AIR3", "AIR4"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_pgain_right_enum,
				  ADC_VOL_CTR_PGA_IN_CONFIG,
				  PGAR_IN_SEL, T9015S_pgain_right_txt);

static const struct snd_kcontrol_new pgain_rn_mux =
SOC_DAPM_ENUM("ROUTE_R", T9015S_pgain_right_enum);

/*line out Left Positive mux */
static const char * const T9015S_out_lp_txt[] = {
	"None", "LOLP_SEL_AIL_INV", "LOLP_SEL_AIL", "Reserved", "LOLP_SEL_DACL"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_out_lp_enum, LINE_OUT_CONFIG,
				  LOLP_SEL_AIL_INV, T9015S_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
SOC_DAPM_ENUM("ROUTE_LP_OUT", T9015S_out_lp_enum);

/*line out Left Negative mux */
static const char * const T9015S_out_ln_txt[] = {
	"None", "LOLN_SEL_AIL", "LOLN_SEL_DACL", "Reserved", "LOLN_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_out_ln_enum, LINE_OUT_CONFIG,
				  LOLN_SEL_AIL, T9015S_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
SOC_DAPM_ENUM("ROUTE_LN_OUT", T9015S_out_ln_enum);

/*line out Right Positive mux */
static const char * const T9015S_out_rp_txt[] = {
	"None", "LORP_SEL_AIR_INV", "LORP_SEL_AIR", "Reserved", "LORP_SEL_DACR"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_out_rp_enum, LINE_OUT_CONFIG,
				  LORP_SEL_AIR_INV, T9015S_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
SOC_DAPM_ENUM("ROUTE_RP_OUT", T9015S_out_rp_enum);

/*line out Right Negative mux */
static const char * const T9015S_out_rn_txt[] = {
	"None", "LORN_SEL_AIR", "LORN_SEL_DACR", "Reserved", "LORN_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015S_out_rn_enum, LINE_OUT_CONFIG,
				  LORN_SEL_AIR, T9015S_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
SOC_DAPM_ENUM("ROUTE_RN_OUT", T9015S_out_rn_enum);

static const struct snd_soc_dapm_widget T9015S_audio_dapm_widgets[] = {

	/* Input */
	SND_SOC_DAPM_INPUT("Linein left 1"),
	SND_SOC_DAPM_INPUT("Linein left 2"),
	SND_SOC_DAPM_INPUT("Linein left 3"),
	SND_SOC_DAPM_INPUT("Linein left 4"),

	SND_SOC_DAPM_INPUT("Linein right 1"),
	SND_SOC_DAPM_INPUT("Linein right 2"),
	SND_SOC_DAPM_INPUT("Linein right 3"),
	SND_SOC_DAPM_INPUT("Linein right 4"),

	/*PGA input */
	SND_SOC_DAPM_PGA("PGAL_IN_EN", AUDIO_CONFIG_BLOCK_ENABLE,
			 PGAL_IN_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGAR_IN_EN", AUDIO_CONFIG_BLOCK_ENABLE,
			 PGAR_IN_EN, 0, NULL, 0),

	/*PGA input source select */
	SND_SOC_DAPM_MUX("Linein left switch", SND_SOC_NOPM,
			 0, 0, &pgain_ln_mux),
	SND_SOC_DAPM_MUX("Linein right switch", SND_SOC_NOPM,
			 0, 0, &pgain_rn_mux),

	/*ADC capture stream */
	SND_SOC_DAPM_ADC("Left ADC", "HIFI Capture", AUDIO_CONFIG_BLOCK_ENABLE,
			 ADCL_EN, 0),
	SND_SOC_DAPM_ADC("Right ADC", "HIFI Capture", AUDIO_CONFIG_BLOCK_ENABLE,
			 ADCR_EN, 0),

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout left N"),
	SND_SOC_DAPM_OUTPUT("Lineout left P"),
	SND_SOC_DAPM_OUTPUT("Lineout right N"),
	SND_SOC_DAPM_OUTPUT("Lineout right P"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE,
			 DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE,
			 DACR_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LOLP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LOLN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout left P switch", SND_SOC_NOPM,
			 0, 0, &line_out_lp_mux),
	SND_SOC_DAPM_MUX("Lineout left N switch", SND_SOC_NOPM,
			 0, 0, &line_out_ln_mux),
	SND_SOC_DAPM_MUX("Lineout right P switch", SND_SOC_NOPM,
			 0, 0, &line_out_rp_mux),
	SND_SOC_DAPM_MUX("Lineout right N switch", SND_SOC_NOPM,
			 0, 0, &line_out_rn_mux),
};

static const struct snd_soc_dapm_route T9015S_audio_dapm_routes[] = {
/* Input path */
	{"Linein left switch", "AIL1", "Linein left 1"},
	{"Linein left switch", "AIL2", "Linein left 2"},
	{"Linein left switch", "AIL3", "Linein left 3"},
	{"Linein left switch", "AIL4", "Linein left 4"},

	{"Linein right switch", "AIR1", "Linein right 1"},
	{"Linein right switch", "AIR2", "Linein right 2"},
	{"Linein right switch", "AIR3", "Linein right 3"},
	{"Linein right switch", "AIR4", "Linein right 4"},

	{"PGAL_IN_EN", NULL, "Linein left switch"},
	{"PGAR_IN_EN", NULL, "Linein right switch"},

	{"Left ADC", NULL, "PGAL_IN_EN"},
	{"Right ADC", NULL, "PGAR_IN_EN"},

/*Output path*/
	{"Lineout left P switch", "LOLP_SEL_DACL", "Left DAC"},
	{"Lineout left P switch", "LOLP_SEL_AIL", "PGAL_IN_EN"},
	{"Lineout left P switch", "LOLP_SEL_AIL_INV", "PGAL_IN_EN"},

	{"Lineout left N switch", "LOLN_SEL_AIL", "PGAL_IN_EN"},
	{"Lineout left N switch", "LOLN_SEL_DACL", "Left DAC"},
	{"Lineout left N switch", "LOLN_SEL_DACL_INV", "Left DAC"},

	{"Lineout right P switch", "LORP_SEL_DACR", "Right DAC"},
	{"Lineout right P switch", "LORP_SEL_AIR", "PGAR_IN_EN"},
	{"Lineout right P switch", "LORP_SEL_AIR_INV", "PGAR_IN_EN"},

	{"Lineout right N switch", "LORN_SEL_AIR", "PGAR_IN_EN"},
	{"Lineout right N switch", "LORN_SEL_DACR", "Right DAC"},
	{"Lineout right N switch", "LORN_SEL_DACR_INV", "Right DAC"},

	{"LOLN_OUT_EN", NULL, "Lineout left N switch"},
	{"LOLP_OUT_EN", NULL, "Lineout left P switch"},
	{"LORN_OUT_EN", NULL, "Lineout right N switch"},
	{"LORP_OUT_EN", NULL, "Lineout right P switch"},

	{"Lineout left N", NULL, "LOLN_OUT_EN"},
	{"Lineout left P", NULL, "LOLP_OUT_EN"},
	{"Lineout right N", NULL, "LORN_OUT_EN"},
	{"Lineout right P", NULL, "LORP_OUT_EN"},
};

static int aml_T9015S_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u32 val = snd_soc_read(codec, AUDIO_CONFIG_BLOCK_ENABLE);

	pr_debug("%s, format:%x, codec = %p\n", __func__, fmt, codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= (0x1 << I2S_MODE);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		val &= ~(0x1 << I2S_MODE);
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, val);

	return 0;
}

static int aml_T9015S_set_dai_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_T9015S_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aml_T9015S_audio_priv *T9015S_audio =
	    snd_soc_codec_get_drvdata(codec);

	T9015S_audio->params = params;

	return 0;
}

static int aml_T9015S_audio_set_bias_level(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->component.dapm.bias_level == SND_SOC_BIAS_OFF)
			snd_soc_cache_sync(codec);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0);
		break;

	default:
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int aml_T9015S_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u32 value = snd_soc_read(codec, AUDIO_CONFIG_BLOCK_ENABLE);
	bool Vmid_eanble = (bool)((value >> VMID_GEN_EN) & 0x1);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && !Vmid_eanble) {
		pr_info("aml_T9015S_prepare caputre!\n");
		value |= 0x1 << VMID_GEN_EN;
		snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, value);
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("aml_T9015S_prepare playback!\n");
		value &= ~(0x1 << VMID_GEN_EN);
		snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, value);
		msleep(20);
		value |= 0x1 << VMID_GEN_EN;
		snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, value);
	}

	return 0;

}

static int aml_T9015S_audio_reset(struct snd_soc_codec *codec)
{
	aml_hiu_reset_update_bits(RESET1_REGISTER, (1 << ACODEC_RESET),
					(1 << ACODEC_RESET));
	udelay(1000);
	return 0;
}

static int aml_T9015S_audio_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);
	return 0;
}

static int aml_T9015S_codec_mute_stream(struct snd_soc_dai *dai, int mute,
				      int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	u32 reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_read(codec, DAC_VOL_CTR_DAC_SOFT_MUTE);
		if (mute)
			reg |= 0x1 << DAC_SOFT_MUTE;
		else
			reg &= ~(0x1 << DAC_SOFT_MUTE);

		snd_soc_write(codec, DAC_VOL_CTR_DAC_SOFT_MUTE, reg);
	}
	return 0;
}

static int aml_T9015S_audio_probe(struct snd_soc_codec *codec)
{
	struct aml_T9015S_audio_priv *T9015S_audio = NULL;

	T9015S_audio = kzalloc(sizeof(struct aml_T9015S_audio_priv),
		GFP_KERNEL);
	if (T9015S_audio == NULL)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, T9015S_audio);

	/*reset audio codec register*/
	aml_T9015S_audio_reset(codec);
	aml_T9015S_audio_start_up(codec);
	aml_T9015S_audio_reg_init(codec);

	aml_aiu_write(AIU_ACODEC_CTRL, (1 << 4)
			   |(1 << 6)
			   |(1 << 11)
			   |(1 << 15)
			   |(2 << 2)
	);

	aml_audin_update_bits(AUDIN_SOURCE_SEL, 3, 3);

	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	T9015S_audio->codec = codec;

	return 0;
}

static int aml_T9015S_audio_remove(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015S_audio_remove!\n");
	aml_T9015S_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_T9015S_audio_suspend(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015S_audio_suspend!\n");
	aml_T9015S_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_T9015S_audio_resume(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015S_audio_resume!\n");
	aml_T9015S_audio_reset(codec);
	aml_T9015S_audio_start_up(codec);
	aml_T9015S_audio_reg_init(codec);
	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_T9015S_audio_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

#define T9015S_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define T9015S_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops T9015S_audio_aif_dai_ops = {
	.hw_params = aml_T9015S_hw_params,
	.prepare = aml_T9015S_prepare,
	.set_fmt = aml_T9015S_set_dai_fmt,
	.set_sysclk = aml_T9015S_set_dai_sysclk,
	.mute_stream = aml_T9015S_codec_mute_stream,
};

struct snd_soc_dai_driver aml_T9015S_audio_dai[] = {
	{
	 .name = "T9015S-audio-hifi",
	 .id = 0,
	 .playback = {
		      .stream_name = "HIFI Playback",
		      .channels_min = 2,
		      .channels_max = 8,
		      .rates = T9015S_AUDIO_STEREO_RATES,
		      .formats = T9015S_AUDIO_FORMATS,
		      },
	 .capture = {
		     .stream_name = "HIFI Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = T9015S_AUDIO_STEREO_RATES,
		     .formats = T9015S_AUDIO_FORMATS,
		     },
	 .ops = &T9015S_audio_aif_dai_ops,
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_aml_T9015S_audio = {
	.probe = aml_T9015S_audio_probe,
	.remove = aml_T9015S_audio_remove,
	.suspend = aml_T9015S_audio_suspend,
	.resume = aml_T9015S_audio_resume,
	.set_bias_level = aml_T9015S_audio_set_bias_level,
	.component_driver = {
		.controls = T9015S_audio_snd_controls,
		.num_controls = ARRAY_SIZE(T9015S_audio_snd_controls),
		.dapm_widgets = T9015S_audio_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(T9015S_audio_dapm_widgets),
		.dapm_routes = T9015S_audio_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(T9015S_audio_dapm_routes),
	}
};

static const struct regmap_config t9015s_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x14,
	.reg_defaults = t9015s_init_list,
	.num_reg_defaults = ARRAY_SIZE(t9015s_init_list),
	.cache_type = REGCACHE_RBTREE,
};

static int aml_T9015S_audio_codec_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res_mem;
	struct device_node *np;
	void __iomem *regs;
	struct regmap *regmap;

	dev_info(&pdev->dev, "aml_T9015S_audio_codec_probe\n");

	np = pdev->dev.of_node;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &t9015s_codec_regmap_config);

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_aml_T9015S_audio,
				     &aml_T9015S_audio_dai[0], 1);
	return ret;
}

static int aml_T9015S_audio_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static void aml_T9015S_audio_codec_shutdown(struct platform_device *pdev)
{
	struct aml_T9015S_audio_priv *aml_acodec;
	struct snd_soc_codec *codec;

	aml_acodec = platform_get_drvdata(pdev);
	codec = aml_acodec->codec;
	if (codec)
		aml_T9015S_audio_remove(codec);
}

static const struct of_device_id aml_T9015S_codec_dt_match[] = {
	{.compatible = "amlogic, aml_codec_T9015S",},
	{},
};

static struct platform_driver aml_T9015S_codec_platform_driver = {
	.driver = {
		   .name = "aml_codec_T9015S",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_T9015S_codec_dt_match,
		   },
	.probe = aml_T9015S_audio_codec_probe,
	.remove = aml_T9015S_audio_codec_remove,
	.shutdown = aml_T9015S_audio_codec_shutdown,
};

static int __init aml_T9015S_audio_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_T9015S_codec_platform_driver);
	if (ret != 0) {
		pr_err(
			"Failed to register AML T9015S codec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_T9015S_audio_modinit);

static void __exit aml_T9015S_audio_exit(void)
{
	platform_driver_unregister(&aml_T9015S_codec_platform_driver);
}

module_exit(aml_T9015S_audio_exit);

MODULE_DESCRIPTION("ASoC AML T9015S audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
