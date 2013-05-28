/*
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
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
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>

#include "../codecs/wm8962.h"
#include "imx-audmux.h"

#define IMX_WM8962_MCLK_RATE 24000000

#define DAI_NAME_SIZE	32

struct imx_wm8962_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	unsigned int clk_frequency;
};

struct imx_priv {
	int hp_gpio;
	int hp_active_low;
	int hp_status;
	int amic_gpio;
	int amic_active_low;
	int amic_status;
	struct platform_device *pdev;
	struct snd_pcm_substream *first_stream;
	struct snd_pcm_substream *second_stream;
};
static struct imx_priv card_priv;
static struct snd_soc_codec *gcodec;

static struct snd_soc_jack imx_hp_jack;
static struct snd_soc_jack_pin imx_hp_jack_pins[] = {
	{
		.pin = "Ext Spk",
		.mask = SND_JACK_HEADPHONE,
	},
};
static struct snd_soc_jack_gpio imx_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 150,
	.invert = 0,
};

static struct snd_soc_jack imx_mic_jack;
static struct snd_soc_jack_pin imx_mic_jack_pins[] = {
	{
		.pin = "DMIC",
		.mask = SND_JACK_MICROPHONE,
	},
};
static struct snd_soc_jack_gpio imx_mic_jack_gpio = {
	.name = "micphone detect",
	.report = SND_JACK_MICROPHONE,
	.debounce_time = 150,
	.invert = 0,
};

static void imx_resume_event(struct work_struct *wor)
{
	struct imx_priv *priv = &card_priv;
	struct snd_soc_jack *jack;
	int enable;
	int report;

	if (priv->hp_gpio != -1) {
		jack = imx_hp_jack_gpio.jack;

		enable = gpio_get_value_cansleep(imx_hp_jack_gpio.gpio);
		if (imx_hp_jack_gpio.invert)
			enable = !enable;

		if (enable)
			report = imx_hp_jack_gpio.report;
		else
			report = 0;

		snd_soc_jack_report(jack, report, imx_hp_jack_gpio.report);
	}

	if (priv->amic_gpio != -1) {
		jack = imx_mic_jack_gpio.jack;

		enable = gpio_get_value_cansleep(imx_mic_jack_gpio.gpio);
		if (imx_mic_jack_gpio.invert)
			enable = !enable;

		if (enable)
			report = imx_mic_jack_gpio.report;
		else
			report = 0;

		snd_soc_jack_report(jack, report, imx_mic_jack_gpio.report);
	}

	return;
}

static int imx_event_hp(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct imx_priv *priv = &card_priv;
	struct platform_device *pdev = priv->pdev;
	char *envp[3];
	char *buf;

	if (priv->hp_gpio != -1) {
		priv->hp_status = gpio_get_value(priv->hp_gpio) ? 1 : 0;

		buf = kmalloc(32, GFP_ATOMIC);
		if (!buf) {
			pr_err("%s kmalloc failed\n", __func__);
			return -ENOMEM;
		}

		if (priv->hp_status != priv->hp_active_low)
			snprintf(buf, 32, "STATE=%d", 2);
		else
			snprintf(buf, 32, "STATE=%d", 0);

		envp[0] = "NAME=headphone";
		envp[1] = buf;
		envp[2] = NULL;
		kobject_uevent_env(&pdev->dev.kobj, KOBJ_CHANGE, envp);
		kfree(buf);
	}

	return 0;
}

static int imx_event_mic(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct imx_priv *priv = &card_priv;
	struct platform_device *pdev = priv->pdev;
	char *envp[3];
	char *buf;

	if (priv->amic_gpio != -1) {
		priv->amic_status = gpio_get_value(priv->amic_gpio) ? 1 : 0;

		buf = kmalloc(32, GFP_ATOMIC);
		if (!buf) {
			pr_err("%s kmalloc failed\n", __func__);
			return -ENOMEM;
		}

		if (priv->amic_status == 0)
			snprintf(buf, 32, "STATE=%d", 2);
		else
			snprintf(buf, 32, "STATE=%d", 0);

		envp[0] = "NAME=amic";
		envp[1] = buf;
		envp[2] = NULL;
		kobject_uevent_env(&pdev->dev.kobj, KOBJ_CHANGE, envp);
		kfree(buf);
	}

	return 0;
}

static const struct snd_kcontrol_new controls[] = {
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
};

static const struct snd_soc_dapm_widget imx_wm8962_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", imx_event_hp),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("DMIC", imx_event_mic),
};

static ssize_t show_headphone(struct device_driver *dev, char *buf)
{
	struct imx_priv *priv = &card_priv;

	/* determine whether hp is plugged in */
	priv->hp_status = gpio_get_value(priv->hp_gpio) ? 1 : 0;

	if (priv->hp_status != priv->hp_active_low)
		strcpy(buf, "headphone\n");
	else
		strcpy(buf, "speaker\n");

	return strlen(buf);
}

static DRIVER_ATTR(headphone, S_IRUGO | S_IWUSR, show_headphone, NULL);

static ssize_t show_amic(struct device_driver *dev, char *buf)
{
	struct imx_priv *priv = &card_priv;

	/* determine whether amic is plugged in */
	priv->amic_status = gpio_get_value(priv->amic_gpio) ? 1 : 0;

	if (priv->amic_status != priv->amic_active_low)
		strcpy(buf, "amic\n");
	else
		strcpy(buf, "dmic\n");

	return strlen(buf);
}

static DRIVER_ATTR(amic, S_IRUGO | S_IWUSR, show_amic, NULL);

static DECLARE_DELAYED_WORK(resume_hp_event, imx_resume_event);

int imx_hifi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct imx_priv *priv = &card_priv;

	if (SNDRV_PCM_TRIGGER_RESUME == cmd) {
		if ((priv->hp_gpio != -1) || (priv->amic_gpio != -1))
			schedule_delayed_work(&resume_hp_event,
				msecs_to_jiffies(200));
	}

	return 0;
}

static int imx_wm8962_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct imx_priv *priv = &card_priv;
	struct platform_device *pdev = priv->pdev;
	int ret;

	gcodec = rtd->codec;

	if (priv->hp_gpio != -1) {
		imx_hp_jack_gpio.gpio = priv->hp_gpio;
		snd_soc_jack_new(codec, "Ext Spk", SND_JACK_LINEOUT,
				&imx_hp_jack);
		snd_soc_jack_add_pins(&imx_hp_jack,
					ARRAY_SIZE(imx_hp_jack_pins),
					imx_hp_jack_pins);
		snd_soc_jack_add_gpios(&imx_hp_jack,
					1, &imx_hp_jack_gpio);

		ret = driver_create_file(pdev->dev.driver,
						&driver_attr_headphone);
		if (ret < 0) {
			ret = -EINVAL;
			return ret;
		}
	}

	if (priv->amic_gpio != -1) {
		imx_mic_jack_gpio.gpio = priv->amic_gpio;
		snd_soc_jack_new(codec, "DMIC", SND_JACK_MICROPHONE,
				&imx_mic_jack);
		snd_soc_jack_add_pins(&imx_mic_jack,
					ARRAY_SIZE(imx_mic_jack_pins),
					imx_mic_jack_pins);
		snd_soc_jack_add_gpios(&imx_mic_jack,
					1, &imx_mic_jack_gpio);

		ret = driver_create_file(pdev->dev.driver, &driver_attr_amic);
		if (ret < 0) {
			ret = -EINVAL;
			return ret;
		}
	} else {
		snd_soc_dapm_nc_pin(&codec->dapm, "DMIC");
	}

	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static int check_hw_params(struct snd_pcm_substream *substream,
		snd_pcm_format_t sample_format,
		unsigned int sample_rate)
{
	struct imx_priv *priv = &card_priv;

	substream->runtime->sample_bits =
		snd_pcm_format_physical_width(sample_format);
	substream->runtime->rate = sample_rate;
	substream->runtime->format = sample_format;

	if (!priv->first_stream) {
		priv->first_stream = substream;
	} else {
		priv->second_stream = substream;

		if (priv->first_stream->runtime->rate !=
				priv->second_stream->runtime->rate) {
			dev_err(substream->pcm->card->dev,
					"\n!KEEP THE SAME SAMPLE RATE: %d!\n",
					priv->first_stream->runtime->rate);
			return -EINVAL;
		}
		if (priv->first_stream->runtime->sample_bits !=
				priv->second_stream->runtime->sample_bits) {
			snd_pcm_format_t first_format =
				priv->first_stream->runtime->format;

			dev_err(substream->pcm->card->dev,
					"\n!KEEP THE SAME FORMAT: %s!\n",
					snd_pcm_format_name(first_format));
			return -EINVAL;
		}
	}
	return 0;
}

static int imx_hifi_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	struct imx_wm8962_data *data = platform_get_drvdata(priv->pdev);
	unsigned int sample_rate = params_rate(params);
	snd_pcm_format_t sample_format = params_format(params);
	int ret = 0;
	u32 dai_format;
	unsigned int pll_out;

	/*
	 * WM8962 doesn't support two substreams in different sample rates or
	 * sample formats. So check the two parameters of two substreams' if
	 * there are two substream for playback and capture in the same time.
	 */
	ret = check_hw_params(substream, sample_format, sample_rate);
	if (ret < 0) {
		pr_err("Failed to match hw params: %d\n", ret);
		return ret;
	}

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, dai_format);
	if (ret < 0) {
		pr_err("Failed to set codec dai fmt: %d\n", ret);
		return ret;
	}

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_IF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret < 0) {
		pr_err("Failed to set cpu dai fmt: %d\n", ret);
		return ret;
	}

	if (sample_format == SNDRV_PCM_FORMAT_S24_LE)
		pll_out = sample_rate * 192;
	else
		pll_out = sample_rate * 256;

	ret = snd_soc_dai_set_pll(codec_dai, WM8962_FLL_MCLK,
				  WM8962_FLL_MCLK, data->clk_frequency,
				  pll_out);
	if (ret < 0)
		pr_err("Failed to start FLL: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai,
					 WM8962_SYSCLK_FLL,
					 pll_out,
					 SND_SOC_CLOCK_IN);
	if (ret < 0) {
		pr_err("Failed to set SYSCLK: %d\n", ret);
		return ret;
	}

	return 0;
}

static int imx_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	int ret;

	/* We don't need to handle anything if there's no substream running */
	if (!priv->first_stream)
		return 0;

	if (priv->first_stream == substream)
		priv->first_stream = priv->second_stream;
	priv->second_stream = NULL;

	if (!priv->first_stream) {
		/*
		 * WM8962 doesn't allow us to continuously setting FLL,
		 * So we set MCLK as sysclk once, which'd remove the limitation.
		 */
		ret = snd_soc_dai_set_sysclk(codec_dai, WM8962_SYSCLK_MCLK,
				0, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			pr_err("Failed to set SYSCLK: %d\n", ret);
			return ret;
		}

		/*
		 * Continuously setting FLL would cause playback distortion.
		 * We can fix it just by mute codec after playback.
		 */
		ret = snd_soc_dai_digital_mute(codec_dai, 1);
		if (ret < 0) {
			pr_err("Failed to set MUTE: %d\n", ret);
			return ret;
		}

		/* Disable FLL and let codec do pm_runtime_put() */
		ret = snd_soc_dai_set_pll(codec_dai, WM8962_FLL,
				WM8962_FLL_MCLK, 0, 0);
		if (ret < 0) {
			pr_err("Failed to set FLL: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static struct snd_soc_ops imx_hifi_ops = {
	.hw_params = imx_hifi_hw_params,
	.hw_free = imx_hifi_hw_free,
	.trigger = imx_hifi_trigger,
};

static int __devinit imx_wm8962_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct imx_priv *priv = &card_priv;
	struct i2c_client *codec_dev;
	struct imx_wm8962_data *data;
	int int_port, ext_port;
	char platform_name[32];
	int ret;

	priv->pdev = pdev;

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
	imx_audmux_v2_configure_port(ext_port,
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
		ret = -EINVAL;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev || !codec_dev->driver) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EINVAL;
	}

	priv->hp_gpio = of_get_named_gpio_flags(np, "hp-det-gpios", 0,
				(enum of_gpio_flags *)&priv->hp_active_low);
	priv->amic_gpio = of_get_named_gpio_flags(np, "mic-det-gpios", 0,
				(enum of_gpio_flags *)&priv->amic_active_low);

	priv->first_stream = NULL;
	priv->second_stream = NULL;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->clk_frequency = IMX_WM8962_MCLK_RATE;

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "wm8962";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_dai_name = dev_name(&ssi_pdev->dev);

	sprintf(platform_name, "imx-pcm-audio.%d", of_alias_get_id(ssi_np, "audio"));
	data->dai.platform_name = platform_name;
	data->dai.init = &imx_wm8962_dai_init;
	data->dai.ops = &imx_hifi_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_wm8962_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_wm8962_dapm_widgets);

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, data);

	return 0;

fail:
	if (ssi_np)
		of_node_put(ssi_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int __devexit imx_wm8962_remove(struct platform_device *pdev)
{
	struct imx_wm8962_data *data = platform_get_drvdata(pdev);

	snd_soc_unregister_card(&data->card);

	return 0;
}

static const struct of_device_id imx_wm8962_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-wm8962", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_wm8962_dt_ids);

static struct platform_driver imx_wm8962_driver = {
	.driver = {
		.name = "imx-wm8962",
		.owner = THIS_MODULE,
		.of_match_table = imx_wm8962_dt_ids,
	},
	.probe = imx_wm8962_probe,
	.remove = __devexit_p(imx_wm8962_remove),
};
module_platform_driver(imx_wm8962_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX WM8962 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-wm8962");
