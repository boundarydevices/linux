/*
 * mt8365-evk.c  --  MT8365 EVK machine driver
 *
 * Copyright (c) 2021 Baylibre SAS
 * Author: Nicolas Belin <nbelin@baylibre.com>
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

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8365-afe-common.h"
#include "../common/mtk-soundcard-driver.h"

#define PREFIX	"mediatek,"

enum PINCTRL_PIN_STATE {
	PIN_STATE_MOSI_ON = 0,
	PIN_STATE_MOSI_OFF,
	PIN_STATE_MISO_ON,
	PIN_STATE_MISO_OFF,
	PIN_STATE_DEFAULT,
	PIN_STATE_DMIC,
	PIN_STATE_MAX
};

static const char * const mt8365_evk_pin_str[PIN_STATE_MAX] = {
	"aud_mosi_on",
	"aud_mosi_off",
	"aud_miso_on",
	"aud_miso_off",
	"default",
	"aud_dmic",
};

struct mt8365_evk_dmic_ctrl_data {
	unsigned int fix_rate;
	unsigned int fix_channels;
	unsigned int fix_bit_width;
};

struct mt8365_evk_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
	struct mt8365_evk_dmic_ctrl_data dmic_data;
};

struct mt8365_dai_link_prop {
	char *name;
	unsigned int link_id;
};

enum {
	/* FE */
	DAI_LINK_DL1_PLAYBACK = 0,
	DAI_LINK_DL2_PLAYBACK,
	DAI_LINK_AWB_CAPTURE,
	DAI_LINK_VUL_CAPTURE,
	/* BE */
	DAI_LINK_2ND_I2S_INTF,
	DAI_LINK_DMIC,
	DAI_LINK_INT_ADDA,
	DAI_LINK_NUM
};

static const struct snd_soc_dapm_widget mt8365_evk_widgets[] = {
	SND_SOC_DAPM_MIC("PMIC MIC", NULL),
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_OUTPUT("HDMI Out"),
};

static const struct snd_soc_dapm_route mt8365_evk_routes[] = {
	{"Headphone", NULL, "MT6357 Playback"},
	{"MT6357 Capture", NULL, "PMIC MIC"},
	{"HDMI Out", NULL, "2ND I2S Playback"},
};

static int mt8365_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	struct mt8365_evk_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int id = rtd->dai_link->id;
	struct mt8365_evk_dmic_ctrl_data *dmic;
	unsigned int fix_rate = 0;
	unsigned int fix_bit_width = 0;
	unsigned int fix_channels = 0;

	if (id == DAI_LINK_DMIC) {
		dmic = &priv->dmic_data;
		fix_rate = dmic->fix_rate;
		fix_bit_width = dmic->fix_bit_width;
		fix_channels = dmic->fix_channels;
	}

	if (fix_rate > 0) {
		struct snd_interval *rate =
			hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

		rate->max = rate->min = fix_rate;
	}

	if (fix_bit_width > 0) {
		struct snd_mask *mask =
			hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

		if (fix_bit_width == 32) {
			snd_mask_none(mask);
			snd_mask_set(mask, SNDRV_PCM_FORMAT_S32_LE);
		} else if (fix_bit_width == 16) {
			snd_mask_none(mask);
			snd_mask_set(mask, SNDRV_PCM_FORMAT_S16_LE);
		}
	}

	if (fix_channels > 0) {
		struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

		channels->min = channels->max = fix_channels;
	}

	return 0;
}

static int mt8365_evk_int_adda_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8365_evk_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (IS_ERR(priv->pin_states[PIN_STATE_MOSI_ON]))
			return ret;

		ret = pinctrl_select_state(priv->pinctrl,
					priv->pin_states[PIN_STATE_MOSI_ON]);
		if (ret)
			dev_err(rtd->card->dev, "%s failed to select state %d\n",
				__func__, ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (IS_ERR(priv->pin_states[PIN_STATE_MISO_ON]))
			return ret;

		ret = pinctrl_select_state(priv->pinctrl,
					priv->pin_states[PIN_STATE_MISO_ON]);
		if (ret)
			dev_err(rtd->card->dev, "%s failed to select state %d\n",
				__func__, ret);
	}

	return 0;
}

static void mt8365_evk_int_adda_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8365_evk_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (IS_ERR(priv->pin_states[PIN_STATE_MOSI_OFF]))
			return;

		ret = pinctrl_select_state(priv->pinctrl,
					priv->pin_states[PIN_STATE_MOSI_OFF]);
		if (ret)
			dev_err(rtd->card->dev, "%s failed to select state %d\n",
				__func__, ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (IS_ERR(priv->pin_states[PIN_STATE_MISO_OFF]))
			return;

		ret = pinctrl_select_state(priv->pinctrl,
					priv->pin_states[PIN_STATE_MISO_OFF]);
		if (ret)
			dev_err(rtd->card->dev, "%s failed to select state %d\n",
				__func__, ret);
	}

}

static struct snd_soc_ops mt8365_evk_int_adda_ops = {
	.startup = mt8365_evk_int_adda_startup,
	.shutdown = mt8365_evk_int_adda_shutdown,
};

SND_SOC_DAILINK_DEFS(playback1,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(awb_capture,
	DAILINK_COMP_ARRAY(COMP_CPU("AWB")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(vul,
	DAILINK_COMP_ARRAY(COMP_CPU("VUL")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s3,
	DAILINK_COMP_ARRAY(COMP_CPU("2ND I2S")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(dmic,
	DAILINK_COMP_ARRAY(COMP_CPU("DMIC")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(primary_codec,
	DAILINK_COMP_ARRAY(COMP_CPU("INT ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC("mt-soc-codec", "mt6357-codec-dai")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8365_evk_dais[] = {
	/* Front End DAI links */
	[DAI_LINK_DL1_PLAYBACK] = {
		.name = "DL1_FE",
		.stream_name = "MultiMedia1_PLayback",
		.id = DAI_LINK_DL1_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_merged_rate = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	[DAI_LINK_DL2_PLAYBACK] = {
		.name = "DL2_FE",
		.stream_name = "MultiMedia2_PLayback",
		.id = DAI_LINK_DL2_PLAYBACK,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_merged_rate = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	[DAI_LINK_AWB_CAPTURE] = {
		.name = "AWB_FE",
		.stream_name = "DL1_AWB_Record",
		.id = DAI_LINK_AWB_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_merged_rate = 1,
		SND_SOC_DAILINK_REG(awb_capture),
	},
	[DAI_LINK_VUL_CAPTURE] = {
		.name = "VUL_FE",
		.stream_name = "MultiMedia1_Capture",
		.id = DAI_LINK_VUL_CAPTURE,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_merged_rate = 1,
		SND_SOC_DAILINK_REG(vul),
	},
	/* Back End DAI links */
	[DAI_LINK_2ND_I2S_INTF] = {
		.name = "2ND I2S BE",
		.no_pcm = 1,
		.id = DAI_LINK_2ND_I2S_INTF,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(i2s3),
	},
	[DAI_LINK_DMIC] = {
		.name = "DMIC BE",
		.no_pcm = 1,
		.id = DAI_LINK_DMIC,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(dmic),
	},
	[DAI_LINK_INT_ADDA] = {
		.name = "MTK Codec",
		.no_pcm = 1,
		.id = DAI_LINK_INT_ADDA,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &mt8365_evk_int_adda_ops,
		SND_SOC_DAILINK_REG(primary_codec),
	},
};

static int mt8365_evk_gpio_probe(struct snd_soc_card *card)
{
	struct mt8365_evk_priv *priv = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int i;

	priv->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(priv->pinctrl)) {
		ret = PTR_ERR(priv->pinctrl);
		dev_err(card->dev, "%s devm_pinctrl_get failed %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0 ; i < PIN_STATE_MAX ; i++) {
		priv->pin_states[i] = pinctrl_lookup_state(priv->pinctrl,
			mt8365_evk_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_info(card->dev, "%s Can't find pin state %s %d\n",
				 __func__, mt8365_evk_pin_str[i], ret);
		}
	}

	for (i = PIN_STATE_DEFAULT ; i < PIN_STATE_MAX ; i++) {
		if (IS_ERR(priv->pin_states[i])) {
			dev_info(card->dev, "%s check pin %s state err\n",
				 __func__, mt8365_evk_pin_str[i]);
			continue;
		}

		/* default state */
		ret = pinctrl_select_state(priv->pinctrl,
				priv->pin_states[i]);
		if (ret)
			dev_info(card->dev, "%s failed to select state %d\n",
				__func__, ret);
	}

	/* turn off mosi pin if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_MOSI_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
				priv->pin_states[PIN_STATE_MOSI_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select state %d\n",
				__func__, ret);
	}

	/* turn off miso pin if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_MISO_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
				priv->pin_states[PIN_STATE_MISO_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select state %d\n",
				__func__, ret);
	}

	return ret;
}

static int link_to_dai(int link_id)
{
	switch (link_id) {
	case DAI_LINK_DMIC:
		return MT8365_AFE_IO_DMIC;
	default:
		break;
	}
	return -1;
}

static void mt8365_evk_parse_of(struct snd_soc_card *card,
	struct device_node *np)
{
	struct mt8365_evk_priv *priv = snd_soc_card_get_drvdata(card);
	size_t i;
	int ret;
	char prop[128];
	unsigned int val;

	static const struct mt8365_dai_link_prop of_dai_links_io[] = {
		{ "dmic",	DAI_LINK_DMIC },
	};

	for (i = 0; i < ARRAY_SIZE(of_dai_links_io); i++) {
		unsigned int link_id = of_dai_links_io[i].link_id;
		struct snd_soc_dai_link *dai_link = &mt8365_evk_dais[link_id];
		struct mt8365_evk_dmic_ctrl_data *dmic;

		/* parse fix rate */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-rate",
			 of_dai_links_io[i].name);
		ret = of_property_read_u32(np, prop, &val);

		if (ret == 0 && mt8365_afe_rate_supported(val,
			link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_rate = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8365_be_hw_params_fixup;
		}

		/* parse fix bit width */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-bit-width",
			 of_dai_links_io[i].name);
		ret = of_property_read_u32(np, prop, &val);
		if (ret == 0 && (val == 32 || val == 16)) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_bit_width = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8365_be_hw_params_fixup;
		}

		/* parse fix channels */
		snprintf(prop, sizeof(prop), PREFIX"%s-fix-channels",
			 of_dai_links_io[i].name);
		ret = of_property_read_u32(np, prop, &val);

		if (ret == 0 && mt8365_afe_channel_supported(val,
			link_to_dai(link_id))) {
			switch (link_id) {
			case DAI_LINK_DMIC:
				dmic = &priv->dmic_data;
				dmic->fix_channels = val;
				break;
			default:
				break;
			}

			dai_link->be_hw_params_fixup =
				mt8365_be_hw_params_fixup;
		}
	}
}

static struct snd_soc_card mt8365_evk_card = {
	.name = "mt8365-evk",
	.owner = THIS_MODULE,
	.dai_link = mt8365_evk_dais,
	.num_links = ARRAY_SIZE(mt8365_evk_dais),
	.dapm_widgets = mt8365_evk_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8365_evk_widgets),
	.dapm_routes = mt8365_evk_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8365_evk_routes),
};

static int mt8365_evk_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8365_evk_card;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *platform_node;
	struct mt8365_evk_priv *priv;
	int i, ret;

	card->dev = dev;
	ret = set_card_codec_info(card);
	if (ret) {
		return dev_err_probe(&pdev->dev, ret, "%s set_card_codec_info failed\n",
				    __func__);
	}

	platform_node = of_parse_phandle(dev->of_node, "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < card->num_links; i++) {
		if (mt8365_evk_dais[i].platforms->name)
			continue;
		mt8365_evk_dais[i].platforms->of_node = platform_node;
	}

	priv = devm_kzalloc(dev, sizeof(struct mt8365_evk_priv),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_err(dev, "%s allocate card private data fail %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, priv);

	mt8365_evk_gpio_probe(card);

	mt8365_evk_parse_of(card, np);

	ret = devm_snd_soc_register_card(dev, card);
	if (ret)
		dev_err(dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);

	return ret;
}

static const struct of_device_id mt8365_evk_dt_match[] = {
	{ .compatible = "mediatek,mt8365-evk", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8365_evk_dt_match);

static struct platform_driver mt8365_evk_driver = {
	.driver = {
		   .name = "mt8365-evk",
		   .of_match_table = mt8365_evk_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8365_evk_dev_probe,
};

module_platform_driver(mt8365_evk_driver);

/* Module information */
MODULE_DESCRIPTION("MT8365 EVK SoC machine driver");
MODULE_AUTHOR("Nicolas Belin <nbelin@baylibre.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt8365-evk");
