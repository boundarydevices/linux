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

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20648"},
};

static int inv_set_dmp(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH,
				st->dmp_start_address >> 8);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH + 1,
				st->dmp_start_address & 0xff);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_calc_gyro_sf(s8 pll)
{
	int a, r;
	int value, t;

	t = 102870L + 81L * pll;
	a = (1L << 30) / t;
	r = (1L << 30) - a * t;
	value = a * 797 * DMP_DIVIDER;
	value += (s64) ((a * 1011387LL * DMP_DIVIDER) >> 20);
	value += r * 797L * DMP_DIVIDER / t;
	value += (s32) ((s64) ((r * 1011387LL * DMP_DIVIDER) >> 20)) / t;
	value <<= 1;

	return value;
}

static int inv_read_timebase(struct inv_mpu_state *st)
{
	int result;
	u8 d;
	s8 t;

/* ((20 * 1270 * NSEC_PER_SEC)/(27 * 1000 * 768)) */
#define INV_PRECISION_CONST 1224922


	result = inv_set_bank(st, BANK_SEL_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_TIMEBASE_CORRECTION_PLL, 1, &d);
	if (result)
		return result;
	t = abs(d & 0x7f);
	if (d & 0x80)
		t = -t;
	st->eis.count_precision = INV_PRECISION_CONST / (1270 + t);
	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = (NSEC_PER_SEC / (1270 + t)) * 1270;
	st->eng_info[ENGINE_PRESSURE].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_PRESSURE].orig_rate = MAX_PRS_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	st->gyro_sf = inv_calc_gyro_sf(t);
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;
	u8 averaging = 4;
	u8 dec3_cfg;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG_1,
				(st->chip_config.fsr
				<< SHIFT_GYRO_FS_SEL) |
				BIT_GYRO_FCHOICE);
	if (result)
		return result;
	switch (averaging) {
	case 1:
		dec3_cfg = 0;
		break;
	case 2:
		dec3_cfg = 1;
		break;
	case 4:
		dec3_cfg = 2;
		break;
	case 8:
		dec3_cfg = 3;
		break;
	case 16:
		dec3_cfg = 4;
		break;
	case 32:
		dec3_cfg = 5;
		break;
	case 64:
		dec3_cfg = 6;
		break;
	case 128:
		dec3_cfg = 7;
		break;
	default:
		dec3_cfg = 0;
		break;
	}
	result = inv_plat_single_write(st, REG_GYRO_CONFIG_2, dec3_cfg);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;
	u8 averaging = 1;
	u8 dec3_cfg;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;

	/* The FCHOICE is 1 even in cycle mode so that averaging is 4 */
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
			(st->chip_config. accel_fs << SHIFT_ACCEL_FS) |
			(ACCEL_CONFIG_LOW_POWER_SET << SHIFT_ACCEL_DLPCFG) |
			BIT_ACCEL_FCHOICE);
	if (result)
		return result;
	switch (averaging) {
	case 4:
		dec3_cfg = 0;
		break;
	case 8:
		dec3_cfg = 1;
		break;
	case 16:
		dec3_cfg = 2;
		break;
	case 32:
		dec3_cfg = 3;
		break;
	default:
		dec3_cfg = 0;
		break;
	}
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, dec3_cfg);

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_secondary(struct inv_mpu_state *st)
{
	int r;

	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_I2C_MST_CTRL, BIT_I2C_MST_P_NSR);
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
					MIN_MST_ODR_CONFIG);
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_I2C_MST_DELAY_CTRL,
					BIT_DELAY_ES_SHADOW);
	if (r)
		return r;

	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}

static int inv_init_secondary(struct inv_mpu_state *st)
{
	st->slv_reg[0].addr = REG_I2C_SLV0_ADDR;
	st->slv_reg[0].reg = REG_I2C_SLV0_REG;
	st->slv_reg[0].ctrl = REG_I2C_SLV0_CTRL;
	st->slv_reg[0].d0 = REG_I2C_SLV0_DO;

	st->slv_reg[1].addr = REG_I2C_SLV1_ADDR;
	st->slv_reg[1].reg = REG_I2C_SLV1_REG;
	st->slv_reg[1].ctrl = REG_I2C_SLV1_CTRL;
	st->slv_reg[1].d0 = REG_I2C_SLV1_DO;

	st->slv_reg[2].addr = REG_I2C_SLV2_ADDR;
	st->slv_reg[2].reg = REG_I2C_SLV2_REG;
	st->slv_reg[2].ctrl = REG_I2C_SLV2_CTRL;
	st->slv_reg[2].d0 = REG_I2C_SLV2_DO;

	st->slv_reg[3].addr = REG_I2C_SLV3_ADDR;
	st->slv_reg[3].reg = REG_I2C_SLV3_REG;
	st->slv_reg[3].ctrl = REG_I2C_SLV3_CTRL;
	st->slv_reg[3].d0 = REG_I2C_SLV3_DO;

	return 0;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size = ACCEL_DATA_SZ;
	st->sensor[SENSOR_GYRO].sample_size = GYRO_DATA_SZ;
	st->sensor[SENSOR_COMPASS].sample_size = CPASS_DATA_SZ;
	st->sensor[SENSOR_COMPASS_CAL].sample_size = CPASS_CALIBR_DATA_SZ;
	st->sensor[SENSOR_ALS].sample_size = ALS_DATA_SZ;
	st->sensor[SENSOR_PRESSURE].sample_size = PRESSURE_DATA_SZ;
	st->sensor[SENSOR_SIXQ].sample_size = QUAT6_DATA_SZ;
	st->sensor[SENSOR_NINEQ].sample_size = QUAT9_DATA_SZ;
	st->sensor[SENSOR_PEDQ].sample_size = PQUAT6_DATA_SZ;
	st->sensor[SENSOR_GEOMAG].sample_size = GEOMAG_DATA_SZ;

	st->sensor[SENSOR_ACCEL].odr_addr = ODR_ACCEL;
	st->sensor[SENSOR_GYRO].odr_addr = ODR_GYRO;
	st->sensor[SENSOR_COMPASS].odr_addr = ODR_CPASS;
	st->sensor[SENSOR_COMPASS_CAL].odr_addr = ODR_CPASS_CALIBR;
	st->sensor[SENSOR_ALS].odr_addr = ODR_ALS;
	st->sensor[SENSOR_PRESSURE].odr_addr = ODR_PRESSURE;
	st->sensor[SENSOR_SIXQ].odr_addr = ODR_QUAT6;
	st->sensor[SENSOR_NINEQ].odr_addr = ODR_QUAT9;
	st->sensor[SENSOR_PEDQ].odr_addr = ODR_PQUAT6;
	st->sensor[SENSOR_GEOMAG].odr_addr = ODR_GEOMAG;

	st->sensor[SENSOR_ACCEL].counter_addr = ODR_CNTR_ACCEL;
	st->sensor[SENSOR_GYRO].counter_addr = ODR_CNTR_GYRO;
	st->sensor[SENSOR_COMPASS].counter_addr = ODR_CNTR_CPASS;
	st->sensor[SENSOR_COMPASS_CAL].counter_addr = ODR_CNTR_CPASS_CALIBR;
	st->sensor[SENSOR_ALS].counter_addr = ODR_CNTR_ALS;
	st->sensor[SENSOR_PRESSURE].counter_addr = ODR_CNTR_PRESSURE;
	st->sensor[SENSOR_SIXQ].counter_addr = ODR_CNTR_QUAT6;
	st->sensor[SENSOR_NINEQ].counter_addr = ODR_CNTR_QUAT9;
	st->sensor[SENSOR_PEDQ].counter_addr = ODR_CNTR_PQUAT6;
	st->sensor[SENSOR_GEOMAG].counter_addr = ODR_CNTR_GEOMAG;

	st->sensor[SENSOR_ACCEL].output = ACCEL_SET;
	st->sensor[SENSOR_GYRO].output = GYRO_SET;
	st->sensor[SENSOR_COMPASS].output = CPASS_SET;
	st->sensor[SENSOR_COMPASS_CAL].output = CPASS_CALIBR_SET;
	st->sensor[SENSOR_ALS].output = ALS_SET;
	st->sensor[SENSOR_PRESSURE].output = PRESSURE_SET;
	st->sensor[SENSOR_SIXQ].output = QUAT6_SET;
	st->sensor[SENSOR_NINEQ].output = QUAT9_SET;
	st->sensor[SENSOR_PEDQ].output = PQUAT6_SET;
	st->sensor[SENSOR_GEOMAG].output = GEOMAG_SET;

	st->sensor[SENSOR_ACCEL].a_en = true;
	st->sensor[SENSOR_GYRO].a_en = false;
	st->sensor[SENSOR_COMPASS].a_en = false;
	st->sensor[SENSOR_COMPASS_CAL].a_en = false;
	st->sensor[SENSOR_ALS].a_en = false;
	st->sensor[SENSOR_PRESSURE].a_en = false;
	st->sensor[SENSOR_SIXQ].a_en = true;
	st->sensor[SENSOR_NINEQ].a_en = true;
	st->sensor[SENSOR_PEDQ].a_en = true;
	st->sensor[SENSOR_GEOMAG].a_en = true;

	st->sensor[SENSOR_ACCEL].g_en = false;
	st->sensor[SENSOR_GYRO].g_en = true;
	st->sensor[SENSOR_COMPASS].g_en = false;
	st->sensor[SENSOR_COMPASS_CAL].g_en = false;
	st->sensor[SENSOR_ALS].g_en = false;
	st->sensor[SENSOR_PRESSURE].g_en = false;
	st->sensor[SENSOR_SIXQ].g_en = true;
	st->sensor[SENSOR_NINEQ].g_en = true;
	st->sensor[SENSOR_PEDQ].g_en = true;
	st->sensor[SENSOR_GEOMAG].g_en = false;

	st->sensor[SENSOR_ACCEL].c_en = false;
	st->sensor[SENSOR_GYRO].c_en = false;
	st->sensor[SENSOR_COMPASS].c_en = true;
	st->sensor[SENSOR_COMPASS_CAL].c_en = true;
	st->sensor[SENSOR_ALS].c_en = false;
	st->sensor[SENSOR_PRESSURE].c_en = false;
	st->sensor[SENSOR_SIXQ].c_en = false;
	st->sensor[SENSOR_NINEQ].c_en = true;
	st->sensor[SENSOR_PEDQ].c_en = false;
	st->sensor[SENSOR_GEOMAG].c_en = true;

	st->sensor[SENSOR_ACCEL].p_en = false;
	st->sensor[SENSOR_GYRO].p_en = false;
	st->sensor[SENSOR_COMPASS].p_en = false;
	st->sensor[SENSOR_COMPASS_CAL].p_en = false;
	st->sensor[SENSOR_ALS].p_en = false;
	st->sensor[SENSOR_PRESSURE].p_en = true;
	st->sensor[SENSOR_SIXQ].p_en = false;
	st->sensor[SENSOR_NINEQ].p_en = false;
	st->sensor[SENSOR_PEDQ].p_en = false;
	st->sensor[SENSOR_GEOMAG].p_en = false;

	st->sensor[SENSOR_ACCEL].engine_base = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base = ENGINE_GYRO;
	st->sensor[SENSOR_COMPASS].engine_base = ENGINE_I2C;
	st->sensor[SENSOR_COMPASS_CAL].engine_base = ENGINE_I2C;
	st->sensor[SENSOR_ALS].engine_base = ENGINE_I2C;
	st->sensor[SENSOR_PRESSURE].engine_base = ENGINE_I2C;
	st->sensor[SENSOR_SIXQ].engine_base = ENGINE_GYRO;
	st->sensor[SENSOR_NINEQ].engine_base = ENGINE_GYRO;
	st->sensor[SENSOR_PEDQ].engine_base = ENGINE_GYRO;
	st->sensor[SENSOR_GEOMAG].engine_base = ENGINE_ACCEL;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_GYRO_CAL].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_MAG_CAL].base = SENSOR_COMPASS_CAL;
	st->sensor_l[SENSOR_L_EIS_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_SIXQ].base = SENSOR_SIXQ;
	st->sensor_l[SENSOR_L_NINEQ].base = SENSOR_NINEQ;
	st->sensor_l[SENSOR_L_PEDQ].base = SENSOR_PEDQ;
	st->sensor_l[SENSOR_L_PRESSURE].base = SENSOR_PRESSURE;
	st->sensor_l[SENSOR_L_ALS].base = SENSOR_ALS;
	st->sensor_l[SENSOR_L_GEOMAG].base = SENSOR_GEOMAG;
	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG_WAKE].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_ALS_WAKE].base = SENSOR_ALS;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].base = SENSOR_SIXQ;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].base = SENSOR_NINEQ;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].base = SENSOR_PEDQ;
	st->sensor_l[SENSOR_L_GEOMAG_WAKE].base = SENSOR_GEOMAG;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].base = SENSOR_PRESSURE;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].base = SENSOR_COMPASS_CAL;

	st->sensor_l[SENSOR_L_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GYRO].header = GYRO_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL].header = GYRO_CALIB_HDR;
	st->sensor_l[SENSOR_L_MAG].header = COMPASS_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL].header = COMPASS_CALIB_HDR;
	st->sensor_l[SENSOR_L_EIS_GYRO].header = EIS_GYRO_HDR;
	st->sensor_l[SENSOR_L_SIXQ].header = SIXQUAT_HDR;
	st->sensor_l[SENSOR_L_NINEQ].header = NINEQUAT_HDR;
	st->sensor_l[SENSOR_L_PEDQ].header = PEDQUAT_HDR;
	st->sensor_l[SENSOR_L_PRESSURE].header = PRESSURE_HDR;
	st->sensor_l[SENSOR_L_ALS].header = ALS_HDR;
	st->sensor_l[SENSOR_L_GEOMAG].header = GEOMAG_HDR;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].header = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_WAKE].header = GYRO_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].header = GYRO_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_WAKE].header = COMPASS_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].header = COMPASS_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].header = SIXQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].header = NINEQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].header = PEDQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].header = PRESSURE_WAKE_HDR;
	st->sensor_l[SENSOR_L_ALS_WAKE].header = ALS_WAKE_HDR;
	st->sensor_l[SENSOR_L_GEOMAG_WAKE].header = GEOMAG_WAKE_HDR;

	for (i = SENSOR_L_ACCEL; i <= SENSOR_L_EIS_GYRO; i++)
		st->sensor_l[i].wake_on = false;

	for (i = SENSOR_L_ACCEL_WAKE; i <= SENSOR_L_MAG_CAL_WAKE; i++)
		st->sensor_l[i].wake_on = true;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].sample_size =
			ACCEL_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].sample_size =
			GYRO_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].sample_size =
			CPASS_ACCURACY_SZ;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].output = ACCEL_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].output = GYRO_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].output =
			CPASS_ACCURACY_SET;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].header = ACCEL_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].header = GYRO_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].header =
			COMPASS_ACCURACY_HDR;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;
	u8 reg = 0x0;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->firmware = 0;
	st->secondary_switch = false;
	st->fifo_count_mode = BYTE_MODE;

	inv_init_secondary(st);
	inv_init_sensor_struct(st);

	/* Implemented for HW_FIX_DISABLE */
	res = inv_plat_read(st, 0x75, 1, &reg);
	if (res) {
		pr_info("Error : cannot read 0x75 (HW_FIX_DIABLE) register\n");
		return res;
	}
	reg |= 0x8;
	res = inv_plat_single_write(st, 0x75, reg);
	res = inv_plat_read(st, 0x75, 1, &reg);
	pr_info("Write %04X to HW_FIX_DIABLE register\n", reg);
	/* End of implemented for HW_FIX_DISABLE */

	res = inv_read_timebase(st);
	if (res)
		return res;
	res = inv_set_dmp(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;
	res = inv_set_secondary(st);

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].previous_ts = 0;

	return res;
}

int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;

	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;

	/* verify whoami */
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;
	pr_info("whoami= %x\n", v);
	if (v == 0x00 || v == 0xff)
		return -ENODEV;

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;
	result = inv_set_power(st, true);
	if (result)
		return result;

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;

	result = inv_read_offset_regs(st,
			st->org_accel_offset_reg, st->org_gyro_offset_reg);
	if (result)
		return result;

	result = inv_init_config(st);
	if (result)
		return result;

	if (SECONDARY_SLAVE_TYPE_COMPASS == plat->sec_slave_type)
		st->chip_config.has_compass = 1;
	else
		st->chip_config.has_compass = 0;
	if (SECONDARY_SLAVE_TYPE_PRESSURE == plat->aux_slave_type)
		st->chip_config.has_pressure = 1;
	else
		st->chip_config.has_pressure = 0;
	if (SECONDARY_SLAVE_TYPE_ALS == plat->read_only_slave_type)
		st->chip_config.has_als = 1;
	else
		st->chip_config.has_als = 0;

	if (st->chip_config.has_compass) {
		result = inv_mpu_setup_compass_slave(st);
		if (result)
			pr_err("compass setup failed\n");
	}
	if (st->chip_config.has_pressure) {
		result = inv_mpu_setup_pressure_slave(st);
		if (result) {
			pr_err("pressure setup failed\n");
			st->chip_config.has_pressure = 0;
			plat->aux_slave_type = SECONDARY_SLAVE_TYPE_NONE;
		}
	}
	if (st->chip_config.has_als) {
		result = inv_mpu_setup_als_slave(st);
		if (result) {
			pr_err("als setup failed\n");
			st->chip_config.has_als = 0;
			plat->read_only_slave_type = SECONDARY_SLAVE_TYPE_NONE;
		}
	}

	st->chip_config.lp_en_mode_off = 0;

	result = inv_set_power(st, false);

	pr_info("%s: initialize result is %d....\n", __func__, result);

	return result;
}
