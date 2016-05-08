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

#ifndef __TW6869_H
#define __TW6869_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/control.h>

#define FSL_G_CHIP_IDENT
#define FSL_QUERYBUF
#define TW_DEFAULT_V4L2_STD        V4L2_STD_NTSC /* V4L2_STD_PAL */

#define TW_DMA_ERR_MAX             30
#define TW_APAGE_MAX               16
#define TW_VBUF_ALLOC              6

#define TW_DEBUG                   1

#define tw_dbg(twdev, fmt, arg...) \
	do { if (TW_DEBUG) dev_info(&twdev->pdev->dev, \
		"%s: " fmt, __func__, ## arg); } while (0)

#define tw_info(twdev, fmt, arg...) \
	dev_info(&twdev->pdev->dev, "%s: " fmt, __func__, ## arg)

#define tw_err(twdev, fmt, arg...) \
	dev_err(&twdev->pdev->dev, "%s: " fmt, __func__, ## arg)

#define PCI_VENDOR_ID_TECHWELL     0x1797
#define PCI_DEVICE_ID_6869         0x6869
#define PCI_DEVICE_ID_6865         0x6865

#define TW_MMIO_BAR                0
#define TW_PAGE_SIZE               4096

#define TW_STD_NTSC_M              0
#define TW_STD_PAL                 1
#define TW_STD_SECAM               2
#define TW_STD_NTSC_443            3
#define TW_STD_PAL_M               4
#define TW_STD_PAL_CN              5
#define TW_STD_PAL_60              6

#define TW_FMT_UYVY                0
#define TW_FMT_RGB565              5
#define TW_FMT_YUYV                6

#define TW_VDMA_SG_MODE            0
#define TW_VDMA_FRAME_MODE         2
#define TW_VDMA_FIELD_MODE         3

#define TW_ID_MAX                  16
#define TW_CH_MAX                  8
#define TW_VIN_MAX                 4
#define TW_BUF_MAX                 4

#define TW_VID                     0x00FF
#define TW_AID                     0xFF00

#define RADDR(idx)                 ((idx) * 0x4)
#define ID2CH(id)                  ((id) & 0x7)
#define ID2SC(id)                  ((id) & 0x3)
#define GROUP(id)                  (((id) & 0x4) * 0x40)

/**
 * Register definitions
 */
#define R32_INT_STATUS             RADDR(0x00)
#define R32_PB_STATUS              RADDR(0x01)
#define R32_DMA_CMD                RADDR(0x02)
#define R32_FIFO_STATUS            RADDR(0x03)
#define R32_VIDEO_PARSER_STATUS    RADDR(0x05)
#define R32_SYS_SOFT_RST           RADDR(0x06)
#define R32_DMA_CHANNEL_ENABLE     RADDR(0x0A)
#define R32_DMA_CONFIG             RADDR(0x0B)
#define R32_DMA_TIMER_INTERVAL     RADDR(0x0C)
#define R32_DMA_CHANNEL_TIMEOUT    RADDR(0x0D)
#define R32_DMA_CHANNEL_CONFIG(id) RADDR(0x10 + ID2CH(id))
#define R32_ADMA_P_ADDR(id)        RADDR(0x18 + ID2CH(id) * 0x2)
#define R32_ADMA_B_ADDR(id)        RADDR(0x19 + ID2CH(id) * 0x2)
#define R32_VIDEO_CONTROL1         RADDR(0x2A)
#define R32_VIDEO_CONTROL2         RADDR(0x2B)
#define R32_AUDIO_CONTROL1         RADDR(0x2C)
#define R32_AUDIO_CONTROL2         RADDR(0x2D)
#define R32_PHASE_REF              RADDR(0x2E)
#define R32_VIDEO_FIELD_CTRL(id)   RADDR(0x39 + ID2CH(id))
#define R32_HSCALER_CTRL(id)       RADDR(0x42 + ID2CH(id))
#define R32_VIDEO_SIZE(id)         RADDR(0x4A + ID2CH(id))
#define R32_VDMA_P_ADDR(id)        RADDR(0x80 + ID2CH(id) * 0x8)
#define R32_VDMA_WHP(id)           RADDR(0x81 + ID2CH(id) * 0x8)
#define R32_VDMA_B_ADDR(id)        RADDR(0x82 + ID2CH(id) * 0x8)
#define R32_VDMA_F2_P_ADDR(id)     RADDR(0x84 + ID2CH(id) * 0x8)
#define R32_VDMA_F2_WHP(id)        RADDR(0x85 + ID2CH(id) * 0x8)
#define R32_VDMA_F2_B_ADDR(id)     RADDR(0x86 + ID2CH(id) * 0x8)

/* BIT[31:8] are hardwired to 0 in all registers */
#define R8_VIDEO_STATUS(id)        RADDR(0x100 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_BRIGHTNESS_CTRL(id)     RADDR(0x101 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_CONTRAST_CTRL(id)       RADDR(0x102 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_SHARPNESS_CTRL(id)      RADDR(0x103 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_SAT_U_CTRL(id)          RADDR(0x104 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_SAT_V_CTRL(id)          RADDR(0x105 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_HUE_CTRL(id)            RADDR(0x106 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_CROPPING_CONTROL(id)    RADDR(0x107 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_VERTICAL_DELAY(id)      RADDR(0x108 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_VERTICAL_ACTIVE(id)     RADDR(0x109 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_HORIZONTAL_DELAY(id)    RADDR(0x10A + GROUP(id) + ID2SC(id) * 0x10)
#define R8_HORIZONTAL_ACTIVE(id)   RADDR(0x10B + GROUP(id) + ID2SC(id) * 0x10)
#define R8_STANDARD_SELECTION(id)  RADDR(0x10E + GROUP(id) + ID2SC(id) * 0x10)
#define R8_STANDARD_RECOGNIT(id)   RADDR(0x10F + GROUP(id) + ID2SC(id) * 0x10)
#define R8_VERTICAL_SCALING(id)    RADDR(0x144 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_SCALING_HIGH(id)        RADDR(0x145 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_HORIZONTAL_SCALING(id)  RADDR(0x146 + GROUP(id) + ID2SC(id) * 0x10)

#define R8_F2CROPPING_CONTROL(id)    RADDR(0x147 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2VERTICAL_DELAY(id)      RADDR(0x148 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2VERTICAL_ACTIVE(id)     RADDR(0x149 + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2HORIZONTAL_DELAY(id)    RADDR(0x14A + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2HORIZONTAL_ACTIVE(id)   RADDR(0x14B + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2VERTICAL_SCALING(id)    RADDR(0x14C + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2SCALING_HIGH(id)        RADDR(0x14D + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2HORIZONTAL_SCALING(id)  RADDR(0x14E + GROUP(id) + ID2SC(id) * 0x10)
#define R8_F2CNT(id)		     RADDR(0x14F + GROUP(id) + ID2SC(id) * 0x10)

#define R8_AVSRST(id)              RADDR(0x180 + GROUP(id))
#define R8_VERTICAL_CONTROL1(id)   RADDR(0x18F + GROUP(id))
#define R8_MISC_CONTROL1(id)       RADDR(0x194 + GROUP(id))
#define R8_MISC_CONTROL2(id)       RADDR(0x196 + GROUP(id))
#define R8_ANALOG_PWR_DOWN(id)     RADDR(0x1CE + GROUP(id))
#define R8_AIGAIN_CTRL(id)         RADDR(0x1D0 + GROUP(id) + ID2SC(id))

/**
 * struct tw6869_buf - instance of one DMA buffer
 */
struct tw6869_buf {
	struct vb2_buffer vb;
	struct list_head list;
	dma_addr_t dma_addr;
};

/**
 * struct tw6869_dma - instance of one DMA channel
 * @dev: parent device
 * @buf: DMA ping-pong buffers
 * @hw_on: switching DMA ON with delay
 * @lock: spinlock controlling access to channel
 * @reg: DMA ping-pong registers
 * @id: DMA id
 * @fld: field1 | field2 ping-pong state
 * @pb: P-buffer | B-buffer ping-pong state
 * @delay: delay in jiffies before switching DMA ON
 * @is_on: DMA channel status
 * @lost: video (audio) signal lost
 * @low_power: channel in a low-power state
 * @err: DMA errors counter
 * @srst: software reset the video (audio) portion
 * @ctrl: restore control state
 * @cfg: configure the DMA channel
 * @isr: DMA channel ISR
 */
struct tw6869_dma {
	struct tw6869_dev *dev;
	struct tw6869_buf * buf[TW_BUF_MAX];
	struct delayed_work hw_on;
	spinlock_t lock;
	unsigned int reg[TW_BUF_MAX];
	unsigned int id;
	unsigned int fld;
	unsigned int pb;
	unsigned int delay;
	unsigned int is_on;
	unsigned int lost;
	unsigned int low_power;
	unsigned int err;
	void (*srst)(struct tw6869_dma *);
	void (*ctrl)(struct tw6869_dma *);
	void (*cfg)(struct tw6869_dma *);
	void (*isr)(struct tw6869_dma *);
};

/**
 * struct tw6869_vch - instance of one video channel
 * @dma: DMA channel
 * @vdev: V4L2 video device
 * @mlock: ioctl serialization mutex
 * @queue: queue maintained by videobuf2 layer
 * @buf_list: list of buffers queued for DMA
 * @hdl: handler for control framework
 * @format: pixel format
 * @std: video standard (e.g. PAL/NTSC)
 * @input: input line for video signal
 * @sequence: frame sequence counter
 * @dcount: number of dropped frames
 * @fps: current frame rate
 * @brightness: control state
 * @contrast: control state
 * @sharpness: control state
 * @saturation: control state
 * @hue: control state
 */
struct tw6869_vch {
	struct tw6869_dma dma;
	struct video_device vdev;
	struct mutex mlock;
	struct vb2_queue queue;
	struct list_head buf_list;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_pix_format format;
	v4l2_std_id std;
	unsigned int input;
	unsigned int sequence;
	unsigned int dcount;
	unsigned int fps;
	unsigned int brightness;
	unsigned int contrast;
	unsigned int sharpness;
	unsigned int saturation;
	unsigned int hue;
};

/**
 * struct tw6869_ach - instance of one audio channel
 * @dma: DMA channel
 * @snd_card: soundcard registered in ALSA layer
 * @ss: PCM substream
 * @buffers: split an contiguous buffer into chunks (DMA pages)
 * @buf_list: ring buffer consisting of DMA pages
 * @ptr: PCM buffer pointer
 * @gain: control state
 */
struct tw6869_ach {
	struct tw6869_dma dma;
	struct snd_card *snd_card;
	struct snd_pcm_substream *ss;
	struct tw6869_buf buffers[TW_APAGE_MAX];
	struct list_head buf_list;
	dma_addr_t ptr;
	unsigned int gain;
};

/**
 * struct tw6869_dev - instance of device
 * @pdev: PCI device
 * @mmio: hardware base address
 * @rlock: spinlock controlling access to the shared registers
 * @vch_max: number of the used video channels
 * @ach_max: number of the used audio channels
 * @dma: DMA channels
 * @v4l2_dev: device registered in V4L2 layer
 * @alloc_ctx: context for videobuf2
 * @vch: array of video channel instance
 * @ach: array of audio channel instance
 */
struct tw6869_dev {
	struct pci_dev *pdev;
	unsigned char __iomem *mmio;
	spinlock_t rlock;
	unsigned int vch_max;
	unsigned int ach_max;
	struct tw6869_dma * dma[TW_ID_MAX];
	struct v4l2_device v4l2_dev;
	struct vb2_alloc_ctx *alloc_ctx;
	struct tw6869_vch vch[TW_CH_MAX];
	struct tw6869_ach ach[TW_CH_MAX];
};

static inline void tw_write(struct tw6869_dev *dev,
		unsigned int reg, unsigned int val)
{
	iowrite32(val, dev->mmio + reg);
}

static inline unsigned int tw_read(struct tw6869_dev *dev,
		unsigned int reg)
{
	return ioread32(dev->mmio + reg);
}

static inline void tw_write_mask(struct tw6869_dev *dev,
		unsigned int reg, unsigned int val, unsigned int mask)
{
	unsigned int v = tw_read(dev, reg);

	v = (v & ~mask) | (val & mask);
	tw_write(dev, reg, v);
}

static inline void tw_clear(struct tw6869_dev *dev,
		unsigned int reg, unsigned int val)
{
	tw_write_mask(dev, reg, 0, val);
}

static inline void tw_set(struct tw6869_dev *dev,
		unsigned int reg, unsigned int val)
{
	tw_write_mask(dev, reg, val, val);
}

static inline unsigned int tw_dma_active(struct tw6869_dma *dma)
{
	return dma->is_on && dma->buf[0] && dma->buf[1];
}

static inline void tw_dma_set_addr(struct tw6869_dma *dma)
{
	unsigned int i;

	for (i = 0; i < TW_BUF_MAX; i++) {
		if (dma->reg[i] && dma->buf[i])
			tw_write(dma->dev, dma->reg[i], dma->buf[i]->dma_addr);
	}
}

static inline unsigned int tw_dma_is_on(struct tw6869_dma *dma)
{
	unsigned int e = tw_read(dma->dev, R32_DMA_CHANNEL_ENABLE);
	unsigned int c = tw_read(dma->dev, R32_DMA_CMD);

	return c & e & BIT(dma->id);
}

static inline void tw_dma_enable(struct tw6869_dma *dma)
{
	tw_set(dma->dev, R32_DMA_CHANNEL_ENABLE, BIT(dma->id));
	tw_set(dma->dev, R32_DMA_CMD, BIT(31) | BIT(dma->id));

	dma->fld = 0;
	dma->pb = 0;
	dma->err = 0;
	dma->lost = 0;
	dma->low_power = 0;
	dma->is_on = tw_dma_is_on(dma);
	tw_dbg(dma->dev, "DMA %u\n", dma->id);
}

static inline void tw_dma_disable(struct tw6869_dma *dma)
{
	tw_clear(dma->dev, R32_DMA_CMD, BIT(dma->id));
	tw_clear(dma->dev, R32_DMA_CHANNEL_ENABLE, BIT(dma->id));

	dma->is_on = tw_dma_is_on(dma);
	tw_dbg(dma->dev, "DMA %u\n", dma->id);
}

int tw6869_video_register(struct tw6869_dev *dev);
void tw6869_video_unregister(struct tw6869_dev *dev);
int tw6869_audio_register(struct tw6869_dev *dev);
void tw6869_audio_unregister(struct tw6869_dev *dev);

#endif /* __TW6869_H */
