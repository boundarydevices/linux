// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek MT8365 Sound Card driver
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Authors: Nicolas Belin <nbelin@baylibre.com>
 *          Aary Patil <aary.patil@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8365-afe-common.h"
#include <linux/pm_runtime.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include "../common/mtk-soc-card.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-soundcard-driver.h"
#include "../common/mtk-dsp-sof-common.h"

#define SOF_DMA_DL1 "SOF_DMA_DL1"
#define SOF_DMA_DL2 "SOF_DMA_DL2"
#define SOF_DMA_AWB "SOF_DMA_AWB"
#define SOF_DMA_VUL "SOF_DMA_VUL"

enum pinctrl_pin_state {
	PIN_STATE_DEFAULT,
	PIN_STATE_DMIC,
	PIN_STATE_MISO_OFF,
	PIN_STATE_MISO_ON,
	PIN_STATE_MOSI_OFF,
	PIN_STATE_MOSI_ON,
	PIN_STATE_MAX
};

static const char * const mt8365_mt6357_pin_str[PIN_STATE_MAX] = {
	"default",
	"dmic",
	"miso_off",
	"miso_on",
	"mosi_off",
	"mosi_on",
};

struct mt8365_mt6357_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
};

struct mt8365_card_data {
	const char *name;
	unsigned long quirk;
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
	DAI_LINK_NUM,
	DAI_LINK_REGULAR_LAST = DAI_LINK_NUM - 1,
	DAI_LINK_SOF_START,
	DAI_LINK_SOF_DL1_BE = DAI_LINK_SOF_START,
	DAI_LINK_SOF_DL2_BE,
	DAI_LINK_SOF_AWB_BE,
	DAI_LINK_SOF_VUL_BE,
	DAI_LINK_SOF_END = DAI_LINK_SOF_VUL_BE,
};

#define	DAI_LINK_REGULAR_NUM	(DAI_LINK_REGULAR_LAST + 1)

static const struct snd_soc_dapm_widget mt8365_mt6357_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HDMI Out"),
	SND_SOC_DAPM_MIXER(SOF_DMA_DL1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_DL2, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_AWB, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER(SOF_DMA_VUL, SND_SOC_NOPM, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route mt8365_mt6357_routes[] = {
	{"HDMI Out", NULL, "2ND I2S Playback"},
	{"DMIC In", NULL, "MICBIAS0"},
	{SOF_DMA_AWB, NULL, "O05"},
	{SOF_DMA_AWB, NULL, "O06"},
	{SOF_DMA_VUL, NULL, "O09"},
	{SOF_DMA_VUL, NULL, "O10"},
	{"I05", NULL, SOF_DMA_DL1},
	{"I06", NULL, SOF_DMA_DL1},
	{"I07", NULL, SOF_DMA_DL2},
	{"I08", NULL, SOF_DMA_DL2},
};

static int mt8365_mt6357_int_adda_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct mt8365_mt6357_priv *priv = soc_card_data->mach_priv;
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

static void mt8365_mt6357_int_adda_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(rtd->card);
	struct mt8365_mt6357_priv *priv = soc_card_data->mach_priv;
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

static const struct snd_soc_ops mt8365_mt6357_int_adda_ops = {
	.startup = mt8365_mt6357_int_adda_startup,
	.shutdown = mt8365_mt6357_int_adda_shutdown,
};

static int mt8365_sof_be_hw_params(struct snd_pcm_substream *substream,
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

static const struct snd_soc_ops mt8365_sof_be_ops = {
	.hw_params = mt8365_sof_be_hw_params,
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
		     DAILINK_COMP_ARRAY(COMP_CODEC("mt6357-sound", "mt6357-snd-codec-aif1")),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* SOF */
SND_SOC_DAILINK_DEFS(AFE_SOF_DL1,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_DL2,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_AWB,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_AWB")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(AFE_SOF_VUL,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_VUL")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8365_mt6357_dais[] = {
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
		.name = "I2S_OUT_BE",
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
		.name = "DMIC_BE",
		.no_pcm = 1,
		.id = DAI_LINK_DMIC,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(dmic),
	},
	[DAI_LINK_INT_ADDA] = {
		.name = "MTK_Codec",
		.no_pcm = 1,
		.id = DAI_LINK_INT_ADDA,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &mt8365_mt6357_int_adda_ops,
		SND_SOC_DAILINK_REG(primary_codec),
	},
	/* SOF DAI links */
	[DAI_LINK_SOF_DL1_BE] = {
		.name = "AFE_SOF_DL1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &mt8365_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_DL1),
	},
	[DAI_LINK_SOF_DL2_BE] = {
		.name = "AFE_SOF_DL2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &mt8365_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_DL2),
	},
	[DAI_LINK_SOF_AWB_BE] = {
		.name = "AFE_SOF_AWB",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ops = &mt8365_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_AWB),
	},
	[DAI_LINK_SOF_VUL_BE] = {
		.name = "AFE_SOF_VUL",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ops = &mt8365_sof_be_ops,
		SND_SOC_DAILINK_REG(AFE_SOF_VUL),
	},
};

static const struct sof_conn_stream g_sof_conn_streams[] = {
	{
		.sof_link = "AFE_SOF_DL1",
		.sof_dma = SOF_DMA_DL1,
		.stream_dir = SNDRV_PCM_STREAM_PLAYBACK
	},
	{
		.sof_link = "AFE_SOF_DL2",
		.sof_dma = SOF_DMA_DL2,
		.stream_dir = SNDRV_PCM_STREAM_PLAYBACK
	},
	{
		.sof_link = "AFE_SOF_AWB",
		.sof_dma = SOF_DMA_AWB,
		.stream_dir = SNDRV_PCM_STREAM_CAPTURE
	},
	{
		.sof_link = "AFE_SOF_VUL",
		.sof_dma = SOF_DMA_VUL,
		.stream_dir = SNDRV_PCM_STREAM_CAPTURE
	},
};

static int mt8365_mt6357_gpio_probe(struct snd_soc_card *card)
{
	struct mtk_soc_card_data *soc_card_data = snd_soc_card_get_drvdata(card);
	struct mt8365_mt6357_priv *priv = soc_card_data->mach_priv;
	int ret, i;

	priv->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(priv->pinctrl)) {
		ret = PTR_ERR(priv->pinctrl);
		return dev_err_probe(card->dev, ret,
				     "Failed to get pinctrl\n");
	}

	for (i = PIN_STATE_DEFAULT ; i < PIN_STATE_MAX ; i++) {
		priv->pin_states[i] = pinctrl_lookup_state(priv->pinctrl,
							   mt8365_mt6357_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_warn(card->dev, "No pin state for %s\n",
				 mt8365_mt6357_pin_str[i]);
		} else {
			ret = pinctrl_select_state(priv->pinctrl,
						   priv->pin_states[i]);
			if (ret) {
				dev_err_probe(card->dev, ret,
					      "Failed to select pin state %s\n",
					      mt8365_mt6357_pin_str[i]);
				return ret;
			}
		}
	}
	return 0;
}

static struct snd_soc_card mt8365_mt6357_soc_card = {
	.owner = THIS_MODULE,
	.dai_link = mt8365_mt6357_dais,
	.num_links = ARRAY_SIZE(mt8365_mt6357_dais),
	.dapm_widgets = mt8365_mt6357_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8365_mt6357_widgets),
	.dapm_routes = mt8365_mt6357_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8365_mt6357_routes),
};

static int mt8365_mt6357_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8365_mt6357_soc_card;
	struct snd_soc_dai_link *dai_link;
	struct mtk_soc_card_data *soc_card_data;
	struct mt8365_mt6357_priv *mach_priv;
	struct device_node *platform_node, *adsp_node;
	struct mt8365_card_data *card_data;
	int sof_on = 0;
	int ret, i;

	card_data = (struct mt8365_card_data *)of_device_get_match_data(&pdev->dev);
	card->dev = &pdev->dev;

	ret = parse_dai_link_info(card);
	if (ret) {
		return dev_err_probe(&pdev->dev, ret, "%s parse_dai_link_info failed\n",
				     __func__);
	}

	ret = snd_soc_of_parse_card_name(card, "model");
	if (ret) {
		dev_err(&pdev->dev, "%s new card name parsing error %d\n",
			__func__, ret);
		return ret;
	}

	if (!card->name)
		card->name = card_data->name;

	soc_card_data = devm_kzalloc(&pdev->dev, sizeof(*soc_card_data), GFP_KERNEL);
	if (!soc_card_data)
		return -ENOMEM;

	mach_priv = devm_kzalloc(&pdev->dev, sizeof(*mach_priv), GFP_KERNEL);
	if (!mach_priv)
		return -ENOMEM;

	soc_card_data->mach_priv = mach_priv;

	adsp_node = of_parse_phandle(pdev->dev.of_node, "mediatek,adsp", 0);
	if (adsp_node) {
		struct mtk_sof_priv *sof_priv;

		sof_priv = devm_kzalloc(&pdev->dev, sizeof(*sof_priv), GFP_KERNEL);
		if (!sof_priv) {
			ret = -ENOMEM;
			goto err_kzalloc;
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
		sof_on = 1;
	}

	if (of_property_read_bool(pdev->dev.of_node, "mediatek,dai-link")) {
		ret = mtk_sof_dailink_parse_of(card, pdev->dev.of_node,
					       "mediatek,dai-link",
					       mt8365_mt6357_dais,
					       ARRAY_SIZE(mt8365_mt6357_dais));
		if (ret) {
			dev_dbg(&pdev->dev, "Parse dai-link fail\n");
			goto err_parse_of;
		}
	} else {
		if (!sof_on)
			card->num_links = DAI_LINK_REGULAR_NUM;
	}

	platform_node = of_parse_phandle(pdev->dev.of_node, "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
		goto err_platform_node;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (!dai_link->platforms->name) {
			if (!strncmp(dai_link->name, "AFE_SOF", strlen("AFE_SOF")) && sof_on)
				dai_link->platforms->of_node = adsp_node;
			else
				dai_link->platforms->of_node = platform_node;
		}
	}

	snd_soc_card_set_drvdata(card, soc_card_data);

	mt8365_mt6357_gpio_probe(card);

	ret = devm_snd_soc_register_card(&pdev->dev, card);

	of_node_put(platform_node);

err_kzalloc:
err_parse_of:
err_platform_node:
	of_node_put(adsp_node);
	return ret;
}

static struct mt8365_card_data mt8365_mt6357_card = {
	.name = "mt8365_mt6357",
};

static const struct of_device_id mt8365_mt6357_dt_match[] = {
	{
		.compatible = "mediatek,mt8365-mt6357",
		.data = &mt8365_mt6357_card,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mt8365_mt6357_dt_match);

static struct platform_driver mt8365_mt6357_driver = {
	.driver = {
		   .name = "mt8365_mt6357",
		   .of_match_table = mt8365_mt6357_dt_match,
		   .pm = &snd_soc_pm_ops,
	},
	.probe = mt8365_mt6357_dev_probe,
};

module_platform_driver(mt8365_mt6357_driver);

/* Module information */
MODULE_DESCRIPTION("MT8365 EVK SoC machine driver");
MODULE_AUTHOR("Nicolas Belin <nbelin@baylibre.com>");
MODULE_AUTHOR("Aary Patil <aary.patil@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: mt8365_mt6357");
