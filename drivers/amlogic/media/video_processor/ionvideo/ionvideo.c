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

#define DEBUG
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "ionvideo.h"
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/platform_device.h>
#include <linux/amlogic/major.h>

#define IONVIDEO_MODULE_NAME "ionvideo"

#define IONVIDEO_VERSION "1.0"
#define RECEIVER_NAME "ionvideo"
#define IONVIDEO_DEVICE_NAME   "ionvideo"

#define V4L2_CID_USER_AMLOGIC_IONVIDEO_BASE  (V4L2_CID_USER_BASE + 0x1100)
#define SCALE_4K_TO_1080P 1
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
			IONVID_DBG("ionvideo: set freerun mode\n");
		}
	}
	return 0;
}

static int vidioc_g_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct v4l2_amlogic_parm *ap
		= (struct v4l2_amlogic_parm *)parms->parm.raw_data;

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(ap, 0, sizeof(struct v4l2_amlogic_parm));
	*ap = dev->am_parm;

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
				struct v4l2_buffer *buf)
{

	struct vframe_s *vf;
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
		ret = ppmgr2_process(vf, &dev->ppmgr2_dev, buf->index);
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
#if SCALE_4K_TO_1080P
		if (vf->width > dev->width || vf->height > dev->height) {
			if (vf->width*dev->height >= dev->width*vf->height) {
				dev->ppmgr2_dev.dst_width = dev->width;
				dev->ppmgr2_dev.dst_height = vf->height
					* dev->width / vf->width;
		} else {
				dev->ppmgr2_dev.dst_width = vf->width
					* dev->height / vf->height;
				dev->ppmgr2_dev.dst_height = dev->height;
			}
		}
#endif
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
		ret = ppmgr2_process(vf, &dev->ppmgr2_dev, buf->index);
		if (ret) {
			vf_put(vf, dev->vf_receiver_name);
			return ret;
		}
		if (dev->wait_ge2d_timeout) {
			IONVID_INFO("ppmgr2_process timeout\n");
			dev->wait_ge2d_timeout = false;
			return -EAGAIN;
		}
		dev->wait_ge2d_timeout = false;
		videoc_omx_compute_pts(dev, vf);
		buf->timecode.frames = 0;
		dev->am_parm.signal_type = vf->signal_type;
		dev->am_parm.master_display_colour
				= vf->prop.master_display_colour;
		vf_put(vf, dev->vf_receiver_name);
		buf->timestamp.tv_sec = dev->pts >> 32;
		buf->timestamp.tv_usec = dev->pts & 0xFFFFFFFF;
		buf->timecode.type = dev->ppmgr2_dev.dst_width;
		buf->timecode.flags = dev->ppmgr2_dev.dst_height;
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
	struct v4l2_buffer *buf;
	struct vframe_s *vf;

	int w, h;

	dprintk(dev, 4, "Thread tick\n");
	/* video seekTo clear list */

	if (!dev)
		return;

	if (dev->active_state == ION_INACTIVE_REQ) {
		dev->active_state = ION_INACTIVE;
		complete(&dev->inactive_done);
	}

	if (dev->active_state == ION_INACTIVE)
		return;

	if (dev->receiver_register == 0)
		return;

	dev->wait_ge2d_timeout = false;
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
	mutex_lock(&dev->mutex_input);
	buf = v4l2q_peek(&dev->input_queue);
	if (buf == NULL) {
		dprintk(dev, 3, "No active queue to serve\n");
		mutex_unlock(&dev->mutex_input);
		schedule_timeout_interruptible(msecs_to_jiffies(20));
		return;
	}
	mutex_unlock(&dev->mutex_input);
	/* Fill buffer */
	if (ionvideo_fillbuff(dev, buf))
		return;
	mutex_lock(&dev->mutex_input);
	buf = v4l2q_pop(&dev->input_queue);
	mutex_unlock(&dev->mutex_input);
	dev->vf_wait_cnt = 0;

	mutex_lock(&dev->mutex_output);
	v4l2q_push(&dev->output_queue, buf);
	dma_q->vb_ready++;
	mutex_unlock(&dev->mutex_output);
	dprintk(dev, 4, "[%p/%d] done\n", buf, buf->index);
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

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;

	dev->is_omx_video_started = 1;
	dma_q->vb_ready = 0;

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

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ionvideo_dev *dev = video_drvdata(file);
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
	v4l2q_init(&dev->input_queue, IONVIDEO_POOL_SIZE + 1,
			&dev->ionvideo_input_queue[0]);
	v4l2q_init(&dev->output_queue, IONVIDEO_POOL_SIZE + 1,
			&dev->ionvideo_output_queue[0]);
	return 0;
}

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

	v4l2q_init(&dev->input_queue, IONVIDEO_POOL_SIZE + 1,
			&dev->ionvideo_input_queue[0]);
	v4l2q_init(&dev->output_queue, IONVIDEO_POOL_SIZE + 1,
			&dev->ionvideo_output_queue[0]);

	//dprintk(dev, 2, "vidioc_open\n");
	IONVID_DBG("ionvideo open\n");
	init_waitqueue_head(&dev->wq);
	return 0;
}

static int vidioc_close(struct file *file)
{
	struct ionvideo_dev *dev = video_drvdata(file);

	struct ionvideo_dmaqueue *dma_q = &dev->vidq;

	if (dma_q->kthread)
		vidioc_streamoff(file, NULL, 0);

	IONVID_DBG("vidioc_close!!!!\n");
	ppmgr2_release(&(dev->ppmgr2_dev));
	//dprintk(dev, 2, "vidioc_close\n");
	IONVID_DBG("vidioc_close\n");
	if (dev->fd_num > 0)
		dev->fd_num--;

	dev->once_record = 0;
	return 0;
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

	if (dev->freerun_mode == 0) {
		if (dev->c_width == 0 || dev->c_height == 0)
			return -EINVAL;

		f->fmt.pix.width = dev->c_width;
		f->fmt.pix.height = dev->c_height;

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

	int ret = vidioc_try_fmt_vid_cap(file, priv, f);

	if (ret < 0)
		return ret;

	dev->fmt = get_format(f);
	dev->pixelsize = dev->fmt->depth;
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	if ((dev->width == 0) || (dev->height == 0))
		IONVID_ERR("ion buffer w/h info is invalid!!!!!!!!!!!\n");

	return 0;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	struct ppmgr2_device *ppmgr2_dev = &(dev->ppmgr2_dev);
	struct dma_buf *dbuf = NULL;
	struct ion_buffer *buffer = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	void *phy_addr = NULL;

	dev->ionvideo_input[p->index] = *p;

	mutex_lock(&dev->mutex_input);
	v4l2q_push(&dev->input_queue, &(dev->ionvideo_input[p->index]));
	mutex_unlock(&dev->mutex_input);

	if (!ppmgr2_dev->phy_addr[p->index]) {
		dbuf = dma_buf_get(p->m.fd);
		buffer = dbuf->priv;
		table = buffer->sg_table;
		page = sg_page(table->sgl);
		phy_addr = (void *)PFN_PHYS(page_to_pfn(page));
		dma_buf_put(dbuf);

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
	return 0;
}

/*
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
*/

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ionvideo_dev *dev = video_drvdata(file);
	struct ionvideo_dmaqueue *dma_q = &dev->vidq;
	struct v4l2_buffer *out_put = NULL;

/*
	if (dev->freerun_mode == 0)
		return vidioc_synchronization_dqbuf(file, priv, p);
*/

	mutex_lock(&dev->mutex_output);
	out_put = v4l2q_pop(&dev->output_queue);

	if (out_put != NULL) {
		dma_q->vb_ready--;
		*p = *out_put;
	} else {
		mutex_unlock(&dev->mutex_output);
		return -EAGAIN;
	}
	mutex_unlock(&dev->mutex_output);
	return 0;
}

#define NUM_INPUTS 10

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static const struct v4l2_file_operations ionvideo_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vidioc_open,
	.release = vidioc_close,
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
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_g_parm = vidioc_g_parm,
};

static const struct video_device ionvideo_template = {
	.name = "ionvideo",
	.fops = &ionvideo_v4l2_fops,
	.ioctl_ops = &ionvideo_ioctl_ops,
	.release = video_device_release,
};

/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
/* struct vb2_dc_conf * ionvideo_dma_ctx = NULL; */
static int ionvideo_v4l2_release(void)
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
	struct ionvideo_dev *dev = (struct ionvideo_dev *)private_data;
	int timeout = 0;
	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		dev->receiver_register = 0;
		dev->is_omx_video_started = 0;
		if (dev->active_state == ION_ACTIVE) {
			/*if player killed thread may have exit.*/
			dev->active_state = ION_INACTIVE_REQ;
			dev->wait_ge2d_timeout = false;
			timeout = wait_for_completion_timeout(
				&dev->inactive_done,
				msecs_to_jiffies(200));
			if (!timeout) {
				IONVID_INFO("unreg:wait timeout\n");
				dev->wait_ge2d_timeout = true;
			}
		}

		/*tsync_avevent(VIDEO_STOP, 0);*/
		pr_info("unreg:ionvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		dev->is_omx_video_started = 1;
		dev->ppmgr2_dev.interlaced_num = 0;
		dev->active_state = ION_ACTIVE;
		init_completion(&dev->inactive_done);
		dev->receiver_register = 1;
		pr_info("reg:ionvideo\n");
	} else if (type == VFRAME_EVENT_PROVIDER_QUREY_STATE) {
		if (dev->vf_wait_cnt > 1)
			return RECEIVER_INACTIVE;
		return RECEIVER_ACTIVE;
	} else if (type == VFRAME_EVENT_PROVIDER_FR_HINT) {
#ifdef CONFIG_AM_VOUT
		if ((data != NULL) && (ionvideo_seek_flag == 0))
			set_vframe_rate_hint((unsigned long)(data));
#endif
	} else if (type == VFRAME_EVENT_PROVIDER_FR_END_HINT) {
#ifdef CONFIG_AM_VOUT
		if (ionvideo_seek_flag == 0) {
			set_vframe_rate_end_hint();
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

	mutex_init(&dev->mutex);

	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	dev->vidq.pdev = dev;

	vfd = &dev->vdev;
	*vfd = ionvideo_template;
	vfd->dev_debug = debug;
	vfd->v4l2_dev = &dev->v4l2_dev;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
				inst + video_nr_base);
	if (ret < 0)
		goto unreg_dev;

	video_set_drvdata(vfd, dev);

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

	mutex_init(&dev->mutex_input);
	mutex_init(&dev->mutex_output);

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

		if (dev->inst == *inst) {
			*receiver_name = dev->vf_receiver_name;
			ionvideo_devlist_unlock(flags);
			return 0;
		}
	}

	ionvideo_devlist_unlock(flags);

	return -ENODEV;
}
EXPORT_SYMBOL(ionvideo_assign_map);

int ionvideo_alloc_map(int *inst)
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
			pr_debug("ionvideo_release_map %d OK\n", dev->inst);
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

static int ionvideo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ionvideo_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long ionvideo_ioctl(struct file *file,
			    unsigned int cmd,
			    ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case IONVIDEO_IOCTL_ALLOC_ID:{
			u32 ionvideo_id = 0;

			ret = ionvideo_alloc_map(&ionvideo_id);
			if (ret != 0)
				break;
			put_user(ionvideo_id, (u32 __user *)argp);
		}
		break;
	case IONVIDEO_IOCTL_FREE_ID:{
			u32 ionvideo_id;

			get_user(ionvideo_id, (u32 __user *)argp);
			ionvideo_release_map(ionvideo_id);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long ionvideo_compat_ioctl(struct file *file,
				unsigned int cmd,
				ulong arg)
{
	long ret = 0;

	ret = ionvideo_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif
static const struct file_operations ionvideo_fops = {
	.owner = THIS_MODULE,
	.open = ionvideo_open,
	.release = ionvideo_release,
	.unlocked_ioctl = ionvideo_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ionvideo_compat_ioctl,
#endif
	.poll = NULL,
};
/* This routine allocates from 1 to n_devs virtual drivers.
 * The real maximum number of virtual drivers will depend on how many drivers
 * will succeed. This is limited to the maximum number of devices that
 * videodev supports, which is equal to VIDEO_NUM_DEVICES.
 */
static int ionvideo_driver_probe(struct platform_device *pdev)
{
	int ret = 0, i;

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
	ionvideo_v4l2_release();
	return 0;
}

static const struct of_device_id ionvideo_dt_match[] = {
	{
		.compatible = "amlogic, ionvideo",
	},
	{}
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
	int ret = -1;
	struct device *devp;

	ret = class_register(&ionvideo_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(IONVIDEO_MAJOR, "ionvideo", &ionvideo_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for ionvideo device\n");
		goto error1;
	}

	devp = device_create(&ionvideo_class,
				NULL,
				MKDEV(IONVIDEO_MAJOR, 0),
				NULL,
				IONVIDEO_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create ionvideo device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	if (platform_driver_register(&ionvideo_drv)) {
		pr_err("Failed to register ionvideo driver\n");
		return -ENODEV;
	}
	return 0;

error1:
	unregister_chrdev(IONVIDEO_MAJOR, "ionvideo");
	class_unregister(&ionvideo_class);
	return ret;

}

static void __exit ionvideo_exit(void)
{
	platform_driver_unregister(&ionvideo_drv);
	device_destroy(&ionvideo_class, MKDEV(IONVIDEO_MAJOR, 0));
	unregister_chrdev(IONVIDEO_MAJOR, IONVIDEO_DEVICE_NAME);
	class_unregister(&ionvideo_class);
}

MODULE_DESCRIPTION("Video Technology Magazine Ion Video Capture Board");
MODULE_AUTHOR("Amlogic, Shuai Cao<shuai.cao@amlogic.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(IONVIDEO_VERSION);

module_init(ionvideo_init);
module_exit(ionvideo_exit);
