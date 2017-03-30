/*
 * Intersil ISL900xx I2C ADC driver
 *
 * Copyright 2017 Troy Kisky <troy.kisky@boundarydevices.com>
 *
 * Based on ti-isl28022.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define ISL_CONFIGURATION		0
#define ISL_SHUNT_VOLTAGE		1
#define ISL_BUS_VOLTAGE			2
/* Power register =  ((ISL_BUS_VOLTAGE >> 3) * ISL_CURRENT) / 5000 */
#define ISL_POWER			3
/* Current register = (ISL_SHUNT_VOLTAGE * ISL_CALIBRATION) >> 12 */
#define ISL_CURRENT			4
#define ISL_CALIBRATION			5
#define ISL_SHUNT_VOLTAGE_THRESHOLD	6
#define ISL_BUS_VOLTAGE_THRESHOLD	7
#define ISL_INTERRUPT_STATUS		8
#define ISL_AUX_CONTROL			9
#define ISL_LAST_REG			9

struct isl28022_state {
	struct i2c_client	*client;
	struct i2c_msg		clear_msg;
	struct completion	completion;

	struct iio_trigger	*trig;
	s64			timestamp;

	struct mutex		read_lock;
	struct mutex		write_lock;
	struct mutex		scan_lock;
	unsigned		rshunt;
	unsigned char		active_scan_mask;
	unsigned char		bytes_per_cycle;
	unsigned char		current_scan_mask;
	unsigned char		last_reg;
	unsigned char		result_index;
	unsigned char		get_index;
	unsigned char		trigger_on;
	unsigned char		vbus_shift;
	unsigned short		cfg;
	/*
	 * DMA (thus cache coherency maintenance) requires the
	 * transfer buffers to live in their own cache lines.
	 */
	unsigned char result_buf[64] ____cacheline_aligned;
	unsigned char clear_int[32];
	unsigned char reg_read[32];
	unsigned char reg_read_interrupt[32];
	unsigned char reg_write[32];
	unsigned char reg_write_interrupt[32];
	unsigned char push_buf[32];
};

struct isl28022_chip_info {
	const struct iio_chan_spec *channels;
	unsigned int num_channels;
};

enum isl2802x_id {
	ISL28022,
};

#define ISL900XX_V_CHAN(index, name, _type)				\
{								\
	.type = _type,						\
	.indexed = 1,						\
	.channel = index,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				BIT(IIO_CHAN_INFO_SCALE),	\
	.address = index,					\
	.datasheet_name = name,				\
	.scan_index = index,					\
	.scan_type = {						\
		.sign = ((index == 0) || (index == 3)) ? 's' : 'u',	\
		.realbits = 16,					\
		.storagebits = 16,				\
		.shift = 0,					\
		.endianness = IIO_BE,				\
	},							\
}


#define DECLARE_ISL900XX_CHANNELS(name) \
const struct iio_chan_spec name ## _channels[] = { \
	ISL900XX_V_CHAN(0, "shunt", IIO_VOLTAGE), \
	ISL900XX_V_CHAN(1, "bus", IIO_VOLTAGE), \
	ISL900XX_V_CHAN(2, "power", IIO_POWER), \
	ISL900XX_V_CHAN(3, "current", IIO_CURRENT), \
	IIO_CHAN_SOFT_TIMESTAMP(16), \
}

static DECLARE_ISL900XX_CHANNELS(isl28022);

static const struct isl28022_chip_info isl28022_chip_info[] = {
	[ISL28022] = {
		.channels	= isl28022_channels,
		.num_channels	= ARRAY_SIZE(isl28022_channels),
	},
};

static int isl28022_read_reg(struct isl28022_state *st, unsigned reg)
{
	struct i2c_msg msgs[2];
	unsigned char *p = &st->reg_read[0];
	int ret;
	int val;

	if (reg > ISL_LAST_REG)
		return -EINVAL;

	msgs[0].addr = st->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = p;

	msgs[1].addr = st->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = p;

	mutex_lock(&st->read_lock);

	p[0] = reg;
	ret = i2c_transfer(st->client->adapter, msgs, 2);

	val = (p[0] << 8) | p[1];
	mutex_unlock(&st->read_lock);

	if (ret < 0) {
		pr_err("%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, val);
	return val;
}

static int isl28022_read_reg_interrupt(struct isl28022_state *st, unsigned reg)
{
	struct i2c_msg msgs[2];
	unsigned char *p = &st->reg_read_interrupt[0];
	int ret;
	int val;

	if (reg > ISL_LAST_REG)
		return -EINVAL;

	msgs[0].addr = st->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = p;

	msgs[1].addr = st->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = p;

	p[0] = reg;
	ret = i2c_transfer(st->client->adapter, msgs, 2);

	val = (p[0] << 8) | p[1];

	if (ret < 0) {
		pr_err("%s:reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}
	return val;
}

static int isl28022_write_reg(struct isl28022_state *st, unsigned reg, unsigned value)
{
	struct i2c_msg msgs[1];
	unsigned char *p = &st->reg_write[0];
	int ret;

	if (reg > ISL_LAST_REG)
		return -EINVAL;

	msgs[0].addr = st->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 3;
	msgs[0].buf = p;

	mutex_lock(&st->write_lock);

	p[0] = reg;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)value;

	ret = i2c_transfer(st->client->adapter, msgs, 1);
	mutex_unlock(&st->write_lock);

	if (ret < 0) {
		pr_err("%s:reg=%x val=%x ret=%d\n", __func__, reg, value, ret);
		return ret;
	}
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, value);
	return 0;
}

static int isl28022_write_reg_interrupt(struct isl28022_state *st, unsigned reg, unsigned value)
{
	struct i2c_msg msgs[1];
	unsigned char *p = &st->reg_write_interrupt[0];
	int ret;

	if (reg > ISL_LAST_REG)
		return -EINVAL;

	msgs[0].addr = st->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 3;
	msgs[0].buf = p;


	p[0] = reg;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)value;

	ret = i2c_transfer(st->client->adapter, msgs, 1);

	if (ret < 0) {
		pr_err("%s:reg=%x val=%x ret=%d\n", __func__, reg, value, ret);
		return ret;
	}
	pr_debug("%s:reg=%x,val=%x\n", __func__, reg, value);
	return 0;
}

/*  **********************  trigger functions ********************** */
static int start_scan(struct isl28022_state *st, unsigned mask, int continuous)
{
	int ret = 0;
	int chan;
	unsigned cfg;

	if (!mask || (mask & 0xfffffff0)) {
		pr_err("%s: mask=%x\n", __func__, mask);
		return -EINVAL;
	}

	chan = __ffs(mask);
	cfg = st->cfg & ~0x7;
	/* Reading vshunt, power, or current requires reading shunt voltage*/
	if (mask & (BIT(0) | BIT(2) | BIT(3)))
		cfg |= 1;
	/* Reading vbus, or power requires reading bus voltage*/
	if (mask & (BIT(1) | BIT(2)))
		cfg |= 2;
	if (continuous)
		cfg |= 4;

	if (!mutex_trylock(&st->scan_lock))
		return -EBUSY;

	if ((cfg & 3) == 3) {
		/*
		 * if reading both voltages,
		 * only interrupt on VBUS adc completion
		 */
		ret = isl28022_write_reg(st, ISL_SHUNT_VOLTAGE_THRESHOLD, 0x7f81);

	} else {
		if (cfg & 1)
			ret = isl28022_write_reg(st, ISL_SHUNT_VOLTAGE_THRESHOLD, 0x817f);
	}
	if (ret < 0)
		goto error1;
	st->cfg = cfg;
	/* Shunt voltage/ bus voltage/ power / current */

	st->result_index = 0;
	st->get_index = 0;

	st->bytes_per_cycle = hweight8(mask) << 1;
	memset(st->push_buf, 0, sizeof(st->push_buf));

	/* Clear any pending interrupts*/
	ret = isl28022_write_reg(st, ISL_INTERRUPT_STATUS, 0xf);
	if (ret < 0)
		goto error1;
	ret = isl28022_write_reg(st, ISL_AUX_CONTROL, BIT(7));
	if (ret < 0)
		goto error1;
	st->current_scan_mask = mask;
	ret = isl28022_write_reg(st, ISL_CONFIGURATION, cfg);
	if (ret < 0)
		goto error1;
	return 0;

error1:
	pr_err("%s: ret=%d\n", __func__, ret);
	st->current_scan_mask = 0;
	isl28022_write_reg(st, ISL_AUX_CONTROL, 0);
	mutex_unlock(&st->scan_lock);
	return ret;
}

static int isl28022_set_trigger_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *iio = iio_trigger_get_drvdata(trig);
	struct isl28022_state *st = iio_priv(iio);
	int ret = 0;

	st->trigger_on = state;
	if (state) {
		ret = start_scan(st, st->active_scan_mask, 1);
		if (ret < 0)
			st->trigger_on = 0;
	} else {
		st->current_scan_mask = 0;
		/* Disble interrupts */
		isl28022_write_reg(st, ISL_AUX_CONTROL, 0);
		/* Power down */
		isl28022_write_reg(st, ISL_CONFIGURATION, st->cfg & ~ 0x7);
		mutex_unlock(&st->scan_lock);
	}
	return ret;
}

static int isl28022_validate_device(struct iio_trigger *trig,
				   struct iio_dev *iio)
{
	struct iio_dev *indio = iio_trigger_get_drvdata(trig);

	if (indio != iio)
		return -EINVAL;

	return 0;
}

static const struct iio_trigger_ops isl28022_trigger_ops = {
	.owner = THIS_MODULE,
	.validate_device = &isl28022_validate_device,
	.set_trigger_state = &isl28022_set_trigger_state,
};

/*  ********************** irq function ********************** */
static irqreturn_t isl28022_irq_handler(int irq, void *data)
{
	struct iio_dev *iio = data;
	struct isl28022_state *st = iio_priv(iio);
	int mask = st->current_scan_mask;
	int chan;
	int ret;
	int i;

	if (!mask) {
		/* Clear interrupts */
		isl28022_write_reg_interrupt(st, ISL_INTERRUPT_STATUS, 0x0f);
		isl28022_write_reg_interrupt(st, ISL_AUX_CONTROL, 0x80);
		/* Adc off */
		isl28022_write_reg_interrupt(st, ISL_CONFIGURATION,
				(st->cfg & ~0x7) | 4);
		msleep(10);
		return IRQ_HANDLED;
	}

	st->timestamp = iio_get_time_ns(iio);
	ret = i2c_transfer(st->client->adapter, &st->clear_msg, 1);
	if (ret < 0) {
		pr_err("%s:ret=%d\n", __func__, ret);
		msleep(10);
		return IRQ_HANDLED;
	}

	i = st->result_index;
	chan = __ffs(mask);
	mask >>= chan;

	while (1) {
		if (mask & 1) {
			unsigned v = isl28022_read_reg_interrupt(st,
					chan + ISL_SHUNT_VOLTAGE);

			if (v < 0) {
				pr_err("%s:v=%d\n", __func__, v);
				msleep(10);
				return IRQ_HANDLED;
			}

			if (chan == 1)
				v >>= st->vbus_shift;

			st->result_buf[i++] = v >> 8;
			st->result_buf[i++] = (unsigned char)v;

			if (i >= ARRAY_SIZE(st->result_buf))
				i = 0;
			if (st->get_index == i)
				break;
		}
		mask >>= 1;
		chan++;
		if (!mask) {
			st->result_index = i;
			break;
		}
	}

	pr_debug("%s: result_index=%x %x\n", __func__,
			st->result_index,
			st->current_scan_mask);
	if (iio_buffer_enabled(iio)) {
		if (st->trigger_on) {
			pr_debug("%s: d\n", __func__);
			iio_trigger_poll_chained(iio->trig);
		} else {
			pr_debug("%s:trigger off, %x\n", __func__,
					st->result_index);
		}
	} else {
		complete(&st->completion);
	}
	return IRQ_HANDLED;
}

/*
 * isl28022_update_scan_mode() setup the transfer buffer for the new
 * scan mask
 */
static int isl28022_update_scan_mode(struct iio_dev *iio,
				     const unsigned long *active_scan_mask)
{
	struct isl28022_state *st = iio_priv(iio);
	unsigned long mask = *active_scan_mask;
	unsigned char next = mask & 0xf;

	if (next != mask)
		return -EINVAL;

	st->active_scan_mask = next;
	return 0;
}

static irqreturn_t isl28022_trigger_handler(int irq, void *dat)
{
	struct iio_poll_func *pf = dat;
	struct iio_dev *iio = pf->indio_dev;
	struct isl28022_state *st = iio_priv(iio);
	unsigned get_index = st->get_index;
	unsigned char *p = &st->result_buf[get_index];
	int cnt;
	int bytes_per_cycle = st->bytes_per_cycle;
	int r = st->result_index;

	if (r < get_index)
		r += ARRAY_SIZE(st->result_buf);
	get_index += bytes_per_cycle;
	if (get_index > r) {
		pr_err("%s: sample not ready, %x %x %x\n", __func__,
			get_index, r, bytes_per_cycle);
		goto out;
	}
	if (get_index >= ARRAY_SIZE(st->result_buf)) {
		get_index -= ARRAY_SIZE(st->result_buf);
		cnt = bytes_per_cycle - get_index;
		memcpy(st->push_buf, p, cnt);
		if (get_index) {
			memcpy(&st->push_buf[cnt], st->result_buf, get_index);
		}
	} else {
		memcpy(st->push_buf, p, bytes_per_cycle);
	}
	p = st->push_buf;
	st->get_index = get_index;
	iio_push_to_buffers_with_timestamp(iio, p, st->timestamp);
out:
	iio_trigger_notify_done(iio->trig);
	return IRQ_HANDLED;
}

static int isl28022_scan_direct(struct isl28022_state *st, unsigned int chan)
{
	int ret = start_scan(st, 1 << chan, 0);

	if (ret < 0)
		return ret;

	ret = wait_for_completion_interruptible_timeout(
		&st->completion, HZ);
	if (ret == 0)
		ret = -ETIMEDOUT;
	if (ret < 0) {
		pr_err("%s:ret=%d\n", __func__, ret);
	} else {
		ret = (st->result_buf[0] << 8) | st->result_buf[1];
	}
	st->current_scan_mask = 0;
	mutex_unlock(&st->scan_lock);
	return ret;
}

static int isl28022_read_raw(struct iio_dev *iio,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long m)
{
	struct isl28022_state *st = iio_priv(iio);
	int ret;
	unsigned pga = (st->cfg >> 11) & 3;
	unsigned vfs = 40 << pga;

	switch (m) {
	case IIO_CHAN_INFO_RAW:
#if 0
		ret = iio_device_claim_direct_mode(iio);
		if (ret < 0)
			return ret;
#endif
		ret = isl28022_scan_direct(st, chan->address);
#if 0
		iio_device_release_direct_mode(iio);
#endif
		if (ret < 0)
			return ret;
		if ((chan->address == 0) || (chan->address == 3))
			ret = (short)ret;
		*val = ret;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		if (chan->address == 0) {
			/* Vshunt */
			*val =  (1 << 24) / 100;
			*val2 = 24;
		} else if (chan->address == 1) {
			/* Vbus */
			*val = 4;
			*val2 = 0;
		} else if (chan->address == 2) {
			/*
			 *  Power(mW)  = Vbus (mV) * Current(mA) * 5000 *
			 *  		 1(mW)/1000(uW)
			 */
			if (st->cfg & (1 << 14))
				vfs <<= 1;
			*val = (4 * 5 * vfs) / st->rshunt;
			*val2 = 15;
		} else  {
			/* Current */
			*val = vfs / st->rshunt;
			*val2 = 15;
		}
		return IIO_VAL_FRACTIONAL_LOG2;
	}
	return -EINVAL;
}

static const struct iio_info isl28022_info = {
	.read_raw		= &isl28022_read_raw,
	.update_scan_mode	= isl28022_update_scan_mode,
	.driver_module		= THIS_MODULE,
};

static ssize_t show_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct isl28022_state *st = iio_priv(iio);
	s32 rval;

	if (st->last_reg > ISL_LAST_REG) {
		int reg;
		int cnt = 0;
		int n;

		for (reg = 0; reg <= ISL_LAST_REG; reg++) {
			rval = isl28022_read_reg(st, reg);
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
	rval = isl28022_read_reg(st, st->last_reg);
	if (rval < 0)
		return rval;
	return sprintf(buf, "0x%x\n", rval);
}

void cfg_update(struct isl28022_state *st, int value)
{
	unsigned cal;
	unsigned pga = (value >> 11) & 3;

	st->cfg = value;
	st->vbus_shift = (value & (1 << 14)) ? 2 : 3;
	/*
	 * Ifs = Vfs / Rshunt
	 *
	 * Ilsb = (Vfs / Rshunt) >> 15
	 *
	 * Cal = (10 uV << 12) / (((Vfs / Rshunt) >> 15) * Rshunt)
	 * Cal = (10 uV << 27) / Vfs
	 * Cal = ((10 uV << 27) / 40000 uV) >> pga
	 * Cal = ((1 << 27) / 4000)  >> pga
	 */
	cal = ((1 << 27) / 4000) >> pga;
	isl28022_write_reg(st, ISL_CALIBRATION, cal);
}

static ssize_t store_reg(struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct iio_dev *iio = container_of(dev, struct iio_dev, dev);
	struct isl28022_state *st = iio_priv(iio);
	unsigned reg, value;
	int numscanned = sscanf(buf,"%x %x\n", &reg, &value);

	if (reg > ISL_LAST_REG) {
		if ((numscanned != 1) || (reg != ~0)) {
			dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
				__func__);
			return -EINVAL;
		}
	}

	if (numscanned == 2) {
		int rval = isl28022_write_reg(st, reg, value);
		if (rval) {
			dev_err(dev, "%s: error %d setting reg 0x%04x to 0x%02x\n",
				__func__, rval, reg, value);
		} else if (reg == ISL_CONFIGURATION) {
			cfg_update(st, value);
		}
	} else if (numscanned == 1) {
		st->last_reg = reg;
	} else {
		dev_err(dev,"%s: invalid register: use form 0xREG [0xVAL]\n",
			__func__);
	}
	return count;
}
static DEVICE_ATTR(isl28022_reg, S_IRUGO|S_IWUSR|S_IWGRP, show_reg, store_reg);

static int isl28022_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct isl28022_state *st;
	struct iio_dev *iio;
	struct i2c_msg	*msgs;
	int ret;
	unsigned brng = 3;
	unsigned pga = 3;
	unsigned badc = 3;
	unsigned sadc = 3;
	unsigned cfg;
	u32 val;
	const struct isl28022_chip_info *info =
			&isl28022_chip_info[id->driver_data];
	struct device *dev = &client->dev;


	iio = devm_iio_device_alloc(dev, sizeof(*st));
	if (!iio)
		return -ENOMEM;

	st = iio_priv(iio);
	st->client = client;
	init_completion(&st->completion);
	mutex_init(&st->read_lock);
	mutex_init(&st->write_lock);
	mutex_init(&st->scan_lock);

	st->clear_int[0] = ISL_INTERRUPT_STATUS;
	st->clear_int[1] = 0;
	st->clear_int[2] = 0xf;

	msgs = &st->clear_msg;
	msgs->addr = client->addr;
	msgs->flags = 0;
	msgs->len = 3;
	msgs->buf = &st->clear_int[0];

	i2c_set_clientdata(client, iio);

	iio->name = id->name;
	iio->dev.parent = dev;
	iio->modes = INDIO_DIRECT_MODE;
	iio->channels = info->channels;
	iio->num_channels = info->num_channels;
	iio->info = &isl28022_info;

	/* reset chip */
	ret = isl28022_write_reg(st, ISL_CONFIGURATION, 0x8000);
	if (ret < 0) {
		dev_err(dev, "I2c write failed(%d)\n", ret);
		goto error2;
	}
	ret = of_property_read_u32(dev->of_node, "bus-voltage-max", &val);
	if (!ret) {
		if (val <= 16) {
			brng = 0;
		} else if (val <= 32) {
			brng = 1;
		} else if (val <= 60) {
			brng = 3;
		} else {
			ret = -EINVAL;
			goto error2;
		}
	}
	ret = of_property_read_u32(dev->of_node, "shunt-voltage-max", &val);
	if (!ret) {
		if (val <= 40) {
			pga = 0;
		} else if (val <= 80) {
			pga = 1;
		} else if (val <= 160) {
			pga = 2;
		} else if (val <= 320) {
			pga = 3;
		} else {
			ret = -EINVAL;
			goto error2;
		}
	}
	ret = of_property_read_u32(dev->of_node, "bus-adc-cfg", &val);
	if (!ret) {
		if (val <= 0x0f) {
			badc = val;
		} else {
			ret = -EINVAL;
			goto error2;
		}
	}
	ret = of_property_read_u32(dev->of_node, "shunt-adc-cfg", &val);
	if (!ret) {
		if (val <= 0x0f) {
			sadc = val;
		} else {
			ret = -EINVAL;
			goto error2;
		}
	}
	ret = of_property_read_u32(dev->of_node, "rshunt", &val);
	st->rshunt = (!ret) ? val : 10;

	cfg = (brng << 13) | (pga << 11) | (badc << 7) | (sadc << 3);

	msleep(1);
	ret = isl28022_write_reg(st, ISL_CONFIGURATION, cfg);
	if (ret < 0) {
		dev_err(dev, "I2c write failed(%d)\n", ret);
		goto error2;
	}
	cfg_update(st, cfg);
	/* Arrange so that every conversion generates an interrupt */
	ret = isl28022_write_reg(st, ISL_SHUNT_VOLTAGE_THRESHOLD, 0x817f);
	if (ret < 0)
		goto error2;
	ret = isl28022_write_reg(st, ISL_BUS_VOLTAGE_THRESHOLD, 0x00ff);
	if (ret < 0)
		goto error2;
	isl28022_write_reg(st, ISL_INTERRUPT_STATUS, 0x0f);
	isl28022_write_reg(st, ISL_AUX_CONTROL, 0x80);

	st->trig = devm_iio_trigger_alloc(dev, "%s-trigger",
							iio->name);
	if (!st->trig) {
		ret = -ENOMEM;
		dev_err(&iio->dev, "Failed to allocate iio trigger\n");
		goto error2;
	}

	st->trig->ops = &isl28022_trigger_ops;
	st->trig->dev.parent = dev;
	iio_trigger_set_drvdata(st->trig, iio);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error2;

	ret = devm_request_threaded_irq(dev,
			client->irq, NULL, isl28022_irq_handler,
			IRQF_ONESHOT, "isl28022", iio);
	if (ret) {
		dev_err(dev, "Failed request irq\n");
		goto error3;
	}

	ret = iio_triggered_buffer_setup(iio, NULL,
					 &isl28022_trigger_handler, NULL);
	if (ret) {
		dev_err(dev, "Failed to setup triggered buffer\n");
		goto error3;
	}

	ret = iio_device_register(iio);
	if (ret) {
		dev_err(dev, "Failed to register iio device\n");
		goto error4;
	}

	if (device_create_file(&iio->dev, &dev_attr_isl28022_reg))
		dev_err(&iio->dev, "Error on creating sysfs file for isl28022_reg\n");
	else
		dev_err(&iio->dev, "created sysfs entry for reading regs\n");
	return 0;
error4:
	iio_triggered_buffer_cleanup(iio);
error3:
	iio_trigger_unregister(st->trig);
error2:
	return ret;
}

static int isl28022_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct isl28022_state *st = iio_priv(iio);

	iio_device_unregister(iio);
	iio_triggered_buffer_cleanup(iio);
	iio_trigger_unregister(st->trig);
	return 0;
}

static const struct i2c_device_id isl2802x_id[] = {
	{ "isl28022", ISL28022 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl2802x_id);

static struct i2c_driver isl28022_driver = {
	.driver = {
		.name	= "isl28022",
	},
	.probe		= isl28022_probe,
	.remove		= isl28022_remove,
	.id_table	= isl2802x_id,
};
module_i2c_driver(isl28022_driver);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("ISL28022 ADC");
MODULE_LICENSE("GPL v2");
