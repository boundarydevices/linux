/*
 * ASoC S/PDIF driver for IMX development boards
 *
 * Copyright (C) 2008-2013 Freescale Semiconductor, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program  is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>


#include "imx-ssi.h"
#include "imx-spdif.h"

#define DAI_NAME_SIZE	32

struct imx_spdif_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
};

static int __devinit imx_spdif_audio_probe(struct platform_device *pdev)
{
	struct device_node *spdif_np;
	struct device_node *np = pdev->dev.of_node;
	struct platform_device *spdif_pdev;
	struct platform_device *codec_dev;
	struct imx_spdif_data *data;
	struct mxc_spdif_data codec_pdata;
	struct property *poldbase;
	char platform_name[32];
	int ret = 0;
	u32 val;

	spdif_np = of_parse_phandle(np, "spdif-controller", 0);
	if (!spdif_np) {
		dev_err(&pdev->dev, "spdif-controller phandle's missing\n");
		ret = -EINVAL;
		goto fail;
	}

	/*
	 * Check the existence of spdif-dai's pinctrl-0
	 * In default, pinctrl-0 should be available.
	 * If not, that means board code deleted the property
	 * And it didn't want to initialize SPDIF.
	 */
	poldbase = of_find_property(spdif_np, "pinctrl-0", 0);
	if (!poldbase) {
		dev_err(&pdev->dev, "spdif-dai wasn't enabled.\n");
		ret = -EINVAL;
		goto fail;
	}

	spdif_pdev = of_find_device_by_node(spdif_np);
	if (!spdif_pdev) {
		dev_err(&pdev->dev, "failed to find SPDIF controller device\n");
		ret = -EINVAL;
		goto fail;
	}
	if (of_property_read_u32(np, "spdif-tx", &val) >= 0)
		codec_pdata.spdif_tx = val;
	else
		codec_pdata.spdif_tx = 0;

	if (of_property_read_u32(np, "spdif-rx", &val) >= 0)
		codec_pdata.spdif_rx = val;
	else
		codec_pdata.spdif_rx = 0;

	codec_dev = platform_device_register_resndata(NULL, "mxc_spdif", -1,
			NULL, 0, &codec_pdata, sizeof(codec_pdata));
	if (!codec_dev) {
		pr_err("%s failed platform_device_alloc\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err1;
	}

	data->dai.name = "SPDIF";
	data->dai.stream_name = "SPDIF";
	data->dai.codec_dai_name = "mxc-spdif";
	data->dai.codec_name = dev_name(&codec_dev->dev);
	data->dai.cpu_dai_name = dev_name(&spdif_pdev->dev);

	sprintf(platform_name, "imx-pcm-audio.%d", of_alias_get_id(spdif_np, "audio"));
	data->dai.platform_name = platform_name;
	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto err1;

	data->card.num_links = 1;
	data->card.dai_link = &data->dai;

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto err1;
	}

	platform_set_drvdata(pdev, data);

	return 0;

err1:
	platform_device_unregister(codec_dev);
fail:
	if (spdif_np)
		of_node_put(spdif_np);

	return ret;
}

static int __devexit imx_spdif_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static const struct of_device_id imx_spdif_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-spdif", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_spdif_dt_ids);

static struct platform_driver imx_spdif_audio_driver = {
	.probe = imx_spdif_audio_probe,
	.remove = __devexit_p(imx_spdif_audio_remove),
	.driver = {
		.of_match_table = imx_spdif_dt_ids,
		.name = "imx-spdif-machine-driver",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(imx_spdif_audio_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale S/PDIF machine driver");
MODULE_LICENSE("GPL");
