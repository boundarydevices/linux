/*
 * include/linux/amlogic/media/v4l_util/videobuf-res.h
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

#ifndef _VIDEOBUF_RES_H
#define _VIDEOBUF_RES_H

#include <media/videobuf-core.h>

struct videobuf_res_privdata {
	/* const* char dev_name; */
	u32 magic;
	resource_size_t start;
	resource_size_t end;
	void *priv;
};

void videobuf_queue_res_init(struct videobuf_queue *q,
				    const struct videobuf_queue_ops *ops,
				    struct device *dev,
				    spinlock_t *irqlock,
				    enum v4l2_buf_type type,
				    enum v4l2_field field,
				    unsigned int msize,
				    void *priv,
				    struct mutex *ext_lock);

resource_size_t videobuf_to_res(struct videobuf_buffer *buf);
void videobuf_res_free(struct videobuf_queue *q,
			      struct videobuf_buffer *buf);

#endif /* _VIDEOBUF_RES_H */
