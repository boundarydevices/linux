/*
 * Copyright 2008-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define SI4763_RATES SNDRV_PCM_RATE_48000

#define SI4763_FORMATS SNDRV_PCM_FMTBIT_S16_LE

static struct snd_soc_dai_driver si4763_codec_dai = {
	.name = "si4763",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SI4763_RATES,
		.formats = SI4763_FORMATS,
	},
};

static struct snd_soc_codec_driver soc_codec_dev_si4763;

static int si4763_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_info(&pdev->dev, "SI4763 Audio codec\n");

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_si4763,
			&si4763_codec_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register codec\n");
		return ret;
	}

	return 0;
}

static int __devexit si4763_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

struct platform_driver si4763_driver = {
	.driver = {
		.name = "si4763",
		.owner = THIS_MODULE,
	},
	.probe = si4763_probe,
	.remove = __devexit_p(si4763_remove),
};

module_platform_driver(si4763_driver);

MODULE_DESCRIPTION("ASoC si4763 codec driver");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_LICENSE("GPL");
