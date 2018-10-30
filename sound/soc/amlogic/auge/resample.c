/*
 * sound/soc/amlogic/auge/resample.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "resample.h"
#include "resample_hw.h"
#include "ddr_mngr.h"
#include "regs.h"
#include "iomap.h"

#define DRV_NAME "audioresample"

#define CLK_RATIO     256

/*#define __PTM_RESAMPLE_CLK__*/

#define RESAMPLE_A    0
#define RESAMPLE_B    1

struct resample_chipinfo {
	int num;    /* support resample a/b */
	int id;

	bool dividor_fn;
};

struct audioresample {
	struct device *dev;

	/* mpll0~3, hifi pll, div3~4, gp0 */
	struct clk *pll;
	/* mst_mclk_a~f, slv_sclk_a~j */
	struct clk *sclk;
	/* resample clk */
	struct clk *clk;

	struct resample_chipinfo *chipinfo;

	int id;

	/*which module should be resampled */
	int resample_module;
	/*  resample to the rate */
	int out_rate;

	/* sync with auge_resample_texts */
	int asrc_rate_idx;

	bool enable;
};

struct audioresample *s_resample_a;

struct audioresample *s_resample_b;

static struct audioresample *get_audioresample(int id)
{
	struct audioresample *p_resample;

	p_resample = ((id == 0) ? s_resample_a : s_resample_b);

	if (!p_resample) {
		pr_info("Not init audio resample\n");
		return NULL;
	}

	return p_resample;
}

int get_resample_module_num(void)
{
	struct audioresample *p_resample = get_audioresample(0);

	if (p_resample && p_resample->chipinfo)
		return p_resample->chipinfo->num;

	return 1;
}

static int resample_clk_set(struct audioresample *p_resample)
{
	int ret = 0;

	/* enable clock */
	if (p_resample->enable) {
		ret = clk_prepare_enable(p_resample->clk);
		if (ret) {
			pr_err("Can't enable resample_clk clock: %d\n",
				ret);
			return -EINVAL;
		}

		ret = clk_prepare_enable(p_resample->sclk);
		if (ret) {
			pr_err("Can't enable resample_src clock: %d\n",
				ret);
			return -EINVAL;
		}

		if (p_resample->out_rate) {
#ifdef __PTM_RESAMPLE_CLK__
			clk_set_rate(p_resample->pll,
				p_resample->out_rate * CLK_RATIO * 2 * 14);
#else
			clk_set_rate(p_resample->pll,
				p_resample->out_rate * CLK_RATIO * 2);
#endif
			clk_set_rate(p_resample->sclk,
				p_resample->out_rate * CLK_RATIO);
			clk_set_rate(p_resample->clk,
				p_resample->out_rate * CLK_RATIO);
		} else {
			/* defaule resample clk */
			clk_set_rate(p_resample->pll, 48000 * CLK_RATIO * 2);
			clk_set_rate(p_resample->sclk, 48000 * CLK_RATIO);
			clk_set_rate(p_resample->clk, 48000 * CLK_RATIO);
		}

		ret = clk_prepare_enable(p_resample->pll);
		if (ret) {
			pr_err("Can't enable pll clock: %d\n", ret);
			return -EINVAL;
		}

		pr_info("%s, resample_pll:%lu, sclk:%lu, clk:%lu\n",
			__func__,
			clk_get_rate(p_resample->pll),
			clk_get_rate(p_resample->sclk),
			clk_get_rate(p_resample->clk));
	} else {
		clk_disable_unprepare(p_resample->clk);
		clk_disable_unprepare(p_resample->sclk);
		clk_disable_unprepare(p_resample->pll);
	}

	return ret;
}

static void audio_resample_init(struct audioresample *p_resample)
{
	resample_clk_set(p_resample);

	aml_set_resample(p_resample->id, p_resample->enable,
		p_resample->resample_module);
}

static int audio_resample_set(
	struct audioresample *p_resample,
	bool enable, int rate)
{
	if (!p_resample)
		return 0;

	p_resample->enable = enable;
	p_resample->out_rate = rate;
	audio_resample_init(p_resample);

	return 0;
}

static const char *const auge_resample_texts[] = {
	"Disable",
	"Enable:32K",
	"Enable:44K",
	"Enable:48K",
	"Enable:88K",
	"Enable:96K",
	"Enable:176K",
	"Enable:192K",
};

static int resample_idx2rate(int index)
{
	int rate = 0;

	if (index == 0)
		rate = 0;
	else if (index == 1)
		rate = 32000;
	else if (index == 2)
		rate = 44100;
	else if (index == 3)
		rate = 48000;
	else if (index == 4)
		rate = 88200;
	else if (index == 5)
		rate = 96000;
	else if (index == 6)
		rate = 176400;
	else if (index == 7)
		rate = 192000;

	return rate;
}

static const struct soc_enum auge_resample_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(auge_resample_texts),
			auge_resample_texts);

static int resample_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioresample *p_resample = snd_kcontrol_chip(kcontrol);

	if (!p_resample) {
		pr_info("audio resample is not init\n");
		return 0;
	}

	ucontrol->value.enumerated.item[0] = p_resample->asrc_rate_idx;

	return 0;
}

int resample_set(int id, int index)
{
	int resample_rate = resample_idx2rate(index);
	struct audioresample *p_resample = get_audioresample(id);

	if (!p_resample)
		return 0;

	if (index == p_resample->asrc_rate_idx)
		return 0;

	p_resample->asrc_rate_idx = index;

	pr_info("%s %s\n",
		__func__,
		auge_resample_texts[index]);

	if (audio_resample_set(p_resample, (bool)index, resample_rate))
		return 0;

	if ((index == 0) || (resample_rate == 0))
		resample_disable(p_resample->id);
	else {
		resample_init(p_resample->id, resample_rate);

		resample_set_hw_param(p_resample->id, index - 1);
	}

	return 0;
}

static int resample_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioresample *p_resample = snd_kcontrol_chip(kcontrol);
	int index = ucontrol->value.enumerated.item[0];

	if (!p_resample) {
		pr_info("audio resample is not init\n");
		return 0;
	}

	resample_set(p_resample->id, index);

	return 0;
}

static int mixer_audiobus_read(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value =
			(((unsigned int)audiobus_read(reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mixer_audiobus_write(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int reg_value = (unsigned int)audiobus_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	reg_value &= max;
	reg_value |= (value << shift);
	audiobus_write(reg, reg_value);

	return 0;
}

/* resample module
 * keep sync with enum toddr_src in ddr_mngr.h
 */
static const char *const auge_resample_module_texts[] = {
	"TDMIN_A",
	"TDMIN_B",
	"TDMIN_C",
	"SPDIFIN",
	"PDMIN",
	"FRATV", /* NONE for axg, g12a, g12b */
	"TDMIN_LB",
	"LOOPBACK_A",
	"FRHDMIRX", /* from tl1 chipset*/
	"LOOPBACK_B",
	"SPDIFIN_LB",
	"VAD",
};

static const struct soc_enum auge_resample_module_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(auge_resample_module_texts),
			auge_resample_module_texts);

static int resample_module_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioresample *p_resample =  snd_kcontrol_chip(kcontrol);

	if (!p_resample) {
		pr_info("audio resample is not init\n");
		return 0;
	}

	ucontrol->value.enumerated.item[0] = p_resample->resample_module;

	return 0;
}

static int resample_module_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioresample *p_resample =  snd_kcontrol_chip(kcontrol);

	if (!p_resample) {
		pr_info("audio resample is not init\n");
		return 0;
	}

	p_resample->resample_module = ucontrol->value.enumerated.item[0];

	/* update info to ddr */
	aml_set_resample(p_resample->id,
		p_resample->enable,
		p_resample->resample_module);

	return 0;
}

static const struct snd_kcontrol_new asrc_a_controls[] = {
	SOC_ENUM_EXT("Hardware resample enable",
		     auge_resample_enum,
		     resample_get_enum,
		     resample_set_enum),
	SOC_SINGLE_EXT_TLV("Hw resample pause enable",
			 EE_AUDIO_RESAMPLEA_CTRL2, 24, 0x1, 0,
			 mixer_audiobus_read, mixer_audiobus_write,
			 NULL),
	SOC_SINGLE_EXT_TLV("Hw resample pause thd",
			 EE_AUDIO_RESAMPLEA_CTRL2, 0, 0xffffff, 0,
			 mixer_audiobus_read, mixer_audiobus_write,
			 NULL),
	SOC_ENUM_EXT("Hw resample module",
		     auge_resample_module_enum,
		     resample_module_get_enum,
		     resample_module_set_enum),
};

static const struct snd_kcontrol_new asrc_b_controls[] = {
	SOC_ENUM_EXT("Hardware resample b enable",
		     auge_resample_enum,
		     resample_get_enum,
		     resample_set_enum),
	SOC_SINGLE_EXT_TLV("Hw resample b pause enable",
			 EE_AUDIO_RESAMPLEB_CTRL2, 24, 0x1, 0,
			 mixer_audiobus_read, mixer_audiobus_write,
			 NULL),
	SOC_SINGLE_EXT_TLV("Hw resample b pause thd",
			 EE_AUDIO_RESAMPLEB_CTRL2, 0, 0xffffff, 0,
			 mixer_audiobus_read, mixer_audiobus_write,
			 NULL),
	SOC_ENUM_EXT("Hw resample b module",
		     auge_resample_module_enum,
		     resample_module_get_enum,
		     resample_module_set_enum),
};

int card_add_resample_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	if (s_resample_a) {
		for (idx = 0; idx < ARRAY_SIZE(asrc_a_controls); idx++) {
			err = snd_ctl_add(card->snd_card,
					snd_ctl_new1(&asrc_a_controls[idx],
						s_resample_a));
			if (err < 0)
				return err;
		}
	}

	if (s_resample_b) {
		for (idx = 0; idx < ARRAY_SIZE(asrc_b_controls); idx++) {
			err = snd_ctl_add(card->snd_card,
					snd_ctl_new1(&asrc_b_controls[idx],
						s_resample_b));
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static struct resample_chipinfo g12a_resample_chipinfo = {
	.dividor_fn = true,
};

static struct resample_chipinfo tl1_resample_a_chipinfo = {
	.num        = 2,
	.id         = RESAMPLE_A,
	.dividor_fn = true,
};

static struct resample_chipinfo tl1_resample_b_chipinfo = {
	.num        = 2,
	.id         = RESAMPLE_B,
	.dividor_fn = true,
};

static const struct of_device_id resample_device_id[] = {
	{
		.compatible = "amlogic, axg-resample",
	},
	{
		.compatible = "amlogic, g12a-resample",
		.data = &g12a_resample_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-resample-a",
		.data = &tl1_resample_a_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-resample-b",
		.data = &tl1_resample_b_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, resample_device_id);

static int resample_platform_probe(struct platform_device *pdev)
{
	struct audioresample *p_resample;
	struct device *dev = &pdev->dev;
	struct resample_chipinfo *p_chipinfo;
	unsigned int resample_module;
	int ret;

	pr_info("%s\n", __func__);

	p_resample = devm_kzalloc(&pdev->dev,
			sizeof(struct audioresample),
			GFP_KERNEL);
	if (!p_resample) {
		/*dev_err(&pdev->dev, "Can't allocate pcm_p\n");*/
		return -ENOMEM;
	}

	/* match data */
	p_chipinfo = (struct resample_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo)
		dev_warn_once(dev, "check whether to update resample chipinfo\n");
	else
		p_resample->id = p_chipinfo->id;

	p_resample->chipinfo = p_chipinfo;

	ret = of_property_read_u32(pdev->dev.of_node, "resample_module",
			&resample_module);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve resample_module\n");
		return -EINVAL;
	}

	/* config from dts */
	p_resample->resample_module = resample_module;

	p_resample->pll = devm_clk_get(&pdev->dev, "resample_pll");
	if (IS_ERR(p_resample->pll)) {
		dev_err(&pdev->dev,
			"Can't retrieve resample_pll clock\n");
		ret = PTR_ERR(p_resample->pll);
		return ret;
	}
	p_resample->sclk = devm_clk_get(&pdev->dev, "resample_src");
	if (IS_ERR(p_resample->sclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve resample_src clock\n");
		ret = PTR_ERR(p_resample->sclk);
		return ret;
	}
	p_resample->clk = devm_clk_get(&pdev->dev, "resample_clk");
	if (IS_ERR(p_resample->clk)) {
		dev_err(&pdev->dev,
			"Can't retrieve resample_clk clock\n");
		ret = PTR_ERR(p_resample->clk);
		return ret;
	}

	ret = clk_set_parent(p_resample->sclk, p_resample->pll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set resample_src parent clock\n");
		ret = PTR_ERR(p_resample->sclk);
		return ret;
	}
	ret = clk_set_parent(p_resample->clk, p_resample->sclk);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set resample_clk parent clock\n");
		ret = PTR_ERR(p_resample->clk);
		return ret;
	}

	p_resample->dev = dev;

	if (p_chipinfo && p_chipinfo->id == 1)
		s_resample_b = p_resample;
	else
		s_resample_a = p_resample;

	return 0;
}


static struct platform_driver resample_platform_driver = {
	.driver = {
		.name  = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(resample_device_id),
	},
	.probe  = resample_platform_probe,
};
module_platform_driver(resample_platform_driver);

