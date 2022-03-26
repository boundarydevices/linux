/*
 * Copyright (C) 2011-2016 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Copyright 2018-2019 NXP
 *
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

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
#include "ov5640_mipi.h"

#define DRIVER_ID_STR	"ov5640_mipisubdev"

static DEFINE_MUTEX(ov5640_mutex);

static void ov5640_lock(void)
{
	mutex_lock(&ov5640_mutex);
}

static void ov5640_unlock(void)
{
	mutex_unlock(&ov5640_mutex);
}

static const struct ov5640_datafmt ov5640_colour_fmts[] = {
	{V4L2_PIX_FMT_YUYV, 0x30, V4L2_COLORSPACE_JPEG, NULL, MEDIA_BUS_FMT_YUYV8_2X8},
	{V4L2_PIX_FMT_UYVY, 0x32, V4L2_COLORSPACE_JPEG, NULL, MEDIA_BUS_FMT_UYVY8_2X8},
};

struct ov5640 {
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_pad pad;
#endif
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler hdl;
	struct clk *sensor_clk;
	unsigned virtual_channel;
	const struct ov5640_datafmt	*fmt;

	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_captureparm streamcap;
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int blue;
	int ae_mode;
	int mirror;
	int vflip;
	int wb;
	u8 reg4300;

	u32 mclk;

	uint16_t roi_x;
	uint16_t roi_y;
	int focus_mode;
	int focus_range;
	int colorfx;
	struct gpio_desc *gpiod_pwdn;
	struct gpio_desc *gpiod_rst;
	int prev_sysclk;
	int prev_HTS;
	int AE_low;
	int AE_high;
	int AE_Target;
	int last_reg;
	struct regulator *io_regulator;
	struct regulator *core_regulator;
	struct regulator *analog_regulator;
	struct regulator *gpo_regulator;

	struct semaphore power_lock;
	int power_open_count;
	struct delayed_work power_down_work;
	int power_on;
};

void ov5640_disable_mipi_v(void)
{
}

static void *ov5640_csi2_info_v(struct ov5640 *sensor, enum ov5640_mode mode)
{
	return sensor;
}

static int ov5640_waiti_csi2_v(void *mipi_csi2_info)
{
	return 0;
}

int ov5640_csi2_enable_v(void)
{
	return 0;
}

static void ov5640_change_mode_direct_v(struct ov5640 *sensor)
{
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
static const struct v4l2_ctrl_ops afs_ctrl_ops;

static const struct std_ctrl sctrl[]  = {
	{&ctrl_ops, V4L2_CID_BRIGHTNESS, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_3A_LOCK, 0, V4L2_LOCK_FOCUS, 0, V4L2_LOCK_FOCUS},
	{&ctrl_ops, V4L2_CID_AUTO_FOCUS_START, 0, 0, 1, 0},
	{&ctrl_ops, V4L2_CID_AUTO_FOCUS_STOP, 0, 0, 1, 0},
	{&afs_ctrl_ops, V4L2_CID_AUTO_FOCUS_STATUS, 0,
		(V4L2_AUTO_FOCUS_STATUS_BUSY |
		 V4L2_AUTO_FOCUS_STATUS_REACHED |
		 V4L2_AUTO_FOCUS_STATUS_FAILED),
		0, V4L2_AUTO_FOCUS_STATUS_IDLE},
	{&ctrl_ops, V4L2_CID_FOCUS_ABSOLUTE, 0, 0xfffffff, 1, 0},
	{&ctrl_ops, V4L2_CID_FOCUS_AUTO, 0, 1, 1, 1},
	{&ctrl_ops, V4L2_CID_CONTRAST, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_SATURATION, -4, 4, 1, 0},
	{&ctrl_ops, V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1},
	{&ctrl_ops, V4L2_CID_DO_WHITE_BALANCE, V4L2_WHITE_BALANCE_MANUAL, V4L2_WHITE_BALANCE_SHADE, 1, 1},
};

struct std_menu_ctrl {
	const struct v4l2_ctrl_ops* ctrl_ops;
	u32 id;
	u8 max;
	u64 mask;
	u8 def;
};

static const struct std_menu_ctrl s_menu_ctrl[]  = {
	{&ctrl_ops, V4L2_CID_AUTO_FOCUS_RANGE,
		V4L2_AUTO_FOCUS_RANGE_INFINITY,
		~(BIT(V4L2_AUTO_FOCUS_RANGE_INFINITY) |
			BIT(V4L2_AUTO_FOCUS_RANGE_NORMAL)),
		V4L2_AUTO_FOCUS_RANGE_NORMAL
	},
	{&ctrl_ops, V4L2_CID_COLORFX, V4L2_COLORFX_GRASS_GREEN, ~0xff, V4L2_COLORFX_NONE},
};

static struct v4l2_subdev_ops ov5640_subdev_ops;

#ifdef CONFIG_MEDIA_CONTROLLER
static int ov5640_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations ov5640_subdev_media_ops = {
	.link_setup = ov5640_link_setup,
};
#endif

static void power_down_callback(struct work_struct *work);
static const struct v4l2_subdev_internal_ops ov5640_sd_internal_ops;
static void power_off_camera(struct ov5640 *sensor);

static int ov5640_probe_v(struct ov5640 *sensor, struct clk *sensor_clk, u32 csi)
{
	struct device *dev = &sensor->i2c_client->dev;
	struct v4l2_ctrl_handler *hdl;
	int i;
	int ret;

	sema_init(&sensor->power_lock, 1);
	INIT_DELAYED_WORK(&sensor->power_down_work, power_down_callback);
	sensor->virtual_channel = csi;
	ret = ov5640_dev_init(sensor);
	if (ret < 0) {
		dev_warn(dev, "Camera init failed\n");
		return ret;
	}

	sensor->sensor_clk = sensor_clk;
	v4l2_i2c_subdev_init(&sensor->subdev, sensor->i2c_client, &ov5640_subdev_ops);
	hdl = &sensor->hdl;
	ret = v4l2_ctrl_handler_init(hdl, ARRAY_SIZE(sctrl));
	if (ret)
		return ret;
	sensor->subdev.internal_ops = &ov5640_sd_internal_ops;

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

	for (i = 0; i < ARRAY_SIZE(s_menu_ctrl); i++) {
		const struct std_menu_ctrl *s = &s_menu_ctrl[i];
		v4l2_ctrl_new_std_menu(hdl, s->ctrl_ops, s->id, s->max, s->mask, s->def);

		if (hdl->error) {
			ret = hdl->error;
			v4l2_ctrl_handler_free(hdl);
			dev_err(dev, "%i:s_menu_ctrl(0x%x) error(%d)\n", i, s->id, ret);
			return ret;
		}
	}

	sensor->subdev.ctrl_handler = hdl;
	v4l2_ctrl_handler_setup(hdl);

	sensor->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#ifdef CONFIG_MEDIA_CONTROLLER
	sensor->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->subdev.entity.ops = &ov5640_subdev_media_ops;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->subdev.entity, 1, &sensor->pad);
	if (ret < 0) {
		dev_err(dev, "entity pad init failed, ret=%d\n", ret);
		return -EINVAL;
	}
#endif

	ret = v4l2_async_register_subdev(&sensor->subdev);
	if (ret < 0) {
		dev_err(dev, "Async register failed, ret=%d\n", ret);
#ifdef CONFIG_MEDIA_CONTROLLER
		media_entity_cleanup(&sensor->subdev.entity);
		return -EINVAL;
#endif
	}
	OV5640_stream_off(sensor);
	power_off_camera(sensor);
	return 0;
}

static struct ov5640 *to_ov5640(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov5640, subdev);
}

static struct ov5640 *dev_to_ov5640_v(struct device *dev)
{
	return container_of(dev_get_drvdata(dev), struct ov5640, subdev);
}

/*!
 * ov5640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static void ov5640_remove_v(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640 *sensor = to_ov5640(client);

	v4l2_async_unregister_subdev(sd);

	ov5640_power_off(sensor);
	clk_disable_unprepare(sensor->sensor_clk);
}

#include "ov5640_mipi_common.c"

static int _ov5640_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5640 *sensor = container_of(ctrl->handler, struct ov5640, hdl);
	struct v4l2_control vc;

	vc.id = ctrl->id;
	vc.value = ctrl->val;

	return ov5640_s_ctrl(sensor, &vc);
}

static int _ov5640_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5640 *sensor = container_of(ctrl->handler, struct ov5640, hdl);
	struct v4l2_control vc;
	int ret;

	vc.id = ctrl->id;
	vc.value = ctrl->val;

	ret = ov5640_g_ctrl(sensor, &vc);
	ctrl->val = vc.value;
	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = _ov5640_s_ctrl,
};

static const struct v4l2_ctrl_ops afs_ctrl_ops = {
	.s_ctrl = _ov5640_s_ctrl,
	.g_volatile_ctrl = _ov5640_g_ctrl,
};

static int _ov5640_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
static int _ov5640_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);

static int _ov5640_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	pr_debug("%s:  %d %d\n", __func__, on, sensor->on);
	if (on) {
		_ov5640_open(sd, NULL);
	} else {
		_ov5640_close(sd, NULL);
	}
	return 0;
}

static int ov5640_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5640_get_register(struct v4l2_subdev *sd,
			       struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	int ret;
	u8 val;

	if (reg->reg & ~0xffff)
		return -EINVAL;

	ret = ov5640_read_reg(sensor, reg->reg, &val);
	if (ret < 0)
		return ret;

	reg->val = (__u64)val;

	return 0;
}

static int ov5640_set_register(struct v4l2_subdev *sd,
			       const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	if (reg->reg & ~0xffff || reg->val & ~0xff)
		return -EINVAL;

	return ov5640_write_reg(sensor, reg->reg, reg->val);
}
#endif

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_power	= _ov5640_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov5640_get_register,
	.s_register	= ov5640_set_register,
#endif
	.subscribe_event = ov5640_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

/*!
 * ov5640_g_frame_interval - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @sd: pointer to standard V4L2 sub device structure
 * @a: pointer to v4l2_subdev_frame_interval
 */
static int ov5640_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *ival)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	ival->interval = sensor->streamcap.timeperframe;

	return 0;
}

/*!
 * ov5640_s_frame_interval - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @sd: pointer to standard V4L2 sub device structure
 * @ival: pointer to v4l2_subdev_frame_interval
 */
static int ov5640_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *ival)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	struct v4l2_fract *timeperframe = &ival->interval;

	sensor->streamcap.timeperframe = *timeperframe;

	return 0;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	struct device *dev = &sensor->i2c_client->dev;

	dev_dbg(dev, "s_stream: %d\n", enable);
	if (enable) {
		_ov5640_open(sd, NULL);
		msleep(1);
		OV5640_stream_on(sensor);
	} else {
		OV5640_stream_off(sensor);
		_ov5640_close(sd, NULL);
	}
	dev_dbg(dev, "s_stream: exit %d\n", enable);
	return 0;
}

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.g_frame_interval = ov5640_g_frame_interval,
	.s_frame_interval = ov5640_s_frame_interval,
	.s_stream = ov5640_s_stream,
};

/*!
 * ov5640_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fse: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index > ov5640_mode_MAX)
		return -EINVAL;

	fse->max_width =
			max(ov5640_mode_info_data[0][fse->index].width,
			    ov5640_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;
	fse->max_height =
			max(ov5640_mode_info_data[0][fse->index].height,
			    ov5640_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;
	return 0;
}

/*!
 * ov5640_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fie: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;
	int i, j, count = 0;

	if (fie->index < 0 || fie->index > ov5640_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		dev_warn(dev, "Please assign pixel format, width and height\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(ov5640_mode_info_data); i++) {
		for (j = 0; j < (ov5640_mode_MAX + 1); j++) {
			if (fie->width == ov5640_mode_info_data[i][j].width
			 && fie->height == ov5640_mode_info_data[i][j].height
			 && ov5640_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->interval.denominator =
						ov5640_framerates[i];
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov5640_colour_fmts))
		return -EINVAL;

	code->code = ov5640_colour_fmts[code->index].code;
	return 0;
}

/* Find a data format by a pixel code in an array */
static const struct ov5640_datafmt
			*ov5640_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5640_colour_fmts); i++)
		if (ov5640_colour_fmts[i].code == code)
			return ov5640_colour_fmts + i;

	return NULL;
}

static int get_capturemode(const struct ov5640_mode_info *info, int cnt,
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

static int ov5640_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct ov5640_datafmt *fmt = ov5640_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	int capturemode;

	if (!fmt) {
		fmt = &ov5640_colour_fmts[0];
		mf->code	= fmt->code;
		mf->colorspace	= fmt->colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	sensor->fmt = fmt;
	sensor->pix.pixelformat = sensor->fmt->pixelformat;

	pr_debug("%s: code=%x\n", __func__, fmt->code);

	capturemode = get_capturemode(ov5640_mode_info_data[0], ov5640_mode_MAX,
			mf->width, mf->height);
	if (capturemode >= 0) {
		struct v4l2_streamparm sp;

		sensor->streamcap.capturemode = capturemode;
		sensor->pix.width = mf->width;
		sensor->pix.height = mf->height;
		sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		sp.parm.capture.timeperframe = sensor->streamcap.timeperframe;
		sp.parm.capture.capturemode = capturemode;

		return ov5640_s_parm(sensor, &sp);
	}

	dev_err(&client->dev, "Set format failed %d, %d\n",
		fmt->code, fmt->colorspace);
	return -EINVAL;
}

static int ov5640_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	const struct ov5640_datafmt *fmt = sensor->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	mf->width	= sensor->pix.width;
	mf->height	= sensor->pix.height;

	return 0;
}

static const struct v4l2_subdev_pad_ops ov5640_subdev_pad_ops = {
	.enum_frame_size       = ov5640_enum_framesizes,
	.enum_frame_interval   = ov5640_enum_frameintervals,
	.enum_mbus_code        = ov5640_enum_mbus_code,
	.set_fmt               = ov5640_set_fmt,
	.get_fmt               = ov5640_get_fmt,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core	= &ov5640_subdev_core_ops,
	.video	= &ov5640_subdev_video_ops,
	.pad	= &ov5640_subdev_pad_ops,
};

static void power_down_callback(struct work_struct *work)
{
	struct ov5640 *sensor = container_of(work, struct ov5640, power_down_work.work);

	down(&sensor->power_lock);
	pr_debug("%s: csi%d %d\n", __func__, sensor->virtual_channel, sensor->power_open_count);
	if (!sensor->power_open_count) {
		ov5640_s_power(sensor, 0);
		sensor->power_on = 0;
	}
	up(&sensor->power_lock);
}

/* power_lock is held */
static void power_off_camera(struct ov5640 *sensor)
{
	if (!sensor->power_open_count) {
		schedule_delayed_work(&sensor->power_down_work, (HZ * 5));
		pr_debug("%s: csi%d\n", __func__, sensor->virtual_channel);
	}
}

/* power_lock is held */
static void power_up_camera(struct ov5640 *sensor)
{
	struct device *dev = &sensor->i2c_client->dev;
	int ret;

	pr_debug("%s: a csi%d %d %d\n", __func__, sensor->virtual_channel, sensor->power_open_count, sensor->power_on);
	if (sensor->power_on)
		return;
	sensor->power_on = 1;
	ret = ov5640_s_power(sensor, 1);
	if (!ret)
		ret = ov5640_dev_init(sensor);
	if (ret < 0)
		dev_warn(dev, "Camera init failed %d\n", ret);
	pr_debug("%s: b csi%d %d\n", __func__, sensor->virtual_channel, sensor->power_open_count);
}

static int _ov5640_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	cancel_delayed_work_sync(&sensor->power_down_work);
	down(&sensor->power_lock);
	pr_debug("%s: csi%d %d\n", __func__, sensor->virtual_channel, sensor->power_open_count);
	if (sensor->power_open_count++ == 0) {
		power_up_camera(sensor);
	}
	up(&sensor->power_lock);
	return 0;
}

static int _ov5640_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	down(&sensor->power_lock);
	pr_debug("%s: csi%d %d\n", __func__, sensor->virtual_channel, sensor->power_open_count);
	if (--sensor->power_open_count == 0) {
		power_off_camera(sensor);
	}
	up(&sensor->power_lock);
	return 0;
}

static const struct v4l2_subdev_internal_ops ov5640_sd_internal_ops = {
	.open = _ov5640_open,
	.close = _ov5640_close,
};
