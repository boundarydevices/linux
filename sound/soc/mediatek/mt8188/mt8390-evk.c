// SPDX-License-Identifier: GPL-2.0
/*
 * mt8390-evk.c  --  MT8390-EVK ALSA SoC machine driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Parker Yang <parker.yang@mediatek.com>
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of_device.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../../codecs/mt6359.h"
#include "../../codecs/mt6359-accdet.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-dsp-sof-common.h"
#include "../common/mtk-soc-card.h"
#include "../common/mtk-soundcard-driver.h"
#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"

#define SOF_DMA_DL2 "SOF_DMA_DL2"
#define SOF_DMA_DL3 "SOF_DMA_DL3"
#define SOF_DMA_UL4 "SOF_DMA_UL4"
#define SOF_DMA_UL5 "SOF_DMA_UL5"

struct mt8188_mt6359_priv {
	struct snd_soc_jack headset_jack;
	struct snd_soc_jack dp_jack;
	struct snd_soc_jack hdmi_jack;
};

struct mt8188_card_data {
	const char *name;
	unsigned long quirk;
};

/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin mt8188_headset_jack_pins[] = {
	{
		.pin = "Headphone",
		.mask = SND_JACK_HEADPHONE,
	},
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
};

/* HDMI jack detection DAPM pins */
static struct snd_soc_jack_pin mt8188_hdmi_jack_pins[] = {
	{
		.pin = "HDMI",
		.mask = SND_JACK_LINEOUT,
	},
};

/* DP jack detection DAPM pins */
static struct snd_soc_jack_pin mt8188_dp_jack_pins[] = {
	{
		.pin = "DP",
		.mask = SND_JACK_LINEOUT,
	},
};

static const struct snd_soc_dapm_widget
	mt8188_mt6359_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("AP DMIC", NULL),
	SND_SOC_DAPM_SINK("HDMI"),
	SND_SOC_DAPM_SINK("DP"),
	SND_SOC_DAPM_MIXER(SOF_DMA_DL2, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_DL3, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_UL4, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_UL5, SND_SOC_NOPM, 0, 0, NULL, 0),
};

#define CKSYS_AUD_TOP_CFG 0x032c
#define CKSYS_AUD_TOP_MON 0x0330

static const struct snd_soc_dapm_route mt8188_mt6359_routes[] = {
	/* SOF Uplink */
	{SOF_DMA_UL4, NULL, "O034"},
	{SOF_DMA_UL4, NULL, "O035"},
	{SOF_DMA_UL5, NULL, "O036"},
	{SOF_DMA_UL5, NULL, "O037"},
	/* SOF Downlink */
	{"I070", NULL, SOF_DMA_DL2},
	{"I071", NULL, SOF_DMA_DL2},
	{"I020", NULL, SOF_DMA_DL3},
	{"I021", NULL, SOF_DMA_DL3},
};

static int mt8188_mt6359_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *cmpnt_afe =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;
	struct mtk_base_afe *afe;
	struct mt8188_afe_private *afe_priv;
	struct mtkaif_param *param;
	int chosen_phase_1, chosen_phase_2;
	int prev_cycle_1, prev_cycle_2;
	int test_done_1, test_done_2;
	int cycle_1, cycle_2;
	int mtkaif_chosen_phase[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_phase_cycle[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_calibration_num_phase;
	bool mtkaif_calibration_ok = true;
	unsigned int monitor = 0;
	int counter;
	int phase;
	int i;

	if (!cmpnt_afe)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(cmpnt_afe);
	afe_priv = afe->platform_priv;
	param = &afe_priv->mtkaif_params;

	dev_dbg(afe->dev, "%s(), start\n", __func__);

	param->mtkaif_calibration_ok = false;
	for (i = 0; i < MT8188_MTKAIF_MISO_NUM; i++) {
		param->mtkaif_chosen_phase[i] = -1;
		param->mtkaif_phase_cycle[i] = 0;
		mtkaif_chosen_phase[i] = -1;
		mtkaif_phase_cycle[i] = 0;
	}

	if (IS_ERR(afe_priv->topckgen)) {
		dev_info(afe->dev, "%s() Cannot find topckgen controller\n",
			 __func__);
		return 0;
	}

	pm_runtime_get_sync(afe->dev);
	mt6359_mtkaif_calibration_enable(cmpnt_codec);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen,
			   CKSYS_AUD_TOP_CFG, 0xffff, 0x4);
	mtkaif_calibration_num_phase = 42;	/* mt6359: 0 ~ 42 */

	for (phase = 0;
	     phase <= mtkaif_calibration_num_phase && mtkaif_calibration_ok;
	     phase++) {
		mt6359_set_mtkaif_calibration_phase(cmpnt_codec,
						    phase, phase, phase);

		regmap_set_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG, 0x1);

		test_done_1 = 0;
		test_done_2 = 0;

		cycle_1 = -1;
		cycle_2 = -1;

		counter = 0;
		while (!(test_done_1 & test_done_2)) {
			regmap_read(afe_priv->topckgen,
				    CKSYS_AUD_TOP_MON, &monitor);
			test_done_1 = (monitor >> 28) & 0x1;
			test_done_2 = (monitor >> 29) & 0x1;

			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;

			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_info(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					 __func__,
					 cycle_1, cycle_2, monitor);
				mtkaif_calibration_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (cycle_1 != prev_cycle_1 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] < 0) {
			mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] = phase - 1;
			mtkaif_phase_cycle[MT8188_MTKAIF_MISO_0] = prev_cycle_1;
		}

		if (cycle_2 != prev_cycle_2 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] < 0) {
			mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] = phase - 1;
			mtkaif_phase_cycle[MT8188_MTKAIF_MISO_1] = prev_cycle_2;
		}

		regmap_clear_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG, 0x1);

		if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] >= 0 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] >= 0)
			break;
	}

	if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] < 0) {
		mtkaif_calibration_ok = false;
		chosen_phase_1 = 0;
	} else {
		chosen_phase_1 = mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0];
	}

	if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] < 0) {
		mtkaif_calibration_ok = false;
		chosen_phase_2 = 0;
	} else {
		chosen_phase_2 = mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1];
	}

	mt6359_set_mtkaif_calibration_phase(cmpnt_codec,
					    chosen_phase_1,
					    chosen_phase_2,
					    0);

	mt6359_mtkaif_calibration_disable(cmpnt_codec);
	pm_runtime_put(afe->dev);

	param->mtkaif_calibration_ok = mtkaif_calibration_ok;
	param->mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] = chosen_phase_1;
	param->mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] = chosen_phase_2;

	for (i = 0; i < MT8188_MTKAIF_MISO_NUM; i++)
		param->mtkaif_phase_cycle[i] = mtkaif_phase_cycle[i];

	dev_info(afe->dev, "%s(), end, calibration ok %d\n",
		 __func__, param->mtkaif_calibration_ok);

	return 0;
}

static int mt8188_mt6359_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct mt8188_mt6359_priv *priv = soc_card_data->mach_priv;
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;
	struct snd_soc_component *cmpnt_accdet =
		asoc_rtd_to_codec(rtd, 2)->component;
	int ret;

	/* set mtkaif protocol */
	mt6359_set_mtkaif_protocol(cmpnt_codec,
				   MT6359_MTKAIF_PROTOCOL_2_CLK_P2);

	/* mtkaif calibration */
	mt8188_mt6359_mtkaif_calibration(rtd);

	ret = snd_soc_card_jack_new(rtd->card, "Headset Jack",
				    SND_JACK_HEADSET | SND_JACK_BTN_0 |
				    SND_JACK_BTN_1 | SND_JACK_BTN_2 |
				    SND_JACK_BTN_3,
				    &priv->headset_jack, mt8188_headset_jack_pins,
				    ARRAY_SIZE(mt8188_headset_jack_pins));
	if (ret) {
		dev_err(rtd->dev, "Headset Jack create failed: %d\n", ret);
		return ret;
	}

	ret = mt6359_accdet_enable_jack_detect(cmpnt_accdet, &priv->headset_jack);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack enable failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt8188_etdm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	int ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk_fs, SND_SOC_CLOCK_OUT);
	if (ret) {
		dev_err(card->dev, "failed to set sysclk\n");
		return ret;
	}

	return 0;
}

static const struct snd_soc_ops mt8188_etdm_ops = {
	.hw_params = mt8188_etdm_hw_params,
};

static int mt8188_dptx_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 256;
	unsigned int mclk_fs = rate * mclk_fs_ratio;

	return snd_soc_dai_set_sysclk(cpu_dai, 0, mclk_fs,
				      SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt8188_dptx_ops = {
	.hw_params = mt8188_dptx_hw_params,
};

static int mt8188_dptx_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct mt8188_mt6359_priv *priv = soc_card_data->mach_priv;
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;
	int ret;

	ret = snd_soc_card_jack_new(rtd->card, "DP Jack", SND_JACK_LINEOUT,
				    &priv->dp_jack, mt8188_dp_jack_pins,
				    ARRAY_SIZE(mt8188_dp_jack_pins));
	if (ret)
		return ret;

	return snd_soc_component_set_jack(cmpnt_codec, &priv->dp_jack, NULL);
}

static int mt8188_hdmi_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct mt8188_mt6359_priv *priv = soc_card_data->mach_priv;
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;
	int ret;

	ret = snd_soc_card_jack_new(rtd->card, "HDMI Jack", SND_JACK_LINEOUT,
				    &priv->hdmi_jack, mt8188_hdmi_jack_pins,
				    ARRAY_SIZE(mt8188_hdmi_jack_pins));
	if (ret)
		return ret;

	return snd_soc_component_set_jack(cmpnt_codec, &priv->hdmi_jack, NULL);
}

static int mt8188_dptx_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				       struct snd_pcm_hw_params *params)
{
	/* fix BE i2s format to S24_LE, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, (__force unsigned int)SNDRV_PCM_FORMAT_LAST);
	params_set_format(params, SNDRV_PCM_FORMAT_S24_LE);
	return 0;
}

enum {
	DAI_LINK_DL2_FE,
	DAI_LINK_DL3_FE,
	DAI_LINK_DL6_FE,
	DAI_LINK_DL7_FE,
	DAI_LINK_DL8_FE,
	DAI_LINK_DL10_FE,
	DAI_LINK_DL11_FE,
	DAI_LINK_UL1_FE,
	DAI_LINK_UL2_FE,
	DAI_LINK_UL3_FE,
	DAI_LINK_UL4_FE,
	DAI_LINK_UL5_FE,
	DAI_LINK_UL6_FE,
	DAI_LINK_UL8_FE,
	DAI_LINK_UL9_FE,
	DAI_LINK_UL10_FE,
	DAI_LINK_ADDA_BE,
	DAI_LINK_DMIC_BE,
	DAI_LINK_DPTX_BE,
	DAI_LINK_ETDM1_IN_BE,
	DAI_LINK_ETDM2_IN_BE,
	DAI_LINK_ETDM1_OUT_BE,
	DAI_LINK_ETDM2_OUT_BE,
	DAI_LINK_ETDM3_OUT_BE,
	DAI_LINK_PCM1_BE,
	DAI_LINK_REGULAR_LAST = DAI_LINK_PCM1_BE,
	DAI_LINK_SOF_START,
	DAI_LINK_SOF_DL2_BE = DAI_LINK_SOF_START,
	DAI_LINK_SOF_DL3_BE,
	DAI_LINK_SOF_UL4_BE,
	DAI_LINK_SOF_UL5_BE,
	DAI_LINK_SOF_END = DAI_LINK_SOF_UL5_BE,
};

#define	DAI_LINK_REGULAR_NUM	(DAI_LINK_REGULAR_LAST + 1)

/* FE */
SND_SOC_DAILINK_DEFS(DL2_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL3_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL6_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL7_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL8_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL10_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL10")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DL11_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL11")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL1_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL2_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL3_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL4_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL5_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL6_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL8_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL9_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL9")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(UL10_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL10")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
/* BE */
SND_SOC_DAILINK_DEFS(ADDA_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DMIC_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DMIC")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(DPTX_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DPTX")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ETDM1_IN_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ETDM1_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ETDM2_IN_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ETDM2_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ETDM1_OUT_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ETDM1_OUT")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ETDM2_OUT_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ETDM2_OUT")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ETDM3_OUT_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("ETDM3_OUT")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(PCM1_BE,
		     DAILINK_COMP_ARRAY(COMP_CPU("PCM1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* SOF */
SND_SOC_DAILINK_DEFS(AFE_SOF_DL2,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_DL3,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_UL4,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_UL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_UL5,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_UL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

static int mt8188_sof_be_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *cmpnt_afe = NULL;
	struct snd_soc_pcm_runtime *runtime;

	/* find afe component */
	for_each_card_rtds(rtd->card, runtime) {
		cmpnt_afe = snd_soc_rtdcom_lookup(runtime, AFE_PCM_NAME);
		if (cmpnt_afe)
			break;
	}

	if (cmpnt_afe && !pm_runtime_active(cmpnt_afe->dev)) {
		dev_err(rtd->dev, "afe pm runtime is not active!!\n");
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_ops mt8188_sof_be_ops = {
	.hw_params = mt8188_sof_be_hw_params,
};

static struct snd_soc_dai_link mt8188_mt6359_dai_links[] = {
	/* FE */
	[DAI_LINK_DL2_FE] = {
		.name = "DL2_FE",
		.stream_name = "DL2 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL2_FE),
	},
	[DAI_LINK_DL3_FE] = {
		.name = "DL3_FE",
		.stream_name = "DL3 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL3_FE),
	},
	[DAI_LINK_DL6_FE] = {
		.name = "DL6_FE",
		.stream_name = "DL6 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL6_FE),
	},
	[DAI_LINK_DL7_FE] = {
		.name = "DL7_FE",
		.stream_name = "DL7 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL7_FE),
	},
	[DAI_LINK_DL8_FE] = {
		.name = "DL8_FE",
		.stream_name = "DL8 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL8_FE),
	},
	[DAI_LINK_DL10_FE] = {
		.name = "DL10_FE",
		.stream_name = "DL10 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL10_FE),
	},
	[DAI_LINK_DL11_FE] = {
		.name = "DL11_FE",
		.stream_name = "DL11 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL11_FE),
	},
	[DAI_LINK_UL1_FE] = {
		.name = "UL1_FE",
		.stream_name = "UL1 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL1_FE),
	},
	[DAI_LINK_UL2_FE] = {
		.name = "UL2_FE",
		.stream_name = "UL2 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL2_FE),
	},
	[DAI_LINK_UL3_FE] = {
		.name = "UL3_FE",
		.stream_name = "UL3 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL3_FE),
	},
	[DAI_LINK_UL4_FE] = {
		.name = "UL4_FE",
		.stream_name = "UL4 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL4_FE),
	},
	[DAI_LINK_UL5_FE] = {
		.name = "UL5_FE",
		.stream_name = "UL5 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL5_FE),
	},
	[DAI_LINK_UL6_FE] = {
		.name = "UL6_FE",
		.stream_name = "UL6 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL6_FE),
	},
	[DAI_LINK_UL8_FE] = {
		.name = "UL8_FE",
		.stream_name = "UL8 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL8_FE),
	},
	[DAI_LINK_UL9_FE] = {
		.name = "UL9_FE",
		.stream_name = "UL9 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL9_FE),
	},
	[DAI_LINK_UL10_FE] = {
		.name = "UL10_FE",
		.stream_name = "UL10 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL10_FE),
	},
	/* BE */
	[DAI_LINK_ADDA_BE] = {
		.name = "ADDA_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(ADDA_BE),
	},
	[DAI_LINK_DMIC_BE] = {
		.name = "DMIC_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(DMIC_BE),
	},
	[DAI_LINK_DPTX_BE] = {
		.name = "DPTX_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &mt8188_dptx_ops,
		.be_hw_params_fixup = mt8188_dptx_hw_params_fixup,
		SND_SOC_DAILINK_REG(DPTX_BE),
	},
	[DAI_LINK_ETDM1_IN_BE] = {
		.name = "ETDM1_IN_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBP_CFP,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.ops = &mt8188_etdm_ops,
		SND_SOC_DAILINK_REG(ETDM1_IN_BE),
	},
	[DAI_LINK_ETDM2_IN_BE] = {
		.name = "ETDM2_IN_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBP_CFP,
		.dpcm_capture = 1,
		.ops = &mt8188_etdm_ops,
		SND_SOC_DAILINK_REG(ETDM2_IN_BE),
	},
	[DAI_LINK_ETDM1_OUT_BE] = {
		.name = "ETDM1_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBC_CFC,
		.dpcm_playback = 1,
		.ops = &mt8188_etdm_ops,
		SND_SOC_DAILINK_REG(ETDM1_OUT_BE),
	},
	[DAI_LINK_ETDM2_OUT_BE] = {
		.name = "ETDM2_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBC_CFC,
		.dpcm_playback = 1,
		.ops = &mt8188_etdm_ops,
		SND_SOC_DAILINK_REG(ETDM2_OUT_BE),
	},
	[DAI_LINK_ETDM3_OUT_BE] = {
		.name = "ETDM3_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBC_CFC,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(ETDM3_OUT_BE),
	},
	[DAI_LINK_PCM1_BE] = {
		.name = "PCM1_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBC_CFC,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(PCM1_BE),
	},
	/* SOF BE */
	[DAI_LINK_SOF_DL2_BE] = {
		.name = "AFE_SOF_DL2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &mt8188_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_DL2),
	},
	[DAI_LINK_SOF_DL3_BE] = {
		.name = "AFE_SOF_DL3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &mt8188_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_DL3),
	},
	[DAI_LINK_SOF_UL4_BE] = {
		.name = "AFE_SOF_UL4",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ops = &mt8188_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_UL4),
	},
	[DAI_LINK_SOF_UL5_BE] = {
		.name = "AFE_SOF_UL5",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ops = &mt8188_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_UL5),
	},
};

static const struct sof_conn_stream g_sof_conn_streams[] = {
	{
		.sof_link = "AFE_SOF_DL2",
		.sof_dma = SOF_DMA_DL2,
		.stream_dir = SNDRV_PCM_STREAM_PLAYBACK
	},
	{
		.sof_link = "AFE_SOF_DL3",
		.sof_dma = SOF_DMA_DL3,
		.stream_dir = SNDRV_PCM_STREAM_PLAYBACK
	},
	{
		.sof_link = "AFE_SOF_UL4",
		.sof_dma = SOF_DMA_UL4,
		.stream_dir = SNDRV_PCM_STREAM_CAPTURE
	},
	{
		.sof_link = "AFE_SOF_UL5",
		.sof_dma = SOF_DMA_UL5,
		.stream_dir = SNDRV_PCM_STREAM_CAPTURE
	},
};

static struct snd_soc_card mt8188_mt6359_soc_card = {
	.name = "mt8390-evk",
	.owner = THIS_MODULE,
	.dai_link = mt8188_mt6359_dai_links,
	.num_links = ARRAY_SIZE(mt8188_mt6359_dai_links),
	.dapm_widgets = mt8188_mt6359_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8188_mt6359_widgets),
	.dapm_routes = mt8188_mt6359_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8188_mt6359_routes),
};

static int mt8188_mt6359_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8188_mt6359_soc_card;
	struct device_node *platform_node, *adsp_node;
	struct snd_soc_dai_link *dai_link;
	struct mt8188_mt6359_priv *priv;
	struct mtk_soc_card_data *soc_card_data;
	struct mt8188_card_data *card_data;
	int ret, i;

	card_data = (struct mt8188_card_data *)of_device_get_match_data(&pdev->dev);
	card->dev = &pdev->dev;
	ret = set_card_codec_info(card);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "%s set_card_codec_info failed\n",
			     __func__);
		return ret;
	}

	ret = snd_soc_of_parse_card_name(card, "model");
	if (ret) {
		dev_err(&pdev->dev, "%s new card name parsing error %d\n",
			__func__, ret);
		return ret;
	}

	if (!card->name)
		card->name = card_data->name;

	if (of_property_read_bool(pdev->dev.of_node, "audio-routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "audio-routing");
		if (ret) {
			dev_dbg(&pdev->dev, "Property 'audio-routing' missing or invalid\n");
			return ret;
		}
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	soc_card_data = devm_kzalloc(&pdev->dev, sizeof(*soc_card_data), GFP_KERNEL);
	if (!soc_card_data)
		return -ENOMEM;

	soc_card_data->mach_priv = priv;

	adsp_node = of_parse_phandle(pdev->dev.of_node, "mediatek,adsp", 0);
	if (adsp_node) {
		struct mtk_sof_priv *sof_priv;

		sof_priv = devm_kzalloc(&pdev->dev, sizeof(*sof_priv), GFP_KERNEL);
		if (!sof_priv) {
			ret = -ENOMEM;
			goto err_adsp_node;
		}
		sof_priv->conn_streams = g_sof_conn_streams;
		sof_priv->num_streams = ARRAY_SIZE(g_sof_conn_streams);
		soc_card_data->sof_priv = sof_priv;
		card->probe = mtk_sof_card_probe;
		card->late_probe = mtk_sof_card_late_probe;
		if (!card->topology_shortname_created) {
			snprintf(card->topology_shortname, 32, "sof-%s", card->name);
			card->topology_shortname_created = true;
		}
		card->name = card->topology_shortname;
	}

	if (of_property_read_bool(pdev->dev.of_node, "mediatek,dai-link")) {
		ret = mtk_sof_dailink_parse_of(card, pdev->dev.of_node,
					       "mediatek,dai-link",
					       mt8188_mt6359_dai_links,
					       ARRAY_SIZE(mt8188_mt6359_dai_links));
		if (ret) {
			dev_err_probe(&pdev->dev, ret, "Parse dai-link fail\n");
			goto err_adsp_node;
		}
	} else {
		if (!adsp_node)
			card->num_links = DAI_LINK_REGULAR_NUM;
	}

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		ret = dev_err_probe(&pdev->dev, -EINVAL,
				    "Property 'platform' missing or invalid\n");
		goto err_adsp_node;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (!dai_link->platforms->name) {
			if (!strncmp(dai_link->name, "AFE_SOF", strlen("AFE_SOF")) && adsp_node)
				dai_link->platforms->of_node = adsp_node;
			else
				dai_link->platforms->of_node = platform_node;
		}
		if (strcmp(dai_link->name, "DPTX_BE") == 0) {
			if (strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai"))
				dai_link->init = mt8188_dptx_codec_init;
		} else if (strcmp(dai_link->name, "ETDM3_OUT_BE") == 0) {
			if (strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai"))
				dai_link->init = mt8188_hdmi_codec_init;
		} else if (strcmp(dai_link->name, "ADDA_BE") == 0) {
			if (strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai"))
				dai_link->init = mt8188_mt6359_init;
		}
	}

	snd_soc_card_set_drvdata(card, soc_card_data);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err_probe(&pdev->dev, ret, "%s snd_soc_register_card fail\n",
			     __func__);

	of_node_put(platform_node);

err_adsp_node:
	of_node_put(adsp_node);

	return ret;
}

static struct mt8188_card_data mt8390_evk_card = {
	.name = "mt8390-evk",
};

static const struct of_device_id mt8188_mt6359_dt_match[] = {
	{.compatible = "mediatek,mt8390-evk", .data = &mt8390_evk_card, },
	{}
};
MODULE_DEVICE_TABLE(of, mt8188_mt6359_dt_match);

static struct platform_driver mt8188_mt6359_driver = {
	.driver = {
		.name = "mt8390_evk",
		.of_match_table = mt8188_mt6359_dt_match,
		.pm = &snd_soc_pm_ops,
	},
	.probe = mt8188_mt6359_dev_probe,
};

module_platform_driver(mt8188_mt6359_driver);

/* Module information */
MODULE_DESCRIPTION("MT8390-EVK ALSA SoC machine driver");
MODULE_AUTHOR("Parker Yang <parker.yang@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mt8390-evk soc card");
