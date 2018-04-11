/*
 *   Boundary Devices FTx06 touch screen controller.
 *
 *   Copyright (c) by Boundary Devices <info@boundarydevices.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/input/mt.h>

#ifdef CONFIG_TOUCHSCREEN_FT5X06_SINGLE_TOUCH
#define USE_ABS_SINGLE
#else
#define USE_ABS_SINGLE
#define USE_ABS_MT
#endif

#define WORK_MODE	0
#define FACTORY_MODE	4

static int calibration[7] = {
	65536,0,0,
	0,65536,0,
	0
};
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

static int screenres[2];
module_param_array(screenres, int, NULL, S_IRUGO | S_IWUSR);

#define MAX_TOUCHES 12

static void translate(int *px, int *py)
{
	int x, y, x1, y1;
	if (calibration[6]) {
		x1 = *px;
		y1 = *py;

		x = calibration[0] * x1 +
			calibration[1] * y1 +
			calibration[2];
		x /= calibration[6];
		if (x < 0)
			x = 0;
		y = calibration[3] * x1 +
			calibration[4] * y1 +
			calibration[5];
		y /= calibration[6];
		if (y < 0)
			y = 0;
		*px = x ;
		*py = y ;
	}
}

struct point {
	int	x;
	int	y;
	int	id;
};

struct ft5x06_ts {
	struct i2c_client *client;
	struct input_dev	*idev;
	int			use_count;
	int			bReady;
	int			irq;
	struct gpio_desc	*wakeup_gpio;
	struct gpio_desc	*reset_gpio;
	struct timer_list	release_timer;
	unsigned		down_mask;
	unsigned		max_x;
	unsigned		max_y;
	unsigned		firmware_bug_hit;
	struct regmap		*regmap;
};
static const char *client_name = "ft5x06";

static int ts_startup(struct ft5x06_ts *ts);
static void ts_shutdown(struct ft5x06_ts *ts);

/*-----------------------------------------------------------------------*/
static void write_reg(struct ft5x06_ts *ts, int regnum, int value)
{
	u8 regnval[] = {
		regnum,
		value
	};
	struct i2c_msg pkt = {
		ts->client->addr, 0, sizeof(regnval), regnval
	};
	int ret = i2c_transfer(ts->client->adapter, &pkt, 1);
	if (ret != 1)
		printk(KERN_WARNING "%s: i2c_transfer failed\n", __func__);
	else
		printk(KERN_DEBUG "%s: set register 0x%02x to 0x%02x\n",
		       __func__, regnum, value);
}

static void set_mode(struct ft5x06_ts *ts, int mode)
{
	write_reg(ts, 0, (mode&7)<<4);
	printk(KERN_DEBUG "%s: changed mode to 0x%02x\n", __func__, mode);
}

static void release_slots(struct ft5x06_ts *ts, unsigned mask)
{
	struct input_dev *idev = ts->idev;

	while (mask) {
		int slot = __ffs(mask);

		mask &= ~(1 << slot);
		input_mt_slot(idev, slot);
		input_mt_report_slot_state(idev,  MT_TOOL_FINGER, 0);
	}
}

static inline void ts_evt_add(struct ft5x06_ts *ts,
			      unsigned buttons, struct point *p)
{
	struct input_dev *idev = ts->idev;
	unsigned down_mask = 0;
	unsigned tmp;
	int i;
	if (!buttons) {
		/* send release to user space. */
#ifdef USE_ABS_MT
		tmp = ts->down_mask;
		ts->down_mask = 0;
		release_slots(ts, tmp);
#else
		ts->down_mask = 0;
#endif
#ifdef USE_ABS_SINGLE
		input_report_abs(idev, ABS_PRESSURE, 0);
		input_report_key(idev, BTN_TOUCH, 0);
#endif
	} else {
#ifdef USE_ABS_MT
		for (i = 0; i < buttons; i++) {
			translate(&p[i].x, &p[i].y);
			input_mt_slot(idev, p[i].id);
			input_mt_report_slot_state(idev,  MT_TOOL_FINGER, 1);
			down_mask |= 1 << p[i].id;
			input_report_abs(idev, ABS_MT_POSITION_X, p[i].x);
			input_report_abs(idev, ABS_MT_POSITION_Y, p[i].y);
		}
		tmp = ts->down_mask & ~down_mask;
		ts->down_mask = down_mask;
		release_slots(ts, tmp);
#else
		ts->down_mask = 1;
		translate(&p[0].x, &p[0].y);
#endif
#ifdef USE_ABS_SINGLE
		input_report_abs(idev, ABS_X, p[0].x);
		input_report_abs(idev, ABS_Y, p[0].y);
		input_report_abs(idev, ABS_PRESSURE, 1);
		input_report_key(idev, BTN_TOUCH, 1);
#endif
	}
	input_sync(idev);
}

static void ts_release_timer(unsigned long data)
{
	struct ft5x06_ts *ts = (struct ft5x06_ts *)data;

	if (ts->down_mask) {
		ts_evt_add(ts, 0, NULL);
		ts->firmware_bug_hit++;
		if ((ts->firmware_bug_hit & 0xff) == 1) {
			dev_info(&ts->client->dev, "release firmware bug hit("
					"%d times)\n", ts->firmware_bug_hit);
		}
	}
}

static int ts_open(struct input_dev *idev)
{
	struct ft5x06_ts *ts = input_get_drvdata(idev);
	return ts_startup(ts);
}

static void ts_close(struct input_dev *idev)
{
	struct ft5x06_ts *ts = input_get_drvdata(idev);
	ts_shutdown(ts);
}

static inline int ts_register(struct ft5x06_ts *ts)
{
	struct input_dev *idev;
	idev = input_allocate_device();
	if (idev == NULL)
		return -ENOMEM;

	ts->max_x = 0x7ff;
	ts->max_y = 0x7ff;
	if (screenres[0])
		ts->max_x = screenres[0] - 1;
	else if (num_registered_fb > 0)
		ts->max_x = registered_fb[0]->var.xres - 1;
	if (screenres[1])
		ts->max_y = screenres[1] - 1;
	else if (num_registered_fb > 0)
		ts->max_y = registered_fb[0]->var.yres - 1;

	pr_info("%s resolution is %dx%d\n", client_name, ts->max_x + 1, ts->max_y + 1);
	ts->idev = idev;
	idev->name      = client_name;
	idev->id.bustype = BUS_I2C;
	idev->id.product = ts->client->addr;
	idev->open      = ts_open;
	idev->close     = ts_close;

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(EV_KEY, idev->evbit);
	__set_bit(EV_SYN, idev->evbit);

#ifdef USE_ABS_MT
	input_mt_init_slots(idev, 16, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(idev, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);
	input_set_abs_params(idev, ABS_MT_TRACKING_ID, 0, MAX_TOUCHES, 0, 0);
#endif
#ifdef USE_ABS_SINGLE
	__set_bit(BTN_TOUCH, idev->keybit);
	input_set_abs_params(idev, ABS_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, ts->max_y, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0, 0);
#endif

	input_set_drvdata(idev, ts);
	return input_register_device(idev);
}

static inline void ts_deregister(struct ft5x06_ts *ts)
{
	if (ts->idev) {
		input_unregister_device(ts->idev);
		input_free_device(ts->idev);
		ts->idev = NULL;
	}
}

#ifdef DEBUG
static void printHex(u8 const *buf, unsigned len)
{
	char hex[512];
	char *next = hex ;
	char *end = hex+sizeof(hex);

	while (len--) {
		next += snprintf(next, end-next, "%02x", *buf++);
		if (next >= end) {
			hex[sizeof(hex)-1] = '\0' ;
			break;
		}
	}
	printk(KERN_ERR "%s\n", hex);
}
#endif

static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct ft5x06_ts *ts = id;
	int ret;
	struct point points[MAX_TOUCHES];
	unsigned char buf[3+(6*MAX_TOUCHES)];

	unsigned char startch[1] = { 0 };
	struct i2c_msg readpkt[2] = {
		{ts->client->addr, 0, 1, startch},
		{ts->client->addr, I2C_M_RD, sizeof(buf), buf}
	};
	int buttons = 0 ;
	int i;
	unsigned char *p;

	del_timer_sync(&ts->release_timer);
	while (gpiod_get_value(ts->wakeup_gpio)) {
		ts->bReady = 0;
		ret = i2c_transfer(ts->client->adapter, readpkt,
				   ARRAY_SIZE(readpkt));
		if (ret != ARRAY_SIZE(readpkt)) {
			dev_err(&ts->client->dev,
				"i2c_transfer failed(%d)\n", ret);
			msleep(100);
			continue;
		}
		p = buf+3;
#ifdef DEBUG
		printHex(buf, sizeof(buf));
#endif
		buttons = buf[2];
		if (buttons > MAX_TOUCHES) {
			int interrupting = gpiod_get_value(ts->wakeup_gpio);
			if (!interrupting) {
				dev_info(&ts->client->dev,
					"invalid button count 0x%02x"
					", irq inactive\n", buttons);
				break;
			}
			/* not garbage from POR */
			dev_err(&ts->client->dev,
				"invalid button count 0x%02x\n", buttons);
			buttons = MAX_TOUCHES;
		}
		for (i = 0; i < buttons; i++) {
			points[i].x = (((p[0] & 0x0f) << 8) | p[1]) & 0x7ff;
			points[i].id = (p[2]>>4);
			points[i].y = (((p[2] & 0x0f) << 8) | p[3]) & 0x7ff;
			if (points[i].x > ts->max_x)
				points[i].x = ts->max_x;
			if (points[i].y > ts->max_y)
				points[i].y = ts->max_y;
			p += 6;
		}

#ifdef DEBUG
		printk(KERN_ERR "%s: buttons = %d, "
				"points[0].x = %d, "
				"points[0].y = %d\n",
		       client_name, buttons, points[0].x, points[0].y);
#endif
		ts_evt_add(ts, buttons, points);
	}
	if (ts->down_mask)
		mod_timer(&ts->release_timer, jiffies + msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

#define ID_G_THGROUP		0x80
#define ID_G_THPEAK		0x81
#define ID_G_THCAL		0x82
#define ID_G_THWATER		0x83
#define ID_G_THTEMP		0x84
#define ID_G_CTRL		0x86
#define ID_G_TIME_ENTER_MONITOR	0x87
#define ID_G_PERIODACTIVE	0x88
#define ID_G_PERIODMONITOR	0x89
#define ID_G_AUTO_CLB_MODE	0xa0
#define ID_G_LIB_VERSION_H	0xa1
#define ID_G_LIB_VERSION_L	0xa2
#define ID_G_CIPHER		0xa3
#define ID_G_MODE		0xa4
#define ID_G_FIRMID		0xa6
#define ID_G_FT5201ID		0xa8
#define ID_G_ERR		0xa9
#define ID_G_CLB		0xaa
#define ID_G_B_AREA_TH		0xae
#define FT5x06_MAX_REG_OFFSET	0xae

/* Undocumented registers */
#define FT5X0X_REG_HEIGHT_B		0x8a
#define FT5X0X_REG_MAX_FRAME		0x8b
#define FT5X0X_REG_FEG_FRAME		0x8e
#define FT5X0X_REG_LEFT_RIGHT_OFFSET	0x92
#define FT5X0X_REG_UP_DOWN_OFFSET	0x93
#define FT5X0X_REG_DISTANCE_LEFT_RIGHT	0x94
#define FT5X0X_REG_DISTANCE_UP_DOWN	0x95
#define FT5X0X_REG_MAX_X_HIGH		0x98
#define FT5X0X_REG_MAX_X_LOW		0x99
#define FT5X0X_REG_MAX_Y_HIGH		0x9a
#define FT5X0X_REG_MAX_Y_LOW		0x9b
#define FT5X0X_REG_K_X_HIGH		0x9c
#define FT5X0X_REG_K_X_LOW		0x9d
#define FT5X0X_REG_K_Y_HIGH		0x9e
#define FT5X0X_REG_K_Y_LOW		0x9f

static int ts_startup(struct ft5x06_ts *ts)
{
	int ret = 0;
	if (ts == NULL)
		return -EIO;

	if (ts->use_count++ != 0)
		goto out;

	ret = request_threaded_irq(ts->irq, NULL, ts_interrupt,
				     IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				     client_name, ts);
	if (ret) {
		pr_err("%s: error requesting irq %d\n", __func__, ts->irq);
		goto out;
	}

	set_mode(ts, WORK_MODE);
 out:
	if (ret)
		ts->use_count--;
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static void ts_shutdown(struct ft5x06_ts *ts)
{
	if (ts) {
		if (--ts->use_count == 0) {
			free_irq(ts->irq, ts);
		}
		del_timer_sync(&ts->release_timer);
	}
}
/*-----------------------------------------------------------------------*/

/* Return 0 if detection is successful, -ENODEV otherwise */
static int detect_ft5x06(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	char buffer;
	struct i2c_msg pkt = {
		client->addr,
		I2C_M_RD,
		sizeof(buffer),
		&buffer
	};
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;
	if (i2c_transfer(adapter, &pkt, 1) != 1)
		return -ENODEV;
	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int ts_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	int err = detect_ft5x06(client);
	if (!err)
		strlcpy(info->type, "ft5x06-ts", I2C_NAME_SIZE);
	return err;
}

static bool ft5x06_readable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ID_G_THGROUP:
	case ID_G_THPEAK:
	case ID_G_THCAL:
	case ID_G_THWATER:
	case ID_G_THTEMP:
	case ID_G_CTRL:
	case ID_G_TIME_ENTER_MONITOR:
	case ID_G_PERIODACTIVE:
	case ID_G_PERIODMONITOR:
	case ID_G_AUTO_CLB_MODE:
	case ID_G_LIB_VERSION_H:
	case ID_G_LIB_VERSION_L:
	case ID_G_CIPHER:
	case ID_G_MODE:
	case ID_G_FIRMID:
	case ID_G_FT5201ID:
	case ID_G_ERR:
	case ID_G_CLB:
	case ID_G_B_AREA_TH:
		return true;
	default:
		return false;
	}
}

static bool ft5x06_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ID_G_THGROUP:
	case ID_G_THPEAK:
	case ID_G_THCAL:
	case ID_G_THWATER:
	case ID_G_THTEMP:
	case ID_G_CTRL:
	case ID_G_TIME_ENTER_MONITOR:
	case ID_G_PERIODACTIVE:
	case ID_G_PERIODMONITOR:
	case ID_G_AUTO_CLB_MODE:
	case ID_G_CLB:
	case ID_G_B_AREA_TH:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config ft5x06_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FT5x06_MAX_REG_OFFSET,
	.readable_reg = ft5x06_readable,
	.writeable_reg = ft5x06_writeable,
};

static ssize_t show_missed_releases(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts *ts = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", ts ? ts->firmware_bug_hit : 0);
}

static DEVICE_ATTR(missed_releases, S_IRUGO, show_missed_releases, NULL);

static int ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct ft5x06_ts *ts;
	struct device *dev = &client->dev;
	struct gpio_desc *gp;
	int retry = 0;
	int val[ARRAY_SIZE(screenres)];
	struct device_node *np = client->dev.of_node;
	struct gpio_desc *wakeup_gpio;

	ts = kzalloc(sizeof(struct ft5x06_ts), GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "Couldn't allocate memory for %s\n", client_name);
		return -ENOMEM;
	}
	ts->client = client;
	ts->irq = client->irq ;

	gp = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
	dev_info(dev, "reset %p\n", gp);
	if (!IS_ERR(gp)) {
		/* release reset */
		ts->reset_gpio = gp;
		gpiod_set_value(gp, 0);
		msleep(1);
	}
	err = detect_ft5x06(client);
	if (err) {
		dev_err(dev, "Could not detect touch screen %d.\n", err);
		goto exit1;
	}
	/*
	 * This is allocated after successful detection to prevent races with other drivers.
	 * But retry, in case other drivers are not as kind.
	 */
	do {
		wakeup_gpio = devm_gpiod_get_index(dev, "wakeup", 0, GPIOD_IN);
		if (!IS_ERR(wakeup_gpio))
			break;
		msleep(100);
		retry++;
		if (retry > 20) {
			dev_err(dev, "wakeup %ld\n", PTR_ERR(wakeup_gpio));
			err = -ENODEV;
			goto exit1;
		}
	} while (1);

	if (of_property_read_u32_array(np, "screen-size", val,
				       ARRAY_SIZE(val)) == 0)
		memcpy(screenres, val, sizeof(screenres));

	ts->regmap = devm_regmap_init_i2c(client, &ft5x06_regmap);
	if (IS_ERR(ts->regmap)) {
		err = PTR_ERR(ts->regmap);
		dev_err(&client->dev, "Failed to allocate regmap: %d\n", err);
		goto exit1;
	}

	ts->wakeup_gpio = wakeup_gpio;
	dev_info(dev, "wakeup %p, retry=%d\n", wakeup_gpio, retry);
	dev_info(dev, "touchscreen irq=%i, wakeup_irq=%i\n", ts->irq,
			gpiod_to_irq(ts->wakeup_gpio));
	i2c_set_clientdata(client, ts);
	err = ts_register(ts);
	if (err == 0) {
		setup_timer(&ts->release_timer, ts_release_timer, (unsigned long)ts);
		if (device_create_file(dev, &dev_attr_missed_releases))
			dev_err(dev, "%s: error creating missed_releases entry\n", __func__);
		return 0;
	}

	printk(KERN_WARNING "%s: ts_register failed\n", client_name);
	ts_deregister(ts);
exit1:
	kfree(ts);
	return err;
}

static int ts_remove(struct i2c_client *client)
{
	struct ft5x06_ts *ts = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_missed_releases);
	ts_deregister(ts);
	if (ts->reset_gpio)
		gpiod_set_value(ts->reset_gpio, 1);
	kfree(ts);
	return 0;
}


/*-----------------------------------------------------------------------*/

static const struct i2c_device_id ts_idtable[] = {
	{ "ft5x06-ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ts_idtable);

static const struct of_device_id ft5x06_dt_ids[] = {
       {
               .compatible = "ft5x06,ft5x06-touch",
       }, {
               /* sentinel */
       }
};
MODULE_DEVICE_TABLE(of, ft5x06_dt_ids);

static struct i2c_driver ts_driver = {
	.driver = {

		.name		= "ft5x06-ts",
		.of_match_table = ft5x06_dt_ids,
	},
	.id_table	= ts_idtable,
	.probe		= ts_probe,
	.remove		= ts_remove,
	.detect		= ts_detect,
};

module_i2c_driver(ts_driver);

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface for FocalTech ft5x06 touch screen controller.");
MODULE_LICENSE("GPL");
