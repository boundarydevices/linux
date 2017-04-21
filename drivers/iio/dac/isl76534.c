/*
 *  isl76534.c - Support for Intersil isl76534
 *
 *  Copyright (C) 2017 Boundary Devices
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/regulator/consumer.h>

enum isl76534_device_ids {
	ID_ISL76534,
};

struct isl76534_state {
	struct i2c_client	*client;
	struct regulator	*vref_reg;
	unsigned 		vref_mv;
	unsigned		last_reg;
	struct mutex		read_lock;
	struct mutex		write_lock;
	unsigned char reg_read[32] ____cacheline_aligned;
	unsigned char reg_write[32] ____cacheline_aligned;
};

static int isl76534_read_reg(struct isl76534_state *isl, unsigned reg)
{
	struct i2c_msg msgs[2];
	unsigned char *p = &isl->reg_read[0];
	int ret;
	int val;

	if (reg > 15)
		return -EINVAL;

	msgs[0].addr = isl->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = p;

	msgs[1].addr = isl->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = p;

	mutex_lock(&isl->read_lock);

	p[0] = reg;
	ret = i2c_transfer(isl->client->adapter, msgs, 2);

	val = (p[0] << 8) | p[1];
	mutex_unlock(&isl->read_lock);

	if (ret < 0) {
		pr_err("%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, val);
	return val;
}

static int isl76534_write_reg(struct isl76534_state *isl, unsigned reg, unsigned value)
{
	struct i2c_msg msgs[1];
	unsigned char *p = &isl->reg_write[0];
	unsigned val = value & ~(1 << 14);
	int ret;

	if ((reg > 15) || (val > 0x3ff))
		return -EINVAL;

	msgs[0].addr = isl->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 3;
	msgs[0].buf = p;

	mutex_lock(&isl->write_lock);

	p[0] = reg;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)value;

	ret = i2c_transfer(isl->client->adapter, msgs, 1);
	mutex_unlock(&isl->write_lock);

	if (ret < 0) {
		pr_err("%s:reg=%x val=%x ret=%d\n", __func__, reg, value, ret);
		return ret;
	}
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, value);
	return 0;
}

static int isl76534_read_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan,
		int *val,
		int *val2,
		long m)
{
	struct isl76534_state *isl = iio_priv(iio);

	switch (m) {
	case IIO_CHAN_INFO_SCALE:
		/* Corresponds to Vref / 2^(bits) */
		*val = isl->vref_mv;
		*val2 = 10;
		return IIO_VAL_FRACTIONAL_LOG2;
	default:
		break;
	}
	return -EINVAL;
}

static int isl76534_write_raw(struct iio_dev *iio,
	struct iio_chan_spec const *chan, int val, int val2, long mask)
{
	struct isl76534_state *isl = iio_priv(iio);
	int ret;
	int ch = chan->channel;


	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = isl76534_write_reg(isl, ch + 1, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct iio_info isl76534_info = {
	.read_raw = isl76534_read_raw,
	.write_raw = isl76534_write_raw,
	.driver_module = THIS_MODULE,
};

#define ISL76534_CHANNEL(chan) {			\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.output = 1,					\
	.channel = (chan),				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	\
	BIT(IIO_CHAN_INFO_SCALE),			\
}

static const struct iio_chan_spec isl76534_channels[] = {
	ISL76534_CHANNEL(0),
	ISL76534_CHANNEL(1),
	ISL76534_CHANNEL(2),
	ISL76534_CHANNEL(3),
	ISL76534_CHANNEL(4),
	ISL76534_CHANNEL(5),
	ISL76534_CHANNEL(6),
	ISL76534_CHANNEL(7),
	ISL76534_CHANNEL(8),
	ISL76534_CHANNEL(9),
	ISL76534_CHANNEL(10),
	ISL76534_CHANNEL(11),
	ISL76534_CHANNEL(12),
	ISL76534_CHANNEL(13),
	ISL76534_CHANNEL(14),
};

static ssize_t show_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct isl76534_state *isl = iio_priv(iio);
	s32 rval;

	if (isl->last_reg > 15) {
		int reg;
		int cnt = 0;
		int n;

		for (reg = 0; reg <= 15; reg++) {
			rval = isl76534_read_reg(isl, reg);
			if (rval >= 0) {
				n = sprintf(buf, "0x%02x:0x%x\n", reg, rval);
				if (n > 0) {
					cnt += n;
					buf += n;
				}
			}
		}
		return cnt;
	}
	rval = isl76534_read_reg(isl, isl->last_reg);
	if (rval < 0)
		return rval;
	return sprintf(buf, "0x%x\n", rval);
}

static ssize_t store_reg(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct isl76534_state *isl = iio_priv(iio);
	unsigned reg, value;
	int numscanned = sscanf(buf,"%x %x\n", &reg, &value);

	if (reg > 15) {
		if ((numscanned != 1) || (reg != ~0)) {
			dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
				__func__);
			return -EINVAL;
		}
	}

	if (numscanned == 2) {
		int rval = isl76534_write_reg(isl, reg, value);
		if (rval) {
			dev_err(dev, "%s: error %d setting reg 0x%04x to 0x%02x\n",
				__func__, rval, reg, value);
		}
	} else if (numscanned == 1) {
		isl->last_reg = reg;
	} else {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
	}
	return count;
}

static DEVICE_ATTR(isl76534_reg, S_IRUGO|S_IWUSR|S_IWGRP, show_reg, store_reg);

static int isl76534_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct isl76534_state *isl;
	struct iio_dev *iio;
	int ret;

	iio = devm_iio_device_alloc(&client->dev, sizeof(*isl));
	if (!iio)
		return -ENOMEM;
	isl = iio_priv(iio);
	i2c_set_clientdata(client, iio);
	isl->client = client;
	mutex_init(&isl->read_lock);
	mutex_init(&isl->write_lock);

	/* establish that the iio_dev is a child of the i2c device */
	iio->dev.parent = &client->dev;

	switch (id->driver_data) {
	case ID_ISL76534:
	default:
		iio->num_channels = 15;
		break;
	}
	iio->channels = isl76534_channels;
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &isl76534_info;

	isl->vref_reg = devm_regulator_get(&client->dev, "vref");
	if (IS_ERR(isl->vref_reg)) {
		ret = PTR_ERR(isl->vref_reg);
		dev_err(&client->dev,
			"Failed to get vref regulator: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(isl->vref_reg);
	if (ret) {
		dev_err(&client->dev,
			"Failed to enable vref regulator: %d\n", ret);
		return ret;
	}

	ret = regulator_get_voltage(isl->vref_reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to get voltage on regulator: %d\n", ret);
		goto exit1;
	}
	isl->vref_mv = ret / 1000;
	msleep(10);

	ret = isl76534_read_reg(isl, 1);
	if (ret < 0) {
		ret = -ENODEV;
		goto exit1;
	}

	/* Software reset */
	ret = isl76534_write_reg(isl, 0, 6);
	if (ret < 0)
		goto exit1;
	msleep(10);

	if (device_create_file(&iio->dev, &dev_attr_isl76534_reg))
		dev_err(&iio->dev, "Error on creating sysfs file for isl76534_reg\n");
	else
		dev_err(&iio->dev, "created sysfs entry for reading regs\n");
	return iio_device_register(iio);

exit1:
	regulator_disable(isl->vref_reg);
	return ret;
}

static int isl76534_remove(struct i2c_client *client)
{
	iio_device_unregister(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id isl76534_id[] = {
	{ "isl76534", ID_ISL76534 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl76534_id);

static struct i2c_driver isl76534_driver = {
	.driver = {
		.name	= "isl76534",
	},
	.probe		= isl76534_probe,
	.remove		= isl76534_remove,
	.id_table	= isl76534_id,
};
module_i2c_driver(isl76534_driver);

MODULE_AUTHOR("Troy Kisky<troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("ISL76534 10-bit 14 channel + VCOM DAC");
MODULE_LICENSE("GPL");
