/*
 * ASoC HDMI Transmitter driver for IMX development boards
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 *
 * based on stmp3780_devb_hdmi.c
 *
 * Vladimir Barinov <vbarinov@embeddedalley.com>
 *
 * Copyright 2008 SigmaTel, Inc
 * Copyright 2008 Embedded Alley Solutions, Inc
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program  is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <asm/dma.h>
#include <mach/hardware.h>

#include <mach/mxc_hdmi.h>
#include <linux/mfd/mxc-hdmi-core.h>
#include "imx-hdmi.h"

/* imx digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link imx_hdmi_dai_link = {
	.name = "IMX HDMI TX",
	.stream_name = "IMX HDMI TX",
	.codec_dai_name = "mxc-hdmi-soc",
	.codec_name = "mxc_hdmi_soc",
	.cpu_dai_name = "hdmi_audio.1",
	.platform_name = "imx-hdmi-soc-audio",
};

static struct snd_soc_card snd_soc_card_imx_hdmi = {
	.name = "imx-hdmi-soc",
	.dai_link = &imx_hdmi_dai_link,
	.num_links = 1,
};

static struct platform_device *imx_hdmi_codec_device;

static int __devinit imx_hdmi_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_imx_hdmi;
	int ret = 0;

	if (!hdmi_get_registered()) {
		dev_err(&pdev->dev, "Initialize HDMI-audio failed. Load HDMI-video first!\n");
		return -ENODEV;
	}

	imx_hdmi_codec_device =
		platform_device_register_simple("mxc_hdmi_soc", -1, NULL, 0);
	if (!imx_hdmi_codec_device) {
		dev_err(&pdev->dev, "Failed to register HDMI audio codec\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card: %d\n", ret);

	return ret;
}

static int __devexit imx_hdmi_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_imx_hdmi;

	platform_device_unregister(imx_hdmi_codec_device);
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id imx_hdmi_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-hdmi", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_hdmi_dt_ids);

static struct platform_driver imx_hdmi_audio_driver = {
	.probe = imx_hdmi_audio_probe,
	.remove = __devexit_p(imx_hdmi_audio_remove),
	.driver = {
		.of_match_table = imx_hdmi_dt_ids,
		.name = "imx-hdmi-machine-driver",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(imx_hdmi_audio_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("IMX HDMI TX ASoC driver");
MODULE_LICENSE("GPL");
