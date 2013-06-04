/*
 * Copyright 2009 Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This code is based on code copyrighted by Freescale,
 * Liam Girdwood, Javier Martin and probably others.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _IMX_PCM_H
#define _IMX_PCM_H

#include <mach/dma.h>

/*
 * Do not change this as the FIQ handler depends on this size
 */
#define IMX_DEFAULT_DMABUF_SIZE	(64 * 1024)
#define IMX_SSI_DMABUF_SIZE	(64 * 1024)
#define IMX_ESAI_DMABUF_SIZE	(256 * 1024)
#define IMX_SPDIF_DMABUF_SIZE	(64 * 1024)

struct imx_pcm_dma_params {
	int dma;
	unsigned long dma_addr;
	int burstsize;
	enum sdma_peripheral_type peripheral_type;
	int dma_buf_size;
};

struct imx_pcm_params {
	struct imx_pcm_dma_params dma_params_tx;
	struct imx_pcm_dma_params dma_params_rx;
};

int snd_imx_pcm_mmap(struct snd_pcm_substream *substream,
		     struct vm_area_struct *vma);
int imx_pcm_new(struct snd_soc_pcm_runtime *rtd);
void imx_pcm_free(struct snd_pcm *pcm);

#endif /* _IMX_PCM_H */
