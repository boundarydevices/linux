/*
 * sound/soc/amlogic/auge/extn.c
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
 * Audio External Input/Out drirver
 * such as fratv, frhdmirx
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "ddr_mngr.h"
#include "audio_utils.h"
#include "frhdmirx_hw.h"

#define DRV_NAME "EXTN"

struct extn {
	struct aml_audio_controller *actrl;
	struct device *dev;
	unsigned int sysclk_freq;

	int irq_frhdmirx;

	struct toddr *tddr;
	struct frddr *fddr;
};

#define PREALLOC_BUFFER		(32 * 1024)
#define PREALLOC_BUFFER_MAX	(256 * 1024)

#define EXTN_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EXTN_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware extn_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,

	.formats = EXTN_FORMATS,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 256 * 1024,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static irqreturn_t extn_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static irqreturn_t frhdmirx_isr(int irq, void *devid)
{
	return IRQ_HANDLED;
}

static int extn_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct extn *p_extn;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	p_extn = (struct extn *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &extn_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_extn->fddr = aml_audio_register_frddr(dev,
			p_extn->actrl,
			extn_ddr_isr, substream);
		if (p_extn->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr\n");
			return -ENXIO;
		}
	} else {
		p_extn->tddr = aml_audio_register_toddr(dev,
			p_extn->actrl,
			extn_ddr_isr, substream);
		if (p_extn->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr\n");
			return -ENXIO;
		}

		if (toddr_src_get() == FRHDMIRX) {
			int ret = request_irq(p_extn->irq_frhdmirx,
					frhdmirx_isr, 0, "irq_frhdmirx",
					p_extn);
			if (ret) {
				dev_err(p_extn->dev, "failed to claim irq_frhdmirx %u\n",
							p_extn->irq_frhdmirx);
				return -ENXIO;
			}
		}
	}

	runtime->private_data = p_extn;

	return 0;
}

static int extn_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_audio_unregister_frddr(p_extn->dev, substream);
	else {
		aml_audio_unregister_toddr(p_extn->dev, substream);

		if (toddr_src_get() == FRHDMIRX)
			free_irq(p_extn->irq_frhdmirx, p_extn);
	}
	runtime->private_data = NULL;

	return 0;
}

static int extn_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int extn_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int extn_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static int extn_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - 8;
	int_addr = frames_to_bytes(runtime, runtime->period_size) / 8;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_extn->fddr;

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_extn->tddr;

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t extn_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_extn->fddr);
	else
		addr = aml_toddr_get_position(p_extn->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

int extn_silence(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *ppos;
	int n;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

static int extn_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops extn_ops = {
	.open      = extn_open,
	.close     = extn_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = extn_hw_params,
	.hw_free   = extn_hw_free,
	.prepare   = extn_prepare,
	.trigger   = extn_trigger,
	.pointer   = extn_pointer,
	.silence   = extn_silence,
	.mmap      = extn_mmap,
};

static int extn_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(
			rtd->pcm, SNDRV_DMA_TYPE_DEV,
			rtd->card->snd_card->dev,
			PREALLOC_BUFFER, PREALLOC_BUFFER_MAX);
}

struct snd_soc_platform_driver extn_platform = {
	.ops = &extn_ops,
	.pcm_new = extn_new,
};

static int extn_dai_probe(struct snd_soc_dai *cpu_dai)
{
	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	return 0;
}

static int extn_dai_remove(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int extn_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static void extn_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
}

static int extn_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_extn->fddr;
		enum frddr_dest dst = frddr_src_get();

		pr_info("%s Expected frddr dst:%s\n",
			__func__,
			frddr_src_get_str(dst));

		aml_frddr_select_dst(fr, dst);
		aml_frddr_set_fifos(fr, 0x40, 0x20);
	} else {
		struct toddr *to = p_extn->tddr;
		unsigned int msb = 32 - 1;
		unsigned int lsb = 32 - bit_depth;
		unsigned int toddr_type;
		unsigned int src = toddr_src_get();
		struct toddr_fmt fmt;

		switch (bit_depth) {
		case 8:
		case 16:
		case 32:
			toddr_type = 0;
			break;
		case 24:
			toddr_type = 4;
			break;
		default:
			pr_err("invalid bit_depth: %d\n", bit_depth);
			return -EINVAL;
		}

		pr_info("%s Expected toddr src:%s\n",
			__func__,
			toddr_src_get_str(src));

		if (src == FRATV)
			fratv_src_select(0);
		else if (src == FRHDMIRX) {
			frhdmirx_ctrl(runtime->channels, 0);
			frhdmirx_src_select(0);
		}

		fmt.type      = toddr_type;
		fmt.msb       = msb;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);
		aml_toddr_set_fifos(to, 0x40);
	}

	return 0;
}

static int extn_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int src = toddr_src_get();

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "External Playback enable\n");

			aml_frddr_enable(p_extn->fddr, true);
		} else {
			dev_info(substream->pcm->card->dev, "External Capture enable\n");

			if (src == FRATV)
				fratv_enable(true);
			else if (src == FRHDMIRX)
				frhdmirx_enable(true);

			aml_toddr_enable(p_extn->tddr, true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "External Playback disable\n");

			aml_frddr_enable(p_extn->fddr, false);
		} else {
			dev_info(substream->pcm->card->dev, "External Capture disable\n");

			if (src == FRATV)
				fratv_enable(false);
			else if (src == FRHDMIRX)
				frhdmirx_enable(false);

			aml_toddr_enable(p_extn->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int extn_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	int ret = 0;

	pr_info("%s:rate:%d, sysclk:%d\n",
		__func__,
		rate,
		p_extn->sysclk_freq);

	return ret;
}

static int extn_dai_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc extn_dai_set_fmt, %#x, %p\n", fmt, p_extn);

	return 0;
}

static int extn_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct extn *p_extn = snd_soc_dai_get_drvdata(cpu_dai);

	p_extn->sysclk_freq = freq;
	pr_info("extn_dai_set_sysclk, %d, %d, %d\n",
			clk_id, freq, dir);

	return 0;
}

static struct snd_soc_dai_ops extn_dai_ops = {
	.startup = extn_dai_startup,
	.shutdown = extn_dai_shutdown,
	.prepare = extn_dai_prepare,
	.trigger = extn_dai_trigger,
	.hw_params = extn_dai_hw_params,
	.set_fmt = extn_dai_set_fmt,
	.set_sysclk = extn_dai_set_sysclk,
};

static struct snd_soc_dai_driver extn_dai[] = {
	{
		.name = "EXTN",
		.id = 0,
		.probe = extn_dai_probe,
		.remove = extn_dai_remove,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = EXTN_RATES,
		      .formats = EXTN_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = EXTN_RATES,
		     .formats = EXTN_FORMATS,
		},
		.ops = &extn_dai_ops,
	},
};

static const struct snd_soc_component_driver extn_component = {
	.name		= DRV_NAME,
};

static const struct of_device_id extn_device_id[] = {
	{
		.compatible = "amlogic, snd-extn",
	},
	{},
};

MODULE_DEVICE_TABLE(of, extn_device_id);

static int extn_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct extn *extn = NULL;
	int ret = 0;


	extn = devm_kzalloc(dev, sizeof(struct extn), GFP_KERNEL);
	if (!extn)
		return -ENOMEM;

	extn->dev = dev;
	dev_set_drvdata(dev, extn);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	extn->actrl = actrl;

	/* irqs */
	extn->irq_frhdmirx = platform_get_irq_byname(pdev, "irq_frhdmirx");
	if (extn->irq_frhdmirx < 0) {
		dev_err(dev, "Failed to get irq_frhdmirx:%d\n",
			extn->irq_frhdmirx);
		return -ENXIO;
	}

	ret = snd_soc_register_component(&pdev->dev,
				&extn_component,
				extn_dai,
				ARRAY_SIZE(extn_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &extn_platform);
}

struct platform_driver extn_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = extn_device_id,
	},
	.probe = extn_platform_probe,
};
module_platform_driver(extn_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic External Input/Output ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, extn_device_id);
