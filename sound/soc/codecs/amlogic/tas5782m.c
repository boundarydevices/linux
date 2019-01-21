/*
 * sound/soc/codecs/amlogic/tas5782m.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/tas57xx.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "tas5782m.h"

#define DEV_NAME	"tas5782m"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void tas5782m_early_suspend(struct early_suspend *h);
static void tas5782m_late_resume(struct early_suspend *h);
#endif

#define tas5782m_RATES (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000)

#define tas5782m_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE | \
	 SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE | \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5782M_REG_00      (0x00)
#define TAS5782M_REG_03      (0x03)
#define TAS5782M_REG_7F      (0x7F)
#define TAS5782M_REG_28      (0x28)
#define TAS5782M_REG_29      (0x29)
#define TAS5782M_REG_3D      (0x3D)
#define TAS5782M_REG_3E      (0x3E)



enum BITSIZE_MODE {
	BITSIZE_MODE_16BITS = 0,
	BITSIZE_MODE_20BITS = 1,
	BITSIZE_MODE_24BITS = 2,
	BITSIZE_MODE_32BITS = 3,
};

/* codec private data */
struct tas5782m_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct tas57xx_platform_data *pdata;
	struct work_struct work;

	unsigned int work_mode;
	unsigned int chip_offset;

	/*Platform provided EQ configuration */
	int num_eq_conf_texts;
	const char **eq_conf_texts;
	int eq_cfg;
	struct soc_enum eq_conf_enum;
	unsigned char Ch1_vol;
	unsigned char Ch2_vol;
	unsigned char Ch1_mute;
	unsigned char Ch2_mute;
	unsigned char master_vol;
	unsigned int mclk;
	unsigned int EQ_enum_value;
	unsigned int DRC_enum_value;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int g_sample_bitsize = 32;

static int tas5782m_ch1_vol_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
			| SNDRV_CTL_ELEM_ACCESS_READWRITE;
	uinfo->count  = 1;

	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 0xff;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5782m_ch2_vol_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
			| SNDRV_CTL_ELEM_ACCESS_READWRITE;
	uinfo->count  = 1;

	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 0xff;
	uinfo->value.integer.step = 1;

	return 0;
}


static int tas5782m_ch1_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
			| SNDRV_CTL_ELEM_ACCESS_READWRITE;
	uinfo->count  = 1;

	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5782m_ch2_mute_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
			| SNDRV_CTL_ELEM_ACCESS_READWRITE;
	uinfo->count  = 1;

	uinfo->value.integer.min  = 0;
	uinfo->value.integer.max  = 1;
	uinfo->value.integer.step = 1;

	return 0;
}



static int tas5782m_ch1_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5782m->Ch1_vol;

	return 0;
}

static int tas5782m_ch2_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5782m->Ch2_vol;

	return 0;
}


static int tas5782m_ch1_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5782m->Ch1_mute;

	return 0;
}

static int tas5782m_ch2_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5782m->Ch2_mute;

	return 0;
}


static void tas5782m_set_volume(struct snd_soc_codec *codec,
				int value, int ch_num)
{
	unsigned char buf[2];
	int write_count = 0;
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	if (value < 0)
		value = 0;
	if (value > 255)
		value = 255;

	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_7F, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = (ch_num == 0) ? TAS5782M_REG_3D : TAS5782M_REG_3E;
	buf[1] = value;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

}

static int tas5782m_ch1_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int value;

	value = ucontrol->value.integer.value[0];
	tas5782m->Ch1_vol = value;
	value = 255 - value;
	tas5782m_set_volume(codec, value, 0);

	return 0;
}

static int tas5782m_ch2_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int value;

	value = ucontrol->value.integer.value[0];
	tas5782m->Ch2_vol = value;
	value = 255 - value;
	tas5782m_set_volume(codec, value, 1);

	return 0;
}

static void tas5782m_set_mute(struct snd_soc_codec *codec,
				unsigned char left_mute,
				unsigned char right_mute)
{
	unsigned char buf[2];
	int write_count = 0;
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);


	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_7F, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_03;
	buf[1] = left_mute << 4 | right_mute;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

}


static int tas5782m_ch1_mute_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int value;

	value = ucontrol->value.integer.value[0];
	tas5782m->Ch1_mute = value;
	tas5782m_set_mute(codec, tas5782m->Ch1_mute, tas5782m->Ch2_mute);

	return 0;
}

static int tas5782m_ch2_mute_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int value;

	value = ucontrol->value.integer.value[0];
	tas5782m->Ch2_mute = value;
	tas5782m_set_mute(codec, tas5782m->Ch1_mute, tas5782m->Ch2_mute);

	return 0;
}



static const struct snd_kcontrol_new tas5782m_snd_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Ch1 Volume",
		.info  = tas5782m_ch1_vol_info,
		.get   = tas5782m_ch1_vol_get,
		.put   = tas5782m_ch1_vol_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Ch2 Volume",
		.info  = tas5782m_ch2_vol_info,
		.get   = tas5782m_ch2_vol_get,
		.put   = tas5782m_ch2_vol_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Ch1 Mute",
		.info  = tas5782m_ch1_mute_info,
		.get   = tas5782m_ch1_mute_get,
		.put   = tas5782m_ch1_mute_set,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Ch2 Mute",
		.info  = tas5782m_ch2_mute_info,
		.get   = tas5782m_ch2_mute_get,
		.put   = tas5782m_ch2_mute_set,
	}

};

static int tas5782m_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tas5782m_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;//-EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;//-EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;//-EINVAL;
	}

	return 0;
}

static int tas5782m_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;

	rate = params_rate(params);

	pr_debug("rate: %u\n", rate);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		pr_debug("24bit\n");
	/* fall through */
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		pr_debug("20bit\n");

		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		pr_debug("16bit\n");

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tas5782m_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	pr_debug("level = %d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static const struct snd_soc_dai_ops tas5782m_dai_ops = {
	.hw_params = tas5782m_hw_params,
	.set_sysclk = tas5782m_set_dai_sysclk,
	.set_fmt = tas5782m_set_dai_fmt,
};

static struct snd_soc_dai_driver tas5782m_dai = {
	.name = DEV_NAME,
	.playback = {
		.stream_name = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = tas5782m_RATES,
		.formats = tas5782m_FORMATS,
	},
	.ops = &tas5782m_dai_ops,
};


static int reset_tas5782m_GPIO(struct snd_soc_codec *codec)
{
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5782m->pdata;
	int ret = 0;

	if (pdata->reset_pin < 0)
		return 0;

	ret = devm_gpio_request_one(codec->dev, pdata->reset_pin,
				GPIOF_OUT_INIT_LOW, "tas5782m-reset-pin");

	if (ret < 0) {
		pr_err("failed!!! devm_gpio_request_one = %d!\n", ret);
		return -1;
	}

	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	udelay(1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	udelay(1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	msleep(20);

	return 0;
}

static int tas5782m_init_i2s_tdm_mode(struct snd_soc_codec *codec, int bit_size)
{
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int work_mod = tas5782m->work_mode;

	int tdm_aofs = 0;
	unsigned char buf[8] = {0};
	int write_count = 0;
	enum BITSIZE_MODE bit_value = BITSIZE_MODE_32BITS;

	write_count = 2;
	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_7F, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count))
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);

	if (bit_size == 16)
		bit_value = BITSIZE_MODE_16BITS;
	else if (bit_size == 20)
		bit_value = BITSIZE_MODE_20BITS;
	else if (bit_size == 24)
		bit_value = BITSIZE_MODE_24BITS;
	else if (bit_size == 32)
		bit_value = BITSIZE_MODE_32BITS;

	buf[0] = TAS5782M_REG_28, buf[1] = (work_mod << 4) | bit_value;
	write_count = 2;
	if (write_count != i2c_master_send(tas5782m->i2c, buf, write_count)) {
		pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
				__func__, __LINE__);
	} else {
		pr_debug("%s %d reg:0x%x, chip_offset:%d reg_off:0x%x data:0x%x\n",
			__func__, __LINE__, tas5782m->i2c->addr,
			tas5782m->chip_offset, buf[0], buf[1]);
	}

	if (work_mod == WORK_MODE_TDM) {
		buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
		write_count = 2;
		if (write_count != i2c_master_send(tas5782m->i2c, buf,
			write_count))
			pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
					__func__, __LINE__);

		buf[0] = TAS5782M_REG_7F, buf[1] = 0x00;
		write_count = 2;
		if (write_count != i2c_master_send(tas5782m->i2c, buf,
			write_count))
			pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
					__func__, __LINE__);

		buf[0] = TAS5782M_REG_00, buf[1] = 0x00;
		write_count = 2;
		if (write_count != i2c_master_send(tas5782m->i2c, buf,
			write_count))
			pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
					__func__, __LINE__);

		tdm_aofs = bit_size * 2 * (tas5782m->chip_offset - 1);

		buf[0] = TAS5782M_REG_29, buf[1] = tdm_aofs;
		write_count = 2;
		if (write_count != i2c_master_send(tas5782m->i2c, buf,
			write_count)) {
			pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
						__func__, __LINE__);
		} else {
			pr_debug("%s %d reg:0x%x, chip_offset:%d reg_off:0x%x",
				__func__, __LINE__, tas5782m->i2c->addr,
				tas5782m->chip_offset, buf[0]);
			pr_debug("tdm_aofs:0x%x SCLKS\n", tdm_aofs);
		}
	}

	return 0;
}

static void tas5782m_init_func(struct work_struct *p_work)
{
	struct tas5782m_priv *tas5782m;
	struct snd_soc_codec *codec;
	struct i2c_client *i2c;
	int i = 0, j = 0, k = 0;
	int value_count;
	unsigned char buf[64] = {0};
	int write_count = 0;
	int data_row = 0;

	tas5782m = container_of(
				p_work, struct tas5782m_priv, work);

	codec = tas5782m->codec;

	reset_tas5782m_GPIO(codec);

	//init register
	tas5782m = snd_soc_codec_get_drvdata(codec);
	dev_info(codec->dev, "tas5782m_init id=%d\n", tas5782m->chip_offset);

	i2c = tas5782m->i2c;
	value_count = ARRAY_SIZE(tas5782m_reg_defaults);
	for (i = 0; i < value_count;) {
		if (tas5782m_reg_defaults[i].reg == CFG_META_BURST) {
			if (tas5782m_reg_defaults[i].def == 2) {
				data_row = 1;
				write_count =
					tas5782m_reg_defaults[i].def;
			} else {
				data_row =
					(tas5782m_reg_defaults[i].def
						+ 2) >> 1;
				write_count =
					tas5782m_reg_defaults[i].def
						+ 1;
			}
			i++;

			j = 0, k = 0;
			for (k = 0; k < data_row; k++) {
				buf[j++] =
					tas5782m_reg_defaults[i+k].reg;
				buf[j++] =
					tas5782m_reg_defaults[i+k].def;
			}

			i += data_row;

			//dump_buf(buf, k);
			if (write_count !=
				i2c_master_send(i2c, buf,
				write_count)) {
				pr_err("%s %d !!!!! i2c_master_send error !!!!!\n",
					__func__, __LINE__);
				break;
			}
		} else {
			i++;
			pr_info("%s %d warning: register value error!\n",
				__func__, __LINE__);
		}
	}

	tas5782m_init_i2s_tdm_mode(codec, g_sample_bitsize);
}

static int tas5782m_probe(struct snd_soc_codec *codec)
{
	struct tas5782m_priv *tas5782m;

#ifdef CONFIG_HAS_EARLYSUSPEND
	tas5782m->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	tas5782m->early_suspend.suspend = tas5782m_early_suspend;
	tas5782m->early_suspend.resume = tas5782m_late_resume;
	tas5782m->early_suspend.param = codec;
	register_early_suspend(&(tas5782m->early_suspend));
#endif

	tas5782m = snd_soc_codec_get_drvdata(codec);
	tas5782m->codec = codec;

	INIT_WORK(&tas5782m->work, tas5782m_init_func);
	schedule_work(&tas5782m->work);

	return 0;
}

static int tas5782m_remove(struct snd_soc_codec *codec)
{
	struct tas5782m_priv *tas5782m;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	unregister_early_suspend(&(tas5782m->early_suspend));
#endif
	tas5782m = snd_soc_codec_get_drvdata(codec);

	cancel_work_sync(&tas5782m->work);

	return 0;
}

#ifdef CONFIG_PM
static int tas5782m_suspend(struct snd_soc_codec *codec)
{
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	dev_info(codec->dev, "tas5782m_suspend!\n");

	if (pdata && pdata->suspend_func)
		pdata->suspend_func();

	return 0;
}

static int tas5782m_resume(struct snd_soc_codec *codec)
{
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);
	struct tas5782m_priv *tas5782m;

	dev_info(codec->dev, "tas5782m_resume!\n");

	if (pdata && pdata->resume_func)
		pdata->resume_func();

	tas5782m = snd_soc_codec_get_drvdata(codec);
	tas5782m->codec = codec;

	INIT_WORK(&tas5782m->work, tas5782m_init_func);
	schedule_work(&tas5782m->work);


	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tas5782m_early_suspend(struct early_suspend *h)
{
}

static void tas5782m_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_dapm_widget tas5782m_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_codec_driver soc_codec_dev_tas5782m = {
	.probe = tas5782m_probe,
	.remove = tas5782m_remove,
#ifdef CONFIG_PM
	.suspend = tas5782m_suspend,
	.resume = tas5782m_resume,
#endif
	.set_bias_level = tas5782m_set_bias_level,
	.component_driver = {
		.controls = tas5782m_snd_controls,
		.num_controls = ARRAY_SIZE(tas5782m_snd_controls),
		.dapm_widgets = tas5782m_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(tas5782m_dapm_widgets),
	}
};

/*
 *static const struct regmap_config tas5782m_regmap = {
 *	.reg_bits = 8,
 *	.val_bits = 8,
 *
 *	.max_register = tas5782m_REGISTER_COUNT,
 *	.reg_defaults = tas5782m_reg_defaults,
 *	.num_reg_defaults =
 *	sizeof(tas5782m_reg_defaults)/sizeof(tas5782m_reg_defaults[0]),
 *	.cache_type = REGCACHE_RBTREE,
 *};
 */

static int tas5782m_parse_dts(struct tas5782m_priv *tas5782m,
				struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_debug("%s pdata->reset_pin = %d!\n", __func__, reset_pin);
	}
	tas5782m->pdata->reset_pin = reset_pin;

	ret = of_property_read_u32(np, "work_mode", &tas5782m->work_mode);
	pr_debug("tas5782m->work_mode:%d(%s)!\n", tas5782m->work_mode,
		(tas5782m->work_mode == WORK_MODE_I2S)?"i2s":"tdm");

	ret = of_property_read_u32(np, "chip_offset", &tas5782m->chip_offset);
	pr_debug("tas5782m->chip_offset:%d!\n", tas5782m->chip_offset);

	return ret;
}

static int tas5782m_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct tas5782m_priv *tas5782m;
	struct tas57xx_platform_data *pdata;
	int ret;
	const char *codec_name = NULL;

	pr_debug("i2c->addr=0x%x\n", i2c->addr);

	tas5782m = devm_kzalloc(&i2c->dev,
		sizeof(struct tas5782m_priv), GFP_KERNEL);
	if (!tas5782m)
		return -ENOMEM;


	tas5782m->i2c = i2c;
	/*
	 * tas5782m->regmap = devm_regmap_init_i2c(i2c, &tas5782m_regmap);
	 * if (IS_ERR(tas5782m->regmap)) {
	 *		ret = PTR_ERR(tas5782m->regmap);
	 *		dev_err(&i2c->dev,
	 *		"Failed to allocate register map: %d\n", ret);
	 *		return ret;
	 * }
	 */
	pdata = devm_kzalloc(&i2c->dev,
		sizeof(struct tas57xx_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	tas5782m->pdata = pdata;

	tas5782m_parse_dts(tas5782m, i2c->dev.of_node);

	if (of_property_read_string(i2c->dev.of_node,
		"codec_name", &codec_name)) {
		pr_info("no codec name\n");
		ret = -1;
	}
	pr_debug("aux name = %s\n", codec_name);
	if (codec_name)
		dev_set_name(&i2c->dev, "%s", codec_name);

	i2c_set_clientdata(i2c, tas5782m);

	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_tas5782m, &tas5782m_dai, 1);
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register codec (%d)\n", ret);

	return ret;
}

static int tas5782m_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct i2c_device_id tas5782m_i2c_id[] = {
	{ "tas5782m", 0 },
	{}
};

static const struct of_device_id tas5782m_of_id[] = {
	{.compatible = "ti, tas5782m",},
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, tas5782m_of_id);

static struct i2c_driver tas5782m_i2c_driver = {
	.driver = {
		.name = DEV_NAME,
		.of_match_table = tas5782m_of_id,
		.owner = THIS_MODULE,
	},
	.probe = tas5782m_i2c_probe,
	.remove = tas5782m_i2c_remove,
	.id_table = tas5782m_i2c_id,
};

module_i2c_driver(tas5782m_i2c_driver);


MODULE_DESCRIPTION("ASoC tas5782m driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
