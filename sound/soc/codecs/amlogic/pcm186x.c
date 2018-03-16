/*
 * pcm186x.c - Texas Instruments PCM186x Universal Audio ADC
 *
 * Copyright (C) 2015-2016 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Andreas Dannenberg <dannenberg@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "pcm186x.h"

#define PCM186X_MAX_NR_BUSY_CHECKS		16

static const char * const pcm186x_supply_names[] = {
	"avdd",		/* Analog power supply. Connect to 3.3-V supply. */
	"dvdd",		/* Digital power supply. Connect to 3.3-V supply. */
	"iovdd"		/* I/O power supply. Connect to 3.3-V or 1.8-V. */
};
#define PCM186x_NUM_SUPPLIES	ARRAY_SIZE(pcm186x_supply_names)

struct pcm186x_priv {
	struct regmap *regmap;
	struct regulator_bulk_data supplies[PCM186x_NUM_SUPPLIES];
	enum pcm186x_type type;
	int irq;
	unsigned int sysclk;
	unsigned int pllclk;		/* 0 if PLL is not used */
	unsigned int dai_format;
	unsigned int tdm_offset;
	unsigned int slots_mask;
};

/*
 * The PCM186x DSP register map has holes in it, hence we define a table so we
 * can process all locations elements in an iterative manner.
 */
static const u8 pcm186x_dsp_registers[] = {
	PCM186X_DSP_MIX1_CH1L,
	PCM186X_DSP_MIX1_CH1R,
	PCM186X_DSP_MIX1_CH2L,
	PCM186X_DSP_MIX1_CH2R,
	PCM186X_DSP_MIX1_I2SL,
	PCM186X_DSP_MIX1_I2SR,
	PCM186X_DSP_MIX2_CH1L,
	PCM186X_DSP_MIX2_CH1R,
	PCM186X_DSP_MIX2_CH2L,
	PCM186X_DSP_MIX2_CH2R,
	PCM186X_DSP_MIX2_I2SL,
	PCM186X_DSP_MIX2_I2SR,
	PCM186X_DSP_MIX3_CH1L,
	PCM186X_DSP_MIX3_CH1R,
	PCM186X_DSP_MIX3_CH2L,
	PCM186X_DSP_MIX3_CH2R,
	PCM186X_DSP_MIX3_I2SL,
	PCM186X_DSP_MIX3_I2SR,
	PCM186X_DSP_MIX4_CH1L,
	PCM186X_DSP_MIX4_CH1R,
	PCM186X_DSP_MIX4_CH2L,
	PCM186X_DSP_MIX4_CH2R,
	PCM186X_DSP_MIX4_I2SL,
	PCM186X_DSP_MIX4_I2SR,
	PCM186X_DSP_LPF_B0,
	PCM186X_DSP_LPF_B1,
	PCM186X_DSP_LPF_B2,
	PCM186X_DSP_LPF_A1,
	PCM186X_DSP_LPF_A2,
	PCM186X_DSP_HPF_B0,
	PCM186X_DSP_HPF_B1,
	PCM186X_DSP_HPF_B2,
	PCM186X_DSP_HPF_A1,
	PCM186X_DSP_HPF_A2,
	PCM186X_DSP_LOSS_THRESH,
	PCM186X_DSP_RES_THRESH,
};

#define PCM186X_DSP_NR_REGISTERS	ARRAY_SIZE(pcm186x_dsp_registers)

static int pcm186x_dsp_access_enable(struct device *dev, struct regmap *map)
{

	int i, ret;
	unsigned int val;

	dev_dbg(dev, "%s()\n", __func__);

	/*
	 * Switch page using the datasheet-recommended special flow for
	 * accessing the memory-mapped DSP registers.
	 */
	for (i = 0; i < PCM186X_MAX_NR_BUSY_CHECKS; i++) {
		ret = regmap_write(map, PCM186X_PAGE, PCM186X_PAGE_1);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}

		/*
		 * Writing the page register at least twice before start
		 * checking the MMAP status register for it to become zero.
		 */
		if (i < 1)
			continue;
		ret = regmap_read(map, PCM186X_MMAP_STAT_CTRL, &val);
		if (ret) {
			dev_err(dev, "Error reading device: %d\n", ret);
			return ret;
		}
		if (!val)
			return 0;
	}

	/*
	 * One reason for encountering a timeout error can be that the PCM186x
	 * DSPs are not being clocked because the driving clock source has been
	 * turned off which can happen in a number of different operating
	 * scenarios.
	 */
	dev_err(dev, "Timeout accessing DSP memory\n");

	return -EBUSY;
	return 0;
}

static int pcm186x_dsp_coefficients_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct device *dev = codec->dev;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct regmap *map = priv->regmap;
	int i, j, ret;
	unsigned int val;

	dev_dbg(dev, "%s()\n", __func__);

	ret = pcm186x_dsp_access_enable(dev, map);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(pcm186x_dsp_registers); i++) {
		/* Set virtual address */
		ret = regmap_write(map, PCM186X_MMAP_ADDRESS,
				   pcm186x_dsp_registers[i]);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}
		/* Issue read request */
		ret = regmap_write(map, PCM186X_MMAP_STAT_CTRL,
				   PCM186X_MMAP_STAT_R_REQ);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}

		/* Wait for the DSP register read to finish but not forever */
		for (j = 0; j < PCM186X_MAX_NR_BUSY_CHECKS; j++) {
			ret = regmap_read(map, PCM186X_MMAP_STAT_CTRL, &val);
			if (ret) {
				dev_err(dev, "Error reading device: %d\n", ret);
				return ret;
			}
			if (!val)
				break;
		}

		if (j == PCM186X_MAX_NR_BUSY_CHECKS) {
			dev_err(dev, "Timeout reading DSP coefficients\n");
			return -EBUSY;
		}

		/* Get, assemble, and populate return data - 24 bits each */
		ret = regmap_read(map, PCM186X_MEM_RDATA0, &val);
		if (ret) {
			dev_err(dev, "Error reading device: %d\n", ret);
			return ret;
		}
		ucontrol->value.integer.value[i] = val;

		ret = regmap_read(map, PCM186X_MEM_RDATA1, &val);
		if (ret) {
			dev_err(dev, "Error reading device: %d\n", ret);
			return ret;
		}
		ucontrol->value.integer.value[i] |= val << 8;

		ret = regmap_read(map, PCM186X_MEM_RDATA2, &val);
		if (ret) {
			dev_err(dev, "Error reading device: %d\n", ret);
			return ret;
		}
		ucontrol->value.integer.value[i] |= val << 16;
	}
	return 0;
}

static int pcm186x_dsp_coefficients_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct device *dev = codec->dev;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct regmap *map = priv->regmap;
	int i, j, ret;
	unsigned int val;

	dev_dbg(dev, "%s()\n", __func__);

	ret = pcm186x_dsp_access_enable(dev, map);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(pcm186x_dsp_registers); i++) {
		/* Set virtual address */
		ret = regmap_write(map, PCM186X_MMAP_ADDRESS,
				   pcm186x_dsp_registers[i]);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}

		/* Set data to write - 24 bits each */
		ret = regmap_write(map, PCM186X_MEM_WDATA0,
				   ucontrol->value.integer.value[i] & 0xff);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}

		ret = regmap_write(priv->regmap, PCM186X_MEM_WDATA1,
				   (ucontrol->value.integer.value[i] &
				    0xff00) >> 8);
		if (ret) {
			dev_err(codec->dev, "Error writing device: %d\n", ret);
			return ret;
		}

		ret = regmap_write(map, PCM186X_MEM_WDATA2,
				   (ucontrol->value.integer.value[i] &
				    0xff0000) >> 16);
		if (ret) {
			dev_err(dev, "Error writing device: %d\n", ret);
			return ret;
		}

		/*
		 * Issue write request. Note that DSP Internal memory can only
		 * be written to when the DSP is running, requesting a WREQ=1
		 * will have no effect otherwise. This is of relevance if the
		 * codec is running as a clock slave, and the clocks stop.
		 */
		ret = regmap_write(map, PCM186X_MMAP_STAT_CTRL,
				   PCM186X_MMAP_STAT_W_REQ);
		if (ret) {
			dev_err(dev, "Error to write device: %d\n", ret);
			return ret;
		}

		/* Wait for the DSP register write to finish but not forever */
		for (j = 0; j < PCM186X_MAX_NR_BUSY_CHECKS; j++) {
			ret = regmap_read(map, PCM186X_MMAP_STAT_CTRL, &val);
			if (ret) {
				dev_err(dev, "Error reading device: %d\n", ret);
				return ret;
			}
			if (!val)
				break;
		}

		if (j == PCM186X_MAX_NR_BUSY_CHECKS) {
			dev_err(dev, "Timeout writing DSP coefficients\n");
			return -EBUSY;
		}
	}
	return 0;
}

static const DECLARE_TLV_DB_SCALE(pcm186x_pga_tlv, -1200, 50, 0);

static const struct snd_kcontrol_new pcm1863_snd_controls[] = {
	SOC_DOUBLE_R_S_TLV("Analog Gain", PCM186X_PGA_VAL_CH1_L,
			   PCM186X_PGA_VAL_CH1_R, 0, -24, 80, 7, 0,
			   pcm186x_pga_tlv),
	SND_SOC_BYTES_EXT("DSP Coefficients", PCM186X_DSP_NR_REGISTERS * 4,
			  pcm186x_dsp_coefficients_get,
			  pcm186x_dsp_coefficients_put),
};

static const struct snd_kcontrol_new pcm1865_snd_controls[] = {
	SOC_DOUBLE_R_S_TLV("ADC1 Analog Gain", PCM186X_PGA_VAL_CH1_L,
			   PCM186X_PGA_VAL_CH1_R, 0, -24, 80, 7, 0,
			   pcm186x_pga_tlv),
	SOC_DOUBLE_R_S_TLV("ADC2 Analog Gain", PCM186X_PGA_VAL_CH2_L,
			   PCM186X_PGA_VAL_CH2_R, 0, -24, 80, 7, 0,
			   pcm186x_pga_tlv),
	SND_SOC_BYTES_EXT("DSP Coefficients", PCM186X_DSP_NR_REGISTERS * 4,
			  pcm186x_dsp_coefficients_get,
			  pcm186x_dsp_coefficients_put),
};

const unsigned int pcm186x_adc_input_channel_sel_value[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x20, 0x30
};

static const char * const pcm186x_adcl_input_channel_sel_text[] = {
	"No Select",
	"VINL1[SE]",					/* Default for ADC1L */
	"VINL2[SE]",					/* Default for ADC2L */
	"VINL2[SE] + VINL1[SE]",
	"VINL3[SE]",
	"VINL3[SE] + VINL1[SE]",
	"VINL3[SE] + VINL2[SE]",
	"VINL3[SE] + VINL2[SE] + VINL1[SE]",
	"VINL4[SE]",
	"VINL4[SE] + VINL1[SE]",
	"VINL4[SE] + VINL2[SE]",
	"VINL4[SE] + VINL2[SE] + VINL1[SE]",
	"VINL4[SE] + VINL3[SE]",
	"VINL4[SE] + VINL3[SE] + VINL1[SE]",
	"VINL4[SE] + VINL3[SE] + VINL2[SE]",
	"VINL4[SE] + VINL3[SE] + VINL2[SE] + VINL1[SE]",
	"{VIN1P, VIN1M}[DIFF]",
	"{VIN4P, VIN4M}[DIFF]",
	"{VIN1P, VIN1M}[DIFF] + {VIN4P, VIN4M}[DIFF]"
};

static const char * const pcm186x_adcr_input_channel_sel_text[] = {
	"No Select",
	"VINR1[SE]",					/* Default for ADC1R */
	"VINR2[SE]",					/* Default for ADC2R */
	"VINR2[SE] + VINR1[SE]",
	"VINR3[SE]",
	"VINR3[SE] + VINR1[SE]",
	"VINR3[SE] + VINR2[SE]",
	"VINR3[SE] + VINR2[SE] + VINR1[SE]",
	"VINR4[SE]",
	"VINR4[SE] + VINR1[SE]",
	"VINR4[SE] + VINR2[SE]",
	"VINR4[SE] + VINR2[SE] + VINR1[SE]",
	"VINR4[SE] + VINR3[SE]",
	"VINR4[SE] + VINR3[SE] + VINR1[SE]",
	"VINR4[SE] + VINR3[SE] + VINR2[SE]",
	"VINR4[SE] + VINR3[SE] + VINR2[SE] + VINR1[SE]",
	"{VIN2P, VIN2M}[DIFF]",
	"{VIN3P, VIN3M}[DIFF]",
	"{VIN2P, VIN2M}[DIFF] + {VIN3P, VIN3M}[DIFF]"
};

static const struct soc_enum pcm186x_adc_input_channel_sel[] = {
	SOC_VALUE_ENUM_SINGLE(PCM186X_ADC1_INPUT_SEL_L, 0,
			      PCM186X_ADC_INPUT_SEL_MASK,
			      ARRAY_SIZE(pcm186x_adcl_input_channel_sel_text),
			      pcm186x_adcl_input_channel_sel_text,
			      pcm186x_adc_input_channel_sel_value),
	SOC_VALUE_ENUM_SINGLE(PCM186X_ADC1_INPUT_SEL_R, 0,
			      PCM186X_ADC_INPUT_SEL_MASK,
			      ARRAY_SIZE(pcm186x_adcr_input_channel_sel_text),
			      pcm186x_adcr_input_channel_sel_text,
			      pcm186x_adc_input_channel_sel_value),
	SOC_VALUE_ENUM_SINGLE(PCM186X_ADC2_INPUT_SEL_L, 0,
			      PCM186X_ADC_INPUT_SEL_MASK,
			      ARRAY_SIZE(pcm186x_adcl_input_channel_sel_text),
			      pcm186x_adcl_input_channel_sel_text,
			      pcm186x_adc_input_channel_sel_value),
	SOC_VALUE_ENUM_SINGLE(PCM186X_ADC2_INPUT_SEL_R, 0,
			      PCM186X_ADC_INPUT_SEL_MASK,
			      ARRAY_SIZE(pcm186x_adcr_input_channel_sel_text),
			      pcm186x_adcr_input_channel_sel_text,
			      pcm186x_adc_input_channel_sel_value),
};

static const struct snd_kcontrol_new pcm186x_adc_mux_controls[] = {
	SOC_DAPM_ENUM("ADC1 Left Input", pcm186x_adc_input_channel_sel[0]),
	SOC_DAPM_ENUM("ADC1 Right Input", pcm186x_adc_input_channel_sel[1]),
	SOC_DAPM_ENUM("ADC2 Left Input", pcm186x_adc_input_channel_sel[2]),
	SOC_DAPM_ENUM("ADC2 Right Input", pcm186x_adc_input_channel_sel[3]),
};

static const struct snd_soc_dapm_widget pcm1863_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("VINL1"),
	SND_SOC_DAPM_INPUT("VINR1"),
	SND_SOC_DAPM_INPUT("VINL2"),
	SND_SOC_DAPM_INPUT("VINR2"),
	SND_SOC_DAPM_INPUT("VINL3"),
	SND_SOC_DAPM_INPUT("VINR3"),
	SND_SOC_DAPM_INPUT("VINL4"),
	SND_SOC_DAPM_INPUT("VINR4"),

	SND_SOC_DAPM_MUX("ADC Left Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[0]),
	SND_SOC_DAPM_MUX("ADC Right Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[1]),

	/*
	 * Put the codec into SLEEP mode when not in use, allowing the
	 * Energysense mechanism to operate.
	 */
	SND_SOC_DAPM_ADC("ADC", "HiFi Capture", PCM186X_POWER_CTRL, 1,  0),
};

static const struct snd_soc_dapm_widget pcm1865_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("VINL1"),
	SND_SOC_DAPM_INPUT("VINR1"),
	SND_SOC_DAPM_INPUT("VINL2"),
	SND_SOC_DAPM_INPUT("VINR2"),
	SND_SOC_DAPM_INPUT("VINL3"),
	SND_SOC_DAPM_INPUT("VINR3"),
	SND_SOC_DAPM_INPUT("VINL4"),
	SND_SOC_DAPM_INPUT("VINR4"),

	SND_SOC_DAPM_MUX("ADC1 Left Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[0]),
	SND_SOC_DAPM_MUX("ADC1 Right Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[1]),
	SND_SOC_DAPM_MUX("ADC2 Left Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[2]),
	SND_SOC_DAPM_MUX("ADC2 Right Capture Source", SND_SOC_NOPM, 0, 0,
			 &pcm186x_adc_mux_controls[3]),

	/*
	 * Put the codec into SLEEP mode when not in use, allowing the
	 * Energysense mechanism to operate.
	 */
	SND_SOC_DAPM_ADC("ADC1", "HiFi Capture 1", PCM186X_POWER_CTRL, 1,  0),
	SND_SOC_DAPM_ADC("ADC2", "HiFi Capture 2", PCM186X_POWER_CTRL, 1,  0),
};

static const struct snd_soc_dapm_route pcm1863_dapm_routes[] = {
	{ "ADC Left Capture Source", NULL, "VINL1" },
	{ "ADC Left Capture Source", NULL, "VINR1" },
	{ "ADC Left Capture Source", NULL, "VINL2" },
	{ "ADC Left Capture Source", NULL, "VINR2" },
	{ "ADC Left Capture Source", NULL, "VINL3" },
	{ "ADC Left Capture Source", NULL, "VINR3" },
	{ "ADC Left Capture Source", NULL, "VINL4" },
	{ "ADC Left Capture Source", NULL, "VINR4" },

	{ "ADC", NULL, "ADC Left Capture Source" },

	{ "ADC Right Capture Source", NULL, "VINL1" },
	{ "ADC Right Capture Source", NULL, "VINR1" },
	{ "ADC Right Capture Source", NULL, "VINL2" },
	{ "ADC Right Capture Source", NULL, "VINR2" },
	{ "ADC Right Capture Source", NULL, "VINL3" },
	{ "ADC Right Capture Source", NULL, "VINR3" },
	{ "ADC Right Capture Source", NULL, "VINL4" },
	{ "ADC Right Capture Source", NULL, "VINR4" },

	{ "ADC", NULL, "ADC Right Capture Source" },
};

static const struct snd_soc_dapm_route pcm1865_dapm_routes[] = {
	{ "ADC1 Left Capture Source", NULL, "VINL1" },
	{ "ADC1 Left Capture Source", NULL, "VINR1" },
	{ "ADC1 Left Capture Source", NULL, "VINL2" },
	{ "ADC1 Left Capture Source", NULL, "VINR2" },
	{ "ADC1 Left Capture Source", NULL, "VINL3" },
	{ "ADC1 Left Capture Source", NULL, "VINR3" },
	{ "ADC1 Left Capture Source", NULL, "VINL4" },
	{ "ADC1 Left Capture Source", NULL, "VINR4" },

	{ "ADC1", NULL, "ADC1 Left Capture Source" },

	{ "ADC1 Right Capture Source", NULL, "VINL1" },
	{ "ADC1 Right Capture Source", NULL, "VINR1" },
	{ "ADC1 Right Capture Source", NULL, "VINL2" },
	{ "ADC1 Right Capture Source", NULL, "VINR2" },
	{ "ADC1 Right Capture Source", NULL, "VINL3" },
	{ "ADC1 Right Capture Source", NULL, "VINR3" },
	{ "ADC1 Right Capture Source", NULL, "VINL4" },
	{ "ADC1 Right Capture Source", NULL, "VINR4" },

	{ "ADC1", NULL, "ADC1 Right Capture Source" },

	{ "ADC2 Left Capture Source", NULL, "VINL1" },
	{ "ADC2 Left Capture Source", NULL, "VINR1" },
	{ "ADC2 Left Capture Source", NULL, "VINL2" },
	{ "ADC2 Left Capture Source", NULL, "VINR2" },
	{ "ADC2 Left Capture Source", NULL, "VINL3" },
	{ "ADC2 Left Capture Source", NULL, "VINR3" },
	{ "ADC2 Left Capture Source", NULL, "VINL4" },
	{ "ADC2 Left Capture Source", NULL, "VINR4" },

	{ "ADC2", NULL, "ADC2 Left Capture Source" },

	{ "ADC2 Right Capture Source", NULL, "VINL1" },
	{ "ADC2 Right Capture Source", NULL, "VINR1" },
	{ "ADC2 Right Capture Source", NULL, "VINL2" },
	{ "ADC2 Right Capture Source", NULL, "VINR2" },
	{ "ADC2 Right Capture Source", NULL, "VINL3" },
	{ "ADC2 Right Capture Source", NULL, "VINR3" },
	{ "ADC2 Right Capture Source", NULL, "VINL4" },
	{ "ADC2 Right Capture Source", NULL, "VINR4" },

	{ "ADC2", NULL, "ADC2 Right Capture Source" },
};

#define PCM186X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int pcm186x_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s()\n", __func__);

	return 0;
}

static int pcm186x_set_pll(struct snd_soc_codec *codec, unsigned int Fref,
			   unsigned int Fout)
{
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	u32 P, R, J, D;
	int ret = 0;

	/* Ensure automatic clock detection is disabled so we can set PLL */
	ret = regmap_update_bits(priv->regmap, PCM186X_CLK_CTRL,
				 PCM186X_CLK_CTRL_CLKDET_EN, 0);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}
	/*
	 * TODO: Implement algorithm to set FLL based on an arbitrary Fref
	 * rather than using specific sets of fixed values. The below is just
	 * a stop-gap measure while the overall clock setup approach is being
	 * finalized.
	 */
	if (Fref != 12000000) {
		dev_err(codec->dev, "unsupported FLL reference frequency: %u\n",
			Fref);
		return -EINVAL;
	}

	if (Fout == 90316800) {
		/** Fout = (Fref x R x J.D) / P **/
		/** Example:*/
		/**	Fref = 12MHz*/
		/**	Fout = (12MHz x 1 x 7.5264) / 1 = 90.3168MHz*/
		R = 1;
		J = 7;
		D = 5264;
		P = 1;
	} else if (Fout == 98304000) {
		R = 1;
		J = 16;
		D = 3840;
		P = 2;
	} else {
		dev_err(codec->dev, "unsupported FLL target frequency: %u\n",
			Fout);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s() R=%u J=%u D=%u P=%u\n", __func__, R, J, D, P);
	/* Program integer divider */
	ret = regmap_write(priv->regmap, PCM186X_PLL_P_DIV, P - 1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	/* Program integer multiplier */
	ret = regmap_write(priv->regmap, PCM186X_PLL_R_DIV, R - 1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	/* Program integer part of the fractional multiplier */
	ret = regmap_write(priv->regmap, PCM186X_PLL_J_DIV, J);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	/*
	 * Program 1/1000th fractional multiplier (0-9999), with the MSB written
	 * last as required by the device.
	 */
	ret = regmap_write(priv->regmap, PCM186X_PLL_D_DIV_LSB, D & 0xff);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	ret = regmap_write(priv->regmap, PCM186X_PLL_D_DIV_MSB, D >> 8);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	/* Source PLL by SCK and enable it */
	ret = regmap_write(priv->regmap, PCM186X_PLL_CTRL, PCM186X_PLL_CTRL_EN);
	if (ret < 0)
		dev_err(codec->dev, "failed to write register: %d\n", ret);
	return ret;

	return 0;
}

static int pcm186x_set_fmt(struct snd_soc_dai *dai, unsigned int format)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int clk_ctrl, pcm_cfg;
	int ret;

	dev_dbg(codec->dev, "%s() format=0x%x\n", __func__, format);

	switch (format & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		if (!priv->sysclk) {
			dev_err(codec->dev, "operating in master mode requires sysclock to be configured!\n");
			return -EINVAL;
		}

		/* Codec is bit/frame clock master */
		clk_ctrl = PCM186X_CLK_CTRL_MST_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		/* Codec is bit/frame clock slave */
		clk_ctrl = PCM186X_CLK_CTRL_SCK_SRC_PLL;
		break;
	default:
		return -EINVAL;
	}

	if (priv->pllclk) {
		if (!priv->sysclk) {
			dev_err(codec->dev, "using PLL requires sysclock to be configured!\n");
			return -EINVAL;
		}

		ret = pcm186x_set_pll(codec, priv->sysclk, priv->pllclk);
		if (ret < 0)
			return ret;

		/* Switch all clock trees to be sourced from the PLL */
		clk_ctrl |= PCM186X_CLK_CTRL_SCK_SRC_PLL |
			    PCM186X_CLK_CTRL_ADC_SRC_PLL |
			    PCM186X_CLK_CTRL_DSP2_SRC_PLL |
			    PCM186X_CLK_CTRL_DSP1_SRC_PLL;
	} else {
		/*
		 * If PLL is not explicitly configured we rely on the automatic
		 * clock detection mechanism in both master and slave modes that
		 * will use either XTAL, SCKI, or BCK as a reference clock. Note
		 * that unused clock inputs should be connected to GND for the
		 * automatic clock detection mechanism to work properly.
		 */
		clk_ctrl |= PCM186X_CLK_CTRL_CLKDET_EN;
	}

	ret = regmap_write(priv->regmap, PCM186X_CLK_CTRL, clk_ctrl);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	switch (format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		pcm_cfg = PCM186X_PCM_CFG_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		pcm_cfg = PCM186X_PCM_CFG_FMT_LEFTJ;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/*
		 * Fall through... DSP_A uses the same basic config as DSP_B
		 * except we need to shift the TDM output by one BCK cycle
		 * which we'll still need to configure.
		 */
	case SND_SOC_DAIFMT_DSP_B:
		pcm_cfg = PCM186X_PCM_CFG_FMT_TDM;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, PCM186X_PCM_CFG,
				 PCM186X_PCM_CFG_FMT_MASK, pcm_cfg);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}
	/*
	 * We only support the non-inverted clocks. Note that clock polarity
	 * depends on the actual FORMAT.
	 */
	//if ((format & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_NF)
	//	return -EINVAL;

	priv->dai_format = format;

	return 0;
}

static int pcm186x_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int first_slot, last_slot, tdm_offset;
	int ret;

	dev_dbg(codec->dev,
		"%s() tx_mask=0x%x rx_mask=0x%x slots=%d slot_width=%d\n",
		__func__, tx_mask, rx_mask, slots, slot_width);

	//if (!tx_mask) {
	//	dev_err(codec->dev, "tdm tx mask must not be 0\n");
	//	return -EINVAL;
	//}
	tx_mask = priv->slots_mask;
	first_slot = __ffs(tx_mask);
	last_slot = __fls(tx_mask);

	if (last_slot - first_slot != hweight32(tx_mask) - 1) {
		dev_err(codec->dev, "tdm tx mask must be contiguous\n");
		return -EINVAL;
	}

	tdm_offset = first_slot * slot_width;

	dev_info(codec->dev, "tdm_offset:0x%x\n", tdm_offset);

	if (tdm_offset > 255) {
		dev_err(codec->dev, "tdm tx slot selection out of bounds\n");
		return -EINVAL;
	}

	priv->tdm_offset = tdm_offset;
	//tdm_offset += 1;
	ret = regmap_write(priv->regmap, PCM186X_TDM_TX_OFFSET, tdm_offset);
	if (ret < 0)
		dev_err(codec->dev, "failed to write register: %d\n", ret);
	return ret;
}

static int pcm186x_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int rate = params_rate(params);
	unsigned int format = params_format(params);
	unsigned int width = params_width(params);
	unsigned int channels = params_channels(params);
	unsigned int pcm_cfg;
	unsigned int master_clk;
	unsigned int div_lrck;
	unsigned int div_bck;
	bool is_tdm_mode;
	unsigned int tdm_offset;
	unsigned int tdm_tx_sel;
	int ret;

	dev_dbg(codec->dev, "%s() rate=%u format=0x%x width=%u channels=%u\n",
		__func__, rate, format, width, channels);

	switch (width) {
	case 16:
		pcm_cfg = PCM186X_PCM_CFG_RX_WLEN_16 |
			  PCM186X_PCM_CFG_TX_WLEN_16;
		break;
	case 20:
		pcm_cfg = PCM186X_PCM_CFG_RX_WLEN_20 |
			  PCM186X_PCM_CFG_TX_WLEN_20;
		break;
	case 24:
		pcm_cfg = PCM186X_PCM_CFG_RX_WLEN_24 |
			  PCM186X_PCM_CFG_TX_WLEN_24;
		break;
	case 32:
		pcm_cfg = PCM186X_PCM_CFG_RX_WLEN_32 |
			  PCM186X_PCM_CFG_TX_WLEN_32;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, PCM186X_PCM_CFG,
				 PCM186X_PCM_CFG_RX_WLEN_MASK |
				 PCM186X_PCM_CFG_TX_WLEN_MASK,
				 pcm_cfg);
	if (ret < 0) {
		dev_err(codec->dev, "failed to write register: %d\n", ret);
		return ret;
	}

	/*
	 * In DSP/TDM mode, the LRCLK divider must be 256 as per datasheet.
	 * Also, complete the codec serial audio interface format configuration
	 * by setting the appropriate TDM offset. For all other operating modes,
	 * size the LRCLK period just long enough to fit the used bits.
	 */
	switch (priv->dai_format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		is_tdm_mode = true;
		tdm_offset = priv->tdm_offset + 1;
		div_lrck = 256;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		is_tdm_mode = true;
		tdm_offset = priv->tdm_offset;
		div_lrck = 256;
		break;
	default:
		is_tdm_mode = false;
		tdm_offset = 0;
		div_lrck = width * channels;
	}

	if (is_tdm_mode) {
		/* Select TDM transmission data */
		switch (channels) {
		case 2:
			tdm_tx_sel = PCM186X_TDM_TX_SEL_2CH;
			break;
		case 4:
			tdm_tx_sel = PCM186X_TDM_TX_SEL_4CH;
			break;
		case 6:
			tdm_tx_sel = PCM186X_TDM_TX_SEL_6CH;
			break;
		case 8:
			tdm_tx_sel = PCM186X_TDM_TX_SEL_2CH;
			break;
		default:
			return -EINVAL;
		}

		ret = regmap_update_bits(priv->regmap, PCM186X_TDM_TX_SEL,
					 PCM186X_TDM_TX_SEL_MASK, tdm_tx_sel);
		if (ret < 0) {
			dev_err(codec->dev, "failed to write register: %d\n",
				ret);
			return ret;
		}

		/* Configure 1/256 duty cycle for LRCK */
		ret = regmap_update_bits(priv->regmap, PCM186X_PCM_CFG,
					 PCM186X_PCM_CFG_TDM_LRCK_MODE,
					 PCM186X_PCM_CFG_TDM_LRCK_MODE);
		if (ret < 0) {
			dev_err(codec->dev, "failed to write register: %d\n",
				ret);
			return ret;
		}
		/* (Re-)configure the final TDM offset */
		ret = regmap_write(priv->regmap, PCM186X_TDM_TX_OFFSET,
				   tdm_offset);
		if (ret < 0) {
			dev_err(codec->dev, "failed to write register: %d\n",
				ret);
			return ret;
		}
	}

	/* Only configure clock dividers in master mode. */
	if ((priv->dai_format & SND_SOC_DAIFMT_MASTER_MASK) ==
	    SND_SOC_DAIFMT_CBM_CFM) {
		/* Use PLL output frequency if PLL is enabled */
		master_clk = priv->pllclk ? priv->pllclk : priv->sysclk;

		div_bck = master_clk / (div_lrck * rate);

		dev_dbg(codec->dev,
			"%s() master_clk=%u div_bck=%u div_lrck=%u\n",
			__func__, master_clk, div_bck, div_lrck);
		ret = regmap_write(priv->regmap, PCM186X_BCK_DIV, div_bck - 1);
		if (ret < 0) {
			dev_err(codec->dev,
				"failed to write register: %d\n", ret);
			return ret;
		}

		ret = regmap_write(priv->regmap, PCM186X_LRK_DIV,
				   div_lrck - 1);
		if (ret < 0) {
			dev_err(codec->dev, "failed to write register: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static int pcm186x_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *priv = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s() clk_id=%d freq=%u dir=%d\n",
		__func__, clk_id, freq, dir);

	priv->sysclk = freq;

	return 0;
}

const struct snd_soc_dai_ops pcm186x_dai_ops = {
	.startup = pcm186x_startup,
	.set_fmt = pcm186x_set_fmt,
	.set_tdm_slot = pcm186x_set_tdm_slot,
	.hw_params = pcm186x_hw_params,
	.set_sysclk = pcm186x_set_dai_sysclk,
};

static struct snd_soc_dai_driver pcm1863_dai = {
	.name = "pcm1863-aif",
	.capture = {
		 .stream_name = "AIF Capture",
		 .channels_min = 1,
		 .channels_max = 8,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = PCM186X_FORMATS,
	 },
	.playback = { /*dummy for device node*/
		 .stream_name = "AIF Playback",
		 .channels_min = 1,
		 .channels_max = 8,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = PCM186X_FORMATS,
	 },
	.ops = &pcm186x_dai_ops,
};

static struct snd_soc_dai_driver pcm1865_dai = {
	.name = "pcm1865-aif",
	.capture = {
		 .stream_name = "AIF Capture",
		 .channels_min = 1,
		 .channels_max = 8,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = PCM186X_FORMATS,
	 },
	.playback = { /*dummy for device node*/
		 .stream_name = "AIF Playback",
		 .channels_min = 1,
		 .channels_max = 8,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = PCM186X_FORMATS,
	 },
	.ops = &pcm186x_dai_ops,
};

static int pcm186x_codec_probe(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s()\n", __func__);

	return 0;
}

static int pcm186x_codec_remove(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s()\n", __func__);

	return 0;
}

static struct regmap *pcm186x_get_regmap(struct device *dev)
{
	struct pcm186x_priv *priv = dev_get_drvdata(dev);

	return priv->regmap;
}

static int pcm186x_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "%s() level=%d\n", __func__, level);

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

static struct snd_soc_codec_driver soc_codec_dev_pcm1863 = {
	.probe = pcm186x_codec_probe,
	.remove = pcm186x_codec_remove,
	.get_regmap = pcm186x_get_regmap,

	.set_bias_level = pcm186x_set_bias_level,

	.component_driver = {
		.controls = pcm1863_snd_controls,
		.num_controls = ARRAY_SIZE(pcm1863_snd_controls),
		.dapm_widgets = pcm1863_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pcm1863_dapm_widgets),
		.dapm_routes = pcm1863_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(pcm1863_dapm_routes),
	}
};

static struct snd_soc_codec_driver soc_codec_dev_pcm1865 = {
	.probe = pcm186x_codec_probe,
	.remove = pcm186x_codec_remove,
	.get_regmap = pcm186x_get_regmap,

	.set_bias_level = pcm186x_set_bias_level,

	.component_driver = {
		.controls = pcm1865_snd_controls,
		.num_controls = ARRAY_SIZE(pcm1865_snd_controls),
		.dapm_widgets = pcm1865_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pcm1865_dapm_widgets),
		.dapm_routes = pcm1865_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(pcm1865_dapm_routes),
	}
};

static bool pcm186x_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PCM186X_PAGE:
	case PCM186X_DEVICE_STATUS:
	case PCM186X_FSAMPLE_STATUS:
	case PCM186X_DIV_STATUS:
	case PCM186X_CLK_STATUS:
	case PCM186X_SUPPLY_STATUS:
	case PCM186X_MMAP_STAT_CTRL:
	case PCM186X_MMAP_ADDRESS:
		return true;
	}

	return false;
}

/*
 * The PCM186x's page register is located on every page, allowing to program it
 * without having to switch pages. Take advantage of this by defining the range
 * such to have this register located inside the data window.
 */
static const struct regmap_range_cfg pcm186x_range = {
	.name = "Pages",
	.range_max = PCM186X_MAX_REGISTER,
	.selector_reg = PCM186X_PAGE,
	.selector_mask = 0xff,
	.window_len = PCM186X_PAGE_LEN,
};

const struct regmap_config pcm186x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.volatile_reg = pcm186x_volatile,

	.ranges = &pcm186x_range,
	.num_ranges = 1,

	.max_register = PCM186X_MAX_REGISTER,
/*
 * TODO: Activate register map cache. It has been temporarily deactivated to
 * eliminate a potential source of trouble during driver development.
 */
	/*.reg_defaults = pcm186x_reg_defaults,*/
	/*.num_reg_defaults = ARRAY_SIZE(pcm186x_reg_defaults),*/
	/*.cache_type = REGCACHE_RBTREE,*/
};
EXPORT_SYMBOL_GPL(pcm186x_regmap);

static irqreturn_t pcm186x_irq(int irq, void *_dev)
{
	struct device *dev = _dev;

	dev_dbg(dev, "%s()\n", __func__);

	return IRQ_HANDLED;
}

int pcm186x_probe(struct device *dev, enum pcm186x_type type, int irq,
		  struct regmap *regmap)
{
	struct pcm186x_priv *priv;
	int i, ret;

	dev_dbg(dev, "%s()\n", __func__);

	priv = devm_kzalloc(dev, sizeof(struct pcm186x_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);
	priv->type = type;
	priv->irq = irq;
	priv->regmap = regmap;

	for (i = 0; i < ARRAY_SIZE(priv->supplies); i++)
		priv->supplies[i].supply = pcm186x_supply_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(priv->supplies),
				      priv->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to request supplies: %d\n", ret);
		return ret;
	}

	/* Reset device registers for a consistent power-on like state */
	ret = regmap_write(regmap, PCM186X_PAGE, PCM186X_RESET);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}
	ret = regmap_write(regmap, PCM186X_PAGE, 0x00);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default L/R GAIN*/
	ret = regmap_write(regmap, PCM186X_PGA_VAL_CH1_L, 0x0C);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}
	ret = regmap_write(regmap, PCM186X_PGA_VAL_CH1_R, 0x0C);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	ret = regmap_write(regmap, PCM186X_PGA_VAL_CH2_L, 0x0C);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}
	ret = regmap_write(regmap, PCM186X_PGA_VAL_CH2_R, 0x0C);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default ADC1 Input Channel Select*/
	ret = regmap_write(regmap, PCM186X_ADC1_INPUT_SEL_L, 0x02);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}
	ret = regmap_write(regmap, PCM186X_ADC1_INPUT_SEL_R, 0x02);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default ADC2 Input Channel Select*/
	ret = regmap_write(regmap, PCM186X_ADC2_INPUT_SEL_L, 0x60);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}
	ret = regmap_write(regmap, PCM186X_ADC2_INPUT_SEL_R, 0x60);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default GPIO0-1 Config*/
	ret = regmap_write(regmap, PCM186X_GPIO1_0_CTRL, 0x00);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default GPIO02-3 Config*/
	ret = regmap_write(regmap, PCM186X_GPIO3_2_CTRL, 0x00);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

	/*default PULL DOWN Config*/
	ret = regmap_write(regmap, PCM186X_GPIO_PULL_CTRL, 0xf0);
	if (ret != 0) {
		dev_err(dev, "failed to write device: %d\n", ret);
		goto err_disable_reg;
	}

#ifdef CONFIG_OF
	if (dev->of_node) {
		const struct device_node *np = dev->of_node;
		u32 val;

		/*
		 * If 'pllclk' is provided, we'll configure and use the PLL as
		 * the main clock source rather than the specified system clock.
		 * Note that this property may become obsolete once the final
		 * (automatic) clock setup algorithm is implemented.
		 */
		if (of_property_read_u32(np, "pllclk", &val) >= 0)
			priv->pllclk = val;

		if (of_property_read_u32(np, "slots_mask", &val) >= 0) {
			priv->slots_mask = val;
			dev_info(dev, "slots_mask:0x%x\n", val);
		}


	}
#endif

	if (irq > 0) {
		ret = request_threaded_irq(irq, NULL, pcm186x_irq,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "pcm186x", dev);
		if (ret) {
			dev_err(dev, "failed to request IRQ #%d\n", irq);
			goto err_disable_reg;
		}
		dev_info(dev, "using IRQ #%d", irq);
	}

	switch (priv->type) {
	case PCM1865:
	case PCM1864:
		ret = snd_soc_register_codec(dev, &soc_codec_dev_pcm1865,
					     &pcm1865_dai, 1);
		break;
	case PCM1863:
	case PCM1862:
		/* Fall through... */
	default:
		ret = snd_soc_register_codec(dev, &soc_codec_dev_pcm1863,
					     &pcm1863_dai, 1);
	}
	if (ret != 0) {
		dev_err(dev, "failed to register CODEC: %d\n", ret);
		goto err_free_irq;
	}

	dev_dbg(dev, "registered codec type: %d\n", priv->type);

	return 0;

err_free_irq:
	if (irq > 0)
		free_irq(irq, dev);

err_disable_reg:
	regulator_bulk_disable(ARRAY_SIZE(priv->supplies), priv->supplies);

	return ret;
}
EXPORT_SYMBOL_GPL(pcm186x_probe);

int pcm186x_remove(struct device *dev)
{
	struct pcm186x_priv *priv = dev_get_drvdata(dev);

	snd_soc_unregister_codec(dev);

	if (priv->irq > 0)
		free_irq(priv->irq, dev);

	regulator_bulk_disable(ARRAY_SIZE(priv->supplies), priv->supplies);

	return 0;
}
EXPORT_SYMBOL_GPL(pcm186x_remove);

#ifdef CONFIG_PM
static int pcm186x_suspend(struct device *dev)
{
	struct pcm186x_priv *priv = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s()\n", __func__);
	ret = regmap_update_bits(priv->regmap, PCM186X_POWER_CTRL,
				 PCM186X_PWR_CTRL_PWRDN,
				 PCM186X_PWR_CTRL_PWRDN);
	if (ret != 0) {
		dev_err(dev, "failed to write register: %d\n", ret);
		return ret;
	}
	ret = regulator_bulk_disable(ARRAY_SIZE(priv->supplies),
				     priv->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to disable supplies: %d\n", ret);
		return ret;
	}

	return 0;
}

static int pcm186x_resume(struct device *dev)
{
	struct pcm186x_priv *priv = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	ret = regulator_bulk_enable(ARRAY_SIZE(priv->supplies),
				    priv->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(priv->regmap, PCM186X_POWER_CTRL,
				 PCM186X_PWR_CTRL_PWRDN, 0);
	if (ret != 0) {
		dev_err(dev, "failed to write register: %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

const struct dev_pm_ops pcm186x_pm_ops = {
	SET_RUNTIME_PM_OPS(pcm186x_suspend, pcm186x_resume, NULL)
};
EXPORT_SYMBOL_GPL(pcm186x_pm_ops);

MODULE_DESCRIPTION("PCM186x Universal Audio ADC driver");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_LICENSE("GPL");
