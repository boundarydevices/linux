/*
 * linux/sound/soc/codecs/aml_codec_txlx_acodec.c
 *
 * Copyright 2017 AMLogic, Inc.
 *
 * Author: Xing Wang <xing.wang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

#include "aml_codec_txlx_acodec.h"

struct txlx_acodec_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;
	struct regmap *regmap;
};

static const struct reg_default txlx_acodec_init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x3403BFFF},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x50502929},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFBFB0000},
	{LINE_OUT_CONFIG, 0x00002222},
	{POWER_CONFIG, 0x00010000},
	{ACODEC_DAC2_CONFIG, 0xFBFB0030},
	{ACODEC_DAC2_CONFIG2, 0x0},
	{ACODEC_7, 0x0}
};

static int txlx_acodec_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(txlx_acodec_init_list); i++)
		snd_soc_write(codec, txlx_acodec_init_list[i].reg,
				txlx_acodec_init_list[i].def);

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
	u32 val1 = (val & (0x1 <<  REG_DAC_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC_GAIN_SEL_1))
					>> (REG_DAC_GAIN_SEL_1 - 1);
	val = val1 | val2;

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_Gain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 add = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_read(codec, add);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
		val |= (0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << REG_DAC_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << REG_DAC_GAIN_SEL_1);
		val |= (0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_write(codec, val, add);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac2_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum DAC_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
			DAC_Gain_texts);

static const struct snd_kcontrol_new txlx_acodec_snd_controls[] = {
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

	/*DAC 2 Digital Volume control */
	SOC_DOUBLE_TLV("DAC 2 Digital Playback Volume",
			   ACODEC_DAC2_CONFIG,
			   DAC2L_VC, DAC2R_VC,
			   0xff, 0, dac2_vol_tlv),

    /*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
			   DAC_Gain_enum,
			   aml_DAC_Gain_get_enum,
			   aml_DAC_Gain_set_enum),

	/* TODO: DAC 2 extra Digital Gain control */
};

/*pgain Left Channel Input */
static const char * const linein_left_txt[] = {
	"None", "AIL1", "AIL2", "AIL3",
};

static const SOC_ENUM_SINGLE_DECL(linein_left_enum,
				  ADC_VOL_CTR_PGA_IN_CONFIG,
				  PGAL_IN_SEL, linein_left_txt);

static const struct snd_kcontrol_new lil_mux =
SOC_DAPM_ENUM("ROUTE_L", linein_left_enum);

/*pgain right Channel Input */
static const char * const linein_right_txt[] = {
	"None", "AIR1", "AIR2", "AIR3",
};

static const SOC_ENUM_SINGLE_DECL(linein_right_enum,
				  ADC_VOL_CTR_PGA_IN_CONFIG,
				  PGAR_IN_SEL, linein_right_txt);

static const struct snd_kcontrol_new lir_mux =
SOC_DAPM_ENUM("ROUTE_R", linein_right_enum);


/*line out 1 Left mux */
static const char * const out_l1l_txt[] = {
	"None", "LO1L_SEL_AIL", "LO1L_SEL_DACL", "Reserved", "LO1L_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo1l_enum, LINE_OUT_CONFIG,
				  LO1L_SEL_AIL, out_l1l_txt);

static const struct snd_kcontrol_new lo1l_mux =
SOC_DAPM_ENUM("LO1L_MUX", out_lo1l_enum);

/*line out 1 right mux */
static const char * const out_l1r_txt[] = {
	"None", "LO1R_SEL_AIR", "LO1R_SEL_DACR", "Reserved", "LO1R_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo1r_enum, LINE_OUT_CONFIG,
				  LO1R_SEL_AIR, out_l1r_txt);

static const struct snd_kcontrol_new lo1r_mux =
SOC_DAPM_ENUM("LO1R_MUX", out_lo1r_enum);

/*line out 2 left mux */
static const char * const out_l2ol_txt[] = {
	"None", "LO2L_SEL_AIL", "LO2L_SEL_DAC2L", "Reserved",
	"LO2L_SEL_DAC2R_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo2l_enum, LINE_OUT_CONFIG,
				  LO2L_SEL_AIL, out_l2ol_txt);

static const struct snd_kcontrol_new lo2l_mux =
SOC_DAPM_ENUM("LO2L_MUX", out_lo2l_enum);

/*line out 2 Right mux */
static const char * const out_lo2r_txt[] = {
	"None", "LO2R_SEL_AIR", "LO2R_SEL_DAC2R", "Reserved",
	"LO2R_SEL_DAC2L_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo2r_enum, LINE_OUT_CONFIG,
				  LO2R_SEL_AIR, out_lo2r_txt);

static const struct snd_kcontrol_new lo2r_mux =
SOC_DAPM_ENUM("LO2R_MUX", out_lo2r_enum);


static const struct snd_soc_dapm_widget txlx_acodec_dapm_widgets[] = {

	/* Input */
	SND_SOC_DAPM_INPUT("Linein left 1"),
	SND_SOC_DAPM_INPUT("Linein left 2"),
	SND_SOC_DAPM_INPUT("Linein left 3"),

	SND_SOC_DAPM_INPUT("Linein right 1"),
	SND_SOC_DAPM_INPUT("Linein right 2"),
	SND_SOC_DAPM_INPUT("Linein right 3"),

	/*PGA input */
	SND_SOC_DAPM_PGA("PGAL_IN_EN", SND_SOC_NOPM,
			 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGAR_IN_EN", SND_SOC_NOPM,
			 0, 0, NULL, 0),

	/*PGA input source select */
	SND_SOC_DAPM_MUX("Linein left switch", SND_SOC_NOPM,
			 0, 0, &lil_mux),
	SND_SOC_DAPM_MUX("Linein right switch", SND_SOC_NOPM,
			 0, 0, &lir_mux),

	/*ADC capture stream */
	SND_SOC_DAPM_ADC("Left ADC", "Capture", AUDIO_CONFIG_BLOCK_ENABLE,
			 ADCL_EN, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Capture", AUDIO_CONFIG_BLOCK_ENABLE,
			 ADCR_EN, 0),

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout 1 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 1 right"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 right"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "Playback",
			AUDIO_CONFIG_BLOCK_ENABLE,
			DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Playback",
			AUDIO_CONFIG_BLOCK_ENABLE,
			DACR_EN, 0),

	/*DAC 2 playback stream */
	SND_SOC_DAPM_DAC("Left DAC2", "Playback",
			ACODEC_DAC2_CONFIG,
			DAC2L_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC2", "Playback",
			ACODEC_DAC2_CONFIG,
			DAC2R_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LO1L_OUT_EN", SND_SOC_NOPM,
			     LO1L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO1R_OUT_EN", SND_SOC_NOPM,
			     LO1R_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO2L_OUT_EN", SND_SOC_NOPM,
			     LO2L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO2R_OUT_EN", SND_SOC_NOPM,
			     LO2R_EN, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout 1 left switch", SND_SOC_NOPM,
			 0, 0, &lo1l_mux),
	SND_SOC_DAPM_MUX("Lineout 1 right switch", SND_SOC_NOPM,
			 0, 0, &lo1r_mux),
	SND_SOC_DAPM_MUX("Lineout 2 left switch", SND_SOC_NOPM,
			 0, 0, &lo2l_mux),
	SND_SOC_DAPM_MUX("Lineout 2 right switch", SND_SOC_NOPM,
			 0, 0, &lo2r_mux),

};

static const struct snd_soc_dapm_route txlx_acodec_dapm_routes[] = {
/* Input path */
	{"Linein left switch", "AIL1", "Linein left 1"},
	{"Linein left switch", "AIL2", "Linein left 2"},
	{"Linein left switch", "AIL3", "Linein left 3"},

	{"Linein right switch", "AIR1", "Linein right 1"},
	{"Linein right switch", "AIR2", "Linein right 2"},
	{"Linein right switch", "AIR3", "Linein right 3"},

	{"PGAL_IN_EN", NULL, "Linein left switch"},
	{"PGAR_IN_EN", NULL, "Linein right switch"},

	{"Left ADC", NULL, "PGAL_IN_EN"},
	{"Right ADC", NULL, "PGAR_IN_EN"},

/*Output path*/
	{"Lineout 1 left switch", NULL, "Left DAC"},
	{"Lineout 1 left switch", NULL, "Right DAC"},
	{"Lineout 1 left switch", NULL, "PGAL_IN_EN"},

	{"Lineout 1 right switch", NULL, "Right DAC"},
	{"Lineout 1 right switch", NULL, "Left DAC"},
	{"Lineout 1 right switch", NULL, "PGAR_IN_EN"},

	{"Lineout 2 left switch", NULL, "Left DAC2"},
	{"Lineout 2 left switch", NULL, "Right DAC2"},
	{"Lineout 2 left switch", NULL, "PGAL_IN_EN"},

	{"Lineout 2 right switch", NULL, "Right DAC2"},
	{"Lineout 2 right switch", NULL, "Left DAC2"},
	{"Lineout 2 right switch", NULL, "PGAR_IN_EN"},

	{"LO1L_OUT_EN", NULL, "Lineout 1 left switch"},
	{"LO1R_OUT_EN", NULL, "Lineout 1 right switch"},
	{"LO2L_OUT_EN", NULL, "Lineout 2 left switch"},
	{"LO2R_OUT_EN", NULL, "Lineout 2 right switch"},

	{"Lineout 1 left", NULL, "LO1L_OUT_EN"},
	{"Lineout 1 right", NULL, "LO1R_OUT_EN"},
	{"Lineout 2 left", NULL, "LO2L_OUT_EN"},
	{"Lineout 2 right", NULL, "LO2R_OUT_EN"},
};

static int txlx_acodec_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
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

static int txlx_acodec_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int txlx_acodec_dai_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct txlx_acodec_priv *aml_acodec =
	    snd_soc_codec_get_drvdata(codec);

	pr_debug("%s!\n", __func__);

	aml_acodec->params = params;

	return 0;
}

static int txlx_acodec_dai_set_bias_level(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->component.dapm.bias_level == SND_SOC_BIAS_OFF) {
#if 0 /*tmp_mask_for_kernel_4_4_9*/
			codec->cache_only = false;
			codec->cache_sync = 1;
#endif
			snd_soc_cache_sync(codec);
		}
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

static int txlx_acodec_dai_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	return 0;
}

static int txlx_acodec_reset(struct snd_soc_codec *codec)
{
	aml_hiu_reset_update_bits(RESET1_REGISTER, (1 << ACODEC_RESET),
					(1 << ACODEC_RESET));
	udelay(1000);
	return 0;
}

static int txlx_acodec_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);

	return 0;
}

static int txlx_acodec_dai_mute_stream(struct snd_soc_dai *dai, int mute,
				      int stream)
{
	struct txlx_acodec_priv *aml_acodec =
		snd_soc_codec_get_drvdata(dai->codec);
	u32 reg;
	int ret = 0;

	pr_debug("%s, mute:%d\n", __func__, mute);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* DAC 1 */
		ret = regmap_read(aml_acodec->regmap,
					DAC_VOL_CTR_DAC_SOFT_MUTE,
					&reg);
		if (ret < 0)
			pr_err("Failed to read dac1\n");
		if (mute)
			reg |= DAC_SOFT_MUTE;
		else
			reg &= ~DAC_SOFT_MUTE;

		ret = regmap_write(aml_acodec->regmap,
					DAC_VOL_CTR_DAC_SOFT_MUTE,
					reg);
		if (ret < 0)
			pr_err("Failed to write dac1\n");

		/* DAC 2 */
		ret = regmap_read(aml_acodec->regmap,
					ACODEC_DAC2_CONFIG2,
					&reg);
		if (ret < 0)
			pr_err("Failed to read dac2\n");

		if (mute)
			reg |= DAC2_SOFT_MUTE;
		else
			reg &= ~DAC2_SOFT_MUTE;

		ret = regmap_write(aml_acodec->regmap,
					ACODEC_DAC2_CONFIG2,
					reg);
		if (ret < 0)
			pr_err("Failed to write dac2\n");

	}
	return 0;
}

struct snd_soc_dai_ops txlx_acodec_dai_ops = {
	.hw_params = txlx_acodec_dai_hw_params,
	.prepare = txlx_acodec_dai_prepare,
	.set_fmt = txlx_acodec_dai_set_fmt,
	.set_sysclk = txlx_acodec_dai_set_sysclk,
	.mute_stream = txlx_acodec_dai_mute_stream,
};

static int txlx_acodec_probe(struct snd_soc_codec *codec)
{
	struct txlx_acodec_priv *aml_acodec =
		snd_soc_codec_get_drvdata(codec);

	if (!aml_acodec) {
		pr_err("Failed to get txlx acodec pri\n");
		return -EINVAL;
	}
#if 0 /*tmp_mask_for_kernel_4_4*/
	snd_soc_codec_set_cache_io(codec, 32, 32, SND_SOC_REGMAP);
#endif
	/*reset audio codec register*/
	txlx_acodec_reset(codec);
	txlx_acodec_start_up(codec);
	txlx_acodec_reg_init(codec);

	aml_aiu_write(AIU_ACODEC_CTRL, (1 << 4)
			   |(1 << 6)
			   |(1 << 11)
			   |(1 << 15)
			   |(2 << 2)
	);

	aml_audin_update_bits(AUDIN_SOURCE_SEL, 3 << 16, 3 << 16);

	aml_acodec->codec = codec;

	txlx_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int txlx_acodec_remove(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	txlx_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int txlx_acodec_suspend(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	txlx_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int txlx_acodec_resume(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	txlx_acodec_reset(codec);
	txlx_acodec_start_up(codec);
	txlx_acodec_reg_init(codec);

	txlx_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_txlx_acodec = {
	.probe = txlx_acodec_probe,
	.remove = txlx_acodec_remove,
	.suspend = txlx_acodec_suspend,
	.resume = txlx_acodec_resume,
	.set_bias_level = txlx_acodec_dai_set_bias_level,
		.component_driver = {
		.controls = txlx_acodec_snd_controls,
		.num_controls = ARRAY_SIZE(txlx_acodec_snd_controls),
		.dapm_widgets = txlx_acodec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(txlx_acodec_dapm_widgets),
		.dapm_routes = txlx_acodec_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(txlx_acodec_dapm_routes),
	}
};

static const struct regmap_config txlx_acodec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x1c,
	.reg_defaults = txlx_acodec_init_list,
	.num_reg_defaults = ARRAY_SIZE(txlx_acodec_init_list),
	.cache_type = REGCACHE_RBTREE,
};

#define TXLX_ACODEC_RATES		SNDRV_PCM_RATE_8000_96000
#define TXLX_ACODEC_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_driver aml_txlx_acodec_dai = {
	.name = "txlx-acodec-hifi",
	.id = 0,
	.playback = {
	      .stream_name = "Playback",
	      .channels_min = 2,
	      .channels_max = 8,
	      .rates = TXLX_ACODEC_RATES,
	      .formats = TXLX_ACODEC_FORMATS,
	      },
	.capture = {
	     .stream_name = "Capture",
	     .channels_min = 2,
	     .channels_max = 8,
	     .rates = TXLX_ACODEC_RATES,
	     .formats = TXLX_ACODEC_FORMATS,
	     },
	.ops = &txlx_acodec_dai_ops,
};

static int aml_txlx_acodec_probe(struct platform_device *pdev)
{
	struct txlx_acodec_priv *aml_acodec;
	struct resource *res_mem;
	struct device_node *np;
	void __iomem *regs;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);

	np = pdev->dev.of_node;

	aml_acodec = devm_kzalloc(&pdev->dev, sizeof(struct txlx_acodec_priv),
				    GFP_KERNEL);
	if (!aml_acodec)
		return -ENOMEM;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	aml_acodec->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &txlx_acodec_regmap_config);

	if (IS_ERR(aml_acodec->regmap))
		return PTR_ERR(aml_acodec->regmap);

	platform_set_drvdata(pdev, aml_acodec);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_txlx_acodec,
				     &aml_txlx_acodec_dai, 1);

	return ret;
}

static int aml_txlx_acodec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static void aml_txlx_acodec_shutdown(struct platform_device *pdev)
{
	struct txlx_acodec_priv *aml_acodec;
	struct snd_soc_codec *codec;

	aml_acodec = platform_get_drvdata(pdev);
	codec = aml_acodec->codec;
	if (codec)
		txlx_acodec_remove(codec);
}

static const struct of_device_id aml_txlx_acodec_dt_match[] = {
	{.compatible = "amlogic, txlx_acodec",},
	{},
};

static struct platform_driver aml_txlx_acodec_platform_driver = {
	.driver = {
		   .name = "aml_codec_txlx_acodec",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_txlx_acodec_dt_match,
		   },
	.probe = aml_txlx_acodec_probe,
	.remove = aml_txlx_acodec_remove,
	.shutdown = aml_txlx_acodec_shutdown,
};

static int __init aml_txlx_acodec_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_txlx_acodec_platform_driver);
	if (ret != 0) {
		pr_err(
			"Failed to register AML txlx acodec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_txlx_acodec_modinit);

static void __exit aml_txlx_acodec_modexit(void)
{
	platform_driver_unregister(&aml_txlx_acodec_platform_driver);
}

module_exit(aml_txlx_acodec_modexit);

MODULE_DESCRIPTION("ASoC AML TXLX audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
