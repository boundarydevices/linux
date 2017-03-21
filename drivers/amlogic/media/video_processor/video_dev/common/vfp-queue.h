/*
 * drivers/amlogic/media/video_processor/video_dev/common/vfp-queue.h
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

#ifndef __VFP_QUEUE_H__
#define __VFP_QUEUE_H__
#include "vfp.h"

static inline bool vfq_full(struct vfq_s *q)
{
	bool ret = (((q->wp+1) % q->size) == q->rp);
	return ret;
}
#endif
