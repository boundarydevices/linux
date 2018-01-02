/*
 * sound/soc/amlogic/meson/dmic.c
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
#undef pr_fmt
#define pr_fmt(fmt) "snd_dmic: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#include "dmic.h"

#define DRV_NAME "snd_dmic"

#define PDM_CTRL		0x40
/* process_header_copy_only_on */
#define CIC_DEC8OR16_SEL        2
#define PDM_HPF_BYPASS          1
#define PDM_ENABLE              0
/* process_header_copy_only_off */
#define PDM_IN_CTRL		0x42
/* process_header_copy_only_on */
#define PDMIN_REV              18
#define PDMCLK_REV             17
#define GET_STA_EN             16
#define SAMPLE_CNT             8
/* process_header_copy_only_off */
#define PDM_VOL_CTRL	0x43
/* process_header_copy_only_on */
#define PDML_INV             1
#define PDMR_INV             0
/* process_header_copy_only_off */
#define PDM_VOL_GAIN_L	0x44
#define PDM_VOL_GAIN_R	0x45
#define PDM_STATUS		0x50

struct aml_dmic_priv *dmic_pub;

static int aml_dmic_codec_probe(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_dai_driver aml_dmic_dai = {
	.name = "dmic-hifi",
	.capture = {
		.stream_name = "dmic Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = SNDRV_PCM_FMTBIT_S32_LE
			| SNDRV_PCM_FMTBIT_S24_LE
			| SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static const struct snd_soc_dapm_widget aml_dmic_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("DMIC AIFIN", "dmic Capture", 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("DMIC IN"),
};

static const struct snd_soc_dapm_route dmic_intercon[] = {
	{"DMIC AIFIN", NULL, "DMIC IN"},
};

static const struct snd_soc_codec_driver aml_dmic = {
	.probe = aml_dmic_codec_probe,
	.component_driver = {
		.dapm_widgets = aml_dmic_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(aml_dmic_dapm_widgets),
		.dapm_routes = dmic_intercon,
		.num_dapm_routes = ARRAY_SIZE(dmic_intercon),
	}
};

static const char *const gate_names[] = {
	"pdm",
};

static int aml_dmic_platform_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk *clk_gate;
	struct aml_dmic_priv *dmic_priv;
	unsigned int val;
	int ret;

	dev_info(&pdev->dev, "Dmic probe!\n");

	clk_gate = devm_clk_get(&pdev->dev, gate_names[0]);
	if (IS_ERR(clk_gate)) {
		dev_err(&pdev->dev, "Can't get aml dmic gate\n");
		return PTR_ERR(clk_gate);
	}
	clk_prepare_enable(clk_gate);

	dmic_priv = devm_kzalloc(&pdev->dev,
		sizeof(struct aml_dmic_priv), GFP_KERNEL);
	if (dmic_priv == NULL)
		return -ENOMEM;

	dmic_priv->dmic_pins =
		devm_pinctrl_get_select(&pdev->dev, "audio_dmic");
	if (IS_ERR(dmic_priv->dmic_pins)) {
		dev_err(&pdev->dev, "pinctrls error!\n");
		return -EINVAL;
	}

	dev_set_drvdata(&pdev->dev, dmic_priv);

	dmic_priv->clk_mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(dmic_priv->clk_mclk)) {
		dev_err(&pdev->dev, "Can't retrieve clk_mclk clock\n");
		ret = PTR_ERR(dmic_priv->clk_mclk);
		goto err;
	}

	dmic_priv->clk_pdm = devm_clk_get(&pdev->dev, "pdm");
	if (IS_ERR(dmic_priv->clk_pdm)) {
		dev_err(&pdev->dev, "Can't retrieve clk_pdm clock\n");
		ret = PTR_ERR(dmic_priv->clk_pdm);
		goto err;
	}

	ret = clk_set_parent(dmic_priv->clk_pdm, dmic_priv->clk_mclk);
	if (ret) {
		pr_err("Can't set dmic pdm clk parent err: %d\n", ret);
		goto err;
	}

	ret = clk_set_rate(dmic_priv->clk_pdm,
		clk_get_rate(dmic_priv->clk_mclk) / 4);
	if (ret) {
		pr_err("Can't set dmic pdm clock rate, err: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(dmic_priv->clk_pdm);
	if (ret) {
		pr_err("Can't enable dmic pdm clock: %d\n", ret);
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	dmic_priv->pdm_base = devm_ioremap_nocache(&pdev->dev, res->start,
					       resource_size(res));
	if (IS_ERR(dmic_priv->pdm_base)) {
		dev_err(&pdev->dev, "Unable to map pdm_base\n");
		return PTR_ERR(dmic_priv->pdm_base);
	}

	writel(0x10000, dmic_priv->pdm_base + (PDM_VOL_GAIN_L<<2));
	writel(0x10000, dmic_priv->pdm_base + (PDM_VOL_GAIN_R<<2));

	val = readl(dmic_priv->pdm_base + (PDM_CTRL<<2));
	writel(1, dmic_priv->pdm_base + (PDM_CTRL<<2));
	val = readl(dmic_priv->pdm_base + (PDM_CTRL<<2));

	dmic_pub = dmic_priv;
	return snd_soc_register_codec(&pdev->dev,
			&aml_dmic, &aml_dmic_dai, 1);
err:
	return ret;
}

static int aml_dmic_platform_remove(struct platform_device *pdev)
{
	struct aml_dmic_priv *dmic_priv = dev_get_drvdata(&pdev->dev);

	if (dmic_priv && dmic_priv->clk_pdm)
		clk_disable_unprepare(dmic_priv->clk_pdm);

	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static void aml_dmic_platform_shutdown(struct platform_device *pdev)
{
	struct aml_dmic_priv *dmic_priv = dev_get_drvdata(&pdev->dev);

	if (dmic_priv && dmic_priv->clk_pdm)
		clk_disable_unprepare(dmic_priv->clk_pdm);

	snd_soc_unregister_codec(&pdev->dev);
}

static const struct of_device_id amlogic_dmic_of_match[] = {
	{.compatible = "aml, aml_snd_dmic"},
	{}
};

static struct platform_driver aml_dmic_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = amlogic_dmic_of_match,
	},
	.probe = aml_dmic_platform_probe,
	.remove = aml_dmic_platform_remove,
	.shutdown = aml_dmic_platform_shutdown,
};

module_platform_driver(aml_dmic_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("amlogic digital mic driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
