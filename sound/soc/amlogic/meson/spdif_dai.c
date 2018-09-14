/*
 * sound/soc/amlogic/meson/spdif_dai.c
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
#define pr_fmt(fmt) "snd_spdif_dai: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/soundcard.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/major.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/amlogic/iomap.h>
#include "audio_hw.h"
#include "spdif_dai.h"
#include "i2s.h"
#include <linux/amlogic/media/sound/aout_notify.h>
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

struct aml_spdif {
	struct clk *clk_mpl1;
	struct clk *clk_i958;
	struct clk *clk_mclk;
	struct clk *clk_spdif;
	struct clk *clk_81;
	int old_samplerate;
	uint clk_div;
	/* spdif dai data in source select.
	 * !Check this with chip spec.
	 */
	uint src;
};
struct aml_spdif *spdif_p;

unsigned int clk81;
EXPORT_SYMBOL(clk81);

static int old_samplerate = -1;
static int flag_samesrc = -1;

static void set_IEC958_clock_div(uint div)
{
	if (div == spdif_p->clk_div)
		return;

	if (div > 0 && div <= 4) {
		pr_info("set 958 audio clk div %d\n", div);
		audio_set_spdif_clk_div(div);
		spdif_p->clk_div = div;
	}
}

void aml_spdif_play(int samesrc)
{
	if (!is_meson_tv_chipset()) {
		uint div = 0;
		static int iec958buf[DEFAULT_PLAYBACK_SIZE];
		struct _aiu_958_raw_setting_t set;
		struct iec958_chsts chstat;
		struct snd_pcm_substream substream;
		struct snd_pcm_runtime runtime;
		int size = DEFAULT_PLAYBACK_SIZE;

		substream.runtime = &runtime;
		runtime.rate = 48000;
		runtime.format = SNDRV_PCM_FORMAT_S16_LE;
		runtime.channels = 2;
		runtime.sample_bits = 16;
		memset((void *)(&set), 0, sizeof(set));
		memset((void *)(&chstat), 0, sizeof(chstat));
		set.chan_stat = &chstat;
		set.chan_stat->chstat0_l = 0x0100;
		set.chan_stat->chstat0_r = 0x0100;
		set.chan_stat->chstat1_l = 0X200;
		set.chan_stat->chstat1_r = 0X200;
		audio_hw_958_enable(0);
		if (old_samplerate != AUDIO_CLK_FREQ_48
				|| samesrc != flag_samesrc) {
			pr_info("enterd %s,set_clock:%d,sample_rate=%d\n",
			__func__, old_samplerate, AUDIO_CLK_FREQ_48);
			old_samplerate = AUDIO_CLK_FREQ_48;
			flag_samesrc = samesrc;
			aml_set_spdif_clk(48000 * 512, samesrc);
		}
		// if (IEC958_mode_codec == 4 || IEC958_mode_codec == 5 ||
		// IEC958_mode_codec == 7 || IEC958_mode_codec == 8) {
		//	pr_info("set 4x audio clk for 958\n");
		//	div = 1;
		// } else if (samesrc) {
		if (samesrc) {
			pr_debug("share the same clock\n");
			div = 2;
		} else {
			pr_info("set normal 512 fs /4 fs\n");
			div = 4;
		}

		set_IEC958_clock_div(div);
		audio_util_set_dac_958_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
		/*clear the same source function as new raw data output */
		audio_i2s_958_same_source(samesrc);
		memset(iec958buf, 0, sizeof(iec958buf));
		audio_set_958outbuf((virt_to_phys(iec958buf)
			+ (DEFAULT_PLAYBACK_SIZE - 1))
				& (~(DEFAULT_PLAYBACK_SIZE - 1)),
			size, 0);
		audio_set_958_mode(AIU_958_MODE_PCM16, &set);
		aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, &substream);
		audio_hw_958_enable(1);
	}
}

static void aml_spdif_play_stop(void)
{
	audio_hw_958_enable(0);
}

static int aml_dai_spdif_set_sysclk(struct snd_soc_dai *cpu_dai,
				    int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_dai_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				 struct snd_soc_dai *dai)
{

	struct snd_soc_pcm_runtime *rtd = NULL;

	rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pr_info("aiu 958 playback enable\n");
			audio_hw_958_enable(1);
		} else {
			pr_info("spdif in capture enable\n");
			audio_in_spdif_enable(1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pr_info("aiu 958 playback disable\n");
			audio_hw_958_enable(0);
		} else {
			pr_info("spdif in capture disable\n");
			audio_in_spdif_enable(0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void aml_hw_iec958_init(struct snd_pcm_substream *substream, int samesrc)
{
	struct _aiu_958_raw_setting_t set;
	struct iec958_chsts chstat;
	unsigned int i2s_mode, iec958_mode;
	unsigned int start, size;
	int sample_rate;
	int div;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (buf == NULL && runtime == NULL) {
		pr_info("buf/%p runtime/%p\n", buf, runtime);
		return;
	}

	i2s_mode = AIU_I2S_MODE_PCM16;
	sample_rate = AUDIO_CLK_FREQ_48;
	memset((void *)(&set), 0, sizeof(set));
	memset((void *)(&chstat), 0, sizeof(chstat));
	set.chan_stat = &chstat;
	switch (runtime->rate) {
	case 192000:
		sample_rate = AUDIO_CLK_FREQ_192;
		break;
	case 176400:
		sample_rate = AUDIO_CLK_FREQ_1764;
		break;
	case 96000:
		sample_rate = AUDIO_CLK_FREQ_96;
		break;
	case 88200:
		sample_rate = AUDIO_CLK_FREQ_882;
		break;
	case 48000:
		sample_rate = AUDIO_CLK_FREQ_48;
		break;
	case 44100:
		sample_rate = AUDIO_CLK_FREQ_441;
		break;
	case 32000:
		sample_rate = AUDIO_CLK_FREQ_32;
		break;
	case 8000:
		sample_rate = AUDIO_CLK_FREQ_8;
		break;
	case 11025:
		sample_rate = AUDIO_CLK_FREQ_11;
		break;
	case 16000:
		sample_rate = AUDIO_CLK_FREQ_16;
		break;
	case 22050:
		sample_rate = AUDIO_CLK_FREQ_22;
		break;
	case 12000:
		sample_rate = AUDIO_CLK_FREQ_12;
		break;
	case 24000:
		sample_rate = AUDIO_CLK_FREQ_22;
		break;
	default:
		sample_rate = AUDIO_CLK_FREQ_441;
		break;
	};
	audio_hw_958_enable(0);
	pr_debug("aml_hw_iec958_init,runtime->rate=%d, same source mode(%d)\n",
	       runtime->rate, samesrc);

	if (old_samplerate != sample_rate || samesrc != flag_samesrc) {
		old_samplerate = sample_rate;
		flag_samesrc = samesrc;
		aml_set_spdif_clk(runtime->rate * 512, samesrc);
	}

	if (IEC958_mode_codec == 4 || IEC958_mode_codec == 5 ||
	IEC958_mode_codec == 7 || IEC958_mode_codec == 8) {
		pr_info("set 4x audio clk for 958\n");
		div = 1;
	} else if (samesrc) {
		pr_debug("share the same clock\n");
		div = 2;
	} else {
		pr_info("set normal 512 fs /4 fs\n");
		div = 4;
	}
	set_IEC958_clock_div(div);
	audio_util_set_dac_958_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
	/*clear the same source function as new raw data output */
	audio_i2s_958_same_source(samesrc);

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		i2s_mode = AIU_I2S_MODE_PCM32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s_mode = AIU_I2S_MODE_PCM24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s_mode = AIU_I2S_MODE_PCM16;
		break;
	}

	/* audio_set_i2s_mode(i2s_mode); */
	/* case 1,raw mode enabled */
	if (IEC958_mode_codec && IEC958_mode_codec != 9) {
		if (IEC958_mode_codec == 1) {
			/* dts, use raw sync-word mode */
			iec958_mode = AIU_958_MODE_RAW;
			pr_debug("iec958 mode RAW\n");
		} else {
			/* ac3,use the same pcm mode as i2s configuration */
			iec958_mode = AIU_958_MODE_PCM_RAW;
			pr_debug("iec958 mode %s\n",
				(i2s_mode == AIU_I2S_MODE_PCM32) ? "PCM32_RAW"
				: ((I2S_MODE == AIU_I2S_MODE_PCM24) ?
				"PCM24_RAW"	: "PCM16_RAW"));
		}
	} else {
		if (i2s_mode == AIU_I2S_MODE_PCM32)
			iec958_mode = AIU_958_MODE_PCM32;
		else if (i2s_mode == AIU_I2S_MODE_PCM24)
			iec958_mode = AIU_958_MODE_PCM24;
		else
			iec958_mode = AIU_958_MODE_PCM16;
		pr_debug("iec958 mode %s\n",
		       (i2s_mode ==
			AIU_I2S_MODE_PCM32) ? "PCM32" : ((i2s_mode ==
							  AIU_I2S_MODE_PCM24) ?
							 "PCM24" : "PCM16"));
	}
	if (iec958_mode == AIU_958_MODE_PCM16
	    || iec958_mode == AIU_958_MODE_PCM24
	    || iec958_mode == AIU_958_MODE_PCM32
	    || IEC958_mode_codec == 9) {
		set.chan_stat->chstat0_l = 0x0100;
		set.chan_stat->chstat0_r = 0x0100;
		set.chan_stat->chstat1_l = 0x200;
		set.chan_stat->chstat1_r = 0x200;
		if (sample_rate == AUDIO_CLK_FREQ_882) {
			pr_info("sample_rate==AUDIO_CLK_FREQ_882\n");
			set.chan_stat->chstat1_l = 0x800;
			set.chan_stat->chstat1_r = 0x800;
		} else if (sample_rate == AUDIO_CLK_FREQ_96) {
			pr_info("sample_rate==AUDIO_CLK_FREQ_96\n");
			set.chan_stat->chstat1_l = 0xa00;
			set.chan_stat->chstat1_r = 0xa00;
		} else if (sample_rate == AUDIO_CLK_FREQ_1764) {
			pr_info("sample_rate==AUDIO_CLK_FREQ_1764\n");
			set.chan_stat->chstat1_l = 0xc00;
			set.chan_stat->chstat1_r = 0xc00;
		} else if (sample_rate == AUDIO_CLK_FREQ_192) {
			pr_info("sample_rate==AUDIO_CLK_FREQ_192\n");
			set.chan_stat->chstat1_l = 0xe00;
			set.chan_stat->chstat1_r = 0xe00;
		}
		start = buf->addr;
		size = snd_pcm_lib_buffer_bytes(substream);
		audio_set_958outbuf(start, size, 0);
		/* audio_set_i2s_mode(AIU_I2S_MODE_PCM16); */
		/* audio_set_aiubuf(start, size); */
	} else {

		set.chan_stat->chstat0_l = 0x1902;
		set.chan_stat->chstat0_r = 0x1902;
		if (IEC958_mode_codec == 4 || IEC958_mode_codec == 5) {
			/* DD+ */
			if (runtime->rate == 32000) {
				set.chan_stat->chstat1_l = 0x300;
				set.chan_stat->chstat1_r = 0x300;
			} else if (runtime->rate == 44100) {
				set.chan_stat->chstat1_l = 0xc00;
				set.chan_stat->chstat1_r = 0xc00;
			} else {
				set.chan_stat->chstat1_l = 0Xe00;
				set.chan_stat->chstat1_r = 0Xe00;
			}
		} else {
			/* DTS,DD */
			if (runtime->rate == 32000) {
				set.chan_stat->chstat1_l = 0x300;
				set.chan_stat->chstat1_r = 0x300;
			} else if (runtime->rate == 44100) {
				set.chan_stat->chstat1_l = 0;
				set.chan_stat->chstat1_r = 0;
			} else {
				set.chan_stat->chstat1_l = 0x200;
				set.chan_stat->chstat1_r = 0x200;
			}
		}
		start = buf->addr;
		size = snd_pcm_lib_buffer_bytes(substream);
		audio_set_958outbuf(start, size,
				    (iec958_mode == AIU_958_MODE_RAW) ? 1 : 0);
		memset((void *)buf->area, 0, size);
	}
	audio_set_958_mode(iec958_mode, &set);

	if (IEC958_mode_codec == 2) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_AC_3, substream);
	} else if (IEC958_mode_codec == 3) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS, substream);
	} else if (IEC958_mode_codec == 4) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS,
					 substream);
	} else if (IEC958_mode_codec == 5) {
		aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS_HD, substream);
	} else if (IEC958_mode_codec == 7 || IEC958_mode_codec == 8) {
		aml_aiu_write(AIU_958_CHSTAT_L0, 0x1902);
		aml_aiu_write(AIU_958_CHSTAT_L1, 0x900);
		aml_aiu_write(AIU_958_CHSTAT_R0, 0x1902);
		aml_aiu_write(AIU_958_CHSTAT_R1, 0x900);
		if (IEC958_mode_codec == 8)
			aout_notifier_call_chain(AOUT_EVENT_RAWDATA_DTS_HD_MA,
			substream);
		else
			aout_notifier_call_chain(AOUT_EVENT_RAWDATA_MAT_MLP,
			substream);
	} else {
		aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, substream);
	}
}

/*
 * special call by the audiodsp,add these code,
 * as there are three cases for 958 s/pdif output
 * 1)NONE-PCM  raw output ,only available when ac3/dts audio,
 * when raw output mode is selected by user.
 * 2)PCM  output for  all audio, when pcm mode is selected by user .
 * 3)PCM  output for audios except ac3/dts,
 * when raw output mode is selected by user
 */

void aml_alsa_hw_reprepare(void)
{
	/* M8 disable it */
#if 0
	/* disable 958 module before call initiation */

	audio_hw_958_enable(0);
	if (playback_substream_handle != 0)
		aml_hw_iec958_init((struct snd_pcm_substream *)
				   playback_substream_handle);
#endif
}

static int aml_dai_spdif_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{

	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s;

	if (!prtd) {
		prtd =
		    (struct aml_runtime_data *)
		    kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			pr_err("alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}
	s = &prtd->s;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		s->device_type = AML_AUDIO_SPDIFOUT;
		/* audio_spdifout_pg_enable(1); */
		/*aml_spdif_play_stop(); */
	} else {
		s->device_type = AML_AUDIO_SPDIFIN;
	}

	return 0;
 out:
	return ret;
}

static void aml_dai_spdif_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	/* struct snd_dma_buffer *buf = &substream->dma_buffer; */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		memset((void *)runtime->dma_area, 0,
		       snd_pcm_lib_buffer_bytes(substream));
		if (IEC958_mode_codec == 6)
			pr_info("8chPCM output:disable aml_spdif_play\n");
	}

}

static int aml_dai_spdif_prepare(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{

	/* struct snd_soc_pcm_runtime *rtd = substream->private_data; */
	struct snd_pcm_runtime *runtime = substream->runtime;
	/* struct aml_runtime_data *prtd = runtime->private_data; */
	/* audio_stream_t *s = &prtd->s; */

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aml_hw_iec958_init(substream, 0);
	} else {
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
			audio_in_spdif_set_buf(runtime->dma_addr,
				       runtime->dma_bytes * 2, spdif_p->src);
			memset((void *)runtime->dma_area,
				0, runtime->dma_bytes * 2);
		} else {
			audio_in_spdif_set_buf(runtime->dma_addr,
				       runtime->dma_bytes, spdif_p->src);
			memset((void *)runtime->dma_area,
				0, runtime->dma_bytes);
		}
	}

	return 0;
}

/* if src_i2s, then spdif parent clk is mclk, otherwise i958clk */
int aml_set_spdif_clk(unsigned long rate, bool src_i2s)
{
	int ret = 0;

	if (src_i2s) {
		ret = clk_set_parent(spdif_p->clk_spdif, spdif_p->clk_mclk);
		if (ret) {
			pr_err("Can't set spdif clk parent: %d\n", ret);
			return ret;
		}
	} else {
		ret = clk_set_rate(spdif_p->clk_mpl1, rate * 4);
		if (ret) {
			pr_err("Can't set spdif mpl1 clock rate: %d\n", ret);
			return ret;
		}
		ret = clk_set_parent(spdif_p->clk_i958, spdif_p->clk_mpl1);
		if (ret) {
			pr_err("Can't set spdif clk parent: %d\n", ret);
			return ret;
		}
		ret = clk_set_rate(spdif_p->clk_i958, rate);
		if (ret) {
			pr_err("Can't set spdif mpl1 clock rate: %d\n", ret);
			return ret;
		}

		ret = clk_set_parent(spdif_p->clk_spdif, spdif_p->clk_i958);
		if (ret) {
			pr_err("Can't set spdif clk parent: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int aml_dai_spdif_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
#if 0
	int srate;

	srate = params_rate(params);
	aml_set_spdif_clk(srate * 512, 0);
#endif
	return 0;
}

static int aml_dai_spdif_suspend(struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *spdif_priv = dev_get_drvdata(cpu_dai->dev);

	aml_spdif_play_stop();
	if (spdif_priv && spdif_priv->clk_spdif)
		clk_disable_unprepare(spdif_priv->clk_spdif);

	return 0;
}

static int aml_dai_spdif_resume(struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *spdif_priv = dev_get_drvdata(cpu_dai->dev);

	/*aml_spdif_play();*/
	if (spdif_priv && spdif_priv->clk_spdif)
		clk_prepare_enable(spdif_priv->clk_spdif);

	return 0;
}

static struct snd_soc_dai_ops spdif_dai_ops = {
	.set_sysclk = aml_dai_spdif_set_sysclk,
	.trigger = aml_dai_spdif_trigger,
	.prepare = aml_dai_spdif_prepare,
	.hw_params = aml_dai_spdif_hw_params,
	.shutdown = aml_dai_spdif_shutdown,
	.startup = aml_dai_spdif_startup,
};

static struct snd_soc_dai_driver aml_spdif_dai[] = {
	{
		.playback = {
			.stream_name = "S/PDIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = (SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_176400 |
			SNDRV_PCM_RATE_192000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE),
		},
		.capture = {
			.stream_name = "S/PDIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = (SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_96000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &spdif_dai_ops,
		.suspend = aml_dai_spdif_suspend,
		.resume = aml_dai_spdif_resume,
	}
};

static const struct snd_soc_component_driver aml_component = {
	.name = "aml-spdif-dai",
};

static const char *const gate_names[] = {
	"iec958", "iec958_amclk"
};

static int aml_dai_spdif_probe(struct platform_device *pdev)
{
	int i, ret;
	struct aml_spdif *spdif_priv = NULL;
	struct clk *clk_gate;

	/* enable spdif power gate first */
	for (i = 0; i < ARRAY_SIZE(gate_names); i++) {
		clk_gate = devm_clk_get(&pdev->dev, gate_names[i]);
		if (IS_ERR(clk_gate)) {
			dev_err(&pdev->dev, "Can't get aml audio gate\n");
			return PTR_ERR(clk_gate);
		}
		clk_prepare_enable(clk_gate);
	}

	spdif_priv = devm_kzalloc(&pdev->dev,
			sizeof(struct aml_spdif), GFP_KERNEL);
	if (!spdif_priv) {
		/*dev_err(&pdev->dev, "Can't allocate spdif_priv\n");*/
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, spdif_priv);
	spdif_p = spdif_priv;

	spdif_priv->clk_mpl1 = devm_clk_get(&pdev->dev, "mpll1");
	if (IS_ERR(spdif_priv->clk_mpl1)) {
		dev_err(&pdev->dev, "Can't retrieve mpll1 clock\n");
		ret = PTR_ERR(spdif_priv->clk_mpl1);
		goto err;
	}

	spdif_priv->clk_i958 = devm_clk_get(&pdev->dev, "i958");
	if (IS_ERR(spdif_priv->clk_i958)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clk_i958 clock\n");
		ret = PTR_ERR(spdif_priv->clk_i958);
		goto err;
	}

	spdif_priv->clk_mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(spdif_priv->clk_mclk)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clk_mclk clock\n");
		ret = PTR_ERR(spdif_priv->clk_mclk);
		goto err;
	}

	spdif_priv->clk_spdif = devm_clk_get(&pdev->dev, "spdif");
	if (IS_ERR(spdif_priv->clk_spdif)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clock\n");
		ret = PTR_ERR(spdif_priv->clk_spdif);
		goto err;
	}
	ret = clk_set_parent(spdif_priv->clk_i958, spdif_priv->clk_mpl1);
	if (ret) {
		pr_err("Can't set i958 clk parent: %d\n", ret);
		return ret;
	}

	ret = clk_set_parent(spdif_priv->clk_spdif, spdif_priv->clk_i958);
	if (ret) {
		pr_err("Can't set spdif clk parent: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(spdif_priv->clk_spdif);
	if (ret) {
		pr_err("Can't enable spdif clock: %d\n", ret);
		goto err;
	}

	spdif_priv->clk_81 = devm_clk_get(&pdev->dev, "clk_81");
	if (IS_ERR(spdif_priv->clk_81)) {
		dev_err(&pdev->dev, "Can't get clk81\n");
		ret = PTR_ERR(spdif_priv->clk_81);
		goto err;
	}
	clk81 = clk_get_rate(spdif_priv->clk_81);

	/* In PAO mode, audio data from hdmi-rx has no channel order,
	 * set default mode from spdif-in
	 */
	spdif_priv->src = SPDIF_IN;
	aml_spdif_play(0);
	ret = snd_soc_register_component(&pdev->dev, &aml_component,
					  aml_spdif_dai,
					  ARRAY_SIZE(aml_spdif_dai));
	if (ret) {
		pr_err("Can't register spdif dai: %d\n", ret);
		goto err_clk_dis;
	}
	return 0;

err_clk_dis:
	clk_disable_unprepare(spdif_priv->clk_spdif);
err:
	return ret;
}

static int aml_dai_spdif_remove(struct platform_device *pdev)
{
	struct aml_spdif *spdif_priv = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	clk_disable_unprepare(spdif_priv->clk_spdif);

	return 0;
}

static void aml_spdif_dai_shutdown(struct platform_device *pdev)
{
	struct aml_spdif *spdif_priv = dev_get_drvdata(&pdev->dev);

	if (spdif_priv && spdif_priv->clk_spdif)
		clk_disable_unprepare(spdif_priv->clk_spdif);
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_spdif_dai_dt_match[] = {
	{.compatible = "amlogic, aml-spdif-dai",
	 },
	{},
};
#else
#define amlogic_spdif_dai_dt_match NULL
#endif

static struct platform_driver aml_spdif_dai_driver = {
	.probe    = aml_dai_spdif_probe,
	.remove   = aml_dai_spdif_remove,
	.shutdown = aml_spdif_dai_shutdown,
	.driver   = {
		.name           = "aml-spdif-dai",
		.owner          = THIS_MODULE,
		.of_match_table = amlogic_spdif_dai_dt_match,
	},
};

static int __init aml_dai_spdif_init(void)
{
	return platform_driver_register(&aml_spdif_dai_driver);
}

module_init(aml_dai_spdif_init);

static void __exit aml_dai_spdif_exit(void)
{
	platform_driver_unregister(&aml_spdif_dai_driver);
}

module_exit(aml_dai_spdif_exit);

MODULE_AUTHOR("jian.xu, <jian.xu@amlogic.com>");
MODULE_DESCRIPTION("Amlogic S/PDIF<HDMI> Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:aml-spdif");
