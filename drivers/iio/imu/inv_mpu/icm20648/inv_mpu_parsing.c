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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/math64.h>

#include "../inv_mpu_iio.h"

static char iden[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

static int inv_apply_soft_iron(struct inv_mpu_state *st, s16 *out_1, s32 *out_2)
{
	int *r, i, j;
	s64 tmp;

	r = st->final_compass_matrix;
	for (i = 0; i < THREE_AXES; i++) {
		tmp = 0;
		for (j = 0; j < THREE_AXES; j++)
			tmp  +=
			(s64)r[i * THREE_AXES + j] * (((int)out_1[j]) << 16);
		out_2[i] = (int)(tmp >> 30);
	}

	return 0;
}

static int inv_process_gyro(struct inv_mpu_state *st, u8 *d, u64 t)
{
	s16 raw[3];
	s16 bias[3];
	s32 calib[3];
	int i;

	for (i = 0; i < 3; i++)
		raw[i] = be16_to_cpup((__be16 *) (d + i * 2));
	for (i = 0; i < 3; i++)
		bias[i] = be16_to_cpup((__be16 *) (d + i * 2 + 6));
	for (i = 0; i < 3; i++) {
		calib[i] = raw[i];
		calib[i] <<= 15;
		calib[i] -= (bias[i] << 10);
	}
	inv_push_gyro_data(st, raw, calib, t);

	return 0;
}
static int inv_push_sensor(struct inv_mpu_state *st, int ind, u64 t, u8 *d)
{
	int i, res;
	s16 out_1[3];
	s32 out_2[3];
	u16 accur;
#ifdef ACCEL_BIAS_TEST
	s16 acc[3], avg[3];
#endif

	res = 0;
	switch (ind) {
	case SENSOR_ACCEL:
		inv_convert_and_push_8bytes(st, ind, d, t, iden);
#ifdef ACCEL_BIAS_TEST
		acc[0] = be16_to_cpup((__be16 *) (d));
		acc[1] = be16_to_cpup((__be16 *) (d + 2));
		acc[2] = be16_to_cpup((__be16 *) (d + 4));
		if (inv_get_3axis_average(acc, avg, 0))
			pr_debug("accel 200 samples average = %5d, %5d, %5d\n", avg[0], avg[1], avg[2]);
#endif
		break;
	case SENSOR_GYRO:
		inv_process_gyro(st, d, t);
		break;
	case SENSOR_COMPASS_CAL:
		for (i = 0; i < 3; i++)
			out_2[i] = be32_to_int(d + i * 4);

		if ((out_2[0] == BAD_CAL_COMPASS_DATA) &&
			(out_2[1] == BAD_CAL_COMPASS_DATA) &&
			(out_2[2] == BAD_CAL_COMPASS_DATA)) {
			pr_info("Bad cal compass\n");
			return 0;
		}
		inv_convert_and_push_16bytes(st, ind, d, t,
						st->plat_data.orientation);
		/* raw sensor needs to be sent after calib sensor */
		if (st->send_raw_compass) {
			inv_push_16bytes_buffer(st, SENSOR_COMPASS,
					st->sensor[SENSOR_COMPASS].ts,
					st->raw_compass_data, 0);
			st->send_raw_compass = false;
		}
		break;
	case SENSOR_COMPASS:
		st->send_raw_compass = true;
		for (i = 0; i < 3; i++)
			out_1[i] = be16_to_cpup((__be16 *) (d + i * 2));
		/* bad compass handling */
		if ((out_1[0] == BAD_COMPASS_DATA) &&
			(out_1[1] == BAD_COMPASS_DATA) &&
			(out_1[2] == BAD_COMPASS_DATA)) {
			pr_info("Bad compass data\n");
			return 0;
		}
		if (st->poke_mode_on) {
			for (i = 0; i < 3; i++)
				out_2[i] = out_1[i];
			inv_push_16bytes_buffer(st, ind, t, out_2, 0);
		} else {
			inv_apply_soft_iron(st, out_1, out_2);
			memcpy(st->raw_compass_data, out_2, 12);
		}
		break;
	case SENSOR_ALS:
		for (i = 0; i < 8; i++)
			st->fifo_data[i] = d[i];
		if (st->chip_config.has_als) {
			res = st->slave_als->read_data(st, out_1);
			inv_push_8bytes_buffer(st, ind, t, out_1);

			return res;
		}
		break;
	case SENSOR_SIXQ:
		inv_convert_and_push_16bytes(st, ind, d, t, iden);
		break;
	case SENSOR_NINEQ:
	case SENSOR_GEOMAG:
		for (i = 0; i < 3; i++)
			out_2[i] = be32_to_int(d + i * 4);
		accur = be16_to_cpup((__be16 *)(d + 3 * 4));
		inv_push_16bytes_buffer(st, ind, t, out_2, accur);
		break;
	case SENSOR_PEDQ:
		inv_convert_and_push_8bytes(st, ind, d, t, iden);
		break;
	case SENSOR_PRESSURE:
		for (i = 0; i < 6; i++)
			st->fifo_data[i] = d[i];
		if (st->chip_config.has_pressure) {
			res = st->slave_pressure->read_data(st, out_1);
			inv_push_8bytes_buffer(st, ind, t, out_1);
		} else {
			for (i = 0; i < 3; i++)
				out_1[i] = be16_to_cpup((__be16 *) (d + i * 2));

			inv_push_8bytes_buffer(st, ind, t, out_1);
		}
		break;
	default:
		break;
	}

	return res;
}

int inv_get_packet_size(struct inv_mpu_state *st, u16 hdr,
			u32 *pk_size, u8 *dptr)
{
	int i, size;
	u16 hdr2;

	size = HEADER_SZ;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output)
			size += st->sensor[i].sample_size;
	}
	if (hdr & HEADER2_SET) {
		size += HEADER2_SZ;
		hdr2 = be16_to_cpup((__be16 *) (dptr + 2));

		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output)
				size += st->sensor_accuracy[i].sample_size;
		}

		if (hdr2 & ACT_RECOG_SET) {
			if (st->chip_config.activity_eng_on) {
				size += ACT_RECOG_SZ;
			} else {
				pr_err("ERROR: activity should not be here\n");
				return -EINVAL;
			}
		}
		if (hdr2 & SECOND_SEN_OFF_SET) {
			if (st->chip_config.activity_eng_on) {
				size += SECOND_AUTO_OFF_SZ;
			} else {
				pr_err("ERROR: activity should not be here\n");
				return -EINVAL;
			}
		}

		if (hdr2 & FLIP_PICKUP_SET) {
			if (st->chip_config.pick_up_enable)
				size += FLIP_PICKUP_SZ;
			else
				pr_err("ERROR: pick up should not be here\n");
		}

		if (hdr2 & FSYNC_SET) {
			if (st->chip_config.eis_enable)
				size += FSYNC_SZ;
			else
				pr_err("ERROR: eis should not be here\n");
		}
	}
	if ((!st->chip_config.step_indicator_on) && (hdr & PED_STEPIND_SET)) {
		pr_err("ERROR: step inditor should not be here=%x\n", hdr);
		return -EINVAL;
	}
	if (hdr & PED_STEPDET_SET) {
		if (st->chip_config.step_detector_on) {
			size += PED_STEPDET_TIMESTAMP_SZ;
		} else {
			pr_err("ERROR: step detector should not be here\n");
			return -EINVAL;
		}
	}
	size += FOOTER_SZ;
	*pk_size = size;

	return 0;
}

static int inv_push_accuracy(struct inv_mpu_state *st, int ind, u8 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	u16 hdr, accur;

	hdr = st->sensor_accuracy[ind].header;
	accur = be16_to_cpup((__be16 *)(d));
	if (st->sensor_acurracy_flag[ind]) {
		if (!accur)
			accur = DEFAULT_ACCURACY;
		else
			st->sensor_acurracy_flag[ind] = 0;
	}
	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(buf + sizeof(hdr), &accur, sizeof(accur));
	iio_push_to_buffers(indio_dev, buf);

	pr_debug("Accuracy for sensor [%d] is [%d]\n", ind, accur);

	return 0;
}
/* this function parse the FIFO information and set the sensor on/off */
static int inv_switch_secondary(struct inv_mpu_state *st, u8 *dptr)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u16 data;
	bool run_setup = false;

	data = be16_to_cpup((__be16 *) (dptr));
	switch (data) {
	case SENCONDARY_GYRO_OFF:
		st->secondary_gyro_on = 0;
		run_setup = true;
		break;
	case SENCONDARY_GYRO_ON:
		st->secondary_gyro_on = 1;
		run_setup = true;
		break;
	case SENCONDARY_COMPASS_OFF:
		st->secondary_mag_on = 0;
		break;
	case SENCONDARY_COMPASS_ON:
		st->secondary_mag_on = 0;
		break;
	case SENCONDARY_PROX_OFF:
		st->secondary_prox_on = 0;
		if (st->chip_config.has_als)
			run_setup = true;
		break;
	case SENCONDARY_PROX_ON:
		st->secondary_prox_on = 1;
		if (st->chip_config.has_als)
			run_setup = true;
		break;
	default:
		break;
	}
	if (run_setup) {
		st->secondary_switch = true;
		st->secondary_switch_data = data;

		set_inv_enable(indio_dev);

		st->secondary_switch = false;
	}

	return 0;
}
/* static int inv_set_fsync_trigger(struct inv_mpu_state *st)
	while receiving a FSYNC trigger for the first time, we set
	cycle mode into continuous mode and set the correponding filter
	parameters.
*/
static int inv_set_fsync_trigger(struct inv_mpu_state *st)
{
	int res;

	if (st->eis.eis_triggered)
		return 0;
	st->eis.eis_triggered = true;
		return 0;
	res = inv_plat_single_write(st, REG_LP_CONFIG, BIT_I2C_MST_CYCLE);
	if (res)
		return res;
	res = inv_set_bank(st, BANK_SEL_2);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_GYRO_CONFIG_1,
					BIT_GYRO_DLPCFG_184Hz |
					(st->chip_config.fsr
					<< SHIFT_GYRO_FS_SEL) |
					BIT_GYRO_FCHOICE);
	res = inv_set_bank(st, BANK_SEL_0);
	if (res)
		return res;
	st->eis.eis_triggered = true;

	return 0;
}

static void inv_calc_gyro_shift(struct inv_mpu_state *st, u16 tmp)
{
	int counter_1k[] = {3, 2, 1, 0, 4};
	int gyro_9k_counter, gyro_1k_counter;

#define GYRO_1K_DUR       (890 * 1000)
#define GYRO_9K_DUR       (110 * 1000)

	gyro_9k_counter = ((tmp >> 12) & 7);
	gyro_1k_counter = (tmp & 0xfff);
	if (gyro_1k_counter > 4)
		gyro_1k_counter = 4;
	st->ts_algo.gyro_ts_shift = counter_1k[gyro_1k_counter] * GYRO_1K_DUR +
				gyro_9k_counter * GYRO_9K_DUR;
	pr_debug("shif= %d, tmp= %x\n", st->ts_algo.gyro_ts_shift, tmp);
}

int inv_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int i;
	s16 s[3];
	u64 t;
	u16 hdr2 = 0;
	bool data_header;
	u8 *d_tmp;
	u16 delay;
	u16 tmp;

	t = 0;
	if (hdr & HEADER2_SET) {
		hdr2 = be16_to_cpup((__be16 *) (dptr));
		dptr += HEADER2_SZ;
	}

	st->eis.eis_frame = false;
	if (hdr & GYRO_SET)
		st->eis.gyro_counter++;
	if (hdr2 & FSYNC_SET) {
		d_tmp = dptr;
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (hdr & st->sensor[i].output)
				dptr += st->sensor[i].sample_size;
		}
		inv_set_fsync_trigger(st);
		st->eis.frame_count++;
		st->eis.eis_frame = true;
		delay = be16_to_cpup((__be16 *) (dptr));
		inv_process_eis(st, delay);

		dptr = d_tmp;
	}

	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output) {
			inv_get_dmp_ts(st, i);
			if (!st->ts_algo.first_sample) {
				st->sensor[i].sample_calib++;
				inv_push_sensor(st, i, st->sensor[i].ts, dptr);
			}
			dptr += st->sensor[i].sample_size;
			t = st->sensor[i].ts;
			data_header = true;
		}
	}
	if (st->ts_algo.first_sample)
		st->ts_algo.first_sample--;

	if (data_header)
		st->header_count--;
	if (hdr & PED_STEPDET_SET) {
		inv_process_step_det(st, dptr);
		dptr += PED_STEPDET_TIMESTAMP_SZ;
		st->step_det_count--;
	}

	if (hdr & PED_STEPIND_SET)
		inv_push_step_indicator(st, t);
	/* if hdr2 is not zero, it contains information and needs parsing */
	if (hdr2) {
		if (hdr2 & ACT_RECOG_SET) {
			t = st->ts_algo.last_run_time;
			s[0] = be16_to_cpup((__be16 *) (dptr));
			s[1] = 0;
			s[2] = 0;
			inv_push_8bytes_kf(st, ACTIVITY_HDR, t, s);
			dptr += ACT_RECOG_SZ;
		}

		if (hdr2 & SECOND_SEN_OFF_SET) {
			inv_switch_secondary(st, dptr);
			dptr += SECOND_AUTO_OFF_SZ;
		}
		if (hdr2 & FSYNC_SET)
			dptr += FSYNC_SZ;

		if (hdr2 & FLIP_PICKUP_SET) {
			sysfs_notify(&indio_dev->dev.kobj, NULL,
				     "poll_pick_up");
			dptr += FLIP_PICKUP_SZ;
			st->chip_config.pick_up_enable = 0;
			inv_check_sensor_on(st);
			set_inv_enable(indio_dev);
			st->wake_sensor_received = true;
			pr_debug("Pick up detected\n");
		}

		/* Implemented for Sensor accuracy */
		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output) {
				inv_push_accuracy(st, i, dptr);
				dptr += st->sensor_accuracy[i].sample_size;
			}
		}
	}
	tmp = be16_to_cpup((__be16 *) (dptr));
	inv_calc_gyro_shift(st, tmp);

	return 0;
}

int inv_pre_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	int i;
	u16 hdr2 = 0;
	bool data_header;

	if (hdr & HEADER2_SET) {
		hdr2 = be16_to_cpup((__be16 *) (dptr));
		dptr += HEADER2_SZ;
	}

	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output) {
			st->sensor[i].count++;
			dptr += st->sensor[i].sample_size;
			data_header = true;
		}
	}
	if (data_header)
		st->header_count++;
	if (hdr & PED_STEPDET_SET) {
		st->step_det_count++;
		dptr += PED_STEPDET_TIMESTAMP_SZ;
	}
	if (hdr2) {
		if (hdr2 & ACT_RECOG_SET)
			dptr += ACT_RECOG_SZ;

		if (hdr2 & SECOND_SEN_OFF_SET)
			dptr += SECOND_AUTO_OFF_SZ;

		if (hdr2 & FLIP_PICKUP_SET)
			dptr += FLIP_PICKUP_SZ;
		if (hdr2 & FSYNC_SET)
			dptr += FSYNC_SZ;
		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output)
				dptr += st->sensor_accuracy[i].sample_size;
		}
	}

	return 0;
}

static int inv_get_gyro_bias(struct inv_mpu_state *st, int *bias)
{
	int b_addr[] = { GYRO_BIAS_X, GYRO_BIAS_Y, GYRO_BIAS_Z };
	int i, r;

	for (i = 0; i < THREE_AXES; i++) {
		r = read_be32_from_mem(st, &bias[i], b_addr[i]);
		if (r)
			return r;
	}

	return 0;
}

static int inv_process_temp_comp(struct inv_mpu_state *st)
{
	u8 d[2];
	int r, l1, scale_t, curr_temp, i;
	s16 temp;
	s64 tmp, recp;
	bool update_slope;
	struct inv_temp_comp *t_c;
	int s_addr[] = { GYRO_SLOPE_X, GYRO_SLOPE_Y, GYRO_SLOPE_Z };

#define TEMP_COMP_WIDTH  4
#define TEMP_COMP_MID_L  (12 + TEMP_COMP_WIDTH)
#define TEMP_COMP_MID_H  (32 + TEMP_COMP_WIDTH)

	if (st->ts_algo.last_run_time - st->last_temp_comp_time < (NSEC_PER_SEC >> 1))
		return 0;
	st->last_temp_comp_time = st->ts_algo.last_run_time;
	if ((!st->gyro_cal_enable) ||
		(!st->chip_config.gyro_enable) || (!st->chip_config.accel_enable))
		return 0;
	r = inv_plat_read(st, REG_TEMPERATURE, 2, d);
	if (r)
		return r;
	temp = (s16) (be16_to_cpup((short *)d));
	scale_t = TEMPERATURE_OFFSET +
		inv_q30_mult((int)temp << MPU_TEMP_SHIFT, TEMPERATURE_SCALE);
	curr_temp = (scale_t >> MPU_TEMP_SHIFT);

	update_slope = false;
	/* check the lower part of the temperature */
	l1 = abs(curr_temp - TEMP_COMP_MID_L);
	l1 = l1 - TEMP_COMP_WIDTH;
	l1 = l1 - TEMP_COMP_WIDTH;
	t_c = &st->temp_comp;
	if (l1 < 0) {
		t_c->t_lo = temp;
		r = inv_get_gyro_bias(st, t_c->b_lo);
		if (r)
			return r;
		t_c->has_low = true;
		update_slope = true;
	}

	l1 = abs(curr_temp - TEMP_COMP_MID_H);
	l1 = l1 - TEMP_COMP_WIDTH;
	l1 = l1 - TEMP_COMP_WIDTH;
	if (l1 < 0) {
		t_c->t_hi = temp;
		r = inv_get_gyro_bias(st, t_c->b_hi);
		if (r)
			return r;
		t_c->has_high = true;
		update_slope = true;
	}
	if (t_c->has_high && t_c->has_low && update_slope) {
		if (t_c->t_hi != t_c->t_lo) {
			recp = (1 << 30) / (t_c->t_hi - t_c->t_lo);
			for (i = 0; i < THREE_AXES; i++) {
				tmp = recp * (t_c->b_hi[i] - t_c->b_lo[i]);
				t_c->slope[i] = (tmp >> 15);
				r = write_be32_to_mem(st,
					t_c->slope[i], s_addr[i]);
				if (r)
					return r;
			}
		}
	}

	return 0;
}

static int inv_process_dmp_interrupt(struct inv_mpu_state *st, u8 dmp_int_status)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int result, step;

#define DMP_INT_SMD  0x02
#define DMP_INT_PED  0x04

	if ((!st->smd.on) && (!st->ped.on))
		return 0;

	if (dmp_int_status & DMP_INT_SMD) {
		pr_debug("Sinificant motion detected\n");
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_smd");
		st->smd.on = false;
		st->trigger_state = EVENT_TRIGGER;
		inv_check_sensor_on(st);
		set_inv_enable(indio_dev);
		st->wake_sensor_received = true;
	}

	if (st->ped.int_on) {
		if (dmp_int_status & DMP_INT_PED) {
			if (st->ped.int_mode) {
				result = inv_get_pedometer_steps(st, &step);
				if (result) {
					pr_info("Failed to read step count\n");
					return result;
				}
				inv_send_steps(st, step, st->ts_algo.last_run_time);
				st->prev_steps = step;
			}
		}
	}

	return 0;
}

/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;
	int i;
	u8 d[1];

#define DMP_INT_FIFO	0x01

	result = wait_event_interruptible_timeout(st->wait_queue,
					st->resume_state, msecs_to_jiffies(300));
	if (result <= 0)
		goto exit_handled;

	mutex_lock(&indio_dev->mlock);

	if (st->chip_config.is_asleep)
		goto end_read_fifo;

	inv_switch_power_in_lp(st, true);

	result = inv_plat_read(st, REG_DMP_INT_STATUS, 1, d);
	if (result) {
		pr_info("REG_DMP_INT_STATUS result [%d]\n", result);
		goto end_read_fifo;
	}

	st->ts_algo.last_run_time = get_time_ns();
	st->wake_sensor_received = false;
	st->activity_size = 0;

	result = inv_process_dmp_interrupt(st, d[0]);
	if (result)
		goto end_read_fifo;

	if (!(d[0] & DMP_INT_FIFO))
		goto end_read_fifo;

	result = inv_process_temp_comp(st);
	if (result)
		goto end_read_fifo;

	result = inv_process_dmp_data(st);

	if (st->activity_size > 0)
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_activity");
	if (result)
		goto err_reset_fifo;

end_read_fifo:
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);

	/* set wake_sensor_received flag when streaming is enabled along with
	 * any wakeup gesture sensor which is reported as FIFO data packet
	 */
	if (st->chip_config.tilt_enable || st->chip_config.pick_up_enable||
			st->step_detector_wake_l_on) {
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (st->sensor[i].on) {
				st->wake_sensor_received = true;
				break;
			}
		}
	}

	if (st->wake_sensor_received)
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));
#else
		__pm_wakeup_event(&st->wake_lock, 200); /* 200 msecs */
#endif
	goto exit_handled;

err_reset_fifo:
	if ((!st->chip_config.gyro_enable) &&
		(!st->chip_config.accel_enable) &&
		(!st->chip_config.slave_enable) &&
		(!st->chip_config.pressure_enable)) {
		inv_switch_power_in_lp(st, false);
		mutex_unlock(&indio_dev->mlock);
		goto exit_handled;
	}

	pr_err("error to reset fifo\n");
	inv_reset_fifo(st, true);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);

exit_handled:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

int inv_flush_batch_data(struct iio_dev *indio_dev, int data)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result = 0;

	if (st->chip_config.gyro_enable ||
		st->chip_config.accel_enable ||
		st->chip_config.slave_enable ||
		st->chip_config.pressure_enable) {
		inv_switch_power_in_lp(st, true);
		st->wake_sensor_received = false;
		if (inv_process_dmp_data(st))
			pr_err("error on batch.. need reset fifo\n");
		if (st->wake_sensor_received)
#ifdef CONFIG_HAS_WAKELOCK
			wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));
#else
			__pm_wakeup_event(&st->wake_lock, 200); /* 200 msecs */
#endif
		inv_switch_power_in_lp(st, false);
	}
	inv_push_marker_to_buffer(st, END_MARKER, data);

	return result;
}
