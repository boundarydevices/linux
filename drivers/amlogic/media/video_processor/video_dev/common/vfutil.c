/*
 * drivers/amlogic/media/video_processor/video_dev/common/vfutil.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
/* #include <linux/fiq_bridge.h> */
#include <linux/fs.h>
/* #include <mach/am_regs.h> */

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>

static struct vframe_provider_s *vfp;
static DEFINE_SPINLOCK(lock);

void v4l_reg_provider(struct vframe_provider_s *prov)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (vfp)
		vf_unreg_provider(vfp);

	vfp = prov;
	spin_unlock_irqrestore(&lock, flags);
}

void v4l_unreg_provider(void)
{
	ulong flags;
	/* int deinterlace_mode = get_deinterlace_mode(); */

	spin_lock_irqsave(&lock, flags);

	vfp = NULL;

	/* if (deinterlace_mode == 2) { */
	/* disable_pre_deinterlace(); */
	/* } */
	/* printk("-----%s------\n",__func__); */
	spin_unlock_irqrestore(&lock, flags);
}

const struct vframe_provider_s *v4l_get_vfp(void)
{
	return vfp;
}
