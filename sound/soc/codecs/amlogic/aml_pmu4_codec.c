/*
 * sound/soc/codecs/amlogic/aml_pmu4_codec.c
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
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/amlogic/aml_pmu4.h>
#include "aml_pmu4_codec.h"

#ifdef CONFIG_USE_OF
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <mach/pinmux.h>
#include <plat/io.h>
#endif

struct aml_pmu4_audio_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;
};

struct pmu4_audio_init_reg {
	u8 reg;
	u16 val;
};

#define AML1220_PMU_CTR_04 0x05

static struct pmu4_audio_init_reg init_list[] = {
	{PMU4_BLOCK_ENABLE, 0xBCFF},
	{PMU4_AUDIO_CONFIG, 0x3400},
	{PMU4_PGA_IN_CONFIG, 0x2929},
	{PMU4_ADC_VOL_CTR, 0x5050},
	{PMU4_DAC_SOFT_MUTE, 0x0000},
	{PMU4_DAC_VOL_CTR, 0xFFFF},
	{PMU4_LINE_OUT_CONFIG, 0x4242},

};

#define PMU4_AUDIO_INIT_REG_LEN ARRAY_SIZE(init_list)

static int aml_pmu4_audio_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < PMU4_AUDIO_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);

	return 0;
}

static unsigned int aml_pmu4_audio_read(struct snd_soc_codec *codec,
					unsigned int reg)
{
	u16 pmu4_audio_reg;
	u16 val;

	pmu4_audio_reg = PMU4_AUDIO_BASE + reg;
	aml_pmu4_read16(pmu4_audio_reg, &val);

	return val;

}

static int aml_pmu4_audio_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int val)
{
	u16 pmu4_audio_reg;

	pmu4_audio_reg = PMU4_AUDIO_BASE + reg;
	aml_pmu4_write16(pmu4_audio_reg, val);

	return 0;
}

static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const struct snd_kcontrol_new pmu4_audio_snd_controls[] = {
	/*PGA_IN Gain */
	SOC_DOUBLE_TLV("PGA IN Gain", PMU4_PGA_IN_CONFIG,
		       PMU4_PGAL_IN_GAIN, PMU4_PGAR_IN_GAIN,
		       0x1f, 0, pga_in_tlv),

	/*ADC Digital Volume control */
	SOC_DOUBLE_TLV("ADC Digital Capture Volume", PMU4_ADC_VOL_CTR,
		       PMU4_ADCL_VOL_CTR, PMU4_ADCR_VOL_CTR,
		       0x7f, 0, adc_vol_tlv),

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume", PMU4_DAC_VOL_CTR,
		       PMU4_DACL_VOL_CTR, PMU4_DACR_VOL_CTR,
		       0xff, 0, dac_vol_tlv),

};

/*pgain Left Channel Input */
static const char * const pmu4_pgain_left_txt[] = {
	"None", "AIL1", "AIL2", "AIL3", "AIL4"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_pgain_left_enum, PMU4_PGA_IN_CONFIG,
				  PMU4_PGAL_IN_SEL, pmu4_pgain_left_txt);

static const struct snd_kcontrol_new pgain_ln_mux =
SOC_DAPM_ENUM("ROUTE_L", pmu4_pgain_left_enum);

/*pgain right Channel Input */
static const char * const pmu4_pgain_right_txt[] = {
	"None", "AIR1", "AIR2", "AIR3", "AIR4"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_pgain_right_enum, PMU4_PGA_IN_CONFIG,
				  PMU4_PGAR_IN_SEL, pmu4_pgain_right_txt);

static const struct snd_kcontrol_new pgain_rn_mux =
SOC_DAPM_ENUM("ROUTE_R", pmu4_pgain_right_enum);

/*line out Left Positive mux */
static const char * const pmu4_out_lp_txt[] = {
	"None", "LOLP_SEL_AIL_INV", "LOLP_SEL_AIL", "Reserved", "LOLP_SEL_DACL"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_out_lp_enum, PMU4_LINE_OUT_CONFIG,
				  PMU4_LOLP_SEL_SHIFT, pmu4_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
SOC_DAPM_ENUM("ROUTE_LP_OUT", pmu4_out_lp_enum);

/*line out Left Negative mux */
static const char * const pmu4_out_ln_txt[] = {
	"None", "LOLN_SEL_AIL", "LOLN_SEL_DACL", "Reserved", "LOLN_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_out_ln_enum, PMU4_LINE_OUT_CONFIG,
				  PMU4_LOLN_SEL_SHIFT, pmu4_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
SOC_DAPM_ENUM("ROUTE_LN_OUT", pmu4_out_ln_enum);

/*line out Right Positive mux */
static const char * const pmu4_out_rp_txt[] = {
	"None", "LORP_SEL_AIR_INV", "LORP_SEL_AIR", "Reserved", "LORP_SEL_DACR"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_out_rp_enum, PMU4_LINE_OUT_CONFIG,
				  PMU4_LORP_SEL_SHIFT, pmu4_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
SOC_DAPM_ENUM("ROUTE_RP_OUT", pmu4_out_rp_enum);

/*line out Right Negative mux */
static const char * const pmu4_out_rn_txt[] = {
	"None", "LORN_SEL_AIR", "LORN_SEL_DACR", "Reserved", "LORN_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(pmu4_out_rn_enum, PMU4_LINE_OUT_CONFIG,
				  PMU4_LORN_SEL_SHIFT, pmu4_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
SOC_DAPM_ENUM("ROUTE_RN_OUT", pmu4_out_rn_enum);

static const struct snd_soc_dapm_widget pmu4_audio_dapm_widgets[] = {

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
	SND_SOC_DAPM_PGA("PGAL_IN_EN", PMU4_BLOCK_ENABLE,
			 PMU4_PGAL_IN_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGAR_IN_EN", PMU4_BLOCK_ENABLE,
			 PMU4_PGAR_IN_EN, 0, NULL, 0),

	/*PGA input source select */
	SND_SOC_DAPM_MUX("Linein left switch", SND_SOC_NOPM,
			 0, 0, &pgain_ln_mux),
	SND_SOC_DAPM_MUX("Linein right switch", SND_SOC_NOPM,
			 0, 0, &pgain_rn_mux),

	/*ADC capture stream */
	SND_SOC_DAPM_ADC("Left ADC", "HIFI Capture", PMU4_BLOCK_ENABLE,
			 PMU4_ADCL_EN, 0),
	SND_SOC_DAPM_ADC("Right ADC", "HIFI Capture", PMU4_BLOCK_ENABLE,
			 PMU4_ADCR_EN, 0),

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout left N"),
	SND_SOC_DAPM_OUTPUT("Lineout left P"),
	SND_SOC_DAPM_OUTPUT("Lineout right N"),
	SND_SOC_DAPM_OUTPUT("Lineout right P"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback", PMU4_BLOCK_ENABLE,
			 PMU4_DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback", PMU4_BLOCK_ENABLE,
			 PMU4_DACR_EN, 0),

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

static const struct snd_soc_dapm_route pmu4_audio_dapm_routes[] = {
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

static int aml_pmu4_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		snd_soc_update_bits(codec, PMU4_AUDIO_CONFIG,
				    PMU4_I2S_MODE, PMU4_I2S_MODE);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		snd_soc_update_bits(codec, PMU4_AUDIO_CONFIG, PMU4_I2S_MODE, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int aml_pmu4_set_dai_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_pmu4_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aml_pmu4_audio_priv *pmu4_audio =
	    snd_soc_codec_get_drvdata(codec);

	pmu4_audio->params = params;

	return 0;
}

static int aml_pmu4_audio_set_bias_level(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (codec->component.dapm.bias_level == SND_SOC_BIAS_OFF) {
#if 0 /*tmp_mask_for_kernel_4_4*/
			codec->cache_only = false;
			codec->cache_sync = 1;
#endif
			snd_soc_cache_sync(codec);
		}
		break;

	case SND_SOC_BIAS_OFF:

		break;

	default:
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int aml_pmu4_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	/*struct snd_soc_codec *codec = dai->codec;*/
	return 0;

}

static int aml_pmu4_audio_reset(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, PMU4_SOFT_RESET, 0xF);
	snd_soc_write(codec, PMU4_SOFT_RESET, 0x0);
	udelay(10*1000);
	return 0;
}

static int aml_pmu4_audio_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, PMU4_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_write(codec, PMU4_BLOCK_ENABLE, 0xB000);
	return 0;
}

static int aml_pmu4_audio_power_init(void)
{
	uint8_t val = 0;

	/*set audio ldo supply en in,reg:0x05,bit 0*/
	aml_pmu4_read(AML1220_PMU_CTR_04, &val);
	aml_pmu4_write(AML1220_PMU_CTR_04, val | 0x01);
	return 0;

}

static int aml_pmu4_codec_mute_stream(struct snd_soc_dai *dai, int mute,
				      int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_read(codec, PMU4_DAC_SOFT_MUTE);
		if (mute)
			reg |= 0x8000;
		else
			reg &= ~0x8000;

		snd_soc_write(codec, PMU4_DAC_SOFT_MUTE, reg);
	}
	return 0;
}

static int aml_pmu4_audio_probe(struct snd_soc_codec *codec)
{
	struct aml_pmu4_audio_priv *pmu4_audio = NULL;

	/*pr_info("enter %s\n", __func__);*/
	pmu4_audio = kzalloc(sizeof(struct aml_pmu4_audio_priv), GFP_KERNEL);
	if (pmu4_audio == NULL)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, pmu4_audio);

	/*enable LDO1V8 for audio*/
	aml_pmu4_audio_power_init();

	/*reset audio codec register*/
	aml_pmu4_audio_reset(codec);
	aml_pmu4_audio_start_up(codec);
	aml_pmu4_audio_reg_init(codec);

	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	pmu4_audio->codec = codec;

	return 0;
}

static int aml_pmu4_audio_remove(struct snd_soc_codec *codec)
{
	aml_pmu4_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_pmu4_audio_suspend(struct snd_soc_codec *codec)
{
	aml_pmu4_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_pmu4_audio_resume(struct snd_soc_codec *codec)
{
	pr_info("enter %s\n", __func__);
	aml_pmu4_audio_power_init();
	aml_pmu4_audio_reset(codec);
	aml_pmu4_audio_start_up(codec);
	aml_pmu4_audio_reg_init(codec);
	codec->component.dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_pmu4_audio_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

#define PMU4_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define PMU4_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops pmu4_audio_aif_dai_ops = {
	.hw_params = aml_pmu4_hw_params,
	.prepare = aml_pmu4_prepare,
	.set_fmt = aml_pmu4_set_dai_fmt,
	.set_sysclk = aml_pmu4_set_dai_sysclk,
	.mute_stream = aml_pmu4_codec_mute_stream,
};

struct snd_soc_dai_driver aml_pmu4_audio_dai[] = {
	{
	 .name = "pmu4-audio-hifi",
	 .id = 0,
	 .playback = {
		      .stream_name = "HIFI Playback",
		      .channels_min = 2,
		      .channels_max = 8,
		      .rates = PMU4_AUDIO_STEREO_RATES,
		      .formats = PMU4_AUDIO_FORMATS,
		      },
	 .capture = {
		     .stream_name = "HIFI Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = PMU4_AUDIO_STEREO_RATES,
		     .formats = PMU4_AUDIO_FORMATS,
		     },
	 .ops = &pmu4_audio_aif_dai_ops,
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_aml_pmu4_audio = {
	.probe = aml_pmu4_audio_probe,
	.remove = aml_pmu4_audio_remove,
	.suspend = aml_pmu4_audio_suspend,
	.resume = aml_pmu4_audio_resume,
	.read = aml_pmu4_audio_read,
	.write = aml_pmu4_audio_write,
	.set_bias_level = aml_pmu4_audio_set_bias_level,
	.component_driver = {
		.controls = pmu4_audio_snd_controls,
		.num_controls = ARRAY_SIZE(pmu4_audio_snd_controls),
		.dapm_widgets = pmu4_audio_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pmu4_audio_dapm_widgets),
		.dapm_routes = pmu4_audio_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(pmu4_audio_dapm_routes),
	},
	.reg_cache_size = 16,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
};

static int aml_pmu4_audio_codec_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "aml_pmu4_audio_codec_probe\n");
	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_aml_pmu4_audio,
				     &aml_pmu4_audio_dai[0], 1);
	return ret;
}

static int aml_pmu4_audio_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id aml_pmu4_codec_dt_match[] = {
	{.compatible = "amlogic, aml_pmu4_codec",},
	{},
};

static struct platform_driver aml_pmu4_codec_platform_driver = {
	.driver = {
		   .name = "aml_pmu4_codec",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_pmu4_codec_dt_match,
		   },
	.probe = aml_pmu4_audio_codec_probe,
	.remove = aml_pmu4_audio_codec_remove,
};

static int __init aml_pmu4_audio_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_pmu4_codec_platform_driver);
	if (ret != 0) {
		pr_info("Failed to register AML PMU4 codec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_pmu4_audio_modinit);

static void __exit aml_pmu4_audio_exit(void)
{
	platform_driver_unregister(&aml_pmu4_codec_platform_driver);
}

module_exit(aml_pmu4_audio_exit);

MODULE_DESCRIPTION("ASoC AML pmu4 audio codec driver");
MODULE_AUTHOR("Chengshun Wang <chengshun.wang@amlogic.com>");
MODULE_LICENSE("GPL");
