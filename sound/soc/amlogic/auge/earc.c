/*
 * sound/soc/amlogic/auge/earc.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#include "earc_hw.h"

#define DRV_NAME "EARC"

struct earc {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct clk *clk_rx_gate;
	struct clk *clk_rx_cmdc;
	struct clk *clk_rx_dmac;
	struct clk *clk_rx_cmdc_srcpll;
	struct clk *clk_rx_dmac_srcpll;
	struct clk *clk_tx_gate;
	struct clk *clk_tx_cmdc;
	struct clk *clk_tx_dmac;
	struct clk *clk_tx_cmdc_srcpll;
	struct clk *clk_tx_dmac_srcpll;

	struct toddr *tddr;
	struct frddr *fddr;

	int irq_rx_cmdc;
	int irq_rx_dmac;
	int irq_tx_cmdc;
	int irq_tx_dmac;

	int sysclk_freq;
};

#define PREALLOC_BUFFER_MAX	(256 * 1024)

#define EARC_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EARC_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware earc_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_PAUSE,

	.formats = EARC_FORMATS,

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

static irqreturn_t earc_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static irqreturn_t earc_rx_cmdc_isr(int irq, void *devid)
{
	return IRQ_HANDLED;
}

static irqreturn_t earc_rx_dmac_isr(int irq, void *devid)
{
	return IRQ_HANDLED;
}

static irqreturn_t earc_tx_cmdc_isr(int irq, void *devid)
{
	return IRQ_HANDLED;
}

static irqreturn_t earc_tx_dmac_isr(int irq, void *devid)
{
	return IRQ_HANDLED;
}

static int earc_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct earc *p_earc;
	int ret = 0;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	p_earc = (struct earc *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &earc_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_earc->fddr = aml_audio_register_frddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream);
		if (p_earc->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr\n");
			return -ENXIO;
		}
		if (p_earc->irq_tx_cmdc > 0) {
			ret = request_irq(p_earc->irq_tx_cmdc,
					earc_tx_cmdc_isr, 0, "tx_cmdc",
					p_earc);
			if (ret) {
				dev_err(p_earc->dev, "failed to claim irq_tx_cmdc %u\n",
							p_earc->irq_tx_cmdc);
				return ret;
			}
		}
		if (p_earc->irq_tx_dmac > 0) {
			ret = request_irq(p_earc->irq_tx_dmac,
					earc_tx_dmac_isr, 0, "tx_dmac",
					p_earc);
			if (ret) {
				dev_err(p_earc->dev, "failed to claim irq_tx_dmac %u\n",
							p_earc->irq_tx_dmac);
				return ret;
			}
		}
	} else {
		p_earc->tddr = aml_audio_register_toddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream);
		if (p_earc->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr\n");
			return -ENXIO;
		}

		ret = request_irq(p_earc->irq_rx_cmdc,
				earc_rx_cmdc_isr, 0, "rx_cmdc",
				p_earc);
		if (ret) {
			dev_err(p_earc->dev, "failed to claim irq_rx_cmdc %u\n",
						p_earc->irq_rx_cmdc);
			return ret;
		}
		ret = request_irq(p_earc->irq_rx_dmac,
				earc_rx_dmac_isr, 0, "rx_dmac",
				p_earc);
		if (ret) {
			dev_err(p_earc->dev, "failed to claim rx_dmac %u\n",
						p_earc->irq_rx_dmac);
			return ret;
		}
	}

	runtime->private_data = p_earc;

	return 0;
}

static int earc_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aml_audio_unregister_frddr(p_earc->dev, substream);

		if (p_earc->irq_tx_cmdc > 0)
			free_irq(p_earc->irq_tx_cmdc, p_earc);

		if (p_earc->irq_tx_dmac > 0)
			free_irq(p_earc->irq_tx_dmac, p_earc);
	} else {
		aml_audio_unregister_toddr(p_earc->dev, substream);
		free_irq(p_earc->irq_rx_cmdc, p_earc);
		free_irq(p_earc->irq_rx_dmac, p_earc);
	}
	runtime->private_data = NULL;

	return 0;
}

static int earc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
			params_buffer_bytes(hw_params));
}

static int earc_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int earc_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static int earc_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - 8;
	int_addr = frames_to_bytes(runtime, runtime->period_size) / 8;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_earc->tddr;

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t earc_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_earc->fddr);
	else
		addr = aml_toddr_get_position(p_earc->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

int earc_silence(struct snd_pcm_substream *substream, int channel,
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

static int earc_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops earc_ops = {
	.open      = earc_open,
	.close     = earc_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = earc_hw_params,
	.hw_free   = earc_hw_free,
	.prepare   = earc_prepare,
	.trigger   = earc_trigger,
	.pointer   = earc_pointer,
	.silence   = earc_silence,
	.mmap      = earc_mmap,
};

static int earc_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(
			rtd->pcm, SNDRV_DMA_TYPE_DEV,
			rtd->card->snd_card->dev,
			PREALLOC_BUFFER_MAX,
			PREALLOC_BUFFER_MAX);
}

struct snd_soc_platform_driver earc_platform = {
	.ops = &earc_ops,
	.pcm_new = earc_new,
};

static int earc_dai_probe(struct snd_soc_dai *cpu_dai)
{
	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);

	return 0;
}

static int earc_dai_remove(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int earc_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;
		enum frddr_dest dst = frddr_src_get();

		pr_info("%s Expected frddr dst:%s\n",
			__func__,
			frddr_src_get_str(dst));

		aml_frddr_select_dst(fr, dst);
		aml_frddr_set_fifos(fr, 0x40, 0x20);
	} else {
		struct toddr *to = p_earc->tddr;
		unsigned int msb = 0, lsb = 0, toddr_type = 0;
		unsigned int src = EARCRX_DMAC;
		struct toddr_fmt fmt;

		if (bit_depth == 32)
			toddr_type = 3;
		else if (bit_depth == 24)
			toddr_type = 4;
		else
			toddr_type = 0;

		pr_info("%s Expected toddr src:%s\n",
			__func__,
			toddr_src_get_str(src));

		msb = 28 - 1;
		if (bit_depth == 16)
			lsb = 28 - bit_depth;
		else
			lsb = 4;

		pr_info("%s m:%d, n:%d, toddr type:%d\n",
			__func__, msb, lsb, toddr_type);

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

		earcrx_cmdc_init();
		earcrx_dmac_init();
		earc_arc_init();

	}

	return 0;
}

static int earc_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX enable\n");

			aml_frddr_enable(p_earc->fddr, true);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX enable\n");

			aml_toddr_enable(p_earc->tddr, true);

			earc_rx_enable(true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX disable\n");

			aml_frddr_enable(p_earc->fddr, false);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX disable\n");

			earc_rx_enable(false);

			aml_toddr_enable(p_earc->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int earc_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	int ret = 0;

	pr_info("%s:rate:%d, sysclk:%d\n",
		__func__,
		rate,
		p_earc->sysclk_freq);

	return ret;
}

static int earc_dai_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc earc_dai_set_fmt, %#x, %p\n", fmt, p_earc);

	return 0;
}

static int earc_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	p_earc->sysclk_freq = freq;
	pr_info("earc_dai_set_sysclk, %d, %d, %d\n",
			clk_id, freq, dir);

	clk_set_rate(p_earc->clk_rx_cmdc, 10000000);
	clk_set_rate(p_earc->clk_rx_dmac, 250000000);

	pr_info("earc rx cmdc clk:%lu rx dmac clk:%lu\n",
		clk_get_rate(p_earc->clk_rx_cmdc),
		clk_get_rate(p_earc->clk_rx_dmac));

	return 0;
}

static int earc_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	/* enable clock gate */
	if (!IS_ERR(p_earc->clk_rx_gate)) {
		ret = clk_prepare_enable(p_earc->clk_rx_gate);
		if (ret) {
			pr_err("Can't enable earc rx_gate: %d\n", ret);
			goto err;
		}
	}

	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x1 << 6);

	/* enable clock */
	if (!IS_ERR(p_earc->clk_rx_cmdc)) {
		ret = clk_prepare_enable(p_earc->clk_rx_cmdc);
		if (ret) {
			pr_err("Can't enable earc clk_rx_cmdc: %d\n", ret);
			goto err;
		}
	}
	if (!IS_ERR(p_earc->clk_rx_dmac)) {
		ret = clk_prepare_enable(p_earc->clk_rx_dmac);
		if (ret) {
			pr_err("Can't enable earc clk_rx_dmac: %d\n", ret);
			goto err;
		}
	}
	if (!IS_ERR(p_earc->clk_tx_cmdc)) {
		ret = clk_prepare_enable(p_earc->clk_tx_cmdc);
		if (ret) {
			pr_err("Can't enable earc clk_tx_cmdc: %d\n", ret);
			goto err;
		}
	}
	if (!IS_ERR(p_earc->clk_tx_dmac)) {
		ret = clk_prepare_enable(p_earc->clk_tx_dmac);
		if (ret) {
			pr_err("Can't enable earc clk_tx_dmac: %d\n", ret);
			goto err;
		}
	}

	return 0;
err:
	pr_err("failed enable clock\n");
	return -EINVAL;
}


static void earc_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	/* disable clock and gate */
	if (!IS_ERR(p_earc->clk_rx_cmdc))
		clk_disable_unprepare(p_earc->clk_rx_cmdc);
	if (!IS_ERR(p_earc->clk_rx_dmac))
		clk_disable_unprepare(p_earc->clk_rx_dmac);
	if (!IS_ERR(p_earc->clk_tx_cmdc))
		clk_disable_unprepare(p_earc->clk_tx_cmdc);
	if (!IS_ERR(p_earc->clk_tx_dmac))
		clk_disable_unprepare(p_earc->clk_tx_dmac);
	if (!IS_ERR(p_earc->clk_rx_gate))
		clk_disable_unprepare(p_earc->clk_rx_gate);

	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x0 << 6);
}

static struct snd_soc_dai_ops earc_dai_ops = {
	.prepare    = earc_dai_prepare,
	.trigger    = earc_dai_trigger,
	.hw_params  = earc_dai_hw_params,
	.set_fmt    = earc_dai_set_fmt,
	.set_sysclk = earc_dai_set_sysclk,
	.startup	= earc_dai_startup,
	.shutdown	= earc_dai_shutdown,
};

static struct snd_soc_dai_driver earc_dai[] = {
	{
		.name     = "EARC/ARC",
		.id       = 0,
		.probe    = earc_dai_probe,
		.remove   = earc_dai_remove,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates        = EARC_RATES,
		      .formats      = EARC_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates        = EARC_RATES,
		     .formats      = EARC_FORMATS,
		},
		.ops    = &earc_dai_ops,
	},
};

static const struct snd_kcontrol_new earc_controls[] = {


};

static const struct snd_soc_component_driver earc_component = {
	.controls       = earc_controls,
	.num_controls   = ARRAY_SIZE(earc_controls),
	.name		= DRV_NAME,
};

static const struct of_device_id earc_device_id[] = {
	{
		.compatible = "amlogic, sm1-snd-earc",
	},
	{},
};

MODULE_DEVICE_TABLE(of, earc_device_id);

static int earc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct earc *p_earc = NULL;
	int ret = 0;


	p_earc = devm_kzalloc(dev, sizeof(struct earc), GFP_KERNEL);
	if (!p_earc)
		return -ENOMEM;

	p_earc->dev = dev;
	dev_set_drvdata(dev, p_earc);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
			platform_get_drvdata(pdev_parent);
	p_earc->actrl = actrl;

	/* clock gate */
	p_earc->clk_rx_gate = devm_clk_get(&pdev->dev, "rx_gate");
	if (IS_ERR(p_earc->clk_rx_gate)) {
		dev_err(&pdev->dev,
			"Can't get earc gate\n");
		return PTR_ERR(p_earc->clk_rx_gate);
	}
	/* RX */
	p_earc->clk_rx_cmdc = devm_clk_get(&pdev->dev, "rx_cmdc");
	if (IS_ERR(p_earc->clk_rx_cmdc)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc\n");
		return PTR_ERR(p_earc->clk_rx_cmdc);
	}
	p_earc->clk_rx_dmac = devm_clk_get(&pdev->dev, "rx_dmac");
	if (IS_ERR(p_earc->clk_rx_dmac)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac\n");
		return PTR_ERR(p_earc->clk_rx_dmac);
	}
	p_earc->clk_rx_cmdc_srcpll = devm_clk_get(&pdev->dev, "rx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_rx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_cmdc_srcpll);
	}
	p_earc->clk_rx_dmac_srcpll = devm_clk_get(&pdev->dev, "rx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_dmac_srcpll);
	}
	ret = clk_set_parent(p_earc->clk_rx_cmdc, p_earc->clk_rx_cmdc_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_cmdc parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_cmdc);
		return ret;
	}
	ret = clk_set_parent(p_earc->clk_rx_dmac, p_earc->clk_rx_dmac_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_dmac parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_dmac);
		return ret;
	}

	/* TX */
	p_earc->clk_tx_cmdc = devm_clk_get(&pdev->dev, "tx_cmdc");
	if (IS_ERR(p_earc->clk_tx_cmdc)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac = devm_clk_get(&pdev->dev, "tx_dmac");
	if (IS_ERR(p_earc->clk_tx_dmac)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_cmdc_srcpll = devm_clk_get(&pdev->dev, "tx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac_srcpll = devm_clk_get(&pdev->dev, "tx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	if (!IS_ERR(p_earc->clk_tx_cmdc) &&
		!IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_cmdc,
				p_earc->clk_tx_cmdc_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_cmdc parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_cmdc);
			return ret;
		}
	}
	if (!IS_ERR(p_earc->clk_tx_dmac) &&
		!IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_dmac,
				p_earc->clk_tx_dmac_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_dmac parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_dmac);
			return ret;
		}
	}

	/* irqs */
	p_earc->irq_rx_cmdc =
		platform_get_irq_byname(pdev, "rx_cmdc");
	if (p_earc->irq_rx_cmdc < 0) {
		dev_err(dev, "platform get irq rx_cmdc failed\n");
		return p_earc->irq_rx_cmdc;
	}
	p_earc->irq_rx_dmac =
		platform_get_irq_byname(pdev, "rx_dmac");
	if (p_earc->irq_rx_dmac < 0) {
		dev_err(dev, "platform get irq rx_dmac failed\n");
		return p_earc->irq_rx_dmac;
	}
	p_earc->irq_tx_cmdc =
		platform_get_irq_byname(pdev, "tx_cmdc");
	if (p_earc->irq_tx_cmdc < 0)
		dev_err(dev, "platform get irq tx_cmdc failed, Check whether support eARC TX\n");
	p_earc->irq_tx_dmac =
		platform_get_irq_byname(pdev, "tx_dmac");
	if (p_earc->irq_tx_dmac < 0)
		dev_err(dev, "platform get irq tx_dmac failed, Check whether support eARC TX\n");

	ret = snd_soc_register_component(&pdev->dev,
				&earc_component,
				earc_dai,
				ARRAY_SIZE(earc_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &earc_platform);
}

struct platform_driver earc_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = earc_device_id,
	},
	.probe = earc_platform_probe,
};
module_platform_driver(earc_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic eARC/ARC TX/RX ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, earc_device_id);
