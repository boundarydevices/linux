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

#define DEVICE_NAME "aml_atvdemod_fe"

#define ATVDEMOD_STATE_IDEL  0
#define ATVDEMOD_STATE_WORK  1
#define ATVDEMOD_STATE_SLEEP 2
static int atvdemod_state = ATVDEMOD_STATE_IDEL;

static DEFINE_MUTEX(atv_demod_list_mutex);
static LIST_HEAD(hybrid_tuner_instance_list);

unsigned int reg_23cf = 0x88188832; /*IIR filter*/
module_param(reg_23cf, uint, 0664);
MODULE_PARM_DESC(reg_23cf, "\n reg_23cf\n");

static int btsc_sap_mode = 1;	/*0: off 1:monitor 2:auto */
module_param(btsc_sap_mode, int, 0644);
MODULE_DESCRIPTION("btsc sap mode\n");

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

	if (atvdemod_state == ATVDEMOD_STATE_WORK) {
		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);
		if ((vpll_lock == 0) && (line_lock == 0))
			retrieve_vpll_carrier_audio_power(&power);
	} else
		pr_err("%s, atv is not work, atvdemod_state: %d.\n",
				__func__, atvdemod_state);

	if (power >= 150)
		*state = 1;
	else
		*state = 0;

	pr_dbg("aml_fe_get_atvaudio_state: %d, power = %d.\n",
			*state, power);
}

int aml_atvdemod_get_btsc_sap_mode(void)
{
	return btsc_sap_mode;
}

unsigned int atvdemod_scan_mode; /*IIR filter*/
module_param(atvdemod_scan_mode, uint, 0664);
MODULE_PARM_DESC(atvdemod_scan_mode, "\n atvdemod_scan_mode\n");

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

	if (atvdemod_state == ATVDEMOD_STATE_WORK)
		ret = 1;

	return ret;
}

int is_atvdemod_scan_mode(void)
{
	return atvdemod_scan_mode;
}
//EXPORT_SYMBOL(is_atvdemod_scan_mode);

void set_atvdemod_scan_mode(int val)
{
	atvdemod_scan_mode = val;
}

int get_atvdemod_state(void)
{
	return atvdemod_state;
}

void set_atvdemod_state(int state)
{
	atvdemod_state = state;
}

int atv_demod_enter_mode(void)
{
	int err_code = 0;

#if 0
	if (atvdemod_state == ATVDEMOD_STATE_WORK)
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

	/* set_aft_thread_enable(1, 0); */
	/*
	 * memset(&(amlatvdemod_devp->parm), 0,
	 * sizeof(amlatvdemod_devp->parm));
	 */
	atvdemod_state = ATVDEMOD_STATE_WORK;

	pr_info("%s: OK.\n", __func__);

	return 0;
}

int atv_demod_leave_mode(void)
{
	if (atvdemod_state == ATVDEMOD_STATE_IDEL)
		return 0;

	/* set_aft_thread_enable(0, 0); */
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
	atvdemod_state = ATVDEMOD_STATE_IDEL;

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
	/* afc_timer_disable(); */

	if (fe->ops.tuner_ops.set_analog_params)
		ret = fe->ops.tuner_ops.set_analog_params(fe, params);

	if (fe->ops.tuner_ops.get_if_frequency)
		ret = fe->ops.tuner_ops.get_if_frequency(fe, if_info);

	atvdemod_param->if_inv = if_info[0];
	atvdemod_param->if_freq = if_info[1];

	if ((atvdemod_param->param.std != amlatvdemod_devp->std) ||
		(atvdemod_param->tuner_id == AM_TUNER_R840) ||
		(atvdemod_param->tuner_id == AM_TUNER_SI2151) ||
		(atvdemod_param->tuner_id == AM_TUNER_MXL661)) {
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
		if (atvdemod_param->param.std != amlatvdemod_devp->std) {
			amlatvdemod_devp->std = atvdemod_param->param.std;
			amlatvdemod_devp->if_freq = atvdemod_param->if_freq;
			amlatvdemod_devp->if_inv = atvdemod_param->if_inv;
			amlatvdemod_devp->tuner_id = atvdemod_param->tuner_id;
			atv_dmd_set_std();
		} else {
			atv_dmd_soft_reset();
			return;
		}

		if (!is_atvdemod_scan_mode())
			atvauddemod_init();

		pr_info("%s: set std color %lld, audio type %lld.\n",
			__func__, amlatvdemod_devp->std, amlatvdemod_devp->std);
		pr_info("%s: set if_freq %d, if_inv %d.\n",
			__func__, amlatvdemod_devp->if_freq,
			amlatvdemod_devp->if_inv);
	}
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
	if (get_atvdemod_state() != ATVDEMOD_STATE_IDEL) {
		atv_demod_leave_mode();
		set_atvdemod_state(ATVDEMOD_STATE_SLEEP);
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
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	atv_demod_leave_mode();

	mutex_lock(&atv_demod_list_mutex);

	if (priv)
		hybrid_tuner_release_state(priv);

	mutex_unlock(&atv_demod_list_mutex);

	fe->analog_demod_priv = NULL;

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

	if (*state == AML_ATVDEMOD_INIT && atvdemod_state != *state) {
		atv_demod_enter_mode();
		if (fe->ops.tuner_ops.init)
			fe->ops.tuner_ops.init(fe);
	} else if (*state == AML_ATVDEMOD_UNINIT && atvdemod_state != *state) {
		atv_demod_leave_mode();
		if (fe->ops.tuner_ops.release)
			fe->ops.tuner_ops.release(fe);
	} else if (*state == AML_ATVDEMOD_RESUME && atvdemod_state != *state) {
		if (get_atvdemod_state() == ATVDEMOD_STATE_SLEEP)
			atv_demod_enter_mode();
		if (fe->ops.tuner_ops.resume)
			fe->ops.tuner_ops.resume(fe);
	}

	mutex_unlock(&atv_demod_list_mutex);

	return 0;
}

static struct analog_demod_ops atvdemod_ops = {
	.info = {
		.name = "atv_demod",
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
	void *p = NULL;
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

	switch (tuner_id) {
	case AM_TUNER_R840:
		break;
	case AM_TUNER_SI2151:
		p = v4l2_attach(si2151_attach, fe, i2c_adap, i2c_addr);
		break;
	case AM_TUNER_MXL661:
		p = v4l2_attach(mxl661_attach, fe, i2c_adap, i2c_addr);
		break;
	}

	if (!p)
		pr_err("%s: v4l2_attach tuner %d error.\n",
				__func__, tuner_id);

	return fe;
}
