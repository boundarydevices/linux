/*
 * Copyright 2015 www.starterkit.ru <info@starterkit.ru>
 *
 * Based on:
 * Driver for Intersil|Techwell TW6869 based DVR cards
 * (c) 2011-12 liran <jli11@intersil.com> [Intersil|Techwell China]
 *
 * V4L2 PCI Skeleton Driver
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates.
 * All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "tw6869.h"

/**
 * tw6869 internals
 */
static int tw6869_vch_dma_locked(struct tw6869_dma *dma)
{
	unsigned int vs = tw_read(dma->dev, R8_VIDEO_STATUS(dma->id));

	return TW_VS_LOCK_MASK == (vs & TW_VS_LOCK_MASK);
}

static void tw6869_vch_dma_srst(struct tw6869_dma *dma)
{
	tw_set(dma->dev, R8_AVSRST(dma->id), BIT(ID2SC(dma->id)));
}

static void tw6869_vch_dma_ctrl(struct tw6869_dma *dma)
{
	struct tw6869_vch *vch = container_of(dma, struct tw6869_vch, dma);

	tw_write(dma->dev, R8_BRIGHTNESS_CTRL(dma->id), vch->brightness);
	tw_write(dma->dev, R8_CONTRAST_CTRL(dma->id), vch->contrast);
	tw_write(dma->dev, R8_SHARPNESS_CTRL(dma->id), vch->sharpness);
	tw_write(dma->dev, R8_SAT_U_CTRL(dma->id), vch->saturation);
	tw_write(dma->dev, R8_SAT_V_CTRL(dma->id), vch->saturation);
	tw_write(dma->dev, R8_HUE_CTRL(dma->id), vch->hue);
}

static int to_tw6869_pixformat(unsigned int pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_UYVY:
		return TW_FMT_UYVY;
	case V4L2_PIX_FMT_YUYV:
		return TW_FMT_YUYV;
	case V4L2_PIX_FMT_RGB565:
		return TW_FMT_RGB565;
	default:
		return -EINVAL;
	}
}

static inline int tw_vch_frame_mode(struct tw6869_vch *vch)
{
	return vch->format.height > 288;
}

static void tw6869_vch_dma_frame_isr(struct tw6869_dma *dma)
{
	struct tw6869_vch *vch = container_of(dma, struct tw6869_vch, dma);
	struct tw6869_buf *done = NULL;
	struct tw6869_buf *next = NULL;
	unsigned int i = dma->pb & 0x1;

	spin_lock(&dma->lock);
	if (tw_dma_active(dma) && !list_empty(&vch->buf_list)) {
		next = list_first_entry(&vch->buf_list, struct tw6869_buf, list);
		list_del(&next->list);
		done = dma->buf[i];
		dma->buf[i] = next;
	}
	spin_unlock(&dma->lock);

	if (done && next) {
		tw_write(dma->dev, dma->reg[i], next->dma_addr);
		v4l2_get_timestamp(&done->vb.v4l2_buf.timestamp);
		done->vb.v4l2_buf.sequence = vch->sequence++;
		vb2_buffer_done(&done->vb, VB2_BUF_STATE_DONE);
	} else {
		tw_err(dma->dev, "vch%u NOBUF seq=%u dcount=%u\n",
			ID2CH(dma->id), vch->sequence, ++vch->dcount);
	}

	dma->fld = 0x0;
	dma->pb ^= 0x1;
}

static void tw6869_vch_dma_field_isr(struct tw6869_dma *dma)
{
	struct tw6869_vch *vch = container_of(dma, struct tw6869_vch, dma);
	struct tw6869_buf *done = NULL;
	struct tw6869_buf *next = NULL;
	unsigned int i = ((dma->fld & 0x1) << 1) | (dma->pb & 0x1);

	spin_lock(&dma->lock);
	if (tw_dma_active(dma) && !list_empty(&vch->buf_list)) {
		next = list_first_entry(&vch->buf_list, struct tw6869_buf, list);
		list_del(&next->list);
		done = dma->buf[i];
		dma->buf[i] = next;
	}
	spin_unlock(&dma->lock);

	if (done && next) {
		tw_write(dma->dev, dma->reg[i], next->dma_addr);
		v4l2_get_timestamp(&done->vb.v4l2_buf.timestamp);
		done->vb.v4l2_buf.sequence = vch->sequence++;
		done->vb.v4l2_buf.field = V4L2_FIELD_BOTTOM;
		vb2_buffer_done(&done->vb, VB2_BUF_STATE_DONE);
	} else {
		tw_err(dma->dev, "vch%u NOBUF seq=%u dcount=%u\n",
			ID2CH(dma->id), vch->sequence, ++vch->dcount);
	}

	dma->fld ^= 0x1;
	if (!dma->fld)
		dma->pb ^= 0x1;
}

static unsigned int tw6869_vch_fields_map(struct tw6869_vch *vch)
{
	unsigned int map[15] = {
		0x00000001, 0x00004001, 0x00104001, 0x00404041, 0x01041041,
		0x01104411, 0x01111111, 0x04444445, 0x04511445, 0x05145145,
		0x05151515, 0x05515455, 0x05551555, 0x05555555, 0x15555555
	};
	unsigned int std_625_50[26] = {
		14, 0, 0, 1, 2, 2,  3,  3,  4,  4,  5,  6,  6,
		    7, 7, 8, 9, 9, 10, 10, 11, 12, 12, 13, 13, 14
	};
	unsigned int std_525_60[31] = {
		14, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
		    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 14, 14
	};
	unsigned int i, m;

	if (vch->std & V4L2_STD_625_50) {
		vch->fps = (!vch->fps || vch->fps > 25) ? 25 : vch->fps;
		i = std_625_50[vch->fps];
	} else {
		vch->fps = (!vch->fps || vch->fps > 30) ? 30 : vch->fps;
		i = std_525_60[vch->fps];
	}

	m = map[i];
	if (tw_vch_frame_mode(vch)) {
		m = (m << 2) | (m << 1);
		m = (m & BIT(30)) ? 0 : m;
	}
	return m;
}

static void tw6869_vch_frame_period(struct tw6869_vch *vch,
		struct v4l2_fract *frameperiod)
{
	if (vch->std & V4L2_STD_625_50) {
		frameperiod->numerator = 1;
		frameperiod->denominator = vch->fps;
	} else {
		frameperiod->numerator = 1001;
		frameperiod->denominator = vch->fps * 1000;
	}
}

struct active_window {
	unsigned short hactive;
	unsigned short hdelay;
	unsigned short vactive;
	unsigned short vdelay;
	unsigned short f2vdelay;
};

const struct active_window ntsc_window = {.hactive = 720, .hdelay = 15, .vactive = 240, .vdelay = 22, .f2vdelay = 22};

void setup_window(struct tw6869_dma *dma, struct v4l2_pix_format *pix, const struct active_window *w)
{
	unsigned cfg;
	unsigned scale;

	cfg = (w->hactive >> 8) | ((w->hdelay >> 8) << 2) |
	      ((w->vactive >> 8) << 4) | ((w->vdelay >> 8) << 6);
	tw_write(dma->dev, R8_CROPPING_CONTROL(dma->id), cfg);
	tw_write(dma->dev, R8_VERTICAL_DELAY(dma->id), w->vdelay & 0xff);
	tw_write(dma->dev, R8_VERTICAL_ACTIVE(dma->id), w->vactive & 0xff);
	tw_write(dma->dev, R8_HORIZONTAL_DELAY(dma->id), w->hdelay & 0xff);
	tw_write(dma->dev, R8_HORIZONTAL_ACTIVE(dma->id), w->hactive & 0xff);

	scale = 720 * 256 / pix->width;
	tw_write(dma->dev, R8_SCALING_HIGH(dma->id), ((scale >> 8) & 0x0F) | 0x10);
	tw_write(dma->dev, R8_HORIZONTAL_SCALING(dma->id), scale & 0xFF);

	tw_write(dma->dev, R8_F2CNT(dma->id), w->vdelay == w->f2vdelay ? 0 : 1);
	if (w->vdelay == w->f2vdelay)
		return;

	cfg = (w->hactive >> 8) | ((w->hdelay >> 8) << 2) |
	      ((w->vactive >> 8) << 4) | ((w->f2vdelay >> 8) << 6);
	tw_write(dma->dev, R8_F2CROPPING_CONTROL(dma->id), cfg);
	tw_write(dma->dev, R8_F2VERTICAL_DELAY(dma->id), w->f2vdelay & 0xff);
	tw_write(dma->dev, R8_F2VERTICAL_ACTIVE(dma->id), w->vactive & 0xff);
	tw_write(dma->dev, R8_F2HORIZONTAL_DELAY(dma->id), w->hdelay & 0xff);
	tw_write(dma->dev, R8_F2HORIZONTAL_ACTIVE(dma->id), w->hactive & 0xff);

	tw_write(dma->dev, R8_F2SCALING_HIGH(dma->id), ((scale >> 8) & 0x0F) | 0x10);
	tw_write(dma->dev, R8_F2HORIZONTAL_SCALING(dma->id), scale & 0xFF);
}

static void tw6869_vch_dma_cfg(struct tw6869_dma *dma)
{
	struct tw6869_vch *vch = container_of(dma, struct tw6869_vch, dma);
	struct v4l2_pix_format *pix = &vch->format;
	unsigned int cfg;
	const struct active_window *w = (vch->std & V4L2_STD_625_50) ?
			NULL : &ntsc_window;

	BUG_ON(!pix->width);

	/*
	 * Enable pre-determined output value indicated by CCS when video loss
	 * is detected (blue screen)
	 */
	tw_write(dma->dev, R8_MISC_CONTROL2(dma->id), 0xe7);
	if (w)
		setup_window(dma, pix, w);
	cfg = BIT(31);
	cfg |= ((vch->std & V4L2_STD_625_50 ? 288 : 240) & 0x1FF) << 16;
	cfg |= (pix->width & 0x7FF);
	tw_write(dma->dev, R32_VIDEO_SIZE(dma->id), cfg);

	cfg = 13 + ID2CH(dma->id);
	if (vch->std & V4L2_STD_625_50) {
		tw_set(dma->dev, R32_VIDEO_CONTROL1, BIT(cfg));
		tw_write(dma->dev, R8_VERTICAL_DELAY(dma->id), 0x18);
	} else {
		tw_clear(dma->dev, R32_VIDEO_CONTROL1, BIT(cfg));
		tw_write(dma->dev, R8_VERTICAL_DELAY(dma->id), 0x14);
	}

	cfg = 16 + ID2CH(dma->id) * 2;
	if (tw_vch_frame_mode(vch)) {
		dma->isr = tw6869_vch_dma_frame_isr;
		tw_write_mask(dma->dev, R32_PHASE_REF,
			TW_VDMA_FRAME_MODE << cfg, 0x3 << cfg);
	} else {
		dma->isr = tw6869_vch_dma_field_isr;
		tw_write_mask(dma->dev, R32_PHASE_REF,
			TW_VDMA_FIELD_MODE << cfg, 0x3 << cfg);
	}

	cfg = tw6869_vch_fields_map(vch);
	if (cfg)
		cfg |= BIT(31);
	tw_write(dma->dev, R32_VIDEO_FIELD_CTRL(dma->id), cfg);

	/* SK-TW6869: only input0 is available */
	cfg = (vch->input & 0x0) << 30;
	cfg |= BIT(27) | (ID2SC(dma->id) << 25);
	cfg |= (to_tw6869_pixformat(pix->pixelformat) & 0x7) << 20;
	tw_write(dma->dev, R32_DMA_CHANNEL_CONFIG(dma->id), cfg);

	cfg = ((vch->std & V4L2_STD_625_50 ? 288 : 240) & 0x3FF) << 22;
	cfg |= (pix->bytesperline & 0x7FF) << 11;
	cfg |= (pix->bytesperline & 0x7FF);
	tw_write(dma->dev, R32_VDMA_WHP(dma->id), cfg);
	tw_write(dma->dev, R32_VDMA_F2_WHP(dma->id), cfg);
}

static void tw6869_vch_fill_pix_format(struct tw6869_vch *vch,
		struct v4l2_pix_format *pix)
{
	switch (pix->width) {
	case 320:
	case 360:
	case 480:
	case 640:
		break;
	default:
		pix->width = 720;
	}

	if (vch->std & V4L2_STD_625_50)
		pix->height = (pix->height == 288) ? 288 : 576;
	else
		pix->height = (pix->height == 240) ? 240 : 480;

	pix->field = (pix->height > 288) ?
		V4L2_FIELD_INTERLACED : V4L2_FIELD_BOTTOM;
	pix->colorspace = V4L2_COLORSPACE_SMPTE170M;
	pix->bytesperline = pix->width * 2;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
}

/**
 * Videobuf2 Operations
 */
static int queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
		unsigned int *nbuffers, unsigned int *nplanes,
		unsigned int sizes[], void *alloc_ctxs[])
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vq);

	if (vq->num_buffers + *nbuffers < TW_VBUF_ALLOC)
		*nbuffers = TW_VBUF_ALLOC - vq->num_buffers;

	if (fmt && fmt->fmt.pix.sizeimage < vch->format.sizeimage)
		return -EINVAL;

	*nplanes = 1;
	sizes[0] = fmt ? fmt->fmt.pix.sizeimage : vch->format.sizeimage;
	alloc_ctxs[0] = vch->dma.dev->alloc_ctx;
	return 0;
}

static int buffer_init(struct vb2_buffer *vb)
{
	struct tw6869_buf *buf = container_of(vb, struct tw6869_buf, vb);

#ifdef FSL_QUERYBUF
	if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
		return 0;
#endif

	buf->dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	INIT_LIST_HEAD(&buf->list);
	return 0;
}

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size = vch->format.sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		tw_err(vch->dma.dev, "buffer too small (%lu < %lu)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);
	return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vb->vb2_queue);
	struct tw6869_buf *buf = container_of(vb, struct tw6869_buf, vb);
	unsigned long flags;

	spin_lock_irqsave(&vch->dma.lock, flags);
	list_add_tail(&buf->list, &vch->buf_list);
	spin_unlock_irqrestore(&vch->dma.lock, flags);
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vq);
	struct tw6869_dma *dma = &vch->dma;
	unsigned int buf_max = tw_vch_frame_mode(vch) ? 2 : 4;
	unsigned long flags;
	unsigned int i;

	if (count < buf_max)
		return -ENOBUFS;

	spin_lock_irqsave(&dma->lock, flags);
	for (i = 0; i < TW_BUF_MAX; i++) {
		if (i < buf_max) {
			dma->buf[i] = list_first_entry(&vch->buf_list,
				struct tw6869_buf, list);
			list_del(&dma->buf[i]->list);
		} else {
			dma->buf[i] = NULL;
		}
	}
	tw_dma_set_addr(dma);
	vch->sequence = 0;
	vch->dcount = 0;
	spin_unlock_irqrestore(&dma->lock, flags);

	spin_lock_irqsave(&dma->dev->rlock, flags);
	tw6869_vch_dma_cfg(dma);
	tw_dma_enable(dma);
	spin_unlock_irqrestore(&dma->dev->rlock, flags);

	return 0;
}

static void tw6869_vch_stop(struct tw6869_vch *vch)
{
	struct tw6869_dma *dma = &vch->dma;
	struct tw6869_buf *buf, *node;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&dma->dev->rlock, flags);
	tw_dma_disable(dma);
	spin_unlock_irqrestore(&dma->dev->rlock, flags);

	spin_lock_irqsave(&dma->lock, flags);
	for (i = 0; i < TW_BUF_MAX; i++) {
		if (dma->buf[i])
			vb2_buffer_done(&dma->buf[i]->vb, VB2_BUF_STATE_ERROR);
		dma->buf[i] = NULL;
	}

	list_for_each_entry_safe(buf, node, &vch->buf_list, list) {
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&dma->lock, flags);

	cancel_delayed_work_sync(&dma->hw_on);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static void stop_streaming(struct vb2_queue *vq)
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vq);

	tw6869_vch_stop(vch);
}
#else
static int stop_streaming(struct vb2_queue *vq)
{
	struct tw6869_vch *vch = vb2_get_drv_priv(vq);

	tw6869_vch_stop(vch);
	return 0;
}
#endif

static struct vb2_ops tw6869_qops = {
	.queue_setup		= queue_setup,
	.buf_init		= buffer_init,
	.buf_prepare		= buffer_prepare,
	.buf_queue		= buffer_queue,
	.start_streaming	= start_streaming,
	.stop_streaming		= stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

/**
 * Video ioctls
 */
static int tw6869_querycap(struct file *file, void *priv,
		struct v4l2_capability *cap)
{
	struct tw6869_vch *vch = video_drvdata(file);

#ifdef FSL_QUERYBUF
	strlcpy(cap->driver, "csi_v4l2", sizeof(cap->driver));
#else
	strlcpy(cap->driver, KBUILD_MODNAME, sizeof(cap->driver));
#endif
	snprintf(cap->card, sizeof(cap->card), "tw6869-vch%u",
		ID2CH(vch->dma.id));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "PCI:%s",
		pci_name(vch->dma.dev->pdev));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int tw6869_try_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct tw6869_vch *vch = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int ret;

	ret = to_tw6869_pixformat(pix->pixelformat);
	if (ret < 0)
		return ret;

	tw6869_vch_fill_pix_format(vch, pix);
	return 0;
}

static int tw6869_s_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct tw6869_vch *vch = video_drvdata(file);
	int ret;

	ret = tw6869_try_fmt_vid_cap(file, priv, f);
	if (ret)
		return ret;

	if (vb2_is_busy(&vch->queue))
		return -EBUSY;

	vch->format = f->fmt.pix;
	return 0;
}

static int tw6869_g_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct tw6869_vch *vch = video_drvdata(file);

	f->fmt.pix = vch->format;
	return 0;
}

static int tw6869_enum_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_fmtdesc *f)
{
	if (f->index > 2)
		return -EINVAL;

	switch (f->index) {
	case 1:
		strlcpy(f->description, "4:2:2, packed, YUYV",
			sizeof(f->description));
		f->pixelformat = V4L2_PIX_FMT_YUYV;
		break;
	case 2:
		strlcpy(f->description, "16 bpp RGB, le",
			sizeof(f->description));
		f->pixelformat = V4L2_PIX_FMT_RGB565;
		break;
	default:
		strlcpy(f->description, "4:2:2, packed, UYVY",
			sizeof(f->description));
		f->pixelformat = V4L2_PIX_FMT_UYVY;
	}
	f->flags = 0;
	return 0;
}

static int tw6869_querystd(struct file *file, void *priv, v4l2_std_id *std)
{
	struct tw6869_vch *vch = video_drvdata(file);
	unsigned int std_now;

	std_now = tw_read(vch->dma.dev, R8_STANDARD_SELECTION(vch->dma.id));
	std_now = (std_now >> 4) & 0x07;

	switch (std_now) {
	case TW_STD_PAL_M:
	case TW_STD_PAL_60:
	case TW_STD_NTSC_M:
	case TW_STD_NTSC_443:
		*std = V4L2_STD_525_60;
		break;
	case TW_STD_PAL:
	case TW_STD_PAL_CN:
	case TW_STD_SECAM:
		*std = V4L2_STD_625_50;
		break;
	default:
		*std = V4L2_STD_UNKNOWN;
	}
	return 0;
}

static int tw6869_s_std(struct file *file, void *priv, v4l2_std_id std)
{
	struct tw6869_vch *vch = video_drvdata(file);
	v4l2_std_id new_std = (std & V4L2_STD_625_50) ?
				V4L2_STD_625_50 : V4L2_STD_525_60;

	if (new_std == vch->std)
		return 0;

	if (vb2_is_busy(&vch->queue))
		return -EBUSY;

	vch->std = new_std;
	vch->fps = (new_std & V4L2_STD_625_50) ? 25 : 30;
	tw6869_vch_fill_pix_format(vch, &vch->format);
	return 0;
}

static int tw6869_g_std(struct file *file, void *priv, v4l2_std_id *std)
{
	struct tw6869_vch *vch = video_drvdata(file);
	v4l2_std_id new_std = 0;

	tw6869_querystd(file, priv, &new_std);
	if (new_std)
		tw6869_s_std(file, priv, new_std);

	vch->fps = (vch->std & V4L2_STD_625_50) ? 25 : 30;
	*std = vch->std;
	return 0;
}

static int tw6869_enum_input(struct file *file, void *priv,
		struct v4l2_input *i)
{
	if (i->index >= TW_VIN_MAX)
		return -EINVAL;

	i->type = V4L2_INPUT_TYPE_CAMERA;
	i->std = V4L2_STD_ALL;
	snprintf(i->name, sizeof(i->name), "Camera %d", i->index);
	return 0;
}

static int tw6869_s_input(struct file *file, void *priv, unsigned int i)
{
	struct tw6869_vch *vch = video_drvdata(file);

	if (i >= TW_VIN_MAX)
		return -EINVAL;

	vch->input = i;
	return 0;
}

static int tw6869_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct tw6869_vch *vch = video_drvdata(file);

	*i = vch->input;
	return 0;
}

static int tw6869_g_parm(struct file *file, void *priv,
		struct v4l2_streamparm *sp)
{
	struct tw6869_vch *vch = video_drvdata(file);
	struct v4l2_captureparm *cp = &sp->parm.capture;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(*cp));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	tw6869_vch_frame_period(vch, &cp->timeperframe);
	return 0;
}

static int tw6869_s_parm(struct file *file, void *priv,
		struct v4l2_streamparm *sp)
{
	struct tw6869_vch *vch = video_drvdata(file);
	unsigned int denominator = sp->parm.capture.timeperframe.denominator;
	unsigned int numerator = sp->parm.capture.timeperframe.numerator;
	unsigned int fps;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (vb2_is_streaming(&vch->queue))
		return -EBUSY;

	fps = (!numerator || !denominator) ? 0 : denominator / numerator;
	if (vch->std & V4L2_STD_625_50)
		vch->fps = (!fps || fps > 25) ? 25 : fps;
	else
		vch->fps = (!fps || fps > 30) ? 30 : fps;

	return tw6869_g_parm(file, priv, sp);
}

static int tw6869_enum_framesizes(struct file *file, void *priv,
		struct v4l2_frmsizeenum *fsize)
{
	struct tw6869_vch *vch = video_drvdata(file);
	unsigned int height = (vch->std & V4L2_STD_625_50) ? 576 : 480;

	if (fsize->index > 5 ||
		to_tw6869_pixformat(fsize->pixel_format) < 0)
		return -EINVAL;

	switch (fsize->index) {
	case 1:
		fsize->discrete.width = 720;
		fsize->discrete.height = height / 2;
		break;
	case 2:
		fsize->discrete.width = 640;
		fsize->discrete.height = height / 2;
		break;
	case 3:
		fsize->discrete.width = 480;
		fsize->discrete.height = height / 2;
		break;
	case 4:
		fsize->discrete.width = 360;
		fsize->discrete.height = height / 2;
		break;
	case 5:
		fsize->discrete.width = 320;
		fsize->discrete.height = height / 2;
		break;
	default: /* Frame mode */
		fsize->discrete.width = 720;
		fsize->discrete.height = height;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	return 0;
}

static int tw6869_enum_frameintervals(struct file *file, void *priv,
		struct v4l2_frmivalenum *fival)
{
	struct tw6869_vch *vch = video_drvdata(file);

	if (fival->index != 0 ||
		to_tw6869_pixformat(fival->pixel_format) < 0)
		return -EINVAL;

	tw6869_vch_frame_period(vch, &fival->discrete);
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	return 0;
}

#ifdef FSL_G_CHIP_IDENT
static int fsl_g_chip_ident(struct file *file, void *priv,
		struct v4l2_dbg_chip_ident *chip)
{
	memset(chip, 0, sizeof(*chip));
	strlcpy(chip->match.name, "vadc", sizeof(chip->match.name));
	return 0;
}
#endif

#ifdef FSL_QUERYBUF
static int fsl_querybuf(struct file *file, void *priv,
		struct v4l2_buffer *b)
{
	struct tw6869_vch *vch = video_drvdata(file);
	dma_addr_t userptr_dma_addr = b->m.userptr;
	struct vb2_buffer *vb;
	struct tw6869_buf *buf;
	int ret;

	ret = vb2_querybuf(&vch->queue, b);
	if (ret)
		return ret;

	vb = vch->queue.bufs[b->index];

	switch (b->memory) {
	case V4L2_MEMORY_USERPTR:
		if (!userptr_dma_addr) {
			tw_err(vch->dma.dev,
				"m.userptr shall contain physical address\n");
			return -EINVAL;
		}
		buf = container_of(vb, struct tw6869_buf, vb);
		buf->dma_addr = userptr_dma_addr;
		break;
	case V4L2_MEMORY_MMAP:
		if (b->flags & V4L2_BUF_FLAG_MAPPED) /* return physical address */
			b->m.offset = vb2_dma_contig_plane_dma_addr(vb, 0);
		break;
	}

	return 0;
}
#endif

/**
 * The control handler
 */
static int tw6869_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tw6869_vch *vch =
		container_of(ctrl->handler, struct tw6869_vch, hdl);
	struct tw6869_dma *dma = &vch->dma;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&dma->lock, flags);
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		vch->brightness = ctrl->val;
		tw_write(dma->dev, R8_BRIGHTNESS_CTRL(dma->id), vch->brightness);
		break;
	case V4L2_CID_CONTRAST:
		vch->contrast = ctrl->val;
		tw_write(dma->dev, R8_CONTRAST_CTRL(dma->id), vch->contrast);
		break;
	case V4L2_CID_SHARPNESS:
		vch->sharpness = (ctrl->val & 0x0F) | 0x10;
		tw_write(dma->dev, R8_SHARPNESS_CTRL(dma->id), vch->sharpness);
		break;
	case V4L2_CID_SATURATION:
		vch->saturation = ctrl->val;
		tw_write(dma->dev, R8_SAT_U_CTRL(dma->id), vch->saturation);
		tw_write(dma->dev, R8_SAT_V_CTRL(dma->id), vch->saturation);
		break;
	case V4L2_CID_HUE:
		vch->hue = ctrl->val;
		tw_write(dma->dev, R8_HUE_CTRL(dma->id), vch->hue);
		break;
	default:
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&dma->lock, flags);

	return ret;
}

/**
 * File operations for the device
 */
static const struct v4l2_ctrl_ops tw6869_ctrl_ops = {
	.s_ctrl = tw6869_s_ctrl,
};

static const struct v4l2_ioctl_ops tw6869_ioctl_ops = {
	.vidioc_querycap = tw6869_querycap,
	.vidioc_try_fmt_vid_cap = tw6869_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = tw6869_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = tw6869_g_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap = tw6869_enum_fmt_vid_cap,

	.vidioc_querystd = tw6869_querystd,
	.vidioc_s_std = tw6869_s_std,
	.vidioc_g_std = tw6869_g_std,

	.vidioc_enum_input = tw6869_enum_input,
	.vidioc_s_input = tw6869_s_input,
	.vidioc_g_input = tw6869_g_input,

	.vidioc_g_parm = tw6869_g_parm,
	.vidioc_s_parm = tw6869_s_parm,

	.vidioc_enum_framesizes = tw6869_enum_framesizes,
	.vidioc_enum_frameintervals = tw6869_enum_frameintervals,

#ifdef FSL_G_CHIP_IDENT
	.vidioc_g_chip_ident = fsl_g_chip_ident,
#endif

#ifdef FSL_QUERYBUF
	.vidioc_querybuf = fsl_querybuf,
#else
	.vidioc_querybuf = vb2_ioctl_querybuf,
#endif
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_log_status = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_file_operations tw6869_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};

static int tw6869_vch_register(struct tw6869_vch *vch)
{
	struct v4l2_ctrl_handler *hdl = &vch->hdl;
	struct vb2_queue *q = &vch->queue;
	struct video_device *vdev = &vch->vdev;
	int ret;

	/* Add the controls */
	v4l2_ctrl_handler_init(hdl, 5);
	v4l2_ctrl_new_std(hdl, &tw6869_ctrl_ops,
		  V4L2_CID_BRIGHTNESS, -128, 127, 1, 0);
	v4l2_ctrl_new_std(hdl, &tw6869_ctrl_ops,
		  V4L2_CID_CONTRAST, 0, 255, 1, 100);
	v4l2_ctrl_new_std(hdl, &tw6869_ctrl_ops,
		  V4L2_CID_SHARPNESS, 0, 15, 1, 1);
	v4l2_ctrl_new_std(hdl, &tw6869_ctrl_ops,
		  V4L2_CID_SATURATION, 0, 255, 1, 128);
	v4l2_ctrl_new_std(hdl, &tw6869_ctrl_ops,
		  V4L2_CID_HUE, -128, 127, 1, 0);
	if (hdl->error) {
		ret = hdl->error;
		return ret;
	}
	v4l2_ctrl_handler_setup(hdl);

	/* Default settings */
	vch->fps = 30;
	vch->std = V4L2_STD_525_60;
	vch->format.pixelformat = V4L2_PIX_FMT_UYVY;
	tw6869_vch_fill_pix_format(vch, &vch->format);

	INIT_LIST_HEAD(&vch->buf_list);
	mutex_init(&vch->mlock);

	/* Initialize the vb2 queue */
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = vch;
	q->buf_struct_size = sizeof(struct tw6869_buf);
	q->ops = &tw6869_qops;
	q->mem_ops = &vb2_dma_contig_memops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	q->min_buffers_needed = TW_BUF_MAX;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#else
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
	q->gfp_flags = __GFP_DMA32;
	q->lock = &vch->mlock;
	ret = vb2_queue_init(q);
	if (ret)
		goto vid_error;

	/* Initialize the video_device structure */
	strlcpy(vdev->name, KBUILD_MODNAME, sizeof(vdev->name));
	vdev->release = video_device_release_empty;
	vdev->fops = &tw6869_fops,
	vdev->ioctl_ops = &tw6869_ioctl_ops,
	vdev->lock = &vch->mlock;
	vdev->queue = q;
	vdev->v4l2_dev = &vch->dma.dev->v4l2_dev;
	vdev->ctrl_handler = hdl;
	vdev->tvnorms = V4L2_STD_ALL;
	video_set_drvdata(vdev, vch);
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret == 0)
		return 0;

vid_error:
	v4l2_ctrl_handler_free(hdl);
	return ret;
}

void tw6869_video_unregister(struct tw6869_dev *dev)
{
	unsigned int i;

	/* Reset and disable all DMA channels */
	tw_write(dev, R32_DMA_CMD, 0x0);
	tw_write(dev, R32_DMA_CHANNEL_ENABLE, 0x0);

	if (!dev->alloc_ctx)
		return;

	if (dev->vch_max > TW_CH_MAX)
		dev->vch_max = TW_CH_MAX;

	for (i = 0; i < dev->vch_max; i++) {
		struct tw6869_vch *vch = &dev->vch[ID2CH(i)];

		video_unregister_device(&vch->vdev);
		v4l2_ctrl_handler_free(&vch->hdl);
	}

	v4l2_device_unregister(&dev->v4l2_dev);
	vb2_dma_contig_cleanup_ctx(dev->alloc_ctx);
	dev->alloc_ctx = NULL;
}

int tw6869_video_register(struct tw6869_dev *dev)
{
	struct pci_dev *pdev = dev->pdev;
	unsigned int i;
	int ret;

	/* Initialize the top-level structure */
	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		return ret;

	dev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(dev->alloc_ctx)) {
		ret = PTR_ERR(dev->alloc_ctx);
		tw_err(dev, "can't allocate buffer context\n");
		v4l2_device_unregister(&dev->v4l2_dev);
		dev->alloc_ctx = NULL;
		return ret;
	}

	if (dev->vch_max > TW_CH_MAX)
		dev->vch_max = TW_CH_MAX;

	for (i = 0; i < dev->vch_max; i++) {
		struct tw6869_vch *vch = &dev->vch[ID2CH(i)];

		ret = tw6869_vch_register(vch);
		if (ret) {
			tw_err(dev, "failed to register vch%u\n", i);
			dev->vch_max = i;
			tw6869_video_unregister(dev);
			return ret;
		}

		vch->dma.locked = tw6869_vch_dma_locked;
		vch->dma.srst = tw6869_vch_dma_srst;
		vch->dma.ctrl = tw6869_vch_dma_ctrl;
		vch->dma.cfg = tw6869_vch_dma_cfg;
		vch->dma.isr = tw6869_vch_dma_frame_isr;
	}
	return 0;
}
