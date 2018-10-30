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
#include "effects_hw_v2_coeff.c"
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
	int mask_en;

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

static void check_set_aed_top(
	struct audioeffect *p_effect,
	int mask_new, bool enable)
{
	int mask_last = p_effect->mask_en;
	bool update_aed_top = false;

	pr_info("%s, mask:0x%x\n", __func__, mask_new);

	if (enable)
		p_effect->mask_en |= mask_new;
	else
		p_effect->mask_en &= ~mask_new;

	if (enable && (!mask_last) && p_effect->mask_en)
		update_aed_top = true; /* to enable */
	else if ((!enable) && mask_last && (!p_effect->mask_en))
		update_aed_top = true; /* to disable */

	if (update_aed_top)
		aml_set_aed(enable, p_effect->effect_module);
}

static int mixer_aed_enable_DC(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);

	p_effect->dc_en = ucontrol->value.integer.value[0];

	aed_dc_enable(p_effect->dc_en);

	check_set_aed_top(p_effect,
		0x1 << AED_DC,
		p_effect->dc_en);

	return 0;
}

static int mixer_aed_enable_ND(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);

	p_effect->nd_en = ucontrol->value.integer.value[0];

	aed_nd_enable(p_effect->nd_en);

	check_set_aed_top(p_effect,
		0x1 << AED_ND,
		p_effect->nd_en);

	return 0;
}

static int mixer_aed_enable_EQ(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);
	int *p_eq_coeff = eq_coeff;
	int len = ARRAY_SIZE(eq_coeff);

	p_effect->eq_en = ucontrol->value.integer.value[0];

	aed_set_ram_coeff(len, p_eq_coeff);
	aed_eq_enable(0, p_effect->eq_en);

	check_set_aed_top(p_effect,
		0x1 << AED_EQ,
		p_effect->eq_en);

	return 0;
}

static int mixer_aed_enable_multiband_DRC(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);
	int *p_multiband_coeff = multiband_drc_coeff;
	int len = ARRAY_SIZE(multiband_drc_coeff);

	if (!p_effect)
		return -EINVAL;

	p_effect->multiband_drc_en = ucontrol->value.integer.value[0];

	aed_set_multiband_drc_coeff(len, p_multiband_coeff);
	aed_multiband_drc_enable(p_effect->multiband_drc_en);

	check_set_aed_top(p_effect,
		0x1 << AED_MDRC,
		p_effect->multiband_drc_en);

	return 0;
}

static int mixer_aed_enable_fullband_DRC(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect =  snd_kcontrol_chip(kcontrol);
	int *p_fullband_coeff = fullband_drc_coeff;
	int len = ARRAY_SIZE(fullband_drc_coeff);

	if (!p_effect)
		return -EINVAL;

	p_effect->fullband_drc_en = ucontrol->value.integer.value[0];

	aed_set_fullband_drc_coeff(len, p_fullband_coeff);
	aed_fullband_drc_enable(p_effect->fullband_drc_en);

	check_set_aed_top(p_effect,
		0x1 << AED_FDRC,
		p_effect->fullband_drc_en);

	return 0;
}

static int mixer_get_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int *val = (int *)ucontrol->value.bytes.data;
	int *p = &eq_coeff[0];

	memcpy(val, p, AED_EQ_LENGTH);

	return 0;
}


static int mixer_set_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	int *val, *p = &eq_coeff[0];

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (int *)data;
	memcpy(p, val, params->max / sizeof(int));

	kfree(data);

	return 0;
}

static int mixer_get_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int *val = (int *)ucontrol->value.bytes.data;
	int *p = &multiband_drc_coeff[0];

	memcpy(val, p, AED_MULTIBAND_DRC_LENGTH);

	return 0;
}

static int mixer_set_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	int *val, *p = &multiband_drc_coeff[0];

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (int *)data;
	memcpy(p, val, params->max / sizeof(int));

	kfree(data);

	return 0;
}

static int mixer_get_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int *val = (int *)ucontrol->value.bytes.data;
	int *p = &fullband_drc_coeff[0];

	memcpy(val, p, AED_FULLBAND_DRC_LENGTH);

	return 0;
}

static int mixer_set_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;
	int *val, *p = &fullband_drc_coeff[0];

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (int *)data;
	memcpy(p, val, params->max / sizeof(int));

	kfree(data);

	return 0;
}

/* aed module
 * check to sync with enum frddr_dest in ddr_mngr.h
 */
static const char *const aed_module_texts[] = {
	"TDMOUT_A",
	"TDMOUT_B",
	"TDMOUT_C",
	"SPDIFIN",
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
	aed_set_ctrl(
		p_effect->eq_en |
		p_effect->multiband_drc_en |
		p_effect->fullband_drc_en,
		0,
		p_effect->effect_module);

	return 0;
}

static const DECLARE_TLV_DB_SCALE(master_vol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(lr_vol_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new snd_effect_controls[] = {

	SOC_SINGLE_EXT("AED DC cut enable",
		AED_DC_EN, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_enable_DC),

	SOC_SINGLE_EXT("AED Noise Detect enable",
		AED_ND_CNTL, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_enable_ND),

	SOC_SINGLE_EXT("AED EQ enable",
		AED_EQ_EN, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_enable_EQ),

	SOC_SINGLE_EXT("AED Multi-band DRC enable",
		AED_MDRC_CNTL, 8, 0x1, 0,
		mixer_aed_read, mixer_aed_enable_multiband_DRC),

	SOC_SINGLE_EXT("AED Full-band DRC enable",
		AED_DRC_CNTL, 0, 0x1, 0,
		mixer_aed_read, mixer_aed_enable_fullband_DRC),

	SND_SOC_BYTES_EXT("AED EQ Parameters",
		AED_EQ_LENGTH,
		mixer_get_EQ_params,
		mixer_set_EQ_params),

	SND_SOC_BYTES_EXT("AED Multi-band DRC Parameters",
		AED_MULTIBAND_DRC_LENGTH,
		mixer_get_multiband_DRC_params,
		mixer_set_multiband_DRC_params),

	SND_SOC_BYTES_EXT("AED Full-band DRC Parameters",
		AED_FULLBAND_DRC_LENGTH,
		mixer_get_fullband_DRC_params,
		mixer_set_fullband_DRC_params),

	SOC_ENUM_EXT("AED module",
		aed_module_enum,
		aed_module_get_enum,
		aed_module_set_enum),

	SOC_SINGLE_EXT("AED Lane mask",
		AED_TOP_CTL, 14, 0xF, 0,
		mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Channel mask",
		AED_TOP_CTL, 18, 0xFF, 0,
		mixer_aed_read, mixer_aed_write),

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

#if 0
static const struct snd_soc_component_driver effect_component_drv = {
	.name               = DRV_NAME,

	.controls		    = snd_effect_controls,
	.num_controls		= ARRAY_SIZE(snd_effect_controls),
};
#endif

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

	eqdrc_clk_set(p_effect);

	eq_enable = of_property_read_bool(pdev->dev.of_node,
			"eq_enable");

	multiband_drc_enable = of_property_read_bool(pdev->dev.of_node,
			"multiband_drc_enable");

	fullband_drc_enable = of_property_read_bool(pdev->dev.of_node,
			"fullband_drc_enable");

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

	pr_info("%s \t eq_en:%d, multi-band drc en:%d, full-band drc en:%d, module:%d, lane_mask:%d, ch_mask:%d\n",
		__func__,
		eq_enable,
		multiband_drc_enable,
		fullband_drc_enable,
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

	aed_set_lane_and_channels(lane_mask, channel_mask);
	aed_set_EQ_volume(0xc0, 0x30, 0x30);

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
