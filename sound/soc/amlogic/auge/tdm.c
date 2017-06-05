/*
 * sound/soc/amlogic/auge/tdm.c
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
#include "tdm_hw.h"

#define DRV_NAME "aml_tdm"

#define TDM_A	0
#define TDM_B	1
#define TDM_C	2
#define LANE_MAX 4

static void dump_pcm_setting(struct pcm_setting *setting)
{
	if (setting == NULL)
		return;

	pr_info("dump_pcm_setting(%p)\n", setting);
	pr_info("\tpcm_mode(%d)\n", setting->pcm_mode);
	pr_info("\tsysclk(%d)\n", setting->sysclk);
	pr_info("\tsysclk_bclk_ratio(%d)\n", setting->sysclk_bclk_ratio);
	pr_info("\tbclk(%d)\n", setting->bclk);
	pr_info("\tbclk_lrclk_ratio(%d)\n", setting->bclk_lrclk_ratio);
	pr_info("\tlrclk(%d)\n", setting->lrclk);
	pr_info("\ttx_mask(%#x)\n", setting->tx_mask);
	pr_info("\trx_mask(%#x)\n", setting->rx_mask);
	pr_info("\tslots(%d)\n", setting->slots);
	pr_info("\tslot_width(%d)\n", setting->slot_width);
	pr_info("\tlane_mask_in(%#x)\n", setting->lane_mask_in);
	pr_info("\tlane_mask_out(%#x)\n", setting->lane_mask_out);
}

struct aml_tdm {
	struct pcm_setting setting;
	struct pinctrl *pin_ctl;
	struct aml_audio_controller *actrl;
	struct device *dev;
	struct clk *clk;
	struct clk *clk_gate;
	struct clk *mclk;
	unsigned int id;
	/* bclk src selection */
	unsigned int clk_sel;
	int irq_tdmout;
	int irq_tdmin;
	unsigned int from_ddr_num;
	unsigned int to_ddr_num;
	struct toddr *tddr;
	struct frddr *fddr;
};

static const struct snd_pcm_hardware aml_tdm_hardware = {
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

static irqreturn_t aml_tdmout_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)devid;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static irqreturn_t aml_tdmin_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)devid;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}
#if 0
/* get counts of '1's in val */
static unsigned int pop_count(unsigned int val)
{
	unsigned int count = 0;

	while (val) {
		count++;
		val = val & (val - 1);
	}

	return count;
}
#endif
static int snd_soc_of_get_slot_mask(struct device_node *np,
				    const char *prop_name,
				    unsigned int *mask)
{
	u32 val;
	const __be32 *of_slot_mask = of_get_property(np, prop_name, &val);
	int i;

	if (!of_slot_mask)
		return 0;

	val /= sizeof(u32);
	for (i = 0; i < val; i++)
		if (be32_to_cpup(&of_slot_mask[i]))
			*mask |= (1 << i);

	return val;
}

static int of_parse_tdm_lane_slot_in(struct device_node *np,
			      unsigned int *lane_mask)
{
	if (lane_mask)
		return snd_soc_of_get_slot_mask(np,
			"dai-tdm-lane-slot-mask-in", lane_mask);

	return -EINVAL;
}

static int of_parse_tdm_lane_slot_out(struct device_node *np,
			      unsigned int *lane_mask)
{
	if (lane_mask)
		return snd_soc_of_get_slot_mask(np,
			"dai-tdm-lane-slot-mask-out", lane_mask);

	return -EINVAL;
}

static int aml_tdm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct aml_tdm *p_tdm;
	int ret = 0;

	p_tdm = (struct aml_tdm *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &aml_tdm_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_tdm->fddr = aml_audio_register_frddr(dev,
			p_tdm->actrl, p_tdm->from_ddr_num);
		if (p_tdm->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr %u\n",
					p_tdm->from_ddr_num);
			return -ENXIO;
		}

		ret = request_irq(p_tdm->irq_tdmout,
			aml_tdmout_isr, 0, "tdmout", substream);
		if (ret) {
			aml_audio_unregister_frddr(dev,
				p_tdm->from_ddr_num);
			dev_err(p_tdm->dev, "failed to claim irq %u\n",
					p_tdm->irq_tdmout);
			return ret;
		}
	} else {
		p_tdm->tddr = aml_audio_register_toddr(dev,
			p_tdm->actrl, p_tdm->to_ddr_num);
		if (p_tdm->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr %u\n",
					p_tdm->to_ddr_num);
			return -ENXIO;
		}

		ret = request_irq(p_tdm->irq_tdmin,
				aml_tdmin_isr, 0, "tdmin", substream);
		if (ret) {
			aml_audio_unregister_toddr(dev, p_tdm->to_ddr_num);
			dev_err(p_tdm->dev, "failed to claim irq %u\n",
						p_tdm->irq_tdmin);
			return ret;
		}
	}

	runtime->private_data = p_tdm;
	return 0;
}

static int aml_tdm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aml_audio_unregister_frddr(p_tdm->dev,
				p_tdm->from_ddr_num);
		free_irq(p_tdm->irq_tdmout, substream);
	} else {
		aml_audio_unregister_toddr(p_tdm->dev,
				p_tdm->to_ddr_num);

		free_irq(p_tdm->irq_tdmin, substream);
	}

	return 0;
}

static int aml_tdm_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_tdm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int aml_tdm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - 8;
	int_addr = frames_to_bytes(runtime, runtime->period_size)/8;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_tdm->fddr;

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_tdm->tddr;

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t aml_tdm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;
	unsigned int addr, start_addr;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_tdm->fddr);
	else
		addr = aml_toddr_get_position(p_tdm->tddr);

	return bytes_to_frames(runtime, addr - start_addr);
}

static int aml_tdm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops aml_tdm_ops = {
	.open = aml_tdm_open,
	.close = aml_tdm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_tdm_hw_params,
	.hw_free = aml_tdm_hw_free,
	.prepare = aml_tdm_prepare,
	.pointer = aml_tdm_pointer,
	.mmap = aml_tdm_mmap,
};

#define PREALLOC_BUFFER		(32 * 1024)
#define PREALLOC_BUFFER_MAX	(256 * 1024)
static int aml_tdm_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(
		rtd->pcm, SNDRV_DMA_TYPE_DEV,
		rtd->card->snd_card->dev, PREALLOC_BUFFER, PREALLOC_BUFFER_MAX);
}

struct snd_soc_platform_driver aml_tdm_platform = {
	.ops = &aml_tdm_ops,
	.pcm_new = aml_tdm_new,
};

static int aml_dai_tdm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	aml_tdm_fifo_reset(p_tdm->actrl, substream->stream, p_tdm->id);

	return 0;
}

static void aml_dai_tdm_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
}

static int aml_dai_tdm_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	int bit_depth;

	bit_depth = snd_pcm_format_width(runtime->format);

	aml_tdm_fifo_ctrl(p_tdm->actrl,
		bit_depth,
		substream->stream,
		p_tdm->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_tdm->fddr;
		enum frddr_dest dst;

		switch (p_tdm->id) {
		case 0:
			dst = TDMOUT_A;
			break;
		case 1:
			dst = TDMOUT_B;
			break;
		case 2:
			dst = TDMOUT_C;
			break;
		default:
			dev_err(p_tdm->dev,	"invalid id: %d\n",
					p_tdm->id);
			return -EINVAL;
		}
		aml_frddr_select_dst(fr, dst);
		aml_frddr_set_fifos(fr, 0x40, 0x20);
	} else {
		struct toddr *to = p_tdm->tddr;
		enum toddr_src src;
		unsigned int lsb = 32 - bit_depth;
		unsigned int toddr_type;

		switch (bit_depth) {
		case 8:
			toddr_type = 0;
			break;
		case 16:
			toddr_type = 2;
			break;
		case 24:
		case 32:
			toddr_type = 4;
			break;
		default:
			dev_err(p_tdm->dev, "invalid bit_depth: %d\n",
					bit_depth);
			return -EINVAL;
		}

		dev_info(substream->pcm->card->dev, "tdm prepare----capture\n");
		switch (p_tdm->id) {
		case 0:
			src = TDMIN_A;
		break;
		case 1:
			src = TDMIN_B;
		break;
		case 2:
			src = TDMIN_C;
		break;
		default:
			dev_err(p_tdm->dev, "invalid id: %d\n",
					p_tdm->id);
			return -EINVAL;
		}

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, toddr_type, 31, lsb);
		aml_toddr_set_fifos(to, 0x40);
	}

	return 0;
}

static int aml_dai_tdm_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		aml_tdm_enable(p_tdm->actrl,
			substream->stream, p_tdm->id, true);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "tdm playback enable\n");
			aml_frddr_enable(p_tdm->fddr, 1);
		} else {
			dev_info(substream->pcm->card->dev, "tdm capture enable\n");
			aml_toddr_enable(p_tdm->tddr, 1);
		}

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		aml_tdm_enable(p_tdm->actrl,
			substream->stream, p_tdm->id, false);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "tdm playback enable\n");
			aml_frddr_enable(p_tdm->fddr, 0);
		} else {
			dev_info(substream->pcm->card->dev, "tdm capture enable\n");
			aml_toddr_enable(p_tdm->tddr, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pcm_setting_init(struct pcm_setting *setting, unsigned int rate,
			unsigned int channels)
{

	setting->lrclk = rate;
	setting->bclk_lrclk_ratio = setting->slots * setting->slot_width;
	setting->bclk = setting->lrclk * setting->bclk_lrclk_ratio;

	/* calculate mclk */
	setting->sysclk_bclk_ratio = 4;
	setting->sysclk = 4 * setting->bclk;

	return 0;
}
static int aml_tdm_set_lanes(struct aml_tdm *p_tdm,
				unsigned int channels, int stream)
{
	struct pcm_setting *setting = &p_tdm->setting;
	unsigned int lanes, swap_val;
	unsigned int lane_mask;
	unsigned int set_num = 0;
	unsigned int i;

	pr_info("asoc debug: %d-%d\n", channels, setting->slots);

	swap_val = 0;
	// calc lanes by channels and slots
	lanes = (channels - 1) / setting->slots + 1;
	if (lanes > 4) {
		pr_err("lanes setting error\n");
		return -EINVAL;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		// set lanes mask acordingly
		lane_mask = setting->lane_mask_out;
		for (i = 0; i < 4; i++) {
			unsigned int ch = i * 2;

			if (i < lanes)
				aml_tdm_set_channel_mask(p_tdm->actrl,
					stream, p_tdm->id, i, setting->tx_mask);

			if ((1 << i) & lane_mask) {
				// each lane only L/R swap
				swap_val |= set_num++ << (ch++ * 4);
				swap_val |= set_num++ << (ch * 4);
			}
		}
		aml_tdm_set_lane_channel_swap(p_tdm->actrl,
			stream, p_tdm->id, swap_val);
	} else {
		lane_mask = setting->lane_mask_in;

		for (i = 0; i < 4; i++) {
			if (i < lanes)
				aml_tdm_set_channel_mask(p_tdm->actrl,
					stream, p_tdm->id, i, setting->rx_mask);
			if ((1 << i) & lane_mask) {
				// each lane only L/R masked
				pr_info("tdmin set lane %d\n", i);
				swap_val |= (i * 2) << (set_num++ * 4);
				swap_val |= (i * 2 + 1) << (set_num++ * 4);
			}
		}

		aml_tdm_set_lane_channel_swap(p_tdm->actrl,
			stream, p_tdm->id, swap_val);
	}

	return 0;
}

static int aml_dai_tdm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct pcm_setting *setting = &p_tdm->setting;
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	int ret;


	ret = pcm_setting_init(setting, rate, channels);
	if (ret)
		return ret;

	dump_pcm_setting(setting);

	/* set pcm dai hw params */
	// TODO: add clk_id
	snd_soc_dai_set_sysclk(cpu_dai,
		0, setting->sysclk, SND_SOC_CLOCK_OUT);
	ret = snd_soc_dai_set_clkdiv(cpu_dai, 0, setting->sysclk_bclk_ratio);
	if (ret)
		return ret;

	ret = snd_soc_dai_set_bclk_ratio(cpu_dai, setting->bclk_lrclk_ratio);
	if (ret)
		return ret;

	ret = aml_tdm_set_lanes(p_tdm, channels, substream->stream);
	if (ret)
		return ret;

	return 0;
}

static int aml_dai_tdm_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	int i;

	for (i = 0; i < 4; i++)
		aml_tdm_set_channel_mask(p_tdm->actrl,
			substream->stream, p_tdm->id, i, 0);

	return 0;
}

static int aml_dai_set_tdm_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc aml_dai_set_tdm_fmt, %#x, %p, id(%d), clksel(%d)\n",
		fmt, p_tdm, p_tdm->id, p_tdm->clk_sel);

	aml_tdm_set_format(p_tdm->actrl,
		&(p_tdm->setting), p_tdm->clk_sel, p_tdm->id, fmt);

	return 0;
}

static void aml_tdm_set_mclk(struct aml_tdm *p_tdm)
{
	struct aml_audio_controller *actrl = p_tdm->actrl;
	unsigned int clk_id, offset;
	unsigned int mpll_freq = 0;

	offset = p_tdm->clk_sel;

	/* slave mode */
	if (offset > MASTER_F)
		return;

	clk_id = p_tdm->id;

	if (p_tdm->setting.sysclk) {
		unsigned int mul = 4;

		mpll_freq = p_tdm->setting.sysclk * mul;
		clk_set_rate(p_tdm->clk, mpll_freq);
		aml_audiobus_write(actrl, EE_AUDIO_MCLK_A_CTRL + offset,
						1 << 31 | //clk enable
						clk_id << 24 | // clk src
						(mul - 1)); //clk_div mclk

		if (offset == 2) {
			//enable another mclka also;
			offset = 0;
			aml_audiobus_write(actrl, EE_AUDIO_MCLK_A_CTRL + offset,
					1 << 31 | //clk enable
					clk_id << 24 | // clk src
					(mul - 1)); //clk_div mclk
		}
	}
}

static int aml_dai_set_tdm_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	p_tdm->setting.sysclk = freq;
	pr_info("aml_dai_set_tdm_sysclk, %d, %d, %d\n",
			clk_id, freq, dir);
	aml_tdm_set_mclk(p_tdm);

	return 0;
}

static int aml_dai_set_bclk_ratio(struct snd_soc_dai *cpu_dai,
						unsigned int ratio)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bclk_ratio, lrclk_hi;

	p_tdm->setting.bclk_lrclk_ratio = ratio;
	bclk_ratio = ratio - 1;
	lrclk_hi = 0;

	if (p_tdm->setting.pcm_mode == SND_SOC_DAIFMT_I2S ||
		p_tdm->setting.pcm_mode == SND_SOC_DAIFMT_LEFT_J) {
		pr_info("aml_dai_set_bclk_ratio, select I2S mode\n");
		lrclk_hi = bclk_ratio / 2;
	} else {
		pr_info("aml_dai_set_bclk_ratio, select TDM mode\n");
	}
	aml_tdm_set_bclk_ratio(p_tdm->actrl,
		p_tdm->clk_sel, lrclk_hi, bclk_ratio);

	return 0;
}

static int aml_dai_set_clkdiv(struct snd_soc_dai *cpu_dai,
						int div_id, int div)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int mclk_ratio;

	pr_info("aml_dai_set_clkdiv, div %d, clksel(%d)\n",
			div, p_tdm->clk_sel);

	p_tdm->setting.sysclk_bclk_ratio = div;
	mclk_ratio = div - 1;
	aml_tdm_set_lrclkdiv(p_tdm->actrl, p_tdm->clk_sel, mclk_ratio);

	return 0;
}

static int aml_dai_set_tdm_slot(struct snd_soc_dai *cpu_dai,
				unsigned int tx_mask, unsigned int rx_mask,
				int slots, int slot_width)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("aml_dai_set_tdm_slot, %x, %x, %d, %d\n",
			tx_mask, rx_mask, slots, slot_width);
	p_tdm->setting.tx_mask = tx_mask;
	p_tdm->setting.rx_mask = rx_mask;
	p_tdm->setting.slots = slots;
	p_tdm->setting.slot_width = slot_width;

	aml_tdm_set_slot(p_tdm->actrl, slots, slot_width, p_tdm->id);

	return 0;
}

static int aml_dai_tdm_probe(struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct device *dev = p_tdm->dev;
	int ret;

	/* config ddr arb */
	aml_tdm_arb_config(p_tdm->actrl);

	ret = clk_prepare_enable(p_tdm->clk);
	if (ret) {
		dev_err(dev, "Can't enable mpll clock: %d\n", ret);
		return ret;
	}

	return 0;
}

static int aml_dai_tdm_remove(struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	clk_disable_unprepare(p_tdm->clk);
	return 0;
}

static struct snd_soc_dai_ops aml_dai_tdm_ops = {
	.startup = aml_dai_tdm_startup,
	.shutdown = aml_dai_tdm_shutdown,
	.prepare = aml_dai_tdm_prepare,
	.trigger = aml_dai_tdm_trigger,
	.hw_params = aml_dai_tdm_hw_params,
	.hw_free = aml_dai_tdm_hw_free,
	.set_fmt = aml_dai_set_tdm_fmt,
	.set_sysclk = aml_dai_set_tdm_sysclk,
	.set_bclk_ratio = aml_dai_set_bclk_ratio,
	.set_clkdiv = aml_dai_set_clkdiv,
	.set_tdm_slot = aml_dai_set_tdm_slot,
};

#define AML_DAI_TDM_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_TDM_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_tdm_dai[] = {
	{
	.name = "TDM-A",
	.id = 1,
	.probe = aml_dai_tdm_probe,
	.remove = aml_dai_tdm_remove,
	.playback = {
	      .channels_min = 1,
	      .channels_max = 32,
	      .rates = AML_DAI_TDM_RATES,
	      .formats = AML_DAI_TDM_FORMATS,
	},
	.capture = {
	     .channels_min = 1,
	     .channels_max = 32,
	     .rates = AML_DAI_TDM_RATES,
	     .formats = AML_DAI_TDM_FORMATS,
	},
	.ops = &aml_dai_tdm_ops,
	.symmetric_rates = 1,
	},
	{
	.name = "TDM-B",
	.id = 2,
	.probe = aml_dai_tdm_probe,
	.remove = aml_dai_tdm_remove,
	.playback = {
	      .channels_min = 1,
	      .channels_max = 8,
	      .rates = AML_DAI_TDM_RATES,
	      .formats = AML_DAI_TDM_FORMATS,
	},
	.capture = {
	     .channels_min = 1,
	     .channels_max = 8,
	     .rates = AML_DAI_TDM_RATES,
	     .formats = AML_DAI_TDM_FORMATS,
	},
	.ops = &aml_dai_tdm_ops,
	.symmetric_rates = 1,
	},
	{
	.name = "TDM-C",
	.id = 3,
	.probe = aml_dai_tdm_probe,
	.remove = aml_dai_tdm_remove,
	.playback = {
	      .channels_min = 1,
	      .channels_max = 8,
	      .rates = AML_DAI_TDM_RATES,
	      .formats = AML_DAI_TDM_FORMATS,
	},
	.capture = {
	     .channels_min = 1,
	     .channels_max = 8,
	     .rates = AML_DAI_TDM_RATES,
	     .formats = AML_DAI_TDM_FORMATS,
	},
	.ops = &aml_dai_tdm_ops,
	.symmetric_rates = 1,
	},
};

static const struct snd_soc_component_driver aml_tdm_component = {
	.name		= DRV_NAME,
};

static const struct of_device_id aml_tdm_device_id[] = {
	{ .compatible = "amlogic, snd-tdma", .data = (void *)TDM_A},
	{ .compatible = "amlogic, snd-tdmb", .data = (void *)TDM_B},
	{ .compatible = "amlogic, snd-tdmc", .data = (void *)TDM_C},
	{},
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);

static int aml_tdm_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct aml_tdm *p_tdm = NULL;
	const struct of_device_id *id;
	unsigned long iddata = 0;
	int ret = 0;

	p_tdm = devm_kzalloc(dev, sizeof(struct aml_tdm), GFP_KERNEL);
	if (!p_tdm)
		return -ENOMEM;


	/* get tdm device id */
	id = of_match_device(of_match_ptr(aml_tdm_device_id), dev);
	pr_info("id = %lu\n", (unsigned long)id->data);
	iddata = (unsigned long)id->data;
	p_tdm->id = (unsigned int)iddata;

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_tdm->actrl = actrl;

	/* get tdm mclk sel configs */
	ret = of_property_read_u32(node, "dai-tdm-clk-sel",
			&p_tdm->clk_sel);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dai-tdm-clk-sel\n");
		return -ENXIO;
	}

	/* get tdm lanes info. if not, set to default 1 */
	ret = of_parse_tdm_lane_slot_in(node,
			&p_tdm->setting.lane_mask_in);
	if (ret < 0)
		p_tdm->setting.lane_mask_in = 0x1;

	ret = of_parse_tdm_lane_slot_out(node,
			&p_tdm->setting.lane_mask_out);
	if (ret < 0)
		p_tdm->setting.lane_mask_out = 0x1;

	/* get sysclk source */
	if (iddata == TDM_A) {
		p_tdm->clk = devm_clk_get(&pdev->dev, "mpll0");
		if (IS_ERR(p_tdm->clk)) {
			dev_err(&pdev->dev, "Can't retrieve mpll0 clock\n");
			return PTR_ERR(p_tdm->clk);
		}
#if 0
		p_tdm->clk_gate = devm_clk_get(&pdev->dev, "gate");
		if (IS_ERR(p_tdm->clk_gate)) {
			dev_err(&pdev->dev, "Can't retrieve clockgate\n");
			return PTR_ERR(p_tdm->clk_gate);
		}

		p_tdm->mclk = devm_clk_get(&pdev->dev, "mclk");
		if (IS_ERR(p_tdm->mclk)) {
			dev_err(&pdev->dev, "Can't retrieve mclk\n");
			return PTR_ERR(p_tdm->mclk);
		}
#endif
	} else if (iddata == TDM_B) {
		p_tdm->clk = devm_clk_get(&pdev->dev, "mpll1");
		if (IS_ERR(p_tdm->clk)) {
			dev_err(&pdev->dev, "Can't retrieve mpll1 clock\n");
			return PTR_ERR(p_tdm->clk);
		}
	} else if (iddata == TDM_C) {
		p_tdm->clk = devm_clk_get(&pdev->dev, "mpll2");
		if (IS_ERR(p_tdm->clk)) {
			dev_err(&pdev->dev, "Can't retrieve mpll2 clock\n");
			return PTR_ERR(p_tdm->clk);
		}
	} else {
		return -EINVAL;
	}

	/* parse DTS configured ddr */
	ret = of_property_read_u32(node, "tdm_from_ddr",
			&p_tdm->from_ddr_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve tdm_from_ddr\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(node, "tdm_to_ddr",
			&p_tdm->to_ddr_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve tdm_to_ddr\n");
		return -ENXIO;
	}

	/* irqs */
	p_tdm->irq_tdmout = platform_get_irq_byname(pdev, "tdmout");
	if (p_tdm->irq_tdmout < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");

	p_tdm->irq_tdmin = platform_get_irq_byname(pdev, "tdmin");
	if (p_tdm->irq_tdmin < 0)
		dev_err(dev, "platform_get_irq_byname failed\n");

	p_tdm->pin_ctl = devm_pinctrl_get_select(dev, "tdm_pins");
	if (IS_ERR(p_tdm->pin_ctl)) {
		dev_info(dev, "aml_tdm_get_pins error!\n");
		/*return PTR_ERR(p_tdm->pin_ctl);*/
	}

	p_tdm->dev = dev;
	dev_set_drvdata(dev, p_tdm);

	ret = devm_snd_soc_register_component(dev, &aml_tdm_component,
					 &aml_tdm_dai[iddata], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, try register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &aml_tdm_platform);
}

struct platform_driver aml_tdm_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_tdm_device_id,
	},
	.probe = aml_tdm_platform_probe,
};
module_platform_driver(aml_tdm_driver);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic TDM ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);
