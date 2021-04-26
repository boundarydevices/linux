/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2004-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2018 NXP.
 */

#ifndef __ASM_ARCH_MXC_DMA_H__
#define __ASM_ARCH_MXC_DMA_H__

#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/dmaengine.h>

/*
 * This enumerates peripheral types. Used for SDMA.
 */
enum sdma_peripheral_type {
	IMX_DMATYPE_SSI,	/* MCU domain SSI */
	IMX_DMATYPE_SSI_SP,	/* Shared SSI */
	IMX_DMATYPE_MMC,	/* MMC */
	IMX_DMATYPE_SDHC,	/* SDHC */
	IMX_DMATYPE_UART,	/* MCU domain UART */
	IMX_DMATYPE_UART_SP,	/* Shared UART */
	IMX_DMATYPE_FIRI,	/* FIRI */
	IMX_DMATYPE_CSPI,	/* MCU domain CSPI */
	IMX_DMATYPE_CSPI_SP,	/* Shared CSPI */
	IMX_DMATYPE_SIM,	/* SIM */
	IMX_DMATYPE_ATA,	/* ATA */
	IMX_DMATYPE_CCM,	/* CCM */
	IMX_DMATYPE_EXT,	/* External peripheral */
	IMX_DMATYPE_MSHC,	/* Memory Stick Host Controller */
	IMX_DMATYPE_MSHC_SP,	/* Shared Memory Stick Host Controller */
	IMX_DMATYPE_DSP,	/* DSP */
	IMX_DMATYPE_MEMORY,	/* Memory */
	IMX_DMATYPE_FIFO_MEMORY,/* FIFO type Memory */
	IMX_DMATYPE_SPDIF,	/* SPDIF */
	IMX_DMATYPE_IPU_MEMORY,	/* IPU Memory */
	IMX_DMATYPE_ASRC,	/* ASRC */
	IMX_DMATYPE_ESAI,	/* ESAI */
	IMX_DMATYPE_SSI_DUAL,	/* SSI Dual FIFO */
	IMX_DMATYPE_ASRC_SP,	/* Shared ASRC */
	IMX_DMATYPE_SAI,	/* SAI */
	IMX_DMATYPE_MULTI_SAI,	/* MULTI FIFOs For Audio */
	IMX_DMATYPE_HDMI,	/* HDMI Audio */
	IMX_DMATYPE_I2C,	/* I2C */
};

enum imx_dma_prio {
	DMA_PRIO_HIGH = 0,
	DMA_PRIO_MEDIUM = 1,
	DMA_PRIO_LOW = 2
};

/**
 * struct sdma_audio_config - special sdma config for audio case
 * @src_fifo_num: source fifo number for mcu_2_sai/sai_2_mcu script
 *                For example, if there are 4 fifos, sdma will fetch
 *                fifos one by one and roll back to the first fifo after
 *                the 4th fifo fetch.
 * @dst_fifo_num: similar as src_fifo_num, but dest fifo instead.
 * @src_fifo_off: source fifo offset, 0 means all fifos are continuous, 1
 *                means 1 word offset between fifos. All offset between
 *                fifos should be same.
 * @dst_fifo_off: dst fifo offset, similar as @src_fifo_off.
 * @words_per_fifo: numbers of words per fifo fetch/fill, 0 means
 *                  one channel per fifo, 1 means 2 channels per fifo..
 *                  If 'src_fifo_num =  4' and 'chans_per_fifo = 1', it
 *                  means the first two words(channels) fetch from fifo1
 *                  and then jump to fifo2 for next two words, and so on
 *                  after the last fifo4 fetched, roll back to fifo1.
 * @sw_done_sel: software done selector, PDM need enable software done feature
 *               in mcu_2_sai/sai_2_mcu script.
 *               Bit31: sw_done eanbled or not
 *               Bit16~Bit0: selector
 *               For example: 0x80000000 means sw_done enabled for done0
 *                            sector which is for PDM on i.mx8mm.
 */
struct sdma_audio_config {
	u8 src_fifo_num;
	u8 dst_fifo_num;
	u8 src_fifo_off;
	u8 dst_fifo_off;
	u8 words_per_fifo;
	u32 sw_done_sel;
};

struct imx_dma_data {
	int dma_request; /* DMA request line */
	int dma_request2; /* secondary DMA request line */
	enum sdma_peripheral_type peripheral_type;
	int priority;
};

static inline int imx_dma_is_ipu(struct dma_chan *chan)
{
	return !strcmp(dev_name(chan->device->dev), "ipu-core");
}

static inline int imx_dma_is_pxp(struct dma_chan *chan)
{
        return strstr(dev_name(chan->device->dev), "pxp") != NULL;
}

static inline int imx_dma_is_general_purpose(struct dma_chan *chan)
{
	return !strcmp(chan->device->dev->driver->name, "imx-sdma") ||
		!strcmp(chan->device->dev->driver->name, "imx-dma");
}

#endif
