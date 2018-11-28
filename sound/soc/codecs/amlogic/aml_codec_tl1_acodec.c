/*
 * linux/sound/soc/codecs/aml_codec_tl1_acodec.c
 *
 * Copyright 2017 AMLogic, Inc.
 *
 * Author: shuyu.li <shuyu.li@amlogic.com>
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
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/auge_utils.h>

#include "../../../soc/amlogic/auge/iomap.h"
#include "../../../soc/amlogic/auge/regs.h"
#include "aml_codec_tl1_acodec.h"

struct tl1_acodec_chipinfo {
	int id;
	bool is_bclk_cap_inv;	//default true
	bool is_bclk_o_inv;		//default false
	bool is_lrclk_inv;		//default false
	bool is_dac_phase_differ_exist;
	bool is_adc_phase_differ_exist;
	int mclk_sel;
};

struct tl1_acodec_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;
	struct regmap *regmap;
	struct tl1_acodec_chipinfo *chipinfo;
	int tdmout_index;
	int dat0_ch_sel;
	int dat1_ch_sel;

	int tdmin_index;
	int adc_output_sel;
	//int input_data_sel;	//tdmouta,tdmoutb,
	//tdmouta,tdmoutb,tdmoutc,tdmina,tdminb,tdminc
	//int clk_sel;
	//tdmouta,tdmoutb,tdmoutc,none,tdmina,tdminb,tdminc
	int dac1_input_sel;
	int dac2_input_sel;
};

static const struct reg_default tl1_acodec_init_list[] = {
	{ACODEC_0, 0x3403BFCF},
	{ACODEC_1, 0x50503030},
	{ACODEC_2, 0xFBFB0000},
	{ACODEC_3, 0x00002222},
	{ACODEC_4, 0x00010000},
	{ACODEC_5, 0xFBFB0033},
	{ACODEC_6, 0x0},
	{ACODEC_7, 0x0}
};
static struct tl1_acodec_chipinfo tl1_acodec_cinfo = {
	.id = 0,
	.is_bclk_cap_inv = true,	//default  true
	.is_bclk_o_inv = false,		//default  false
	.is_lrclk_inv = false,

	.is_dac_phase_differ_exist = false,
	.is_adc_phase_differ_exist = true,
	//if is_adc_phase_differ=true,modified tdmin_in_rev_ws,revert ws(lrclk);
	//0 :disable; 1: enable;
	//.mclk_sel = 1,
};

static int tl1_acodec_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tl1_acodec_init_list); i++)
		snd_soc_write(codec, tl1_acodec_init_list[i].reg,
				tl1_acodec_init_list[i].def);

	return 0;
}

static int aml_DAC_Gain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);

	u32 reg_addr = ACODEC_1;
	u32 val = snd_soc_read(codec, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC_GAIN_SEL_1))
					>> (REG_DAC_GAIN_SEL_1);
	val = val1 | (val2<<1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_Gain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ACODEC_1;
	u32 val = snd_soc_read(codec, reg_addr);

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

	snd_soc_write(codec, reg_addr, val);
	return 0;
}

static int aml_DAC2_Gain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);

	u32 reg_addr = ACODEC_7;
	u32 val = snd_soc_read(codec, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_1))
					>> (REG_DAC2_GAIN_SEL_1);
	val = val1 | (val2<<1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC2_Gain_set_enum(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ACODEC_7;
	u32 val = snd_soc_read(codec, reg_addr);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
		val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << REG_DAC2_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << REG_DAC2_GAIN_SEL_1);
		val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_write(codec, reg_addr, val);
	return 0;
}


static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac2_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };
static const char *const DAC2_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum DAC_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
			DAC_Gain_texts);
static const struct soc_enum DAC2_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC2_Gain_texts),
			DAC2_Gain_texts);

static const struct snd_kcontrol_new tl1_acodec_snd_controls[] = {
	/*PGA_IN Gain */
	SOC_DOUBLE_TLV("PGA IN Gain", ACODEC_1,
		       PGAL_IN_GAIN, PGAR_IN_GAIN,
		       0x1f, 0, pga_in_tlv),

	/*ADC Digital Volume control */
	SOC_DOUBLE_TLV("ADC Digital Capture Volume", ACODEC_1,
		       ADCL_VC, ADCR_VC,
		       0x7f, 0, adc_vol_tlv),

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume",
			   ACODEC_2,
			   DACL_VC, DACR_VC,
			   0xff, 0, dac_vol_tlv),

	/*DAC 2 Digital Volume control */
	SOC_DOUBLE_TLV("DAC 2 Digital Playback Volume",
			   ACODEC_5,
			   DAC2L_VC, DAC2R_VC,
			   0xff, 0, dac2_vol_tlv),

    /*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
			   DAC_Gain_enum,
			   aml_DAC_Gain_get_enum,
			   aml_DAC_Gain_set_enum),

	/* TODO: DAC 2 extra Digital Gain control */
	SOC_ENUM_EXT("DAC2 Extra Digital Gain",
			   DAC2_Gain_enum,
			   aml_DAC2_Gain_get_enum,
			   aml_DAC2_Gain_set_enum),
};

/*pgain Left Channel Input */
static const char * const linein_left_txt[] = {
	"None", "AIL1", "AIL2", "AIL3", "AIL4",
};

static const SOC_ENUM_SINGLE_DECL(linein_left_enum,
				  ACODEC_1,
				  PGAL_IN_SEL, linein_left_txt);

static const struct snd_kcontrol_new lil_mux =
SOC_DAPM_ENUM("ROUTE_L", linein_left_enum);

/*pgain right Channel Input */
static const char * const linein_right_txt[] = {
	"None", "AIR1", "AIR2", "AIR3", "AIR4",
};

static const SOC_ENUM_SINGLE_DECL(linein_right_enum,
				  ACODEC_1,
				  PGAR_IN_SEL, linein_right_txt);

static const struct snd_kcontrol_new lir_mux =
SOC_DAPM_ENUM("ROUTE_R", linein_right_enum);


/*line out 1 Left mux */
static const char * const out_lo1l_txt[] = {
	"None", "LO1L_SEL_INL", "LO1L_SEL_DACL", "Reserved", "LO1L_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo1l_enum, ACODEC_3,
				  LO1L_SEL_INL, out_lo1l_txt);

static const struct snd_kcontrol_new lo1l_mux =
SOC_DAPM_ENUM("LO1L_MUX", out_lo1l_enum);

/*line out 1 right mux */
static const char * const out_lo1r_txt[] = {
	"None", "LO1R_SEL_INR", "LO1R_SEL_DACR", "Reserved", "LO1R_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo1r_enum, ACODEC_3,
				  LO1R_SEL_INR, out_lo1r_txt);

static const struct snd_kcontrol_new lo1r_mux =
SOC_DAPM_ENUM("LO1R_MUX", out_lo1r_enum);

/*line out 2 left mux */
static const char * const out_lo2l_txt[] = {
	"None", "LO2L_SEL_INL", "LO2L_SEL_DAC2L", "Reserved",
	"LO2L_SEL_DAC2R_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo2l_enum, ACODEC_3,
				  LO2L_SEL_INL, out_lo2l_txt);

static const struct snd_kcontrol_new lo2l_mux =
SOC_DAPM_ENUM("LO2L_MUX", out_lo2l_enum);

/*line out 2 Right mux */
static const char * const out_lo2r_txt[] = {
	"None", "LO2R_SEL_INR", "LO2R_SEL_DAC2R", "Reserved",
	"LO2R_SEL_DAC2L_INV"
};

static const SOC_ENUM_SINGLE_DECL(out_lo2r_enum, ACODEC_3,
				  LO2R_SEL_INR, out_lo2r_txt);

static const struct snd_kcontrol_new lo2r_mux =
SOC_DAPM_ENUM("LO2R_MUX", out_lo2r_enum);


static const struct snd_soc_dapm_widget tl1_acodec_dapm_widgets[] = {

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
	SND_SOC_DAPM_ADC("Left ADC", "Capture", ACODEC_0,
			 ADCL_EN, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Capture", ACODEC_0,
			 ADCR_EN, 0),

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout 1 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 1 right"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 right"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "Playback",
			ACODEC_0,
			DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Playback",
			ACODEC_0,
			DACR_EN, 0),

	/*DAC 2 playback stream */
	SND_SOC_DAPM_DAC("Left DAC2", "Playback",
			ACODEC_5,
			DAC2L_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC2", "Playback",
			ACODEC_5,
			DAC2R_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LO1L_OUT_EN", ACODEC_0,
			     LO1L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO1R_OUT_EN", ACODEC_0,
			     LO1R_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO2L_OUT_EN", ACODEC_0,
			     LO2L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LO2R_OUT_EN", ACODEC_0,
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

static const struct snd_soc_dapm_route tl1_acodec_dapm_routes[] = {
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

static int tl1_acodec_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	u32 val = snd_soc_read(codec, ACODEC_0);

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

	snd_soc_write(codec, ACODEC_0, val);

	return 0;
}

static int tl1_acodec_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tl1_acodec_dai_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct tl1_acodec_priv *aml_acodec =
	    snd_soc_codec_get_drvdata(codec);

	pr_debug("%s!\n", __func__);

	aml_acodec->params = params;

	return 0;
}

static int tl1_acodec_dai_set_bias_level(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->component.dapm.bias_level == SND_SOC_BIAS_OFF) {
			snd_soc_cache_sync(codec);
		}
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, ACODEC_0, 0);
		break;

	default:
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int tl1_acodec_dai_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	return 0;
}

//TODO, need to check
static int tl1_acodec_reset(struct snd_soc_codec *codec)
{
	struct tl1_acodec_priv *tl1_acodec =
			snd_soc_codec_get_drvdata(codec);
	if (tl1_acodec)
		auge_acodec_reset();
	udelay(1000);
	return 0;
}
//TODO, need to check
static int tl1_acodec_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, ACODEC_0, 0xF000);
	msleep(200);
	snd_soc_write(codec, ACODEC_0, 0xB000);

	return 0;
}

static int tl1_acodec_dai_mute_stream(struct snd_soc_dai *dai, int mute,
				      int stream)
{
	struct tl1_acodec_priv *aml_acodec =
		snd_soc_codec_get_drvdata(dai->codec);
	u32 reg_val;
	int ret;

	pr_debug("%s, mute:%d\n", __func__, mute);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* DAC 1 */
		ret = regmap_read(aml_acodec->regmap,
					ACODEC_2,
					&reg_val);
		if (mute)
			reg_val |= (0x1<<DAC_SOFT_MUTE);
		else
			reg_val &= ~(0x1<<DAC_SOFT_MUTE);

		ret = regmap_write(aml_acodec->regmap,
					ACODEC_2,
					reg_val);

		/* DAC 2 */
		ret = regmap_read(aml_acodec->regmap,
					ACODEC_6,
					&reg_val);
		if (mute)
			reg_val |= (0x1<<DAC2_SOFT_MUTE);
		else
			reg_val &= ~(0x1<<DAC2_SOFT_MUTE);

		ret = regmap_write(aml_acodec->regmap,
					ACODEC_6,
					reg_val);
	}

	return 0;
}

struct snd_soc_dai_ops tl1_acodec_dai_ops = {
	.hw_params = tl1_acodec_dai_hw_params,
	.prepare = tl1_acodec_dai_prepare,
	.set_fmt = tl1_acodec_dai_set_fmt,
	.set_sysclk = tl1_acodec_dai_set_sysclk,
	.mute_stream = tl1_acodec_dai_mute_stream,
};

static int tl1_acodec_probe(struct snd_soc_codec *codec)
{
	struct tl1_acodec_priv *aml_acodec =
		snd_soc_codec_get_drvdata(codec);

	if (!aml_acodec) {
		pr_err("Failed to get tl1 acodec pri\n");
		return -EINVAL;
	}

	/*reset audio codec register*/
	tl1_acodec_reset(codec);
	tl1_acodec_start_up(codec);
	tl1_acodec_reg_init(codec);

	aml_acodec->codec = codec;
	tl1_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	pr_info("%s\n", __func__);
	return 0;
}

static int tl1_acodec_remove(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	tl1_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int tl1_acodec_suspend(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	tl1_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int tl1_acodec_resume(struct snd_soc_codec *codec)
{
	pr_info("%s!\n", __func__);

	tl1_acodec_reset(codec);
	tl1_acodec_start_up(codec);
	tl1_acodec_reg_init(codec);

	tl1_acodec_dai_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tl1_acodec = {
	.probe = tl1_acodec_probe,
	.remove = tl1_acodec_remove,
	.suspend = tl1_acodec_suspend,
	.resume = tl1_acodec_resume,
	.set_bias_level = tl1_acodec_dai_set_bias_level,
		.component_driver = {
		.controls = tl1_acodec_snd_controls,
		.num_controls = ARRAY_SIZE(tl1_acodec_snd_controls),
		.dapm_widgets = tl1_acodec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tl1_acodec_dapm_widgets),
		.dapm_routes = tl1_acodec_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(tl1_acodec_dapm_routes),
	}
};

static const struct regmap_config tl1_acodec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x1c,
	.reg_defaults = tl1_acodec_init_list,
	.num_reg_defaults = ARRAY_SIZE(tl1_acodec_init_list),
	.cache_type = REGCACHE_RBTREE,
};

#define TL1_ACODEC_RATES		SNDRV_PCM_RATE_8000_96000
#define TL1_ACODEC_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_driver aml_tl1_acodec_dai = {
	.name = "tl1-acodec-hifi",
	.id = 0,
	.playback = {
	      .stream_name = "Playback",
	      .channels_min = 2,
	      .channels_max = 8,
	      .rates = TL1_ACODEC_RATES,
	      .formats = TL1_ACODEC_FORMATS,
	      },
	.capture = {
	     .stream_name = "Capture",
	     .channels_min = 2,
	     .channels_max = 8,
	     .rates = TL1_ACODEC_RATES,
	     .formats = TL1_ACODEC_FORMATS,
	     },
	.ops = &tl1_acodec_dai_ops,
};
static int tl1_acodec_set_toacodec(struct tl1_acodec_priv *aml_acodec)
{
	int dat0_sel, dat1_sel, lrclk_sel, bclk_sel, mclk_sel;
	unsigned int update_bits_msk = 0x0, update_bits = 0x0;

	update_bits_msk = 0x80FF7777;
	if (aml_acodec->chipinfo->is_bclk_cap_inv == true)
		update_bits |= (0x1<<9);
	if (aml_acodec->chipinfo->is_bclk_o_inv == true)
		update_bits |= (0x1<<8);
	if (aml_acodec->chipinfo->is_lrclk_inv == true)
		update_bits |= (0x1<<10);

	dat0_sel = (aml_acodec->tdmout_index<<2)+aml_acodec->dat0_ch_sel;
	dat0_sel = dat0_sel<<16;
	dat1_sel = (aml_acodec->tdmout_index<<2)+aml_acodec->dat1_ch_sel;
	dat1_sel = dat1_sel<<20;
	lrclk_sel = (aml_acodec->tdmout_index)<<12;
	bclk_sel = (aml_acodec->tdmout_index)<<4;

	//mclk_sel = aml_acodec->chipinfo->mclk_sel;
	mclk_sel = aml_acodec->tdmin_index;

	update_bits |= dat0_sel|dat1_sel|lrclk_sel|bclk_sel|mclk_sel;
	update_bits |= 0x1<<31;

	audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, update_bits_msk,
						update_bits);
	pr_info("%s, is_bclk_cap_inv %s\n", __func__,
		aml_acodec->chipinfo->is_bclk_cap_inv?"true":"false");
	pr_info("%s, is_bclk_o_inv %s\n", __func__,
		aml_acodec->chipinfo->is_bclk_o_inv?"true":"false");
	pr_info("%s, is_lrclk_inv %s\n", __func__,
		aml_acodec->chipinfo->is_lrclk_inv?"true":"false");
	pr_info("%s read EE_AUDIO_TOACODEC_CTRL0=0x%08x\n", __func__,
		audiobus_read(EE_AUDIO_TOACODEC_CTRL0));

	return 0;
}

static int aml_tl1_acodec_probe(struct platform_device *pdev)
{
	struct tl1_acodec_priv *aml_acodec;
	struct tl1_acodec_chipinfo *p_chipinfo;
	struct resource *res_mem;
	struct device_node *np;
	void __iomem *regs;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);

	np = pdev->dev.of_node;

	aml_acodec = devm_kzalloc(&pdev->dev, sizeof(struct tl1_acodec_priv),
				    GFP_KERNEL);
	if (!aml_acodec)
		return -ENOMEM;
	/* match data */
	p_chipinfo = (struct tl1_acodec_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_chipinfo)
		dev_warn_once(&pdev->dev, "check whether to update tl1_acodec_chipinfo\n");

	aml_acodec->chipinfo = p_chipinfo;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	aml_acodec->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tl1_acodec_regmap_config);
	if (IS_ERR(aml_acodec->regmap))
		return PTR_ERR(aml_acodec->regmap);

	of_property_read_u32(
			pdev->dev.of_node,
			"tdmout_index",
			&aml_acodec->tdmout_index);
	pr_info("aml_tl1_acodec tdmout_index=%d\n",
		aml_acodec->tdmout_index);

	of_property_read_u32(
			pdev->dev.of_node,
			"tdmin_index",
			&aml_acodec->tdmin_index);
	pr_info("aml_tl1_acodec tdmin_index=%d\n",
		aml_acodec->tdmin_index);

	tl1_acodec_set_toacodec(aml_acodec);

	platform_set_drvdata(pdev, aml_acodec);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_tl1_acodec,
				     &aml_tl1_acodec_dai, 1);
	if (ret)
		pr_info("%s call snd_soc_register_codec error\n", __func__);
	else
		pr_info("%s over\n", __func__);
	pr_info("%s read EE_AUDIO_TOACODEC_CTRL0=0x%08x\n", __func__,
		audiobus_read(EE_AUDIO_TOACODEC_CTRL0));
	return ret;
}

static int aml_tl1_acodec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static void aml_tl1_acodec_shutdown(struct platform_device *pdev)
{
	struct tl1_acodec_priv *aml_acodec;
	struct snd_soc_codec *codec;

	aml_acodec = platform_get_drvdata(pdev);
	codec = aml_acodec->codec;
	if (codec)
		tl1_acodec_remove(codec);
}

static const struct of_device_id aml_tl1_acodec_dt_match[] = {
	{
		.compatible = "amlogic, tl1_acodec",
		.data = &tl1_acodec_cinfo,
	},
	{},
};

static struct platform_driver aml_tl1_acodec_platform_driver = {
	.driver = {
		   .name = "tl1_acodec",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_tl1_acodec_dt_match,
		   },
	.probe = aml_tl1_acodec_probe,
	.remove = aml_tl1_acodec_remove,
	.shutdown = aml_tl1_acodec_shutdown,
};

static int __init aml_tl1_acodec_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_tl1_acodec_platform_driver);
	if (ret != 0) {
		pr_err(
			"Failed to register AML tl1 acodec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_tl1_acodec_modinit);

static void __exit aml_tl1_acodec_modexit(void)
{
	platform_driver_unregister(&aml_tl1_acodec_platform_driver);
}

module_exit(aml_tl1_acodec_modexit);

MODULE_DESCRIPTION("ASoC AML TL1 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
