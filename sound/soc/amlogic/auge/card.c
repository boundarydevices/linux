/*
 * sound/soc/amlogic/auge/card.c
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include "card.h"

struct aml_jack {
	struct snd_soc_jack jack;
	struct snd_soc_jack_pin pin;
	struct snd_soc_jack_gpio gpio;
};

struct aml_card_data {
	struct snd_soc_card snd_card;
	struct aml_dai_props {
		struct aml_dai cpu_dai;
		struct aml_dai codec_dai;
		unsigned int mclk_fs;
	} *dai_props;
	unsigned int mclk_fs;
	struct aml_jack hp_jack;
	struct aml_jack mic_jack;
	struct snd_soc_dai_link *dai_link;
};

#define aml_priv_to_dev(priv) ((priv)->snd_card.dev)
#define aml_priv_to_link(priv, i) ((priv)->snd_card.dai_link + (i))
#define aml_priv_to_props(priv, i) ((priv)->dai_props + (i))

#define DAI	"sound-dai"
#define CELL	"#sound-dai-cells"
#define PREFIX	"aml-audio-card,"

#define aml_card_init_hp(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 1, prefix)
#define aml_card_init_mic(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 0, prefix)
static int aml_card_init_jack(struct snd_soc_card *card,
				      struct aml_jack *sjack,
				      int is_hp, char *prefix)
{
	struct device *dev = card->dev;
	enum of_gpio_flags flags;
	char prop[128];
	char *pin_name;
	char *gpio_name;
	int mask;
	int det;

	sjack->gpio.gpio = -ENOENT;

	if (is_hp) {
		snprintf(prop, sizeof(prop), "%shp-det-gpio", prefix);
		pin_name	= "Headphones";
		gpio_name	= "Headphone detection";
		mask		= SND_JACK_HEADPHONE;
	} else {
		snprintf(prop, sizeof(prop), "%smic-det-gpio", prefix);
		pin_name	= "Mic Jack";
		gpio_name	= "Mic detection";
		mask		= SND_JACK_MICROPHONE;
	}

	det = of_get_named_gpio_flags(dev->of_node, prop, 0, &flags);
	if (det == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (gpio_is_valid(det)) {
		sjack->pin.pin		= pin_name;
		sjack->pin.mask		= mask;

		sjack->gpio.name	= gpio_name;
		sjack->gpio.report	= mask;
		sjack->gpio.gpio	= det;
		sjack->gpio.invert	= !!(flags & OF_GPIO_ACTIVE_LOW);
		sjack->gpio.debounce_time = 150;

		snd_soc_card_jack_new(card, pin_name, mask,
				      &sjack->jack,
				      &sjack->pin, 1);

		snd_soc_jack_add_gpios(&sjack->jack, 1,
				       &sjack->gpio);
	}

	return 0;
}

static void aml_card_remove_jack(struct aml_jack *sjack)
{
	if (gpio_is_valid(sjack->gpio.gpio))
		snd_soc_jack_free_gpios(&sjack->jack, 1, &sjack->gpio);
}

static int aml_card_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv =	snd_soc_card_get_drvdata(rtd->card);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	int ret;

	ret = clk_prepare_enable(dai_props->cpu_dai.clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(dai_props->codec_dai.clk);
	if (ret)
		clk_disable_unprepare(dai_props->cpu_dai.clk);

	return ret;
}

static void aml_card_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_card_data *priv =	snd_soc_card_get_drvdata(rtd->card);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);

	clk_disable_unprepare(dai_props->cpu_dai.clk);

	clk_disable_unprepare(dai_props->codec_dai.clk);
}

static int aml_card_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	unsigned int mclk, mclk_fs = 0;
	int i = 0, ret = 0;

	pr_info("%s ..numcodec:%d\n", __func__, rtd->num_codecs);
	if (priv->mclk_fs)
		mclk_fs = priv->mclk_fs;
	else if (dai_props->mclk_fs)
		mclk_fs = dai_props->mclk_fs;

	if (mclk_fs) {
		mclk = params_rate(params) * mclk_fs;

		for (i = 0; i < rtd->num_codecs; i++) {
			struct snd_soc_dai *codec_dai = rtd->codec_dais[i];

			ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
				SND_SOC_CLOCK_IN);

			if (ret && ret != -ENOTSUPP)
				goto err;
		}
		ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk,
					     SND_SOC_CLOCK_OUT);
		if (ret && ret != -ENOTSUPP)
			goto err;
	}
	return 0;
err:
	return ret;
}

static struct snd_soc_ops aml_card_ops = {
	.startup = aml_card_startup,
	.shutdown = aml_card_shutdown,
	.hw_params = aml_card_hw_params,
};

static int aml_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct aml_card_data *priv =	snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *codec = rtd->codec_dai;
	struct snd_soc_dai *cpu = rtd->cpu_dai;
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	int ret;

	ret = aml_card_init_dai(codec, &dai_props->codec_dai);
	if (ret < 0)
		return ret;

	ret = aml_card_init_dai(cpu, &dai_props->cpu_dai);
	if (ret < 0)
		return ret;

	ret = aml_card_init_hp(rtd->card, &priv->hp_jack, PREFIX);
	if (ret < 0)
		return ret;

	ret = aml_card_init_mic(rtd->card, &priv->hp_jack, PREFIX);
	if (ret < 0)
		return ret;

	return 0;
}

static int aml_card_dai_link_of(struct device_node *node,
					struct aml_card_data *priv,
					int idx,
					bool is_top_level_node)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct snd_soc_dai_link *dai_link = aml_priv_to_link(priv, idx);
	struct aml_dai_props *dai_props = aml_priv_to_props(priv, idx);
	struct aml_dai *cpu_dai = &dai_props->cpu_dai;
	struct aml_dai *codec_dai = &dai_props->codec_dai;
	struct device_node *cpu = NULL;
	struct device_node *plat = NULL;
	struct device_node *codec = NULL;
	char prop[128];
	char *prefix = "";
	int ret, single_cpu;

	/* For single DAI link & old style of DT node */
	if (is_top_level_node)
		prefix = PREFIX;

	snprintf(prop, sizeof(prop), "%scpu", prefix);
	cpu = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%splat", prefix);
	plat = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%scodec", prefix);
	codec = of_get_child_by_name(node, prop);

	if (!cpu || !codec) {
		ret = -EINVAL;
		dev_err(dev, "%s: Can't find %s DT node\n", __func__, prop);
		goto dai_link_of_err;
	}

	ret = aml_card_parse_daifmt(dev, node, codec,
					    prefix, &dai_link->dai_fmt);
	if (ret < 0)
		goto dai_link_of_err;

	of_property_read_u32(node, "mclk-fs", &dai_props->mclk_fs);

	ret = aml_card_parse_cpu(cpu, dai_link,
					 DAI, CELL, &single_cpu);
	if (ret < 0)
		goto dai_link_of_err;

#if 0
	ret = aml_card_parse_codec(codec, dai_link, DAI, CELL);
#else
	ret = snd_soc_of_get_dai_link_codecs(dev, codec, dai_link);
#endif
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_platform(plat, dai_link, DAI, CELL);
	if (ret < 0)
		goto dai_link_of_err;

	ret = snd_soc_of_parse_tdm_slot(cpu,	&cpu_dai->tx_slot_mask,
						&cpu_dai->rx_slot_mask,
						&cpu_dai->slots,
						&cpu_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = snd_soc_of_parse_tdm_slot(codec,	&codec_dai->tx_slot_mask,
						&codec_dai->rx_slot_mask,
						&codec_dai->slots,
						&codec_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_clk_cpu(cpu, dai_link, cpu_dai);
	if (ret < 0)
		goto dai_link_of_err;

#if 0
	ret = aml_card_parse_clk_codec(codec, dai_link, codec_dai);
	if (ret < 0)
		goto dai_link_of_err;
#endif

	ret = aml_card_canonicalize_dailink(dai_link);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_set_dailink_name(dev, dai_link,
						"%s-%s",
						dai_link->cpu_dai_name,
						dai_link->codecs->dai_name);
	if (ret < 0)
		goto dai_link_of_err;

	dai_link->ops = &aml_card_ops;
	dai_link->init = aml_card_dai_init;

	dev_dbg(dev, "\tname : %s\n", dai_link->stream_name);
	dev_dbg(dev, "\tformat : %04x\n", dai_link->dai_fmt);
	dev_dbg(dev, "\tcpu : %s / %d\n",
		dai_link->cpu_dai_name,
		dai_props->cpu_dai.sysclk);
	dev_dbg(dev, "\tcodec : %s / %d\n",
		dai_link->codecs->dai_name,
		dai_props->codec_dai.sysclk);

	aml_card_canonicalize_cpu(dai_link, single_cpu);

dai_link_of_err:
	of_node_put(cpu);
	of_node_put(codec);

	return ret;
}

static int aml_card_parse_aux_devs(struct device_node *node,
					   struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *aux_node;
	int i, n, len;

	if (!of_find_property(node, PREFIX "aux-devs", &len))
		return 0;		/* Ok to have no aux-devs */

	n = len / sizeof(__be32);
	if (n <= 0)
		return -EINVAL;

	priv->snd_card.aux_dev = devm_kzalloc(dev,
			n * sizeof(*priv->snd_card.aux_dev), GFP_KERNEL);
	if (!priv->snd_card.aux_dev)
		return -ENOMEM;

	for (i = 0; i < n; i++) {
		aux_node = of_parse_phandle(node, PREFIX "aux-devs", i);
		if (!aux_node)
			return -EINVAL;
		priv->snd_card.aux_dev[i].codec_of_node = aux_node;
	}

	priv->snd_card.num_aux_devs = n;
	return 0;
}

static int aml_card_parse_of(struct device_node *node,
				     struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *dai_link;
	int ret;

	if (!node)
		return -EINVAL;

	dai_link = of_get_child_by_name(node, PREFIX "dai-link");

	/* The off-codec widgets */
	if (of_property_read_bool(node, PREFIX "widgets")) {
		ret = snd_soc_of_parse_audio_simple_widgets(&priv->snd_card,
					PREFIX "widgets");
		if (ret)
			goto card_parse_end;
	}

	/* DAPM routes */
	if (of_property_read_bool(node, PREFIX "routing")) {
		ret = snd_soc_of_parse_audio_routing(&priv->snd_card,
					PREFIX "routing");
		if (ret)
			goto card_parse_end;
	}

	/* Factor to mclk, used in hw_params() */
	of_property_read_u32(node, PREFIX "mclk-fs", &priv->mclk_fs);

	/* Single/Muti DAI link(s) & New style of DT node */
	if (dai_link) {
		struct device_node *np = NULL;
		int i = 0;

		for_each_child_of_node(node, np) {
			dev_dbg(dev, "\tlink %d:\n", i);
			ret = aml_card_dai_link_of(np, priv,
							   i, false);
			if (ret < 0) {
				of_node_put(np);
				goto card_parse_end;
			}
			i++;
		}
	} else {
		/* For single DAI link & old style of DT node */
		ret = aml_card_dai_link_of(node, priv, 0, true);
		if (ret < 0)
			goto card_parse_end;
	}

	ret = aml_card_parse_card_name(&priv->snd_card, PREFIX);
	if (ret < 0)
		goto card_parse_end;

	ret = aml_card_parse_aux_devs(node, priv);

card_parse_end:
	of_node_put(dai_link);

	return ret;
}

static int aml_card_probe(struct platform_device *pdev)
{
	struct aml_card_data *priv;
	struct snd_soc_dai_link *dai_link;
	struct aml_dai_props *dai_props;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int num, ret;

	/* Get the number of DAI links */
	if (np && of_get_child_by_name(np, PREFIX "dai-link"))
		num = of_get_child_count(np);
	else
		num = 1;

	/* Allocate the private data and the DAI link array */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dai_props = devm_kzalloc(dev, sizeof(*dai_props) * num, GFP_KERNEL);
	dai_link  = devm_kzalloc(dev, sizeof(*dai_link)  * num, GFP_KERNEL);
	if (!dai_props || !dai_link)
		return -ENOMEM;

	priv->dai_props			= dai_props;
	priv->dai_link			= dai_link;

	/* Init snd_soc_card */
	priv->snd_card.owner		= THIS_MODULE;
	priv->snd_card.dev		= dev;
	priv->snd_card.dai_link		= priv->dai_link;
	priv->snd_card.num_links	= num;

	if (np && of_device_is_available(np)) {

		ret = aml_card_parse_of(np, priv);
		if (ret < 0) {
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "%s, parse error %d\n",
					__func__, ret);
			goto err;
		}

	} else {
		struct aml_card_info *cinfo;

		cinfo = dev->platform_data;
		if (!cinfo) {
			dev_err(dev, "no info for asoc-aml-card\n");
			return -EINVAL;
		}

		if (!cinfo->name ||
		    !cinfo->codec_dai.name ||
		    !cinfo->codec ||
		    !cinfo->platform ||
		    !cinfo->cpu_dai.name) {
			dev_err(dev, "insufficient aml_card_info settings\n");
			return -EINVAL;
		}

		priv->snd_card.name	=
				(cinfo->card) ? cinfo->card : cinfo->name;
		dai_link->name		= cinfo->name;
		dai_link->stream_name	= cinfo->name;
		dai_link->platform_name	= cinfo->platform;
		dai_link->codec_name	= cinfo->codec;
		dai_link->cpu_dai_name	= cinfo->cpu_dai.name;
		dai_link->codec_dai_name = cinfo->codec_dai.name;
		dai_link->dai_fmt	= cinfo->daifmt;
		dai_link->init		= aml_card_dai_init;
		memcpy(&priv->dai_props->cpu_dai, &cinfo->cpu_dai,
					sizeof(priv->dai_props->cpu_dai));
		memcpy(&priv->dai_props->codec_dai, &cinfo->codec_dai,
					sizeof(priv->dai_props->codec_dai));
	}

	snd_soc_card_set_drvdata(&priv->snd_card, priv);

	ret = devm_snd_soc_register_card(&pdev->dev, &priv->snd_card);
	if (ret >= 0)
		return ret;
err:
	aml_card_clean_reference(&priv->snd_card);

	return ret;
}

static int aml_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	aml_card_remove_jack(&priv->hp_jack);
	aml_card_remove_jack(&priv->mic_jack);

	return aml_card_clean_reference(card);
}

static const struct of_device_id aml_of_match[] = {
	{ .compatible = "amlogic, sound-card", },
	{},
};
MODULE_DEVICE_TABLE(of, aml_of_match);

static struct platform_driver aml_card = {
	.driver = {
		.name = "asoc-aml-card",
		.pm = &snd_soc_pm_ops,
		.of_match_table = aml_of_match,
	},
	.probe = aml_card_probe,
	.remove = aml_card_remove,
};

module_platform_driver(aml_card);

MODULE_ALIAS("platform:asoc-aml-card");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC aml Sound Card");
MODULE_AUTHOR("AMLogic, Inc.");
