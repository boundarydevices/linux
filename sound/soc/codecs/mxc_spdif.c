/*
 * MXC SPDIF ALSA Soc Codec Driver
 *
 * Copyright (C) 2007-2013 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <sound/soc.h>
#include <sound/pcm.h>

#include "../fsl/imx-spdif.h"

#define DRV_NAME "mxc_spdif"

struct snd_soc_dai_driver mxc_spdif_codec_dai = {
	.name		= "mxc-spdif",
	.playback	= {
		.rates		= MXC_SPDIF_RATES_PLAYBACK,
		.formats	= MXC_SPDIF_FORMATS_PLAYBACK,
	},
	.capture	= {
		.rates		= MXC_SPDIF_RATES_CAPTURE,
		.formats	= MXC_SPDIF_FORMATS_CAPTURE,
	},
};

struct snd_soc_codec_driver soc_codec_dev_spdif;

static int __devinit mxc_spdif_probe(struct platform_device *pdev)
{
	struct mxc_spdif_data *codec_pdata = dev_get_platdata(&pdev->dev);

	if (codec_pdata->spdif_tx) {
		mxc_spdif_codec_dai.playback.stream_name = "Playback";
		mxc_spdif_codec_dai.playback.channels_min = 2;
		mxc_spdif_codec_dai.playback.channels_max = 2;
	}

	if (codec_pdata->spdif_rx) {
		mxc_spdif_codec_dai.capture.stream_name = "Capture";
		mxc_spdif_codec_dai.capture.channels_min = 2;
		mxc_spdif_codec_dai.capture.channels_max = 2;
	}

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_spdif,
			&mxc_spdif_codec_dai, 1);
}

static int __devexit mxc_spdif_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver mxc_spdif_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mxc_spdif_probe,
	.remove = __devexit_p(mxc_spdif_remove),
};

module_platform_driver(mxc_spdif_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale S/PDIF transmitter");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
