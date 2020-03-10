/*
 * STMicroelectronics lsm6ds3 i2c driver
 *
 * Copyright 2014-2016 STMicroelectronics Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/iio/iio.h>

#include "st_lsm6ds3.h"

static int st_lsm6ds3_i2c_read(struct lsm6ds3_data *cdata,
				u8 reg_addr, int len, u8 *data, bool b_lock)
{
	int err = 0;
	struct i2c_msg msg[2];
	struct i2c_client *client = to_i2c_client(cdata->dev);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	if (b_lock) {
		mutex_lock(&cdata->bank_registers_lock);
		err = i2c_transfer(client->adapter, msg, 2);
		mutex_unlock(&cdata->bank_registers_lock);
	} else
		err = i2c_transfer(client->adapter, msg, 2);

	return err;
}

static int st_lsm6ds3_i2c_write(struct lsm6ds3_data *cdata,
				u8 reg_addr, int len, u8 *data, bool b_lock)
{
	int err = 0;
	u8 send[len + 1];
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(cdata->dev);

	send[0] = reg_addr;
	memcpy(&send[1], data, len * sizeof(u8));
	len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = send;

	if (b_lock) {
		mutex_lock(&cdata->bank_registers_lock);
		err = i2c_transfer(client->adapter, &msg, 1);
		mutex_unlock(&cdata->bank_registers_lock);
	} else
		err = i2c_transfer(client->adapter, &msg, 1);

	return err;
}

static const struct st_lsm6ds3_transfer_function st_lsm6ds3_tf_i2c = {
	.write = st_lsm6ds3_i2c_write,
	.read = st_lsm6ds3_i2c_read,
};

static int st_lsm6ds3_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err;
	struct lsm6ds3_data *cdata;

	cdata = kmalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->dev = &client->dev;
	cdata->name = client->name;
	i2c_set_clientdata(client, cdata);

	cdata->spi_connection = false;
	cdata->tf = &st_lsm6ds3_tf_i2c;

	err = st_lsm6ds3_common_probe(cdata, client->irq);
	if (err < 0)
		goto free_data;

	return 0;

free_data:
	kfree(cdata);
	return err;
}

static int st_lsm6ds3_i2c_remove(struct i2c_client *client)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(client);

	st_lsm6ds3_common_remove(cdata, client->irq);
	kfree(cdata);

	return 0;
}

#ifdef CONFIG_PM
static int st_lsm6ds3_suspend(struct device *dev)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return st_lsm6ds3_common_suspend(cdata);
}

static int st_lsm6ds3_resume(struct device *dev)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return st_lsm6ds3_common_resume(cdata);
}

static const struct dev_pm_ops st_lsm6ds3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_lsm6ds3_suspend, st_lsm6ds3_resume)
};

#define ST_LSM6DS3_PM_OPS		(&st_lsm6ds3_pm_ops)
#else /* CONFIG_PM */
#define ST_LSM6DS3_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id st_lsm6ds3_id_table[] = {
	{ LSM6DS3_DEV_NAME },
	{ LSM6DS33_DEV_NAME },
	{ },
};
MODULE_DEVICE_TABLE(i2c, st_lsm6ds3_id_table);

#ifdef CONFIG_OF
static const struct of_device_id lsm6ds3_of_match[] = {
	{
		.compatible = "st,lsm6ds3",
		.data = LSM6DS3_DEV_NAME,
	},
	{
		.compatible = "st,lsm6ds33",
		.data = LSM6DS33_DEV_NAME,
	},
	{}
};
MODULE_DEVICE_TABLE(of, lsm6ds3_of_match);
#else /* CONFIG_OF */
#define lsm6ds3_of_match		NULL
#endif /* CONFIG_OF */

static struct i2c_driver st_lsm6ds3_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "st-lsm6ds3-i2c",
		.pm = ST_LSM6DS3_PM_OPS,
		.of_match_table = of_match_ptr(lsm6ds3_of_match),
	},
	.probe = st_lsm6ds3_i2c_probe,
	.remove = st_lsm6ds3_i2c_remove,
	.id_table = st_lsm6ds3_id_table,
};
module_i2c_driver(st_lsm6ds3_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics lsm6ds3 i2c driver");
MODULE_LICENSE("GPL v2");
