/*
 * drivers/amlogic/atv_demod/atv_demod_afc.c
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

#include "atvdemod_func.h"
#include "atv_demod_afc.h"
#include "atv_demod_ops.h"
#include "atv_demod_debug.h"

static DEFINE_MUTEX(afc_mutex);


unsigned int afc_limit = 2100;/* +/-2.1Mhz */
unsigned int afc_timer_delay = 1;
unsigned int afc_timer_delay2 = 10;
unsigned int afc_timer_delay3 = 10; /* 100ms */
unsigned int afc_wave_cnt = 4;

static int afc_range[11] = {0, -500, 500, -1000, 1000,
		-1500, 1500, -2000, 2000, -2500, 2500};

bool afc_timer_en = true;


static void atv_demod_afc_do_work_pre(struct atv_demod_afc *afc)
{
	struct atv_demod_priv *priv =
			container_of(afc, struct atv_demod_priv, afc);
	struct dvb_frontend *fe = afc->fe;
	struct analog_parameters *param = &priv->atvdemod_param.param;

	afc->pre_unlock_cnt++;
	if (!afc->lock) {
		afc->pre_lock_cnt = 0;
		if (afc->status != AFC_LOCK_STATUS_PRE_UNLOCK && afc->offset) {
			param->frequency -= afc->offset * 1000;
			afc->offset = 0;
			afc->pre_step = 0;
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
		}

		if (afc->pre_unlock_cnt <= afc_wave_cnt) {/*40ms*/
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
			return;
		}

		if (afc->offset == afc_range[afc->pre_step]) {
			param->frequency -= afc_range[afc->pre_step] * 1000;
			afc->offset -= afc_range[afc->pre_step];
		}

		afc->pre_step++;
		if (afc->pre_step < AFC_LOCK_PRE_STEP_NUM) {
			param->frequency += afc_range[afc->pre_step] * 1000;
			afc->offset += afc_range[afc->pre_step];
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
		} else {
			param->frequency -= afc->offset * 1000;
			afc->offset = 0;
			afc->pre_step = 0;
			afc->status = AFC_LOCK_STATUS_PRE_OVER_RANGE;
		}

		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);

		afc->pre_unlock_cnt = 0;
		pr_afc("%s,unlock offset:%d KHz, set freq:%d\n",
				__func__, afc->offset, param->frequency);
	} else {
		afc->pre_lock_cnt++;
		pr_afc("%s,afc_pre_lock_cnt:%d\n",
				__func__, afc->pre_lock_cnt);
		if (afc->pre_lock_cnt >= afc_wave_cnt * 2) {/*100ms*/
			afc->pre_lock_cnt = 0;
			afc->pre_unlock_cnt = 0;
			afc->status = AFC_LOCK_STATUS_PRE_LOCK;
		}
	}
}

void atv_demod_afc_do_work(struct work_struct *work)
{
	struct atv_demod_afc *afc =
			container_of(work, struct atv_demod_afc, work);
	struct atv_demod_priv *priv =
				container_of(afc, struct atv_demod_priv, afc);
	struct dvb_frontend *fe = afc->fe;
	struct analog_parameters *param = &priv->atvdemod_param.param;

	int freq_offset = 100;
	int tmp = 0;
	int field_lock = 0;
	static int audio_overmodul;

	if (afc->state == false)
		return;

	retrieve_vpll_carrier_lock(&tmp);/* 0 means lock, 1 means unlock */
	afc->lock = (tmp == 0);

	/*pre afc:speed up afc*/
	if ((afc->status != AFC_LOCK_STATUS_POST_PROCESS) &&
		(afc->status != AFC_LOCK_STATUS_POST_LOCK) &&
		(afc->status != AFC_LOCK_STATUS_PRE_LOCK)) {
		atv_demod_afc_do_work_pre(afc);
		return;
	}

	afc->pre_step = 0;

	if (afc->lock) {
		if (0 == ((audio_overmodul++) % 10)) {
			aml_audio_overmodulation(1);
			audio_overmodul = 0;
		}
	}

	retrieve_frequency_offset(&freq_offset);

	if (++(afc->wave_cnt) <= afc_wave_cnt) {/*40ms*/
		afc->status = AFC_LOCK_STATUS_POST_PROCESS;
		pr_afc("%s,wave_cnt:%d is wave,lock:%d,freq_offset:%d ignore\n",
				__func__, afc->wave_cnt, afc->lock,
				freq_offset);
		return;
	}

	/*retrieve_frequency_offset(&freq_offset);*/
	retrieve_field_lock(&tmp);
	field_lock = tmp;

	pr_afc("%s,freq_offset:%d lock:%d field_lock:%d freq:%d\n",
			__func__, freq_offset, afc->lock, field_lock,
			param->frequency);

	if (afc->lock && (abs(freq_offset) < AFC_BEST_LOCK &&
		abs(afc->offset) <= afc_limit) && field_lock) {
		afc->status = AFC_LOCK_STATUS_POST_LOCK;
		afc->wave_cnt = 0;
		pr_afc("%s,afc lock, set wave_cnt 0\n", __func__);
		return;
	}

	if (!afc->lock || (afc->lock && !field_lock)) {
		afc->status = AFC_LOCK_STATUS_POST_UNLOCK;
		afc->pre_lock_cnt = 0;
		param->frequency -= afc->offset * 1000;
		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);
		pr_afc("%s,freq_offset:%d , set freq:%d\n",
				__func__, freq_offset, param->frequency);
		afc->wave_cnt = 0;
		afc->offset = 0;
		pr_afc("%s, [post lock --> unlock] set offset 0.\n", __func__);
		return;
	}

	if (abs(afc->offset) > afc_limit) {
		afc->no_sig_cnt++;
		if (afc->no_sig_cnt == 20) {
			param->frequency -= afc->offset * 1000;
			pr_afc("%s,afc no_sig trig, set freq:%d\n",
						__func__, param->frequency);
			if (fe->ops.tuner_ops.set_analog_params)
				fe->ops.tuner_ops.set_analog_params(fe, param);
			afc->wave_cnt = 0;
			afc->offset = 0;
			afc->status = AFC_LOCK_STATUS_POST_OVER_RANGE;
		}
		return;
	}

	afc->no_sig_cnt = 0;
	if (abs(freq_offset) >= AFC_BEST_LOCK) {
		param->frequency += freq_offset * 1000;
		afc->offset += freq_offset;
		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);

		pr_afc("%s,freq_offset:%d , set freq:%d\n",
				__func__, freq_offset, param->frequency);
	}

	afc->wave_cnt = 0;
	afc->status = AFC_LOCK_STATUS_POST_PROCESS;
}

static void atv_demod_afc_timer_handler(unsigned long arg)
{
	struct atv_demod_afc *afc = (struct atv_demod_afc *)arg;
	struct dvb_frontend *fe = afc->fe;
	unsigned int delay_ms = 0;

	if (afc->state == false)
		return;

	if (afc->status == AFC_LOCK_STATUS_POST_OVER_RANGE ||
		afc->status == AFC_LOCK_STATUS_PRE_OVER_RANGE ||
		afc->status == AFC_LOCK_STATUS_POST_LOCK)
		delay_ms = afc_timer_delay2;/*100ms*/
	else
		delay_ms = afc_timer_delay;/*10ms*/

	afc->timer.function = atv_demod_afc_timer_handler;
	afc->timer.data = arg;
	afc->timer.expires = jiffies + ATVDEMOD_INTERVAL * delay_ms;
	add_timer(&afc->timer);

	if (afc->timer_delay_cnt > 0) {
		afc->timer_delay_cnt--;
		return;
	}

	if ((afc_timer_en == false) || (fe->ops.info.type != FE_ANALOG))
		return;

	schedule_work(&afc->work);
}

static void atv_demod_afc_disable(struct atv_demod_afc *afc)
{
	mutex_lock(&afc->mtx);

	if (afc_timer_en && (afc->state == true)) {
		afc->state = false;
		del_timer_sync(&afc->timer);
		cancel_work_sync(&afc->work);
	}

	mutex_unlock(&afc->mtx);

	pr_afc("%s: state: %d.\n", __func__, afc->state);
}

static void atv_demod_afc_enable(struct atv_demod_afc *afc)
{
	mutex_lock(&afc->mtx);

	if (afc_timer_en && (afc->state == false)) {
		init_timer(&afc->timer);
		afc->timer.function = atv_demod_afc_timer_handler;
		afc->timer.data = (ulong) afc;
		/* after afc_timer_delay3 enable demod auto detect */
		afc->timer.expires = jiffies +
				ATVDEMOD_INTERVAL * afc_timer_delay3;
		afc->offset = 0;
		afc->no_sig_cnt = 0;
		afc->pre_step = 0;
		afc->status = AFC_LOCK_STATUS_NULL;
		add_timer(&afc->timer);
		afc->state = true;
	}

	mutex_unlock(&afc->mtx);

	pr_afc("%s: state: %d.\n", __func__, afc->state);
}

void atv_demod_afc_init(struct atv_demod_afc *afc)
{
	mutex_lock(&afc_mutex);

	mutex_init(&afc->mtx);

	afc->state = false;
	afc->timer_delay_cnt = 0;
	afc->disable = atv_demod_afc_disable;
	afc->enable = atv_demod_afc_enable;

	INIT_WORK(&afc->work, atv_demod_afc_do_work);

	mutex_unlock(&afc_mutex);
}

