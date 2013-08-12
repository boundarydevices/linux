/*
 * Copyright (C) 2010-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/platform_device.h>


/*
 *     Here add one platform module "imx-pcm-asrc" as pcm platform module.
 *     If we use the asrc_p2p node as the pcm platform, there will be one issue.
 * snd_soc_dapm_new_dai_widgets will be called twice, one in probe link_dais,
 * one in probe platform. so there will be two dai_widgets added to widget list.
 * but only the seconed one will be recorded in dai->playback_widget.
 *     Machine driver will add the audio route, but when it go through the
 * widget list, it will found the cpu_dai widget is the first one in the list.
 * add use the first one to link the audio route.
 *     when use the fe/be architecture for asrc p2p, it need to go through from
 * the fe->cpu_dai->playback_widget. but this is the second widget, so the
 * result is that it can't find a availble audio route for p2p case. So here
 * use another pcm platform to avoid this issue.
 */
static struct platform_driver imx_pcm_driver = {
	.driver = {
		.name = "imx-pcm-asrc",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(imx_pcm_driver);
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("i.MX ASoC PCM driver");
MODULE_ALIAS("platform:imx-pcm-asrc");
MODULE_LICENSE("GPL");
