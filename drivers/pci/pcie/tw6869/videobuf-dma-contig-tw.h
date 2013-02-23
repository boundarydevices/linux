/*
 * helper functions for physically contiguous capture buffers
 *
 * The functions support hardware lacking scatter gather support
 * (i.e. the buffers must be linear in physical memory)
 *
 * Copyright (c) 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2
 */
#ifndef _VIDEOBUF_DMA_CONTIG_TW_H
#define _VIDEOBUF_DMA_CONTIG_TW_H

#include <linux/dma-mapping.h>
#include <media/videobuf-core.h>
#include <linux/version.h>

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
void videobuf_queue_dma_contig_init_isl(struct videobuf_queue *q,
				    const struct videobuf_queue_ops *ops,
				    struct device *dev,
				    spinlock_t *irqlock,
				    enum v4l2_buf_type type,
				    enum v4l2_field field,
				    unsigned int msize,
				    void *priv,
				    struct mutex *ext_lock);
#else
void videobuf_queue_dma_contig_init_isl(struct videobuf_queue *q,
				    struct videobuf_queue_ops *ops,
				    struct device *dev,
				    spinlock_t *irqlock,
				    enum v4l2_buf_type type,
				    enum v4l2_field field,
				    unsigned int msize,
				    void *priv);
#endif

dma_addr_t videobuf_to_dma_contig_isl(struct videobuf_buffer *buf);
void* videobuf_to_vaddr_isl(struct videobuf_buffer *buf);
void videobuf_dma_contig_free_isl(struct videobuf_queue *q,
			      struct videobuf_buffer *buf);

#endif /* _VIDEOBUF_DMA_CONTIG_TW_H */
