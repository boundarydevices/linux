/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2018 STMicroelectronics Inc.
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

#include "st_asm330lhh.h"

#define ST_ASM330LHH_REG_FIFO_CTRL3_ADDR	0x09
#define ST_ASM330LHH_REG_BDR_XL_MASK		GENMASK(3, 0)
#define ST_ASM330LHH_REG_BDR_GY_MASK		GENMASK(7, 4)

#define ST_ASM330LHH_REG_FIFO_CTRL4_ADDR	0x0a
#define ST_ASM330LHH_REG_BDR_TEMP_MASK		GENMASK(5, 4)

#define ST_ASM330LHH_REG_INT1_CTRL_ADDR		0x0d
#define ST_ASM330LHH_REG_INT2_CTRL_ADDR		0x0e
#define ST_ASM330LHH_REG_FIFO_TH_MASK		BIT(3)

#define ST_ASM330LHH_REG_WHOAMI_ADDR		0x0f
#define ST_ASM330LHH_WHOAMI_VAL			0x6b
#define ST_ASM330LHH_CTRL1_XL_ADDR		0x10
#define ST_ASM330LHH_CTRL2_G_ADDR		0x11
#define ST_ASM330LHH_REG_CTRL3_C_ADDR		0x12
#define ST_ASM330LHH_REG_SW_RESET_MASK		BIT(0)
#define ST_ASM330LHH_REG_BOOT_MASK		BIT(7)
#define ST_ASM330LHH_REG_BDU_MASK		BIT(6)

#define ST_ASM330LHH_REG_CTRL4_C_ADDR		0x13
#define ST_ASM330LHH_REG_DRDY_MASK		BIT(3)

#define ST_ASM330LHH_REG_CTRL5_C_ADDR		0x14
#define ST_ASM330LHH_REG_ROUNDING_MASK		GENMASK(6, 5)

#define ST_ASM330LHH_REG_CTRL10_C_ADDR		0x19
#define ST_ASM330LHH_REG_TIMESTAMP_EN_MASK	BIT(5)

#define ST_ASM330LHH_REG_STATUS_ADDR		0x1e
#define ST_ASM330LHH_REG_STATUS_TDA		BIT(2)

#define ST_ASM330LHH_REG_OUT_TEMP_L_ADDR	0x20
#define ST_ASM330LHH_REG_OUT_TEMP_H_ADDR	0x21

#define ST_ASM330LHH_REG_OUTX_L_A_ADDR		0x28
#define ST_ASM330LHH_REG_OUTY_L_A_ADDR		0x2a
#define ST_ASM330LHH_REG_OUTZ_L_A_ADDR		0x2c

#define ST_ASM330LHH_REG_OUTX_L_G_ADDR		0x22
#define ST_ASM330LHH_REG_OUTY_L_G_ADDR		0x24
#define ST_ASM330LHH_REG_OUTZ_L_G_ADDR		0x26

#define ST_ASM330LHH_REG_TAP_CFG0_ADDR		0x56
#define ST_ASM330LHH_REG_LIR_MASK		BIT(0)

#define ST_ASM330LHH_REG_MD1_CFG_ADDR		0x5e
#define ST_ASM330LHH_REG_MD2_CFG_ADDR		0x5f
#define ST_ASM330LHH_REG_INT2_TIMESTAMP_MASK	BIT(0)
#define ST_ASM330LHH_REG_INT_EMB_FUNC_MASK	BIT(1)

#define ST_ASM330LHH_INTERNAL_FREQ_FINE		0x63

/* Temperature in uC */
#define ST_ASM330LHH_TEMP_GAIN			256
#define ST_ASM330LHH_TEMP_FS_GAIN		(1000000 / ST_ASM330LHH_TEMP_GAIN)
#define ST_ASM330LHH_TEMP_OFFSET		(6400)

static const struct st_asm330lhh_odr_table_entry st_asm330lhh_odr_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.reg = {
			.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  13, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
		.odr_avl[7] = { 833, 0x07 },
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.reg = {
			.addr = ST_ASM330LHH_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  13, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
		.odr_avl[7] = { 833, 0x07 },
	},
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	[ST_ASM330LHH_ID_TEMP] = {
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {   2, 0x01 },
		.odr_avl[2] = {  13, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
	},
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
};

static const struct st_asm330lhh_fs_table_entry st_asm330lhh_fs_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.size = ST_ASM330LHH_FS_ACC_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_2G_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_4G_GAIN,
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_8G_GAIN,
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_16G_GAIN,
			.val = 0x1,
		},
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.size = ST_ASM330LHH_FS_GYRO_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_250_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_500_GAIN,
			.val = 0x4,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_1000_GAIN,
			.val = 0x8,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_2000_GAIN,
			.val = 0x0C,
		},
		.fs_avl[4] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_4000_GAIN,
			.val = 0x1,
		},
	},
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	[ST_ASM330LHH_ID_TEMP] = {
		.size = ST_ASM330LHH_FS_TEMP_LIST_SIZE,
		.fs_avl[0] = {
			.reg = { 0 },
			.gain = ST_ASM330LHH_TEMP_FS_GAIN,
			.val = 0x0
		},
	},
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
};

static const struct iio_chan_spec st_asm330lhh_acc_channels[] = {
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTX_L_A_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTY_L_A_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTZ_L_A_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_EVENT_CHANNEL(IIO_ACCEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhh_gyro_channels[] = {
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTX_L_G_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTY_L_G_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTZ_L_G_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_EVENT_CHANNEL(IIO_ANGL_VEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
static const struct iio_chan_spec st_asm330lhh_temp_channels[] = {
	{
		.type = IIO_TEMP,
		.address = ST_ASM330LHH_REG_OUT_TEMP_L_ADDR,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		}
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */

int __st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val)
{
	u8 data, old_data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(old_data), &old_data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	/* avoid to write same value */
	data = (old_data & ~mask) | ((val << __ffs(mask)) & mask);
	if (old_data == data)
		goto out;

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

out:
	mutex_unlock(&hw->lock);

	return (err < 0) ? err : 0;
}

#ifdef CONFIG_DEBUG_FS
static int st_asm330lhh_reg_access(struct iio_dev *iio_dev,
				 unsigned int reg, unsigned int writeval,
				 unsigned int *readval)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int ret;

	mutex_lock(&iio_dev->mlock);
	if (readval == NULL) {
		ret = sensor->hw->tf->write(sensor->hw->dev, reg, 1,
					    (u8 *)&writeval);
	} else {
		ret = sensor->hw->tf->read(sensor->hw->dev, reg, 1,
					   (u8 *)readval);
	}
	mutex_unlock(&iio_dev->mlock);

	return (ret < 0) ? ret : 0;
}
#endif /* CONFIG_DEBUG_FS */

static int st_asm330lhh_check_whoami(struct st_asm330lhh_hw *hw)
{
	int err;
	u8 data;

	err = hw->tf->read(hw->dev, ST_ASM330LHH_REG_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_ASM330LHH_WHOAMI_VAL) {
		dev_err(hw->dev, "unsupported whoami [%02x]\n", data);
		return -ENODEV;
	}

	return err;
}

static int st_asm330lhh_get_odr_calibration(struct st_asm330lhh_hw *hw)
{
	int err;
	s8 data;

	err = hw->tf->read(hw->dev,
			   ST_ASM330LHH_INTERNAL_FREQ_FINE,
			   sizeof(data), (u8 *)&data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %d register\n",
				ST_ASM330LHH_INTERNAL_FREQ_FINE);
		return err;
	}

	hw->odr_calib = (data * 15) / 10000;

	return 0;
}

static int st_asm330lhh_set_full_scale(struct st_asm330lhh_sensor *sensor,
				     u32 gain)
{
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, err;
	u8 val;

	for (i = 0; i < st_asm330lhh_fs_table[id].size; i++)
		if (st_asm330lhh_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_asm330lhh_fs_table[id].size)
		return -EINVAL;

	val = st_asm330lhh_fs_table[id].fs_avl[i].val;
	err = st_asm330lhh_write_with_mask(sensor->hw,
				st_asm330lhh_fs_table[id].fs_avl[i].reg.addr,
				st_asm330lhh_fs_table[id].fs_avl[i].reg.mask,
				val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

int st_asm330lhh_get_odr_val(enum st_asm330lhh_sensor_id id, u16 odr, u8 *val)
{
	int i;

	for (i = 0; i < ST_ASM330LHH_ODR_LIST_SIZE; i++)
		if (st_asm330lhh_odr_table[id].odr_avl[i].hz >= odr)
			break;

	if (i == ST_ASM330LHH_ODR_LIST_SIZE)
		return -EINVAL;

	*val = st_asm330lhh_odr_table[id].odr_avl[i].val;

	return 0;
}

static u16 st_asm330lhh_check_odr_dependency(struct st_asm330lhh_hw *hw,
					   u16 odr,
					   enum st_asm330lhh_sensor_id ref_id)
{
	struct st_asm330lhh_sensor *ref = iio_priv(hw->iio_devs[ref_id]);
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

static int st_asm330lhh_set_odr(struct st_asm330lhh_sensor *sensor, u16 req_odr)
{
	struct st_asm330lhh_hw *hw = sensor->hw;
	enum st_asm330lhh_sensor_id id = sensor->id;
	int err;
	u8 val = 0;

	switch (id) {
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	case ST_ASM330LHH_ID_TEMP:
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
	case ST_ASM330LHH_ID_ACC: {
		u16 odr;
		int i;

		id = ST_ASM330LHH_ID_ACC;
		for (i = ST_ASM330LHH_ID_ACC; i < ST_ASM330LHH_ID_MAX; i++) {
			if (!hw->iio_devs[i])
				continue;

			if (i == sensor->id)
				continue;

			odr = st_asm330lhh_check_odr_dependency(hw, req_odr, i);
			if (odr != req_odr) {
				/* device already configured */
				return 0;
			}
		}
		break;
	}
	default:
		break;
	}

	err = st_asm330lhh_get_odr_val(id, req_odr, &val);
	if (err < 0)
		return err;

	err = st_asm330lhh_write_with_mask(hw,
					   st_asm330lhh_odr_table[id].reg.addr,
					   st_asm330lhh_odr_table[id].reg.mask,
					   val);
	if (err < 0)
		return err;

	return 0;
}

int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				 bool enable)
{
	u16 odr = enable ? sensor->odr : 0;
	int err;

	err = st_asm330lhh_set_odr(sensor, odr);
	if (err < 0)
		return err;

	if (enable)
		sensor->hw->enable_mask |= BIT(sensor->id);
	else
		sensor->hw->enable_mask &= ~BIT(sensor->id);

	return 0;
}

static int st_asm330lhh_read_oneshot(struct st_asm330lhh_sensor *sensor,
				   u8 addr, int *val)
{
	int err, delay;
	__le16 data;

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	if (sensor->id == ST_ASM330LHH_ID_TEMP) {
		u8 status;

		mutex_lock(&sensor->hw->fifo_lock);
		err = sensor->hw->tf->read(sensor->hw->dev,
					   ST_ASM330LHH_REG_STATUS_ADDR,
					   sizeof(status), &status);
		if (err < 0)
			goto unlock;

		if (status & ST_ASM330LHH_REG_STATUS_TDA) {
			err = sensor->hw->tf->read(sensor->hw->dev,
						   addr, sizeof(data),
						   (u8 *)&data);
			if (err < 0)
				goto unlock;

			sensor->old_data = data;
		} else {
			data = sensor->old_data;
		}
unlock:
		mutex_unlock(&sensor->hw->fifo_lock);

	} else {
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
		err = st_asm330lhh_sensor_set_enable(sensor, true);
		if (err < 0)
			return err;

		/* Use big delay for data valid because of drdy mask enabled */
		delay = 10000000 / sensor->odr;
		usleep_range(delay, 2 * delay);

		err = st_asm330lhh_read_atomic(sensor->hw, addr, sizeof(data),
					       (u8 *)&data);
		if (err < 0)
			return err;

		st_asm330lhh_sensor_set_enable(sensor, false);
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	}
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */

	*val = (s16)le16_to_cpu(data);

	return IIO_VAL_INT;
}

static int st_asm330lhh_read_raw(struct iio_dev *iio_dev,
			       struct iio_chan_spec const *ch,
			       int *val, int *val2, long mask)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&iio_dev->mlock);
		if (iio_buffer_enabled(iio_dev)) {
			ret = -EBUSY;
			mutex_unlock(&iio_dev->mlock);
			break;
		}
		ret = st_asm330lhh_read_oneshot(sensor, ch->address, val);
		mutex_unlock(&iio_dev->mlock);
		break;
	case IIO_CHAN_INFO_OFFSET:
		switch (ch->type) {
		case IIO_TEMP:
			*val = sensor->offset;
			ret = IIO_VAL_INT;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = sensor->odr;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (ch->type) {
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
		case IIO_TEMP:
			*val = 1;
			*val2 = ST_ASM330LHH_TEMP_GAIN;
			ret = IIO_VAL_FRACTIONAL;
			break;
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
		case IIO_ACCEL:
		case IIO_ANGL_VEL:
			*val = 0;
			*val2 = sensor->gain;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int st_asm330lhh_write_raw(struct iio_dev *iio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_asm330lhh_set_full_scale(sensor, val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		u8 data;

		err = st_asm330lhh_get_odr_val(sensor->id, val, &data);
		if (!err) {
			sensor->odr = val;

			/*
			 * VTS test testSamplingRateHotSwitchOperation not
			 * toggle the enable status of sensor after changing
			 * the ODR -> force it
			 */
			if (sensor->hw->enable_mask & BIT(sensor->id)) {
				switch (sensor->id) {
				case ST_ASM330LHH_ID_GYRO:
				case ST_ASM330LHH_ID_ACC: {
					err = st_asm330lhh_set_odr(sensor,
								   sensor->odr);
					if (err < 0)
						break;

					st_asm330lhh_update_batching(iio_dev, 1);
					}
					break;
				default:
					break;
				}
			}
		}
		break;
	}
	default:
		err = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return err;
}

static ssize_t
st_asm330lhh_sysfs_sampling_freq_avail(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < ST_ASM330LHH_ODR_LIST_SIZE; i++) {
		if (!st_asm330lhh_odr_table[id].odr_avl[i].hz)
			continue;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_asm330lhh_odr_table[id].odr_avl[i].hz);
	}

	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_asm330lhh_sysfs_scale_avail(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_asm330lhh_fs_table[id].size; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_asm330lhh_fs_table[id].fs_avl[i].gain);
	buf[len - 1] = '\n';

	return len;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_asm330lhh_sysfs_sampling_freq_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_asm330lhh_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(in_temp_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_asm330lhh_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_asm330lhh_get_watermark,
		       st_asm330lhh_set_watermark, 0);

static struct attribute *st_asm330lhh_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_acc_attribute_group = {
	.attrs = st_asm330lhh_acc_attributes,
};

static const struct iio_info st_asm330lhh_acc_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_asm330lhh_acc_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
#ifdef CONFIG_DEBUG_FS
	.debugfs_reg_access = &st_asm330lhh_reg_access,
#endif /* CONFIG_DEBUG_FS */
};

static struct attribute *st_asm330lhh_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_gyro_attribute_group = {
	.attrs = st_asm330lhh_gyro_attributes,
};

static const struct iio_info st_asm330lhh_gyro_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_asm330lhh_gyro_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static struct attribute *st_asm330lhh_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_temp_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_temp_attribute_group = {
	.attrs = st_asm330lhh_temp_attributes,
};

static const struct iio_info st_asm330lhh_temp_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_asm330lhh_temp_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static const unsigned long st_asm330lhh_available_scan_masks[] = { 0x7, 0x0 };

static int st_asm330lhh_of_get_pin(struct st_asm330lhh_hw *hw, int *pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32(np, "st,int-pin", pin);
}

static int st_asm330lhh_get_int_reg(struct st_asm330lhh_hw *hw, u8 *drdy_reg)
{
	int err = 0, int_pin;

	if (st_asm330lhh_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (int_pin) {
	case 1:
		*drdy_reg = ST_ASM330LHH_REG_INT1_CTRL_ADDR;
		break;
	case 2:
		*drdy_reg = ST_ASM330LHH_REG_INT2_CTRL_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported interrupt pin\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_asm330lhh_reset_device(struct st_asm330lhh_hw *hw)
{
	int err;

	/* sw reset */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_SW_RESET_MASK, 1);
	if (err < 0)
		return err;

	msleep(50);

	/* boot */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_BOOT_MASK, 1);

	msleep(50);

	return err;
}

static int st_asm330lhh_init_timestamp_engine(struct st_asm330lhh_hw *hw,
					    bool enable)
{
	int err;

	/* Init timestamp engine. */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL10_C_ADDR,
					   ST_ASM330LHH_REG_TIMESTAMP_EN_MASK,
					   enable);
	if (err < 0)
		return err;

	/* Enable timestamp rollover interrupt on INT2. */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_MD2_CFG_ADDR,
					   ST_ASM330LHH_REG_INT2_TIMESTAMP_MASK,
					   enable);

	return err;
}

static int st_asm330lhh_init_device(struct st_asm330lhh_hw *hw)
{
	u8 drdy_reg;
	int err;

	/* latch interrupts */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_TAP_CFG0_ADDR,
					 ST_ASM330LHH_REG_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_BDU_MASK, 1);
	if (err < 0)
		return err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL5_C_ADDR,
					 ST_ASM330LHH_REG_ROUNDING_MASK, 3);
	if (err < 0)
		return err;

	err = st_asm330lhh_init_timestamp_engine(hw, true);
	if (err < 0)
		return err;

	err = st_asm330lhh_get_int_reg(hw, &drdy_reg);
	if (err < 0)
		return err;

	/* Enable DRDY MASK for filters settling time */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL4_C_ADDR,
					 ST_ASM330LHH_REG_DRDY_MASK, 1);
	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	return st_asm330lhh_write_with_mask(hw, drdy_reg,
					 ST_ASM330LHH_REG_FIFO_TH_MASK, 1);
}

static struct iio_dev *st_asm330lhh_alloc_iiodev(struct st_asm330lhh_hw *hw,
					       enum st_asm330lhh_sensor_id id)
{
	struct st_asm330lhh_sensor *sensor;
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
	case ST_ASM330LHH_ID_ACC:
		iio_dev->channels = st_asm330lhh_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_acc_channels);
		iio_dev->name = "asm330lhh_accel";
		iio_dev->info = &st_asm330lhh_acc_info;
		iio_dev->available_scan_masks =
					st_asm330lhh_available_scan_masks;

		sensor->batch_reg.addr = ST_ASM330LHH_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_ASM330LHH_REG_BDR_XL_MASK;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[1].hz;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[0].gain;
		sensor->offset = 0;
		break;
	case ST_ASM330LHH_ID_GYRO:
		iio_dev->channels = st_asm330lhh_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_gyro_channels);
		iio_dev->name = "asm330lhh_gyro";
		iio_dev->info = &st_asm330lhh_gyro_info;
		iio_dev->available_scan_masks =
					st_asm330lhh_available_scan_masks;

		sensor->batch_reg.addr = ST_ASM330LHH_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_ASM330LHH_REG_BDR_GY_MASK;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[1].hz;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[0].gain;
		sensor->offset = 0;
		break;
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE
	case ST_ASM330LHH_ID_TEMP:
		iio_dev->channels = st_asm330lhh_temp_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_temp_channels);
		iio_dev->name = "asm330lhh_temp";
		iio_dev->info = &st_asm330lhh_temp_info;

		sensor->batch_reg.addr = ST_ASM330LHH_REG_FIFO_CTRL4_ADDR;
		sensor->batch_reg.mask = ST_ASM330LHH_REG_BDR_TEMP_MASK;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[1].hz;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[0].gain;
		sensor->offset = ST_ASM330LHH_TEMP_OFFSET;
		break;
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE */
	default:
		return NULL;
	}

	return iio_dev;
}

int st_asm330lhh_probe(struct device *dev, int irq,
		     const struct st_asm330lhh_transfer_function *tf_ops)
{
	struct st_asm330lhh_hw *hw;
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

	err = st_asm330lhh_check_whoami(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_get_odr_calibration(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_reset_device(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_init_device(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		hw->iio_devs[i] = st_asm330lhh_alloc_iiodev(hw, i);
		if (!hw->iio_devs[i])
			return -ENOMEM;
	}

	if (hw->irq > 0) {
		err = st_asm330lhh_buffers_setup(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP)
	err = device_init_wakeup(dev, 1);
	if (err)
		return err;
#endif /* CONFIG_PM && CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */

#ifdef TIMESTAMP_HW
	hw->delta_hw_ts = 0ull;
#endif /* TIMESTAMP_HW */

	dev_info(dev, "Device probed\n");

	return 0;
}
EXPORT_SYMBOL(st_asm330lhh_probe);

static int __maybe_unused st_asm330lhh_suspend(struct device *dev)
{
	struct st_asm330lhh_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor;
	int i, err = 0;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhh_set_odr(sensor, 0);
		if (err < 0)
			return err;
	}

	if (st_asm330lhh_is_fifo_enabled(hw))
		err = st_asm330lhh_suspend_fifo(hw);
#ifdef CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */
	dev_info(dev, "Suspending device\n");

	return err;
}

static int __maybe_unused st_asm330lhh_resume(struct device *dev)
{
	struct st_asm330lhh_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor;
	int i, err = 0;

	dev_info(dev, "Resuming device\n");
#ifdef CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhh_set_odr(sensor, sensor->odr);
		if (err < 0)
			return err;
	}

	if (st_asm330lhh_is_fifo_enabled(hw))
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_CONT);

	return err;
}

const struct dev_pm_ops st_asm330lhh_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_asm330lhh_suspend, st_asm330lhh_resume)
};
EXPORT_SYMBOL(st_asm330lhh_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_asm330lhh driver");
MODULE_LICENSE("GPL v2");
