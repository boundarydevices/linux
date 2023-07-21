// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "mtk-mdp3-core.h"
#include "mtk-mdp3-capture.h"

#include "mt8183/mdp3-plat-mt8183.h"
#include "mt8188/mdp3-plat-mt8188.h"
#include "mt8195/mdp3-plat-mt8195.h"

static inline struct mdp_cap_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mdp_cap_ctx, fh);
}

static inline struct mdp_frame *ctx_get_frame(struct mdp_cap_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->curr_param.output;
	else
		return &ctx->curr_param.captures[0];
}

static int mdp_cap_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct video_device *vdev = video_drvdata(file);

	strscpy(cap->driver, MDP_MODULE_NAME, sizeof(cap->driver));
	strscpy(cap->card, "mdp-capture", sizeof(cap->card));
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | V4L2_CAP_STREAMING
		| V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", vdev->v4l2_dev->name);

	return 0;
}

static int mdp_cap_enum_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	struct mdp_cap_ctx *ctx = fh_to_ctx(fh);

	return mdp_enum_fmt_mplane(ctx->mdp_dev, f);
}

static int mdp_cap_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mdp_cap_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame;
	struct v4l2_pix_format_mplane *pix_mp;

	frame = ctx_get_frame(ctx, f->type);
	*f = frame->format;
	pix_mp = &f->fmt.pix_mp;
	pix_mp->colorspace = ctx->curr_param.colorspace;
	pix_mp->xfer_func = ctx->curr_param.xfer_func;
	pix_mp->ycbcr_enc = ctx->curr_param.ycbcr_enc;
	pix_mp->quantization = ctx->curr_param.quant;

	return 0;
}

static void _mdp_cap_fill_v4l2_format(struct v4l2_pix_format_mplane *pix_mp,
				      const struct mdp_format *fmt)
{
	if (MDP_COLOR_IS_YUV(fmt->mdp_color)) {
		pix_mp->colorspace = V4L2_COLORSPACE_REC709;
		pix_mp->xfer_func = V4L2_XFER_FUNC_709;
		pix_mp->ycbcr_enc = V4L2_YCBCR_ENC_709;
		pix_mp->quantization = V4L2_QUANTIZATION_LIM_RANGE;
	} else {
		pix_mp->colorspace = V4L2_COLORSPACE_SRGB;
		pix_mp->xfer_func = V4L2_XFER_FUNC_709;
		pix_mp->ycbcr_enc = V4L2_YCBCR_ENC_709;
		pix_mp->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	}
}

static int mdp_cap_s_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mdp_cap_ctx *ctx = fh_to_ctx(fh);
	struct mdp_frame *frame = ctx_get_frame(ctx, f->type);
	struct mdp_frame *capture;
	const struct mdp_format *fmt;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	enum mdp_ycbcr_profile profile;

	fmt = mdp_try_fmt_mplane(ctx->mdp_dev, f, &ctx->curr_param, ctx->id);

	if (!fmt)
		return -EINVAL;

	if (vb2_is_busy(&ctx->cap_q))
		return -EBUSY;

	_mdp_cap_fill_v4l2_format(pix_mp, fmt);
	frame->format = *f;
	frame->mdp_fmt = fmt;
	frame->usage = V4L2_TYPE_IS_OUTPUT(f->type) ?
		MDP_BUFFER_USAGE_HDMI_RX : MDP_BUFFER_USAGE_MDP;
	if (V4L2_TYPE_IS_OUTPUT(f->type) && MDP_COLOR_IS_RGB(fmt->mdp_color))
		profile = MDP_YCBCR_PROFILE_RGB;
	else
		profile = mdp_map_ycbcr_prof_mplane(f, fmt->mdp_color);
	frame->ycbcr_prof = profile;

	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		capture->crop.c.left = 0;
		capture->crop.c.top = 0;
		capture->crop.c.width = f->fmt.pix_mp.width;
		capture->crop.c.height = f->fmt.pix_mp.height;
		ctx->curr_param.colorspace = f->fmt.pix_mp.colorspace;
		ctx->curr_param.ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->curr_param.quant = f->fmt.pix_mp.quantization;
		ctx->curr_param.xfer_func = f->fmt.pix_mp.xfer_func;
	} else {
		capture->compose.left = 0;
		capture->compose.top = 0;
		capture->compose.width = f->fmt.pix_mp.width;
		capture->compose.height = f->fmt.pix_mp.height;
		ctx->curr_param.colorspace = f->fmt.pix_mp.colorspace;
		ctx->curr_param.ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->curr_param.quant = f->fmt.pix_mp.quantization;
		ctx->curr_param.xfer_func = f->fmt.pix_mp.xfer_func;
	}
	return 0;
}

static int mdp_cap_try_fmt_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mdp_cap_ctx *ctx = fh_to_ctx(fh);
	struct mdp_format *fmt;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;

	fmt = mdp_try_fmt_mplane(ctx->mdp_dev, f, &ctx->curr_param, ctx->id);
	if (!fmt)
		return -EINVAL;

	_mdp_cap_fill_v4l2_format(pix_mp, fmt);

	return 0;
}

static inline void mdp_cap_ctx_set_state(struct mdp_cap_ctx *ctx, u32 state)
{
	atomic_or(state, &ctx->curr_param.state);
}

static inline bool mdp_cap_ctx_is_state_set(struct mdp_cap_ctx *ctx, u32 mask)
{
	return ((atomic_read(&ctx->curr_param.state) & mask) == mask);
}

static void mdp_cap_buf_queue_ec(struct mdp_cap_ctx *ctx, struct v4l2_cap_buffer *buf)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->slock, flags);
	list_add_tail(&buf->list, &ctx->ec_queue);
	spin_unlock_irqrestore(&ctx->slock, flags);
}

static struct vb2_v4l2_buffer *_mdp_cap_get_next_buffer(struct mdp_cap_ctx *ctx)
{
	struct v4l2_cap_buffer *b;
	unsigned long flags;

	spin_lock_irqsave(&ctx->slock, flags);
	if (list_empty(&ctx->vb_queue)) {
		spin_unlock_irqrestore(&ctx->slock, flags);
		return NULL;
	}
	b = list_first_entry(&ctx->vb_queue, struct v4l2_cap_buffer, list);
	spin_unlock_irqrestore(&ctx->slock, flags);

	return &b->vb;
}

static struct vb2_v4l2_buffer *mdp_cap_buf_remove_vb(struct mdp_cap_ctx *ctx)
{
	struct v4l2_cap_buffer *b;
	unsigned long flags;

	spin_lock_irqsave(&ctx->slock, flags);
	if (list_empty(&ctx->vb_queue)) {
		spin_unlock_irqrestore(&ctx->slock, flags);
		return NULL;
	}
	b = list_first_entry(&ctx->vb_queue, struct v4l2_cap_buffer, list);
	list_del(&b->list);
	spin_unlock_irqrestore(&ctx->slock, flags);

	return &b->vb;
}

static struct vb2_v4l2_buffer *mdp_cap_buf_remove_ec(struct mdp_cap_ctx *ctx)
{
	struct v4l2_cap_buffer *b;
	unsigned long flags;

	spin_lock_irqsave(&ctx->slock, flags);
	if (list_empty(&ctx->ec_queue)) {
		spin_unlock_irqrestore(&ctx->slock, flags);
		return NULL;
	}
	b = list_first_entry(&ctx->ec_queue, struct v4l2_cap_buffer, list);
	list_del(&b->list);
	spin_unlock_irqrestore(&ctx->slock, flags);

	return &b->vb;
}

static void mdp_cap_process_done(void *priv, int vb_state)
{
	struct mdp_cap_ctx *ctx = priv;
	struct vb2_v4l2_buffer *dst_vbuf;

	dst_vbuf = (struct vb2_v4l2_buffer *)mdp_cap_buf_remove_ec(ctx);

	if (!dst_vbuf)/* add to avoid null pointer deference when stop streaming. */
		return;

	ctx->curr_param.frame_no = ctx->frame_count[MDP_CAP_DST];
	dst_vbuf->sequence = ctx->frame_count[MDP_CAP_DST]++;
	dst_vbuf->vb2_buf.timestamp = ktime_get_ns();

	vb2_buffer_done(&dst_vbuf->vb2_buf, vb_state);
}

void mdp_cap_job_finish(struct mdp_cap_ctx *ctx)
{
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_DONE;

	mdp_cap_process_done(ctx, vb_state);
}

void mdp_cap_all_buf_remove(struct mdp_cap_ctx *ctx)
{
	struct vb2_v4l2_buffer *vb;

	vb = mdp_cap_buf_remove_ec(ctx);
	while (vb) {
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
		vb = mdp_cap_buf_remove_ec(ctx);
	}

	vb = mdp_cap_buf_remove_vb(ctx);
	while (vb) {
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
		vb = mdp_cap_buf_remove_vb(ctx);
	}
}

static void mdp_cap_device_run(void *priv, struct v4l2_cap_buffer *buf)
{
	struct mdp_cap_ctx *ctx = priv;
	struct mdp_frame *frame;
	struct vb2_v4l2_buffer *dst_vb;
	struct vb2_buffer *src_vb = NULL;
	struct img_ipi_frameparam param = {};
	struct mdp_cmdq_param task = {};
	enum vb2_buffer_state vb_state = VB2_BUF_STATE_ERROR;
	enum mdp_cmdq_user u_id = MDP_CMDQ_USER_CAP;
	int ret;

	if (mdp_cap_ctx_is_state_set(ctx, MDP_M2M_CTX_ERROR)) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"mdp_cap_ctx is in error state\n");
		goto worker_end;
	}

	dst_vb = &buf->vb;
	param.frame_no = ctx->curr_param.frame_no;
	param.type = ctx->curr_param.type;
	param.num_inputs = 1;
	param.num_outputs = 1;

	/* setup capture device source config */
	frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	mdp_set_src_config(&param.inputs[0], frame,
			   src_vb, MDP_BUFFER_USAGE_HDMI_RX);

	/* setup capture device capture config */
	frame = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	mdp_set_dst_config(&param.outputs[0], frame,
			   &dst_vb->vb2_buf, MDP_BUFFER_USAGE_HDMI_RX);

	ctx->pp_enable = false;
	if (mdp_check_pp_enable(ctx->mdp_dev, frame)) {
		param.type = MDP_STREAM_TYPE_DUAL_BITBLT;
		ctx->pp_enable = true;
	}

	ret = mdp_vpu_process(&ctx->mdp_dev->vpu, &param, MDP_VPU_UID_CAP);
	if (ret) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"VPU MDP process failed: %d\n", ret);
		goto worker_end;
	}

	task.config = ctx->mdp_dev->vpu.config[MDP_VPU_UID_CAP];
	task.param = &param;
	task.composes[0] = &frame->compose;
	task.cmdq_cb = NULL;
	task.cb_data = NULL;
	task.mdp_ctx = ctx;
	task.user = u_id;

	ret = mdp_cmdq_send(ctx->mdp_dev, &task);

	if (ret) {
		dev_err(&ctx->mdp_dev->pdev->dev,
			"CMDQ sendtask failed: %d\n", ret);
		goto worker_end;
	}

	return;

worker_end:
	mdp_cap_process_done(ctx, vb_state);
}

static int mdp_cap_queue_setup(struct vb2_queue *q,
			       unsigned int *num_buffers,
			       unsigned int *num_planes, unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct mdp_cap_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_pix_format_mplane *pix_mp;
	u32 i;

	pix_mp = &ctx_get_frame(ctx, q->type)->format.fmt.pix_mp;

	/* from VIDIOC_CREATE_BUFS */
	if (*num_planes) {
		if (*num_planes != pix_mp->num_planes)
			return -EINVAL;
		for (i = 0; i < pix_mp->num_planes; ++i)
			if (sizes[i] < pix_mp->plane_fmt[i].sizeimage)
				return -EINVAL;
	} else {/* from VIDIOC_REQBUFS */
		*num_planes = pix_mp->num_planes;
		for (i = 0; i < pix_mp->num_planes; ++i)
			sizes[i] = pix_mp->plane_fmt[i].sizeimage;
	}

	return 0;
}

static int mdp_cap_buf_prepare(struct vb2_buffer *vb)
{
	struct mdp_cap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_pix_format_mplane *pix_mp;
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);
	u32 i;

	v4l2_buf->field = V4L2_FIELD_NONE;

	if (V4L2_TYPE_IS_CAPTURE(vb->type)) {
		pix_mp = &ctx_get_frame(ctx, vb->type)->format.fmt.pix_mp;
		for (i = 0; i < pix_mp->num_planes; ++i) {
			vb2_set_plane_payload(vb, i,
					      pix_mp->plane_fmt[i].sizeimage);
		}
	}
	return 0;
}

static void mdp_cap_handler(struct mdp_cap_ctx *ctx)
{
	struct v4l2_cap_buffer *buf, *tmp;

	if (list_empty(&ctx->vb_queue))
		return;

	list_for_each_entry_safe(buf, tmp, &ctx->vb_queue, list)
		if (!atomic_read(&ctx->mdp_dev->cap_discard)) {
			mdp_cap_buf_remove_vb(ctx);
			mdp_cap_buf_queue_ec(ctx, buf);

			mdp_cap_device_run(ctx, buf);
			ctx->cap_status = CAP_STATUS_START;
		}
}

static void mdp_cap_wait_done_and_disable(struct mdp_cap_ctx *ctx)
{
	u8 pp_used = (ctx->pp_enable) ? MDP_PP_USED_2 : MDP_PP_USED_1;
	int i, ret;

	ret = wait_event_timeout(ctx->mdp_dev->callback_wq,
				 !atomic_read(&ctx->mdp_dev->job_count[MDP_CMDQ_USER_CAP]),
				 2 * HZ);
	if (ret == 0)
		dev_err(&ctx->mdp_dev->pdev->dev,
			"flushed cap_dev cmdq task incomplete, count=%d\n",
			atomic_read(&ctx->mdp_dev->job_count[MDP_CMDQ_USER_CAP]));

	for (i = 0; i < pp_used; i++) {
		mtk_mutex_unprepare(ctx->mutex[i]);
		mdp_comp_clocks_off(&ctx->mdp_dev->pdev->dev, ctx->comps[i],
				    ctx->num_comps);
		kfree(ctx->comps[i]);
		ctx->comps[i] = NULL;
	}
}

static int mdp_cap_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mdp_cap_ctx *ctx = vb2_get_drv_priv(q);
	struct mdp_frame *capture;
	struct vb2_queue *vq;
	int ret;
	bool out_streaming, cap_streaming;

	if (V4L2_TYPE_IS_CAPTURE(q->type))
		ctx->frame_count[MDP_CAP_DST] = 0;

	capture = ctx_get_frame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	vq = &ctx->cap_q;
	cap_streaming = vb2_is_streaming(vq);

	/* Check to see if scaling ratio is within supported range */
	if (V4L2_TYPE_IS_CAPTURE(q->type) && out_streaming) {
		ret = mdp_check_scaling_ratio(&capture->crop.c,
					      &capture->compose,
					      capture->rotation,
					      ctx->curr_param.limit);
		if (ret) {
			dev_err(&ctx->mdp_dev->pdev->dev,
				"Out of scaling range\n");
			return ret;
		}
	}

	if (!mdp_cap_ctx_is_state_set(ctx, MDP_VPU_INIT)) {
		ret = mdp_vpu_get_locked(ctx->mdp_dev);
		if (ret) {
			dev_err(&ctx->mdp_dev->pdev->dev,
				"VPU init failed %d\n", ret);
			return -EINVAL;
		}
		mdp_cap_ctx_set_state(ctx, MDP_VPU_INIT);
	}

	mdp_cap_handler(ctx);

	return 0;
}

static void mdp_cap_stop_streaming(struct vb2_queue *q)
{
	struct mdp_cap_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	ctx->cap_status = CAP_STATUS_STOP;
	atomic_set(&ctx->mdp_dev->cap_discard, 1);

	mdp_cap_wait_done_and_disable(ctx);
	mdp_cap_all_buf_remove(ctx);
}

static void mdp_cap_buf_queue(struct vb2_buffer *vb)
{
	struct mdp_cap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct v4l2_cap_buffer *buf = container_of(v4l2_buf, struct v4l2_cap_buffer, vb);
	unsigned long flags;

	v4l2_buf->field = V4L2_FIELD_NONE;

	spin_lock_irqsave(&ctx->slock, flags);
	if (!atomic_read(&ctx->mdp_dev->cap_discard))
		list_add_tail(&buf->list, &ctx->vb_queue);
	spin_unlock_irqrestore(&ctx->slock, flags);

	if (ctx->cap_status == CAP_STATUS_START)
		mdp_cap_handler(ctx);
}

static const struct vb2_ops mdp_cap_qops = {
	.queue_setup	= mdp_cap_queue_setup,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_prepare	= mdp_cap_buf_prepare,
	.start_streaming = mdp_cap_start_streaming,
	.stop_streaming	= mdp_cap_stop_streaming,
	.buf_queue	= mdp_cap_buf_queue,
};

static int mdp_cap_queue_init(void *priv)
{
	struct mdp_cap_ctx *ctx = priv;
	int ret;

	ctx->cap_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->cap_q.io_modes = VB2_MMAP | VB2_DMABUF;
	ctx->cap_q.ops = &mdp_cap_qops;
	ctx->cap_q.mem_ops = &vb2_dma_contig_memops;
	ctx->cap_q.drv_priv = ctx;
	ctx->cap_q.buf_struct_size = sizeof(struct v4l2_cap_buffer);
	ctx->cap_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	ctx->cap_q.dev = &ctx->mdp_dev->pdev->dev;
	ctx->cap_q.lock = &ctx->ctx_lock;
	ctx->cap_q.min_buffers_needed = MIN_BUF_NEED;

	ret = vb2_queue_init(&ctx->cap_q);

	if (ret)
		return ret;

	INIT_LIST_HEAD(&ctx->vb_queue);
	INIT_LIST_HEAD(&ctx->ec_queue);
	spin_lock_init(&ctx->slock);

	return 0;
}

static int mdp_cap_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct mdp_dev *mdp = video_get_drvdata(vdev);
	struct mdp_cap_ctx *ctx;
	struct device *dev = &mdp->pdev->dev;
	int ret;
	struct v4l2_format default_format = {};
	const struct mdp_limit *limit = mdp->mdp_data->def_limit;

	if (mutex_lock_interruptible(&mdp->cap_lock))
		return -ERESTARTSYS;

	if (mdp->cap_open_count > 0) {
		dev_err(dev, "Failed to open, the capture device is in use.\n");
		mutex_unlock(&mdp->cap_lock);
		return -EBUSY;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		mutex_unlock(&mdp->cap_lock);
		return -ENOMEM;
	}

	ctx->id = ida_alloc(&mdp->mdp_ida, GFP_KERNEL);
	ctx->mdp_dev = mdp;

	v4l2_fh_init(&ctx->fh, vdev);
	file->private_data = &ctx->fh;

	v4l2_fh_add(&ctx->fh);

	mutex_init(&ctx->ctx_lock);

	/* initialize mdp capture device queue*/
	mdp_cap_queue_init(ctx);
	mdp->cap_vdev->queue = &ctx->cap_q;

	ret = mdp_frameparam_init(mdp, &ctx->curr_param);
	if (ret) {
		dev_err(dev, "Failed to initialize mdp parameter\n");
		goto err_release_handler;
	}

	mdp->cap_open_count++;
	mutex_unlock(&mdp->cap_lock);

	/* Default format */
	default_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	switch (mdp->rx_cap_intf.rx_color_space) {
	case HDMIRX_INTF_CS_RGB:
		default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_RGB24;
		break;
	case HDMIRX_INTF_CS_YUV444:
		default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
		break;
	case HDMIRX_INTF_CS_YUV422:
		default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV422M;
		break;
	case HDMIRX_INTF_CS_YUV420:
		default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
		break;
	default:
		default_format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
		break;
	}

	default_format.fmt.pix_mp.width = mdp->rx_cap_intf.rx_width;
	default_format.fmt.pix_mp.height = mdp->rx_cap_intf.rx_height;
	mdp_cap_s_fmt_mplane(file, &ctx->fh, &default_format);

	default_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	default_format.fmt.pix_mp.width = limit->out_limit.wmin;
	default_format.fmt.pix_mp.height = limit->out_limit.hmin;
	mdp_cap_s_fmt_mplane(file, &ctx->fh, &default_format);

	return 0;

err_release_handler:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&mdp->cap_lock);

	return ret;
}

static int mdp_cap_release(struct file *file)
{
	struct mdp_cap_ctx *ctx = fh_to_ctx(file->private_data);
	struct mdp_dev *mdp = video_drvdata(file);
	struct device *dev = &mdp->pdev->dev;

	mutex_lock(&mdp->cap_lock);
	if (mdp_cap_ctx_is_state_set(ctx, MDP_VPU_INIT))
		mdp_vpu_put_locked(mdp);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	ida_free(&mdp->mdp_ida, ctx->id);
	if (--mdp->cap_open_count < 0)
		mdp->cap_open_count = 0;
	mutex_unlock(&mdp->cap_lock);

	kfree(ctx);

	return 0;
}

static int mdp_cap_streamon(struct file *file, void *priv, enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(file);
	struct mdp_cap_ctx *ctx = fh_to_ctx(priv);
	int ret;

	atomic_set(&ctx->mdp_dev->cap_discard, 0);

	ret = vb2_streamon(vdev->queue, type);
	return ret;
}

static int mdp_cap_enum_framesizes(struct file *file, void *priv,
				   struct v4l2_frmsizeenum *fsize)
{
	struct video_device *vdev = video_devdata(file);
	struct mdp_dev *mdp = video_get_drvdata(vdev);
	const struct mdp_limit *limit = mdp->mdp_data->def_limit;

	if (fsize->index)
		return -EINVAL;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = limit->cap_limit.wmin;
	fsize->stepwise.max_width = limit->cap_limit.wmax;
	fsize->stepwise.step_width = 2;
	fsize->stepwise.min_height = limit->cap_limit.hmin;
	fsize->stepwise.max_height = limit->cap_limit.hmax;
	fsize->stepwise.step_height = 2;

	return 0;
}

static int mdp_cap_enum_frameintervals(struct file *file, void *priv,
				       struct v4l2_frmivalenum *fival)
{
	struct video_device *vdev = video_devdata(file);
	struct mdp_dev *mdp = video_get_drvdata(vdev);

	if (fival->index != 0)
		return -EINVAL;
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = mdp->rx_cap_intf.rx_frame_rate;

	return 0;
}

static const struct v4l2_ioctl_ops mdp_cap_ioctl_ops = {
	.vidioc_querycap		= mdp_cap_querycap,
	.vidioc_enum_framesizes		= mdp_cap_enum_framesizes,
	.vidioc_enum_frameintervals	= mdp_cap_enum_frameintervals,
	.vidioc_enum_fmt_vid_cap	= mdp_cap_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= mdp_cap_g_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= mdp_cap_s_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= mdp_cap_try_fmt_mplane,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_streamon		= mdp_cap_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static const struct v4l2_file_operations mdp_cap_fops = {
	.owner		= THIS_MODULE,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= vb2_fop_mmap,
	.open		= mdp_cap_open,
	.release	= mdp_cap_release,
};

void mdp_capture_device_release(struct video_device *vdev)
{
	/* Should not release mdp context, do nothing currently */
}

int mdp_capture_device_register(struct mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;
	int ret = 0;

	mdp->cap_vdev = video_device_alloc();
	if (!mdp->cap_vdev) {
		dev_err(dev, "Failed to allocate capture video device\n");
		ret = -ENOMEM;
		goto err_video_alloc;
	}
	mdp->cap_vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
		V4L2_CAP_STREAMING;
	mdp->cap_vdev->fops = &mdp_cap_fops;
	mdp->cap_vdev->ioctl_ops = &mdp_cap_ioctl_ops;
	mdp->cap_vdev->release = mdp_capture_device_release;
	mdp->cap_vdev->lock = &mdp->cap_lock;
	mdp->cap_vdev->vfl_dir = VFL_DIR_RX;
	mdp->cap_vdev->v4l2_dev = &mdp->v4l2_dev;
	snprintf(mdp->cap_vdev->name, sizeof(mdp->cap_vdev->name), "%s:cap",
		 MDP_MODULE_NAME);
	video_set_drvdata(mdp->cap_vdev, mdp);

	ret = video_register_device(mdp->cap_vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_err(dev, "Failed to register video device\n");
		goto err_video_register;
	}

	v4l2_info(&mdp->v4l2_dev, "Capture Driver registered as /dev/video%d",
		  mdp->cap_vdev->num);
	return 0;

err_video_register:
	video_device_release(mdp->cap_vdev);
err_video_alloc:

	return ret;
}

void mdp_capture_device_unregister(struct mdp_dev *mdp)
{
	struct device *dev = &mdp->pdev->dev;

	if (mdp->cap_vdev) {
		/* Unregister Capture Driver */
		video_unregister_device(mdp->cap_vdev);
		video_device_release(mdp->cap_vdev);
		mdp->cap_vdev = NULL;
	}
}

static int mdp_cap_probe(struct hdmirx_capture_interface *intf)
{
	struct mdp_dev *mdp = (struct mdp_dev *)intf->priv;
	int ret;

	dev_dbg(&mdp->pdev->dev, "w[%u], h[%u], fr[%u], cs[%d]\n",
		intf->width, intf->height, intf->frame_rate, intf->color_space);

	atomic_set(&mdp->cap_discard, 0);

	mdp->rx_cap_intf.rx_width = intf->width;
	mdp->rx_cap_intf.rx_height = intf->height;
	mdp->rx_cap_intf.rx_frame_rate = intf->frame_rate;
	mdp->rx_cap_intf.rx_color_space = intf->color_space;

	ret = mdp_capture_device_register(mdp);
	if (ret)
		v4l2_err(&mdp->v4l2_dev, "Failed to register cap device, ret=%d\n", ret);

	return ret;
}

static void mdp_cap_disconnect(struct hdmirx_capture_interface *intf)
{
	struct mdp_dev *mdp = (struct mdp_dev *)intf->priv;
	enum mdp_cmdq_user u_id = MDP_CMDQ_USER_CAP;
	int ret;
	struct vb2_queue *cap_queue;
	struct mdp_cap_ctx *ctx;

	if (!mdp->cap_vdev) {
		dev_err(&mdp->pdev->dev, "already disconnected! ignore it.\n");
		return;
	}

	cap_queue = mdp->cap_vdev->queue;
	if (cap_queue)
		ctx = container_of(cap_queue, struct mdp_cap_ctx, cap_q);

	/* stop all capture device jobs */
	atomic_set(&mdp->cap_discard, 1);
	if (atomic_read(&mdp->job_count[u_id]))
		mdp_cap_wait_done_and_disable(ctx);

	mdp_capture_device_unregister(mdp);
}

static const struct hdmirx_capture_ops mdp_cap_driver_ops = {
	.probe		= mdp_cap_probe,
	.disconnect	= mdp_cap_disconnect,
};

struct mdp_cap_driver mdp_cap_driver = {
	.driver = {
		.name	= "mdp3cap",
		.ops	= &mdp_cap_driver_ops,
	},
};

int mdp_cap_init(struct mdp_dev *mdp)
{
	struct device_node *node;
	struct platform_device *hdmirx_pdev;
	int ret;

	/* find mtk-hdmirx node */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8195-hdmirx");
	if (!node) {
		dev_dbg(&mdp->pdev->dev, "find hdmirx node failed\n");
		return -ENODEV;
	}

	hdmirx_pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (WARN_ON(!hdmirx_pdev)) {
		dev_err(&mdp->pdev->dev, "find hdmirx pdev failed\n");
		return -ENODEV;
	}

	/* register data for capture device interface */
	mdp_cap_driver.driver.hdmirx_dev = &hdmirx_pdev->dev;
	mdp_cap_driver.driver.priv = (void *)mdp;

	ret = hdmirx_register_capture_driver(&mdp_cap_driver.driver);
	if (ret < 0) {
		dev_err(&mdp->pdev->dev, "hdmirx_register_capture_driver failed\n");
		return ret;
	}

	return 0;
}

void mdp_cap_deinit(void)
{
	hdmirx_unregister_capture_driver(&mdp_cap_driver.driver);
	mdp_cap_driver.driver.hdmirx_dev = NULL;
	mdp_cap_driver.driver.priv = NULL;
}
