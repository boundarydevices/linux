/*
 * sound/soc/codecs/amlogic/pdm_dummy.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <sound/soc.h>

#define DUMMY_RATES		(SNDRV_PCM_RATE_96000 |\
					SNDRV_PCM_RATE_64000 |\
					SNDRV_PCM_RATE_48000 |\
					SNDRV_PCM_RATE_32000 |\
					SNDRV_PCM_RATE_16000 |\
					SNDRV_PCM_RATE_8000)

#define DUMMY_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
					SNDRV_PCM_FMTBIT_S24_LE |\
					SNDRV_PCM_FMTBIT_S32_LE)

static int pdm_dummy_set_fmt(
	struct snd_soc_dai *codec_dai,
	unsigned int fmt)
{
	return 0;
}

static int pdm_dummy_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int pdm_dummy_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}


static struct snd_soc_dai_ops pdm_dummy_ops = {
	.set_fmt = pdm_dummy_set_fmt,
	.hw_params = pdm_dummy_hw_params,
	.prepare = pdm_dummy_prepare,
};

struct snd_soc_dai_driver pdm_dummy_dai = {
	.name = "pdm",
	.capture = {
		.stream_name = "PDM Capture",
		.channels_min = 1,
		.channels_max = 16,
		.rates = DUMMY_RATES,
		.formats = DUMMY_FORMATS,
	},
	.ops = &pdm_dummy_ops,
};

static int pdm_dummy_probe(struct snd_soc_codec *codec)
{
	return 0;
}

static int pdm_dummy_remove(struct snd_soc_codec *codec)
{
	return 0;
};

struct snd_soc_codec_driver soc_codec_dev_pdm_dummy = {
	.probe = pdm_dummy_probe,
	.remove = pdm_dummy_remove,
};


static const struct of_device_id pdm_dummy_codec_device_id[] = {
	{ .compatible = "amlogic, pdm_dummy_codec", },
	{},
};

static int pdm_dummy_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
		&soc_codec_dev_pdm_dummy,
		&pdm_dummy_dai, 1);
}

static int pdm_dummy_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static struct platform_driver pdm_dummy_platform_driver = {
	.driver = {
		.name = "pdm",
		.owner = THIS_MODULE,
		.of_match_table = pdm_dummy_codec_device_id,
	},
	.probe = pdm_dummy_platform_probe,
	.remove = pdm_dummy_platform_remove,
};

static int __init pdm_dummy_init(void)
{
	return platform_driver_register(&pdm_dummy_platform_driver);
}

static void __exit pdm_dummy_exit(void)
{
	platform_driver_unregister(&pdm_dummy_platform_driver);
}

module_init(pdm_dummy_init);
module_exit(pdm_dummy_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("ASoC pdm_dummy codec driver");
MODULE_LICENSE("GPL");
