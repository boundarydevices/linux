/*
 * Copyright 2005-2009 Freescale Semiconductor, Inc. All Rights Reserved.
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
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/v4l2-int-device.h>
#include <linux/clk.h>
#include <mach/gpio.h>
#include <linux/proc_fs.h>
#include <linux/firmware.h>
#include "ov5640.h"
#include <linux/zutil.h>
#include <mach/clock.h>
#include <linux/fsl_devices.h>

#define DEBUGMSG(arg...)

MODULE_AUTHOR("Boundary Devices, Inc.");
MODULE_DESCRIPTION("Omnivision OV5642 Camera driver");
MODULE_LICENSE("GPL");

#define DRIVER_NAME "ov5640"

static const struct i2c_device_id ov5640_id[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static int ov5640_remove(struct i2c_client *client);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = DRIVER_NAME,
	.probe = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_id,
};

#define OV5640_VOLTAGE_ANALOG               2775000
#define OV5640_VOLTAGE_DIGITAL_CORE         1500000
#define OV5640_VOLTAGE_DIGITAL_IO           2500000
#define OV5640_VOLTAGE_GPIO           	    -1

#define OV5640_XCLK_MIN 6000000
#define OV5640_XCLK_MAX 24000000

#define CHIPID_HIGH 0x300a
#define CHIPID_LOW  0x300b

/*!
 * Maintains the information on the current state of the sesor.
 */
#define IO_REGULATOR 0
#define CORE_REGULATOR 1
#define ANALOG_REGULATOR 2
#define GPO_REGULATOR 3
#define NUM_REGULATORS 4

struct sensor {
	struct v4l2_subdev sd;
	struct device *dev ;
	const struct mxc_camera_platform_data *platform_data;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_captureparm streamcap;
        struct ov5640_setting *settings ;
	unsigned num_settings ;

	struct regulator *regulators[NUM_REGULATORS];
	struct clk *tclk ;

	int ident;
	int power_on ;
	int enable_count ;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	int cur_format ;
};

static inline struct sensor *to_sensor(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sensor, sd);
}

static int ov5640_power_on(struct sensor *sensor)
{
	int retval = 0 ;
	BUG_ON(sensor->power_on < 0);
	if (0 == sensor->power_on++) {
		unsigned i = 0 ;
		for (i = 0 ; i < NUM_REGULATORS ; i++) {
			struct regulator *r = sensor->regulators[i];
			if (r) {
				if (regulator_enable(r) != 0) {
					printk(KERN_ERR "%s: error enabling regulator %d\n", __func__, i );
					break;
				}
			}
		}
		if (i < NUM_REGULATORS) {
			while( 0 < i ){
				struct regulator *r = sensor->regulators[--i];
				if (r)
					regulator_disable(r);
			}
			--sensor->power_on ;
			retval = -EIO ;
		}
	}
	return retval ;
}

static void ov5640_power_off(struct sensor *sensor)
{
	if ((0 < sensor->power_on) && (0 == --sensor->power_on)) {
		unsigned i = 0 ;
		for (i = 0 ; i < NUM_REGULATORS ; i++) {
			struct regulator *r = sensor->regulators[i];
			if (r)
                                regulator_disable(r);
		}
	}
}

static int ov5640_enable(struct sensor *sensor)
{
	int retval = 0 ;
	if (0 == sensor->enable_count++) {
		if(sensor->platform_data->power_down && sensor->platform_data->reset && sensor->tclk) {
			clk_enable(sensor->tclk);
			DEBUGMSG ("%s: clock enabled? %d\n", __func__, clk_is_enabled(sensor->tclk));
			msleep(5); // T2
			gpio_set_value(sensor->platform_data->power_down,0);
			DEBUGMSG ("%s: clear power-down pin 0x%x, level %u\n", __func__, sensor->platform_data->power_down, gpio_get_value(sensor->platform_data->power_down) );
			msleep(1); // T3
			gpio_set_value(sensor->platform_data->reset,1);
			msleep(20);
		} else {
			printk(KERN_ERR "%s: missing power_down or reset GPIO\n", __func__ );
			retval = -EINVAL ;
			sensor->enable_count-- ;
		}
	}
	return retval ;
}

static void ov5640_disable(struct sensor *sensor)
{
	if ((0 < sensor->enable_count) && (0 == --sensor->enable_count)) {
		gpio_set_value(sensor->platform_data->power_down,1);
		gpio_set_value(sensor->platform_data->reset,0);
		if(sensor->tclk) {
			clk_disable(sensor->tclk);
			DEBUGMSG ("%s: clock enabled? %d\n", __func__, clk_is_enabled(sensor->tclk));
		} else
			printk(KERN_ERR "%s: no clock\n", __func__ );
	}
}

static s32 ov5640_read_reg(struct i2c_client *i2c,u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

// DEBUGMSG ("%s: 0x%04x\n", __func__, reg );
	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if (2 != i2c_master_send(i2c, au8RegBuf, 2)) {
		pr_err("%s:read reg error:reg=%x\n",
				__func__, reg);
		return -1;
	}

	if (1 != i2c_master_recv(i2c, &u8RdVal, 1)) {
		pr_err("%s:read reg error:reg=%x,val=%x\n",
				__func__, reg, u8RdVal);
		return -1;
	}

// DEBUGMSG ("%s: == 0x%02x\n", __func__, u8RdVal );
	*val = u8RdVal;

	return u8RdVal;
}

static s32 ov5640_write_reg(struct i2c_client *i2c,u16 reg, u8 val)
{
	u8 readback ;
	u8 au8Buf[3] = {0};

// DEBUGMSG ("%s: %04x -> %02x\n", __func__, reg, val );
	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(i2c, au8Buf, 3) < 0) {
		pr_err("%s:write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	if(0 > ov5640_read_reg(i2c,reg, &readback)){
		pr_err("%s: readback err\n", __func__ );
	} else if(readback != val) {
		pr_err("%s: readback mismatch: reg %04x %02x != %02x\n", __func__, reg, val, readback);
	}

	return 0;
}

static int ov5640_init_mode(struct sensor *sensor, struct ov5640_setting const *s)
{
	struct ov5640_reg_value const *pModeSetting = NULL;
	s32 i = 0;
	s32 iModeSettingArySize = 0;
	int retval = 0;

printk(KERN_ERR "%s: mode %u (%ux%u, %s)\n", __func__, s-sensor->settings, s->xres, s->yres, s->name );

	pModeSetting = s->registers ;
	iModeSettingArySize = s->reg_count ;

	sensor->pix.width = s->xres ;
	sensor->pix.height = s->yres ;

	for (i = 0; i < iModeSettingArySize; ++i, ++pModeSetting) {
		retval = ov5640_write_reg(sensor->i2c_client,pModeSetting->addr, pModeSetting->value);
		if (retval < 0)
			goto err;
	}
err:
	return retval;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_subdev *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = to_sensor(s);
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

DEBUGMSG ("%s\n", __func__ );
	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
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
static int ioctl_s_parm(struct v4l2_subdev *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = to_sensor(s);
	int ret = 0;

	if (a->type==V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		if (a->parm.capture.capturemode < sensor->num_settings) {
			sensor->cur_format = sensor->streamcap.capturemode = (u32)a->parm.capture.capturemode;
		}
		else {
			pr_err("%s: invalid mode %u\n", __func__, a->parm.capture.capturemode);
			ret = -EINVAL ;
		}
	} else {
		pr_err("%s: unknown type %d\n", __func__, a->type);
		ret = -EINVAL;
	}

	return ret;
}

static void pixformat_from_setting
	( struct ov5640_setting const *s,
	  struct v4l2_pix_format *pix )
{
	pix->width = s->xres ;
	pix->height = s->yres ;
	pix->pixelformat = s->v4l_fourcc ;
	pix->field = V4L2_FIELD_ANY ;
	pix->bytesperline = s->stride ;
	pix->sizeimage = s->sizeimage ;
	pix->colorspace = V4L2_COLORSPACE_JPEG ;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_subdev *s, struct v4l2_format *f)
{
	int retval = -EINVAL ;
	struct sensor *sensor = to_sensor(s);
	if (sensor && ((unsigned)sensor->cur_format < sensor->num_settings)) {
		struct ov5640_setting const *s = sensor->settings+sensor->cur_format ;
		f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
                pixformat_from_setting(s,&sensor->pix);
		memcpy(&f->fmt.pix, &sensor->pix, sizeof(f->fmt.pix));
		retval = 0 ;
	} else {
		printk(KERN_ERR "%s: no sensor(%p) or bad format(%d)\n", __func__, sensor, sensor ? sensor->cur_format : -1 );
		memset(&f->fmt.pix, 0, sizeof(f->fmt.pix));
	}
DEBUGMSG ("%s: %ux%u, fourcc %04x colorspace %u, %u bpl\n", __func__, sensor->pix.width, sensor->pix.height, sensor->pix.pixelformat, sensor->pix.colorspace, sensor->pix.bytesperline);

	return retval ;
}

static int find_matching_format(struct sensor const *sensor, struct v4l2_pix_format const *f){
	int i ;
	for (i = 0 ; i < sensor->num_settings; i++) {
		struct ov5640_setting const *s = sensor->settings + i ;
		DEBUGMSG ("try %s\n", s->name);
		if ((f->width == s->xres)
		    &&
		    (f->height == s->yres)
		    &&
		    (f->pixelformat == s->v4l_fourcc)) {
			return i ;
		}
	}
	return -1 ;
}

static int ioctl_s_fmt_cap(struct v4l2_subdev *s, struct v4l2_format *f)
{
	int retval = -EINVAL ;
	struct sensor *sensor = to_sensor(s);
	if (sensor && (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		sensor->cur_format = find_matching_format(sensor,&f->fmt.pix);
		if ((unsigned)sensor->cur_format < sensor->num_settings) {
			DEBUGMSG ("%s: set format to %u:%s\n", __func__, sensor->cur_format, sensor->settings[sensor->cur_format].name );
			pixformat_from_setting(sensor->settings+sensor->cur_format,&sensor->pix);
			memcpy(&f->fmt.pix, &sensor->pix, sizeof(f->fmt.pix));
			retval = 0 ;
		} else {
			printk(KERN_ERR "%s: not found %ux%u, 4cc 0x%08x\n", __func__, f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.pixelformat);
		}
	}
DEBUGMSG ("%s: %ux%u, fourcc %04x colorspace %u\n", __func__, sensor->pix.width, sensor->pix.height, sensor->pix.pixelformat, sensor->pix.colorspace);

	return retval ;
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
static int ioctl_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *vc)
{
	int ret = 0;
        struct sensor *sensor = to_sensor(sd);
DEBUGMSG ("%s\n", __func__ );

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = sensor->brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = sensor->hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = sensor->contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = sensor->saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = sensor->red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = sensor->blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = sensor->ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *vc)
{
	int retval = 0;

	pr_err("In ov5640:ioctl_s_ctrl %d\n", vc->id);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		printk(KERN_ERR "%s: V4L2_CID_BRIGHTNESS: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_CONTRAST:
		printk(KERN_ERR "%s: V4L2_CID_CONTRAST: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_SATURATION:
		printk(KERN_ERR "%s: V4L2_CID_SATURATION: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_HUE:
		printk(KERN_ERR "%s: V4L2_CID_HUE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		printk(KERN_ERR "%s: V4L2_CID_AUTO_WHITE_BALANCE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		printk(KERN_ERR "%s: V4L2_CID_DO_WHITE_BALANCE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_RED_BALANCE:
		printk(KERN_ERR "%s: V4L2_CID_RED_BALANCE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_BLUE_BALANCE:
		printk(KERN_ERR "%s: V4L2_CID_BLUE_BALANCE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_GAMMA:
		printk(KERN_ERR "%s: V4L2_CID_GAMMA: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_EXPOSURE:
		printk(KERN_ERR "%s: V4L2_CID_EXPOSURE: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_AUTOGAIN:
		printk(KERN_ERR "%s: V4L2_CID_AUTOGAIN: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_GAIN:
		printk(KERN_ERR "%s: V4L2_CID_GAIN: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_HFLIP:
		printk(KERN_ERR "%s: V4L2_CID_HFLIP: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_VFLIP:
		printk(KERN_ERR "%s: V4L2_CID_VFLIP: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_PRIVATE_BASE:
		printk(KERN_ERR "%s: MXC rotation control: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_PRIVATE_BASE+1:
		printk(KERN_ERR "%s: MXC flash control: %d\n", __func__, vc->value );
		break;
	case V4L2_CID_PRIVATE_BASE+2:
		printk(KERN_ERR "%s: MXC display control: %d\n", __func__, vc->value );
		break;
	default:
		printk(KERN_ERR "%s: unsupported control: value %d\n", __func__, vc->value );
		retval = -EPERM;
		break;
	}

	return retval;
}

static int ioctl_enum_fmt_cap(struct v4l2_subdev *s, struct v4l2_fmtdesc *f)
{
	int retval = -EINVAL ;
        struct sensor *sensor = to_sensor(s);
printk(KERN_ERR "%s: %d\n", __func__, f->index );
	if (f->index < sensor->num_settings) {
		struct ov5640_setting const *s = sensor->settings + f->index ;
		f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
		f->flags = 0 ;
		strcpy(f->description,s->name);
		f->pixelformat = s->v4l_fourcc ;
		retval = 0 ;
	}
	return retval ;
}

static int ioctl_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *a)
{
	int retval = 0;
        a->flags = V4L2_CTRL_FLAG_DISABLED ;

	switch (a->id) {
	case V4L2_CID_BRIGHTNESS:
		printk(KERN_ERR "V4L2_CID_BRIGHTNESS\n");
		break;
	case V4L2_CID_HUE:
		printk(KERN_ERR "V4L2_CID_HUE\n");
		break;
	case V4L2_CID_CONTRAST:
		printk(KERN_ERR "V4L2_CID_CONTRAST\n");
		break;
	case V4L2_CID_SATURATION:
		printk(KERN_ERR "V4L2_CID_SATURATION\n");
		break;
	case V4L2_CID_RED_BALANCE:
		printk(KERN_ERR "V4L2_CID_RED_BALANCE\n");
		break;
	case V4L2_CID_BLUE_BALANCE:
		printk(KERN_ERR "V4L2_CID_BLUE_BALANCE\n");
		break;
	case V4L2_CID_EXPOSURE:
		printk(KERN_ERR "V4L2_CID_EXPOSURE\n");
		break;
	case V4L2_CID_HFLIP:
		printk(KERN_ERR "V4L2_CID_HFLIP\n");
		break;
	case V4L2_CID_VFLIP:
		printk(KERN_ERR "V4L2_CID_VFLIP\n");
		break;
	default:
		printk(KERN_ERR "unrecognized ctrl: %d\n", a->id);
		if(V4L2_CID_LASTP1 < a->id)
			retval = -EINVAL ;
	}

	return retval ;
}

static int ioctl_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct sensor *sensor = to_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int rval = v4l2_chip_ident_i2c_client(client, chip, sensor->ident, 0);
printk (KERN_ERR "%s: %d: %x\n", __func__, rval, rval? sensor->ident: 0 );
	return rval ;
}

static int ov5640_init(struct v4l2_subdev *sd, u32 val)
{
	struct sensor *sensor = to_sensor(sd);
	sensor->dev = (struct device *)val ;

	printk (KERN_ERR "%s: %u\n", __func__, val );
	return 0 ;
}

static int next_field
	( unsigned char const **d,
	  unsigned *bytes_left,
	  void *field,
	  unsigned fieldsize){
	if(*bytes_left >= fieldsize) {
		*bytes_left -= fieldsize ;
		memcpy(field,*d,fieldsize);
		*d += fieldsize ;
		return 0 ;
	}
	printk (KERN_ERR "next firmware field: %u bytes exceeds max %u\n", fieldsize, *bytes_left);
	printk (KERN_ERR "sizeof(struct ov5640_setting) == %u\n", sizeof(struct ov5640_setting));
	printk (KERN_ERR "offsetof(struct ov5640_setting,fps) == %u\n", offsetof(struct ov5640_setting,fps));
	printk (KERN_ERR "offsetof(struct ov5640_setting,mclk) == %u\n", offsetof(struct ov5640_setting,mclk));
	printk (KERN_ERR "offsetof(struct ov5640_setting,registers) == %u\n", offsetof(struct ov5640_setting,registers));
	return 1 ;
}

static void free_settings(struct sensor *sensor) {
	if (sensor->settings) {
		unsigned i ;
		for (i=0; i < sensor->num_settings ; i++ ) {
                        struct ov5640_setting *s = sensor->settings+i ;
			if (s->registers)
				kfree(s->registers);
		}
		kfree(sensor->settings);
		sensor->settings = 0 ;
		sensor->num_settings = 0 ;
	}
}

static int validate_fw
	(const struct firmware *fw,
         struct sensor *sensor)
{
	unsigned i ;
	unsigned char const *data = (unsigned char *)fw->data ;
	unsigned left = fw->size ;
	unsigned long num_settings ;
	if (0 != next_field(&data,&left,&num_settings,sizeof(num_settings)))
		goto bad_data ;

	if (sensor->settings) {
		free_settings(sensor);
	} // clear old data

	printk (KERN_ERR "%s: %lu settings in firmware file\n", __func__, num_settings );
	sensor->settings = kzalloc(num_settings*sizeof(*sensor->settings),GFP_KERNEL);
	if (0 == sensor->settings){
		printk (KERN_ERR "%s: alloc error\n", __func__);
		goto bad_data ;
	}

	for (i=0; i < num_settings; i++) {
		unsigned save_crc ;
		unsigned early_crc,crc ;
		unsigned reg_bytes ;
                struct ov5640_setting *s = sensor->settings+i ;
		if (0 != next_field(&data,&left,s,OV5640_SETTING_SIZE)) {
			printk (KERN_ERR "%s: Error reading setting %u\n", __func__, i );
			goto free_data ;
		}

		save_crc = s->crc ;
		s->crc = 0 ;		// clear dynamic fields
		s->registers = 0 ;
		early_crc = zlib_adler32(0,(Byte *)s,OV5640_SETTING_SIZE);
                reg_bytes = s->reg_count*sizeof(s->registers[0]);
		s->registers = (struct ov5640_reg_value *)kzalloc(reg_bytes,GFP_KERNEL);
		if (0 == s->registers) {
			printk (KERN_ERR "%s: error allocating %u bytes of regs\n", __func__, reg_bytes );
			goto free_data ;
		}
		sensor->num_settings++ ;

		if (0 != next_field(&data,&left,(void *)s->registers,reg_bytes)) {
			printk (KERN_ERR "%s: Error reading %u bytes of regs\n", __func__, reg_bytes );
			goto free_data ;
		}
		crc = zlib_adler32(early_crc,(Byte *)s->registers,reg_bytes);
		if (crc != save_crc) {
			printk (KERN_ERR "%s: crc mismatch on setting[%u]:%s:%u registers:%08x+%08x:%08x\n", __func__, i, s->name,s->reg_count, early_crc,crc,save_crc);
			goto free_data ;
		}
	}

	printk (KERN_ERR "%s: %u settings loaded\n", __func__, sensor->num_settings);
	return 0 ;
free_data:
	free_settings(sensor);

bad_data:
	printk (KERN_ERR "%s: Error parsing firmware\n", __func__ );
	return -1 ;
}

static int ov5640_load_fw(struct v4l2_subdev *sd)
{
	int rv = -EINVAL ;
	struct sensor *sensor = to_sensor(sd);
	if (sensor && sensor->dev) {
		const struct firmware *fw_entry = 0;
		if (0 == (rv = request_firmware(&fw_entry, DRIVER_NAME, sensor->dev))) {
			printk (KERN_ERR "%s: firmware loaded: %u bytes at %p\n", __func__, fw_entry->size, fw_entry->data );
			if (0 == validate_fw(fw_entry,sensor)) {
				rv = 0 ;
			}
			release_firmware(fw_entry);
		} else {
			printk (KERN_ERR "%s: firmware not available: %d\n", __func__, rv);
		}
	} else {
		printk (KERN_ERR "%s: No sensor(%p) or dev(%p)\n", __func__, sensor, sensor?sensor->dev:0);
	}
	return rv ;
}

static int ov5640_reset(struct v4l2_subdev *sd, u32 val)
{
	printk (KERN_ERR "%s: %u\n", __func__, val );
	return 0 ;
}


/*
 * read_proc
 *	- dumps the 30xx registers
 */
static int read_proc(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	return snprintf(page,count,"%s: no range defined\n", __func__);
}

static int write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char localbuf[256];
	if( count < sizeof(localbuf) ){
		if(copy_from_user(localbuf,buffer,count)){
			printk(KERN_ERR "Error reading user buf\n" );
		} else {
			struct sensor *sensor = (struct sensor *)data ;
			localbuf[count]=0;
			if (0 == ov5640_enable(sensor)) {
				int addr0 ;
				int addr1 ;
				int value ;
				int numScanned ;
				if( (2 == (numScanned = sscanf(localbuf,"%04x-%04x", &addr0, &addr1)))
				    ||
				    (2 == (numScanned = sscanf(localbuf,"%04x:%04x", &addr0, &addr1))) ){
					if (addr0 > addr1) {
						int tmp = addr0 ;
						addr0 = addr1 ;
						addr1 = tmp ;
					} // swap
					if (0xFFFF >= addr1) {
						int r ;
						for (r=addr0; r<=addr1; r++) {
							unsigned char regval ;
							if( 0 <= ov5640_read_reg(sensor->i2c_client,r, &regval) ){
								printk(KERN_ERR "reg[%04x] == %02x\n", r, regval );
							}
							else
								printk(KERN_ERR "Error reading register %04x\n", r);
						}
					} else
						printk (KERN_ERR "Invalid register addresses (max 0xFFFF)\n");
				} else if(2 == (numScanned = sscanf(localbuf,"%04x %02x", &addr0, &value)) ){
					if( (0xFFFF >= addr0) && (0xff >= value) ){
						ov5640_write_reg(sensor->i2c_client,addr0, value);
					}
					else
						printk(KERN_ERR "Invalid data: %s\n", localbuf);
				} else if(1 == numScanned){
					if(0xFFFF > addr0){
						unsigned char regval ;
						if( 0 <= ov5640_read_reg(sensor->i2c_client,addr0, &regval) ){
							printk(KERN_ERR "reg[%04x] == %02x\n", addr0, regval );
						}
						else
							printk(KERN_ERR "Error reading register %04x\n", addr0);
					}
				}
				else
					printk(KERN_ERR "Invalid data: %s\n", localbuf);
				ov5640_disable(sensor);
			} else
				printk(KERN_ERR "%s: error enabling camera\n", __func__);
		}
	}

	return count ;
}

static struct regulator *init_regulator(char const *name, struct device *dev,int min_uV, int max_uV)
{
	struct regulator *rval = 0 ;
	if (name) {
		rval = regulator_get(dev, name);
                if (!IS_ERR(rval)) {
			dev_info(dev, "%s: setting regulator %s voltage to %u/%u\n", __func__, name, min_uV, max_uV );
			if ((0 <= min_uV) && (0 <= max_uV)) {
				regulator_set_voltage(rval, min_uV, max_uV );
			}
			if (0 == regulator_enable(rval)) {
				dev_info(dev, "%s: enabled regulator %s\n", __func__, name );
			} else {
				dev_err(dev, "%s: error enabling regulator %s\n", __func__, name );
				regulator_put(rval);
				rval = 0 ;
			}
		} else {
			dev_err(dev, "%s: error getting regulator %s\n", __func__, name);
			rval = 0 ;
		}
	}
	return rval ;
}

static int ioctl_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0 ;
	struct sensor *sensor = to_sensor(sd);

	if (enable) {
		if (0 == ov5640_enable(sensor)) {
			printk(KERN_ERR "%s: sensor enabled\n", __func__ );
			ret = ov5640_init_mode(sensor, sensor->settings+sensor->cur_format);
			if (0 == ret) {
				printk(KERN_ERR "%s: enabled sensor with format %d (%s)\n", __func__, sensor->cur_format, sensor->settings[sensor->cur_format].name);
			} else {
				printk(KERN_ERR "%s: Error initializing mode\n", __func__ );
			}
		} else
			printk(KERN_ERR "%s: error enabling sensor\n", __func__ );
	} else {
                ov5640_disable(sensor);
	}

	return ret ;
}

static long ov5640_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	if (vidioc_int_g_ifparm_num == cmd) {
		struct sensor *sensor = to_sensor(sd);
		printk (KERN_ERR "%s: fill in ifparm here\n", __func__ );
                if ((unsigned)sensor->cur_format < sensor->num_settings) {
			memcpy(arg,sensor->settings+sensor->cur_format,sizeof(sensor->settings[0]));
			return 0 ;
		} else {
			printk (KERN_ERR "%s: g_ifparm_num: no current setting\n", __func__ );
			return -EINVAL ;
		}
	}
	printk( KERN_ERR "%s: cmd %d, arg %p\n", __func__, cmd, arg );
	return -ENOTSUPP ;
}

/* ----------------------------------------------------------------------- */
static const struct v4l2_subdev_core_ops ov5640_core_ops = {
	.g_ctrl = ioctl_g_ctrl,
	.s_ctrl = ioctl_s_ctrl,
	.queryctrl = ioctl_queryctrl,
	.g_chip_ident = ioctl_g_chip_ident,
	.init = ov5640_init,
	.load_fw = ov5640_load_fw,
	.reset = ov5640_reset,
	.ioctl = ov5640_ioctl,
};

static const struct v4l2_subdev_video_ops ov5640_video_ops = {
	.g_fmt = ioctl_g_fmt_cap,
	.s_fmt = ioctl_s_fmt_cap,
	.s_stream = ioctl_s_stream,
	.enum_fmt = ioctl_enum_fmt_cap,
	.g_parm = ioctl_g_parm,
	.s_parm = ioctl_s_parm,
};

static const struct v4l2_subdev_ops ov5640_ops = {
	.core = &ov5640_core_ops,
	.video = &ov5640_video_ops,
};

#define CLOCK_NAME "csi_mclk1"
/*!
 * ov5640 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int retval;
	int i;
	u8 RegVal = 0;
	struct mxc_camera_platform_data *plat_data = client->dev.platform_data;
        struct proc_dir_entry *pde ;
        struct sensor *sensor ;

	printk(KERN_ERR "%s: %s: %p\n", __func__, id->name, plat_data );
	if (0 == plat_data) {
		printk (KERN_ERR "%s: invalid platform data\n", __func__ );
		return 0 ;
	}
	printk(KERN_ERR "%s: client %p\n", __func__, client );

	sensor = kzalloc(sizeof(*sensor),GFP_KERNEL);
	dev_set_drvdata(&client->dev,sensor);
	sensor->ident = V4L2_IDENT_OV5640 ;

	v4l_info(client, "chip found @ 0x%x (%s)\n", client->addr, client->adapter->name);

	sensor->platform_data = plat_data ;
printk(KERN_ERR "%s: i2c addr 0x%x, csi %d, clock %d, priv %p, driver data %p\n", __func__, client->addr, plat_data->csi, plat_data->mclk, client->dev.p, dev_get_drvdata(&client->dev));

	if(plat_data->power_down) {
		printk(KERN_ERR "%s: power-down pin 0x%x, level %u\n", __func__, plat_data->power_down, gpio_get_value(plat_data->power_down) );
		gpio_set_value(plat_data->power_down,1);
	} else
		printk(KERN_ERR "%s: no power-down pin\n", __func__ );

	if(plat_data->reset) {
		printk(KERN_ERR "%s: reset pin 0x%x, level %u\n", __func__, plat_data->reset, gpio_get_value(plat_data->reset) );
		gpio_set_value(plat_data->reset,0);
	} else
		printk(KERN_ERR "%s: no reset pin\n", __func__ );

	/* Set initial values for the sensor struct. */
	sensor->cur_format = 0 ;

	printk(KERN_ERR "platform clock is %d\n", sensor->platform_data->mclk);
	sensor->tclk = clk_get(NULL, CLOCK_NAME);
	if(sensor->tclk){
		int mclk = sensor->platform_data->mclk;
		if (OV5640_XCLK_MAX < mclk) {
			mclk = OV5640_XCLK_MAX ;
			printk (KERN_INFO "%s: clamping input clock to %u from %u Hz\n", __func__, mclk, sensor->platform_data->mclk);
		} else if (OV5640_XCLK_MIN > mclk) {
			mclk = OV5640_XCLK_MIN ;
			printk (KERN_INFO "%s: clamping input clock to %u from %u Hz\n", __func__, mclk, sensor->platform_data->mclk);
		}

		printk(KERN_ERR "%s: have clock %s/%p, rate %lu\n", __func__, CLOCK_NAME, sensor->tclk, clk_get_rate(sensor->tclk));
		clk_set_rate(sensor->tclk,mclk);
		printk(KERN_ERR "%s: clock now %s/%p, rate %lu\n", __func__, CLOCK_NAME, sensor->tclk, clk_get_rate(sensor->tclk));
		printk(KERN_ERR "%s: enabled? %d\n", __func__, clk_is_enabled(sensor->tclk));
	} else
		printk(KERN_ERR "%s: error gettin clock %s\n", __func__, CLOCK_NAME );

	sensor->i2c_client = client;
	sensor->pix.pixelformat = V4L2_PIX_FMT_UYVY;
	sensor->pix.width = 640;
	sensor->pix.height = 480;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = 1;
	sensor->streamcap.timeperframe.numerator = 15;

        sensor->regulators[IO_REGULATOR] = init_regulator(plat_data->io_regulator,&client->dev,OV5640_VOLTAGE_DIGITAL_IO,OV5640_VOLTAGE_DIGITAL_IO);
        sensor->regulators[CORE_REGULATOR] = init_regulator(plat_data->core_regulator,&client->dev,OV5640_VOLTAGE_DIGITAL_CORE,OV5640_VOLTAGE_DIGITAL_CORE);
        sensor->regulators[ANALOG_REGULATOR] = init_regulator(plat_data->analog_regulator,&client->dev,OV5640_VOLTAGE_ANALOG,OV5640_VOLTAGE_ANALOG);
        sensor->regulators[GPO_REGULATOR] = init_regulator(plat_data->gpo_regulator,&client->dev,OV5640_VOLTAGE_GPIO,OV5640_VOLTAGE_GPIO);

	if (0 != ov5640_power_on(sensor)) {
		printk (KERN_ERR "%s: error powering on camera\n", __func__ );
		goto err3 ;
	}

	if (0 != ov5640_enable(sensor)) {
		printk (KERN_ERR "%s: error enabling sensor\n", __func__ );
		goto err4 ;
	}

	retval = ov5640_read_reg(client,CHIPID_HIGH, &RegVal);
	if( (retval < 0) || (RegVal != 0x56) ){
		printk(KERN_ERR "%s: not ov5642: %d/%u\n", __func__, retval, RegVal);
		retval = -ENOENT ;
		goto err5 ;
	}

	retval = ov5640_read_reg(client,CHIPID_LOW, &RegVal);
	if( (retval < 0) || (RegVal != 0x42) ){
		printk(KERN_ERR "%s: not ov5642: %d/%u\n", __func__, retval, RegVal);
		retval = -ENOENT ;
		goto err5 ;
	}
	else
		retval = 0 ;

	pde = create_proc_entry("driver/ov5640", 0, 0);
	if( pde ) {
		pde->read_proc  = read_proc ;
		pde->write_proc = write_proc ;
		pde->data = sensor ;
	}
	else
		printk( KERN_ERR "Error creating 3000-register proc entry\n" );

	v4l2_i2c_subdev_init(&sensor->sd, client, &ov5640_ops);
	printk (KERN_ERR "%s: subdev name <%s>\n", __func__, sensor->sd.name );

	ov5640_disable(sensor);

	printk(KERN_ERR "%s: registered Omnivision 5642 camera\n", __func__ );

	return retval;
err5:
	ov5640_disable(sensor);

err4:
	ov5640_power_off(sensor);

err3:
	for (i = 0; i < NUM_REGULATORS; i++) {
		if (sensor->regulators[i]) {
			regulator_put(sensor->regulators[i]);
                        sensor->regulators[i] = 0 ;
		}
	}
	if(sensor->tclk) {
                clk_disable(sensor->tclk);
		clk_put(sensor->tclk);
		sensor->tclk = 0 ;
	}
	kfree(sensor);
	return -1;
}

/*!
 * ov5640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov5640_remove(struct i2c_client *client)
{
	int i ;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct sensor *sensor = to_sensor(sd);

	if ((0 == sd) || (0 == sensor) ){
		printk (KERN_ERR "%s: invalid sd(%p) or sensor(%p)\n", __func__, sd, sensor);
		return -1 ;
	}
	printk(KERN_ERR "%s: sensor %p, driver data %p\n", __func__, sensor, dev_get_drvdata(&client->dev) );

	if(sensor->tclk) {
		printk(KERN_ERR "release clock %p\n", sensor->tclk);
                clk_disable(sensor->tclk);
		clk_put(sensor->tclk);
		sensor->tclk = 0 ;
	}
	remove_proc_entry("driver/ov5640", NULL /* parent dir */);

	ov5640_power_off(sensor);

	for (i=0; i < NUM_REGULATORS; i++) {
		if (sensor->regulators[i]) {
                        regulator_put(sensor->regulators[i]);
			sensor->regulators[i]=0;
		}
	}
	v4l2_device_unregister_subdev(sd);
	return 0;
}
