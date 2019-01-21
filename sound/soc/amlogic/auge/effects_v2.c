/*
 * sound/soc/amlogic/auge/effects_v2.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "effects_v2.h"
#include "effects_hw_v2.h"
#include "effects_hw_v2_coeff.h"
#include "ddr_mngr.h"
#include "regs.h"
#include "iomap.h"

#define DRV_NAME "Effects"

/*
 * AED Diagram
 * DC -- ND -- MIX -- EQ -- Mutiband DRC -- LR Vol
 *    -- Fullband DRC -- Master Volume
 */

enum {
	AED_DC,
	AED_ND,
	AED_EQ,
	AED_MDRC,
	AED_FDRC
};

struct effect_chipinfo {
	/* v1 is for G12X(g12a, g12b)
	 * v2 is for tl1
	 */
	bool v2;
};

struct audioeffect {
	struct device *dev;

	/* gate */
	struct clk *gate;
	/* source mpll */
	struct clk *srcpll;
	/* eqdrc clk */
	struct clk *clk;

	struct effect_chipinfo *chipinfo;

	bool dc_en;
	bool nd_en;
	bool eq_en;
	bool multiband_drc_en;
	bool fullband_drc_en;

	int lane_mask;
	int ch_mask;

	/*which module should be effected */
	int effect_module;
};

struct audioeffect *s_effect;

static struct audioeffect *get_audioeffects(void)
{
	if (!s_effect) {
		pr_info("Not init audio effects\n");
		return NULL;
	}

	return s_effect;
}

bool check_aed_v2(void)
{
	struct audioeffect *p_effect = get_audioeffects();

	if (!p_effect)
		return false;

	if (p_effect->chipinfo && p_effect->chipinfo->v2)
		return true;

	return false;
}

static int eqdrc_clk_set(struct audioeffect *p_effect)
{
	int ret = 0;

	ret = clk_prepare_enable(p_effect->clk);
	if (ret) {
		pr_err("Can't enable eqdrc clock: %d\n",
			ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(p_effect->srcpll);
	if (ret) {
		pr_err("Can't enable eqdrc src pll clock: %d\n",
			ret);
		return -EINVAL;
	}

	/* defaule clk */
	clk_set_rate(p_effect->clk, 200000000);

	pr_info("%s, src pll:%lu, clk:%lu\n",
		__func__,
		clk_get_rate(p_effect->srcpll),
		clk_get_rate(p_effect->clk));

	return 0;
}

static int mixer_aed_read(struct snd_kcontrol *kcontrol,
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

static int mixer_aed_write(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int new_val = (unsigned int)eqdrc_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	new_val &= max;
	new_val |= (value << shift);

	eqdrc_write(reg, new_val);

	return 0;
}

static int mixer_get_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &EQ_COEFF[0];
	int i;

	aed_get_ram_coeff(EQ_FILTER_RAM_ADD, EQ_FILTER_SIZE_CH, p);

	for (i = 0; i < EQ_FILTER_SIZE_CH; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int str2int(char *str, unsigned int *data, int size)
{
	int num = 0;
	unsigned int temp = 0;
	char *ptr = str;
	unsigned int *val = data;

	while (size-- != 0) {
		if ((*ptr >= '0') && (*ptr <= '9')) {
			temp = temp * 16 + (*ptr - '0');
		} else if ((*ptr >= 'a') && (*ptr <= 'f')) {
			temp = temp * 16 + (*ptr - 'a' + 10);
		} else if (*ptr == ' ') {
			*(val+num) = temp;
			temp = 0;
			num++;
		}
		ptr++;
	}

	return num;
}

static int mixer_set_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[FILTER_PARAM_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &EQ_COEFF[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (FILTER_PARAM_SIZE + 1) || band_id >= EQ_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &EQ_COEFF[band_id*FILTER_PARAM_SIZE];
	for (i = 0; i < FILTER_PARAM_SIZE; i++, p++, p_data++) {
		*p = *p_data;
		*(p + EQ_FILTER_SIZE_CH) = *p_data;
	}

	p = &EQ_COEFF[band_id*FILTER_PARAM_SIZE];
	aed_set_ram_coeff((EQ_FILTER_RAM_ADD +
		band_id*FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	p = &EQ_COEFF[band_id*FILTER_PARAM_SIZE + EQ_FILTER_SIZE_CH];
	aed_set_ram_coeff((EQ_FILTER_RAM_ADD +
		EQ_FILTER_SIZE_CH + band_id*FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	return 0;
}

static int mixer_get_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &CROSSOVER_COEFF[0];
	int i;

	aed_get_ram_coeff(CROSSOVER_FILTER_RAM_ADD, CROSSOVER_FILTER_SIZE, p);

	for (i = 0; i < CROSSOVER_FILTER_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[FILTER_PARAM_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &CROSSOVER_COEFF[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (FILTER_PARAM_SIZE + 1) ||
			band_id >= CROSSOVER_FILTER_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &CROSSOVER_COEFF[band_id*FILTER_PARAM_SIZE];
	for (i = 0; i < FILTER_PARAM_SIZE; i++)
		*p++ = *p_data++;

	p = &CROSSOVER_COEFF[band_id*FILTER_PARAM_SIZE];
	aed_set_ram_coeff((CROSSOVER_FILTER_RAM_ADD +
		band_id*FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	return 0;
}

static int mixer_get_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &multiband_drc_coeff[0];
	int i;

	for (i = 0; i < 3; i++) {
		aed_get_multiband_drc_coeff(i,
			p + i * AED_SINGLE_BAND_DRC_SIZE);
	}

	for (i = 0; i < AED_MULTIBAND_DRC_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_SINGLE_BAND_DRC_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[MULTIBAND_DRC_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &multiband_drc_coeff[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, MULTIBAND_DRC_PARAM_BYTE);

	num = str2int(p_string, p_data, MULTIBAND_DRC_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (AED_SINGLE_BAND_DRC_SIZE + 1) ||
			band_id >= AED_MULTIBAND_DRC_BANDS) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

    /*Don't update offset and gain*/
	p_data = &tmp_data[1];
	p = &multiband_drc_coeff[band_id*AED_SINGLE_BAND_DRC_SIZE];
	for (i = 0; i < (AED_SINGLE_BAND_DRC_SIZE - 2) ; i++)
		*p++ = *p_data++;

	p = &multiband_drc_coeff[0];
	aed_set_multiband_drc_coeff(band_id, p);

	return 0;
}

static int mixer_get_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &fullband_drc_coeff[0];
	int i;

	aed_get_fullband_drc_coeff(AED_FULLBAND_DRC_SIZE, p);

	for (i = 0; i < AED_FULLBAND_DRC_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FULLBAND_DRC_OFFSET + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_FULLBAND_DRC_BYTES];
	char *p_string = &tmp_string[0];
	unsigned int *p = &fullband_drc_coeff[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_FULLBAND_DRC_BYTES);

	num = str2int(p_string, p_data, AED_FULLBAND_DRC_BYTES);
	band_id = tmp_data[0];
	if (num != (AED_FULLBAND_DRC_OFFSET + 1) ||
			band_id >= AED_FULLBAND_DRC_GROUP_SIZE) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &fullband_drc_coeff[band_id*AED_FULLBAND_DRC_OFFSET];
	for (i = 0; i < AED_FULLBAND_DRC_OFFSET; i++)
		*p++ = *p_data++;

	p = &fullband_drc_coeff[band_id*AED_FULLBAND_DRC_OFFSET];
	aed_set_fullband_drc_coeff(band_id, p);

	return 0;
}

/* aed module
 * check to sync with enum frddr_dest in ddr_mngr.h
 */
static const char *const aed_module_texts[] = {
	"TDMOUT_A",
	"TDMOUT_B",
	"TDMOUT_C",
	"SPDIFOUT_A",
	"SPDIFOUT_B",
};

static const struct soc_enum aed_module_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(aed_module_texts),
			aed_module_texts);

static int aed_module_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);

	if (!p_effect)
		return -EINVAL;

	ucontrol->value.enumerated.item[0] = p_effect->effect_module;

	return 0;
}

static int aed_module_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);

	if (!p_effect)
		return -EINVAL;

	p_effect->effect_module = ucontrol->value.enumerated.item[0];

	/* update info to ddr and modules */
	aml_set_aed(1, p_effect->effect_module);

	return 0;
}

static void aed_set_filter_data(void)
{
	int *p;

	/* set default filter param*/
	p = &DC_CUT_COEFF[0];
	aed_set_ram_coeff(DC_CUT_FILTER_RAM_ADD, DC_CUT_FILTER_SIZE, p);
	p = &EQ_COEFF[0];
	aed_set_ram_coeff(EQ_FILTER_RAM_ADD, EQ_FILTER_SIZE, p);
	p = &CROSSOVER_COEFF[0];
	aed_set_ram_coeff(CROSSOVER_FILTER_RAM_ADD, CROSSOVER_FILTER_SIZE, p);

}

static void aed_set_drc_data(void)
{
	unsigned int *p = &multiband_drc_coeff[0];
	int i;

	/*set MDRC default value*/
	for (i = 0; i < AED_MULTIBAND_DRC_BANDS; i++)
		aed_set_multiband_drc_coeff(i, p);


	/*set FDRC default value*/
	p = &fullband_drc_coeff[0];
	for (i = 0; i < AED_FULLBAND_DRC_GROUP_SIZE; i++) {
		aed_set_fullband_drc_coeff(i,
			p + i * AED_FULLBAND_DRC_OFFSET);
	}
}

static const DECLARE_TLV_DB_SCALE(master_vol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(lr_vol_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new snd_effect_controls[] = {

	SOC_SINGLE_EXT("AED DC cut enable",
		AED_DC_EN, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Noise Detect enable",
		AED_ND_CNTL, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED EQ enable",
		AED_EQ_EN, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED EQ Parameters",
		(EQ_FILTER_SIZE_CH * 4),
		mixer_get_EQ_params,
		mixer_set_EQ_params),

	SOC_SINGLE_EXT("AED Multi-band DRC enable",
		AED_MDRC_CNTL, 8, 0x1, 0,
		mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED Crossover Filter Parameters",
		(CROSSOVER_FILTER_SIZE * 4),
		mixer_get_crossover_params,
		mixer_set_crossover_params),

	SND_SOC_BYTES_EXT("AED Multi-band DRC Parameters",
		(AED_MULTIBAND_DRC_SIZE * 4),
		mixer_get_multiband_DRC_params,
		mixer_set_multiband_DRC_params),

	SOC_SINGLE_EXT("AED Full-band DRC enable",
		AED_DRC_CNTL, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED Full-band DRC Parameters",
		AED_FULLBAND_DRC_BYTES,
		mixer_get_fullband_DRC_params,
		mixer_set_fullband_DRC_params),

	SOC_ENUM_EXT("AED module",
		aed_module_enum,
		aed_module_get_enum,
		aed_module_set_enum),

	SOC_SINGLE_EXT_TLV("AED Lch volume",
		AED_EQ_VOLUME, 0, 0xFF, 1,
		mixer_aed_read, mixer_aed_write,
		lr_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED Rch volume",
		AED_EQ_VOLUME, 8, 0xFF, 1,
		mixer_aed_read, mixer_aed_write,
		lr_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED master volume",
		AED_EQ_VOLUME, 16, 0x3FF, 1,
		mixer_aed_read, mixer_aed_write,
		master_vol_tlv),
};

int card_add_effect_v2_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	if (!s_effect) {
		pr_info("effect_v2 is not init\n");
		return 0;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_effect_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&snd_effect_controls[idx],
					s_effect));
		if (err < 0)
			return err;
	}

	return 0;
}

static struct effect_chipinfo tl1_effect_chipinfo = {
	.v2 = true,
};

static const struct of_device_id effect_device_id[] = {
	{
		.compatible = "amlogic, snd-effect-v1"
	},
	{
		.compatible = "amlogic, snd-effect-v2",
		.data       = &tl1_effect_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-effect",
		.data       = &tl1_effect_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, effect_device_id);

static int effect_platform_probe(struct platform_device *pdev)
{
	struct audioeffect *p_effect;
	struct device *dev = &pdev->dev;
	struct effect_chipinfo *p_chipinfo;
	bool eq_enable = false;
	bool multiband_drc_enable = false;
	bool fullband_drc_enable = false;
	int lane_mask = -1, channel_mask = -1, eqdrc_module = -1;
	int ret;

	pr_info("%s, line:%d\n", __func__, __LINE__);

	p_effect = devm_kzalloc(&pdev->dev,
			sizeof(struct audioeffect),
			GFP_KERNEL);
	if (!p_effect) {
		dev_err(&pdev->dev, "Can't allocate pcm_p\n");
		return -ENOMEM;
	}

	/* match data */
	p_chipinfo = (struct effect_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo)
		dev_warn_once(dev, "check whether to update effect chipinfo\n");
	p_effect->chipinfo = p_chipinfo;

	p_effect->gate = devm_clk_get(&pdev->dev, "gate");
	if (IS_ERR(p_effect->gate)) {
		dev_err(&pdev->dev,
			"Can't retrieve eqdrc gate clock\n");
		ret = PTR_ERR(p_effect->gate);
		return ret;
	}

	p_effect->srcpll = devm_clk_get(&pdev->dev, "srcpll");
	if (IS_ERR(p_effect->srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve source mpll clock\n");
		ret = PTR_ERR(p_effect->srcpll);
		return ret;
	}

	p_effect->clk = devm_clk_get(&pdev->dev, "eqdrc");
	if (IS_ERR(p_effect->clk)) {
		dev_err(&pdev->dev,
			"Can't retrieve eqdrc clock\n");
		ret = PTR_ERR(p_effect->clk);
		return ret;
	}

	ret = clk_set_parent(p_effect->clk, p_effect->srcpll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set eqdrc clock parent clock\n");
		ret = PTR_ERR(p_effect->clk);
		return ret;
	}

	ret = eqdrc_clk_set(p_effect);
	if (ret < 0) {
		dev_err(&pdev->dev, "set eq drc module clk fail!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"eqdrc_module",
			&eqdrc_module);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve eqdrc_module\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"lane_mask",
			&lane_mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve lane_mask\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"channel_mask",
			&channel_mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve channel_mask\n");
		return -EINVAL;
	}

	pr_info("%s \t module:%d, lane_mask:%d, ch_mask:%d\n",
		__func__,
		eqdrc_module,
		lane_mask,
		channel_mask
		);

	/* config from dts */
	p_effect->eq_en            = eq_enable;
	p_effect->multiband_drc_en = multiband_drc_enable;
	p_effect->fullband_drc_en  = fullband_drc_enable;
	p_effect->lane_mask        = lane_mask;
	p_effect->ch_mask          = channel_mask;
	p_effect->effect_module    = eqdrc_module;

	/*set eq/drc module lane & channels*/
	aed_set_lane_and_channels(lane_mask, channel_mask);
	/*set master & channel volume gain to 0dB*/
	aed_set_volume(0xc0, 0x30, 0x30);
	/*set default mixer gain*/
	aed_set_mixer_params();
	/*all 20 bands for EQ1*/
	aed_eq_taps(EQ_BAND);
	/*set default filter param*/
	aed_set_filter_data();
	/*set multi-band drc param*/
	aed_set_multiband_drc_param();
	/*set multi/full-band drc data*/
	aed_set_drc_data();
	/*set full-band drc param, enable 2 band*/
	aed_set_fullband_drc_param(2);
	/*set EQ/DRC module enable*/
	aml_set_aed(1, p_effect->effect_module);

	p_effect->dev = dev;
	s_effect = p_effect;
	dev_set_drvdata(&pdev->dev, p_effect);

	return 0;
}

static struct platform_driver effect_platform_driver = {
	.driver = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(effect_device_id),
	},
	.probe  = effect_platform_probe,
};
module_platform_driver(effect_platform_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("Amlogic Audio Effects driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
