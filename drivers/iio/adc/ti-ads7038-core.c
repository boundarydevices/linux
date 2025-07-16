// SPDX-License-Identifier: GPL-2.0-or-later
/* This driver supports TI 12Bit ADC devices
 *
 *	 - ADS7038 with SPI interface
 *	 - ADS7138 with I2C interface (future)
 *
 * Copyright (C) 2023 SYS TEC electronic AG
 * Author: Andre Werner <andre.werner@systec-electronic.com>
 */
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/types.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include "linux/regulator/consumer.h"

#include "ti-ads7038.h"

#define ADS7038_V_CHAN(_chan, _addr)				\
	{							\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.address = _addr,					\
	.channel = _chan,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_index = _addr,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 12,					\
		.storagebits = 16,				\
		.shift = 4,					\
		.endianness = IIO_CPU,				\
	},							\
	.datasheet_name = "AIN"#_chan,				\
	}

static const struct iio_chan_spec ads7038_channels[] = {
	ADS7038_V_CHAN(0, AIN0), ADS7038_V_CHAN(1, AIN1),
	ADS7038_V_CHAN(2, AIN2), ADS7038_V_CHAN(3, AIN3),
	ADS7038_V_CHAN(4, AIN4), ADS7038_V_CHAN(5, AIN5),
	ADS7038_V_CHAN(6, AIN6), ADS7038_V_CHAN(7, AIN7),
};

static int ads7038_read_raw(struct iio_dev *indio_dev,
		     struct iio_chan_spec const *chan,
		     int *val, int *val2,
		     long mask)
{
	unsigned int ret;
	struct ads7038_ch_meas_result tmp_val;
	struct ads7038_data *const data = (struct ads7038_data *)iio_priv(indio_dev);
	struct ads7038_info *const info = (struct ads7038_info *)data->info;

	ret = info->read_channel(indio_dev, chan->channel, &tmp_val);

	if (ret < 0) {
		dev_err(&indio_dev->dev, "Read channel returned with error %d", ret);
		return ret;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = tmp_val.raw;

		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		ret = regulator_get_voltage(data->reg);
		if (ret < 0)
			break;

		*val = ret / 1000;	/* uV -> mV */
		*val2 = (1 << chan->scan_type.realbits) - 1;

		ret = IIO_VAL_FRACTIONAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct iio_info ads7038_iio_info = {
	.read_raw = ads7038_read_raw,
};

int ads7038_common_probe(struct device *parent, const struct ads7038_info *info,
			 struct regmap *const regmap,
			 struct regulator *const ref_voltage_reg,
			 const char *name, int irq)
{
	struct ads7038_data *data;
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(parent, sizeof(struct ads7038_data));
	if (!indio_dev)
		return -ENOMEM;

	indio_dev->name = name;
	indio_dev->channels = ads7038_channels;
	indio_dev->num_channels = ARRAY_SIZE(ads7038_channels);
	indio_dev->info = &ads7038_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	data = (struct ads7038_data *)iio_priv(indio_dev);
	mutex_init(&data->lock);
	data->dev = parent;
	data->info = info;
	data->regmap = regmap;
	data->reg = ref_voltage_reg;

	/* Link general device driver with IIO device driver data */
	dev_set_drvdata(parent, indio_dev);

	iio_device_register(indio_dev);

	return 0;
}
EXPORT_SYMBOL(ads7038_common_probe);

int ads7038_common_remove(struct device *parent)
{
	struct iio_dev *indio_dev = (struct iio_dev *)dev_get_drvdata(parent);

	iio_device_unregister(indio_dev);

	return 0;
}
EXPORT_SYMBOL(ads7038_common_remove);

MODULE_AUTHOR("Andre Werner <andre.werner@systec-electronic.com>");
MODULE_DESCRIPTION("ADS7038 and ADS7138 core driver");
MODULE_LICENSE("GPL");
