/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include "../codecs/wm8960.h"

#define DAI_NAME_SIZE	32
#define DEFAULT_MCLK_FREQ (12000000)

struct imx_wm8960_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
	bool is_codec_master;
	bool stream[2];
};

struct imx_priv {
	struct snd_soc_codec *codec;
	struct platform_device *pdev;
	struct snd_card *snd_card;
};

static struct imx_priv card_priv;

/* -1 for reserved value */
static const int sysclk_divs[] = { 1, -1, 2, -1 };

/* Multiply 256 for internal 256 div */
static const int dac_divs[] = { 256, 384, 512, 768, 1024, 1408, 1536 };

/* Multiply 10 to eliminate decimials */
static const int bclk_divs[] = {
	10, 15, 20, 30, 40, 55, 60, 80, 110,
	120, 160, 220, 240, 320, 320, 320
};

static int imx_hifi_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = codec_dai->codec->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct device *dev = card->dev;
	unsigned int sample_rate = params_rate(params);
	unsigned int sysclk, pll_out;
	int i, j, k, ret = 0;
	int bclk = snd_soc_params_to_bclk(params);

	if (params_channels(params) == 1)
		bclk *= 2;

	data->stream[tx] = true;

	if (data->stream[tx] && data->stream[!tx])
		return 0;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, data->dai.dai_fmt);
	if (ret) {
		dev_err(dev, "failed to set cpu dai fmt: %d\n", ret);
		return ret;
	}
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, data->dai.dai_fmt);
	if (ret) {
		dev_err(dev, "failed to set codec dai fmt: %d\n", ret);
		return ret;
	}

	/**
	 * codec ADCLRC pin configured as GPIO, DACLRC pin is used as a frame
	 * clock for ADCs and DACs
	 **/
	snd_soc_update_bits(codec_dai->codec, WM8960_IFACE2, 1<<6, 1<<6);
	snd_soc_update_bits(codec_dai->codec, WM8960_ADDCTL4, 7<<4, 1<<4);

	/* Enable headphone jack detect */
	snd_soc_update_bits(codec_dai->codec, WM8960_ADDCTL2, 1<<6, 1<<6);
	snd_soc_update_bits(codec_dai->codec, WM8960_ADDCTL4, 3<<2, 2<<2);
	snd_soc_update_bits(codec_dai->codec, WM8960_ADDCTL1, 1, 1);

	if (!data->is_codec_master) {
		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_OUT);
		if (ret) {
			dev_err(dev, "failed to set cpu sysclk: %d\n", ret);
			return ret;
		}
		return 0;
	} else {
		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN);
		if (ret) {
			dev_err(dev, "failed to set cpu sysclk: %d\n", ret);
			return ret;
		}
	}

	data->clk_frequency = clk_get_rate(data->codec_clk);
	if (data->clk_frequency != DEFAULT_MCLK_FREQ) {
		ret = clk_set_rate(data->codec_clk, DEFAULT_MCLK_FREQ);
		if (ret < 0)
			dev_dbg(dev, "fail to set codec clock frequency.\n");
		else
			data->clk_frequency = DEFAULT_MCLK_FREQ;
	}

	/* Use MCLK to provide sysclk directly*/
	sysclk = data->clk_frequency;

	for (i = 0; i < ARRAY_SIZE(sysclk_divs); ++i) {
		if (sysclk_divs[i] == -1)
			continue;
		sysclk /= sysclk_divs[i];
		for (j = 0; j < ARRAY_SIZE(dac_divs); ++j) {
			if (sysclk == sample_rate * dac_divs[j]) {
				for (k = 0; k < ARRAY_SIZE(bclk_divs); ++k)
					if (sysclk == bclk * bclk_divs[k] / 10) {
						snd_soc_dai_set_sysclk(codec_dai, WM8960_SYSCLK_MCLK, sysclk, 0);
						snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, i << 1);
						return 0;
					}
			}
		}
	}

	/* Use PLL to provide sysclk */
	for (i = 0; i < ARRAY_SIZE(sysclk_divs); ++i) {
		if (sysclk_divs[i] == -1)
			continue;
		for (j = 0; j < ARRAY_SIZE(dac_divs); ++j) {
			sysclk = sample_rate * dac_divs[j];
			pll_out = sysclk * sysclk_divs[i];

			for (k = 0; k < ARRAY_SIZE(bclk_divs); ++k) {
				if (sysclk == bclk * bclk_divs[k] / 10) {
					/* Set codec pll */
					ret = snd_soc_dai_set_pll(codec_dai, 0, 0, data->clk_frequency, pll_out);
					if (ret != 0)
						continue;
					/* Set codec sysclk */
					snd_soc_dai_set_sysclk(codec_dai, WM8960_SYSCLK_PLL, sysclk, 0);
					snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, i << 1);
					return 0;
				}
			}
		}
	}

	dev_err(dev, "failed to configure system clock");
	return -EINVAL;
}

static int imx_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = codec_dai->codec->card;
	struct imx_wm8960_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	data->stream[tx] = false;

	/* Power down PLL to save power*/
	if (data->is_codec_master && !data->stream[tx] && !data->stream[!tx])
		snd_soc_dai_set_pll(codec_dai, 0, 0, 0, 0);

	return 0;
}

static struct snd_soc_ops imx_hifi_ops = {
	.hw_params = imx_hifi_hw_params,
	.hw_free = imx_hifi_hw_free,
};

static const struct snd_soc_dapm_route imx_wm8960_dapm_route[] = {
	{ "LINPUT1", NULL, "MICB" },
};

static int imx_wm8960_probe(struct platform_device *pdev)
{
	struct device_node *cpu_np, *codec_np;
	struct platform_device *cpu_pdev;
	struct imx_priv *priv = &card_priv;
	struct i2c_client *codec_dev;
	struct imx_wm8960_data *data;
	int ret;

	priv->pdev = pdev;

	cpu_np = of_parse_phandle(pdev->dev.of_node, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "failed to find SAI platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev || !codec_dev->dev.driver) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	if (of_property_read_bool(pdev->dev.of_node, "codec-master")) {
		data->dai.dai_fmt = SND_SOC_DAIFMT_CBM_CFM;
		data->is_codec_master = true;
	} else
		data->dai.dai_fmt = SND_SOC_DAIFMT_CBS_CFS;

	data->codec_clk = devm_clk_get(&codec_dev->dev, "mclk");
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		dev_err(&pdev->dev, "failed to get codec clk: %d\n", ret);
		goto fail;
	}

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "wm8960-hifi";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_dai_name = dev_name(&cpu_pdev->dev);
	data->dai.platform_of_node = cpu_np;
	data->dai.ops = &imx_hifi_ops;
	data->dai.dai_fmt |= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF;

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.dai_link = &data->dai;
	data->card.dapm_routes = imx_wm8960_dapm_route;
	data->card.num_dapm_routes = ARRAY_SIZE(imx_wm8960_dapm_route);

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);
	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	priv->snd_card = data->card.snd_card;
fail:
	if (cpu_np)
		of_node_put(cpu_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static const struct of_device_id imx_wm8960_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-wm8960", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_wm8960_dt_ids);

static struct platform_driver imx_wm8960_driver = {
	.driver = {
		.name = "imx-wm8960",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_wm8960_dt_ids,
	},
	.probe = imx_wm8960_probe,
};
module_platform_driver(imx_wm8960_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX WM8960 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-wm8960");
