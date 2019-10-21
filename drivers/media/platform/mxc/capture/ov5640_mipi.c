/*
 * Copyright (C) 2011-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2019 NXP
 */

/*
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
#include <linux/mipi_csi2.h>
#include "v4l2-int-device.h"
#include "mxc_v4l2_capture.h"
#include "ov5640_mipi.h"

#define DRIVER_ID_STR	"ov5640_mipi"

static void ov5640_lock(void)
{
	mxc_camera_common_lock();
}

static void ov5640_unlock(void)
{
	mxc_camera_common_unlock();
}

static const struct ov5640_datafmt ov5640_colour_fmts[] = {
	{V4L2_PIX_FMT_YUYV, 0x32, V4L2_COLORSPACE_JPEG, "YUYV"},
	{V4L2_PIX_FMT_UYVY, 0x32, V4L2_COLORSPACE_JPEG, "UYVY"},
};

struct ov5640 {
	struct sensor_data s;
	struct v4l2_sensor_dimension spix;
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
};

void ov5640_disable_mipi_v(void)
{
	void *mipi_csi2_info;

	mipi_csi2_info = mipi_csi2_get_info();

	/* disable mipi csi2 */
	if (mipi_csi2_info)
		if (mipi_csi2_get_status(mipi_csi2_info))
			mipi_csi2_disable(mipi_csi2_info);
}

static void *ov5640_csi2_info_v(struct ov5640 *sensor, enum ov5640_mode mode)
{
	void *mipi_csi2_info = mipi_csi2_get_info();

	/* initial mipi dphy */
	if (!mipi_csi2_info) {
		printk(KERN_ERR "%s() in %s: Fail to get mipi_csi2_info!\n",
		       __func__, __FILE__);
		return NULL;
	}

	if (!mipi_csi2_get_status(mipi_csi2_info))
		mipi_csi2_enable(mipi_csi2_info);

	if (!mipi_csi2_get_status(mipi_csi2_info)) {
		pr_err("Can not enable mipi csi2 driver!\n");
		return NULL;
	}

	mipi_csi2_set_lanes(mipi_csi2_info, 2);

	/*Only reset MIPI CSI2 HW at sensor initialize*/
	if (mode == ov5640_mode_INIT)
		mipi_csi2_reset(mipi_csi2_info);

	if ((sensor->pix.pixelformat == V4L2_PIX_FMT_UYVY) ||
			(sensor->pix.pixelformat == V4L2_PIX_FMT_YUYV))
		mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_YUV422);
	else if (sensor->pix.pixelformat == V4L2_PIX_FMT_RGB565)
		mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_RGB565);
	else
		pr_err("currently this sensor format can not be supported!\n");
	return mipi_csi2_info;
}

static int ov5640_waiti_csi2_v(void *mipi_csi2_info)
{
	if (mipi_csi2_info) {
		unsigned int i = 0;
		u32 mipi_reg;

		/* wait for mipi sensor ready */
		while (1) {
			mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
			if (mipi_reg != 0x200)
				break;
			if (i++ >= 20) {
				pr_err("mipi csi2 can not receive sensor clk! %x\n", mipi_reg);
				return -EIO;
			}
			msleep(10);
		}

		i = 0;
		/* wait for mipi stable */
		while (1) {
			mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
			if (!mipi_reg)
				break;
			if (i++ >= 20) {
				pr_err("mipi csi2 can not receive data correctly!\n");
				return -EIO;
			}
			msleep(10);
		}

	}
	return 0;
}

int ov5640_csi2_enable_v(void)
{
	void *mipi_csi2_info = mipi_csi2_get_info();

	/* enable mipi csi2 */
	if (mipi_csi2_info)
		mipi_csi2_enable(mipi_csi2_info);
	else {
		printk(KERN_ERR "%s() in %s: Fail to get mipi_csi2_info!\n",
		       __func__, __FILE__);
		return -EPERM;
	}
	return 0;
}

static void ov5640_change_mode_direct_v(struct ov5640 *sensor)
{
	OV5640_stream_on(sensor);
}

static void ov5640_reset_pwrdn(struct ov5640 *sensor)
{
	ov5640_reset(sensor);
	gpiod_set_value_cansleep(sensor->gpiod_pwdn, 1);
}

static struct v4l2_int_device ov5640_int_device;

static void _ov5640_reset_pwrdn(void)
{
	ov5640_reset_pwrdn(ov5640_int_device.priv);
}

static int ov5640_probe_v(struct ov5640 *sensor, struct clk *sensor_clk, u32 csi)
{
	struct device *dev = &sensor->i2c_client->dev;
	int ret;

	ov5640_power_off(sensor);

	ret = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(sensor->s.mclk_source));
	if (ret) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "ipu_id",
					&sensor->s.ipu_id);
	if (ret) {
		dev_err(dev, "ipu_id missing or invalid\n");
		return ret;
	}

	sensor->virtual_channel = csi | (sensor->s.ipu_id << 1);
	sensor->s.csi = csi;
	sensor->s.io_init = _ov5640_reset_pwrdn;
	sensor->s.mipi_camera = 1;
	sensor->s.sensor_clk = sensor_clk;
	ov5640_int_device.priv = sensor;
	i2c_set_clientdata(sensor->i2c_client, sensor);
	ret = v4l2_int_device_register(&ov5640_int_device);

//	clk_disable_unprepare(sensor_clk);
	return ret;
}

static struct ov5640 *dev_to_ov5640_v(struct device *dev)
{
	return dev_get_drvdata(dev);
}

/*!
 * ov5640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static void ov5640_remove_v(struct i2c_client *client)
{
	struct ov5640 *sensor = i2c_get_clientdata(client);

	v4l2_int_device_unregister(&ov5640_int_device);
	ov5640_power_off(sensor);
}

#include "ov5640_mipi_common.c"

static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	return ov5640_s_ctrl(s->priv, vc);
}

static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	return ov5640_g_ctrl(s->priv, vc);
}

static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	return ov5640_s_power(s->priv, on);
}

/*!
 * ov5640_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	return ov5640_g_parm(s->priv, a);
}

/*!
 * ov5640_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	return ov5640_s_parm(s->priv, a);
}

/*!
 * ov5640_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_framesizes(struct ov5640 *sensor,
				 struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > ov5640_mode_MAX)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->pixel_format = sensor->pix.pixelformat;
	fsize->discrete.width =
			max(ov5640_mode_info_data[0][fsize->index].width,
			    ov5640_mode_info_data[1][fsize->index].width);
	fsize->discrete.height =
			max(ov5640_mode_info_data[0][fsize->index].height,
			    ov5640_mode_info_data[1][fsize->index].height);
	return 0;
}

static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fse)
{
	return ov5640_enum_framesizes(s->priv, fse);
}

/*!
 * ov5640_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fie: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_frameintervals(struct ov5640 *sensor,
					 struct v4l2_frmivalenum *fie)
{
	struct device *dev = &sensor->i2c_client->dev;
	int i, j, count = 0;

	if (fie->index < 0 || fie->index > ov5640_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0) {
		dev_warn(dev, "Please assign pixel format, width and height\n");
		return -EINVAL;
	}

	fie->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fie->discrete.numerator = 1;

	for (i = 0; i < ARRAY_SIZE(ov5640_mode_info_data); i++) {
		for (j = 0; j < (ov5640_mode_MAX + 1); j++) {
			if (fie->width == ov5640_mode_info_data[i][j].width
			 && fie->height == ov5640_mode_info_data[i][j].height
			 && ov5640_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->discrete.denominator =
						ov5640_framerates[i];
				return 0;
			}
		}
	}
	return -EINVAL;
}

static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
					 struct v4l2_frmivalenum *fie)
{
	return ov5640_enum_frameintervals(s->priv, fie);
}

/*!
 * ov5640_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ov5640_enum_fmt_cap(struct ov5640 *sensor,
			      struct v4l2_fmtdesc *fmt)
{
	if (fmt->index > ARRAY_SIZE(ov5640_colour_fmts))
		return -EINVAL;

	fmt->pixelformat = ov5640_colour_fmts[fmt->index].pixelformat;
	strncpy(fmt->description, ov5640_colour_fmts[fmt->index].name,
			ARRAY_SIZE(fmt->description));
	return 0;
}

static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	return ov5640_enum_fmt_cap(s->priv, fmt);
}

static int find_fmt(u32 pixelformat)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5640_colour_fmts); i++) {
		if (pixelformat == ov5640_colour_fmts[i].pixelformat)
			return i;
	}
	return -EINVAL;
}

static int ov5640_s_fmt_cap(struct ov5640 *sensor, struct v4l2_format *f)
{
	int i;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		i = find_fmt(f->fmt.pix.pixelformat);
		if (i < 0)
			return i;

		sensor->pix.pixelformat = f->fmt.pix.pixelformat;
		sensor->fmt = &ov5640_colour_fmts[i];
		pr_debug("%s: %s\n", __func__, sensor->fmt->name);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ioctl_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	return ov5640_s_fmt_cap(s->priv, f);
}

static int ov5640_g_fmt_cap(struct ov5640 *sensor, struct v4l2_format *f)
{
	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		f->fmt.pix = sensor->pix;
		pr_debug("%s: %dx%d\n", __func__, sensor->pix.width, sensor->pix.height);
		break;

	case V4L2_BUF_TYPE_SENSOR:
		pr_debug("%s: left=%d, top=%d, %dx%d\n", __func__,
			sensor->spix.left, sensor->spix.top,
			sensor->spix.swidth, sensor->spix.sheight);
		f->fmt.spix = sensor->spix;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		break;

	default:
		f->fmt.pix = sensor->pix;
		break;
	}

	return 0;
}

/*!
 * ov5640_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	return ov5640_g_fmt_cap(s->priv, f);
}

/*!
 * ov5640_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ov5640_g_chip_ident(struct ov5640 *sensor, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"ov5640_mipi_camera");

	return 0;
}

static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	return ov5640_g_chip_ident(s->priv, id);
}

/*!
 * ov5640_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	return ov5640_dev_init(s->priv);
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	ov5640_disable_mipi_v();
	return 0;
}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */
static int ov5640_g_ifparm(struct ov5640 *sensor, struct v4l2_ifparm *p)
{
	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = sensor->mclk;
	pr_debug("   clock_curr=mclk=%d\n", sensor->mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = OV5640_XCLK_MIN;
	p->u.bt656.clock_max = OV5640_XCLK_MAX;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

	return 0;
}

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	return ov5640_g_ifparm(s->priv, p);
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

static int ov5640_send_command(struct ov5640 *sensor, struct v4l2_send_command_control *vc) {
	int ret = -1;
	int retval1;
	u8 regval;
	u8 loca_val=0;

	switch (vc->id) {
		case 105: //step to specified position
			pr_debug("Stepping to position: %d\n", vc->value0);
			if(vc->value0 < 0 || vc->value0 > 255)
				return ret;
			loca_val = vc->value0;
			retval1 = ov5640_read_reg(sensor, 0x3602,&regval);
			if (0 > retval1) {
				pr_err("ov5640_read_reg(sensor, 3602): %d\n", retval1);
				return retval1;
			}
			regval &= 0x0f;
			regval |= (loca_val&7) << 5; 	/* low 3 bits */
			ov5640_write_reg(sensor, 0x3602, regval);
			ov5640_write_reg(sensor, 0x3603, loca_val >> 3);
			ret = 0;
			break;
		default:
			pr_err("%s:Unknown ctrl 0x%x\n", __func__, vc->id);
			break;
	}

	return ret;
}

static int ioctl_send_command(struct v4l2_int_device *s, struct v4l2_send_command_control *vc)
{
	return ov5640_send_command(s->priv, vc);
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
static struct v4l2_int_ioctl_desc ov5640_ioctl_desc[] = {
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *) ioctl_s_power},
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_enum_frameintervals_num,
			(v4l2_int_ioctl_func *) ioctl_enum_frameintervals},
	{vidioc_int_enum_fmt_cap_num,
			(v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
/*	{vidioc_int_try_fmt_cap_num,
			(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */
	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_s_fmt_cap},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *) ioctl_g_chip_ident},
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *) ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *) ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
			(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *) ioctl_init},
/*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
	{vidioc_int_send_command_num,
				(v4l2_int_ioctl_func *) ioctl_send_command},
};
#pragma GCC diagnostic pop

static struct v4l2_int_slave ov5640_slave = {
	.ioctls = ov5640_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(ov5640_ioctl_desc),
};

static struct v4l2_int_device ov5640_int_device = {
	.module = THIS_MODULE,
	.name = DRIVER_ID_STR,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &ov5640_slave,
	},
};
