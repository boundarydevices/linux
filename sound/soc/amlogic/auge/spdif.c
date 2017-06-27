/*
 * sound/soc/amlogic/auge/spdif.c
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>


#include "ddr_mngr.h"
#include "spdif_hw.h"


#define DRV_NAME "aml_spdif"

struct aml_spdif {
	struct pinctrl *pin_ctl;
	struct aml_audio_controller *actrl;
	struct device *dev;
	struct clk *gate_spdifin;
	struct clk *gate_spdifout;
	struct clk *sysclk;
	struct clk *fixed_clk;
	struct clk *clk_spdifin;
	struct clk *clk_spdifout;
	unsigned int sysclk_freq;
	/* bclk src selection */
	int irq_toddr;
	int irq_frddr;
	int irq_spdifin;
	unsigned int from_ddr_num;
	unsigned int to_ddr_num;
	struct toddr *tddr;
	struct frddr *fddr;
};

static const struct snd_pcm_hardware aml_spdif_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 256 * 1024,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 32,
};

static irqreturn_t aml_spdifout_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static irqreturn_t aml_spdifin_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

/* detect PCM/RAW and sample changes by the source */
static irqreturn_t aml_spdifin_status_isr(int irq, void *devid)
{
	struct aml_spdif *p_spdif = (struct aml_spdif *)devid;

	aml_spdifin_status_check(p_spdif->actrl);

	return IRQ_HANDLED;
}

static int aml_spdif_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct aml_spdif *p_spdif;
	int ret = 0;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	p_spdif = (struct aml_spdif *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &aml_spdif_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_spdif->fddr = aml_audio_register_frddr(dev,
			p_spdif->actrl,
			p_spdif->from_ddr_num);
		if (p_spdif->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr %u\n",
					p_spdif->from_ddr_num);
			return -ENXIO;
		}

		ret = request_irq(p_spdif->irq_frddr,
			aml_spdifout_isr, 0, "spdifout", substream);
		if (ret) {
			aml_audio_unregister_frddr(dev, p_spdif->from_ddr_num);
			dev_err(p_spdif->dev, "failed to claim irq %u\n",
					p_spdif->irq_frddr);
			return ret;
		}
	} else {
		p_spdif->tddr = aml_audio_register_toddr(dev,
			p_spdif->actrl,
			p_spdif->to_ddr_num);
		if (p_spdif->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr %u\n",
					p_spdif->to_ddr_num);
			return -ENXIO;
		}

		ret = request_irq(p_spdif->irq_toddr,
				aml_spdifin_isr, 0, "spdifin", substream);
		if (ret) {
			aml_audio_unregister_toddr(dev, p_spdif->to_ddr_num);
			dev_err(p_spdif->dev, "failed to claim irq %u\n",
						p_spdif->irq_toddr);
			return ret;
		}

		ret = request_irq(p_spdif->irq_spdifin,
				aml_spdifin_status_isr, 0, "irq_spdifin",
				p_spdif);
		if (ret) {
			dev_err(p_spdif->dev, "failed to claim irq_spdifin %u\n",
						p_spdif->irq_spdifin);
			//return ret;
		}

	}

	runtime->private_data = p_spdif;

	return 0;
}


static int aml_spdif_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aml_audio_unregister_frddr(p_spdif->dev, p_spdif->from_ddr_num);
		free_irq(p_spdif->irq_frddr, substream);
	} else {
		aml_audio_unregister_toddr(p_spdif->dev, p_spdif->to_ddr_num);
		free_irq(p_spdif->irq_toddr, substream);
		free_irq(p_spdif->irq_spdifin, p_spdif);
	}

	runtime->private_data = NULL;

	return 0;
}


static int aml_spdif_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_spdif_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int aml_spdif_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static int aml_spdif_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - 8;
	int_addr = frames_to_bytes(runtime, runtime->period_size) / 8;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_spdif->tddr;

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t aml_spdif_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;
	unsigned int addr, start_addr;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_spdif->fddr);
	else
		addr = aml_toddr_get_position(p_spdif->tddr);

	return bytes_to_frames(runtime, addr - start_addr);
}

int aml_spdif_silence(struct snd_pcm_substream *substream, int channel,
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

static int aml_spdif_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops aml_spdif_ops = {
	.open      = aml_spdif_open,
	.close     = aml_spdif_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = aml_spdif_hw_params,
	.hw_free   = aml_spdif_hw_free,
	.prepare   = aml_spdif_prepare,
	.trigger   = aml_spdif_trigger,
	.pointer   = aml_spdif_pointer,
	.silence   = aml_spdif_silence,
	.mmap      = aml_spdif_mmap,
};

#define PREALLOC_BUFFER		(32 * 1024)
#define PREALLOC_BUFFER_MAX	(256 * 1024)
static int aml_spdif_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(
			rtd->pcm, SNDRV_DMA_TYPE_DEV,
			rtd->card->snd_card->dev,
			PREALLOC_BUFFER, PREALLOC_BUFFER_MAX);
}

struct snd_soc_platform_driver aml_spdif_platform = {
	.ops = &aml_spdif_ops,
	.pcm_new = aml_spdif_new,
};

static int aml_dai_spdif_probe(struct snd_soc_dai *cpu_dai)
{
	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	return 0;
}

static int aml_dai_spdif_remove(struct snd_soc_dai *cpu_dai)
{
	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	return 0;
}

static int aml_dai_spdif_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	aml_spdif_fifo_reset(p_spdif->actrl, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* enable clock gate */

		ret = clk_prepare_enable(p_spdif->gate_spdifout);
		/* enable clock */
		ret = clk_prepare_enable(p_spdif->sysclk);
		if (ret) {
			pr_err("Can't enable pcm sysclk clock: %d\n", ret);
			goto err;
		}
		ret = clk_prepare_enable(p_spdif->clk_spdifout);
		if (ret) {
			pr_err("Can't enable pcm clk_spdifout clock: %d\n",
				ret);
			goto err;
		}

	} else {
		/* enable clock gate */
		ret = clk_prepare_enable(p_spdif->gate_spdifin);
		/* enable clock */
		ret = clk_prepare_enable(p_spdif->fixed_clk);
		if (ret) {
			pr_err("Can't enable pcm fixed_clk clock: %d\n", ret);
			goto err;
		}
		ret = clk_prepare_enable(p_spdif->clk_spdifin);
		if (ret) {
			pr_err("Can't enable pcm clk_spdifin clock: %d\n", ret);
			goto err;
		}
	}

	return 0;
err:
	pr_err("failed enable clock\n");
	return -EINVAL;
}

static void aml_dai_spdif_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);

	/* disable clock and gate */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		clk_disable_unprepare(p_spdif->clk_spdifout);
		clk_disable_unprepare(p_spdif->sysclk);
		clk_disable_unprepare(p_spdif->gate_spdifout);
	} else {
		clk_disable_unprepare(p_spdif->clk_spdifin);
		clk_disable_unprepare(p_spdif->fixed_clk);
		clk_disable_unprepare(p_spdif->gate_spdifin);
	}
}


static int aml_dai_spdif_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = 0;

	pr_info("%s stream:%d\n", __func__, substream->stream);

	bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;

		aml_frddr_select_dst(fr, SPDIFOUT);
		aml_frddr_set_fifos(fr, 0x40, 0x20);
	} else {
		struct toddr *to = p_spdif->tddr;
		unsigned int lsb, toddr_type;

		switch (bit_depth) {
		case 8:
			toddr_type = 0;
			break;
		case 16:
			toddr_type = 1;
			break;
		case 24:
			toddr_type = 4;
			break;
		case 32:
			toddr_type = 3;
			break;
		default:
			dev_err(p_spdif->dev,
				"runtime format invalid bit_depth: %d\n",
				bit_depth);
			return -EINVAL;
		}

		if (bit_depth <= 24)
			lsb = 28 - bit_depth;
		else
			lsb = 4;

		// to ddr spdifin
		aml_toddr_select_src(to, SPDIFIN);
		aml_toddr_set_format(to, toddr_type, 27, lsb);
		aml_toddr_set_fifos(to, 0x40);
	}

	aml_spdif_fifo_ctrl(p_spdif->actrl, bit_depth, substream->stream);

	return 0;
}

static int aml_dai_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "spdif playback enable\n");
			aml_frddr_enable(p_spdif->fddr, 1);
		} else {
			dev_info(substream->pcm->card->dev, "spdif capture enable\n");
			aml_toddr_enable(p_spdif->tddr, 1);
		}
		aml_spdif_enable(p_spdif->actrl,
			substream->stream, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "spdif playback disable\n");
			aml_frddr_enable(p_spdif->fddr, 0);
		} else {
			dev_info(substream->pcm->card->dev, "spdif capture disable\n");
			aml_toddr_enable(p_spdif->tddr, 0);
		}
		aml_spdif_enable(p_spdif->actrl,
			substream->stream, false);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_dai_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	int ret = 0;

	pr_info("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rate *= 128;

		snd_soc_dai_set_sysclk(cpu_dai,
				0, rate, SND_SOC_CLOCK_OUT);
	} else {
		clk_set_rate(p_spdif->clk_spdifin, 250000000);
	}

	return ret;
}

static int aml_dai_set_spdif_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc aml_dai_set_spdif_fmt, %#x, %p\n", fmt, p_spdif);

	return 0;
}

static void aml_set_spdifclk(struct aml_spdif *p_spdif)
{
	unsigned int mpll_freq = 0;

	pr_info("asoc debug: %s-%d, sys freq:%d\n", __func__, __LINE__,
		p_spdif->sysclk_freq);
	if (p_spdif->sysclk_freq) {
		unsigned int mul = 4;

		mpll_freq = p_spdif->sysclk_freq * mul;
		clk_set_rate(p_spdif->sysclk, mpll_freq);
		clk_set_rate(p_spdif->clk_spdifout,
			p_spdif->sysclk_freq);
	}
}

static int aml_dai_set_spdif_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);

	p_spdif->sysclk_freq = freq;
	pr_info("aml_dai_set_spdif_sysclk, %d, %d, %d\n",
			clk_id, freq, dir);

	aml_set_spdifclk(p_spdif);

	return 0;
}

static struct snd_soc_dai_ops aml_dai_spdif_ops = {
	.startup = aml_dai_spdif_startup,
	.shutdown = aml_dai_spdif_shutdown,
	.prepare = aml_dai_spdif_prepare,
	.trigger = aml_dai_spdif_trigger,
	.hw_params = aml_dai_spdif_hw_params,
	.set_fmt = aml_dai_set_spdif_fmt,
	.set_sysclk = aml_dai_set_spdif_sysclk,
};

#define AML_DAI_SPDIF_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_SPDIF_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_spdif_dai = {
	.name = "SPDIF",
	.id = 0,
	.probe = aml_dai_spdif_probe,
	.remove = aml_dai_spdif_remove,
	.playback = {
	      .channels_min = 1,
	      .channels_max = 2,
	      .rates = AML_DAI_SPDIF_RATES,
	      .formats = AML_DAI_SPDIF_FORMATS,
	},
	.capture = {
	     .channels_min = 1,
	     .channels_max = 2,
	     .rates = AML_DAI_SPDIF_RATES,
	     .formats = AML_DAI_SPDIF_FORMATS,
	},
	.ops = &aml_dai_spdif_ops,
};

static const struct snd_soc_component_driver aml_spdif_component = {
	.name		= DRV_NAME,
};

static int aml_spdif_clks_parse_of(struct aml_spdif *p_spdif)
{
	struct device *dev = p_spdif->dev;
	int ret = 0;

	/* clock gate */
	p_spdif->gate_spdifin = devm_clk_get(dev, "gate_spdifin");
	if (IS_ERR(p_spdif->gate_spdifin)) {
		dev_err(dev, "Can't get spdifin gate\n");
		return PTR_ERR(p_spdif->gate_spdifin);
	}

	p_spdif->gate_spdifout = devm_clk_get(dev, "gate_spdifout");
	if (IS_ERR(p_spdif->gate_spdifout)) {
		dev_err(dev, "Can't get spdifout gate\n");
		return PTR_ERR(p_spdif->gate_spdifout);
	}

	p_spdif->sysclk = devm_clk_get(dev, "sysclk");
	if (IS_ERR(p_spdif->sysclk)) {
		dev_err(dev, "Can't retrieve sysclk clock\n");
		return PTR_ERR(p_spdif->sysclk);
	}

	p_spdif->fixed_clk = devm_clk_get(dev, "fixed_clk");
	if (IS_ERR(p_spdif->fixed_clk)) {
		dev_err(dev, "Can't retrieve fixed_clk\n");
		return PTR_ERR(p_spdif->fixed_clk);
	}

	p_spdif->clk_spdifin = devm_clk_get(dev, "clk_spdifin");
	if (IS_ERR(p_spdif->clk_spdifin)) {
		dev_err(dev, "Can't retrieve spdifin clock\n");
		return PTR_ERR(p_spdif->clk_spdifin);
	}

	p_spdif->clk_spdifout = devm_clk_get(dev, "clk_spdifout");
	if (IS_ERR(p_spdif->clk_spdifout)) {
		dev_err(dev, "Can't retrieve spdifout clock\n");
		return PTR_ERR(p_spdif->clk_spdifout);
	}

	ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
	if (ret) {
		dev_err(dev,
			"Can't set clk_spdifin parent clock\n");
		ret = PTR_ERR(p_spdif->clk_spdifin);
		return ret;
	}

	ret = clk_set_parent(p_spdif->clk_spdifout, p_spdif->sysclk);
	if (ret) {
		dev_err(dev,
			"Can't set clk_spdifout parent clock\n");
		ret = PTR_ERR(p_spdif->clk_spdifout);
		return ret;
	}

	return 0;
}

static int aml_spdif_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct aml_spdif *aml_spdif = NULL;
	int ret = 0;

	aml_spdif = devm_kzalloc(dev, sizeof(struct aml_spdif), GFP_KERNEL);
	if (!aml_spdif)
		return -ENOMEM;

	aml_spdif->dev = dev;
	dev_set_drvdata(dev, aml_spdif);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	aml_spdif->actrl = actrl;

	/* parse DTS configured ddr */
	ret = of_property_read_u32(node, "spdif_from_ddr",
			&aml_spdif->from_ddr_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve spdif_from_ddr\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(node, "spdif_to_ddr",
			&aml_spdif->to_ddr_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve spdif_to_ddr\n");
		return -ENXIO;
	}

	ret = aml_spdif_clks_parse_of(aml_spdif);
	if (ret)
		return -EINVAL;

	/* irqs */
	aml_spdif->irq_spdifin = platform_get_irq_byname(pdev, "irq_spdifin");
	if (aml_spdif->irq_spdifin < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");

	aml_spdif->irq_toddr = platform_get_irq_byname(pdev, "irq_toddr");
	if (aml_spdif->irq_toddr < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");

	aml_spdif->irq_frddr = platform_get_irq_byname(pdev, "irq_frddr");
	if (aml_spdif->irq_frddr < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");

	/* spdif pinmux */
	aml_spdif->pin_ctl = devm_pinctrl_get_select(dev, "spdif_pins");
	if (IS_ERR(aml_spdif->pin_ctl)) {
		dev_info(dev, "aml_spdif_get_pins error!\n");
		return PTR_ERR(aml_spdif->pin_ctl);
	}

	ret = devm_snd_soc_register_component(dev, &aml_spdif_component,
					 &aml_spdif_dai, 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &aml_spdif_platform);
}

static const struct of_device_id aml_spdif_device_id[] = {
	{ .compatible = "amlogic, snd-spdif" },
	{},
};
MODULE_DEVICE_TABLE(of, aml_spdif_device_id);

struct platform_driver aml_spdif_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_spdif_device_id,
	},
	.probe = aml_spdif_platform_probe,
};
module_platform_driver(aml_spdif_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic SPDIF ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_spdif_device_id);
