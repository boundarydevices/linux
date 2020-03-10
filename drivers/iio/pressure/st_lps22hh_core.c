/*
 * STMicroelectronics lps22hh driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Armando Visconti <armando.visconti@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include "st_lps22hh.h"

#define ST_LPS22HH_PRESS_FS_AVL_GAIN		(1000000000UL / 4096UL)
#define ST_LPS22HH_TEMP_FS_AVL_GAIN		(1000000000UL / 100UL)

#define ST_LPS22HH_ODR_LIST_NUM			8
struct st_lps22hh_odr_table_t {
	u8 addr;
	u8 mask;
	u8 odr_avl[ST_LPS22HH_ODR_LIST_NUM];
};

const static struct st_lps22hh_odr_table_t st_lps22hh_odr_table = {
	.addr = 0x10,
	.mask = 0x70,
	.odr_avl = { 0, 1, 10, 25, 50, 75, 100, 200 },
};

const struct iio_event_spec st_lps22hh_fifo_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

static const struct iio_chan_spec st_lps22hh_press_channels[] = {
	{
		.type = IIO_PRESSURE,
		.address = 0x28,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.channel2 = IIO_NO_MOD,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = 24,
			.storagebits = 32,
			.endianness = IIO_LE,
		},
	},
	{
		.type = IIO_PRESSURE,
		.scan_index = -1,
		.indexed = -1,
		.event_spec = &st_lps22hh_fifo_flush_event,
		.num_event_specs = 1,
	},
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static const struct iio_chan_spec st_lps22hh_temp_channels[] = {
	{
		.type = IIO_TEMP,
		.address = 0x2b,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.channel2 = IIO_NO_MOD,
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
	{
		.type = IIO_TEMP,
		.scan_index = -1,
		.indexed = -1,
		.event_spec = &st_lps22hh_fifo_flush_event,
		.num_event_specs = 1,
	},
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

int st_lps22hh_write_with_mask(struct st_lps22hh_hw *hw, u8 addr, u8 mask,
			       u8 val)
{
	int err;
	u8 data;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		goto unlock;

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);
	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
unlock:
	mutex_unlock(&hw->lock);

	return err;
}

static int st_lps22hh_check_whoami(struct st_lps22hh_hw *hw)
{
	int err;
	u8 data;

	err = hw->tf->read(hw->dev, ST_LPS22HH_WHO_AM_I_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read Who-Am-I register.\n");

		return err;
	}
	if (data != ST_LPS22HH_WHO_AM_I_DEF) {
		dev_err(hw->dev, "Who-Am-I value not valid.\n");
		return -ENODEV;
	}
	return 0;
}

static int st_lps22hh_get_odr(struct st_lps22hh_sensor *sensor, u8 odr)
{
	int i;

	for (i = 0; i < ST_LPS22HH_ODR_LIST_NUM; i++) {
		if (st_lps22hh_odr_table.odr_avl[i] == odr)
			break;
	}
	return i == ST_LPS22HH_ODR_LIST_NUM ? -EINVAL : i;
}

int st_lps22hh_set_enable(struct st_lps22hh_sensor *sensor, bool enable)
{
	struct st_lps22hh_hw *hw = sensor->hw;
	u32 max_odr = enable ? sensor->odr : 0;
	int i;

	for (i = 0; i < ST_LPS22HH_SENSORS_NUMB; i++) {
		if (sensor->type == i)
			continue;

		if (hw->enable_mask & BIT(i)) {
			struct st_lps22hh_sensor *temp;

			temp = iio_priv(hw->iio_devs[i]);
			max_odr = max_t(u32, max_odr, temp->odr);
		}
	}

	if (max_odr != hw->odr) {
		int err, ret;

		ret = st_lps22hh_get_odr(sensor, max_odr);
		if (ret < 0)
			return ret;

		err = st_lps22hh_write_with_mask(hw, st_lps22hh_odr_table.addr,
						 st_lps22hh_odr_table.mask, ret);
		if (err < 0)
			return err;

		hw->odr = max_odr;
	}

	if (enable)
		hw->enable_mask |= BIT(sensor->type);
	else
		hw->enable_mask &= ~BIT(sensor->type);

	return 0;
}

int st_lps22hh_init_sensors(struct st_lps22hh_hw *hw)
{
	int err;

	/* Reboot memory content */
	err = st_lps22hh_write_with_mask(hw, ST_LPS22HH_CTRL2_ADDR,
					 ST_LPS22HH_BOOT_MASK, 1);
	if (err < 0)
		return err;

	msleep(10);

	/* soft reset the device on power on. */
	err = st_lps22hh_write_with_mask(hw, ST_LPS22HH_CTRL2_ADDR,
					 ST_LPS22HH_SOFT_RESET_MASK, 1);
	if (err < 0)
		return err;

	msleep(200);

	/* enable low noise */
	err = st_lps22hh_write_with_mask(hw, ST_LPS22HH_CTRL2_ADDR,
					 ST_LPS22HH_LOW_NOISE_EN_MASK, 1);
	if (err < 0)
		return err;

	/* enable latched interrupt mode */
	err = st_lps22hh_write_with_mask(hw, ST_LPS22HH_LIR_ADDR,
					 ST_LPS22HH_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable BDU */
	return st_lps22hh_write_with_mask(hw, ST_LPS22HH_CTRL1_ADDR,
					  ST_LPS22HH_BDU_MASK, 1);
}

static ssize_t
st_lps22hh_get_sampling_frequency_avail(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int i, len = 0;

	for (i = 1; i < ST_LPS22HH_ODR_LIST_NUM; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_lps22hh_odr_table.odr_avl[i]);
	}
	buf[len - 1] = '\n';

	return len;
}

static ssize_t
st_lps22hh_sysfs_get_hwfifo_watermark(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct st_lps22hh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", sensor->hw->watermark);
}

static ssize_t
st_lps22hh_sysfs_get_hwfifo_watermark_min(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", 1);
}

static ssize_t
st_lps22hh_sysfs_get_hwfifo_watermark_max(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", ST_LPS22HH_MAX_FIFO_LENGTH);
}

static int st_lps22hh_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *ch,
			       int *val, int *val2, long mask)
{
	struct st_lps22hh_sensor *sensor = iio_priv(indio_dev);
	struct st_lps22hh_hw *hw = sensor->hw;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW: {
		u8 len = ch->scan_type.realbits / 8;
		u8 data[4] = {};

		mutex_lock(&indio_dev->mlock);
		if (iio_buffer_enabled(indio_dev)) {
			mutex_unlock(&indio_dev->mlock);
			ret = -EBUSY;
			break;
		}

		ret = st_lps22hh_set_enable(sensor, true);
		if (ret < 0) {
			mutex_unlock(&indio_dev->mlock);
			ret = -EBUSY;
			break;
		}

		msleep(40);
		ret = hw->tf->read(hw->dev, ch->address, len, data);
		if (ret < 0) {
			mutex_unlock(&indio_dev->mlock);
			return ret;
		}

		if (sensor->type == ST_LPS22HH_PRESS)
			*val = (s32)get_unaligned_le32(data);
		else if (sensor->type == ST_LPS22HH_TEMP)
			*val = (s16)get_unaligned_le16(data);

		ret = st_lps22hh_set_enable(sensor, false);
		mutex_unlock(&indio_dev->mlock);

		if (ret < 0)
			return ret;

		ret = IIO_VAL_INT;
		break;
	}
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = sensor->gain;
		ret = IIO_VAL_INT_PLUS_NANO;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = sensor->odr;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int st_lps22hh_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *ch,
				int val, int val2, long mask)
{
	struct st_lps22hh_sensor *sensor = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret = st_lps22hh_get_odr(sensor, val);
		if (ret > 0)
			sensor->odr = val;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_lps22hh_get_sampling_frequency_avail);
static IIO_DEVICE_ATTR(hwfifo_watermark, S_IWUSR | S_IRUGO,
		       st_lps22hh_sysfs_get_hwfifo_watermark,
		       st_lps22hh_sysfs_set_hwfifo_watermark, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_min, S_IRUGO,
		       st_lps22hh_sysfs_get_hwfifo_watermark_min, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, S_IRUGO,
		       st_lps22hh_sysfs_get_hwfifo_watermark_max, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, S_IWUSR, NULL,
		       st_lps22hh_sysfs_flush_fifo, 0);

static struct attribute *st_lps22hh_press_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_min.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static struct attribute *st_lps22hh_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_min.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lps22hh_press_attribute_group = {
	.attrs = st_lps22hh_press_attributes,
};
static const struct attribute_group st_lps22hh_temp_attribute_group = {
	.attrs = st_lps22hh_temp_attributes,
};

static const struct iio_info st_lps22hh_press_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lps22hh_press_attribute_group,
	.read_raw = st_lps22hh_read_raw,
	.write_raw = st_lps22hh_write_raw,
};

static const struct iio_info st_lps22hh_temp_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lps22hh_temp_attribute_group,
	.read_raw = st_lps22hh_read_raw,
	.write_raw = st_lps22hh_write_raw,
};

int st_lps22hh_common_probe(struct device *dev, int irq, const char *name,
			 const struct st_lps22hh_transfer_function *tf_ops)
{
	struct st_lps22hh_sensor *sensor;
	struct st_lps22hh_hw *hw;
	struct iio_dev *iio_dev;
	int err, i;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)hw);
	hw->dev = dev;
	hw->tf = tf_ops;
	hw->irq = irq;
	/* set initial watermark */
	hw->watermark = 1;

	mutex_init(&hw->lock);
	mutex_init(&hw->fifo_lock);

	err = st_lps22hh_check_whoami(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ST_LPS22HH_SENSORS_NUMB; i++) {
		iio_dev = devm_iio_device_alloc(dev, sizeof(*sensor));
		if (!iio_dev)
			return -ENOMEM;

		hw->iio_devs[i] = iio_dev;
		sensor = iio_priv(iio_dev);
		sensor->hw = hw;
		sensor->type = i;
		sensor->odr = 1;

		switch (i) {
		case ST_LPS22HH_PRESS:
			sensor->gain = ST_LPS22HH_PRESS_FS_AVL_GAIN;
			scnprintf(sensor->name, sizeof(sensor->name),
				  "%s_press", name);
			iio_dev->channels = st_lps22hh_press_channels;
			iio_dev->num_channels =
				ARRAY_SIZE(st_lps22hh_press_channels);
			iio_dev->info = &st_lps22hh_press_info;
			break;
		case ST_LPS22HH_TEMP:
			sensor->gain = ST_LPS22HH_TEMP_FS_AVL_GAIN;
			scnprintf(sensor->name, sizeof(sensor->name),
				  "%s_temp", name);
			iio_dev->channels = st_lps22hh_temp_channels;
			iio_dev->num_channels =
				ARRAY_SIZE(st_lps22hh_temp_channels);
			iio_dev->info = &st_lps22hh_temp_info;
			break;
		default:
			return -EINVAL;
		};
		iio_dev->name = sensor->name;
		iio_dev->modes = INDIO_DIRECT_MODE;
	}

	err = st_lps22hh_init_sensors(hw);
	if (err < 0)
		return err;

	if (irq > 0) {
		err = st_lps22hh_allocate_buffers(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_LPS22HH_SENSORS_NUMB; i++) {
		err = devm_iio_device_register(dev, hw->iio_devs[i]);
		if (err)
			return err;
	}
	return 0;
}
EXPORT_SYMBOL(st_lps22hh_common_probe);

MODULE_DESCRIPTION("STMicroelectronics lps22hh driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");
