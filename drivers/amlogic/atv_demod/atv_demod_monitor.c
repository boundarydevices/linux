/*
 * drivers/amlogic/atv_demod/atv_demod_monitor.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/types.h>
#include <uapi/linux/dvb/frontend.h>
#include <linux/amlogic/cpu_version.h>

#include "atvdemod_func.h"
#include "atvauddemod_func.h"
#include "atv_demod_monitor.h"
#include "atv_demod_ops.h"
#include "atv_demod_debug.h"

static DEFINE_MUTEX(monitor_mutex);

bool atvdemod_mixer_tune_en;
bool atvdemod_overmodulated_en;
bool audio_det_en;
bool atvdemod_det_snr_en = true;
bool audio_thd_en = true;
bool atvdemod_det_nonstd_en;
bool atvaudio_det_outputmode_en = true;

unsigned int atvdemod_timer_delay = 100; /* 1s */
unsigned int atvdemod_timer_delay2 = 10; /* 100ms */

bool atvdemod_timer_en = true;


static void atv_demod_monitor_do_work(struct work_struct *work)
{
	struct atv_demod_monitor *monitor =
			container_of(work, struct atv_demod_monitor, work);

	if (!monitor->state)
		return;

	if (atvdemod_mixer_tune_en)
		atvdemod_mixer_tune();

	if (atvdemod_overmodulated_en)
		atvdemod_video_overmodulated();

	if (atvdemod_det_snr_en)
		atvdemod_det_snr_serice();

	if (audio_thd_en)
		audio_thd_det();

	if (atvaudio_det_outputmode_en &&
		(is_meson_txlx_cpu() || is_meson_txhd_cpu()))
		atvauddemod_set_outputmode();

	if (atvdemod_det_nonstd_en)
		atv_dmd_non_std_set(true);
}

static void atv_demod_monitor_timer_handler(unsigned long arg)
{
	struct atv_demod_monitor *monitor = (struct atv_demod_monitor *)arg;

	/* 100ms timer */
	monitor->timer.data = arg;
	monitor->timer.expires = jiffies +
			ATVDEMOD_INTERVAL * atvdemod_timer_delay2;
	add_timer(&monitor->timer);

	if (atvdemod_timer_en == 0)
		return;

	if (vdac_enable_check_dtv())
		return;

	schedule_work(&monitor->work);
}

static void atv_demod_monitor_enable(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor->mtx);

	if (atvdemod_timer_en && !monitor->state) {
		atv_dmd_non_std_set(false);

		init_timer(&monitor->timer);
		monitor->timer.data = (ulong) monitor;
		monitor->timer.function = atv_demod_monitor_timer_handler;
		/* after 1s enable demod auto detect */
		monitor->timer.expires = jiffies +
				ATVDEMOD_INTERVAL * atvdemod_timer_delay;
		add_timer(&monitor->timer);
		monitor->state = true;
	}

	mutex_unlock(&monitor->mtx);

	pr_dbg("%s: state: %d.\n", __func__, monitor->state);
}

static void atv_demod_monitor_disable(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor->mtx);

	if (atvdemod_timer_en && monitor->state) {
		monitor->state = false;
		atv_dmd_non_std_set(false);
		del_timer_sync(&monitor->timer);
		cancel_work_sync(&monitor->work);
	}

	mutex_unlock(&monitor->mtx);

	pr_dbg("%s: state: %d.\n", __func__, monitor->state);
}

void atv_demod_monitor_init(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor_mutex);

	mutex_init(&monitor->mtx);

	monitor->state = false;
	monitor->lock = false;
	monitor->disable = atv_demod_monitor_disable;
	monitor->enable = atv_demod_monitor_enable;

	INIT_WORK(&monitor->work, atv_demod_monitor_do_work);

	mutex_unlock(&monitor_mutex);
}

