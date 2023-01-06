// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI DMIC I/F Control
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt8195-afe-clk.h"
#include "mt8195-afe-common.h"
#include "mt8195-reg.h"

#define DMIC_MAX_CH (8)

enum {
	DMIC0,
	DMIC1,
	DMIC2,
	DMIC3,
	DMIC_NUM,
};

struct mtk_dai_dmic_ctrl_reg {
	unsigned int con0;
};

struct mtk_dai_dmic_priv {
	unsigned int clk_phase_sel_ch[2];
	unsigned int dmic_src_sel[DMIC_MAX_CH];
	unsigned int iir_on;
	unsigned int two_wire_mode;
	unsigned int channels;
	unsigned int dmic_clk_mono;
	unsigned int clk_index[DMIC_NUM];
};

static const struct mtk_dai_dmic_ctrl_reg dmic_ctrl_regs[DMIC_NUM] = {
	[DMIC0] = {
		.con0 = AFE_DMIC0_UL_SRC_CON0,
	},
	[DMIC1] = {
		.con0 = AFE_DMIC1_UL_SRC_CON0,
	},
	[DMIC2] = {
		.con0 = AFE_DMIC2_UL_SRC_CON0,
	},
	[DMIC3] = {
		.con0 = AFE_DMIC3_UL_SRC_CON0,
	},
};

static const struct mtk_dai_dmic_ctrl_reg *get_dmic_ctrl_reg(int id)
{
	if (id < 0 || id >= DMIC_NUM)
		return NULL;

	return &dmic_ctrl_regs[id];
}

static const struct reg_sequence mtk_dai_dmic_iir_coeff_reg_defaults[] = {
	{ AFE_DMIC0_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC0_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC0_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC1_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC1_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC1_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC2_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC2_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC2_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC3_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC3_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC3_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_10_09, 0x0000C048 },
};

static int mtk_dai_dmic_load_iir_coeff_table(struct mtk_base_afe *afe)
{
	return regmap_multi_reg_write(afe->regmap,
			mtk_dai_dmic_iir_coeff_reg_defaults,
			ARRAY_SIZE(mtk_dai_dmic_iir_coeff_reg_defaults));
}

static int mtk_dai_dmic_configure_array(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	unsigned int *dmic_src_sel = NULL;
	unsigned int mask =
			PWR2_TOP_CON_DMIC8_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC7_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC6_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC5_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC4_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC3_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC2_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC1_SRC_SEL_MASK;
	unsigned int val = 0;

	if (dai->id < 0)
		return -EINVAL;

	dmic_priv = afe_priv->dai_priv[dai->id];
	dmic_src_sel = dmic_priv->dmic_src_sel;
	val = PWR2_TOP_CON_DMIC8_SRC_SEL_VAL(dmic_src_sel[7]) |
	      PWR2_TOP_CON_DMIC7_SRC_SEL_VAL(dmic_src_sel[6]) |
	      PWR2_TOP_CON_DMIC6_SRC_SEL_VAL(dmic_src_sel[5]) |
	      PWR2_TOP_CON_DMIC5_SRC_SEL_VAL(dmic_src_sel[4]) |
	      PWR2_TOP_CON_DMIC4_SRC_SEL_VAL(dmic_src_sel[3]) |
	      PWR2_TOP_CON_DMIC3_SRC_SEL_VAL(dmic_src_sel[2]) |
	      PWR2_TOP_CON_DMIC2_SRC_SEL_VAL(dmic_src_sel[1]) |
	      PWR2_TOP_CON_DMIC1_SRC_SEL_VAL(dmic_src_sel[0]);

	regmap_update_bits(afe->regmap, PWR2_TOP_CON0, mask, val);

	return 0;
}

static int mtk_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	const struct mtk_dai_dmic_ctrl_reg *reg = NULL;
	unsigned int channels;
	unsigned int ch_base;
	unsigned int msk;

	dev_dbg(afe->dev, "%s(), name %s, event 0x%x\n",
		__func__, w->name, event);

	dmic_priv = afe_priv->dai_priv[MT8195_AFE_IO_DMIC_IN];
	channels = dmic_priv->channels;
	ch_base = dmic_priv->dmic_clk_mono ? 1 : 2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* request fifo soft rst */
		msk = 0;
		if (channels > ch_base * 3)
			msk |= PWR2_TOP_CON1_DMIC4_FIFO_SOFT_RST_EN;
		if (channels > ch_base * 2)
			msk |= PWR2_TOP_CON1_DMIC3_FIFO_SOFT_RST_EN;
		if (channels > ch_base)
			msk |= PWR2_TOP_CON1_DMIC2_FIFO_SOFT_RST_EN;
		if (channels > 0)
			msk |= PWR2_TOP_CON1_DMIC1_FIFO_SOFT_RST_EN;

		regmap_set_bits(afe->regmap, PWR2_TOP_CON1, msk);

		msk = 0;
		msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH1_CTL;
		msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH2_CTL;
		msk |= AFE_DMIC_UL_SRC_CON0_UL_SDM_3_LEVEL_CTL;
		if (dmic_priv->iir_on)
			msk |= AFE_DMIC_UL_SRC_CON0_UL_IIR_ON_TMP_CTL;

		if (channels > ch_base * 3) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base * 2) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > 0) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		msk = AFE_DMIC_UL_SRC_CON0_UL_SRC_ON_TMP_CTL;
		if (channels > ch_base * 3) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base * 2) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > 0) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
			if (reg)
				regmap_set_bits(afe->regmap, reg->con0, msk);
		}

		mt8195_afe_enable_clk(afe, afe_priv->clk[MT8195_CLK_AUD_AFE_DMIC]);

		/* release fifo soft rst */
		msk = 0;
		if (channels > ch_base * 3)
			msk |= PWR2_TOP_CON1_DMIC4_FIFO_SOFT_RST_EN;
		if (channels > ch_base * 2)
			msk |= PWR2_TOP_CON1_DMIC3_FIFO_SOFT_RST_EN;
		if (channels > ch_base)
			msk |= PWR2_TOP_CON1_DMIC2_FIFO_SOFT_RST_EN;
		if (channels > 0)
			msk |= PWR2_TOP_CON1_DMIC1_FIFO_SOFT_RST_EN;

		regmap_clear_bits(afe->regmap, PWR2_TOP_CON1, msk);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		msk =  AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH1_CTL |
			AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH2_CTL |
			AFE_DMIC_UL_SRC_CON0_UL_SRC_ON_TMP_CTL |
			AFE_DMIC_UL_SRC_CON0_UL_IIR_ON_TMP_CTL |
			AFE_DMIC_UL_SRC_CON0_UL_SDM_3_LEVEL_CTL;

		if (channels > ch_base * 3) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
			if (reg)
				regmap_clear_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base * 2) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
			if (reg)
				regmap_clear_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > ch_base * 1) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
			if (reg)
				regmap_clear_bits(afe->regmap, reg->con0, msk);
		}
		if (channels > 0) {
			reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
			if (reg)
				regmap_clear_bits(afe->regmap, reg->con0, msk);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		usleep_range(125, 126);

		mt8195_afe_disable_clk(afe, afe_priv->clk[MT8195_CLK_AUD_AFE_DMIC]);
		break;
	default:
		break;
	}

	return 0;
}

/* dai ops */
static int mtk_dai_dmic_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	unsigned int ch_base;
	unsigned int two_wire_mode = 0;
	unsigned int clk_phase_sel_ch1 = 0;
	unsigned int clk_phase_sel_ch2 = 0;
	unsigned int iir_on = 0;
	const struct mtk_dai_dmic_ctrl_reg *reg = NULL;
	unsigned int val = 0;
	unsigned int msk = 0;

	if (dai->id < 0)
		return -EINVAL;

	dmic_priv = afe_priv->dai_priv[dai->id];
	two_wire_mode = dmic_priv->two_wire_mode;
	clk_phase_sel_ch1 = dmic_priv->clk_phase_sel_ch[0];
	clk_phase_sel_ch2 = dmic_priv->clk_phase_sel_ch[1];
	iir_on = dmic_priv->iir_on;
	ch_base = dmic_priv->dmic_clk_mono ? 1 : 2;

	if (two_wire_mode)
		val |= AFE_DMIC_UL_SRC_CON0_UL_TWO_WIRE_MODE_CTL;
	else
		clk_phase_sel_ch2 = clk_phase_sel_ch1 + 4;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_TWO_WIRE_MODE_CTL_MASK;

	val |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_CH1(clk_phase_sel_ch1);
	val |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_CH2(clk_phase_sel_ch2);
	msk |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_MASK;

	mtk_dai_dmic_configure_array(dai);

	switch (rate) {
	case 48000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_48K;
		break;
	case 32000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_32K;
		break;
	case 16000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_16K;
		break;
	case 8000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_8K;
		break;
	default:
		dev_dbg(afe->dev, "%s invalid rate %u, use 48000Hz\n", __func__, rate);
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_48K;
		break;
	}
	msk |= AFE_DMIC_UL_VOICE_MODE_MASK;

	if (iir_on) {
		mtk_dai_dmic_load_iir_coeff_table(afe);
		val |= AFE_DMIC_UL_SRC_CON0_UL_IIR_MODE_CTL(0);
		msk |= AFE_DMIC_UL_SRC_CON0_UL_IIR_MODE_CTL_MASK;
	}

	if (channels > ch_base * 3) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base * 2) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > 0) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}

	dmic_priv->channels = channels;

	return 0;

}

static const struct snd_soc_dai_ops mtk_dai_dmic_ops = {
	.hw_params	= mtk_dai_dmic_hw_params,
};

/* dai driver */
#define MTK_DMIC_RATES (SNDRV_PCM_RATE_8000 |\
		       SNDRV_PCM_RATE_16000 |\
		       SNDRV_PCM_RATE_32000 |\
		       SNDRV_PCM_RATE_48000)

#define MTK_DMIC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_dmic_driver[] = {
	{
		.name = "DMIC",
		.id = MT8195_AFE_IO_DMIC_IN,
		.capture = {
			.stream_name = "DMIC Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_DMIC_RATES,
			.formats = MTK_DMIC_FORMATS,
		},
		.ops = &mtk_dai_dmic_ops,
	},
};

static const struct snd_soc_dapm_widget mtk_dai_dmic_widgets[] = {
	SND_SOC_DAPM_MIXER("I004", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I005", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I006", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I007", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I008", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I009", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I010", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I011", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DMIC_CK_ON",
			    PWR2_TOP_CON1,
			    PWR2_TOP_CON1_DMIC_CKDIV_ON_SHIFT, 0,
			    mtk_dmic_event,
			    SND_SOC_DAPM_PRE_POST_PMU |
			    SND_SOC_DAPM_PRE_POST_PMD),


	SND_SOC_DAPM_INPUT("DMIC_INPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_dmic_routes[] = {
	{"I004", NULL, "DMIC Capture"},
	{"I005", NULL, "DMIC Capture"},
	{"I006", NULL, "DMIC Capture"},
	{"I007", NULL, "DMIC Capture"},
	{"I008", NULL, "DMIC Capture"},
	{"I009", NULL, "DMIC Capture"},
	{"I010", NULL, "DMIC Capture"},
	{"I011", NULL, "DMIC Capture"},

	{"DMIC Capture", NULL, "DMIC_CK_ON"},
	{"DMIC Capture", NULL, "DMIC_INPUT"},
};

static int init_dmic_priv_data(struct mtk_base_afe *afe)
{
	const struct device_node *of_node = afe->dev->of_node;
	struct mt8195_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv;
	int ret = 0;

	dmic_priv = devm_kzalloc(afe->dev, sizeof(struct mtk_dai_dmic_priv),
				 GFP_KERNEL);
	if (!dmic_priv)
		return -ENOMEM;

	dmic_priv->two_wire_mode =
		of_property_read_bool(of_node, "mediatek,dmic-two-wire-mode");

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-clk-index",
					 &dmic_priv->clk_index[0], DMIC_NUM);
	if (ret) {
		dmic_priv->clk_index[0] = DMIC0;
		dmic_priv->clk_index[1] = DMIC1;
		dmic_priv->clk_index[2] = DMIC2;
		dmic_priv->clk_index[3] = DMIC3;
	}

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-clk-phases",
					 &dmic_priv->clk_phase_sel_ch[0],
					 2);
	if (ret != 0 && !dmic_priv->two_wire_mode) {
		dmic_priv->clk_phase_sel_ch[0] = 0;
		dmic_priv->clk_phase_sel_ch[1] = 4;
	}

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-src-sels",
					 &dmic_priv->dmic_src_sel[0],
					 DMIC_MAX_CH);
	if (ret != 0) {
		if (dmic_priv->two_wire_mode) {
			dmic_priv->dmic_src_sel[0] = 0;
			dmic_priv->dmic_src_sel[1] = 1;
			dmic_priv->dmic_src_sel[2] = 2;
			dmic_priv->dmic_src_sel[3] = 3;
			dmic_priv->dmic_src_sel[4] = 4;
			dmic_priv->dmic_src_sel[5] = 5;
			dmic_priv->dmic_src_sel[6] = 6;
			dmic_priv->dmic_src_sel[7] = 7;
		} else {
			dmic_priv->dmic_src_sel[0] = 0;
			dmic_priv->dmic_src_sel[2] = 2;
			dmic_priv->dmic_src_sel[4] = 4;
			dmic_priv->dmic_src_sel[6] = 6;
		}
	}

	dmic_priv->iir_on = of_property_read_bool(of_node,
						  "mediatek,dmic-iir-on");

	dmic_priv->dmic_clk_mono = of_property_read_bool(of_node,
							 "mediatek,dmic-clk-mono");
	afe_priv->dai_priv[MT8195_AFE_IO_DMIC_IN] = dmic_priv;
	return 0;
}

int mt8195_dai_dmic_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_dmic_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_dmic_driver);

	dai->dapm_widgets = mtk_dai_dmic_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_dmic_widgets);
	dai->dapm_routes = mtk_dai_dmic_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_dmic_routes);

	return init_dmic_priv_data(afe);
}
