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

int inv_stop_dmp(struct inv_mpu_state *st)
{
	return inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
}

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_STEPCTR);

	return r;
}

int inv_process_step_det(struct inv_mpu_state *st, u8 *dptr)
{
	u32 tmp;
	u64 t;
	s16 s[3];

	tmp = be32_to_int(dptr);
	tmp = inv_get_cntr_diff(st->ts_algo.start_dmp_counter, tmp);
	t = st->ts_algo.last_run_time - (u64) tmp * st->eng_info[ENGINE_ACCEL].dur;
	if (st->step_detector_l_on)
		inv_push_8bytes_buffer(st, STEP_DETECTOR_HDR, t, s);
	if (st->step_detector_wake_l_on)
		inv_push_8bytes_buffer(st, STEP_DETECTOR_WAKE_HDR, t, s);

	return 0;
}

int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_TIMECTR);

	return r;
}

int inv_read_pedometer_counter(struct inv_mpu_state *st)
{
	int result;
	u32 last_step_counter, curr_counter;
	u64 counter;

	result = read_be32_from_mem(st, &last_step_counter, STPDET_TIMESTAMP);
	if (result)
		return result;
	if (0 != last_step_counter) {
		result = read_be32_from_mem(st, &curr_counter, DMPRATE_CNTR);
		if (result)
			return result;
		counter = inv_get_cntr_diff(curr_counter, last_step_counter);
		st->ped.last_step_time = get_time_ns() - counter *
		    st->eng_info[ENGINE_ACCEL].dur;
	}

	return 0;
}

