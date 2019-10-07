/*
 * Copyright (C) 2011-2012 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2014 Boundary Devices
 */

/*
 * Modifyed by: Edison Fernández <edison.fernandez@ridgerun.com>
 * Added support to use it with Nitrogen6x
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#define DEBUG 1
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-dapm.h>
#include "../../../../../sound/soc/fsl/imx-audmux.h"
#include "../../../../../sound/soc/fsl/fsl_ssi.h"
#include <linux/slab.h>
#include "tc358743_regs.h"
#include "tc358743_h2c.h"

#define DRIVER_ID_STR	"tc358743_mipisubdev"

struct tc358743_datafmt {
	u32	pixelformat;
	enum v4l2_colorspace	colorspace;
	const char* name;
	u32	code;
};

static const struct tc358743_datafmt tc358743_colour_fmts[] = {
//	{V4L2_PIX_FMT_YUYV, V4L2_COLORSPACE_JPEG, NULL, MEDIA_BUS_FMT_YUYV8_2X8},
	{V4L2_PIX_FMT_UYVY, V4L2_COLORSPACE_JPEG, NULL, MEDIA_BUS_FMT_UYVY8_2X8},
};

/*!
 * Maintains the information on the current state of the sensor.
 */
struct tc_data {
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler hdl;
	struct clk *sensor_clk;
	int lanes;
	struct v4l2_sensor_dimension spix;
	unsigned virtual_channel;
	const struct tc358743_datafmt *fmt;
	struct v4l2_pix_format pix;
	struct i2c_client *i2c_client;
	struct v4l2_captureparm streamcap;
	bool on;
	struct mutex access_lock;
	int det_changed;
#define REGULATOR_IO		0
#define REGULATOR_CORE		1
#define REGULATOR_GPO		2
#define REGULATOR_ANALOG	3
#define REGULATOR_CNT		4
	struct regulator *regulator[REGULATOR_CNT];
	u32 lock;
	enum tc358743_mode mode;
	u32 fps;
	u32 audio;
	int pwn_gpio;
	int rst_gpio;
	u16 hpd_active;
	int edid_initialized;

	u32 mclk;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int blue;
	int ae_mode;
};

static void *tc358743_mipi_csi2_get_info_v(struct tc_data *td)
{
	return td;
}

static int tc358743_mipi_reset_v(struct tc_data *td, void *mci,
		u32 lanes, u32 type)
{
	td->lanes = lanes;
	pr_debug("%s:lanes=%d\n", __func__, td->lanes);
	return 0;
}

static int tc358743_mipi_wait_v(void *mipi_csi2_info)
{
	return 0;
}

struct std_ctrl {
	const struct v4l2_ctrl_ops* ctrl_ops;
	u32 id;
	s64 min;
	s64 max;
	u64 step;
	s64 def;
};

static const struct v4l2_ctrl_ops ctrl_ops;

static const struct std_ctrl sctrl[]  = {
	{&ctrl_ops, V4L2_CID_BRIGHTNESS, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_CONTRAST, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_SATURATION, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1},
	{&ctrl_ops, V4L2_CID_DO_WHITE_BALANCE, V4L2_WHITE_BALANCE_MANUAL, V4L2_WHITE_BALANCE_SHADE, 1, 1},
};

static struct v4l2_subdev_ops tc358743_subdev_ops;
static int tc358743_dev_init(struct tc_data *td);

static int tc358743_probe_v(struct tc_data *td, struct clk *sensor_clk, u32 csi)
{
	struct device *dev = &td->i2c_client->dev;
	struct v4l2_ctrl_handler *hdl;
	int i;
	int ret;

	ret = tc358743_dev_init(td);
	if (ret < 0) {
		dev_warn(dev, "Camera init failed\n");
		return ret;
	}

	td->sensor_clk = sensor_clk;
	v4l2_i2c_subdev_init(&td->subdev, td->i2c_client, &tc358743_subdev_ops);
	hdl = &td->hdl;
	v4l2_ctrl_handler_init(hdl, ARRAY_SIZE(sctrl));

	for (i = 0; i < ARRAY_SIZE(sctrl); i++) {
		const struct std_ctrl *s = &sctrl[i];
		v4l2_ctrl_new_std(hdl, s->ctrl_ops, s->id, s->min, s->max, s->step, s->def);

		if (hdl->error) {
			ret = hdl->error;
			v4l2_ctrl_handler_free(hdl);
			dev_err(dev, "%i:ctrl(0x%x) error(%d)\n", i, s->id, ret);
			return ret;
		}
	}

	td->subdev.ctrl_handler = hdl;
	v4l2_ctrl_handler_setup(hdl);

	td->subdev.grp_id = 678;
	ret = v4l2_async_register_subdev(&td->subdev);
	if (ret < 0)
		dev_err(dev, "Async register failed, ret=%d\n", ret);
	return 0;
}

static struct tc_data *to_tc358743(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct tc_data, subdev);
}

static struct tc_data *dev_to_tc_data_v(struct device *dev)
{
	return container_of(dev_get_drvdata(dev), struct tc_data, subdev);
}

static s32 power_control(struct tc_data *td, int on);

static void tc358743_remove_v(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tc_data *td = to_tc358743(client);

	v4l2_async_unregister_subdev(sd);

	power_control(td, 0);
	clk_disable_unprepare(td->sensor_clk);
}

#include "tc358743_h2c_common.c"

static int _tc358743_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tc_data *td = container_of(ctrl->handler, struct tc_data, hdl);
	struct v4l2_control vc;

	vc.id = ctrl->id;
	vc.value = ctrl->val;

	return tc358743_s_ctrl(td, &vc);
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = _tc358743_s_ctrl,
};

static int _tc358743_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);

	return tc358743_s_power(td, on);
}

static int tc358743_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

static struct v4l2_subdev_core_ops tc358743_subdev_core_ops = {
	.s_power	= _tc358743_s_power,
	.subscribe_event = tc358743_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

/*!
 * tc358743_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int _tc358743_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);

	return tc358743_g_parm(td, a);
}

/*!
 * ov5460_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int _tc358743_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);

	return tc358743_s_parm(td, a);
}

static char lanes_flag[] = {
	V4L2_MBUS_CSI2_1_LANE, V4L2_MBUS_CSI2_2_LANE,
	V4L2_MBUS_CSI2_3_LANE, V4L2_MBUS_CSI2_4_LANE
};

static int tc358743_g_mbus_config(struct v4l2_subdev *sd,
			     struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);

	cfg->type = V4L2_MBUS_CSI2;

	if ((td->lanes <= 0) || (td->lanes > 4))
		td->lanes = 4;
	/* Support for non-continuous CSI-2 clock is missing in the driver */
	cfg->flags = V4L2_MBUS_CSI2_CONTINUOUS_CLOCK | lanes_flag[td->lanes - 1];
	pr_debug("%s:lanes=%d\n", __func__, td->lanes);

	return 0;
}

static void tc358743_stream_on(struct tc_data *td)
{
	/* It is critical for CSI receiver to see lane transition
	 * LP11->HS. Set to non-continuous mode to enable clock lane
	 * LP11 state. */
	tc358743_write_reg(td, TXOPTIONCNTRL, 0, 4);
	/* Set to continuous mode to trigger LP11->HS transition */
	tc358743_write_reg(td, TXOPTIONCNTRL, MASK_CONTCLKMODE, 4);
	/* Unmute video */
	tc358743_write_reg(td, VI_MUTE, MASK_AUTO_MUTE, 1);

	mutex_lock(&td->access_lock);
	tc358743_modify_reg(td, CONFCTL, ~(MASK_VBUFEN | MASK_ABUFEN),
			(MASK_VBUFEN | MASK_ABUFEN), 2);
	mutex_unlock(&td->access_lock);
}

static void tc358743_stream_off(struct tc_data *td)
{
	/* Mute video so that all data lanes go to LSP11 state.
	 * No data is output to CSI Tx block. */
	tc358743_write_reg(td, VI_MUTE, MASK_AUTO_MUTE | MASK_VI_MUTE, 1);

	mutex_lock(&td->access_lock);
	tc358743_modify_reg(td, CONFCTL, ~(MASK_VBUFEN | MASK_ABUFEN), 0x0, 2);
	mutex_unlock(&td->access_lock);
}

static int tc358743_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);
	struct device *dev = &td->i2c_client->dev;

	dev_info(dev, "s_stream: %d\n", enable);
	if (enable)
		tc358743_stream_on(td);
	else
		tc358743_stream_off(td);
	return 0;
}

static struct v4l2_subdev_video_ops tc358743_subdev_video_ops = {
	.g_parm = _tc358743_g_parm,
	.s_parm = _tc358743_s_parm,
	.g_mbus_config = tc358743_g_mbus_config,
	.s_stream = tc358743_s_stream,
};

#define max_field(data, index, field, max) \
	max = 0; \
	for (i = 0; i < tc358743_max_fps; i++ ) {	\
		if (max < data[i][index].field)		\
			max = data[i][index].field;		\
	}

/*!
 * tc358743_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fse: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int tc358743_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	int i;
	int max;
	int index = fse->index;

	if (fse->index > tc358743_mode_MAX)
		return -EINVAL;

	max_field(tc358743_mode_info_data, index, width, max);
	fse->min_width = fse->max_width = max;
	max_field(tc358743_mode_info_data, index, height, max);
	fse->min_height = fse->max_height = max;
	return 0;
}

/*!
 * tc358743_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fie: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int tc358743_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;
	int i, j, count = 0;

	if (fie->index < 0 || fie->index > tc358743_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		dev_warn(dev, "Please assign pixel format, width and height\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(tc358743_mode_info_data); i++) {
		for (j = 0; j < (tc358743_mode_MAX + 1); j++) {
			if (fie->width == tc358743_mode_info_data[i][j].width
			 && fie->height == tc358743_mode_info_data[i][j].height
			 && tc358743_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->interval.denominator =
						tc358743_fps_list[i];
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int tc358743_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(tc358743_colour_fmts))
		return -EINVAL;

	code->code = tc358743_colour_fmts[code->index].code;
	return 0;
}

/* Find a data format by a pixel code in an array */
static const struct tc358743_datafmt
			*tc358743_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tc358743_colour_fmts); i++)
		if (tc358743_colour_fmts[i].code == code)
			return tc358743_colour_fmts + i;

	return NULL;
}

static int get_capturemode(const struct tc358743_mode_info *info, int cnt,
		int width, int height)
{
	int i;

	for (i = 0; i <= cnt; i++) {
		if ((info[i].width == width) &&
		     (info[i].height == height))
			return i;
	}

	return -1;
}

static int tc358743_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct tc358743_datafmt *fmt = tc358743_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);
	int capturemode;

	if (!fmt) {
		fmt = &tc358743_colour_fmts[0];
		mf->code	= fmt->code;
		mf->colorspace	= fmt->colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	td->fmt = fmt;
	td->pix.pixelformat = td->fmt->pixelformat;

	pr_debug("%s: code=%x\n", __func__, fmt->code);

	capturemode = get_capturemode(tc358743_mode_info_data[0], tc358743_mode_MAX,
			mf->width, mf->height);
	if (capturemode >= 0) {
		td->streamcap.capturemode = capturemode;
		td->pix.width = mf->width;
		td->pix.height = mf->height;
		return 0;
	}

	dev_err(&client->dev, "Set format failed %d, %d\n",
		fmt->code, fmt->colorspace);
	return -EINVAL;
}

static int tc358743_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tc_data *td = to_tc358743(client);
	const struct tc358743_datafmt *fmt = td->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	mf->width	= td->pix.width;
	mf->height	= td->pix.height;

	return 0;
}

static const struct v4l2_subdev_pad_ops tc358743_subdev_pad_ops = {
	.enum_frame_size       = tc358743_enum_framesizes,
	.enum_frame_interval   = tc358743_enum_frameintervals,
	.enum_mbus_code        = tc358743_enum_mbus_code,
	.set_fmt               = tc358743_set_fmt,
	.get_fmt               = tc358743_get_fmt,
};

static struct v4l2_subdev_ops tc358743_subdev_ops = {
	.core	= &tc358743_subdev_core_ops,
	.video	= &tc358743_subdev_video_ops,
	.pad	= &tc358743_subdev_pad_ops,
};
