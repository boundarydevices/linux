/*
 * sound/soc/amlogic/meson/pcm_dai.c
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
#define pr_fmt(fmt) "snd_pcm_dai: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "pcm_dai.h"
#include "pcm.h"
#include "i2s.h"
#include "audio_hw_pcm.h"

#include <linux/of.h>

#define DEV_NAME "aml-pcm-dai"

#define AML_DAI_PCM_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_PCM_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define PCM_DEFAULT_SAMPLERATE 8000
#define PCM_DEFAULT_MCLK_RATIO_SR 256
#define PCM_24BIT_MCLK_RATIO_SR 384
#define PCM_32BIT_MCLK_RATIO_SR 512

static int aml_pcm_set_clk(struct aml_pcm *pcm, unsigned long rate)
{
	int ret = 0;

	ret = clk_set_rate(pcm->clk_mpll, rate * 60);
	if (ret) {
		pr_info("Cannot set pcm mpll\n");
		return ret;
	}
	ret = clk_set_parent(pcm->clk_pcm_mclk, pcm->clk_mpll);
	if (ret) {
		pr_info("Cannot set pcm mclk parent\n");
		return ret;
	}

	ret = clk_set_rate(pcm->clk_pcm_mclk, rate);
	if (ret) {
		pr_info("Cannot set pcm mclk rate:%lu\n", rate);
		return ret;
	}

	return 0;
}

static int aml_pcm_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	pr_debug("***Entered %s\n", __func__);
	return 0;
}

static void aml_pcm_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	pr_debug("***Entered %s\n", __func__);
}

static int aml_pcm_dai_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pcm_runtime_data *prtd = runtime->private_data;
	struct aml_pcm *pcm = snd_soc_dai_get_drvdata(dai);
	int mclk_rate, pcm_bit;

	pr_debug("***Entered %s\n", __func__);

	/* set bclk */
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE)
		pcm_bit = 32;
	else if (runtime->format == SNDRV_PCM_FORMAT_S24_LE)
		pcm_bit = 24;
	else
		pcm_bit = 16;

	mclk_rate = runtime->rate * pcm_bit * runtime->channels;
	pr_info("%s rate:%d, bits:%d, channels:%d, mclk:%d\n",
		__func__,
		runtime->rate,
		pcm_bit,
		runtime->channels,
		mclk_rate);

	aml_pcm_set_clk(pcm, mclk_rate);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info(
		       "%s playback stream buffer start: %ld size: 0x%x\n",
		       __func__, (long)prtd->buffer_start, prtd->buffer_size);
		pcm_out_set_buf(prtd->buffer_start, prtd->buffer_size);
	} else {
		pr_info(
		       "%s capture stream buffer start: %ld size: 0x%x\n",
		       __func__, (long)prtd->buffer_start, prtd->buffer_size);
		pcm_in_set_buf(prtd->buffer_start, prtd->buffer_size);
	}

	memset(runtime->dma_area, 0, runtime->dma_bytes);
	prtd->buffer_offset = 0;
	prtd->data_size = 0;
	prtd->period_elapsed = 0;

	return 0;
}

static int aml_pcm_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	struct aml_pcm *pcm_p = dev_get_drvdata(dai->dev);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* TODO */
		if (pcm_p && pcm_p->pcm_mode) {
			pr_info("aiu pcm master stream %d enable\n\n",
				substream->stream);
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				pcm_master_out_enable(substream, 1);
			else
				pcm_master_in_enable(substream, 1);
		} else {
			pr_info("aiu slave pcm stream %d enable\n\n",
				substream->stream);
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				pcm_out_enable(substream, 1);
			else
				pcm_in_enable(substream, 1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (pcm_p && pcm_p->pcm_mode) {
			pr_info("aiu master pcm stream %d disable\n\n",
				substream->stream);
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				pcm_master_out_enable(substream, 0);
			else
				pcm_master_in_enable(substream, 0);
		} else {
			pr_info("aiu slave pcm stream %d disable\n\n",
				substream->stream);
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				pcm_out_enable(substream, 0);
			else
				pcm_in_enable(substream, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_pcm_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	pr_debug("***Entered %s:%s\n", __FILE__, __func__);
	return 0;
}

static int aml_pcm_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_debug("***Entered %s\n", __func__);

	aml_set_pcm_format(fmt & SND_SOC_DAIFMT_FORMAT_MASK);

	return 0;
}

static int aml_pcm_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	pr_debug("***Entered %s\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
static int aml_pcm_dai_suspend(struct snd_soc_dai *dai)
{

	pr_debug("***Entered %s\n", __func__);
	return 0;
}

static int aml_pcm_dai_resume(struct snd_soc_dai *dai)
{
	pr_debug("***Entered %s\n", __func__);
	return 0;
}

#else				/* CONFIG_PM */
#define aml_pcm_dai_suspend	NULL
#define aml_pcm_dai_resume	NULL
#endif				/* CONFIG_PM */


static struct snd_soc_dai_ops aml_pcm_dai_ops = {
	.startup = aml_pcm_dai_startup,
	.shutdown = aml_pcm_dai_shutdown,
	.prepare = aml_pcm_dai_prepare,
	.trigger = aml_pcm_dai_trigger,
	.hw_params = aml_pcm_dai_hw_params,
	.set_fmt = aml_pcm_dai_set_fmt,
	.set_sysclk = aml_pcm_dai_set_sysclk,
};

struct snd_soc_dai_driver aml_pcm_dai[] = {
	{
	 .suspend = aml_pcm_dai_suspend,
	 .resume = aml_pcm_dai_resume,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 16,
		      .rates = AML_DAI_PCM_RATES,
		      .formats = AML_DAI_PCM_FORMATS,},
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 16,
		     .rates = AML_DAI_PCM_RATES,
		     .formats = AML_DAI_PCM_FORMATS,},
	 .ops = &aml_pcm_dai_ops,
	 },

};
EXPORT_SYMBOL_GPL(aml_pcm_dai);

static const struct snd_soc_component_driver aml_component = {
	.name = DEV_NAME,
};

static int aml_pcm_dai_probe(struct platform_device *pdev)
{
	struct pinctrl *pin_ctl;
	struct aml_pcm *pcm_p = NULL;
	int ret;

	pr_debug("enter %s\n", __func__);

	pin_ctl = devm_pinctrl_get_select(&pdev->dev, "audio_pcm");
	if (IS_ERR(pin_ctl)) {
		pin_ctl = NULL;
		pr_err("aml audio pcm dai pinmux set error!\n");
	}

	pcm_p = devm_kzalloc(&pdev->dev, sizeof(struct aml_pcm), GFP_KERNEL);
	if (!pcm_p) {
		/*dev_err(&pdev->dev, "Can't allocate pcm_p\n");*/
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, pcm_p);

	/* is PCM master? */
	ret =
	    of_property_read_u32((&pdev->dev)->of_node, "pcm_mode",
				 &pcm_p->pcm_mode);

	pr_info("pcm mode detection =%d\n", pcm_p->pcm_mode);

	if (pcm_p->pcm_mode) {
		pcm_p->clk_mpll = devm_clk_get(&pdev->dev, "mpll0");
		if (IS_ERR(pcm_p->clk_mpll)) {
			dev_err(&pdev->dev, "Can't retrieve mpll0 clock\n");
			ret = PTR_ERR(pcm_p->clk_mpll);
			goto err;
		}

		pcm_p->clk_pcm_mclk = devm_clk_get(&pdev->dev, "pcm_mclk");
		if (IS_ERR(pcm_p->clk_pcm_mclk)) {
			dev_err(&pdev->dev, "Can't retrieve clk_pcm_mclk clock\n");
			ret = PTR_ERR(pcm_p->clk_pcm_mclk);
			goto err;
		}
		pcm_p->clk_pcm_sync = devm_clk_get(&pdev->dev, "pcm_sclk");
		if (IS_ERR(pcm_p->clk_pcm_sync)) {
			dev_err(&pdev->dev, "Can't retrieve clk_pcm_sync clock\n");
			ret = PTR_ERR(pcm_p->clk_pcm_sync);
			goto err;
		}

		/* Default 256fs */
		ret = aml_pcm_set_clk(pcm_p,
			PCM_DEFAULT_SAMPLERATE * PCM_DEFAULT_MCLK_RATIO_SR);
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't set aml_pcm clk :%d\n", ret);
			goto err;
		}

		ret = clk_prepare_enable(pcm_p->clk_pcm_mclk);
		if (ret) {
			dev_err(&pdev->dev,
			"Can't enable pcm clk_pcm_mclk clock: %d\n", ret);
			goto err;
		}
		ret = clk_prepare_enable(pcm_p->clk_pcm_sync);
		if (ret) {
			dev_err(&pdev->dev,
			"Can't enable pcm clk_pcm_sync clock: %d\n", ret);
			goto err;
		}
	}

	ret = snd_soc_register_component(&pdev->dev, &aml_component,
					  aml_pcm_dai, ARRAY_SIZE(aml_pcm_dai));

err:
	return ret;

}

static int aml_pcm_dai_remove(struct platform_device *pdev)
{
	struct aml_pcm *pcm_priv = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(pcm_priv->clk_pcm_mclk);
	clk_disable_unprepare(pcm_priv->clk_pcm_sync);

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static void aml_pcm_dai_plat_shutdown(struct platform_device *pdev)
{
	struct aml_pcm *pcm_priv = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(pcm_priv->clk_pcm_mclk);
	clk_disable_unprepare(pcm_priv->clk_pcm_sync);

	snd_soc_unregister_component(&pdev->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_pcm_dai_match[] = {
	{.compatible = "amlogic, aml-pcm-dai",
	 },
	{},
};
#else
#define amlogic_pcm_dai_match NULL
#endif

static struct platform_driver aml_pcm_dai_driver = {
	.driver = {
		   .name = DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_pcm_dai_match,
		   },

	.probe = aml_pcm_dai_probe,
	.remove = aml_pcm_dai_remove,
	.shutdown = aml_pcm_dai_plat_shutdown,
};

static int __init aml_pcm_dai_modinit(void)
{
	return platform_driver_register(&aml_pcm_dai_driver);
}

module_init(aml_pcm_dai_modinit);

static void __exit aml_pcm_dai_modexit(void)
{
	platform_driver_unregister(&aml_pcm_dai_driver);
}

module_exit(aml_pcm_dai_modexit);

/* Module information */
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML DAI driver for ALSA");
MODULE_LICENSE("GPL");
