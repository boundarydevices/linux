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
#include <linux/mipi_csi2.h>
#include "v4l2-int-device.h"
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>
#include "../../../../../sound/soc/fsl/imx-audmux.h"
#include "../../../../../sound/soc/fsl/fsl_ssi.h"
#include <linux/slab.h>
#include "mxc_v4l2_capture.h"
#include "tc358743_regs.h"
#include "tc358743_h2c.h"
#include "../../../../mxc/mipi/mxc_mipi_csi2.h"

#define DRIVER_ID_STR	"tc358743_mipi"

/*!
 * Maintains the information on the current state of the sensor.
 */
struct tc_data {
	struct sensor_data s;
	struct v4l2_sensor_dimension spix;
	unsigned virtual_channel;
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
	void *mipi_csi2_info;

	mipi_csi2_info = mipi_csi2_get_info();
	if (!mipi_csi2_info) {
		pr_err("Fail to get mipi_csi2_info!\n");
		return NULL;
	}
	return mipi_csi2_info;
}

static int tc358743_mipi_reset_v(struct tc_data *td, void *mci,
		u32 lanes, u32 type)
{
	pr_debug("%s: mipi_csi2_info:\n"
	"mipi_en:       %d\n"
	"datatype:      %d\n"
	"dphy_clk:      %p\n"
	"pixel_clk:     %p\n"
	"mipi_csi2_base:%p\n"
	"pdev:          %p\n"
	, __func__,
	((struct mipi_csi2_info *)mci)->mipi_en,
	((struct mipi_csi2_info *)mci)->datatype,
	((struct mipi_csi2_info *)mci)->dphy_clk,
	((struct mipi_csi2_info *)mci)->pixel_clk,
	((struct mipi_csi2_info *)mci)->mipi_csi2_base,
	((struct mipi_csi2_info *)mci)->pdev
	);
	if (mipi_csi2_get_status(mci)) {
		mipi_csi2_disable(mci);
		msleep(1);
	}
	mipi_csi2_enable(mci);

	if (!mipi_csi2_get_status(mci)) {
		pr_err("Can not enable mipi csi2 driver!\n");
		return -EINVAL;
	}
	lanes = mipi_csi2_set_lanes(mci, lanes);
	pr_debug("Now Using %d lanes\n", lanes);

	mipi_csi2_reset(mci);
	mipi_csi2_set_datatype(mci, type);
	return 0;
}

static int tc358743_mipi_wait_v(void *mipi_csi2_info)
{
	unsigned i = 0;
	unsigned j;
	u32 mipi_reg;
	u32 mipi_reg_test[10];

	/* wait for mipi sensor ready */
	for (;;) {
		mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
		mipi_reg_test[i++] = mipi_reg;
		if (mipi_reg != 0x200)
			break;
		if (i >= 10) {
			pr_err("mipi csi2 can not receive sensor clk!\n");
			return -1;
		}
		msleep(10);
	}

	for (j = 0; j < i; j++) {
		pr_debug("%d  mipi csi2 dphy status %x\n", j, mipi_reg_test[j]);
	}

	i = 0;

	/* wait for mipi stable */
	for (;;) {
		mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
		mipi_reg_test[i++] = mipi_reg;
		if (!mipi_reg)
			break;
		if (i >= 10) {
			pr_err("mipi csi2 can not reveive data correctly!\n");
			return -1;
		}
		msleep(10);
	}

	for (j = 0; j < i; j++) {
		pr_debug("%d  mipi csi2 err1 %x\n", j, mipi_reg_test[j]);
	}
	return 0;
}

static void tc_reset(struct tc_data *td);

static void tc358743_reset_pwrdn(struct tc_data *td)
{
	tc_reset(td);
	if (gpio_is_valid(td->pwn_gpio))
		gpio_set_value_cansleep(td->pwn_gpio, 1);
}

static struct v4l2_int_device tc358743_int_device;

static void tc_io_init(void)
{
	tc358743_reset_pwrdn(tc358743_int_device.priv);
}

static int tc358743_probe_v(struct tc_data *td, struct clk *sensor_clk, u32 csi)
{
	struct device *dev = &td->i2c_client->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &td->s.mclk_source);
	if (ret) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "ipu_id",
					&td->s.ipu_id);
	if (ret) {
		dev_err(dev, "ipu_id missing or invalid\n");
		return ret;
	}
	if ((unsigned)td->s.ipu_id || csi) {
		dev_err(dev, "invalid ipu/csi\n");
		return -EINVAL;
	}

	/* Set initial values for the struct. */
	td->s.io_init = tc_io_init;
	td->s.csi = csi;
	td->s.mipi_camera = 1;
	td->s.sensor_clk = sensor_clk;

	if (!IS_ERR(td->s.sensor_clk))
		clk_prepare_enable(td->s.sensor_clk);

	tc358743_int_device.priv = td;
	i2c_set_clientdata(td->i2c_client, td);

	mutex_unlock(&td->access_lock);
	ret = v4l2_int_device_register(&tc358743_int_device);
	mutex_lock(&td->access_lock);
	if (ret) {
		pr_err("%s:  v4l2_int_device_register failed, error=%d\n",
			__func__, ret);
	}
	return ret;
}

static struct tc_data *to_tc358743(const struct i2c_client *client)
{
	return i2c_get_clientdata(client);
}

static struct tc_data *dev_to_tc_data_v(struct device *dev)
{
	return dev_get_drvdata(dev);
}

static void tc358743_remove_v(struct i2c_client *client)
{
	v4l2_int_device_unregister(&tc358743_int_device);
}

#include "tc358743_h2c_common.c"

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	return tc358743_s_ctrl(s->priv, vc);
}

static int tc358743_g_ctrl(struct tc_data *td, struct v4l2_control *vc)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = td->brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = td->hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = td->contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = td->saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = td->red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = td->blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = td->ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	return tc358743_g_ctrl(s->priv, vc);
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	return tc358743_s_power(s->priv, on);
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	return tc358743_g_parm(s->priv, a);
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	return tc358743_s_parm(s->priv, a);
}

static int tc358743_enum_framesizes(struct tc_data *td,
				 struct v4l2_frmsizeenum *fsize)
{
	enum tc358743_mode query_mode= fsize->index;
	enum tc358743_mode mode = td->mode;

	if (IS_COLORBAR(mode)) {
		if (query_mode > MAX_COLORBAR)
			return -EINVAL;
		mode = query_mode;
	} else {
		if (query_mode)
			return -EINVAL;
	}
	pr_debug("%s, mode: %d\n", __func__, mode);

	fsize->pixel_format = get_pixelformat(0, mode);
	fsize->discrete.width =
			    tc358743_mode_info_data[0][mode].width;
	fsize->discrete.height =
			    tc358743_mode_info_data[0][mode].height;
	pr_debug("%s %d:%d format: %x\n", __func__, fsize->discrete.width, fsize->discrete.height, fsize->pixel_format);
	return 0;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	return tc358743_enum_framesizes(s->priv, fsize);
}

static int tc358743_enum_fmt_cap(struct tc_data *td,
			      struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;

	if (!index)
		index = td->streamcap.capturemode;
	pr_debug("%s, INDEX: %d\n", __func__, index);
	if (index >= tc358743_mode_MAX)
		return -EINVAL;

	fmt->pixelformat = get_pixelformat(0, index);

	pr_debug("%s: format: %x\n", __func__, fmt->pixelformat);
	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	return tc358743_enum_fmt_cap(s->priv, fmt);
}

static int tc358743_try_fmt_cap(struct tc_data *td,
			     struct v4l2_format *f)
{
	u32 tgt_fps;	/* target frames per secound */
	enum tc358743_frame_rate frame_rate;
	int ret = 0;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int mode;

	pr_debug("%s\n", __func__);

	mutex_lock(&td->access_lock);
	tgt_fps = td->streamcap.timeperframe.denominator /
		  td->streamcap.timeperframe.numerator;

	frame_rate = tc_fps_to_index(tgt_fps);
	if (frame_rate < 0) {
		pr_debug("%s: %d fps (%d,%d) is not supported\n", __func__,
			 tgt_fps, td->streamcap.timeperframe.denominator,
			 td->streamcap.timeperframe.numerator);
		ret = -EINVAL;
		goto out;
	}
	mode = td->streamcap.capturemode;
	td->pix.pixelformat = get_pixelformat(frame_rate, mode);
	td->pix.width = pix->width = tc358743_mode_info_data[frame_rate][mode].width;
	td->pix.height = pix->height = tc358743_mode_info_data[frame_rate][mode].height;
	pr_debug("%s: %dx%d\n", __func__, td->pix.width, td->pix.height);

	pix->pixelformat = td->pix.pixelformat;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = pix->width * 4;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	default:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	}

	{
		pr_debug("SYS_STATUS: 0x%x\n", tc358743_read_reg_val(td, 0x8520));
		pr_debug("VI_STATUS0: 0x%x\n", tc358743_read_reg_val(td, 0x8521));
		pr_debug("VI_STATUS1: 0x%x\n", tc358743_read_reg_val(td, 0x8522));
		pr_debug("VI_STATUS2: 0x%x\n", tc358743_read_reg_val(td, 0x8525));
		pr_debug("VI_STATUS3: 0x%x\n", tc358743_read_reg_val(td, 0x8528));
		pr_debug("%s %d:%d format: %x\n", __func__, pix->width, pix->height, pix->pixelformat);
	}
out:
	mutex_unlock(&td->access_lock);
	return ret;
}

static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
			     struct v4l2_format *f)
{
	return tc358743_try_fmt_cap(s->priv, f);
}

static int tc358743_g_fmt_cap(struct tc_data *td, struct v4l2_format *f)
{
	int mode = td->streamcap.capturemode;

	td->pix.pixelformat = get_pixelformat(0, mode);
	td->pix.width = tc358743_mode_info_data[0][mode].width;
	td->pix.height = tc358743_mode_info_data[0][mode].height;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		f->fmt.pix = td->pix;
		pr_debug("%s: %dx%d\n", __func__, td->pix.width, td->pix.height);
		break;

	case V4L2_BUF_TYPE_SENSOR:
		pr_debug("%s: left=%d, top=%d, %dx%d\n", __func__,
			td->spix.left, td->spix.top,
			td->spix.swidth, td->spix.sheight);
		f->fmt.spix = td->spix;
		break;

	case V4L2_BUF_TYPE_PRIVATE:
		pr_debug("%s: private\n", __func__);
		break;

	default:
		f->fmt.pix = td->pix;
		pr_debug("%s: type=%d, %dx%d\n", __func__, f->type, td->pix.width, td->pix.height);
		break;
	}
	return 0;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	return tc358743_g_fmt_cap(s->priv, f);
}

static int tc358743_g_chip_ident(struct tc_data *td, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"tc358743_mipi");

	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	return tc358743_g_chip_ident(s->priv, id);
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	return tc358743_dev_init(s->priv);
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	void *mci;

	mci = mipi_csi2_get_info();

	/* disable mipi csi2 */
	if (mci)
		if (mipi_csi2_get_status(mci))
			mipi_csi2_disable(mci);

	return 0;
}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int tc358743_g_ifparm(struct tc_data *td, struct v4l2_ifparm *p)
{
	pr_debug("%s\n", __func__);

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = TC358743_XCLK_MIN;
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = TC358743_XCLK_MIN;
	p->u.bt656.clock_max = TC358743_XCLK_MAX;

	return 0;
}

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	return tc358743_g_ifparm(s->priv, p);
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	pr_debug("%s\n", __func__);
	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc tc358743_ioctl_desc[] = {
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func*) ioctl_s_power},
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *) ioctl_g_chip_ident},

	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func*) ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func*) ioctl_g_ifparm},
	{vidioc_int_init_num, (v4l2_int_ioctl_func*) ioctl_init},
};

static struct v4l2_int_slave tc358743_slave = {
	.ioctls = tc358743_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tc358743_ioctl_desc),
};

static struct v4l2_int_device tc358743_int_device = {
	.module = THIS_MODULE,
	.name = "tc358743",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &tc358743_slave,
	},
};
