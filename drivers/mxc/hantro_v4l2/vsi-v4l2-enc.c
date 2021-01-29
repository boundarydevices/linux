/*
 *    VSI V4L2 encoder entry.
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

static int vsi_enc_querycap(
	struct file *file,
	void *priv,
	struct v4l2_capability *cap)
{
	struct vsi_v4l2_dev_info *hwinfo;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	hwinfo = vsiv4l2_get_hwinfo();
	if (hwinfo->encformat == 0)
		return -ENODEV;
	pr_debug("%s", __func__);
	strlcpy(cap->driver, "vsi_v4l2", sizeof("vsi_v4l2"));
	strlcpy(cap->card, "vsi_v4l2enc", sizeof("vsi_v4l2enc"));
	strlcpy(cap->bus_info, "platform:vsi_v4l2enc", sizeof("platform:vsi_v4l2enc"));

	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vsi_enc_reqbufs(
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
	pr_debug("%s:%d ask for %d buffer, got %d:%d:%d", __func__, p->type, p->count, q->num_buffers, ret, ctx->status);
	return ret;
}

/*choose input source of index*/
static int vsi_enc_s_input(struct file *file, void *priv, unsigned int index)
{
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;

	pr_debug("%s", __func__);
	return 0;
}

static int vsi_enc_s_parm(struct file *filp, void *priv, struct v4l2_streamparm *parm)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	pr_debug("%s:%d:%d", __func__, parm->parm.output.timeperframe.numerator,
		parm->parm.output.timeperframe.denominator);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(parm->type, ctx->flag))
		return -EINVAL;

	if (binputqueue(parm->type)) {
		ctx->mediacfg.encparams.general.inputRateNumer = parm->parm.output.timeperframe.denominator;
		ctx->mediacfg.encparams.general.inputRateDenom = parm->parm.output.timeperframe.numerator;
		ctx->mediacfg.outputparam = parm->parm.output;
	} else {
		ctx->mediacfg.encparams.general.outputRateNumer = parm->parm.capture.timeperframe.denominator;
		ctx->mediacfg.encparams.general.outputRateDenom = parm->parm.capture.timeperframe.numerator;
		ctx->mediacfg.capparam = parm->parm.capture;
	}
	set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);
	return 0;
}

static int vsi_enc_g_parm(struct file *filp, void *priv, struct v4l2_streamparm *parm)
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

static int vsi_enc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);

	pr_debug("%s:%d", __func__, f->type);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;
	return vsiv4l2_getfmt(ctx, f);
}

static int vsi_enc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	int ret;

	pr_debug("%s:%d:%d:%x", __func__, f->fmt.pix_mp.width, f->fmt.pix_mp.height, f->fmt.pix_mp.pixelformat);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;
	ret = vsiv4l2_setfmt(ctx, f);
	set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);
	return ret;
}

static int vsi_enc_querybuf(
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
	pr_debug("%s::%ld:%d:%d:%d", __func__, ctx->flag, buf->type, q->type, buf->index);
	ret = vb2_querybuf(q, buf);
	if (buf->memory == V4L2_MEMORY_MMAP) {
		if (ret == 0 && q == &ctx->output_que)
			buf->m.planes[0].m.mem_offset += OUTF_BASE;
	}

	return ret;
}

static int vsi_enc_trystartenc(struct vsi_v4l2_ctx *ctx)
{
	int ret = 0;
	struct vsi_queued_buf *buf, *node;
	struct video_device *vdev = ctx->dev->venc;

	if (ctx->input_que.streaming && ctx->output_que.streaming) {
		if (!list_empty(&ctx->queued_list)) {
			list_for_each_entry_safe(buf, node, &ctx->queued_list, list) {
				if (!binputqueue(buf->qb.type))
					ret = vb2_qbuf(&ctx->output_que, vdev->v4l2_dev->mdev, &buf->qb);
				else
					ret = vb2_qbuf(&ctx->input_que, vdev->v4l2_dev->mdev, &buf->qb);
				list_del(&buf->list);
				vfree(buf);
			}
		}
		if ((ctx->status == VSI_STATUS_INIT ||
			ctx->status == ENC_STATUS_STOPPED ||
			ctx->status == ENC_STATUS_STOPPED_BYUSR ||
			ctx->status == ENC_STATUS_RESET) &&
			ctx->input_que.queued_count >= ctx->input_que.min_buffers_needed &&
			ctx->output_que.queued_count >= ctx->output_que.min_buffers_needed) {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMON, NULL);
			if (ret == 0)
				ctx->status = ENC_STATUS_ENCODING;
		}
	}
	return ret;
}

static int bufto_queuelist(struct vsi_v4l2_ctx *ctx, struct v4l2_buffer *buf)
{
	struct vsi_queued_buf *qb;
	struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;
	int i, planeno = (binputqueue(buf->type) ? pcfg->srcplanes : pcfg->dstplanes);

	qb = vmalloc(sizeof(struct vsi_queued_buf));
	if (qb == NULL)
		return -ENOMEM;
	qb->qb = *buf;
	for (i = 0; i < planeno; i++)
		qb->planes[i] = buf->m.planes[i];
	qb->qb.m.planes = qb->planes;
	list_add_tail(&qb->list, &ctx->queued_list);
	return 0;
}

static void clear_quelist(struct vsi_v4l2_ctx *ctx)
{
	struct vsi_queued_buf *buf, *node;

	if (!list_empty(&ctx->queued_list)) {
		list_for_each_entry_safe(buf, node, &ctx->queued_list, list) {
			list_del(&buf->list);
			vfree(buf);
		}
	}
}

static int vsi_enc_qbuf(struct file *filp, void *priv, struct v4l2_buffer *buf)
{
	int ret;
	//struct vb2_queue *vq = vb->vb2_queue;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);
	struct video_device *vdev = ctx->dev->venc;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(buf->type, ctx->flag))
		return -EINVAL;
	if (ctx->status == ENC_STATUS_STOPPED_BYUSR)
		return 0;
	if ((ctx->status == ENC_STATUS_STOPPED ||
		ctx->status == ENC_STATUS_RESET ||
		ctx->status == ENC_STATUS_DRAINING) &&
		binputqueue(buf->type))
		return bufto_queuelist(ctx, buf);

	if (!binputqueue(buf->type))
		ret = vb2_qbuf(&ctx->output_que, vdev->v4l2_dev->mdev, buf);
	else {
		if (test_and_clear_bit(CTX_FLAG_FORCEIDR_BIT, &ctx->flag))
			ctx->srcvbufflag[buf->index] |= FORCE_IDR;

		ret = vb2_qbuf(&ctx->input_que, vdev->v4l2_dev->mdev, buf);
	}
	pr_debug("%s:%d:%d:%d, %d:%d, %d:%d",
		__func__, buf->type, buf->index, buf->bytesused,
		buf->m.planes[0].bytesused, buf->m.planes[0].length,
		buf->m.planes[1].bytesused, buf->m.planes[1].length);
	if (ret == 0 && ctx->status != ENC_STATUS_ENCODING &&
		ctx->status != ENC_STATUS_STOPPED)
		ret = vsi_enc_trystartenc(ctx);

	return ret;
}

static int vsi_enc_streamon(struct file *filp, void *priv, enum v4l2_buf_type type)
{
	int ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	pr_debug("%s:%d:%d:%d", __func__,
		ctx->input_que.streaming, ctx->output_que.streaming, ret);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(type, ctx->flag))
		return -EINVAL;
	if (ctx->status == ENC_STATUS_ENCODING)
		return 0;

	if (!binputqueue(type)) {
		ret = vb2_streamon(&ctx->output_que, type);
		printbufinfo(&ctx->output_que);
	} else {
		ret = vb2_streamon(&ctx->input_que, type);
		printbufinfo(&ctx->input_que);
	}

	if (ret == 0) {
		if (ctx->status == ENC_STATUS_STOPPED_BYUSR)
			ctx->status = ENC_STATUS_STOPPED;
		ret = vsi_enc_trystartenc(ctx);
	}

	return ret;
}

static int vsi_enc_streamoff(
	struct file *file,
	void *priv,
	enum v4l2_buf_type type)
{
	int i, ret;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *q;

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(type, ctx->flag))
		return -EINVAL;

	if (ctx->status != ENC_STATUS_ENCODING &&
		ctx->status != ENC_STATUS_DRAINING &&
		ctx->status != ENC_STATUS_STOPPED &&
		ctx->status != ENC_STATUS_STOPPED_BYUSR &&
		ctx->status != ENC_STATUS_RESET)
		return -EINVAL;

	if (binputqueue(type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;

	return_all_buffers(q, VB2_BUF_STATE_DONE, 1);
	ret = vb2_streamoff(q, type);
	pr_debug("%s:%d:%d", __func__, type, ret);
	if (ret == 0) {
		if (binputqueue(type)) {
			ctx->status = ENC_STATUS_STOPPED_BYUSR;
			clear_quelist(ctx);
			clear_bit(CTX_FLAG_FORCEIDR_BIT, &ctx->flag);
			for (i = 0; i < VIDEO_MAX_FRAME; i++)
				ctx->srcvbufflag[i] = 0;
		} else
			ctx->status = ENC_STATUS_RESET;
	}
	return ret;
}

static int vsi_enc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	int ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;
	struct vb2_buffer *vb;
	struct vsi_vpu_buf *vsibuf;
	struct v4l2_event event = {.type = V4L2_EVENT_EOS};

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(p->type, ctx->flag))
		return -EINVAL;
	if (binputqueue(p->type))
		q = &ctx->input_que;
	else
		q = &ctx->output_que;

	if (ctx->status == ENC_STATUS_STOPPED_BYUSR ||
		ctx->status == ENC_STATUS_STOPPED) {
		p->bytesused = 0;
		return -EPIPE;
	}
	printbufinfo(q);
	ret = vb2_dqbuf(q, p, file->f_flags & O_NONBLOCK);

	if (ret == 0) {
		vb = q->bufs[p->index];
		vsibuf = vb_to_vsibuf(vb);
		if (mutex_lock_interruptible(&ctx->ctxlock))
			return -EBUSY;
		list_del(&vsibuf->list);
		mutex_unlock(&ctx->ctxlock);
		p->flags &= ~(V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME);
		if (!binputqueue(p->type)) {
			if (ctx->vbufflag[p->index] & FRAMETYPE_I)
				p->flags |= V4L2_BUF_FLAG_KEYFRAME;
			else  if (ctx->vbufflag[p->index] & FRAMETYPE_P)
				p->flags |= V4L2_BUF_FLAG_PFRAME;
			else  if (ctx->vbufflag[p->index] & FRAMETYPE_B)
				p->flags |= V4L2_BUF_FLAG_BFRAME;
		}
	}
	if (!binputqueue(p->type)) {
		if (ret == 0) {
			if (ctx->vbufflag[p->index] & LAST_BUFFER_FLAG) {
				p->flags |= V4L2_BUF_FLAG_LAST;
				v4l2_event_queue_fh(&ctx->fh, &event);
				if (ctx->status == ENC_STATUS_DRAINING)
					ctx->status = ENC_STATUS_STOPPED;
			}
		}
	}
	pr_debug("%s:%d:%d:%d:%x:%d", __func__, p->type, p->index, ret, p->flags, ctx->status);
	return ret;
}

static int vsi_enc_prepare_buf(
	struct file *file,
	void *priv,
	struct v4l2_buffer *p)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;
	struct video_device *vdev = ctx->dev->venc;

	pr_debug("%s:%d", __func__, p->type);
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

static int vsi_enc_expbuf(
	struct file *file,
	void *priv,
	struct v4l2_exportbuffer *p)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vb2_queue *q;

	pr_debug("%s:%d", __func__, p->type);
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

static int vsi_enc_try_fmt(struct file *file, void *prv, struct v4l2_format *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);

	pr_debug("%s:%d", __func__, f->type);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;
	if (vsi_find_format(ctx, f) == NULL)
		return -EINVAL;
	f->fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;

	return 0;
}

static int vsi_enc_enum_fmt(struct file *file, void *prv, struct v4l2_fmtdesc *f)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_video_fmt *pfmt;
	int braw = brawfmt(ctx->flag, f->type);

	pr_debug("%s:%d", __func__, f->type);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (!isvalidtype(f->type, ctx->flag))
		return -EINVAL;

	pfmt = vsi_get_format_enc(f->index, braw);
	if (pfmt == NULL)
		return -EINVAL;

	if (pfmt->name && strlen(pfmt->name))
		strlcpy(f->description, pfmt->name, strlen(pfmt->name) + 1);
	f->pixelformat = pfmt->fourcc;
	f->flags = pfmt->flag;
	return 0;
}

static int vsi_enc_set_selection(struct file *file, void *prv, struct v4l2_selection *s)
{
	int ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT &&
		s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;
	if (s->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;
	ret = vsiv4l2_verifycrop(s);
	if (!ret) {
		pcfg->encparams.general.horOffsetSrc = s->r.left;
		pcfg->encparams.general.verOffsetSrc = s->r.top;
		pcfg->encparams.general.width = s->r.width;
		pcfg->encparams.general.height = s->r.height;
		set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);
	}
	return ret;
}

static int vsi_enc_get_selection(struct file *file, void *prv, struct v4l2_selection *s)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

	pr_debug("%s", __func__);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT &&
		s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;
	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		s->r.left = pcfg->encparams.general.horOffsetSrc;
		s->r.top = pcfg->encparams.general.verOffsetSrc;
		s->r.width = pcfg->encparams.general.width;
		s->r.height = pcfg->encparams.general.height;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = pcfg->encparams.general.lumWidthSrc;
		s->r.height = pcfg->encparams.general.lumHeightSrc;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vsi_enc_subscribe_event(
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

static int vsi_enc_encoder_cmd(struct file *file, void *fh, struct v4l2_encoder_cmd *cmd)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	//u32 flag = cmd->flags;
	int ret = -EBUSY;

	/// refer to https://linuxtv.org/downloads/v4l-dvb-apis/userspace-api/v4l/dev-encoder.html
	pr_debug("%s:%d:%d", __func__, ctx->status, cmd->cmd);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
		if (ctx->status == ENC_STATUS_ENCODING) {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_CMD_STOP, cmd);
			if (ret == 0)
				ctx->status = ENC_STATUS_DRAINING;
		} else if (ctx->status != ENC_STATUS_DRAINING)
			ret = 0;
		break;
	case V4L2_ENC_CMD_START:
		if (ctx->status == ENC_STATUS_STOPPED ||
			ctx->status == ENC_STATUS_STOPPED_BYUSR) {
			vb2_streamon(&ctx->input_que, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
			vb2_streamon(&ctx->input_que, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
			ret = vsi_enc_trystartenc(ctx);
		} else if (ctx->status == ENC_STATUS_ENCODING)
			ret = 0;
		break;
	case V4L2_ENC_CMD_PAUSE:
	case V4L2_ENC_CMD_RESUME:
	default:
		break;
	}
	return ret;
}

static int vsi_enc_encoder_enum_framesizes(struct file *file, void *priv,
				  struct v4l2_frmsizeenum *fsize)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	struct v4l2_format fmt;

	pr_debug("%s:%x", __func__, fsize->pixel_format);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	if (fsize->index != 0)		//only stepwise
		return -EINVAL;

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.pixelformat = fsize->pixel_format;
	if (vsi_find_format(ctx,  &fmt) != NULL)
		vsi_enum_encfsize(fsize, ctx->mediacfg.outfmt_fourcc);
	else {
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt.fmt.pix_mp.pixelformat = fsize->pixel_format;
		if (vsi_find_format(ctx,  &fmt) == NULL)
			return -EINVAL;
		vsi_enum_encfsize(fsize, fsize->pixel_format);
	}

	return 0;
}

/* ioctl handler */
/* take VIDIOC_S_INPUT for example, ioctl goes to V4l2-ioctl.c.: v4l_s_input() -> V4l2-dev.c: v4l2_ioctl_ops.vidioc_s_input() */
/* ioctl cmd could be disabled by v4l2_disable_ioctl() */
static const struct v4l2_ioctl_ops vsi_enc_ioctl = {
	.vidioc_querycap = vsi_enc_querycap,
	.vidioc_reqbufs             = vsi_enc_reqbufs,
	.vidioc_prepare_buf         = vsi_enc_prepare_buf,
	//create_buf can be provided now since we don't know buf type in param
	.vidioc_querybuf            = vsi_enc_querybuf,
	.vidioc_qbuf                = vsi_enc_qbuf,
	.vidioc_dqbuf               = vsi_enc_dqbuf,
	.vidioc_streamon        = vsi_enc_streamon,
	.vidioc_streamoff       = vsi_enc_streamoff,
	.vidioc_s_input             = vsi_enc_s_input,
	.vidioc_s_parm		= vsi_enc_s_parm,
	.vidioc_g_parm		= vsi_enc_g_parm,
	//.vidioc_g_fmt_vid_cap = vsi_enc_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane = vsi_enc_g_fmt,
	//.vidioc_s_fmt_vid_cap = vsi_enc_s_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vsi_enc_s_fmt,
	.vidioc_expbuf = vsi_enc_expbuf,		//this is used to export MMAP ptr as prime fd to user space app

	//.vidioc_g_fmt_vid_out = vsi_enc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vsi_enc_g_fmt,
	//.vidioc_s_fmt_vid_out = vsi_enc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = vsi_enc_s_fmt,
	//.vidioc_try_fmt_vid_cap = vsi_enc_try_fmt,
	.vidioc_try_fmt_vid_cap_mplane = vsi_enc_try_fmt,
	//.vidioc_try_fmt_vid_out = vsi_enc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vsi_enc_try_fmt,
	.vidioc_enum_fmt_vid_cap = vsi_enc_enum_fmt,
	.vidioc_enum_fmt_vid_out = vsi_enc_enum_fmt,

	//.vidioc_g_fmt_vid_out = vsi_enc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vsi_enc_g_fmt,

	.vidioc_s_selection = vsi_enc_set_selection,		//VIDIOC_S_SELECTION, VIDIOC_S_CROP
	.vidioc_g_selection = vsi_enc_get_selection,		//VIDIOC_G_SELECTION, VIDIOC_G_CROP

	.vidioc_subscribe_event = vsi_enc_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,

	//fixme: encoder cmd stop will make streamoff not coming from ffmpeg. Maybe this is the right way to get finished, check later
	.vidioc_encoder_cmd = vsi_enc_encoder_cmd,
	.vidioc_enum_framesizes = vsi_enc_encoder_enum_framesizes,
};

/*setup buffer information before real allocation*/
static int vsi_enc_queue_setup(
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

static void vsi_enc_buf_queue(struct vb2_buffer *vb)
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
	else
		list_add_tail(&vsibuf->list, &ctx->input_list);
	mutex_unlock(&ctx->ctxlock);
	ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_BUF_RDY, vb);
	if (ret != 0)
		ctx->error = ret;
}

static int vsi_enc_buf_init(struct vb2_buffer *vb)
{
	pr_debug("%s:%d", __func__, vb->index);
	return 0;
}

static int vsi_enc_buf_prepare(struct vb2_buffer *vb)
{
	/*any valid init operation on buffer vb*/
	/*gspca and rockchip both check buffer size here*/
	//like vb2_set_plane_payload(vb, 0, 1920*1080);
	return 0;
}

static int vsi_enc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	pr_debug("%s:%d", __func__, q->type);

	return 0;
}
static void vsi_enc_stop_streaming(struct vb2_queue *vq)
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

static void vsi_enc_buf_finish(struct vb2_buffer *vb)
{
	/*post process of buffer befre release*/
	/*just for simulation*/
	pr_debug("%s", __func__);
}

static void vsi_enc_buf_cleanup(struct vb2_buffer *vb)
{
	pr_debug("%s", __func__);
}

static void vsi_enc_buf_wait_finish(struct vb2_queue *vq)
{
	vb2_ops_wait_finish(vq);
	pr_debug("%s\n", __func__);
}

static void vsi_enc_buf_wait_prepare(struct vb2_queue *vq)
{
	int empty = list_empty(&vq->done_list);

	pr_debug("%s:%d", __func__, empty);
	vb2_ops_wait_prepare(vq);
}

static struct vb2_ops vsi_enc_qops = {
	.queue_setup = vsi_enc_queue_setup,
	.wait_prepare = vsi_enc_buf_wait_prepare,	/*these two are just mutex protection for done_que*/
	.wait_finish = vsi_enc_buf_wait_finish,
	.buf_init = vsi_enc_buf_init,
	.buf_prepare = vsi_enc_buf_prepare,
	.buf_finish = vsi_enc_buf_finish,
	.buf_cleanup = vsi_enc_buf_cleanup,
	.start_streaming = vsi_enc_start_streaming,
	.stop_streaming = vsi_enc_stop_streaming,
	.buf_queue = vsi_enc_buf_queue,
	//fill_user_buffer
	//int (*buf_out_validate)(struct vb2_buffer *vb);
	//void (*buf_request_complete)(struct vb2_buffer *vb);
};

static int v4l2_enc_open(struct file *filp)
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
	ctx->flag = CTX_FLAG_ENC;
	set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);

	ctx->frameidx = 0;
	q = &ctx->input_que;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->min_buffers_needed = MIN_FRAME_4ENC;
	q->drv_priv = &ctx->fh;
	q->lock = &ctx->ctxlock;
	q->buf_struct_size = sizeof(struct vsi_vpu_buf);		//used to alloc mem control structures in reqbuf
	q->ops = &vsi_enc_qops;		/*it might be used to identify input and output */
	q->mem_ops = &vb2_dma_contig_memops;
	q->memory = VB2_MEMORY_UNKNOWN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	INIT_LIST_HEAD(&ctx->input_list);
	ret = vb2_queue_init(q);
	/*q->buf_ops = &v4l2_buf_ops is set here*/
	if (ret)
		goto err_enc_dec_exit;

	q = &ctx->output_que;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->min_buffers_needed = 1;
	q->drv_priv = &ctx->fh;
	q->lock = &ctx->ctxlock;
	q->buf_struct_size = sizeof(struct vsi_vpu_buf);
	q->ops = &vsi_enc_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->memory = VB2_MEMORY_UNKNOWN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	INIT_LIST_HEAD(&ctx->output_list);
	ret = vb2_queue_init(q);
	if (ret) {
		vb2_queue_release(&ctx->input_que);
		goto err_enc_dec_exit;
	}
	INIT_LIST_HEAD(&ctx->queued_list);
	vsiv4l2_initcfg(ctx);
	vsi_setup_ctrls(&ctx->ctrlhdl);
	vfh = (struct v4l2_fh *)filp->private_data;
	vfh->ctrl_handler = &ctx->ctrlhdl;
	atomic_set(&ctx->srcframen, 0);
	atomic_set(&ctx->dstframen, 0);
	ctx->status = VSI_STATUS_INIT;

	return 0;

err_enc_dec_exit:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	vsi_remove_ctx(ctx);
	kfree(ctx);
	vsi_v4l2_quitinstance();
	return ret;
}

static int v4l2_enc_mmap(struct file *filp, struct vm_area_struct *vma)
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

static __poll_t vsi_enc_poll(struct file *file, poll_table *wait)
{
	__poll_t res;
	__poll_t ret = 0;
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(file->private_data);
	int dstn = atomic_read(&ctx->dstframen);
	int srcn = atomic_read(&ctx->srcframen);

	pr_debug("%s:%x:%d:%d", __func__, wait->_key,
	       ctx->output_que.streaming, ctx->input_que.streaming);
	if (!vsi_v4l2_daemonalive())
		return POLLERR;

	if (v4l2_event_pending(&ctx->fh)) {
		pr_debug("poll event");
		ret |= POLLPRI;
	}

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

static const struct v4l2_file_operations v4l2_enc_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_enc_open,
	.release = v4l2_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_enc_mmap,
	.poll = vsi_enc_poll,
};

struct video_device *v4l2_probe_enc(struct platform_device *pdev, struct vsi_v4l2_device *vpu)
{
	struct video_device *venc;
	int ret = 0;

	pr_debug("%s", __func__);

	/*init video device0, encoder */
	venc = video_device_alloc();
	if (!venc) {
		v4l2_err(&vpu->v4l2_dev, "Failed to allocate enc device\n");
		ret = -ENOMEM;
		goto err;
	}

	venc->fops = &v4l2_enc_fops;
	venc->ioctl_ops = &vsi_enc_ioctl;
	venc->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	venc->release = video_device_release;
	venc->lock = &vpu->lock;
	venc->v4l2_dev = &vpu->v4l2_dev;
	venc->vfl_dir = VFL_DIR_M2M;
	venc->vfl_type = VSI_DEVTYPE;
	venc->queue = NULL;

	video_set_drvdata(venc, vpu);

	ret = video_register_device(venc, VSI_DEVTYPE, 0);
	if (ret) {
		v4l2_err(&vpu->v4l2_dev, "Failed to register enc device\n");
		video_device_release(venc);
		goto err;
	}

	return venc;
err:
	return NULL;
}

void v4l2_release_enc(struct video_device *venc)
{
	video_unregister_device(venc);
}

