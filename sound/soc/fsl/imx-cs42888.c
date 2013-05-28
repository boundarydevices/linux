/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_i2c.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/initval.h>

#include <mach/clock.h>

#include "fsl_esai.h"
#include "fsl_asrc.h"
#include "../codecs/cs42888.h"

#define CODEC_CLK_EXTER_OSC   1
#define CODEC_CLK_ESAI_HCKT   2

struct imx_priv_state {
	int hw;
};

static struct imx_priv_state hw_state;
unsigned int mclk_freq;
unsigned int codec_mclk;

static int imx_3stack_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	if (!cpu_dai->active)
		hw_state.hw = 0;

	return 0;
}

static void imx_3stack_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	if (!cpu_dai->active)
		hw_state.hw = 0;
}

static int imx_3stack_surround_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dpcm *dpcm;
	unsigned int rate = params_rate(params);
	u32 dai_format = 0;
	unsigned int lrclk_ratio = 0;
	if (hw_state.hw)
		return 0;
	/*currently we only have one fe, asrc, if someday we have multi fe,
	then we must think about to distinct them.*/
	list_for_each_entry(dpcm, &rtd->dpcm[substream->stream].fe_clients,
							list_fe) {
		if (dpcm->be == rtd) {
			struct snd_soc_pcm_runtime *fe = dpcm->fe;
			struct snd_soc_dai *dai = fe->cpu_dai;
			struct imx_asrc_p2p *asrc_p2p;
			asrc_p2p = snd_soc_dai_get_drvdata(dai);
			rate     = asrc_p2p->output_rate;
			break;
		}
	}

	hw_state.hw = 1;

	if (codec_mclk & CODEC_CLK_ESAI_HCKT) {
		switch (rate) {
		case 32000:
			lrclk_ratio = 5;
			break;
		case 48000:
			lrclk_ratio = 5;
			break;
		case 64000:
			lrclk_ratio = 2;
			break;
		case 96000:
			lrclk_ratio = 2;
			break;
		case 128000:
			lrclk_ratio = 2;
			break;
		case 44100:
			lrclk_ratio = 5;
			break;
		case 88200:
			lrclk_ratio = 2;
			break;
		case 176400:
			lrclk_ratio = 0;
			break;
		case 192000:
			lrclk_ratio = 0;
			break;
		default:
			pr_info("Rate not support.\n");
			return -EINVAL;;
		}

		dai_format = SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS;

	} else if (codec_mclk & CODEC_CLK_EXTER_OSC) {
		switch (rate) {
		case 32000:
			lrclk_ratio = 3;
			break;
		case 48000:
			lrclk_ratio = 3;
			break;
		case 64000:
			lrclk_ratio = 1;
			break;
		case 96000:
			lrclk_ratio = 1;
			break;
		case 128000:
			lrclk_ratio = 1;
			break;
		case 44100:
			lrclk_ratio = 3;
			break;
		case 88200:
			lrclk_ratio = 1;
			break;
		case 176400:
			lrclk_ratio = 0;
			break;
		case 192000:
			lrclk_ratio = 0;
			break;
		default:
			pr_info("Rate not support.\n");
			return -EINVAL;;
		}

		dai_format = SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM;
	}

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, dai_format);
	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai, 0x3, 0x3, 2, 32);
	/* set the ESAI system clock as output */
	if (codec_mclk & CODEC_CLK_ESAI_HCKT)
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_CLK_EXTAL_DIV,
			mclk_freq, SND_SOC_CLOCK_OUT);
	else if (codec_mclk & CODEC_CLK_EXTER_OSC)
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_CLK_EXTAL,
			mclk_freq, SND_SOC_CLOCK_IN);
	/* set the ratio */
	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_PSR, 1);

	if (codec_mclk & CODEC_CLK_ESAI_HCKT)
		snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_PM, 2);
	else if (codec_mclk & CODEC_CLK_EXTER_OSC)
		snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_PM, 0);

	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_FP, lrclk_ratio);

	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_RX_DIV_PSR, 1);

	if (codec_mclk & CODEC_CLK_ESAI_HCKT)
		snd_soc_dai_set_clkdiv(cpu_dai, ESAI_RX_DIV_PM, 2);
	else if (codec_mclk & CODEC_CLK_EXTER_OSC)
		snd_soc_dai_set_clkdiv(cpu_dai, ESAI_RX_DIV_PM, 0);

	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_RX_DIV_FP, lrclk_ratio);

	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, dai_format);
	/* set codec Master clock */
	if (codec_mclk & CODEC_CLK_ESAI_HCKT)
		snd_soc_dai_set_sysclk(codec_dai, 0, mclk_freq,\
			SND_SOC_CLOCK_IN);
	else if (codec_mclk & CODEC_CLK_EXTER_OSC)
		snd_soc_dai_set_sysclk(codec_dai, 0, mclk_freq,\
			SND_SOC_CLOCK_OUT);
	return 0;
}

static struct snd_soc_ops imx_3stack_surround_ops = {
	.startup = imx_3stack_startup,
	.shutdown = imx_3stack_shutdown,
	.hw_params = imx_3stack_surround_hw_params,
};

static const struct snd_soc_dapm_widget imx_3stack_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Line out jack */
	{"Line Out Jack", NULL, "AOUT1L"},
	{"Line Out Jack", NULL, "AOUT1R"},
	{"Line Out Jack", NULL, "AOUT2L"},
	{"Line Out Jack", NULL, "AOUT2R"},
	{"Line Out Jack", NULL, "AOUT3L"},
	{"Line Out Jack", NULL, "AOUT3R"},
	{"Line Out Jack", NULL, "AOUT4L"},
	{"Line Out Jack", NULL, "AOUT4R"},
	{"AIN1L", NULL, "Line In Jack"},
	{"AIN1R", NULL, "Line In Jack"},
	{"AIN2L", NULL, "Line In Jack"},
	{"AIN2R", NULL, "Line In Jack"},
	{"esai-playback",  NULL, "asrc-playback"},
	{"Playback",  NULL, "esai-playback"}, /*Playback is the codec dai*/
};

static struct snd_soc_dai_link imx_3stack_dai[] = {
	{
		.name = "HiFi",
		.stream_name = "HiFi",
		.codec_dai_name = "CS42888",
		.ops = &imx_3stack_surround_ops,
	},
	{
		.name = "HiFi-ASRC-FE",
		.stream_name = "HiFi-ASRC-FE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ops = &imx_3stack_surround_ops,
	},
	{
		.name = "HiFi-ASRC-BE",
		.stream_name = "HiFi-ASRC-BE",
		.codec_dai_name = "CS42888",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ops = &imx_3stack_surround_ops,
	},
};

static struct snd_soc_card snd_soc_card_imx_3stack = {
	.name = "cs42888-audio",
	.dai_link = imx_3stack_dai,
	.dapm_widgets = imx_3stack_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(imx_3stack_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __devinit imx_3stack_cs42888_probe(struct platform_device *pdev)
{
	struct device_node *esai_np, *codec_np;
	struct device_node *asrc_np;
	struct platform_device *esai_pdev;
	struct platform_device *asrc_pdev = NULL;
	struct i2c_client *codec_dev;
	char platform_name[32];
	int ret;

	esai_np = of_parse_phandle(pdev->dev.of_node, "esai-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!esai_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	asrc_np = of_parse_phandle(pdev->dev.of_node, "asrc-controller", 0);
	if (asrc_np)
		asrc_pdev = of_find_device_by_node(asrc_np);

	esai_pdev = of_find_device_by_node(esai_np);
	if (!esai_pdev) {
		dev_err(&pdev->dev, "failed to find ESAI platform device\n");
		ret = -EINVAL;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev || !codec_dev->driver) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		ret = -EINVAL;
		goto fail;
	}
	sprintf(platform_name, "imx-pcm-audio.%d",
					of_alias_get_id(esai_np, "audio"));

	/*if there is no asrc controller, we only enable one device*/
	if (!asrc_pdev) {
		imx_3stack_dai[0].codec_of_node   = codec_np;
		imx_3stack_dai[0].cpu_dai_name    = dev_name(&esai_pdev->dev);
		imx_3stack_dai[0].platform_name   = platform_name;
		snd_soc_card_imx_3stack.num_links = 1;
	} else {
		imx_3stack_dai[0].codec_of_node   = codec_np;
		imx_3stack_dai[0].cpu_dai_name    = dev_name(&esai_pdev->dev);
		imx_3stack_dai[0].platform_name   = platform_name;
		imx_3stack_dai[1].cpu_dai_name    = dev_name(&asrc_pdev->dev);
		imx_3stack_dai[1].platform_name   = platform_name;
		imx_3stack_dai[2].codec_of_node   = codec_np;
		imx_3stack_dai[2].cpu_dai_name    = dev_name(&esai_pdev->dev);
		snd_soc_card_imx_3stack.num_links = 3;
	}

	ret = of_property_read_u32(codec_np, "sysclk", &mclk_freq);
	if (ret) {
		printk(KERN_ERR "%s: failed to get sysclk!\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	ret = of_property_read_u32(codec_np, "mclk-source", &codec_mclk);
	if (ret) {
		printk(KERN_ERR "%s: failed to get mclk source\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	snd_soc_card_imx_3stack.dev = &pdev->dev;

	ret = snd_soc_register_card(&snd_soc_card_imx_3stack);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}
fail:
	if (esai_np)
		of_node_put(esai_np);
	if (codec_np)
		of_node_put(codec_np);
	return ret;
}

static int __devexit imx_3stack_cs42888_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&snd_soc_card_imx_3stack);
	return 0;
}

static const struct of_device_id imx_cs42888_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-cs42888", },
	{ /* sentinel */ }
};

static struct platform_driver imx_3stack_cs42888_driver = {
	.probe = imx_3stack_cs42888_probe,
	.remove = __devexit_p(imx_3stack_cs42888_remove),
	.driver = {
		.name = "imx-cs42888",
		.owner = THIS_MODULE,
		.of_match_table = imx_cs42888_dt_ids,
	},
};
module_platform_driver(imx_3stack_cs42888_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("ALSA SoC cs42888 Machine Layer Driver");
MODULE_LICENSE("GPL");
