// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_seninf.h"
#include "mtk_seninf_route.h"
#include "mtk_seninf_ops.h"

#define to_std_fmt_code(code) \
	((code) & 0xFFFF)

void mtk_seninf_init_res(struct seninf_core *core)
{
	int i;
	int start_seninf_mux;

	start_seninf_mux = SENINF_MUX1;

	INIT_LIST_HEAD(&core->list_mux);
	for (i = start_seninf_mux; i < g_seninf_ops->mux_num; i++) {
		core->mux[i].idx = i;
		list_add_tail(&core->mux[i].list, &core->list_mux);
	}

#ifdef SENINF_DEBUG
	INIT_LIST_HEAD(&core->list_cam_mux);
	for (i = 0; i < g_seninf_ops->cam_mux_num; i++) {
		core->cam_mux[i].idx = i;
		list_add_tail(&core->cam_mux[i].list, &core->list_cam_mux);
	}
#endif
}

void mtk_seninf_release_res(struct seninf_core *core)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, &core->list_mux) {
		list_del_init(pos);
	}

#ifdef SENINF_DEBUG
	list_for_each_safe(pos, n, &core->list_cam_mux) {
		list_del_init(pos);
	}
#endif
}

struct seninf_mux *mtk_seninf_mux_get(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	if (!list_empty(&core->list_mux)) {
		ent = list_first_entry(&core->list_mux,
				       struct seninf_mux, list);
		list_move_tail(&ent->list, &ctx->list_mux);
	}

	mutex_unlock(&core->mutex);

	return ent;
}

struct seninf_mux *mtk_seninf_mux_get_pref(struct seninf_ctx *ctx,
					       int *pref_idx, int pref_cnt)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	list_for_each_entry(ent, &core->list_mux, list) {
		for (i = 0; i < pref_cnt; i++) {
			if (ent->idx == pref_idx[i]) {
				list_move_tail(&ent->list,
					       &ctx->list_mux);
				mutex_unlock(&core->mutex);
				return ent;
			}
		}
	}

	mutex_unlock(&core->mutex);

	return mtk_seninf_mux_get(ctx);
}

void mtk_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux)
{
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);
	list_move_tail(&mux->list, &core->list_mux);
	mutex_unlock(&core->mutex);
}


void mtk_seninf_get_vcinfo_test(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	if (ctx->is_test_model == 1) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_GENERAL_DATA_MIN_NUM;
		vc->out_pad = PAD_SRC_GENERAL0;
		vc->group = 0;
	} else {
		dev_err(ctx->dev, "Invalid test model\n");
	}
}

struct seninf_vc *mtk_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad == idx)
			return &vcinfo->vc[i];
	}

	return NULL;
}

unsigned int mtk_seninf_get_vc_feature(struct v4l2_subdev *sd, unsigned int pad)
{
	struct seninf_vc *pvc = NULL;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	pvc = mtk_seninf_get_vc_by_pad(ctx, pad);
	if (pvc)
		return pvc->feature;

	return VC_NONE;
}

static u8 get_mbus_format_by_dt(unsigned int dt)
{
	switch (dt) {
	case 0x2a:
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	case 0x2b:
		return MEDIA_BUS_FMT_SBGGR10_1X10;
	case 0x2c:
		return MEDIA_BUS_FMT_SBGGR12_1X12;
	case 0x1e:
		return MEDIA_BUS_FMT_UYVY8_1X16;
	default:
		/* default raw8 for other data types */
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	}
}

static u8 get_dt_by_mbus_fmt(unsigned int fmt)
{
	switch (fmt) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return 0x2a;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return 0x2b;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return 0x2c;
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		return 0x1e;
	default:
		return 0;
	}
}

static int get_vcinfo_by_pad_fmt(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	u8 dt;

	vcinfo->cnt = 0;
	dt = get_dt_by_mbus_fmt(to_std_fmt_code(ctx->fmt[PAD_SINK].format.code));
	if (dt == 0)
		return -1;

	vc = &vcinfo->vc[vcinfo->cnt++];
	vc->vc = 0;
	vc->dt = dt;
	vc->feature = VC_GENERAL_DATA_MIN_NUM;
	vc->out_pad = PAD_SRC_GENERAL0;
	vc->group = 0;

	return 0;
}

#ifdef SENINF_VC_ROUTING
#define has_op(master, op) \
	(master->ops && master->ops->op)
#define call_op(master, op) \
	(has_op(master, op) ? master->ops->op(master) : 0)

/* Copy the one value to another. */
static void ptr_to_ptr(struct v4l2_ctrl *ctrl,
		       union v4l2_ctrl_ptr from, union v4l2_ctrl_ptr to)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	memcpy(to.p, from.p, ctrl->elems * ctrl->elem_size);
}

/* Copy the current value to the new value */
static void cur_to_new(struct v4l2_ctrl *ctrl)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	ptr_to_ptr(ctrl, ctrl->p_cur, ctrl->p_new);
}

/* Helper function to get a single control */
static int get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *master = ctrl->cluster[0];
	int ret = 0;
	int i;

	if (ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY) {
		pr_info("%s ctrl->flags&V4L2_CTRL_FLAG_WRITE_ONLY\n",
			__func__);
		return -EACCES;
	}

	v4l2_ctrl_lock(master);
	if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE) {
		pr_info("%s master->ncontrols:%d",
			__func__, master->ncontrols);
		for (i = 0; i < master->ncontrols; i++)
			cur_to_new(master->cluster[i]);
		ret = call_op(master, g_volatile_ctrl);
	}
	v4l2_ctrl_unlock(master);

	return ret;
}

int mtk_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	int ret = 0;
	int i, grp, grp_metadata;
	struct mtk_mbus_frame_desc fd = {0};
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int desc;
	struct v4l2_subdev_format raw_fmt, sub_fmt = {0};
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;

	if (!ctx->sensor_sd)
		return -EINVAL;

	sub_fmt.pad = ctx->sensor_pad_idx;
	sub_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(ctx->sensor_sd, pad, get_fmt,
				NULL, &sub_fmt);

	fd.type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
	fd.num_entries = SENINF_VC_COUNT;
	for (i = 0; i < fd.num_entries; i++) {
		fd.entry[i].bus.csi2.channel = i;
		fd.entry[i].bus.csi2.data_type = get_dt_by_mbus_fmt(sub_fmt.format.code);
		fd.entry[i].bus.csi2.hsize = sub_fmt.format.width;
		fd.entry[i].bus.csi2.vsize = sub_fmt.format.height;

		dev_dbg(ctx->dev, "%s, get_fmt sensor(pad=%d) vc=%d dt=0x%x, width=%u, height=%u\n",
			__func__,
			sub_fmt.pad,
			i,
			get_dt_by_mbus_fmt(sub_fmt.format.code),
			sub_fmt.format.width,
			sub_fmt.format.height);
	}

	vcinfo->cnt = 0;
	grp = 0;
	grp_metadata = -1;

	for (i = 0; i < fd.num_entries; i++) {
		vc = &vcinfo->vc[vcinfo->cnt];
		vc->vc = fd.entry[i].bus.csi2.channel;
		vc->dt = fd.entry[i].bus.csi2.data_type;
		desc = fd.entry[i].bus.csi2.user_data_desc;

		switch (desc) {
		case VC_GENERAL_EMBEDDED:
			vc->feature = VC_GENERAL_EMBEDDED;
			vc->out_pad = PAD_SRC_GENERAL0;
			break;
		default:
			if (vc->dt == 0x2a || vc->dt == 0x2b ||
			    vc->dt == 0x2c) {
				vc->out_pad = PAD_SRC_GENERAL0;
				vc->feature = VC_GENERAL_DATA_MIN_NUM;
				vc->group = grp++;
			} else if (vc->dt == 0x1e) {
				vc->out_pad = PAD_SRC_GENERAL0;
				vc->feature = VC_GENERAL_DATA_MIN_NUM;
			} else {
				dev_info(ctx->dev, "unknown desc %d, dt 0x%x\n",
					desc, vc->dt);
				continue;
			}
			break;
		}

		if (grp_metadata < 0)
			grp_metadata = grp++;
		vc->group = grp_metadata;

		if (vc->dt == 0x1e)
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize * 2;
		else
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
		vc->exp_vsize = fd.entry[i].bus.csi2.vsize;

		/* update pad fotmat */
		if (vc->exp_hsize && vc->exp_vsize) {
			ctx->fmt[vc->out_pad].format.width = vc->exp_vsize;
			ctx->fmt[vc->out_pad].format.height = vc->exp_hsize;
		}

		ctx->fmt[vc->out_pad].format.code =
			get_mbus_format_by_dt(vc->dt);

		dev_info(ctx->dev, "%s vc[%d] vc 0x%x dt 0x%x pad %d exp %dx%d grp 0x%x code 0x%x\n",
			__func__,
			vcinfo->cnt, vc->vc, vc->dt, vc->out_pad,
			vc->exp_hsize, vc->exp_vsize, vc->group,
			ctx->fmt[vc->out_pad].format.code);
		vcinfo->cnt++;
	}

	return 0;
}
#endif

#ifndef SENINF_VC_ROUTING
int mtk_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	return get_vcinfo_by_pad_fmt(ctx);
}
#endif

void mtk_seninf_release_mux(struct seninf_ctx *ctx)
{
	struct seninf_mux *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, &ctx->list_mux, list) {
		mtk_seninf_mux_put(ctx, ent);
	}
}

int mtk_seninf_is_vc_enabled(struct seninf_ctx *ctx, struct seninf_vc *vc)
{
#ifdef SENINF_VC_ROUTING
	return 1;
#else
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

#ifdef SENINF_DEBUG
	if (ctx->is_test_streamon)
		return 1;
#endif

	if (vc->out_pad != PAD_SRC_GENERAL0) {
		if (media_pad_remote_pad_first(&ctx->pads[vc->out_pad]))
			return 1;
		else
			return 0;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		u8 out_pad = vcinfo->vc[i].out_pad;

		if ((out_pad == PAD_SRC_GENERAL0) &&
			media_pad_remote_pad_first(&ctx->pads[out_pad]))
			return 1;
	}

	return 0;

#endif
}

int mtk_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt)
{
	int i;
	struct seninf_vc *vc;

	for (i = 0; i < ctx->vcinfo.cnt; i++) {
		vc = &ctx->vcinfo.vc[i];
		if (vc->vc == ch && vc->dt == dt) {
#ifdef SENINF_DEBUG
			if (ctx->is_test_streamon)
				return 1;
#endif
			if (media_pad_remote_pad_first(&ctx->pads[vc->out_pad]))
				return 1;
			return 0;
		}
	}

	return 0;
}

/* Debug Only */
#ifdef SENINF_DEBUG
void mtk_seninf_release_cam_mux(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct seninf_cam_mux *ent, *tmp;

	mutex_lock(&core->mutex);

	/* release all cam muxs */
	list_for_each_entry_safe(ent, tmp, &ctx->list_cam_mux, list) {
		list_move_tail(&ent->list, &core->list_cam_mux);
	}

	mutex_unlock(&core->mutex);
}

void mtk_seninf_alloc_cam_mux(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_cam_mux *ent;

	mutex_lock(&core->mutex);

	/* allocate all cam muxs */
	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		ent = list_first_entry_or_null(&core->list_cam_mux,
					       struct seninf_cam_mux, list);
		if (ent) {
			list_move_tail(&ent->list, &ctx->list_cam_mux);
			ctx->pad2cam[vc->out_pad] = ent->idx;
			dev_info(ctx->dev, "pad%d -> cam%d\n",
				 vc->out_pad, ent->idx);
		}
	}

	mutex_unlock(&core->mutex);
}
#endif

int mtk_seninf_get_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int *pixelMode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	*pixelMode = vc->pixel_mode;

	return 0;
}

int mtk_seninf_set_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int pixelMode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	vc->pixel_mode = pixelMode;
	if (ctx->streaming) {
		update_isp_clk(ctx);
		g_seninf_ops->_update_mux_pixel_mode(ctx, vc->mux, pixelMode);
	}
	// if streaming, update ispclk and update pixle mode seninf mux and reset

	return 0;
}

int _mtk_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg, bool disable_last)
{
	int vc_en, old_camtg;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	if (pad_id < PAD_SRC_GENERAL0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	ctx->pad2cam[pad_id] = camtg;
	dev_info(ctx->dev, "%s pad_id %d camtg %d\n", __func__, pad_id, camtg);

	return 0;
}

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	return _mtk_seninf_set_camtg(sd, pad_id, camtg, true);
}
EXPORT_SYMBOL(mtk_cam_seninf_set_camtg);
