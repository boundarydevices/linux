/*
 * ADS7924 4-Channel, I2C, 12-Bit SAR ADC
 *
 * Copyright (C) 2017 Boundary Devices, Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

#define ADS7924_BITS			12
#define ADS7924_CHANNELS		4

/*
 * ADS7924 registers definition
 */
#define ADS7924_MODECTRL		0x00
#define ADS7924_INTCTRL			0x01
#define ADS7924_DATA(x)			(0x02 + 2*x)
#define ADS7924_ULR(x)			(0x0A + 2*x)
#define ADS7924_LLR(x)			(0x0B + 2*x)
#define ADS7924_INTCONFIG		0x12
#define ADS7924_SLPCONFIG		0x13
#define ADS7924_ACQCONFIG		0x14
#define ADS7924_PWRCONFIG		0x15
#define ADS7924_RESET			0x16

/*
 * ADS7924 MODECTRL register details
 */
#define ADS7924_MODE_MODE_MASK		0x3F
#define ADS7924_MODE_MODE_OFFSET	2
#define ADS7924_MODE_AWAKE		0x20
#define ADS7924_MODE_MAN_SINGLE		0x30
#define ADS7924_MODE_MAN_SCAN		0x32
#define ADS7924_MODE_AUTO_SINGLE	0x31
#define ADS7924_MODE_AUTO_SCAN		0x33
#define ADS7924_MODE_AUTO_SINGLE_SLP	0x39
#define ADS7924_MODE_AUTO_SCAN_SLP	0x3B
#define ADS7924_MODE_AUTO_BURST_SLP	0x3F

#define ADS7924_MODE_CHAN_MASK		0x03
#define ADS7924_MODE_CHAN_OFFSET	0

#define ADS7924_MODE_DEFAULT		(ADS7924_MODE_AUTO_SCAN)

/*
 * ADS7924 ADS7924_INTCTRL register details
 */
#define ADS7924_INT_STATUS_MASK		0x0F
#define ADS7924_INT_STATUS_OFFSET	4
#define ADS7924_INT_ENABLE_MASK		0x0F
#define ADS7924_INT_ENABLE_OFFSET	0

/*
 * ADS7924 ADS7924_INTCONFIG register details
 */
#define ADS7924_INTCFG_ACOUNT_EVERY	0x00
#define ADS7924_INTCFG_ACOUNT_MASK	0x03
#define ADS7924_INTCFG_ACOUNT_OFFSET	5
#define ADS7924_INTCFG_ACOUNT_TIME(x)	(x)
#define ADS7924_INTCFG_CONFIG_MASK	0x07
#define ADS7924_INTCFG_CONFIG_ALARM	0x00
#define ADS7924_INTCFG_CONFIG_ONE_RDY	0x02
#define ADS7924_INTCFG_CONFIG_ALL_RDY	0x06
#define ADS7924_INTCFG_CONFIG_OFFSET	2

#define ADS7924_INTCFG_DEFAULT		(ADS7924_INTCFG_CONFIG_ALARM)

/*
 * ADS7924 ADS7924_SLPCONFIG register details
 */
#define ADS7924_SLPCFG_SLPMULT8		BIT(4)
#define ADS7924_SLPCFG_SLPTIME_MASK	0x07
#define ADS7924_SLPCFG_SLPTIME_2_5MS	0x00
#define ADS7924_SLPCFG_SLPTIME_5MS	0x01
#define ADS7924_SLPCFG_SLPTIME_10MS	0x02
#define ADS7924_SLPCFG_SLPTIME_20MS	0x03
#define ADS7924_SLPCFG_SLPTIME_40MS	0x04
#define ADS7924_SLPCFG_SLPTIME_80MS	0x05
#define ADS7924_SLPCFG_SLPTIME_160MS	0x06
#define ADS7924_SLPCFG_SLPTIME_320MS	0x07
#define ADS7924_SLPCFG_SLPTIME_OFFSET	0

#define ADS7924_SLPTIME_DEFAULT		(320)

struct ads7924 {
	struct i2c_client	*client;
	struct regulator	*reg;
	struct gpio_desc	*reset_gpio;
	uint8_t mode;
	uint8_t irq_enabled;
};

static int ads7924_set_irq_mode(struct ads7924 *ads, uint8_t irq_mode)
{
	struct i2c_client *client = ads->client;
	uint8_t buffer = irq_mode << ADS7924_INTCFG_CONFIG_OFFSET;

	/* Make it so the int only occurs when that converions is above limit */
	buffer |= 1 << ADS7924_INTCFG_ACOUNT_OFFSET;

	dev_dbg(&client->dev, "%s: set INTCONFIG: %#02x\n", __func__, buffer);

	return i2c_smbus_write_byte_data(client, ADS7924_INTCONFIG, buffer);
}

static int ads7924_set_sleep(struct ads7924 *ads, int sleep_ms)
{
	struct i2c_client *client = ads->client;
	uint8_t buffer;

	switch (sleep_ms) {
	case 0 ... 4:
		buffer = ADS7924_SLPCFG_SLPTIME_2_5MS;
		break;
	case 5 ... 9:
		buffer = ADS7924_SLPCFG_SLPTIME_5MS;
		break;
	case 10 ... 19:
		buffer = ADS7924_SLPCFG_SLPTIME_10MS;
		break;
	case 20 ... 39:
		buffer = ADS7924_SLPCFG_SLPTIME_20MS;
		break;
	case 40 ... 79:
		buffer = ADS7924_SLPCFG_SLPTIME_40MS;
		break;
	case 80 ... 159:
		buffer = ADS7924_SLPCFG_SLPTIME_80MS;
		break;
	case 160 ... 319:
		buffer = ADS7924_SLPCFG_SLPTIME_160MS;
		break;
	case 320 ... 639:
		buffer = ADS7924_SLPCFG_SLPTIME_320MS;
		break;
	case 640 ... 1279:
		buffer = ADS7924_SLPCFG_SLPTIME_80MS | ADS7924_SLPCFG_SLPMULT8;
		break;
	case 1280 ... 2559:
		buffer = ADS7924_SLPCFG_SLPTIME_160MS | ADS7924_SLPCFG_SLPMULT8;
		break;
	case 2560 ... 3200:
		buffer = ADS7924_SLPCFG_SLPTIME_320MS | ADS7924_SLPCFG_SLPMULT8;
		break;
	default:
		dev_err(&client->dev, "%s: wrong value (%d); use default\n",
			__func__, sleep_ms);
		buffer = ADS7924_SLPCFG_SLPTIME_320MS;
		break;
	}

	dev_dbg(&client->dev, "%s: set SLPCONFIG: %#02x\n", __func__, buffer);

	return i2c_smbus_write_byte_data(client, ADS7924_SLPCONFIG, buffer);
}

static int ads7924_set_mode(struct ads7924 *ads, uint8_t mode, uint8_t chan)
{
	struct i2c_client *client = ads->client;
	uint8_t buffer;

	buffer = (chan & ADS7924_MODE_CHAN_MASK) << ADS7924_MODE_CHAN_OFFSET;
	buffer |= (mode & ADS7924_MODE_MODE_MASK) << ADS7924_MODE_MODE_OFFSET;

	ads->mode = mode;

	dev_dbg(&client->dev, "%s: set MODECNTRL: %#02x\n", __func__, buffer);

	return i2c_smbus_write_byte_data(client, ADS7924_MODECTRL, buffer);
}

static irqreturn_t ads7924_event_handler(int irq, void *private)
{
	struct iio_dev *iio = private;
	struct ads7924 *ads = iio_priv(iio);
	struct i2c_client *client = ads->client;
	s64 timestamp = iio_get_time_ns(iio);
	int ret;
	int i;
	uint16_t data;

	/* Need to read INTCONFIG to clear the alarm interrupt */
	ret = i2c_smbus_read_byte_data(client, ADS7924_INTCONFIG);
	if (ret < 0)
		return IRQ_HANDLED;

	/* Check if interrupt occurs because of alarm or ready value */
	data = (ret >> ADS7924_INTCFG_CONFIG_OFFSET) &
		ADS7924_INTCFG_CONFIG_MASK;
	if (data != ADS7924_INTCFG_CONFIG_ALARM) {
		/* In case of data ready, it must be read */
		for (i = 0; i < ADS7924_CHANNELS; i++)
			i2c_smbus_read_word_swapped(client, ADS7924_DATA(i));
	}

	/* Check alarm status */
	ret = i2c_smbus_read_byte_data(client, ADS7924_INTCTRL);
	if (ret < 0)
		return IRQ_HANDLED;

	/*
	 * Only check the AEN/STA bits since ALRM_ST are set even is the
	 * interrupt is disabled.
	 */
	for (i = 0; i < ADS7924_CHANNELS; i++)
		/* Check if that channel int occurred and is enabled */
		if (ret & BIT(i)) {
			dev_dbg(&client->dev, "%s: alarm for channel %d\n",
				__func__, i);
			/*
			 * We don't know if the alarm is because of the lower
			 * or upper limit, so send an 'either' event.
			 * Check if it is ok since we declare both falling and
			 * rising capabilites
			 */
			iio_push_event(iio,
				       IIO_UNMOD_EVENT_CODE(IIO_VOLTAGE,
							    i,
							    IIO_EV_TYPE_THRESH,
							    IIO_EV_DIR_EITHER),
				       timestamp);
		}

	return IRQ_HANDLED;
}

static int ads7924_read_event_value(struct iio_dev *iio,
				    const struct iio_chan_spec *chan,
				    enum iio_event_type type,
				    enum iio_event_direction dir,
				    enum iio_event_info info,
				    int *val, int *val2)
{
	struct ads7924 *ads = iio_priv(iio);
	struct i2c_client *client = ads->client;
	int reg;
	int ret;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING)
		reg = ADS7924_LLR(chan->channel);
	else if (dir == IIO_EV_DIR_RISING)
		reg = ADS7924_ULR(chan->channel);
	else
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	dev_dbg(&client->dev, "%s: get chan%d threshold(%d): %x\n", __func__,
		chan->channel, dir, ret << 4);

	/* Threshold value is an 8-bit register, scale the value */
	*val = ret << 4;

	return IIO_VAL_INT;
}

static int ads7924_write_event_value(struct iio_dev *iio,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir,
				     enum iio_event_info info,
				     int val, int val2)
{
	struct ads7924 *ads = iio_priv(iio);
	struct i2c_client *client = ads->client;
	int reg;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	if (dir == IIO_EV_DIR_FALLING)
		reg = ADS7924_LLR(chan->channel);
	else if (dir == IIO_EV_DIR_RISING)
		reg = ADS7924_ULR(chan->channel);
	else
		return -EINVAL;

	/* Threshold value is an 8-bit register, scale the value */
	val >>= 4;

	dev_dbg(&client->dev, "%s: set chan%d threshold(%d): %x\n", __func__,
		chan->channel, dir, val);

	return i2c_smbus_write_byte_data(client, reg, val);
}

static int ads7924_read_event_config(struct iio_dev *iio,
				     const struct iio_chan_spec *chan,
				     enum iio_event_type type,
				     enum iio_event_direction dir)
{
	struct ads7924 *ads = iio_priv(iio);
	struct i2c_client *client = ads->client;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	/*
	 * Can't read the configuration from the INTCNTRL register since it
	 * the AEN bits only report 1 if the alarm actually occurs.
	 * Instead, we're usinga a variable from the ads7924 structure.
	 */
	dev_dbg(&client->dev, "%s: get chan%d state: %d\n", __func__,
		chan->channel, (ads->irq_enabled & BIT(chan->channel) ? 1 : 0));

	if (ads->irq_enabled & BIT(chan->channel))
		return 1;

	return 0;
}

static int ads7924_write_event_config(struct iio_dev *iio,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir,
				      int state)
{
	struct ads7924 *ads = iio_priv(iio);
	struct i2c_client *client = ads->client;
	int ret;

	/* Sanity check */
	if (chan->type != IIO_VOLTAGE)
		return -EINVAL;

	dev_dbg(&client->dev, "%s: set chan%d state: %d\n", __func__,
		chan->channel, state);

	/* Get interrupts control configuration */
	ret = i2c_smbus_read_byte_data(client, ADS7924_INTCTRL);
	if (ret < 0)
		return ret;

	/* Enable/Disable this channel interrupts */
	if (state) {
		ret |= BIT(chan->channel);
		ads->irq_enabled |= BIT(chan->channel);
	} else {
		ret &= ~BIT(chan->channel);
		ads->irq_enabled &= ~BIT(chan->channel);
	}

	return i2c_smbus_write_byte_data(client, ADS7924_INTCTRL, ret);
}

static int ads7924_read_raw(struct iio_dev *iio,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	int ret;
	int vref;
	struct ads7924 *ads = iio_priv(iio);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if ((ads->mode == ADS7924_MODE_MAN_SINGLE) ||
		    (ads->mode == ADS7924_MODE_MAN_SCAN)) {
			/* When not in AUTO mode, must force the conversion */
			ret = ads7924_set_mode(ads, ADS7924_MODE_MAN_SINGLE,
					       chan->channel);
			if (ret < 0)
				return ret;
			/* Wait for conversion and acquisition time */
			udelay(20);
		}
		ret = i2c_smbus_read_word_swapped(ads->client,
						  ADS7924_DATA(chan->channel));
		if (ret < 0)
			return ret;

		*val = (ret >> 4) & 0xfff;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		vref = regulator_get_voltage(ads->reg);
		if (vref < 0)
			return vref;
		*val = vref / 1000;
		*val2 = ADS7924_BITS;
		return IIO_VAL_FRACTIONAL_LOG2;
	default:
		return -EINVAL;
	}
}

static const struct iio_event_spec ads7924_events[] = {
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

#define ADS7924_VOLTAGE_CHAN(_chan)				\
{								\
	.type = IIO_VOLTAGE,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.indexed = 1,						\
	.channel = _chan,					\
	.event_spec = ads7924_events,				\
	.num_event_specs = ARRAY_SIZE(ads7924_events),		\
}

static const struct iio_chan_spec ads7924_channels[] = {
	ADS7924_VOLTAGE_CHAN(0),
	ADS7924_VOLTAGE_CHAN(1),
	ADS7924_VOLTAGE_CHAN(2),
	ADS7924_VOLTAGE_CHAN(3),
};

static const struct iio_info ads7924_info = {
	.read_raw = &ads7924_read_raw,
	.read_event_config = &ads7924_read_event_config,
	.write_event_config = &ads7924_write_event_config,
	.read_event_value = &ads7924_read_event_value,
	.write_event_value = &ads7924_write_event_value,
	.driver_module = THIS_MODULE,
};

static int ads7924_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ads7924 *ads;
	struct iio_dev *iio;
	struct device_node *np = client->dev.of_node;
	uint8_t irq_mode = ADS7924_INTCFG_DEFAULT;
	uint8_t mode = ADS7924_MODE_DEFAULT;
	int sleep = ADS7924_SLPTIME_DEFAULT;
	int ret;

	iio = devm_iio_device_alloc(&client->dev, sizeof(*ads));
	if (!iio)
		return -ENOMEM;
	ads = iio_priv(iio);

	/* Need to know vref voltage value */
	ads->reg = devm_regulator_get(&client->dev, "vref");
	if (IS_ERR(ads->reg))
		return PTR_ERR(ads->reg);

	ret = regulator_enable(ads->reg);
	if (ret)
		return ret;

	/* Get reset pin and initialize it */
	ads->reset_gpio = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ads->reset_gpio)) {
		dev_err(&client->dev, "Can't get reset gpio\n");
		return PTR_ERR(ads->reset_gpio);
	}
	udelay(1);
	gpiod_set_value(ads->reset_gpio, 0);
	/* wait a little after reset released */
	msleep(2);

	/* Initialize all the structures */
	i2c_set_clientdata(client, iio);
	ads->client = client;
	iio->name = id->name;
	iio->channels = ads7924_channels;
	iio->num_channels = ARRAY_SIZE(ads7924_channels);
	iio->dev.parent = &client->dev;
	iio->info = &ads7924_info;
	iio->modes = INDIO_DIRECT_MODE;

	/* Making sure to disable the interrupts first */
	ads->irq_enabled = 0;
	ret = i2c_smbus_write_byte_data(client, ADS7924_INTCTRL, 0);
	if (ret)
		goto error;

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, &ads7924_event_handler,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						id->name, iio);
		if (ret)
			goto error;
	}

	/* Configuring chip with custom or default values */
	ret = of_property_read_u8(np, "adc-irq-mode", &irq_mode);
	if (ret)
		dev_info(&client->dev, "No irq-mode found, using default\n");

	ret = of_property_read_u8(np, "adc-mode", &mode);
	if (ret)
		dev_info(&client->dev, "No mode found, using default\n");

	ret = of_property_read_u32(np, "adc-sleep-ms", &sleep);
	if (ret)
		dev_info(&client->dev, "No sleep-ms found, using default\n");

	ret = ads7924_set_irq_mode(ads, irq_mode);
	if (ret < 0) {
		dev_err(&client->dev, "Can't set chip mode!\n");
		goto error;
	}

	ret = ads7924_set_sleep(ads, sleep);
	if (ret < 0) {
		dev_err(&client->dev, "Can't set sleep time!\n");
		goto error;
	}

	ret = ads7924_set_mode(ads, mode, 0);
	if (ret < 0) {
		dev_err(&client->dev, "Can't set chip mode!\n");
		goto error;
	}

	ret = iio_device_register(iio);
	if (ret)
		goto error;

	dev_info(&client->dev, "probed!\n");

	return 0;
error:
	regulator_disable(ads->reg);
	return ret;
}

static int ads7924_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct ads7924 *ads = iio_priv(iio);

	iio_device_unregister(iio);
	regulator_disable(ads->reg);

	dev_info(&client->dev, "removed!\n");

	return 0;
}

static const struct i2c_device_id ads7924_id[] = {
	{ "ads7924", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ads7924_id);

#ifdef CONFIG_OF
static const struct of_device_id ads7924_of_match[] = {
	{ .compatible = "ti,ads7924" },
	{ }
};
MODULE_DEVICE_TABLE(of, ads7924_of_match);
#endif

static struct i2c_driver ads7924_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = of_match_ptr(ads7924_of_match),
	},
	.probe = ads7924_probe,
	.remove = ads7924_remove,
	.id_table = ads7924_id,
};
module_i2c_driver(ads7924_driver);

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("TI ADS7924 ADC driver");
MODULE_LICENSE("GPL");
