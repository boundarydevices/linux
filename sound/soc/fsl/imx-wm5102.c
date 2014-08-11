/*
 * Copyright 2014 Boundary Devices
 *
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
#include <linux/clk.h>
#include <sound/soc.h>

#include "../codecs/wm5102.h"
#include "imx-audmux.h"


struct imx_wm5102_data {
	struct snd_soc_dai_link dai[1];
	struct snd_soc_card card;
	int sysclk_rate;
	int asyncclk_rate;
};


/* BCLK2 is fixed at this currently */
#define BCLK2_RATE (64 * 8000)

#define MCLK_RATE 24000000

#define SYS_AUDIO_RATE 44100
#define SYS_MCLK_RATE  (SYS_AUDIO_RATE * 512)

#define DAI_AIF1    0

static int wm5102_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct imx_wm5102_data *wm5102 = card->drvdata;
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level != SND_SOC_BIAS_STANDBY)
			break;

		ret = snd_soc_codec_set_pll(codec, WM5102_FLL1,
					    ARIZONA_FLL_SRC_MCLK1,
					    MCLK_RATE,
					    wm5102->sysclk_rate);
		if (ret < 0)
			pr_err("Failed to start FLL: %d\n", ret);

		if (wm5102->asyncclk_rate) {
			ret = snd_soc_codec_set_pll(codec, WM5102_FLL2,
						    ARIZONA_FLL_SRC_AIF2BCLK,
						    BCLK2_RATE,
						    wm5102->asyncclk_rate);
			if (ret < 0)
				pr_err("Failed to start FLL: %d\n", ret);
		}
		break;

	default:
		break;
	}

	return 0;
}

static int wm5102_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct imx_wm5102_data *wm5102 = card->drvdata;
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, 0, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop FLL: %d\n", ret);
			return ret;
		}

		if (wm5102->asyncclk_rate) {
			ret = snd_soc_codec_set_pll(codec, WM5102_FLL2,
						    0, 0, 0);
			if (ret < 0) {
				pr_err("Failed to stop FLL: %d\n", ret);
				return ret;
			}
		}
		break;

	default:
		break;
	}

	dapm->bias_level = level;

	return 0;
}

static int wm5102_late_probe(struct snd_soc_card *card)
{
	struct imx_wm5102_data *wm5102 = card->drvdata;
	struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
	struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       wm5102->sysclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(codec->dev, "Failed to set SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(aif1_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret)
		dev_err(aif1_dai->dev, "Failed to set AIF1 clock: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_OPCLK, 0,
				       SYS_MCLK_RATE, SND_SOC_CLOCK_OUT);
	if (ret)
		dev_err(codec->dev, "Failed to set OPCLK: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       wm5102->asyncclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(codec->dev, "Failed to set ASYNCCLK: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_dai_link dai_wm5102[] = {
	{
		.name = "HiFi",
		.stream_name = "HiFi",
		.codec_dai_name = "wm5102-aif1",
		.codec_name = "wm5102-codec",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM,
	},
};


static const struct snd_soc_card wm5102_card = {
	.name = "wm5102 WM5102",
	.owner = THIS_MODULE,
	.dai_link = dai_wm5102,
	.num_links = ARRAY_SIZE(dai_wm5102),
	.late_probe = wm5102_late_probe,

	.set_bias_level = wm5102_set_bias_level,
	.set_bias_level_post = wm5102_set_bias_level_post,
};

static int imx_wm5102_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np;
	struct platform_device *ssi_pdev;
	struct imx_wm5102_data *data;
	int int_port, ext_port;
	int ret;

	pr_info("%s\n", __func__);
	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	if (!ssi_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}
	data->sysclk_rate = 45158400;
	data->asyncclk_rate = 49152000;
	data->card = wm5102_card;
	data->dai[0] = data->card.dai_link[0];
	data->card.dai_link = data->dai;
	data->card.dev = &pdev->dev;
	data->card.dai_link[0].cpu_of_node = ssi_np;
	data->card.dai_link[0].platform_of_node = ssi_np;
	data->card.drvdata = data;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, data);
	pr_info("%s success\n", __func__);
fail:
	if (ssi_np)
		of_node_put(ssi_np);
	return ret;
}

static int imx_wm5102_remove(struct platform_device *pdev)
{
	struct imx_wm5102_data *data = platform_get_drvdata(pdev);

	snd_soc_unregister_card(&data->card);
	return 0;
}

static const struct of_device_id imx_wm5102_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-wm5102", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_wm5102_dt_ids);

static struct platform_driver imx_wm5102_driver = {
	.driver = {
		.name = "imx-wm5102",
		.owner = THIS_MODULE,
		.of_match_table = imx_wm5102_dt_ids,
	},
	.probe = imx_wm5102_probe,
	.remove = imx_wm5102_remove,
};
module_platform_driver(imx_wm5102_driver);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("i.MX WM5102 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-wm5102");
