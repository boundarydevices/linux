/*
 * Copyright (C) 2015 VanguardiaSur - www.vanguardiasur.com.ar
 *
 * Copyright (C) 2015 Industrial Research Institute for Automation
 * and Measurements PIAP
 * Written by Krzysztof Hałasa
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <sound/pcm.h>

#include "tw686x-regs.h"

#define TYPE_MAX_CHANNELS	0x0F
#define TYPE_SECOND_GEN		0x10
#define TW686X_DEF_PHASE_REF	0x1518

#define TW686X_FIELD_MODE	0x3
#define TW686X_FRAME_MODE	0x2
/* 0x1 is reserved */
#define TW686X_SG_MODE		0x0

#define TW686X_PAGE_SIZE	4096
#define TW686X_AUDIO_PAGE_MAX	16

struct tw686x_format {
	char *name;
	unsigned fourcc;
	unsigned depth;
	unsigned mode;
};

struct tw686x_dma_desc {
	dma_addr_t phys;
	void *virt;
	unsigned size;
};

struct tw686x_audio_buf {
	dma_addr_t dma;
	void *virt;
	struct list_head list;
};

struct tw686x_v4l2_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

struct tw686x_audio_channel {
	struct tw686x_dev *dev;
	struct snd_pcm_substream *ss;
	unsigned int ch;
	struct tw686x_audio_buf *curr_bufs[2];
	struct tw686x_dma_desc dma_descs[2];
	dma_addr_t ptr;

	struct tw686x_audio_buf buf[TW686X_AUDIO_PAGE_MAX];
	struct list_head buf_list;
	spinlock_t lock;
};

struct tw686x_video_channel {
	struct tw686x_dev *dev;

	struct vb2_queue vidq;
	struct list_head vidq_queued;
	struct video_device *device;
	struct tw686x_v4l2_buf *curr_bufs[2];
	struct tw686x_dma_desc dma_descs[2];

	struct v4l2_ctrl_handler ctrl_handler;
	const struct tw686x_format *format;
	struct mutex vb_mutex;
	spinlock_t qlock;
	v4l2_std_id video_standard;
	unsigned int width, height;
	unsigned int h_halve, v_halve;
	unsigned int ch;
	unsigned int num;
	unsigned int fps;
	unsigned int input;
	unsigned int sequence;
	unsigned int pb;
	bool no_signal;
};

/**
 * struct tw686x_dev - global device status
 * @lock: spinlock controlling access to the
 *        shared device registers (DMA enable/disable).
 */
struct tw686x_dev {
	spinlock_t lock;

	struct v4l2_device v4l2_dev;
	struct snd_card *snd_card;

	char name[32];
	unsigned int type;
	struct pci_dev *pci_dev;
	__u32 __iomem *mmio;

	void *alloc_ctx;

	struct tw686x_video_channel *video_channels;
	struct tw686x_audio_channel *audio_channels;

	int audio_rate; /* per-device value */

	struct timer_list dma_delay_timer;
	u32 pending_dma_en; /* must be protected by lock */
	u32 pending_dma_cmd; /* must be protected by lock */
};

static inline uint32_t reg_read(struct tw686x_dev *dev, unsigned reg)
{
	return readl(dev->mmio + reg);
}

static inline void reg_write(struct tw686x_dev *dev, unsigned reg,
			     uint32_t value)
{
	writel(value, dev->mmio + reg);
}

static inline void tw686x_disable_channel(struct tw686x_dev *dev,
					  unsigned int channel)
{
	u32 dma_en = reg_read(dev, DMA_CHANNEL_ENABLE);
	u32 dma_cmd = reg_read(dev, DMA_CMD);

	dma_en &= ~BIT(channel);
	dma_cmd &= ~BIT(channel);

	/* Must remove it from pending too */
	dev->pending_dma_en &= ~BIT(channel);
	dev->pending_dma_cmd &= ~BIT(channel);

	/* Stop DMA if no channels are enabled */
	if (!dma_en)
		dma_cmd = 0;
	reg_write(dev, DMA_CHANNEL_ENABLE, dma_en);
	reg_write(dev, DMA_CMD, dma_cmd);
}

static inline void tw686x_enable_channel(struct tw686x_dev *dev,
					 unsigned int channel)
{
	u32 dma_en = reg_read(dev, DMA_CHANNEL_ENABLE);
	u32 dma_cmd = reg_read(dev, DMA_CMD);

	dev->pending_dma_en |= dma_en | BIT(channel);
	dev->pending_dma_cmd |= dma_cmd | BIT(31) | BIT(channel);
}

static inline unsigned max_channels(struct tw686x_dev *dev)
{
	return dev->type & TYPE_MAX_CHANNELS; /* 4 or 8 channels */
}

static inline unsigned is_second_gen(struct tw686x_dev *dev)
{
	/* each channel has its own DMA SG table */
	return dev->type & TYPE_SECOND_GEN;
}

int tw686x_video_init(struct tw686x_dev *dev);
void tw686x_video_free(struct tw686x_dev *dev);
void tw686x_video_irq(struct tw686x_dev *dev, unsigned long requests,
		      unsigned int pb_status, unsigned int fifo_status,
		      unsigned int *reset_ch);

int tw686x_audio_init(struct tw686x_dev *dev);
void tw686x_audio_free(struct tw686x_dev *dev);
void tw686x_audio_irq(struct tw686x_dev *dev, unsigned long requests,
		      unsigned int pb_status);
