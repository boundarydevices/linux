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

struct inv_local_store {
	u8 reg_lp_config;
	u8 reg_delay_enable;
	u8 reg_delay_time;
	u8 reg_gyro_smplrt;
	bool wom_on;
	bool activity_eng_on;
	int accel_cal_ind;
};

static struct inv_local_store local;

static int compass_cal_param[] = { 200, 70, 35, 15, 8, 4 };

struct inv_accel_cal_params {
	int freq;
	int rate;
	int bac_rate;
	int gain;
	int alpha;
	int a;
};

static struct inv_accel_cal_params accel_cal_para[] = {
	{
		.freq = 1000,
		},
	{
		.freq = 225,
		.rate = 0,
		.bac_rate = 3,
		.gain = DEFAULT_ACCEL_GAIN,
		.alpha = 1026019965,
		.a = 47721859,
		},
	{
		.freq = 112,
		.gain = DEFAULT_ACCEL_GAIN_112,
		.rate = 0,
		.bac_rate = 1,
		.alpha = 977872018,
		.a = 95869806,
	},
	{
		.freq = 56,
		.gain = PED_ACCEL_GAIN,
		.rate = 0,
		.bac_rate = 0,
		.alpha = 882002213,
		.a = 191739611,
		},
	{
		.freq = 15,
		.gain = DEFAULT_ACCEL_GAIN,
		.rate = 0,
		.bac_rate = 0,
		.alpha = 357913941,
		.a = 715827883,
	},
	{
		.freq = 5,
		.gain = DEFAULT_ACCEL_GAIN,
		.rate = 0,
		.bac_rate = 0,
		.alpha = 107374182,
		.a = 966367642,
	},
};

static int accel_gyro_rate[] = { 5, 6, 7, 8, 9, 10, 11, 12, 13,
	14, 15, 17, 18, 22, 25, 28, 32, 37, 45,
	56, 75, 112, 225
};

static int inv_set_batchmode(struct inv_mpu_state *st, bool enable)
{
	if (enable)
		st->cntl2 |= BATCH_MODE_EN;

	return 0;
}

static int inv_calc_engine_dur(struct inv_engine_info *ei)
{
	if (!ei->running_rate)
		return -EINVAL;
	ei->dur = ei->base_time / ei->orig_rate;
	ei->dur *= ei->divider;

	return 0;
}

static int inv_batchmode_calc(struct inv_mpu_state *st)
{
	int b, timeout;
	int i, bps;
	enum INV_ENGINE eng;

	bps = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			bps += (st->sensor[i].sample_size + 2) *
				st->sensor[i].rate;
		}
	}
	if (bps) {
		b = st->batch.timeout * bps;
		if ((b > (FIFO_SIZE * MSEC_PER_SEC)) &&
			(!st->batch.overflow_on))
			timeout = FIFO_SIZE * MSEC_PER_SEC / bps;
		else
			timeout = st->batch.timeout;
	} else {
		if (st->chip_config.step_detector_on ||
			st->step_counter_l_on ||
			st->step_counter_wake_l_on ||
			st->chip_config.activity_eng_on) {
			timeout = st->batch.timeout;
		} else {
			return -EINVAL;
		}
	}
	if (st->chip_config.gyro_enable)
		eng = ENGINE_GYRO;
	else if (st->chip_config.accel_enable)
		eng = ENGINE_ACCEL;
	else
		eng = ENGINE_I2C;
	b = st->eng_info[eng].dur / USEC_PER_MSEC;
	st->batch.engine_base = eng;
	st->batch.counter = timeout * USEC_PER_MSEC / b;

	if (st->batch.counter)
		st->batch.on = true;

	return 0;
}

static int inv_set_default_batch(struct inv_mpu_state *st)
{
	if (st->batch.max_rate > DEFAULT_BATCH_RATE) {
		st->batch.default_on = true;
		st->batch.counter = DEFAULT_BATCH_TIME * NSEC_PER_MSEC /
			st->eng_info[ENGINE_GYRO].dur;
	}

	return 0;
}

int inv_batchmode_setup(struct inv_mpu_state *st)
{
	int r;
	bool on;
	s16 mask[ENGINE_NUM_MAX] = { 1, 2, 8, 8 };

	st->batch.on = false;
	st->batch.default_on = false;
	if (st->batch.timeout > 0) {
		r = inv_batchmode_calc(st);
		if (r)
			return r;
	} else {
		r = inv_set_default_batch(st);
		if (r)
			return r;
	}

	on = (st->batch.on || st->batch.default_on);

	if (on) {
		r = write_be32_to_mem(st, 0, BM_BATCH_CNTR);
		if (r)
			return r;
		r = write_be32_to_mem(st, st->batch.counter, BM_BATCH_THLD);
		if (r)
			return r;
		r = inv_write_2bytes(st, BM_BATCH_MASK,
				mask[st->batch.engine_base]);
		if (r)
			return r;
		r = inv_write_2bytes(st, FIFO_WATERMARK, FIFO_SIZE);
		if (r)
			return r;
	} else {
		/* disable FIFO WM interrupt */
		r = inv_write_2bytes(st, FIFO_WATERMARK, HARDWARE_FIFO_SIZE);
		if (r)
			return r;
	}

	r = inv_set_batchmode(st, on);

	return r;
}

static int inv_turn_on_fifo(struct inv_mpu_state *st)
{
	u8 w, x;
	int r;

	/* clear FIFO data */
	r = inv_plat_single_write(st, REG_FIFO_RST, MAX_5_BIT_VALUE);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_FIFO_RST, MAX_5_BIT_VALUE - 1);
	if (r)
		return r;
	w = 0;
	x = 0;
	r = inv_plat_single_write(st, REG_FIFO_EN_2, 0);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_FIFO_EN, 0);
	if (r)
		return r;

	/* turn on user ctrl register */
	w = BIT_DMP_RST;
	r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);
	if (r)
		return r;
	usleep_range(DMP_RESET_TIME, DMP_RESET_TIME + 100);

	/* turn on interrupt */
	w = BIT_DMP_INT_EN;
	r = inv_plat_single_write(st, REG_INT_ENABLE, w);
	if (r)
		return r;

	w = BIT_FIFO_EN;
	w |= BIT_DMP_EN;
	if (st->chip_config.slave_enable && (!st->poke_mode_on))
		w |= BIT_I2C_MST_EN;
	r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);

	return r;
}

/*
 *  inv_reset_fifo() - Reset FIFO related registers.
 */
int inv_reset_fifo(struct inv_mpu_state *st, bool turn_off)
{
	int r, i;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;
	int max_rate = 0;

	r = inv_turn_on_fifo(st);
	if (r)
		return r;

	ts_algo->last_run_time = get_time_ns();
	ts_algo->reset_ts = ts_algo->last_run_time;

	/* Drop first some samples for better timestamping.
	 * The number depends on the frequency not to make
	 * large latency for the first sample at application.
	 */
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on)
			max_rate = max(st->sensor[i].rate, max_rate);
	}
	if (max_rate < 5)
		ts_algo->first_sample = 0;
	else if (max_rate < 100)
		ts_algo->first_sample = 1;
	else
		ts_algo->first_sample = 2;

	st->last_temp_comp_time = ts_algo->last_run_time;
	st->left_over_size = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		st->sensor[i].calib_flag = 0;
		st->sensor[i].time_calib = ts_algo->last_run_time;
	}

	ts_algo->calib_counter = 0;

	return 0;
}

static int inv_turn_on_engine(struct inv_mpu_state *st)
{
	u8 w;
	int r;

	if ((st->chip_config.gyro_enable | st->chip_config.accel_enable)
		&& (!st->poke_mode_on)) {
		w = BIT_PWR_PRESSURE_STBY;
		if (!st->chip_config.gyro_enable)
			w |= BIT_PWR_GYRO_STBY;
		if (!st->chip_config.accel_enable)
			w |= BIT_PWR_ACCEL_STBY;
	} else {
		w = (BIT_PWR_GYRO_STBY |
			BIT_PWR_ACCEL_STBY | BIT_PWR_PRESSURE_STBY);
	}
	r = inv_plat_single_write(st, REG_PWR_MGMT_2, w);
	if (r)
		return r;
	if ((!st->chip_config.eis_enable) || (!st->eis.eis_triggered)) {
		/* secondary cycle mode should be set all the time */
		r = inv_plat_single_write(st, REG_LP_CONFIG, BIT_I2C_MST_CYCLE |
							BIT_ACCEL_CYCLE |
							BIT_GYRO_CYCLE);
		if (r)
			return r;
		inv_set_gyro_sf(st);
	}
	inv_set_bank(st, BANK_SEL_2);
	inv_plat_single_write(st, REG_ODR_ALIGN_EN, BIT_ODR_ALIGN_EN);
	inv_set_bank(st, BANK_SEL_0);
	if (st->chip_config.has_compass) {
		if (st->chip_config.compass_enable)
			r = st->slave_compass->resume(st);
		else
			r = st->slave_compass->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_als) {
		if (st->chip_config.als_enable)
			r = st->slave_als->resume(st);
		else
			r = st->slave_als->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_pressure) {
		if (st->chip_config.pressure_enable)
			r = st->slave_pressure->resume(st);
		else
			r = st->slave_pressure->suspend(st);
		if (r)
			return r;
	}

	/* secondary cycle mode should be set all the time */
	w = BIT_I2C_MST_CYCLE;
	if (st->chip_config.low_power_gyro_on)
		w |= BIT_GYRO_CYCLE;
	w |= BIT_ACCEL_CYCLE;
	if (w != local.reg_lp_config) {
		r = inv_plat_single_write(st, REG_LP_CONFIG, w);
		if (r)
			return r;
		local.reg_lp_config = w;
	}

	return 0;
}

static int inv_setup_dmp_rate(struct inv_mpu_state *st)
{
	int i, result;
	int div[SENSOR_NUM_MAX];
	bool d_flag;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (!st->sensor[i].rate) {
				pr_err("sensor %d rate is zero\n", i);
				return -EINVAL;
			}
			pr_info(
			"[Before]sensor %d rate [%d], running_rate %d\n",
			i, st->sensor[i].rate,
			st->eng_info[st->sensor[i].engine_base].running_rate);

			div[i] =
				st->eng_info[st->sensor[i].engine_base].
				running_rate / st->sensor[i].rate;
			if (!div[i])
				div[i] = 1;
			if (SENSOR_PRESSURE == i) {
				if (st->eng_info[st->sensor[i].engine_base].running_rate
						* 100 / div[i] > MAX_PRESSURE_RATE * 150)
					div[i]++;
			}
			st->sensor[i].rate = st->eng_info
				[st->sensor[i].engine_base].running_rate / div[i];

			pr_info(
			"sensor %d rate [%d] div [%d] running_rate [%d]\n",
				i, st->sensor[i].rate, div[i],
			st->eng_info[st->sensor[i].engine_base].running_rate);
		}
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->cntl |= st->sensor[i].output;
			st->sensor[i].dur =
				st->eng_info[st->sensor[i].engine_base].dur *
				div[i];
			st->sensor[i].div = div[i];
			pr_info("sensor %d div [%d] odr_addr [%02X] cntl [%04X]\n",
				i, div[i], st->sensor[i].odr_addr, st->cntl);
			result = inv_write_2bytes(st,
					st->sensor[i].odr_addr, div[i] - 1);
			if (result)
				return result;
			/* reset all data counter */
			result = inv_write_2bytes(st,
					st->sensor[i].counter_addr, 0);
			if (result)
				return result;
		}
	}

	d_flag = 0;
	for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
		if (st->sensor_accuracy[i].on)
			st->cntl2 |= st->sensor_accuracy[i].output;
		d_flag |= st->sensor_accuracy[i].on;
	}
	d_flag |= st->chip_config.activity_eng_on;
	d_flag |= st->chip_config.pick_up_enable;
	d_flag |= st->chip_config.eis_enable;
	if (d_flag)
		st->cntl |= HEADER2_SET;

	if (st->chip_config.step_indicator_on)
		st->cntl |= PED_STEPIND_SET;
	if (st->chip_config.step_detector_on)
		st->cntl |= PED_STEPDET_SET;
	if (st->chip_config.activity_eng_on) {
		st->cntl2 |= ACT_RECOG_SET;
		st->cntl2 |= SECOND_SEN_OFF_SET;
	}
	if (st->chip_config.pick_up_enable)
		st->cntl2 |= FLIP_PICKUP_SET;

	if (st->chip_config.eis_enable)
		st->cntl2 |= FSYNC_SET;

	if (!st->chip_config.dmp_event_int_on) {
		result = inv_batchmode_setup(st);
		if (result)
			return result;
	} else {
		st->batch.on = false;
	}

	return 0;
}

static int inv_set_div(struct inv_mpu_state *st, int a_d, int g_d)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;

	if (st->chip_config.eis_enable)
		inv_plat_single_write(st, REG_FSYNC_CONFIG, BIT_TRIGGER_EIS);
	else
		inv_plat_single_write(st, REG_FSYNC_CONFIG, 0);

	if (local.reg_gyro_smplrt != g_d) {
		result = inv_plat_single_write(st, REG_GYRO_SMPLRT_DIV, g_d);
		if (result)
			return result;
		local.reg_gyro_smplrt = g_d;
	}
	result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_2, a_d);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_rate(struct inv_mpu_state *st)
{
	int g_d, a_d, result;

	result = inv_setup_dmp_rate(st);
	if (result)
		return result;

	g_d = st->eng_info[ENGINE_GYRO].divider - 1;
	a_d = st->eng_info[ENGINE_ACCEL].divider - 1;
	result = inv_set_div(st, a_d, g_d);

	return result;
}

static int inv_set_fifo_size(struct inv_mpu_state *st)
{
	int result;
	u8 cfg;

	/* use one FIFO in DMP mode */
	cfg = BIT_SINGLE_FIFO_CFG;
	result = inv_plat_single_write(st, REG_FIFO_CFG, cfg);
	if (result)
		return result;

	return 0;
}

/*
 *  inv_set_fake_secondary() - set fake secondary I2C such that
 *                           I2C data in the same position.
 */
static int inv_set_fake_secondary(struct inv_mpu_state *st)
{
	int r;
	u8 bytes, ind;

	/* may need to saturate the master I2C counter like Scorpion did */
	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	if (st->sec_set.delay_enable != local.reg_delay_enable) {
		r = inv_plat_single_write(st, REG_I2C_MST_DELAY_CTRL,
				st->sec_set.delay_enable);
		if (r)
			return r;
		local.reg_delay_enable = st->sec_set.delay_enable;
	}
	if (st->sec_set.delay_time != local.reg_delay_time) {
		r = inv_plat_single_write(st, REG_I2C_SLV4_CTRL,
				st->sec_set.delay_time);
		if (r)
			return r;
		local.reg_delay_time = st->sec_set.delay_time;
	}
	/* odr config is changed during slave setup */
	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
			st->sec_set.odr_config);
	if (r)
		return r;
	r = inv_set_bank(st, BANK_SEL_0);
	if (r)
		return r;

	/*111, 110 */
	if (st->chip_config.compass_enable && st->chip_config.als_enable)
		return 0;
	/* 100 */
	if (st->chip_config.compass_enable &&
		(!st->chip_config.als_enable) && (!st->chip_config.pressure_enable))
		return 0;
	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;

	if (st->chip_config.pressure_enable) {
		/* 001 */
		if ((!st->chip_config.compass_enable) &&
			(!st->chip_config.als_enable)) {
			r = inv_read_secondary(st, 0,
				st->plat_data.aux_i2c_addr,
				BMP280_DIG_T1_LSB_REG,
				DATA_AKM_99_BYTES_DMP);
			if (r)
				return r;
			r = inv_read_secondary(st, 2,
				st->plat_data.aux_i2c_addr,
				BMP280_DIG_T1_LSB_REG,
				DATA_ALS_BYTES_DMP);
			r = inv_set_bank(st, BANK_SEL_0);

			return r;
		}

		if (st->chip_config.compass_enable &&
			(!st->chip_config.als_enable)) {
			/* 101 */
			ind = 2;
			if ((COMPASS_ID_AK09911 ==
				st->plat_data.sec_slave_id) ||
				(COMPASS_ID_AK09912 ==
				st->plat_data.sec_slave_id) ||
				(COMPASS_ID_AK09916 == st->plat_data.sec_slave_id))
				bytes = DATA_ALS_BYTES_DMP;
			else
				bytes = DATA_ALS_BYTES_DMP +
				DATA_AKM_99_BYTES_DMP -
				DATA_AKM_89_BYTES_DMP;
		} else {	/* 011 */
			ind = 0;
			bytes = DATA_AKM_99_BYTES_DMP;
		}
		r = inv_read_secondary(st, ind, st->plat_data.aux_i2c_addr,
			BMP280_DIG_T1_LSB_REG, bytes);
	} else {/* compass disabled; als enabled, pressure disabled 010 */
		r = inv_read_secondary(st, 0, st->plat_data.read_only_i2c_addr,
			APDS9900_AILTL_REG,
			DATA_AKM_99_BYTES_DMP);
	}
	if (r)
		return r;
	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}

static int inv_set_ICM20628_secondary(struct inv_mpu_state *st)
{
	int rate, compass_rate, pressure_rate, als_rate, min_rate, base;
	int mst_odr_config, d, delay;

	if (st->chip_config.compass_enable)
		compass_rate = st->chip_config.compass_rate;
	else
		compass_rate = 0;
	if (st->chip_config.pressure_enable)
		pressure_rate = st->sensor[SENSOR_PRESSURE].rate;
	else
		pressure_rate = 0;
	if (st->chip_config.als_enable)
		als_rate = max(st->sensor[SENSOR_ALS].rate,
			       MPU_INIT_SENSOR_RATE);
	else
		als_rate = 0;
	if (compass_rate)
		rate = compass_rate;
	else
		rate = max(pressure_rate, als_rate);
	mst_odr_config = 0;
	min_rate = BASE_SAMPLE_RATE;
	while (rate < min_rate) {
		mst_odr_config++;
		min_rate >>= 1;
	}
	base = BASE_SAMPLE_RATE / (1 << mst_odr_config);
	if (base < rate) {
		mst_odr_config--;
		base = BASE_SAMPLE_RATE / (1 << mst_odr_config);
	}
	if (mst_odr_config < MIN_MST_ODR_CONFIG)
		mst_odr_config = MIN_MST_ODR_CONFIG;
	if (compass_rate) {
		if (mst_odr_config > MAX_MST_ODR_CONFIG)
			mst_odr_config = MAX_MST_ODR_CONFIG;
	} else {
		if (mst_odr_config > MAX_MST_NON_COMPASS_ODR_CONFIG)
			mst_odr_config = MAX_MST_NON_COMPASS_ODR_CONFIG;
	}

	base = BASE_SAMPLE_RATE / (1 << mst_odr_config);

	if ((!st->chip_config.gyro_enable) && (!st->chip_config.accel_enable)) {
		st->eng_info[ENGINE_I2C].running_rate = base;
		st->eng_info[ENGINE_I2C].divider = (1 << mst_odr_config);
	}
	pr_debug("%s compass_running_rate %d  base %d\n",
		__func__, st->eng_info[ENGINE_I2C].running_rate, base);

	inv_calc_engine_dur(&st->eng_info[ENGINE_I2C]);

	d = 0;
	if (d > 0)
		d -= 1;
	if (d > MAX_5_BIT_VALUE)
		d = MAX_5_BIT_VALUE;

	/* I2C_MST_DLY is set to slow down secondary I2C */
	if (d)
		delay = 0x1F;
	else
		delay = 0;

	st->sec_set.delay_enable = delay;
	st->sec_set.delay_time = d;
	st->sec_set.odr_config = mst_odr_config;

	pr_info("mst_odr_conf %X no modified\n", mst_odr_config);

	return 0;
}

static int inv_set_master_delay(struct inv_mpu_state *st)
{

	if (!st->chip_config.slave_enable)
		return 0;
	inv_set_ICM20628_secondary(st);

	return 0;
}

static void inv_enable_accel_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	if (enable)
		st->motion_event_cntl |= (ACCEL_CAL_EN);
}

static void inv_enable_gyro_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	if (enable)
		st->motion_event_cntl |= (GYRO_CAL_EN);
}

static void inv_enable_compass_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	if (enable)
		st->motion_event_cntl |= COMPASS_CAL_EN;
}

static int inv_set_wom(struct inv_mpu_state *st)
{
	int result;
	u8 d[4] = { 0, 0, 0, 0 };

	if (st->chip_config.wom_on)
		d[3] = 1;

	if (local.wom_on != st->chip_config.wom_on) {
		result = mem_w(WOM_ENABLE, ARRAY_SIZE(d), d);
		if (result)
			return result;
		local.wom_on = st->chip_config.wom_on;
	}

	inv_write_2bytes(st, DATA_RDY_STATUS, st->chip_config.gyro_enable |
			(st->chip_config.accel_enable << 1) |
			(st->chip_config.slave_enable << 3));

	return 0;
}

static void inv_setup_events(struct inv_mpu_state *st)
{
	if (st->ped.engine_on)
		st->motion_event_cntl |= (PEDOMETER_EN);
	if (st->smd.on)
		st->motion_event_cntl |= (SMD_EN);
	if (st->ped.int_on)
		st->motion_event_cntl |= (PEDOMETER_INT_EN);
	if (st->chip_config.pick_up_enable)
		st->motion_event_cntl |= (FLIP_PICKUP_EN);
	if (st->chip_config.geomag_enable)
		st->motion_event_cntl |= GEOMAG_RV_EN;
	if (st->sensor[SENSOR_NINEQ].on)
		st->motion_event_cntl |= NINE_AXIS_EN;
	if (!st->chip_config.activity_eng_on)
		st->motion_event_cntl |= BAC_ACCEL_ONLY_EN;
}

static int inv_setup_sensor_interrupt(struct inv_mpu_state *st)
{
	int i, ind, rate;

	ind = -1;
	rate = 0;

	st->intr_cntl = 0;
	if (st->batch.on) {
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (st->sensor[i].on)
				st->intr_cntl |= st->sensor[i].output;
		}
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (st->sensor[i].rate > rate) {
				ind = i;
				rate = st->sensor[i].rate;
			}
		}
	}

	if (ind != -1)
		st->intr_cntl |= st->sensor[ind].output;
	if (st->chip_config.step_detector_on)
		st->intr_cntl |= PED_STEPDET_SET;
	if (st->chip_config.activity_eng_on)
		st->intr_cntl |= HEADER2_SET;
	if (st->chip_config.pick_up_enable)
		st->intr_cntl |= HEADER2_SET;
	if (st->chip_config.eis_enable)
		st->intr_cntl |= HEADER2_SET;

	pr_debug("eis for Header2 %s\n",
		st->chip_config.eis_enable == 1 ? "Enabled" : "Disabled");
	return inv_write_2bytes(st, DATA_INTR_CTL, st->intr_cntl);
}

static int inv_mpu_reset_pickup(struct inv_mpu_state *st)
{
	int result;

	result = write_be32_to_mem(st, 0, FP_PICKUP_CNTR);
	if (result)
		return result;

	return 0;
}

static int inv_setup_dmp(struct inv_mpu_state *st)
{
	int result, i, tmp, min_diff, ind;

	result = inv_setup_sensor_interrupt(st);
	if (result)
		return result;

	i = 0;
	ind = 0;
	min_diff = accel_cal_para[0].freq;
	while (i < ARRAY_SIZE(accel_cal_para)) {
		tmp = abs(accel_cal_para[i].freq -
				st->eng_info[ENGINE_ACCEL].running_rate);
		if (tmp < min_diff) {
			min_diff = tmp;
			ind = i;
		}
		i++;
	}
	i = ind;
		result = inv_write_2bytes(st, BAC_RATE,
					accel_cal_para[i].bac_rate);
		if (result)
			return result;
		result = write_be32_to_mem(st,
					accel_cal_para[i].bac_rate, FP_RATE);
		if (result)
			return result;

		result = inv_write_2bytes(st,
					ACCEL_CAL_RATE,
					accel_cal_para[i].rate);
		if (result)
			return result;
		result = write_be32_to_mem(st,
					accel_cal_para[i].alpha,
					ACCEL_ALPHA_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st,
					accel_cal_para[i].a,
					ACCEL_A_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st,
					accel_cal_para[i].gain,
					ACCEL_ONLY_GAIN);
		if (result)
			return result;
		local.accel_cal_ind = i;

	i = 0;
	min_diff = compass_cal_param[0];
	while (i < ARRAY_SIZE(compass_cal_param)) {
		tmp = abs(compass_cal_param[i] -
			st->eng_info[ENGINE_I2C].running_rate);
		if (tmp < min_diff) {
			min_diff = tmp;
			ind = i;
		}
		i++;
	}

	if ((st->eng_info[ENGINE_I2C].running_rate)) {
		result =
			inv_write_2bytes(st, CPASS_TIME_BUFFER,
				compass_cal_param[ind]);
		if (result)
			return result;
	}

	if (st->chip_config.activity_eng_on) {
		result = write_be32_to_mem(st, 0, BAC_STATE);
		if (result)
			return result;

		result = write_be32_to_mem(st, 0, BAC_STATE_PREV);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_ACT_ON);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_ACT_OFF);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_STILL_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_RUN_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_DRIVE_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_WALK_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_SMD_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_BIKE_S_F);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_E1_SHORT);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_E2_SHORT);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_E3_SHORT);
		if (result)
			return result;
		result = write_be32_to_mem(st, 0, BAC_VAR_RUN);
		if (result)
			return result;
		result = write_be32_to_mem(st, 1, BAC_TILT_INIT);
		if (result)
			return result;

		local.activity_eng_on = st->chip_config.activity_eng_on;
	}

	inv_enable_accel_cal_V3(st, st->accel_cal_enable);
	inv_enable_gyro_cal_V3(st, st->gyro_cal_enable);
	inv_enable_compass_cal_V3(st, st->calib_compass_on);

	if (st->ped.engine_on) {
		result = write_be32_to_mem(st, 0, DMPRATE_CNTR);
		if (result)
			return result;
		result = write_be16_to_mem(st, 0, PEDSTEP_IND);
		if (result)
			return result;
	}

	if (st->chip_config.pick_up_enable) {
		result = inv_mpu_reset_pickup(st);
		if (result)
			return result;
	}

	inv_setup_events(st);

	result = inv_set_wom(st);
	if (result)
		return result;

	result = inv_write_2bytes(st, DATA_OUT_CTL1, st->cntl);
	if (result)
		return result;
	result = inv_write_2bytes(st, DATA_OUT_CTL2, st->cntl2);
	if (result)
		return result;
	result = inv_write_2bytes(st, MOTION_EVENT_CTL, st->motion_event_cntl);

	if (st->chip_config.gyro_enable) {
		if (st->eng_info[ENGINE_GYRO].running_rate ==
			MPU_DEFAULT_DMP_FREQ)
			result = write_be32_to_mem(st, st->gyro_sf, GYRO_SF);
		else
			result = write_be32_to_mem(st,
						st->gyro_sf << 1, GYRO_SF);
	}
	pr_info("setup DMP  cntl [%04X] cntl2 [%04X] motion_event [%04X]",
				st->cntl, st->cntl2, st->motion_event_cntl);

	return result;
}

static int inv_get_accel_gyro_rate(int compass_rate)
{
	int i;

	i = 0;
	while ((i < ARRAY_SIZE(accel_gyro_rate)) &&
		compass_rate > accel_gyro_rate[i])
		i++;

	return accel_gyro_rate[i];
}

static int inv_determine_engine(struct inv_mpu_state *st)
{
	int i;
	bool a_en, g_en, c_en, p_en, data_on, ped_on;
	int compass_rate, nineq_rate, accel_rate, gyro_rate;

#define MIN_GYRO_RATE         112

	a_en = false;
	g_en = false;
	c_en = false;
	p_en = false;
	ped_on = false;
	data_on = false;
	compass_rate = NINEQ_MIN_COMPASS_RATE;
	nineq_rate = 0;
	gyro_rate = MIN_GYRO_RATE;
	accel_rate = PEDOMETER_FREQ;

	st->chip_config.geomag_enable = 0;
	if (st->sensor[SENSOR_NINEQ].on)
		nineq_rate = max(nineq_rate, NINEQ_MIN_COMPASS_RATE);
	if (st->sensor[SENSOR_GEOMAG].on) {
		st->chip_config.geomag_enable = 1;
		nineq_rate = max(nineq_rate, GEOMAG_MIN_COMPASS_RATE);
	}
	/* loop the streaming sensors to see which engine needs to be turned on
	 */
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			data_on = true;
			a_en |= st->sensor[i].a_en;
			g_en |= st->sensor[i].g_en;
			c_en |= st->sensor[i].c_en;
			p_en |= st->sensor[i].p_en;
			if (st->sensor[i].c_en)
				compass_rate =
					max(compass_rate, st->sensor[i].rate);
			if (st->sensor[i].p_en)
				compass_rate =
					max(compass_rate, st->sensor[i].rate);
		}
	}
	g_en |= st->secondary_gyro_on;

	/* tilt and activity both uses activity engine */
	st->chip_config.activity_eng_on = (st->chip_config.activity_on |
					st->chip_config.tilt_enable);
	/* step detector and step detector wake and pedometer in batch mode
	 * all use step detector sensor in the base sensor of invensense
	 */
	if (st->step_detector_l_on ||
		st->step_detector_wake_l_on || (st->ped.on && st->batch.timeout && st->ped.int_mode))
		st->chip_config.step_detector_on = true;
	else
		st->chip_config.step_detector_on = false;
	/* ped_on engine is on depends on the step detector and step indicator
	 * and activity engine on
	 */
	if (st->chip_config.step_detector_on ||
		st->chip_config.step_indicator_on ||
		st->chip_config.activity_eng_on) {
		ped_on = true;
		data_on = true;
	}
	/* smd depends on ped engine */
	if (st->smd.on)
		ped_on = true;

	/* pedometer interrupt is enabled when ped int mode */
	if (st->ped.int_mode && st->ped.on)
		st->ped.int_on = 1;
	else
		st->ped.int_on = 0;

	if (st->ped.on || ped_on)
		st->ped.engine_on = true;
	else
		st->ped.engine_on = false;
	/* ped engine needs accel engine on */
	if (st->ped.engine_on)
		a_en = true;
	/* pick up needs accel engine on */
	if (st->chip_config.pick_up_enable)
		a_en = true;
	/* eis needs gyro engine. gyro rate is 225 by default.
	 * eis_triggerred set to be false when it is turned false
	 */
	if (st->chip_config.eis_enable) {
		g_en = true;
		gyro_rate = MPU_DEFAULT_DMP_FREQ;
		st->eis.frame_count = 0;
		st->eis.fsync_delay = 0;
		st->eis.gyro_counter = 0;
		st->eis.voting_count = 0;
		st->eis.voting_count_sub = 0;
	} else {
		st->eis.eis_triggered = false;
	}

	if (data_on)
		st->chip_config.dmp_event_int_on = 0;
	else
		st->chip_config.dmp_event_int_on = 1;

	if (st->chip_config.dmp_event_int_on)
		st->chip_config.wom_on = 1;
	else
		st->chip_config.wom_on = 0;

	if (compass_rate < nineq_rate)
		compass_rate = nineq_rate;
	st->chip_config.compass_rate = compass_rate;
	if (st->sensor[SENSOR_SIXQ].on || st->sensor[SENSOR_NINEQ].on ||
		st->sensor[SENSOR_PEDQ].on || st->sensor[SENSOR_GEOMAG].on) {
		/* if 6 Q or 9 Q is on, set gyro/accel to default rate */
		accel_rate = MPU_DEFAULT_DMP_FREQ;
		gyro_rate = MPU_DEFAULT_DMP_FREQ;
	} else {
		/* determine the fastest accel engine rate when it is not
		 * using default DMP frequency
		 */
		if (st->sensor[SENSOR_ACCEL].on) {
			accel_rate = max(accel_rate,
						st->sensor[SENSOR_ACCEL].rate);
		}
		if (st->sensor[SENSOR_GYRO].on)
			gyro_rate = max(gyro_rate,
					st->sensor[SENSOR_GYRO].rate);
	}

	if (compass_rate < MIN_COMPASS_RATE)
		compass_rate = MIN_COMPASS_RATE;
	st->ts_algo.clock_base = ENGINE_I2C;
	if (c_en && (!g_en) && (!a_en)) {
		a_en = true;
		accel_rate = compass_rate;
	}
	if (g_en) {
		/* gyro engine needs to be fastest */
		if (a_en)
			gyro_rate = max(gyro_rate, accel_rate);
		if (c_en || p_en) {
			if (gyro_rate < compass_rate)
				gyro_rate =
					inv_get_accel_gyro_rate(compass_rate);
		}
		accel_rate = gyro_rate;
		compass_rate = gyro_rate;
		st->ts_algo.clock_base = ENGINE_GYRO;
		st->sensor[SENSOR_COMPASS].engine_base = ENGINE_GYRO;
		st->sensor[SENSOR_COMPASS_CAL].engine_base = ENGINE_GYRO;
	} else if (a_en) {
		/* accel engine needs to be fastest if gyro engine is off */
		if (c_en || p_en) {
			if (accel_rate <= compass_rate)
				accel_rate =
					inv_get_accel_gyro_rate(compass_rate);
		}
		compass_rate = accel_rate;
		gyro_rate = accel_rate;
		st->ts_algo.clock_base = ENGINE_ACCEL;
		st->sensor[SENSOR_COMPASS].engine_base = ENGINE_ACCEL;
		st->sensor[SENSOR_COMPASS_CAL].engine_base = ENGINE_ACCEL;
	}

	st->eng_info[ENGINE_GYRO].running_rate = gyro_rate;
	st->eng_info[ENGINE_ACCEL].running_rate = accel_rate;
	st->eng_info[ENGINE_PRESSURE].running_rate = MPU_DEFAULT_DMP_FREQ;
	st->eng_info[ENGINE_I2C].running_rate = compass_rate;
	/* engine divider for pressure and compass is set later */
	st->eng_info[ENGINE_GYRO].divider =
		(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
		(MPU_DEFAULT_DMP_FREQ / st->eng_info[ENGINE_GYRO].running_rate);
	st->eng_info[ENGINE_ACCEL].divider =
		(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
		(MPU_DEFAULT_DMP_FREQ / st->eng_info[ENGINE_ACCEL].running_rate);
	if (g_en)
		st->eng_info[ENGINE_I2C].divider =
		(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
			(MPU_DEFAULT_DMP_FREQ /
				st->eng_info[ENGINE_GYRO].running_rate);
	else
		st->eng_info[ENGINE_I2C].divider =
		(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
			(MPU_DEFAULT_DMP_FREQ /
				st->eng_info[ENGINE_ACCEL].running_rate);

	for (i = 0; i < SENSOR_L_NUM_MAX; i++)
		st->sensor_l[i].counter = 0;

	inv_calc_engine_dur(&st->eng_info[ENGINE_GYRO]);
	inv_calc_engine_dur(&st->eng_info[ENGINE_ACCEL]);

	if (st->debug_determine_engine_on)
		return 0;

	st->chip_config.gyro_enable = g_en;
	st->gyro_cal_enable = g_en;

	st->chip_config.accel_enable = a_en;
	st->accel_cal_enable = a_en;

	st->chip_config.compass_enable = c_en;
	st->calib_compass_on = c_en;

	st->chip_config.pressure_enable = p_en;
	st->chip_config.dmp_on = 1;
	pr_info("gen: %d aen: %d cen: %d grate: %d arate: %d\n",
				g_en, a_en, c_en, gyro_rate, accel_rate);
	if (st->sensor[SENSOR_ALS].on || st->secondary_prox_on)
		st->chip_config.als_enable = 1;
	else
		st->chip_config.als_enable = 0;

	if (c_en || st->chip_config.als_enable || p_en)
		st->chip_config.slave_enable = 1;
	else
		st->chip_config.slave_enable = 0;

	/* setting up accel accuracy output */
	if (st->sensor[SENSOR_ACCEL].on || st->sensor[SENSOR_NINEQ].on ||
				st->sensor[SENSOR_SIXQ].on)
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = false;

	if (st->sensor[SENSOR_GYRO].on || st->sensor[SENSOR_NINEQ].on ||
				st->sensor[SENSOR_SIXQ].on)
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = false;

	/* Setting up accurcy outpu of compass */
	if (st->sensor[SENSOR_COMPASS].on || st->sensor[SENSOR_COMPASS_CAL].on
		|| st->sensor[SENSOR_NINEQ].on)
		st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on = false;

	st->cntl = 0;
	st->cntl2 = 0;
	st->motion_event_cntl = 0;
	st->intr_cntl = 0;
	st->send_raw_compass = false;

	inv_set_master_delay(st);

	return 0;
}

/*
 *  set_inv_enable() - enable function.
 */
int set_inv_enable(struct iio_dev *indio_dev)
{
	int result;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	u8 w;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = inv_stop_dmp(st);
	if (result)
		return result;

	inv_determine_engine(st);
	if (!st->secondary_switch) {
		result = inv_set_rate(st);
		if (result) {
			pr_err("inv_set_rate error\n");
			return result;
		}
	}
	if (!st->secondary_switch) {
		result = inv_setup_dmp(st);
		if (result) {
			pr_err("setup dmp error\n");
			return result;
		}
	}
	result = inv_turn_on_engine(st);
	if (result) {
		pr_err("inv_turn_on_engine error\n");
		return result;
	}
	if (!st->secondary_switch) {
		result = inv_set_fifo_size(st);
		if (result) {
			pr_err("inv_set_fifo_size error\n");
			return result;
		}
	}
	if ((!st->secondary_switch) || (st->secondary_switch &&
			(st->secondary_switch_data != SENCONDARY_GYRO_OFF) &&
			(st->secondary_switch_data != SENCONDARY_GYRO_ON))) {
		result = inv_set_fake_secondary(st);
		if (result)
			return result;
	}
	if (!st->secondary_switch) {
		result = inv_reset_fifo(st, false);
		if (result)
			return result;
	} else {
		/* Recover only REG_USER_CTRL */
		w = BIT_FIFO_EN;
		w |= BIT_DMP_EN;
		if (st->chip_config.slave_enable && (!st->poke_mode_on))
			w |= BIT_I2C_MST_EN;
		result = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);
		if (result)
			return result;
	}
	result = inv_switch_power_in_lp(st, false);
	if ((!st->chip_config.gyro_enable) &&
		(!st->chip_config.accel_enable) &&
		(!st->chip_config.slave_enable) &&
		(!st->chip_config.pressure_enable)) {
		inv_set_power(st, false);

		return 0;
	}

	return result;
}

static int inv_set_flip_pickup_gesture_params_V3(struct inv_mpu_state *st)
{
	int var_error_alpha;
	int still_threshold;
	int middle_still_threshold;
	int not_still_threshold;
	int vibration_rejection_threshold;
	int maximum_pickup_time_threshold;
	int pickup_timeout_threshold;
	int still_consistency_count_threshold;
	int motion_consistency_count_threshold;
	int vibration_count_threshold;
	int steady_tilt_threshold;
	int steady_tilt_upper_threshold;
	int accel_z_flat_threshold_minus;
	int accel_z_flat_threshold_plus;
	int device_in_pocket_threshold;
	int result;

	var_error_alpha = 107374182L;
	still_threshold = 4L;
	middle_still_threshold = 10L;
	not_still_threshold = 40L;
	vibration_rejection_threshold = 65100L;
	maximum_pickup_time_threshold = 30L;
	pickup_timeout_threshold = 150L;
	still_consistency_count_threshold = 80;
	motion_consistency_count_threshold = 10;
	vibration_count_threshold = 3;
	steady_tilt_threshold = 6710886L;
	steady_tilt_upper_threshold = 140928614L;
	accel_z_flat_threshold_minus = 60397978L;
	accel_z_flat_threshold_plus = 6710886L;
	device_in_pocket_threshold = 100L;

	result = write_be32_to_mem(st, var_error_alpha, FP_VAR_ALPHA);
	result += write_be32_to_mem(st, still_threshold, FP_STILL_TH);
	result += write_be32_to_mem(st, middle_still_threshold,
				FP_MID_STILL_TH);
	result += write_be32_to_mem(st, not_still_threshold, FP_NOT_STILL_TH);
	result += write_be32_to_mem(st, vibration_rejection_threshold,
				FP_VIB_REJ_TH);
	result += write_be32_to_mem(st, maximum_pickup_time_threshold,
				FP_MAX_PICKUP_T_TH);
	result += write_be32_to_mem(st, pickup_timeout_threshold,
				FP_PICKUP_TIMEOUT_TH);
	result += write_be32_to_mem(st, still_consistency_count_threshold,
				FP_STILL_CONST_TH);
	result += write_be32_to_mem(st, motion_consistency_count_threshold,
				FP_MOTION_CONST_TH);
	result += write_be32_to_mem(st, vibration_count_threshold,
				FP_VIB_COUNT_TH);
	result += write_be32_to_mem(st, steady_tilt_threshold,
				FP_STEADY_TILT_TH);
	result += write_be32_to_mem(st, steady_tilt_upper_threshold,
				FP_STEADY_TILT_UP_TH);
	result += write_be32_to_mem(st, accel_z_flat_threshold_minus,
				FP_Z_FLAT_TH_MINUS);
	result += write_be32_to_mem(st, accel_z_flat_threshold_plus,
				FP_Z_FLAT_TH_PLUS);
	result += write_be32_to_mem(st, device_in_pocket_threshold,
				FP_DEV_IN_POCKET_TH);
	return result;
}

int inv_write_compass_matrix(struct inv_mpu_state *st, int *adj)
{
	int addr[] = {CPASS_MTX_00, CPASS_MTX_01, CPASS_MTX_02,
			CPASS_MTX_10, CPASS_MTX_11, CPASS_MTX_12,
			CPASS_MTX_20, CPASS_MTX_21, CPASS_MTX_22};
	int r, i;

	for (i = 0; i < 9; i++) {
		r = write_be32_to_mem(st, adj[i], addr[i]);
		if (r)
			return r;
	}

	return 0;
}

static int inv_compass_dmp_cal(struct inv_mpu_state *st)
{
	s8 *compass_m, *m;
	s8 trans[NINE_ELEM];
	s32 tmp_m[NINE_ELEM];
	int i, j, k;
	int sens[THREE_AXES];
	int *adj;
	int scale, shift, r;

	compass_m = st->plat_data.secondary_orientation;
	m = st->plat_data.orientation;
	for (i = 0; i < THREE_AXES; i++)
		for (j = 0; j < THREE_AXES; j++)
			trans[THREE_AXES * j + i] = m[THREE_AXES * i + j];

	adj = st->current_compass_matrix;
	st->slave_compass->get_scale(st, &scale);

	if ((COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) ||
		(COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) ||
		(COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) ||
		(COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) ||
		(COMPASS_ID_AK09916 == st->plat_data.sec_slave_id))
		shift = AK89XX_SHIFT;
	else
		shift = AK99XX_SHIFT;

	for (i = 0; i < THREE_AXES; i++) {
		sens[i] = st->chip_info.compass_sens[i] + 128;
		sens[i] = inv_q30_mult(sens[i] << shift, scale);
	}

	for (i = 0; i < NINE_ELEM; i++) {
		adj[i] = compass_m[i] * sens[i % THREE_AXES];
		tmp_m[i] = 0;
	}
	for (i = 0; i < THREE_AXES; i++)
		for (j = 0; j < THREE_AXES; j++)
			for (k = 0; k < THREE_AXES; k++)
				tmp_m[THREE_AXES * i + j] +=
					trans[THREE_AXES * i + k] *
					adj[THREE_AXES * k + j];

	for (i = 0; i < NINE_ELEM; i++)
		st->final_compass_matrix[i] = adj[i];

	r = inv_write_compass_matrix(st, tmp_m);

	return 0;
}

static int inv_write_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = write_be32_to_mem(st, st->gyro_sf, GYRO_SF);

	return result;
}

int inv_write_accel_sf(struct inv_mpu_state *st)
{
	int result;

	result = write_be32_to_mem(st,
		(DMP_ACCEL_SCALE_2G << st->chip_config.accel_fs), ACC_SCALE);
	if (result)
		return result;
	result = write_be32_to_mem(st,
		(DMP_ACCEL_SCALE2_2G >> st->chip_config.accel_fs), ACC_SCALE2);

	return result;
}

int inv_setup_dmp_firmware(struct inv_mpu_state *st)
{
	int result;
	u8 v[2] = {0, 0};

	result = mem_w(DATA_OUT_CTL1, 2, v);
	if (result)
		return result;
	result = mem_w(DATA_OUT_CTL2, 2, v);
	if (result)
		return result;
	result = mem_w(MOTION_EVENT_CTL, 2, v);
	if (result)
		return result;
	result = write_be32_to_mem(st, 12000, SMD_VAR_TH);
	if (result)
		return result;

	result = inv_write_accel_sf(st);
	if (result)
		return result;
	result = inv_write_gyro_sf(st);
	if (result)
		return result;

	if (st->chip_config.has_compass) {
		result = inv_compass_dmp_cal(st);
		if (result)
			return result;
	}
	result = inv_set_flip_pickup_gesture_params_V3(st);

	return result;
}

static int inv_save_interrupt_config(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_read(st, REG_INT_ENABLE, 1, &st->int_en);

	return res;
}

int inv_stop_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_save_interrupt_config(st);
	if (res)
		return res;

	res = inv_plat_single_write(st, REG_INT_ENABLE, 0);

	return res;
}

int inv_restore_interrupt(struct inv_mpu_state *st)
{
	int res;

	res = inv_plat_single_write(st, REG_INT_ENABLE, st->int_en);

	return res;
}

int inv_stop_stream_interrupt(struct inv_mpu_state *st)
{
	int res;
	int i;
	u16 cntl = st->intr_cntl;

	/* keep streaming if any gesture reported by FIFO data is enabled */
	if (st->chip_config.tilt_enable ||
			st->chip_config.pick_up_enable ||
			st->step_detector_l_on ||
			st->step_detector_wake_l_on ||
			st->chip_config.activity_on)
		return 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		cntl &= ~st->sensor[i].output;

	res = inv_switch_power_in_lp(st, true);
	if (res)
		return res;
	res = inv_write_2bytes(st, DATA_INTR_CTL, cntl);
	if (res)
		return res;
	res = inv_switch_power_in_lp(st, false);

	return res;
}

int inv_restore_stream_interrupt(struct inv_mpu_state *st)
{
	int res;

	/* skip as did nothing in inv_stop_stream_interrupt() */
	if (st->chip_config.tilt_enable ||
			st->chip_config.pick_up_enable ||
			st->step_detector_l_on ||
			st->step_detector_wake_l_on ||
			st->chip_config.activity_on)
		return 0;

	res = inv_switch_power_in_lp(st, true);
	if (res)
		return res;
	res = inv_write_2bytes(st, DATA_INTR_CTL, st->intr_cntl);
	if (res)
		return res;
	res = inv_switch_power_in_lp(st, false);

	return res;
}

int inv_read_offset_regs(struct inv_mpu_state *st, s16 accel[3], s16 gyro[3])
{
	int res = 0;
	u8 data[2];

	/* accel */
	res = inv_set_bank(st, BANK_SEL_1);
	if (res)
		goto restore_bank;

	res = inv_plat_read(st, REG_XA_OFFS_H, 2, data);
	if (res)
		goto restore_bank;
	accel[0] = (data[0] << 8) | data[1];

	res = inv_plat_read(st, REG_YA_OFFS_H, 2, data);
	if (res)
		goto restore_bank;
	accel[1] = (data[0] << 8) | data[1];

	res = inv_plat_read(st, REG_ZA_OFFS_H, 2, data);
	if (res)
		goto restore_bank;
	accel[2] = (data[0] << 8) | data[1];

	pr_info("read accel offset regs: %d, %d, %d\n",
			accel[0], accel[1], accel[2]);

	/* gyro */
	res = inv_set_bank(st, BANK_SEL_2);
	if (res)
		goto restore_bank;

	res = inv_plat_read(st, REG_XG_OFFS_USR_H, 2, data);
	if (res)
		goto restore_bank;
	gyro[0] = (data[0] << 8) | data[1];

	res = inv_plat_read(st, REG_YG_OFFS_USR_H, 2, data);
	if (res)
		goto restore_bank;
	gyro[1] = (data[0] << 8) | data[1];

	res = inv_plat_read(st, REG_ZG_OFFS_USR_H, 2, data);
	if (res)
		goto restore_bank;
	gyro[2] = (data[0] << 8) | data[1];

	pr_info("read gyro offset regs: %d, %d, %d\n",
			gyro[0], gyro[1], gyro[2]);

restore_bank:
	inv_set_bank(st, BANK_SEL_0);

	return res;
}

int inv_write_offset_regs(struct inv_mpu_state *st, const s16 accel[3], const s16 gyro[3])
{
	int res = 0;

	/* accel */
	res = inv_set_bank(st, BANK_SEL_1);
	if (res)
		goto restore_bank;

	res = inv_plat_single_write(st, REG_XA_OFFS_H,
			(accel[0] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_XA_OFFS_H + 1,
			accel[0] & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_YA_OFFS_H,
			(accel[1] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_YA_OFFS_H + 1,
			accel[1] & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_ZA_OFFS_H,
			(accel[2] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_ZA_OFFS_H + 1,
			accel[2] & 0xff);
	if (res)
		goto restore_bank;

	pr_info("write accel offset regs: %d, %d, %d\n",
			accel[0], accel[1], accel[2]);

	/* gyro */
	res = inv_set_bank(st, BANK_SEL_2);
	if (res)
		goto restore_bank;

	res = inv_plat_single_write(st, REG_XG_OFFS_USR_H,
			(gyro[0] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_XG_OFFS_USR_H + 1,
			gyro[0] & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_YG_OFFS_USR_H,
			(gyro[1] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_YG_OFFS_USR_H + 1,
			gyro[1] & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_ZG_OFFS_USR_H,
			(gyro[2] >> 8) & 0xff);
	if (res)
		goto restore_bank;
	res = inv_plat_single_write(st, REG_ZG_OFFS_USR_H + 1,
			gyro[2] & 0xff);
	if (res)
		goto restore_bank;

	pr_info("write gyro offset regs: %d, %d, %d\n",
			gyro[0], gyro[1], gyro[2]);

restore_bank:
	inv_set_bank(st, BANK_SEL_0);

	return res;
}
