/*
 * MXC HDMI ALSA Soc Codec Driver
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "../fsl/imx-hdmi.h"

static struct snd_soc_dai_driver mxc_hdmi_codec_dai = {
	.name = "mxc-hdmi-soc",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = MXC_HDMI_RATES_PLAYBACK,
		.formats = MXC_HDMI_FORMATS_PLAYBACK,
	},
};

struct snd_soc_codec_driver soc_codec_dev_hdmi;

static int __devinit mxc_hdmi_codec_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_hdmi,
			&mxc_hdmi_codec_dai, 1);
}

static int __devexit mxc_hdmi_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

struct platform_driver mxc_hdmi_driver = {
	.driver = {
		.name = "mxc_hdmi_soc",
		.owner = THIS_MODULE,
	},
	.probe = mxc_hdmi_codec_probe,
	.remove = __devexit_p(mxc_hdmi_codec_remove),
};
module_platform_driver(mxc_hdmi_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXC HDMI Audio");
MODULE_LICENSE("GPL");
