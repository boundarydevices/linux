/*
 * Driver for EETI eGalax Multiple Touch Controller
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 *
 * based on max11801_ts.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* EETI eGalax serial touch screen controller is a I2C based multiple
 * touch screen controller, it supports 5 point multiple touch. */

/* TODO:
  - auto idle mode support
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>

/*
 * Mouse Mode: some panel may configure the controller to mouse mode,
 * which can only report one point at a given time.
 * This driver will ignore events in this mode.
 */
#define REPORT_MODE_SINGLE		0x1
/*
 * Vendor Mode: this mode is used to transfer some vendor specific
 * messages.
 * This driver will ignore events in this mode.
 */
#define REPORT_MODE_VENDOR		0x3
/* Multiple Touch Mode */
#define REPORT_MODE_MTTOUCH		0x4

#define MAX_SUPPORT_POINTS		5

#define EVENT_MODE		0
#define EVENT_STATUS		1
#define EVENT_VALID_OFFSET	7
#define EVENT_VALID_MASK	(0x1 << EVENT_VALID_OFFSET)
#define EVENT_ID_OFFSET		2
#define EVENT_ID_MASK		(0xf << EVENT_ID_OFFSET)
#define EVENT_IN_RANGE		(0x1 << 1)
#define EVENT_DOWN_UP		(0X1 << 0)

#define MAX_I2C_DATA_LEN	10

#define EGALAX_MAX_X   32767
#define EGALAX_MAX_Y   32767
#define EGALAX_MAX_TRIES 100

struct egalax_pointer {
	bool valid;
	bool status;
	u16 x;
	u16 y;
};

struct egalax_ts {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct egalax_pointer		events[MAX_SUPPORT_POINTS];
};

static irqreturn_t egalax_ts_interrupt(int irq, void *dev_id)
{
	struct egalax_ts *ts = dev_id;
	struct input_dev *input_dev = ts->input_dev;
	struct i2c_client *client = ts->client;
	struct egalax_pointer *events = ts->events;
	u8 buf[MAX_I2C_DATA_LEN];
	int i, id, ret, x, y;
	int tries = 0;
	bool down, valid;
	u8 state;

	do {
		ret = i2c_master_recv(client, buf, MAX_I2C_DATA_LEN);
	} while (ret == -EAGAIN && tries++ < EGALAX_MAX_TRIES);

	if (ret < 0)
		return IRQ_HANDLED;

	dev_dbg(&client->dev, "recv ret:%d", ret);
	for (i = 0; i < MAX_I2C_DATA_LEN; i++)
		dev_dbg(&client->dev, " %x ", buf[i]);

	if (buf[0] != REPORT_MODE_VENDOR
	    && buf[0] != REPORT_MODE_SINGLE
	    && buf[0] != REPORT_MODE_MTTOUCH) {
		/* invalid point */
		return IRQ_HANDLED;
	}

	if (buf[0] == REPORT_MODE_VENDOR) {
		dev_dbg(&client->dev, "vendor message, ignored\n");
		return IRQ_HANDLED;
	}

	state = buf[1];
	x = (buf[3] << 8) | buf[2];
	y = (buf[5] << 8) | buf[4];

	/* Currently, the panel Freescale using on SMD board _NOT_
	 * support single pointer mode. All event are going to
	 * multiple pointer mode.  Add single pointer mode according
	 * to EETI eGalax I2C programming manual.
	 */
	if (buf[0] == REPORT_MODE_SINGLE) {
		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);
		input_report_key(input_dev, BTN_TOUCH, !!state);
		input_sync(input_dev);
		return IRQ_HANDLED;
	}

	valid = state & EVENT_VALID_MASK;
	id = (state & EVENT_ID_MASK) >> EVENT_ID_OFFSET;
	down = state & EVENT_DOWN_UP;

	if (!valid || id > MAX_SUPPORT_POINTS) {
		dev_dbg(&client->dev, "point invalid\n");
		return IRQ_HANDLED;
	}

	if (down) {
		events[id].valid = valid;
		events[id].status = down;
		events[id].x = x;
		events[id].y = y;

#ifdef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);
		input_event(ts->input_dev, EV_KEY, BTN_TOUCH, 1);
		input_report_abs(input_dev, ABS_PRESSURE, 1);
#endif
	} else {
		dev_dbg(&client->dev, "release id:%d\n", id);
		events[id].valid = 0;
		events[id].status = 0;
#ifdef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_report_abs(input_dev, ABS_PRESSURE, 0);
#else
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
		input_event(input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(input_dev);
#endif
	}

#ifndef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
	/* report all pointers */
	for (i = 0; i < MAX_SUPPORT_POINTS; i++) {
		if (!events[i].valid)
			continue;
		dev_dbg(&client->dev, "report id:%d valid:%d x:%d y:%d",
			i, valid, x, y);
			input_report_abs(input_dev,
				 ABS_MT_TRACKING_ID, i);
		input_report_abs(input_dev,
				 ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_X, events[i].x);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_Y, events[i].y);
		input_mt_sync(input_dev);
	}
#endif
	input_sync(input_dev);

	return IRQ_HANDLED;
}

/* wake up controller by an falling edge of interrupt gpio.  */
static int egalax_wake_up_device(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	int gpio;
	int ret;

	if (!np)
		return -ENODEV;

	gpio = of_get_named_gpio(np, "wakeup-gpios", 0);
	if (!gpio_is_valid(gpio))
		return -ENODEV;

	ret = gpio_request(gpio, "egalax_irq");
	if (ret < 0) {
		dev_err(&client->dev,
			"request gpio failed, cannot wake up controller: %d\n",
			ret);
		return ret;
	}

	/* wake up controller via an falling edge on IRQ gpio. */
	gpio_direction_output(gpio, 0);
	gpio_set_value(gpio, 1);

	/* controller should be waken up, return irq.  */
	gpio_direction_input(gpio);
	gpio_free(gpio);

	return 0;
}

static int egalax_firmware_version(struct i2c_client *client)
{
	static const u8 cmd[MAX_I2C_DATA_LEN] = { 0x03, 0x03, 0xa, 0x01, 0x41 };
	int ret;

	ret = i2c_master_send(client, cmd, MAX_I2C_DATA_LEN);
	if (ret < 0)
		return ret;

	return 0;
}

static int egalax_ts_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct egalax_ts *ts;
	struct input_dev *input_dev;
	int ret;
	int error;

	ts = kzalloc(sizeof(struct egalax_ts), GFP_KERNEL);
	if (!ts) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_ts;
	}

	ts->client = client;
	ts->input_dev = input_dev;

	/* controller may be in sleep, wake it up. */
	error = egalax_wake_up_device(client);
	if (error) {
		dev_err(&client->dev, "Failed to wake up the controller\n");
		goto err_free_dev;
	}

	ret = egalax_firmware_version(client);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read firmware version\n");
		error = -EIO;
		goto err_free_dev;
	}

	input_dev->name = "eGalax Touch Screen";
	input_dev->phys = "I2C",
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x0EEF;
	input_dev->id.product = 0x0020;
	input_dev->id.version = 0x0001;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	input_set_abs_params(input_dev, ABS_X, 0, EGALAX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, EGALAX_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 1, 0, 0);

#ifndef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				0, EGALAX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, EGALAX_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
			     MAX_SUPPORT_POINTS, 0, 0);
#endif
	input_set_drvdata(input_dev, ts);

	error = request_threaded_irq(client->irq, NULL, egalax_ts_interrupt,
				     IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				     "egalax_ts", ts);
	if (error < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_dev;
	}

	error = input_register_device(ts->input_dev);
	if (error)
		goto err_free_irq;

	i2c_set_clientdata(client, ts);
	return 0;

err_free_irq:
	free_irq(client->irq, ts);
err_free_dev:
	input_free_device(input_dev);
err_free_ts:
	kfree(ts);

	return error;
}

static int egalax_ts_remove(struct i2c_client *client)
{
	struct egalax_ts *ts = i2c_get_clientdata(client);

	free_irq(client->irq, ts);

	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static const struct i2c_device_id egalax_ts_id[] = {
	{ "egalax_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, egalax_ts_id);

#ifdef CONFIG_PM_SLEEP
static int egalax_ts_suspend(struct device *dev)
{
	static const u8 suspend_cmd[MAX_I2C_DATA_LEN] = {
		0x3, 0x6, 0xa, 0x3, 0x36, 0x3f, 0x2, 0, 0, 0
	};
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	ret = i2c_master_send(client, suspend_cmd, MAX_I2C_DATA_LEN);
	return ret > 0 ? 0 : ret;
}

static int egalax_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return egalax_wake_up_device(client);
}
#endif

static SIMPLE_DEV_PM_OPS(egalax_ts_pm_ops, egalax_ts_suspend, egalax_ts_resume);

static struct of_device_id egalax_ts_dt_ids[] = {
	{ .compatible = "eeti,egalax_ts" },
	{ /* sentinel */ }
};

static struct i2c_driver egalax_ts_driver = {
	.driver = {
		.name	= "egalax_ts",
		.owner	= THIS_MODULE,
		.pm	= &egalax_ts_pm_ops,
		.of_match_table	= of_match_ptr(egalax_ts_dt_ids),
	},
	.id_table	= egalax_ts_id,
	.probe		= egalax_ts_probe,
	.remove		= egalax_ts_remove,
};

module_i2c_driver(egalax_ts_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Touchscreen driver for EETI eGalax touch controller");
MODULE_LICENSE("GPL");
