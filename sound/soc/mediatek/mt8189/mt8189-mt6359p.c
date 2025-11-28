// SPDX-License-Identifier: GPL-2.0
/*
 *  mt8189-mt6359p.c  --  mt8189 mt6359p ALSA SoC machine driver
 *
 *  Copyright (c) 2025 MediaTek Inc.
 *  Author: Darren Ye <darren.ye@mediatek.com>
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>

#include "mtk-afe-platform-driver.h"
#include "mtk-soundcard-driver.h"
#include "mtk-soc-card.h"
#include "mt8189-afe-common.h"
#include "mt8189-afe-clk.h"

#include "../../codecs/mt6359.h"
#include "../../codecs/mt6359-accdet.h"
#include "../../codecs/nau8825.h"
#include "../../codecs/rt5682s.h"
#include "../../codecs/rt5682.h"

#define NAU8825_HS_PRESENT	BIT(0)
#define RT5682S_HS_PRESENT	BIT(1)
#define RT5650_HS_PRESENT	BIT(2)
#define RT5682I_HS_PRESENT	BIT(3)

/*
 * Nau88l25
 */
#define NAU8825_CODEC_DAI  "nau8825-hifi"

/*
 * Rt5682s
 */
#define RT5682S_CODEC_DAI     "rt5682s-aif1"

/*
 * Rt5650
 */
#define RT5650_CODEC_DAI     "rt5645-aif1"

/*
 * Rt5682i
 */
#define RT5682I_CODEC_DAI     "rt5682-aif1"

enum mt8189_jacks {
	MT8189_JACK_HEADSET,
	MT8189_JACK_DP,
	MT8189_JACK_HDMI,
	MT8189_JACK_MAX,
};

static struct snd_soc_jack_pin mt8189_dp_jack_pins[] = {
	{
		.pin = "DP",
		.mask = SND_JACK_LINEOUT,
	},
};

static struct snd_soc_jack_pin mt8189_hdmi_jack_pins[] = {
	{
		.pin = "HDMI",
		.mask = SND_JACK_LINEOUT,
	},
};

static struct snd_soc_jack_pin nau8825_jack_pins[] = {
	{
		.pin    = "Headphone Jack",
		.mask   = SND_JACK_HEADPHONE,
	},
	{
		.pin    = "Headset Mic",
		.mask   = SND_JACK_MICROPHONE,
	},
};

static struct snd_soc_jack_pin mt6359_jack_pins[] = {
	{
		.pin    = "Headphone Jack",
		.mask   = SND_JACK_HEADPHONE,
	},
	{
		.pin    = "Headset Mic",
		.mask   = SND_JACK_MICROPHONE,
	},
};

static const struct snd_kcontrol_new mt8189_dumb_spk_controls[] = {
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
};

static const struct snd_soc_dapm_widget mt8189_dumb_spk_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

static const struct snd_soc_dapm_widget mt8189_nau8825_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_SINK("DP"),
};

static const struct snd_soc_dapm_widget mt8189_mt6359_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
};

static const struct snd_kcontrol_new mt8189_nau8825_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

/*
 * if need additional control for the ext spk amp that is connected
 * after Lineout Buffer / HP Buffer on the codec, put the control in
 * mt8189_mt6359p_spk_amp_event()
 */
#define EXT_SPK_AMP_W_NAME "Ext_Speaker_Amp"

static const struct snd_soc_dapm_widget mt8189_mt6359p_widgets[] = {
	SND_SOC_DAPM_PINCTRL("ETDMIN_SPK_PIN", "aud-gpio-i2sin1-on", "aud-gpio-i2sin1-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_SPK_PIN", "aud-gpio-i2sout1-on", "aud-gpio-i2sout1-off"),
	SND_SOC_DAPM_PINCTRL("ETDMIN_HP_PIN", "aud-gpio-i2sin0-on", "aud-gpio-i2sin0-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_HP_PIN", "aud-gpio-i2sout0-on", "aud-gpio-i2sout0-off"),
	SND_SOC_DAPM_PINCTRL("ETDMOUT_HDMI_PIN", "aud-gpio-pcm-on", "aud-gpio-pcm-off"),
	SND_SOC_DAPM_PINCTRL("AP_DMIC0_PIN", "aud-gpio-ap-dmic-on", "aud-gpio-ap-dmic-off"),
	SND_SOC_DAPM_PINCTRL("AP_DMIC1_PIN", "aud-gpio-ap-dmic1-on", "aud-gpio-ap-dmic1-off"),
	SND_SOC_DAPM_PINCTRL("PMIC_CODEC_PIN", "aud-gpio-pmic-on", "aud-gpio-pmic-off"),
};

static const struct snd_soc_dapm_route mt8189_mt6359p_routes[] = {
};

static const struct snd_kcontrol_new mt8189_mt6359p_controls[] = {
	SOC_DAPM_PIN_SWITCH(EXT_SPK_AMP_W_NAME),
};

/*
 * define mtk_spk_i2s_mck node in dts when need mclk,
 * BE i2s need assign snd_soc_ops = mt8189_mt6359p_i2s_ops
 */
static int mt8189_mt6359p_i2s_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);

	return snd_soc_dai_set_sysclk(cpu_dai,
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt8189_mt6359p_i2s_ops = {
	.hw_params = mt8189_mt6359p_i2s_hw_params,
};

static int mt8189_dptx_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 256;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	struct snd_soc_dai *dai = snd_soc_rtd_to_cpu(rtd, 0);

	return snd_soc_dai_set_sysclk(dai, 0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt8189_dptx_ops = {
	.hw_params = mt8189_dptx_hw_params,
};

static int mt8189_dptx_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				       struct snd_pcm_hw_params *params)
{
	dev_dbg(rtd->dev, "%s(), fix format to 32bit\n", __func__);

	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, (__force unsigned int)SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);

	return 0;
}

static int mt8189_mt6359_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6359_NAME);
	struct snd_soc_dapm_widget *pin_w = NULL, *w;
	int phase;
	unsigned int monitor = 0;
	int test_done_1, test_done_2;
	int cycle_1, cycle_2;
	int prev_cycle_1, prev_cycle_2;
	int counter;
	int mtkaif_calib_ok;

	if (!afe_priv->topckgen || !afe->regmap)
		return 0;

	for_each_card_widgets(rtd->card, w) {
		if (!strcmp(w->name, "PMIC_CODEC_PIN")) {
			pin_w = w;
			break;
		}
	}

	if (pin_w)
		dapm_pinctrl_event(pin_w, NULL, SND_SOC_DAPM_PRE_PMU);
	else
		dev_warn(afe->dev, "%s(), no pinmux widget\n", __func__);

	pm_runtime_get_sync(afe->dev);
	mt6359_mtkaif_calibration_enable(codec_component);

	/* set clock protocol 2 */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb8);
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb9);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen,
			   CKSYS_AUD_TOP_CFG, 0xffff, 0x4);

	mtkaif_calib_ok = true;
	afe_priv->mtkaif_calibration_num_phase = 42;	/* mt6359: 0 ~ 42 */
	afe_priv->mtkaif_chosen_phase[0] = -1;
	afe_priv->mtkaif_chosen_phase[1] = -1;
	afe_priv->mtkaif_chosen_phase[2] = -1;

	for (phase = 0;
	     phase <= afe_priv->mtkaif_calibration_num_phase &&
	     mtkaif_calib_ok;
	     phase++) {
		mt6359_set_mtkaif_calibration_phase(codec_component,
						    phase, phase, phase);

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x1);

		test_done_1 = 0;
		test_done_2 = 0;
		cycle_1 = -1;
		cycle_2 = -1;
		counter = 0;
		while (test_done_1 == 0 ||
		       test_done_2 == 0) {
			regmap_read(afe_priv->topckgen,
				    CKSYS_AUD_TOP_MON, &monitor);

			/* get test status */
			if (test_done_1 == 0)
				test_done_1 = (monitor >> 28) & 0x1;
			if (test_done_2 == 0)
				test_done_2 = (monitor >> 29) & 0x1;

			/* get delay cycle */
			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;
			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_warn(afe->dev,
					 "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					 __func__,
					 cycle_1, cycle_2, monitor);
				mtkaif_calib_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (cycle_1 != prev_cycle_1 &&
		    afe_priv->mtkaif_chosen_phase[0] < 0) {
			afe_priv->mtkaif_chosen_phase[0] = phase - 1;
			afe_priv->mtkaif_phase_cycle[0] = prev_cycle_1;
		}

		if (cycle_2 != prev_cycle_2 &&
		    afe_priv->mtkaif_chosen_phase[1] < 0) {
			afe_priv->mtkaif_chosen_phase[1] = phase - 1;
			afe_priv->mtkaif_phase_cycle[1] = prev_cycle_2;
		}

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x0);
	}

	mt6359_set_mtkaif_calibration_phase(codec_component,
					    (afe_priv->mtkaif_chosen_phase[0] < 0) ?
					    0 : afe_priv->mtkaif_chosen_phase[0],
					    (afe_priv->mtkaif_chosen_phase[1] < 0) ?
					    0 : afe_priv->mtkaif_chosen_phase[1],
					    (afe_priv->mtkaif_chosen_phase[2] < 0) ?
					    0 : afe_priv->mtkaif_chosen_phase[2]);

	/* disable rx fifo */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb8);

	mt6359_mtkaif_calibration_disable(codec_component);

	if (pin_w)
		dapm_pinctrl_event(pin_w, NULL, SND_SOC_DAPM_POST_PMD);

	pm_runtime_put(afe->dev);

	dev_info(afe->dev, "%s(), mtkaif_chosen_phase[0/1]:%d/%d\n",
		 __func__,
		 afe_priv->mtkaif_chosen_phase[0],
		 afe_priv->mtkaif_chosen_phase[1]);

	return 0;
}

static int mt8189_mt6359_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_jack *jack = &soc_card_data->card_data->jacks[MT8189_JACK_HEADSET];
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8189_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *cmpnt;
	struct snd_soc_dai *codec_dai;
	int i, ret;

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		cmpnt = codec_dai->component;
		if (strcmp(cmpnt->name, "mt6359-sound") == 0) {
			/* set mtkaif protocol */
			mt6359_set_mtkaif_protocol(cmpnt,
						   MTKAIF_PROTOCOL_2_CLK_P2);
			afe_priv->mtkaif_protocol = MTKAIF_PROTOCOL_2_CLK_P2;

			/* mtkaif calibration */
			mt8189_mt6359_mtkaif_calibration(rtd);
		} else if (strcmp(cmpnt->name, "mt6359-accdet") == 0) {
			ret = snd_soc_dapm_new_controls(&card->dapm, mt8189_mt6359_widgets,
							ARRAY_SIZE(mt8189_mt6359_widgets));
			if (ret) {
				dev_err(rtd->dev, "unable to add card widget, ret %d\n", ret);
				return ret;
			}

			ret = snd_soc_card_jack_new_pins(rtd->card, "Headset Jack",
							 SND_JACK_HEADSET | SND_JACK_BTN_0 |
							 SND_JACK_BTN_1 | SND_JACK_BTN_2 |
							 SND_JACK_BTN_3,
							 jack, mt6359_jack_pins,
							 ARRAY_SIZE(mt6359_jack_pins));
			if (ret) {
				dev_err(rtd->dev, "Headset Jack create failed: %d\n", ret);
				return ret;
			}

			ret = mt6359_accdet_enable_jack_detect(cmpnt, jack);
			if (ret) {
				dev_err(rtd->dev, "Headset Jack enable failed: %d\n", ret);
				return ret;
			}
		} else {
			dev_err(rtd->dev, "Component '%s' is invalid.\n", cmpnt->name);
		}
	}

	return 0;
}

/* FE */
SND_SOC_DAILINK_DEFS(playback0,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback1,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback2,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback3,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback4,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback5,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback6,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback7,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback8,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback23,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL23")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback24,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL24")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback25,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL25")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback_24ch,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL_24CH")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture0,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture1,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture2,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture3,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture4,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture5,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture6,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture7,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture8,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL8")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture9,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL9")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture10,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL10")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture24,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL24")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture25,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL25")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_cm0,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL_CM0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_cm1,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL_CM1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback_hdmi,
		     DAILINK_COMP_ARRAY(COMP_CPU("HDMI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
/* BE */
SND_SOC_DAILINK_DEFS(adda,
		     DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ap_dmic,
		     DAILINK_COMP_ARRAY(COMP_CPU("AP_DMIC")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ap_dmic_ch34,
		     DAILINK_COMP_ARRAY(COMP_CPU("AP_DMIC_CH34")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sin0,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2SIN0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sin1,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2SIN1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sout0,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2SOUT0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sout1,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2SOUT1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(pcm0,
		     DAILINK_COMP_ARRAY(COMP_CPU("PCM 0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(tdm_dptx,
		     DAILINK_COMP_ARRAY(COMP_CPU("TDM_DPTX")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src0,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src1,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src2,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src3,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src4,
		     DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt8189_mt6359p_dai_links[] = {
	/* Front End DAI links */
	{
		.name = "DL0_FE",
		.stream_name = "DL0 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback0),
	},
	{
		.name = "DL1_FE",
		.stream_name = "DL1 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "UL0_FE",
		.stream_name = "UL0 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
				SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture0),
	},
	{
		.name = "UL1_FE",
		.stream_name = "UL1 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
				SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture1),
	},
	{
		.name = "UL2_FE",
		.stream_name = "UL2 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
				SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture2),
	},
	{
		.name = "HDMI_FE",
		.stream_name = "HDMI Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
				SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback_hdmi),
	},
	{
		.name = "DL2_FE",
		.stream_name = "DL2 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
				SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	{
		.name = "DL3_FE",
		.stream_name = "DL3 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback3),
	},
	{
		.name = "DL4_FE",
		.stream_name = "DL4 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback4),
	},
	{
		.name = "DL5_FE",
		.stream_name = "DL5 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback5),
	},
	{
		.name = "DL6_FE",
		.stream_name = "DL6 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback6),
	},
	{
		.name = "DL7_FE",
		.stream_name = "DL7 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback7),
	},
	{
		.name = "DL8 FE",
		.stream_name = "DL8 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback8),
	},
	{
		.name = "DL23 FE",
		.stream_name = "DL23 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback23),
	},
	{
		.name = "DL24 FE",
		.stream_name = "DL24 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback24),
	},
	{
		.name = "DL25 FE",
		.stream_name = "DL25 Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback25),
	},
	{
		.name = "DL_24CH_FE",
		.stream_name = "DL_24CH Playback",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback_24ch),
	},
	{
		.name = "UL9_FE",
		.stream_name = "UL9 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture9),
	},
	{
		.name = "UL3_FE",
		.stream_name = "UL3 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture3),
	},
	{
		.name = "UL7_FE",
		.stream_name = "UL7 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture7),
	},
	{
		.name = "UL4_FE",
		.stream_name = "UL4 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture4),
	},
	{
		.name = "UL5_FE",
		.stream_name = "UL5 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture5),
	},
	{
		.name = "UL_CM0_FE",
		.stream_name = "UL_CM0 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_cm0),
	},
	{
		.name = "UL_CM1_FE",
		.stream_name = "UL_CM1 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_cm1),
	},
	{
		.name = "UL10_FE",
		.stream_name = "UL10 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture10),
	},
	{
		.name = "UL6_FE",
		.stream_name = "UL6 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture6),
	},
	{
		.name = "UL25_FE",
		.stream_name = "UL25 Capture",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture25),
	},
	{
		.name = "UL8_FE",
		.stream_name = "UL8 Capture_Mono_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture8),
	},
	{
		.name = "UL24_FE",
		.stream_name = "UL24 Capture_Mono_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture24),
	},
	/* Back End DAI links */
	{
		.name = "ADDA_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt8189_mt6359_init,
		SND_SOC_DAILINK_REG(adda),
	},
	{
		.name = "I2SIN0_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt8189_mt6359p_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(i2sin0),
	},
	{
		.name = "I2SIN1_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt8189_mt6359p_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(i2sin1),
	},
	{
		.name = "I2SOUT0_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt8189_mt6359p_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(i2sout0),
	},
	{
		.name = "I2SOUT1_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt8189_mt6359p_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(i2sout1),
	},
	{
		.name = "AP_DMIC_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(ap_dmic),
	},
	{
		.name = "AP_DMIC_CH34_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(ap_dmic_ch34),
	},
	{
		.name = "TDM_DPTX_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt8189_dptx_ops,
		.be_hw_params_fixup = mt8189_dptx_hw_params_fixup,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(tdm_dptx),
	},
	{
		.name = "PCM_0_BE",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBC_CFC
			| SND_SOC_DAIFMT_GATED,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm0),
	},
	{
		.name = "HW_SRC_0",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src0),
	},
	{
		.name = "HW_SRC_1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src1),
	},
	{
		.name = "HW_SRC_2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src2),
	},
	{
		.name = "HW_SRC_3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src3),
	},
	{
		.name = "HW_SRC_4",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src4),
	},
};

static int mt8189_dumb_amp_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret = 0;

	ret = snd_soc_dapm_new_controls(&card->dapm, mt8189_dumb_spk_widgets,
					ARRAY_SIZE(mt8189_dumb_spk_widgets));
	if (ret) {
		dev_err(rtd->dev, "unable to add Dumb Speaker dapm, ret %d\n", ret);
		return ret;
	}

	ret = snd_soc_add_card_controls(card, mt8189_dumb_spk_controls,
					ARRAY_SIZE(mt8189_dumb_spk_controls));
	if (ret) {
		dev_err(rtd->dev, "unable to add Dumb card controls, ret %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt8189_dptx_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_jack *jack = &soc_card_data->card_data->jacks[MT8189_JACK_DP];
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	int ret = 0;

	ret = snd_soc_card_jack_new_pins(rtd->card, "DP Jack", SND_JACK_LINEOUT,
					 jack, mt8189_dp_jack_pins,
					 ARRAY_SIZE(mt8189_dp_jack_pins));
	if (ret) {
		dev_err(rtd->dev, "%s, new jack failed: %d\n", __func__, ret);
		return ret;
	}

	ret = snd_soc_component_set_jack(component, jack, NULL);
	if (ret) {
		dev_err(rtd->dev, "%s, set jack failed on %s (ret=%d)\n",
			__func__, component->name, ret);
		return ret;
	}

	return 0;
}

static int mt8189_hdmi_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_jack *jack = &soc_card_data->card_data->jacks[MT8189_JACK_HDMI];
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	int ret = 0;

	ret = snd_soc_card_jack_new_pins(rtd->card, "HDMI Jack", SND_JACK_LINEOUT,
					 jack, mt8189_hdmi_jack_pins,
					 ARRAY_SIZE(mt8189_hdmi_jack_pins));
	if (ret) {
		dev_err(rtd->dev, "%s, new jack failed: %d\n", __func__, ret);
		return ret;
	}

	ret = snd_soc_component_set_jack(component, jack, NULL);
	if (ret) {
		dev_err(rtd->dev, "%s, set jack failed on %s (ret=%d)\n",
			__func__, component->name, ret);
		return ret;
	}

	return 0;
}

static int mt8189_headset_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(card);
	struct snd_soc_jack *jack = &soc_card_data->card_data->jacks[MT8189_JACK_HEADSET];
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;
	int ret;
	int type;

	ret = snd_soc_dapm_new_controls(&card->dapm, mt8189_nau8825_widgets,
					ARRAY_SIZE(mt8189_nau8825_widgets));
	if (ret) {
		dev_err(rtd->dev, "unable to add nau8825 card widget, ret %d\n", ret);
		return ret;
	}

	ret = snd_soc_add_card_controls(card, mt8189_nau8825_controls,
					ARRAY_SIZE(mt8189_nau8825_controls));
	if (ret) {
		dev_err(rtd->dev, "unable to add nau8825 card controls, ret %d\n", ret);
		return ret;
	}

	ret = snd_soc_card_jack_new_pins(rtd->card, "Headset Jack",
					 SND_JACK_HEADSET | SND_JACK_BTN_0 |
					 SND_JACK_BTN_1 | SND_JACK_BTN_2 |
					 SND_JACK_BTN_3,
					 jack,
					 nau8825_jack_pins,
					 ARRAY_SIZE(nau8825_jack_pins));
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	type = SND_JACK_HEADSET | SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 | SND_JACK_BTN_3;
	ret = snd_soc_component_set_jack(component, jack, (void *)&type);

	if (ret) {
		dev_err(rtd->dev, "Headset Jack call-back failed: %d\n", ret);
		return ret;
	}

	return 0;
};

static void mt8189_headset_codec_exit(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = snd_soc_rtd_to_codec(rtd, 0)->component;

	snd_soc_component_set_jack(component, NULL, NULL);
}

static int mt8189_nau8825_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	unsigned int rate = params_rate(params);
	unsigned int bit_width = params_width(params);
	int clk_freq, ret;

	clk_freq = rate * 2 * bit_width;
	dev_dbg(codec_dai->dev, "clk_freq %d, rate: %d, bit_width: %d\n",
		clk_freq, rate, bit_width);

	/* Configure clock for codec */
	ret = snd_soc_dai_set_sysclk(codec_dai, NAU8825_CLK_FLL_BLK, 0,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "can't set BCLK clock %d\n", ret);
		return ret;
	}

	/* Configure pll for codec */
	ret = snd_soc_dai_set_pll(codec_dai, 0, 0, clk_freq,
				  params_rate(params) * 256);
	if (ret < 0) {
		dev_err(codec_dai->dev, "can't set BCLK: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_ops mt8189_nau8825_ops = {
	.hw_params = mt8189_nau8825_hw_params,
};

static int mt8189_headset_i2s_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	unsigned int rate = params_rate(params);
	int bitwidth;
	int ret;

	bitwidth = snd_pcm_format_width(params_format(params));
	if (bitwidth < 0) {
		dev_err(card->dev, "invalid bit width: %d\n", bitwidth);
		return bitwidth;
	}

	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0x00, 0x0, 0x2, bitwidth);
	if (ret) {
		dev_err(card->dev, "failed to set tdm slot\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, 1, rate * 32, rate * 512);
	if (ret) {
		dev_err(card->dev, "failed to set pll\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, 1, rate * 512, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(card->dev, "failed to set sysclk\n");
		return ret;
	}

	return snd_soc_dai_set_sysclk(cpu_dai, 0, rate * 512,
				      SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt8189_headset_i2s_ops = {
	.hw_params = mt8189_headset_i2s_hw_params,
};

static int mt8189_mt6359p_soc_card_probe(struct mtk_soc_card_data *soc_card_data, bool legacy)
{
	struct snd_soc_card *card = soc_card_data->card_data->card;
	struct snd_soc_dai_link *dai_link;
	bool init_nau8825 = false;
	bool init_rt5682s = false;
	bool init_rt5650 = false;
	bool init_rt5682i = false;
	bool init_dumb = false;
	int i;

	dev_dbg(card->dev, "%s(), legacy: %d\n", __func__, legacy);

	for_each_card_prelinks(card, i, dai_link) {
		if (strcmp(dai_link->name, "TDM_DPTX_BE") == 0) {
			if (dai_link->num_codecs &&
			    strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai"))
				dai_link->init = mt8189_dptx_codec_init;
		} else if (strcmp(dai_link->name, "PCM_0_BE") == 0) {
			if (dai_link->num_codecs &&
			    strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai"))
				dai_link->init = mt8189_hdmi_codec_init;
		} else if (strcmp(dai_link->name, "I2SOUT0_BE") == 0 ||
			   strcmp(dai_link->name, "I2SIN0_BE") == 0) {
			if (!strcmp(dai_link->codecs->dai_name, NAU8825_CODEC_DAI)) {
				dai_link->ops = &mt8189_nau8825_ops;
				if (!init_nau8825) {
					dai_link->init = mt8189_headset_codec_init;
					dai_link->exit = mt8189_headset_codec_exit;
					init_nau8825 = true;
				}
			} else if (!strcmp(dai_link->codecs->dai_name, RT5682S_CODEC_DAI)) {
				dai_link->ops = &mt8189_headset_i2s_ops;
				if (!init_rt5682s) {
					dai_link->init = mt8189_headset_codec_init;
					dai_link->exit = mt8189_headset_codec_exit;
					init_rt5682s = true;
				}
			} else if (!strcmp(dai_link->codecs->dai_name, RT5650_CODEC_DAI)) {
				dai_link->ops = &mt8189_headset_i2s_ops;
				if (!init_rt5650) {
					dai_link->init = mt8189_headset_codec_init;
					dai_link->exit = mt8189_headset_codec_exit;
					init_rt5650 = true;
				}
			} else if (!strcmp(dai_link->codecs->dai_name, RT5682I_CODEC_DAI)) {
				dai_link->ops = &mt8189_headset_i2s_ops;
				if (!init_rt5682i) {
					dai_link->init = mt8189_headset_codec_init;
					dai_link->exit = mt8189_headset_codec_exit;
					init_rt5682i = true;
				}
			} else {
				if (strcmp(dai_link->codecs->dai_name, "snd-soc-dummy-dai")) {
					if (!init_dumb) {
						dai_link->init = mt8189_dumb_amp_init;
						init_dumb = true;
					}
				}
			}
		}
	}

	return 0;
}

static struct snd_soc_card mt8189_mt6359p_soc_card = {
	.owner = THIS_MODULE,
	.dai_link = mt8189_mt6359p_dai_links,
	.num_links = ARRAY_SIZE(mt8189_mt6359p_dai_links),
	.dapm_widgets = mt8189_mt6359p_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8189_mt6359p_widgets),
	.dapm_routes = mt8189_mt6359p_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8189_mt6359p_routes),
	.controls = mt8189_mt6359p_controls,
	.num_controls = ARRAY_SIZE(mt8189_mt6359p_controls),
};


static const struct mtk_soundcard_pdata mt8189_mt6359p_card = {
	.card_name = "mt8189-mt6359p",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8189_mt6359p_soc_card,
		.num_jacks = MT8189_JACK_MAX,
	},
	.sof_priv = NULL,
	.soc_probe = mt8189_mt6359p_soc_card_probe,
};

static const struct mtk_soundcard_pdata mt8189_nau8825_card = {
	.card_name = "mt8189_nau8825",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8189_mt6359p_soc_card,
		.num_jacks = MT8189_JACK_MAX,
		.flags = NAU8825_HS_PRESENT
	},
	.sof_priv = NULL,
	.soc_probe = mt8189_mt6359p_soc_card_probe,
};

static const struct mtk_soundcard_pdata mt8189_rt5650_card = {
	.card_name = "mt8189_rt5650",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8189_mt6359p_soc_card,
		.num_jacks = MT8189_JACK_MAX,
		.flags = RT5650_HS_PRESENT
	},
	.sof_priv = NULL,
	.soc_probe = mt8189_mt6359p_soc_card_probe,
};

static const struct mtk_soundcard_pdata mt8189_rt5682s_card = {
	.card_name = "mt8189_rt5682s",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8189_mt6359p_soc_card,
		.num_jacks = MT8189_JACK_MAX,
		.flags = RT5682S_HS_PRESENT
	},
	.sof_priv = NULL,
	.soc_probe = mt8189_mt6359p_soc_card_probe,
};

static const struct mtk_soundcard_pdata mt8189_rt5682i_card = {
	.card_name = "mt8189_rt5682i",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8189_mt6359p_soc_card,
		.num_jacks = MT8189_JACK_MAX,
		.flags = RT5682I_HS_PRESENT
	},
	.sof_priv = NULL,
	.soc_probe = mt8189_mt6359p_soc_card_probe,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt8189_mt6359p_dt_match[] = {
	{.compatible = "mediatek,mt8189-mt6359p-sound", .data = &mt8189_mt6359p_card,},
	{.compatible = "mediatek,mt8189-nau8825-sound", .data = &mt8189_nau8825_card,},
	{.compatible = "mediatek,mt8189-rt5650-sound", .data = &mt8189_rt5650_card,},
	{.compatible = "mediatek,mt8189-rt5682s-sound", .data = &mt8189_rt5682s_card,},
	{.compatible = "mediatek,mt8189-rt5682i-sound", .data = &mt8189_rt5682i_card,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8189_mt6359p_dt_match);
#endif

static struct platform_driver mt8189_mt6359p_driver = {
	.driver = {
		.name = "mt8189-mt6359p",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = mt8189_mt6359p_dt_match,
#endif
		.pm = &snd_soc_pm_ops,
	},
	.probe = mtk_soundcard_common_probe,
};

module_platform_driver(mt8189_mt6359p_driver);


/* Module information */
MODULE_DESCRIPTION("MT8189 MT6359p ALSA SoC machine driver");
MODULE_AUTHOR("Darren Ye <darren.ye@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mt8189 mt6359p soc card");
