/*
 * Copyright (C) 2008-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fsl_devices.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <mach/ssi.h>

#include "imx-ssi.h"
#include "imx-audmux.h"

#define SI4763_SYSCLK	24576000

static struct imx_si4763_priv {
	int sysclk;
	int hw;
	int active;
	struct platform_device *pdev;
	struct platform_device *codec_dev;
} card_priv;

static struct platform_device *imx_3stack_snd_device;

static int imx_audmux_config(int slave, int master)
{
	unsigned int ptcr, pdcr;
	slave = slave - 1;
	master = master - 1;

	ptcr = IMX_AUDMUX_V2_PTCR_SYN |
		IMX_AUDMUX_V2_PTCR_TFSDIR |
		IMX_AUDMUX_V2_PTCR_TFSEL(slave) |
		IMX_AUDMUX_V2_PTCR_TCLKDIR |
		IMX_AUDMUX_V2_PTCR_TCSEL(slave);
	pdcr = IMX_AUDMUX_V2_PDCR_RXDSEL(slave);
	imx_audmux_v2_configure_port(master, ptcr, pdcr);

	ptcr = IMX_AUDMUX_V2_PTCR_SYN;
	pdcr = IMX_AUDMUX_V2_PDCR_RXDSEL(master);
	imx_audmux_v2_configure_port(slave, ptcr, pdcr);

	return 0;
}


static int imx_3stack_si4763_startup(struct snd_pcm_substream *substream)
{
	struct imx_si4763_priv *priv = &card_priv;

	priv->active++;
	return 0;
}

static int imx_3stack_si4763_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int channels = params_channels(params);
	struct imx_ssi *ssi_mode = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;
	u32 dai_format;
	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS;

	ssi_mode->flags |= IMX_SSI_SYN;

	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai,
			channels == 1 ? 0xfffffffe : 0xfffffffc,
			channels == 1 ? 0xfffffffe : 0xfffffffc,
			2, 32);

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, dai_format);
	if (ret < 0)
		return ret;

	/* set the SSI system clock as input (unused) */
	snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);

	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_PM, 5);
	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_2, 1);
	snd_soc_dai_set_clkdiv(cpu_dai, IMX_SSI_TX_DIV_PSR, 0);
	return 0;
}

static void imx_3stack_si4763_shutdown(struct snd_pcm_substream *substream)
{
	struct imx_si4763_priv *priv = &card_priv;
	priv->active--;
}

/*
 * imx_3stack bt DAI opserations.
 */
static struct snd_soc_ops imx_3stack_si4763_ops = {
	.startup = imx_3stack_si4763_startup,
	.hw_params = imx_3stack_si4763_hw_params,
	.shutdown = imx_3stack_si4763_shutdown,
};

static struct snd_soc_dai_link imx_3stack_dai = {
	.name = "imx-si4763",
	.stream_name = "imx-si4763",
	.codec_dai_name = "si4763",
	.codec_name     = "si4763",
	.ops = &imx_3stack_si4763_ops,
};

static struct snd_soc_card snd_soc_card_imx_3stack = {
	.name = "imx-audio-si4763",
	.dai_link = &imx_3stack_dai,
	.num_links = 1,
};

static int __devinit imx_3stack_si4763_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *audmux_np;
	struct platform_device *ssi_pdev;
	struct imx_si4763_priv *priv = &card_priv;
	struct snd_soc_card *card = &snd_soc_card_imx_3stack;
	struct property *poldbase;
	int int_port, ext_port;
	char platform_name[32];
	int ret;

	/*
	 * Check the existence of pinctrl-0 in audmux node
	 * In default, pinctrl-0 should be available.
	 * If not, that means board code deleted the property
	 * and it didn't want to initialize si4763.
	 */
	audmux_np = of_find_node_by_path("/soc/aips-bus@02100000/audmux@021d8000");
	if (!audmux_np) {
		dev_err(&pdev->dev, "Failed to find audmux node\n");
		return -EINVAL;
	}

	poldbase = of_find_property(audmux_np, "pinctrl-0", 0);
	if (!poldbase) {
		dev_err(&pdev->dev, "tunner isn't enabled.\n");
		return -EINVAL;
	}

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

	imx_audmux_config(int_port, ext_port);

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	if (!ssi_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		return -EINVAL;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EINVAL;
		goto err1;
	}

	priv->sysclk = SI4763_SYSCLK;

	priv->codec_dev = platform_device_register_simple("si4763", -1, NULL, 0);
	if (!priv->codec_dev) {
		dev_err(&pdev->dev, "Failed to register si4763 codec\n");
		goto err1;
	}

	card->dev = &pdev->dev;
	card->dai_link->cpu_dai_name = dev_name(&ssi_pdev->dev);
	card->dai_link->codec_name = dev_name(&priv->codec_dev->dev);

	sprintf(platform_name, "imx-pcm-audio.%d", of_alias_get_id(ssi_np, "audio"));
	card->dai_link->platform_name = platform_name;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card: %d\n", ret);
		goto err2;
	}

	return 0;
err2:
	platform_device_unregister(priv->codec_dev);
err1:
	if (ssi_np)
		of_node_put(ssi_np);

	return ret;
}

static int __devexit imx_3stack_si4763_remove(struct platform_device *pdev)
{
	struct imx_si4763_priv *priv = &card_priv;

	platform_device_unregister(priv->codec_dev);
	platform_device_unregister(imx_3stack_snd_device);

	return 0;
}


static const struct of_device_id imx_si4763_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-si4763", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_si4763_dt_ids);

static struct platform_driver imx_si4763_driver = {
	.driver = {
		.name = "imx-tuner-si4763",
		.owner = THIS_MODULE,
		.of_match_table = imx_si4763_dt_ids,
	},
	.probe = imx_3stack_si4763_probe,
	.remove = __devexit_p(imx_3stack_si4763_remove),
};

module_platform_driver(imx_si4763_driver);

/* Module information */
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("ALSA SoC si4763 imx");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:imx-si4763");
