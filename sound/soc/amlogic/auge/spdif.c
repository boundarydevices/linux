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
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/extcon.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>


#include "ddr_mngr.h"
#include "spdif_hw.h"
#include "audio_utils.h"

#define DRV_NAME "aml_spdif"

#define SPDIF_A	0
#define SPDIF_B	1

struct spdif_chipinfo {
	unsigned int id;

	/* add ch_cnt to ch_num */
	bool chnum_en;
	/* Reg_clr_interrupt[7:0] for each bit of irq_status[7:0]; */
	bool clr_irq_bits;
	/* find PaPb */
	bool irq_papb;
	/* reg_hold_start_en; 1: add delay to match TDM out when share buff; */
	bool hold_start;
	/* eq/drc */
	bool eq_drc_en;
};

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
	int irq_spdifin;
	struct toddr *tddr;
	struct frddr *fddr;

	/* external connect */
	struct extcon_dev *edev;

	unsigned int id;
	struct spdif_chipinfo *chipinfo;
	unsigned int clk_cont; /* CONTINUOUS CLOCK */
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

static const unsigned int spdifin_extcon[] = {
	EXTCON_SPDIFIN_SAMPLERATE,
	EXTCON_SPDIFIN_AUDIOTYPE,
	EXTCON_NONE,
};

/* current sample mode and its sample rate */
int sample_mode[] = {
	24000,
	32000,
	44100,
	46000,
	48000,
	96000,
	192000,
};

static const char *const spdifin_samplerate[] = {
	"N/A",
	"24000",
	"32000",
	"44100",
	"46000",
	"48000",
	"96000",
	"192000"
};

static int spdifin_samplerate_get_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int val = spdifin_get_sample_rate();

	if (val == 0x7)
		val = 0;
	else
		val += 1;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static const struct soc_enum spdifin_sample_rate_enum[] = {
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdifin_samplerate),
			spdifin_samplerate),
};

/* spdif in audio format detect: LPCM or NONE-LPCM */
struct sppdif_audio_info {
	unsigned char aud_type;
	/*IEC61937 package presamble Pc value*/
	short pc;
	char *aud_type_str;
};

static const char *const spdif_audio_type_texts[] = {
	"LPCM",
	"AC3",
	"EAC3",
	"DTS",
	"DTS-HD",
	"TRUEHD",
	"PAUSE"
};

static const struct sppdif_audio_info type_texts[] = {
	{0, 0, "LPCM"},
	{1, 0x1, "AC3"},
	{2, 0x15, "EAC3"},
	{3, 0xb, "DTS-I"},
	{3, 0x0c, "DTS-II"},
	{3, 0x0d, "DTS-III"},
	{3, 0x11, "DTS-IV"},
	{4, 0, "DTS-HD"},
	{5, 0x16, "TRUEHD"},
	{6, 0x103, "PAUSE"},
	{6, 0x003, "PAUSE"},
	{6, 0x100, "PAUSE"},
};
static const struct soc_enum spdif_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_audio_type_texts),
			spdif_audio_type_texts);

static int spdifin_audio_type_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int audio_type = 0;
	int i;
	int total_num = sizeof(type_texts)/sizeof(struct sppdif_audio_info);
	int pc = spdifin_get_audio_type();

	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			break;
		}
	}
	ucontrol->value.enumerated.item[0] = audio_type;

	return 0;
}

static const struct snd_kcontrol_new snd_spdif_controls[] = {

	SOC_ENUM_EXT("SPDIFIN Sample Rate", spdifin_sample_rate_enum,
				spdifin_samplerate_get_enum,
				NULL),

	SOC_ENUM_EXT("SPDIFIN Audio Type",
			 spdif_audio_type_enum,
			 spdifin_audio_type_get_enum,
			 NULL),
};

static void spdifin_status_event(struct aml_spdif *p_spdif)
{
	int intrpt_status;

	if (p_spdif == NULL)
		return;

	/*
	 * interrupt status, check and clear by reg_clk_interrupt;
	 */
	intrpt_status = aml_spdifin_status_check(p_spdif->actrl);

	if (intrpt_status & 0x1)
		pr_warn_once("over flow!!\n");
	if (intrpt_status & 0x2)
		pr_warn_once("parity error\n");

	if (intrpt_status & 0x4) {
		int mode = (intrpt_status >> 28) & 0x7;

		pr_warn_once("sample mode changed\n");
		if (mode == 0x7) {
			pr_debug("Default value, not detect sample rate\n");
			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_SAMPLERATE, 0);
		} else if (mode >= 0) {
			pr_debug("Event: EXTCON_SPDIFIN_SAMPLERATE, new sample rate:%d\n",
				sample_mode[mode]);

			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_SAMPLERATE, 1);
		}
	}

	if (intrpt_status & 0x8) {
		pr_warn_once("Pc changed, try to read spdifin audio type\n");
		extcon_set_state(p_spdif->edev,
			EXTCON_SPDIFIN_AUDIOTYPE, 1);
	} else
		extcon_set_state(p_spdif->edev,
			EXTCON_SPDIFIN_AUDIOTYPE, 0);

	if (intrpt_status & 0x10)
		pr_warn_once("Pd changed\n");
	if (intrpt_status & 0x20)
		pr_warn_once("nonpcm to pcm\n");
	if (intrpt_status & 0x40)
		pr_warn_once("valid changed\n");
}

static irqreturn_t aml_spdif_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

/* detect PCM/RAW and sample changes by the source */
static irqreturn_t aml_spdifin_status_isr(int irq, void *devid)
{
	struct aml_spdif *p_spdif = (struct aml_spdif *)devid;

	aml_spdifin_status_check(p_spdif->actrl);

	spdifin_status_event(p_spdif);

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
			aml_spdif_ddr_isr, substream);
		if (p_spdif->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr\n");
			return -ENXIO;
		}
	} else {
		p_spdif->tddr = aml_audio_register_toddr(dev,
			p_spdif->actrl,
			aml_spdif_ddr_isr, substream);
		if (p_spdif->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr\n");
			return -ENXIO;
		}

		ret = request_irq(p_spdif->irq_spdifin,
				aml_spdifin_status_isr, 0, "irq_spdifin",
				p_spdif);
		if (ret) {
			dev_err(p_spdif->dev, "failed to claim irq_spdifin %u\n",
						p_spdif->irq_spdifin);
			return ret;
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
		aml_audio_unregister_frddr(p_spdif->dev, substream);
	} else {
		aml_audio_unregister_toddr(p_spdif->dev, substream);
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
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_spdif->fddr);
	else
		addr = aml_toddr_get_position(p_spdif->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
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
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;

	if (p_spdif->id == SPDIF_A) {
		ret = snd_soc_add_dai_controls(cpu_dai, snd_spdif_controls,
					ARRAY_SIZE(snd_spdif_controls));
		if (ret < 0)
			pr_err("%s, failed add snd spdif controls\n", __func__);
	}

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

	if (!p_spdif->clk_cont)
		aml_spdif_fifo_reset(p_spdif->actrl,
			substream->stream,
			p_spdif->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		if (p_spdif->clk_cont) {
			pr_info("spdif_%s keep clk continuous\n",
				(p_spdif->id == 0) ? "a":"b");
			return 0;
		}
		/* enable clock gate */
		ret = clk_prepare_enable(p_spdif->gate_spdifout);

		/* clock parent */
		ret = clk_set_parent(p_spdif->clk_spdifout, p_spdif->sysclk);
		if (ret) {
			pr_err("Can't set clk_spdifout parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifout);
			goto err;
		}

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

		/* clock parent */
		ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
		if (ret) {
			pr_err("Can't set clk_spdifin parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifin);
			goto err;
		}

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
		if (p_spdif->clk_cont) {
			pr_info("spdif_%s keep clk continuous\n",
				(p_spdif->id == 0) ? "a":"b");
			return;
		}

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
	unsigned int fifo_id = 0;

	bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;
		enum frddr_dest dst;
		struct iec958_chsts chsts;

		switch (p_spdif->id) {
		case 0:
			dst = SPDIFOUT;
			break;
		case 1:
			dst = SPDIFOUT_B;
			break;
		default:
			dev_err(p_spdif->dev, "invalid id: %d\n", p_spdif->id);
			return -EINVAL;
		}

		fifo_id = aml_frddr_get_fifo_id(fr);
		aml_frddr_select_dst(fr, dst);
		aml_frddr_set_fifos(fr, 0x40, 0x20);

		/* check channel status info, and set them */
		spdif_get_channel_status_info(&chsts, runtime->rate);
		spdif_set_channel_status_info(&chsts, p_spdif->id);

		/* TOHDMITX_CTRL0 */
		if (p_spdif->id == 1) {
			spdifoutb_to_hdmitx_ctrl(p_spdif->id);

			/* notify to hdmitx */
			spdif_notify_to_hdmitx(substream);
		}
	} else {
		struct toddr *to = p_spdif->tddr;
		unsigned int msb, lsb, toddr_type;

		if (loopback_is_enable()) {
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
				pr_err(
					"runtime format invalid bit_depth: %d\n",
					bit_depth);
				return -EINVAL;
			}
			msb = 32 - 1;
			lsb = 32 - bit_depth;
		} else {
			switch (bit_depth) {
			case 8:
			case 16:
				toddr_type = 0;
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

			msb = 28 - 1;
			if (bit_depth <= 24)
				lsb = 28 - bit_depth;
			else
				lsb = 4;
		}

		// to ddr spdifin
		aml_toddr_select_src(to, SPDIFIN);
		aml_toddr_set_format(to, toddr_type, msb, lsb,
			runtime->channels,
			bit_depth);
		aml_toddr_set_fifos(to, 0x40);
	}

	aml_spdif_fifo_ctrl(p_spdif->actrl, bit_depth,
			substream->stream, p_spdif->id, fifo_id);

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
			substream->stream, p_spdif->id, true);
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
		if (!p_spdif->clk_cont)
			aml_spdif_enable(p_spdif->actrl,
				substream->stream, p_spdif->id, false);
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

/*#define G12A_PTM*/

static void aml_set_spdifclk(struct aml_spdif *p_spdif)
{
	unsigned int mpll_freq = 0;

	pr_info("asoc debug: %s-%d, sys freq:%d\n", __func__, __LINE__,
		p_spdif->sysclk_freq);
	if (p_spdif->sysclk_freq) {
		unsigned int mul = 4;
		int ret;

		if (spdif_is_4x_clk()) {
			pr_info("set 4x audio clk for 958\n");
			p_spdif->sysclk_freq *= 4;
		} else {
			pr_info("set normal 512 fs /4 fs\n");
		}
		mpll_freq = p_spdif->sysclk_freq * mul;

#ifdef G12A_PTM
		mpll_freq = p_spdif->sysclk_freq * 57;
#endif
		clk_set_rate(p_spdif->sysclk, mpll_freq);
		clk_set_rate(p_spdif->clk_spdifout,
			p_spdif->sysclk_freq);

		ret = clk_prepare_enable(p_spdif->sysclk);
		if (ret) {
			pr_err("Can't enable pcm sysclk clock: %d\n", ret);
			return;
		}
		ret = clk_prepare_enable(p_spdif->clk_spdifout);
		if (ret) {
			pr_err("Can't enable clk_spdifout clock: %d\n", ret);
			return;
		}
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

static struct snd_soc_dai_driver aml_spdif_dai[] = {
	{
		.name = "SPDIF",
		.id = 1,
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
			 /* spdif 2ch + tdmin_lb 8ch(fake for loopback) */
		     .channels_max = 10,
		     .rates = AML_DAI_SPDIF_RATES,
		     .formats = AML_DAI_SPDIF_FORMATS,
		},
		.ops = &aml_dai_spdif_ops,
	},
	{
		.name = "SPDIF-B",
		.id = 2,
		.probe = aml_dai_spdif_probe,
		.remove = aml_dai_spdif_remove,
		.playback = {
			  .channels_min = 1,
			  .channels_max = 2,
			  .rates = AML_DAI_SPDIF_RATES,
			  .formats = AML_DAI_SPDIF_FORMATS,
		},
		.ops = &aml_dai_spdif_ops,
	}
};

static const struct snd_soc_component_driver aml_spdif_component = {
	.name		= DRV_NAME,
};

static int aml_spdif_parse_of(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);

	int ret = 0;

	/* clock for spdif in */
	if (p_spdif->id == 0) {
		/* clock gate */
		p_spdif->gate_spdifin = devm_clk_get(dev, "gate_spdifin");
		if (IS_ERR(p_spdif->gate_spdifin)) {
			dev_err(dev, "Can't get spdifin gate\n");
			return PTR_ERR(p_spdif->gate_spdifin);
		}
		/* pll */
		p_spdif->fixed_clk = devm_clk_get(dev, "fixed_clk");
		if (IS_ERR(p_spdif->fixed_clk)) {
			dev_err(dev, "Can't retrieve fixed_clk\n");
			return PTR_ERR(p_spdif->fixed_clk);
		}
		/* spdif in clk */
		p_spdif->clk_spdifin = devm_clk_get(dev, "clk_spdifin");
		if (IS_ERR(p_spdif->clk_spdifin)) {
			dev_err(dev, "Can't retrieve spdifin clock\n");
			return PTR_ERR(p_spdif->clk_spdifin);
		}
		ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
		if (ret) {
			dev_err(dev,
				"Can't set clk_spdifin parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifin);
			return ret;
		}

		/* irqs */
		p_spdif->irq_spdifin =
			platform_get_irq_byname(pdev, "irq_spdifin");
		if (p_spdif->irq_spdifin < 0)
			dev_err(dev, "platform_get_irq_byname failed\n");

		/* spdif pinmux */
		p_spdif->pin_ctl = devm_pinctrl_get_select(dev, "spdif_pins");
		if (IS_ERR(p_spdif->pin_ctl)) {
			dev_info(dev, "aml_spdif_get_pins error!\n");
			return PTR_ERR(p_spdif->pin_ctl);
		}

		/* spdifin sample rate change event */
		p_spdif->edev = devm_extcon_dev_allocate(dev, spdifin_extcon);
		if (IS_ERR(p_spdif->edev)) {
			pr_err("failed to allocate spdifin extcon!!!\n");
			ret = -ENOMEM;
			return ret;
		}
		p_spdif->edev->dev.parent  = dev;
		p_spdif->edev->name = "spdifin_event";

		dev_set_name(&p_spdif->edev->dev, "spdifin_event");
		ret = extcon_dev_register(p_spdif->edev);
		if (ret < 0)
			pr_err("SPDIF IN extcon failed to register!!, ignore it\n");
	}

	/* clock for spdif in */
	/* clock gate */
	p_spdif->gate_spdifout = devm_clk_get(dev, "gate_spdifout");
	if (IS_ERR(p_spdif->gate_spdifout)) {
		dev_err(dev, "Can't get spdifout gate\n");
		return PTR_ERR(p_spdif->gate_spdifout);
	}
	/* pll */
	p_spdif->sysclk = devm_clk_get(dev, "sysclk");
	if (IS_ERR(p_spdif->sysclk)) {
		dev_err(dev, "Can't retrieve sysclk clock\n");
		return PTR_ERR(p_spdif->sysclk);
	}
	/* spdif out clock */
	p_spdif->clk_spdifout = devm_clk_get(dev, "clk_spdifout");
	if (IS_ERR(p_spdif->clk_spdifout)) {
		dev_err(dev, "Can't retrieve spdifout clock\n");
		return PTR_ERR(p_spdif->clk_spdifout);
	}

	return 0;
}

struct spdif_chipinfo axg_spdif_chipinfo = {
	.id = SPDIF_A,
};

struct spdif_chipinfo g12a_spdif_a_chipinfo = {
	.id           = SPDIF_A,
	.chnum_en     = true,
	.clr_irq_bits = true,
	.irq_papb     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

struct spdif_chipinfo g12a_spdif_b_chipinfo = {
	.id           = SPDIF_B,
	.chnum_en     = true,
	.clr_irq_bits = true,
	.irq_papb     = true,
	.hold_start   = true,
	.eq_drc_en    = true,
};

static const struct of_device_id aml_spdif_device_id[] = {
	{
		.compatible = "amlogic, axg-snd-spdif",
		.data = &axg_spdif_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-spdif-a",
		.data = &g12a_spdif_a_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-spdif-b",
		.data = &g12a_spdif_b_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_spdif_device_id);

static int aml_spdif_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct aml_spdif *aml_spdif = NULL;
	struct spdif_chipinfo *p_spdif_chipinfo;
	int ret = 0;


	aml_spdif = devm_kzalloc(dev, sizeof(struct aml_spdif), GFP_KERNEL);
	if (!aml_spdif)
		return -ENOMEM;

	aml_spdif->dev = dev;
	dev_set_drvdata(dev, aml_spdif);

	/* match data */
	p_spdif_chipinfo = (struct spdif_chipinfo *)
		of_device_get_match_data(dev);
	if (p_spdif_chipinfo) {
		aml_spdif->id = p_spdif_chipinfo->id;
		/* for spdif_b, clk be continuous,
		 * and keep silence when no valid data
		 */
		if (aml_spdif->id == 1)
			aml_spdif->clk_cont = 1;

		if (aml_spdif->clk_cont)
			spdifout_play_with_zerodata(aml_spdif->id);

		aml_spdif->chipinfo = p_spdif_chipinfo;
	} else
		dev_warn_once(dev,
			"check whether to update spdif chipinfo\n");

	pr_info("%s, spdif ID = %u\n", __func__, aml_spdif->id);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	aml_spdif->actrl = actrl;

	ret = aml_spdif_parse_of(pdev);
	if (ret)
		return -EINVAL;

	ret = devm_snd_soc_register_component(dev, &aml_spdif_component,
		&aml_spdif_dai[aml_spdif->id], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}

	pr_info("%s, register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &aml_spdif_platform);
}

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
