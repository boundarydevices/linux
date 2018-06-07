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
		if ((vpll_lock == 0) && (line_lock == 0)) {
			retrieve_vpll_carrier_audio_power(&power);
			*state = 1;
		} else {
			*state = 0;
			pr_audio("vpll_lock: 0x%x, line_lock: 0x%x\n",
					vpll_lock, line_lock);
		}
	} else {
		*state = 0;
		pr_audio("%s, atv is not work, atv_state: %d.\n",
				__func__, atv_state);
	}

	/* If the atv signal is locked, it means there is audio data,
	 * so no need to check the power value.
	 */
#if 0
	if (power >= 150)
		*state = 1;
	else
		*state = 0;
#endif
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
	/* vdac_enable(1, 1); */
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

	/* vdac_enable(0, 1); */
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
	struct aml_atvdemod_parameters *p = &priv->atvdemod_param;

	priv->standby = false;

	p->param.frequency = params->frequency;
	p->param.mode = params->mode;
	p->param.audmode = params->audmode;
	p->param.std = params->std;

	/* afc tune disable,must cancel wq before set tuner freq*/
	afc_timer_disable(fe);

	if (fe->ops.tuner_ops.set_analog_params)
		ret = fe->ops.tuner_ops.set_analog_params(fe, params);

	if (fe->ops.tuner_ops.get_if_frequency)
		ret = fe->ops.tuner_ops.get_if_frequency(fe, if_info);

	p->if_inv = if_info[0];
	p->if_freq = if_info[1];

	if ((p->param.std != amlatvdemod_devp->std) ||
		(p->tuner_id == AM_TUNER_R840) ||
		(p->tuner_id == AM_TUNER_SI2151) ||
		(p->tuner_id == AM_TUNER_MXL661) ||
		(p->tuner_id == AM_TUNER_SI2159)) {
		/* open AGC if needed */
		if (amlatvdemod_devp->pin != NULL)
			devm_pinctrl_put(amlatvdemod_devp->pin);

		if (amlatvdemod_devp->pin_name)
			amlatvdemod_devp->pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
					amlatvdemod_devp->pin_name);
#if 0 /* unused */
		last_frq = p->param.frequency;
		last_std = p->param.std;
#endif
		if (amlatvdemod_devp->std != p->param.std ||
			amlatvdemod_devp->audmode != p->param.audmode ||
			amlatvdemod_devp->if_freq != p->if_freq ||
			amlatvdemod_devp->if_inv != p->if_inv ||
			amlatvdemod_devp->tuner_id != p->tuner_id) {
			amlatvdemod_devp->std = p->param.std;
			amlatvdemod_devp->audmode = p->param.audmode;
			amlatvdemod_devp->if_freq = p->if_freq;
			amlatvdemod_devp->if_inv = p->if_inv;
			amlatvdemod_devp->tuner_id = p->tuner_id;

			atv_dmd_set_std();

		} else
			atv_dmd_soft_reset();

		if (!atv_demod_get_scan_mode())
			atvauddemod_init();
	}

	/* afc tune enable */
	/* analog_search_flag == 0 or afc_range != 0 means searching */
	if ((fe->ops.info.type == FE_ANALOG)
			&& (atv_demod_get_scan_mode() == 0)
			&& (p->param.mode == 0))
		afc_timer_enable(fe);
}

static int atv_demod_has_signal(struct dvb_frontend *fe, u16 *signal)
{
	int vpll_lock = 0;
	int line_lock = 0;

	retrieve_vpll_carrier_lock(&vpll_lock);

	/* add line lock status for atv scan */
	retrieve_vpll_carrier_line_lock(&line_lock);

	if (vpll_lock == 0 && line_lock == 0) {
		*signal = V4L2_HAS_LOCK;
		pr_info("%s locked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
	} else {
		*signal = V4L2_TIMEDOUT;
		pr_info("%s unlocked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
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


unsigned int tuner_status_cnt = 8; /* 4-->16 test on sky mxl661 */

bool slow_mode;

typedef int (*hook_func_t) (void);
hook_func_t aml_fe_hook_atv_status;
hook_func_t aml_fe_hook_hv_lock;
hook_func_t aml_fe_hook_get_fmt;

void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
	hook_func_t get_fmt)
{
	aml_fe_hook_atv_status = atv_mode;
	aml_fe_hook_hv_lock = cvd_hv_lock;
	aml_fe_hook_get_fmt = get_fmt;

	pr_info("%s: OK.\n", __func__);
}
EXPORT_SYMBOL(aml_fe_hook_cvd);

static v4l2_std_id atvdemod_fmt_2_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		std = V4L2_STD_PAL_DK;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		std = V4L2_STD_PAL_BG;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
		std = V4L2_STD_NTSC_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		std = V4L2_STD_SECAM_L;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		std = V4L2_STD_SECAM_DK;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%0x.\n", __func__, fmt);
	}

	return std;
}

static v4l2_std_id atvdemod_fe_tvin_fmt_to_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_443;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_50:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_I;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_M:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_60:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_60;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_Nc;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		std = V4L2_COLOR_STD_SECAM | V4L2_STD_SECAM_L;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%x\n", __func__, fmt);
		break;
	}

	return std;
}

static void atvdemod_fe_try_analog_format(struct v4l2_frontend *v4l2_fe,
		int auto_search_std, v4l2_std_id *video_fmt,
		unsigned int *audio_fmt)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int i = 0;
	int try_vfmt_cnt = 300;
	int varify_cnt = 0;
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;

	if (auto_search_std & 0x01) {
		for (i = 0; i < try_vfmt_cnt; i++) {
			if (aml_fe_hook_get_fmt == NULL) {
				pr_err("%s: aml_fe_hook_get_fmt == NULL.\n",
						__func__);
				break;
			}
			std_bk = aml_fe_hook_get_fmt();
			if (std_bk) {
				varify_cnt++;
				pr_dbg("get varify_cnt:%d, cnt:%d, std_bk:0x%x\n",
						varify_cnt, i,
						(unsigned int) std_bk);
				if ((v4l2_fe->tuner_id == AM_TUNER_R840
					&& varify_cnt > 0)
					|| varify_cnt > 3)
					break;
			}

			if (i == (try_vfmt_cnt / 3) ||
				(i == (try_vfmt_cnt / 3) * 2)) {
				/* Before enter search,
				 * need set the std,
				 * then, try others std.
				 */
				if (p->std & V4L2_COLOR_STD_PAL)
					p->std = V4L2_COLOR_STD_NTSC
					| V4L2_STD_NTSC_M;
#if 0 /*for secam */
				else if (p->std & V4L2_COLOR_STD_NTSC)
					p->std = V4L2_COLOR_STD_SECAM
					| V4L2_STD_SECAM;
#endif
				else if (p->std & V4L2_COLOR_STD_NTSC)
					p->std = V4L2_COLOR_STD_PAL
					| V4L2_STD_PAL_DK;

				p->frequency += 1;
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;

				fe->ops.analog_ops.set_params(fe, &params);
			}
			usleep_range(30 * 1000, 30 * 1000 + 100);
		}

		pr_dbg("get std_bk cnt:%d, std_bk: 0x%x\n",
				i, (unsigned int) std_bk);

		if (std_bk == 0) {
			pr_err("%s: failed to get video fmt, assume PAL.\n",
					__func__);
			std_bk = TVIN_SIG_FMT_CVBS_PAL_I;
			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_DK;

			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);

			usleep_range(20 * 1000, 20 * 1000 + 100);
		}

		std_bk = atvdemod_fe_tvin_fmt_to_v4l2_std(std_bk);
	} else {
		/* Only search std by user setting,
		 * so no need tvafe identify signal.
		 */
		std_bk = p->std;
	}

	*video_fmt = std_bk;

	if (!(auto_search_std & 0x02)) {
		*audio_fmt = p->audmode;
		return;
	}

	if (std_bk & V4L2_COLOR_STD_NTSC) {
#if 1 /* For TV Signal Generator(TG39) test, NTSC need support other audio.*/
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
		audio = aml_audiomode_autodet(fe);
		pr_info("autodet audmode 0x%x\n", audio);
		audio = atvdemod_fmt_2_v4l2_std(audio);
		pr_info("v4l2_std audmode 0x%x\n", audio);
#if 0 /* I don't know what's going on here */
		if (audio == V4L2_STD_PAL_M)
			audio = V4L2_STD_NTSC_M;
		else
			std_bk = V4L2_COLOR_STD_PAL;
#endif
#else /* Now, force to NTSC_M, Ours demod only support M for NTSC.*/
		audio = V4L2_STD_NTSC_M;
		*video_fmt |= V4L2_STD_NTSC_M;
#endif
	} else if (std_bk & V4L2_COLOR_STD_SECAM) {
#if 1 /* For support SECAM-DK/BG/I/L */
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L);
		audio = aml_audiomode_autodet(fe);
		pr_info("autodet audmode 0x%x\n", audio);
		audio = atvdemod_fmt_2_v4l2_std(audio);
		pr_info("v4l2_std audmode 0x%x\n", audio);
#else
		audio = V4L2_STD_SECAM_L;
#endif
	} else {
		/*V4L2_COLOR_STD_PAL*/
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
		audio = aml_audiomode_autodet(fe);
		pr_info("autodet audmode 0x%x\n", audio);
		audio = atvdemod_fmt_2_v4l2_std(audio);
		pr_info("v4l2_std audmode 0x%x\n", audio);
#if 0 /* Why do this to me? We need support PAL_M.*/
		if (audio == V4L2_STD_PAL_M) {
			audio = atvdemod_fmt_2_v4l2_std(broad_std_except_pal_m);
			pr_info("select audmode 0x%x\n", audio);
		}
#endif
	}

	*audio_fmt = audio;
}

static int atvdemod_fe_afc_closer(struct v4l2_frontend *v4l2_fe, int minafcfreq,
		int maxafcfreq, int isAutoSearch)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	int afc = 100;
	__u32 set_freq;
	int count = 25;
	int lock_cnt = 0;
	static int freq_success;
	static int temp_freq, temp_afc;
	struct timespec time_now;
	static struct timespec success_time;
	unsigned int tuner_id = v4l2_fe->tuner_id;

	pr_dbg("[%s] freq_success: %d, freq: %d, minfreq: %d, maxfreq: %d\n",
		__func__, freq_success, p->frequency, minafcfreq, maxafcfreq);

	/* avoid more search the same program, except < 45.00Mhz */
	if (abs(p->frequency - freq_success) < 3000000
			&& p->frequency > 45000000) {
		ktime_get_ts(&time_now);
		pr_err("[%s] tv_sec now:%ld,tv_sec success:%ld\n",
				__func__, time_now.tv_sec, success_time.tv_sec);
		/* beyond 10s search same frequency is ok */
		if ((time_now.tv_sec - success_time.tv_sec) < 10)
			return -1;
	}

	/*do the auto afc make sure the afc < 50k or the range from api */
	if ((fe->ops.analog_ops.get_afc || fe->ops.tuner_ops.get_afc) &&
		fe->ops.tuner_ops.set_analog_params) {

		set_freq = p->frequency;
		while (abs(afc) > AFC_BEST_LOCK) {
			if (tuner_id == AM_TUNER_SI2151 ||
				tuner_id == AM_TUNER_SI2159 ||
				tuner_id == AM_TUNER_R840)
				usleep_range(20 * 1000, 20 * 1000 + 100);
			else if (tuner_id == AM_TUNER_MXL661)
				usleep_range(30 * 1000, 30 * 1000 + 100);

			if (fe->ops.analog_ops.get_afc &&
			((tuner_id == AM_TUNER_R840) ||
			(tuner_id == AM_TUNER_SI2151) ||
			(tuner_id == AM_TUNER_SI2159) ||
			(tuner_id == AM_TUNER_MXL661)))
				fe->ops.analog_ops.get_afc(fe, &afc);
			else if (fe->ops.tuner_ops.get_afc)
				fe->ops.tuner_ops.get_afc(fe, &afc);

			pr_dbg("[%s] get afc %d khz, freq %u.\n",
					__func__, afc, p->frequency);

			if (afc == 0xffff) {
				/*last lock, but this unlock,so try get afc*/
				if (lock_cnt > 0) {
					p->frequency = temp_freq +
							temp_afc * 1000;
					pr_err("[%s] force lock, f:%d\n",
							__func__, p->frequency);
					break;
				}

				afc = 500;
			} else {
				lock_cnt++;
				temp_freq = p->frequency;
				if (afc > 50)
					temp_afc = 500;
				else if (afc < -50)
					temp_afc = -500;
				else
					temp_afc = afc;
			}

			if (((abs(afc) > (500 - AFC_BEST_LOCK))
				&& (abs(afc) < (500 + AFC_BEST_LOCK))
				&& (abs(afc) != 500))
				|| ((abs(afc) == 500) && (lock_cnt > 0))) {
				p->frequency += afc * 1000;
				break;
			}

			if (afc >= (500 + AFC_BEST_LOCK))
				afc = 500;

			p->frequency += afc * 1000;

			if (unlikely(p->frequency > maxafcfreq)) {
				pr_err("[%s] [%d] is exceed maxafcfreq[%d]\n",
					__func__, p->frequency, maxafcfreq);
				p->frequency = set_freq;
				return -1;
			}
#if 0 /*if enable, it would miss program*/
			if (unlikely(c->frequency < minafcfreq)) {
				pr_dbg("[%s] [%d] is exceed minafcfreq[%d]\n",
						__func__,
						c->frequency, minafcfreq);
				c->frequency = set_freq;
				return -1;
			}
#endif
			if (likely(!(count--))) {
				pr_err("[%s] exceed the afc count\n", __func__);
				p->frequency = set_freq;
				return -1;
			}

			/* delete it will miss program
			 * when c->frequency equal program frequency
			 */
			p->frequency++;
			if (fe->ops.tuner_ops.set_analog_params) {
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;
				fe->ops.tuner_ops.set_analog_params(fe,
						&params);
			}
		}

		freq_success = p->frequency;
		ktime_get_ts(&success_time);
		pr_dbg("[%s] get afc %d khz done, freq %u.\n",
				__func__, afc, p->frequency);
	}

	return 0;
}

static int atvdemod_fe_set_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	struct dvb_frontend *fe = &amlatvdemod_devp->v4l2_fe.fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		aud_mode = tvp->data & 0xFF;
		priv->sound_sys.output_mode = tvp->data & 0xFF;
		break;

	case V4L2_SLOW_SEARCH_MODE:
		tvp->data = slow_mode;
		break;

	default:
		pr_dbg("%s: property %d doesn't exist\n",
				__func__, tvp->cmd);
		return -EINVAL;
	}

	return 0;
}

static int atvdemod_fe_get_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		tvp->data = ((aud_std  & 0xFF) << 16)
				| ((signal_audmode & 0xFF) << 8)
				| (aud_mode & 0xFF);
		break;

	case V4L2_SLOW_SEARCH_MODE:
		slow_mode = tvp->data;
		break;

	default:
		pr_dbg("%s: property %d doesn't exist\n",
				__func__, tvp->cmd);
		return -EINVAL;
	}

	return 0;
}

static enum v4l2_search atvdemod_fe_search(struct v4l2_frontend *v4l2_fe)
{
	struct analog_parameters params;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	enum v4l2_status tuner_state = V4L2_TIMEDOUT;
	enum v4l2_status ade_state = V4L2_TIMEDOUT;
	bool pll_lock = false;
	/*struct atv_status_s atv_status;*/
	__u32 set_freq = 0;
	__u32 minafcfreq = 0, maxafcfreq = 0;
	__u32 afc_step = 0;
	int tuner_status_cnt_local = tuner_status_cnt;
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;
	int double_check_cnt = 1;
	int auto_search_std = 0;
	int search_count = 0;
	bool try_secam = false;
	int ret = -1;
	unsigned int tuner_id = v4l2_fe->tuner_id;
	int priv_cfg = 0;

	if (unlikely(!fe || !p ||
			!fe->ops.tuner_ops.get_status ||
			!fe->ops.analog_ops.has_signal ||
			!fe->ops.analog_ops.set_params ||
			!fe->ops.analog_ops.set_config)) {
		pr_err("[%s] error: NULL function or pointer.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	if (p->afc_range == 0) {
		pr_err("[%s] afc_range == 0, skip the search\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	pr_info("[%s] afc_range: [%d], tuner: [%d], freq: [%d], flag: [%d].\n",
			__func__, p->afc_range, tuner_id,
			p->frequency, p->flag);

	/* backup the freq by api */
	set_freq = p->frequency;

	/* Before enter search, need set the std first.
	 * If set p->analog.std == 0, will search all std (PAL/NTSC/SECAM),
	 * and need tvafe identify signal type.
	 */
	if (p->std == 0) {
		p->std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		auto_search_std = 0x01;
		pr_dbg("[%s] user std is 0, so set it to NTSC | M.\n",
				__func__);
	}

	if (p->audmode == 0) {
		if (auto_search_std)
			p->audmode = p->std & 0x00FFFFFF;
		else {
			if (p->std & V4L2_COLOR_STD_PAL)
				p->audmode = V4L2_STD_PAL_DK;
			else if (p->std & V4L2_COLOR_STD_NTSC)
				p->audmode = V4L2_STD_NTSC_M;
			else if (p->std & V4L2_COLOR_STD_SECAM)
				p->audmode = V4L2_STD_PAL_DK;

			p->std = (p->std & 0xFF000000) | p->audmode;
		}
		auto_search_std |= 0x02;
		pr_dbg("[%s] user audmode is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->audmode));
	}

	priv_cfg = AML_ATVDEMOD_SCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	/*set the afc_range and start freq*/
	minafcfreq = p->frequency - p->afc_range;
	maxafcfreq = p->frequency + p->afc_range;

	/*from the current freq start, and set the afc_step*/
	/*if step is 2Mhz,r840 will miss program*/
	if (slow_mode || (tuner_id == AM_TUNER_R840)
			|| (p->afc_range == ATV_AFC_1_0MHZ)) {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	} else if (!slow_mode) {
		afc_step = ATV_AFC_2_0MHZ;
	} else {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	}

	/**enter auto search mode**/
	pr_dbg("[%s] Auto search std: 0x%08x, audmode: 0x%08x\n",
			__func__, (unsigned int) p->std, p->audmode);

	while (minafcfreq <= p->frequency &&
			p->frequency <= maxafcfreq) {

		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;
		fe->ops.analog_ops.set_params(fe, &params);

		pr_dbg("[%s] [%d] is processing, [min=%d, max=%d].\n",
				__func__, p->frequency, minafcfreq, maxafcfreq);

		pll_lock = false;
		tuner_status_cnt_local = tuner_status_cnt;
		do {
			if (tuner_id == AM_TUNER_MXL661) {
				usleep_range(30 * 1000, 30 * 1000 + 100);
			} else if (tuner_id == AM_TUNER_R840) {
				usleep_range(20 * 1000, 20 * 1000 + 100);
				fe->ops.tuner_ops.get_status(fe,
						&tuner_state);
			} else {
				/* AM_TUNER_SI2151 and AM_TUNER_SI2159 */
				usleep_range(10 * 1000, 10 * 1000 + 100);
			}

			fe->ops.analog_ops.has_signal(fe, (u16 *)&ade_state);
			tuner_status_cnt_local--;
			if (((ade_state == V4L2_HAS_LOCK ||
				tuner_state == V4L2_HAS_LOCK) &&
				(tuner_id != AM_TUNER_R840)) ||
				((ade_state == V4L2_HAS_LOCK &&
				tuner_state == V4L2_HAS_LOCK) &&
				(tuner_id == AM_TUNER_R840))) {
				pll_lock = true;
				break;
			}

			if (tuner_status_cnt_local == 0) {
				if (auto_search_std &&
					try_secam == false &&
					!(p->std & V4L2_COLOR_STD_SECAM) &&
					!(p->std & V4L2_STD_SECAM_L)) {
					/* backup the std and audio mode */
					std_bk = p->std;
					audio = p->audmode;

					p->std = (V4L2_COLOR_STD_SECAM
							| V4L2_STD_SECAM_L);
					p->audmode = V4L2_STD_SECAM_L;

					params.frequency = p->frequency;
					params.mode = p->afc_range;
					params.audmode = p->audmode;
					params.std = p->std;
					fe->ops.analog_ops.set_params(fe,
							&params);

					try_secam = true;

					tuner_status_cnt_local =
						tuner_status_cnt / 2;

					continue;
				}

				if (try_secam) {
					p->std = std_bk;
					p->audmode = audio;

					params.frequency = p->frequency;
					params.mode = p->afc_range;
					params.audmode = p->audmode;
					params.std = p->std;
					fe->ops.analog_ops.set_params(fe,
							&params);

					try_secam = false;
				}

				break;
			}
		} while (1);

		std_bk = 0;
		audio = 0;

		if (pll_lock) {

			pr_dbg("[%s] freq: [%d] pll lock success\n",
					__func__, p->frequency);
#if 0 /* In get_pll_status has line_lock check.*/
			if (fee->tuner->drv->id == AM_TUNER_MXL661) {
				fe->ops.analog_ops.get_atv_status(fe,
						&atv_status);
				if (atv_status.atv_lock)
					usleep_range(30 * 1000,
						30 * 1000 + 100);
			}
#endif
			ret = atvdemod_fe_afc_closer(v4l2_fe, minafcfreq,
					maxafcfreq + ATV_AFC_500KHZ, 1);
			if (ret == 0) {
				atvdemod_fe_try_analog_format(v4l2_fe,
						auto_search_std,
						&std_bk, &audio);

				pr_dbg("[%s] freq:%d, std_bk:0x%x, audmode:0x%x, search OK.\n",
						__func__, p->frequency,
						(unsigned int) std_bk, audio);

				if (std_bk != 0) {
					p->audmode = audio;
					p->std = std_bk;
					/*avoid std unenable */
					p->frequency -= 1;
					std_bk = 0;
					audio = 0;
				}

				/* sync param */
				/* aml_fe_analog_sync_frontend(fe); */
				priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
				fe->ops.analog_ops.set_config(fe, &priv_cfg);
				return V4L2_SEARCH_SUCCESS;
			}
		}

		/*avoid sound format is not match after search over */
		if (std_bk != 0 && audio != 0) {
			p->std = std_bk;
			p->audmode = audio;

			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);
			std_bk = 0;
			audio = 0;
		}

		pr_dbg("[%s] freq[analog.std:0x%08x] is[%d] unlock\n",
				__func__,
				(uint32_t) p->std, p->frequency);

		if (p->frequency >= 44200000 &&
			p->frequency <= 44300000 &&
			double_check_cnt) {
			double_check_cnt--;
			pr_err("%s 44.25Mhz double check\n", __func__);
		} else {
			++search_count;
			p->frequency += afc_step * ((search_count % 2) ?
					-search_count : search_count);
		}
	}

	pr_dbg("[%s] [%d] over of range [min=%d, max=%d], search failed.\n",
			__func__, p->frequency, minafcfreq, maxafcfreq);
	p->frequency = set_freq;

	priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	return DVBFE_ALGO_SEARCH_FAILED;
}

static struct v4l2_frontend_ops atvdemod_fe_ops = {
	.set_property = atvdemod_fe_set_property,
	.get_property = atvdemod_fe_get_property,
	.search = atvdemod_fe_search,
};

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct v4l2_frontend *v4l2_fe,
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

	memcpy(&v4l2_fe->ops, &atvdemod_fe_ops,
			sizeof(struct v4l2_frontend_ops));

	return fe;
}
EXPORT_SYMBOL(aml_atvdemod_attach);
