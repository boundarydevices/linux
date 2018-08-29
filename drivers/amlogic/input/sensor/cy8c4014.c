/*
 * drivers/amlogic/input/sensor/cy8c4014.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/sched.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>


#define DRV_VERSION		"1.0.0.0"
#define CY8C4014_DEV_NAME	"cy8c4014"
#define CY8C4014_SCHE_DELAY	200
#define CY8C4014_ADDR	0x8

struct cy8c4014_data {
	struct delayed_work work;
	struct input_dev *input_dev;
	struct i2c_client *i2c_client;
	int  delay;
	int  enable;
	struct mutex mutex;
};
struct cy8c4014_data *cy8c4014_data;
static struct i2c_driver cy8c4014_driver;

static int cy8c4014_reset(struct i2c_client *client)
{
	return 0;
}

int cy8c4014_read(struct i2c_client *dev, int add, uint8_t *val)
{
	int ret;
	uint8_t buf[2] = {};
	struct i2c_msg msg[] = {
		{
			.addr  = CY8C4014_ADDR,
			.flags = 0,
			.len   = sizeof(buf),
			.buf   = buf,
		},
		{
			.addr  = CY8C4014_ADDR,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = val,
		}
	};


	buf[0] = add & 0xff;
	buf[1] = (add >> 8) & 0x0f;
	ret = i2c_transfer(dev->adapter, msg, 2);
	if (ret < 0) {
		pr_info("%s: i2c transfer failed, ret:%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void cy8c4014_schedwork(struct work_struct *work)
{
	unsigned long delay = msecs_to_jiffies(cy8c4014_data->delay);
	struct input_dev *input_dev = cy8c4014_data->input_dev;
	uint8_t x = 0;
	/*uint8_t y = 0;*/

	cy8c4014_read(cy8c4014_data->i2c_client, 0x00, &x);
	/*cy8c4014_read(cy8c4014_data->i2c_client,0x01,&y);*/
	if (x != 0xFF)
		input_report_abs(input_dev, ABS_X, x);
	/*if(y != 0xFF)*/
		/*input_report_abs(input_dev, ABS_Y, y);*/
	input_sync(input_dev);
	schedule_delayed_work(&cy8c4014_data->work, delay);
}

static ssize_t cy8c4014_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "cy8c4014 sensor Auto Enable = %d\n",
			cy8c4014_data->enable);

	return ret;
}

static int cy8c4014_sensor_enable(void)
{
	int ret = 0;

	unsigned long delay = msecs_to_jiffies(cy8c4014_data->delay);

	if (cy8c4014_data->enable == 1)
		return ret;

	mutex_lock(&cy8c4014_data->mutex);

	schedule_delayed_work(&cy8c4014_data->work, delay);
	cy8c4014_data->enable = 1;

	mutex_unlock(&cy8c4014_data->mutex);

	return ret;
}

static int cy8c4014_sensor_disable(void)
{
	int ret = 0;

	if (cy8c4014_data->enable == 0)
		return ret;

	mutex_lock(&cy8c4014_data->mutex);

	cancel_delayed_work(&cy8c4014_data->work);
	cy8c4014_data->enable = 0;

	mutex_unlock(&cy8c4014_data->mutex);

	return ret;
}
static ssize_t cy8c4014_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ls_auto, ret;

	ls_auto = -1;
	ret = kstrtoint(buf, 10, &ls_auto);
	if (ret == -1)
		return -EINVAL;
	if (ls_auto != 0 && ls_auto != 1)
		return -EINVAL;

	if (ls_auto)
		cy8c4014_sensor_enable();
	else
		cy8c4014_sensor_disable();


	return count;
}

static ssize_t cy8c4014_poll_delay_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "cy8c4014 sensor Poll Delay = %d ms\n",
			cy8c4014_data->delay);

	return ret;
}

static ssize_t cy8c4014_poll_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int new_delay, ret;

	ret = kstrtoint(buf, 10, &new_delay);
	if (ret == -1)
		return -EINVAL;
	pr_info("new delay = %d ms, old delay = %d ms\n",  new_delay,
			cy8c4014_data->delay);

	cy8c4014_data->delay = new_delay;

	if (cy8c4014_data->enable) {
		cy8c4014_sensor_disable();
		cy8c4014_sensor_enable();
	}

	return count;
}

static struct device_attribute dev_attr_cy8c4014_enable =
__ATTR(enable, 0664, cy8c4014_enable_show,
	cy8c4014_enable_store);

static struct device_attribute dev_attr_cy8c4014_delay =
__ATTR(delay, 0664, cy8c4014_poll_delay_show,
	cy8c4014_poll_delay_store);

static struct attribute *sensor_sysfs_attrs[] = {
&dev_attr_cy8c4014_enable.attr,
&dev_attr_cy8c4014_delay.attr,
NULL
};

static struct attribute_group cy8c4014_attribute_group = {
.attrs = sensor_sysfs_attrs,
};

static int cy8c4014_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct input_dev *idev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	pr_info(" start cy8c4014 probe !!\n");

	if (!i2c_check_functionality(adapter,
		I2C_FUNC_SMBUS_WRITE_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA)){
		ret = -EIO;
		return ret;
	}

	/* data memory allocation */
	cy8c4014_data = kzalloc(sizeof(struct cy8c4014_data), GFP_KERNEL);
	if (cy8c4014_data == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	cy8c4014_data->i2c_client = client;

	i2c_set_clientdata(client, cy8c4014_data);

	cy8c4014_reset(cy8c4014_data->i2c_client);

	INIT_DELAYED_WORK(&cy8c4014_data->work, cy8c4014_schedwork);
	cy8c4014_data->delay = CY8C4014_SCHE_DELAY;

	idev = input_allocate_device();
	if (!idev) {
		pr_alert("%s: cy8c4014 allocate input device failed.\n",
				__func__);
		goto kfree_exit;
	}

	idev->name = CY8C4014_DEV_NAME;
	idev->id.bustype = BUS_I2C;
	input_set_capability(idev, EV_ABS, ABS_X);
	input_set_capability(idev, EV_ABS, ABS_Y);
	input_set_abs_params(idev, ABS_X, 0, 255, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 255, 0, 0);
	cy8c4014_data->input_dev = idev;
	input_set_drvdata(idev, cy8c4014_data);

	ret = input_register_device(idev);
	if (ret < 0) {
		input_free_device(idev);
		goto kfree_exit;
	}

	mutex_init(&cy8c4014_data->mutex);
	/* register the attributes */
	ret = sysfs_create_group(&idev->dev.kobj, &cy8c4014_attribute_group);
	if (ret)
		goto unregister_exit;

	schedule_delayed_work(&cy8c4014_data->work, CY8C4014_SCHE_DELAY);
	cy8c4014_data->enable = 1;
	return ret;

unregister_exit:
	input_unregister_device(idev);
	input_free_device(idev);

kfree_exit:
	kfree(cy8c4014_data);
	return ret;
}

static int cy8c4014_remove(struct i2c_client *client)
{
	i2c_unregister_device(cy8c4014_data->i2c_client);
	cancel_delayed_work(&cy8c4014_data->work);
	kfree(cy8c4014_data);
	return 0;
}
static const struct i2c_device_id cy8c4014_id[] = {
	{ CY8C4014_DEV_NAME, 0 },
	{ }
};

static struct i2c_driver cy8c4014_driver = {
	.driver = {
		.name	= CY8C4014_DEV_NAME,
		.owner  = THIS_MODULE,
	},
	.probe = cy8c4014_probe,
	.remove = cy8c4014_remove,
	.id_table = cy8c4014_id,
};

static int __init cy8c4014_init(void)
{
	i2c_add_driver(&cy8c4014_driver);
	return 0;
}

static void __exit cy8c4014_exit(void)
{
	i2c_del_driver(&cy8c4014_driver);
}

module_init(cy8c4014_init);
module_exit(cy8c4014_exit);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("CYPRESS CY8C4014 driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

