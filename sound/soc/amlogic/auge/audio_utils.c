/*
 * sound/soc/amlogic/auge/audio_utils.c
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

#include "audio_utils.h"
#include "regs.h"
#include "iomap.h"
#include "loopback_hw.h"

static unsigned int loopback_enable;

static const char *const loopback_enable_texts[] = {
	"Disable",
	"Enable",
};

static const struct soc_enum loopback_enable_enum =
	SOC_ENUM_SINGLE(EE_AUDIO_LB_CTRL0,
			31,
			ARRAY_SIZE(loopback_enable_texts),
			loopback_enable_texts);


static int loopback_enable_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = loopback_enable;

	return 0;
}

static int loopback_enable_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	loopback_enable = ucontrol->value.enumerated.item[0];

	return 0;
}


static unsigned int loopback_datain;

static const char *const loopback_datain_texts[] = {
	"Tdmin A",
	"Tdmin B",
	"Tdmin C",
	"Spdif In",
	"Pdm In",
};

static const struct soc_enum loopback_datain_enum =
	SOC_ENUM_SINGLE(EE_AUDIO_LB_CTRL0, 0, ARRAY_SIZE(loopback_datain_texts),
			loopback_datain_texts);


static int loopback_datain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = loopback_datain;

	return 0;
}

static int loopback_datain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	loopback_datain = ucontrol->value.enumerated.item[0];
	audiobus_update_bits(EE_AUDIO_LB_CTRL0, 0, loopback_datain);

	return 0;
}

static unsigned int loopback_tdminlb;

static const char *const loopback_tdminlb_texts[] = {
	"tdmoutA",
	"tdmoutB",
	"tdmoutC",
	"tdminA",
	"tdminB",
	"tdminC",
};

static const struct soc_enum loopback_tdminlb_enum =
	SOC_ENUM_SINGLE(EE_AUDIO_TDMIN_LB_CTRL,
			20,
			ARRAY_SIZE(loopback_tdminlb_texts),
			loopback_tdminlb_texts);


static int loopback_tdminlb_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = loopback_tdminlb;

	return 0;
}

static int loopback_tdminlb_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	loopback_tdminlb = ucontrol->value.enumerated.item[0];
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL,
		0xf << 20,
		loopback_datain);

	return 0;
}


static const struct snd_kcontrol_new snd_auge_controls[] = {
	/* loopback enable */
	SOC_ENUM_EXT("Loopback Enable",
		     loopback_enable_enum,
		     loopback_enable_get_enum,
		     loopback_enable_set_enum),

	/* loopback data in source */
	SOC_ENUM_EXT("Loopback datain source",
		     loopback_datain_enum,
		     loopback_datain_get_enum,
		     loopback_datain_set_enum),

	/* loopback data tdmin lb */
	SOC_ENUM_EXT("Loopback tmdin lb source",
		     loopback_tdminlb_enum,
		     loopback_tdminlb_get_enum,
		     loopback_tdminlb_set_enum),

};


int snd_card_add_kcontrols(struct snd_soc_card *card)
{
	pr_info("%s card:%p\n", __func__, card);
	return snd_soc_add_card_controls(card,
		snd_auge_controls, ARRAY_SIZE(snd_auge_controls));

}

int loopback_parse_of(struct device_node *node,
	struct loopback_cfg *lb_cfg)
{
	int ret;

	ret = of_property_read_u32(node, "lb_mode",
		&lb_cfg->lb_mode);
	if (ret) {
		pr_err("failed to get lb_mode, set it default\n");
		lb_cfg->lb_mode = 0;
		ret = 0;
	}

	ret = of_property_read_u32(node, "datain_src",
		&lb_cfg->datain_src);
	if (ret) {
		pr_err("failed to get datain_src\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datain_chnum",
		&lb_cfg->datain_chnum);
	if (ret) {
		pr_err("failed to get datain_chnum\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datain_chmask",
		&lb_cfg->datain_chmask);
	if (ret) {
		pr_err("failed to get datain_chmask\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = of_property_read_u32(node, "datalb_src",
		&lb_cfg->datalb_src);
	if (ret) {
		pr_err("failed to get datalb_src\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datalb_chnum",
		&lb_cfg->datalb_chnum);
	if (ret) {
		pr_err("failed to get datalb_chnum\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datalb_chmask",
		&lb_cfg->datalb_chmask);
	if (ret) {
		pr_err("failed to get datalb_chmask\n");
		ret = -EINVAL;
		goto fail;
	}


	loopback_datain = lb_cfg->datain_src;
	loopback_tdminlb = lb_cfg->datalb_src;

	pr_info("parse loopback:, \tlb mode:%d\n",
		lb_cfg->lb_mode);
	pr_info("\tdatain_src:%d, datain_chnum:%d, datain_chumask:%x\n",
		lb_cfg->datain_src,
		lb_cfg->datain_chnum,
		lb_cfg->datain_chmask);
	pr_info("\tdatalb_src:%d, datalb_chnum:%d, datalb_chumask:%x\n",
		lb_cfg->datalb_src,
		lb_cfg->datalb_chnum,
		lb_cfg->datalb_chmask);

fail:
	return ret;
}

int loopback_prepare(
	struct snd_pcm_substream *substream,
	struct loopback_cfg *lb_cfg)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int bitwidth;
	unsigned int datain_toddr_type, datalb_toddr_type;
	unsigned int datain_msb, datain_lsb, datalb_msb, datalb_lsb;
	struct lb_cfg datain_cfg;
	struct lb_cfg datalb_cfg;
	struct audio_data ddrdata;
	struct data_in datain;
	struct data_lb datalb;

	if (!lb_cfg)
		return -EINVAL;

	bitwidth = snd_pcm_format_width(runtime->format);
	switch (lb_cfg->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
	case DATAIN_PDM:
		datain_toddr_type = 0;
		datain_msb = 32 - 1;
		datain_lsb = 0;
		break;
	case DATAIN_SPDIF:
		datain_toddr_type = 3;
		datain_msb = 27;
		datain_lsb = 4;
		if (bitwidth <= 24)
			datain_lsb = 28 - bitwidth;
		else
			datain_lsb = 4;
		break;
	default:
		pr_err("unsupport data in source:%d\n",
			lb_cfg->datain_src);
		return -EINVAL;
	}

	switch (lb_cfg->datalb_src) {
	case TDMINLB_TDMOUTA:
	case TDMINLB_TDMOUTB:
	case TDMINLB_TDMOUTC:
	case TDMINLB_PAD_TDMINA:
	case TDMINLB_PAD_TDMINB:
	case TDMINLB_PAD_TDMINC:
		if (bitwidth == 24) {
			datalb_toddr_type = 4;
			datalb_msb = 32 - 1;
			datalb_lsb = 32 - bitwidth;
		} else {
			datalb_toddr_type = 0;
			datalb_msb = 32 - 1;
			datalb_lsb = 0;
		}
		break;
	default:
		pr_err("unsupport data lb source:%d\n",
			lb_cfg->datalb_src);
		return -EINVAL;
	}

	datain_cfg.ext_signed    = 0;
	datain_cfg.chnum         = runtime->channels;
	datain_cfg.chmask        = lb_cfg->datain_chmask;
	ddrdata.combined_type    = datain_toddr_type;
	ddrdata.msb              = datain_msb;
	ddrdata.lsb              = datain_lsb;
	ddrdata.src              = lb_cfg->datain_src;
	datain.config            = &datain_cfg;
	datain.ddrdata           = &ddrdata;

	datalb_cfg.ext_signed    = 0;
	datalb_cfg.chnum         = lb_cfg->datalb_chnum;
	datalb_cfg.chmask        = lb_cfg->datalb_chmask;
	datalb.config            = &datalb_cfg;
	datalb.ddr_type          = datalb_toddr_type;
	datalb.msb               = datalb_msb;
	datalb.lsb               = datalb_lsb;

	datain_config(&datain);
	datalb_config(&datalb);

	datalb_ctrl(lb_cfg->datalb_src, bitwidth);
	lb_enable_ex(lb_cfg->lb_mode, true);

	return 0;
}

int loopback_is_enable(void)
{
	return (loopback_enable == 1);
}
