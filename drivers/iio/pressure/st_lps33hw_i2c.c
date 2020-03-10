/*
 * STMicroelectronics lps33hw i2c driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/i2c.h>

#include "st_lps33hw.h"

static int st_lps33hw_i2c_read(struct device *dev, u8 addr, int len, u8 *data)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	return i2c_transfer(client->adapter, msg, 2);
}

static int st_lps33hw_i2c_write(struct device *dev, u8 addr, int len, u8 *data)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msg;
	u8 send[len + 1];

	send[0] = addr;
	memcpy(&send[1], data, len * sizeof(u8));
	len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = send;

	return i2c_transfer(client->adapter, &msg, 1);
}

static const struct st_lps33hw_transfer_function st_lps33hw_tf_i2c = {
	.write = st_lps33hw_i2c_write,
	.read = st_lps33hw_i2c_read,
};

static int st_lps33hw_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	return st_lps33hw_common_probe(&client->dev, client->irq, client->name,
				       &st_lps33hw_tf_i2c);
}

static const struct i2c_device_id st_lps33hw_ids[] = {
	{ "lps33hw" },
	{ "lps35hw" },
	{}
};
MODULE_DEVICE_TABLE(i2c, st_lps33hw_ids);

static const struct of_device_id st_lps33hw_id_table[] = {
	{ .compatible = "st,lps33hw" },
	{ .compatible = "st,lps35hw" },
	{},
};
MODULE_DEVICE_TABLE(of, st_lps33hw_id_table);

static struct i2c_driver st_lps33hw_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "st_lps33hw_i2c",
		   .of_match_table = of_match_ptr(st_lps33hw_id_table),
	},
	.probe = st_lps33hw_i2c_probe,
	.id_table = st_lps33hw_ids,
};
module_i2c_driver(st_lps33hw_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics lps33hw i2c driver");
MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_LICENSE("GPL v2");
