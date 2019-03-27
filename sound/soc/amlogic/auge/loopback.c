/*
 * sound/soc/amlogic/auge/loopback.c
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <sound/pcm_params.h>

#include "loopback.h"
#include "loopback_hw.h"
#include "loopback_match_table.c"
#include "ddr_mngr.h"
#include "tdm_hw.h"
#include "pdm_hw.h"

#define DRV_NAME "loopback"

/*#define __PTM_PDM_CLK__*/
/*#define __PTM_TDM_CLK__*/

struct loopback {
	struct device *dev;
	struct aml_audio_controller *actrl;
	unsigned int id;

	/*
	 * datain
	 */

	/* PDM clocks */
	struct clk *pdm_clk_gate;
	struct clk *pdm_sysclk_srcpll;
	struct clk *pdm_dclk_srcpll;
	struct clk *pdm_sysclk;
	struct clk *pdm_dclk;
	unsigned int dclk_idx;
	/* TDM clocks */
	struct clk *tdmin_mpll;
	struct clk *tdmin_mclk;

	/* datain info */
	enum datain_src datain_src;
	unsigned int datain_chnum;
	unsigned int datain_chmask;
	unsigned int datain_lane_mask; /* related with data lane */

	/*
	 * datalb
	 */

	/* TDMIN_LB clocks */
	struct clk *tdminlb_mpll;
	struct clk *tdminlb_mclk;
	unsigned int mclk_fs_ratio;

	/* datalb info */
	enum datalb_src datalb_src;
	unsigned int datalb_chnum;
	unsigned int datalb_chmask;
	unsigned int datalb_lane_mask; /* related with data lane */

	unsigned int sysclk_freq;

	struct toddr *tddr;

	struct loopback_chipinfo *chipinfo;
};

#define LOOPBACK_RATES      (SNDRV_PCM_RATE_8000_192000)
#define LOOPBACK_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
						SNDRV_PCM_FMTBIT_S24_LE |\
						SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware loopback_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_PAUSE,

	.formats = LOOPBACK_FORMATS,

	.period_bytes_max = 256 * 64,
	.buffer_bytes_max = 512 * 1024,
	.period_bytes_min = 64,
	.periods_min = 1,
	.periods_max = 1024,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static irqreturn_t loopback_ddr_isr(int irq, void *data)
{
	struct snd_pcm_substream *ss = (struct snd_pcm_substream *)data;
	struct snd_soc_pcm_runtime *rtd = ss->private_data;
	struct device *dev = rtd->platform->dev;
	struct loopback *p_loopback = (struct loopback *)dev_get_drvdata(dev);
	unsigned int status;

	if (!snd_pcm_running(ss))
		return IRQ_NONE;

	status = aml_toddr_get_status(p_loopback->tddr) & MEMIF_INT_MASK;
	if (status & MEMIF_INT_COUNT_REPEAT) {
		snd_pcm_period_elapsed(ss);

		aml_toddr_ack_irq(p_loopback->tddr, MEMIF_INT_COUNT_REPEAT);
	} else
		dev_dbg(dev, "unexpected irq - STS 0x%02x\n",
			status);

	return !status ? IRQ_NONE : IRQ_HANDLED;
}

static int loopback_open(struct snd_pcm_substream *ss)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct snd_soc_pcm_runtime *rtd = ss->private_data;
	struct device *dev = rtd->platform->dev;
	struct loopback *p_loopback = (struct loopback *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(ss, &loopback_hardware);

	if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
		p_loopback->tddr = aml_audio_register_toddr(dev,
			p_loopback->actrl,
			loopback_ddr_isr, ss);
		if (p_loopback->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr\n");
			return -ENXIO;
		}
	}

	runtime->private_data = p_loopback;

	return 0;
}

static int loopback_close(struct snd_pcm_substream *ss)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = runtime->private_data;

	if (ss->stream == SNDRV_PCM_STREAM_CAPTURE)
		aml_audio_unregister_toddr(p_loopback->dev, ss);

	runtime->private_data = NULL;

	return 0;
}

static int loopback_hw_params(
	struct snd_pcm_substream *ss,
	 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(ss, params_buffer_bytes(hw_params));
}

static int loopback_hw_free(struct snd_pcm_substream *ss)
{
	snd_pcm_lib_free_pages(ss);

	return 0;
}

static int loopback_trigger(
	struct snd_pcm_substream *ss,
	int cmd)
{
	return 0;
}

static int loopback_prepare(struct snd_pcm_substream *ss)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - 8;
	int_addr = frames_to_bytes(runtime, runtime->period_size) / 8;

	if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
		struct toddr *to = p_loopback->tddr;

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t loopback_pointer(
	struct snd_pcm_substream *ss)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames = 0;

	if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
		start_addr = runtime->dma_addr;
		addr = aml_toddr_get_position(p_loopback->tddr);

		frames = bytes_to_frames(runtime, addr - start_addr);
	}
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

int loopback_silence(
	struct snd_pcm_substream *ss,
	int channel,
	snd_pcm_uframes_t pos,
	snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	char *ppos;
	int n;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

static int loopback_mmap(struct snd_pcm_substream *ss,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(ss, vma);
}

static struct snd_pcm_ops loopback_ops = {
	.open      = loopback_open,
	.close     = loopback_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = loopback_hw_params,
	.hw_free   = loopback_hw_free,
	.prepare   = loopback_prepare,
	.trigger   = loopback_trigger,
	.pointer   = loopback_pointer,
	.silence   = loopback_silence,
	.mmap      = loopback_mmap,
};

static int loopback_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_substream *ss;
	int size = loopback_hardware.buffer_bytes_max;
	int ret = -EINVAL;

	/* only capture */
	ss = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (ss) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
					rtd->platform->dev,
					size, &ss->dma_buffer);
		if (ret) {
			dev_err(rtd->dev, "Cannot allocate buffer(s)\n");
			return ret;
		}
	}

	return ret;
}

static void loopback_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *ss;

	ss = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (ss) {
		snd_dma_free_pages(&ss->dma_buffer);
		ss->dma_buffer.area = NULL;
		ss->dma_buffer.addr = 0;
	}
}

struct snd_soc_platform_driver loopback_platform_drv = {
	.pcm_new  = loopback_pcm_new,
	.pcm_free = loopback_pcm_free,
	.ops      = &loopback_ops,
};

static int loopback_dai_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int loopback_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static int datain_pdm_startup(struct loopback *p_loopback)
{
	int ret;

	/* enable clock gate */
	ret = clk_prepare_enable(p_loopback->pdm_clk_gate);

	/* enable clock */
	ret = clk_prepare_enable(p_loopback->pdm_sysclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm pdm_sysclk_srcpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_loopback->pdm_dclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm pdm_dclk_srcpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_loopback->pdm_sysclk);
	if (ret) {
		pr_err("Can't enable pcm pdm_sysclk clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_loopback->pdm_dclk);
	if (ret) {
		pr_err("Can't enable pcm pdm_dclk clock: %d\n", ret);
		goto err;
	}

	return 0;
err:
	pr_err("failed enable pdm clock\n");
	return -EINVAL;
}

static void datain_pdm_shutdown(struct loopback *p_loopback)
{
	/* disable clock and gate */
	clk_disable_unprepare(p_loopback->pdm_dclk);
	clk_disable_unprepare(p_loopback->pdm_sysclk);
	clk_disable_unprepare(p_loopback->pdm_sysclk_srcpll);
	clk_disable_unprepare(p_loopback->pdm_dclk_srcpll);
	clk_disable_unprepare(p_loopback->pdm_clk_gate);
}

static int tdminlb_startup(struct loopback *p_loopback)
{
	int ret;

	/* enable clock */
	ret = clk_prepare_enable(p_loopback->tdminlb_mpll);
	if (ret) {
		pr_err("Can't enable tdminlb_mpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_loopback->tdminlb_mclk);
	if (ret) {
		pr_err("Can't enable tdminlb_mclk clock: %d\n", ret);
		goto err;
	}

	return 0;
err:
	pr_err("failed enable pdm clock\n");
	return -EINVAL;
}

static void tdminlb_shutdown(struct loopback *p_loopback)
{
	/* disable clock and gate */
	clk_disable_unprepare(p_loopback->tdminlb_mclk);
	clk_disable_unprepare(p_loopback->tdminlb_mpll);
}

static int loopback_dai_startup(
	struct snd_pcm_substream *ss,
	struct snd_soc_dai *dai)
{
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	if (ss->stream != SNDRV_PCM_STREAM_CAPTURE) {
		ret = -EINVAL;
		goto err;
	}

	pr_info("%s\n", __func__);

	/* datain */
	switch (p_loopback->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
		break;
	case DATAIN_SPDIF:
		break;
	case DATAIN_PDM:
		ret = datain_pdm_startup(p_loopback);
		if (ret < 0)
			goto err;
		break;
	case DATAIN_LOOPBACK:
		break;
	default:
		break;
	}

	/* datalb */
	switch (p_loopback->datalb_src) {
	case TDMINLB_TDMOUTA ... TDMINLB_PAD_TDMINC:
		tdminlb_startup(p_loopback);
		break;
	case SPDIFINLB_SPDIFOUTA ... SPDIFINLB_SPDIFOUTB:
		break;
	default:
		break;
	}

	return ret;
err:
	pr_err("Failed to enable datain clock\n");
	return ret;
}

static void loopback_dai_shutdown(
	struct snd_pcm_substream *ss,
	struct snd_soc_dai *dai)
{
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);

	pr_info("%s\n", __func__);

	/* datain */
	switch (p_loopback->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
		break;
	case DATAIN_SPDIF:
		break;
	case DATAIN_PDM:
		datain_pdm_shutdown(p_loopback);
		break;
	case DATAIN_LOOPBACK:
		break;
	default:
		break;
	}

	/* datalb */
	switch (p_loopback->datalb_src) {
	case TDMINLB_TDMOUTA ... TDMINLB_PAD_TDMINC:
		tdminlb_shutdown(p_loopback);
		break;
	case SPDIFINLB_SPDIFOUTA ... SPDIFINLB_SPDIFOUTB:
		break;
	default:
		break;
	}

}

static void loopback_set_clk(struct loopback *p_loopback,
	int rate, bool enable)
{
	unsigned int mul = 2;
	unsigned int mpll_freq, mclk_freq;
	/* assume datain_lb in i2s format, 2ch, 32bit */
	unsigned int bit_depth = 32, i2s_ch = 2;
	unsigned int sclk_div = 4 - 1;
	unsigned int ratio = i2s_ch * bit_depth - 1;

	/* lb_datain clk is set
	 * prepare clocks for tdmin_lb
	 */

	/* mpll, mclk */
	if (p_loopback->datalb_src >= 3) {
		mclk_freq = rate * p_loopback->mclk_fs_ratio;
		mpll_freq = mclk_freq * mul;
		clk_set_rate(p_loopback->tdminlb_mpll, mpll_freq);
		clk_set_rate(p_loopback->tdminlb_mclk, mclk_freq);
		pr_info("%s, rate:%d  mclk:%d, mpll:%d, get mclk:%lu mpll:%lu\n",
			__func__,
			rate,
			mclk_freq,
			mpll_freq,
			clk_get_rate(p_loopback->tdminlb_mclk),
			clk_get_rate(p_loopback->tdminlb_mpll));
	}

#ifdef __PTM_TDM_CLK__
	ratio = 18 * 2;
#endif

	tdminlb_set_clk(p_loopback->datalb_src, sclk_div, ratio, enable);
}

static int loopback_set_ctrl(struct loopback *p_loopback, int bitwidth)
{
	unsigned int datain_toddr_type, datalb_toddr_type;
	unsigned int datain_msb, datain_lsb, datalb_msb, datalb_lsb;
	struct data_cfg datain_cfg;
	struct data_cfg datalb_cfg;

	if (!p_loopback)
		return -EINVAL;

	switch (p_loopback->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
	case DATAIN_PDM:
		datain_toddr_type = 0;
		datain_msb = 32 - 1;
		datain_lsb = 0;
		break;
	case DATAIN_SPDIF:
		datain_toddr_type = 3;
		datain_msb = 27;
		datain_lsb = 4;
		if (bitwidth <= 24)
			datain_lsb = 28 - bitwidth;
		else
			datain_lsb = 4;
		break;
	default:
		pr_err("unsupport data in source:%d\n",
		p_loopback->datain_src);
		return -EINVAL;
	}

	switch (p_loopback->datalb_src) {
	case TDMINLB_TDMOUTA:
	case TDMINLB_TDMOUTB:
	case TDMINLB_TDMOUTC:
	case TDMINLB_PAD_TDMINA:
	case TDMINLB_PAD_TDMINB:
	case TDMINLB_PAD_TDMINC:
		if (bitwidth == 24) {
			datalb_toddr_type = 4;
			datalb_msb = 32 - 1;
			datalb_lsb = 32 - bitwidth;
		} else {
			datalb_toddr_type = 0;
			datalb_msb = 32 - 1;
			datalb_lsb = 0;
		}
		break;
	default:
		pr_err("unsupport data lb source:%d\n",
		p_loopback->datalb_src);
		return -EINVAL;
	}

	datain_cfg.ext_signed = 0;
	datain_cfg.chnum      = p_loopback->datain_chnum;
	datain_cfg.chmask     = p_loopback->datain_chmask;
	datain_cfg.type       = datain_toddr_type;
	datain_cfg.m          = datain_msb;
	datain_cfg.n          = datain_lsb;
	datain_cfg.src        = p_loopback->datain_src;

	datalb_cfg.ext_signed  = 0;
	datalb_cfg.chnum       = p_loopback->datalb_chnum;
	datalb_cfg.chmask      = p_loopback->datalb_chmask;
	datalb_cfg.type        = datalb_toddr_type;
	datalb_cfg.m           = datalb_msb;
	datalb_cfg.n           = datalb_lsb;
	datalb_cfg.datalb_src  = p_loopback->datalb_src;

	if (p_loopback->chipinfo) {
		datain_cfg.ch_ctrl_switch = p_loopback->chipinfo->ch_ctrl;
		datalb_cfg.ch_ctrl_switch = p_loopback->chipinfo->ch_ctrl;
	} else {
		datain_cfg.ch_ctrl_switch = 0;
		datalb_cfg.ch_ctrl_switch = 0;
	}

	lb_set_datain_cfg(p_loopback->id, &datain_cfg);
	lb_set_datalb_cfg(p_loopback->id, &datalb_cfg);

	tdminlb_set_format(1); /* tdmin_lb i2s mode */
	tdminlb_set_lanemask_and_chswap(0x76543210,
		p_loopback->datalb_lane_mask);
	tdminlb_set_ctrl(p_loopback->datalb_src);

	return 0;
}

static void datatin_pdm_cfg(
	struct snd_pcm_runtime *runtime,
	struct loopback *p_loopback)
{
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);
	unsigned int osr;
	struct pdm_info info;

	info.bitdepth	  = bit_depth;
	info.channels	  = p_loopback->datain_chnum;
	info.lane_masks   = p_loopback->datain_lane_mask;
	info.dclk_idx	  = p_loopback->dclk_idx;
	info.bypass       = 0;
	info.sample_count = pdm_get_sample_count(0, p_loopback->dclk_idx);
	aml_pdm_ctrl(&info);

	/* filter for pdm */
	osr = pdm_get_ors(p_loopback->dclk_idx, runtime->rate);

	aml_pdm_filter_ctrl(osr, 1);
}

static int loopback_dai_prepare(
	struct snd_pcm_substream *ss,
	struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
		struct toddr *to = p_loopback->tddr;
		unsigned int msb = 32 - 1;
		unsigned int lsb = 32 - bit_depth;
		unsigned int toddr_type;
		struct toddr_fmt fmt;
		unsigned int src;

		if (p_loopback->id == 0)
			src = LOOPBACK_A;
		else
			src = LOOPBACK_B;

		pr_info("%s Expected toddr src:%s\n",
			__func__,
			toddr_src_get_str(src));

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
			dev_err(p_loopback->dev,
				"invalid bit_depth: %d\n",
				bit_depth);
			return -EINVAL;
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

		switch (p_loopback->datain_src) {
		case DATAIN_TDMA:
		case DATAIN_TDMB:
		case DATAIN_TDMC:
			break;
		case DATAIN_SPDIF:
			break;
		case DATAIN_PDM:
			datatin_pdm_cfg(runtime, p_loopback);
			break;
		case DATAIN_LOOPBACK:
			break;
		default:
			dev_err(p_loopback->dev,
				"unexpected datain src 0x%02x\n",
				p_loopback->datain_src);
			return -EINVAL;
		}

		switch (p_loopback->datalb_src) {
		case TDMINLB_TDMOUTA:
		case TDMINLB_TDMOUTB:
		case TDMINLB_TDMOUTC:
			break;
		case TDMINLB_PAD_TDMINA:
		case TDMINLB_PAD_TDMINB:
		case TDMINLB_PAD_TDMINC:
			break;
		case SPDIFINLB_SPDIFOUTA:
		case SPDIFINLB_SPDIFOUTB:
			break;
		default:
			dev_err(p_loopback->dev,
				"unexpected datalb src 0x%02x\n",
				p_loopback->datalb_src);
			return -EINVAL;
		}

		/* config for loopback, datain, datalb */
		loopback_set_ctrl(p_loopback, bit_depth);
	}

	return 0;
}

static int loopback_dai_trigger(
	struct snd_pcm_substream *ss,
	int cmd,
	struct snd_soc_dai *dai)
{
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);

	pr_info("%s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
			dev_info(ss->pcm->card->dev, "Loopback Capture enable\n");

			pdm_fifo_reset();
			tdminlb_fifo_enable(true);

			aml_toddr_enable(p_loopback->tddr, true);
			/* loopback */
			lb_enable(p_loopback->id, true);
			/* tdminLB */
			tdminlb_enable(p_loopback->datalb_src, true);
			/* pdm */
			pdm_enable(1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (ss->stream == SNDRV_PCM_STREAM_CAPTURE) {
			dev_info(ss->pcm->card->dev, "Loopback Capture disable\n");
			pdm_enable(0);

			/* loopback */
			lb_enable(p_loopback->id, false);
			/* tdminLB */
			tdminlb_fifo_enable(false);
			tdminlb_enable(p_loopback->datalb_src, false);

			aml_toddr_enable(p_loopback->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void datain_pdm_set_clk(struct loopback *p_loopback)
{
	unsigned int pdm_sysclk_srcpll_freq, pdm_dclk_srcpll_freq;

	pdm_sysclk_srcpll_freq = clk_get_rate(p_loopback->pdm_sysclk_srcpll);
	pdm_dclk_srcpll_freq = clk_get_rate(p_loopback->pdm_dclk_srcpll);

#ifdef __PTM_PDM_CLK__
	clk_set_rate(p_loopback->pdm_sysclk, 133333351);
	clk_set_rate(p_loopback->pdm_dclk_srcpll, 24576000 * 15); /* 350m */
	clk_set_rate(p_loopback->pdm_dclk, 3072000);
#else
	clk_set_rate(p_loopback->pdm_sysclk, 133333351);

	if (pdm_dclk_srcpll_freq == 0)
		clk_set_rate(p_loopback->pdm_dclk_srcpll, 24576000);
	else
		pr_info("pdm pdm_dclk_srcpll:%lu\n",
			clk_get_rate(p_loopback->pdm_dclk_srcpll));
#endif

	clk_set_rate(p_loopback->pdm_dclk,
		pdm_dclkidx2rate(p_loopback->dclk_idx));

	pr_info("pdm pdm_sysclk:%lu pdm_dclk:%lu\n",
		clk_get_rate(p_loopback->pdm_sysclk),
		clk_get_rate(p_loopback->pdm_dclk));
}

static void datalb_tdminlb_set_clk(struct loopback *p_loopback)
{
	int mpll_freq = p_loopback->sysclk_freq * 2;

	clk_set_rate(p_loopback->tdminlb_mpll, mpll_freq);
	clk_set_rate(p_loopback->tdminlb_mclk, p_loopback->sysclk_freq);

	pr_info("tdmin lb mpll:%lu mclk:%lu\n",
		clk_get_rate(p_loopback->tdminlb_mpll),
		clk_get_rate(p_loopback->tdminlb_mclk));
}

static int loopback_dai_hw_params(
	struct snd_pcm_substream *ss,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);
	unsigned int rate, channels;
	snd_pcm_format_t format;
	int ret = 0;

	rate = params_rate(params);
	channels = params_channels(params);
	format = params_format(params);

	pr_info("%s:rate:%d, sysclk:%d\n",
		__func__,
		rate,
		p_loopback->sysclk_freq);

	switch (p_loopback->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
		break;
	case DATAIN_SPDIF:
		break;
	case DATAIN_PDM:
		datain_pdm_set_clk(p_loopback);
		break;
	case DATAIN_LOOPBACK:
		break;
	default:
		break;
	}

	/* datalb */
	switch (p_loopback->datalb_src) {
	case TDMINLB_TDMOUTA ... TDMINLB_PAD_TDMINC:
		datalb_tdminlb_set_clk(p_loopback);
		break;
	case SPDIFINLB_SPDIFOUTA ... SPDIFINLB_SPDIFOUTB:
		break;
	default:
		break;
	}

	loopback_set_clk(p_loopback, runtime->rate, true);

	return ret;
}

int loopback_dai_hw_free(
	struct snd_pcm_substream *ss,
	struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = ss->runtime;
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);

	pr_info("%s\n", __func__);

	if (0)
		loopback_set_clk(p_loopback, runtime->rate, false);

	return 0;
}

static int loopback_dai_set_fmt(
	struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);

	pr_info("asoc loopback_dai_set_fmt, %#x, %p\n", fmt, p_loopback);

	return 0;
}

static int loopback_dai_set_sysclk(
	struct snd_soc_dai *dai,
	int clk_id, unsigned int freq, int dir)
{
	struct loopback *p_loopback = snd_soc_dai_get_drvdata(dai);

	p_loopback->sysclk_freq = freq;
	pr_info("\n%s, %d, %d, %d\n",
		__func__,
		clk_id, freq, dir);

	return 0;
}

static struct snd_soc_dai_ops loopback_dai_ops = {
	.startup    = loopback_dai_startup,
	.shutdown   = loopback_dai_shutdown,
	.prepare    = loopback_dai_prepare,
	.trigger    = loopback_dai_trigger,
	.hw_params  = loopback_dai_hw_params,
	.hw_free    = loopback_dai_hw_free,
	.set_fmt    = loopback_dai_set_fmt,
	.set_sysclk = loopback_dai_set_sysclk,
};

static struct snd_soc_dai_driver loopback_dai[] = {
	{
		.name    = "LOOPBACK-A",
		.id      = 0,
		.probe   = loopback_dai_probe,
		.remove  = loopback_dai_remove,

		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates        = LOOPBACK_RATES,
		     .formats      = LOOPBACK_FORMATS,
		},
		.ops     = &loopback_dai_ops,
	},
	{
		.name    = "LOOPBACK-B",
		.id      = 1,
		.probe   = loopback_dai_probe,
		.remove  = loopback_dai_remove,

		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates        = LOOPBACK_RATES,
		     .formats      = LOOPBACK_FORMATS,
		},
		.ops     = &loopback_dai_ops,
	},
};

static const char *const datain_src_texts[] = {
	"TDMIN_A",
	"TDMIN_B",
	"TDMIN_C",
	"SPDIFIN",
	"PDMIN",
};

static const struct soc_enum datain_src_enum =
	SOC_ENUM_SINGLE(EE_AUDIO_LB_CTRL0, 0, ARRAY_SIZE(datain_src_texts),
	datain_src_texts);

static int datain_src_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct loopback *p_loopback = dev_get_drvdata(component->dev);

	if (!p_loopback)
		return 0;

	ucontrol->value.enumerated.item[0] = p_loopback->datain_src;

	return 0;
}

static int datain_src_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct loopback *p_loopback = dev_get_drvdata(component->dev);

	if (!p_loopback)
		return 0;

	p_loopback->datain_src = ucontrol->value.enumerated.item[0];

	lb_set_datain_src(p_loopback->id, p_loopback->datain_src);

	return 0;
}

static const char *const datalb_tdminlb_texts[] = {
	"TDMOUT_A",
	"TDMOUT_B",
	"TDMOUT_C",
	"TDMIN_A",
	"TDMIN_B",
	"TDMIN_C",
};

static const struct soc_enum datalb_tdminlb_enum =
	SOC_ENUM_SINGLE(EE_AUDIO_TDMIN_LB_CTRL,
		20,
		ARRAY_SIZE(datalb_tdminlb_texts),
		datalb_tdminlb_texts);

static int datalb_tdminlb_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct loopback *p_loopback = dev_get_drvdata(component->dev);

	if (!p_loopback)
		return 0;

	ucontrol->value.enumerated.item[0] = p_loopback->datalb_src;

	return 0;
}

static int datalb_tdminlb_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct loopback *p_loopback = dev_get_drvdata(component->dev);

	if (!p_loopback)
		return 0;

	p_loopback->datalb_src = ucontrol->value.enumerated.item[0];

	tdminlb_set_src(p_loopback->datalb_src);

	return 0;
}

static const struct snd_kcontrol_new snd_loopback_controls[] = {
	/* loopback data in source */
	SOC_ENUM_EXT("Loopback datain source",
		datain_src_enum,
		datain_src_get_enum,
		datain_src_set_enum),

	/* loopback data tdmin lb */
	SOC_ENUM_EXT("Loopback tdmin lb source",
		datalb_tdminlb_enum,
		datalb_tdminlb_get_enum,
		datalb_tdminlb_set_enum),
};

static const struct snd_soc_component_driver loopback_component = {
	.name         = DRV_NAME,
	.controls     = snd_loopback_controls,
	.num_controls = ARRAY_SIZE(snd_loopback_controls),
};

static int snd_soc_of_get_slot_mask(
	struct device_node *np,
	const char *prop_name,
	unsigned int *mask)
{
	u32 val;
	const __be32 *of_slot_mask = of_get_property(np, prop_name, &val);
	int i;

	if (!of_slot_mask)
		return -EINVAL;

	val /= sizeof(u32);
	for (i = 0; i < val; i++)
		if (be32_to_cpup(&of_slot_mask[i]))
			*mask |= (1 << i);

	return val;
}

static int datain_pdm_parse_of(
	struct device *dev,
	struct loopback *p_loopback)
{
	int ret;

	/* clock gate */
	p_loopback->pdm_clk_gate = devm_clk_get(dev, "pdm_gate");
	if (IS_ERR(p_loopback->pdm_clk_gate)) {
		dev_err(dev,
			"Can't get pdm gate\n");
		return PTR_ERR(p_loopback->pdm_clk_gate);
	}

	p_loopback->pdm_sysclk_srcpll = devm_clk_get(dev, "pdm_sysclk_srcpll");
	if (IS_ERR(p_loopback->pdm_sysclk_srcpll)) {
		dev_err(dev,
			"Can't retrieve pll clock\n");
		ret = PTR_ERR(p_loopback->pdm_sysclk_srcpll);
		goto err;
	}

	p_loopback->pdm_dclk_srcpll = devm_clk_get(dev, "pdm_dclk_srcpll");
	if (IS_ERR(p_loopback->pdm_dclk_srcpll)) {
		dev_err(dev,
			"Can't retrieve data src clock\n");
		ret = PTR_ERR(p_loopback->pdm_dclk_srcpll);
		goto err;
	}

	p_loopback->pdm_sysclk = devm_clk_get(dev, "pdm_sysclk");
	if (IS_ERR(p_loopback->pdm_sysclk)) {
		dev_err(dev,
			"Can't retrieve pdm_sysclk clock\n");
		ret = PTR_ERR(p_loopback->pdm_sysclk);
		goto err;
	}

	p_loopback->pdm_dclk = devm_clk_get(dev, "pdm_dclk");
	if (IS_ERR(p_loopback->pdm_dclk)) {
		dev_err(dev,
			"Can't retrieve pdm_dclk clock\n");
		ret = PTR_ERR(p_loopback->pdm_dclk);
		goto err;
	}

	ret = clk_set_parent(p_loopback->pdm_sysclk,
			p_loopback->pdm_sysclk_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set pdm_sysclk parent clock\n");
		ret = PTR_ERR(p_loopback->pdm_sysclk);
		goto err;
	}

	ret = clk_set_parent(p_loopback->pdm_dclk,
			p_loopback->pdm_dclk_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set pdm_dclk parent clock\n");
		ret = PTR_ERR(p_loopback->pdm_dclk);
		goto err;
	}

	return 0;

err:
	return ret;
}

static int datain_parse_of(
	struct device_node *node,
	struct loopback *p_loopback)
{
	struct platform_device *pdev;
	int ret;

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		dev_err(&pdev->dev,
			"failed to find platform device\n");
		return -EINVAL;
	}

	switch (p_loopback->datain_src) {
	case DATAIN_TDMA:
	case DATAIN_TDMB:
	case DATAIN_TDMC:
		break;
	case DATAIN_SPDIF:
		break;
	case DATAIN_PDM:
		ret = datain_pdm_parse_of(&pdev->dev, p_loopback);
		if (ret < 0)
			goto err;
		break;
	case DATAIN_LOOPBACK:
		break;
	default:
		break;
	}

	return 0;
err:
	pr_err("%s, error:%d\n", __func__, ret);
	return -EINVAL;
}

static int datalb_tdminlb_parse_of(
	struct device_node *node,
	struct loopback *p_loopback)
{
	struct platform_device *pdev;
	int ret;

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		dev_err(&pdev->dev,
			"failed to find platform device\n");
		return -EINVAL;
	}

	/* mpll used for tdmin_lb */
	p_loopback->tdminlb_mpll = devm_clk_get(&pdev->dev, "tdminlb_mpll");
	if (IS_ERR(p_loopback->tdminlb_mpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve tdminlb_mpll clock\n");
		return PTR_ERR(p_loopback->tdminlb_mpll);
	}
	p_loopback->tdminlb_mclk = devm_clk_get(&pdev->dev, "tdminlb_mclk");
	if (IS_ERR(p_loopback->tdminlb_mclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve tdminlb_mclk clock\n");
		return PTR_ERR(p_loopback->tdminlb_mclk);
	}

	ret = clk_set_parent(p_loopback->tdminlb_mclk,
			p_loopback->tdminlb_mpll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set tdminlb_mclk parent clock\n");
		ret = PTR_ERR(p_loopback->tdminlb_mpll);
		goto err;
	}

	ret = of_property_read_u32(node, "mclk-fs",
		&p_loopback->mclk_fs_ratio);
	if (ret) {
		pr_warn_once("failed to get mclk-fs node, set it default\n");
		p_loopback->mclk_fs_ratio = 256;
		ret = 0;
	}

	return 0;
err:
	pr_err("%s, error:%d\n", __func__, ret);
	return -EINVAL;
}

static int loopback_parse_of(
	struct device_node *node,
	struct loopback *p_loopback)
{
	struct platform_device *pdev;
	int ret;

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		dev_err(&pdev->dev, "failed to find platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = of_property_read_u32(node, "datain_src",
		&p_loopback->datain_src);
	if (ret) {
		pr_err("failed to get datain_src\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datain_chnum",
		&p_loopback->datain_chnum);
	if (ret) {
		pr_err("failed to get datain_chnum\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datain_chmask",
		&p_loopback->datain_chmask);
	if (ret) {
		pr_err("failed to get datain_chmask\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = snd_soc_of_get_slot_mask(node, "datain-lane-mask-in",
		&p_loopback->datain_lane_mask);
	if (ret < 0) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "datain lane-slot-mask should be set\n");
		goto fail;
	}

	ret = of_property_read_u32(node, "datalb_src",
		&p_loopback->datalb_src);
	if (ret) {
		pr_err("failed to get datalb_src\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datalb_chnum",
		&p_loopback->datalb_chnum);
	if (ret) {
		pr_err("failed to get datalb_chnum\n");
		ret = -EINVAL;
		goto fail;
	}
	ret = of_property_read_u32(node, "datalb_chmask",
		&p_loopback->datalb_chmask);
	if (ret) {
		pr_err("failed to get datalb_chmask\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = snd_soc_of_get_slot_mask(node, "datalb-lane-mask-in",
		&p_loopback->datalb_lane_mask);
	if (ret < 0) {
		ret = -EINVAL;
		pr_err("datalb lane-slot-mask should be set\n");
		goto fail;
	}

	pr_info("\tdatain_src:%d, datain_chnum:%d, datain_chumask:%x\n",
		p_loopback->datain_src,
		p_loopback->datain_chnum,
		p_loopback->datain_chmask);
	pr_info("\tdatalb_src:%d, datalb_chnum:%d, datalb_chmask:%x\n",
		p_loopback->datalb_src,
		p_loopback->datalb_chnum,
		p_loopback->datalb_chmask);
	pr_info("\tdatain_lane_mask:0x%x, datalb_lane_mask:0x%x\n",
		p_loopback->datain_lane_mask,
		p_loopback->datalb_lane_mask);

	ret = datain_parse_of(node, p_loopback);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to parse datain\n");
		return ret;
	}
	ret = datalb_tdminlb_parse_of(node, p_loopback);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to parse datalb\n");
		return ret;
	}

	return 0;

fail:
	return ret;
}

static int loopback_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct loopback *p_loopback = NULL;
	struct loopback_chipinfo *p_chipinfo = NULL;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent = NULL;
	struct aml_audio_controller *actrl = NULL;
	int ret = 0;

	p_loopback = devm_kzalloc(&pdev->dev,
			sizeof(struct loopback),
			GFP_KERNEL);
	if (!p_loopback)
		return -ENOMEM;

	/* match data */
	p_chipinfo = (struct loopback_chipinfo *)
		of_device_get_match_data(dev);
	if (p_chipinfo) {
		p_loopback->chipinfo = p_chipinfo;
		p_loopback->id = p_chipinfo->id;
	} else
		dev_warn_once(dev,
			"check whether to update loopback chipinfo\n");

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_loopback->actrl = actrl;

	ret = loopback_parse_of(dev->of_node, p_loopback);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to parse loopback info\n");
		return ret;
	}

	p_loopback->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, p_loopback);

	ret = devm_snd_soc_register_component(&pdev->dev,
				&loopback_component,
				&loopback_dai[p_loopback->id],
				1);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, p_loopback->id:%d register soc platform\n",
		__func__,
		p_loopback->id);

	return devm_snd_soc_register_platform(dev,
		&loopback_platform_drv);
}

static struct platform_driver loopback_platform_driver = {
	.driver = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(loopback_device_id),
	},
	.probe  = loopback_platform_probe,
};
module_platform_driver(loopback_platform_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("Amlogic Loopback driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
