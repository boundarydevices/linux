/*
 * drivers/amlogic/media/common/ge2d/ge2d_wq.h
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

#ifndef _GE2D_WQ_H_
#define _GE2D_WQ_H_
#ifdef CONFIG_AMLOGIC_ION
#include <ion/ion.h>

extern struct ion_client *ge2d_ion_client;
#endif

extern ssize_t work_queue_status_show(struct class *cla,
		struct class_attribute *attr, char *buf);

extern ssize_t free_queue_status_show(struct class *cla,
		struct class_attribute *attr, char *buf);

extern int ge2d_setup(int irq, struct reset_control *rstc);
extern int ge2d_wq_init(struct platform_device *pdev,
	int irq, struct clk *clk);
extern int ge2d_wq_deinit(void);

int ge2d_buffer_alloc(struct ge2d_dmabuf_req_s *ge2d_req_buf);
int ge2d_buffer_free(int index);
int ge2d_buffer_export(struct ge2d_dmabuf_exp_s *ge2d_exp_buf);
void ge2d_buffer_dma_flush(int dma_fd);
void ge2d_buffer_cache_flush(int dma_fd);
#endif
