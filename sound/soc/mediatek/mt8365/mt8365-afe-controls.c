/*
 * mt8365-afe-controls.c  --  Mediatek 8365 audio controls
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jia Zeng <jia.zeng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8365-afe-controls.h"
#include "mt8365-afe-common.h"
#include "mt8365-reg.h"
#include "mt8365-afe-utils.h"
#include "../common/mtk-base-afe.h"
#include <sound/soc.h>

#define ENUM_TO_STR(enum) #enum

enum mixer_control_func {
	CTRL_SINEGEN_LOOPBACK_MODE = 0,
	CTRL_SINEGEN_TIMING,
	CTRL_AP_LOOPBACK,
	CTRL_DMIC_MODE,
};

enum sinegen_in_out_sel {
	SINE_TONE_INPUT = 0x0,
	SINE_TONE_OUTPUT = 0x1,
};

enum sinegen_loopback_mode {
	AFE_SGEN_I0I1 = 0,
	AFE_SGEN_I2,
	AFE_SGEN_I3I4,
	AFE_SGEN_I5I6,
	AFE_SGEN_I7I8,
	AFE_SGEN_I9I22,
	AFE_SGEN_I10I11,
	AFE_SGEN_I12I13,
	AFE_SGEN_I14I15,
	AFE_SGEN_I16I17,
	AFE_SGEN_I18I19,
	AFE_SGEN_I20I21,
	AFE_SGEN_I27_TO_I34,
	AFE_SGEN_I23I24,
	AFE_SGEN_I25I26,
	AFE_SGEN_IN_MAX,
	AFE_SGEN_O0O1	= AFE_SGEN_IN_MAX,
	AFE_SGEN_O2,
	AFE_SGEN_O3O4,
	AFE_SGEN_O5O6,
	AFE_SGEN_O7O8,
	AFE_SGEN_O9O10,
	AFE_SGEN_O11,
	AFE_SGEN_O12,
	AFE_SGEN_O13O14,
	AFE_SGEN_O15O16,
	AFE_SGEN_O17_TO_O26,
	AFE_SGEN_O19O20,
	AFE_SGEN_O17O18,
	/* O27/O28 O29/O30 share sgen */
	AFE_SGEN_O27_TO_O30,
	AFE_SGEN_OFF,
	AFE_SGEN_NUM,
};

enum sinegen_timing {
	AFE_SGEN_8K = 0,
	AFE_SGEN_11K,
	AFE_SGEN_12K,
	AFE_SGEN_16K,
	AFE_SGEN_22K,
	AFE_SGEN_24K,
	AFE_SGEN_32K,
	AFE_SGEN_44K,
	AFE_SGEN_48K,
	AFE_SGEN_88K,
	AFE_SGEN_96K,
	AFE_SGEN_176K,
	AFE_SGEN_192K,
};

enum {
	AP_LOOPBACK_NONE = 0,
	AP_LOOPBACK_AMIC_TO_SPK,
	AP_LOOPBACK_AMIC_TO_HP,
	AP_LOOPBACK_HEADSET_MIC_TO_SPK,
	AP_LOOPBACK_HEADSET_MIC_TO_HP,
	AP_LOOPBACK_DUAL_AMIC_TO_SPK,
	AP_LOOPBACK_DUAL_AMIC_TO_HP,
};

static const char *const sinegen_loopback_mode_func[] = {
	ENUM_TO_STR(AFE_SGEN_I0I1),
	ENUM_TO_STR(AFE_SGEN_I2),
	ENUM_TO_STR(AFE_SGEN_I3I4),
	ENUM_TO_STR(AFE_SGEN_I5I6),
	ENUM_TO_STR(AFE_SGEN_I7I8),
	ENUM_TO_STR(AFE_SGEN_I9I22),
	ENUM_TO_STR(AFE_SGEN_I10I11),
	ENUM_TO_STR(AFE_SGEN_I12I13),
	ENUM_TO_STR(AFE_SGEN_I14I15),
	ENUM_TO_STR(AFE_SGEN_I16I17),
	ENUM_TO_STR(AFE_SGEN_I18I19),
	ENUM_TO_STR(AFE_SGEN_I20I21),
	ENUM_TO_STR(AFE_SGEN_I27_TO_I34),
	ENUM_TO_STR(AFE_SGEN_I23I24),
	ENUM_TO_STR(AFE_SGEN_I25I26),
	ENUM_TO_STR(AFE_SGEN_O0O1),
	ENUM_TO_STR(AFE_SGEN_O2),
	ENUM_TO_STR(AFE_SGEN_O3O4),
	ENUM_TO_STR(AFE_SGEN_O5O6),
	ENUM_TO_STR(AFE_SGEN_O7O8),
	ENUM_TO_STR(AFE_SGEN_O9O10),
	ENUM_TO_STR(AFE_SGEN_O11),
	ENUM_TO_STR(AFE_SGEN_O12),
	ENUM_TO_STR(AFE_SGEN_O13O14),
	ENUM_TO_STR(AFE_SGEN_O15O16),
	ENUM_TO_STR(AFE_SGEN_O17_TO_O26),
	ENUM_TO_STR(AFE_SGEN_O19O20),
	ENUM_TO_STR(AFE_SGEN_O17O18),
	ENUM_TO_STR(AFE_SGEN_O27_TO_O30),
	ENUM_TO_STR(AFE_SGEN_OFF),
};

static const char *const sinegen_timing_func[] = {
	ENUM_TO_STR(AFE_SGEN_8K),
	ENUM_TO_STR(AFE_SGEN_11K),
	ENUM_TO_STR(AFE_SGEN_12K),
	ENUM_TO_STR(AFE_SGEN_16K),
	ENUM_TO_STR(AFE_SGEN_22K),
	ENUM_TO_STR(AFE_SGEN_24K),
	ENUM_TO_STR(AFE_SGEN_32K),
	ENUM_TO_STR(AFE_SGEN_44K),
	ENUM_TO_STR(AFE_SGEN_48K),
	ENUM_TO_STR(AFE_SGEN_88K),
	ENUM_TO_STR(AFE_SGEN_96K),
	ENUM_TO_STR(AFE_SGEN_176K),
	ENUM_TO_STR(AFE_SGEN_192K),
};

static const char *const ap_loopback_func[] = {
	ENUM_TO_STR(AP_LOOPBACK_NONE),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_AMIC_TO_HP),
};

static const char *const dmic_mode_func[] = {
	ENUM_TO_STR(DMIC_MODE_3P25M),
	ENUM_TO_STR(DMIC_MODE_1P625M),
	ENUM_TO_STR(DMIC_MODE_812P5K),
	ENUM_TO_STR(DMIC_MODE_406P25K),
};

static const struct soc_enum mt8365_afe_soc_enums[] = {
	[CTRL_SINEGEN_LOOPBACK_MODE] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_loopback_mode_func),
				    sinegen_loopback_mode_func),
	[CTRL_SINEGEN_TIMING] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_timing_func),
				    sinegen_timing_func),
	[CTRL_AP_LOOPBACK] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ap_loopback_func),
					ap_loopback_func),
	[CTRL_DMIC_MODE] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dmic_mode_func),
					dmic_mode_func),
};

static int mt8365_afe_hw_gain1_vol_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int val = 0;

	mt8365_afe_enable_main_clk(afe);
	regmap_read(afe->regmap, AFE_GAIN1_CON1, &val);
	mt8365_afe_disable_main_clk(afe);
	ucontrol->value.integer.value[0] = val & AFE_GAIN1_CON1_MASK;

	return 0;
}

static int mt8365_afe_hw_gain1_vol_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	mt8365_afe_enable_main_clk(afe);
	regmap_update_bits(afe->regmap, AFE_GAIN1_CON1,
		AFE_GAIN1_CON1_MASK, val);
	mt8365_afe_disable_main_clk(afe);
	return 0;
}

static int mt8365_afe_hw_gain1_sampleperstep_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int val = 0;

	mt8365_afe_enable_main_clk(afe);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &val);
	mt8365_afe_disable_main_clk(afe);

	val = (val & AFE_GAIN1_CON0_SAMPLE_PER_STEP_MASK) >> 8;
	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int mt8365_afe_hw_gain1_sampleperstep_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	mt8365_afe_enable_main_clk(afe);
	regmap_update_bits(afe->regmap, AFE_GAIN1_CON0,
		AFE_GAIN1_CON0_SAMPLE_PER_STEP_MASK, val << 8);
	mt8365_afe_disable_main_clk(afe);
	return 0;
}

static int mt8365_afe_cm_bypass_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm1;
	else if (!strcmp(kcontrol->id.name, "CM2_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm2;

	return 0;
}

static int mt8365_afe_cm_bypass_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		data->bypass_cm1 = (ucontrol->value.integer.value[0]) ?
				   true : false;
	else if (!strcmp(kcontrol->id.name, "CM2_Bypass_Switch"))
		data->bypass_cm2 = (ucontrol->value.integer.value[0]) ?
				   true : false;

	return 0;
}

static int mt8365_afe_singen_enable_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int val = 0;

	mt8365_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SGEN_CON0, &val);

	mt8365_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & AFE_SINEGEN_CON0_EN);

	return 0;
}

static int mt8365_afe_singen_enable_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	mt8365_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0]) {
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_TML);

		regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
				   AFE_SINEGEN_CON0_FREQ_DIV_CH2_MASK |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH1_MASK,
				   AFE_SINEGEN_CON0_FREQ_DIV_CH2(2) |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH1(1));

		regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   AFE_SINEGEN_CON0_EN);
	} else {
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   0x0);

		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_TML);
	}

	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static int mt8365_afe_sinegen_loopback_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mode;
	unsigned int val = 0;
	unsigned int in_out_sel;

	mt8365_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SGEN_CON0, &val);

	mt8365_afe_disable_main_clk(afe);

	in_out_sel = (val & AFE_SINEGEN_CON0_IN_OUT_SEL) >> 27;
	mode = (val & AFE_SINEGEN_CON0_MODE_MASK) >> 28;

	if (in_out_sel == SINE_TONE_OUTPUT)
		mode += AFE_SGEN_IN_MAX;

	/* O27O28/O29O30 sgen val is same 15 */
	if (mode == AFE_SGEN_O27_TO_O30 + 2)
		mode -= 2;

	ucontrol->value.integer.value[0] = mode;

	return 0;
}

static int mt8365_afe_sinegen_loopback_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mode;
	unsigned int val;
	unsigned int in_out_sel = SINE_TONE_INPUT;

	mode = ucontrol->value.integer.value[0];

	/* O27O28/O29O30 sgen val is same 15 */
	if (mode == AFE_SGEN_O27_TO_O30)
		mode += 2;

	/* O** generate SGEN */
	if (mode >= AFE_SGEN_IN_MAX) {
		mode -= AFE_SGEN_IN_MAX;
		in_out_sel = SINE_TONE_OUTPUT;
	}

	val = (mode << 28) & AFE_SINEGEN_CON0_MODE_MASK;

	mt8365_afe_enable_main_clk(afe);

	if ((mode < AFE_SGEN_OFF) && (mode >= 0)) {
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
				   AFE_SINEGEN_CON0_MODE_MASK |
				   AFE_SINEGEN_CON0_IN_OUT_SEL |
				   AFE_SINEGEN_CON0_AMP_DIV_CH2_MASK |
				   AFE_SINEGEN_CON0_AMP_DIV_CH1_MASK |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH2_MASK |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH1_MASK |
				   AFE_SINEGEN_CON0_EN,
				   val |
				   (in_out_sel << 27) |
				   AFE_SINEGEN_CON0_AMP_DIV_CH2(6) |
				   AFE_SINEGEN_CON0_AMP_DIV_CH1(7) |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH2(2) |
				   AFE_SINEGEN_CON0_FREQ_DIV_CH1(2) |
				   AFE_SINEGEN_CON0_EN);
	} else {
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
			0xffffffff, 0xf0000000);
	}
	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static int mt8365_afe_sinegen_timing_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int timing;
	unsigned int val = 0;

	mt8365_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SGEN_CON0, &val);

	mt8365_afe_disable_main_clk(afe);

	val = (val & AFE_SINEGEN_CON0_TIMING_CH1_MASK) >> 8;

	switch (val) {
	case AFE_SINEGEN_CON0_TIMING_8K:
		timing = AFE_SGEN_8K;
		break;
	case AFE_SINEGEN_CON0_TIMING_11D025K:
		timing = AFE_SGEN_11K;
		break;
	case AFE_SINEGEN_CON0_TIMING_12K:
		timing = AFE_SGEN_12K;
		break;
	case AFE_SINEGEN_CON0_TIMING_16K:
		timing = AFE_SGEN_16K;
		break;
	case AFE_SINEGEN_CON0_TIMING_22D05K:
		timing = AFE_SGEN_22K;
		break;
	case AFE_SINEGEN_CON0_TIMING_24K:
		timing = AFE_SGEN_24K;
		break;
	case AFE_SINEGEN_CON0_TIMING_32K:
		timing = AFE_SGEN_32K;
		break;
	case AFE_SINEGEN_CON0_TIMING_44D1K:
		timing = AFE_SGEN_44K;
		break;
	case AFE_SINEGEN_CON0_TIMING_48K:
		timing = AFE_SGEN_48K;
		break;
	case AFE_SINEGEN_CON0_TIMING_88D2K:
		timing = AFE_SGEN_88K;
		break;
	case AFE_SINEGEN_CON0_TIMING_96K:
		timing = AFE_SGEN_96K;
		break;
	case AFE_SINEGEN_CON0_TIMING_176D4K:
		timing = AFE_SGEN_176K;
		break;
	case AFE_SINEGEN_CON0_TIMING_192K:
		timing = AFE_SGEN_192K;
		break;
	default:
		timing = AFE_SGEN_8K;
		break;
	}

	ucontrol->value.integer.value[0] = timing;

	return 0;
}

static int mt8365_afe_sinegen_timing_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int timing;
	unsigned int val;

	timing = ucontrol->value.integer.value[0];

	switch (timing) {
	case AFE_SGEN_8K:
		val = AFE_SINEGEN_CON0_TIMING_8K;
		break;
	case AFE_SGEN_11K:
		val = AFE_SINEGEN_CON0_TIMING_11D025K;
		break;
	case AFE_SGEN_12K:
		val = AFE_SINEGEN_CON0_TIMING_12K;
		break;
	case AFE_SGEN_16K:
		val = AFE_SINEGEN_CON0_TIMING_16K;
		break;
	case AFE_SGEN_22K:
		val = AFE_SINEGEN_CON0_TIMING_22D05K;
		break;
	case AFE_SGEN_24K:
		val = AFE_SINEGEN_CON0_TIMING_24K;
		break;
	case AFE_SGEN_32K:
		val = AFE_SINEGEN_CON0_TIMING_32K;
		break;
	case AFE_SGEN_44K:
		val = AFE_SINEGEN_CON0_TIMING_44D1K;
		break;
	case AFE_SGEN_48K:
		val = AFE_SINEGEN_CON0_TIMING_48K;
		break;
	case AFE_SGEN_88K:
		val = AFE_SINEGEN_CON0_TIMING_88D2K;
		break;
	case AFE_SGEN_96K:
		val = AFE_SINEGEN_CON0_TIMING_96K;
		break;
	case AFE_SGEN_176K:
		val = AFE_SINEGEN_CON0_TIMING_176D4K;
		break;
	case AFE_SGEN_192K:
		val = AFE_SINEGEN_CON0_TIMING_192K;
		break;
	default:
		val = AFE_SINEGEN_CON0_TIMING_8K;
		break;
	}

	mt8365_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
			   AFE_SINEGEN_CON0_TIMING_CH1_MASK |
			   AFE_SINEGEN_CON0_TIMING_CH2_MASK,
			   AFE_SINEGEN_CON0_TIMING_CH1(val) |
			   AFE_SINEGEN_CON0_TIMING_CH2(val));

	mt8365_afe_disable_main_clk(afe);

	return 0;
}

static int mt8365_afe_ap_loopback_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_control_data *data = &afe_priv->ctrl_data;

	ucontrol->value.integer.value[0] = data->loopback_type;

	return 0;
}

static int mt8365_afe_ap_loopback_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_control_data *data = &afe_priv->ctrl_data;

	uint32_t sample_rate = 48000;
	long val = ucontrol->value.integer.value[0];

	if (data->loopback_type == val)
		return 0;

	if (data->loopback_type != AP_LOOPBACK_NONE) {
		if (val == AP_LOOPBACK_AMIC_TO_SPK ||
		    val == AP_LOOPBACK_AMIC_TO_HP) {
			/* disconnect I03 <-> O03, I03 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN3,
					   AFE_CONN3_I03_O03_S,
					   0);
			regmap_update_bits(afe->regmap, AFE_CONN4,
					   AFE_CONN4_I03_O04_S,
					   0);
		} else {
			/* disconnect I03 <-> O03, I04 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN3,
					   AFE_CONN3_I03_O03_S,
					   0);
			regmap_update_bits(afe->regmap, AFE_CONN4,
					   AFE_CONN4_I04_O04_S,
					   0);
		}

		/* disable downlink part */
		regmap_update_bits(afe->regmap,
					   AFE_ADDA_DL_SRC2_CON0, 0x1, 0x0);
		regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, 0x0);

		/* disable uplink part */
		/* disable aud_pad_top fifo */
		regmap_update_bits(afe->regmap,
					   AFE_AUD_PAD_TOP, 0xffffffff, 0x30);
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x0);
		/* de suggest disable ADDA_UL_SRC at least wait 125us */
		udelay(150);
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x0);

		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DAC);
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_DAC_PREDIS);
		mt8365_afe_disable_top_cg(afe, MT8365_TOP_CG_ADC);
		mt8365_afe_disable_main_clk(afe);
	}

	if (val != AP_LOOPBACK_NONE) {
		mt8365_afe_enable_main_clk(afe);
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_ADC);
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DAC);
		mt8365_afe_enable_top_cg(afe, MT8365_TOP_CG_DAC_PREDIS);

		if (val == AP_LOOPBACK_AMIC_TO_SPK ||
		    val == AP_LOOPBACK_AMIC_TO_HP) {
			/* connect I03 <-> O03, I03 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN3,
					   AFE_CONN3_I03_O03_S,
					   AFE_CONN3_I03_O03_S);
			regmap_update_bits(afe->regmap, AFE_CONN4,
					   AFE_CONN4_I03_O04_S,
					   AFE_CONN4_I03_O04_S);
		} else {
			/* connect I03 <-> O03, I04 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN3,
					   AFE_CONN3_I03_O03_S,
					   AFE_CONN3_I03_O03_S);
			regmap_update_bits(afe->regmap, AFE_CONN4,
					   AFE_CONN4_I04_O04_S,
					   AFE_CONN4_I04_O04_S);
		}

		/* 16 bit by default */
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
				AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04, 0);

		/* configure uplink */
		if (sample_rate == 32000) {
			regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0,
					   0x000e0000, (2 << 17));
		} else {
			regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0,
					   0x000e0000, (3 << 17));
		}

		/* Using Internal ADC */
		regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 0x1, 0x0);
		/* enable ul src */
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);

		/* configure downlink */
		regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON0,
				   0xffffffff, 0);
		regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON1,
				   0xffffffff, 0);

		if (sample_rate == 32000) {
			regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0,
					   0xffffffff, 0x63001802);
			regmap_update_bits(afe->regmap, AFE_I2S_CON1,
					   0xf << 8, 0x9 << 8);
		} else {
			regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0,
					   0xffffffff, 0x83001802);
			regmap_update_bits(afe->regmap, AFE_I2S_CON1,
					   0xf << 8, 0xa << 8);
		}

		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON1,
				   0xffffffff, 0xf74f0000);
		/* SA suggest use default value for sdm */
		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON,
				   0xffffffff, 0x0700701e);
		regmap_update_bits(afe->regmap,
				   AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
		regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, 0x1);

		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x1);

		/* enable aud_pad_top fifo */
		regmap_update_bits(afe->regmap,
				   AFE_AUD_PAD_TOP, 0xffffffff, 0x31);

	}

	data->loopback_type = ucontrol->value.integer.value[0];

	return 0;
}

static int mt8365_afe_dmic_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_dmic_data *dmic_data = &afe_priv->dmic_data;

	ucontrol->value.integer.value[0] = dmic_data->dmic_mode;

	return 0;
}

static int mt8365_afe_dmic_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8365_afe_private *afe_priv = afe->platform_priv;
	struct mt8365_dmic_data *dmic_data = &afe_priv->dmic_data;

	unsigned int val = ucontrol->value.integer.value[0];

	if (dmic_data->dmic_mode == val)
		return 0;

	dmic_data->dmic_mode = ucontrol->value.integer.value[0];

	return 0;
}


static const struct snd_kcontrol_new mt8365_afe_controls[] = {
	SOC_SINGLE_BOOL_EXT("CM1_Bypass_Switch",
			    0,
			    mt8365_afe_cm_bypass_get,
			    mt8365_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("CM2_Bypass_Switch",
			    0,
			    mt8365_afe_cm_bypass_get,
			    mt8365_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("Audio_SineGen_Enable",
			    0,
			    mt8365_afe_singen_enable_get,
			    mt8365_afe_singen_enable_put),
	SOC_ENUM_EXT("Audio_SideGen_Switch",
		     mt8365_afe_soc_enums[CTRL_SINEGEN_LOOPBACK_MODE],
		     mt8365_afe_sinegen_loopback_mode_get,
		     mt8365_afe_sinegen_loopback_mode_put),
	SOC_ENUM_EXT("Audio_SideGen_SampleRate",
		     mt8365_afe_soc_enums[CTRL_SINEGEN_TIMING],
		     mt8365_afe_sinegen_timing_get,
		     mt8365_afe_sinegen_timing_put),
	SOC_ENUM_EXT("AP_Loopback_Select",
			 mt8365_afe_soc_enums[CTRL_AP_LOOPBACK],
			 mt8365_afe_ap_loopback_get,
			 mt8365_afe_ap_loopback_put),
	SOC_SINGLE_EXT("HW Gain1 Volume",
				0,
				0,
				0x80000,
				0,
				mt8365_afe_hw_gain1_vol_get,
				mt8365_afe_hw_gain1_vol_put),
	SOC_SINGLE_EXT("HW Gain1 SamplePerStep",
				0,
				0,
				255,
				0,
				mt8365_afe_hw_gain1_sampleperstep_get,
				mt8365_afe_hw_gain1_sampleperstep_put),
	SOC_ENUM_EXT("DMIC_Mode_Select",
			 mt8365_afe_soc_enums[CTRL_DMIC_MODE],
			 mt8365_afe_dmic_mode_get,
			 mt8365_afe_dmic_mode_put),
};

#if 0
int mt8365_afe_add_controls(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform, mt8365_afe_controls,
					     ARRAY_SIZE(mt8365_afe_controls));
}
#endif
