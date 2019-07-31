/*
 * STMicroelectronics st_lsm6dsr sensor driver
 *
 * Copyright 2019 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <linux/platform_data/st_sensors_pdata.h>

#include "st_lsm6dsr.h"

#define ST_LSM6DSR_REG_INT1_CTRL_ADDR		0x0d
#define ST_LSM6DSR_REG_INT2_CTRL_ADDR		0x0e
#define ST_LSM6DSR_REG_FIFO_TH_MASK		BIT(3)
#define ST_LSM6DSR_REG_WHOAMI_ADDR		0x0f
#define ST_LSM6DSR_WHOAMI_VAL			0x6b
#define ST_LSM6DSR_CTRL1_XL_ADDR		0x10
#define ST_LSM6DSR_CTRL2_G_ADDR			0x11
#define ST_LSM6DSR_REG_CTRL3_C_ADDR		0x12
#define ST_LSM6DSR_REG_SW_RESET_MASK		BIT(0)
#define ST_LSM6DSR_REG_BOOT_MASK		BIT(7)
#define ST_LSM6DSR_REG_BDU_MASK			BIT(6)
#define ST_LSM6DSR_REG_CTRL5_C_ADDR		0x14
#define ST_LSM6DSR_REG_ROUNDING_MASK		GENMASK(6, 5)
#define ST_LSM6DSR_REG_CTRL10_C_ADDR		0x19
#define ST_LSM6DSR_REG_TIMESTAMP_EN_MASK	BIT(5)

#define ST_LSM6DSR_REG_OUTX_L_A_ADDR		0x28
#define ST_LSM6DSR_REG_OUTY_L_A_ADDR		0x2a
#define ST_LSM6DSR_REG_OUTZ_L_A_ADDR		0x2c

#define ST_LSM6DSR_REG_OUTX_L_G_ADDR		0x22
#define ST_LSM6DSR_REG_OUTY_L_G_ADDR		0x24
#define ST_LSM6DSR_REG_OUTZ_L_G_ADDR		0x26

#define ST_LSM6DSR_REG_TAP_CFG0_ADDR		0x56
#define ST_LSM6DSR_REG_LIR_MASK			BIT(0)

#define ST_LSM6DSR_REG_FIFO_CTRL3_ADDR		0x09
#define ST_LSM6DSR_REG_BDR_XL_MASK		GENMASK(3, 0)
#define ST_LSM6DSR_REG_BDR_GY_MASK		GENMASK(7, 4)

#define ST_LSM6DSR_REG_MD1_CFG_ADDR		0x5e
#define ST_LSM6DSR_REG_MD2_CFG_ADDR		0x5f
#define ST_LSM6DSR_REG_INT2_TIMESTAMP_MASK	BIT(0)
#define ST_LSM6DSR_REG_INT_EMB_FUNC_MASK	BIT(1)
#define ST_LSM6DSR_INTERNAL_FREQ_FINE		0x63

/* Embedded function */
#define ST_LSM6DSR_REG_EMB_FUNC_INT1_ADDR	0x0a
#define ST_LSM6DSR_REG_EMB_FUNC_INT2_ADDR	0x0e

struct st_lsm6dsr_std_entry {
	u16 odr;
	u8 val;
};

struct st_lsm6dsr_std_entry st_lsm6dsr_std_table[] = {
	{  12, 7 },
	{  26, 7 },
	{  52, 8 },
	{ 104, 8 },
	{ 208, 8 },
	{ 416, 8 },
	{ 833, 8 },
};

static const struct st_lsm6dsr_odr_table_entry st_lsm6dsr_odr_table[] = {
	[ST_LSM6DSR_ID_ACC] = {
		.reg = {
			.addr = ST_LSM6DSR_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  12, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
		.odr_avl[7] = { 833, 0x07 },
	},
	[ST_LSM6DSR_ID_GYRO] = {
		.reg = {
			.addr = ST_LSM6DSR_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  12, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
		.odr_avl[7] = { 833, 0x07 },
	}
};

static const struct st_lsm6dsr_fs_table_entry st_lsm6dsr_fs_table[] = {
	[ST_LSM6DSR_ID_ACC] = {
		.size = ST_LSM6DSR_FS_ACC_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSR_ACC_FS_2G_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSR_ACC_FS_4G_GAIN,
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSR_ACC_FS_8G_GAIN,
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSR_ACC_FS_16G_GAIN,
			.val = 0x1,
		},
	},
	[ST_LSM6DSR_ID_GYRO] = {
		.size = ST_LSM6DSR_FS_GYRO_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSR_GYRO_FS_250_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSR_GYRO_FS_500_GAIN,
			.val = 0x4,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSR_GYRO_FS_1000_GAIN,
			.val = 0x8,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSR_GYRO_FS_2000_GAIN,
			.val = 0x0C,
		},
		.fs_avl[4] = {
			.reg = {
				.addr = ST_LSM6DSR_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSR_GYRO_FS_4000_GAIN,
			.val = 0x1,
		},
	}
};

static const struct iio_chan_spec st_lsm6dsr_acc_channels[] = {
	ST_LSM6DSR_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSR_REG_OUTX_L_A_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_LSM6DSR_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSR_REG_OUTY_L_A_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_LSM6DSR_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSR_REG_OUTZ_L_A_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_LSM6DSR_EVENT_CHANNEL(IIO_ACCEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_lsm6dsr_gyro_channels[] = {
	ST_LSM6DSR_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSR_REG_OUTX_L_G_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_LSM6DSR_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSR_REG_OUTY_L_G_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_LSM6DSR_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSR_REG_OUTZ_L_G_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_LSM6DSR_EVENT_CHANNEL(IIO_ANGL_VEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_lsm6dsr_step_counter_channels[] = {
	{
		.type = IIO_STEP_COUNTER,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
	ST_LSM6DSR_EVENT_CHANNEL(IIO_STEP_COUNTER, flush),
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static const struct iio_chan_spec st_lsm6dsr_step_detector_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_STEP_DETECTOR, thr),
};

static const struct iio_chan_spec st_lsm6dsr_sign_motion_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_SIGN_MOTION, thr),
};

static const struct iio_chan_spec st_lsm6dsr_tilt_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_TILT, thr),
};

static const struct iio_chan_spec st_lsm6dsr_glance_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

static const struct iio_chan_spec st_lsm6dsr_motion_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

static const struct iio_chan_spec st_lsm6dsr_no_motion_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

static const struct iio_chan_spec st_lsm6dsr_wakeup_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

static const struct iio_chan_spec st_lsm6dsr_pickup_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

static const struct iio_chan_spec st_lsm6dsr_orientation_channels[] = {
	{
		.type = IIO_GESTURE,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = 8,
			.storagebits = 8,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static const struct iio_chan_spec st_lsm6dsr_wrist_channels[] = {
	ST_LSM6DSR_EVENT_CHANNEL(IIO_GESTURE, thr),
};

int __st_lsm6dsr_write_with_mask(struct st_lsm6dsr_hw *hw, u8 addr, u8 mask,
				 u8 val)
{
	u8 data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

out:
	mutex_unlock(&hw->lock);

	return err;
}

static int st_lsm6dsr_check_whoami(struct st_lsm6dsr_hw *hw)
{
	int err;
	u8 data;

	err = hw->tf->read(hw->dev, ST_LSM6DSR_REG_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_LSM6DSR_WHOAMI_VAL) {
		dev_err(hw->dev, "unsupported whoami [%02x]\n", data);
		return -ENODEV;
	}

	return 0;
}

static int st_lsm6dsr_get_odr_calibration(struct st_lsm6dsr_hw *hw)
{
	int err;
	s8 data;

	err = hw->tf->read(hw->dev, ST_LSM6DSR_INTERNAL_FREQ_FINE, sizeof(data),
			   (u8 *)&data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %d register\n",
				ST_LSM6DSR_INTERNAL_FREQ_FINE);
		return err;
	}

	hw->odr_calib = (data * 15) / 10000;
	dev_info(hw->dev, "ODR Calibration Factor %d\n", hw->odr_calib);

	return 0;
}

static int st_lsm6dsr_set_full_scale(struct st_lsm6dsr_sensor *sensor,
				     u32 gain)
{
	enum st_lsm6dsr_sensor_id id = sensor->id;
	int i, err;
	u8 val;

	for (i = 0; i < st_lsm6dsr_fs_table[id].size; i++)
		if (st_lsm6dsr_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_lsm6dsr_fs_table[id].size)
		return -EINVAL;

	val = st_lsm6dsr_fs_table[id].fs_avl[i].val;
	err = st_lsm6dsr_write_with_mask(sensor->hw,
					 st_lsm6dsr_fs_table[id].fs_avl[i].reg.addr,
					 st_lsm6dsr_fs_table[id].fs_avl[i].reg.mask,
					 val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

int st_lsm6dsr_get_odr_val(enum st_lsm6dsr_sensor_id id, u16 odr, u8 *val)
{
	int i;

	for (i = 0; i < ST_LSM6DSR_ODR_LIST_SIZE; i++)
		if (st_lsm6dsr_odr_table[id].odr_avl[i].hz >= odr)
			break;

	if (i == ST_LSM6DSR_ODR_LIST_SIZE)
		return -EINVAL;

	*val = st_lsm6dsr_odr_table[id].odr_avl[i].val;

	return 0;
}

static int st_lsm6dsr_set_std_level(struct st_lsm6dsr_sensor *sensor, u16 odr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(st_lsm6dsr_std_table); i++)
		if (st_lsm6dsr_std_table[i].odr == odr)
			break;

	if (i == ARRAY_SIZE(st_lsm6dsr_std_table))
		return -EINVAL;

	sensor->std_level = st_lsm6dsr_std_table[i].val;
	sensor->std_samples = 0;

	return 0;
}

static u16 st_lsm6dsr_check_odr_dependency(struct st_lsm6dsr_hw *hw, u16 odr,
					   enum st_lsm6dsr_sensor_id ref_id)
{
	struct st_lsm6dsr_sensor *ref = iio_priv(hw->iio_devs[ref_id]);
	bool enable = odr > 0;
	u16 ret;

	if (enable) {
		if (hw->enable_mask & BIT(ref_id))
			ret = max_t(u16, ref->odr, odr);
		else
			ret = odr;
	} else {
		ret = (hw->enable_mask & BIT(ref_id)) ? ref->odr : 0;
	}

	return ret;
}

static int st_lsm6dsr_set_odr(struct st_lsm6dsr_sensor *sensor, u16 req_odr)
{
	struct st_lsm6dsr_hw *hw = sensor->hw;
	enum st_lsm6dsr_sensor_id id = sensor->id;
	int err;
	u8 val;

	switch (sensor->id) {
	case ST_LSM6DSR_ID_STEP_COUNTER:
	case ST_LSM6DSR_ID_STEP_DETECTOR:
	case ST_LSM6DSR_ID_SIGN_MOTION:
	case ST_LSM6DSR_ID_NO_MOTION:
	case ST_LSM6DSR_ID_MOTION:
	case ST_LSM6DSR_ID_GLANCE:
	case ST_LSM6DSR_ID_WAKEUP:
	case ST_LSM6DSR_ID_PICKUP:
	case ST_LSM6DSR_ID_ORIENTATION:
	case ST_LSM6DSR_ID_WRIST_TILT:
	case ST_LSM6DSR_ID_TILT:
	case ST_LSM6DSR_ID_EXT0:
	case ST_LSM6DSR_ID_EXT1:
	case ST_LSM6DSR_ID_ACC: {
		u16 odr;
		int i;

		id = ST_LSM6DSR_ID_ACC;
		for (i = ST_LSM6DSR_ID_ACC; i <= ST_LSM6DSR_ID_TILT; i++) {
			if (!hw->iio_devs[i])
				continue;

			if (i == sensor->id)
				continue;

			odr = st_lsm6dsr_check_odr_dependency(hw, req_odr, i);
			if (odr != req_odr)
				/* device already configured */
				return 0;
		}
		break;
	}
	default:
		break;
	}

	err = st_lsm6dsr_get_odr_val(id, req_odr, &val);
	if (err < 0)
		return err;

	return st_lsm6dsr_write_with_mask(hw, st_lsm6dsr_odr_table[id].reg.addr,
					  st_lsm6dsr_odr_table[id].reg.mask,
					  val);
}

int st_lsm6dsr_sensor_set_enable(struct st_lsm6dsr_sensor *sensor,
				 bool enable)
{
	u16 odr = enable ? sensor->odr : 0;
	int err;

	err = st_lsm6dsr_set_odr(sensor, odr);
	if (err < 0)
		return err;

	if (enable)
		sensor->hw->enable_mask |= BIT(sensor->id);
	else
		sensor->hw->enable_mask &= ~BIT(sensor->id);

	return 0;
}

static int st_lsm6dsr_read_oneshot(struct st_lsm6dsr_sensor *sensor,
				   u8 addr, int *val)
{
	int err, delay;
	__le16 data;

	err = st_lsm6dsr_sensor_set_enable(sensor, true);
	if (err < 0)
		return err;

	delay = 1000000 / sensor->odr;
	usleep_range(delay, 2 * delay);

	err = st_lsm6dsr_read_atomic(sensor->hw, addr, sizeof(data),
				     (u8 *)&data);
	if (err < 0)
		return err;

	st_lsm6dsr_sensor_set_enable(sensor, false);

	*val = (s16)le16_to_cpu(data);

	return IIO_VAL_INT;
}

static int st_lsm6dsr_read_raw(struct iio_dev *iio_dev,
			       struct iio_chan_spec const *ch,
			       int *val, int *val2, long mask)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(iio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&iio_dev->mlock);
		if (iio_buffer_enabled(iio_dev)) {
			ret = -EBUSY;
			mutex_unlock(&iio_dev->mlock);
			break;
		}
		ret = st_lsm6dsr_read_oneshot(sensor, ch->address, val);
		mutex_unlock(&iio_dev->mlock);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = sensor->odr;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = sensor->gain;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int st_lsm6dsr_write_raw(struct iio_dev *iio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_lsm6dsr_set_full_scale(sensor, val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		u8 data;

		err = st_lsm6dsr_set_std_level(sensor, val);
		if (err < 0)
			break;

		err = st_lsm6dsr_get_odr_val(sensor->id, val, &data);
		if (!err)
			sensor->odr = val;
		break;
	}
	default:
		err = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return err;
}

static int st_lsm6dsr_read_event_config(struct iio_dev *iio_dev,
					const struct iio_chan_spec *chan,
					enum iio_event_type type,
					enum iio_event_direction dir)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(iio_dev);
	struct st_lsm6dsr_hw *hw = sensor->hw;

	return !!(hw->enable_mask & BIT(sensor->id));
}

static int st_lsm6dsr_write_event_config(struct iio_dev *iio_dev,
					 const struct iio_chan_spec *chan,
					 enum iio_event_type type,
					 enum iio_event_direction dir,
					 int state)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);
	err = st_lsm6dsr_embfunc_sensor_set_enable(sensor, state);
	mutex_unlock(&iio_dev->mlock);

	return err;
}

static ssize_t
st_lsm6dsr_sysfs_sampling_frequency_avail(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_lsm6dsr_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < ST_LSM6DSR_ODR_LIST_SIZE; i++) {
		if (!st_lsm6dsr_odr_table[id].odr_avl[i].hz)
			continue;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_lsm6dsr_odr_table[id].odr_avl[i].hz);
	}

	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_lsm6dsr_sysfs_scale_avail(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct st_lsm6dsr_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_lsm6dsr_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_lsm6dsr_fs_table[id].size; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_lsm6dsr_fs_table[id].fs_avl[i].gain);
	buf[len - 1] = '\n';

	return len;
}

static ssize_t
st_lsm6dsr_sysfs_reset_step_counter(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	int err;

	err = st_lsm6dsr_reset_step_counter(iio_dev);

	return err < 0 ? err : size;
}
static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_lsm6dsr_sysfs_sampling_frequency_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_lsm6dsr_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_lsm6dsr_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_lsm6dsr_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_lsm6dsr_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_lsm6dsr_get_watermark,
		       st_lsm6dsr_set_watermark, 0);
static IIO_DEVICE_ATTR(reset_counter, 0200, NULL,
		       st_lsm6dsr_sysfs_reset_step_counter, 0);

static struct attribute *st_lsm6dsr_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dsr_acc_attribute_group = {
	.attrs = st_lsm6dsr_acc_attributes,
};

static const struct iio_info st_lsm6dsr_acc_info = {
	.attrs = &st_lsm6dsr_acc_attribute_group,
	.read_raw = st_lsm6dsr_read_raw,
	.write_raw = st_lsm6dsr_write_raw,
};

static struct attribute *st_lsm6dsr_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dsr_gyro_attribute_group = {
	.attrs = st_lsm6dsr_gyro_attributes,
};

static const struct iio_info st_lsm6dsr_gyro_info = {
	.attrs = &st_lsm6dsr_gyro_attribute_group,
	.read_raw = st_lsm6dsr_read_raw,
	.write_raw = st_lsm6dsr_write_raw,
};

static struct attribute *st_lsm6dsr_step_counter_attributes[] = {
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_reset_counter.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dsr_step_counter_attribute_group = {
	.attrs = st_lsm6dsr_step_counter_attributes,
};

static const struct iio_info st_lsm6dsr_step_counter_info = {
	.attrs = &st_lsm6dsr_step_counter_attribute_group,
};

static struct attribute *st_lsm6dsr_step_detector_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_step_detector_attribute_group = {
	.attrs = st_lsm6dsr_step_detector_attributes,
};

static const struct iio_info st_lsm6dsr_step_detector_info = {
	.attrs = &st_lsm6dsr_step_detector_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_sign_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_sign_motion_attribute_group = {
	.attrs = st_lsm6dsr_sign_motion_attributes,
};

static const struct iio_info st_lsm6dsr_sign_motion_info = {
	.attrs = &st_lsm6dsr_sign_motion_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_tilt_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_tilt_attribute_group = {
	.attrs = st_lsm6dsr_tilt_attributes,
};

static const struct iio_info st_lsm6dsr_tilt_info = {
	.attrs = &st_lsm6dsr_tilt_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_glance_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_glance_attribute_group = {
	.attrs = st_lsm6dsr_glance_attributes,
};

static const struct iio_info st_lsm6dsr_glance_info = {
	.attrs = &st_lsm6dsr_glance_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_motion_attribute_group = {
	.attrs = st_lsm6dsr_motion_attributes,
};

static const struct iio_info st_lsm6dsr_motion_info = {
	.attrs = &st_lsm6dsr_motion_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_no_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_no_motion_attribute_group = {
	.attrs = st_lsm6dsr_no_motion_attributes,
};

static const struct iio_info st_lsm6dsr_no_motion_info = {
	.attrs = &st_lsm6dsr_no_motion_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_wakeup_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_wakeup_attribute_group = {
	.attrs = st_lsm6dsr_wakeup_attributes,
};

static const struct iio_info st_lsm6dsr_wakeup_info = {
	.attrs = &st_lsm6dsr_wakeup_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_pickup_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_pickup_attribute_group = {
	.attrs = st_lsm6dsr_pickup_attributes,
};

static const struct iio_info st_lsm6dsr_pickup_info = {
	.attrs = &st_lsm6dsr_pickup_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static struct attribute *st_lsm6dsr_orientation_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_orientation_attribute_group = {
	.attrs = st_lsm6dsr_orientation_attributes,
};

static const struct iio_info st_lsm6dsr_orientation_info = {
	.attrs = &st_lsm6dsr_orientation_attribute_group,
};

static struct attribute *st_lsm6dsr_wrist_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dsr_wrist_attribute_group = {
	.attrs = st_lsm6dsr_wrist_attributes,
};

static const struct iio_info st_lsm6dsr_wrist_info = {
	.attrs = &st_lsm6dsr_wrist_attribute_group,
	.read_event_config = st_lsm6dsr_read_event_config,
	.write_event_config = st_lsm6dsr_write_event_config,
};

static const unsigned long st_lsm6dsr_available_scan_masks[] = { 0x7, 0x0 };
static const unsigned long st_lsm6dsr_sc_available_scan_masks[] = { 0x1, 0x0 };

static int st_lsm6dsr_of_get_pin(struct st_lsm6dsr_hw *hw, int *pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32(np, "st,int-pin", pin);
}

static int st_lsm6dsr_get_int_reg(struct st_lsm6dsr_hw *hw, u8 *drdy_reg,
				  u8 *ef_irq_reg)
{
	int err = 0, int_pin;

	if (st_lsm6dsr_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (int_pin) {
	case 1:
		hw->embfunc_irq_reg = ST_LSM6DSR_REG_EMB_FUNC_INT1_ADDR;
		*ef_irq_reg = ST_LSM6DSR_REG_MD1_CFG_ADDR;
		*drdy_reg = ST_LSM6DSR_REG_INT1_CTRL_ADDR;
		break;
	case 2:
		hw->embfunc_irq_reg = ST_LSM6DSR_REG_EMB_FUNC_INT2_ADDR;
		*ef_irq_reg = ST_LSM6DSR_REG_MD2_CFG_ADDR;
		*drdy_reg = ST_LSM6DSR_REG_INT2_CTRL_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported interrupt pin\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_lsm6dsr_reset_device(struct st_lsm6dsr_hw *hw)
{
	int err;

	/* sw reset */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_CTRL3_C_ADDR,
					 ST_LSM6DSR_REG_SW_RESET_MASK, 1);
	if (err < 0)
		return err;

	msleep(50);

	/* boot */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_CTRL3_C_ADDR,
					 ST_LSM6DSR_REG_BOOT_MASK, 1);

	msleep(50);

	return err;
}

static int st_lsm6dsr_init_timestamp_engine(struct st_lsm6dsr_hw *hw,
					    bool enable)
{
	int err;

	/* Init timestamp engine. */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_CTRL10_C_ADDR,
					 ST_LSM6DSR_REG_TIMESTAMP_EN_MASK, enable);
	if (err < 0)
		return err;

	/* Enable timestamp rollover interrupt on INT2. */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_MD2_CFG_ADDR,
					 ST_LSM6DSR_REG_INT2_TIMESTAMP_MASK, enable);

	return err;
}

static int st_lsm6dsr_init_device(struct st_lsm6dsr_hw *hw)
{
	u8 drdy_reg, ef_irq_reg;
	int err;

	/* latch interrupts */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_TAP_CFG0_ADDR,
					 ST_LSM6DSR_REG_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_CTRL3_C_ADDR,
					 ST_LSM6DSR_REG_BDU_MASK, 1);
	if (err < 0)
		return err;

	err = st_lsm6dsr_write_with_mask(hw, ST_LSM6DSR_REG_CTRL5_C_ADDR,
					 ST_LSM6DSR_REG_ROUNDING_MASK, 3);
	if (err < 0)
		return err;

	err = st_lsm6dsr_init_timestamp_engine(hw, true);
	if (err < 0)
		return err;

	err = st_lsm6dsr_get_int_reg(hw, &drdy_reg, &ef_irq_reg);
	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	err = st_lsm6dsr_write_with_mask(hw, drdy_reg,
					 ST_LSM6DSR_REG_FIFO_TH_MASK, 1);
	if (err < 0)
		return err;

	/* enable enbedded function interrupts */
	err = st_lsm6dsr_write_with_mask(hw, ef_irq_reg,
					 ST_LSM6DSR_REG_INT_EMB_FUNC_MASK, 1);
	if (err < 0)
		return err;

	/* init finite state machine */
	return st_lsm6dsr_fsm_init(hw);
}

static struct iio_dev *st_lsm6dsr_alloc_iiodev(struct st_lsm6dsr_hw *hw,
					       enum st_lsm6dsr_sensor_id id)
{
	struct st_lsm6dsr_sensor *sensor;
	struct iio_dev *iio_dev;

	iio_dev = devm_iio_device_alloc(hw->dev, sizeof(*sensor));
	if (!iio_dev)
		return NULL;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = hw->dev;

	sensor = iio_priv(iio_dev);
	sensor->id = id;
	sensor->hw = hw;
	sensor->watermark = 1;

	switch (id) {
	case ST_LSM6DSR_ID_ACC:
		iio_dev->channels = st_lsm6dsr_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_acc_channels);
		iio_dev->name = "lsm6dsr_accel";
		iio_dev->info = &st_lsm6dsr_acc_info;
		iio_dev->available_scan_masks = st_lsm6dsr_available_scan_masks;

		sensor->batch_reg.addr = ST_LSM6DSR_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_LSM6DSR_REG_BDR_XL_MASK;
		sensor->max_watermark = ST_LSM6DSR_MAX_FIFO_DEPTH;
		sensor->odr = st_lsm6dsr_odr_table[id].odr_avl[1].hz;
		sensor->gain = st_lsm6dsr_fs_table[id].fs_avl[0].gain;
		break;
	case ST_LSM6DSR_ID_GYRO:
		iio_dev->channels = st_lsm6dsr_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_gyro_channels);
		iio_dev->name = "lsm6dsr_gyro";
		iio_dev->info = &st_lsm6dsr_gyro_info;
		iio_dev->available_scan_masks = st_lsm6dsr_available_scan_masks;

		sensor->batch_reg.addr = ST_LSM6DSR_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_LSM6DSR_REG_BDR_GY_MASK;
		sensor->max_watermark = ST_LSM6DSR_MAX_FIFO_DEPTH;
		sensor->odr = st_lsm6dsr_odr_table[id].odr_avl[1].hz;
		sensor->gain = st_lsm6dsr_fs_table[id].fs_avl[0].gain;
		break;
	case ST_LSM6DSR_ID_STEP_COUNTER:
		iio_dev->channels = st_lsm6dsr_step_counter_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dsr_step_counter_channels);
		iio_dev->name = "lsm6dsr_step_c";
		iio_dev->info = &st_lsm6dsr_step_counter_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->max_watermark = 1;
		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_STEP_DETECTOR:
		iio_dev->channels = st_lsm6dsr_step_detector_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dsr_step_detector_channels);
		iio_dev->name = "lsm6dsr_step_d";
		iio_dev->info = &st_lsm6dsr_step_detector_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_SIGN_MOTION:
		iio_dev->channels = st_lsm6dsr_sign_motion_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dsr_sign_motion_channels);
		iio_dev->name = "lsm6dsr_sign_motion";
		iio_dev->info = &st_lsm6dsr_sign_motion_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_TILT:
		iio_dev->channels = st_lsm6dsr_tilt_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_tilt_channels);
		iio_dev->name = "lsm6dsr_tilt";
		iio_dev->info = &st_lsm6dsr_tilt_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_GLANCE:
		iio_dev->channels = st_lsm6dsr_glance_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_glance_channels);
		iio_dev->name = "lsm6dsr_glance";
		iio_dev->info = &st_lsm6dsr_glance_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_MOTION:
		iio_dev->channels = st_lsm6dsr_motion_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_motion_channels);
		iio_dev->name = "lsm6dsr_motion";
		iio_dev->info = &st_lsm6dsr_motion_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_NO_MOTION:
		iio_dev->channels = st_lsm6dsr_no_motion_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_no_motion_channels);
		iio_dev->name = "lsm6dsr_no_motion";
		iio_dev->info = &st_lsm6dsr_no_motion_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_WAKEUP:
		iio_dev->channels = st_lsm6dsr_wakeup_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_wakeup_channels);
		iio_dev->name = "lsm6dsr_wk";
		iio_dev->info = &st_lsm6dsr_wakeup_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_PICKUP:
		iio_dev->channels = st_lsm6dsr_pickup_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_pickup_channels);
		iio_dev->name = "lsm6dsr_pickup";
		iio_dev->info = &st_lsm6dsr_pickup_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_ORIENTATION:
		iio_dev->channels = st_lsm6dsr_orientation_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_orientation_channels);
		iio_dev->name = "lsm6dsr_dev_orientation";
		iio_dev->info = &st_lsm6dsr_orientation_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	case ST_LSM6DSR_ID_WRIST_TILT:
		iio_dev->channels = st_lsm6dsr_wrist_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dsr_wrist_channels);
		iio_dev->name = "lsm6dsr_wrist";
		iio_dev->info = &st_lsm6dsr_wrist_info;
		iio_dev->available_scan_masks = st_lsm6dsr_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dsr_odr_table[ST_LSM6DSR_ID_ACC].odr_avl[2].hz;
		break;
	default:
		return NULL;
	}

	return iio_dev;
}

int st_lsm6dsr_probe(struct device *dev, int irq,
		     const struct st_lsm6dsr_transfer_function *tf_ops)
{
	struct st_lsm6dsr_hw *hw;
	int i, err;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)hw);

	mutex_init(&hw->lock);
	mutex_init(&hw->fifo_lock);
	mutex_init(&hw->page_lock);

	hw->dev = dev;
	hw->irq = irq;
	hw->tf = tf_ops;

	err = st_lsm6dsr_check_whoami(hw);
	if (err < 0)
		return err;

	err = st_lsm6dsr_get_odr_calibration(hw);
	if (err < 0)
		return err;

	err = st_lsm6dsr_reset_device(hw);
	if (err < 0)
		return err;

	err = st_lsm6dsr_init_device(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ARRAY_SIZE(st_lsm6dsr_main_sensor_list); i++) {
		enum st_lsm6dsr_sensor_id id = st_lsm6dsr_main_sensor_list[i];

		hw->iio_devs[id] = st_lsm6dsr_alloc_iiodev(hw, id);
		if (!hw->iio_devs[id])
			return -ENOMEM;
	}

	err = st_lsm6dsr_shub_probe(hw);
	if (err < 0)
		return err;

	if (hw->irq > 0) {
		err = st_lsm6dsr_buffers_setup(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_LSM6DSR_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP)
	err = device_init_wakeup(dev, 1);
	if (err)
		return err;
#endif /* CONFIG_PM && CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP */

#ifdef TIMESTAMP_HW
	hw->delta_hw_ts = 0ull;
#endif /* TIMESTAMP_HW */

	dev_info(dev, "Device probed\n");

	return 0;
}
EXPORT_SYMBOL(st_lsm6dsr_probe);

static int __maybe_unused st_lsm6dsr_suspend(struct device *dev)
{
	struct st_lsm6dsr_hw *hw = dev_get_drvdata(dev);
	struct st_lsm6dsr_sensor *sensor;
	int i, err = 0;

	for (i = 0; i < ST_LSM6DSR_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_lsm6dsr_set_odr(sensor, 0);
		if (err < 0)
			return err;
	}

	if (st_lsm6dsr_is_fifo_enabled(hw))
		err = st_lsm6dsr_suspend_fifo(hw);
#ifdef CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP */
	dev_info(dev, "Suspending device\n");

	return err;
}

static int __maybe_unused st_lsm6dsr_resume(struct device *dev)
{
	struct st_lsm6dsr_hw *hw = dev_get_drvdata(dev);
	struct st_lsm6dsr_sensor *sensor;
	int i, err = 0;

	dev_info(dev, "Resuming device\n");
#ifdef CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_LSM6DSR_MAY_WAKEUP */

	for (i = 0; i < ST_LSM6DSR_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_lsm6dsr_set_odr(sensor, sensor->odr);
		if (err < 0)
			return err;
	}

	if (st_lsm6dsr_is_fifo_enabled(hw))
		err = st_lsm6dsr_set_fifo_mode(hw, ST_LSM6DSR_FIFO_CONT);

	return err;
}

const struct dev_pm_ops st_lsm6dsr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_lsm6dsr_suspend, st_lsm6dsr_resume)
};
EXPORT_SYMBOL(st_lsm6dsr_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_lsm6dsr driver");
MODULE_LICENSE("GPL v2");
