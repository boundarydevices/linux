/*
 * iqs5xx.c - Touchscreen driver for Azoteq IQS5xx controller
 * using firmware B000.
 *
 * Copyright (C) 2015 Boundary Devices Inc.
 * Copyright (C) 2013 Azoteq (Pty) Ltd
 * Author: Alwino van der Merwe <alwino.vandermerwe@azoteq.com>
 *
 * Based on mcs5000_ts.c and azoteqiqs440_ts.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include "iqs5xx.h"
#include "iqs5xx_init.h"

/* Device specific resolutions */
#define IQS550_MAX_X		3584
#define IQS550_MAX_Y		2304

/* 35 bytes to read to get all the information for 5 fingers */
#define IQS5XX_MAX_TOUCHES	(5)
#define IQS5XX_TOUCH_SIZE	(7)
#define IQS5XX_BLOCK_SIZE	(IQS5XX_TOUCH_SIZE * IQS5XX_MAX_TOUCHES)

/* Structure to keep the info of the reported touches */
struct iqs5xx_touch_info iqs5xx_touch[IQS5XX_MAX_TOUCHES];

/* Each client has this additional data */
struct iqs5xx_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int reset_gpio;
};

/* Since all the Linux SMBUS/I2C functions consider that the command
 * is a 8-bit value, we can't use them. Instead we will base our xfer
 * function on the i2c_smbus_xfer_emulated with a different handling
 * for the 16-bit command value.
 */
static s32 iqs5xx_xfer_block(struct i2c_client *client, u16 command,
			     char read_write, u8 length, u8 *buffer)
{
	/* So we need to generate a series of msgs. In the case of writing, we
	  need to use only one message; when reading, we need two. We initialize
	  most things with sane defaults, to keep the code below somewhat
	  simpler. */
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+3];
	unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX+2];
	int num = read_write == I2C_SMBUS_READ ? 2 : 1;
	int i;
	int status;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = msgbuf0,
		}, {
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = 0,
			.buf = msgbuf1,
		},
	};

	/* 16-bit command value endianness is explained in
	 * iqs5xx-B000_trackpad_datasheet.pdf
	 */
	msgbuf0[0] = (u8)(command >> 8);
	msgbuf0[1] = (u8)(command & 0xFF);

	if (read_write == I2C_SMBUS_READ) {
		msg[1].len = length;
	} else {
		msg[0].len = length + 2;
		if (msg[0].len > I2C_SMBUS_BLOCK_MAX + 2) {
			dev_err(&client->adapter->dev,
				"Invalid block write size %d\n",
				buffer[0]);
			return -EINVAL;
		}
		for (i = 2; i < msg[0].len; i++)
			msgbuf0[i] = buffer[i - 2];
	}

	status = i2c_transfer(client->adapter, msg, num);
	if (status < 0)
		return status;

	if (read_write == I2C_SMBUS_READ)
		for (i = 0; i < length; i++)
			buffer[i] = msgbuf1[i];

	return 0;
}

static void iqs5xx_end_comm(struct i2c_client *client)
{
	u8 buffer[1] = {0};

	iqs5xx_xfer_block(client, END_COMMUNICATION, I2C_SMBUS_WRITE,
			  ARRAY_SIZE(buffer), buffer);
}

static s32 iqs5xx_write_block(struct i2c_client *client, u16 command,
			      u8 length, u8 *buffer)
{
	return iqs5xx_xfer_block(client, command, I2C_SMBUS_WRITE,
				 length, buffer);
}

static s32 iqs5xx_read_block(struct i2c_client *client, u16 command,
			     u8 length, u8 *buffer)
{
	return iqs5xx_xfer_block(client, command, I2C_SMBUS_READ,
				 length, buffer);
}

static void iqs5xx_setup(struct i2c_client *client)
{
	u8 system_ctrl[] = {
		SYSTEM_CONTROL_0_VAL | ACK_RESET ,
		SYSTEM_CONTROL_1_VAL
	};
	u8 system_config[] = {
		SYSTEM_CONFIG_0_VAL | SETUP_COMP | ALP_RE_ATI | RE_ATI,
		SYSTEM_CONFIG_1_VAL | PROX_EVENT | TOUCH_EVENT | SNAP_EVENT |
		ALP_PROX_EVENT | RE_ATI_EVENT | TP_EVENT | GESTURE_EVENT |
		EVENT_MODE
	};

	iqs5xx_write_block(client, SYSTEM_CONTROL_0, ARRAY_SIZE(system_ctrl),
			   system_ctrl);
	iqs5xx_write_block(client, SYSTEM_CONFIG_0, ARRAY_SIZE(system_config),
			   system_config);
}

/* Helper macro to understand which offset we look at */
#define IDX_INFO(x) (x - GESTURE_EVENTS_0)
#define IDX_TOUCH(x) (x - XY_DATA)

/* Interrupt event fires on the rising edge of the RDY signal from the IQS5xx */
static irqreturn_t iqs5xx_ts_interrupt(int irq, void *dev_id)
{
	struct iqs5xx_ts_data *data = dev_id;
	struct i2c_client *client = data->client;
	u8 buffer[IQS5XX_BLOCK_SIZE];
	int err;
	u8 no_touches = 0;
	u8 i;

	/* Read only status + nb of finger, then read the rest */
	err = iqs5xx_read_block(client, GESTURE_EVENTS_0, 5 , buffer);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		goto out;
	}

	/* Check if a reset occurred */
	if (buffer[IDX_INFO(SYSTEM_INFO_0)] & 0x80) {
		dev_info(&client->dev, "%s, reset detected\n", __func__);
		iqs5xx_setup(client);
		goto out;
	}

	/* Report the number of fingers on the screen */
	no_touches = buffer[IDX_INFO(NUM_OF_FINGERS)];
	dev_dbg(&client->dev, "%s, # of fingers: %d (events %x%x)\n", __func__,
		 no_touches, buffer[IDX_INFO(GESTURE_EVENTS_0)],
		 buffer[IDX_INFO(GESTURE_EVENTS_1)]);

	/* Report multiple touches with the IQS5xx */
	if (no_touches == 0) {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		goto sync;
	}

	/* Read the values for the nb of fingers found*/
	err = iqs5xx_read_block(client, XY_DATA, no_touches *
				IQS5XX_TOUCH_SIZE, buffer);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		goto out;
	}

	/* Process touch events */
	for (i = 0; i < no_touches; i++) {
		struct iqs5xx_touch_info *touch = &iqs5xx_touch[i];

		touch->x_pos = (u16)((buffer[(i * IQS5XX_TOUCH_SIZE) +
				      IDX_TOUCH(ABS_X_HIGH)] << 8) +
				     (buffer[(i * IQS5XX_TOUCH_SIZE) +
				      IDX_TOUCH(ABS_X_LOW)]));
		touch->y_pos = (u16)((buffer[(i * IQS5XX_TOUCH_SIZE) +
				      IDX_TOUCH(ABS_Y_HIGH)] << 8) +
				     (buffer[(i * IQS5XX_TOUCH_SIZE) +
				      IDX_TOUCH(ABS_Y_LOW)]));
		touch->strength = (u16)((buffer[(i * IQS5XX_TOUCH_SIZE) +
					 IDX_TOUCH(T_STRENTGH_HIGH)] << 8) +
					(buffer[(i * IQS5XX_TOUCH_SIZE) +
					 IDX_TOUCH(T_STRENTGH_LOW)]));

		/* Implement small filter */
		if ((abs(touch->prev_x - touch->x_pos) < 15) &&
		    (abs(touch->prev_y - touch->y_pos) < 15)) {
			touch->x_pos = touch->prev_x;
			touch->y_pos = touch->prev_y;
		}

		/* Save the current X Y positions */
		touch->prev_x = touch->x_pos;
		touch->prev_y = touch->y_pos;

		/* Report the key as pressed */
		/*input_report_abs(data->input_dev, ABS_PRESSURE,
				   touch->strength);*/
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				 IQS550_MAX_X - touch->y_pos);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				 touch->x_pos);
		input_mt_sync(data->input_dev);
	}
sync:
	input_sync(data->input_dev);
out:
	iqs5xx_end_comm(client);

	return IRQ_HANDLED;
}

static int iqs5xx_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct iqs5xx_ts_data *data;
	struct input_dev *input_dev;
	struct device_node *np = client->dev.of_node;
	int ret;

	dev_dbg(&client->dev, "%s\n", __func__);

	/* Allocate memory */
	data = kzalloc(sizeof(struct iqs5xx_ts_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	/* Read info from device tree */
	if (!np) {
		ret = -ENODEV;
		goto err_free_mem;
	}

	data->reset_gpio = of_get_named_gpio(np, "wakeup-gpios", 0);
	if (!gpio_is_valid(data->reset_gpio)) {
		ret = -ENODEV;
		goto err_free_mem;
	}

	ret = gpio_request(data->reset_gpio, "iqs5xx_rst");
	if (ret < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n",
			ret);
		goto err_free_mem;
	}

	/* Hold the NRST low until master setup has been done */
	gpio_direction_output(data->reset_gpio, 0);

	/* Save the stuctures to be used in the driver */
	data->client = client;
	data->input_dev = input_dev;

	input_dev->name = "Azoteq IQS5xx Touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	/* Give settings to Android such as the minimum and maximum
	 * X and Y positions that will be reported
	 */
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	/* input_set_abs_params(input_dev, ABS_PRESSURE, 0, 65536, 0, 0); */
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
			     IQS5XX_MAX_TOUCHES - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0,
			     IQS550_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0,
			     IQS550_MAX_Y, 0, 0);

	/* Set driver data */
	input_set_drvdata(input_dev, data);

	/* Request the interrupt on a rising trigger and
	 * only one trigger per rising edge
	 */
	ret = request_threaded_irq(client->irq, NULL, iqs5xx_ts_interrupt,
				   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				   DEVICE_NAME, data);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_mem;
	}

	/* Register the device */
	ret = input_register_device(data->input_dev);
	if (ret < 0)
		goto err_free_irq;

	/* Now that everything is setup, release the reset pin */
	gpio_set_value(data->reset_gpio, 1);

	/* Call the pysical init for first communication */
	i2c_set_clientdata(client, data);

	dev_info(&client->dev, "%s: complete\n", __func__);

	return 0;

err_free_irq:
	free_irq(client->irq, data);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
	return ret;
}

static int iqs5xx_ts_remove(struct i2c_client *client)
{
	struct iqs5xx_ts_data *data = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s\n", __func__);
	gpio_free(data->reset_gpio);
	free_irq(client->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static int iqs5xx_ts_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int iqs5xx_ts_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

/* Standard stucture with the device id for identification */
static const struct i2c_device_id iqs5xx_ts_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, iqs5xx_ts_id);

static struct of_device_id iqs5xx_ts_dt_ids[] = {
	{ .compatible = "azoteq,iqs5xx" },
	{ /* sentinel */ }
};

static SIMPLE_DEV_PM_OPS(iqs5xx_ts_pm_ops, iqs5xx_ts_suspend, iqs5xx_ts_resume);

/* Standard stucture containing the driver information and procedures */
static struct i2c_driver iqs5xx_ts_driver = {
	.probe		= iqs5xx_ts_probe,
	.remove		= iqs5xx_ts_remove,
	.driver		= {
		.name	= DEVICE_NAME,
		.of_match_table	= of_match_ptr(iqs5xx_ts_dt_ids),
		.pm	= &iqs5xx_ts_pm_ops,
	},
	.id_table	= iqs5xx_ts_id,
};

static int __init iqs5xx_ts_init(void)
{
	pr_debug("%s\n", __func__);

	return i2c_add_driver(&iqs5xx_ts_driver);
}

/* Remove the driver */
static void __exit iqs5xx_ts_exit(void)
{
	pr_debug("%s\n", __func__);

	i2c_del_driver(&iqs5xx_ts_driver);
}

module_init(iqs5xx_ts_init);
module_exit(iqs5xx_ts_exit);

/* Module information */
MODULE_AUTHOR("Alwino van der Merwe <alwino.vandermerwe@azoteq.com>");
MODULE_DESCRIPTION("Touchscreen driver for Azoteq IQS5xx controller");
MODULE_LICENSE("GPL");
