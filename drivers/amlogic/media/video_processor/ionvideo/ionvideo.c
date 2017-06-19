/*
 * drivers/amlogic/media/video_processor/ionvideo/ionvideo.c
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

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "ionvideo.h"
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/platform_device.h>

#define IONVIDEO_MODULE_NAME "ionvideo"

#define IONVIDEO_VERSION "1.0"
#define RECEIVER_NAME "ionvideo"

#define V4L2_CID_USER_AMLOGIC_IONVIDEO_BASE  (V4L2_CID_USER_BASE + 0x1100)

static struct mutex ppmgr2_ge2d_canvas_mutex;

static unsigned int video_nr_base = 13;
module_param(video_nr_base, uint, 0644);
MODULE_PARM_DESC(video_nr_base, "videoX start number, 13 is the base nr");

static int scaling_rate = 100;
static int ionvideo_seek_flag;

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned int n_devs = 9;
#else
static unsigned int n_devs = 1;
#endif

module_param(n_devs, uint, 0644);
MODULE_PARM_DESC(n_devs, "number of video devices to create");

static unsigned int debug;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static unsigned int freerun_mode = 1;
module_param(freerun_mode, uint, 0664);
MODULE_PARM_DESC(freerun_mode, "av synchronization");

static const struct ionvideo_fmt formats[] = {
	{.name = "RGB32 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB32, /* argb */
	.depth = 32, },

	{.name = "RGB565 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
	.depth = 16, },

	{.name = "RGB24 (LE)",
	.fourcc = V4L2_PIX_FMT_RGB24, /* rgb */
	.depth = 24, },

	{.name = "RGB24 (BE)",
	.fourcc = V4L2_PIX_FMT_BGR24, /* bgr */
	.depth = 24, },

	{.name = "12  Y/CbCr 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV12,
	.depth = 12, },

	{.name = "12  Y/CrCb 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV21,
	.depth = 12, },

	{.name = "YUV420P",
	.fourcc = V4L2_PIX_FMT_YUV420,
	.depth = 12, },

	{.name = "YVU420P",
	.fourcc = V4L2_PIX_FMT_YVU420,
	.depth = 12, }
};

/* supported controls
 * static struct v4l2_queryctrl ionvideo_node_qctrl[] =
 * {
 *  {
 *		.id = V4L2_CID_USER_AMLOGIC_IONVIDEO_BASE,
 *		.type = V4L2_CTRL_TYPE_INTEGER,
 *		.name = "freerun_mode",
 *		.minimum = 0,
 *		.maximum = 1,
 *		.step = 1,
 *		.default_value = 1,
 *		.flags = V4L2_CTRL_FLAG_SLIDER,
 *	}
 *	};
 */

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	if (ctrl->id == V4L2_CID_USER_AMLOGIC_IONVIDEO_BASE) {
		if (ctrl->value) {
			dev->freerun_mode = 1;
			IONVID_INFO("ionvideo: set freerun mode\n");
		}
	}
	return 0;
}

static const struct ionvideo_fmt *__get_format(u32 pixelformat)
{
	const struct ionvideo_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

static const struct ionvideo_fmt *get_format(struct v4l2_format *f)
{
	return __get_format(f->fmt.pix.pixelformat);
}

static DEFINE_SPINLOCK(devlist_lock);
static unsigned long ionvideo_devlist_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&devlist_lock, flags);
	return flags;
}

static void ionvideo_devlist_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&devlist_lock, flags);
}

static LIST_HEAD(ionvideo_devlist);

static DEFINE_SPINLOCK(ion_states_lock);
static int ionvideo_vf_get_states(struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;
	struct vframe_provider_s *vfp;

	vfp = vf_get_provider(RECEIVER_NAME);
	spin_lock_irqsave(&ion_states_lock, flags);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(states, vfp->op_arg);

	spin_unlock_irqrestore(&ion_states_lock, flags);
	return ret;
}

/* ------------------------------------------------------------------
 * DMA and thread functions
 * ------------------------------------------------------------------
 */
unsigned int get_ionvideo_debug(void)
{
	return debug;
}
EXPORT_SYMBOL(get_ionvideo_debug);

static void videoc_omx_compute_pts(struct ionvideo_dev *dev,
					struct vframe_s *vf)
{
	if (dev->pts == 0) {
		if (dev->is_omx_video_started == 0) {
			dev->pts = dev->last_pts_us64
				+ (DUR2PTS(vf->duration) * 100 / 9);
			if ((vf->type & 0x1) == VIDTYPE_INTERLACE)
				dev->pts += (DUR2PTS(vf->duration) * 100 / 9);
		}
	}
	if (dev->is_omx_video_started)
		dev->is_omx_video_started = 0;

	dev->last_pts_us64 = dev->pts;
}

#if 0
static int canvas_is_valid(struct ionvideo_dev *dev, int index)
{
	struct ppmgr2_device *ppd;
	int ret = 0;

	if (!dev)
		return 0;
	ppd = &dev->ppmgr2_dev;
	if (!ppd)
		return 0;
	if (ppd->canvas_id[index] >= 0)
		ret = 1;
	if (ppd->canvas_id[index] < 0)
		IONVID_ERR("cancas %d is invalid\n", ppd->canvas_id[index]);
	return ret;
}
#endif

static int ionvideo_fillbuff(struct ionvideo_dev *dev,
				struct ionvideo_buffer *buf)
{

	struct vframe_s *vf;
#ifdef IONVIDEO_DEBUG
	struct vb2_buffer *vb = &(buf->vb);
#endif
	int ret = 0;
	/* ------------------------------------------------------- */
#if 0
	if (!canvas_is_valid(dev, vb->v4l2_buf.index))
		return -1;
#endif
	vf = vf_get(dev->vf_receiver_name);
	if (!vf)
		return -EAGAIN;
	if (vf->flag & VFRAME_FLAG_SWITCHING_FENSE) {
		vf_put(vf, dev->vf_receiver_name);
		return -EAGAIN;
	}

	if (vf && dev->once_record == 1) {
		dev->once_record = 0;
		if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) == 0x3)
			dev->ppmgr2_dev.bottom_first = 1;
		else
			dev->ppmgr2_dev.bottom_first = 0;

	}
	if (dev->freerun_mode == 0) {
		if ((vf->type & 0x1) == VIDTYPE_INTERLACE) {
			if ((dev->ppmgr2_dev.bottom_first
				&& (vf->type & 0x2)) || (dev
				->ppmgr2_dev
				.bottom_first
				== 0
				&& ((vf
				->type
				& 0x2)
				== 0))) {
				buf->pts = vf->pts;
				buf->duration = vf->duration;
			}
		} else {
			buf->pts = vf->pts;
			buf->duration = vf->duration;
		}
#ifdef IONVIDEO_DEBUG
		ret = ppmgr2_process(vf, &dev->ppmgr2_dev, vb->v4l2_buf.index);
#endif
		if (ret) {
			vf_put(vf, dev->vf_receiver_name);
			return ret;
		}
		vf_put(vf, dev->vf_receiver_name);
	} else {
		if ((vf->type & 0x1) == VIDTYPE_INTERLACE) {
			if ((dev->ppmgr2_dev.bottom_first
				&& (vf->type & 0x2)) || (dev
				->ppmgr2_dev
				.bottom_first
				== 0
				&& ((vf
				->type
				& 0x2)
				== 0)))
				dev->pts = vf->pts_us64;
		} else
			dev->pts = vf->pts_us64;
		/* for omx AdaptivePlayback */
		if (vf->width <= ((dev->width + 31) & (~31)))
			dev->ppmgr2_dev.dst_width = vf->width;
		if (vf->height <= dev->height)
			dev->ppmgr2_dev.dst_height = vf->height;

		if ((dev->ppmgr2_dev.dst_width >= 1920) && (dev->ppmgr2_dev
			.dst_height
			>= 1080)
			&& (vf->type & VIDTYPE_INTERLACE)) {
			dev->ppmgr2_dev.dst_width = dev->ppmgr2_dev.dst_width
				* scaling_rate
							/ 100;
			dev->ppmgr2_dev.dst_height = dev->ppmgr2_dev.dst_height
				* scaling_rate
							/ 100;
		}
#ifdef IONVIDEO_DEBUG
		ret = ppmgr2_process(vf, &dev->ppmgr2_dev, vb->v4l2_buf.index);
#endif
		if (ret) {
			vf_put(vf, dev->vf_receiver_name);
			return ret;
		}
		videoc_omx_compute_pts(dev, vf);
		vf_put(vf, dev->vf_receiver_name);
#ifdef IONVIDEO_DEBUG
		buf->vb.v4l2_buf.timestamp.tv_sec = dev->pts >> 32;
		buf->vb.v4l2_buf.timestamp.tv_usec = dev->pts & 0xFFFFFFFF;
		buf->vb.v4l2_buf.timecode.type = dev->ppmgr2_dev.dst_width;
		buf->vb.v4l2_buf.timecode.flags = dev->ppmgr2_dev.dst_height;
#endif
	}
	/* ------------------------------------------------------- */
	return 0;
}

static int ionvideo_size_changed(struct ionvideo_dev *dev, int aw, int ah)
{

	v4l_bound_align_image(&aw, 48, MAX_WIDTH, 5, &ah, 32, MAX_HEIGHT, 0, 0);
	dev->c_width = aw;
	dev->c_height = ah;
	if (aw != dev->width || ah != dev->height) {
		dprintk(dev, 2, "Video frame size changed w:%d h:%d\n", aw, ah);
		return -EAGAIN;
	}
	return 0;
}

static void ionvideo_thread_tick(struct ionvideo_dev *dev)
{
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	struct ionvideo_buffer *buf;
	unsigned long flags = 0;
	struct vframe_s *vf;

	int w, h;

	dprintk(dev, 4, "Thread tick\n");
	/* video seekTo clear list */

	if (!dev)
		return;

	vf = vf_peek(dev->vf_receiver_name);
	if (!vf) {
		dev->vf_wait_cnt++;
		/* msleep(5); */
		usleep_range(1000, 2000);
		return;
	}
	dev->ppmgr2_dev.dst_width = dev->width;
	dev->ppmgr2_dev.dst_height = dev->height;
	if ((vf->width >= 1920) && (vf->height >= 1080)
		&& (vf->type & VIDTYPE_INTERLACE)) {
		dev->ppmgr2_dev.dst_width = vf->width * scaling_rate / 100;
		dev->ppmgr2_dev.dst_height = vf->height * scaling_rate / 100;
		w = dev->ppmgr2_dev.dst_width;
		h = dev->ppmgr2_dev.dst_height;
	} else {
		w = vf->width;
		h = vf->height;
	}
	if (dev->freerun_mode == 0 && ionvideo_size_changed(dev, w, h)) {
		/* msleep(10); */
		usleep_range(4000, 5000);
		return;
	}
	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 3, "No active queue to serve\n");
		spin_unlock_irqrestore(&dev->slock, flags);
		schedule_timeout_interruptible(msecs_to_jiffies(20));
		return;
	}
	buf = list_entry(dma_q->active.next, struct ionvideo_buffer, list);
	spin_unlock_irqrestore(&dev->slock, flags);
	/* Fill buffer */
	if (ionvideo_fillbuff(dev, buf))
		return;
	dev->vf_wait_cnt = 0;

	spin_lock_irqsave(&dev->slock, flags);
	list_del(&buf->list);
	spin_unlock_irqrestore(&dev->slock, flags);
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
	dma_q->vb_ready++;
#ifdef IONVIDEO_DEBUG
	dprintk(dev, 4, "[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
#endif
}

#define frames_to_ms(frames)					\
((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void ionvideo_sleep(struct ionvideo_dev *dev)
{
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	/* int timeout; */
	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 4, "%s dma_q=0x%08lx\n", __func__, (unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	/* timeout = msecs_to_jiffies(frames_to_ms(1)); */

	ionvideo_thread_tick(dev);

	/* schedule_timeout_interruptible(timeout); */

stop_task: remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int ionvideo_thread(void *data)
{
	struct ionvideo_dev *dev = data;

	dprintk(dev, 2, "thread started\n");

	set_freezable();

	dev->thread_stopped = 0;
	for (;;) {
		ionvideo_sleep(dev);

		if (kthread_should_stop())
			break;
	}
	dev->thread_stopped = 1;
	wake_up_interruptible(&dev->wq);
	dprintk(dev, 2, "thread: exit\n");
	return 0;
}

static int ionvideo_start_generating(struct ionvideo_dev *dev)
{
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;

	dev->is_omx_video_started = 1;

	dprintk(dev, 2, "%s\n", __func__);

	/* Resets frame counters */
	dev->ms = 0;
	/* dev->jiffies = jiffies; */

	init_waitqueue_head(&dev->wq);

	/* dma_q->ini_jiffies = jiffies; */
	dma_q->kthread = kthread_run(ionvideo_thread, dev, dev->v4l2_dev.name);

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 2, "returning from %s\n", __func__);
	return 0;
}

static void ionvideo_stop_generating(struct ionvideo_dev *dev)
{
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;

	dprintk(dev, 2, "%s\n", __func__);

	/* shutdown control thread */
	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
	wait_event_interruptible_timeout(dev->wq,
			dev->thread_stopped != 0, HZ/5);
	/*
	 * Typical driver might need to wait here until dma engine stops.
	 * In this case we can abort imiedetly, so it's just a noop.
	 */

	/* Release all active buffers */
	while (!list_empty(&dma_q->active)) {
		struct ionvideo_buffer *buf;

		buf = list_entry(dma_q->active.next, struct ionvideo_buffer,
					list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
#ifdef IONVIDEO_DEBUG
		dprintk(dev, 2, "[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
#endif
	}
}
/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */
#ifdef IONVIDEO_DEBUG
static int queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
			unsigned int *nbuffers, unsigned int *nplanes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vq);
	unsigned long size;

	if (fmt)
		size = fmt->fmt.pix.sizeimage;
	else
		size = (dev->width * dev->height * dev->pixelsize) >> 3;

	if (size == 0)
		return -EINVAL;

	if (*nbuffers == 0)
		*nbuffers = 32;

	while (size * *nbuffers > vid_limit * MAX_WIDTH * MAX_HEIGHT)
		(*nbuffers)--;

	*nplanes = 1;

	sizes[0] = size;

	/*
	 * videobuf2-vmalloc allocator is context-less so no need to set
	 * alloc_ctxs array.
	 */

	dprintk(dev, 2, "%s, count=%d, size=%ld\n", __func__, *nbuffers, size);

	return 0;
}
#endif

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct ionvideo_buffer *buf = container_of(vb, struct ionvideo_buffer,
							vb);
	unsigned long size;
#ifdef IONVIDEO_DEBUG
	dprintk(dev, 2, "%s, field=%d\n", __func__, vb->v4l2_buf.field);
#endif
	WARN_ON(dev->fmt == NULL);

	/*
	 * Theses properties only change when queue is idle, see s_fmt.
	 * The below checks should not be performed here, on each
	 * buffer_prepare (i.e. on each qbuf). Most of the code in this function
	 * should thus be moved to buffer_init and s_fmt.
	 */
	if (dev->width < 48 || dev->width > MAX_WIDTH
		|| dev->height < 32 || dev->height > MAX_HEIGHT)
		return -EINVAL;

	size = (dev->width * dev->height * dev->pixelsize) >> 3;
	if (vb2_plane_size(vb, 0) < size) {
		dprintk(dev, 1, "%s data will not fit into plane (%lu < %lu)\n",
			__func__, vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb, 0, size);

	buf->fmt = dev->fmt;

	return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct ionvideo_buffer *buf = container_of(vb, struct ionvideo_buffer,
							vb);
	struct ionvideo_dmaqueue *vidq = &dev->vidq;
	unsigned long flags = 0;

	dprintk(dev, 2, "%s\n", __func__);

	spin_lock_irqsave(&dev->slock, flags);
	list_add_tail(&buf->list, &vidq->active);
	spin_unlock_irqrestore(&dev->slock, flags);
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vq);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;

	dev->is_actived = 1;
	dma_q->vb_ready = 0;
	dprintk(dev, 2, "%s\n", __func__);
	return ionvideo_start_generating(dev);
}

/* abort streaming and wait for last buffer */
static void stop_streaming(struct vb2_queue *vq)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vq);

	dev->is_actived = 0;
	dprintk(dev, 2, "%s\n", __func__);
	ionvideo_stop_generating(dev);
}

static void ionvideo_lock(struct vb2_queue *vq)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vq);

	mutex_lock(&dev->mutex);
}

static void ionvideo_unlock(struct vb2_queue *vq)
{
	struct ionvideo_dev *dev = vb2_get_drv_priv(vq);

	mutex_unlock(&dev->mutex);
}

static const struct vb2_ops ionvideo_video_qops = {
#ifdef IONVIDEO_DEBUG
	.queue_setup = queue_setup,
#endif
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.start_streaming = start_streaming,
	.stop_streaming = stop_streaming,
	.wait_prepare = ionvideo_unlock,
	.wait_finish = ionvideo_lock,
};

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_open(struct file *file)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	if (dev->fd_num > 0 || ppmgr2_init(&(dev->ppmgr2_dev)) < 0) {
		pr_err("vidioc_open error\n");
		return -EBUSY;
	}

	dev->fd_num++;
	dev->pts = 0;
	dev->c_width = 0;
	dev->c_height = 0;
	dev->once_record = 1;
	dev->ppmgr2_dev.bottom_first = 0;
	dev->skip_frames = 0;
	dev->vf_wait_cnt = 0;
	/*for libplayer osd*/
	dev->freerun_mode = freerun_mode;
	dprintk(dev, 2, "vidioc_open\n");
	IONVID_INFO("ionvideo open\n");
	init_waitqueue_head(&dev->wq);
	return v4l2_fh_open(file);
}

static int vidioc_release(struct file *file)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	ionvideo_stop_generating(dev);
	IONVID_INFO("ionvideo_stop_generating!!!!\n");
	ppmgr2_release(&(dev->ppmgr2_dev));
	dprintk(dev, 2, "vidioc_release\n");
	IONVID_INFO("ionvideo release\n");
	if (dev->fd_num > 0)
		dev->fd_num--;

	dev->once_record = 0;
	return vb2_fop_release(file);
}

static int vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	strcpy(cap->driver, "ionvideo");
	strcpy(cap->card, "ionvideo");
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
			dev->v4l2_dev.name);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
				| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_fmtdesc *f)
{
	const struct ionvideo_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct vb2_queue *q = &dev->vb_vidq;
	int ret = 0;
	unsigned long flags;

	if (dev->freerun_mode == 0) {
		if (dev->c_width == 0 || dev->c_height == 0)
			return -EINVAL;

		f->fmt.pix.width = dev->c_width;
		f->fmt.pix.height = dev->c_height;
		spin_lock_irqsave(&q->done_lock, flags);
		ret = list_empty(&q->done_list);
		spin_unlock_irqrestore(&q->done_lock, flags);
		if (!ret)
			return -EAGAIN;

	} else {
		f->fmt.pix.width = dev->width;
		f->fmt.pix.height = dev->height;
	}
	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	f->fmt.pix.pixelformat = dev->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * dev->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (dev->fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	const struct ionvideo_fmt *fmt;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) unknown.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	v4l_bound_align_image(&f->fmt.pix.width, 48, MAX_WIDTH, 4,
				&f->fmt.pix.height, 32, MAX_HEIGHT, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	if (fmt->is_yuv)
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	f->fmt.pix.priv = 0;
	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct vb2_queue *q = &dev->vb_vidq;

	int ret = vidioc_try_fmt_vid_cap(file, priv, f);

	if (ret < 0)
		return ret;

	if (vb2_is_busy(q)) {
		dprintk(dev, 1, "%s device busy\n", __func__);
		return -EBUSY;
	}
	dev->fmt = get_format(f);
	dev->pixelsize = dev->fmt->depth;
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	if ((dev->width == 0) || (dev->height == 0))
		IONVID_ERR("ion buffer w/h info is invalid!!!!!!!!!!!\n");

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	static const struct v4l2_frmsize_stepwise sizes = {
		48, MAX_WIDTH, 4, 32, MAX_HEIGHT, 1};
	int i;

	if (fsize->index)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (formats[i].fourcc == fsize->pixel_format)
			break;
	if (i == ARRAY_SIZE(formats))
		return -EINVAL;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise = sizes;
	return 0;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	struct ppmgr2_device *ppmgr2_dev = &(dev->ppmgr2_dev);
	int ret = 0;

	p->length = 0;
	ret = vb2_ioctl_qbuf(file, priv, p);
	if (ret != 0)
		return ret;


	if (!ppmgr2_dev->phy_addr[p->index]) {
		struct vb2_buffer *vb;
		struct vb2_queue *q;
		void *phy_addr = NULL;

		q = dev->vdev.queue;
		vb = q->bufs[p->index];
		phy_addr = vb2_plane_cookie(vb, 0);
		ppmgr2_dev->phy_addr[p->index] = phy_addr;
		ppmgr2_dev->dst_buffer_width = ALIGN(dev->width, 32);
		ppmgr2_dev->dst_buffer_height = dev->height;
		ppmgr2_dev->ge2d_fmt =  v4l_to_ge2d_format(dev->fmt->fourcc);
		if (phy_addr) {
#if 0
			ret = ppmgr2_canvas_config(ppmgr2_dev, dev->width,
							dev->height,
							dev->fmt->fourcc,
							phy_addr, p->index);
#endif
		} else {
			return -ENOMEM;
		}
	}
	wake_up_interruptible(&dma_q->wq);
	return ret;
}

static int vidioc_synchronization_dqbuf(struct file *file, void *priv,
					struct v4l2_buffer *p)
{
	struct vb2_buffer *vb = NULL;
	struct vb2_queue *q;
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_buffer *buf;
	int ret = 0;
	int d = 0;
	unsigned long flags;

	q = dev->vdev.queue;
	if (dev->receiver_register) {
		/* clear the frame buffer queue */
		while (!list_empty(&q->done_list)) {
			ret = vb2_ioctl_dqbuf(file, priv, p);
			if (ret)
				return ret;

			ret = vb2_ioctl_qbuf(file, priv, p);
			if (ret)
				return ret;

		}
		IONVID_INFO("init to clear the done list buffer.done\n");
		dev->receiver_register = 0;
		dev->is_video_started = 0;
		return -EAGAIN;
	}
	spin_lock_irqsave(&q->done_lock, flags);
	if (list_empty(&q->done_list)) {
		spin_unlock_irqrestore(&q->done_lock, flags);
		return -EAGAIN;
	}
	vb = list_first_entry(&q->done_list, struct vb2_buffer,
				done_entry);
	spin_unlock_irqrestore(&q->done_lock, flags);

	buf = container_of(vb, struct ionvideo_buffer, vb);
	if (dev->is_video_started == 0) {
		IONVID_INFO("Execute the VIDEO_START cmd. pts=%llx\n",
			buf->pts);
		tsync_avevent_locked(
			VIDEO_START,
			buf->pts ? buf->pts : timestamp_vpts_get());
		d = 0;
		dev->is_video_started = 1;
	} else {
		if (buf->pts == 0) {
			buf->pts =
				timestamp_vpts_get() + DUR2PTS(
					buf->duration);
		}

		if (abs(timestamp_pcrscr_get() - buf->pts) >
			tsync_vpts_discontinuity_margin()) {
			tsync_avevent_locked(
				VIDEO_TSTAMP_DISCONTINUITY,
				buf->pts);
		} else {
			timestamp_vpts_set(buf->pts);
		}
		d = (buf->pts - timestamp_pcrscr_get());
	}

	if (d > 450) {
		return -EAGAIN;
	} else if (d < -11520) {
		int s = 3;

		while (s--) {
			ret = vb2_ioctl_dqbuf(file, priv, p);
			if (ret)
				return ret;


			if (buf->pts == 0) {
				buf->pts =
					timestamp_vpts_get() + DUR2PTS(
						buf->duration);
			}

			if (abs(timestamp_pcrscr_get() - buf->pts) >
				tsync_vpts_discontinuity_margin()) {
				tsync_avevent_locked(
					VIDEO_TSTAMP_DISCONTINUITY,
					buf->pts);
			} else {
				timestamp_vpts_set(buf->pts);
			}

			if (list_empty(&q->done_list))
				break;
			ret = vb2_ioctl_qbuf(file, priv, p);
			if (ret)
				return ret;
			dev->skip_frames++;
		}
		dprintk(dev, 1, "s:%u\n", dev->skip_frames);
	} else {
		ret = vb2_ioctl_dqbuf(file, priv, p);
		if (ret)
			return ret;

	}
	p->timestamp.tv_sec = 0;
	p->timestamp.tv_usec = timestamp_vpts_get();

	return 0;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	int ret = 0;

	if (dev->freerun_mode == 0)
		return vidioc_synchronization_dqbuf(file, priv, p);

	ret = vb2_ioctl_dqbuf(file, priv, p);
	if (ret == 0)
		dma_q->vb_ready--;
	return ret;
}

#define NUM_INPUTS 10
/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index >= NUM_INPUTS)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	sprintf(inp->name, "Camera %u", inp->index);
	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	*i = dev->input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	if (i >= NUM_INPUTS)
		return -EINVAL;

	if (i == dev->input)
		return 0;

	dev->input = i;
	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static const struct v4l2_file_operations ionvideo_fops = {
	.owner = THIS_MODULE,
	.open = vidioc_open,
	.release = vidioc_release,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,/* V4L2 ioctl handler */
	.mmap = vb2_fop_mmap,
};

static const struct v4l2_ioctl_ops ionvideo_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_enum_input = vidioc_enum_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_s_input = vidioc_s_input,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_log_status = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_s_ctrl = vidioc_s_ctrl,
};

static const struct video_device ionvideo_template = {
	.name = "ionvideo",
	.fops = &ionvideo_fops,
	.ioctl_ops = &ionvideo_ioctl_ops,
	.release = video_device_release_empty,
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
/* struct vb2_dc_conf * ionvideo_dma_ctx = NULL; */
static int ionvideo_release(void)
{
	struct ionvideo_dev *dev;
	struct list_head *list;
	unsigned long flags;

	flags = ionvideo_devlist_lock();

	while (!list_empty(&ionvideo_devlist)) {
		list = ionvideo_devlist.next;
		list_del(list);
		ionvideo_devlist_unlock(flags);

		dev = list_entry(list, struct ionvideo_dev, ionvideo_devlist);

		v4l2_info(&dev->v4l2_dev, "unregistering %s\n",
				video_device_node_name(&dev->vdev));
		video_unregister_device(&dev->vdev);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);

		flags = ionvideo_devlist_lock();
	}
	/* vb2_dma_contig_cleanup_ctx(ionvideo_dma_ctx); */

	ionvideo_devlist_unlock(flags);

	return 0;
}

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
#ifdef CONFIG_AM_VOUT
	char *configured[2];
	char framerate[20] = {0};
#endif
	struct ionvideo_dev *dev = (struct ionvideo_dev *)private_data;

	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		dev->receiver_register = 0;
		dev->is_omx_video_started = 0;
		/*tsync_avevent(VIDEO_STOP, 0);*/
		IONVID_INFO("unreg:ionvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		dev->receiver_register = 1;
		dev->is_omx_video_started = 1;
		dev->ppmgr2_dev.interlaced_num = 0;
		IONVID_INFO("reg:ionvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		if (dev->vf_wait_cnt > 1)
			return RECEIVER_INACTIVE;
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
		if ((data != NULL) && (ionvideo_seek_flag == 0)) {
#ifdef CONFIG_AM_VOUT
			/*set_vframe_rate_hint((unsigned long)(data));*/
			sprintf(framerate, "FRAME_RATE_HINT=%lu",
			(unsigned long)data);
			configured[0] = framerate;
			configured[1] = NULL;
			kobject_uevent_env(&(dev->vdev.dev.kobj),
				KOBJ_CHANGE, configured);
			pr_info("%s: sent uevent %s\n",
					__func__, configured[0]);
#endif
		}
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
#ifdef CONFIG_AM_VOUT
		if (ionvideo_seek_flag == 0) {
			configured[0] = "FRAME_RATE_END_HINT";
			configured[1] = NULL;
			kobject_uevent_env(&(dev->vdev.dev.kobj),
					KOBJ_CHANGE, configured);
			pr_info("%s: sent uevent %s\n",
				__func__, configured[0]);
		}
#endif
	}

	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
	.event_cb = video_receiver_event_fun
};

static int __init ionvideo_create_instance(int inst)
{
	struct ionvideo_dev *dev;
	struct video_device *vfd;
	struct vb2_queue *q;
	int ret;
	unsigned long flags;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
	    "%s-%03d", IONVIDEO_MODULE_NAME, inst);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_dev;

	dev->fmt = &formats[0];
	dev->width = 640;
	dev->height = 480;
	dev->pixelsize = dev->fmt->depth;
	dev->fd_num = 0;
	dev->ionvideo_v4l_num = inst + video_nr_base;
	dev->ppmgr2_dev.ge2d_canvas_mutex = &ppmgr2_ge2d_canvas_mutex;

	/* initialize locks */
	spin_lock_init(&dev->slock);

	/* initialize queue */
	q = &dev->vb_vidq;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF | VB2_READ;
	q->drv_priv = dev;
	q->buf_struct_size = sizeof(struct ionvideo_buffer);
	q->ops = &ionvideo_video_qops;
	q->mem_ops = &vb2_ion_memops;
#ifdef IONVIDEO_DEBUG
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#else
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif

#ifdef IONVIDEO_DEBUG
	ret = vb2_queue_init(q);
	if (ret)
		goto unreg_dev;
#endif

	mutex_init(&dev->mutex);

	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	dev->vidq.pdev = dev;

	vfd = &dev->vdev;
	*vfd = ionvideo_template;
	vfd->dev_debug = debug;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->queue = q;
#ifdef IONVIDEO_DEBUG
	set_bit(V4L2_FL_USE_FH_PRIO, &vfd->flags);
#endif

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &dev->mutex;
	video_set_drvdata(vfd, dev);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
				inst + video_nr_base);
	if (ret < 0)
		goto unreg_dev;

	dev->inst = inst;
	snprintf(dev->vf_receiver_name, ION_VF_RECEIVER_NAME_SIZE,
		(inst == 0) ? RECEIVER_NAME : RECEIVER_NAME ".%x",
		inst & 0xff);

	vf_receiver_init(&dev->video_vf_receiver,
			dev->vf_receiver_name,
			&video_vf_receiver, dev);
	vf_reg_receiver(&dev->video_vf_receiver);
	v4l2_info(&dev->v4l2_dev, "V4L2 device registered as %s\n",
		video_device_node_name(vfd));

	/* add to device list */
	flags = ionvideo_devlist_lock();
	list_add_tail(&dev->ionvideo_devlist, &ionvideo_devlist);
	ionvideo_devlist_unlock(flags);

	return 0;

unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);
	return ret;
}

int ionvideo_assign_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct ionvideo_dev *dev = NULL;
	struct list_head *p;

	flags = ionvideo_devlist_lock();

	list_for_each(p, &ionvideo_devlist) {
		dev = list_entry(p, struct ionvideo_dev, ionvideo_devlist);

		if ((dev->inst == *inst) && (!dev->mapped)) {
			dev->mapped = true;
			*inst = dev->inst;
			*receiver_name = dev->vf_receiver_name;
			ionvideo_devlist_unlock(flags);
			return 0;
		}
	}

	ionvideo_devlist_unlock(flags);

	return -ENODEV;
}

int ionvideo_alloc_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct ionvideo_dev *dev = NULL;
	struct list_head *p;

	flags = ionvideo_devlist_lock();

	list_for_each(p, &ionvideo_devlist) {
		dev = list_entry(p, struct ionvideo_dev, ionvideo_devlist);

		if ((dev->inst >= 0) && (!dev->mapped)) {
			dev->mapped = true;
			*inst = dev->inst;
			*receiver_name = dev->vf_receiver_name;
			ionvideo_devlist_unlock(flags);
			return 0;
		}
	}

	ionvideo_devlist_unlock(flags);
	return -ENODEV;
}

void ionvideo_release_map(int inst)
{
	unsigned long flags;
	struct ionvideo_dev *dev = NULL;
	struct list_head *p;

	flags = ionvideo_devlist_lock();

	list_for_each(p, &ionvideo_devlist) {
		dev = list_entry(p, struct ionvideo_dev, ionvideo_devlist);
		if ((dev->inst == inst) && (dev->mapped)) {
			dev->mapped = false;
			pr_info("ionvideo_release_map %d OK\n", dev->inst);
			break;
		}
	}

	ionvideo_devlist_unlock(flags);
}

static ssize_t vframe_states_show(struct class *class,
					struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct vframe_states states;
	/* unsigned long flags; */

	if (ionvideo_vf_get_states(&states) == 0) {
		ret += sprintf(buf + ret,
			       "vframe_pool_size=%d\n", states.vf_pool_size);

		ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
				states.buf_free_num);
		ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
				states.buf_recycle_num);
		ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
				states.buf_avail_num);
	} else {
		ret += sprintf(buf + ret, "vframe no states\n");
	}

	return ret;
}

static ssize_t scaling_rate_show(struct class *cla,
					struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current scaling rate is %d\n", scaling_rate);
}

static ssize_t scaling_rate_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t size;
	char *endp = NULL;
	unsigned long tmp;
	int ret;

	ret = kstrtoul(buf, 0, &tmp);
	if (ret != 0) {
		IONVID_ERR("ERROR parsing string:%s to unsigned long\n", buf);
		return ret;
	}
	/* scaling_rate = simple_strtoul(buf, &endp, 0); */
	scaling_rate = tmp;
	size = endp - buf;
	return count;
}

static ssize_t scaling_ionvideo_seek_flag_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ionvideo_seek_flag);
}

static ssize_t scaling_ionvideo_seek_flag_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &ionvideo_seek_flag);
	if (r != 0)
		return -EINVAL;
	return count;
}

static struct class_attribute ion_video_class_attrs[] = {
__ATTR_RO(vframe_states),
__ATTR(scaling_rate,
0644,
scaling_rate_show,
scaling_rate_write),
__ATTR(ionvideo_seek_flag,
0644,
scaling_ionvideo_seek_flag_show,
scaling_ionvideo_seek_flag_write),
__ATTR_NULL };
static struct class ionvideo_class = {.name = "ionvideo", .class_attrs =
ion_video_class_attrs, };

/* This routine allocates from 1 to n_devs virtual drivers.
 * The real maximum number of virtual drivers will depend on how many drivers
 * will succeed. This is limited to the maximum number of devices that
 * videodev supports, which is equal to VIDEO_NUM_DEVICES.
 */
static int ionvideo_driver_probe(struct platform_device *pdev)
{
	int ret = 0, i;

	ret = class_register(&ionvideo_class);
	if (ret < 0)
		return ret;
	if (n_devs <= 0)
		n_devs = 1;

	mutex_init(&ppmgr2_ge2d_canvas_mutex);

	for (i = 0; i < n_devs; i++) {
		ret = ionvideo_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		IONVID_ERR("ionvideo: error %d while loading driver\n", ret);
		return ret;
	}

	IONVID_INFO("Video Technology Magazine Ion Video\n");
	IONVID_INFO("Capture Board ver %s successfully loaded\n",
	IONVIDEO_VERSION);

	/* n_devs will reflect the actual number of allocated devices */
	n_devs = i;

	return ret;
}

static int ionvideo_drv_remove(struct platform_device *pdev)
{
	ionvideo_release();
	class_unregister(&ionvideo_class);
	return 0;
}

static const struct of_device_id ionvideo_dt_match[] = {
	{
		.compatible = "amlogic, ionvideo",
	},
};

/* general interface for a linux driver .*/
static struct platform_driver ionvideo_drv = {
.probe = ionvideo_driver_probe,
.remove = ionvideo_drv_remove,
.driver = {
		.name = "ionvideo",
		.owner = THIS_MODULE,
		.of_match_table = ionvideo_dt_match,
	}
};

static int __init ionvideo_init(void)
{
	if (platform_driver_register(&ionvideo_drv)) {
		pr_err("Failed to register ionvideo driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit ionvideo_exit(void)
{
	platform_driver_unregister(&ionvideo_drv);
}

MODULE_DESCRIPTION("Video Technology Magazine Ion Video Capture Board");
MODULE_AUTHOR("Amlogic, Shuai Cao<shuai.cao@amlogic.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(IONVIDEO_VERSION);

module_init(ionvideo_init);
module_exit(ionvideo_exit);
