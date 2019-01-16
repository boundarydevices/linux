/*
 * drivers/amlogic/media/video_processor/videosync/videosync.h
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

#ifndef _VIDEOSYNC_H
#define _VIDEOSYNC_H

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
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include "vfp.h"
#include <linux/spinlock.h>

#define VIDEOSYNC_S_COUNT 1

#define VIDEOSYNC_ACTIVE 0
#define VIDEOSYNC_INACTIVE_REQ 1
#define VIDEOSYNC_INACTIVE 2

struct videosync_priv_s {
	int vp_id; /* reserved */
	struct videosync_s *dev_s;
};

struct videosync_dev {
	struct videosync_s *video_prov;
	struct task_struct *kthread;
	struct completion thread_active;
	struct mutex vp_mutex;
	spinlock_t dev_s_num_slock;
	u32 active_dev_s_num;
};
extern bool omx_secret_mode;

#define VIDEOSYNC_IOC_MAGIC  'P'
#define VIDEOSYNC_IOC_ALLOC_ID   _IOR(VIDEOSYNC_IOC_MAGIC, 0x00, int)
#define VIDEOSYNC_IOC_FREE_ID    _IOW(VIDEOSYNC_IOC_MAGIC, 0x01, int)
#define VIDEOSYNC_IOC_SET_FREERUN_MODE    _IOW(VIDEOSYNC_IOC_MAGIC, 0x02, int)
#define VIDEOSYNC_IOC_GET_FREERUN_MODE    _IOR(VIDEOSYNC_IOC_MAGIC, 0x03, int)
#define VIDEOSYNC_IOC_SET_OMX_VPTS _IOW(VIDEOSYNC_IOC_MAGIC, 0x04, unsigned int)
#define VIDEOSYNC_IOC_GET_OMX_VPTS _IOR(VIDEOSYNC_IOC_MAGIC, 0x05, unsigned int)
#define VIDEOSYNC_IOC_GET_OMX_VERSION \
	_IOR(VIDEOSYNC_IOC_MAGIC, 0x06, unsigned int)
#define VIDEOSYNC_IOC_SET_OMX_ZORDER \
	_IOW(VIDEOSYNC_IOC_MAGIC, 0x07, unsigned int)

#define VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE 32
#define VIDEOSYNC_S_POOL_SIZE 16
#define VIDEOSYNC_VF_NAME_SIZE 32

extern int videosync_assign_map(char **receiver_name, int *inst);

struct videosync_buffer_states {
	int buf_ready_num;
	int buf_queued_num;
	int total_num;
};

struct videosync_operations_s {
	struct vframe_s *(*peek)(void *op_arg);
	struct vframe_s *(*get)(void *op_arg);
	void (*put)(struct vframe_s *, void *op_arg);
	int (*event_cb)(int type, void *data, void *private_data);
	int (*buffer_states)(struct videosync_buffer_states *states,
		void *op_arg);
};

struct display_area {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};
struct videosync_s {
	void *dev;
	int index;
	int fd_num;
	char vf_receiver_name[VIDEOSYNC_S_VF_RECEIVER_NAME_SIZE];
	int inst;
	bool mapped;
	bool receiver_register;
	struct vframe_receiver_s vp_vf_receiver;
	struct vfq_s queued_q;
	struct vfq_s ready_q;
	struct vframe_s *videosync_pool_queued[VIDEOSYNC_S_POOL_SIZE + 1];
	struct vframe_s *videosync_pool_ready[VIDEOSYNC_S_POOL_SIZE + 1];
	int active_state;
	const struct videosync_operations_s *ops;
	struct completion inactive_done;
	struct vframe_s *cur_dispbuf;
	spinlock_t timestamp_lock;
	struct mutex omx_mutex;
	u32 system_time_up;
	u32 system_time;
	u32 system_time_scale_remainder;
	u32 omx_pts;
	u32 vpts_ref;
	u32 video_frame_repeat_count;
	u32 freerun_mode;
	u32 first_frame_toggled;
	u32 get_frame_count;
	u32 put_frame_count;
	void *op_arg;
	char *name;
	struct display_area rect;
	u32 zorder;
	struct vframe_provider_s video_vf_prov;
	char vf_provider_name[VIDEOSYNC_VF_NAME_SIZE];
};


#endif

