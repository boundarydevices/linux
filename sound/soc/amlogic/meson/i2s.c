/*
 * sound/soc/amlogic/meson/i2s.c
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
#define pr_fmt(fmt) "snd_i2s: " fmt

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
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <linux/major.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

/* Amlogic headers */
#include <linux/amlogic/iomap.h>
#include "i2s.h"
#include "spdif_dai.h"
#include "audio_hw.h"
#include <linux/amlogic/media/sound/aiu_regs.h>
#include <linux/amlogic/media/sound/audin_regs.h>
#include <linux/amlogic/media/sound/audio_iomap.h>

/*
 * timer for i2s data update, hw timer, hrtimer, timer
 * once select only one way to update
 */
/*#define USE_HW_TIMER*/
/*#define USE_HRTIMER*/
#ifdef USE_HW_TIMER
#define XRUN_NUM 100 /*1ms*100=100ms timeout*/
#else
#define XRUN_NUM 10 /*10ms*10=100ms timeout*/
#endif

unsigned long aml_i2s_playback_start_addr;
EXPORT_SYMBOL(aml_i2s_playback_start_addr);

unsigned long aml_i2s_playback_phy_start_addr;
EXPORT_SYMBOL(aml_i2s_playback_phy_start_addr);

unsigned long aml_i2s_alsa_write_addr;
EXPORT_SYMBOL(aml_i2s_alsa_write_addr);

unsigned int aml_i2s_playback_channel = 2;
EXPORT_SYMBOL(aml_i2s_playback_channel);

unsigned int aml_i2s_playback_format = 16;
EXPORT_SYMBOL(aml_i2s_playback_format);

unsigned int aml_i2s_playback_running_flag;
EXPORT_SYMBOL(aml_i2s_playback_running_flag);

static int trigger_underrun;
void aml_audio_hw_trigger(void)
{
	trigger_underrun = 1;
}
EXPORT_SYMBOL(aml_audio_hw_trigger);

#ifndef USE_HRTIMER
static void aml_i2s_timer_callback(unsigned long data);
#endif

/*--------------------------------------------------------------------------*
 * Hardware definition
 *--------------------------------------------------------------------------*
 * TODO: These values were taken from the AML platform driver, check
 *	 them against real values for AML
 */
static const struct snd_pcm_hardware aml_i2s_hardware = {
	.info =
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_MMAP_VALID |
#endif
	SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 32 * 1024 * 2,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 128 * 1024 * 2 * 2,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	.fifo_size = 4,
#else
	.fifo_size = 0,
#endif
};

static const struct snd_pcm_hardware aml_i2s_capture = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER |
	    SNDRV_PCM_INFO_MMAP |
	    SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_PAUSE,

	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 64,
	.period_bytes_max = 32 * 1024 * 2,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 64 * 1024 * 4,

	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
	.fifo_size = 0,
};

static unsigned int period_sizes[] = {
	64, 128, 256, 512, 1024, 2048, 4096, 8192,
	16384, 32768, 65536, 65536 * 2, 65536 * 4
};

static struct snd_pcm_hw_constraint_list hw_constraints_period_sizes = {
	.count = ARRAY_SIZE(period_sizes),
	.list = period_sizes,
	.mask = 0
};

/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------
 * Helper functions
 *--------------------------------------------------------------------------
 */
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
static int aml_i2s_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* malloc DMA buffer */
		size = aml_i2s_hardware.buffer_bytes_max;

		buf->area = dmam_alloc_coherent(pcm->card->dev, size,
				&buf->addr, GFP_KERNEL);
		dev_info(pcm->card->dev, "aml-i2s %d: playback preallocate_dma_buffer: area=%p, addr=%p, size=%ld\n",
			stream, (void *) buf->area, (void *) buf->addr, size);
		if (!buf->area)
			return -ENOMEM;

	} else {
		/* malloc DMA buffer */
		size = aml_i2s_capture.buffer_bytes_max;
		buf->area = dmam_alloc_coherent(pcm->card->dev, size,
				&buf->addr, GFP_KERNEL);
		dev_info(pcm->card->dev, "aml-i2s %d: capture preallocate_dma_buffer: area=%p, addr=%p, size=%ld\n",
			stream, (void *) buf->area, (void *) buf->addr, size);
		if (!buf->area)
			return -ENOMEM;
	}


	buf->bytes = size;
	return 0;
}
#else
static int aml_i2s_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{

	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = NULL;
	size_t size = 0;

	tmp_buf = kzalloc(sizeof(struct aml_audio_buffer), GFP_KERNEL);
	if (tmp_buf == NULL) {
		/*dev_err(&pcm->card->dev, "allocate tmp buffer error\n");*/
		return -ENOMEM;
	}
	buf->private_data = tmp_buf;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* malloc DMA buffer */
		size = aml_i2s_hardware.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		/* one size for i2s output, another for 958,
		 *and 128 for alignment
		 */
		buf->area = dma_alloc_coherent(pcm->card->dev, size + 4096,
					       &buf->addr, GFP_KERNEL);
		if (!buf->area) {
			dev_err(pcm->card->dev, "alloc playback DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_hardware.buffer_bytes_max;
		tmp_buf->buffer_start = kzalloc(size, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			dev_err(pcm->card->dev, "alloc playback tmp buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		tmp_buf->buffer_size = size;

	} else {
		/* malloc DMA buffer */
		size = aml_i2s_capture.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		buf->area = dma_alloc_coherent(pcm->card->dev, size * 2,
					       &buf->addr, GFP_KERNEL);

		if (!buf->area) {
			dev_err(pcm->card->dev, "alloc capture DMA buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		buf->bytes = size;
		/* malloc tmp buffer */
		size = aml_i2s_capture.period_bytes_max;
		tmp_buf->buffer_start = kzalloc(size + 64, GFP_KERNEL);
		if (tmp_buf->buffer_start == NULL) {
			dev_err(pcm->card->dev, "alloc capture tmp buffer error\n");
			kfree(tmp_buf);
			buf->private_data = NULL;
			return -ENOMEM;
		}
		tmp_buf->buffer_size = size;
	}

	return 0;

}
#endif

/*--------------------------------------------------------------------------
 * ISR
 *--------------------------------------------------------------------------
 *--------------------------------------------------------------------------
 * i2s operations
 *--------------------------------------------------------------------------
 */
static int aml_i2s_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;

	/*
	 * this may get called several times by oss emulation
	 * with different params
	 */
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);
	s->I2S_addr = runtime->dma_addr;

	/*
	 * Both capture and playback need to reset the last ptr
	 * to the start address, playback and capture use
	 * different address calculate, so we reset the different
	 * start address to the last ptr
	 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* s->last_ptr must initialized as dma buffer's start addr */
		s->last_ptr = runtime->dma_addr;
	} else {

		s->last_ptr = 0;
	}

	return 0;
}

static int aml_i2s_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aml_i2s_prepare(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buf->private_data;
#endif

	if (s && s->device_type == AML_AUDIO_I2SOUT && trigger_underrun) {
		dev_info(substream->pcm->card->dev, "clear i2s out trigger underrun\n");
		trigger_underrun = 0;
	}
	if (s && s->device_type == AML_AUDIO_I2SOUT) {
		aml_i2s_playback_channel = runtime->channels;
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE)
			aml_i2s_playback_format = 16;
		else if (runtime->format == SNDRV_PCM_FORMAT_S32_LE)
			aml_i2s_playback_format = 32;
		else if (runtime->format == SNDRV_PCM_FORMAT_S24_LE)
			aml_i2s_playback_format = 24;
	}
#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE
	tmp_buf->cached_len = 0;
#endif
	tmp_buf->find_start = 0;
	tmp_buf->invert_flag = 0;
	tmp_buf->cached_sample = 0;
#endif

	return 0;
}

#ifdef USE_HW_TIMER
int hw_timer_init;
static irqreturn_t audio_isr_handler(int irq, void *data)
{
	struct aml_runtime_data *prtd = data;
	struct snd_pcm_substream *substream = prtd->substream;

	aml_i2s_timer_callback((unsigned long)substream);

	return IRQ_HANDLED;
}

static int snd_free_hw_timer_irq(void *data)
{
	free_irq(INT_TIMER_D, data);

	return 0;
}

static int snd_request_hw_timer(void *data)
{
	int ret = 0;

	if (hw_timer_init == 0) {
		aml_isa_write(ISA_TIMERD, TIMER_COUNT);
		aml_isa_update_bits(ISA_TIMER_MUX, 3 << 6,
					TIMERD_RESOLUTION << 6);
		aml_isa_update_bits(ISA_TIMER_MUX, 1 << 15, TIMERD_MODE << 15);
		aml_isa_update_bits(ISA_TIMER_MUX, 1 << 19, 1 << 19);
		hw_timer_init = 1;
	}
	ret = request_irq(INT_TIMER_D, audio_isr_handler,
				IRQF_SHARED, "timerd_irq", data);
		if (ret < 0) {
			pr_err("audio hw interrupt register fail\n");
			return -1;
		}
	return 0;
}
#endif

#ifdef USE_HRTIMER
static void aml_i2s_hrtimer_set_rate(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;

	prtd->wakeups_per_second = ktime_set(0, 1000000000 / (runtime->rate));
}

static enum hrtimer_restart aml_i2s_hrtimer_callback(struct hrtimer *timer)
{
	struct aml_runtime_data *prtd =  container_of(timer,
					struct aml_runtime_data, hrtimer);
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *s = &prtd->s;
	unsigned int last_ptr, size;
	unsigned long flags = 0;
	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (prtd->active == 0) {
		hrtimer_forward_now(timer, prtd->wakeups_per_second);
		spin_unlock_irqrestore(&prtd->timer_lock, flags);
		return HRTIMER_RESTART;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		last_ptr = read_i2s_rd_ptr();
		if (last_ptr < s->last_ptr)
			size = runtime->dma_bytes + last_ptr - s->last_ptr;
		else
			size = last_ptr - s->last_ptr;
		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			snd_pcm_period_elapsed(substream);
		}
	} else {
		last_ptr = (audio_in_i2s_wr_ptr() - s->I2S_addr) / 2;
		if (last_ptr < s->last_ptr)
			size = runtime->dma_bytes + last_ptr - s->last_ptr;
		else
			size = last_ptr - s->last_ptr;

		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			snd_pcm_period_elapsed(substream);
		}
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);
	hrtimer_forward_now(timer, prtd->wakeups_per_second);

	return HRTIMER_RESTART;
}

#endif

static void start_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (!prtd->active) {
#ifndef USE_HW_TIMER
#ifdef USE_HRTIMER
		hrtimer_start(&prtd->hrtimer, prtd->wakeups_per_second,
				  HRTIMER_MODE_REL);
#else
		mod_timer(&prtd->timer, jiffies + 1);
#endif
#endif
		prtd->active = 1;
		prtd->xrun_num = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);

}

static void stop_timer(struct aml_runtime_data *prtd)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (prtd->active) {
#ifndef USE_HW_TIMER
#ifdef USE_HRTIMER
		hrtimer_cancel(&prtd->hrtimer);
#else
		del_timer(&prtd->timer);
#endif
#endif
		prtd->active = 0;
		prtd->xrun_num = 0;
	}
	spin_unlock_irqrestore(&prtd->timer_lock, flags);
}


static int aml_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct aml_runtime_data *prtd = rtd->private_data;
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#ifdef USE_HRTIMER
		aml_i2s_hrtimer_set_rate(substream);
#endif
		start_timer(prtd);
		break;		/* SNDRV_PCM_TRIGGER_START */
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		stop_timer(prtd);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t aml_i2s_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct audio_stream *s = &prtd->s;
	unsigned int addr, ptr;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (s->device_type == AML_AUDIO_I2SOUT)
			ptr = read_i2s_rd_ptr();
		else
			ptr = read_iec958_rd_ptr();
		addr = ptr - s->I2S_addr;
		return bytes_to_frames(runtime, addr);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (s->device_type == AML_AUDIO_I2SIN)
			ptr = audio_in_i2s_wr_ptr();
		else if (s->device_type == AML_AUDIO_I2SIN2)
			ptr = audio_in_i2s2_wr_ptr();
		else
			ptr = audio_in_spdif_wr_ptr();
		addr = ptr - s->I2S_addr;
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
		return bytes_to_frames(runtime, addr);
#else
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE)
			return bytes_to_frames(runtime, addr) >> 1;
		else
			return bytes_to_frames(runtime, addr);
#endif
	}

	return 0;
}

#ifndef USE_HRTIMER
static void aml_i2s_timer_callback(unsigned long data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = NULL;
	struct audio_stream *s = NULL;
	int elapsed = 0;
	unsigned int last_ptr, size = 0;
	unsigned long flags = 0;

	if (runtime == NULL || runtime->private_data == NULL)
		return;

	prtd = runtime->private_data;
	s = &prtd->s;

	if (prtd->active == 0)
		return;

	spin_lock_irqsave(&prtd->timer_lock, flags);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (s->device_type == AML_AUDIO_I2SOUT)
			last_ptr = read_i2s_rd_ptr();
		else
			last_ptr = read_iec958_rd_ptr();
		if (last_ptr < s->last_ptr) {
			size =
				runtime->dma_bytes + last_ptr -
				(s->last_ptr);
		} else {
			size = last_ptr - (s->last_ptr);
		}
		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			elapsed = 1;
		}
	} else {
		if (s->device_type == AML_AUDIO_I2SIN)
			last_ptr = audio_in_i2s_wr_ptr();
		else if (s->device_type == AML_AUDIO_I2SIN2)
			last_ptr = audio_in_i2s2_wr_ptr();
		else
			last_ptr = audio_in_spdif_wr_ptr();

		if (last_ptr < s->last_ptr) {
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
			size = runtime->dma_bytes +
					(last_ptr - (s->last_ptr));
#else
			if (runtime->format == SNDRV_PCM_FORMAT_S16_LE)
				size = runtime->dma_bytes +
					(last_ptr - (s->last_ptr)) / 2;
			else
				size = runtime->dma_bytes +
					(last_ptr - (s->last_ptr));
#endif
			prtd->xrun_num = 0;
		} else if (last_ptr == s->last_ptr) {
			if (prtd->xrun_num++ > XRUN_NUM) {
				prtd->xrun_num = 0;
				s->size = runtime->period_size;
			}
		} else {
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
			size = (last_ptr - (s->last_ptr));
#else
			if (runtime->format == SNDRV_PCM_FORMAT_S16_LE)
				size = (last_ptr - (s->last_ptr)) / 2;
			else
				size = last_ptr - (s->last_ptr);
#endif
			prtd->xrun_num = 0;
		}

		s->last_ptr = last_ptr;
		s->size += bytes_to_frames(substream->runtime, size);
		if (s->size >= runtime->period_size) {
			s->size %= runtime->period_size;
			elapsed = 1;
		}
	}

#ifndef USE_HW_TIMER
	mod_timer(&prtd->timer, jiffies + 1);
#endif

	spin_unlock_irqrestore(&prtd->timer_lock, flags);
	if (elapsed)
		snd_pcm_period_elapsed(substream);
}

#endif

static int aml_i2s_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct audio_stream *s = &prtd->s;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_hardware);
		if (s->device_type == AML_AUDIO_I2SOUT) {
			aml_i2s_playback_start_addr = (unsigned long)buf->area;
			aml_i2s_playback_phy_start_addr = buf->addr;
		}
	} else {
		snd_soc_set_runtime_hwparams(substream, &aml_i2s_capture);
	}

	/* ensure that period size is a multiple of 32bytes */
	ret =
	    snd_pcm_hw_constraint_list(runtime, 0,
				       SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				       &hw_constraints_period_sizes);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev,
			"set period bytes constraint error\n");
		goto out;
	}

	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev, "set period error\n");
		goto out;
	}
	if (!prtd) {
		prtd = kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
		if (prtd == NULL) {
			dev_err(substream->pcm->card->dev, "alloc aml_runtime_data error\n");
			ret = -ENOMEM;
			goto out;
		}
		prtd->substream = substream;
		runtime->private_data = prtd;
	}

	spin_lock_init(&prtd->timer_lock);

#if defined(USE_HW_TIMER)
	ret = snd_request_hw_timer(prtd);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev, "request audio hw timer failed\n");
		goto out;
	}
#elif defined(USE_HRTIMER)
	hrtimer_init(&prtd->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	prtd->hrtimer.function = aml_i2s_hrtimer_callback;
	pr_info("hrtimer inited..\n");
#else
	init_timer(&prtd->timer);
	prtd->timer.function = &aml_i2s_timer_callback;
	prtd->timer.data = (unsigned long)substream;
#endif

 out:
	return ret;
}

static int aml_i2s_close(struct snd_pcm_substream *substream)
{
	struct aml_runtime_data *prtd = substream->runtime->private_data;
	struct audio_stream *s = &prtd->s;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (s->device_type == AML_AUDIO_I2SOUT)
			aml_i2s_playback_running_flag = 0;
	}
#ifdef USE_HW_TIMER
	snd_free_hw_timer_irq(prtd);
#endif
	kfree(prtd);
	prtd = NULL;

	return 0;
}

#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
static int aml_i2s_copy_playback(struct snd_pcm_runtime *runtime, int channel,
				 snd_pcm_uframes_t pos,
				 void __user *buf, snd_pcm_uframes_t count,
				 struct snd_pcm_substream *substream)
{
	int res = 0;
	int n;
	unsigned long offset = frames_to_bytes(runtime, pos);
	char *hwbuf = runtime->dma_area + offset;
	struct aml_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	struct audio_stream *s = &prtd->s;
	struct device *dev = substream->pcm->card->dev;
#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE
	void *ubuf = tmp_buf->buffer_start;
	int i = 0, j = 0;
	int align = runtime->channels * 32;
	int cached_len = tmp_buf->cached_len;
	char *cache_buffer_bytes = tmp_buf->cache_buffer_bytes;
#endif
	n = frames_to_bytes(runtime, count);
	if (n > tmp_buf->buffer_size) {
		dev_err(dev, "FATAL_ERR:UserData/%d > buffer_size/%d\n",
				n, tmp_buf->buffer_size);
		return -EFAULT;
	}

#ifndef CONFIG_AMLOGIC_SND_SPLIT_MODE
	res = copy_from_user(ubuf, buf, n);
	if (res)
		return -EFAULT;

	/*mask align byte(64 or 256)*/
	if ((cached_len != 0 || (n % align) != 0)) {
		int byte_size = n;
		int total_len;
		int output_len;
		int next_cached_len;
		char cache_buffer_bytes_tmp[256];

		offset -= cached_len;
		hwbuf = runtime->dma_area + offset;

		total_len = byte_size + cached_len;
		output_len = total_len & (~(align - 1));
		next_cached_len = total_len - output_len;

		if (next_cached_len)
			memcpy((void *)cache_buffer_bytes_tmp,
				(void *)((char *)ubuf +
				byte_size - next_cached_len),
				next_cached_len);
		memmove((void *)((char *)ubuf + cached_len),
				(void *)ubuf, output_len - cached_len);
		if (cached_len)
			memcpy((void *)ubuf,
				(void *)cache_buffer_bytes, cached_len);
		if (next_cached_len)
			memcpy((void *)cache_buffer_bytes,
				(void *)cache_buffer_bytes_tmp,
				next_cached_len);

		tmp_buf->cached_len = next_cached_len;
		n = output_len;
	}
	/*end of mask*/
#endif

	if (s->device_type == AML_AUDIO_I2SOUT) {
		aml_i2s_alsa_write_addr = offset;
#ifdef CONFIG_AMLOGIC_AMAUDIO2
		if (amaudio2_read_enable == 1 && get_pcm_cache_space() > n
				&& amaudio2_enable == 0)
			cache_pcm_write(buf, n);
#endif
		aml_i2s_playback_running_flag = 1;
	}

	if (access_ok(VERIFY_READ, buf, frames_to_bytes(runtime, count))) {
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE
		res = copy_from_user(hwbuf, buf, n);
		if (res)
			return -EFAULT;
#else
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
			int16_t *tfrom, *to;

			tfrom = (int16_t *) ubuf;
			to = (int16_t *) hwbuf;

			if (runtime->channels == 8) {
				int16_t *lf, *cf, *rf, *ls,
						*rs, *lef, *sbl, *sbr;

				lf = to;
				cf = to + 16 * 1;
				rf = to + 16 * 2;
				ls = to + 16 * 3;
				rs = to + 16 * 4;
				lef = to + 16 * 5;
				sbl = to + 16 * 6;
				sbr = to + 16 * 7;
				for (j = 0; j < n; j += 256) {
					for (i = 0; i < 16; i++) {
						*lf++ = (*tfrom++);
						*cf++ = (*tfrom++);
						*rf++ = (*tfrom++);
						*ls++ = (*tfrom++);
						*rs++ = (*tfrom++);
						*lef++ = (*tfrom++);
						*sbl++ = (*tfrom++);
						*sbr++ = (*tfrom++);
					}
					lf += 7 * 16;
					cf += 7 * 16;
					rf += 7 * 16;
					ls += 7 * 16;
					rs += 7 * 16;
					lef += 7 * 16;
					sbl += 7 * 16;
					sbr += 7 * 16;
				}
			} else if (runtime->channels == 2) {
				int16_t *left, *right;

				left = to;
				right = to + 16;

				for (j = 0; j < n; j += 64) {
					for (i = 0; i < 16; i++) {
						*left++ = (*tfrom++);
						*right++ = (*tfrom++);
					}
					left += 16;
					right += 16;
				}
			}
		} else if (runtime->format == SNDRV_PCM_FORMAT_S24_LE
			   && I2S_MODE == AIU_I2S_MODE_PCM24) {
			int32_t *tfrom, *to;

			tfrom = (int32_t *) ubuf;
			to = (int32_t *) hwbuf;

			if (runtime->channels == 8) {
				int32_t *lf, *cf, *rf, *ls,
					*rs, *lef, *sbl, *sbr;

				lf = to;
				cf = to + 8 * 1;
				rf = to + 8 * 2;
				ls = to + 8 * 3;
				rs = to + 8 * 4;
				lef = to + 8 * 5;
				sbl = to + 8 * 6;
				sbr = to + 8 * 7;
				for (j = 0; j < n; j += 256) {
					for (i = 0; i < 8; i++) {
						*lf++ = (*tfrom++);
						*cf++ = (*tfrom++);
						*rf++ = (*tfrom++);
						*ls++ = (*tfrom++);
						*rs++ = (*tfrom++);
						*lef++ = (*tfrom++);
						*sbl++ = (*tfrom++);
						*sbr++ = (*tfrom++);
					}
					lf += 7 * 8;
					cf += 7 * 8;
					rf += 7 * 8;
					ls += 7 * 8;
					rs += 7 * 8;
					lef += 7 * 8;
					sbl += 7 * 8;
					sbr += 7 * 8;
				}
			} else if (runtime->channels == 2) {
				int32_t *left, *right;

				left = to;
				right = to + 8;

				for (j = 0; j < n; j += 64) {
					for (i = 0; i < 8; i++) {
						*left++ = (*tfrom++);
						*right++ = (*tfrom++);
					}
					left += 8;
					right += 8;
				}
			}

		} else if (runtime->format == SNDRV_PCM_FORMAT_S32_LE) {
			int32_t *tfrom, *to;

			tfrom = (int32_t *) ubuf;
			to = (int32_t *) hwbuf;

			if (runtime->channels == 8) {
				int32_t *lf, *cf, *rf, *ls,
					*rs, *lef, *sbl, *sbr;

				lf = to;
				cf = to + 8 * 1;
				rf = to + 8 * 2;
				ls = to + 8 * 3;
				rs = to + 8 * 4;
				lef = to + 8 * 5;
				sbl = to + 8 * 6;
				sbr = to + 8 * 7;
				for (j = 0; j < n; j += 256) {
					for (i = 0; i < 8; i++) {
						*lf++ = (*tfrom++) >> 8;
						*cf++ = (*tfrom++) >> 8;
						*rf++ = (*tfrom++) >> 8;
						*ls++ = (*tfrom++) >> 8;
						*rs++ = (*tfrom++) >> 8;
						*lef++ = (*tfrom++) >> 8;
						*sbl++ = (*tfrom++) >> 8;
						*sbr++ = (*tfrom++) >> 8;
					}
					lf += 7 * 8;
					cf += 7 * 8;
					rf += 7 * 8;
					ls += 7 * 8;
					rs += 7 * 8;
					lef += 7 * 8;
					sbl += 7 * 8;
					sbr += 7 * 8;
				}
			} else if (runtime->channels == 2) {
				int32_t *left, *right;

				left = to;
				right = to + 8;

				for (j = 0; j < n; j += 64) {
					for (i = 0; i < 8; i++) {
						*left++ = (*tfrom++) >> 8;
						*right++ = (*tfrom++) >> 8;
					}
					left += 8;
					right += 8;
				}
			}
		}
#endif
	} else {
		res = -EFAULT;
	}
	return res;
}

static int find_start_position(void *ubuf, int bytes, int *find_start)
{
	uint32_t *ptr = (uint32_t *)ubuf;
	int i = 0, revert = 0, pos = 0;
	uint32_t sample2 = *ptr++;
	uint32_t sample1 = 0;

	for (i = 0; i < (bytes - 4); i += 4) {
		sample1 = sample2;
		sample2 = *ptr++;
		if (((sample1 >> 28) & 0xf) == 0xf &&
				((sample2 >> 28) & 0xf) == 0x3) {
			if (pos % 2 == 1)
				revert = 1;
			*find_start = 1;
			pr_info("first start position = %d, revert = %d\n",
					pos, revert);
			break;
		}
		pos++;
	}
	return revert;
}

static int aml_i2s_copy_capture(struct snd_pcm_runtime *runtime, int channel,
				snd_pcm_uframes_t pos,
				void __user *buf, snd_pcm_uframes_t count,
				struct snd_pcm_substream *substream)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_dma_buffer *buffer = &substream->dma_buffer;
	struct aml_audio_buffer *tmp_buf = buffer->private_data;
	void *ubuf = tmp_buf->buffer_start;
	unsigned long offset = frames_to_bytes(runtime, pos);
	int res = 0, n = 0, i = 0, j = 0;
	unsigned char r_shift = 8;

	n = frames_to_bytes(runtime, count);

	/*amlogic HW only supports 32bit mode by capture*/
	if (access_ok(VERIFY_WRITE, buf, frames_to_bytes(runtime, count))) {
		if (runtime->channels == 2) {
			if (pos % 8)
				dev_err(dev, "audio data unligned\n");

			if (is_audin_lr_invert_check()
					&& audio_in_source == 0
					&& tmp_buf->find_start == 0) {
				char *hwbuf = runtime->dma_area + offset * 2;
				int32_t *tfrom = (int32_t *)hwbuf;
				int32_t *to = (int32_t *)ubuf;
				int32_t *left = tfrom;
				int32_t *right = tfrom + 8;

				for (j = 0; j < n * 2; j += 64) {
					for (i = 0; i < 8; i++) {
						*to++ = (int32_t)(*left++);
						*to++ = (int32_t)(*right++);
					}
					left += 8;
					right += 8;
				}
				tmp_buf->invert_flag =
						find_start_position(ubuf, n,
						&(tmp_buf->find_start));
			}

			if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
				char *hwbuf = runtime->dma_area + offset * 2;
				int32_t *tfrom = (int32_t *)hwbuf;
				int16_t *to = (int16_t *)ubuf;
				int32_t *left = tfrom;
				int32_t *right = tfrom + 8;

				if ((n * 2) % 64)
					dev_err(dev, "audio data unaligned 64 bytes\n");

				if (tmp_buf->invert_flag == 1)
					*to++ = (int16_t)
						(tmp_buf->cached_sample);

				for (j = 0; j < n * 2; j += 64) {
					for (i = 0; i < 8; i++) {
						*to++ = (int16_t)
							((*left++) >> r_shift);
						*to++ = (int16_t)
							((*right++) >> r_shift);
					}
					left += 8;
					right += 8;
				}

				if (tmp_buf->invert_flag == 1)
					tmp_buf->cached_sample = *(to - 1);

				/* clean hw buffer */
				memset(hwbuf, 0, n * 2);
			} else {
				char *hwbuf = runtime->dma_area + offset;
				int32_t *tfrom = (int32_t *)hwbuf;
				int32_t *to = (int32_t *)ubuf;
				int32_t *left = tfrom;
				int32_t *right = tfrom + 8;

				if (n % 64)
					dev_err(dev, "audio data unaligned 64 bytes\n");

				if (tmp_buf->invert_flag == 1)
					*to++ = (int32_t)
						(tmp_buf->cached_sample);

				if (runtime->format == SNDRV_PCM_FORMAT_S24_LE)
					r_shift = 0;

				for (j = 0; j < n; j += 64) {
					for (i = 0; i < 8; i++) {
						*to++ = (int32_t)
							((*left++) << r_shift);
						*to++ = (int32_t)
							((*right++) << r_shift);
					}
					left += 8;
					right += 8;
				}

				if (tmp_buf->invert_flag == 1)
					tmp_buf->cached_sample = *(to - 1);

				memset(hwbuf, 0, n);
			}
		} else if (runtime->channels == 8) {
			/* for fifo0 1 ch mode, i2s_ctrl b[13:10]=0xf */
			if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
				char *hwbuf = runtime->dma_area + offset*2;
				int32_t *tfrom = (int32_t *)hwbuf;
				int16_t *to = (int16_t *)ubuf;

				for (j = 0; j < n*2; j += 4) {
					*to++ = (int16_t)(((*tfrom++) >> 8)
							& 0xffff);
				}
				memset(hwbuf, 0, n*2);
			} else {
				/* S24_LE or S32_LE */
				char *hwbuf = runtime->dma_area + offset;
				int32_t *tfrom = (int32_t *)hwbuf;
				int32_t *to = (int32_t *)ubuf;

				if (runtime->format == SNDRV_PCM_FORMAT_S24_LE)
					r_shift = 0;
				for (j = 0; j < n; j += 4) {
					*to++ = (int32_t)((*tfrom++)
						       << r_shift);
				}
				memset(hwbuf, 0, n);
			}
		}
	}
	res = copy_to_user(buf, ubuf, n);
	return res;
}

static int aml_i2s_copy(struct snd_pcm_substream *substream, int channel,
			snd_pcm_uframes_t pos,
			void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret =
		    aml_i2s_copy_playback(runtime, channel, pos, buf, count,
					  substream);
	} else {
		ret =
		    aml_i2s_copy_capture(runtime, channel, pos, buf, count,
					 substream);
	}
	return ret;
}
#endif

int aml_i2s_silence(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	char *ppos;
	int n;
	struct snd_pcm_runtime *runtime = substream->runtime;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
static int aml_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_info(substream->pcm->card->dev,
		"\narea=%p,addr=%ld,bytes=%ld,rate:%d, channels:%d, subformat:%d\n",
		runtime->dma_area, (long)runtime->dma_addr, runtime->dma_bytes,
		runtime->rate, runtime->channels, runtime->subformat);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
		runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}
#endif

static struct snd_pcm_ops aml_i2s_ops = {
	.open = aml_i2s_open,
	.close = aml_i2s_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_i2s_hw_params,
	.hw_free = aml_i2s_hw_free,
	.prepare = aml_i2s_prepare,
	.trigger = aml_i2s_trigger,
	.pointer = aml_i2s_pointer,
#ifdef CONFIG_AMLOGIC_SND_SPLIT_MODE_MMAP
	.mmap = aml_pcm_mmap,
#else
	.copy = aml_i2s_copy,
#endif
	.silence = aml_i2s_silence,
};

/*--------------------------------------------------------------------------
 * ASoC platform driver
 *--------------------------------------------------------------------------
 */
static u64 aml_i2s_dmamask = 0xffffffff;

static int aml_i2s_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_card *card = rtd->card;
	struct snd_pcm *pcm = rtd->pcm;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &aml_i2s_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = aml_i2s_preallocate_dma_buffer(pcm,
						     SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = aml_i2s_preallocate_dma_buffer(pcm,
						     SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

 out:
	return ret;
}

static void aml_i2s_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	struct aml_audio_buffer *tmp_buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev, buf->bytes,
				  buf->area, buf->addr);
		buf->area = NULL;

		tmp_buf = buf->private_data;
		if (tmp_buf != NULL && tmp_buf->buffer_start != NULL)
			kfree(tmp_buf->buffer_start);
		if (tmp_buf != NULL)
			kfree(tmp_buf);
		buf->private_data = NULL;
	}
}

static const char *const output_swap_texts[] = { "L/R", "L/L", "R/R", "R/L" };

static const struct soc_enum output_swap_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(output_swap_texts),
			output_swap_texts);

static int aml_output_swap_get_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = read_i2s_mute_swap_reg();

	return 0;
}

static int aml_output_swap_set_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	audio_i2s_swap_left_right(ucontrol->value.enumerated.item[0]);

	return 0;
}

bool aml_audio_i2s_mute_flag;
static int aml_audio_set_i2s_mute(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	aml_audio_i2s_mute_flag = ucontrol->value.integer.value[0];

	pr_info("aml_audio_i2s_mute_flag: flag=%d\n", aml_audio_i2s_mute_flag);

	if (aml_audio_i2s_mute_flag)
		aml_audio_i2s_mute();
	else
		aml_audio_i2s_unmute();

	return 0;
}

static int aml_audio_get_i2s_mute(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aml_audio_i2s_mute_flag;

	return 0;
}

static const struct snd_kcontrol_new aml_i2s_controls[] = {
	SOC_SINGLE_BOOL_EXT("Audio i2s mute",
				0, aml_audio_get_i2s_mute,
				aml_audio_set_i2s_mute),

	SOC_ENUM_EXT("Output Swap",
				output_swap_enum,
				aml_output_swap_get_enum,
				aml_output_swap_set_enum),
};

static int aml_i2s_probe(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform,
			aml_i2s_controls, ARRAY_SIZE(aml_i2s_controls));
}

struct snd_soc_platform_driver aml_soc_platform = {
	.probe = aml_i2s_probe,
	.ops = &aml_i2s_ops,
	.pcm_new = aml_i2s_new,
	.pcm_free = aml_i2s_free_dma_buffers,
};

static int aml_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &aml_soc_platform);
}

static int aml_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_audio_dt_match[] = {
	{.compatible = "amlogic, aml-i2s",},
	{},
};
#else
#define amlogic_audio_dt_match NULL
#endif

#ifdef CONFIG_HIBERNATION
static unsigned long isa_timerd_saved;
static unsigned long isa_timerd_mux_saved;

static int aml_i2s_freeze(struct device *dev)
{
	isa_timerd_saved = aml_isa_read(ISA_TIMERD);
	isa_timerd_mux_saved = aml_isa_read(ISA_TIMER_MUX);

	return 0;
}

static int aml_i2s_thaw(struct device *dev)
{
	return 0;
}

static int aml_i2s_restore(struct device *dev)
{
	aml_isa_write(ISA_TIMERD, isa_timerd_saved);
	aml_isa_write(ISA_TIMER_MUX, isa_timerd_mux_saved);

	return 0;
}

const struct dev_pm_ops aml_i2s_pm = {
	.freeze		= aml_i2s_freeze,
	.thaw		= aml_i2s_thaw,
	.restore	= aml_i2s_restore,
};
#endif

static struct platform_driver aml_i2s_driver = {
	.driver = {
		.name           = "aml-i2s",
		.owner          = THIS_MODULE,
		.of_match_table = amlogic_audio_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm             = &aml_i2s_pm,
#endif
		},

	.probe = aml_soc_platform_probe,
	.remove = aml_soc_platform_remove,
};

static int __init aml_alsa_audio_init(void)
{
	return platform_driver_register(&aml_i2s_driver);
}

static void __exit aml_alsa_audio_exit(void)
{
	platform_driver_unregister(&aml_i2s_driver);
}

module_init(aml_alsa_audio_init);
module_exit(aml_alsa_audio_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AML audio driver for ALSA");
