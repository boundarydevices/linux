// SPDX-License-Identifier: GPL-2.0
/*
 * linux/sound/soc/codecs/tlv320adc3101.c
 *
 * Copyright 2019 Baylibre
 *
 * Author: Nicolas Belin <nbelin@baylibre.com>
 *
 * Based on sound/soc/codecs/tlv320aic332x4.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

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
	u8 nadc;
	u8 madc;
	u8 aosr;
};

struct adc3101_priv {
	struct regmap *regmap;
	u32 sysclk;
	int rstn_gpio;
	struct clk *mclk;
	u32 tdm_offset;
	u32 tdm_additional_offset;
	u32 right_pin_select;
	u32 left_pin_select;
	u32 fmt;
	struct regulator *supply_iov;
	struct regulator *supply_dv;
	struct regulator *supply_av;
	unsigned int minus6db_left_input;
	unsigned int minus6db_right_input;
	struct device *dev;
};

static int adc3101_get_adc_left_input_switch(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = adc3101->minus6db_left_input;
	return 0;
}

static int adc3101_put_adc_left_input_switch(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);
	unsigned int minus6db_left_input = ucontrol->value.integer.value[0];

	if (minus6db_left_input > 1)
		return -EINVAL;

	adc3101->minus6db_left_input = minus6db_left_input;
	snd_soc_component_update_bits(component, ADC3101_LPGAPIN,
		ADC3101_PGAPIN_6DB_MASK,
		minus6db_left_input ? ADC3101_PGAPIN_6DB_MASK : 0);
	snd_soc_component_update_bits(component, ADC3101_LPGAPIN2,
		ADC3101_PGAPIN2_6DB_MASK,
		minus6db_left_input ? ADC3101_PGAPIN2_6DB_MASK : 0);

	return 0;
}

static int adc3101_get_adc_right_input_switch(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = adc3101->minus6db_right_input;
	return 0;
}

static int adc3101_put_adc_right_input_switch(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);
	unsigned int minus6db_right_input = ucontrol->value.integer.value[0];

	if (minus6db_right_input > 1)
		return -EINVAL;

	adc3101->minus6db_right_input = minus6db_right_input;
	snd_soc_component_update_bits(component, ADC3101_RPGAPIN,
		ADC3101_PGAPIN_6DB_MASK,
		minus6db_right_input ? ADC3101_PGAPIN_6DB_MASK : 0);
	snd_soc_component_update_bits(component, ADC3101_RPGAPIN2,
		ADC3101_PGAPIN2_6DB_MASK,
		minus6db_right_input ? ADC3101_PGAPIN2_6DB_MASK : 0);

	return 0;
}

/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);
/* -12dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_adc_vol, -1200, 50, 0);

static const struct snd_kcontrol_new adc3101_snd_controls[] = {
	SOC_SINGLE("ADC Mute Left Switch", ADC3101_FADCVOL, 7, 1, 0),
	SOC_SINGLE("ADC Mute Right Switch", ADC3101_FADCVOL, 3, 1, 0),
	SOC_DOUBLE_R_S_TLV("ADC Level Volume", ADC3101_LADCVOL,
			ADC3101_RADCVOL, 0, -0x18, 0x28, 6, 0, tlv_adc_vol),
	SOC_DOUBLE_R_TLV("PGA Level Volume", ADC3101_LAPGAVOL,
			ADC3101_RAPGAVOL, 0, 0x50, 0, tlv_step_0_5),
	SOC_SINGLE_BOOL_EXT("Minus 6dB ADC Left input Switch", 0,
			    adc3101_get_adc_left_input_switch,
			    adc3101_put_adc_left_input_switch),
	SOC_SINGLE_BOOL_EXT("Minus 6dB ADC Right input Switch", 0,
			    adc3101_get_adc_right_input_switch,
			    adc3101_put_adc_right_input_switch),
};

static const struct adc3101_rate_divs adc3101_divs[] = {

	//conditions
	//MCLK 50MHz MAX
	//33MHz Max after NADC
	//2.8 MHz < AOSR × ADC_fs < 6.2 MHz

	/* 8k rate */
	{2048000, 8000, 1, 1, 0}, //256 mclk_fs, AOSR=0 means 256

	/* 11.025k rate */
	{2822400, 11025, 1, 1, 0}, //64 mclk_fs, AOSR=0 means 256

	/* 16k rate */
	{4096000, 16000, 1, 1, 0}, //256 mclk_fs, AOSR=0 means 256

	/* 22.05k rate */
	{5644800, 22050, 1, 1, 0}, //256 mclk_fs, AOSR=0 means 256

	/* 32k rate */
	{4096000, 32000, 1, 1, 128}, //128 mclk_fs
	{8192000, 32000, 1, 2, 128}, //256 mclk_fs
	{22579200, 32000, 2, 2, 128}, //512 mclk_fs

	/* 44.1k rate */
	{2822400, 44100, 1, 1, 64}, //64 mclk_fs
	{5644800, 44100, 1, 1, 128}, //128 mclk_fs
	{11289600, 44100, 1, 2, 128}, //256 mclk_fs
	{22579200, 44100, 2, 2, 128}, //512 mclk_fs

	/* 48k rate */
	{3072000, 48000, 1, 1, 64}, //64 mclk_fs
	{6144000, 48000, 1, 1, 128}, //128 mclk_fs
	{12288000, 48000, 1, 2, 128}, //256 mclk_fs
	{24576000, 48000, 2, 2, 128}, //512 mclk_fs

	/* 96k rate */
	{24576000, 96000, 2, 2, 64}, //256 mclk_fs

};

static const struct snd_soc_dapm_widget adc3101_dapm_widgets[] = {
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture",
			ADC3101_ADC_DIGITAL, 6, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", ADC3101_ADC_DIGITAL, 7, 0),
	SND_SOC_DAPM_INPUT("IN1_L"),
	SND_SOC_DAPM_INPUT("IN1_R"),
};

static const struct snd_soc_dapm_route adc3101_dapm_routes[] = {
	/* Right Input */
	{"Right ADC", NULL, "IN1_R"},
	/* Left Input */
	{"Left ADC", NULL, "IN1_L"},
};

static const struct regmap_range_cfg adc3101_regmap_pages[] = {
	{
		.name = "Pages",
		.selector_reg = 0,
		.selector_mask  = 0xff,
		.window_start = 0,
		.window_len = 128,
		.range_min = 0,
		.range_max = ADC3101_APGAFLAGS,
	},
};

const struct regmap_config adc3101_regmap_config = {
	.max_register = ADC3101_APGAFLAGS,
	.ranges = adc3101_regmap_pages,
	.num_ranges = ARRAY_SIZE(adc3101_regmap_pages),
};
EXPORT_SYMBOL(adc3101_regmap_config);

static inline int adc3101_get_divs(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adc3101_divs); i++) {
		if ((adc3101_divs[i].rate == rate)
			&& (adc3101_divs[i].mclk == mclk)) {
			return i;
		}
	}

	return -EINVAL;
}

static int adc3101_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "frequency to set DAI system clock=%d\n", freq);
	adc3101->sysclk = freq;
	return 0;
}

static int adc3101_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_component *component = dai->component;
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);
	unsigned int first_slot, last_slot, tdm_offset;

	dev_dbg(component->dev,
		"%s() tx_mask=0x%x rx_mask=0x%x slots=%d slot_width=%d\n",
		__func__, tx_mask, rx_mask, slots, slot_width);

	if (!tx_mask) {
		dev_err(component->dev, "tdm tx mask must not be 0\n");
		return -EINVAL;
	}

	first_slot = __ffs(tx_mask);
	last_slot = __fls(tx_mask);

	if (last_slot - first_slot != hweight32(tx_mask) - 1) {
		dev_err(component->dev, "tdm tx mask must be contiguous\n");
		return -EINVAL;
	}

	tdm_offset = first_slot * slot_width;
	tdm_offset += adc3101->tdm_additional_offset;

	if (tdm_offset > 255) {
		dev_err(component->dev,
			"tdm tx slot selection out of bounds\n");
		return -EINVAL;
	}

	adc3101->tdm_offset = tdm_offset;
	dev_dbg(component->dev, "tdm offset is %d\n", adc3101->tdm_offset);

	/* tdm offset */
	snd_soc_component_write(component, ADC3101_CH_OFFSET_1, tdm_offset);
	/* second channel always after the first one */
	snd_soc_component_write(component, ADC3101_CH_OFFSET_2, 0);


	return 0;
}

static int adc3101_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev,
		"adc3101: fmt to set DAI fmt=0x%x\n",
		fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		dev_err(component->dev,
			"adc3101: invalid DAI master mode not supported\n");
		return -EINVAL;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		dev_err(component->dev,
			"adc3101: invalid DAI master/slave interface\n");
		return -EINVAL;
	}

	adc3101->tdm_additional_offset = 0;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		/* same as B but add offset 1 */
		adc3101->tdm_additional_offset = 0x01;
		fallthrough;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		adc3101->fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		dev_err(component->dev,
			"adc3101: invalid DAI interface format\n");
		return -EINVAL;
	}

	return 0;
}

static int adc3101_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);
	u8 iface1_reg = 0;
	u8 iface2_reg = 0;
	u8 i2s_tdm_reg = 0;
	int i;

	i = adc3101_get_divs(adc3101->sysclk, params_rate(params));
	if (i < 0) {
		dev_err(component->dev,
			"adc3101: sampling rate not supported\n");
		return i;
	}

	/* NADC divider value  and enable */
	snd_soc_component_write(component, ADC3101_NADC, adc3101_divs[i].nadc |
							ADC3101_NADCEN);

	/* MADC divider value and enable */
	snd_soc_component_write(component, ADC3101_MADC, adc3101_divs[i].madc |
							ADC3101_MADCEN);

	/* AOSR value */
	snd_soc_component_write(component, ADC3101_AOSR, adc3101_divs[i].aosr);

	/* check the wanted interface configuration */
	switch (adc3101->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		iface1_reg |= (ADC3101_DSP_MODE <<
			       ADC3101_IFACE1_DATATYPE_SHIFT);
		iface1_reg |= ADC3101_3STATE;
		iface2_reg |= ADC3101_BCLKINV_MASK; /* invert bit clock */
		i2s_tdm_reg |= ADC3101_TDM_EN | ADC3101_EARLY_STATE_EN;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface1_reg |= (ADC3101_RIGHT_J_MODE <<
			       ADC3101_IFACE1_DATATYPE_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface1_reg |= (ADC3101_LEFT_J_MODE <<
			       ADC3101_IFACE1_DATATYPE_SHIFT);
		break;
	default:
		dev_err(component->dev,
			"adc3101: invalid DAI interface format\n");
		return -EINVAL;
	}

	switch (params_width(params)) {
	case 16:
		iface1_reg |= (ADC3101_WORD_LEN_16BITS <<
			       ADC3101_IFACE1_DATALEN_SHIFT);
		break;
	case 20:
		iface1_reg |= (ADC3101_WORD_LEN_20BITS <<
			       ADC3101_IFACE1_DATALEN_SHIFT);
		break;
	case 24:
		iface1_reg |= (ADC3101_WORD_LEN_24BITS <<
			       ADC3101_IFACE1_DATALEN_SHIFT);
		break;
	case 32:
		iface1_reg |= (ADC3101_WORD_LEN_32BITS <<
			       ADC3101_IFACE1_DATALEN_SHIFT);
		break;
	}

	/* BDIVCLKIN is always ADC_CLK */
	iface2_reg |= ADC3101_BDIVCLKIN_ADC_CLK << ADC3101_BDIVCLKIN_SHIFT;

	/* writing the iface 1 & 2 settings */
	snd_soc_component_write(component, ADC3101_IFACE1, iface1_reg);
	snd_soc_component_write(component, ADC3101_IFACE2, iface2_reg);

	/* enabling tdm if needed */
	snd_soc_component_write(component, ADC3101_I2S_TDM, i2s_tdm_reg);

	return 0;
}

static int adc3101_mute(struct snd_soc_dai *dai, int mute, int direction)
{
	struct snd_soc_component *component = dai->component;

	snd_soc_component_update_bits(component, ADC3101_FADCVOL,
			    ADC3101_MUTE_MASK, mute ? ADC3101_MUTE : 0);

	return 0;
}

static int adc3101_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
	return 0;
}

#define ADC3101_RATES	SNDRV_PCM_RATE_8000_96000
#define ADC3101_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops adc3101_ops = {
	.hw_params = adc3101_hw_params,
	.mute_stream = adc3101_mute,
	.set_tdm_slot = adc3101_set_tdm_slot,
	.set_fmt = adc3101_set_dai_fmt,
	.set_sysclk = adc3101_set_dai_sysclk,
	.no_capture_mute = 1,
};

static struct snd_soc_dai_driver adc3101_dai = {
	.name = "tlv320adc3101-aif",
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ADC3101_RATES,
			.formats = ADC3101_FORMATS,
	},
	.ops = &adc3101_ops,

};

static int adc3101_component_probe(struct snd_soc_component *component)
{

	struct adc3101_priv *adc3101 = snd_soc_component_get_drvdata(component);
	struct device *dev = adc3101->dev;
	u8 clk_mux = 0;

	if (gpio_is_valid(adc3101->rstn_gpio)) {
		gpio_set_value(adc3101->rstn_gpio, 0);
		ndelay(10);
		gpio_set_value(adc3101->rstn_gpio, 1);
		ndelay(10);
	} else {
		dev_err(dev,
			"invalid reset gpio. Your adc may not work properly\n");
	}

	/* SW reset */
	snd_soc_component_write(component, ADC3101_RESET, ADC3101_RESET_VALUE);

	/* MCLK as an input, not supporting anything else */
	clk_mux |= (ADC3101_PLL_CLKIN_MCLK << ADC3101_PLL_CLKIN_SHIFT) |
			(ADC3101_CODEC_CLKIN_MCLK << ADC3101_CODEC_CLKIN_MCLK);
	snd_soc_component_write(component, ADC3101_CLKMUX, clk_mux);

	/* left pin selection */
	switch (adc3101->left_pin_select) {
	case CH_SEL1:
	case CH_SEL2:
	case CH_SEL3:
	case CH_SEL4:
		snd_soc_component_update_bits(component, ADC3101_LPGAPIN,
			ADC3101_PGAPIN_SEL_MASK <<
			(2 * adc3101->left_pin_select),
			adc3101->minus6db_left_input);
		break;
	case CH_SEL1X:
	case CH_SEL2X:
	case CH_SEL3X:
		snd_soc_component_update_bits(component, ADC3101_LPGAPIN2,
			ADC3101_PGAPIN_SEL_MASK <<
			(2 * adc3101->left_pin_select - 8),
			adc3101->minus6db_left_input);
		break;
	default:
		dev_err(component->dev, "wrong left pin selection\n");
		return -EINVAL;
	}

	/* right pin selection */
	switch (adc3101->right_pin_select) {
	case CH_SEL1:
	case CH_SEL2:
	case CH_SEL3:
	case CH_SEL4:
		snd_soc_component_update_bits(component, ADC3101_RPGAPIN,
			ADC3101_PGAPIN_SEL_MASK <<
			(2 * adc3101->right_pin_select),
			adc3101->minus6db_right_input);
		break;
	case CH_SEL1X:
	case CH_SEL2X:
	case CH_SEL3X:
		snd_soc_component_update_bits(component, ADC3101_RPGAPIN2,
			ADC3101_PGAPIN_SEL_MASK <<
			(2 * adc3101->right_pin_select - 8),
			adc3101->minus6db_right_input);
		break;
	default:
		dev_err(component->dev, "wrong right pin selection\n");
		return -EINVAL;
	}

	/* unmute the left analog PGA */
	snd_soc_component_update_bits(component, ADC3101_LAPGAVOL,
							ADC3101_APGA_MUTE, 0);
	/* unmute the right analog PGA */
	snd_soc_component_update_bits(component, ADC3101_RAPGAVOL,
							ADC3101_APGA_MUTE, 0);

	/* update the soft stepping only */
	snd_soc_component_update_bits(component, ADC3101_ADC_DIGITAL,
			ADC3101_SOFT_STEPPING_MASK,
			ADC3101_SOFT_STEPPING_DISABLE);

	/* unmute */
	snd_soc_component_update_bits(component, ADC3101_FADCVOL,
			ADC3101_MUTE_MASK, 0);

	return 0;
}

static const struct snd_soc_component_driver soc_component_dev_adc3101 = {
	.probe			= adc3101_component_probe,
	.set_bias_level		= adc3101_set_bias_level,
	.controls		= adc3101_snd_controls,
	.num_controls		= ARRAY_SIZE(adc3101_snd_controls),
	.dapm_widgets		= adc3101_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(adc3101_dapm_widgets),
	.dapm_routes		= adc3101_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(adc3101_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static int adc3101_parse_dt(struct adc3101_priv *adc3101,
		struct device_node *np)
{
	struct device *dev = adc3101->dev;
	int ret = 0;

	adc3101->rstn_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	if (!gpio_is_valid(adc3101->rstn_gpio)) {
		dev_err(dev, "Invalid reset gpio\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "left-pin-select",
					&adc3101->left_pin_select);
	if (!ret) {
		if (adc3101->left_pin_select > CH_SEL3X) {
			dev_err(dev, "wrong left pin selection\n");
			return -EINVAL;
		}
	} else {
		dev_err(dev, "left pin not selected\n");
		return ret;
	}

	ret = of_property_read_u32(np, "right-pin-select",
					&adc3101->right_pin_select);
	if (!ret) {
		if (adc3101->right_pin_select > CH_SEL3X) {
			dev_err(dev, "wrong right pin selection\n");
			return -EINVAL;
		}
	} else {
		dev_err(dev, "right pin not selected\n");
		return ret;
	}

	return 0;
}

static void adc3101_disable_regulators(struct adc3101_priv *adc3101)
{
	if (!IS_ERR(adc3101->supply_iov))
		regulator_disable(adc3101->supply_iov);

	if (!IS_ERR(adc3101->supply_dv))
		regulator_disable(adc3101->supply_dv);

	if (!IS_ERR(adc3101->supply_av))
		regulator_disable(adc3101->supply_av);
}

static int adc3101_setup_regulators(struct device *dev,
		struct adc3101_priv *adc3101)
{
	int ret = 0;

	adc3101->supply_iov = devm_regulator_get(dev, "iov");
	adc3101->supply_dv = devm_regulator_get_optional(dev, "dv");
	adc3101->supply_av = devm_regulator_get_optional(dev, "av");

	/* Check for the regulators */
	if (IS_ERR(adc3101->supply_iov)) {
		dev_err(dev, "Missing supply 'iov'\n");
		return PTR_ERR(adc3101->supply_iov);
	}

	if (IS_ERR(adc3101->supply_dv)) {
		dev_err(dev, "Missing supply 'dv'\n");
		return PTR_ERR(adc3101->supply_dv);
	}

	if (IS_ERR(adc3101->supply_av)) {
		dev_err(dev, "Missing supply 'av'\n");
		return PTR_ERR(adc3101->supply_av);
	}

	ret = regulator_enable(adc3101->supply_iov);
	if (ret) {
		dev_err(dev, "Failed to enable regulator iov\n");
		return ret;
	}

	ret = regulator_enable(adc3101->supply_dv);
	if (ret) {
		dev_err(dev, "Failed to enable regulator dv\n");
		goto error_dv;
	}

	ret = regulator_enable(adc3101->supply_av);
	if (ret) {
		dev_err(dev, "Failed to enable regulator av\n");
		goto error_av;
	}

	return 0;

error_av:
	regulator_disable(adc3101->supply_dv);

error_dv:
	regulator_disable(adc3101->supply_iov);
	return ret;
}

int adc3101_probe(struct device *dev, struct regmap *regmap)
{
	struct adc3101_priv *adc3101;
	struct device_node *np = dev->of_node;
	int ret;

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	adc3101 = devm_kzalloc(dev, sizeof(struct adc3101_priv),
			       GFP_KERNEL);
	if (adc3101 == NULL)
		return -ENOMEM;

	adc3101->dev = dev;
	dev_set_drvdata(dev, adc3101);
	adc3101->regmap = regmap;

	if (np) {
		ret = adc3101_parse_dt(adc3101, np);
		if (ret) {
			dev_err(dev, "Failed to parse DT node\n");
			return ret;
		}
	} else {
		dev_err(dev, "Could not parse DT node\n");
		return -EINVAL;
	}

	/* setting default values */
	adc3101->fmt = SND_SOC_DAIFMT_DSP_A;
	adc3101->sysclk = 0;
	adc3101->tdm_offset = 0;
	adc3101->tdm_additional_offset = 0;
	adc3101->minus6db_left_input = 1;
	adc3101->minus6db_right_input = 1;

	if (gpio_is_valid(adc3101->rstn_gpio)) {
		ret = devm_gpio_request_one(dev, adc3101->rstn_gpio,
				GPIOF_OUT_INIT_LOW, "tlv320adc3101 rstn");
		if (ret != 0)
			return ret;
	}

	ret = adc3101_setup_regulators(dev, adc3101);
	if (ret) {
		dev_err(dev, "Failed to setup regulators\n");
		return ret;
	}

	ret = devm_snd_soc_register_component(dev,
			&soc_component_dev_adc3101, &adc3101_dai, 1);
	if (ret) {
		dev_err(dev, "Failed to register component\n");
		adc3101_disable_regulators(adc3101);
		return ret;
	}

	if (gpio_is_valid(adc3101->rstn_gpio)) {
		gpio_set_value(adc3101->rstn_gpio, 0);
		ndelay(10);
		gpio_set_value(adc3101->rstn_gpio, 1);
		ndelay(10);
	} else {
		dev_err(dev,
			"invalid reset gpio. Your adc may not work properly\n");
	}

	/* SW reset */
	ret = regmap_write(adc3101->regmap, ADC3101_RESET, ADC3101_RESET_VALUE);
	if (ret) {
		dev_err(adc3101->dev,
			"failed to write the reset register: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(adc3101_probe);

int adc3101_remove(struct device *dev)
{
	struct adc3101_priv *adc3101 = dev_get_drvdata(dev);

	adc3101_disable_regulators(adc3101);

	return 0;
}
EXPORT_SYMBOL(adc3101_remove);

MODULE_DESCRIPTION("ASoC tlv320adc3101 codec driver");
MODULE_AUTHOR("Nicolas Belin <nbelin.com>");
MODULE_LICENSE("GPL v2");
