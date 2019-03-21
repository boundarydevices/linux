/*
 * sound/soc/codecs/amlogic/aml_codec_t9015.c
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
#include <linux/amlogic/media/sound/audio_iomap.h>
#include <linux/amlogic/media/sound/auge_utils.h>

#include "aml_codec_t9015.h"

struct aml_T9015_audio_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;

	/* codec is used by meson or auge arch */
	bool is_auge_arch;
	/* tocodec ctrl supports in and out data */
	bool tocodec_inout;
	/* attach which tdm when play */
	int tdmout_index;
	/* channel map */
	int ch0_sel;
	int ch1_sel;
};

static const struct reg_default t9015_init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x0000300F},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x00000000},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFBFB0000},
	{LINE_OUT_CONFIG, 0x00001111},
	{POWER_CONFIG, 0x00010000},
};

static int aml_T9015_audio_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t9015_init_list); i++)
		snd_soc_write(codec, t9015_init_list[i].reg,
				t9015_init_list[i].def);

	return 0;
}

static int aml_DAC_Gain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	u32 reg_addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_read(codec, reg_addr);
	u32 val1 = (val & (0x1 << DAC_GAIN_SEL_L))
					>> DAC_GAIN_SEL_L;
	u32 val2 = (val & (0x1 << DAC_GAIN_SEL_H))
					>> (DAC_GAIN_SEL_H);

	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;

	return 0;
}

static int aml_DAC_Gain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	u32 addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_read(codec, addr);

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

	snd_soc_write(codec, addr, val);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum DAC_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
			DAC_Gain_texts);

static const struct snd_kcontrol_new T9015_audio_snd_controls[] = {

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

/*line out Left Positive mux */
static const char * const T9015_out_lp_txt[] = {
	"None", "LOLP_SEL_DACL", "LOLP_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_lp_enum, LINE_OUT_CONFIG,
				  LOLP_SEL_DACL, T9015_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
SOC_DAPM_ENUM("ROUTE_LP_OUT", T9015_out_lp_enum);

/*line out Left Negative mux */
static const char * const T9015_out_ln_txt[] = {
	"None", "LOLN_SEL_DACL_INV", "LOLN_SEL_DACL"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_ln_enum, LINE_OUT_CONFIG,
				  LOLN_SEL_DACL_INV, T9015_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
SOC_DAPM_ENUM("ROUTE_LN_OUT", T9015_out_ln_enum);

/*line out Right Positive mux */
static const char * const T9015_out_rp_txt[] = {
	"None", "LORP_SEL_DACR", "LORP_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_rp_enum, LINE_OUT_CONFIG,
				  LORP_SEL_DACR, T9015_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
SOC_DAPM_ENUM("ROUTE_RP_OUT", T9015_out_rp_enum);

/*line out Right Negative mux */
static const char * const T9015_out_rn_txt[] = {
	"None", "LORN_SEL_DACR_INV", "LORN_SEL_DACR"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_rn_enum, LINE_OUT_CONFIG,
				  LORN_SEL_DACR_INV, T9015_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
SOC_DAPM_ENUM("ROUTE_RN_OUT", T9015_out_rn_enum);

static const struct snd_soc_dapm_widget T9015_audio_dapm_widgets[] = {

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
	SND_SOC_DAPM_OUT_DRV("LOLP_OUT_EN", AUDIO_CONFIG_BLOCK_ENABLE,
			     VMID_GEN_EN, 0, NULL, 0),
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

static const struct snd_soc_dapm_route T9015_audio_dapm_routes[] = {
    /*Output path*/
	{"Lineout left P switch", "LOLP_SEL_DACL", "Left DAC"},
	{"Lineout left P switch", "LOLP_SEL_DACL_INV", "Left DAC"},

	{"Lineout left N switch", "LOLN_SEL_DACL_INV", "Left DAC"},
	{"Lineout left N switch", "LOLN_SEL_DACL", "Left DAC"},

	{"Lineout right P switch", "LORP_SEL_DACR", "Right DAC"},
	{"Lineout right P switch", "LORP_SEL_DACR_INV", "Right DAC"},

	{"Lineout right N switch", "LORN_SEL_DACR_INV", "Right DAC"},
	{"Lineout right N switch", "LORN_SEL_DACR", "Right DAC"},

	{"LOLN_OUT_EN", NULL, "Lineout left N switch"},
	{"LOLP_OUT_EN", NULL, "Lineout left P switch"},
	{"LORN_OUT_EN", NULL, "Lineout right N switch"},
	{"LORP_OUT_EN", NULL, "Lineout right P switch"},

	{"Lineout left N", NULL, "LOLN_OUT_EN"},
	{"Lineout left P", NULL, "LOLP_OUT_EN"},
	{"Lineout right N", NULL, "LORN_OUT_EN"},
	{"Lineout right P", NULL, "LORP_OUT_EN"},
};

static int aml_T9015_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
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

static int aml_T9015_set_dai_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_T9015_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aml_T9015_audio_priv *T9015_audio =
	    snd_soc_codec_get_drvdata(codec);

	T9015_audio->params = params;

	return 0;
}

static int aml_T9015_audio_set_bias_level(struct snd_soc_codec *codec,
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
		snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0);
		break;

	default:
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int aml_T9015_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	/*struct snd_soc_codec *codec = dai->codec;*/
	return 0;

}

static int aml_T9015_audio_reset(struct snd_soc_codec *codec)
{
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_codec_get_drvdata(codec);

	if (T9015_audio && T9015_audio->is_auge_arch)
		auge_acodec_reset();
	else
		aml_cbus_update_bits(RESET1_REGISTER,
				(1 << ACODEC_RESET),
				(1 << ACODEC_RESET));

	udelay(1000);

	return 0;
}

static int aml_T9015_audio_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);

	return 0;
}

static int aml_T9015_codec_mute_stream(struct snd_soc_dai *dai, int mute,
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

static int aml_T9015_audio_probe(struct snd_soc_codec *codec)
{
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_codec_get_drvdata(codec);
	if (!T9015_audio) {
		pr_info("T9015_audio is null!\n");
		return -ENODEV;
	}
	/*reset audio codec register*/
	aml_T9015_audio_reset(codec);
	aml_T9015_audio_start_up(codec);
	aml_T9015_audio_reg_init(codec);

	if (T9015_audio && T9015_audio->is_auge_arch) {
		if (T9015_audio->tocodec_inout)
			auge_toacodec_ctrl_ext(
				T9015_audio->tdmout_index,
				T9015_audio->ch0_sel,
				T9015_audio->ch1_sel);
		else
			auge_toacodec_ctrl(T9015_audio->tdmout_index);
	} else
		aml_write_cbus(AIU_ACODEC_CTRL,
				(1 << 4)
				|(1 << 6)
				|(1 << 11)
				|(1 << 15)
				|(2 << 2));

	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	T9015_audio->codec = codec;

	return 0;
}

static int aml_T9015_audio_remove(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015_audio_remove!\n");

	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int aml_T9015_audio_suspend(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015_audio_suspend!\n");

	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int aml_T9015_audio_resume(struct snd_soc_codec *codec)
{
	pr_info("aml_T9015_audio_resume!\n");

	aml_T9015_audio_reset(codec);
	aml_T9015_audio_start_up(codec);
	aml_T9015_audio_reg_init(codec);
	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

#define T9015_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define T9015_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops T9015_audio_aif_dai_ops = {
	.hw_params = aml_T9015_hw_params,
	.prepare = aml_T9015_prepare,
	.set_fmt = aml_T9015_set_dai_fmt,
	.set_sysclk = aml_T9015_set_dai_sysclk,
	.mute_stream = aml_T9015_codec_mute_stream,
};

struct snd_soc_dai_driver aml_T9015_audio_dai[] = {
	{
	 .name = "T9015-audio-hifi",
	 .id = 0,
	 .playback = {
		      .stream_name = "HIFI Playback",
		      .channels_min = 2,
		      .channels_max = 8,
		      .rates = T9015_AUDIO_STEREO_RATES,
		      .formats = T9015_AUDIO_FORMATS,
		      },
	 .capture = {
			 .stream_name = "HIFI Capture",
			 .channels_min = 2,
			 .channels_max = 8,
			 .rates = T9015_AUDIO_STEREO_RATES,
			 .formats = T9015_AUDIO_FORMATS,
			 },
	 .ops = &T9015_audio_aif_dai_ops,
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_aml_T9015_audio = {
	.probe = aml_T9015_audio_probe,
	.remove = aml_T9015_audio_remove,
	.suspend = aml_T9015_audio_suspend,
	.resume = aml_T9015_audio_resume,
	.set_bias_level = aml_T9015_audio_set_bias_level,
	.component_driver = {
		.controls = T9015_audio_snd_controls,
		.num_controls = ARRAY_SIZE(T9015_audio_snd_controls),
		.dapm_widgets = T9015_audio_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(T9015_audio_dapm_widgets),
		.dapm_routes = T9015_audio_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(T9015_audio_dapm_routes),
	}
};

static const struct regmap_config t9015_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x14,
	.reg_defaults = t9015_init_list,
	.num_reg_defaults = ARRAY_SIZE(t9015_init_list),
	.cache_type = REGCACHE_RBTREE,
};

static int aml_T9015_audio_codec_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res_mem;
	void __iomem *regs;
	struct regmap *regmap;
	struct aml_T9015_audio_priv *T9015_audio;

	dev_info(&pdev->dev, "aml_T9015_audio_codec_probe\n");

	T9015_audio = devm_kzalloc(&pdev->dev,
					sizeof(struct aml_T9015_audio_priv),
					GFP_KERNEL);
	if (!T9015_audio)
		return -ENOMEM;
	platform_set_drvdata(pdev, T9015_audio);

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &t9015_codec_regmap_config);

	T9015_audio->is_auge_arch = of_property_read_bool(
			pdev->dev.of_node,
			"is_auge_used");

	T9015_audio->tocodec_inout = of_property_read_bool(
			pdev->dev.of_node,
			"tocodec_inout");

	of_property_read_u32(
			pdev->dev.of_node,
			"tdmout_index",
			&T9015_audio->tdmout_index);

	of_property_read_u32(
			pdev->dev.of_node,
			"ch0_sel",
			&T9015_audio->ch0_sel);

	of_property_read_u32(
			pdev->dev.of_node,
			"ch1_sel",
			&T9015_audio->ch1_sel);

	pr_info("T9015 acodec used by %s, tdmout:%d\n",
		T9015_audio->is_auge_arch ? "auge" : "meson",
		T9015_audio->tdmout_index);

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_aml_T9015_audio,
				     &aml_T9015_audio_dai[0], 1);
	return ret;
}

static int aml_T9015_audio_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static void aml_T9015_audio_codec_shutdown(struct platform_device *pdev)
{
	struct aml_T9015_audio_priv *aml_acodec;
	struct snd_soc_codec *codec;

	aml_acodec = platform_get_drvdata(pdev);
	codec = aml_acodec->codec;
	if (codec)
		aml_T9015_audio_remove(codec);
}

static const struct of_device_id aml_T9015_codec_dt_match[] = {
	{.compatible = "amlogic, aml_codec_T9015",},
	{},
};

static struct platform_driver aml_T9015_codec_platform_driver = {
	.driver = {
		   .name = "aml_codec_T9015",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_T9015_codec_dt_match,
		   },
	.probe = aml_T9015_audio_codec_probe,
	.remove = aml_T9015_audio_codec_remove,
	.shutdown = aml_T9015_audio_codec_shutdown,
};

static int __init aml_T9015_audio_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_T9015_codec_platform_driver);
	if (ret != 0) {
		pr_err(
			"Failed to register AML T9015 codec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_T9015_audio_modinit);

static void __exit aml_T9015_audio_exit(void)
{
	platform_driver_unregister(&aml_T9015_codec_platform_driver);
}

module_exit(aml_T9015_audio_exit);

MODULE_DESCRIPTION("ASoC AML T9015 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
