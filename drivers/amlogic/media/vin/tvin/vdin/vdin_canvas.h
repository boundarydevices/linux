/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_canvas.h
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

#ifndef __VDIN_CANVAS_H
#define __VDIN_CANVAS_H

#include <linux/sizes.h>
#include <linux/amlogic/media/vfm/vframe.h>

#define VDIN_CANVAS_MAX_WIDTH_UHD       4096
#define VDIN_CANVAS_MAX_WIDTH_HD        1920
#define VDIN_CANVAS_MAX_HEIGH			2228
#define VDIN_YUV422_8BIT_PER_PIXEL_BYTE		2
#define VDIN_YUV422_10BIT_PER_PIXEL_BYTE	3
#define VDIN_YUV444_10BIT_PER_PIXEL_BYTE	4
#define VDIN_YUV444_8BIT_PER_PIXEL_BYTE		3
#define VDIN_MIN_SOURCE_BITDEPTH		8


#define VDIN_YUV444_MAX_CMA_WIDTH       4096
#define VDIN_YUV444_MAX_CMA_HEIGH       2160

extern const unsigned int vdin_canvas_ids[2][VDIN_CANVAS_MAX_CNT];
extern void vdin_canvas_init(struct vdin_dev_s *devp);
extern void vdin_canvas_start_config(struct vdin_dev_s *devp);
extern void vdin_canvas_auto_config(struct vdin_dev_s *devp);
extern unsigned int vdin_cma_alloc(struct vdin_dev_s *devp);
extern void vdin_cma_release(struct vdin_dev_s *devp);
extern void vdin_cma_malloc_mode(struct vdin_dev_s *devp);
#endif /* __VDIN_CANVAS_H */

