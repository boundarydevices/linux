/*
 * sound/soc/amlogic/auge/clks.c
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
#define pr_fmt(fmt) "audio_clocks: " fmt

#include <linux/of_device.h>

#include "audio_clks.h"

#define DRV_NAME "audio-clocks"

static const struct of_device_id audio_clocks_of_match[] = {
	{
		.compatible = "amlogic, axg-audio-clocks",
		.data       = &axg_audio_clks_init,
	},
	{
		.compatible = "amlogic, g12a-audio-clocks",
		.data       = &g12a_audio_clks_init,
	},
	{
		.compatible = "amlogic, tl1-audio-clocks",
		.data       = &tl1_audio_clks_init,
	},
	{
		.compatible = "amlogic, sm1-audio-clocks",
		.data		= &sm1_audio_clks_init,
	},
	{},
};
MODULE_DEVICE_TABLE(of, audio_clocks_of_match);

static int audio_clocks_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct clk **clks;
	struct clk_onecell_data *clk_data;
	void __iomem *clk_base;
	struct audio_clk_init *p_audioclk_init;
	int ret;

	p_audioclk_init = (struct audio_clk_init *)
		of_device_get_match_data(dev);
	if (!p_audioclk_init) {
		dev_warn_once(dev,
			"check whether to update audio clock chipinfo\n");
		return -EINVAL;
	}

	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		dev_err(dev,
			"Unable to map clk base\n");
		return -ENXIO;
	}

	clk_data = devm_kmalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clks = devm_kmalloc(dev,
			p_audioclk_init->clk_num * sizeof(*clks),
			GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	if (p_audioclk_init) {
		p_audioclk_init->clk_gates(clks, clk_base);
		p_audioclk_init->clks(clks, clk_base);
	}

	clk_data->clks = clks;
	clk_data->clk_num = p_audioclk_init->clk_num;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get,
			clk_data);
	if (ret) {
		dev_err(dev, "%s fail ret: %d\n", __func__, ret);

		return ret;
	}

	return 0;
}

static struct platform_driver audio_clocks_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = audio_clocks_of_match,
	},
	.probe  = audio_clocks_probe,
};
module_platform_driver(audio_clocks_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio clocks ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
