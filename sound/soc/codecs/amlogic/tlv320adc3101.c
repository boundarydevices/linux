/*
 * linux/sound/soc/codecs/tlv320adc3101.c
 *
 * Copyright 2011 Amlogic
 * Author: Alex Deng <alex.deng@amlogic.com>
 * Based on sound/soc/codecs/tlv320aic320x and
 * TI driver for kernel 2.6.27.
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tlv320adc3101.h"

struct adc3101_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
	u8 blck_N;
};

struct adc3101_priv {
	struct regmap *regmap;
	u32 sysclk;
	u32 power_cfg;
	u32 micpga_routing;
	bool swapdacs;
	int rstn_gpio;
	struct snd_soc_codec *codec;
	/* for more control */
	int codec_cnt;
	int codec_mask;
	unsigned int slot_number;
	struct i2c_client *client[4];
	u8 page_no;
	/* differential_pair
	 * 0: Single-ended;
	 * 1: Differential Pair;
	 */
	unsigned int differential_pair;
};

enum{
	MASK_1 = 1 << 0,
	MASK_2 = 1 << 1,
	MASK_3 = 1 << 2,
	MASK_4 = 1 << 3
};

static int	lr_gain = 0x20;
module_param(lr_gain, int, 0664);
MODULE_PARM_DESC(lr_gain, "PGA Level Volume");

/* 0dB min, 1dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_1, 0, 100, 0);
/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);

static const struct snd_kcontrol_new adc3101_snd_controls[] = {
	SOC_DOUBLE_R_TLV("PCM Playback Volume", ADC3101_LDACVOL,
			ADC3101_RDACVOL, 0, 0x30, 0, tlv_step_0_5),
	SOC_DOUBLE_R_TLV("HP Driver Gain Volume", ADC3101_HPLGAIN,
			ADC3101_HPRGAIN, 0, 0x1D, 0, tlv_step_1),
	SOC_DOUBLE_R_TLV("LO Driver Gain Volume", ADC3101_LOLGAIN,
			ADC3101_LORGAIN, 0, 0x1D, 0, tlv_step_1),
	SOC_DOUBLE_R("HP DAC Playback Switch", ADC3101_HPLGAIN,
			ADC3101_HPRGAIN, 6, 0x01, 1),
	SOC_DOUBLE_R("LO DAC Playback Switch", ADC3101_LOLGAIN,
			ADC3101_LORGAIN, 6, 0x01, 1),
	SOC_DOUBLE_R("Mic PGA Switch", ADC3101_LMICPGAVOL,
			ADC3101_RMICPGAVOL, 7, 0x01, 1),

	SOC_SINGLE("ADCFGA Left Mute Switch", ADC3101_ADCFGA, 7, 1, 0),
	SOC_SINGLE("ADCFGA Right Mute Switch", ADC3101_ADCFGA, 3, 1, 0),

	SOC_DOUBLE_R_TLV("ADC Level Volume", ADC3101_LADCVOL,
			ADC3101_RADCVOL, 0, 0x28, 0, tlv_step_0_5),
	SOC_DOUBLE_R_TLV("PGA Level Volume", ADC3101_LMICPGAVOL,
			ADC3101_RMICPGAVOL, 0, 0x5f, 0, tlv_step_0_5),

	SOC_SINGLE("Auto-mute Switch", ADC3101_DACMUTE, 4, 7, 0),

	SOC_SINGLE("AGC Left Switch", ADC3101_LAGC1, 7, 1, 0),
	SOC_SINGLE("AGC Right Switch", ADC3101_RAGC1, 7, 1, 0),
	SOC_DOUBLE_R("AGC Target Level", ADC3101_LAGC1, ADC3101_RAGC1,
			4, 0x07, 0),
	SOC_DOUBLE_R("AGC Gain Hysteresis", ADC3101_LAGC1, ADC3101_RAGC1,
			0, 0x03, 0),
	SOC_DOUBLE_R("AGC Hysteresis", ADC3101_LAGC2, ADC3101_RAGC2,
			6, 0x03, 0),
	SOC_DOUBLE_R("AGC Noise Threshold", ADC3101_LAGC2, ADC3101_RAGC2,
			1, 0x1F, 0),
	SOC_DOUBLE_R("AGC Max PGA", ADC3101_LAGC3, ADC3101_RAGC3,
			0, 0x7F, 0),
	SOC_DOUBLE_R("AGC Attack Time", ADC3101_LAGC4, ADC3101_RAGC4,
			3, 0x1F, 0),
	SOC_DOUBLE_R("AGC Decay Time", ADC3101_LAGC5, ADC3101_RAGC5,
			3, 0x1F, 0),
	SOC_DOUBLE_R("AGC Noise Debounce", ADC3101_LAGC6, ADC3101_RAGC6,
			0, 0x1F, 0),
	SOC_DOUBLE_R("AGC Signal Debounce", ADC3101_LAGC7, ADC3101_RAGC7,
			0, 0x0F, 0),
};

static const struct adc3101_rate_divs adc3101_divs[] = {
	/* mclk	rate p  j  d	dosr ndac mdac  aosr nadc  madc	blk_N */

	/* 8k rate */
	{ADC3101_FREQ_12000000, 8000, 1, 7, 6800, 768, 5, 3, 128, 5, 18, 24},
	{ADC3101_FREQ_24000000, 8000, 2, 7, 6800, 768, 15, 1, 64, 45, 4, 24},
	{ADC3101_FREQ_25000000, 8000, 2, 7, 3728, 768, 15, 1, 64, 45, 4, 24},
	/* 11.025k rate */
	{ADC3101_FREQ_12000000, 11025, 1, 7, 5264, 512, 8, 2, 128, 8, 8, 16},
	{ADC3101_FREQ_24000000, 11025, 2, 7, 5264, 512, 16, 1, 64, 32, 4, 16},
	/* 16k rate */
	{ADC3101_FREQ_12000000, 16000, 1, 7, 6800, 384, 5, 3, 128, 5, 9, 12},
	{ADC3101_FREQ_24000000, 16000, 2, 7, 6800, 384, 15, 1, 64, 18, 5, 12},
	{ADC3101_FREQ_25000000, 16000, 2, 7, 3728, 384, 15, 1, 64, 18, 5, 12},
	/* 22.05k rate */
	{ADC3101_FREQ_12000000, 22050, 1, 7, 5264, 256, 4, 4, 128, 4, 8, 8},
	{ADC3101_FREQ_24000000, 22050, 2, 7, 5264, 256, 16, 1, 64, 16, 4, 8},
	{ADC3101_FREQ_25000000, 22050, 2, 7, 2253, 256, 16, 1, 64, 16, 4, 8},
	/* 32k rate */
	{ADC3101_FREQ_12000000, 32000, 1, 7, 1680, 192, 2, 7, 64, 2, 21, 6},
	{ADC3101_FREQ_24000000, 32000, 2, 7, 1680, 192, 7, 2, 64, 7, 6, 6},
	/* 44.1k rate */
	{ADC3101_FREQ_12000000, 44100, 1, 7, 5264, 128, 2, 8, 128, 2, 8, 4},
	{ADC3101_FREQ_24000000, 44100, 2, 7, 5264, 128, 8, 2, 64, 8, 4, 4},
	{ADC3101_FREQ_25000000, 44100, 2, 7, 2253, 128, 8, 2, 64, 8, 4, 4},
	/* 48k rate */
	{ADC3101_FREQ_12000000, 48000, 1, 8, 1920, 128, 2, 8, 128, 2, 8, 4},
	{ADC3101_FREQ_24000000, 48000, 2, 8, 1920, 128, 8, 2, 64, 8, 4, 4},
	{ADC3101_FREQ_25000000, 48000, 2, 7, 8643, 128, 8, 2, 64, 8, 4, 4},

	{ADC3101_FREQ_2048000, 8000, 1, 4, 0, 128, 1, 1, 128, 1, 2, 1},
	{ADC3101_FREQ_4096000, 16000, 1, 4, 0, 128, 1, 1, 128, 1, 2, 1},
	{ADC3101_FREQ_8192000, 32000, 1, 4, 0, 128, 1, 1, 128, 1, 2, 1},
	{ADC3101_FREQ_11289600, 44100, 1, 4, 0, 128, 1, 1, 128, 1, 2, 1},
	{ADC3101_FREQ_12288000, 48000, 1, 4, 0, 128, 2, 1, 128, 1, 2, 1},
};

static const struct snd_kcontrol_new hpl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC Switch", ADC3101_HPLROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_L Switch", ADC3101_HPLROUTE, 2, 1, 0),
};

static const struct snd_kcontrol_new hpr_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC Switch", ADC3101_HPRROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_R Switch", ADC3101_HPRROUTE, 2, 1, 0),
};

static const struct snd_kcontrol_new lol_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC Switch", ADC3101_LOLROUTE, 3, 1, 0),
};

static const struct snd_kcontrol_new lor_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC Switch", ADC3101_LORROUTE, 3, 1, 0),
};

static const struct snd_kcontrol_new left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1_L P Switch", ADC3101_LMICPGAPIN, 6, 1, 0),
	SOC_DAPM_SINGLE("IN2_L P Switch", ADC3101_LMICPGAPIN, 4, 1, 0),
	SOC_DAPM_SINGLE("IN3_L P Switch", ADC3101_LMICPGAPIN, 2, 1, 0),
};

static const struct snd_kcontrol_new right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1_R P Switch", ADC3101_RMICPGAPIN, 6, 1, 0),
	SOC_DAPM_SINGLE("IN2_R P Switch", ADC3101_RMICPGAPIN, 4, 1, 0),
	SOC_DAPM_SINGLE("IN3_R P Switch", ADC3101_RMICPGAPIN, 2, 1, 0),
};

static const struct snd_soc_dapm_widget adc3101_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 0, 0,
			   &left_input_mixer_controls[0],
			   ARRAY_SIZE(left_input_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 0, 0,
			   &right_input_mixer_controls[0],
			   ARRAY_SIZE(right_input_mixer_controls)),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", ADC3101_ADCSETUP, 7, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", ADC3101_ADCSETUP, 6, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias", ADC3101_MICBIAS, 6, 0),

	SND_SOC_DAPM_INPUT("IN1_L"),
	SND_SOC_DAPM_INPUT("IN1_R"),
	SND_SOC_DAPM_INPUT("IN2_L"),
	SND_SOC_DAPM_INPUT("IN2_R"),
	SND_SOC_DAPM_INPUT("IN3_L"),
	SND_SOC_DAPM_INPUT("IN3_R"),
};

static const struct snd_soc_dapm_route adc3101_dapm_routes[] = {
	/* Left input */
	{"Left Input Mixer", "IN1_L P Switch", "IN1_L"},
	{"Left Input Mixer", "IN2_L P Switch", "IN2_L"},
	{"Left Input Mixer", "IN3_L P Switch", "IN3_L"},

	{"Left ADC", NULL, "Left Input Mixer"},

	/* Right Input */
	{"Right Input Mixer", "IN1_R P Switch", "IN1_R"},
	{"Right Input Mixer", "IN2_R P Switch", "IN2_R"},
	{"Right Input Mixer", "IN3_R P Switch", "IN3_R"},

	{"Right ADC", NULL, "Right Input Mixer"},
};

static const struct regmap_range_cfg adc3101_regmap_pages[] = {
	{
		.range_min = 0,
		.range_max = ADC3101_RMICPGAVOL,
		.selector_reg = ADC3101_PSEL,
		.selector_mask = 0xff,
		.selector_shift = 0,
		.window_start = 0,
		.window_len = 128,
	},
};

static const struct regmap_config adc3101_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.ranges = adc3101_regmap_pages,
	.num_ranges = ARRAY_SIZE(adc3101_regmap_pages),
	.max_register = ADC3101_RMICPGAVOL,
};

static inline int adc3101_get_divs(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adc3101_divs); i++) {
		if ((adc3101_divs[i].rate == rate)
		    && (adc3101_divs[i].mclk == mclk)) {
			return i;
		}
	}

	pr_err("adc3101: master clock and sample rate is not supported\n");
	return -EINVAL;
}

static int adc3101_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct adc3101_priv *adc3101 = snd_soc_codec_get_drvdata(codec);

	pr_info("%s freq:%d\n", __func__, freq);
	switch (freq) {
	case ADC3101_FREQ_12000000:
	case ADC3101_FREQ_24000000:
	case ADC3101_FREQ_25000000:
	case ADC3101_FREQ_11289600:
	case ADC3101_FREQ_12288000:
	case ADC3101_FREQ_2048000:
	case ADC3101_FREQ_4096000:
	case ADC3101_FREQ_8192000:
		adc3101->sysclk = freq;
		return 0;
	}

	pr_err("adc3101: invalid frequency to set DAI system clock\n");
	return -1;
}

static int adc3101_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct adc3101_priv *adc3101 = NULL;
	u8 iface_reg_1;
	u8 dsp_a_val;
	u8 iface_reg_2;

	adc3101 = snd_soc_codec_get_drvdata(codec);
	if (adc3101 == NULL)
		return -EINVAL;

	pr_info("[%s]:slot_number=%d\n", __func__, adc3101->slot_number);

	iface_reg_1 = snd_soc_read(codec, ADC3101_IFACE1);
	iface_reg_1 = iface_reg_1 & ~(3 << 6 | 3 << 2);
	dsp_a_val = snd_soc_read(codec, ADC3101_DATASLOTOFFSETCTL);
	dsp_a_val = 0;
	iface_reg_2 = snd_soc_read(codec, ADC3101_IFACE3);
	iface_reg_2 = iface_reg_2 & ~(1 << 3);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg_1 |= ADC3101_BCLKMASTER | ADC3101_WCLKMASTER;
		pr_info("%s DAIFMT_CBM_CFM\n", __func__);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		pr_err("adc3101: invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	/* set lrclk/bclk invertion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
	case SND_SOC_DAIFMT_IB_NF:
		iface_reg_2 |= (1 << 3); /* invert bit clock */
		break;
	case SND_SOC_DAIFMT_NB_IF:
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg_1 |= (ADC3101_DSP_MODE << ADC3101_PLLJ_SHIFT);
		iface_reg_2 |= (1 << 3); /* invert bit clock */

		/* set bclk offset according to diffrent adc */
		if (adc3101->slot_number == 0)
			dsp_a_val = 0;
		else if (adc3101->slot_number == 1)
			dsp_a_val = 64;
		else if (adc3101->slot_number == 2)
			dsp_a_val = 128;
		else if (adc3101->slot_number == 3)
			dsp_a_val = 192;
		else
			dsp_a_val = 0x01; /* default add offset 1 */
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg_1 |= (ADC3101_DSP_MODE << ADC3101_PLLJ_SHIFT);
		iface_reg_2 |= (1 << 3); /* invert bit clock */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg_1 |=
			(ADC3101_RIGHT_JUSTIFIED_MODE << ADC3101_PLLJ_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg_1 |=
			(ADC3101_LEFT_JUSTIFIED_MODE << ADC3101_PLLJ_SHIFT);
		break;
	default:
		pr_err("adc3101: invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_write(codec, ADC3101_IFACE1, iface_reg_1);
	snd_soc_write(codec, ADC3101_DATASLOTOFFSETCTL, dsp_a_val);
	snd_soc_write(codec, ADC3101_IFACE3, iface_reg_2);

	return 0;
}

static int __maybe_unused adc3101_hw_high_pass_filter(
	struct snd_soc_codec *codec)
{
	/*page 4*/
	char datas[4][31] = {
		/* data 1*/
		{
			0x0E,
			0x7F, 0xBE, 0x80, 0x42,
			0x7F, 0xBE, 0x7F, 0xBE,
			0x80, 0x84, 0x7F, 0xFF,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x7F, 0xFF, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00
		},
		/* data 2 */
		{
			0x2c,
			0x7F, 0xFF, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x7F, 0xFF,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
		},
		/* data 3 */
		{
			0x4E,
			0x7F, 0xBE, 0x80,
			0x42, 0x7F, 0xBE, 0x7F,
			0xBE, 0x80, 0x84, 0x7F,
			0xFF, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x7F, 0xFF, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00
		},
		/* data 4 */
		{
			0x6C,
			0x7F, 0xFF, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x7F, 0xFF,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
		}
	};
	int i = 0, j = 0, c = 0;
	int ret = 0;
	int nums[] = {31, 21, 31, 21};
	struct adc3101_priv *adc3101 = snd_soc_codec_get_drvdata(codec);

	pr_info("%s ...\n", __func__);

	snd_soc_write(codec, 0x3D, 2);
	snd_soc_write(codec, 0x51, 0);
	/* page 4 */
	snd_soc_write(codec, ADC3101_PSEL, 0x4);

	for (i = 0; i < adc3101->codec_cnt; i++) {
		for (c = 0; c < 4; c++) {
			for (j = 0; j < nums[c]; j++) {
				ret = i2c_smbus_write_byte(
						adc3101->client[i],
						datas[c][j]);
				if (ret < 0)
					pr_err("%x write error\n",
						adc3101->client[i]->addr);
			}
		}
	}

	snd_soc_write(codec, 0x51, 0xC0);
	snd_soc_write(codec, 0x52, 0x00);
	pr_info("%s done\n", __func__);

	return 0;
}


static int adc3101_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct adc3101_priv *adc3101 = snd_soc_codec_get_drvdata(codec);
	u8 data;
	int i;

	pr_info("%s ...\n", __func__);

	i = adc3101_get_divs(adc3101->sysclk, params_rate(params));
	if (i < 0) {
		pr_err("adc3101: sampling rate not supported\n");
		return i;
	}

	/* Use PLL as CODEC_CLKIN and DAC_MOD_CLK as BDIV_CLKIN */
	snd_soc_write(codec, ADC3101_CLKMUX, 0);

	/* We will fix R value to 1 and will make P & J=K.D as varialble */
	data = snd_soc_read(codec, ADC3101_PLLPR);
	data &= ~(7 << 4);

	snd_soc_write(codec, ADC3101_PLLPR,
		      (data | (adc3101_divs[i].p_val << 4) | 0x01));

	snd_soc_write(codec, ADC3101_PLLJ, adc3101_divs[i].pll_j);

	snd_soc_write(codec, ADC3101_PLLDMSB, (adc3101_divs[i].pll_d >> 8));
	snd_soc_write(codec, ADC3101_PLLDLSB,
		      (adc3101_divs[i].pll_d & 0xff));
	/* NADC divider value */
	data = snd_soc_read(codec, ADC3101_NADC);
	data &= ~(0x7f);
	data |= 0x80;
	snd_soc_write(codec, ADC3101_NADC, data | adc3101_divs[i].nadc);

	pr_info("%s NADC = 0x%02x\n",
		__func__,
		snd_soc_read(codec, ADC3101_NADC));

	/* MADC divider value */
	data = snd_soc_read(codec, ADC3101_MADC);
	data &= ~(0x7f);
	data |= 0x80;
	snd_soc_write(codec, ADC3101_MADC, data | adc3101_divs[i].madc);
	pr_info("%s MADC = 0x%02x\n",
		__func__,
		snd_soc_read(codec, ADC3101_MADC));

	/* AOSR value */
	snd_soc_write(codec, ADC3101_AOSR, adc3101_divs[i].aosr);
	pr_info("%s AOSR=%02x\n",
		__func__,
		snd_soc_read(codec, ADC3101_AOSR));

	data = snd_soc_read(codec, ADC3101_IFACE1);
	data = data & ~(3 << 4);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (ADC3101_WORD_LEN_20BITS << ADC3101_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (ADC3101_WORD_LEN_24BITS << ADC3101_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (ADC3101_WORD_LEN_32BITS << ADC3101_DOSRMSB_SHIFT);
		break;
	}
	snd_soc_write(codec, ADC3101_IFACE1, data);
	pr_info("%s iface1 = %02x\n", __func__, data);

	snd_soc_write(codec, ADC3101_MICBIAS, 0x50);

	if (adc3101->differential_pair == 1) {
		pr_info("%s differential pair\n", __func__);
		snd_soc_write(codec, ADC3101_LMICPGANIN, 0x33); //54
		snd_soc_write(codec, ADC3101_RMICPGANIN, 0x33); //57
		snd_soc_write(codec, ADC3101_LMICPGAPIN, 0x3F); //52
		snd_soc_write(codec, ADC3101_RMICPGAPIN, 0x3F); //55
	} else {
		pr_info("%s single end\n", __func__);
		snd_soc_write(codec, ADC3101_LMICPGANIN, 0x3F); //54
		snd_soc_write(codec, ADC3101_RMICPGANIN, 0x3F); //57
		snd_soc_write(codec, ADC3101_LMICPGAPIN, 0xCF); //52
		snd_soc_write(codec, ADC3101_RMICPGAPIN, 0xCF); //55
	}
	snd_soc_write(codec, ADC3101_LMICPGAVOL, lr_gain);
	snd_soc_write(codec, ADC3101_RMICPGAVOL, lr_gain);
	snd_soc_write(codec, ADC3101_ADCSETUP, 0xc2);
	snd_soc_write(codec, ADC3101_ADCFGA, 0);

	pr_info("%s ADCSETUP = %02x\n", __func__,
			snd_soc_read(codec, ADC3101_ADCSETUP));
	pr_info("%s DOUTCTL=%02x\n", __func__,
			snd_soc_read(codec, ADC3101_DOUTCTL));
	pr_info("%s MICBIAS=%02x\n", __func__,
			snd_soc_read(codec, ADC3101_MICBIAS));

	return 0;
}

static int adc3101_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 dac_reg;

	dac_reg = snd_soc_read(codec, ADC3101_DACMUTE) & ~ADC3101_MUTEON;
	if (mute)
		snd_soc_write(codec, ADC3101_DACMUTE, dac_reg | ADC3101_MUTEON);
	else
		snd_soc_write(codec, ADC3101_DACMUTE, dac_reg);

	return 0;
}

static int adc3101_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	pr_debug("%s ..\n", __func__);
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* Switch on PLL */
		snd_soc_update_bits(codec, ADC3101_PLLPR,
				    ADC3101_PLLEN, ADC3101_PLLEN);

		/* Switch on NDAC Divider */
		snd_soc_update_bits(codec, ADC3101_NDAC,
				    ADC3101_NDACEN, ADC3101_NDACEN);

		/* Switch on MDAC Divider */
		snd_soc_update_bits(codec, ADC3101_MDAC,
				    ADC3101_MDACEN, ADC3101_MDACEN);

		/* Switch on NADC Divider */
		snd_soc_update_bits(codec, ADC3101_NADC,
				    ADC3101_NADCEN, ADC3101_NADCEN);

		/* Switch on MADC Divider */
		snd_soc_update_bits(codec, ADC3101_MADC,
				    ADC3101_MADCEN, ADC3101_MADCEN);

		/* Switch on BCLK_N Divider */
		snd_soc_update_bits(codec, ADC3101_BCLKN,
				    ADC3101_BCLKEN, ADC3101_BCLKEN);

		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* Switch off PLL */
		snd_soc_update_bits(codec, ADC3101_PLLPR,
				    ADC3101_PLLEN, 0);

		/* Switch off NDAC Divider */
		snd_soc_update_bits(codec, ADC3101_NDAC,
				    ADC3101_NDACEN, 0);

		/* Switch off MDAC Divider */
		snd_soc_update_bits(codec, ADC3101_MDAC,
				    ADC3101_MDACEN, 0);

		/* Switch off NADC Divider */
		snd_soc_update_bits(codec, ADC3101_NADC,
				    ADC3101_NADCEN, 0);

		/* Switch off MADC Divider */
		snd_soc_update_bits(codec, ADC3101_MADC,
				    ADC3101_MADCEN, 0);

		/* Switch off BCLK_N Divider */
		snd_soc_update_bits(codec, ADC3101_BCLKN,
				    ADC3101_BCLKEN, 0);
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->component.dapm.bias_level = level;
	snd_soc_codec_init_bias_level(codec, level);

	return 0;
}

#define ADC3101_RATES	SNDRV_PCM_RATE_8000_96000
#define ADC3101_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE \
			 | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops adc3101_ops = {
	.hw_params = adc3101_hw_params,
	.digital_mute = adc3101_mute,
	.set_fmt = adc3101_set_dai_fmt,
	.set_sysclk = adc3101_set_dai_sysclk,
};

static struct snd_soc_dai_driver adc3101_dai[] = {
	{
		.name = "tlv320adc3101-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},

		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 8,//2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},
		.ops = &adc3101_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tlv320adc3101-hifi@19",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},

		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 8,//2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},
		.ops = &adc3101_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tlv320adc3101-hifi@1a",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},

		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 8,//2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},
		.ops = &adc3101_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tlv320adc3101-hifi@1b",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},

		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,},
		.ops = &adc3101_ops,
		.symmetric_rates = 1,
	},
};

static int adc3101_suspend(struct snd_soc_codec *codec)
{
	adc3101_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int adc3101_resume(struct snd_soc_codec *codec)
{
	adc3101_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int adc3101_codec_probe(struct snd_soc_codec *codec)
{
	u32 tmp_reg;

	pr_info("%s ...\n", __func__);

	snd_soc_write(codec, ADC3101_RESET, 0x01);

	/*
	 * Workaround: for an unknown reason, the ADC needs to be powered up
	 * and down for the first capture to work properly. It seems related to
	 * a HW BUG or some kind of behavior not documented in the datasheet.
	 */
	tmp_reg = snd_soc_read(codec, ADC3101_ADCSETUP);
	snd_soc_write(codec, ADC3101_ADCSETUP, tmp_reg |
				ADC3101_LADC_EN | ADC3101_RADC_EN);
	snd_soc_write(codec, ADC3101_ADCSETUP, tmp_reg);

	pr_info("%s done...\n", __func__);

	return 0;
}

static int adc3101_remove(struct snd_soc_codec *codec)
{
	adc3101_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static struct snd_soc_codec_driver __maybe_unused soc_codec_dev_adc3101_2 = {
	.probe = adc3101_codec_probe,
	.remove = adc3101_remove,
	.suspend = adc3101_suspend,
	.resume = adc3101_resume,
	.set_bias_level = adc3101_set_bias_level,

};
static struct snd_soc_codec_driver soc_codec_dev_adc3101 = {
	.probe = adc3101_codec_probe,
	.remove = adc3101_remove,
	.suspend = adc3101_suspend,
	.resume = adc3101_resume,
	.set_bias_level = adc3101_set_bias_level,

	.component_driver = {
		.controls = adc3101_snd_controls,
		.num_controls = ARRAY_SIZE(adc3101_snd_controls),
		.dapm_widgets = adc3101_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(adc3101_dapm_widgets),
		.dapm_routes = adc3101_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(adc3101_dapm_routes),
	}
};

static int adc3101_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct adc3101_priv *adc3101 = NULL;
	int ret = 0;

	pr_err("tlv320 %s ...\n", __func__);

	adc3101 = devm_kzalloc(&i2c->dev, sizeof(struct adc3101_priv),
			GFP_KERNEL);
	if (adc3101 == NULL)
		return -ENOMEM;
	adc3101->codec_cnt = 0;

	adc3101->regmap = devm_regmap_init_i2c(i2c, &adc3101_i2c_regmap);
	if (IS_ERR(adc3101->regmap)) {
		pr_info("%s failed devm_regmap_init_i2c\n", __func__);
		return PTR_ERR(adc3101->regmap);
	}
	i2c_set_clientdata(i2c, adc3101);

	adc3101->power_cfg = 0;
	adc3101->swapdacs = false;
	adc3101->micpga_routing = 0;
	adc3101->rstn_gpio = -1;

	ret = of_get_named_gpio(i2c->dev.of_node, "gpio-reset", 0);
	if (ret > 0)
		adc3101->rstn_gpio = ret;

	if (adc3101->rstn_gpio > 0) {
		ret = devm_gpio_request_one(&i2c->dev,
					    adc3101->rstn_gpio,
					    GPIOF_OUT_INIT_HIGH,
					    "adc3101-reset-pin");
		if (ret < 0) {
			dev_err(&i2c->dev, "not able to acquire gpio\n");
			return ret;
		}
	}
	ret = of_property_read_u32(i2c->dev.of_node, "differential_pair",
		&adc3101->differential_pair);
	if (ret) {
		pr_err("failed to get differential_pair, set it default\n");
		adc3101->differential_pair = 0;
		ret = 0;
	}

	ret = of_property_read_u32(i2c->dev.of_node, "slot_number",
		&adc3101->slot_number);
	if (ret) {
		pr_err("failed to get slot_number, set it default\n");
		adc3101->slot_number = 0;
		ret = 0;
	}
	pr_info("%s i2c:%p\n", __func__, i2c);
	ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_adc3101, adc3101_dai, 1);

	pr_info("%s %x done\n", __func__, i2c->addr);

	return ret;
}

static int adc3101_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);

	return 0;
}

static const struct of_device_id tlv320adc3101_of_match[] = {
	{.compatible = "ti,tlv320adc3101"},
	{},
};
MODULE_DEVICE_TABLE(of, tlv320aic31xx_of_match);

static const struct i2c_device_id adc3101_i2c_id[] = {
	{ "tlv320adc3101", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adc3101_i2c_id);

static struct i2c_driver adc3101_i2c_driver = {
	.driver = {
		.name = "tlv320adc3101",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tlv320adc3101_of_match),
	},
	.probe =    adc3101_i2c_probe,
	.remove =   adc3101_i2c_remove,
	.id_table = adc3101_i2c_id,
};

module_i2c_driver(adc3101_i2c_driver);

MODULE_DESCRIPTION("ASoC tlv320adc3101 codec driver");
MODULE_AUTHOR("alex.deng <alex.deng@amlogic.com>");
MODULE_LICENSE("GPL");
