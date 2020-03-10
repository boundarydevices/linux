/*
 * STMicroelectronics st_mag40 driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Matteo Dameno <matteo.dameno@st.com>
 * Armando Visconti <armando.visconti@st.com>
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/delay.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <asm/unaligned.h>

#include "st_mag40_core.h"

struct st_mag40_odr_reg {
	u32 hz;
	u8 value;
};

#define ST_MAG40_ODR_TABLE_SIZE		4
static const struct st_mag40_odr_table_t {
	u8 addr;
	u8 mask;
	struct st_mag40_odr_reg odr_avl[ST_MAG40_ODR_TABLE_SIZE];
} st_mag40_odr_table = {
	.addr = ST_MAG40_ODR_ADDR,
	.mask = ST_MAG40_ODR_MASK,
	.odr_avl[0] = { .hz = 10, .value = ST_MAG40_CFG_REG_A_ODR_10Hz, },
	.odr_avl[1] = { .hz = 20, .value = ST_MAG40_CFG_REG_A_ODR_20Hz, },
	.odr_avl[2] = { .hz = 50, .value = ST_MAG40_CFG_REG_A_ODR_50Hz, },
	.odr_avl[3] = { .hz = 100, .value = ST_MAG40_CFG_REG_A_ODR_100Hz, },
};

#define ST_MAG40_ADD_CHANNEL(device_type, modif, index, mod, 	\
			     endian, sbits, rbits, addr, s) 	\
{								\
	.type = device_type,					\
	.modified = modif,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_index = index,					\
	.channel2 = mod,					\
	.address = addr,					\
	.scan_type = {						\
		.sign = s,					\
		.realbits = rbits,				\
		.shift = sbits - rbits,				\
		.storagebits = sbits,				\
		.endianness = endian,				\
	},							\
}

static const struct iio_chan_spec st_mag40_channels[] = {
	ST_MAG40_ADD_CHANNEL(IIO_MAGN, 1, 0, IIO_MOD_X, IIO_LE, 16, 16,
				ST_MAG40_OUTX_L_ADDR, 's'),
	ST_MAG40_ADD_CHANNEL(IIO_MAGN, 1, 1, IIO_MOD_Y, IIO_LE, 16, 16,
				ST_MAG40_OUTY_L_ADDR, 's'),
	ST_MAG40_ADD_CHANNEL(IIO_MAGN, 1, 2, IIO_MOD_Z, IIO_LE, 16, 16,
				ST_MAG40_OUTZ_L_ADDR, 's'),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

int st_mag40_write_register(struct st_mag40_data *cdata, u8 reg_addr,
			    u8 mask, u8 data)
{
	int err;
	u8 val;

	mutex_lock(&cdata->lock);

	err = cdata->tf->read(cdata, reg_addr, sizeof(val), &val);
	if (err < 0)
		goto unlock;

	val = ((val & ~mask) | ((data << __ffs(mask)) & mask));

	err = cdata->tf->write(cdata, reg_addr, sizeof(val), &val);

unlock:
	mutex_unlock(&cdata->lock);

	return err < 0 ? err : 0;
}

static int st_mag40_write_odr(struct st_mag40_data *cdata, uint32_t odr)
{
	int err, i;

	for (i = 0; i < ST_MAG40_ODR_TABLE_SIZE; i++)
		if (st_mag40_odr_table.odr_avl[i].hz >= odr)
			break;

	if (i == ST_MAG40_ODR_TABLE_SIZE)
		return -EINVAL;

	err = st_mag40_write_register(cdata, st_mag40_odr_table.addr,
				      st_mag40_odr_table.mask,
				      st_mag40_odr_table.odr_avl[i].value);
	if (err < 0)
		return err;

	cdata->odr = odr;
	cdata->samples_to_discard = ST_MAG40_TURNON_TIME_SAMPLES_NUM;

	return 0;
}

int st_mag40_set_enable(struct st_mag40_data *cdata, bool state)
{
	u8 mode;

	mode = state ? ST_MAG40_CFG_REG_A_MD_CONT : ST_MAG40_CFG_REG_A_MD_IDLE;

	if (state) {
		cdata->ts = cdata->ts_irq = st_mag40_get_timestamp();
		cdata->delta_ts = div_s64(1000000000LL, cdata->odr);
	}

	return st_mag40_write_register(cdata, ST_MAG40_EN_ADDR,
				       ST_MAG40_EN_MASK, mode);
}

int st_mag40_init_sensors(struct st_mag40_data *cdata)
{
	int err;

	/*
	 * Enable block data update feature.
	 */
	err = st_mag40_write_register(cdata, ST_MAG40_CFG_REG_C_ADDR,
				      ST_MAG40_CFG_REG_C_BDU_MASK, 1);
	if (err < 0)
		return err;

	/*
	 * Enable the temperature compensation feature
	 */
	err = st_mag40_write_register(cdata, ST_MAG40_CFG_REG_A_ADDR,
				      ST_MAG40_TEMP_COMP_EN, 1);
	if (err < 0)
		return err;

	err = st_mag40_write_register(cdata, ST_MAG40_CFG_REG_B_ADDR,
				      ST_MAG40_CFG_REG_B_OFF_CANC_MASK, 1);

	return err < 0 ? err : 0;
}

static ssize_t st_mag40_get_sampling_frequency(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct st_mag40_data *cdata = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", cdata->odr);
}

static ssize_t st_mag40_set_sampling_frequency(struct device * dev,
					       struct device_attribute * attr,
					       const char *buf, size_t count)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_mag40_data *cdata = iio_priv(iio_dev);
	unsigned int odr;
	int err;

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	err = st_mag40_write_odr(cdata, odr);

	return err < 0 ? err : count;
}

static ssize_t
st_mag40_get_sampling_frequency_avail(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	int i, len = 0;

	for (i = 0; i < ST_MAG40_ODR_TABLE_SIZE; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_mag40_odr_table.odr_avl[i].hz);
	buf[len - 1] = '\n';

	return len;
}

static int st_mag40_read_oneshot(struct st_mag40_data *cdata,
				 u8 addr, int *val)
{
	u8 data[2];
	int err;

	err = st_mag40_set_enable(cdata, true);
	if (err < 0)
		return err;

	msleep(40);

	err = cdata->tf->read(cdata, addr, sizeof(data), data);
	if (err < 0)
		return err;

	*val = (s16)get_unaligned_le16(data);

	err = st_mag40_set_enable(cdata, false);

	return err < 0 ? err : IIO_VAL_INT;
}

static int st_mag40_read_raw(struct iio_dev *iio_dev,
			     struct iio_chan_spec const *ch,
			     int *val, int *val2, long mask)
{
	struct st_mag40_data *cdata = iio_priv(iio_dev);
	int ret;

	mutex_lock(&iio_dev->mlock);

	if (iio_buffer_enabled(iio_dev)) {
		mutex_unlock(&iio_dev->mlock);
		return -EBUSY;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = st_mag40_read_oneshot(cdata, ch->address, val);
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1500;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return ret;
}

static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
			      st_mag40_get_sampling_frequency,
			      st_mag40_set_sampling_frequency);
static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_mag40_get_sampling_frequency_avail);

static struct attribute *st_mag40_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_mag40_attribute_group = {
	.attrs = st_mag40_attributes,
};

static const struct iio_info st_mag40_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_mag40_attribute_group,
	.read_raw = &st_mag40_read_raw,
};

int st_mag40_common_probe(struct iio_dev *iio_dev)
{
	struct st_mag40_data *cdata = iio_priv(iio_dev);
	int32_t err;
	u8 wai;

	mutex_init(&cdata->lock);

	err = cdata->tf->read(cdata, ST_MAG40_WHO_AM_I_ADDR,
			      sizeof(wai), &wai);
	if (err < 0) {
		dev_err(cdata->dev, "failed to read Who-Am-I register.\n");

		return err;
	}

	if (wai != ST_MAG40_WHO_AM_I_DEF) {
		dev_err(cdata->dev, "Who-Am-I value not valid. (%02x)\n", wai);
		return -ENODEV;
	}

	cdata->odr = st_mag40_odr_table.odr_avl[0].hz;

	iio_dev->channels = st_mag40_channels;
	iio_dev->num_channels = ARRAY_SIZE(st_mag40_channels);
	iio_dev->info = &st_mag40_info;
	iio_dev->modes = INDIO_DIRECT_MODE;

	err = st_mag40_init_sensors(cdata);
	if (err < 0)
		return err;

	if (cdata->irq > 0) {
		err = st_mag40_allocate_ring(iio_dev);
		if (err < 0)
			return err;

		err = st_mag40_allocate_trigger(iio_dev);
		if (err < 0)
			goto deallocate_ring;
	}

	err = devm_iio_device_register(cdata->dev, iio_dev);
	if (err)
		goto iio_trigger_deallocate;

	return 0;

iio_trigger_deallocate:
	st_mag40_deallocate_trigger(cdata);

deallocate_ring:
	st_mag40_deallocate_ring(iio_dev);

	return err;
}
EXPORT_SYMBOL(st_mag40_common_probe);

void st_mag40_common_remove(struct iio_dev *iio_dev)
{
	struct st_mag40_data *cdata = iio_priv(iio_dev);

	if (cdata->irq > 0) {
		st_mag40_deallocate_trigger(cdata);
		st_mag40_deallocate_ring(iio_dev);
	}
}
EXPORT_SYMBOL(st_mag40_common_remove);

#ifdef CONFIG_PM
int st_mag40_common_suspend(struct st_mag40_data *cdata)
{
	return 0;
}
EXPORT_SYMBOL(st_mag40_common_suspend);

int st_mag40_common_resume(struct st_mag40_data *cdata)
{
	return 0;
}
EXPORT_SYMBOL(st_mag40_common_resume);
#endif /* CONFIG_PM */

MODULE_DESCRIPTION("STMicroelectronics st_mag40 driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_LICENSE("GPL v2");
