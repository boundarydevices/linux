/*
 *    VSI V4L2 decoder entry.
 *
 *    Copyright (c) 2019, VeriSilicon Inc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License, version 2, as
 *    published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License version 2 for more details.
 *
 *    You may obtain a copy of the GNU General Public License
 *    Version 2 at the following locations:
 *    https://opensource.org/licenses/gpl-2.0.php
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/videodev2.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#include <linux/delay.h>
#include <linux/version.h>
#include "vsi-v4l2-priv.h"

static int vsi_dec_querycap(
	struct file *file,
	void *priv,
	struct v4l2_capability *cap)
{
	struct vsi_v4l2_dev_info *hwinfo;

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	hwinfo = vsiv4l2_get_hwinfo();
	if (hwinfo->decformat == 0)
		return -ENODEV;
	strlcpy(cap->driver, "vsi_v4l2", sizeof("vsi_v4l2"));
	strlcpy(cap->card, "vsi_v4l2dec", sizeof("vsi_v4l2dec"));
	strlcpy(cap->bus_info, "platform:vsi_v4l2dec", sizeof("platform:vsi_v4l2dec"));

	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vsi_dec_reqbufs(
	struct file *filp,
	void *priv,
	struct v4l2_requestbuffers *p)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);
	int ret;
	struct vb2_queue *q;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(p->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(p->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;
	ret = vb2_reqbufs(q, p);
	pr_debug("ctx %lx:%s:%d ask for %d buffer, got %d:%d", ctx->ctxid, __func__, p->type, p->count, q->num_buffers, ret);
	if (ret == 0) {
		print_queinfo(q);
		if (p->count == 0 && binputqueue(p->type)) {
			p->capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP | V4L2_BUF_CAP_SUPPORTS_USERPTR | V4L2_BUF_CAP_SUPPORTS_DMABUF;
			//ctx->status = VSI_STATUS_INIT;
		}
	}
	return ret;
}

/*choose input source of index*/
static int vsi_dec_s_input(struct file *file, void *priv, unsigned int index)
{
	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	return 0;
}

static int vsi_dec_s_parm(struct file *filp, void *priv, struct v4l2_streamparm *parm)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	pr_debug("%s:%d:%d", __func__, parm->parm.output.timeperframe.numerator,
		parm->parm.output.timeperframe.denominator);

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(parm->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(parm->type))
		ctx->mediacfg.outputparam = parm->parm.output;
	else
		ctx->mediacfg.capparam = parm->parm.capture;
	return 0;
}

static int vsi_dec_g_parm(struct file *filp, void *priv, struct v4l2_streamparm *parm)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	pr_debug("%s:%d", __func__, parm->type);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(parm->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(parm->type))
		parm->parm.output = ctx->mediacfg.outputparam;
	else
		parm->parm.capture = ctx->mediacfg.capparam;
	return 0;
}

static int vsi_dec_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);

	pr_debug("%lx:%s", ctx->ctxid, __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;

	return vsiv4l2_getfmt(ctx, f);
}

static int vsi_dec_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);

	pr_debug("%lx:%s:%d:%d:%x", ctx->ctxid, __func__,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height, f->fmt.pix_mp.pixelformat);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;

	return vsiv4l2_setfmt(ctx, f);
}

static int vsi_dec_querybuf(
	struct file *filp,
	void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);
	struct vb2_queue *q;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(buf->type, ctx->flag))
		return -EINVAL;
	if (binputqueue(buf->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;
	pr_debug("%s::%lx:%d:%d:%d", __func__, ctx->flag, buf->type, q->type, buf->index);
	ret = vb2_querybuf(q, buf);
	if (buf->memory == V4L2_MEMORY_MMAP) {
		if (ret == 0 && q == &ctx->output_que)
			buf->m.offset += OUTF_BASE;
	}
	return ret;
}

static int vsi_dec_qbuf(struct file *filp, void *priv, struct v4l2_buffer *buf)
{
	int ret;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);
	struct video_device *vdev = ctx->dev->vdec;

	pr_debug("%lx:%s:%d:%d:%d", ctx->ctxid, __func__, buf->type, buf->index, buf->bytesused);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(buf->type, ctx->flag))
		return -EINVAL;
	if (binputqueue(buf->type) && buf->bytesused == 0 &&
		ctx->status == DEC_STATUS_DECODING) {
		if (test_and_clear_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag)) {
			ctx->status = DEC_STATUS_DRAINING;
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_CMD_STOP, NULL);
		} else
			ret = 0;
		return ret;
	}
	if (!binputqueue(buf->type)) {
		ctx->outbuflen[buf->index] = buf->length;
		ret = vb2_qbuf(&ctx->output_que, vdev->v4l2_dev->mdev, buf);
	} else {
		ctx->inbuflen[buf->index] = buf->length;
		ctx->inbufbytes[buf->index] = buf->bytesused;
		ret = vb2_qbuf(&ctx->input_que, vdev->v4l2_dev->mdev, buf);
	}
	if (ret == 0 && ctx->status == VSI_STATUS_INIT &&
		ctx->input_que.queued_count >= ctx->input_que.min_buffers_needed
		&& ctx->input_que.streaming) {
		ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMON_OUTPUT, NULL);
		if (ret == 0)
			ctx->status = DEC_STATUS_DECODING;
	}
	return ret;
}

static int vsi_dec_streamon(struct file *filp, void *priv, enum v4l2_buf_type type)
{
	int ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(type, ctx->flag))
		return -EINVAL;

	if (!binputqueue(type)) {
		ret = vb2_streamon(&ctx->output_que, type);
		if (ret == 0) {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMON_CAPTURE, NULL);
			if (ret == 0) {
				if (ctx->status == DEC_STATUS_DRAINING)
					ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_CMD_STOP, NULL);
				else
					ctx->status = DEC_STATUS_DECODING;
			}
		}
		printbufinfo(&ctx->output_que);
	} else {
		ret = vb2_streamon(&ctx->input_que, type);
		if (ret == 0 && ctx->input_que.queued_count >= ctx->input_que.min_buffers_needed)
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMON_OUTPUT, NULL);
		printbufinfo(&ctx->input_que);
	}
	pr_debug("%lx:%s:%d:%d:%d", ctx->ctxid, __func__, ctx->input_que.streaming, ctx->output_que.streaming, ret);

	return ret;
}

static int vsi_checkctx_srcbuf(struct vsi_v4l2_ctx *ctx)
{
	int ret = 0;

	if (mutex_lock_interruptible(&ctx->ctxlock))
		return 1;
	if (ctx->queued_srcnum == 0 || ctx->error < 0)
		ret = 1;
	mutex_unlock(&ctx->ctxlock);
	return ret;
}

static int vsi_dec_streamoff(
	struct file *file,
	void *priv,
	enum v4l2_buf_type type)
{
	int ret;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *q;
	enum v4l2_buf_type otype;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (ctx->error < 0)
		return -EFAULT;
	if (!isvalidtype(type, ctx->flag))
		return -EINVAL;

	otype = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE ? V4L2_BUF_TYPE_VIDEO_OUTPUT :
		V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (binputqueue(type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;

	if (q->streaming == 0 && ctx->status == VSI_STATUS_INIT)
		return 0;

	if (binputqueue(type)) {
		ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMOFF_OUTPUT, NULL);
		ctx->status = DEC_STATUS_SEEK;
	} else {
		ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMOFF_CAPTURE, NULL);
		ctx->status = DEC_STATUS_STOPPED;
	}
	if (binputqueue(type)) {
		if (!(test_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag))) {
			if (wait_event_interruptible(ctx->retbuf_queue,
				vsi_checkctx_srcbuf(ctx) != 0))
				return -ERESTARTSYS;
		} else {
			clear_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag);
			clear_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag);
		}
	} else {
		ctx->buffed_capnum = 0;
		ctx->buffed_cropcapnum = 0;
	}
	return_all_buffers(q, VB2_BUF_STATE_DONE, 1);
	ret = vb2_streamoff(q, type);
	pr_debug("%lx:%s:%d:%d", ctx->ctxid, __func__, type, ret);
	return ret;
}

static int vsi_dec_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	int ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;
	struct vb2_buffer *vb;
	struct vsi_vpu_buf *vsibuf;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(p->type, ctx->flag))
		return -EINVAL;
	if (binputqueue(p->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;

	printbufinfo(q);
	ret = vb2_dqbuf(q, p, file->f_flags & O_NONBLOCK);
	if (ctx->status == DEC_STATUS_STOPPED) {
		p->bytesused = 0;
		return -EPIPE;
	}
	if (ret == 0) {
		vb = q->bufs[p->index];
		vsibuf = vb_to_vsibuf(vb);
		if (mutex_lock_interruptible(&ctx->ctxlock))
			return -EBUSY;
		list_del(&vsibuf->list);
		if (!binputqueue(p->type)) {
			ctx->buffed_capnum--;
			ctx->buffed_cropcapnum--;
		}
		mutex_unlock(&ctx->ctxlock);
		if (ctx->status != DEC_STATUS_ENDSTREAM &&
			!(test_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag)) &&
			p->bytesused == 0)
			return -EAGAIN;
	}
	if (!binputqueue(p->type)) {
		struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

		if (p->bytesused == 0 && (ctx->status == DEC_STATUS_ENDSTREAM || test_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag))) {
			p->flags |= V4L2_BUF_FLAG_LAST;
			ctx->status = DEC_STATUS_STOPPED;
			clear_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag);
		} else if (ctx->status == DEC_STATUS_RESCHANGE && ctx->buffed_capnum == 0) {
			struct v4l2_event event = {
				.type = V4L2_EVENT_SOURCE_CHANGE,
				.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
			};
			pcfg->decparams.dec_info.dec_info = pcfg->decparams_bkup.dec_info.dec_info;
			pcfg->decparams.dec_info.io_buffer.srcwidth = pcfg->decparams_bkup.io_buffer.srcwidth;
			pcfg->decparams.dec_info.io_buffer.srcheight = pcfg->decparams_bkup.io_buffer.srcheight;
			pcfg->decparams.dec_info.io_buffer.output_width = pcfg->decparams_bkup.io_buffer.output_width;
			pcfg->decparams.dec_info.io_buffer.output_height = pcfg->decparams_bkup.io_buffer.output_height;
			v4l2_event_queue_fh(&ctx->fh, &event);
		} else if (test_bit(CTX_FLAG_CROPCHANGE_BIT, &ctx->flag) && ctx->buffed_cropcapnum == 0) {
			struct v4l2_event event = {
				.type = V4L2_EVENT_CROPCHANGE,
			};
			pcfg->decparams.dec_info.dec_info = pcfg->decparams_bkup.dec_info.dec_info;
			v4l2_event_queue_fh(&ctx->fh, &event);
			clear_bit(CTX_FLAG_CROPCHANGE_BIT, &ctx->flag);
		}
		p->field = V4L2_FIELD_NONE;
	}
	pr_debug("%lx:%s:%d:%d:%x:%d", ctx->ctxid, __func__, p->type, p->index, p->flags, p->bytesused);
	return ret;
}

static int vsi_dec_prepare_buf(
	struct file *file,
	void *priv,
	struct v4l2_buffer *p)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;
	struct video_device *vdev = ctx->dev->vdec;

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(p->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(p->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;
	return vb2_prepare_buf(q, vdev->v4l2_dev->mdev, p);
}

static int vsi_dec_expbuf(
	struct file *file,
	void *priv,
	struct v4l2_exportbuffer *p)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(p->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(p->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;
	return vb2_expbuf(q, p);
}

static int vsi_dec_try_fmt(struct file *file, void *prv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;

	if (vsi_find_format(ctx, f) == NULL)
		return -EINVAL;
	dec_getvui(f, &ctx->mediacfg.decparams.dec_info.dec_info);
	return 0;
}

static int vsi_dec_enum_fmt(struct file *file, void *prv, struct v4l2_fmtdesc *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_video_fmt *pfmt;
	int braw = brawfmt(ctx->flag, f->type);

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;

	pfmt = vsi_get_format_dec(f->index, braw, ctx);
	if (pfmt == NULL)
		return -EINVAL;

	if (pfmt->name && strlen(pfmt->name))
		strlcpy(f->description, pfmt->name, strlen(pfmt->name) + 1);
	f->pixelformat = pfmt->fourcc;
	f->flags = pfmt->flag;
	pr_debug("%s:%d:%d:%x", __func__, f->index, f->type, pfmt->fourcc);
	return 0;
}

static int vsi_dec_set_selection(struct file *file, void *prv, struct v4l2_selection *s)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

	//NXP has no PP, so any crop like setting won't work for decoder
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	pcfg->decparams.dec_info.dec_info.visible_rect.left = s->r.left;
	pcfg->decparams.dec_info.dec_info.visible_rect.top = s->r.top;
	pcfg->decparams.dec_info.dec_info.visible_rect.width = s->r.width;
	pcfg->decparams.dec_info.dec_info.visible_rect.height = s->r.height;

	pr_debug("%lx:%s", ctx->ctxid, __func__);
	return 0;
}

static int vsi_dec_get_selection(struct file *file, void *prv, struct v4l2_selection *s)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	s->r.left = pcfg->decparams.dec_info.dec_info.visible_rect.left;
	s->r.top = pcfg->decparams.dec_info.dec_info.visible_rect.top;
	s->r.width = pcfg->decparams.dec_info.dec_info.visible_rect.width;
	s->r.height = pcfg->decparams.dec_info.dec_info.visible_rect.height;

	pr_debug("%lx:%s: %d,%d,%d,%d", ctx->ctxid, __func__, s->r.left, s->r.top, s->r.width, s->r.height);

	return 0;
}

static int vsi_dec_subscribe_event(
	struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	int ret;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	ret = v4l2_event_subscribe(fh, sub, 0, NULL);	//&v4l2_ctrl_sub_ev_ops);
	pr_debug("%s:%d", __func__, ret);
	return ret;
}

int vsi_dec_decoder_cmd(struct file *file, void *fh, struct v4l2_decoder_cmd *cmd)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	//u32 flag = cmd->flags;
	int ret = -EBUSY;

	pr_debug("%lx:%s:%d", ctx->ctxid, __func__, cmd->cmd);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
		set_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag);
		if (ctx->status == DEC_STATUS_DECODING) {
			ctx->status = DEC_STATUS_DRAINING;
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_CMD_STOP, NULL);
		} else if ((ctx->status == VSI_STATUS_INIT && !test_bit(CTX_FLAG_DAEMONLIVE_BIT, &ctx->flag)) ||
				ctx->status == DEC_STATUS_STOPPED) {
			struct v4l2_event event = {
				.type = V4L2_EVENT_EOS,
			};
			clear_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag);
			v4l2_event_queue_fh(&ctx->fh, &event);
			ret = 0;
		}
		break;
	case V4L2_DEC_CMD_START:
		if (ctx->status == DEC_STATUS_STOPPED) {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_CMD_START, cmd);
			if (ret == 0)
				ctx->status = DEC_STATUS_DECODING;
		}
		break;
	case V4L2_DEC_CMD_RESET:
		vsi_v4l2_reset_ctx(ctx);
		break;
	case V4L2_DEC_CMD_PAUSE:
	case V4L2_DEC_CMD_RESUME:
	default:
		break;
	}
	return ret;
}


static const struct v4l2_ioctl_ops vsi_dec_ioctl = {
	.vidioc_querycap = vsi_dec_querycap,
	.vidioc_reqbufs             = vsi_dec_reqbufs,
	.vidioc_prepare_buf         = vsi_dec_prepare_buf,
	//create_buf can be provided now since we don't know buf type in param
	.vidioc_querybuf            = vsi_dec_querybuf,
	.vidioc_qbuf                = vsi_dec_qbuf,
	.vidioc_dqbuf               = vsi_dec_dqbuf,
	.vidioc_streamon        = vsi_dec_streamon,
	.vidioc_streamoff       = vsi_dec_streamoff,
	.vidioc_s_input             = vsi_dec_s_input,
	.vidioc_s_parm		= vsi_dec_s_parm,
	.vidioc_g_parm		= vsi_dec_g_parm,
	.vidioc_g_fmt_vid_cap = vsi_dec_g_fmt,
	//.vidioc_g_fmt_vid_cap_mplane = vsi_dec_g_fmt,
	.vidioc_s_fmt_vid_cap = vsi_dec_s_fmt,
	//.vidioc_s_fmt_vid_cap_mplane = vsi_dec_s_fmt,
	.vidioc_expbuf = vsi_dec_expbuf,		//this is used to export MMAP ptr as prime fd to user space app

	.vidioc_g_fmt_vid_out = vsi_dec_g_fmt,
	//.vidioc_g_fmt_vid_out_mplane = vsi_dec_g_fmt,
	.vidioc_s_fmt_vid_out = vsi_dec_s_fmt,
	//.vidioc_s_fmt_vid_out_mplane = vsi_dec_s_fmt,
	.vidioc_try_fmt_vid_cap = vsi_dec_try_fmt,
	//.vidioc_try_fmt_vid_cap_mplane = vsi_dec_try_fmt,
	.vidioc_try_fmt_vid_out = vsi_dec_try_fmt,
	//.vidioc_try_fmt_vid_out_mplane = vsi_dec_try_fmt,

	.vidioc_enum_fmt_vid_cap = vsi_dec_enum_fmt,
	.vidioc_enum_fmt_vid_out = vsi_dec_enum_fmt,

	.vidioc_g_fmt_vid_out = vsi_dec_g_fmt,
	//.vidioc_g_fmt_vid_out_mplane = vsi_dec_g_fmt,

	.vidioc_s_selection = vsi_dec_set_selection,		//VIDIOC_S_SELECTION, VIDIOC_S_CROP
	.vidioc_g_selection = vsi_dec_get_selection,		//VIDIOC_G_SELECTION, VIDIOC_G_CROP

	.vidioc_subscribe_event = vsi_dec_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,

	.vidioc_decoder_cmd = vsi_dec_decoder_cmd,
};

/*setup buffer information before real allocation*/
static int vsi_dec_queue_setup(
	struct vb2_queue *vq,
	unsigned int *nbuffers,
	unsigned int *nplanes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);
	int i;

	vsiv4l2_buffer_config(ctx, vq->type, nbuffers, nplanes, sizes);
	pr_debug("%s:%d,%d,%d", __func__, *nbuffers, *nplanes, sizes[0]);

	for (i = 0; i < *nplanes; i++)
		alloc_devs[i] = ctx->dev->dev;
	return 0;
}

static void vsi_dec_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);
	struct vsi_vpu_buf *vsibuf;
	int ret;

	pr_debug("%s", __func__);
	if (mutex_lock_interruptible(&ctx->ctxlock))
		return;
	vsibuf = vb_to_vsibuf(vb);
	if (!binputqueue(vq->type))
		list_add_tail(&vsibuf->list, &ctx->output_list);
	else {
		list_add_tail(&vsibuf->list, &ctx->input_list);
		ctx->queued_srcnum++;
	}
	mutex_unlock(&ctx->ctxlock);
	ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_BUF_RDY, vb);
	if (ret != 0)
		ctx->error = ret;
}

static int vsi_dec_buf_init(struct vb2_buffer *vb)
{
	pr_debug("%s:%d", __func__, vb->index);
	return 0;
}

static int vsi_dec_buf_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static int vsi_dec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	pr_debug("%s:%d", __func__, q->type);

	return 0;
}
static void vsi_dec_stop_streaming(struct vb2_queue *vq)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);
	struct list_head *plist;

	if (binputqueue(vq->type))
		plist = &ctx->input_list;
	else
		plist = &ctx->output_list;
	pr_debug("%s:%d:%d", __func__, vq->type, list_empty(plist));
	//return_all_buffers(vq, VB2_BUF_STATE_ERROR);
}

static void vsi_dec_buf_finish(struct vb2_buffer *vb)
{
	pr_debug("%s", __func__);
}

static void vsi_dec_buf_cleanup(struct vb2_buffer *vb)
{
	//struct vb2_queue *vq = vb->vb2_queue;
	//struct vsi_v4l2_ctx *ctx = fh_to_ctx(vq->drv_priv);

	pr_debug("%s", __func__);
	//ctx->dst_bufs[vb->index] = NULL;
}

static void vsi_dec_buf_wait_finish(struct vb2_queue *vq)
{
	vb2_ops_wait_finish(vq);
	pr_debug("%s\n", __func__);
}

static void vsi_dec_buf_wait_prepare(struct vb2_queue *vq)
{
	int empty = list_empty(&vq->done_list);

	pr_debug("%s:%d", __func__, empty);
	vb2_ops_wait_prepare(vq);
}

static struct vb2_ops vsi_dec_qops = {
	.queue_setup = vsi_dec_queue_setup,
	.wait_prepare = vsi_dec_buf_wait_prepare,	/*these two are just mutex protection for done_que*/
	.wait_finish = vsi_dec_buf_wait_finish,
	.buf_init = vsi_dec_buf_init,
	.buf_prepare = vsi_dec_buf_prepare,
	.buf_finish = vsi_dec_buf_finish,
	.buf_cleanup = vsi_dec_buf_cleanup,
	.start_streaming = vsi_dec_start_streaming,
	.stop_streaming = vsi_dec_stop_streaming,
	.buf_queue = vsi_dec_buf_queue,
	//fill_user_buffer
	//int (*buf_out_validate)(struct vb2_buffer *vb);
	//void (*buf_request_complete)(struct vb2_buffer *vb);
};

static int v4l2_dec_open(struct file *filp)
{
	//struct video_device *vdev = video_devdata(filp);
	struct vsi_v4l2_device *dev = video_drvdata(filp);
	struct vsi_v4l2_ctx *ctx = NULL;
	struct vb2_queue *q;
	int ret = 0;
	struct v4l2_fh *vfh;
	pid_t  pid;

	/* Allocate memory for context */
	//fh->video_devdata = struct video_device, struct video_device->video_drvdata = struct vsi_v4l2_device
	if (vsi_v4l2_addinstance(&pid) < 0)
		return -EBUSY;

	ctx = vsi_create_ctx();
	if (ctx == NULL) {
		vsi_v4l2_quitinstance();
		return -ENOMEM;
	}

	v4l2_fh_init(&ctx->fh, video_devdata(filp));
	filp->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->dev = dev;
	mutex_init(&ctx->ctxlock);
	ctx->flag = CTX_FLAG_DEC;
	set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);

	ctx->frameidx = 0;
	q = &ctx->input_que;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->min_buffers_needed = 1;
	q->drv_priv = &ctx->fh;
	q->lock = &ctx->ctxlock;
	q->buf_struct_size = sizeof(struct vsi_vpu_buf);		//used to alloc mem control structures in reqbuf
	q->ops = &vsi_dec_qops;		/*it might be used to identify input and output */
	q->mem_ops = &vb2_dma_contig_memops;
	q->memory = VB2_MEMORY_UNKNOWN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	INIT_LIST_HEAD(&ctx->input_list);
	ret = vb2_queue_init(q);
	/*q->buf_ops = &v4l2_buf_ops is set here*/
	if (ret)
		goto err_enc_dec_exit;

	q = &ctx->output_que;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = &ctx->fh;
	q->lock = &ctx->ctxlock;
	q->buf_struct_size = sizeof(struct vsi_vpu_buf);
	q->ops = &vsi_dec_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->memory = VB2_MEMORY_UNKNOWN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->min_buffers_needed = 1;
	INIT_LIST_HEAD(&ctx->output_list);
	ret = vb2_queue_init(q);
	if (ret) {
		vb2_queue_release(&ctx->input_que);
		goto err_enc_dec_exit;
	}
	vsiv4l2_initcfg(ctx);
	vsi_setup_ctrls(&ctx->ctrlhdl);
	vfh = (struct v4l2_fh *)filp->private_data;
	vfh->ctrl_handler = &ctx->ctrlhdl;
	atomic_set(&ctx->srcframen, 0);
	atomic_set(&ctx->dstframen, 0);
	ctx->status = VSI_STATUS_INIT;

	//dev->vdev->queue = q;
	//single queue is used for v4l2 default ops such as ioctl, read, write and poll
	//If we wanna manage queue by ourselves, leave it null and don't use default v4l2 ioctl/read/write/poll interfaces.

	return 0;

err_enc_dec_exit:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	vsi_remove_ctx(ctx);
	kfree(ctx);
	vsi_v4l2_quitinstance();
	return ret;
}

static int v4l2_dec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	if (offset < OUTF_BASE) {
		ret = vb2_mmap(&ctx->input_que, vma);
	} else {
		vma->vm_pgoff -= (OUTF_BASE >> PAGE_SHIFT);
		offset -= OUTF_BASE;
		ret = vb2_mmap(&ctx->output_que, vma);
	}
	return ret;
}

static __poll_t vsi_dec_poll(struct file *file, poll_table *wait)
{
	__poll_t res = 0;
	__poll_t ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	int dstn = atomic_read(&ctx->dstframen);
	int srcn = atomic_read(&ctx->srcframen);

	pr_debug("%s:%x:%d:%d", __func__, wait->_key,
	       ctx->output_que.streaming, ctx->input_que.streaming);
	if (!vsi_v4l2_daemonalive())
		return POLLERR;
	if (ctx->status == DEC_STATUS_SEEK) {
		ret = (POLLIN | POLLRDNORM);
		if (ctx->error < 0)
			ret |= POLLERR;
		return ret;
	}
	if (vb2_is_streaming(&ctx->output_que))
		res = vb2_poll(&ctx->output_que, file, wait);
	res |= vb2_poll(&ctx->input_que, file, wait);

	if (res & EPOLLERR)
		ret |= POLLERR;
	if (res & EPOLLPRI)
		ret |= POLLPRI;
	if (res & EPOLLIN)
		ret |= POLLIN | POLLRDNORM;
	if (res & EPOLLOUT)
		ret |= POLLOUT | POLLWRNORM;
	/*recheck for poll hang*/
	if (ret == 0) {
		if (dstn != atomic_read(&ctx->dstframen))
			res = vb2_poll(&ctx->output_que, file, wait);
		if (srcn != atomic_read(&ctx->srcframen))
			res |= vb2_poll(&ctx->input_que, file, wait);
		if (res & EPOLLERR)
			ret |= POLLERR;
		if (res & EPOLLPRI)
			ret |= POLLPRI;
		if (res & EPOLLIN)
			ret |= POLLIN | POLLRDNORM;
		if (res & EPOLLOUT)
			ret |= POLLOUT | POLLWRNORM;
	}
	if (ctx->error < 0)
		ret |= POLLERR;
	pr_debug("poll out %x", ret);
	return ret;
}

static const struct v4l2_file_operations v4l2_dec_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_dec_open,
	.release = v4l2_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_dec_mmap,
	.poll = vsi_dec_poll,
};

struct video_device *v4l2_probe_dec(struct platform_device *pdev, struct vsi_v4l2_device *vpu)
{
	struct video_device *vdec;
	int ret = 0;

	pr_debug("%s", __func__);

	vdec = video_device_alloc();
	if (!vdec) {
		v4l2_err(&vpu->v4l2_dev, "Failed to allocate dec device\n");
		ret = -ENOMEM;
		goto err;
	}

	vdec->fops = &v4l2_dec_fops;
	vdec->ioctl_ops = &vsi_dec_ioctl;
	vdec->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	vdec->release = video_device_release;
	vdec->lock = &vpu->lock;
	vdec->v4l2_dev = &vpu->v4l2_dev;
	vdec->vfl_dir = VFL_DIR_M2M;
	vdec->vfl_type = VSI_DEVTYPE;
	vpu->vdec = vdec;
	vdec->queue = NULL;

	video_set_drvdata(vdec, vpu);

	ret = video_register_device(vdec, VSI_DEVTYPE, 0);
	if (ret) {
		v4l2_err(&vpu->v4l2_dev, "Failed to register dec device\n");
		video_device_release(vdec);
		goto err;
	}
	return vdec;

err:
	return NULL;
}

void v4l2_release_dec(struct video_device *vdec)
{
	video_unregister_device(vdec);
}

