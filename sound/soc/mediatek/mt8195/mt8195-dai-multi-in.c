// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI for MULTI_IN
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 */

#include <linux/regmap.h>
#include <linux/delay.h>
#include <sound/pcm_params.h>
#include "mt8195-afe-clk.h"
#include "mt8195-afe-common.h"
#include "mt8195-reg.h"

#define DAI_TO_MULTI_IN_ID(x) (x - MT8195_AFE_IO_MULTI_IN_START)
#define STR_FROM_ENUM(e) #e

enum {
	MTK_DAI_MULTI_IN_FMT_I2S = 0,
	MTK_DAI_MULTI_IN_FMT_LJ,
	MTK_DAI_MULTI_IN_FMT_RJ,
};

enum {
	DISCONNECT = 0,
	EARC_RX_EXT,
	EARC_RX_INT,
	HDMI_RX_I2S,
	HDMI_RX_DSD,
	SPDIF_IN,
	ETDM_OUT3_I2S,
	ETDM_OUT3_DSD,
	ETDM_IN2_EXT,
};

enum {
	SUPPLY_SEQ_MULTI_IN_CG,
	SUPPLY_SEQ_MULTI_IN_EN,
};

static const char * const multi_in_mux_map[] = {
	STR_FROM_ENUM(DISCONNECT),
	STR_FROM_ENUM(EARC_RX_EXT),
	STR_FROM_ENUM(EARC_RX_INT),
	STR_FROM_ENUM(HDMI_RX_I2S),
	STR_FROM_ENUM(HDMI_RX_DSD),
	STR_FROM_ENUM(SPDIF_IN),
	STR_FROM_ENUM(ETDM_OUT3_I2S),
	STR_FROM_ENUM(ETDM_OUT3_DSD),
	STR_FROM_ENUM(ETDM_IN2_EXT),
};

static int multi_in_mux_map_value[] = {
	DISCONNECT,
	EARC_RX_EXT,
	EARC_RX_INT,
	HDMI_RX_I2S,
	HDMI_RX_DSD,
	SPDIF_IN,
	ETDM_OUT3_I2S,
	ETDM_OUT3_DSD,
	ETDM_IN2_EXT,
};

struct mtk_dai_multi_in_mux {
	const char *name;
	unsigned int name_len;
	unsigned int dai_id;
};

struct mtk_dai_multi_in_priv {
	unsigned int source;
	unsigned int lrck_inv;
	unsigned int bck_inv;
	unsigned int format;
};

static const struct mtk_dai_multi_in_mux multi_in_muxs[] = {
	{
		.name = "MULTI_IN1_MUX",
		.name_len = strlen("MULTI_IN1_MUX"),
		.dai_id = MT8195_AFE_IO_MULTI_IN1,
	},
	{
		.name = "MULTI_IN2_MUX",
		.name_len = strlen("MULTI_IN2_MUX"),
		.dai_id = MT8195_AFE_IO_MULTI_IN2,
	},
};

static int mtk_dai_multi_in_found_dai_id_from_mux_name(const char *name)
{
	int i = 0;
	const struct mtk_dai_multi_in_mux *mux = NULL;

	for (i = 0; i < ARRAY_SIZE(multi_in_muxs); i++) {
		mux = &multi_in_muxs[i];
		if (!strncmp(name, mux->name, mux->name_len))
			return mux->dai_id;
	}

	return -EINVAL;
}

static int mtk_dai_multi_in_mux_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *component = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	int dai_id = -EINVAL;
	struct mtk_dai_multi_in_priv *multi_in_priv = NULL;

	dai_id = mtk_dai_multi_in_found_dai_id_from_mux_name(w->name);
	if (dai_id < 0)
		return -EINVAL;

	multi_in_priv = afe_priv->dai_priv[dai_id];

	ucontrol->value.integer.value[0] = multi_in_priv->source;

	return 0;
}

static int mtk_dai_multi_in_mux_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *component = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	int dai_id = -EINVAL;
	struct mtk_dai_multi_in_priv *multi_in_priv = NULL;

	dai_id = mtk_dai_multi_in_found_dai_id_from_mux_name(w->name);
	if (dai_id < 0)
		return -EINVAL;

	multi_in_priv = afe_priv->dai_priv[dai_id];

	multi_in_priv->source = ucontrol->value.integer.value[0];

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int mtk_multi_in_cg_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		__func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8195_afe_enable_clk(afe, afe_priv->clk[MT8195_CLK_TOP_MPHONE_SLAVE_B]);
		mt8195_afe_enable_clk(afe, afe_priv->clk[MT8195_CLK_AUD_MULTI_IN]);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt8195_afe_disable_clk(afe, afe_priv->clk[MT8195_CLK_AUD_MULTI_IN]);
		mt8195_afe_disable_clk(afe, afe_priv->clk[MT8195_CLK_TOP_MPHONE_SLAVE_B]);
		break;
	default:
		break;
	}

	return 0;
}

static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(multi_in1_mux_map_enum,
					      SND_SOC_NOPM,
					      0,
					      GENMASK(3, 0),
					      multi_in_mux_map,
					      multi_in_mux_map_value);

static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(multi_in2_mux_map_enum,
					      SND_SOC_NOPM,
					      0,
					      GENMASK(3, 0),
					      multi_in_mux_map,
					      multi_in_mux_map_value);

static const struct snd_kcontrol_new multi_in1_mux_control =
	SOC_DAPM_ENUM_EXT("MULTI_IN1_MUX",
			  multi_in1_mux_map_enum,
			  mtk_dai_multi_in_mux_get,
			  mtk_dai_multi_in_mux_put);

static const struct snd_kcontrol_new multi_in2_mux_control =
	SOC_DAPM_ENUM_EXT("MULTI_IN2_MUX",
			  multi_in2_mux_map_enum,
			  mtk_dai_multi_in_mux_get,
			  mtk_dai_multi_in_mux_put);

static const struct snd_soc_dapm_widget mtk_dai_multi_in_widgets[] = {
	SND_SOC_DAPM_MUX("MULTI_IN1_MUX", SND_SOC_NOPM, 0, 0,
			 &multi_in1_mux_control),
	SND_SOC_DAPM_MUX("MULTI_IN2_MUX", SND_SOC_NOPM, 0, 0,
			&multi_in2_mux_control),
	/* cg */
	SND_SOC_DAPM_SUPPLY_S("MULTI_IN_CG", SUPPLY_SEQ_MULTI_IN_CG,
			      SND_SOC_NOPM, 0, 0,
			      mtk_multi_in_cg_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* en */
	SND_SOC_DAPM_SUPPLY_S("MULTI_IN1_EN", SUPPLY_SEQ_MULTI_IN_EN,
			      AFE_MPHONE_MULTI_CON0, AFE_MPHONE_MULTI_CON0_EN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("MULTI_IN2_EN", SUPPLY_SEQ_MULTI_IN_EN,
			      AFE_MPHONE_MULTI2_CON0, AFE_MPHONE_MULTI_CON0_EN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_INPUT("MULTI_INPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_multi_in_routes[] = {
	{"MULTI_IN1_MUX", "EARC_RX_EXT", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "EARC_RX_INT", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "HDMI_RX_I2S", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "HDMI_RX_DSD", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "SPDIF_IN", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "ETDM_OUT3_I2S", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "ETDM_OUT3_DSD", "MULTI1 Capture"},
	{"MULTI_IN1_MUX", "ETDM_IN2_EXT", "MULTI1 Capture"},
	{"UL1", NULL, "MULTI_IN1_MUX"},

	{"MULTI_IN2_MUX", "EARC_RX_EXT", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "EARC_RX_INT", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "HDMI_RX_I2S", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "HDMI_RX_DSD", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "SPDIF_IN", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "ETDM_OUT3_I2S", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "ETDM_OUT3_DSD", "MULTI2 Capture"},
	{"MULTI_IN2_MUX", "ETDM_IN2_EXT", "MULTI2 Capture"},
	{"UL6", NULL, "MULTI_IN2_MUX"},

	{"MULTI1 Capture", NULL, "MULTI_INPUT"},
	{"MULTI2 Capture", NULL, "MULTI_INPUT"},
	/* cg */
	{"MULTI1 Capture", NULL, "MULTI_IN_CG"},
	{"MULTI2 Capture", NULL, "MULTI_IN_CG"},
	/* en */
	{"MULTI1 Capture", NULL, "MULTI_IN1_EN"},
	{"MULTI2 Capture", NULL, "MULTI_IN2_EN"},
};

struct multi_in_con_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
};

static const struct multi_in_con_reg
	multi_in_con_regs[MT8195_AFE_IO_MULTI_IN_NUM] = {
	{
		.con0 = AFE_MPHONE_MULTI_CON0,
		.con1 = AFE_MPHONE_MULTI_CON1,
		.con2 = AFE_MPHONE_MULTI_CON2,
	},
	{
		.con0 = AFE_MPHONE_MULTI2_CON0,
		.con1 = AFE_MPHONE_MULTI2_CON1,
		.con2 = AFE_MPHONE_MULTI2_CON2,
	},
};

/* dai ops */
static int mtk_dai_multi_in_fmt_config(struct snd_soc_dai *dai,
				       unsigned int bit_width,
				       unsigned int channels)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	unsigned int id = DAI_TO_MULTI_IN_ID(dai->id);
	struct mtk_dai_multi_in_priv *multi_in_priv = NULL;
	const struct multi_in_con_reg *reg = &multi_in_con_regs[id];
	unsigned int source = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	if (dai->id < 0)
		return -EINVAL;

	multi_in_priv = afe_priv->dai_priv[dai->id];
	source = multi_in_priv->source;

	if (source == SPDIF_IN) {
		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;
		} else {
			val |= AFE_MPHONE_MULTI_CON0_16BIT_DATA |
				AFE_MPHONE_MULTI_CON0_16BIT_SWAP;
		}
	} else {
		if (bit_width == 24)
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;
		else
			val |= AFE_MPHONE_MULTI_CON0_16BIT_DATA;
	}
	mask |= AFE_MPHONE_MULIT_CON0_24BIT_DATA_MASK;
	mask |= AFE_MPHONE_MULTI_CON0_16BIT_SWAP_MASK;

	regmap_update_bits(afe->regmap, reg->con0, mask, val);

	mask = 0;
	val = 0;
	val |= AFE_MPHONE_MULTI_CON1_CH_NUM(channels) |
			AFE_MPHONE_MULTI_CON1_BIT_NUM(bit_width) |
			AFE_MPHONE_MULTI_CON1_SYNC_ON;

	mask |= AFE_MPHONE_MULTI_CON1_CH_NUM_MASK;
	mask |= AFE_MPHONE_MULTI_CON1_BIT_NUM_MASK;
	mask |= AFE_MPHONE_MULTI_CON1_SYNC_ON_MASK;

	if (source == SPDIF_IN) {
		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
				AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
				AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON1_COMPACT_MODE |
				AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
				AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
				AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		}
	} else {
		val |= AFE_MPHONE_MULTI_CON1_HBR_MODE;
		mask |= AFE_MPHONE_MULTI_CON1_HBR_MODE_MASK;

		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
					AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON1_COMPACT_MODE |
				AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
				AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
				AFE_MPHONE_MULTI_CON1_LRCK_16_CYCLE;
		}

		if (multi_in_priv->format == MTK_DAI_MULTI_IN_FMT_I2S) {
			val |= AFE_MPHONE_MULTI_CON1_DELAY_DATA |
				AFE_MPHONE_MULTI_CON1_LEFT_ALIGN;
		} else if (multi_in_priv->format == MTK_DAI_MULTI_IN_FMT_LJ) {
			val |= AFE_MPHONE_MULTI_CON1_LEFT_ALIGN;
		}

		mask |= AFE_MPHONE_MULTI_CON1_LEFT_ALIGN_MASK;
		mask |= AFE_MPHONE_MULTI_CON1_DELAY_DATA_MASK;

		if (multi_in_priv->bck_inv)
			val |= AFE_MPHONE_MULTI_CON1_BCK_INV;
		mask |= AFE_MPHONE_MULTI_CON1_BCK_INV_MASK;

		if (multi_in_priv->lrck_inv)
			val |= AFE_MPHONE_MULTI_CON1_LRCK_INV;
		mask |= AFE_MPHONE_MULTI_CON1_LRCK_INV_MASK;
	}

	mask |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE_MASK;
	mask |= AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS_MASK;
	mask |= AFE_MPHONE_MULTI_CON1_LRCK_CYCLE_SEL_MASK;

	regmap_update_bits(afe->regmap, reg->con1, mask, val);

	return 0;
}

enum {
	MULTI_IN_MUX_START = 0,
	MULTI_IN_SPLIN_BCK_SEL = MULTI_IN_MUX_START,
	MULTI_IN_SPLIN_LRCK_SEL,
	MULTI_IN_HDMI_RX_INT_DSD_SEL,
	MULTI_IN_SPDIFIN_SEL,
	MULTI_IN_SDATA0_SEL, /* 1st level mux */
	MULTI_IN_SDATA1_SEL, /* 1st level mux */
	MULTI_IN_SDATA2_SEL, /* 1st level mux */
	MULTI_IN_SDATA3_SEL, /* 1st level mux */
	MULTI_IN_SDATA5_SEL, /* 1st level mux (don't have sdata4_sel) */
	MULTI_IN_SDATA0_MUX, /* 2nd level mux */
	MULTI_IN_SDATA1_MUX, /* 2nd level mux */
	MULTI_IN_SDATA2_MUX, /* 2nd level mux */
	MULTI_IN_SDATA3_MUX, /* 2nd level mux */
	MULTI_IN_SDATA4_MUX, /* 2nd level mux */
	MULTI_IN_SDATA5_MUX, /* 2nd level mux */
	MULTI_IN_MUX_NUM,
};

/* MULTI_IN_SPLIN_BCK_SEL */
enum {
	MULTI_IN_SPLIN_BCK_SEL_SPLIN_SLAVE = 0,
	MULTI_IN_SPLIN_BCK_SEL_ETDM_IN2_SLAVE,
	MULTI_IN_SPLIN_BCK_SEL_EARC_INT = 3,
	MULTI_IN_SPLIN_BCK_SEL_ETDM_IN2_MASTER,
	MULTI_IN_SPLIN_BCK_SEL_ETDM_OUT3_MASTER,
	MULTI_IN_SPLIN_BCK_SEL_HDMI_RX_INT,
};

/* MULTI_IN_SPLIN_LRCK_SEL */
enum {
	MULTI_IN_SPLIN_LRCK_SEL_SPLIN_SLAVE = 0,
	MULTI_IN_SPLIN_LRCK_SEL_ETDM_IN2_SLAVE,
	MULTI_IN_SPLIN_LRCK_SEL_EARC_INT = 3,
	MULTI_IN_SPLIN_LRCK_SEL_ETDM_IN2_MASTER,
	MULTI_IN_SPLIN_LRCK_SEL_ETDM_OUT3_MASTER,
	MULTI_IN_SPLIN_LRCK_SEL_HDMI_RX_INT,
	MULTI_IN_SPLIN_LRCK_SEL_ETDM_OUT3_SDATA4,
};

/* MULTI_IN_HDMI_RX_INT_DSD_SEL */
enum {
	MULTI_IN_HDMI_RX_INT_DSD_SEL_I2S = 0,
	MULTI_IN_HDMI_RX_INT_DSD_SEL_DSD,
};

/* MULTI_IN_SPDIFIN_SEL */
enum {
	MULTI_IN_SPDIFIN_SEL_SPLIN = 0,
	MULTI_IN_SPDIFIN_SEL_SPDFIN,
};

/* MULTI_IN_SDATA0_SEL */
enum {
	MULTI_IN_SDATA0_SEL_SPLIN = 0,
	MULTI_IN_SDATA0_SEL_ETDM_OUT3_SDATA0,
	MULTI_IN_SDATA0_SEL_ETDM_IN2_SDATA0_EXT,
	MULTI_IN_SDATA0_SEL_EARC_SDATA0_INT,
	MULTI_IN_SDATA0_SEL_HDMI_RX_SDATA0_INT,
};

/* MULTI_IN_SDATA1_SEL */
enum {
	MULTI_IN_SDATA1_SEL_SPLIN = 0,
	MULTI_IN_SDATA1_SEL_ETDM_OUT3_SDATA1,
	MULTI_IN_SDATA1_SEL_ETDM_IN2_SDATA1_EXT,
	MULTI_IN_SDATA1_SEL_EARC_SDATA1_INT,
	MULTI_IN_SDATA1_SEL_HDMI_RX_SDATA1_INT,
};

/* MULTI_IN_SDATA2_SEL */
enum {
	MULTI_IN_SDATA2_SEL_SPLIN = 0,
	MULTI_IN_SDATA2_SEL_ETDM_OUT3_SDATA2,
	MULTI_IN_SDATA2_SEL_ETDM_IN2_SDATA2_EXT,
	MULTI_IN_SDATA2_SEL_EARC_SDATA2_INT,
	MULTI_IN_SDATA2_SEL_HDMI_RX_SDATA2_INT,
};

/* MULTI_IN_SDATA3_SEL */
enum {
	MULTI_IN_SDATA3_SEL_SPLIN = 0,
	MULTI_IN_SDATA3_SEL_ETDM_OUT3_SDATA3,
	MULTI_IN_SDATA3_SEL_ETDM_IN2_SDATA3_EXT,
	MULTI_IN_SDATA3_SEL_EARC_SDATA3_INT,
	MULTI_IN_SDATA3_SEL_HDMI_RX_SDATA3_INT,
};

/* MULTI_IN_SDATA5_SEL */
enum {
	MULTI_IN_SDATA5_SEL_ETDM_OUT3_SDATA5 = 1,
	MULTI_IN_SDATA5_SEL_HDMI_RX_SDATA5_INT,
};

/* MULTI_IN_SDATA0_MUX */
enum {
	MULTI_IN_SDATA0_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA0_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA0_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA0_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA0_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA0_MUX_SPLIN_SDATA5,
};

/* MULTI_IN_SDATA1_MUX */
enum {
	MULTI_IN_SDATA1_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA1_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA1_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA1_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA1_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA1_MUX_SPLIN_SDATA5,
};

/* MULTI_IN_SDATA2_MUX */
enum {
	MULTI_IN_SDATA2_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA2_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA2_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA2_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA2_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA2_MUX_SPLIN_SDATA5,
};

/* MULTI_IN_SDATA3_MUX */
enum {
	MULTI_IN_SDATA3_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA3_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA3_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA3_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA3_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA3_MUX_SPLIN_SDATA5,
};

/* MULTI_IN_SDATA4_MUX */
enum {
	MULTI_IN_SDATA4_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA4_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA4_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA4_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA4_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA4_MUX_SPLIN_SDATA5,
};

/* MULTI_IN_SDATA5_MUX */
enum {
	MULTI_IN_SDATA5_MUX_SPLIN_SDATA0 = 0,
	MULTI_IN_SDATA5_MUX_SPLIN_SDATA1,
	MULTI_IN_SDATA5_MUX_SPLIN_SDATA2,
	MULTI_IN_SDATA5_MUX_SPLIN_SDATA3,
	MULTI_IN_SDATA5_MUX_SPLIN_LRCK,
	MULTI_IN_SDATA5_MUX_SPLIN_SDATA5,
};

struct mtk_dai_multi_in_mux_reg {
	unsigned int addr;
	unsigned int shift;
	unsigned int mask;
};

static const struct mtk_dai_multi_in_mux_reg
	multi_in_mux_regs[MT8195_AFE_IO_MULTI_IN_NUM][MULTI_IN_MUX_NUM] = {
	/* MULTI_IN1 */
	{
		[MULTI_IN_SPLIN_BCK_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 0,
			.mask = GENMASK(2, 0),
		},
		[MULTI_IN_SPLIN_LRCK_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 3,
			.mask = GENMASK(5, 3),
		},
		[MULTI_IN_HDMI_RX_INT_DSD_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 20,
			.mask = BIT(20),
		},
		[MULTI_IN_SPDIFIN_SEL] = {
			.addr = AFE_MPHONE_MULTI_CON2,
			.shift = 19,
			.mask = BIT(19),
		},
		[MULTI_IN_SDATA0_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 6,
			.mask = GENMASK(8, 6),
		},
		[MULTI_IN_SDATA1_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 9,
			.mask = GENMASK(11, 9),
		},
		[MULTI_IN_SDATA2_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 12,
			.mask = GENMASK(14, 12),
		},
		[MULTI_IN_SDATA3_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 15,
			.mask = GENMASK(17, 15),
		},
		[MULTI_IN_SDATA5_SEL] = {
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 18,
			.mask = GENMASK(19, 18),
		},
		[MULTI_IN_SDATA0_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 14,
			.mask = GENMASK(16, 14),
		},
		[MULTI_IN_SDATA1_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 17,
			.mask = GENMASK(19, 17),
		},
		[MULTI_IN_SDATA2_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 20,
			.mask = GENMASK(22, 20),
		},
		[MULTI_IN_SDATA3_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 23,
			.mask = GENMASK(25, 23),
		},
		[MULTI_IN_SDATA4_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 26,
			.mask = GENMASK(28, 26),
		},
		[MULTI_IN_SDATA5_MUX] = {
			.addr = AFE_MPHONE_MULTI_CON0,
			.shift = 29,
			.mask = GENMASK(31, 29),
		},
	},
	/* MULTI_IN2 */
	{
		[MULTI_IN_SPLIN_BCK_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 0,
			.mask = GENMASK(2, 0),
		},
		[MULTI_IN_SPLIN_LRCK_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 3,
			.mask = GENMASK(5, 3),
		},
		[MULTI_IN_HDMI_RX_INT_DSD_SEL] = {
			/*
			 * mphone_multi_1 and 2 share the same mux here,
			 * due to HDMI RX can't output two fmt at the same time.
			 */
			.addr = AFE_LOOPBACK_CFG0,
			.shift = 20,
			.mask = BIT(20),
		},
		[MULTI_IN_SPDIFIN_SEL] = {
			.addr = AFE_MPHONE_MULTI2_CON2,
			.shift = 19,
			.mask = BIT(19),
		},
		[MULTI_IN_SDATA0_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 6,
			.mask = GENMASK(8, 6),
		},
		[MULTI_IN_SDATA1_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 9,
			.mask = GENMASK(11, 9),
		},
		[MULTI_IN_SDATA2_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 12,
			.mask = GENMASK(14, 12),
		},
		[MULTI_IN_SDATA3_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 15,
			.mask = GENMASK(17, 15),
		},
		[MULTI_IN_SDATA5_SEL] = {
			.addr = AFE_LOOPBACK_CFG2,
			.shift = 18,
			.mask = GENMASK(19, 18),
		},
		[MULTI_IN_SDATA0_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 14,
			.mask = GENMASK(16, 14),
		},
		[MULTI_IN_SDATA1_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 17,
			.mask = GENMASK(19, 17),
		},
		[MULTI_IN_SDATA2_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 20,
			.mask = GENMASK(22, 20),
		},
		[MULTI_IN_SDATA3_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 23,
			.mask = GENMASK(25, 23),
		},
		[MULTI_IN_SDATA4_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 26,
			.mask = GENMASK(28, 26),
		},
		[MULTI_IN_SDATA5_MUX] = {
			.addr = AFE_MPHONE_MULTI2_CON0,
			.shift = 29,
			.mask = GENMASK(31, 29),
		},
	},
};

static int mtk_dai_multi_in_mux_config(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	unsigned int id = DAI_TO_MULTI_IN_ID(dai->id);
	struct mtk_dai_multi_in_priv *multi_in_priv = NULL;
	const struct mtk_dai_multi_in_mux_reg *reg = NULL;
	unsigned int source = 0;
	unsigned int splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_SPLIN_SLAVE;
	unsigned int splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_SPLIN_SLAVE;
	unsigned int hdmi_rx_int_dsd_sel = MULTI_IN_HDMI_RX_INT_DSD_SEL_I2S;
	unsigned int spdifin_sel = MULTI_IN_SPDIFIN_SEL_SPLIN;
	unsigned int sdata0_sel = MULTI_IN_SDATA0_SEL_SPLIN;
	unsigned int sdata1_sel = MULTI_IN_SDATA1_SEL_SPLIN;
	unsigned int sdata2_sel = MULTI_IN_SDATA2_SEL_SPLIN;
	unsigned int sdata3_sel = MULTI_IN_SDATA3_SEL_SPLIN;
	unsigned int sdata5_sel = 0;
	unsigned int sdata0_mux = MULTI_IN_SDATA0_MUX_SPLIN_SDATA0;
	unsigned int sdata1_mux = MULTI_IN_SDATA1_MUX_SPLIN_SDATA1;
	unsigned int sdata2_mux = MULTI_IN_SDATA2_MUX_SPLIN_SDATA2;
	unsigned int sdata3_mux = MULTI_IN_SDATA3_MUX_SPLIN_SDATA3;
	unsigned int sdata4_mux = MULTI_IN_SDATA4_MUX_SPLIN_LRCK;
	unsigned int sdata5_mux = MULTI_IN_SDATA5_MUX_SPLIN_SDATA5;

	if (dai->id < 0)
		return -EINVAL;

	multi_in_priv = afe_priv->dai_priv[dai->id];
	source = multi_in_priv->source;

	switch (source) {
	case EARC_RX_EXT:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_SPLIN_SLAVE;
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_SPLIN_SLAVE;
		sdata0_sel = MULTI_IN_SDATA0_SEL_SPLIN;
		sdata1_sel = MULTI_IN_SDATA1_SEL_SPLIN;
		sdata2_sel = MULTI_IN_SDATA2_SEL_SPLIN;
		sdata3_sel = MULTI_IN_SDATA3_SEL_SPLIN;
		break;
	case EARC_RX_INT:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_EARC_INT;
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_EARC_INT;
		sdata0_sel = MULTI_IN_SDATA0_SEL_EARC_SDATA0_INT;
		sdata1_sel = MULTI_IN_SDATA1_SEL_EARC_SDATA1_INT;
		sdata2_sel = MULTI_IN_SDATA2_SEL_EARC_SDATA2_INT;
		sdata3_sel = MULTI_IN_SDATA3_SEL_EARC_SDATA3_INT;
		break;
	case HDMI_RX_I2S:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_HDMI_RX_INT;
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_HDMI_RX_INT;
		hdmi_rx_int_dsd_sel = MULTI_IN_HDMI_RX_INT_DSD_SEL_I2S;
		sdata0_sel = MULTI_IN_SDATA0_SEL_HDMI_RX_SDATA0_INT;
		sdata1_sel = MULTI_IN_SDATA1_SEL_HDMI_RX_SDATA1_INT;
		sdata2_sel = MULTI_IN_SDATA2_SEL_HDMI_RX_SDATA2_INT;
		sdata3_sel = MULTI_IN_SDATA3_SEL_HDMI_RX_SDATA3_INT;
		break;
	case HDMI_RX_DSD:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_HDMI_RX_INT;
		/* dsd mode, use lrck for sdata4 */
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_HDMI_RX_INT;
		hdmi_rx_int_dsd_sel = MULTI_IN_HDMI_RX_INT_DSD_SEL_DSD;
		sdata0_sel = MULTI_IN_SDATA0_SEL_HDMI_RX_SDATA0_INT;
		sdata1_sel = MULTI_IN_SDATA1_SEL_HDMI_RX_SDATA1_INT;
		sdata2_sel = MULTI_IN_SDATA2_SEL_HDMI_RX_SDATA2_INT;
		sdata3_sel = MULTI_IN_SDATA3_SEL_HDMI_RX_SDATA3_INT;
		sdata5_sel = MULTI_IN_SDATA5_SEL_HDMI_RX_SDATA5_INT;
		sdata0_mux = MULTI_IN_SDATA0_MUX_SPLIN_SDATA0;
		sdata1_mux = MULTI_IN_SDATA1_MUX_SPLIN_LRCK;
		sdata2_mux = MULTI_IN_SDATA2_MUX_SPLIN_SDATA1;
		sdata3_mux = MULTI_IN_SDATA3_MUX_SPLIN_SDATA5;
		sdata4_mux = MULTI_IN_SDATA4_MUX_SPLIN_SDATA3;
		sdata5_mux = MULTI_IN_SDATA5_MUX_SPLIN_SDATA2;
		break;
	case SPDIF_IN:
		spdifin_sel = MULTI_IN_SPDIFIN_SEL_SPDFIN;
		break;
	case ETDM_OUT3_I2S:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_ETDM_OUT3_MASTER;
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_ETDM_OUT3_MASTER;
		sdata0_sel = MULTI_IN_SDATA0_SEL_ETDM_OUT3_SDATA0;
		sdata1_sel = MULTI_IN_SDATA1_SEL_ETDM_OUT3_SDATA1;
		sdata2_sel = MULTI_IN_SDATA2_SEL_ETDM_OUT3_SDATA2;
		sdata3_sel = MULTI_IN_SDATA3_SEL_ETDM_OUT3_SDATA3;
		break;
	case ETDM_OUT3_DSD:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_ETDM_OUT3_MASTER;
		/* dsd mode, use lrck for sdata4 */
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_ETDM_OUT3_SDATA4;
		sdata0_sel = MULTI_IN_SDATA0_SEL_ETDM_OUT3_SDATA0;
		sdata1_sel = MULTI_IN_SDATA1_SEL_ETDM_OUT3_SDATA1;
		sdata2_sel = MULTI_IN_SDATA2_SEL_ETDM_OUT3_SDATA2;
		sdata3_sel = MULTI_IN_SDATA3_SEL_ETDM_OUT3_SDATA3;
		sdata5_sel = MULTI_IN_SDATA5_SEL_ETDM_OUT3_SDATA5;
		sdata0_mux = MULTI_IN_SDATA0_MUX_SPLIN_SDATA0;
		sdata1_mux = MULTI_IN_SDATA1_MUX_SPLIN_SDATA1;
		sdata2_mux = MULTI_IN_SDATA2_MUX_SPLIN_SDATA2;
		sdata3_mux = MULTI_IN_SDATA3_MUX_SPLIN_SDATA3;
		sdata4_mux = MULTI_IN_SDATA4_MUX_SPLIN_LRCK;
		sdata5_mux = MULTI_IN_SDATA5_MUX_SPLIN_SDATA5;
		break;
	case ETDM_IN2_EXT:
		splin_bck_sel = MULTI_IN_SPLIN_BCK_SEL_ETDM_IN2_SLAVE;
		splin_lrck_sel = MULTI_IN_SPLIN_LRCK_SEL_ETDM_IN2_SLAVE;
		sdata0_sel = MULTI_IN_SDATA0_SEL_ETDM_IN2_SDATA0_EXT;
		sdata1_sel = MULTI_IN_SDATA1_SEL_ETDM_IN2_SDATA1_EXT;
		sdata2_sel = MULTI_IN_SDATA2_SEL_ETDM_IN2_SDATA2_EXT;
		sdata3_sel = MULTI_IN_SDATA3_SEL_ETDM_IN2_SDATA3_EXT;
		break;
	default:
		dev_dbg(dai->dev, "%s no valid source\n", __func__);
		return -EINVAL;
	}

	/* MULTI_IN_SPLIN_BCK_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SPLIN_BCK_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   splin_bck_sel << reg->shift);

	/* MULTI_IN_SPLIN_LRCK_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SPLIN_LRCK_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   splin_lrck_sel << reg->shift);

	/* MULTI_IN_HDMI_RX_INT_DSD_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_HDMI_RX_INT_DSD_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   hdmi_rx_int_dsd_sel << reg->shift);

	/* MULTI_IN_SPDIFIN_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SPDIFIN_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   spdifin_sel << reg->shift);

	/* MULTI_IN_SDATA0_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA0_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata0_sel << reg->shift);

	/* MULTI_IN_SDATA1_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA1_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata1_sel << reg->shift);

	/* MULTI_IN_SDATA2_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA2_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata2_sel << reg->shift);

	/* MULTI_IN_SDATA3_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA3_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata3_sel << reg->shift);

	/* MULTI_IN_SDATA5_SEL */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA5_SEL];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata5_sel << reg->shift);

	/* MULTI_IN_SDATA0_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA0_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata0_mux << reg->shift);

	/* MULTI_IN_SDATA1_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA1_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata1_mux << reg->shift);

	/* MULTI_IN_SDATA2_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA2_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata2_mux << reg->shift);

	/* MULTI_IN_SDATA3_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA3_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata3_mux << reg->shift);

	/* MULTI_IN_SDATA4_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA4_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata4_mux << reg->shift);

	/* MULTI_IN_SDATA5_MUX */
	reg = &multi_in_mux_regs[id][MULTI_IN_SDATA5_MUX];
	regmap_update_bits(afe->regmap, reg->addr, reg->mask,
			   sdata5_mux << reg->shift);


	return 0;
}

static int mtk_dai_multi_in_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	int ret;
	unsigned int bit_width = params_width(params);
	unsigned int channels = params_channels(params);

	ret = mtk_dai_multi_in_fmt_config(dai, bit_width, channels);
	if (ret)
		return ret;

	ret = mtk_dai_multi_in_mux_config(dai);
	if (ret)
		return ret;

	return ret;
}

static int mtk_dai_multi_in_set_fmt(struct snd_soc_dai *dai,
				    unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_multi_in_priv *multi_in_priv = NULL;

	if (dai->id < 0)
		return -EINVAL;

	multi_in_priv = afe_priv->dai_priv[dai->id];

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		multi_in_priv->format = MTK_DAI_MULTI_IN_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		multi_in_priv->format = MTK_DAI_MULTI_IN_FMT_LJ;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		multi_in_priv->format = MTK_DAI_MULTI_IN_FMT_RJ;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		multi_in_priv->bck_inv = 0;
		multi_in_priv->lrck_inv = 0;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		multi_in_priv->bck_inv = 0;
		multi_in_priv->lrck_inv = 1;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		multi_in_priv->bck_inv = 1;
		multi_in_priv->lrck_inv = 0;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		multi_in_priv->bck_inv = 1;
		multi_in_priv->lrck_inv = 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_multi_in_ops = {
	.hw_params	= mtk_dai_multi_in_hw_params,
	.set_fmt	= mtk_dai_multi_in_set_fmt,
};

/* dai driver */
#define MTK_MULTI_IN_RATES (SNDRV_PCM_RATE_8000_48000 |\
			    SNDRV_PCM_RATE_88200 |\
			    SNDRV_PCM_RATE_96000 |\
			    SNDRV_PCM_RATE_176400 |\
			    SNDRV_PCM_RATE_192000)

#define MTK_MULTI_IN_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			      SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_multi_in_driver[] = {
	{
		.name = "MULTI_IN1",
		.id = MT8195_AFE_IO_MULTI_IN1,
		.capture = {
			.stream_name = "MULTI1 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_MULTI_IN_RATES,
			.formats = MTK_MULTI_IN_FORMATS,
		},
		.ops = &mtk_dai_multi_in_ops,
	},
	{
		.name = "MULTI_IN2",
		.id = MT8195_AFE_IO_MULTI_IN2,
		.capture = {
			.stream_name = "MULTI2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_MULTI_IN_RATES,
			.formats = MTK_MULTI_IN_FORMATS,
		},
		.ops = &mtk_dai_multi_in_ops,
	},
};

static int init_multi_in_priv_data(struct mtk_base_afe *afe)
{
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_multi_in_priv *multi_in_priv;
	int i = MT8195_AFE_IO_MULTI_IN_START;

	for (; i < MT8195_AFE_IO_MULTI_IN_END; i++) {
		multi_in_priv = devm_kzalloc(afe->dev,
					     sizeof(struct mtk_dai_multi_in_priv),
					     GFP_KERNEL);
		if (!multi_in_priv)
			return -ENOMEM;

		afe_priv->dai_priv[i] = multi_in_priv;
	}

	return 0;
}

int mt8195_dai_multi_in_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_multi_in_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_multi_in_driver);
	dai->dapm_widgets = mtk_dai_multi_in_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_multi_in_widgets);
	dai->dapm_routes = mtk_dai_multi_in_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_multi_in_routes);

	return init_multi_in_priv_data(afe);
}
