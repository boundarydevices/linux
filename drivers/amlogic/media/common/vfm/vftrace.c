/*
 * drivers/amlogic/media/common/vfm/vftrace.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "vftrace.h"

struct trace_info {
	void *vf;
	int type;
	int index;
	u64 in_time_us;		/*in trace jiffies */
	u32 duration;
	u32 pts;
};

struct vf_trace {
	const char *name;
	int max;
	int w_index;
	int num;
	int get;
	spinlock_t lock;
	int use_lock;
	struct trace_info trace_buf[1];
};

void *vftrace_alloc_trace(const char *name, int get, int max)
{
	int buf_size = sizeof(struct vf_trace) +
		max * sizeof(struct trace_info);
	struct vf_trace *trace = kmalloc(buf_size, GFP_KERNEL);

	memset(trace, 0, buf_size);
	trace->max = max;
	trace->w_index = 0;
	trace->name = name;
	trace->get = get;
	trace->use_lock = 1;
	spin_lock_init(&trace->lock);
	return trace;
}

void vftrace_free_trace(void *handle)
{
	kfree(handle);
}

#define TRACE_LOCK(trace) \
	do {\
		if (trace->use_lock)\
			spin_lock_irqsave(&trace->lock, flags);\
	} while (0)

#define TRACE_UNLOCK(trace) \
	do {\
		if (trace->use_lock)\
			spin_unlock_irqrestore(&trace->lock, flags);\
	} while (0)

void vftrace_info_in(void *vhandle, struct vframe_s *vf)
{
	struct vf_trace *vftrace = vhandle;
	struct trace_info *info;
	unsigned long flags = 0;
	struct timeval tv;

	if (!vftrace || !vf)
		return;
	do_gettimeofday(&tv);
	TRACE_LOCK(vftrace);
	info = &vftrace->trace_buf[vftrace->w_index];
	info->index = vf->index;
	info->type = vf->type;
	info->vf = vf;
	info->pts = vf->pts;
	info->in_time_us = div64_u64(timeval_to_ns(&tv), 1000);
	info->duration = info->duration;
	vftrace->w_index++;
	vftrace->num++;
	if (vftrace->w_index >= vftrace->max)
		vftrace->w_index = 0;
	TRACE_UNLOCK(vftrace);
}

static void vftrace_dump_trace_info(struct trace_info *info, int i)
{
	pr_info("%d: \tvf:%p:%d\ttype:%x \tpts:%d \td:%d \tt:%lldUs\n",
		i,
		info->vf,
		info->index,
		info->type,
		info->pts,
		info->duration,
		info->in_time_us);
}

void vftrace_dump_trace_infos(void *vhandle)
{
	struct vf_trace *vftrace = vhandle;
	struct trace_info *info;
	unsigned long flags = 0;

	if (!vftrace) {
		pr_info("not trace, trace not enabled!\n");
		return;
	}
	pr_info("start dump %s trace: %s num:%d\n", vftrace->name,
		vftrace->get ? "GET" : "PUT", vftrace->num);
	TRACE_LOCK(vftrace);
	{
		int i;
		int j = vftrace->w_index;

		if (vftrace->num < vftrace->max) {
			j -= vftrace->num;
			if (j < 0)
				j += vftrace->max;
		}
		for (i = 0; i < vftrace->max; i++) {
			info = &vftrace->trace_buf[j];
			vftrace_dump_trace_info(info, i);
			j++;
			if (j >= vftrace->max)
				j = 0;
		}
	}
	TRACE_UNLOCK(vftrace);
}
