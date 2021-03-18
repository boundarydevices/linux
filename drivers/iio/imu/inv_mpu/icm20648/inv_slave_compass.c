/*
* Copyright (C) 2017-2019 InvenSense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include "../inv_mpu_iio.h"

/* AKM definitions */
#define REG_AKM_ID			0x00
#define REG_AKM_INFO			0x01
#define REG_AKM_STATUS			0x02
#define REG_AKM_MEASURE_DATA		0x03
#define REG_AKM_MODE			0x0A
#define REG_AKM_ST_CTRL			0x0C
#define REG_AKM_SENSITIVITY		0x10
#define REG_AKM8963_CNTL1		0x0A

/* AK09911 register definition */
#define REG_AK09911_DMP_READ		0x3
#define REG_AK09911_STATUS1		0x10
#define REG_AK09911_CNTL2		0x31
#define REG_AK09911_SENSITIVITY		0x60
#define REG_AK09911_MEASURE_DATA	0x11

/* AK09912 register definition */
#define REG_AK09912_DMP_READ		0x3
#define REG_AK09912_STATUS1		0x10
#define REG_AK09912_CNTL1		0x30
#define REG_AK09912_CNTL2		0x31
#define REG_AK09912_SENSITIVITY		0x60
#define REG_AK09912_MEASURE_DATA	0x11

/* AK09916 register definition */
#define REG_AK09916_DMP_READ		0x3
#define REG_AK09916_STATUS1		0x10
#define REG_AK09916_CNTL2		0x31
#define REG_AK09916_MEASURE_DATA	0x11

#define DATA_AKM_ID			0x48
#define DATA_AKM_MODE_PD		0x00
#define DATA_AKM_MODE_SM		0x01
#define DATA_AKM_MODE_ST		0x08
#define DATA_AK09911_MODE_ST		0x10
#define DATA_AK09912_MODE_ST		0x10
#define DATA_AK09916_MODE_ST		0x10
#define DATA_AKM_MODE_FR		0x0F
#define DATA_AK09911_MODE_FR		0x1F
#define DATA_AK09912_MODE_FR		0x1F
#define DATA_AKM_SELF_TEST		0x40
#define DATA_AKM_DRDY			0x01
#define DATA_AKM8963_BIT		0x10
#define DATA_AKM_STAT_MASK		0x0C

/* 0.3 uT * (1 << 30) */
#define DATA_AKM8975_SCALE		322122547
/* 0.6 uT * (1 << 30) */
#define DATA_AKM8972_SCALE		644245094
/* 0.6 uT * (1 << 30) */
#define DATA_AKM8963_SCALE0		644245094
/* 0.15 uT * (1 << 30) */
#define DATA_AKM8963_SCALE1		161061273
/* 0.6 uT * (1 << 30) */
#define DATA_AK09911_SCALE		644245094
/* 0.15 uT * (1 << 30) */
#define DATA_AK09912_SCALE		161061273
/* 0.15 uT * (1 << 30) */
#define DATA_AK09916_SCALE		161061273
#define DATA_MLX_SCALE			(4915 * (1L << 15))
#define DATA_MLX_SCALE_EMPIRICAL	(26214 * (1L << 15))

#define DATA_AKM8963_SCALE_SHIFT	4
#define DATA_AKM_MIN_READ_TIME		(9 * NSEC_PER_MSEC)

/* AK09912C NSF */
/* 0:disable, 1:Low, 2:Middle, 3:High */
#define DATA_AK9912_NSF			1
#define DATA_AK9912_NSF_SHIFT		5

#define DEF_ST_COMPASS_WAIT_MIN		(10 * 1000)
#define DEF_ST_COMPASS_WAIT_MAX		(15 * 1000)
#define DEF_ST_COMPASS_TRY_TIMES	10
#define DEF_ST_COMPASS_8963_SHIFT	2
#define X	0
#define Y	1
#define Z	2

/* milliseconds between each access */
#define AKM_RATE_SCALE			10
#define MLX_RATE_SCALE			50

static const short AKM8975_ST_Lower[3] = { -100, -100, -1000 };
static const short AKM8975_ST_Upper[3] = { 100, 100, -300 };

static const short AKM8972_ST_Lower[3] = { -50, -50, -500 };
static const short AKM8972_ST_Upper[3] = { 50, 50, -100 };

static const short AKM8963_ST_Lower[3] = { -200, -200, -3200 };
static const short AKM8963_ST_Upper[3] = { 200, 200, -800 };

static const short AK09911_ST_Lower[3] = { -30, -30, -400 };
static const short AK09911_ST_Upper[3] = { 30, 30, -50 };

static const short AK09912_ST_Lower[3] = { -200, -200, -1600 };
static const short AK09912_ST_Upper[3] = { 200, 200, -400 };

static const short AK09916_ST_Lower[3] = { -200, -200, -1000 };
static const short AK09916_ST_Upper[3] = { 200, 200, -200 };

static bool secondary_resume_state;
/*
 *  inv_setup_compass_akm() - Configure akm series compass.
 */
static int inv_setup_compass_akm(struct inv_mpu_state *st)
{
	int result;
	u8 data[4];
	u8 sens, mode, cmd, addr;

	addr = st->plat_data.secondary_i2c_addr;

	result = inv_execute_read_secondary(st, 0, addr, REG_AKM_ID, 1, data);
	if (result) {
		pr_info("%s: read secondary failed\n", __func__);
		return result;
	}
	if (data[0] != DATA_AKM_ID) {
		pr_info
	("%s: DATA_AKM_ID check failed data[0] [%x] ID [%x] Addr [0x%X]\n",
		__func__, data[0], DATA_AKM_ID, addr);
		return -ENXIO;
	}

	/* set AKM register for mode control */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		mode = REG_AK09911_CNTL2;
	else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id)
		mode = REG_AK09912_CNTL2;
	else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id)
		mode = REG_AK09916_CNTL2;
	else
		mode = REG_AKM_MODE;

	/* AK09916 not have Fuse ROM */
	if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id) {
		/* set dummy sens which should be utilized
		 * in the same manner as AK09912 to make no adjustment. */
		st->chip_info.compass_sens[0] = 128;
		st->chip_info.compass_sens[1] = 128;
		st->chip_info.compass_sens[2] = 128;
		goto skip_akm_fuse_rom_read;
	}

	/* set AKM to Fuse ROM access mode */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		sens = REG_AK09911_SENSITIVITY;
		cmd = DATA_AK09911_MODE_FR;
	} else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) {
		sens = REG_AK09912_SENSITIVITY;
		cmd = DATA_AK09912_MODE_FR;
	} else {
		sens = REG_AKM_SENSITIVITY;
		cmd = DATA_AKM_MODE_FR;
	}
	inv_set_bank(st, BANK_SEL_3);
	result = inv_read_secondary(st, 0, addr, sens, THREE_AXES);
	if (result)
		return result;
	result = inv_write_secondary(st, 1, addr, mode, cmd);
	if (result)
		return result;

	if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) {
		result = inv_write_secondary(st, 2, addr, REG_AK09912_CNTL1,
				DATA_AK9912_NSF <<
				DATA_AK9912_NSF_SHIFT);
		if (result)
			return result;
	}
	inv_set_bank(st, BANK_SEL_0);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis |
			BIT_I2C_MST_EN);
	msleep(SECONDARY_INIT_WAIT);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_plat_read(st, REG_EXT_SLV_SENS_DATA_00, THREE_AXES,
			st->chip_info.compass_sens);
	if (result)
		return result;
	result = inv_execute_write_secondary(st, 0, addr, mode,
			DATA_AKM_MODE_PD);
	if (result)
		return result;

skip_akm_fuse_rom_read:
	pr_debug("%s senx=%d, seny=%d, senz=%d\n",
		st->hw->name,
		st->chip_info.compass_sens[0],
		st->chip_info.compass_sens[1], st->chip_info.compass_sens[2]);

	/* output data for slave 1 is fixed, single measure mode */
	st->slave_compass->scale = 1;
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8975_ST_Upper;
		st->slave_compass->st_lower = AKM8975_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8972_ST_Upper;
		st->slave_compass->st_lower = AKM8972_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8963_ST_Upper;
		st->slave_compass->st_lower = AKM8963_ST_Lower;
		data[0] = DATA_AKM_MODE_SM |
			(st->slave_compass->scale << DATA_AKM8963_SCALE_SHIFT);
	} else if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AK09911_ST_Upper;
		st->slave_compass->st_lower = AK09911_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AK09912_ST_Upper;
		st->slave_compass->st_lower = AK09912_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AK09916_ST_Upper;
		st->slave_compass->st_lower = AK09916_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else {
		return -EINVAL;
	}
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_write_secondary(st, 1, addr, mode, data[0]);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_SLV2_CTRL, 0);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_akm_read_data(struct inv_mpu_state *st, short *o)
{
	int result;
	int i;
	u8 d[DATA_AKM_99_BYTES_DMP - 1];
	u8 *sens;

	sens = st->chip_info.compass_sens;
	result = 0;
	for (i = 0; i < 6; i++)
		d[1 + i] = st->fifo_data[i];
	for (i = 0; i < 3; i++)
		o[i] = (short)((d[i * 2 + 1] << 8) | d[i * 2 + 2]);

	return result;
}

static int inv_check_akm_self_test(struct inv_mpu_state *st)
{
	int result;
	u8 data[6], mode, addr;
	u8 counter, cntl;
	short x, y, z;
	u8 *sens;
	int shift;
	u8 slv_ctrl[2];
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	u8 odr_cfg;
#endif
	addr = st->plat_data.secondary_i2c_addr;
	sens = st->chip_info.compass_sens;

	/* back up registers */
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	/* SLV0_CTRL */
	result = inv_plat_read(st, REG_I2C_SLV0_CTRL, 1, &slv_ctrl[0]);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_SLV0_CTRL, 0);
	if (result)
		return result;
	/* SLV1_CTRL */
	result = inv_plat_read(st, REG_I2C_SLV1_CTRL, 1, &slv_ctrl[1]);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_SLV1_CTRL, 0);
	if (result)
		return result;
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	/* I2C_MST ODR */
	result = inv_plat_read(st, REG_I2C_MST_ODR_CONFIG, 1, &odr_cfg);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG, 0);
	if (result)
		return result;
#endif
	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		mode = REG_AK09911_CNTL2;
	else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id)
		mode = REG_AK09912_CNTL2;
	else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id)
		mode = REG_AK09916_CNTL2;
	else
		mode = REG_AKM_MODE;
	/* set to power down mode */
	result = inv_execute_write_secondary(st, 0, addr, mode,
			DATA_AKM_MODE_PD);
	if (result)
		goto AKM_fail;

	/* write 1 to ASTC register */
	if ((COMPASS_ID_AK09911 != st->plat_data.sec_slave_id) &&
		(COMPASS_ID_AK09912 != st->plat_data.sec_slave_id) &&
		(COMPASS_ID_AK09916 != st->plat_data.sec_slave_id)) {
		result = inv_execute_write_secondary(st, 0, addr,
				REG_AKM_ST_CTRL,
				DATA_AKM_SELF_TEST);
		if (result)
			goto AKM_fail;
	}
	/* set self test mode */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		result = inv_execute_write_secondary(st, 0, addr, mode,
				DATA_AK09911_MODE_ST);
	else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id)
		result = inv_execute_write_secondary(st, 0, addr, mode,
				DATA_AK09912_MODE_ST);
	else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id)
		result = inv_execute_write_secondary(st, 0, addr, mode,
				DATA_AK09916_MODE_ST);
	else
		result = inv_execute_write_secondary(st, 0, addr, mode,
				DATA_AKM_MODE_ST);

	if (result)
		goto AKM_fail;
	counter = DEF_ST_COMPASS_TRY_TIMES;
	while (counter > 0) {
		usleep_range(DEF_ST_COMPASS_WAIT_MIN, DEF_ST_COMPASS_WAIT_MAX);
		if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
			result = inv_execute_read_secondary(st, 0, addr,
					REG_AK09911_STATUS1,
					1, data);
		else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id)
			result = inv_execute_read_secondary(st, 0, addr,
					REG_AK09912_STATUS1,
					1, data);
		else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id)
			result = inv_execute_read_secondary(st, 0, addr,
					REG_AK09916_STATUS1,
					1, data);
		else
			result = inv_execute_read_secondary(st, 0, addr,
					REG_AKM_STATUS, 1,
					data);
		if (result)
			goto AKM_fail;
		if ((data[0] & DATA_AKM_DRDY) == 0)
			counter--;
		else
			counter = 0;
	}
	if ((data[0] & DATA_AKM_DRDY) == 0) {
		result = -EINVAL;
		goto AKM_fail;
	}
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		result = inv_execute_read_secondary(st, 0, addr,
				REG_AK09911_MEASURE_DATA,
				BYTES_PER_SENSOR, data);
	} else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) {
		result = inv_execute_read_secondary(st, 0, addr,
				REG_AK09912_MEASURE_DATA,
				BYTES_PER_SENSOR, data);
	} else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id) {
		result = inv_execute_read_secondary(st, 0, addr,
				REG_AK09916_MEASURE_DATA,
				BYTES_PER_SENSOR, data);
	} else {
		result = inv_execute_read_secondary(st, 0, addr,
				REG_AKM_MEASURE_DATA,
				BYTES_PER_SENSOR, data);
	}
	if (result)
		goto AKM_fail;

	x = le16_to_cpup((__le16 *) (&data[0]));
	y = le16_to_cpup((__le16 *) (&data[2]));
	z = le16_to_cpup((__le16 *) (&data[4]));
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		shift = 7;
	else
		shift = 8;
	x = ((x * (sens[0] + 128)) >> shift);
	y = ((y * (sens[1] + 128)) >> shift);
	z = ((z * (sens[2] + 128)) >> shift);
	if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) {
		result = inv_execute_read_secondary(st, 0, addr,
				REG_AKM8963_CNTL1, 1,
				&cntl);
		if (result)
			goto AKM_fail;
		if (0 == (cntl & DATA_AKM8963_BIT)) {
			x <<= DEF_ST_COMPASS_8963_SHIFT;
			y <<= DEF_ST_COMPASS_8963_SHIFT;
			z <<= DEF_ST_COMPASS_8963_SHIFT;
		}
	}

	pr_debug("lowerx=%d, upperx=%d, x=%d\n",
		st->slave_compass->st_lower[X],
		st->slave_compass->st_upper[X], x);
	pr_debug("lowery=%d, uppery=%d, y=%d\n",
		st->slave_compass->st_lower[Y],
		st->slave_compass->st_upper[Y], y);
	pr_debug("lowerz=%d, upperz=%d, z=%d\n",
		st->slave_compass->st_lower[Z],
		st->slave_compass->st_upper[Z], z);

	result = -EINVAL;
	if (x > st->slave_compass->st_upper[X] ||
		x < st->slave_compass->st_lower[X])
		goto AKM_fail;
	if (y > st->slave_compass->st_upper[Y] ||
		y < st->slave_compass->st_lower[Y])
		goto AKM_fail;
	if (z > st->slave_compass->st_upper[Z] ||
		z < st->slave_compass->st_lower[Z])
		goto AKM_fail;
	result = 0;
AKM_fail:
	/*write 0 to ASTC register */
	if ((COMPASS_ID_AK09911 != st->plat_data.sec_slave_id) &&
		(COMPASS_ID_AK09912 != st->plat_data.sec_slave_id) &&
		(COMPASS_ID_AK09916 != st->plat_data.sec_slave_id)) {
		result |= inv_execute_write_secondary(st, 0, addr,
				REG_AKM_ST_CTRL, 0);
	}
	/*set to power down mode */
	result |= inv_execute_write_secondary(st, 0, addr, mode,
			DATA_AKM_MODE_PD);

	/* restore registers */
	result |= inv_set_bank(st, BANK_SEL_3);
	result |= inv_plat_single_write(st, REG_I2C_SLV0_CTRL, slv_ctrl[0]);
	result |= inv_plat_single_write(st, REG_I2C_SLV1_CTRL, slv_ctrl[1]);
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	result |= inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG, odr_cfg);
#endif
	result |= inv_set_bank(st, BANK_SEL_0);

	return result;
}

/*
 *  inv_write_akm_scale() - Configure the akm scale range.
 */
static int inv_write_akm_scale(struct inv_mpu_state *st, int data)
{
	char d, en;
	int result;

	if (COMPASS_ID_AK8963 != st->plat_data.sec_slave_id)
		return 0;
	en = !!data;
	if (st->slave_compass->scale == en)
		return 0;
	d = (DATA_AKM_MODE_SM | (en << DATA_AKM8963_SCALE_SHIFT));
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_SLV1_DO, d);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;
	st->slave_compass->scale = en;

	return 0;
}

/*
 *  inv_read_akm_scale() - show AKM scale.
 */
static int inv_read_akm_scale(struct inv_mpu_state *st, int *scale)
{
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8975_SCALE;
	else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8972_SCALE;
	else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id)
		if (st->slave_compass->scale)
			*scale = DATA_AKM8963_SCALE1;
		else
			*scale = DATA_AKM8963_SCALE0;
	else if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		*scale = DATA_AK09911_SCALE;
	else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id)
		*scale = DATA_AK09912_SCALE;
	else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id)
		*scale = DATA_AK09916_SCALE;
	else
		return -EINVAL;

	return IIO_VAL_INT;
}

static int inv_suspend_akm(struct inv_mpu_state *st)
{
	int result;

	if (!secondary_resume_state)
		return 0;

	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;

	/* slave 0 is disabled */
	result = inv_plat_single_write(st, REG_I2C_SLV0_CTRL, 0);
	if (result)
		return result;
	/* slave 1 is disabled */
	result = inv_plat_single_write(st, REG_I2C_SLV1_CTRL, 0);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	secondary_resume_state = false;

	return result;
}

static int inv_resume_akm(struct inv_mpu_state *st)
{
	int result;
	u8 reg_addr, bytes;

	if (secondary_resume_state)
		return 0;

	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;

	/* slave 0 is used to read data from compass */
	/*read mode */
	result = inv_plat_single_write(st, REG_I2C_SLV0_ADDR,
			INV_MPU_BIT_I2C_READ |
			st->plat_data.secondary_i2c_addr);
	if (result)
		return result;
	/* AKM status register address is 1 */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		reg_addr = REG_AK09911_DMP_READ;
		bytes = DATA_AKM_99_BYTES_DMP;
	} else if (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) {
		reg_addr = REG_AK09912_DMP_READ;
		bytes = DATA_AKM_99_BYTES_DMP;
	} else if (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id) {
		reg_addr = REG_AK09916_DMP_READ;
		bytes = DATA_AKM_99_BYTES_DMP;
	} else {
		reg_addr = REG_AKM_INFO;
		bytes = DATA_AKM_89_BYTES_DMP;
	}
	result = inv_plat_single_write(st, REG_I2C_SLV0_REG, reg_addr);
	if (result)
		return result;

	/* slave 0 is enabled, read 10 or 8 bytes from here, swap bytes */
	result = inv_plat_single_write(st, REG_I2C_SLV0_CTRL,
			INV_MPU_BIT_GRP |
			INV_MPU_BIT_BYTE_SW |
			INV_MPU_BIT_SLV_EN | bytes);
	if (result)
		return result;
	/* slave 1 is enabled, write byte length is 1 */
	result = inv_plat_single_write(st, REG_I2C_SLV1_CTRL,
			INV_MPU_BIT_SLV_EN | 1);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	secondary_resume_state = true;

	return result;
}

static struct inv_mpu_slave slave_akm = {
	.suspend = inv_suspend_akm,
	.resume = inv_resume_akm,
	.get_scale = inv_read_akm_scale,
	.set_scale = inv_write_akm_scale,
	.self_test = inv_check_akm_self_test,
	.setup = inv_setup_compass_akm,
	.read_data = inv_akm_read_data,
	.rate_scale = AKM_RATE_SCALE,
	.min_read_time = DATA_AKM_MIN_READ_TIME,
};

int inv_mpu_setup_compass_slave(struct inv_mpu_state *st)
{
	switch (st->plat_data.sec_slave_id) {
	case COMPASS_ID_AK8975:
	case COMPASS_ID_AK8972:
	case COMPASS_ID_AK8963:
	case COMPASS_ID_AK09911:
	case COMPASS_ID_AK09912:
	case COMPASS_ID_AK09916:
		st->slave_compass = &slave_akm;
		break;
	default:
		return -EINVAL;
	}

	return st->slave_compass->setup(st);
}
