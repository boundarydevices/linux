// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 BayLibre
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_camsv.h"

static inline struct mtk_cam_dev *to_mtk_cam_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mtk_cam_dev, subdev);
}

static const u32 mtk_cam_mbus_formats[] = {
	MEDIA_BUS_FMT_SBGGR8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SBGGR12_1X12,
	MEDIA_BUS_FMT_SGBRG12_1X12,
	MEDIA_BUS_FMT_SGRBG12_1X12,
	MEDIA_BUS_FMT_SRGGB12_1X12,
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_VYUY8_1X16,
	MEDIA_BUS_FMT_YUYV8_1X16,
	MEDIA_BUS_FMT_YVYU8_1X16,
};

/* -----------------------------------------------------------------------------
 * V4L2 Subdev Operations
 */

static int mtk_cam_cio_stream_on(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	struct v4l2_subdev *seninf;
	int ret;

	if (!cam->seninf) {
		cam->seninf = media_entity_remote_pad(&cam->subdev_pads[MTK_CAM_CIO_PAD_SENINF]);
		if (!cam->seninf) {
			dev_err(dev, "%s: No SENINF connected\n", __func__);
			return -ENOLINK;
		}
	}

	seninf = media_entity_to_v4l2_subdev(cam->seninf->entity);

	/* Seninf must stream on first */
	ret = v4l2_subdev_call(seninf, pad, s_stream, cam->seninf->index, 1);
	if (ret) {
		dev_err(dev, "failed to stream on %s:%d\n",
			seninf->entity.name, ret);
		return ret;
	}

	cam->streaming = true;

	return 0;
}

static int mtk_cam_cio_stream_off(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	struct v4l2_subdev *seninf;
	int ret;

	if (cam->seninf) {
		seninf = media_entity_to_v4l2_subdev(cam->seninf->entity);

		ret = v4l2_subdev_call(seninf, pad, s_stream,
				       cam->seninf->index, 0);
		if (ret) {
			dev_err(dev, "failed to stream off %s:%d\n",
				seninf->entity.name, ret);
			return ret;
		}
	}

	cam->streaming = false;

	return 0;
}

static int mtk_cam_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_cam_dev *cam = to_mtk_cam_dev(sd);

	if (enable) {
		/* Align vb2_core_streamon design */
		if (cam->streaming) {
			dev_warn(cam->dev, "already streaming on\n");
			return 0;
		}
		return mtk_cam_cio_stream_on(cam);
	}

	if (!cam->streaming) {
		dev_warn(cam->dev, "already streaming off\n");
		return 0;
	}

	return mtk_cam_cio_stream_off(cam);
}

static struct v4l2_mbus_framefmt *
mtk_cam_get_pad_format(struct mtk_cam_dev *cam,
		       struct v4l2_subdev_state *sd_state,
		       unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(&cam->subdev, sd_state, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &cam->formats[pad];
	default:
		return NULL;
	}
}

static int mtk_cam_init_cfg(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state)
{
	static const struct v4l2_mbus_framefmt def_format = {
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.width = IMG_DEF_WIDTH,
		.height = IMG_DEF_HEIGHT,
		.field = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.quantization = V4L2_QUANTIZATION_DEFAULT,
	};

	struct mtk_cam_dev *cam = to_mtk_cam_dev(sd);
	u32 which = sd_state ? V4L2_SUBDEV_FORMAT_TRY
		  : V4L2_SUBDEV_FORMAT_ACTIVE;
	struct v4l2_mbus_framefmt *format;
	unsigned int i;

	for (i = 0; i < sd->entity.num_pads; i++) {
		format = mtk_cam_get_pad_format(cam, sd_state, i, which);
		*format = def_format;
	}

	return 0;
}

static int mtk_cam_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(mtk_cam_mbus_formats))
		return -EINVAL;

	code->code = mtk_cam_mbus_formats[code->index];

	return 0;
}

static int mtk_cam_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct mtk_cam_dev *cam = to_mtk_cam_dev(sd);

	fmt->format = *mtk_cam_get_pad_format(cam, sd_state, fmt->pad,
					      fmt->which);

	return 0;
}

static int mtk_cam_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct mtk_cam_dev *cam = to_mtk_cam_dev(sd);
	struct v4l2_mbus_framefmt *format;
	unsigned int i;

	/*
	 * We only support pass-through mode, the format on source pads can't
	 * be modified.
	 */
	if (fmt->pad != MTK_CAM_CIO_PAD_SENINF)
		return mtk_cam_get_fmt(sd, sd_state, fmt);

	for (i = 0; i < ARRAY_SIZE(mtk_cam_mbus_formats); ++i) {
		if (mtk_cam_mbus_formats[i] == fmt->format.code)
			break;
	}

	if (i == ARRAY_SIZE(mtk_cam_mbus_formats))
		fmt->format.code = mtk_cam_mbus_formats[0];

	format = mtk_cam_get_pad_format(cam, sd_state, fmt->pad, fmt->which);
	format->width = fmt->format.width;
	format->height = fmt->format.height;
	format->code = fmt->format.code;

	fmt->format = *format;

	/* Propagate the format to the source pad. */
	format = mtk_cam_get_pad_format(cam, sd_state, MTK_CAM_CIO_PAD_VIDEO,
					fmt->which);
	format->width = fmt->format.width;
	format->height = fmt->format.height;
	format->code = fmt->format.code;

	return 0;
}

static int mtk_cam_subdev_registered(struct v4l2_subdev *sd)
{
	struct mtk_cam_dev *cam = to_mtk_cam_dev(sd);

	/* Create the video device and link. */
	return mtk_cam_video_register(cam);
}

static const struct v4l2_subdev_video_ops mtk_cam_subdev_video_ops = {
	.s_stream = mtk_cam_sd_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_cam_subdev_pad_ops = {
	.init_cfg = mtk_cam_init_cfg,
	.enum_mbus_code = mtk_cam_enum_mbus_code,
	.set_fmt = mtk_cam_set_fmt,
	.get_fmt = mtk_cam_get_fmt,
	.link_validate = v4l2_subdev_link_validate_default,
};

static const struct v4l2_subdev_ops mtk_cam_subdev_ops = {
	.video = &mtk_cam_subdev_video_ops,
	.pad = &mtk_cam_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops mtk_cam_internal_ops = {
	.registered = mtk_cam_subdev_registered,
};

/* -----------------------------------------------------------------------------
 * Media Entity Operations
 */

static const struct media_entity_operations mtk_cam_media_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
	.get_fwnode_pad = v4l2_subdev_get_fwnode_pad_1_to_1,
};

/* -----------------------------------------------------------------------------
 * Init & Cleanup
 */

static int mtk_cam_v4l2_register(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	int ret;

	/* Initialize subdev pads */
	ret = media_entity_pads_init(&cam->subdev.entity,
				     ARRAY_SIZE(cam->subdev_pads),
				     cam->subdev_pads);
	if (ret) {
		dev_err(dev, "failed to initialize media pads:%d\n", ret);
		return ret;
	}

	cam->subdev_pads[MTK_CAM_CIO_PAD_SENINF].flags = MEDIA_PAD_FL_SINK;
	cam->subdev_pads[MTK_CAM_CIO_PAD_VIDEO].flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize subdev */
	v4l2_subdev_init(&cam->subdev, &mtk_cam_subdev_ops);

	cam->subdev.dev = dev;
	cam->subdev.entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	cam->subdev.entity.ops = &mtk_cam_media_entity_ops;
	cam->subdev.internal_ops = &mtk_cam_internal_ops;
	cam->subdev.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	strscpy(cam->subdev.name, dev_name(dev), V4L2_SUBDEV_NAME_SIZE);
	v4l2_set_subdevdata(&cam->subdev, cam);

	mtk_cam_init_cfg(&cam->subdev, NULL);

	ret = v4l2_async_register_subdev(&cam->subdev);
	if (ret) {
		dev_err(dev, "failed to initialize subdev:%d\n", ret);
		goto fail_clean_media_entiy;
	}

	return 0;

fail_clean_media_entiy:
	media_entity_cleanup(&cam->subdev.entity);

	return ret;
}

static void mtk_cam_v4l2_unregister(struct mtk_cam_dev *cam)
{
	mtk_cam_video_unregister(&cam->vdev);

	media_entity_cleanup(&cam->subdev.entity);
	v4l2_async_unregister_subdev(&cam->subdev);
}

int mtk_cam_dev_init(struct mtk_cam_dev *cam_dev)
{
	int ret;

	mutex_init(&cam_dev->op_lock);

	/* v4l2 sub-device registration */
	ret = mtk_cam_v4l2_register(cam_dev);
	if (ret) {
		mutex_destroy(&cam_dev->op_lock);
		return ret;
	}

	return ret;
}

void mtk_cam_dev_cleanup(struct mtk_cam_dev *cam)
{
	mtk_cam_v4l2_unregister(cam);
	mutex_destroy(&cam->op_lock);
}
