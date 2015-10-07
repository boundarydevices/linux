/*
 * iqs5xx.c - Touchscreen driver for Azoteq IQS5xx controller
 *
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

// DEvice specific resolutions
#define IQS550_MAX_X		3584
#define IQS550_MAX_Y		2304

// 36 bytes to read form the XY Status register - all the necessary info resides here
#define IQS5XX_BLOCK_SIZE	36

// first time interrupt was triggered
unsigned char firstFlag = 0;

// structure to keep the info of the reported touches
struct IQS5xx_fingers_structure IQS5xx_touches [5];

/* Each client has this additional data */
struct iqs5xx_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct iqs5xx_ts_platform_data *platform_data;
	int reset_gpio;
};

/* The IQS5xx specific setup is done here */

/* Platform data for the Azoteq IQS5xx touchscreen driver */
struct iqs5xx_ts_platform_data {
	void (*cfg_pin)(void);
	int x_size;
	int y_size;
};

/* Interrupt event fires on the rising edge of the RDY signal from the Azoteq IQS5xx */
static irqreturn_t iqs5xx_ts_interrupt(int irq, void *dev_id)
{
	struct iqs5xx_ts_data *data = dev_id;
	struct i2c_client *client = data->client;
	u8 buffer[40];
	int err;
	u8 noOfTouches = 0;
	u8 i;

	err = i2c_smbus_read_i2c_block_data(client, XY_DATA, IQS5XX_BLOCK_SIZE, buffer);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		goto out;
	}

	// report the number of fingers on the screen
	noOfTouches = buffer[0] & 0x07;

	/* Report multiple touches with the IQS5xx */
	if (noOfTouches == 0) {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_key(data->input_dev, BTN_TOUCH, 0);
	} else if (noOfTouches == 1) {
		/* If only one touch is present, handle it as a single touch
		 * Multi touches are reported in a different manner
		 */
		IQS5xx_touches[0].ID = buffer[1];	// ID of this touch
		IQS5xx_touches[0].XPos = (u16)((buffer[2] << 8) + (buffer[3]));	// XPos
		IQS5xx_touches[0].YPos = (u16)((buffer[4] << 8) + (buffer[5]));
		IQS5xx_touches[0].touchStrength = (u16)((buffer[6] << 8) + (buffer[7]));

		// implement small filter
		if ((abs(IQS5xx_touches[0].prev_x - IQS5xx_touches[0].XPos) < 20) &&
		    (abs(IQS5xx_touches[0].prev_y - IQS5xx_touches[0].YPos) < 20)) {
			IQS5xx_touches[0].XPos = IQS5xx_touches[0].prev_x;
			IQS5xx_touches[0].YPos = IQS5xx_touches[0].prev_y;
		}

		/* Save the current X Y positions for use in the next communication window */
		IQS5xx_touches[0].prev_x = IQS5xx_touches[0].XPos;
		IQS5xx_touches[0].prev_y = IQS5xx_touches[0].YPos;

		// report the key as pressed, if it is not a proximity that is detected
		if (IQS5xx_touches[0].ID < 50) {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, IQS5xx_touches[0].ID);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, IQS550_MAX_X - IQS5xx_touches[0].YPos);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, IQS5xx_touches[0].XPos);
			input_mt_sync(data->input_dev);
		}
	} else {
		// Multi Touch - More than 1 touch is present
		for (i = 0; i < noOfTouches; i++) {
			IQS5xx_touches[i].ID = buffer[(i * 7) + 1];	// ID of this touch
			IQS5xx_touches[i].XPos = (u16)((buffer[(i * 7) + 2] << 8) + (buffer[(i * 7) + 3]));	// XPos
			IQS5xx_touches[i].YPos = (u16)((buffer[(i * 7) + 4] << 8) + (buffer[(i * 7) +5]));
			IQS5xx_touches[i].touchStrength = (u16)((buffer[(i * 7) + 6] << 8) + (buffer[(i * 7) + 7]));

			// implement small filter
			if ((abs(IQS5xx_touches[i].prev_x - IQS5xx_touches[i].XPos) < 15) &&
			    (abs(IQS5xx_touches[i].prev_y - IQS5xx_touches[i].YPos) < 15)) {
				IQS5xx_touches[i].XPos = IQS5xx_touches[i].prev_x;
				IQS5xx_touches[i].YPos = IQS5xx_touches[i].prev_y;
			}

			/* Save the current X Y positions for use in the next communication window */
			IQS5xx_touches[i].prev_x = IQS5xx_touches[i].XPos;
			IQS5xx_touches[i].prev_y = IQS5xx_touches[i].YPos;

			if (IQS5xx_touches[i].ID < 50) {
				// report the key as pressed
				//input_report_abs(data->input_dev, ABS_PRESSURE, IQS5xx_touches[i].touchStrength);
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, IQS5xx_touches[i].ID);
				input_report_key(data->input_dev, BTN_TOUCH, 1);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X, IQS550_MAX_X - IQS5xx_touches[i].YPos);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y, IQS5xx_touches[i].XPos);
				input_mt_sync(data->input_dev);
			}
		}
	}
	input_sync(data->input_dev);
out:
	return IRQ_HANDLED;
}

/* The probe function is called when Android is looking
 * for the IQS440 on the I2C bus (I2C-2)
 */
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

	/* datasheet says to hold the NRST low until master setup has been done */
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
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_TOUCHES, 0, 0);
	//input_set_abs_params(input_dev, ABS_PRESSURE, 0, 65536, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, IQS550_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, IQS550_MAX_Y, 0, 0);

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
	.driver 	= {
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
