/*
 * ads1000.c - driver for the ads1000 12-bit A/D converter
 *
 * Copyright (C) Boundary Devices
 *
 * Inspired by ADS1015 driver by Dirk Eibach
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c/ads1000.h>
#include <linux/i2c.h>

/* sysfs callback function */
static ssize_t show
	(struct device *dev,
	 struct device_attribute *da,
	 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ads1000_platform_data *pdata = client->dev.platform_data;
	s32 data = i2c_smbus_read_word_data(client, 0);
	if (0 > data) {
		pr_err("%s: error %d reading reg\n",
		       __FILE__, data);
	} else {
		data = swab16(data);
	}
	if (pdata && pdata->denominator)
		data = (data * pdata->numerator)/pdata->denominator;
	else
		pr_err("%s: invalid platform data\n", __FILE__);
	return (data < 0) ? data : sprintf(buf, "%d\n", data);
}

static const struct sensor_device_attribute ads1000_attr =
	SENSOR_ATTR(mv, S_IRUGO, show, NULL, 0);

/*
 * Driver interface
 */
static int ads1000_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &ads1000_attr.dev_attr);
	return 0;
}

static int ads1000_probe
	(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	struct ads1000_platform_data *pdata = client->dev.platform_data;
	int err = device_create_file(&client->dev, &ads1000_attr.dev_attr);
	if (0 > err) {
		dev_err(&client->dev, "%s: error %d creating %s\n",
			__func__, err, ads1000_attr.dev_attr.attr.name);
	}
	if ((0 == err) && (0 == pdata)) {
		struct device_node *np = client->dev.of_node;
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		pdata->numerator = 1;
		pdata->denominator = 1;
		client->dev.platform_data = pdata;
		if (np) {
			if (of_property_read_u32(np, "numerator",
						 &pdata->numerator)){
				dev_err(&client->dev,
					"%s: default to numerator == 1\n",
					__func__);
				pdata->numerator = 1;
			}
			if (of_property_read_u32(np, "denominator",
						 &pdata->denominator)) {
				dev_err(&client->dev,
					"%s: default to denominator == 1\n",
					__func__);
				pdata->denominator = 1;
			}
		} else
			dev_err(&client->dev,
				"%s: no device tree/no platform data\n",
				__func__);
	}
	return err;
}

static const struct i2c_device_id ads1000_id[] = {
	{ "ads1000", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ads1000_id);

static struct i2c_driver ads1000_driver = {
	.driver = {
		.name = "ads1000",
	},
	.probe = ads1000_probe,
	.remove = ads1000_remove,
	.id_table = ads1000_id,
};

static int __init ads1000_init(void)
{
	return i2c_add_driver(&ads1000_driver);
}

static void __exit ads1000_exit(void)
{
	i2c_del_driver(&ads1000_driver);
}

MODULE_AUTHOR("Eric Nelson <eric.nelson@boundarydevices.com>");
MODULE_DESCRIPTION("ADS1000 driver");
MODULE_LICENSE("GPL");

module_init(ads1000_init);
module_exit(ads1000_exit);
