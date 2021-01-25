/*
 *    VSI V4L2 kernel driver main entrance.
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

#define DRIVER_NAME	"vsiv4l2"

struct platform_device *gvsidev;
static s32 self;
struct idr inst_array;
static struct device *vsidaemondev;
static struct mutex vsi_ctx_array_lock;		//it only protect ctx between release from app and msg from daemon
static ulong ctx_seqid;

static void release_ctx(struct vsi_v4l2_ctx *ctx, int notifydaemon)
{
	int ret = 0;

	if (notifydaemon == 1 && (ctx->status != VSI_STATUS_INIT || ctx->error < 0)) {
		if (isdecoder(ctx))
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_DESTROY_DEC, NULL);
		else
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMOFF, NULL);
	}
	/*vsi_vpu_buf obj is freed here, together with all buffer memory */
	return_all_buffers(&ctx->input_que, VB2_BUF_STATE_DONE, 0);
	return_all_buffers(&ctx->output_que, VB2_BUF_STATE_DONE, 0);
	if (mutex_lock_interruptible(&vsi_ctx_array_lock))
		return;
	idr_remove(&inst_array, CTX_ARRAY_ID(ctx->ctxid));
	mutex_unlock(&vsi_ctx_array_lock);
	vb2_queue_release(&ctx->input_que);
	vb2_queue_release(&ctx->output_que);
	v4l2_ctrl_handler_free(&ctx->ctrlhdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	ctx = NULL;
}

void vsi_remove_ctx(struct vsi_v4l2_ctx *ctx)
{
	if (mutex_lock_interruptible(&vsi_ctx_array_lock))
		return;
	idr_remove(&inst_array, CTX_ARRAY_ID(ctx->ctxid));
	mutex_unlock(&vsi_ctx_array_lock);
}

struct vsi_v4l2_ctx *vsi_create_ctx(void)
{
	struct vsi_v4l2_ctx *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return NULL;
	if (mutex_lock_interruptible(&vsi_ctx_array_lock)) {
		kfree(ctx);
		return NULL;
	}
	ctx->ctxid = idr_alloc(&inst_array, (void *)ctx, 1, 0, GFP_KERNEL);
	if (ctx->ctxid < 0) {
		kfree(ctx);
		ctx = NULL;
	} else {
		ctx_seqid++;
		if (ctx_seqid >= CTX_SEQID_UPLIMT)
			ctx_seqid = 1;
		pr_debug("create ctx with %lx:%lx", ctx->ctxid, ctx_seqid);
		ctx->ctxid |= (ctx_seqid << 32);
	}
	mutex_unlock(&vsi_ctx_array_lock);
	init_waitqueue_head(&ctx->retbuf_queue);

	return ctx;
}

static struct vsi_v4l2_ctx *find_ctx(unsigned long ctxid)
{
	unsigned long id = CTX_ARRAY_ID(ctxid);
	unsigned long seq = CTX_SEQ_ID(ctxid);
	struct vsi_v4l2_ctx *ctx;

	if (mutex_lock_interruptible(&vsi_ctx_array_lock))
		return NULL;
	ctx  = (struct vsi_v4l2_ctx *)idr_find(&inst_array, id);
	mutex_unlock(&vsi_ctx_array_lock);
	pr_debug("search for ctx %lx", ctxid);
	if (ctx && (CTX_SEQ_ID(ctx->ctxid)  == seq))
		return ctx;
	else
		return NULL;
}

int vsi_v4l2_reset_ctx(struct vsi_v4l2_ctx *ctx)
{
	int ret = 0;

	if (ctx->status != VSI_STATUS_INIT) {
		if (isdecoder(ctx)) {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_DESTROY_DEC, NULL);
			ctx->queued_srcnum = ctx->buffed_capnum = ctx->buffed_cropcapnum = 0;
			wake_up_interruptible_all(&ctx->retbuf_queue);
			ctx->flag = CTX_FLAG_DEC;
		} else {
			ret = vsiv4l2_execcmd(ctx, V4L2_DAEMON_VIDIOC_STREAMOFF, NULL);
			ctx->flag = CTX_FLAG_ENC;
		}
		set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);
		return_all_buffers(&ctx->input_que, VB2_BUF_STATE_DONE, 0);
		return_all_buffers(&ctx->output_que, VB2_BUF_STATE_DONE, 0);
		ctx->status = VSI_STATUS_INIT;
		ctx->error = 0;
	}
	return ret;
}

int v4l2_release(struct file *filp)
{
	struct vsi_v4l2_ctx *ctx = fh_to_ctx(filp->private_data);

	/*normal streaming end should fall here*/
	vsi_clear_daemonmsg(CTX_ARRAY_ID(ctx->ctxid));
	release_ctx(ctx, 1);
	vsi_v4l2_quitinstance();
	return 0;
}

//default ctrls are in v4l2-controls.h
//VIDIOC_QUERYCTRL, VIDIOC_G_EXT_CTRLS/VIDIOC_S_EXT_CTRLS
//for get and set print_control in v4l2-utils could be an example internal
// ctrl usage is through v4l2_ctrl_find()/v4l2_ctrl_g_ctrl()/v4l2_ctrl_s_ctrl
static int vsi_v4l2_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret;
	struct vsi_v4l2_ctx *ctx = ctrl_to_ctx(ctrl);

	pr_debug("%s:%d=%d", __func__, ctrl->id, ctrl->val);
	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.intraPicRate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		ret = vsi_set_profile(ctx, ctrl->id, ctrl->val);
		return ret;
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		ctx->mediacfg.encparams.general.bitPerSecond = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		ret = get_Level(ctx, 0, 1, ctrl->val);
		if (ret >= 0)
			ctx->mediacfg.encparams.specific.enc_h26x_cmd.avclevel = ret;
		else
			return ret;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		ret = get_Level(ctx, 1, 1, ctrl->val);
		if (ret >= 0)
			ctx->mediacfg.encparams.specific.enc_h26x_cmd.hevclevel = ret;
		else
			return ret;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.qpMax = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.qpMin = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		if (ctrl->val != 0)
			return -EINVAL;
		/*in fact nothing to do*/
		break;
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.bFrameQpDelta = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		if (ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
			ctx->mediacfg.encparams.specific.enc_h26x_cmd.hrdConformance = 0;
		else
			ctx->mediacfg.encparams.specific.enc_h26x_cmd.hrdConformance = 1;
		break;
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		set_bit(CTX_FLAG_FORCEIDR_BIT, &ctx->flag);
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		break;
	case V4L2_CID_DIS_REORDER:
		ctx->mediacfg.decparams.io_buffer.no_reordering_decoding = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		ctx->mediacfg.multislice_mode = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.sliceSize = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.picRc = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.ctbRc = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.qpHdrI = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP:
		ctx->mediacfg.encparams.specific.enc_h26x_cmd.qpHdrP = ctrl->val;
		break;
	case V4L2_CID_ROTATE:
		switch (ctrl->val) {
		case 90:
			ctx->mediacfg.encparams.general.rotation = VCENC_ROTATE_90L;
			break;
		case 180:
			ctx->mediacfg.encparams.general.rotation = VCENC_ROTATE_180R;
			break;
		case 270:
			ctx->mediacfg.encparams.general.rotation = VCENC_ROTATE_90R;
			break;
		case 0:
		default:
			ctx->mediacfg.encparams.general.rotation = VCENC_ROTATE_0;
			break;
		}
		break;
	case V4L2_CID_ROI:
		if (ctrl->p_new.p)
			vsiv4l2_setROI(ctx, ctrl->p_new.p);
		break;
	case V4L2_CID_IPCM:
		if (ctrl->p_new.p)
			vsiv4l2_setIPCM(ctx, ctrl->p_new.p);
		break;
	default:
		return 0;
	}
	set_bit(CTX_FLAG_CONFIGUPDATE_BIT, &ctx->flag);
	return 0;
}

static int vsi_v4l2_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vsi_v4l2_ctx *ctx = ctrl_to_ctx(ctrl);

	if (!vsi_v4l2_daemonalive())
		return -ENODEV;
	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		ctrl->val = ctx->mediacfg.minbuf_4capture;	//these two may come from resoultion change
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		ctrl->val = ctx->mediacfg.minbuf_4output;
		break;
	case V4L2_CID_ROI_COUNT:
		ctrl->val = vsiv4l2_getROIcount();
		break;
	case V4L2_CID_IPCM_COUNT:
		ctrl->val = vsiv4l2_getIPCMcount();
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
/********* for ext ctrl *************/
static bool vsi_ctrl_equal(const struct v4l2_ctrl *ctrl, u32 idx,
		      union v4l2_ctrl_ptr ptr1,
		      union v4l2_ctrl_ptr ptr2)
{
	//always update now, fix it later
	return 0;
}

static void vsi_ctrl_init(const struct v4l2_ctrl *ctrl, u32 idx,
		     union v4l2_ctrl_ptr ptr)
{
	void *p = ptr.p + idx * ctrl->elem_size;

	pr_debug("%s:%d", __func__, idx);
	memset(p, 0, ctrl->elem_size);
}

static void vsi_ctrl_log(const struct v4l2_ctrl *ctrl)
{
	//do nothing now
}

static int vsi_ctrl_validate(const struct v4l2_ctrl *ctrl, u32 idx,
			union v4l2_ctrl_ptr ptr)
{
	//always true
	return 0;
}

static const struct v4l2_ctrl_type_ops vsi_type_ops = {
	.equal = vsi_ctrl_equal,
	.init = vsi_ctrl_init,
	.log = vsi_ctrl_log,
	.validate = vsi_ctrl_validate,
};
/********* for ext ctrl *************/

static const struct v4l2_ctrl_ops vsi_ctrl_ops = {
	.s_ctrl = vsi_v4l2_s_ctrl,
	.g_volatile_ctrl = vsi_v4l2_g_volatile_ctrl,
};

static struct v4l2_ctrl_config vsi_v4l2_ctrl_defs[] = {
	{
		.ops = &vsi_ctrl_ops,
		.id = V4L2_CID_DIS_REORDER,
		.name = "frame disable reoder ctrl",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &vsi_ctrl_ops,
		.id = V4L2_CID_ROI_COUNT,
		.name = "get max ROI region number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = V4L2_MAX_ROI_REGIONS,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &vsi_ctrl_ops,
		.type_ops = &vsi_type_ops,
		.id = V4L2_CID_ROI,
		.name = "vsi priv v4l2 roi params set",
		.type = VSI_V4L2_CMPTYPE_ROI,
		.min = 0,
		.max = V4L2_MAX_ROI_REGIONS,
		.step = 1,
		.def = 0,
		.elem_size = sizeof(struct v4l2_enc_roi_params),
	},
	{
		.ops = &vsi_ctrl_ops,
		.id = V4L2_CID_IPCM_COUNT,
		.name = "get max IPCM region number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = V4L2_MAX_IPCM_REGIONS,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &vsi_ctrl_ops,
		.type_ops = &vsi_type_ops,
		.id = V4L2_CID_IPCM,
		.name = "vsi priv v4l2 ipcm params set",
		.type = VSI_V4L2_CMPTYPE_IPCM,
		.min = 0,
		.max = V4L2_MAX_IPCM_REGIONS,
		.step = 1,
		.def = 0,
		.elem_size = sizeof(struct v4l2_enc_ipcm_params),
	},
	/* kernel defined controls */
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 1,
		.max = MAX_INTRA_PIC_RATE,
		.step = 1,
		.def = DEFAULT_INTRA_PIC_RATE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 10000,
		.max = 288000000,
		.step = 1,
		.def = 2097152,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.max = V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH,
		.def = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_VP8_PROFILE_0,
		.max = V4L2_MPEG_VIDEO_VP8_PROFILE_3,
		.def = V4L2_MPEG_VIDEO_VP8_PROFILE_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		.max = V4L2_MPEG_VIDEO_VP9_PROFILE_3,
		.def = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min =  V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		.max = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		.def = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.max = V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		.def = V4L2_MPEG_VIDEO_H264_LEVEL_5_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		.max = V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1,
		.def = V4L2_MPEG_VIDEO_HEVC_LEVEL_5,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = 51,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.max = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		.def = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0,
		.step = 1,
		.def = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.max = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR,
		.def = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE,	//volatile contains read
		.min = 1,
		.max = MAX_MIN_BUFFERS_FOR_CAPTURE,
		.step = 1,
		.def = 1,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.min = 1,
		.max = MAX_MIN_BUFFERS_FOR_OUTPUT,
		.step = 1,
		.def = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
		.type = V4L2_CTRL_TYPE_BUTTON,
		.min = 0,
		.max = 0,
		.step = 0,
		.def = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.min = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.max = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB,
		.def = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 1,
		.max = 120,		//1920 div 16
		.step = 1,
		.def = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 51,
		.step = 1,
		.def = DEFAULT_QP,
	},
	{
		.id = V4L2_CID_ROTATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 270,
		.step = 90,
		.def = 0,
	},
};

#define is_vsi_ctrl(x) ((V4L2_CTRL_ID2WHICH(x) == V4L2_CTRL_CLASS_USER) && \
			  V4L2_CTRL_DRIVER_PRIV(x))

int vsi_setup_ctrls(struct v4l2_ctrl_handler *handler)
{
	int i, ctrl_num = ARRAY_SIZE(vsi_v4l2_ctrl_defs);
	struct v4l2_ctrl *ctrl = NULL;

	v4l2_ctrl_handler_init(handler, ctrl_num);

	if (handler->error)
		return handler->error;

	for (i = 0; i < ctrl_num; i++) {
		vsi_v4l2_update_ctrlcfg(&vsi_v4l2_ctrl_defs[i]);
		if (is_vsi_ctrl(vsi_v4l2_ctrl_defs[i].id))
			ctrl = v4l2_ctrl_new_custom(handler, &vsi_v4l2_ctrl_defs[i], NULL);
		else {
			if (vsi_v4l2_ctrl_defs[i].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(handler, &vsi_ctrl_ops,
					vsi_v4l2_ctrl_defs[i].id,
					vsi_v4l2_ctrl_defs[i].max,
					0,
					vsi_v4l2_ctrl_defs[i].def);
			} else {
				ctrl = v4l2_ctrl_new_std(handler,
					&vsi_ctrl_ops,
					vsi_v4l2_ctrl_defs[i].id,
					vsi_v4l2_ctrl_defs[i].min,
					vsi_v4l2_ctrl_defs[i].max,
					vsi_v4l2_ctrl_defs[i].step,
					vsi_v4l2_ctrl_defs[i].def);
			}
		}
		if (ctrl && (vsi_v4l2_ctrl_defs[i].flags & V4L2_CTRL_FLAG_VOLATILE))
			ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

		if (handler->error) {
			pr_err("fail to set ctrl %d:%d", i, handler->error);
			break;
		}
	}

	v4l2_ctrl_handler_setup(handler);
	return handler->error;
}

/*orphan error msg from daemon write, should not call daemon back*/

int vsi_v4l2_handle_picconsumed(unsigned long ctxid)
{
	struct vsi_v4l2_ctx *ctx;
	struct v4l2_event event = {
		.type = V4L2_EVENT_SKIP,
	};

	pr_debug("%s:%lx got picconsumed event", __func__, ctxid);
	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -1;

	v4l2_event_queue_fh(&ctx->fh, &event);
	return 0;
}

int vsi_v4l2_handleerror(unsigned long ctxid, int error)
{
	struct vsi_v4l2_ctx *ctx;
	struct v4l2_event event = {
		.type = V4L2_EVENT_CODEC_ERROR,
	};

	pr_err("%s:%lx got error %d", __func__, ctxid, error);
	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -1;

	ctx->error = (error > 0 ? -error:error);
	v4l2_event_queue_fh(&ctx->fh, &event);
	wake_up_interruptible_all(&ctx->retbuf_queue);
	wake_up_interruptible_all(&ctx->input_que.done_wq);
	wake_up_interruptible_all(&ctx->output_que.done_wq);
	wake_up_interruptible_all(&ctx->fh.wait);

	return 0;
}

int vsi_v4l2_notify_reschange(struct vsi_v4l2_msg *pmsg)
{
	unsigned long ctxid = pmsg->inst_id;
	struct vsi_v4l2_ctx *ctx;

	struct v4l2_event event = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};
	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -ESRCH;

	if (isdecoder(ctx)) {
		struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;

		pr_debug("ctx %lx sending event res change:%d:%d", ctx->ctxid, ctx->status, list_empty(&ctx->output_que.done_list));
		pcfg->minbuf_4output =
			pcfg->minbuf_4capture = pmsg->params.dec_params.dec_info.dec_info.needed_dpb_nums;
		pcfg->sizeimagedst[0] =
			pmsg->params.dec_params.io_buffer.OutBufSize;
		if (ctx->status == DEC_STATUS_DECODING && !list_empty(&ctx->output_que.done_list)) {
			ctx->status = DEC_STATUS_RESCHANGE;
			pcfg->decparams_bkup.dec_info = pmsg->params.dec_params.dec_info;
			pcfg->decparams_bkup.io_buffer.srcwidth = pmsg->params.dec_params.io_buffer.srcwidth;
			pcfg->decparams_bkup.io_buffer.srcheight = pmsg->params.dec_params.io_buffer.srcheight;
			pcfg->decparams_bkup.io_buffer.output_width = pmsg->params.dec_params.io_buffer.output_width;
			pcfg->decparams_bkup.io_buffer.output_height = pmsg->params.dec_params.io_buffer.output_height;
		} else {
			pcfg->decparams.dec_info.dec_info = pmsg->params.dec_params.dec_info.dec_info;
			pcfg->decparams.dec_info.io_buffer.srcwidth = pmsg->params.dec_params.dec_info.io_buffer.srcwidth;
			pcfg->decparams.dec_info.io_buffer.srcheight = pmsg->params.dec_params.dec_info.io_buffer.srcheight;
			pcfg->decparams.dec_info.io_buffer.output_width = pmsg->params.dec_params.dec_info.io_buffer.output_width;
			pcfg->decparams.dec_info.io_buffer.output_height = pmsg->params.dec_params.dec_info.io_buffer.output_height;
			v4l2_event_queue_fh(&ctx->fh, &event);
		}
		if (pmsg->params.dec_params.dec_info.dec_info.colour_description_present_flag)
			dec_updatevui(&pmsg->params.dec_params.dec_info.dec_info, &pcfg->decparams.dec_info.dec_info);
		set_bit(CTX_FLAG_SRCCHANGED_BIT, &ctx->flag);
	}
	return 0;
}

int vsi_v4l2_handle_warningmsg(struct vsi_v4l2_msg *pmsg)
{
	unsigned long ctxid = pmsg->inst_id;
	struct vsi_v4l2_ctx *ctx;
	struct v4l2_event event = {
		.type = V4L2_EVENT_INVALID_OPTION,
	};

	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -ESRCH;
	v4l2_event_queue_fh(&ctx->fh, &event);
	return 0;
}

int vsi_v4l2_handle_cropchange(struct vsi_v4l2_msg *pmsg)
{
	unsigned long ctxid = pmsg->inst_id;
	struct vsi_v4l2_ctx *ctx;

	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -ESRCH;

	if (isdecoder(ctx)) {
		struct vsi_v4l2_mediacfg *pcfg = &ctx->mediacfg;
		struct v4l2_event event = {
			.type = V4L2_EVENT_CROPCHANGE,
		};

		pr_debug("ctx %lx sending crop res change:%d:%d", ctx->ctxid, ctx->status, ctx->buffed_cropcapnum);
		if (ctx->status == DEC_STATUS_DECODING && ctx->buffed_cropcapnum > 0) {
			pcfg->decparams_bkup.dec_info.dec_info.frame_width = pmsg->params.dec_params.pic_info.pic_info.width;
			pcfg->decparams_bkup.dec_info.dec_info.frame_height = pmsg->params.dec_params.pic_info.pic_info.height;
			pcfg->decparams_bkup.dec_info.dec_info.visible_rect.left = pmsg->params.dec_params.pic_info.pic_info.crop_left;
			pcfg->decparams_bkup.dec_info.dec_info.visible_rect.top = pmsg->params.dec_params.pic_info.pic_info.crop_top;
			pcfg->decparams_bkup.dec_info.dec_info.visible_rect.width = pmsg->params.dec_params.pic_info.pic_info.crop_width;
			pcfg->decparams_bkup.dec_info.dec_info.visible_rect.height = pmsg->params.dec_params.pic_info.pic_info.crop_height;
			set_bit(CTX_FLAG_CROPCHANGE_BIT, &ctx->flag);
		} else {
			pcfg->decparams.dec_info.dec_info.frame_width = pmsg->params.dec_params.pic_info.pic_info.width;
			pcfg->decparams.dec_info.dec_info.frame_height = pmsg->params.dec_params.pic_info.pic_info.height;
			pcfg->decparams.dec_info.dec_info.visible_rect.left = pmsg->params.dec_params.pic_info.pic_info.crop_left;
			pcfg->decparams.dec_info.dec_info.visible_rect.top = pmsg->params.dec_params.pic_info.pic_info.crop_top;
			pcfg->decparams.dec_info.dec_info.visible_rect.width = pmsg->params.dec_params.pic_info.pic_info.crop_width;
			pcfg->decparams.dec_info.dec_info.visible_rect.height = pmsg->params.dec_params.pic_info.pic_info.crop_height;
			v4l2_event_queue_fh(&ctx->fh, &event);
		}
	}
	return 0;
}

int vsi_v4l2_bufferdone(struct vsi_v4l2_msg *pmsg)
{
	unsigned long ctxid = pmsg->inst_id;
	int inbufidx, outbufidx, bytesused[4] = {0};
	struct vsi_v4l2_ctx *ctx;
	struct vb2_queue *vq = NULL;
	struct vb2_buffer	*vb;

	ctx = find_ctx(ctxid);
	if (ctx == NULL)
		return -1;

	if (isencoder(ctx)) {
		inbufidx = pmsg->params.enc_params.io_buffer.inbufidx;
		outbufidx = pmsg->params.enc_params.io_buffer.outbufidx;
		bytesused[0] = pmsg->params.enc_params.io_buffer.bytesused;
	} else {
		inbufidx = pmsg->params.dec_params.io_buffer.inbufidx;
		outbufidx = pmsg->params.dec_params.io_buffer.outbufidx;
		bytesused[0] = pmsg->params.dec_params.io_buffer.bytesused;
	}
	pr_debug("ctx %lx:%s:%ld:%lx:%d:%d", ctx->ctxid, __func__,
		pmsg->inst_id, ctx->flag, inbufidx, outbufidx);
	//write comes over once, so avoid this problem.
	if (inbufidx >= 0 && inbufidx < ctx->input_que.num_buffers) {
		vq = &ctx->input_que;
		vb = vq->bufs[inbufidx];
		atomic_inc(&ctx->srcframen);
		if (mutex_lock_interruptible(&ctx->ctxlock))
			return -1;
		if (ctx->input_que.streaming && vb->state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		ctx->queued_srcnum--;
		mutex_unlock(&ctx->ctxlock);
		if (ctx->queued_srcnum == 0)
			wake_up_interruptible_all(&ctx->retbuf_queue);
	}
	if (outbufidx >= 0 && outbufidx < ctx->output_que.num_buffers) {
		if (mutex_lock_interruptible(&ctx->ctxlock))
			return -EBUSY;
		if (bytesused[0] > 0)
			ctx->frameidx++;
		mutex_unlock(&ctx->ctxlock);
		vq = &ctx->output_que;
		vb = vq->bufs[outbufidx];
		atomic_inc(&ctx->dstframen);
		if (vb->state == VB2_BUF_STATE_ACTIVE) {
			vb->planes[0].bytesused = bytesused[0];
			if (mutex_lock_interruptible(&ctx->ctxlock))
				return -EBUSY;
			if (isencoder(ctx)) {
				vb->timestamp = pmsg->params.enc_params.io_buffer.timestamp;
				ctx->vbufflag[outbufidx] = pmsg->param_type;
				if (vb->planes[0].bytesused == 0 || (pmsg->param_type & LAST_BUFFER_FLAG))
					ctx->vbufflag[outbufidx] |= LAST_BUFFER_FLAG;
				pr_debug("enc output framed %d:%d size = %d,flag=%x, timestamp=%lld", outbufidx, (int)ctx->frameidx, vb->planes[0].bytesused, ctx->vbufflag[outbufidx], vb->timestamp);
			} else {
				if (bytesused[0] == 0) {
					if ((ctx->status == DEC_STATUS_DRAINING) || test_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag)) {
						ctx->status = DEC_STATUS_ENDSTREAM;
						set_bit(CTX_FLAG_ENDOFSTRM_BIT, &ctx->flag);
						clear_bit(CTX_FLAG_PRE_DRAINING_BIT, &ctx->flag);
					}
				} else
					vb->timestamp = pmsg->params.dec_params.io_buffer.timestamp;
				ctx->buffed_capnum++;
				ctx->buffed_cropcapnum++;
				pr_debug("dec output framed %d:%d size = %d", outbufidx, (int)ctx->frameidx, vb->planes[0].bytesused);
			}
			if (vb->state == VB2_BUF_STATE_ACTIVE)
				vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			mutex_unlock(&ctx->ctxlock);
		}
	}
	return 0;
}

static void vsi_daemonsdevice_release(struct device *dev)
{
}

static int v4l2_probe(struct platform_device *pdev)
{
	struct vsi_v4l2_device *vpu = NULL;
	struct video_device *venc, *vdec;
	int ret = 0;

	pr_debug("%s", __func__);
	if (gvsidev != NULL)
		return 0;
	vpu = kzalloc(sizeof(*vpu), GFP_KERNEL);
	if (!vpu)
		return -ENOMEM;

	vpu->dev = &pdev->dev;
	vpu->pdev = pdev;
	mutex_init(&vpu->lock);
	mutex_init(&vpu->irqlock);

	ret = v4l2_device_register(&pdev->dev, &vpu->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		kfree(vpu);
		return ret;
	}
	platform_set_drvdata(pdev, vpu);

	vpu->venc = NULL;
	vpu->vdec = NULL;
	venc = v4l2_probe_enc(pdev, vpu);
	if (venc == NULL)
		goto err;
	vpu->venc = venc;

	vdec = v4l2_probe_dec(pdev, vpu);
	if (vdec == NULL)
		goto err;
	vpu->vdec = vdec;

	ret = vsiv4l2_initdaemon();
	if (ret < 0)
		goto err;

	vsidaemondev = kzalloc(sizeof(struct device), GFP_KERNEL);
	vsidaemondev->devt = MKDEV(VSI_DAEMON_DEVMAJOR, 0);
	dev_set_name(vsidaemondev, "%s", VSI_DAEMON_FNAME);
	vsidaemondev->release = vsi_daemonsdevice_release;
	ret = device_register(vsidaemondev);
	if (ret < 0) {
		kfree(vsidaemondev);
		vsidaemondev = NULL;
		vsiv4l2_cleanupdaemon();
		goto err;
	}
	idr_init(&inst_array);

	gvsidev = pdev;
	return 0;

err:
	if (vpu->venc) {
		v4l2_release_enc(vpu->venc);
		video_device_release(vpu->venc);
	}
	if (vpu->vdec) {
		v4l2_release_dec(vpu->vdec);
		video_device_release(vpu->vdec);
	}
	v4l2_device_unregister(&vpu->v4l2_dev);
	kfree(vpu);

	return ret;
}

static int v4l2_remove(struct platform_device *pdev)
{
	struct vsi_v4l2_device *vpu = platform_get_drvdata(pdev);

	v4l2_release_dec(vpu->vdec);
	v4l2_release_enc(vpu->venc);
	v4l2_device_unregister(&vpu->v4l2_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(vpu);

	return 0;
}

static const struct platform_device_id v4l2_platform_ids[] = {
	{
		.name            = DRIVER_NAME,
	},
	{ },
};

static const struct of_device_id v4l2_of_match[] = {
	{ .compatible = "platform-vsiv4l2", },
	{/* sentinel */}
};

static struct platform_driver v4l2_drm_platform_driver = {
	.probe      = v4l2_probe,
	.remove      = v4l2_remove,
	.driver      = {
		.name      = DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = v4l2_of_match,
	},
	.id_table = v4l2_platform_ids,
};

static const struct platform_device_info v4l2_platform_info = {
	.name		= DRIVER_NAME,
	.id		= -1,
	.dma_mask	= DMA_BIT_MASK(64),
};

static ssize_t BandWidth_show(struct device *kdev,
				     struct device_attribute *attr, char *buf)
{
	/*
	 * sys/bus/platform/drivers/vsiv4l2/xxxxx.vpu/BandWidth
	 * used to show bandwidth info to user space
	 */
	u64 bandwidth;

	bandwidth = vsi_v4l2_getbandwidth();
	return snprintf(buf, PAGE_SIZE, "%lld\n", bandwidth);
}

static DEVICE_ATTR_RO(BandWidth);

static struct attribute *vsi_v4l2_attrs[] = {
	&dev_attr_BandWidth.attr,
	NULL,
};

static const struct attribute_group vsi_v4l2_attr_group = {
	.attrs = vsi_v4l2_attrs,
};

void __exit vsi_v4l2_cleanup(void)
{
	void *obj;
	int id;

	gvsidev = NULL;
	idr_for_each_entry(&inst_array, obj, id) {
		if (obj) {
			release_ctx(obj, 0);
			vsi_v4l2_quitinstance();
		}
	}

	device_unregister(vsidaemondev);
	kfree(vsidaemondev);
	vsiv4l2_cleanupdaemon();
	if (self)
		platform_device_unregister(gvsidev);
	platform_driver_unregister(&v4l2_drm_platform_driver);
	gvsidev = NULL;
	self = 0;
	pr_debug("%s", __func__);
}

int __init vsi_v4l2_init(void)
{
	int result;

	self = 0;
	vsidaemondev = NULL;
	mutex_init(&vsi_ctx_array_lock);
	ctx_seqid = 0;
	result = platform_driver_register(&v4l2_drm_platform_driver);
	if (result < 0) {
		platform_device_unregister(gvsidev);
		gvsidev = NULL;
	}

	if (gvsidev == NULL) {
		gvsidev = platform_device_register_full(&v4l2_platform_info);
		if (gvsidev == NULL) {
			pr_err("v4l2 create platform device fail");
			platform_driver_unregister(&v4l2_drm_platform_driver);
			return -1;
		}
		self = 1;
	}
	if (devm_device_add_group(&gvsidev->dev, &vsi_v4l2_attr_group))
		pr_err("fail to create sysfs API");

	pr_debug("v4l2 device created");

	return result;
}

module_init(vsi_v4l2_init);
module_exit(vsi_v4l2_cleanup);

/* module description */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("VSI v4l2 manager");

