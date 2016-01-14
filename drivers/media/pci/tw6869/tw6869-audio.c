/*
 * Copyright 2015 www.starterkit.ru <info@starterkit.ru>
 *
 * Based on:
 * Driver for Intersil|Techwell TW6869 based DVR cards
 * (c) 2011-12 liran <jli11@intersil.com> [Intersil|Techwell China]
 *
 * V4L2 PCI Skeleton Driver
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates.
 * All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "tw6869.h"

/**
 * tw6869 internals
 */
static int tw6869_ach_dma_locked(struct tw6869_dma *dma)
{
	return 1;
}

static void tw6869_ach_dma_srst(struct tw6869_dma *dma)
{
	/* tw_set(dma->dev, R8_AVSRST(dma->id), BIT(4)); */
}

static void tw6869_ach_dma_ctrl(struct tw6869_dma *dma)
{
	struct tw6869_ach *ach = container_of(dma, struct tw6869_ach, dma);

	tw_write(dma->dev, R8_AIGAIN_CTRL(dma->id), ach->gain);
}

static void tw6869_ach_dma_cfg(struct tw6869_dma *dma)
{
	return;
}

static void tw6869_ach_dma_isr(struct tw6869_dma *dma)
{
	struct tw6869_ach *ach = container_of(dma, struct tw6869_ach, dma);
	struct tw6869_buf *done = NULL;
	struct tw6869_buf *next = NULL;
	unsigned int i = dma->pb & 0x1;

	spin_lock(&dma->lock);
	if (tw_dma_active(dma) && !list_empty(&ach->buf_list)) {
		next = list_first_entry(&ach->buf_list, struct tw6869_buf, list);
		list_move_tail(&next->list, &ach->buf_list);
		done = dma->buf[i ^ 0x1];
		dma->buf[i] = next;
	}
	spin_unlock(&dma->lock);

	if (done && next) {
		tw_write(dma->dev, dma->reg[i], next->dma_addr);
		ach->ptr = done->dma_addr - ach->buffers[0].dma_addr;
		snd_pcm_period_elapsed(ach->ss);
	} else {
		tw_err(dma->dev, "ach%u NOBUF\n", ID2CH(dma->id));
	}

	dma->fld = 0x0;
	dma->pb ^= 0x1;
}

/**
 * Control Interface
 */
static int tw6869_capture_volume_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	info->count = 1;
	info->value.integer.min = 0;
	info->value.integer.max = 15;
	info->value.integer.step = 1;
	return 0;
}

static int tw6869_capture_volume_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct tw6869_ach *ach = snd_kcontrol_chip(kcontrol);

	value->value.integer.value[0] = ach->gain;
	return 0;
}

static int tw6869_capture_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct tw6869_ach *ach = snd_kcontrol_chip(kcontrol);
	struct tw6869_dma *dma = &ach->dma;
	unsigned long flags;

	if (ach->gain != value->value.integer.value[0]) {
		spin_lock_irqsave(&dma->lock, flags);
		ach->gain = value->value.integer.value[0];
		tw_write(dma->dev, R8_AIGAIN_CTRL(dma->id), ach->gain);
		spin_unlock_irqrestore(&dma->lock, flags);

		return 1; /* Indicate control was changed w/o error */
	}
	return 0;
}

static struct snd_kcontrol_new tw6869_capture_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Capture Volume",
	.info = tw6869_capture_volume_info,
	.get = tw6869_capture_volume_get,
	.put = tw6869_capture_volume_put,
};

/**
 * PCM Interface
 */
static int tw6869_pcm_hw_params(struct snd_pcm_substream *ss,
		struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(ss, params_buffer_bytes(hw_params));
}

static int tw6869_pcm_hw_free(struct snd_pcm_substream *ss)
{
	return snd_pcm_lib_free_pages(ss);
}

static const struct snd_pcm_hardware tw6869_capture_hw = {
	.info			= (SNDRV_PCM_INFO_MMAP |
				   SNDRV_PCM_INFO_INTERLEAVED |
				   SNDRV_PCM_INFO_BLOCK_TRANSFER |
				   SNDRV_PCM_INFO_MMAP_VALID),
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= SNDRV_PCM_RATE_48000,
	.rate_min		= 48000,
	.rate_max		= 48000,
	.channels_min		= 1,
	.channels_max		= 1,
	.buffer_bytes_max	= TW_PAGE_SIZE * TW_APAGE_MAX,
	.period_bytes_min	= TW_PAGE_SIZE,
	.period_bytes_max	= TW_PAGE_SIZE,
	.periods_min		= TW_APAGE_MAX,
	.periods_max		= TW_APAGE_MAX,
};

static int tw6869_pcm_open(struct snd_pcm_substream *ss)
{
	struct tw6869_ach *ach = snd_pcm_substream_chip(ss);
	struct snd_pcm_runtime *rt = ss->runtime;
	int ret;

	rt->hw = tw6869_capture_hw;
	ret = snd_pcm_hw_constraint_integer(rt, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	ach->ss = ss;
	return 0;
}

static int tw6869_pcm_close(struct snd_pcm_substream *ss)
{
	return 0;
}

static int tw6869_pcm_prepare(struct snd_pcm_substream *ss)
{
	struct tw6869_ach *ach = snd_pcm_substream_chip(ss);
	struct tw6869_dma *dma = &ach->dma;
	struct snd_pcm_runtime *rt = ss->runtime;
	unsigned int period = snd_pcm_lib_period_bytes(ss);
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&dma->lock, flags);
	INIT_LIST_HEAD(&ach->buf_list);
	for (i = 0; i < TW_BUF_MAX; i++)
		dma->buf[i] = NULL;

	if (period != TW_PAGE_SIZE || rt->periods != TW_APAGE_MAX) {
		spin_unlock_irqrestore(&dma->lock, flags);
		return -EINVAL;
	}

	for (i = 0; i < rt->periods; i++) {
		ach->buffers[i].dma_addr = rt->dma_addr + period * i;
		INIT_LIST_HEAD(&ach->buffers[i].list);
		list_add_tail(&ach->buffers[i].list, &ach->buf_list);
	}
	spin_unlock_irqrestore(&dma->lock, flags);

	return 0;
}

static int tw6869_ach_start(struct tw6869_ach *ach)
{
	struct tw6869_dma *dma = &ach->dma;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&dma->lock, flags);
	if (list_empty(&ach->buf_list)) {
		spin_unlock_irqrestore(&dma->lock, flags);
		return -EIO;
	}

	for (i = 0; i < TW_BUF_MAX; i++) {
		if (i < 2) {
			dma->buf[i] = list_first_entry(&ach->buf_list,
				struct tw6869_buf, list);
			list_move_tail(&dma->buf[i]->list, &ach->buf_list);
		} else {
			dma->buf[i] = NULL;
		}
	}
	tw_dma_set_addr(dma);
	ach->ptr = 0;
	spin_unlock_irqrestore(&dma->lock, flags);

	spin_lock_irqsave(&dma->dev->rlock, flags);
	tw6869_ach_dma_cfg(dma);
	tw_dma_enable(dma);
	spin_unlock_irqrestore(&dma->dev->rlock, flags);

	return 0;
}

static int tw6869_ach_stop(struct tw6869_ach *ach)
{
	struct tw6869_dma *dma = &ach->dma;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&dma->dev->rlock, flags);
	tw_dma_disable(dma);
	spin_unlock_irqrestore(&dma->dev->rlock, flags);

	spin_lock_irqsave(&dma->lock, flags);
	INIT_LIST_HEAD(&ach->buf_list);
	for (i = 0; i < TW_BUF_MAX; i++)
		dma->buf[i] = NULL;
	spin_unlock_irqrestore(&dma->lock, flags);

	cancel_delayed_work_sync(&dma->hw_on);
	return 0;
}

static int tw6869_pcm_trigger(struct snd_pcm_substream *ss, int cmd)
{
	struct tw6869_ach *ach = snd_pcm_substream_chip(ss);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		return tw6869_ach_start(ach);
	case SNDRV_PCM_TRIGGER_STOP:
		return tw6869_ach_stop(ach);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t tw6869_pcm_pointer(struct snd_pcm_substream *ss)
{
	struct tw6869_ach *ach = snd_pcm_substream_chip(ss);

	return bytes_to_frames(ss->runtime, ach->ptr);
}

static struct snd_pcm_ops tw6869_pcm_ops = {
	.open = tw6869_pcm_open,
	.close = tw6869_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = tw6869_pcm_hw_params,
	.hw_free = tw6869_pcm_hw_free,
	.prepare = tw6869_pcm_prepare,
	.trigger = tw6869_pcm_trigger,
	.pointer = tw6869_pcm_pointer,
};

static int tw6869_ach_register(struct tw6869_ach *ach)
{
	struct pci_dev *pdev = ach->dma.dev->pdev;
	static struct snd_device_ops ops = { NULL };
	struct snd_card *card;
	struct snd_pcm *pcm;
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	ret = snd_card_new(&pdev->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
		THIS_MODULE, 0, &card);
#else
	ret = snd_card_create(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
		THIS_MODULE, 0, &card);
#endif
	if (ret < 0)
		return ret;

	snd_card_set_dev(card, &pdev->dev);
	ach->snd_card = card;

	strlcpy(card->driver, KBUILD_MODNAME, sizeof(card->driver));
	snprintf(card->shortname, sizeof(card->shortname), "tw6869-ach%u",
		ID2CH(ach->dma.id));
	snprintf(card->longname, sizeof(card->longname), "%s on %s",
		card->shortname, pci_name(pdev));
	strlcpy(card->mixername, card->shortname, sizeof(card->mixername));

	ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, ach, &ops);
	if (ret < 0)
		goto snd_error;

	ach->gain = 0x08;
	ret = snd_ctl_add(card, snd_ctl_new1(&tw6869_capture_volume, ach));
	if (ret < 0)
		goto snd_error;

	ret = snd_pcm_new(card, card->driver, 0, 0, 1, &pcm);
	if (ret < 0)
		goto snd_error;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &tw6869_pcm_ops);
	snd_pcm_chip(pcm) = ach;
	pcm->info_flags = 0;
	snprintf(pcm->name, sizeof(pcm->name), "%s ADC", card->shortname);

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm,
				SNDRV_DMA_TYPE_DEV,
				snd_dma_pci_data(pdev),
				TW_PAGE_SIZE * TW_APAGE_MAX,
				TW_PAGE_SIZE * TW_APAGE_MAX);
	if (ret < 0)
		goto snd_error;

	ret = snd_card_register(card);
	if (ret == 0)
		return 0;

snd_error:
	snd_card_free(card);
	ach->snd_card = NULL;
	return ret;
}

void tw6869_audio_unregister(struct tw6869_dev *dev)
{
	unsigned int i;

	/* Reset and disable audio DMA */
	tw_clear(dev, R32_DMA_CMD, TW_AID);
	tw_clear(dev, R32_DMA_CHANNEL_ENABLE, TW_AID);

	if (dev->ach_max > TW_CH_MAX)
		dev->ach_max = TW_CH_MAX;

	for (i = 0; i < dev->ach_max; i++) {
		struct tw6869_ach *ach = &dev->ach[ID2CH(i)];

		snd_card_free(ach->snd_card);
		ach->snd_card = NULL;
	}
}

int tw6869_audio_register(struct tw6869_dev *dev)
{
	unsigned int i;
	int ret;

	if (dev->ach_max > TW_CH_MAX)
		dev->ach_max = TW_CH_MAX;

	for (i = 0; i < dev->ach_max; i++) {
		struct tw6869_ach *ach = &dev->ach[ID2CH(i)];

		ret = tw6869_ach_register(ach);
		if (ret) {
			tw_err(dev, "failed to register ach%u\n", i);
			dev->ach_max = i;
			return ret;
		}

		ach->dma.locked = tw6869_ach_dma_locked;
		ach->dma.srst = tw6869_ach_dma_srst;
		ach->dma.ctrl = tw6869_ach_dma_ctrl;
		ach->dma.cfg = tw6869_ach_dma_cfg;
		ach->dma.isr = tw6869_ach_dma_isr;
	}
	return 0;
}
