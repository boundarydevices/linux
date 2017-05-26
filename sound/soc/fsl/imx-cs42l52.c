/*
 * Copyright 2012, 2014 Freescale Semiconductor, Inc.
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
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
#include <linux/switch.h>
#endif

#include "../codecs/cs42l52.h"
#include "imx-audmux.h"

#define DAI_NAME_SIZE	32

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
#define BIT_HEADSET			(1 << 0)
#define BIT_HEADSET_NO_MIC	(1 << 1)
#endif

struct imx_cs42l52_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];

	struct platform_device *pdev;

	int hs_det_gpio[2];
	int hs_det_active[2];

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
	struct switch_dev sdev;
#endif
};


static struct snd_soc_jack imx_cs42l52_hs_jack;

static struct snd_soc_jack_pin imx_cs42l52_hs_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
};

static struct snd_soc_jack_gpio imx_cs42l52_hs_jack_gpio[] = {
	{
		.name = "headset0 detect",
		.report = SND_JACK_HEADSET,
		.debounce_time = 150,
		.invert = 0,
	},
	{
		.name = "headset1 detect",
		.report = SND_JACK_HEADSET,
		.debounce_time = 150,
		.invert = 0,
	},
};


static int imx_cs42l52_hs_status_check(void)
{
	struct snd_soc_codec *codec = imx_cs42l52_hs_jack.codec;
	struct snd_soc_card *card = codec->card;
	struct imx_cs42l52_data *data = snd_soc_card_get_drvdata(card);
	int status;
	int i;

	status = 0;

	for (i = 0; i < ARRAY_SIZE(data->hs_det_gpio); ++i)
		if (gpio_is_valid(data->hs_det_gpio[i]))
			break;

	/* all gpios invalid */
	if (i == ARRAY_SIZE(data->hs_det_gpio))
		return status;

	for (i = 0; i < ARRAY_SIZE(data->hs_det_gpio); ++i)
		if (gpio_is_valid(data->hs_det_gpio[i])) {
			if (gpio_get_value(data->hs_det_gpio[i]) == data->hs_det_active[i]) {
				status |= imx_cs42l52_hs_jack_gpio[i].report;
				break;
			}
		}

	status &= SND_JACK_HEADSET;

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
	if (status)
		switch(status) {
			case SND_JACK_HEADSET:
			case SND_JACK_MICROPHONE:
				switch_set_state(&data->sdev, BIT_HEADSET);
				break;
			case SND_JACK_HEADPHONE:
				switch_set_state(&data->sdev, BIT_HEADSET_NO_MIC);
				break;
		}
	else
		switch_set_state(&data->sdev, 0);
#endif

	return status;
}

static int imx_cs42l52_gpio_init(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->card;
	struct imx_cs42l52_data *data = snd_soc_card_get_drvdata(card);
	struct platform_device *pdev = data->pdev;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(data->hs_det_gpio); ++i)
		if (gpio_is_valid(data->hs_det_gpio[i]))
			break;

	if (i == ARRAY_SIZE(data->hs_det_gpio))
		return 0;

	ret = snd_soc_jack_new(codec, "Headset Jack", SND_JACK_HEADSET,
							&imx_cs42l52_hs_jack);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_jack_new failed (%d)\n", ret);
		return ret;
	}

	ret = snd_soc_jack_add_pins(&imx_cs42l52_hs_jack,
								ARRAY_SIZE(imx_cs42l52_hs_pins),
								imx_cs42l52_hs_pins);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_jack_add_pins failed (%d)\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(imx_cs42l52_hs_jack_gpio); ++i)
		if (gpio_is_valid(data->hs_det_gpio[i])) {
			imx_cs42l52_hs_jack_gpio[i].gpio = data->hs_det_gpio[i];
			imx_cs42l52_hs_jack_gpio[i].jack_status_check =
						imx_cs42l52_hs_status_check;
		}

	ret = snd_soc_jack_add_gpios(&imx_cs42l52_hs_jack,
								ARRAY_SIZE(imx_cs42l52_hs_jack_gpio),
								imx_cs42l52_hs_jack_gpio);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_jack_add_gpios failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_dapm_widget imx_cs42l52_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("Ext Mic", NULL),
};

static int imx_cs42l52_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int dai_fmt = rtd->dai_link->dai_fmt;
	struct snd_soc_card *card = rtd->card;
	struct imx_cs42l52_data *data = snd_soc_card_get_drvdata(card);
	struct platform_device *pdev = data->pdev;
	int ret;

	/* set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_fmt);
	if (ret) {
		dev_err(&pdev->dev, "failed to set the format for codec side\n");
		return ret;
	}

	/* set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_fmt);
	if (ret) {
		dev_err(&pdev->dev, "failed to set the format for cpu side\n");
		return ret;
	}

	return 0;
}

static int imx_cs42l52_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct imx_cs42l52_data *data = snd_soc_card_get_drvdata(codec->card);
	struct platform_device *pdev = data->pdev;
	int ret;

	ret = snd_soc_dapm_nc_pin(dapm, "AIN1L");
	if (ret) {
		dev_err(&pdev->dev, "failed permanently disable pin %s: %d\n", "AIN1L", ret);
		return ret;
	}

	ret = snd_soc_dapm_nc_pin(dapm, "AIN1R");
	if (ret) {
		dev_err(&pdev->dev, "failed permanently disable pin %s: %d\n", "AIN1R", ret);
		return ret;
	}

	ret = snd_soc_dapm_nc_pin(dapm, "AIN2L");
	if (ret) {
		dev_err(&pdev->dev, "failed permanently disable pin %s: %d\n", "AIN2L", ret);
		return ret;
	}

	ret = snd_soc_dapm_nc_pin(dapm, "AIN2R");
	if (ret) {
		dev_err(&pdev->dev, "failed permanently disable pin %s: %d\n", "AIN2R", ret);
		return ret;
	}

	ret = snd_soc_dapm_force_enable_pin(dapm, "Mic Bias");
	if (ret) {
		dev_err(&pdev->dev, "failed to force enable pin \"Mic Bias\": %d \n", ret);
		return ret;
	}

	ret = snd_soc_dapm_sync(dapm);
	if (ret) {
		dev_err(&pdev->dev, "failed to sync: %d \n", ret);
		return ret;
	}

	ret = imx_cs42l52_gpio_init(codec);
	if (ret) {
		dev_err(&pdev->dev, "imx_cs42l52_gpio_init failed = %d", ret);
		return ret;
	}

	return 0;
}


static struct snd_soc_ops imx_cs42l52_ops = {
	.hw_params = imx_cs42l52_hw_params,
};


#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
static int imx_cs42l52_set_bias_level(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
	struct snd_soc_codec *codec;

	if (card->rtd == NULL
		|| card->rtd[0].codec_dai == NULL
		|| card->rtd[0].codec_dai->codec == NULL
		|| card->rtd[0].codec_dai->codec->driver == NULL)
		return 0;

	codec = card->rtd[0].codec_dai->codec;

	return 0;
}
#endif


static int imx_cs42l52_audmux_config(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int int_port, ext_port;
	int ret;

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

	return 0;
}

static int imx_cs42l52_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np;
	struct platform_device *cpu_pdev;
	struct i2c_client *codec_dev;
	struct imx_cs42l52_data *data;
	int i, ret;

	cpu_np = of_parse_phandle(np, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_np = of_parse_phandle(np, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "audio codec missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	if (strstr(cpu_np->name, "ssi")) {
		ret = imx_cs42l52_audmux_config(pdev);
		if (ret)
			goto fail;
	}

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->pdev = pdev;

	data->dai.name = "CS42L52";
	data->dai.stream_name = "CS42L52";
	data->dai.codec_dai_name = "cs42l52";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_dai_name = dev_name(&cpu_pdev->dev);
	data->dai.cpu_of_node = cpu_np;
	data->dai.platform_of_node = cpu_np;
	data->dai.init = &imx_cs42l52_dai_init;
	data->dai.ops = &imx_cs42l52_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBM_CFM;

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_cs42l52_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_cs42l52_dapm_widgets);

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
	data->card.set_bias_level = imx_cs42l52_set_bias_level;
#endif

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
	data->sdev.name = "h2w";
	ret = switch_dev_register(&data->sdev);
	if (ret) {
		ret = -EINVAL;
		goto fail;
	}
#endif

	for (i = 0; i < ARRAY_SIZE(data->hs_det_gpio); ++i)
		data->hs_det_gpio[i] = of_get_named_gpio_flags(np, "hs-det-gpio", i,
							(enum of_gpio_flags *)&data->hs_det_active[i]);

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

fail:
	if (cpu_np)
		of_node_put(cpu_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_cs42l52_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_cs42l52_data *data = snd_soc_card_get_drvdata(card);

	snd_soc_jack_free_gpios(&imx_cs42l52_hs_jack,
				ARRAY_SIZE(imx_cs42l52_hs_jack_gpio),
				imx_cs42l52_hs_jack_gpio);

#ifdef CONFIG_SND_SOC_IMX_CS42L52_ANDROID
	switch_dev_unregister(&data->sdev);
#endif

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id imx_cs42l52_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-cs42l52", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_cs42l52_dt_ids);

static struct platform_driver imx_cs42l52_driver = {
	.driver = {
		.name = "imx-cs42l52",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_cs42l52_dt_ids,
	},
	.probe = imx_cs42l52_probe,
	.remove = imx_cs42l52_remove,
};
module_platform_driver(imx_cs42l52_driver);

MODULE_AUTHOR("Maxim Paymushkin <maxim.paymushkin@gmail.com>");
MODULE_DESCRIPTION("Freescale i.MX CS42L52 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-cs42l52");
