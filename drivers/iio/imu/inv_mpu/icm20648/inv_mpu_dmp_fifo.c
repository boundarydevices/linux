/*
 * Copyright (C) 2017-2018 InvenSense, Inc.
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

static int inv_get_step_params(struct inv_mpu_state *st)
{
	int result;

	result = inv_switch_power_in_lp(st, true);
	st->prev_steps = st->curr_steps;
	result |= read_be32_from_mem(st, &st->curr_steps, PEDSTD_STEPCTR);
	result |= read_be32_from_mem(st, &st->ts_algo.start_dmp_counter,
								DMPRATE_CNTR);
	result |= inv_switch_power_in_lp(st, false);

	return result;
}
/**
  * static int inv_prescan_data(struct inv_mpu_state *st, u8 * dptr, int len)
  *  prescan data to know what type of data and how many samples of data
  *  in current FIFO reading.
*/
static int inv_prescan_data(struct inv_mpu_state *st, u8 *dptr, int len)
{
	int res, pk_size, i;
	bool done_flag;
	u16 hdr;

	done_flag = false;
	st->header_count = 0;
	st->step_det_count = 0;
	st->ts_algo.calib_counter++;
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].count = 0;
	while (!done_flag) {
		if (len > HEADER_SZ) {
			hdr = (u16) be16_to_cpup((__be16 *) (dptr));
			if (!hdr) {
				pr_err("error header zero\n");
				st->left_over_size = 0;
				return -EINVAL;
			}
			res = inv_get_packet_size(st, hdr, &pk_size, dptr);
			if (res) {
				if (!st->chip_config.is_asleep)
					pr_err
				("prescan error in header parsing=%x size=%d\n",
				hdr, len);
				st->left_over_size = 0;

				return -EINVAL;
			}
			if (len >= pk_size) {
				inv_pre_parse_packet(st, hdr, dptr + HEADER_SZ);
				len -= pk_size;
				dptr += pk_size;
			} else {
				done_flag = true;
			}
		} else {
			done_flag = true;
		}
	}
	if (st->step_det_count)
		inv_get_step_params(st);
	inv_bound_timestamp(st);

	return 0;
}
int inv_process_dmp_data(struct inv_mpu_state *st)
{
	int total_bytes, tmp, res, fifo_count, pk_size;
	u8 *dptr, *d;
	u16 hdr;
	u8 data[2];
	bool done_flag;

	st->ts_algo.last_run_time = get_time_ns();
	res = inv_plat_read(st, REG_FIFO_COUNT_H, FIFO_COUNT_BYTE, data);
	if (res) {
		pr_debug("read REG_FIFO_COUNT_H failed\n");
		return res;
	}
	fifo_count = be16_to_cpup((__be16 *) (data));
	if (!fifo_count) {
		pr_debug("REG_FIFO_COUNT_H size is 0\n");
		return 0;
	}

	if (fifo_count >= HARDWARE_FIFO_SIZE) {
		pr_warn("fifo overflow fifo_count=%d\n", fifo_count);
		return -EOVERFLOW;
	}

	st->fifo_count = fifo_count;
	d = st->fifo_data_store;

	if (st->left_over_size > LEFT_OVER_BYTES) {
		st->left_over_size = 0;
		pr_debug("left over size error\n");
		return -EINVAL;
	}

	if (st->left_over_size > 0)
		memcpy(d, st->left_over, st->left_over_size);

	dptr = d + st->left_over_size;
	total_bytes = fifo_count;
	while (total_bytes > 0) {
		if (total_bytes < MAX_FIFO_READ_SIZE)
			tmp = total_bytes;
		else
			tmp = MAX_FIFO_READ_SIZE;
		res = inv_plat_read(st, REG_FIFO_R_W, tmp, dptr);
		if (res < 0) {
			pr_debug("read REG_FIFO_R_W is failed\n");
			return res;
		}
		dptr += tmp;
		total_bytes -= tmp;
	}
	dptr = d;
	total_bytes = fifo_count + st->left_over_size;
	res = inv_prescan_data(st, dptr, total_bytes);
	if (res) {
		pr_info("prescan failed\n");
		return -EINVAL;
	}
	dptr = d;
	done_flag = false;
	pr_debug("dd: %x, %x, %x, %x, %x, %x, %x, %x\n", d[0], d[1], d[2],
						d[3], d[4], d[5], d[6], d[7]);
	pr_debug("dd2: %x, %x, %x, %x, %x, %x, %x, %x\n", d[8], d[9], d[10],
					d[11], d[12], d[13], d[14], d[15]);
	while (!done_flag) {
		if (total_bytes > HEADER_SZ) {
			hdr = (u16) be16_to_cpup((__be16 *) (dptr));
			res = inv_get_packet_size(st, hdr, &pk_size, dptr);
			if (res) {
				pr_err
		("processing error in header parsing=%x fifo_count= %d\n",
		hdr, fifo_count);
				st->left_over_size = 0;

				return -EINVAL;
			}
			if (total_bytes >= pk_size) {
				inv_parse_packet(st, hdr, dptr + HEADER_SZ);
				total_bytes -= pk_size;
				dptr += pk_size;
			} else {
				done_flag = true;
			}
		} else {
			done_flag = true;
		}
	}
	st->left_over_size = total_bytes;
	if (st->left_over_size > LEFT_OVER_BYTES) {
		st->left_over_size = 0;
		return -EINVAL;
	}

	if (st->left_over_size)
		memcpy(st->left_over, dptr, st->left_over_size);

	return 0;
}
