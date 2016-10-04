/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>

#include "../codecs/sgtl5000.h"
#include "imx-audmux.h"

#define DAI_NAME_SIZE	32

struct imx_sgtl5000_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
	struct gpio_desc *mute_hp;
	struct gpio_desc *mute_lo;
	struct gpio_desc *amp_standby;
	struct gpio_desc *amp_gain[2];
	int amp_gain_value;
	bool limit_16bit_samples;
};

static int imx_sgtl5000_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(rtd->card);
	struct device *dev = rtd->card->dev;
	int ret;

	ret = snd_soc_dai_set_sysclk(rtd->codec_dai, SGTL5000_SYSCLK,
				     data->clk_frequency, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(dev, "could not set codec driver clock params\n");
		return ret;
	}

	return 0;
}

int do_mute(struct gpio_desc *gd, int mute)
{
	if (!gd)
		return 0;

	gpiod_set_value(gd, mute);
	return 0;
}

static int event_hp(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);
	int mute = SND_SOC_DAPM_EVENT_ON(event) ? 0 : 1;

	if (mute) {
		do_mute(data->mute_hp, mute);
		return do_mute(data->amp_standby, mute);
	}
	do_mute(data->amp_standby, mute);
	return do_mute(data->mute_hp, mute);
}

static int event_lo(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);

	return do_mute(data->mute_lo, SND_SOC_DAPM_EVENT_ON(event) ? 0 : 1);
}

static const struct snd_soc_dapm_widget imx_sgtl5000_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", event_hp),
	SND_SOC_DAPM_SPK("Line Out Jack", event_lo),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

static int imx_sgtl_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(rtd->card);

	if (data->limit_16bit_samples) {
		snd_pcm_hw_constraint_minmax(substream->runtime,
			SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16, 16);
	}
	return 0;
}


static struct snd_soc_ops imx_sgtl_ops = {
	.startup	= imx_sgtl_startup,
};


static int amp_gain_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);
	int value = ucontrol->value.integer.value[0];
	int i;

	if (value > 3)
		return -EINVAL;

	data->amp_gain_value = value;
	for (i = 0; i < 2; i++) {
		struct gpio_desc *gd = data->amp_gain[i];

		if (gd)
			gpiod_set_value(gd, value & 1);
		value >>= 1;
	}
	return 0;
}

static int amp_gain_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_sgtl5000_data *data = container_of(card,
			struct imx_sgtl5000_data, card);

	ucontrol->value.integer.value[0] = data->amp_gain_value;
	return 0;
}

static const struct snd_kcontrol_new more_controls[] = {
	SOC_SINGLE_EXT("amp_gain", 0, 0, 3, 0,
		       amp_gain_get, amp_gain_set),
};

static int imx_sgtl5000_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct i2c_client *codec_dev;
	struct imx_sgtl5000_data *data = NULL;
	struct gpio_desc *gd = NULL;
	int int_port, ext_port;
	int ret;
	int i;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!ssi_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EPROBE_DEFER;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EPROBE_DEFER;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->codec_clk = clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		goto fail;
	}

	data->clk_frequency = clk_get_rate(data->codec_clk);

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "sgtl5000";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_of_node = ssi_np;
	data->dai.platform_of_node = ssi_np;
	data->dai.init = &imx_sgtl5000_dai_init;
	data->dai.ops = &imx_sgtl_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;

	data->limit_16bit_samples = of_property_read_bool(pdev->dev.of_node, "limit-to-16-bit-samples");
	gd = devm_gpiod_get_index(&pdev->dev, "mute", 0);
	if (!IS_ERR(gd)) {
		gpiod_direction_output(gd, 0);
		gpiod_set_value(gd, 1);
		data->mute_hp = gd;
	}
	gd = devm_gpiod_get_index(&pdev->dev, "line-out-mute", 0);
	if (!IS_ERR(gd)) {
		gpiod_direction_output(gd, 0);
		gpiod_set_value(gd, 1);
		data->mute_lo = gd;
	}
	gd = devm_gpiod_get_index(&pdev->dev, "amp-standby", 0);
	if (!IS_ERR(gd)) {
		gpiod_direction_output(gd, 0);
		gpiod_set_value(gd, 1);
		data->amp_standby = gd;
	}
	for (i = 0; i < 2; i++) {
		gd = devm_gpiod_get_index(&pdev->dev, "amp-gain", i);
		if (!IS_ERR(gd)) {
			gpiod_direction_output(gd, 0);
			data->amp_gain[i] = gd;
		}
	}

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	if (data->amp_gain[0]) {
		data->card.controls	 = more_controls;
		data->card.num_controls  = ARRAY_SIZE(more_controls);
	}
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_sgtl5000_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_sgtl5000_dapm_widgets);

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);

	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	of_node_put(ssi_np);
	of_node_put(codec_np);

	return 0;

fail:
	if (data && !IS_ERR(data->codec_clk))
		clk_put(data->codec_clk);
	of_node_put(ssi_np);
	of_node_put(codec_np);

	return ret;
}

static int imx_sgtl5000_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_sgtl5000_data *data = snd_soc_card_get_drvdata(card);
	int i;

	if (data->amp_standby)
		gpiod_set_value(data->amp_standby, 1);
	if (data->mute_lo)
		gpiod_set_value(data->mute_lo, 1);
	if (data->mute_hp)
		gpiod_set_value(data->mute_hp, 1);
	for (i = 0; i < 2; i++) {
		struct gpio_desc *gd = data->amp_gain[i];

		if (gd)
			gpiod_set_value(gd, 0);
	}
	clk_put(data->codec_clk);

	return 0;
}

static const struct of_device_id imx_sgtl5000_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-sgtl5000", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_sgtl5000_dt_ids);

static struct platform_driver imx_sgtl5000_driver = {
	.driver = {
		.name = "imx-sgtl5000",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_sgtl5000_dt_ids,
	},
	.probe = imx_sgtl5000_probe,
	.remove = imx_sgtl5000_remove,
};
module_platform_driver(imx_sgtl5000_driver);

MODULE_AUTHOR("Shawn Guo <shawn.guo@linaro.org>");
MODULE_DESCRIPTION("Freescale i.MX SGTL5000 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-sgtl5000");
