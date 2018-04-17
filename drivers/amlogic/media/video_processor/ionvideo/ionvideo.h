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
#include <linux/amlogic/cpu_version.h>

#include <linux/mm.h>
/* #include <mach/mod_gate.h> */

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>

#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

#define IONVIDEO_POOL_SIZE 16

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

/* v4l2_amlogic_parm must < u8[200] */
struct v4l2_amlogic_parm {
		u32	signal_type;
		struct vframe_master_display_colour_s
		 master_display_colour;
	};

struct v4l2q_s {
	int rp;
	int wp;
	int size;
	int pre_rp;
	int pre_wp;
	struct v4l2_buffer **pool;
};

static inline void v4l2q_lookup_start(struct v4l2q_s *q)
{
	q->pre_rp = q->rp;
	q->pre_wp = q->wp;
}
static inline void v4l2q_lookup_end(struct v4l2q_s *q)
{
	q->rp = q->pre_rp;
	q->wp = q->pre_wp;
}

static inline void v4l2q_init(struct v4l2q_s *q, u32 size,
		struct v4l2_buffer **pool)
{
	q->rp = q->wp = 0;
	q->size = size;
	q->pool = pool;
}

static inline bool v4l2q_empty(struct v4l2q_s *q)
{
	return q->rp == q->wp;
}

static inline void v4l2q_push(struct v4l2q_s *q, struct v4l2_buffer *vf)
{
	int wp = q->wp;

	/*ToDo*/
	smp_mb();

	q->pool[wp] = vf;

	/*ToDo*/
	smp_wmb();

	q->wp = (wp == (q->size - 1)) ? 0 : (wp + 1);
}

static inline struct v4l2_buffer *v4l2q_pop(struct v4l2q_s *q)
{
	struct v4l2_buffer *vf;
	int rp;

	if (v4l2q_empty(q))
		return NULL;

	rp = q->rp;

	/*ToDo*/
	smp_rmb();

	vf = q->pool[rp];

	/*ToDo*/
	smp_mb();

	q->rp = (rp == (q->size - 1)) ? 0 : (rp + 1);

	return vf;
}

static inline struct v4l2_buffer *v4l2q_peek(struct v4l2q_s *q)
{
	return (v4l2q_empty(q)) ? NULL : q->pool[q->rp];
}

static inline int v4l2q_level(struct v4l2q_s *q)
{
	int level = q->wp - q->rp;

	if (level < 0)
		level += q->size;

	return level;
}

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

#define ION_ACTIVE 0
#define ION_INACTIVE_REQ 1
#define ION_INACTIVE 2

struct ion_buffer {
	struct kref ref;
	union {
		struct rb_node node;
		struct list_head list;
	};
	struct ion_device *dev;
	struct ion_heap *heap;
	unsigned long flags;
	unsigned long private_flags;
	size_t size;
	void *priv_virt;
	struct mutex lock;
	int kmap_cnt;
	void *vaddr;
	int dmap_cnt;
	struct sg_table *sg_table;
	struct page **pages;
	struct list_head vmas;
	/* used to track orphaned buffers */
	int handle_count;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
};

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
	int active_state;
	struct completion inactive_done;

	struct v4l2q_s input_queue;
	struct v4l2q_s output_queue;
	struct v4l2_buffer *ionvideo_input_queue[IONVIDEO_POOL_SIZE + 1];
	struct v4l2_buffer *ionvideo_output_queue[IONVIDEO_POOL_SIZE + 1];
	struct mutex mutex_input;
	struct mutex mutex_output;
	struct v4l2_buffer ionvideo_input[IONVIDEO_POOL_SIZE + 1];
	bool wait_ge2d_timeout;
	struct v4l2_amlogic_parm am_parm;
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

#define IONVIDEO_IOC_MAGIC  'I'
#define IONVIDEO_IOCTL_ALLOC_ID   _IOW(IONVIDEO_IOC_MAGIC, 0x00, int)
#define IONVIDEO_IOCTL_FREE_ID    _IOW(IONVIDEO_IOC_MAGIC, 0x01, int)

#endif
