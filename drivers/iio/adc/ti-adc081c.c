/*
 * TI ADC081C/ADC101C/ADC121C 8/10/12-bit ADC driver
 *
 * Copyright (C) 2012 Avionic Design GmbH
 * Copyright (C) 2016 Intel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Datasheets:
 *	http://www.ti.com/lit/ds/symlink/adc081c021.pdf
 *	http://www.ti.com/lit/ds/symlink/adc101c021.pdf
 *	http://www.ti.com/lit/ds/symlink/adc121c021.pdf
 *
 * The devices have a very similar interface and differ mostly in the number of
 * bits handled. For the 8-bit and 10-bit models the least-significant 4 or 2
 * bits of value registers are reserved.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/acpi.h>

#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regulator/consumer.h>

/*
 * ADC081C registers definition
 */
#define ADC081C_RESULT			0x00	/* 2 bytes */
#define ADC081C_ALERT			0x01	/* 1 byte */
#define ADC081C_CONFIG			0x02	/* 1 byte */
#define ADC081C_LOW_LIMIT		0x03	/* 2 bytes */
#define ADC081C_HIGH_LIMIT		0x04	/* 2 bytes */
#define ADC081C_HYSTERESIS		0x05	/* 2 bytes */
#define ADC081C_LOWEST_CNV		0x06	/* 2 bytes */
#define ADC081C_HIGHEST_CNV		0x07	/* 2 bytes */

#define SINGLE_BYTE_REGS	(BIT(ADC081C_ALERT) | BIT(ADC081C_CONFIG))

struct adc081c {
	struct i2c_client *i2c;
	struct regulator *ref;
	unsigned samples_per_sec;
	unsigned char cfg;
	unsigned char enables;
	unsigned char low_limit;
	unsigned char high_limit;

	/* 8, 10 or 12 */
	int bits;
};

static enum iio_event_direction ev_type[] = {
	IIO_EV_DIR_EITHER, IIO_EV_DIR_FALLING,
	IIO_EV_DIR_RISING, IIO_EV_DIR_EITHER
};

static irqreturn_t adc081c_event_handler(int irq, void *private)
{
	struct iio_dev *iio = private;
	struct adc081c *adc = iio_priv(iio);
	struct i2c_client *client = adc->i2c;
	s64 timestamp = iio_get_time_ns(iio);
	int ret;
	enum iio_event_direction dir;

	/* Need to read INTCONFIG to clear the alarm interrupt */
	ret = i2c_smbus_read_byte_data(client, ADC081C_ALERT);
	if (ret < 0)
		return IRQ_HANDLED;

	ret &= 3;
	dir = ev_type[ret];
	/*
	 * Don't clear ADC081C_ALERT, we want another interrupt when things return
	 * to normal.
	 * i2c_smbus_write_byte_data(client, ADC081C_ALERT, ret);
	 */

	iio_push_event(iio, IIO_UNMOD_EVENT_CODE(IIO_VOLTAGE, 0,
			IIO_EV_TYPE_THRESH, dir), timestamp);

	return IRQ_HANDLED;
}

static int adc081c_read_event_value(struct iio_dev *iio,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir,
				    enum iio_event_info info,
				    int *val, int *val2)
{
	struct adc081c *adc = iio_priv(iio);
	int limit;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING)
		limit = adc->low_limit;
	else if (dir == IIO_EV_DIR_RISING)
		limit = adc->high_limit;
	else
		return -EINVAL;

	*val = limit;
	return IIO_VAL_INT;
}

static int adc081c_write_event_value(struct iio_dev *iio,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir,
				     enum iio_event_info info,
				     int val, int val2)
{
	struct adc081c *adc = iio_priv(iio);
	struct i2c_client *client = adc->i2c;
	int reg;
	int mask;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING) {
		adc->low_limit = val;
		reg = ADC081C_LOW_LIMIT;
		mask = 1;
	} else if (dir == IIO_EV_DIR_RISING) {
		adc->high_limit = val;
		reg = ADC081C_HIGH_LIMIT;
		mask = 2;
	} else {
		return -EINVAL;
	}

	if (adc->enables & mask) {
		val = (val & 0xff) << 4;
		dev_dbg(&client->dev, "%s: set threshold reg(%d): %x\n", __func__,
			reg, val);
		return i2c_smbus_write_word_swapped(client, reg, val);
	}
	return 0;
}

static int adc081c_read_event_config(struct iio_dev *iio,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir)
{
	struct adc081c *adc = iio_priv(iio);
	struct i2c_client *client = adc->i2c;
	int mask;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING)
		mask = 1;
	else if (dir == IIO_EV_DIR_RISING)
		mask = 2;
	else
		return -EINVAL;

	dev_dbg(&client->dev, "%s: get chan state: %d\n", __func__,
		(adc->enables & mask) ? 1 : 0);

	return (adc->enables & mask) ? 1 : 0;
}

static int adc081c_write_event_config(struct iio_dev *iio,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir,
				      int state)
{
	struct adc081c *adc = iio_priv(iio);
	struct i2c_client *client = adc->i2c;
	int mask;
	int val;
	int off;
	int reg;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING) {
		reg = ADC081C_LOW_LIMIT;
		mask = 1;
		val = adc->low_limit;
		off = 0;
	} else if (dir == IIO_EV_DIR_RISING) {
		reg = ADC081C_HIGH_LIMIT;
		mask = 2;
		val = adc->high_limit;
		off = 0xff;
	} else {
		dev_err(&client->dev, "%s: dir=%d\n", __func__, dir);
		return -EINVAL;
	}

	dev_dbg(&client->dev, "%s: set chan%d state: %d\n", __func__,
		chan->channel, state);

	if (state)
		adc->enables |= mask;
	else
		adc->enables &= ~mask;
	i2c_smbus_write_word_swapped(client, reg, (state ? val : off) << 4);
	return i2c_smbus_write_byte_data(client, ADC081C_CONFIG, adc->enables ? adc->cfg : 0);
}

static int adc081c_read_raw(struct iio_dev *iio,
			    struct iio_chan_spec const *channel, int *value,
			    int *shift, long mask)
{
	struct adc081c *adc = iio_priv(iio);
	int err;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		err = i2c_smbus_read_word_swapped(adc->i2c, ADC081C_RESULT);
		if (err < 0)
			return err;

		*value = (err & 0xFFF) >> (12 - adc->bits);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		err = regulator_get_voltage(adc->ref);
		if (err < 0)
			return err;

		*value = err / 1000;
		*shift = adc->bits;

		return IIO_VAL_FRACTIONAL_LOG2;

	default:
		break;
	}

	return -EINVAL;
}

static int adc081c_debugfs_reg_access(struct iio_dev *iio,
				      unsigned reg, unsigned writeval,
				      unsigned *readval)
{
	struct adc081c *adc = iio_priv(iio);
	int ret;

	if (reg > ADC081C_HIGHEST_CNV)
		return -EINVAL;

	if (readval) {
		ret = (BIT(reg) & SINGLE_BYTE_REGS) ?
			i2c_smbus_read_byte_data(adc->i2c, reg) :
			i2c_smbus_read_word_swapped(adc->i2c, reg);
		if (ret >= 0) {
			*readval = ret;
			ret = 0;
		}
	} else {
		if (BIT(reg) & SINGLE_BYTE_REGS) {
			if (writeval > 0xff)
				return -EINVAL;
			ret = i2c_smbus_write_byte_data(adc->i2c, reg, writeval);
		} else {
			if (writeval > 0xffff)
				return -EINVAL;
			ret = i2c_smbus_write_word_swapped(adc->i2c, reg, writeval);
		}
	}
	return ret;
}

static const struct iio_event_spec adc081c_events[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	}, {
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
};

#define ADCxx1C_CHAN(_bits) {					\
	.type = IIO_VOLTAGE,					\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.event_spec = adc081c_events,				\
	.num_event_specs = ARRAY_SIZE(adc081c_events),		\
	.scan_type = {						\
		.sign = 'u',					\
		.realbits = (_bits),				\
		.storagebits = 16,				\
		.shift = 12 - (_bits),				\
		.endianness = IIO_CPU,				\
	},							\
}

#define DEFINE_ADCxx1C_CHANNELS(_name, _bits)				\
	static const struct iio_chan_spec _name ## _channels[] = {	\
		ADCxx1C_CHAN((_bits)),					\
		IIO_CHAN_SOFT_TIMESTAMP(1),				\
	};								\

#define ADC081C_NUM_CHANNELS 2

struct adcxx1c_model {
	const struct iio_chan_spec* channels;
	int bits;
};

#define ADCxx1C_MODEL(_name, _bits)					\
	{								\
		.channels = _name ## _channels,				\
		.bits = (_bits),					\
	}

DEFINE_ADCxx1C_CHANNELS(adc081c,  8);
DEFINE_ADCxx1C_CHANNELS(adc101c, 10);
DEFINE_ADCxx1C_CHANNELS(adc121c, 12);

/* Model ids are indexes in _models array */
enum adcxx1c_model_id {
	ADC081C = 0,
	ADC101C = 1,
	ADC121C = 2,
};

static struct adcxx1c_model adcxx1c_models[] = {
	ADCxx1C_MODEL(adc081c,  8),
	ADCxx1C_MODEL(adc101c, 10),
	ADCxx1C_MODEL(adc121c, 12),
};

static const struct iio_info adc081c_info = {
	.read_raw = adc081c_read_raw,
	.read_event_config = &adc081c_read_event_config,
	.write_event_config = &adc081c_write_event_config,
	.read_event_value = &adc081c_read_event_value,
	.write_event_value = &adc081c_write_event_value,
	.debugfs_reg_access = &adc081c_debugfs_reg_access,
	.driver_module = THIS_MODULE,
};

static irqreturn_t adc081c_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct adc081c *data = iio_priv(indio_dev);
	u16 buf[8]; /* 2 bytes data + 6 bytes padding + 8 bytes timestamp */
	int ret;

	ret = i2c_smbus_read_word_swapped(data->i2c, ADC081C_RESULT);
	if (ret < 0)
		goto out;
	buf[0] = ret;
	iio_push_to_buffers_with_timestamp(indio_dev, buf,
					   iio_get_time_ns(indio_dev));
out:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int adc081c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct iio_dev *iio;
	struct adc081c *adc;
	struct adcxx1c_model *model;
	int err;
	int sps = 400;
	int cycle;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EOPNOTSUPP;

	if (ACPI_COMPANION(&client->dev)) {
		const struct acpi_device_id *ad_id;

		ad_id = acpi_match_device(client->dev.driver->acpi_match_table,
					  &client->dev);
		if (!ad_id)
			return -ENODEV;
		model = &adcxx1c_models[ad_id->driver_data];
	} else {
		model = &adcxx1c_models[id->driver_data];
	}

	err = of_property_read_u32(np, "samples-per-sec", &sps);
	if (err)
		dev_info(&client->dev, "samples-per-sec not found, using default\n");

	iio = devm_iio_device_alloc(&client->dev, sizeof(*adc));
	if (!iio)
		return -ENOMEM;

	adc = iio_priv(iio);
	adc->i2c = client;
	adc->bits = model->bits;
	adc->samples_per_sec = sps;
	if (sps) {
		/* 1000000 us/sec / 32 /1.1574 us/sample = 27000 samples/sec */
		cycle = fls(27000 / sps);
		if (!cycle)
			cycle = 1;
		else if (cycle > 7)
			cycle = 7;
		adc->cfg = (cycle << 5) | 4;
	} else {
		adc->cfg = (7 << 5) | 4;
	}
	adc->low_limit = 0x0;
	adc->high_limit = 0xff;

	adc->ref = devm_regulator_get(&client->dev, "vref");
	if (IS_ERR(adc->ref))
		return PTR_ERR(adc->ref);

	err = regulator_enable(adc->ref);
	if (err < 0)
		return err;

	iio->dev.parent = &client->dev;
	iio->dev.of_node = client->dev.of_node;
	iio->name = id->name;
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &adc081c_info;

	iio->channels = model->channels;
	iio->num_channels = ADC081C_NUM_CHANNELS;

	err = iio_triggered_buffer_setup(iio, NULL, adc081c_trigger_handler, NULL);
	if (err < 0) {
		dev_err(&client->dev, "iio triggered buffer setup failed\n");
		goto err_regulator_disable;
	}

	err = i2c_smbus_write_byte_data(client, ADC081C_CONFIG, 0x00);
	if (err < 0)
		goto err_regulator_disable;

	i2c_smbus_write_word_swapped(client, ADC081C_LOW_LIMIT, 0x0);
	i2c_smbus_write_word_swapped(client, ADC081C_HIGH_LIMIT, 0xff0);
	if (client->irq) {
		/*
		 * Interrupt when levels go out of range, and when
		 * they return to normal levels
		 */
		err = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, &adc081c_event_handler,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						id->name, iio);
		if (err)
			goto err_regulator_disable;
	}

	err = iio_device_register(iio);
	if (err < 0)
		goto err_buffer_cleanup;

	i2c_set_clientdata(client, iio);

	return 0;

err_buffer_cleanup:
	iio_triggered_buffer_cleanup(iio);
err_regulator_disable:
	regulator_disable(adc->ref);

	return err;
}

static int adc081c_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct adc081c *adc = iio_priv(iio);

	iio_device_unregister(iio);
	iio_triggered_buffer_cleanup(iio);
	regulator_disable(adc->ref);

	return 0;
}

static const struct i2c_device_id adc081c_id[] = {
	{ "adc081c", ADC081C },
	{ "adc101c", ADC101C },
	{ "adc121c", ADC121C },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adc081c_id);

#ifdef CONFIG_OF
static const struct of_device_id adc081c_of_match[] = {
	{ .compatible = "ti,adc081c" },
	{ .compatible = "ti,adc101c" },
	{ .compatible = "ti,adc121c" },
	{ }
};
MODULE_DEVICE_TABLE(of, adc081c_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id adc081c_acpi_match[] = {
	{ "ADC081C", ADC081C },
	{ "ADC101C", ADC101C },
	{ "ADC121C", ADC121C },
	{ }
};
MODULE_DEVICE_TABLE(acpi, adc081c_acpi_match);
#endif

static struct i2c_driver adc081c_driver = {
	.driver = {
		.name = "adc081c",
		.of_match_table = of_match_ptr(adc081c_of_match),
		.acpi_match_table = ACPI_PTR(adc081c_acpi_match),
	},
	.probe = adc081c_probe,
	.remove = adc081c_remove,
	.id_table = adc081c_id,
};
module_i2c_driver(adc081c_driver);

MODULE_AUTHOR("Thierry Reding <thierry.reding@avionic-design.de>");
MODULE_DESCRIPTION("Texas Instruments ADC081C/ADC101C/ADC121C driver");
MODULE_LICENSE("GPL v2");
