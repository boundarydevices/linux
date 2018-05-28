/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/i2c.h>
#include <uapi/linux/dvb/frontend.h>

#include <linux/amlogic/aml_atvdemod.h>

#include "drivers/media/tuners/tuner-i2c.h"
#include "drivers/media/dvb-core/dvb_frontend.h"
#include "drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.h"

#include "atvdemod_func.h"
#include "atvauddemod_func.h"
#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atv_demod_ops.h"
#include "atv_demod_v4l2.h"

#define DEVICE_NAME "aml_atvdemod"

#define ATVDEMOD_STATE_IDEL  0
#define ATVDEMOD_STATE_WORK  1
#define ATVDEMOD_STATE_SLEEP 2
static int atvdemod_state = ATVDEMOD_STATE_IDEL;

static DEFINE_MUTEX(atv_demod_list_mutex);
static LIST_HEAD(hybrid_tuner_instance_list);

unsigned int reg_23cf = 0x88188832; /*IIR filter*/
unsigned int btsc_sap_mode = 1;	/*0: off 1:monitor 2:auto */

int afc_offset;
unsigned int afc_limit = 2100;/*+-2.1Mhz*/

static int no_sig_cnt;
struct timer_list aml_timer;
#define AML_INTERVAL		(HZ/100)   /* 10ms, #define HZ 100 */
static unsigned int timer_init_state;
static unsigned int aft_thread_enable;
static unsigned int aft_thread_delaycnt;


bool aml_timer_en = true;
unsigned int timer_delay = 1;
unsigned int timer_delay2 = 10;
unsigned int timer_delay3 = 10;/*100ms*/
unsigned int afc_wave_cnt = 4;


#define AFC_LOCK_STATUS_NULL 0
#define AFC_LOCK_STATUS_PRE_UNLOCK 1
#define AFC_LOCK_STATUS_PRE_LOCK 2
#define AFC_LOCK_STATUS_PRE_OVER_RANGE 3
#define AFC_LOCK_STATUS_POST_PROCESS 4
#define AFC_LOCK_STATUS_POST_LOCK 5
#define AFC_LOCK_STATUS_POST_UNLOCK 6
#define AFC_LOCK_STATUS_POST_OVER_RANGE 7
#define AFC_LOCK_STATUS_NUM 8

#define AFC_PRE_STEP_NUM 9
static int afc_range[11] = {0, -500, 500, -1000, 1000,
		-1500, 1500, -2000, 2000, -2500, 2500};
static unsigned int afc_pre_step;
static unsigned int afc_pre_lock_cnt;
static unsigned int afc_pre_unlock_cnt;
static unsigned int afc_lock_status = AFC_LOCK_STATUS_NULL;

static void aml_fe_do_work_pre(int lock)
{
	struct dvb_frontend *fe = &amlatvdemod_devp->v4l2_fe.fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct analog_parameters *param = &priv->atvdemod_param.param;

	afc_pre_unlock_cnt++;
	if (!lock) {
		afc_pre_lock_cnt = 0;
		if (afc_lock_status != AFC_LOCK_STATUS_PRE_UNLOCK &&
			afc_offset) {
			param->frequency -= afc_offset * 1000;
			afc_offset = 0;
			afc_pre_step = 0;
			afc_lock_status = AFC_LOCK_STATUS_PRE_UNLOCK;
		}

		if (afc_pre_unlock_cnt <= afc_wave_cnt) {/*40ms*/
			afc_lock_status = AFC_LOCK_STATUS_PRE_UNLOCK;
			return;
		}

		if (afc_offset == afc_range[afc_pre_step]) {
			param->frequency -= afc_range[afc_pre_step] * 1000;
			afc_offset -= afc_range[afc_pre_step];
		}

		afc_pre_step++;
		if (afc_pre_step < AFC_PRE_STEP_NUM) {
			param->frequency += afc_range[afc_pre_step] * 1000;
			afc_offset += afc_range[afc_pre_step];
			afc_lock_status = AFC_LOCK_STATUS_PRE_UNLOCK;
		} else {
			param->frequency -= afc_offset * 1000;
			afc_offset = 0;
			afc_pre_step = 0;
			afc_lock_status = AFC_LOCK_STATUS_PRE_OVER_RANGE;
		}

		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);

		afc_pre_unlock_cnt = 0;
		pr_afc("%s,unlock afc_offset:%d KHz, set freq:%d\n",
				__func__, afc_offset, param->frequency);
	} else {
		afc_pre_lock_cnt++;
		pr_afc("%s,afc_pre_lock_cnt:%d\n",
				__func__, afc_pre_lock_cnt);
		if (afc_pre_lock_cnt >= afc_wave_cnt * 2) {/*100ms*/
			afc_pre_lock_cnt = 0;
			afc_pre_unlock_cnt = 0;
			afc_lock_status = AFC_LOCK_STATUS_PRE_LOCK;
		}
	}
}

static void aml_fe_do_work(struct work_struct *work)
{
	struct dvb_frontend *fe = &amlatvdemod_devp->v4l2_fe.fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct analog_parameters *param = &priv->atvdemod_param.param;
	int afc = 100;
	int lock = 0;
	int tmp = 0;
	int field_lock = 0;
	static int audio_overmodul;
	static int wave_cnt;

	if (timer_init_state == 0)
		return;

	retrieve_vpll_carrier_lock(&tmp);/* 0 means lock, 1 means unlock */
	lock = !tmp;

	/*pre afc:speed up afc*/
	if ((afc_lock_status != AFC_LOCK_STATUS_POST_PROCESS) &&
		(afc_lock_status != AFC_LOCK_STATUS_POST_LOCK) &&
		(afc_lock_status != AFC_LOCK_STATUS_PRE_LOCK)) {
		aml_fe_do_work_pre(lock);
		return;
	}

	afc_pre_step = 0;

	if (lock) {
		if (0 == ((audio_overmodul++) % 10))
			aml_audio_overmodulation(1);
	}

	retrieve_frequency_offset(&afc);

	if (++wave_cnt <= afc_wave_cnt) {/*40ms*/
		afc_lock_status = AFC_LOCK_STATUS_POST_PROCESS;
		pr_afc("%s,wave_cnt:%d is wave,lock:%d,afc:%d ignore\n",
				__func__, wave_cnt, lock, afc);
		return;
	}

	/*retrieve_frequency_offset(&afc);*/
	retrieve_field_lock(&tmp);
	field_lock = tmp;

	pr_afc("%s,afc:%d lock:%d field_lock:%d freq:%d\n",
			__func__, afc, lock, field_lock, param->frequency);

	if (lock && (abs(afc) < AFC_BEST_LOCK &&
		abs(afc_offset) <= afc_limit) && field_lock) {
		afc_lock_status = AFC_LOCK_STATUS_POST_LOCK;
		wave_cnt = 0;
		pr_afc("%s,afc lock, set wave_cnt 0\n", __func__);
		return;
	}

	if (!lock) {
		afc_lock_status = AFC_LOCK_STATUS_POST_UNLOCK;
		afc_pre_lock_cnt = 0;
		param->frequency -= afc_offset * 1000;
		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);
		pr_afc("%s,afc:%d , set freq:%d\n",
				__func__, afc, param->frequency);
		wave_cnt = 0;
		afc_offset = 0;
		pr_afc("%s, [post lock --> unlock]\n", __func__);
		return;
	}

	if (abs(afc_offset) > afc_limit) {
		no_sig_cnt++;
		if (no_sig_cnt == 20) {
			param->frequency -= afc_offset * 1000;
			pr_afc("%s,afc no_sig trig, set freq:%d\n",
						__func__, param->frequency);
			if (fe->ops.tuner_ops.set_analog_params)
				fe->ops.tuner_ops.set_analog_params(fe, param);
			wave_cnt = 0;
			afc_offset = 0;
			afc_lock_status = AFC_LOCK_STATUS_POST_OVER_RANGE;
		}
		return;
	}

	no_sig_cnt = 0;
	if (abs(afc) >= AFC_BEST_LOCK) {
		param->frequency += afc * 1000;
		afc_offset += afc;
		if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, param);

		pr_afc("%s,afc:%d , set freq:%d\n",
				__func__, afc, param->frequency);
	}

	wave_cnt = 0;
	afc_lock_status = AFC_LOCK_STATUS_POST_PROCESS;
}

void aml_timer_handler(unsigned long arg)
{
	struct dvb_frontend *fe = (struct dvb_frontend *)arg;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	unsigned int delay_ms = 0;

	if ((fe == NULL) || (priv == NULL) || (timer_init_state == 0))
		return;

	if (afc_lock_status == AFC_LOCK_STATUS_POST_OVER_RANGE ||
		afc_lock_status == AFC_LOCK_STATUS_PRE_OVER_RANGE ||
		afc_lock_status == AFC_LOCK_STATUS_POST_LOCK)
		delay_ms = timer_delay2;/*100ms*/
	else
		delay_ms = timer_delay;/*10ms*/

	aml_timer.function = aml_timer_handler;
	aml_timer.data = arg;
	aml_timer.expires = jiffies + AML_INTERVAL * delay_ms;
	add_timer(&aml_timer);

	if (!aft_thread_enable) {
		/*pr_info("%s, stop aft thread\n", __func__);*/
		return;
	}

	if (aft_thread_delaycnt > 0) {
		aft_thread_delaycnt--;
		return;
	}

	if ((aml_timer_en == false) || (fe->ops.info.type != FE_ANALOG))
		return;

	schedule_work(&priv->demod_wq);
}

static void afc_timer_disable(struct dvb_frontend *fe)
{
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (aml_timer_en && (timer_init_state == 1)) {
		del_timer_sync(&aml_timer);
		cancel_work_sync(&priv->demod_wq);
		timer_init_state = 0;
	}
}

static void afc_timer_enable(struct dvb_frontend *fe)
{
	if (fe && aml_timer_en && (timer_init_state == 0)) {
		init_timer(&aml_timer);
		aml_timer.function = aml_timer_handler;
		aml_timer.data = (ulong) fe;
		/* after 5s enable demod auto detect */
		aml_timer.expires = jiffies + AML_INTERVAL * timer_delay3;
		afc_offset = 0;
		no_sig_cnt = 0;
		afc_pre_step = 0;
		afc_lock_status = AFC_LOCK_STATUS_NULL;
		add_timer(&aml_timer);
		timer_init_state = 1;
	}
}

static void set_aft_thread_enable(int enable, unsigned int delay)
{
	if (enable == aft_thread_enable)
		return;

	aft_thread_enable = enable;
	aft_thread_delaycnt = delay;

	if (aft_thread_enable)
		afc_timer_enable(&amlatvdemod_devp->v4l2_fe.fe);
	else
		afc_timer_disable(&amlatvdemod_devp->v4l2_fe.fe);
}

/*
 * add interface for audio driver to get atv audio state.
 * state:
 * 0 - no data.
 * 1 - has data.
 */
void aml_fe_get_atvaudio_state(int *state)
{
	int power = 0;
	int vpll_lock = 0;
	int line_lock = 0;
	int atv_state = atv_demod_get_state();

	if (atv_state == ATVDEMOD_STATE_WORK) {
		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);
		if ((vpll_lock == 0) && (line_lock == 0))
			retrieve_vpll_carrier_audio_power(&power);
	} else
		pr_audio("%s, atv is not work, atv_state: %d.\n",
				__func__, atv_state);

	if (power >= 150)
		*state = 1;
	else
		*state = 0;

	pr_audio("aml_fe_get_atvaudio_state: %d, power = %d.\n",
			*state, power);
}

int aml_atvdemod_get_btsc_sap_mode(void)
{
	return btsc_sap_mode;
}

unsigned int atvdemod_scan_mode; /*IIR filter*/

/* ret:5~100;the val is bigger,the signal is better */
int aml_atvdemod_get_snr(struct dvb_frontend *fe)
{
#if 1
	return get_atvdemod_snr_val();
#else
	unsigned int snr_val;
	int ret;

	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	/* snr_val:900000~0xffffff,ret:5~15 */
	if (snr_val > 900000)
		ret = 15 - (snr_val - 900000)*10/(0xffffff - 900000);
	/* snr_val:158000~900000,ret:15~30 */
	else if (snr_val > 158000)
		ret = 30 - (snr_val - 158000)*15/(900000 - 158000);
	/* snr_val:31600~158000,ret:30~50 */
	else if (snr_val > 31600)
		ret = 50 - (snr_val - 31600)*20/(158000 - 31600);
	/* snr_val:316~31600,ret:50~80 */
	else if (snr_val > 316)
		ret = 80 - (snr_val - 316)*30/(31600 - 316);
	/* snr_val:0~316,ret:80~100 */
	else
		ret = 100 - (316 - snr_val)*20/316;
	return ret;
#endif
}

int aml_atvdemod_get_snr_ex(void)
{
#if 1
	return get_atvdemod_snr_val();
#else
	unsigned int snr_val;
	int ret;

	snr_val = atv_dmd_rd_long(APB_BLOCK_ADDR_VDAGC, 0x50) >> 8;
	/* snr_val:900000~0xffffff,ret:5~15 */
	if (snr_val > 900000)
		ret = 15 - (snr_val - 900000)*10/(0xffffff - 900000);
	/* snr_val:158000~900000,ret:15~30 */
	else if (snr_val > 158000)
		ret = 30 - (snr_val - 158000)*15/(900000 - 158000);
	/* snr_val:31600~158000,ret:30~50 */
	else if (snr_val > 31600)
		ret = 50 - (snr_val - 31600)*20/(158000 - 31600);
	/* snr_val:316~31600,ret:50~80 */
	else if (snr_val > 316)
		ret = 80 - (snr_val - 316)*30/(31600 - 316);
	/* snr_val:0~316,ret:80~100 */
	else
		ret = 100 - (316 - snr_val)*20/316;
	return ret;
#endif
}

int is_atvdemod_work(void)
{
	int ret = 0;

	if (atv_demod_get_state() == ATVDEMOD_STATE_WORK)
		ret = 1;

	return ret;
}

static int atv_demod_get_scan_mode(void)
{
	return atvdemod_scan_mode;
}

static void atv_demod_set_scan_mode(int val)
{
	atvdemod_scan_mode = val;
}

int atv_demod_get_state(void)
{
	return atvdemod_state;
}

static void atv_demod_set_state(int state)
{
	atvdemod_state = state;
}

int atv_demod_enter_mode(void)
{
	int err_code = 0;

#if 0
	if (atv_demod_get_state() == ATVDEMOD_STATE_WORK)
		return 0;
#endif
	if (amlatvdemod_devp->pin_name != NULL)
		amlatvdemod_devp->pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
				amlatvdemod_devp->pin_name);

	adc_set_pll_cntl(1, 0x1, NULL);
	usleep_range(2000, 2100);
	atvdemod_clk_init();
	err_code = atvdemod_init();

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
		aud_demod_clk_gate(1);
		atvauddemod_init();
	}
	if (err_code) {
		pr_dbg("[amlatvdemod..]%s init atvdemod error.\n", __func__);
		return err_code;
	}

	set_aft_thread_enable(1, 0);
	/*
	 * memset(&(amlatvdemod_devp->parm), 0,
	 * sizeof(amlatvdemod_devp->parm));
	 */
	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;

	atv_demod_set_state(ATVDEMOD_STATE_WORK);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

int atv_demod_leave_mode(void)
{
	if (atv_demod_get_state() == ATVDEMOD_STATE_IDEL)
		return 0;

	set_aft_thread_enable(0, 0);
	atvdemod_uninit();
	if (amlatvdemod_devp->pin != NULL) {
		devm_pinctrl_put(amlatvdemod_devp->pin);
		amlatvdemod_devp->pin = NULL;
	}

	adc_set_pll_cntl(0, 0x1, NULL);
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		aud_demod_clk_gate(0);

	/*
	 * memset(&(amlatvdemod_devp->parm), 0,
	 * sizeof(amlatvdemod_devp->parm));
	 */
	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;
	atv_demod_set_state(ATVDEMOD_STATE_IDEL);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void atv_demod_set_params(struct dvb_frontend *fe,
		struct analog_parameters *params)
{
	int ret = -1;
	u32 if_info[2] = { 0 };
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct aml_atvdemod_parameters *atvdemod_param = &priv->atvdemod_param;

	priv->standby = false;

	atvdemod_param->param.frequency = params->frequency;
	atvdemod_param->param.mode = params->mode;
	atvdemod_param->param.audmode = params->audmode;
	atvdemod_param->param.std = params->std;

	/* afc tune disable,must cancel wq before set tuner freq*/
	afc_timer_disable(fe);

	if (fe->ops.tuner_ops.set_analog_params)
		ret = fe->ops.tuner_ops.set_analog_params(fe, params);

	if (fe->ops.tuner_ops.get_if_frequency)
		ret = fe->ops.tuner_ops.get_if_frequency(fe, if_info);

	atvdemod_param->if_inv = if_info[0];
	atvdemod_param->if_freq = if_info[1];

	if ((atvdemod_param->param.std != amlatvdemod_devp->std) ||
		(atvdemod_param->tuner_id == AM_TUNER_R840) ||
		(atvdemod_param->tuner_id == AM_TUNER_SI2151) ||
		(atvdemod_param->tuner_id == AM_TUNER_MXL661) ||
		(atvdemod_param->tuner_id == AM_TUNER_SI2159)) {
		/* open AGC if needed */
		if (amlatvdemod_devp->pin != NULL)
			devm_pinctrl_put(amlatvdemod_devp->pin);

		if (amlatvdemod_devp->pin_name)
			amlatvdemod_devp->pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
					amlatvdemod_devp->pin_name);
#if 0
		last_frq = atvdemod_param->param.frequency;
		last_std = atvdemod_param->param.std;
#endif
		if (amlatvdemod_devp->std != atvdemod_param->param.std ||
		amlatvdemod_devp->audmode != atvdemod_param->param.audmode ||
		amlatvdemod_devp->if_freq != atvdemod_param->if_freq ||
		amlatvdemod_devp->if_inv != atvdemod_param->if_inv ||
		amlatvdemod_devp->tuner_id != atvdemod_param->tuner_id) {
			amlatvdemod_devp->std = atvdemod_param->param.std;
			amlatvdemod_devp->audmode =
					atvdemod_param->param.audmode;
			amlatvdemod_devp->if_freq = atvdemod_param->if_freq;
			amlatvdemod_devp->if_inv = atvdemod_param->if_inv;
			amlatvdemod_devp->tuner_id = atvdemod_param->tuner_id;
			atv_dmd_set_std();
		} else {
			atv_dmd_soft_reset();
			return;
		}

		if (!atv_demod_get_scan_mode())
			atvauddemod_init();

		pr_info("[%s] set std color %s, audio type %s.\n",
			__func__,
			v4l2_std_to_str((0xff000000 & amlatvdemod_devp->std)),
			v4l2_std_to_str((0xffffff & amlatvdemod_devp->std)));
		pr_info("[%s] set if_freq %d, if_inv %d.\n",
			__func__, amlatvdemod_devp->if_freq,
			amlatvdemod_devp->if_inv);
	}

	/* afc tune enable */
	/* analog_search_flag == 0 or afc_range != 0 means searching */
	if ((fe->ops.info.type == FE_ANALOG)
			&& (atv_demod_get_scan_mode() == 0)
			&& (atvdemod_param->param.mode == 0))
		afc_timer_enable(fe);
}

static int atv_demod_has_signal(struct dvb_frontend *fe, u16 *signal)
{
	int vpll_lock = 0;
	int line_lock = 0;

	retrieve_vpll_carrier_lock(&vpll_lock);

	/* add line lock status for atv scan */
	retrieve_vpll_carrier_line_lock(&line_lock);

	if ((vpll_lock & 0x1) == 0 && line_lock == 0) {
		*signal = V4L2_HAS_LOCK;
		pr_info("visual carrier lock:locked\n");
	} else {
		*signal = V4L2_TIMEDOUT;
		pr_info("visual carrier lock:unlocked\n");
	}

	return 0;
}

static void atv_demod_standby(struct dvb_frontend *fe)
{
	if (atv_demod_get_state() != ATVDEMOD_STATE_IDEL) {
		atv_demod_leave_mode();
		atv_demod_set_state(ATVDEMOD_STATE_SLEEP);
	}

	pr_info("%s: OK.\n", __func__);
}

static void atv_demod_tuner_status(struct dvb_frontend *fe)
{
	pr_info("%s.\n", __func__);
}

static int atv_demod_get_afc(struct dvb_frontend *fe, s32 *afc)
{
	*afc = retrieve_vpll_carrier_afc();

	return 0;
}

static void atv_demod_release(struct dvb_frontend *fe)
{
	int instance = 0;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	atv_demod_leave_mode();

	mutex_lock(&atv_demod_list_mutex);

	if (priv)
		instance = hybrid_tuner_release_state(priv);

	if (instance == 0)
		fe->analog_demod_priv = NULL;

	mutex_unlock(&atv_demod_list_mutex);

	pr_info("%s: OK.\n", __func__);
}

static int atv_demod_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	int *state = (int *) priv_cfg;

	if (!state) {
		pr_err("%s: state == NULL.\n", __func__);
		return -1;
	}

	mutex_lock(&atv_demod_list_mutex);

	switch (*state) {
	case AML_ATVDEMOD_INIT:
		if (atv_demod_get_state() != ATVDEMOD_STATE_WORK) {
			if (fe->ops.tuner_ops.set_config)
				fe->ops.tuner_ops.set_config(fe, NULL);
			atv_demod_enter_mode();
		}
		break;

	case AML_ATVDEMOD_UNINIT:
		if (atv_demod_get_state() != ATVDEMOD_STATE_IDEL) {
			atv_demod_leave_mode();
			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
		}
		break;

	case AML_ATVDEMOD_RESUME:
		if (atv_demod_get_state() == ATVDEMOD_STATE_SLEEP) {
			atv_demod_enter_mode();
			if (fe->ops.tuner_ops.resume)
				fe->ops.tuner_ops.resume(fe);
		}
		break;

	case AML_ATVDEMOD_SCAN_MODE:
		atv_demod_set_scan_mode(1);
		afc_timer_disable(fe);
		break;

	case AML_ATVDEMOD_UNSCAN_MODE:
		atv_demod_set_scan_mode(0);
		afc_timer_enable(fe);
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static struct analog_demod_ops atvdemod_ops = {
	.info = {
		.name = DEVICE_NAME,
	},
	.set_params     = atv_demod_set_params,
	.has_signal     = atv_demod_has_signal,
	.standby        = atv_demod_standby,
	.tuner_status   = atv_demod_tuner_status,
	.get_afc        = atv_demod_get_afc,
	.release        = atv_demod_release,
	.set_config     = atv_demod_set_config,
	.i2c_gate_ctrl  = NULL,
};

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, u8 i2c_addr, u32 tuner_id)
{
	int instance = 0;
	struct atv_demod_priv *priv = NULL;

	mutex_lock(&atv_demod_list_mutex);

	instance = hybrid_tuner_request_state(struct atv_demod_priv, priv,
			hybrid_tuner_instance_list,
			i2c_adap, i2c_addr, DEVICE_NAME);

	priv->atvdemod_param.tuner_id = tuner_id;

	switch (instance) {
	case 0:
		mutex_unlock(&atv_demod_list_mutex);
		return NULL;
	case 1:
		INIT_WORK(&priv->demod_wq, aml_fe_do_work);
		fe->analog_demod_priv = priv;
		priv->standby = true;
		pr_info("aml_atvdemod found\n");
		break;
	default:
		fe->analog_demod_priv = priv;
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	fe->ops.info.type = FE_ANALOG;

	memcpy(&fe->ops.analog_ops, &atvdemod_ops,
			sizeof(struct analog_demod_ops));

	return fe;
}
EXPORT_SYMBOL(aml_atvdemod_attach);
