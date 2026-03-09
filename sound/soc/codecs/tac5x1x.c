// SPDX-License-Identifier: GPL-2.0
//
// tac5x1x.c
//
// Copyright 2024 Texas Instruments, Inc.
//
// Author: Kevin Lu <kevin-lu@ti.com>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
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

#include "tac5x1x.h"

enum tac5x1x_type {
	TAC5111 = 0,
	TAC5112,
	TAC5211,
	TAC5212,
	TAC5311,
	TAC5312,
	TAC5411,
	TAC5412,
};

const struct of_device_id tac5x1x_of_match[] = {
	{ .compatible = "ti,tac5111" },
	{ .compatible = "ti,tac5112" },
	{ .compatible = "ti,tac5211" },
	{ .compatible = "ti,tac5212" },
	{ .compatible = "ti,tac5311" },
	{ .compatible = "ti,tac5312" },
	{ .compatible = "ti,tac5411" },
	{ .compatible = "ti,tac5412" },
	{}
};
MODULE_DEVICE_TABLE(of, tac5x1x_of_match);

const struct i2c_device_id tac5x1x_id[] = {
	{ "tac5111", TAC5111 },
	{ "tac5112", TAC5112 },
	{ "tac5211", TAC5211 },
	{ "tac5212", TAC5212 },
	{ "tac5311", TAC5311 },
	{ "tac5312", TAC5312 },
	{ "tac5411", TAC5411 },
	{ "tac5412", TAC5412 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tac5x1x_id);

static const struct regmap_range_cfg tac5x1x_ranges[] = {
	{
		.range_min = 0,
		.range_max = 12 * 128,
		.selector_reg = TAC_PAGE_SELECT,
		.selector_mask = 0xff,
		.selector_shift = 0,
		.window_start = 0,
		.window_len = 128,
	},
};

struct regmap_config tac5x1x_regmap = {
	.max_register = 12 * 128,
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.ranges = tac5x1x_ranges,
	.num_ranges = ARRAY_SIZE(tac5x1x_ranges),
};

struct tac5x1x_setup_gpio {
	int gpio_func[3];
	int gpi1_func;
};

struct tac5x1x_priv {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct device *dev;
	enum tac5x1x_type codec_type;
	bool pll_setup;
	int irq;
	int vref_vg;
	int adc_en;
	int dac_en;
	int micbias_en;
	int micbias_vg;
	int agc_en;
	int uad_en;
	int vad_en;
	int uag_en;
	int dac_mute;
	int dac_dig_vol[4];
	int dac_chns;
	struct mutex mutex;
	struct snd_soc_jack *jack;
	struct regulator *supply_avdd;
	struct regulator *supply_iovdd;
	struct tac5x1x_setup_gpio *gpio_setup;
};

static int tac5x1x_set_GPO1_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;
	int gpio_check;

	val = snd_soc_component_read(component, TAC5X1X_GPO1);
	gpio_check = ((val & TAC5X1X_GPIOx_CFG_MASK) >> 0);;
	if (gpio_check != TAC5X1X_GPIO_GPO) {
		printk(KERN_ERR "%s: GPO1 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPO1_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
		TAC5X1X_GPO1_VAL, val);

	return 1;
};

static int tac5x1x_get_GPIO1_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = ((val & TAC5X1X_GPIO1_MON) >> 0);

	return 0;
};

static int tac5x1x_set_GPIO1_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;
	int gpio_check;

	val = snd_soc_component_read(component, TAC5X1X_GPIO1);
	gpio_check = ((val & TAC5X1X_GPIOx_CFG_MASK) >> 0);
	if (gpio_check == TAC5X1X_GPIO_DISABLE) {
		dev_info(component->dev,
			"%s: GPIO1 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPIO1_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
		TAC5X1X_GPIO1_VAL, val);

	return 1;
};

static int tac5x1x_get_GPIO2_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = ((val & TAC5X1X_GPIO2_MON) >> 0);

	return 0;
};

static int tac5x1x_set_GPIO2_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;
	int gpio_check;

	val = snd_soc_component_read(component, TAC5X1X_GPIO2);
	gpio_check = ((val & TAC5X1X_GPIOx_CFG_MASK) >> 0);
	if (gpio_check == TAC5X1X_GPIO_DISABLE) {
		dev_info(component->dev,
			"%s: GPIO2 is not configure as a GPO output\n",
			__func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		val = 0;
	else
		val = TAC5X1X_GPIO2_VAL;

	ucontrol->value.integer.value[0] = (val >> 0);
	snd_soc_component_update_bits(component, TAC5X1X_GPIOVAL,
		TAC5X1X_GPIO2_VAL, val);

	return 1;
};

static int tac5x1x_get_GPI1_gpio(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int val;

	val = snd_soc_component_read(component, TAC5X1X_GPIOVAL);
	ucontrol->value.integer.value[0] = ((val & TAC5X1X_GPI1_MON) >> 0);

	return 0;
};

static const struct snd_kcontrol_new tac5x1x_GPO1[] = {
	SOC_SINGLE_BOOL_EXT("GPO1 GPO", 0, NULL, tac5x1x_set_GPO1_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPIO1_I[] = {
	SOC_SINGLE_BOOL_EXT("GPIO1 GPI", 0, tac5x1x_get_GPIO1_gpio, NULL),
};

static const struct snd_kcontrol_new tac5x1x_GPIO1_O[] = {
	SOC_SINGLE_BOOL_EXT("GPIO1 GPO", 0, NULL, tac5x1x_set_GPIO1_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPIO2_I[] = {
	SOC_SINGLE_BOOL_EXT("GPIO2 GPI", 0, tac5x1x_get_GPIO2_gpio, NULL),
};

static const struct snd_kcontrol_new tac5x1x_GPIO2_O[] = {
	SOC_SINGLE_BOOL_EXT("GPIO2 GPO", 0, NULL, tac5x1x_set_GPIO2_gpio),
};

static const struct snd_kcontrol_new tac5x1x_GPI1[] = {
	SOC_SINGLE_BOOL_EXT("GPI1 GPI", 0, tac5x1x_get_GPI1_gpio, NULL),
};

/* Record */
/* ADC Analog/PDM Selection */
static const char * const tac5x1x_input_source_text[] = {"Analog", "PDM"};

static SOC_ENUM_SINGLE_DECL(tac5x1x_in1_source_enum,
			    TAC5X1X_INTF4, 7,
			    tac5x1x_input_source_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_in2_source_enum,
			    TAC5X1X_INTF4, 6,
			    tac5x1x_input_source_text);

static const struct snd_kcontrol_new tac5x1x_dapm_in1_source_control[] = {
	SOC_DAPM_ENUM("CH1 Source MUX", tac5x1x_in1_source_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_in2_source_control[] = {
	SOC_DAPM_ENUM("CH2 Source MUX", tac5x1x_in2_source_enum),
};
#if 1
/* ADC Analog source Selection */
static const char * const tac5x1x_input_analog_sel_text[] = {
	"Differential",
	"Single-ended",
	"Single-ended mux INxP",
	"Single-ended mux INxM",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_config_enum,
			    TAC5X1X_ADCCH1C0, 6,
			    tac5x1x_input_analog_sel_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_config_enum,
			    TAC5X1X_ADCCH2C0, 6,
			    tac5x1x_input_analog_sel_text);

static const struct snd_kcontrol_new tac5x1x_dapm_adc1_config_control[] = {
	SOC_DAPM_ENUM("ADC1 Analog MUX", tac5x1x_adc1_config_enum),
};
static const struct snd_kcontrol_new tac5x1x_dapm_adc2_config_control[] = {
	SOC_DAPM_ENUM("ADC2 Analog MUX", tac5x1x_adc2_config_enum),
};

#endif
/* ADC full-scale selection */
static const char * const tac5x1x_adc_fscale_text[] =
	{"1/2/5-VRMS", "2/4/10-VRMS"};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_fscale_enum,
			    TAC5X1X_ADCCH1C0, 1,
			    tac5x1x_adc_fscale_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_fscale_enum,
			    TAC5X1X_ADCCH2C0, 1,
			    tac5x1x_adc_fscale_text);

static const struct snd_kcontrol_new tac5x1x_dapm_adc1_fscale_control[] = {
	SOC_DAPM_ENUM("ADC1 FSCALE MUX", tac5x1x_adc1_fscale_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_adc2_fscale_control[] = {
	SOC_DAPM_ENUM("ADC2 FSCALE MUX", tac5x1x_adc2_fscale_enum),
};

/* Impedance Selection */
static const char * const resistor_text[] = {
	"5 kOhm",
	"10 kOhm",
	"40 kOhm",
};
static SOC_ENUM_SINGLE_DECL(adc1_resistor_enum, TAC5X1X_ADCCH1C0, 4,
			    resistor_text);
static SOC_ENUM_SINGLE_DECL(adc2_resistor_enum, TAC5X1X_ADCCH2C0, 4,
			    resistor_text);

/* PDM Data input pin Selection */
static const char * const pdm_pin_text[] = {
	"Disable",
	"GPIO1",
	"GPIO2",
	"GPI1",
};
static SOC_ENUM_SINGLE_DECL(pdm_12_pin_enum, TAC5X1X_INTF4, 2,
			    pdm_pin_text);
static SOC_ENUM_SINGLE_DECL(pdm_34_pin_enum, TAC5X1X_INTF4, 0,
			    pdm_pin_text);
static const struct snd_kcontrol_new pdm_12_pin_controls[] = {
	SOC_DAPM_ENUM("PDM chn12 Datain Select", pdm_12_pin_enum),
};
static const struct snd_kcontrol_new pdm_34_pin_controls[] = {
	SOC_DAPM_ENUM("PDM chn34 Datain Select", pdm_34_pin_enum),
};

static const char * const pdmclk_text[] = {
	"2.8224 MHz or 3.072 MHz",
	"1.4112 MHz or 1.536 MHz",
	"705.6 kHz or 768 kHz",
	"5.6448 MHz or 6.144 MHz"
};

static SOC_ENUM_SINGLE_DECL(pdmclk_select_enum, TAC5X1X_CNTCLK0, 6,
			    pdmclk_text);

/* Digital Volume control. From -80 to 47 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(record_dig_vol_tlv, -8000, 50, 0);

/* Gain Calibration control. From -0.8db to 0.7db dB in 0.1 dB steps */
static DECLARE_TLV_DB_MINMAX(record_gain_cali_tlv, -80, 70);

/* Analog Level control. From -12 to 24 dB in 6 dB steps */
static DECLARE_TLV_DB_SCALE(playback_Analog_level_tlv, -1200, 600, 0);

/* Digital Volume control. From -100 to 27 dB in 0.5 dB steps */
static DECLARE_TLV_DB_SCALE(dac_dig_vol_tlv, -10000, 50, 0);//mute ?

/* Gain Calibration control. From -0.8db to 0.7db dB in 0.1 dB steps */
static DECLARE_TLV_DB_MINMAX(playback_gain_cali_tlv, -80, 70);

/* Output Source Selection */
static const char * const tac5x1x_output_source_text[] = {
	"Disabled",
	"DAC Input",
	"Analog Bypass",
	"DAC + Analog Bypass Mix",
	"DAC -> OUTxP, INxP -> OUTxM",
	"INxM -> OUTxP, DAC -> OUTxM",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_out1_source_enum,
			TAC5X1X_OUT1CFG0, 5,
			tac5x1x_output_source_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_out2_source_enum,
			TAC5X1X_OUT2CFG0, 5,
			tac5x1x_output_source_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_source_control[] = {
	SOC_DAPM_ENUM("OUT1X MUX", tac5x1x_out1_source_enum),
};

static const struct snd_kcontrol_new tac5x1x_dapm_out2_source_control[] = {
	SOC_DAPM_ENUM("OUT2X MUX", tac5x1x_out2_source_enum),
};

/* Output Config Selection */
static const char * const tac5x1x_output_config_text[] = {
	"Differential",
	"Stereo Single-ended",
	"Mono Single-ended at OUTxP only",
	"Mono Single-ended at OUTxM only",
	"Pseudo differential with OUTxM as VCOM",
	"Pseudo differential with OUTxM as external sensing",
	"Pseudo differential with OUTxP as VCOM",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_out1_config_enum,
			TAC5X1X_OUT1CFG0, 2,
			tac5x1x_output_config_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_out2_config_enum,
			TAC5X1X_OUT2CFG0, 2,
			tac5x1x_output_config_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_config_control[] = {
	SOC_DAPM_ENUM("OUT1X Config MUX", tac5x1x_out1_config_enum),
};
static const struct snd_kcontrol_new tac5x1x_dapm_out2_config_control[] = {
	SOC_DAPM_ENUM("OUT2X Config MUX", tac5x1x_out2_config_enum),
};

static const char * const tac5x1x_wideband_text[] = {
	"Audio BW 24-kHz",
	"Wide BW 96-kHz",
};
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_wideband_enum,
			TAC5X1X_ADCCH1C0, 0,
			tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_wideband_enum,
			TAC5X1X_ADCCH2C0, 0,
			tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_dac1_wideband_enum,
			TAC5X1X_OUT1CFG1, 0,
			tac5x1x_wideband_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_dac2_wideband_enum,
			TAC5X1X_OUT2CFG1, 0,
			tac5x1x_wideband_text);

static const char * const tac5x1x_tolerance_text[] = {
	"AC Coupled with 100mVpp",
	"AC/DC Coupled with 1Vpp",
	"AC/DC Coupled with Rail-to-rail",
};

static SOC_ENUM_SINGLE_DECL(tac5x1x_adc1_tolerance_enum,
			TAC5X1X_ADCCH1C0, 2,
			tac5x1x_tolerance_text);
static SOC_ENUM_SINGLE_DECL(tac5x1x_adc2_tolerance_enum,
			TAC5X1X_ADCCH2C0, 2,
			tac5x1x_tolerance_text);

/* Output Drive Selection */
static const char * const tac5x1x_output_driver_text[] = {
	"Line-out",
	"Headphone",
	"4 ohm",
	"FD Receiver/Debug",
};

static SOC_ENUM_SINGLE_DECL(out1p_driver_enum,
			TAC5X1X_OUT1CFG1, 6,
			tac5x1x_output_driver_text);

static SOC_ENUM_SINGLE_DECL(out2p_driver_enum,
			TAC5X1X_OUT2CFG1, 6,
			tac5x1x_output_driver_text);

static const struct snd_kcontrol_new tac5x1x_dapm_out1_driver_control[] = {
	SOC_DAPM_ENUM("OUT1 driver MUX", out1p_driver_enum),
};
static const struct snd_kcontrol_new tac5x1x_dapm_out2_driver_control[] = {
	SOC_DAPM_ENUM("OUT2 driver MUX", out2p_driver_enum),
};

/* Decimation Filter Selection */
static const char * const decimation_filter_text[] = {
	"Linear Phase", "Low Latency", "Ultra-low Latency"
};

static SOC_ENUM_SINGLE_DECL(decimation_filter_record_enum, TAC5X1X_DSP0, 6,
			    decimation_filter_text);
static SOC_ENUM_SINGLE_DECL(decimation_filter_playback_enum, TAC5X1X_DSP1, 6,
			    decimation_filter_text);

static const struct snd_kcontrol_new tx_ch1_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH1, 5, 1, 0);
static const struct snd_kcontrol_new tx_ch2_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH2, 5, 1, 0);
static const struct snd_kcontrol_new tx_pdm_ch1_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH1, 5, 1, 0);
static const struct snd_kcontrol_new tx_pdm_ch2_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH2, 5, 1, 0);

static const struct snd_kcontrol_new tx_ch3_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH3, 5, 1, 0);
static const struct snd_kcontrol_new tx_ch4_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASITXCH4, 5, 1, 0);

static const struct snd_kcontrol_new rx_ch1_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH1, 5, 1, 0);
static const struct snd_kcontrol_new rx_ch2_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH2, 5, 1, 0);
static const struct snd_kcontrol_new rx_ch3_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH3, 5, 1, 0);
static const struct snd_kcontrol_new rx_ch4_asi_switch =
	SOC_DAPM_SINGLE("Switch", TAC5X1X_PASIRXCH4, 5, 1, 0);

static const struct snd_kcontrol_new tac5x1x_snd_controls[] = {
	SOC_DOUBLE_R_TLV("OUT1 Analog Level Playback Volume", TAC5X1X_OUT1CFG1,
		TAC5X1X_OUT1CFG2, 3, 6, 1,
		playback_Analog_level_tlv),

	SOC_ENUM("Playback Decimation Filter",
		decimation_filter_playback_enum),

	SOC_ENUM("DAC1 Audio BW", tac5x1x_dac1_wideband_enum),

	SOC_DOUBLE_R_TLV("DAC1 Digital Playback Volume", TAC5X1X_DACCH1A0,
			TAC5X1X_DACCH1B0, 0, 0xff, 0, dac_dig_vol_tlv),

	SOC_DOUBLE_R_TLV("DAC1 Gain Calibration Playback Volume", TAC5X1X_DACCH1A1,
			TAC5X1X_DACCH1B1, 4, 0xf, 0, playback_gain_cali_tlv),

	SOC_ENUM("Record Decimation Filter",
		decimation_filter_record_enum),

	SOC_ENUM("ADC1 Audio BW", tac5x1x_adc1_wideband_enum),

	SOC_SINGLE_TLV("ADC1 Digital Capture Volume", TAC5X1X_ADCCH1C2,
			0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("ADC1 Fine Capture Volume", TAC5X1X_ADCCH1C3,
			0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("ADC1 Phase Capture Volume", TAC5X1X_ADCCH1C4,
			2, 0, 63, 0),
};

static const struct snd_kcontrol_new tac5111_5211_snd_controls[] = {
	SOC_ENUM("ADC1 Common-mode Tolerance", tac5x1x_adc1_tolerance_enum),

	SOC_ENUM("ADC1 Impedance Select", adc1_resistor_enum),
};

static const struct snd_kcontrol_new tac5x12_snd_controls[] = {
	SOC_DOUBLE_R_TLV("OUT2 Analog Level Playback Volume", TAC5X1X_OUT2CFG1,
			TAC5X1X_OUT2CFG2, 3, 6, 1, playback_Analog_level_tlv),

	SOC_DOUBLE_R_TLV("DAC2 Digital Playback Volume", TAC5X1X_DACCH2A0,
			TAC5X1X_DACCH2B0, 0, 0xff, 0, dac_dig_vol_tlv),

	SOC_DOUBLE_R_TLV("DAC2 Gain Calibration Playback Volume",
			TAC5X1X_DACCH2A0, TAC5X1X_DACCH2B0, 4, 0xf,
			0, playback_gain_cali_tlv),

	SOC_ENUM("DAC2 Audio BW", tac5x1x_dac2_wideband_enum),
	SOC_ENUM("ADC2 Audio BW", tac5x1x_adc2_wideband_enum),

	SOC_SINGLE_TLV("ADC2 Digital Capture Volume", TAC5X1X_ADCCH2C2,
			0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("ADC2 Fine Capture Volume", TAC5X1X_ADCCH2C3,
			0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("ADC2 Phase Capture Volume", TAC5X1X_ADCCH2C4,
			2, 0, 63, 0),
};

static const struct snd_kcontrol_new tac5112_5212_snd_controls[] = {
	SOC_ENUM("ADC2 Common-mode Tolerance", tac5x1x_adc2_tolerance_enum),

	SOC_ENUM("ADC2 Impedance Select", adc2_resistor_enum),
};

static const struct snd_kcontrol_new tac5x1x_pdm_snd_controls[] = {
	SOC_ENUM("PDM Clk Divider Capture Switch", pdmclk_select_enum),

	SOC_SINGLE_TLV("PDM1 Digital Capture Volume", TAC5X1X_ADCCH1C2,
			0, 0xff, 0, record_dig_vol_tlv),
	SOC_SINGLE_TLV("PDM2 Digital Capture Volume", TAC5X1X_ADCCH2C2,
			0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("PDM1 Fine Capture Volume", TAC5X1X_ADCCH1C3,
			0, 0xff, 0, record_gain_cali_tlv),
	SOC_SINGLE_TLV("PDM2 Fine Capture Volume", TAC5X1X_ADCCH2C3,
			0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("PDM1 Phase Capture Volume", TAC5X1X_ADCCH1C4,
			2, 0, 63, 0),
	SOC_SINGLE_RANGE("PDM2 Phase Capture Volume", TAC5X1X_ADCCH2C4,
			2, 0, 63, 0),

	SOC_SINGLE_TLV("PDM3 Digital Capture Volume", TAC5X1X_ADCCH3C2,
			0, 0xff, 0, record_dig_vol_tlv),
	SOC_SINGLE_TLV("PDM4 Digital Capture Volume", TAC5X1X_ADCCH4C2,
			0, 0xff, 0, record_dig_vol_tlv),

	SOC_SINGLE_TLV("PDM3 Fine Capture Volume", TAC5X1X_ADCCH3C3,
			0, 0xff, 0, record_gain_cali_tlv),
	SOC_SINGLE_TLV("PDM4 Fine Capture Volume", TAC5X1X_ADCCH4C3,
			0, 0xff, 0, record_gain_cali_tlv),

	SOC_SINGLE_RANGE("PDM3 Phase Capture Volume", TAC5X1X_ADCCH3C4,
			2, 0, 63, 0),
	SOC_SINGLE_RANGE("PDM4 Phase Capture Volume", TAC5X1X_ADCCH4C4,
			2, 0, 63, 0),
};

static const struct snd_soc_dapm_widget tac5x1x_dapm_widgets[] = {
	/* DAC1 */
	SND_SOC_DAPM_AIF_IN("ASI IN1", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUT1"),

	SND_SOC_DAPM_MUX("OUT1x Source", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out1_source_control),

	SND_SOC_DAPM_MUX("OUT1x Config", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out1_config_control),

	SND_SOC_DAPM_MUX("OUT1x Driver", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out1_driver_control),

	SND_SOC_DAPM_DAC("Left DAC Enable", "Left Playback", TAC5X1X_CH_EN, 3, 0),
	SND_SOC_DAPM_PGA("Left DAC Power", TAC5X1X_PWR_CFG, 6, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH("ASI_RX_CH1_EN", SND_SOC_NOPM, 0, 0,
			&rx_ch1_asi_switch),

	/* ADC1 */
	SND_SOC_DAPM_INPUT("AIN1"),

	SND_SOC_DAPM_MUX("ADC1 Full-Scale", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_adc1_fscale_control),

	SND_SOC_DAPM_MUX("ADC1 Config", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_adc1_config_control),

	SND_SOC_DAPM_ADC("CH1_ADC_EN", "CH1 Capture", TAC5X1X_CH_EN, 7, 0),

	SND_SOC_DAPM_SWITCH("ASI_TX_CH1_EN", SND_SOC_NOPM, 0, 0,
			&tx_ch1_asi_switch),

	SND_SOC_DAPM_MICBIAS("Mic Bias", TAC5X1X_PWR_CFG, 5, 0),
};

static const struct snd_soc_dapm_widget tac5x12_dapm_widgets[] = {
	/* DAC2 */
	SND_SOC_DAPM_OUTPUT("OUT2"),

	SND_SOC_DAPM_AIF_IN("ASI IN2", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("OUT2x Source", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out2_source_control),

	SND_SOC_DAPM_MUX("OUT2x Config", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out2_config_control),

	SND_SOC_DAPM_MUX("OUT2x Driver", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_out2_driver_control),

	SND_SOC_DAPM_DAC("Right DAC Enable",
			"Right Playback", TAC5X1X_CH_EN, 2, 0),
	SND_SOC_DAPM_PGA("Right DAC Power", TAC5X1X_PWR_CFG, 6, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH("ASI_RX_CH2_EN", SND_SOC_NOPM, 0, 0,
			&rx_ch2_asi_switch),

	/* ADC2 */
	SND_SOC_DAPM_INPUT("AIN2"),

	SND_SOC_DAPM_MUX("ADC2 Full-Scale", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_adc2_fscale_control),

	SND_SOC_DAPM_MUX("ADC2 Config", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_adc2_config_control),

	SND_SOC_DAPM_ADC("CH2_ADC_EN", "CH2 Capture", TAC5X1X_CH_EN, 6, 0),

	SND_SOC_DAPM_SWITCH("ASI_TX_CH2_EN", SND_SOC_NOPM, 0, 0,
			&tx_ch2_asi_switch),

};

static const struct snd_soc_dapm_widget tac5x1x_dapm_pdm_widgets[] = {
	/* PDM */
	SND_SOC_DAPM_INPUT("DIN1"),
	SND_SOC_DAPM_INPUT("DIN2"),
	SND_SOC_DAPM_INPUT("DIN3"),
	SND_SOC_DAPM_INPUT("DIN4"),

	SND_SOC_DAPM_MUX("PDM ch1 & ch2 Datain Select", SND_SOC_NOPM, 0, 0,
			   pdm_12_pin_controls),
	SND_SOC_DAPM_MUX("PDM ch3 & ch4 Datain Select", SND_SOC_NOPM, 0, 0,
			   pdm_34_pin_controls),

	SND_SOC_DAPM_ADC("CH1_PDM_EN", "PDM CH1 Capture", TAC5X1X_CH_EN, 7, 0),
	SND_SOC_DAPM_ADC("CH2_PDM_EN", "PDM CH2 Capture", TAC5X1X_CH_EN, 6, 0),
	SND_SOC_DAPM_ADC("CH3_PDM_EN", "PDM CH3 Capture", TAC5X1X_CH_EN, 5, 0),
	SND_SOC_DAPM_ADC("CH4_PDM_EN", "PDM CH4 Capture", TAC5X1X_CH_EN, 4, 0),

	SND_SOC_DAPM_SWITCH("PDM_ASI_TX_CH1_EN", SND_SOC_NOPM, 0, 0,
			&tx_pdm_ch1_asi_switch),
	SND_SOC_DAPM_SWITCH("PDM_ASI_TX_CH2_EN", SND_SOC_NOPM, 0, 0,
			&tx_pdm_ch2_asi_switch),
	SND_SOC_DAPM_SWITCH("PDM_ASI_TX_CH3_EN", SND_SOC_NOPM, 0, 0,
			&tx_ch3_asi_switch),
	SND_SOC_DAPM_SWITCH("PDM_ASI_TX_CH4_EN", SND_SOC_NOPM, 0, 0,
			&tx_ch4_asi_switch),

};

static const struct snd_soc_dapm_widget tac5x1x_common_dapm_widgets[] = {

	SND_SOC_DAPM_MUX("IN1 Source Mux", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_in1_source_control),

	SND_SOC_DAPM_MUX("IN2 Source Mux", SND_SOC_NOPM, 0, 0,
			tac5x1x_dapm_in2_source_control),

	SND_SOC_DAPM_PGA("ADC Power", TAC5X1X_PWR_CFG, 7, 0, NULL, 0),

	SND_SOC_DAPM_AIF_OUT("AIF OUT", "ASI Capture", 0, SND_SOC_NOPM, 0, 0),

};

static const struct snd_soc_dapm_route tac5x1x_dapm_routes[] = {
	/* Left Output */

	{"ASI_RX_CH1_EN", "Switch", "ASI IN1"},

	{"OUT1x Source", "DAC + Analog Bypass Mix", "ASI_RX_CH1_EN"},
	{"OUT1x Source", "DAC -> OUTxP, INxP -> OUTxM", "ASI_RX_CH1_EN"},
	{"OUT1x Source", "INxM -> OUTxP, DAC -> OUTxM", "ASI_RX_CH1_EN"},

	{"OUT1x Config", "Differential", "OUT1x Source"},
	// {"OUT1x Config", "Stereo Single-ended", "OUT1x Source"},
	{"OUT1x Config", "Mono Single-ended at OUTxP only", "OUT1x Source"},
	{"OUT1x Config", "Mono Single-ended at OUTxM only", "OUT1x Source"},
	{"OUT1x Config",
		"Pseudo differential with OUTxM as VCOM",
		"OUT1x Source"},
	{"OUT1x Config",
		"Pseudo differential with OUTxM as external sensing",
		"OUT1x Source"},
	{"OUT1x Config", "Pseudo differential with OUTxP as VCOM",
		"OUT1x Source"},


	{"OUT1x Driver", "Line-out", "OUT1x Config"},
	{"OUT1x Driver", "Headphone", "OUT1x Config"},

	{"Left DAC Enable", NULL, "OUT1x Driver"},
	{"Left DAC Power", NULL, "Left DAC Enable"},

	{"OUT1", NULL, "Left DAC Power"},

	/* ADC channel1 */
	{"IN1 Source Mux", "Analog",  "AIN1"},
	{"IN2 Source Mux", "Analog",  "IN1 Source Mux"},

	{"CH1_ADC_EN", NULL, "IN2 Source Mux"},
	{"ASI_TX_CH1_EN", "Switch", "CH1_ADC_EN"},

	{"ADC1 Config", "Differential", "ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended", "ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended mux INxP", "ASI_TX_CH1_EN"},
	{"ADC1 Config", "Single-ended mux INxM", "ASI_TX_CH1_EN"},

	{"ADC1 Full-Scale", "1/2/5-VRMS", "ADC1 Config"},
	{"ADC1 Full-Scale", "2/4/10-VRMS", "ADC1 Config"},

	{"Mic Bias", NULL,  "ADC1 Full-Scale"},

};

static const struct snd_soc_dapm_route tac5x12_dapm_routes[] = {
	/* Right Output */

	{"ASI_RX_CH2_EN", "Switch", "ASI IN2"},

	{"OUT2x Source", "DAC + Analog Bypass Mix", "ASI_RX_CH1_EN"},
	{"OUT2x Source", "DAC -> OUTxP, INxP -> OUTxM", "ASI_RX_CH1_EN"},
	{"OUT2x Source", "INxM -> OUTxP, DAC -> OUTxM", "ASI_RX_CH1_EN"},

	{"OUT2x Config", "Differential", "OUT2x Source"},
	// {"OUT2x Config", "Stereo Single-ended", "OUT2x Source"},
	{"OUT2x Config", "Mono Single-ended at OUTxP only", "OUT2x Source"},
	{"OUT2x Config", "Mono Single-ended at OUTxM only", "OUT2x Source"},
	{"OUT2x Config",
		"Pseudo differential with OUTxM as VCOM", "OUT2x Source"},
	{"OUT2x Config",
		"Pseudo differential with OUTxM as external sensing",
		"OUT2x Source"},
	{"OUT1x Config", "Pseudo differential with OUTxP as VCOM",
		"OUT2x Source"},

	{"OUT2x Driver", "Line-out", "OUT2x Config"},
	{"OUT2x Driver", "Headphone", "OUT2x Config"},

	{"Right DAC Enable", NULL, "OUT2x Driver"},

	{"Right DAC Power", NULL, "Right DAC Enable"},

	{"OUT2", NULL, "Right DAC Power"},

	/* ADC channel2 */
	{"CH2_ADC_EN", NULL, "AIN2"},

	{"ASI_TX_CH2_EN", "Switch", "CH2_ADC_EN"},

	{"ADC2 Config", "Differential", "ASI_TX_CH2_EN"},
	{"ADC2 Config", "Single-ended", "ASI_TX_CH2_EN"},
	{"ADC2 Config", "Single-ended mux INxP", "ASI_TX_CH2_EN"},
	{"ADC2 Config", "Single-ended mux INxM", "ASI_TX_CH2_EN"},

	{"ADC2 Full-Scale", "1/2/5-VRMS", "ADC2 Config"},
	{"ADC2 Full-Scale", "2/4/10-VRMS", "ADC2 Config"},

	{"Mic Bias", NULL, "ADC2 Full-Scale"},
};

static const struct snd_soc_dapm_route tac5x1x_dapm_pdm_routes[] = {
	/* PDM channel1 & Channel2 */
	{"IN1 Source Mux", "PDM", "DIN1"},
	{"IN2 Source Mux", "PDM", "DIN2"},

	{"PDM_ASI_TX_CH1_EN", "Switch", "IN1 Source Mux"},
	{"PDM_ASI_TX_CH2_EN", "Switch", "IN2 Source Mux"},

	{"CH1_PDM_EN", NULL, "PDM_ASI_TX_CH1_EN"},
	{"CH2_PDM_EN", NULL, "PDM_ASI_TX_CH2_EN"},

	{"PDM ch1 & ch2 Datain Select", "GPIO1", "CH1_PDM_EN"},
	{"PDM ch1 & ch2 Datain Select", "GPIO2", "CH1_PDM_EN"},
	{"PDM ch1 & ch2 Datain Select", "GPI1", "CH1_PDM_EN"},

	{"ADC Power", NULL, "PDM ch1 & ch2 Datain Select"},

	/* PDM channel3 & Channel4 */
	{"PDM_ASI_TX_CH3_EN", "Switch", "DIN3"},
	{"PDM_ASI_TX_CH4_EN", "Switch", "DIN4"},

	{"CH3_PDM_EN", NULL, "PDM_ASI_TX_CH3_EN"},
	{"CH4_PDM_EN", NULL, "PDM_ASI_TX_CH4_EN"},

	{"PDM ch3 & ch4 Datain Select", "GPIO1", "CH3_PDM_EN"},
	{"PDM ch3 & ch4 Datain Select", "GPIO2", "CH3_PDM_EN"},
	{"PDM ch3 & ch4 Datain Select", "GPI1", "CH3_PDM_EN"},

	{"ADC Power", NULL, "PDM ch3 & ch4 Datain Select"},
};

static const struct snd_soc_dapm_route tac5x1x_common_dapm_routes[] = {
	{"ADC Power", NULL, "Mic Bias"},

	{"AIF OUT", NULL, "ADC Power"},
};

static const struct reg_default tac5x1x_reg_defaults[] = {
	{TAC5X1X_PGSEL, 0x00},
	{TAC5X1X_INT, 0x10},
	{TAC5X1X_ADCCH1C0, 0x14},
	{TAC5X1X_ADCCH2C0, 0x14},
	{TAC5X1X_OUT1CFG0, 0x28},
	{TAC5X1X_OUT2CFG0, 0x28},
	{TAC5X1X_CH_EN, 0x00},
	{TAC5X1X_PASITXCH1, 0x20},
	{TAC5X1X_PASITXCH2, 0x01},
	{TAC5X1X_PASIRXCH1, 0x20},
	{TAC5X1X_PASIRXCH2, 0x21},
	{},
};

static int tac5x1x_pwr_ctrl(struct snd_soc_component *component,
			bool power_state)
{
	int pwr_ctrl = 0;
	int active_ctrl = 0;
	int ret = 0;
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);

	if (power_state) {
		active_ctrl = TAC5X1X_VREF_SLEEP_ACTIVE_MASK;
		snd_soc_component_update_bits(component, TAC5X1X_VREFCFG,
				TAC5X1X_VREFCFG_MICBIAS_VAL_MASK,
				tac5x1x->micbias_vg << 2);
		snd_soc_component_update_bits(component, TAC5X1X_VREFCFG,
				TAC5X1X_VREFCFG_VREF_FSCALE_MASK,
				tac5x1x->vref_vg);

		if (tac5x1x->uad_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_UAD_EN;
		if (tac5x1x->vad_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_VAD_EN;
		if (tac5x1x->uag_en)
			pwr_ctrl |= TAC5X1X_PWR_CFG_UAG_EN;
	} else {
		active_ctrl = 0x0;
		pwr_ctrl &= 0;
	}

	ret = snd_soc_component_update_bits(component, TAC5X1X_VREF,
		TAC5X1X_VREF_SLEEP_EXIT_VREF_EN |
		TAC5X1X_VREF_SLEEP_ACTIVE_MASK,
		active_ctrl);
	if (ret < 0) {
		dev_err(tac5x1x->dev,
			"%s, device active or sleep failed!/n", __func__);
		goto out;
	}

	ret = snd_soc_component_update_bits(component, TAC5X1X_PWR_CFG,
		TAC5X1X_PWR_CFG_UAD_EN |
		TAC5X1X_PWR_CFG_UAG_EN |
		TAC5X1X_PWR_CFG_VAD_EN,
		pwr_ctrl);
	if (ret < 0)
		dev_err(tac5x1x->dev,
			"%s, Power control set failed!/n", __func__);
out:
	return ret;
}

static int tac5x1x_set_dai_fmt(struct snd_soc_dai *codec_dai, u32 fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	//struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int iface_reg_1 = 0;
	int iface_reg_2 = 0;
	int iface_reg_3 = 0;
	int iface_reg_4 = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg_1 |= TAC5X1X_PASI_MODE_MASK;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		dev_err(component->dev,
			"%s: invalid DAI master/slave interface\n",
			__func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface_reg_2 |= TAC5X1X_PASI_FMT_I2S;
		iface_reg_4 = 16;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg_2 |= TAC5X1X_PASI_FMT_TDM;
		iface_reg_3 |= BIT(0); /* add offset 1 */
		iface_reg_4 = 1;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg_2 |= TAC5X1X_PASI_FMT_TDM;
		iface_reg_4 = 1;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg_2 |= TAC5X1X_PASI_FMT_LJ;
		iface_reg_4 = 16;
		break;
	default:
		dev_err(component->dev,
			"%s: invalid DAI interface format\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, TAC5X1X_CNTCLK2,
				TAC5X1X_PASI_MODE_MASK , iface_reg_1);
	snd_soc_component_update_bits(component, TAC5X1X_PASI0,
				TAC5X1X_PASI_FMT_MASK, iface_reg_2);
	snd_soc_component_update_bits(component, TAC5X1X_PASITX1,
				TAC5X1X_PASITX_OFFSET_MASK, iface_reg_3);
	snd_soc_component_update_bits(component, TAC5X1X_PASIRX0,
				TAC5X1X_PASIRX_OFFSET_MASK, iface_reg_3);
	snd_soc_component_update_bits(component, TAC5X1X_PASIRXCH2,
				TAC5X1X_PASIRX_SLOT_MASK, iface_reg_4);

	return 0;
}

static int tac5x1x_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int sample_rate = 0;
	int word_length = 0;

	switch (params_rate(params)) {
	case 24000:
		sample_rate = 25;
		break;
	case 32000:
		sample_rate = 23;
		break;
	case 44100:
	case 48000:
		sample_rate = 20;
		break;
	case 64000:
		sample_rate = 18;
		break;
	case 96000:
		sample_rate = 15;
		break;
	case 192000:
		sample_rate = 10;
		break;
	default:
		/* Auto detect sample rate */
		sample_rate = 0;
		break;
	}

	switch (params_physical_width(params)) {
	case 16:
		word_length |= TAC5X1X_WORD_LEN_16BITS;
		break;
	case 20:
		word_length |= TAC5X1X_WORD_LEN_20BITS;
		break;
	case 24:
		word_length |= TAC5X1X_WORD_LEN_24BITS;
		break;
	case 32:
		word_length |= TAC5X1X_WORD_LEN_32BITS;
		break;
	default:
		dev_err(tac5x1x->dev, "%s, set word length failed\n", __func__);
		return -EINVAL;
	}

//	snd_soc_component_update_bits(component, TAC5X1X_CLK0,
//				TAC5X1X_PASI_SAMP_RATE_MASK, sample_rate << 2);
	snd_soc_component_update_bits(component, TAC5X1X_PASI0,
				TAC5X1X_PASI_DATALEN_MASK, word_length);

	tac5x1x_pwr_ctrl(component, true);

	return 0;
}

static int tac5x1x_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		ret = tac5x1x_pwr_ctrl(component, true);
		if (ret < 0)
			dev_err(tac5x1x->dev,
				"%s, power up failed!/n", __func__);
		break;
	case SND_SOC_BIAS_OFF:
		ret = tac5x1x_pwr_ctrl(component, false);
		if (ret < 0)
			dev_err(tac5x1x->dev,
				"%s, power down failed!/n", __func__);
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops tac5x1x_ops = {
	.hw_params = tac5x1x_hw_params,
	.set_fmt = tac5x1x_set_dai_fmt,
	.no_capture_mute = 1,
};

static struct snd_soc_dai_driver tac5x1x_dai = {
	.name = "tac5x1x-hifi",
	.playback = {
			 .stream_name = "Playback",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = TAC5X1X_RATES,
			 .formats = TAC5X1X_FORMATS,},
	.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TAC5X1X_RATES,
			.formats = TAC5X1X_FORMATS,},
	.ops = &tac5x1x_ops,
	.symmetric_rate = 1,
};

static void tac5x1x_setup_gpios(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int *gpio_func = tac5x1x->gpio_setup->gpio_func;

	/* setup GPIO functions */
	/* GPIO1 */
	if (gpio_func[0] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPIO1,
			TAC5X1X_GPIOx_CFG_MASK,
			gpio_func[0] << 4);
		if (gpio_func[0] == TAC5X1X_GPIO_GPI)
			snd_soc_add_component_controls(component,
				tac5x1x_GPIO1_I,
				ARRAY_SIZE(tac5x1x_GPIO1_I));
		else if (gpio_func[0] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component,
				tac5x1x_GPIO1_O,
				ARRAY_SIZE(tac5x1x_GPIO1_O));
	}
	/* GPIO2 */
	if (gpio_func[1] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPIO2,
			TAC5X1X_GPIOx_CFG_MASK,
			gpio_func[1] << 4);
		if (gpio_func[1] == TAC5X1X_GPIO_GPI)
			snd_soc_add_component_controls(component,
				tac5x1x_GPIO2_I,
				ARRAY_SIZE(tac5x1x_GPIO2_I));
		else if (gpio_func[1] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component,
				tac5x1x_GPIO2_O,
				ARRAY_SIZE(tac5x1x_GPIO2_O));
	}
	/* GPO1 */
	if (gpio_func[2] <= TAC5X1X_GPIO_DAISY_OUT) {
		snd_soc_component_update_bits(component, TAC5X1X_GPO1,
			TAC5X1X_GPIOx_CFG_MASK,
			gpio_func[2] << 4);
		if (gpio_func[2] == TAC5X1X_GPIO_GPO)
			snd_soc_add_component_controls(component, tac5x1x_GPO1,
				ARRAY_SIZE(tac5x1x_GPO1));
	}
	/* GPI1 */
	if (tac5x1x->gpio_setup->gpi1_func) {
		snd_soc_component_update_bits(component, TAC5X1X_GPI1,
			TAC5X1X_GPI1_CFG_MASK, TAC5X1X_GPI1_CFG_MASK);
		snd_soc_add_component_controls(component, tac5x1x_GPI1,
			ARRAY_SIZE(tac5x1x_GPI1));
	}
}

static int tac5x1x_reset(struct snd_soc_component *component)
{
	int ret = 0;
	int index;

	ret = snd_soc_component_write(component, TAC5X1X_RESET, 1);
	if (ret < 0)
		goto out;
	/* Wait >= 10 ms after entering sleep mode. */
	usleep_range(10000, 100000);

	for (index = 0; index < ARRAY_SIZE(tac5x1x_reg_defaults); index++) {
		ret = snd_soc_component_write(component,
			tac5x1x_reg_defaults[index].reg,
			tac5x1x_reg_defaults[index].def);
		if (ret < 0)
			goto out;
	}

out:
	return ret;
}

static int tac5x1x_add_controls(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int *gpio_func = tac5x1x->gpio_setup->gpio_func;
	int ret;
	
	switch (tac5x1x->codec_type)
	{
	/* For Mono */
	case TAC5111:
	case TAC5211:
		ret = snd_soc_add_component_controls(
			component, tac5111_5211_snd_controls,
			ARRAY_SIZE(tac5111_5211_snd_controls));
		if (ret)
			return ret;
		fallthrough;
	case TAC5311:
	case TAC5411:
		break;
	/* For Stereo */
	case TAC5112:
	case TAC5212:
		ret = snd_soc_add_component_controls(
			component, tac5111_5211_snd_controls,
			ARRAY_SIZE(tac5111_5211_snd_controls));
		if (ret)
			return ret;
		ret = snd_soc_add_component_controls(
			component, tac5112_5212_snd_controls,
			ARRAY_SIZE(tac5112_5212_snd_controls));
		if (ret)
			return ret;
		fallthrough;
	case TAC5312:
	case TAC5412:
		ret = snd_soc_add_component_controls(
			component, tac5x12_snd_controls,
			ARRAY_SIZE(tac5x12_snd_controls));
		if (ret)
			return ret;
		break;
	default:
		break;
	}
	/* If enabled PDM GPIO*/
	if (memchr(gpio_func, TAC5X1X_GPIO_PDMCLK, 
		ARRAY_SIZE(tac5x1x->gpio_setup->gpio_func))) {
		ret = snd_soc_add_component_controls(
			component, tac5x1x_pdm_snd_controls,
			ARRAY_SIZE(tac5x1x_pdm_snd_controls));
		if (ret)
			return ret;
	}

	return 0;
}

static int tac5x1x_add_widgets(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	int *gpio_func = tac5x1x->gpio_setup->gpio_func;
	int ret;

	switch (tac5x1x->codec_type)
	{
	case TAC5111:
	case TAC5211:
	case TAC5311:
	case TAC5411:
		break;
	case TAC5112:
	case TAC5212:
	case TAC5312:
	case TAC5412:
		ret = snd_soc_dapm_new_controls(
			dapm, tac5x12_dapm_widgets,
			ARRAY_SIZE(tac5x12_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tac5x12_dapm_routes,
					      ARRAY_SIZE(tac5x12_dapm_routes));
		if (ret)
			return ret;
		break;
	default:
		break;
	}
	ret = snd_soc_dapm_new_controls(
		dapm, tac5x1x_common_dapm_widgets,
		ARRAY_SIZE(tac5x1x_common_dapm_widgets));
	if (ret)
		return ret;
	ret = snd_soc_dapm_add_routes(dapm, tac5x1x_common_dapm_routes,
					ARRAY_SIZE(tac5x1x_common_dapm_routes));
	if (ret)
		return ret;
	/* If enabled PDM GPIO*/
	if (memchr(gpio_func, TAC5X1X_GPIO_PDMCLK,
		ARRAY_SIZE(tac5x1x->gpio_setup->gpio_func))) {
		ret = snd_soc_dapm_new_controls(
			dapm, tac5x1x_dapm_pdm_widgets,
			ARRAY_SIZE(tac5x1x_dapm_pdm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(dapm, tac5x1x_dapm_pdm_routes,
					ARRAY_SIZE(tac5x1x_dapm_pdm_routes));
		if (ret)
			return ret;
	}
	return 0;
}

static int tac5x1x_component_probe(struct snd_soc_component *component)
{
	struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	int ret;

	//snd_soc_component_init_regmap(component, tac5x1x->regmap);

	ret = tac5x1x_reset(component);
	if (ret < 0) {
		dev_err(tac5x1x->dev, "%s, device reset failed\n", __func__);
		goto out;
	}

	ret = tac5x1x_add_controls(component);
	if (ret < 0)
		return ret;

	ret = tac5x1x_add_widgets(component);
	if (ret < 0)
		return ret;

	if (tac5x1x->gpio_setup)
		tac5x1x_setup_gpios(component);

out:
	return ret;
}

#ifdef CONFIG_PM
static int tac5x1x_soc_suspend(struct snd_soc_component *component)
{
	//struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	//regulator_disable(tac5x1x->supply_avdd);
	//regulator_disable(tac5x1x->supply_iovdd);
	return 0;
}

static int tac5x1x_soc_resume(struct snd_soc_component *component)
{
	//struct tac5x1x_priv *tac5x1x = snd_soc_component_get_drvdata(component);
	//regulator_enable(tac5x1x->supply_avdd);
	//regulator_enable(tac5x1x->supply_iovdd);
	return 0;
}
#else
#define tac5x1x_soc_suspend	NULL
#define tac5x1x_soc_resume	NULL
#endif /* CONFIG_PM */

static const struct snd_soc_component_driver soc_component_dev_tac5x1x = {
	.probe			= tac5x1x_component_probe,
	.set_bias_level		= tac5x1x_set_bias_level,
	.suspend		= tac5x1x_soc_suspend,
	.resume			= tac5x1x_soc_resume,
	.controls		= tac5x1x_snd_controls,
	.num_controls		= ARRAY_SIZE(tac5x1x_snd_controls),
	.dapm_widgets		= tac5x1x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tac5x1x_dapm_widgets),
	.dapm_routes		= tac5x1x_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(tac5x1x_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static int tac5x1x_parse_dt(struct tac5x1x_priv *tac5x1x,
		struct device_node *np)
{
	struct tac5x1x_setup_gpio *tac5x1x_setup;
	int vref_value = TAC5X1X_VERF_2_5V;
	int micbias_value = TAC5X1X_MICBIAS_VREF;
	int ret;

	tac5x1x_setup = devm_kzalloc(tac5x1x->dev, sizeof(*tac5x1x_setup),
							GFP_KERNEL);
	if (!tac5x1x_setup)
		return -ENOMEM;

	ret = fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,vref",
				 &vref_value);
	if (ret) {
		dev_err(tac5x1x->dev, "Fail to get verf E:%d\n", ret);	
		goto out;
	}
	ret = fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,micbias-vg",
				 &micbias_value);
	if (ret) {
		dev_err(tac5x1x->dev, "Fail to get micbias-vg E:%d\n", ret);	
		goto out;
	}

	if (micbias_value == TAC5X1X_MICBIAS_AVDD) {
		tac5x1x->micbias_vg = micbias_value;
		tac5x1x->vref_vg = TAC5X1X_VERF_2_75V;
		tac5x1x->micbias_en = true;
	} else {
		switch (vref_value) {
		case TAC5X1X_VERF_2_75V:
		case TAC5X1X_VERF_2_5V:
			switch (micbias_value) {
			case TAC5X1X_MICBIAS_VREF:
			case TAC5X1X_MICBIAS_0_5VREF:
				tac5x1x->micbias_vg = micbias_value;
				break;
			default:
				dev_err(tac5x1x->dev,
					"Bad tac5x1x-micbias-vg value %d\n",
					micbias_value);
				tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
				break;
			}
			tac5x1x->vref_vg = vref_value;
			tac5x1x->micbias_en = true;
			break;
		case TAC5X1X_VERF_1_375V:
			if (micbias_value == TAC5X1X_MICBIAS_VREF)
				tac5x1x->micbias_vg = micbias_value;

			else {
				dev_err(tac5x1x->dev,
					"Bad tac5x1x-micbias-vg value %d\n",
					micbias_value);
				tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
			}
			tac5x1x->micbias_en = true;
			tac5x1x->vref_vg = vref_value;
			break;
		default:
			dev_err(tac5x1x->dev,
				"Bad tac5x1x-vref-vg value %d\n",
				vref_value);
			tac5x1x->vref_vg = TAC5X1X_VERF_2_5V;
			tac5x1x->micbias_vg = TAC5X1X_MICBIAS_AVDD;
			tac5x1x->micbias_en = true;
			break;
		}
	}

	if (fwnode_property_read_u32(tac5x1x->dev->fwnode, "ti,gpi1-func",
				&tac5x1x_setup->gpi1_func))
		dev_err(tac5x1x->dev,
			"%s, Fail to get gpi1-func value\n", __func__);

	if (fwnode_property_read_u32_array(tac5x1x->dev->fwnode,
		"ti,gpios-func",
		tac5x1x_setup->gpio_func, 3))
		dev_err(tac5x1x->dev,
			"%s, Fail to get gpios-func value\n", __func__);

	tac5x1x->gpio_setup = tac5x1x_setup;

out:
	return ret;
}

static void tac5x1x_disable_regulators(struct tac5x1x_priv *tac5x1x)
{
	if (!IS_ERR(tac5x1x->supply_iovdd))
		regulator_disable(tac5x1x->supply_iovdd);

	if (!IS_ERR(tac5x1x->supply_avdd))
		regulator_disable(tac5x1x->supply_avdd);
}

static int tac5x1x_setup_regulators(struct device *dev,
		struct tac5x1x_priv *tac5x1x)
{
	int ret = 0;

	tac5x1x->supply_iovdd = devm_regulator_get(dev, "iovdd");
	tac5x1x->supply_avdd = devm_regulator_get(dev, "avdd");

	/* Check if the regulator requirements are fulfilled */

	if (IS_ERR(tac5x1x->supply_iovdd)) {
		dev_err(dev, "Missing supply 'iovdd'\n");
		return PTR_ERR(tac5x1x->supply_iovdd);
	}

	if (IS_ERR(tac5x1x->supply_avdd)) {
		dev_err(dev, "Missing supply 'avdd'\n");
		return PTR_ERR(tac5x1x->supply_avdd);
	}

	ret = regulator_enable(tac5x1x->supply_iovdd);
	if (ret) {
		dev_err(dev, "Failed to enable regulator iovdd\n");
		goto error_supply;
	}

	ret = regulator_enable(tac5x1x->supply_avdd);
	if (ret) {
		dev_err(dev, "Failed to enable regulator avdd\n");
		goto error_supply;
	}

	return 0;

error_supply:
	regulator_disable(tac5x1x->supply_avdd);
	regulator_disable(tac5x1x->supply_iovdd);
	return ret;
}

int tac5x1x_probe(struct device *dev, struct regmap *regmap)
{
	struct tac5x1x_priv *tac5x1x;
	struct device_node *np = dev->of_node;
	int ret;

	tac5x1x = devm_kzalloc(dev, sizeof(struct tac5x1x_priv),
				   GFP_KERNEL);
	if (tac5x1x == NULL)
		return -ENOMEM;

	tac5x1x->dev = dev;
	tac5x1x->regmap = regmap;
	tac5x1x->codec_type = (kernel_ulong_t)dev_get_drvdata(dev);

	dev_set_drvdata(dev, tac5x1x);

	if (np) {
		ret = tac5x1x_parse_dt(tac5x1x, np);
		if (ret) {
			dev_err(dev, "Failed to parse DT node\n");
			return ret;
		}
	} else
		dev_err(dev, "%s: Fail to get device node\n", __func__);

	ret = tac5x1x_setup_regulators(dev, tac5x1x);
	if (ret) {
		dev_err(dev, "Failed to setup regulators\n");
		return ret;
	}

	ret = devm_snd_soc_register_component(dev,
			&soc_component_dev_tac5x1x, &tac5x1x_dai, 1);
	if (ret) {
		dev_err(dev, "Failed to register component\n");
		goto err_disable_regulators;
	}

	return 0;

err_disable_regulators:
	tac5x1x_disable_regulators(tac5x1x);

	return ret;
}
EXPORT_SYMBOL(tac5x1x_probe);

int tac5x1x_remove(struct device *dev)
{
	struct tac5x1x_priv *tac5x1x = dev_get_drvdata(dev);

	tac5x1x_disable_regulators(tac5x1x);

	return 0;
}
EXPORT_SYMBOL(tac5x1x_remove);

MODULE_DESCRIPTION("ASoC tac5x1x codec driver");
MODULE_AUTHOR("Kevin Lu <kevin-lu@ti.com>");
MODULE_LICENSE("GPL v2");
