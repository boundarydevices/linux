/*
 * mt8365-pumpkin.c  --  MT8365 PUMPKIN machine driver
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

#define PREFIX	"mediatek,"

enum PINCTRL_PIN_STATE {
	PIN_STATE_MOSI_ON = 0,
	PIN_STATE_MOSI_OFF,
	PIN_STATE_MISO_ON,
	PIN_STATE_MISO_OFF,
	PIN_STATE_DEFAULT,
	PIN_STATE_I2S1,
	PIN_STATE_TDM_OUT,
	PIN_STATE_MAX
};

static const char * const mt8365_pumpkin_pin_str[PIN_STATE_MAX] = {
	"aud_mosi_on",
	"aud_mosi_off",
	"aud_miso_on",
	"aud_miso_off",
	"default",
	"aud_i2s1",
	"aud_tdm_out",
};

struct mt8365_pumpkin_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
};

enum {
	/* FE */
	DAI_LINK_DL1_PLAYBACK = 0,
	DAI_LINK_DL2_PLAYBACK,
	DAI_LINK_AWB_CAPTURE,
	DAI_LINK_VUL_CAPTURE,
	DAI_LINK_TDM_OUT,
	/* BE */
	DAI_LINK_I2S_INTF,
	DAI_LINK_INT_ADDA,
	DAI_LINK_TDM_OUT_IO,
	DAI_LINK_NUM
};

static const struct snd_soc_dapm_widget mt8365_pumpkin_widgets[] = {
	SND_SOC_DAPM_MIC("PMIC MIC", NULL),
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_OUTPUT("TDM_OUT Out"),
	SND_SOC_DAPM_OUTPUT("I2S1 Out"),
};

static const struct snd_soc_dapm_route mt8365_pumpkin_routes[] = {
	{"Headphone", NULL, "MT6357 Playback"},
	{"MT6357 Capture", NULL, "PMIC MIC"},
	{"TDM_OUT Out", NULL, "TDM_OUT Playback"},
	{"I2S1 Out", NULL, "I2S Playback"},

};

static int mt8365_pumpkin_int_adda_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8365_pumpkin_priv *priv = snd_soc_card_get_drvdata(rtd->card);
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

static void mt8365_pumpkin_int_adda_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8365_pumpkin_priv *priv = snd_soc_card_get_drvdata(rtd->card);
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

static struct snd_soc_ops mt8365_pumpkin_int_adda_ops = {
	.startup = mt8365_pumpkin_int_adda_startup,
	.shutdown = mt8365_pumpkin_int_adda_shutdown,
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
SND_SOC_DAILINK_DEFS(tdm_out_fe,
	DAILINK_COMP_ARRAY(COMP_CPU("TDM_OUT")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(primary_codec,
	DAILINK_COMP_ARRAY(COMP_CPU("INT ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC("mt-soc-codec", "mt6357-codec-dai")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
	SND_SOC_DAILINK_DEFS(tdm_out_io,
	DAILINK_COMP_ARRAY(COMP_CPU("TDM_OUT_IO")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));


/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8365_pumpkin_dais[] = {
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
		SND_SOC_DAILINK_REG(vul),
	},
	[DAI_LINK_TDM_OUT] = {
		.name = "TDM_OUT_FE",
		.stream_name = "TDM_Playback",
		.id = DAI_LINK_TDM_OUT,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(tdm_out_fe),
	},

	/* Back End DAI links */
		[DAI_LINK_I2S_INTF] = {
		.name = "I2S BE",
		.no_pcm = 1,
		.id = DAI_LINK_I2S_INTF,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(i2s1),
	},
	[DAI_LINK_INT_ADDA] = {
		.name = "MTK Codec",
		.no_pcm = 1,
		.id = DAI_LINK_INT_ADDA,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &mt8365_pumpkin_int_adda_ops,
		SND_SOC_DAILINK_REG(primary_codec),
	},
	[DAI_LINK_TDM_OUT_IO] = {
		.name = "TDM_OUT BE",
		.no_pcm = 1,
		.id = DAI_LINK_TDM_OUT_IO,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(tdm_out_io),
	},

};

static int mt8365_pumpkin_gpio_probe(struct snd_soc_card *card)
{
	struct mt8365_pumpkin_priv *priv = snd_soc_card_get_drvdata(card);
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
			mt8365_pumpkin_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_info(card->dev, "%s Can't find pin state %s %d\n",
				 __func__, mt8365_pumpkin_pin_str[i], ret);
		}
	}

	for (i = PIN_STATE_DEFAULT ; i < PIN_STATE_MAX ; i++) {
		if (IS_ERR(priv->pin_states[i])) {
			dev_info(card->dev, "%s check pin %s state err\n",
				 __func__, mt8365_pumpkin_pin_str[i]);
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

static struct snd_soc_card mt8365_pumpkin_card = {
	.name = "mt-snd-card",
	.owner = THIS_MODULE,
	.dai_link = mt8365_pumpkin_dais,
	.num_links = ARRAY_SIZE(mt8365_pumpkin_dais),
	.dapm_widgets = mt8365_pumpkin_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8365_pumpkin_widgets),
	.dapm_routes = mt8365_pumpkin_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8365_pumpkin_routes),
};

static int mt8365_pumpkin_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8365_pumpkin_card;
	struct device *dev = &pdev->dev;
	struct device_node *platform_node;
	struct mt8365_pumpkin_priv *priv;
	int i, ret;

	platform_node = of_parse_phandle(dev->of_node, "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < card->num_links; i++) {
		if (mt8365_pumpkin_dais[i].platforms->name)
			continue;
		mt8365_pumpkin_dais[i].platforms->of_node = platform_node;
	}

	card->dev = dev;

	priv = devm_kzalloc(dev, sizeof(struct mt8365_pumpkin_priv),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_err(dev, "%s allocate card private data fail %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, priv);

	mt8365_pumpkin_gpio_probe(card);

	ret = devm_snd_soc_register_card(dev, card);
	if (ret)
		dev_err(dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);

	return ret;
}

static const struct of_device_id mt8365_pumpkin_dt_match[] = {
	{ .compatible = "mediatek,mt8365-pumpkin", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8365_pumpkin_dt_match);

static struct platform_driver mt8365_pumpkin_driver = {
	.driver = {
		   .name = "mt8365-pumpkin",
		   .of_match_table = mt8365_pumpkin_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8365_pumpkin_dev_probe,
};

module_platform_driver(mt8365_pumpkin_driver);

/* Module information */
MODULE_DESCRIPTION("MT8365 PUMPKIN SoC machine driver");
MODULE_AUTHOR("Nicolas Belin <nbelin@baylibre.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mt8365-pumpkin");
