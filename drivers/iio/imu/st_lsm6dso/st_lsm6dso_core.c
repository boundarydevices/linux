/*
 * STMicroelectronics st_lsm6dso sensor driver
 *
 * Copyright 2020 STMicroelectronics Inc.
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

#include "st_lsm6dso.h"

static struct st_lsm6dso_suspend_resume_entry
	st_lsm6dso_suspend_resume[ST_LSM6DSO_SUSPEND_RESUME_REGS] = {
		[ST_LSM6DSO_CTRL1_XL_REG] = {
			.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
			.mask = GENMASK(3, 2),
		},
		[ST_LSM6DSO_CTRL2_G_REG] = {
			.addr = ST_LSM6DSO_CTRL2_G_ADDR,
			.mask = GENMASK(3, 2),
		},
		[ST_LSM6DSO_REG_CTRL3_C_REG] = {
			.addr = ST_LSM6DSO_REG_CTRL3_C_ADDR,
			.mask = ST_LSM6DSO_REG_BDU_MASK       |
				ST_LSM6DSO_REG_PP_OD_MASK     |
				ST_LSM6DSO_REG_H_LACTIVE_MASK,
		},
		[ST_LSM6DSO_REG_CTRL4_C_REG] = {
			.addr = ST_LSM6DSO_REG_CTRL4_C_ADDR,
			.mask = ST_LSM6DSO_REG_DRDY_MASK,
		},
		[ST_LSM6DSO_REG_CTRL5_C_REG] = {
			.addr = ST_LSM6DSO_REG_CTRL5_C_ADDR,
			.mask = ST_LSM6DSO_REG_ROUNDING_MASK,
		},
		[ST_LSM6DSO_REG_CTRL10_C_REG] = {
			.addr = ST_LSM6DSO_REG_CTRL10_C_ADDR,
			.mask = ST_LSM6DSO_REG_TIMESTAMP_EN_MASK,
		},
		[ST_LSM6DSO_REG_TAP_CFG0_REG] = {
			.addr = ST_LSM6DSO_REG_TAP_CFG0_ADDR,
			.mask = ST_LSM6DSO_REG_LIR_MASK,
		},
		[ST_LSM6DSO_REG_INT1_CTRL_REG] = {
			.addr = ST_LSM6DSO_REG_INT1_CTRL_ADDR,
			.mask = ST_LSM6DSO_REG_INT_FIFO_TH_MASK,
		},
		[ST_LSM6DSO_REG_INT2_CTRL_REG] = {
			.addr = ST_LSM6DSO_REG_INT2_CTRL_ADDR,
			.mask = ST_LSM6DSO_REG_INT_FIFO_TH_MASK,
		},
		[ST_LSM6DSO_REG_FIFO_CTRL1_REG] = {
			.addr = ST_LSM6DSO_REG_FIFO_CTRL1_ADDR,
			.mask = GENMASK(7, 0),
		},
		[ST_LSM6DSO_REG_FIFO_CTRL2_REG] = {
			.addr = ST_LSM6DSO_REG_FIFO_CTRL2_ADDR,
			.mask = ST_LSM6DSO_REG_FIFO_WTM8_MASK,
		},
		[ST_LSM6DSO_REG_FIFO_CTRL3_REG] = {
			.addr = ST_LSM6DSO_REG_FIFO_CTRL3_ADDR,
			.mask = ST_LSM6DSO_REG_BDR_XL_MASK |
				ST_LSM6DSO_REG_BDR_GY_MASK,
		},
		[ST_LSM6DSO_REG_FIFO_CTRL4_REG] = {
			.addr = ST_LSM6DSO_REG_FIFO_CTRL4_ADDR,
			.mask = ST_LSM6DSO_REG_DEC_TS_MASK |
				ST_LSM6DSO_REG_ODR_T_BATCH_MASK,
		},
	};

/**
 * List of supported ODR
 *
 * The following table is complete list of supported ODR by Acc, Gyro and Temp
 * sensors. ODR value can be also decimal (i.e 12.5 Hz)
 */
static const struct st_lsm6dso_odr_table_entry st_lsm6dso_odr_table[] = {
	[ST_LSM6DSO_ID_ACC] = {
		.odr_size = 8,
		.reg = {
			.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0,       0x00 },
		.odr_avl[1] = {  12, 500000,  0x01 },
		.odr_avl[2] = {  26, 0,       0x02 },
		.odr_avl[3] = {  52, 0,       0x03 },
		.odr_avl[4] = { 104, 0,       0x04 },
		.odr_avl[5] = { 208, 0,       0x05 },
		.odr_avl[6] = { 416, 0,       0x06 },
		.odr_avl[7] = { 833, 0,       0x07 },
	},
	[ST_LSM6DSO_ID_GYRO] = {
		.odr_size = 8,
		.reg = {
			.addr = ST_LSM6DSO_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0,       0x00 },
		.odr_avl[1] = {  12, 500000,  0x01 },
		.odr_avl[2] = {  26, 0,       0x02 },
		.odr_avl[3] = {  52, 0,       0x03 },
		.odr_avl[4] = { 104, 0,       0x04 },
		.odr_avl[5] = { 208, 0,       0x05 },
		.odr_avl[6] = { 416, 0,       0x06 },
		.odr_avl[7] = { 833, 0,       0x07 },
	},
	[ST_LSM6DSO_ID_TEMP] = {
		.odr_size = 2,
		.odr_avl[0] = {  0, 0,        0x00 },
		.odr_avl[1] = { 12, 500000,   0x02 },
	},
};

/**
 * List of supported Full Scale Value
 *
 * The following table is complete list of supported Full Scale by Acc, Gyro
 * and Temp sensors.
 */
static const struct st_lsm6dso_fs_table_entry st_lsm6dso_fs_table[] = {
	[ST_LSM6DSO_ID_ACC] = {
		.size = ST_LSM6DSO_FS_ACC_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSO_ACC_FS_2G_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSO_ACC_FS_4G_GAIN,
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSO_ACC_FS_8G_GAIN,
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_LSM6DSO_ACC_FS_16G_GAIN,
			.val = 0x1,
		},
	},
	[ST_LSM6DSO_ID_GYRO] = {
		.size = ST_LSM6DSO_FS_GYRO_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSO_GYRO_FS_250_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSO_GYRO_FS_500_GAIN,
			.val = 0x4,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSO_GYRO_FS_1000_GAIN,
			.val = 0x8,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_LSM6DSO_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_LSM6DSO_GYRO_FS_2000_GAIN,
			.val = 0x0C,
		},
	},
	[ST_LSM6DSO_ID_TEMP] = {
		.size = ST_LSM6DSO_FS_TEMP_LIST_SIZE,
		.fs_avl[0] = {
			.reg = { 0 },
			.gain = ST_LSM6DSO_TEMP_FS_GAIN,
			.val = 0x0
		},
	},
};

/**
 * Accelerometer IIO channels description
 *
 * Accelerometer exports to IIO framework the following data channels:
 * X Axis (16 bit signed in little endian)
 * Y Axis (16 bit signed in little endian)
 * Z Axis (16 bit signed in little endian)
 * Timestamp (64 bit signed in little endian)
 * Accelerometer exports to IIO framework the following event channels:
 * Flush event done
 */
static const struct iio_chan_spec st_lsm6dso_acc_channels[] = {
	ST_LSM6DSO_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSO_REG_OUTX_L_A_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_LSM6DSO_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSO_REG_OUTY_L_A_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_LSM6DSO_DATA_CHANNEL(IIO_ACCEL, ST_LSM6DSO_REG_OUTZ_L_A_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_LSM6DSO_EVENT_CHANNEL(IIO_ACCEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

/**
 * Gyro IIO channels description
 *
 * Gyro exports to IIO framework the following data channels:
 * X Axis (16 bit signed in little endian)
 * Y Axis (16 bit signed in little endian)
 * Z Axis (16 bit signed in little endian)
 * Timestamp (64 bit signed in little endian)
 * Gyro exports to IIO framework the following event channels:
 * Flush event done
 */
static const struct iio_chan_spec st_lsm6dso_gyro_channels[] = {
	ST_LSM6DSO_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSO_REG_OUTX_L_G_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_LSM6DSO_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSO_REG_OUTY_L_G_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_LSM6DSO_DATA_CHANNEL(IIO_ANGL_VEL, ST_LSM6DSO_REG_OUTZ_L_G_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_LSM6DSO_EVENT_CHANNEL(IIO_ANGL_VEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

/**
 * Step Counter IIO channels description
 *
 * Step Counter exports to IIO framework the following data channels:
 * Step Counters (16 bit unsigned in little endian)
 * Timestamp (64 bit signed in little endian)
 * Step Counter exports to IIO framework the following event channels:
 * Flush event done
 */
static const struct iio_chan_spec st_lsm6dso_step_counter_channels[] = {
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
	ST_LSM6DSO_EVENT_CHANNEL(IIO_STEP_COUNTER, flush),
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

/**
 * @brief  Step Detector IIO channels description
 *
 * Step Detector exports to IIO framework the following event channels:
 * Step detection event detection
 */
static const struct iio_chan_spec st_lsm6dso_step_detector_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_STEP_DETECTOR, thr),
};

/**
 * Significant Motion IIO channels description
 *
 * Significant Motion exports to IIO framework the following event channels:
 * Significant Motion event detection
 */
static const struct iio_chan_spec st_lsm6dso_sign_motion_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_SIGN_MOTION, thr),
};

/**
 * Tilt IIO channels description
 *
 * Tilt exports to IIO framework the following event channels:
 * Tilt event detection
 */
static const struct iio_chan_spec st_lsm6dso_tilt_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_TILT, thr),
};

/**
 * Temperature IIO channels description
 *
 * Temperature exports to IIO framework the following data channels:
 * Temperature (16 bit signed in little endian)
 * Temperature exports to IIO framework the following event channels:
 * Temperature event threshold
 */
static const struct iio_chan_spec st_lsm6dso_temp_channels[] = {
	{
		.type = IIO_TEMP,
		.address = ST_LSM6DSO_REG_OUT_TEMP_L_ADDR,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_OFFSET) |
				      BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		}
	},
	ST_LSM6DSO_EVENT_CHANNEL(IIO_TEMP, flush),
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

/**
 * Glance IIO channels description
 *
 * Glance exports to IIO framework the following event channels:
 * Glance event detection
 */
static const struct iio_chan_spec st_lsm6dso_glance_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

/**
 * Motion IIO channels description
 *
 * Motion exports to IIO framework the following event channels:
 * Motion event detection
 */
static const struct iio_chan_spec st_lsm6dso_motion_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

/**
 * No Motion IIO channels description
 *
 * No Motion exports to IIO framework the following event channels:
 * No Motion event detection
 */
static const struct iio_chan_spec st_lsm6dso_no_motion_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

/**
 * Wakeup IIO channels description
 *
 * Wakeup exports to IIO framework the following event channels:
 * Wakeup event detection
 */
static const struct iio_chan_spec st_lsm6dso_wakeup_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

/**
 * Pickup IIO channels description
 *
 * Pickup exports to IIO framework the following event channels:
 * Pickup event detection
 */
static const struct iio_chan_spec st_lsm6dso_pickup_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

/**
 * Orientation IIO channels description
 *
 * Orientation exports to IIO framework the following data channels:
 * Orientation (8 bit unsigned in little endian)
 * Timestamp (64 bit signed in little endian)
 */
static const struct iio_chan_spec st_lsm6dso_orientation_channels[] = {
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

/**
 * Wrist IIO channels description
 *
 * Wrist exports to IIO framework the following event channels:
 * Wrist event detection
 */
static const struct iio_chan_spec st_lsm6dso_wrist_channels[] = {
	ST_LSM6DSO_EVENT_CHANNEL(IIO_GESTURE, thr),
};

int __st_lsm6dso_write_with_mask(struct st_lsm6dso_hw *hw, u8 addr, u8 mask,
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

/**
 * Detect device ID
 *
 * Check the value of the Device ID if valid
 *
 * @param  hw: ST IMU MEMS hw instance
 * @return  0 if OK, negative value for ERROR
 */
static int st_lsm6dso_check_whoami(struct st_lsm6dso_hw *hw)
{
	int err;
	u8 data;

	err = hw->tf->read(hw->dev, ST_LSM6DSO_REG_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_LSM6DSO_WHOAMI_VAL) {
		dev_err(hw->dev, "unsupported whoami [%02x]\n", data);
		return -ENODEV;
	}

	return 0;
}

/**
 * Get timestamp calibration
 *
 * Read timestamp calibration data and trim delta time
 *
 * @param  hw: ST IMU MEMS hw instance
 * @return  0 if OK, negative value for ERROR
 */
static int st_lsm6dso_get_odr_calibration(struct st_lsm6dso_hw *hw)
{
	int err;
	s8 data;
	s64 odr_calib;

	err = hw->tf->read(hw->dev, ST_LSM6DSO_INTERNAL_FREQ_FINE, sizeof(data),
			   (u8 *)&data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %d register\n",
				ST_LSM6DSO_INTERNAL_FREQ_FINE);
		return err;
	}

	odr_calib = (data * 37500) / 1000;
	hw->ts_delta_ns = ST_LSM6DSO_TS_DELTA_NS - odr_calib;

	dev_info(hw->dev, "Freq Fine %lld (ts %lld)\n",
		 odr_calib, hw->ts_delta_ns);

	return 0;
}

/**
 * Set sensor Full Scale
 *
 * Set new Full Scale value for a specific sensor
 *
 * @param  sensor: ST IMU sensor instance
 * @param  gain: New gain value
 * @return  0 if OK, negative value for ERROR
 */
static int st_lsm6dso_set_full_scale(struct st_lsm6dso_sensor *sensor, u32 gain)
{
	enum st_lsm6dso_sensor_id id = sensor->id;
	int i, err;
	u8 val;

	for (i = 0; i < st_lsm6dso_fs_table[id].size; i++)
		if (st_lsm6dso_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_lsm6dso_fs_table[id].size)
		return -EINVAL;

	val = st_lsm6dso_fs_table[id].fs_avl[i].val;
	err = st_lsm6dso_write_with_mask(sensor->hw,
				st_lsm6dso_fs_table[id].fs_avl[i].reg.addr,
				st_lsm6dso_fs_table[id].fs_avl[i].reg.mask,
				val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

/**
 * Get a valid ODR
 *
 * Check a valid ODR closest to the passed value
 *
 * @param  id: Sensor Identifier
 * @param  odr: Most significant part of ODR value (in Hz).
 * @param  uodr: Least significant part of ODR value (in micro Hz).
 * @param  podr: User data pointer.
 * @param  puodr: User data pointer.
 * @param  val: ODR register value data pointer.
 * @return  0 if OK, negative value for ERROR
 */
int st_lsm6dso_get_odr_val(enum st_lsm6dso_sensor_id id, int odr, int uodr,
			   int *podr, int *puodr, u8 *val)
{
	int i;
	int sensor_odr;
	int all_odr = ST_LSM6DSO_ODR_EXPAND(odr, uodr);

	for (i = 0; i < st_lsm6dso_odr_table[id].odr_size; i++) {
		sensor_odr =
		   ST_LSM6DSO_ODR_EXPAND(st_lsm6dso_odr_table[id].odr_avl[i].hz,
		   st_lsm6dso_odr_table[id].odr_avl[i].uhz);
		if (sensor_odr >= all_odr)
			break;
	}

	if (i == st_lsm6dso_odr_table[id].odr_size)
		return -EINVAL;

	*val = st_lsm6dso_odr_table[id].odr_avl[i].val;
	*podr = st_lsm6dso_odr_table[id].odr_avl[i].hz;
	*puodr = st_lsm6dso_odr_table[id].odr_avl[i].uhz;

	return 0;
}

static u16 st_lsm6dso_check_odr_dependency(struct st_lsm6dso_hw *hw,
					   int odr, int uodr,
					   enum st_lsm6dso_sensor_id ref_id)
{
	struct st_lsm6dso_sensor *ref = iio_priv(hw->iio_devs[ref_id]);
	bool enable = odr > 0;
	u16 ret;

	if (enable) {
		/* uodr not used */
		if (hw->enable_mask & BIT(ref_id))
			ret = max_t(int, ref->odr, odr);
		else
			ret = odr;
	} else {
		ret = (hw->enable_mask & BIT(ref_id)) ? ref->odr : 0;
	}

	return ret;
}

/**
 * Set new ODR to sensor
 * Set a valid ODR closest to the passed value
 *
 * @param  sensor: ST IMU sensor instance
 * @param  req_odr: Most significant part of ODR value (in Hz).
 * @param  req_uodr: Least significant part of ODR value (in micro Hz).
 * @return  0 if OK, negative value for ERROR
 */
static int st_lsm6dso_set_odr(struct st_lsm6dso_sensor *sensor, int req_odr,
			      int req_uodr)
{
	struct st_lsm6dso_hw *hw = sensor->hw;
	enum st_lsm6dso_sensor_id id = sensor->id;
	int err;
	u8 val;

	switch (id) {
	case ST_LSM6DSO_ID_STEP_COUNTER:
	case ST_LSM6DSO_ID_STEP_DETECTOR:
	case ST_LSM6DSO_ID_SIGN_MOTION:
	case ST_LSM6DSO_ID_TILT:
	case ST_LSM6DSO_ID_NO_MOTION:
	case ST_LSM6DSO_ID_MOTION:
	case ST_LSM6DSO_ID_GLANCE:
	case ST_LSM6DSO_ID_WAKEUP:
	case ST_LSM6DSO_ID_PICKUP:
	case ST_LSM6DSO_ID_ORIENTATION:
	case ST_LSM6DSO_ID_WRIST_TILT:
	case ST_LSM6DSO_ID_TEMP:
	case ST_LSM6DSO_ID_EXT0:
	case ST_LSM6DSO_ID_EXT1:
	case ST_LSM6DSO_ID_ACC: {
		int odr;
		int i;

		id = ST_LSM6DSO_ID_ACC;
		for (i = ST_LSM6DSO_ID_ACC; i <= ST_LSM6DSO_ID_TILT; i++) {
			if (!hw->iio_devs[i])
				continue;

			if (i == sensor->id)
				continue;

			/* req_uodr not used */
			odr = st_lsm6dso_check_odr_dependency(hw, req_odr,
							      req_uodr, i);
			if (odr != req_odr)
				/* device already configured */
				return 0;
		}
		break;
	}
	default:
		break;
	}

	err = st_lsm6dso_get_odr_val(id, req_odr, req_uodr, &req_odr,
				       &req_uodr, &val);
	if (err < 0)
		return err;

	return st_lsm6dso_write_with_mask(hw, st_lsm6dso_odr_table[id].reg.addr,
					  st_lsm6dso_odr_table[id].reg.mask,
					  val);
}

/**
 * Enable or Disable sensor
 *
 * @param  sensor: ST IMU sensor instance
 * @param  enable: Enable or disable the sensor [true,false].
 * @return  0 if OK, negative value for ERROR
 */
int st_lsm6dso_sensor_set_enable(struct st_lsm6dso_sensor *sensor,
				 bool enable)
{
	int uodr = 0;
	int odr = 0;
	int err;

	if (enable) {
		odr = sensor->odr;
		uodr = sensor->uodr;
	}

	err = st_lsm6dso_set_odr(sensor, odr, uodr);
	if (err < 0)
		return err;

	if (enable)
		sensor->hw->enable_mask |= BIT(sensor->id);
	else
		sensor->hw->enable_mask &= ~BIT(sensor->id);

	return 0;
}

/**
 * Single sensor read operation
 *
 * @param  sensor: ST IMU sensor instance
 * @param  addr: Output data register value.
 * @param  val: Output data buffer.
 * @return  IIO_VAL_INT if OK, negative value for ERROR
 */
static int st_lsm6dso_read_oneshot(struct st_lsm6dso_sensor *sensor,
				   u8 addr, int *val)
{
	int err, delay;
	__le16 data;

	err = st_lsm6dso_sensor_set_enable(sensor, true);
	if (err < 0)
		return err;

	delay = 1000000 / sensor->odr;
	usleep_range(delay, 2 * delay);

	err = st_lsm6dso_read_atomic(sensor->hw, addr, sizeof(data),
				     (u8 *)&data);
	if (err < 0)
		return err;

	st_lsm6dso_sensor_set_enable(sensor, false);

	*val = (s16)le16_to_cpu(data);

	return IIO_VAL_INT;
}

/**
 * Read Sensor data configuration
 *
 * @param  iio_dev: IIO Device.
 * @param  ch: IIO Channel.
 * @param  val: Data Buffer (MSB).
 * @param  val2: Data Buffer (LSB).
 * @param  mask: Data Mask.
 * @return  0 if OK, -EINVAL value for ERROR
 */
static int st_lsm6dso_read_raw(struct iio_dev *iio_dev,
			       struct iio_chan_spec const *ch,
			       int *val, int *val2, long mask)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(iio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&iio_dev->mlock);
		if (iio_buffer_enabled(iio_dev)) {
			ret = -EBUSY;
			mutex_unlock(&iio_dev->mlock);
			break;
		}
		ret = st_lsm6dso_read_oneshot(sensor, ch->address, val);
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
		*val = (int)sensor->odr;
		*val2 = (int)sensor->uodr;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (ch->type) {
		case IIO_TEMP:
			*val = 1;
			*val2 = ST_LSM6DSO_TEMP_GAIN;
			ret = IIO_VAL_FRACTIONAL;
			break;
		case IIO_ACCEL:
		case IIO_ANGL_VEL: {
			*val = 0;
			*val2 = sensor->gain;
			ret = IIO_VAL_INT_PLUS_MICRO;
			}
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

/**
 * Write Sensor data configuration
 *
 * @param  iio_dev: IIO Device.
 * @param  chan: IIO Channel.
 * @param  val: Data Buffer (MSB).
 * @param  val2: Data Buffer (LSB).
 * @param  mask: Data Mask.
 * @return  0 if OK, -EINVAL value for ERROR
 */
static int st_lsm6dso_write_raw(struct iio_dev *iio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_lsm6dso_set_full_scale(sensor, val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		u8 data;
		int todr, tuodr;

		err = st_lsm6dso_get_odr_val(sensor->id, val, val2, &todr,
					     &tuodr, &data);
		if (!err) {
			sensor->odr = todr;
			sensor->uodr = tuodr;
		}

		/*
		 * VTS test testSamplingRateHotSwitchOperation not toggle the
		 * enable status of sensor after changing the ODR -> force it
		 */
		if (sensor->hw->enable_mask & BIT(sensor->id)) {
			switch(sensor->id) {
			case ST_LSM6DSO_ID_GYRO:
			case ST_LSM6DSO_ID_ACC:
				err = st_lsm6dso_set_odr(sensor, sensor->odr,
							 sensor->uodr);
				/* I2C interface err can be positive */
				if (err < 0)
					break;

				err = st_lsm6dso_update_batching(iio_dev, 1);
			default:
				break;
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

#ifdef CONFIG_DEBUG_FS
static int st_lsm6dso_reg_access(struct iio_dev *iio_dev, unsigned int reg,
				 unsigned int writeval, unsigned int *readval)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(iio_dev);
	int ret;

	mutex_lock(&iio_dev->mlock);
	if (readval == NULL) {
		ret = sensor->hw->tf->write(sensor->hw->dev, reg, 1,
					    (u8 *)&writeval);
	} else {
		sensor->hw->tf->read(sensor->hw->dev, reg, 1,
				     (u8 *)readval);
		ret = 0;
	}
	mutex_unlock(&iio_dev->mlock);

	return ret;
}
#endif /* CONFIG_DEBUG_FS */

/**
 * Read sensor event configuration
 *
 * @param  iio_dev: IIO Device.
 * @param  chan: IIO Channel.
 * @param  type: Event Type.
 * @param  dir: Event Direction.
 * @return  1 if Enabled, 0 Disabled
 */
static int st_lsm6dso_read_event_config(struct iio_dev *iio_dev,
					const struct iio_chan_spec *chan,
					enum iio_event_type type,
					enum iio_event_direction dir)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(iio_dev);
	struct st_lsm6dso_hw *hw = sensor->hw;

	return !!(hw->enable_mask & BIT(sensor->id));
}

/**
 * Write sensor event configuration
 *
 * @param  iio_dev: IIO Device.
 * @param  chan: IIO Channel.
 * @param  type: Event Type.
 * @param  dir: Event Direction.
 * @param  state: New event state.
 * @return  0 if OK, negative for ERROR
 */
static int st_lsm6dso_write_event_config(struct iio_dev *iio_dev,
					 const struct iio_chan_spec *chan,
					 enum iio_event_type type,
					 enum iio_event_direction dir,
					 int state)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);
	err = st_lsm6dso_embfunc_sensor_set_enable(sensor, state);
	mutex_unlock(&iio_dev->mlock);

	return err;
}

/**
 * Get a list of available sensor ODR
 *
 * List of available ODR returned separated by commas
 *
 * @param  dev: IIO Device.
 * @param  attr: IIO Channel attribute.
 * @param  buf: User buffer.
 * @return  buffer len
 */
static ssize_t
st_lsm6dso_sysfs_sampling_frequency_avail(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_lsm6dso_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < ST_LSM6DSO_ODR_LIST_SIZE; i++) {
		if (!st_lsm6dso_odr_table[id].odr_avl[i].hz)
			continue;

		if (st_lsm6dso_odr_table[id].odr_avl[i].uhz == 0)
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				st_lsm6dso_odr_table[id].odr_avl[i].hz);
		else
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d.%d ",
				st_lsm6dso_odr_table[id].odr_avl[i].hz,
				st_lsm6dso_odr_table[id].odr_avl[i].uhz);
	}

	buf[len - 1] = '\n';

	return len;
}

/**
 * Get a list of available sensor Full Scale
 *
 * List of available Full Scale returned separated by commas
 *
 * @param  dev: IIO Device.
 * @param  attr: IIO Channel attribute.
 * @param  buf: User buffer.
 * @return  buffer len
 */
static ssize_t st_lsm6dso_sysfs_scale_avail(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct st_lsm6dso_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_lsm6dso_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_lsm6dso_fs_table[id].size; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_lsm6dso_fs_table[id].fs_avl[i].gain);
	buf[len - 1] = '\n';

	return len;
}

/**
 * Reset step counter value
 *
 * @param  dev: IIO Device.
 * @param  attr: IIO Channel attribute.
 * @param  buf: User buffer.
 * @param  size: User buffer size.
 * @return  buffer len, negative for ERROR
 */
static ssize_t
st_lsm6dso_sysfs_reset_step_counter(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	int err;

	err = st_lsm6dso_reset_step_counter(iio_dev);

	return err < 0 ? err : size;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_lsm6dso_sysfs_sampling_frequency_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_lsm6dso_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_lsm6dso_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_temp_scale_available, 0444,
		       st_lsm6dso_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_lsm6dso_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_lsm6dso_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_lsm6dso_get_watermark,
		       st_lsm6dso_set_watermark, 0);
static IIO_DEVICE_ATTR(reset_counter, 0200, NULL,
		       st_lsm6dso_sysfs_reset_step_counter, 0);

static struct attribute *st_lsm6dso_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dso_acc_attribute_group = {
	.attrs = st_lsm6dso_acc_attributes,
};

static const struct iio_info st_lsm6dso_acc_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_acc_attribute_group,
	.read_raw = st_lsm6dso_read_raw,
	.write_raw = st_lsm6dso_write_raw,
#ifdef CONFIG_DEBUG_FS
	/* connect debug info to first device */
	.debugfs_reg_access = st_lsm6dso_reg_access,
#endif /* CONFIG_DEBUG_FS */
};

static struct attribute *st_lsm6dso_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dso_gyro_attribute_group = {
	.attrs = st_lsm6dso_gyro_attributes,
};

static const struct iio_info st_lsm6dso_gyro_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_gyro_attribute_group,
	.read_raw = st_lsm6dso_read_raw,
	.write_raw = st_lsm6dso_write_raw,
};

static struct attribute *st_lsm6dso_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_temp_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dso_temp_attribute_group = {
	.attrs = st_lsm6dso_temp_attributes,
};

static const struct iio_info st_lsm6dso_temp_info = {
	.attrs = &st_lsm6dso_temp_attribute_group,
	.read_raw = st_lsm6dso_read_raw,
	.write_raw = st_lsm6dso_write_raw,
};

static struct attribute *st_lsm6dso_step_counter_attributes[] = {
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_reset_counter.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lsm6dso_step_counter_attribute_group = {
	.attrs = st_lsm6dso_step_counter_attributes,
};

static const struct iio_info st_lsm6dso_step_counter_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_step_counter_attribute_group,
};

static struct attribute *st_lsm6dso_step_detector_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_step_detector_attribute_group = {
	.attrs = st_lsm6dso_step_detector_attributes,
};

static const struct iio_info st_lsm6dso_step_detector_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_step_detector_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_sign_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_sign_motion_attribute_group = {
	.attrs = st_lsm6dso_sign_motion_attributes,
};

static const struct iio_info st_lsm6dso_sign_motion_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_sign_motion_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_tilt_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_tilt_attribute_group = {
	.attrs = st_lsm6dso_tilt_attributes,
};

static const struct iio_info st_lsm6dso_tilt_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_tilt_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_glance_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_glance_attribute_group = {
	.attrs = st_lsm6dso_glance_attributes,
};

static const struct iio_info st_lsm6dso_glance_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_glance_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_motion_attribute_group = {
	.attrs = st_lsm6dso_motion_attributes,
};

static const struct iio_info st_lsm6dso_motion_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_motion_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_no_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_no_motion_attribute_group = {
	.attrs = st_lsm6dso_no_motion_attributes,
};

static const struct iio_info st_lsm6dso_no_motion_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_no_motion_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_wakeup_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_wakeup_attribute_group = {
	.attrs = st_lsm6dso_wakeup_attributes,
};

static const struct iio_info st_lsm6dso_wakeup_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_wakeup_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_pickup_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_pickup_attribute_group = {
	.attrs = st_lsm6dso_pickup_attributes,
};

static const struct iio_info st_lsm6dso_pickup_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_pickup_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static struct attribute *st_lsm6dso_orientation_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_orientation_attribute_group = {
	.attrs = st_lsm6dso_orientation_attributes,
};

static const struct iio_info st_lsm6dso_orientation_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_orientation_attribute_group,
};

static struct attribute *st_lsm6dso_wrist_attributes[] = {
	NULL,
};

static const struct attribute_group st_lsm6dso_wrist_attribute_group = {
	.attrs = st_lsm6dso_wrist_attributes,
};

static const struct iio_info st_lsm6dso_wrist_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lsm6dso_wrist_attribute_group,
	.read_event_config = st_lsm6dso_read_event_config,
	.write_event_config = st_lsm6dso_write_event_config,
};

static const unsigned long st_lsm6dso_available_scan_masks[] = { 0x7, 0x0 };
static const unsigned long st_lsm6dso_sc_available_scan_masks[] = { 0x1, 0x0 };

static int st_lsm6dso_of_get_pin(struct st_lsm6dso_hw *hw, int *pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32(np, "st,int-pin", pin);
}

static int st_lsm6dso_get_int_reg(struct st_lsm6dso_hw *hw, u8 *drdy_reg,
				  u8 *ef_irq_reg)
{
	int err = 0, int_pin;

	if (st_lsm6dso_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (int_pin) {
	case 1:
		hw->embfunc_pg0_irq_reg = ST_LSM6DSO_REG_MD1_CFG_ADDR;
		hw->embfunc_irq_reg = ST_LSM6DSO_REG_EMB_FUNC_INT1_ADDR;
		*ef_irq_reg = ST_LSM6DSO_REG_MD1_CFG_ADDR;
		*drdy_reg = ST_LSM6DSO_REG_INT1_CTRL_ADDR;
		break;
	case 2:
		hw->embfunc_pg0_irq_reg = ST_LSM6DSO_REG_MD2_CFG_ADDR;
		hw->embfunc_irq_reg = ST_LSM6DSO_REG_EMB_FUNC_INT2_ADDR;
		*ef_irq_reg = ST_LSM6DSO_REG_MD2_CFG_ADDR;
		*drdy_reg = ST_LSM6DSO_REG_INT2_CTRL_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported interrupt pin\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_lsm6dso_reset_device(struct st_lsm6dso_hw *hw)
{
	int err;

	/* disable I3C */
	err = st_lsm6dso_write_with_mask(hw, ST_LSM6DSO_REG_CTRL9_XL_ADDR,
					 ST_LSM6DSO_REG_I3C_DISABLE_MASK, 1);
	if (err < 0)
		return err;

	/* sw reset */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_CTRL3_C_ADDR,
					 ST_LSM6DSO_REG_SW_RESET_MASK, 1);
	if (err < 0)
		return err;

	usleep_range(15, 20);

	/* boot */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_CTRL3_C_ADDR,
					 ST_LSM6DSO_REG_BOOT_MASK, 1);

	msleep(20);

	return err;
}

static int st_lsm6dso_init_device(struct st_lsm6dso_hw *hw)
{
	u8 drdy_reg, ef_irq_reg;
	int err;

	/* configure latch interrupts enabled */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_TAP_CFG0_ADDR,
					 ST_LSM6DSO_REG_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_CTRL3_C_ADDR,
					 ST_LSM6DSO_REG_BDU_MASK, 1);
	if (err < 0)
		return err;

	/* enable rouding for fast FIFO reading */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_CTRL5_C_ADDR,
					 ST_LSM6DSO_REG_ROUNDING_MASK, 3);
	if (err < 0)
		return err;

	/* init timestamp engine */
	err = st_lsm6dso_write_with_mask(hw,
					 ST_LSM6DSO_REG_CTRL10_C_ADDR,
					 ST_LSM6DSO_REG_TIMESTAMP_EN_MASK, 1);
	if (err < 0)
		return err;

	/* configure interrupt registers */
	err = st_lsm6dso_get_int_reg(hw, &drdy_reg, &ef_irq_reg);
	if (err < 0)
		return err;

	/* Enable DRDY MASK for filters settling time */
	err = st_lsm6dso_write_with_mask(hw, ST_LSM6DSO_REG_CTRL4_C_ADDR,
					 ST_LSM6DSO_REG_DRDY_MASK, 1);
	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	err = st_lsm6dso_write_with_mask(hw, drdy_reg,
					 ST_LSM6DSO_REG_INT_FIFO_TH_MASK, 1);
	if (err < 0)
		return err;

	/* enable enbedded function interrupts */
	err = st_lsm6dso_write_with_mask(hw, ef_irq_reg,
					 ST_LSM6DSO_REG_INT_EMB_FUNC_MASK, 1);
	if (err < 0)
		return err;

	/* init finite state machine */
	return st_lsm6dso_fsm_init(hw);
}

/**
 * Allocate IIO device
 *
 * @param  hw: ST IMU MEMS hw instance.
 * @param  id: Sensor Identifier.
 * @retval  struct iio_dev *, NULL if ERROR
 */
static struct iio_dev *st_lsm6dso_alloc_iiodev(struct st_lsm6dso_hw *hw,
					       enum st_lsm6dso_sensor_id id)
{
	struct st_lsm6dso_sensor *sensor;
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

	sensor->decimator = 0;
	sensor->dec_counter = 0;

	switch (id) {
	case ST_LSM6DSO_ID_ACC:
		iio_dev->channels = st_lsm6dso_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_acc_channels);
		iio_dev->name = "lsm6dso_accel";
		iio_dev->info = &st_lsm6dso_acc_info;
		iio_dev->available_scan_masks = st_lsm6dso_available_scan_masks;

		sensor->batch_reg.addr = ST_LSM6DSO_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_LSM6DSO_REG_BDR_XL_MASK;
		sensor->max_watermark = ST_LSM6DSO_MAX_FIFO_DEPTH;
		sensor->odr = st_lsm6dso_odr_table[id].odr_avl[1].hz;
		sensor->uodr = st_lsm6dso_odr_table[id].odr_avl[1].uhz;
		st_lsm6dso_set_full_scale(sensor,
				st_lsm6dso_fs_table[id].fs_avl[1].gain);
		break;
	case ST_LSM6DSO_ID_GYRO:
		iio_dev->channels = st_lsm6dso_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_gyro_channels);
		iio_dev->name = "lsm6dso_gyro";
		iio_dev->info = &st_lsm6dso_gyro_info;
		iio_dev->available_scan_masks = st_lsm6dso_available_scan_masks;

		sensor->batch_reg.addr = ST_LSM6DSO_REG_FIFO_CTRL3_ADDR;
		sensor->batch_reg.mask = ST_LSM6DSO_REG_BDR_GY_MASK;
		sensor->max_watermark = ST_LSM6DSO_MAX_FIFO_DEPTH;
		sensor->odr = st_lsm6dso_odr_table[id].odr_avl[1].hz;
		sensor->uodr = st_lsm6dso_odr_table[id].odr_avl[1].uhz;
		st_lsm6dso_set_full_scale(sensor,
				st_lsm6dso_fs_table[id].fs_avl[2].gain);
		break;
	case ST_LSM6DSO_ID_TEMP:
		iio_dev->channels = st_lsm6dso_temp_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_temp_channels);
		iio_dev->name = "lsm6dso_temp";
		iio_dev->info = &st_lsm6dso_temp_info;

		sensor->batch_reg.addr = ST_LSM6DSO_REG_FIFO_CTRL4_ADDR;
		sensor->batch_reg.mask = ST_LSM6DSO_REG_ODR_T_BATCH_MASK;
		sensor->max_watermark = ST_LSM6DSO_MAX_FIFO_DEPTH;
		sensor->odr = st_lsm6dso_odr_table[id].odr_avl[1].hz;
		sensor->uodr = st_lsm6dso_odr_table[id].odr_avl[1].uhz;
		sensor->gain = st_lsm6dso_fs_table[id].fs_avl[0].gain;
		sensor->offset = ST_LSM6DSO_TEMP_OFFSET;
		break;
	case ST_LSM6DSO_ID_STEP_COUNTER:
		iio_dev->channels = st_lsm6dso_step_counter_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dso_step_counter_channels);
		iio_dev->name = "lsm6dso_step_c";
		iio_dev->info = &st_lsm6dso_step_counter_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->max_watermark = 1;
		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_STEP_DETECTOR:
		iio_dev->channels = st_lsm6dso_step_detector_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dso_step_detector_channels);
		iio_dev->name = "lsm6dso_step_d";
		iio_dev->info = &st_lsm6dso_step_detector_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_SIGN_MOTION:
		iio_dev->channels = st_lsm6dso_sign_motion_channels;
		iio_dev->num_channels =
			ARRAY_SIZE(st_lsm6dso_sign_motion_channels);
		iio_dev->name = "lsm6dso_sign_motion";
		iio_dev->info = &st_lsm6dso_sign_motion_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_TILT:
		iio_dev->channels = st_lsm6dso_tilt_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_tilt_channels);
		iio_dev->name = "lsm6dso_tilt";
		iio_dev->info = &st_lsm6dso_tilt_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_GLANCE:
		iio_dev->channels = st_lsm6dso_glance_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_glance_channels);
		iio_dev->name = "lsm6dso_glance";
		iio_dev->info = &st_lsm6dso_glance_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_MOTION:
		iio_dev->channels = st_lsm6dso_motion_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_motion_channels);
		iio_dev->name = "lsm6dso_motion";
		iio_dev->info = &st_lsm6dso_motion_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_NO_MOTION:
		iio_dev->channels = st_lsm6dso_no_motion_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_lsm6dso_no_motion_channels);
		iio_dev->name = "lsm6dso_no_motion";
		iio_dev->info = &st_lsm6dso_no_motion_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_WAKEUP:
		iio_dev->channels = st_lsm6dso_wakeup_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_wakeup_channels);
		iio_dev->name = "lsm6dso_wk";
		iio_dev->info = &st_lsm6dso_wakeup_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_PICKUP:
		iio_dev->channels = st_lsm6dso_pickup_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_pickup_channels);
		iio_dev->name = "lsm6dso_pickup";
		iio_dev->info = &st_lsm6dso_pickup_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_ORIENTATION:
		iio_dev->channels = st_lsm6dso_orientation_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_lsm6dso_orientation_channels);
		iio_dev->name = "lsm6dso_dev_orientation";
		iio_dev->info = &st_lsm6dso_orientation_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	case ST_LSM6DSO_ID_WRIST_TILT:
		iio_dev->channels = st_lsm6dso_wrist_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_lsm6dso_wrist_channels);
		iio_dev->name = "lsm6dso_wrist";
		iio_dev->info = &st_lsm6dso_wrist_info;
		iio_dev->available_scan_masks =
					st_lsm6dso_sc_available_scan_masks;

		sensor->odr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].hz;
		sensor->uodr =
			st_lsm6dso_odr_table[ST_LSM6DSO_ID_ACC].odr_avl[2].uhz;
		break;
	default:
		return NULL;
	}

	return iio_dev;
}

/**
 * Probe device function
 * Implements [MODULE] feature for Power Management
 *
 * @param  dev: Device pointer.
 * @param  irq: I2C/SPI client irq.
 * @param  tf_ops: Bus Transfer Function pointer.
 * @retval  struct iio_dev *, NULL if ERROR
 */
int st_lsm6dso_probe(struct device *dev, int irq,
		     const struct st_lsm6dso_transfer_function *tf_ops)
{
	struct st_lsm6dso_hw *hw;
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

	err = st_lsm6dso_check_whoami(hw);
	if (err < 0)
		return err;

	err = st_lsm6dso_get_odr_calibration(hw);
	if (err < 0)
		return err;

	err = st_lsm6dso_reset_device(hw);
	if (err < 0)
		return err;

	err = st_lsm6dso_init_device(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ARRAY_SIZE(st_lsm6dso_main_sensor_list); i++) {
		enum st_lsm6dso_sensor_id id = st_lsm6dso_main_sensor_list[i];

		hw->iio_devs[id] = st_lsm6dso_alloc_iiodev(hw, id);
		if (!hw->iio_devs[id])
			return -ENOMEM;
	}

	err = st_lsm6dso_shub_probe(hw);
	if (err < 0)
		return err;

	if (hw->irq > 0) {
		err = st_lsm6dso_buffers_setup(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_LSM6DSO_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP)
	err = device_init_wakeup(dev, 1);
	if (err)
		return err;
#endif /* CONFIG_PM && CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP */

	dev_info(dev, "Device probed\n");

	return 0;
}
EXPORT_SYMBOL(st_lsm6dso_probe);

static int __maybe_unused st_lsm6dso_bk_regs(struct st_lsm6dso_hw *hw)
{
       int i, err = 0;
       u8 data, addr;

       mutex_lock(&hw->page_lock);
       for (i = 0; i < ST_LSM6DSO_SUSPEND_RESUME_REGS; i++) {
               addr = st_lsm6dso_suspend_resume[i].addr;
               err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
               if (err < 0) {
                       dev_err(hw->dev, "failed to read whoami register\n");
                       goto out_lock;
               }

               st_lsm6dso_suspend_resume[i].val = data;
       }

out_lock:
       mutex_unlock(&hw->page_lock);

       return err;
}

static int __maybe_unused st_lsm6dso_restore_regs(struct st_lsm6dso_hw *hw)
{
       int i, err = 0;
       u8 data, addr;

       mutex_lock(&hw->page_lock);
       for (i = 0; i < ST_LSM6DSO_SUSPEND_RESUME_REGS; i++) {
               addr = st_lsm6dso_suspend_resume[i].addr;
               err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
               if (err < 0) {
                       dev_err(hw->dev, "failed to read %02x reg\n", addr);
                       goto out_lock;
               }

               data &= ~st_lsm6dso_suspend_resume[i].mask;
               data |= (st_lsm6dso_suspend_resume[i].val &
                        st_lsm6dso_suspend_resume[i].mask);

               err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
               if (err < 0) {
                       dev_err(hw->dev, "failed to write %02x reg\n", addr);
                       goto out_lock;
               }
       }

out_lock:
       mutex_unlock(&hw->page_lock);

       return err;
}

/**
 * Power Management suspend callback [MODULE]
 * Implements [MODULE] feature for Power Management
 *
 * @param  dev: Device pointer.
 * @retval  0 is OK, negative value if ERROR
 */
static int __maybe_unused st_lsm6dso_suspend(struct device *dev)
{
	struct st_lsm6dso_hw *hw = dev_get_drvdata(dev);
	struct st_lsm6dso_sensor *sensor;
	int i, err = 0;

	dev_info(dev, "Suspending device\n");

	for (i = 0; i < ST_LSM6DSO_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		sensor = iio_priv(hw->iio_devs[i]);
		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		/* power off enabled sensors */
		err = st_lsm6dso_set_odr(sensor, 0, 0);
		if (err < 0)
			return err;
	}

	if (st_lsm6dso_is_fifo_enabled(hw)) {
		err = st_lsm6dso_suspend_fifo(hw);
		if (err < 0)
			return err;
	}

	err = st_lsm6dso_bk_regs(hw);

#ifdef CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP */

	return err < 0 ? err : 0;
}

/**
 * Power Management resume callback [MODULE]
 * Implements [MODULE] feature for Power Management
 *
 * @param  dev: Device pointer.
 * @retval  0 is OK, negative value if ERROR
 */
static int __maybe_unused st_lsm6dso_resume(struct device *dev)
{
	struct st_lsm6dso_hw *hw = dev_get_drvdata(dev);
	struct st_lsm6dso_sensor *sensor;
	int i, err = 0;

	dev_info(dev, "Resuming device\n");

#ifdef CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_LSM6DSO_MAY_WAKEUP */

	err = st_lsm6dso_restore_regs(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ST_LSM6DSO_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		sensor = iio_priv(hw->iio_devs[i]);
		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_lsm6dso_set_odr(sensor, sensor->odr, sensor->uodr);
		if (err < 0)
			return err;
	}

	err = st_lsm6dso_reset_hwts(hw);
	if (err < 0)
		return err;

	if (st_lsm6dso_is_fifo_enabled(hw))
		err = st_lsm6dso_set_fifo_mode(hw, ST_LSM6DSO_FIFO_CONT);

	return err < 0 ? err : 0;
}

const struct dev_pm_ops st_lsm6dso_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_lsm6dso_suspend, st_lsm6dso_resume)
};
EXPORT_SYMBOL(st_lsm6dso_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_lsm6dso driver");
MODULE_LICENSE("GPL v2");
