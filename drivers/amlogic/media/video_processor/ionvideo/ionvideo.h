/*
 * drivers/amlogic/media/video_processor/ionvideo/ionvideo.h
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

#ifndef _IONVIDEO_H
#define _IONVIDEO_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <media/videobuf2-core.h>

#include <linux/mm.h>
/* #include <mach/mod_gate.h> */

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>

#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include "videobuf2-ion.h"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

#define IONVID_INFO(fmt, args...) pr_info("ionvid: info: "fmt"", ## args)
#define IONVID_DBG(fmt, args...) pr_debug("ionvid: dbg: "fmt"", ## args)
#define IONVID_ERR(fmt, args...) pr_err("ionvid: err: "fmt"", ## args)

#define DUR2PTS(x) ((x) - ((x) >> 4))

#define dprintk(dev, level, fmt, arg...)                    \
v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

#define ppmgr2_printk(level, fmt, arg...)                   \
do {                                                    \
	if (get_ionvideo_debug() >= level)                  \
		pr_debug("ppmgr2-dev: " fmt, ## arg);  \
} while (0)

#define PPMGR2_CANVAS_INDEX_SRC (PPMGR2_CANVAS_INDEX + 3)

/* ------------------------------------------------------------------
 * Basic structures
 * ------------------------------------------------------------------
 */

struct ionvideo_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	u8 depth;
	bool is_yuv;
};

/* buffer for one video frame */
struct ionvideo_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer vb;
	struct list_head list;
	const struct ionvideo_fmt *fmt;
	u64 pts;
	u32 duration;
};

struct ionvideo_dmaqueue {
	struct list_head active;

	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
	/* Counters to control fps rate */
	int vb_ready;
	struct ionvideo_dev *pdev;
};

struct ppmgr2_device {
	int dst_width;
	int dst_height;
	int dst_buffer_width;
	int dst_buffer_height;
	int ge2d_fmt;
	int canvas_id[PPMGR2_MAX_CANVAS];
	void *phy_addr[PPMGR2_MAX_CANVAS];
	int phy_size;

	struct ge2d_context_s *context;
	struct config_para_ex_s ge2d_config;

	int angle;
	int mirror;
	int paint_mode;
	int interlaced_num;
	int bottom_first;

	struct mutex *ge2d_canvas_mutex;
};

#define ION_VF_RECEIVER_NAME_SIZE 32

struct ionvideo_dev {
	struct list_head ionvideo_devlist;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	int fd_num;
	int ionvideo_v4l_num;

	spinlock_t slock;
	struct mutex mutex;

	struct ionvideo_dmaqueue vidq;

	/* Several counters */
	unsigned int ms;
	unsigned long jiffies;

	/* Input Number */
	int input;

	/* video capture */
	const struct ionvideo_fmt *fmt;
	unsigned int width, height;
	unsigned int c_width, c_height;
	struct vb2_queue vb_vidq;
	unsigned int field_count;

	unsigned int pixelsize;

	struct ppmgr2_device ppmgr2_dev;
	struct vframe_receiver_s video_vf_receiver;
	u64 pts;
	u8 receiver_register;
	u8 is_video_started;
	u32 skip;
	int once_record;
	u8 is_omx_video_started;
	int is_actived;
	u64 last_pts_us64;
	unsigned int freerun_mode;
	unsigned int skip_frames;

	wait_queue_head_t wq;

	char vf_receiver_name[ION_VF_RECEIVER_NAME_SIZE];
	int inst;
	bool mapped;
	bool thread_stopped;
	int vf_wait_cnt;
};

unsigned int get_ionvideo_debug(void);

int ppmgr2_init(struct ppmgr2_device *ppd);
int ppmgr2_canvas_config(struct ppmgr2_device *ppd, int index);
int ppmgr2_process(struct vframe_s *vf, struct ppmgr2_device *ppd, int index);
int ppmgr2_top_process(struct vframe_s *vf, struct ppmgr2_device *ppd,
			int index);
int ppmgr2_bottom_process(struct vframe_s *vf, struct ppmgr2_device *ppd,
				int index);
void ppmgr2_release(struct ppmgr2_device *ppd);
void ppmgr2_set_angle(struct ppmgr2_device *ppd, int angle);
void ppmgr2_set_mirror(struct ppmgr2_device *ppd, int mirror);
void ppmgr2_set_paint_mode(struct ppmgr2_device *ppd, int paint_mode);
int v4l_to_ge2d_format(int v4l2_format);

#endif
