/*
 * drivers/amlogic/media/video_processor/video_dev/amlvideo.h
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

#ifndef AMLVIDEO_H_
#define AMLVIDEO_H_


#define AMLVIDEO_POOL_SIZE 16

struct vivi_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	int depth;
};

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct vivi_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct vivi_fmt *fmt;
};

struct vivi_dmaqueue {
	struct list_head active;

	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
};

static LIST_HEAD(vivi_devlist);

#define AMLVIDEO_VF_NAME_SIZE 32

struct vivi_dev {
	struct list_head vivi_devlist;
	struct v4l2_device v4l2_dev;

	spinlock_t slock;
	struct mutex mutex;
	int users;

	/* various device info */
	struct video_device *vfd;

	struct vivi_dmaqueue vidq;

	struct videobuf_res_privdata *res;
	struct vfq_s q_ready;
	u8 first_frame;
	u64 last_pts_us64;
	struct vframe_s *vf;
	struct vframe_s *amlvideo_pool_ready[AMLVIDEO_POOL_SIZE + 1];
	int index;
	struct mutex vfpMutex;
	int amlvideo_v4l_num;
	char vf_receiver_name[AMLVIDEO_VF_NAME_SIZE];
	char vf_provider_name[AMLVIDEO_VF_NAME_SIZE];
	int inst;
	struct vframe_provider_s video_vf_prov;
	struct vframe_receiver_s video_vf_recv;
};

struct vivi_fh {
	struct vivi_dev *dev;

	/* video capture */
	struct vivi_fmt *fmt;
	unsigned int width, height;
	struct videobuf_queue vb_vidq;
	unsigned int is_streamed_on;

	enum v4l2_buf_type type;
};

extern bool omx_secret_mode;
extern void get_ppmgr_buf_info(char **start, unsigned int *size);

#endif /* AMLVIDEO_H_ */
