/*
 * sound/soc/amlogic/auge/effects.c
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
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/moduleparam.h>

#include "effects_hw.h"

#include "effects.h"
#include "ddr_mngr.h"

static unsigned int aml_EQ_param_length = 100;
static unsigned int aml_EQ_param[100] = {
	/*channel 1 param*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef0*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef1*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef2*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef3*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef4*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef5*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef6*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef7*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef8*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef9*/
	/*channel 2 param*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef0*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef1*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef2*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef3*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef4*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef5*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef6*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef7*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef8*/
	0x800000, 0x00, 0x00, 0x00, 0x00, /*eq_ch1_coef9*/
};

static unsigned int aml_DRC_param_length = 6;
static u32 aml_drc_table[6] = {
	0x0000111c, 0x00081bfc, 0x00001571,  /*drc_ae, drc_aa, drc_ad*/
	0x0380111c, 0x03881bfc, 0x03801571,  /*drc_ae_1m, drc_aa_1m, drc_ad_1m*/
};

static u32 aml_drc_tko_table[6] = {
	0x0,		0x0,	 /*offset0, offset1*/
	0xcb000000, 0x0,	 /*thd0, thd1*/
	0xa0000,	0x40000, /*k0, k1*/
};

static const char *const aed_req_module_texts[] = {
	"TDMOUT_A",
	"TDMOUT_B",
	"TDMOUT_C",
	"SPDIFOUT_A",
	"SPDIFOUT_B",
};

module_param_array(aml_EQ_param, uint, &aml_EQ_param_length, 0664);
MODULE_PARM_DESC(aml_EQ_param, "An array of aml EQ param");

module_param_array(aml_drc_table, uint, &aml_DRC_param_length, 0664);
MODULE_PARM_DESC(aml_drc_table, "An array of aml DRC table param");

module_param_array(aml_drc_tko_table, uint, &aml_DRC_param_length, 0664);
MODULE_PARM_DESC(aml_drc_tko_table, "An array of aml DRC tko table param");

static int mixer_eqdrc_read(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value =
			(((unsigned int)eqdrc_read(reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mixer_eqdrc_write(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int reg_value = (unsigned int)eqdrc_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	reg_value &= max;
	reg_value |= (value << shift);
	eqdrc_write(reg, reg_value);

	return 0;
}

static int mixer_set_AED_req_ctrl(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value = ucontrol->value.integer.value[0];

	if (value > 4) {
		pr_err("unknown module:%d\n", value);
		return -EINVAL;
	}
	pr_info("AED req_sel0 module:%s\n", aed_req_module_texts[value]);

	/* REQ_SEL0 */
	aed_req_sel(false, 0, value);

	return 0;
}

static int mixer_set_EQ(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value = ucontrol->value.integer.value[0];
	int eqdrc_module;

	aed_set_eq(value, aml_EQ_param_length, &aml_EQ_param[0]);

	eqdrc_module = aed_get_req_sel(0);
	aml_set_aed(value, eqdrc_module);

	return 0;
}

static int mixer_set_DRC_params(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value = ucontrol->value.integer.value[0];
	int eqdrc_module;

	aed_set_drc(value, aml_DRC_param_length, &aml_drc_table[0],
		aml_DRC_param_length, &aml_drc_tko_table[0]);

	eqdrc_module = aed_get_req_sel(0);
	aml_set_aed(value, eqdrc_module);

	return 0;
}

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new snd_eqdrc_controls[] = {
	/*
	 * 0:multiply gain after eq
	 * 1:multiply gain after ng
	 */
	SOC_SINGLE_EXT_TLV("EQ Volume Pos",
			 AED_EQ_VOLUME_G12X, 28, 0x1, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write,
			 NULL),

	SOC_SINGLE_EXT_TLV("EQ master volume",
			 AED_EQ_VOLUME_G12X, 16, 0x3FF, 1,
			 mixer_eqdrc_read, mixer_eqdrc_write,
			 mvol_tlv),

	SOC_SINGLE_EXT_TLV("EQ ch1 volume",
			 AED_EQ_VOLUME_G12X, 8, 0xFF, 1,
			 mixer_eqdrc_read, mixer_eqdrc_write,
			 chvol_tlv),

	SOC_SINGLE_EXT_TLV("EQ ch2 volume",
			 AED_EQ_VOLUME_G12X, 0, 0xFF, 1,
			 mixer_eqdrc_read, mixer_eqdrc_write,
			 chvol_tlv),

	SOC_SINGLE_EXT("EQ master volume mute",
			 AED_MUTE_G12X, 31, 0x1, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("EQ/DRC Channel Mask",
			 AED_TOP_CTL_G12X, 18, 0xff, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("EQ/DRC Lane Mask",
			 AED_TOP_CTL_G12X, 14, 0xf, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("EQ/DRC Req Module",
			 AED_TOP_REQ_CTL_G12X, 0, 0x7, 0,
			 mixer_eqdrc_read, mixer_set_AED_req_ctrl),

	SOC_SINGLE_EXT("EQ enable",
			 AED_EQ_EN_G12X, 0, 0x1, 0,
			 mixer_eqdrc_read, mixer_set_EQ),

	SOC_SINGLE_EXT("DRC enable",
			 AED_DRC_EN, 0, 0x1, 0,
			 mixer_eqdrc_read, mixer_set_DRC_params),

	SOC_SINGLE_EXT("NG enable",
			 AED_NG_CTL, 0, 0x1, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("NG noise thd",
			 AED_NG_THD0, 8, 0x7FFF, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("NG signal thd",
			 AED_NG_THD1, 8, 0x7FFF, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),

	SOC_SINGLE_EXT("NG counter thd",
			 AED_NG_CNT_THD, 0, 0xFFFF, 0,
			 mixer_eqdrc_read, mixer_eqdrc_write),
};

int card_add_effects_init(struct snd_soc_card *card)
{
	struct device_node *audio_effect_np;
	int eq_enable = -1, drc_enable = -1, eqdrc_module = -1;
	int lane_mask = -1, channel_mask = -1;

	audio_effect_np = of_parse_phandle(card->dev->of_node,
		"aml-audio-card,effect", 0);
	if (audio_effect_np == NULL) {
		pr_warn("no node %s for eq/drc info!\n", "audio_effect");
		return -EINVAL;
	}

	of_property_read_u32(audio_effect_np,
		"eq_enable",
		&eq_enable);
	of_property_read_u32(audio_effect_np,
		"drc_enable",
		&drc_enable);
	of_property_read_u32(audio_effect_np,
		"eqdrc_module",
		&eqdrc_module);
	of_property_read_u32(audio_effect_np,
		"lane_mask",
		&lane_mask);
	of_property_read_u32(audio_effect_np,
		"channel_mask",
		&channel_mask);

	init_EQ_DRC_module();

	if (eq_enable >= 0)
		aed_set_eq(1, aml_EQ_param_length, &aml_EQ_param[0]);

	if (drc_enable >= 0)
		aed_set_drc(1, aml_DRC_param_length, &aml_drc_table[0],
			aml_DRC_param_length, &aml_drc_tko_table[0]);

	/* sel 0 in default */
	if (eqdrc_module >= 0)
		aed_req_sel(false, 0, eqdrc_module);

	if (lane_mask >= 0)
		aed_set_lane(lane_mask);
	if (channel_mask >= 0)
		aed_set_channel(channel_mask);

	/* init eq/drc in defalut */
	set_internal_EQ_volume(0xc0, 0x30, 0x30);

	/* eq/drc mixer controls */
	return snd_soc_add_card_controls(card,
		snd_eqdrc_controls, ARRAY_SIZE(snd_eqdrc_controls));
}
