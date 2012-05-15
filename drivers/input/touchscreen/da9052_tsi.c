/*
 * da9052_tsi.c  --  TSI driver for Dialog DA9052
 *
 * Copyright(c) 2009 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd <dchen@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/mfd/da9052/reg.h>
#include <linux/mfd/da9052/tsi_cfg.h>
#include <linux/mfd/da9052/tsi.h>
#include <linux/mfd/da9052/gpio.h>
#include <linux/mfd/da9052/adc.h>
#include <linux/regulator/consumer.h>

#define TSI_XN	1		//pin 1
#define TSI_YP	2
#define TSI_ADC	3
#define TSI_YN	4
#define TSI_XP	5		//pin 5

#define WAIT_FOR_PEN_DOWN	0
#define WAIT_FOR_SAMPLING	1
#define SAMPLING_ACTIVE		2

static int calibration[7];
module_param_array(calibration, int, NULL, S_IRUGO | S_IWUSR);

static unsigned char drop_samples = 1;
module_param(drop_samples, byte, S_IRUGO | S_IWUSR);

static unsigned char ts_mode = 255;
module_param(ts_mode, byte, S_IRUGO | S_IWUSR);

void da9052_calibrate(unsigned *px, unsigned *py)
{
	int x, y, x1, y1;
	if (calibration[6]) {
		x1 = *px;
		y1 = *py;

		x = calibration[0] * x1 +
			calibration[1] * y1 +
			calibration[2];
		x /= calibration[6];
		if (x < 0)
			x = 0;
		y = calibration[3] * x1 +
			calibration[4] * y1 +
			calibration[5];
		y /= calibration[6];
		if (y < 0)
			y = 0;
		*px = x ;
		*py = y ;
	}
}

u32 da9052_tsi_get_input_dev(struct da9052_tsi_info *ts, u8 off)
{
	if (off > NUM_INPUT_DEVS-1)
		return -EINVAL;
	return (u32)ts->input_devs[off];
}

static ssize_t write_da9052_reg(struct da9052 *da9052, u8 reg_addr, u8 data)
{
	ssize_t ret = 0;
	struct da9052_ssc_msg ssc_msg;

	ssc_msg.addr =  reg_addr;
	ssc_msg.data =  data;
	ret = da9052->write(da9052, &ssc_msg);
	if (ret) {
		pr_debug("%s: da9052_ssc_write Failed %d\n", __func__, ret);
	}

	return ret;
}

static ssize_t da9052_tsi_config_manual_mode(struct da9052_ts_priv *priv,
						u8 state)
{
	ssize_t ret=0;
	struct da9052_tsi_info *ts = &priv->tsi_info;
	unsigned set_mask;
	if (state == ENABLE)
		set_mask = DA9052_TSICONTB_TSIMAN;
	else if (state == DISABLE)
		set_mask = 0;
	else {
		pr_debug("%s: Invalid state Passed\n", __func__);
		return -EINVAL;
	}
	ret = priv->da9052->register_modify(priv->da9052, DA9052_TSICONTB_REG,
			DA9052_TSICONTB_TSIMAN, set_mask);
	if (ret)
		return ret;

	if (state == DISABLE)
		ts->tsi_conf.man_cont.tsi_cont_b.tsi_man = RESET;
	else
		ts->tsi_conf.man_cont.tsi_cont_b.tsi_man = SET;

	ret = write_da9052_reg(priv->da9052, DA9052_ADCMAN_REG, DA9052_ADC_TSI);
	if (ret) {
		pr_debug("%s: ADC write Failed\n", __func__);
	}
	return ret;
}

static ssize_t da9052_tsi_config_gpio(struct da9052_ts_priv *priv)
{
	ssize_t ret = 0;
	struct da9052_ssc_msg ssc_msg[3];
	struct da9052_modify_msg mod_msg[3];

	ssc_msg[0].addr  = DA9052_GPIO0203_REG;
	ssc_msg[1].addr  = DA9052_GPIO0405_REG;
	ssc_msg[2].addr  = DA9052_GPIO0607_REG;

	mod_msg[0].clear_mask = DA9052_GPIO0203_GPIO3PIN;
	mod_msg[0].set_mask = 0;
	mod_msg[1].clear_mask = (DA9052_GPIO0405_GPIO4PIN | DA9052_GPIO0405_GPIO5PIN);
	mod_msg[1].set_mask = 0;
	mod_msg[2].clear_mask = (DA9052_GPIO0607_GPIO6PIN | DA9052_GPIO0607_GPIO7PIN);
	mod_msg[2].set_mask = 0;

	ret = priv->da9052->modify_many(priv->da9052, ssc_msg, mod_msg, 3);
	if (ret) {
		pr_debug("%s: da9052_ssc_modify_many Failed\n", __func__);
	}
	return ret;
}

static ssize_t da9052_tsi_config_measure_seq(struct da9052_ts_priv *priv,
						enum TSI_MEASURE_SEQ seq)
{
	unsigned set_mask;
	if (seq == XYZP_MODE)
		set_mask = 0;
	else if (seq == XP_MODE)
		set_mask = DA9052_TSICONTA_TSIMODE;
	else {
		pr_debug("%s: Invalid Value passed\n", __func__);
		return -EINVAL;
	}
	return priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
			DA9052_TSICONTA_TSIMODE, set_mask);
}

static ssize_t da9052_tsi_config_delay(struct da9052_ts_priv *priv,
					enum TSI_DELAY delay)
{
	if (delay > priv->tsi_pdata->max_tsi_delay) {
		pr_debug("%s: invalid value for tsi delay!!!\n", __func__);
		return -EINVAL;
	}
	return priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
			DA9052_TSICONTA_TSIDELAY,
			(delay << priv->tsi_pdata->tsi_delay_bit_shift));
}

static ssize_t da9052_tsi_config_skip_slots(struct da9052_ts_priv *priv,
					enum TSI_SLOT_SKIP skip)
{
	if (skip > priv->tsi_pdata->max_tsi_skip_slot) {
		pr_debug("%s: invalid value for tsi skip slots!!!\n", __func__);
		return -EINVAL;
	}
	return priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
			DA9052_TSICONTA_TSISKIP,
			(skip << priv->tsi_pdata->tsi_skip_bit_shift));
}

static ssize_t da9052_tsi_config_power_supply(struct da9052_ts_priv *priv,
						u8 state)
{
	struct da9052_tsi_info *ts = &priv->tsi_info;

	if (priv->config_index >= DA9052_5_WIRE_ADC_IO1)
		return 0;

	if (state != ENABLE && state != DISABLE) {
		pr_debug("%s: Invalid state Passed\n", __func__);
		return -EINVAL;
	}

	if (!IS_ERR(ts->ts_regulator)) {
		if (state == ENABLE) {
			if (!ts->ts_reg_en) {
				if (!regulator_enable(ts->ts_regulator)) {
					ts->ts_reg_en = 1;
					pr_debug("%s: enabled regulator\n", __func__);
				} else {
					pr_err("%s: error enabling regulator\n", __func__);
					regulator_put(ts->ts_regulator);
					ts->ts_regulator = NULL;
				}
			}
		} else {
			if (ts->ts_reg_en) {
				ts->ts_reg_en = 0;
				if (!regulator_disable(ts->ts_regulator)) {
					pr_debug("%s: disabled regulator\n", __func__);
				} else {
					pr_err("%s: error disabling regulator\n", __func__);
					regulator_put(ts->ts_regulator);
					ts->ts_regulator = NULL;
				}
			}
		}
	}
	return 0;
}

#ifdef OLD_WAY
void insert_tsi_point(struct da9052_ts_priv *priv, unsigned x, unsigned y, unsigned z)
{
	struct da9052_tsi *tsi = &priv->tsi_reg;
	struct da9052_tsi_reg *pdata;
	mutex_lock(&tsi->tsi_fifo_lock);
	pdata = &tsi->tsi_fifo[tsi->tsi_fifo_end];
	pdata->x = x;
	pdata->y = y;
	pdata->z = z;
	pdata->pressed = 1;
	incr_with_wrap(tsi->tsi_fifo_end);
	if (tsi->tsi_fifo_end == tsi->tsi_fifo_start)
		tsi->tsi_fifo_start++;
	mutex_unlock(&tsi->tsi_fifo_lock);
}
void insert_tsi_release(struct da9052_ts_priv *priv)
{

}
#else

void insert_tsi_point(struct da9052_ts_priv *priv, unsigned x, unsigned y, unsigned z)
{
	struct da9052_tsi_info *ts = &priv->tsi_info;
	struct input_dev *ip_dev = (struct input_dev *)da9052_tsi_get_input_dev(
			ts, (u8)TSI_INPUT_DEVICE_OFF);
	mutex_lock(&ts->point_lock);
	priv->tsi_reg.sum.x += x;
	priv->tsi_reg.sum.y += y;
	priv->tsi_reg.sum.z += z;
	priv->tsi_reg.sum_cnt++;
	if (priv->tsi_reg.sum_cnt >= TSI_AVERAGE_FILTER_SIZE) {
		x = priv->tsi_reg.sum.x / TSI_AVERAGE_FILTER_SIZE;
		y = priv->tsi_reg.sum.y / TSI_AVERAGE_FILTER_SIZE;
		z = priv->tsi_reg.sum.z / TSI_AVERAGE_FILTER_SIZE;
		priv->tsi_reg.sum.x = 0;
		priv->tsi_reg.sum.y = 0;
		priv->tsi_reg.sum.z = 0;
		priv->tsi_reg.sum_cnt = 0;
		if (priv->drop_samples) {
			priv->drop_samples--;
			goto exit1;
		}
		da9052_calibrate(&x, &y);
		input_report_abs(ip_dev, ABS_X, x);
		input_report_abs(ip_dev, ABS_Y, y);
		input_report_abs(ip_dev, ABS_PRESSURE, z);
		input_report_key(ip_dev, BTN_TOUCH, 1);
		input_sync(ip_dev);
		if (priv->config_index) {
			ts->release_pending = 1;
		} else {
			if (!ts->release_pending) {
				ts->release_pending = 1;
				ts->release_timer.expires = jiffies + msecs_to_jiffies(30);
				add_timer(&ts->release_timer);
			} else {
				mod_timer(&ts->release_timer, jiffies + msecs_to_jiffies(30));
			}
		}
		pr_debug("%s: x=%x, y=%x, z=%x, pressed\n",
			__func__, x, y, z);
	}
exit1:
	mutex_unlock(&ts->point_lock);
}

void insert_tsi_release(struct da9052_ts_priv *priv)
{
	struct da9052_tsi_info *ts = &priv->tsi_info;
	struct input_dev *ip_dev = (struct input_dev *)da9052_tsi_get_input_dev(
			ts, (u8)TSI_INPUT_DEVICE_OFF);
	mutex_lock(&ts->point_lock);
	if (ts->release_pending) {
		ts->release_pending = 0;
		del_timer(&ts->release_timer);

		priv->tsi_reg.sum.x = 0;
		priv->tsi_reg.sum.y = 0;
		priv->tsi_reg.sum.z = 0;
		priv->tsi_reg.sum_cnt = 0;
		input_report_abs(ip_dev, ABS_PRESSURE, 0);
		input_report_key(ip_dev, BTN_TOUCH, 0);
		input_sync(ip_dev);
		pr_debug("%s: released\n", __func__);
	}
	priv->drop_samples = drop_samples;
	mutex_unlock(&ts->point_lock);
}

static void release_work_rtn(struct work_struct *work)
{
	struct da9052_tsi_info *ts = container_of(work, struct da9052_tsi_info, release_work);
	struct da9052_ts_priv *priv = container_of(ts, struct da9052_ts_priv, tsi_info);
	insert_tsi_release(priv);
}

static void release_timer_rtn(unsigned long data)
{
	struct da9052_ts_priv *priv = (struct da9052_ts_priv *)data;
	struct da9052_tsi_info *ts = &priv->tsi_info;
	schedule_work(&ts->release_work);
}

#endif

#define MGP_even(pin,type,mode) ((pin) | ((type) << 2) | (mode) << 3)
#define MGP_odd(pin,type,mode) (MGP_even(pin,type,mode) << 4)

#define PIN_ADC		0
#define PIN_TSIREF	0
#define PIN_XP		0
#define PIN_XM		0
#define PIN_YP		0
#define PIN_YM		0
#define PIN_GPI		1
#define PIN_GPO_OD	2
#define PIN_GPO		3

#define TYPE_VDD_IO1		0
#define TYPE_VDD_IO2		1
#define TYPE_ACTIVE_LOW		0
#define TYPE_ACTIVE_HIGH	1

#define MODE_LOW	0
#define MODE_HIGH	1
#define MODE_NODEBOUNCE	0
#define MODE_DEBOUNCE	1
#define MODE_NOLDO_EN	1

#define setup_reg(reg, val)						\
	{								\
		.addr = reg,						\
		.data = val,						\
	}

#define setup_gpreg(reg, type1, io1, val1, type2, io2, val2)	setup_reg(reg, \
		MGP_even(type1, io1, val1) | MGP_odd(type2, io2, val2))

#define setup_gpreg_io1(reg, type1, val1, type2, val2)	setup_gpreg(reg, \
		type1, TYPE_VDD_IO1, val1, type2, TYPE_VDD_IO1, val2)

#define setup_gpreg_io2(reg, type1, val1, type2, val2)	setup_gpreg(reg, \
		type1, TYPE_VDD_IO2, val1, type2, TYPE_VDD_IO2, val2)

struct da9052_state_msg
{
	int	cnt;
	const struct da9052_ssc_msg *msg;
};

/*
 * GP0 shorted to GP2,
 * GP1 unused
 * GP2 sense
 * GP3 YM
 * GP4 YP
 * GP5 XM
 * GP6 XP
 * GP7 vref (LDO9 touch screen reference voltage)
 */

/*
 * ST_CUR_IDLE, sense pulled high by GP0
 * yp(0)(gp4)    xp(0)(gp6)
 * xm(0)(gp5)    ym(0)(gp3)
 */
const struct da9052_ssc_msg adc_io1_cur_idle[] = {
	setup_gpreg_io1(DA9052_GPIO0001_REG, PIN_GPO_OD, MODE_HIGH, PIN_GPO_OD, MODE_HIGH),
	setup_gpreg_io1(DA9052_GPIO0203_REG, PIN_GPI, MODE_NOLDO_EN, PIN_GPO, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_GPO, MODE_LOW, PIN_TSIREF, MODE_LOW),
};
const struct da9052_ssc_msg adc_io2_cur_idle[] = {
	setup_gpreg_io2(DA9052_GPIO0001_REG, PIN_GPO_OD, MODE_HIGH, PIN_GPO_OD, MODE_HIGH),
	setup_gpreg_io2(DA9052_GPIO0203_REG, PIN_GPI, MODE_NOLDO_EN, PIN_GPO, MODE_LOW),
	setup_gpreg_io2(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
	setup_gpreg_io2(DA9052_GPIO0607_REG, PIN_GPO, MODE_LOW, PIN_TSIREF, MODE_LOW),
};

/*
 * ST_CUR_X
 * yp(0)(gp4)    xp(1)(gp6)
 * xm(0)(gp5)    ym(1)(gp3)
 */
const struct da9052_ssc_msg adc_io1_cur_x[] = {
	setup_gpreg_io1(DA9052_GPIO0001_REG, PIN_GPI, MODE_NODEBOUNCE, PIN_GPO_OD, MODE_HIGH),
	setup_gpreg_io1(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_HIGH),
#if 0
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
#endif
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_GPO, MODE_HIGH, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};
const struct da9052_ssc_msg adc_io2_cur_x[] = {
	setup_gpreg_io2(DA9052_GPIO0001_REG, PIN_GPI, MODE_NODEBOUNCE, PIN_GPO_OD, MODE_HIGH),
	setup_gpreg_io2(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_HIGH),
#if 0
	setup_gpreg_io2(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
#endif
	setup_gpreg_io2(DA9052_GPIO0607_REG, PIN_GPO, MODE_HIGH, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};

/*
 * ST_CUR_Y
 * yp(1)(gp4)    xp(1)(gp6)
 * xm(0)(gp5)    ym(0)(gp3)
 */
const struct da9052_ssc_msg adc_io1_cur_y[] = {
#if 0
	setup_gpreg_io1(DA9052_GPIO0001_REG, PIN_GPI, MODE_NODEBOUNCE, PIN_GPO_OD, MODE_HIGH),
#endif
	setup_gpreg_io1(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_GPO, MODE_HIGH, PIN_GPO, MODE_LOW),
#if 0
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_GPO, MODE_HIGH, PIN_TSIREF, MODE_LOW),
#endif
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};
const struct da9052_ssc_msg adc_io2_cur_y[] = {
#if 0
	setup_gpreg_io2(DA9052_GPIO0001_REG, PIN_GPI, MODE_NODEBOUNCE, PIN_GPO_OD, MODE_HIGH),
#endif
	setup_gpreg_io2(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_LOW),
	setup_gpreg_io2(DA9052_GPIO0405_REG, PIN_GPO, MODE_HIGH, PIN_GPO, MODE_LOW),
#if 0
	setup_gpreg_io2(DA9052_GPIO0607_REG, PIN_GPO, MODE_HIGH, PIN_TSIREF, MODE_LOW),
#endif
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};

/*
 * ST_CUR_Z
 */
const struct da9052_ssc_msg adc_io1_cur_z[] = {
	setup_gpreg_io1(DA9052_GPIO0001_REG, PIN_GPO_OD, MODE_HIGH, PIN_GPO_OD, MODE_HIGH),
#if 0
	setup_gpreg_io1(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_LOW),
#endif
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_GPO, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};
const struct da9052_ssc_msg adc_io2_cur_z[] = {
	setup_gpreg_io2(DA9052_GPIO0001_REG, PIN_GPO_OD, MODE_HIGH, PIN_GPO_OD, MODE_HIGH),
#if 0
	setup_gpreg_io2(DA9052_GPIO0203_REG, PIN_ADC, MODE_NODEBOUNCE, PIN_GPO, MODE_LOW),
#endif
	setup_gpreg_io2(DA9052_GPIO0405_REG, PIN_GPO, MODE_LOW, PIN_GPO, MODE_LOW),
	setup_gpreg_io2(DA9052_GPIO0607_REG, PIN_GPO, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_ADCCONT_REG, 0),
	setup_reg(DA9052_ADCMAN_REG, 0x16),	/* manual conversion */
};

const struct da9052_state_msg adc_io1[] = {
	{ARRAY_SIZE(adc_io1_cur_idle), adc_io1_cur_idle},
	{ARRAY_SIZE(adc_io1_cur_x), adc_io1_cur_x},
	{ARRAY_SIZE(adc_io1_cur_y), adc_io1_cur_y},
	{ARRAY_SIZE(adc_io1_cur_z), adc_io1_cur_z},
};
const struct da9052_state_msg adc_io2[] = {
	{ARRAY_SIZE(adc_io2_cur_idle), adc_io2_cur_idle},
	{ARRAY_SIZE(adc_io2_cur_x), adc_io2_cur_x},
	{ARRAY_SIZE(adc_io2_cur_y), adc_io2_cur_y},
	{ARRAY_SIZE(adc_io2_cur_z), adc_io2_cur_z},
};

#define TSIA_PEN_DET_EN		(1 << 1)
#define TSIA_XP_MODE		(1 << 2)
#define TSIA_SKIP		(2 << 3)	//skip 5 slots
#define TSIA_DELAY		(3 << 6)	//settle delay of 4 slots
#define TSIA_DEFAULT		(TSIA_XP_MODE | TSIA_SKIP | TSIA_DELAY)

#define TSIB_CLOSE_XP		(1 << 0)
#define TSIB_CLOSE_XM		(1 << 1)
#define TSIB_CLOSE_YP		(1 << 2)
#define TSIB_CLOSE_YM		(1 << 3)
#define TSIB_MANUAL		(1 << 6)
#define TSIB_ADCREF_SELECT	(1 << 7)	//always use Y+/Y- for reference

#define TSIB_MEASURE_XY		TSIB_CLOSE_YP | TSIB_CLOSE_YM | TSIB_MANUAL | TSIB_ADCREF_SELECT
#define TSIB_MEASURE_Z				TSIB_CLOSE_YM | TSIB_MANUAL

/*
 * GP2 used as xp, always GPO
 * GP3 used as ym, always tsi mode
 * GP4 used as yp, always tsi mode
 * GP5 used as xm, always GPO
 * GP6 used as sense, always tsi mode
 */
#define setup_xp_io1(xp_val)	setup_gpreg_io1(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_YM, MODE_LOW)
#define setup_xp_io2(xp_val)	setup_gpreg_io2(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_YM, MODE_LOW)

#define setup_xm_io1(xm_val)	setup_gpreg_io1(	\
		DA9052_GPIO0405_REG, PIN_YP, MODE_LOW, PIN_GPO, xm_val)
#define setup_xm_io2(xm_val)	setup_gpreg_io2(	\
		DA9052_GPIO0405_REG, PIN_YP, MODE_LOW, PIN_GPO, xm_val)

/*
 * ST_CUR_IDLE, sense(XP - GP6) - pulled high, ym - grounded
 * yp(_)    xp(0)
 * xm(0)    ym(_)
 */
const struct da9052_ssc_msg yxsxy_io1_cur_idle[] = {
	/* GP0 shorted to GP2, GP1 unused */
	setup_gpreg(DA9052_GPIO0001_REG, PIN_GPI, TYPE_ACTIVE_HIGH, MODE_NODEBOUNCE,
			PIN_GPO_OD, TYPE_VDD_IO1, MODE_HIGH),
	setup_xp_io1(MODE_LOW),
	setup_xm_io1(MODE_LOW),
	/* GP6 XP used as sense, GP7 vref */
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_XP, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
};
const struct da9052_ssc_msg yxsxy_io2_cur_idle[] = {
	/* GP0 shorted to GP2, GP1 unused */
	setup_gpreg(DA9052_GPIO0001_REG, PIN_GPI, TYPE_ACTIVE_HIGH, MODE_NODEBOUNCE,
			PIN_GPO_OD, TYPE_VDD_IO2, MODE_HIGH),
	setup_xp_io2(MODE_LOW),
	setup_xm_io2(MODE_LOW),
	/* GP6 XP used as sense, GP7 vref */
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_XP, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
};

/*
 * ST_CUR_X, XM,YP: high, XP,YM low
 * yp(1)    xp(0)
 * xm(1)    ym(0)
 */
const struct da9052_ssc_msg yxsxy_io1_cur_x[] = {
#if 0
	setup_xp_io1(MODE_LOW),
#endif
	setup_xm_io1(MODE_HIGH),
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_XY),
};
const struct da9052_ssc_msg yxsxy_io2_cur_x[] = {
#if 0
	setup_xp_io2(MODE_LOW),
#endif
	setup_xm_io2(MODE_HIGH),
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_XY),
};

/*
 * ST_CUR_Y, XM,YM: low, XP,YP: high
 * yp(1)    xp(1)
 * xm(0)    ym(0)
 */
const struct da9052_ssc_msg yxsxy_io1_cur_y[] = {
	setup_xp_io1(MODE_HIGH),
	setup_xm_io1(MODE_LOW),
#if 0
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
#endif
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_XY),
};
const struct da9052_ssc_msg yxsxy_io2_cur_y[] = {
	setup_xp_io2(MODE_HIGH),
	setup_xm_io2(MODE_LOW),
#if 0
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
#endif
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_XY),
};

/* ST_CUR_Z */
const struct da9052_ssc_msg yxsxy_io1_cur_z[] = {
	setup_xp_io1(MODE_LOW),
#if 0
	setup_xm_io1(MODE_LOW),
#endif
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_Z),
};
const struct da9052_ssc_msg yxsxy_io2_cur_z[] = {
	setup_xp_io2(MODE_LOW),
#if 0
	setup_xm_io2(MODE_LOW),
#endif
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, TSIB_MEASURE_Z),
};

const struct da9052_state_msg yxsxy_io1[] = {
	{ARRAY_SIZE(yxsxy_io1_cur_idle), yxsxy_io1_cur_idle},
	{ARRAY_SIZE(yxsxy_io1_cur_x), yxsxy_io1_cur_x},
	{ARRAY_SIZE(yxsxy_io1_cur_y), yxsxy_io1_cur_y},
	{ARRAY_SIZE(yxsxy_io1_cur_z), yxsxy_io1_cur_z},
};
const struct da9052_state_msg yxsxy_io2[] = {
	{ARRAY_SIZE(yxsxy_io2_cur_idle), yxsxy_io2_cur_idle},
	{ARRAY_SIZE(yxsxy_io2_cur_x), yxsxy_io2_cur_x},
	{ARRAY_SIZE(yxsxy_io2_cur_y), yxsxy_io2_cur_y},
	{ARRAY_SIZE(yxsxy_io2_cur_z), yxsxy_io2_cur_z},
};

/*
 * The new rev of board has J34
 * pin 1 - XM(GP5)	use as ym, switch between GPO & tsi mode
 * pin 2 - YM(GP3)	use as xm, always tsi mode
 * pin 3 - XP(GP6)	use as sense, always tsi mode
 * pin 4 - GP2		use as xp, always GPO
 * pin 5 - YP(GP4)	use as yp, always tsi mode
 */
#define N_TSIB_MEASURE_XY	TSIB_CLOSE_YP | TSIB_CLOSE_XM | TSIB_MANUAL | TSIB_ADCREF_SELECT
#define N_TSIB_MEASURE_Z			TSIB_CLOSE_YM | TSIB_MANUAL

#define nsetup_xp_io1(xp_val)	setup_gpreg_io1(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_YM, MODE_LOW)
#define nsetup_xp_io2(xp_val)	setup_gpreg_io2(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_YM, MODE_LOW)

#define nsetup_xp_xm_io1(xp_val, xm_val)	setup_gpreg_io1(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_GPO, xm_val)
#define nsetup_xp_xm_io2(xp_val, xm_val)	setup_gpreg_io2(	\
		DA9052_GPIO0203_REG, PIN_GPO, xp_val, PIN_GPO, xm_val)

/* ST_CUR_IDLE, YM(xm) low, XP(sense) high */
const struct da9052_ssc_msg xysxy_io1_cur_idle[] = {
	setup_gpreg(DA9052_GPIO0001_REG, PIN_GPI, TYPE_ACTIVE_HIGH, MODE_NODEBOUNCE,
			PIN_GPO_OD, TYPE_VDD_IO1, MODE_HIGH),
	nsetup_xp_io1(MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_YP, MODE_LOW, PIN_XM, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_XP, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
};
const struct da9052_ssc_msg xysxy_io2_cur_idle[] = {
	setup_gpreg(DA9052_GPIO0001_REG, PIN_GPI, TYPE_ACTIVE_HIGH, MODE_NODEBOUNCE,
			PIN_GPO_OD, TYPE_VDD_IO2, MODE_HIGH),
	nsetup_xp_io2(MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0405_REG, PIN_YP, MODE_LOW, PIN_XM, MODE_LOW),
	setup_gpreg_io1(DA9052_GPIO0607_REG, PIN_XP, MODE_LOW, PIN_TSIREF, MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
};

/*
 * ST_CUR_X, XM,YP: low, XP,YM high
 *  YP(yp)(1)    GP2(xp)(0)
 *  YM(xm)(1)     XM(ym)(0)
 */
const struct da9052_ssc_msg xysxy_io1_cur_x[] = {
	nsetup_xp_xm_io1(MODE_LOW, MODE_HIGH),
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_XY),
};
const struct da9052_ssc_msg xysxy_io2_cur_x[] = {
	nsetup_xp_xm_io2(MODE_LOW, MODE_HIGH),
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_XY),
};

/*
 * ST_CUR_Y, XM,YM: low, XP,YP: high
 * YP(yp)(1)    GP2(xp)(1)
 * YM(xm)(0)     XM(yn)(0)
 */
const struct da9052_ssc_msg xysxy_io1_cur_y[] = {
	nsetup_xp_xm_io1(MODE_HIGH, MODE_LOW),
#if 0
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
#endif
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_XY),
};
const struct da9052_ssc_msg xysxy_io2_cur_y[] = {
	nsetup_xp_xm_io2(MODE_HIGH, MODE_LOW),
#if 0
	setup_reg(DA9052_TSICONTA_REG, TSIA_DEFAULT),
#endif
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_XY),
};

/* ST_CUR_Z */
const struct da9052_ssc_msg xysxy_io1_cur_z[] = {
	nsetup_xp_io1(MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_Z),
};
const struct da9052_ssc_msg xysxy_io2_cur_z[] = {
	nsetup_xp_io2(MODE_LOW),
	setup_reg(DA9052_TSICONTA_REG, TSIA_PEN_DET_EN | TSIA_DEFAULT),
	setup_reg(DA9052_TSICONTB_REG, N_TSIB_MEASURE_Z),
};

const struct da9052_state_msg xysxy_io1[] = {
	{ARRAY_SIZE(xysxy_io1_cur_idle), xysxy_io1_cur_idle},
	{ARRAY_SIZE(xysxy_io1_cur_x), xysxy_io1_cur_x},
	{ARRAY_SIZE(xysxy_io1_cur_y), xysxy_io1_cur_y},
	{ARRAY_SIZE(xysxy_io1_cur_z), xysxy_io1_cur_z},
};
const struct da9052_state_msg xysxy_io2[] = {
	{ARRAY_SIZE(xysxy_io2_cur_idle), xysxy_io2_cur_idle},
	{ARRAY_SIZE(xysxy_io2_cur_x), xysxy_io2_cur_x},
	{ARRAY_SIZE(xysxy_io2_cur_y), xysxy_io2_cur_y},
	{ARRAY_SIZE(xysxy_io2_cur_z), xysxy_io2_cur_z},
};

const struct da9052_state_msg* ts_config[] = {
	yxsxy_io1, xysxy_io1,
	yxsxy_io2, xysxy_io2,
	adc_io1, adc_io2,
};

void da9052_config_5w_measure(struct da9052_ts_priv *priv, unsigned state)
{
	int ret;
	const struct da9052_state_msg* cfg = ts_config[priv->config_index - 1];
	priv->tsi_reg.cur_state = state;
//	pr_debug("%s: entry %d\n", __func__, state);

	da9052_lock(priv->da9052);
	ret = priv->da9052->write_many(priv->da9052, (struct  da9052_ssc_msg *)cfg[state].msg, cfg[state].cnt);
	da9052_unlock(priv->da9052);
	if (ret) {
		pr_debug("%s: Error in reading TSI data\n", __func__);
	}
}

static void da9052_tsi_5w_data_ready_handler(struct da9052_eh_nb *eh_data, u32 event)
{
	struct da9052_tsi_reg *pdata;
	struct da9052_ssc_msg tsi_data[3];
	s32 ret;
	u16 sample;
	unsigned cnt;
	struct da9052_ts_priv *priv =
		container_of(eh_data, struct da9052_ts_priv, datardy_nb);

//	pr_debug("%s: entry\n", __func__);
	if (priv->tsi_reg.tsi_state !=  SAMPLING_ACTIVE)
		return;

	if (!priv->tsi_reg.cur_state)
		return;

	if (priv->config_index >= DA9052_5_WIRE_ADC_IO1) {
		tsi_data[0].addr  = DA9052_ADCRESL_REG;
		tsi_data[1].addr  = DA9052_ADCRESH_REG;
		cnt = 2;
	} else {
		tsi_data[0].addr  = DA9052_TSIXMSB_REG;
		tsi_data[1].addr  = DA9052_TSIYMSB_REG;
		tsi_data[2].addr  = DA9052_TSILSB_REG;
		cnt = 3;
	}

	da9052_lock(priv->da9052);
	ret = priv->da9052->read_many(priv->da9052, tsi_data, 2);
	da9052_unlock(priv->da9052);
	if (ret) {
		pr_debug("%s: Error in reading TSI data\n", __func__);
		return;
	}
	if (priv->config_index >= DA9052_5_WIRE_ADC_IO1)
		sample = (tsi_data[0].data & 3) | (tsi_data[1].data << 2);
	else
		sample = (tsi_data[2].data & 3) | (tsi_data[0].data << 2);

	pdata = &priv->tsi_reg.cur_sample;
	switch (priv->tsi_reg.cur_state) {
	case ST_CUR_X:
		pdata->x = sample;
		da9052_config_5w_measure(priv, ST_CUR_Y);
		break;
	case ST_CUR_Y:
		pdata->y = sample;
		da9052_config_5w_measure(priv, ST_CUR_Z);
		break;
	case ST_CUR_Z:
		pdata->z = sample;
		if (sample < 0x200) {
			/* touch still detected */
			da9052_config_5w_measure(priv, ST_CUR_X);
			insert_tsi_point(priv, pdata->x, pdata->y,
					pdata->z);
		} else {
			/* touch NOT detected */
			da9052_config_5w_measure(priv, ST_CUR_IDLE);
			tsi_data[0].addr  = DA9052_STATUSC_REG;
			da9052_lock(priv->da9052);
			ret = priv->da9052->read_many(priv->da9052, tsi_data, 1);
			da9052_unlock(priv->da9052);
			pr_debug("%s: x=%x, y=%x, z=%x, not pressed statusc=%x\n",
				__func__, pdata->x, pdata->y, pdata->z,
				tsi_data[0].data);

			priv->tsi_reg.tsi_state =  WAIT_FOR_PEN_DOWN;
			insert_tsi_release(priv);
		}
		break;
	}
}

static void da9052_tsi_data_ready_handler(struct da9052_eh_nb *eh_data, u32 event)
{
	struct da9052_ssc_msg tsi_data[4];
	s32 ret;
	u16 x, y, z, pressed;
	struct da9052_ts_priv *priv =
		container_of(eh_data, struct da9052_ts_priv, datardy_nb);

	pr_debug("%s: entry\n", __func__);
	if (priv->tsi_reg.tsi_state !=  SAMPLING_ACTIVE)
		return;

	tsi_data[0].addr  = DA9052_TSIXMSB_REG;
	tsi_data[1].addr  = DA9052_TSIYMSB_REG;
	tsi_data[2].addr  = DA9052_TSILSB_REG;
	tsi_data[3].addr  = DA9052_TSIZMSB_REG;

	tsi_data[0].data  = 0;
	tsi_data[1].data  = 0;
	tsi_data[2].data  = 0;
	tsi_data[3].data  = 0;

	da9052_lock(priv->da9052);

	ret = priv->da9052->read_many(priv->da9052, tsi_data, 4);
	da9052_unlock(priv->da9052);
	if (ret) {
		pr_debug("%s: Error in reading TSI data\n", __func__ );
		return;
	}
	pressed = tsi_data[2].data;
	x = (tsi_data[0].data << 2) | (pressed & 3);
	y = (tsi_data[1].data << 2) | ((pressed >> 2) & 3);
	z = (tsi_data[3].data << 2) | ((pressed >> 4) & 3);
	pressed = (pressed >> 6) & 1;
	if (z < 0x20)
		pressed = 0;
	if (pressed)
		insert_tsi_point(priv, x, y, z);
	else {
		/* Disable auto mode */
		priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
				DA9052_TSICONTA_AUTOTSIEN, 0);

		priv->tsi_reg.tsi_state =  WAIT_FOR_PEN_DOWN;
		pr_debug("%s: x=%x, y=%x, z=%x released\n", __func__, x, y, z);
		insert_tsi_release(priv);
	}
}

static void da9052_tsi_reg_datardy_event(struct da9052_ts_priv *priv)
{
	ssize_t ret = 0;
	struct da9052_tsi_info  *ts = &priv->tsi_info;

	pr_debug("%s: entry\n", __func__);
	if(ts->datardy_reg_status)
	{
		pr_debug("%s: Data Ready Registeration is already done\n", __func__);
		return;
	}

	ret = priv->da9052->register_event_notifier(priv->da9052,
						&priv->datardy_nb);

	if(ret) {
		pr_debug("%s: EH Registeration Failed: ret = %d\n", __func__, ret);
		ts->datardy_reg_status = RESET;
	} else
		ts->datardy_reg_status = SET;

	return;
}
static void da9052_tsi_reg_pendwn_event(struct da9052_ts_priv *priv);

static void da9052_tsi_pen_down_handler(struct da9052_eh_nb *eh_data, u32 event)
{
	struct da9052_ts_priv *priv =
		container_of(eh_data, struct da9052_ts_priv, pd_nb);
	struct da9052_tsi_info *ts = &priv->tsi_info;

	pr_debug("%s: entry\n", __func__);
	if (priv->tsi_reg.tsi_state !=  WAIT_FOR_PEN_DOWN)
		return;

	priv->tsi_reg.tsi_state = WAIT_FOR_SAMPLING;

	if (ts->tsi_conf.state != TSI_AUTO_MODE) {
		pr_debug("%s: Configure TSI to auto mode, then call this API.\n", __func__);
		goto fail;
	}

	if (da9052_tsi_config_power_supply(priv, ENABLE))
		goto fail;

	priv->tsi_reg.tsi_state =  SAMPLING_ACTIVE;
	if (priv->config_index) {
		da9052_config_5w_measure(priv, ST_CUR_X);
		pr_debug("%s: ready for adc, %d\n", __func__, priv->datardy_nb.eve_type);
	} else {
		/* Enable auto mode */
		if (priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
				DA9052_TSICONTA_AUTOTSIEN, DA9052_TSICONTA_AUTOTSIEN))
			goto fail;
	}
	goto success;

fail:
	pr_debug("%s failed\n", __func__);
	if (ts->pd_reg_status) {
		priv->da9052->unregister_event_notifier(priv->da9052,
						&priv->pd_nb);
		ts->pd_reg_status = RESET;

		priv->da9052->register_event_notifier(priv->da9052,
					&priv->datardy_nb);
		da9052_tsi_reg_pendwn_event(priv);
	}

success:
	pr_debug("%s: Exit\n", __func__);
}

static void da9052_tsi_reg_pendwn_event(struct da9052_ts_priv *priv)
{
	ssize_t ret = 0;
	struct da9052_tsi_info  *ts = &priv->tsi_info;
	pr_debug("%s: entry\n", __func__);

	if (ts->pd_reg_status) {
		pr_debug("%s: Pen down Registeration is already done\n", __func__);
		return;
	}

	ret = priv->da9052->register_event_notifier(priv->da9052, &priv->pd_nb);
	if (ret) {
		pr_debug("%s: EH Registeration Failed: ret = %d\n", __func__, ret);
		ts->pd_reg_status = RESET;
	} else
		ts->pd_reg_status = SET;

	priv->os_data_cnt = 0;
	priv->raw_data_cnt = 0;
	if (priv->config_index)
		da9052_config_5w_measure(priv, ST_CUR_IDLE);
	return;
}

static ssize_t da9052_tsi_config_state(struct da9052_ts_priv *priv,
					enum TSI_STATE state)
{
	s32 ret;
	struct da9052_tsi_info *ts = &priv->tsi_info;
	pr_debug("%s: entry\n", __func__);

	if (ts->tsi_conf.state == state)
		return 0;

	switch (state) {
	case TSI_AUTO_MODE:
		ts->tsi_zero_data_cnt = 0;
		priv->early_data_flag = TRUE;
		priv->debounce_over = FALSE;
		priv->win_reference_valid = FALSE;

		clean_tsi_fifos(priv);
		/* Disable auto mode */
		ret = priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
				DA9052_TSICONTA_AUTOTSIEN, 0);
		if (ret)
			return ret;

		ret = da9052_tsi_config_manual_mode(priv, DISABLE);
		if (ret)
			return ret;

		ret = da9052_tsi_config_power_supply(priv, DISABLE);
		if (ret)
			return ret;

		da9052_tsi_reg_pendwn_event(priv);
		da9052_tsi_reg_datardy_event(priv);
		ret = priv->da9052->event_enable(priv->da9052, priv->pd_nb.eve_type);
		if (ret)
			return ret;

		ret = priv->da9052->event_enable(priv->da9052, priv->datardy_nb.eve_type);
		if (ret)
			return ret;

		if (priv->config_index < DA9052_5_WIRE_ADC_IO1) {
			/* Enable pen detect*/
			ret = priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
					DA9052_TSICONTA_PENDETEN, DA9052_TSICONTA_PENDETEN);
			if (ret)
				return ret;
		}
		break;

	case TSI_IDLE:
		ret = priv->da9052->event_disable(priv->da9052, priv->datardy_nb.eve_type);
		if (ret)
			return ret;
		ret = priv->da9052->event_disable(priv->da9052, priv->pd_nb.eve_type);
		if (ret)
			return ret;

		/* Disable pen detect, auto mode */
		ret = priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
				DA9052_TSICONTA_PENDETEN | DA9052_TSICONTA_AUTOTSIEN, 0);
		if (ret)
			return ret;

		ret = da9052_tsi_config_manual_mode(priv, DISABLE);
		if (ret)
			return ret;

		ret = da9052_tsi_config_power_supply(priv, DISABLE);
		if (ret)
			return ret;

		if (ts->pd_reg_status) {
			priv->da9052->unregister_event_notifier(priv->da9052,
								&priv->pd_nb);
			ts->pd_reg_status = RESET;
		}
		break;

	default:
		pr_debug("%s:  Invalid state passed\n", __func__);
		return -EINVAL;
	}

	ts->tsi_conf.state = state;

	return 0;
}

#ifdef OLD_WAY
static void da9052_tsi_penup_event(struct da9052_ts_priv *priv)
{

	struct da9052_tsi_info *ts = &priv->tsi_info;
	struct input_dev *ip_dev =
		(struct input_dev *)da9052_tsi_get_input_dev(&priv->tsi_info,
		(u8)TSI_INPUT_DEVICE_OFF);

	/* Disable auto mode */
	if (priv->da9052->register_modify(priv->da9052, DA9052_TSICONTA_REG,
			DA9052_TSICONTA_AUTOTSIEN, 0))
		goto exit;

	if (da9052_tsi_config_power_supply(priv, ENABLE))
		goto exit;

	if (priv->da9052->event_enable(priv->da9052, priv->pd_nb.eve_type))
		goto exit;


	priv->tsi_reg.tsi_state =  WAIT_FOR_PEN_DOWN;
	if (priv->config_index)
		da9052_config_5w_measure(priv, ST_CUR_IDLE);
	ts->tsi_zero_data_cnt = 0;
	priv->early_data_flag = TRUE;
	priv->debounce_over = FALSE;
	priv->win_reference_valid = FALSE;

	pr_debug("The raw data count is %d \n", priv->raw_data_cnt);
	pr_debug("The OS data count is %d \n", priv->os_data_cnt);
	pr_debug("PEN UP DECLARED \n");
	input_report_abs(ip_dev, ABS_PRESSURE, 0);
	input_report_key(ip_dev, BTN_TOUCH, 0);
	input_sync(ip_dev);
	priv->os_data_cnt = 0;
	priv->raw_data_cnt = 0;

exit:
	clean_tsi_fifos(priv);
	return;
}

static s32 da9052_tsi_get_rawdata(struct da9052_tsi *tsi, struct da9052_tsi_reg *buf, u8 cnt) {
	u32 data_cnt = 0;
	u32 rem_data_cnt = 0;

	mutex_lock(&tsi->tsi_fifo_lock);

	if (tsi->tsi_fifo_start < tsi->tsi_fifo_end) {
		data_cnt = (tsi->tsi_fifo_end - tsi->tsi_fifo_start);

		if (cnt < data_cnt)
			data_cnt = cnt;

		memcpy(buf, &tsi->tsi_fifo[tsi->tsi_fifo_start],
				sizeof(struct da9052_tsi_reg) * data_cnt);

		tsi->tsi_fifo_start += data_cnt;

		if (tsi->tsi_fifo_start == tsi->tsi_fifo_end) {
			tsi->tsi_fifo_start = 0;
			tsi->tsi_fifo_end = 0;
		}
	} else if (tsi->tsi_fifo_start > tsi->tsi_fifo_end) {
		data_cnt = ((TSI_FIFO_SIZE - tsi->tsi_fifo_start)
		+ tsi->tsi_fifo_end);

		if (cnt < data_cnt)
			data_cnt = cnt;

		if (data_cnt <= (TSI_FIFO_SIZE - tsi->tsi_fifo_start)) {
			memcpy(buf, &tsi->tsi_fifo[tsi->tsi_fifo_start],
				sizeof(struct da9052_tsi_reg) * data_cnt);

			tsi->tsi_fifo_start += data_cnt;
			if (tsi->tsi_fifo_start >= TSI_FIFO_SIZE)
				tsi->tsi_fifo_start = 0;
		} else {
			memcpy(buf, &tsi->tsi_fifo[tsi->tsi_fifo_start],
				sizeof(struct da9052_tsi_reg)
				* (TSI_FIFO_SIZE - tsi->tsi_fifo_start));

			rem_data_cnt = (data_cnt -
				(TSI_FIFO_SIZE - tsi->tsi_fifo_start));

			memcpy(buf, &tsi->tsi_fifo[0],
				sizeof(struct da9052_tsi_reg) * rem_data_cnt);

			tsi->tsi_fifo_start = rem_data_cnt;
		}

		if (tsi->tsi_fifo_start == tsi->tsi_fifo_end) {
				tsi->tsi_fifo_start = 0;
			tsi->tsi_fifo_end = 0;
		}
	} else
		data_cnt = 0;

	mutex_unlock(&tsi->tsi_fifo_lock);

	return data_cnt;
}


int da9052_tsi_get_reg_data(struct da9052_ts_priv *priv)
{
	int copy_cnt = 0;
	if (down_interruptible(&priv->tsi_reg_fifo.lock))
		return 0;
	for (;;) {
		int ret;
		u32 free_cnt;
		u32 poll_cnt;
		s32 last = TSI_REG_DATA_BUF_SIZE;
		s32 insert = priv->tsi_reg_fifo.tail;
		s32 remove = priv->tsi_reg_fifo.head;
		if (remove == 0)
			remove = TSI_REG_DATA_BUF_SIZE;
		if (insert < remove)
			last = remove - 1;
		free_cnt = last - insert;
		poll_cnt = TSI_POLL_SAMPLE_CNT - copy_cnt;
		if (free_cnt > poll_cnt)
			free_cnt = poll_cnt;
		if (free_cnt == 0)
			break;
		ret = da9052_tsi_get_rawdata(&priv->tsi_reg,
				&priv->tsi_reg_fifo.data[insert],
				free_cnt);
		if (ret <= 0) {
			if (ret < 0)
				copy_cnt = ret;
			break;
		}
		if (ret > free_cnt) {
			pr_err("%s: EH copied more data\n", __func__);
			copy_cnt = -EINVAL;
			break;
		}
		insert += ret;
		if (insert >= TSI_REG_DATA_BUF_SIZE)
			insert = 0;
		priv->tsi_reg_fifo.tail = insert;
		copy_cnt += ret;
		if (ret != free_cnt)
			break;
	}
	up(&priv->tsi_reg_fifo.lock);
	return copy_cnt;
}

static ssize_t da9052_tsi_reg_proc_thread(void *ptr)
{
	u32 data_cnt;
	struct da9052_tsi_info *ts;
	struct da9052_ts_priv *priv = (struct da9052_ts_priv *)ptr;

	pr_debug("%s: entry\n", __func__);
	set_freezable();

	while (priv->tsi_reg_proc_thread.state == ACTIVE) {

		try_to_freeze();

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(priv->
					tsi_reg_data_poll_interval));

		ts = &priv->tsi_info;

		if (!ts->pen_dwn_event)
			continue;

		data_cnt = da9052_tsi_get_reg_data(priv);

		da9052_tsi_process_reg_data(priv);

		if (data_cnt)
			ts->tsi_zero_data_cnt = 0;
		else {
			if (++(ts->tsi_zero_data_cnt) >
						     ts->tsi_penup_count) {
				pr_debug("%s: pen_dwn_event %d\n", __func__, ts->pen_dwn_event);
				ts->pen_dwn_event = RESET;
				da9052_tsi_penup_event(priv);
			}
		}
	}

	complete_and_exit(&priv->tsi_reg_proc_thread.notifier, 0);
	return 0;
}

void start_reg_proc_thread(struct da9052_ts_priv *priv)
{
	init_completion(&priv->tsi_reg_proc_thread.notifier);
	priv->tsi_reg_proc_thread.state = ACTIVE;
	priv->tsi_reg_proc_thread.pid =
				kernel_thread(da9052_tsi_reg_proc_thread,
					priv, CLONE_KERNEL | SIGCHLD);
}

void start_raw_proc_thread(struct da9052_ts_priv *priv)
{
	init_completion(&priv->tsi_raw_proc_thread.notifier);
	priv->tsi_raw_proc_thread.state = ACTIVE;
	priv->tsi_raw_proc_thread.pid =
				kernel_thread(da9052_tsi_raw_proc_thread,
					priv, CLONE_KERNEL | SIGCHLD);
}

void init_tsi_threads(struct da9052_ts_priv *priv)
{
	ts->tsi_calib = get_calib_config();
	da9052_init_tsi_fifos(priv);

	start_reg_proc_thread(priv);
	start_raw_proc_thread(priv);
}
void tsi_fifo_init(struct da9052_ts_priv *priv)
{
	mutex_init(&priv->tsi_reg.tsi_fifo_lock);
}
void tsi_fifo_destroy(struct da9052_ts_priv *priv)
{
	mutex_destroy(&priv->tsi_reg.tsi_fifo_lock);

	priv->tsi_reg_proc_thread.state = INACTIVE;
	wait_for_completion(&priv->tsi_reg_proc_thread.notifier);

	priv->tsi_raw_proc_thread.state = INACTIVE;
	wait_for_completion(&priv->tsi_raw_proc_thread.notifier);
}
#else

void tsi_fifo_init(struct da9052_ts_priv *priv)
{
}
void tsi_fifo_destroy(struct da9052_ts_priv *priv)
{
}
void clean_tsi_fifos(struct da9052_ts_priv *priv)
{
}
void init_tsi_threads(struct da9052_ts_priv *priv)
{
}
#endif

static ssize_t da9052_tsi_suspend(struct platform_device *dev,
							pm_message_t state)
{
	printk(KERN_INFO "%s: called\n", __func__);
	return 0;
}

static ssize_t da9052_tsi_resume(struct platform_device *dev)
{
	printk(KERN_INFO "%s: called\n", __func__);
	return 0;
}

static ssize_t __devinit da9052_tsi_set_sampling_mode(struct da9052_ts_priv *priv,
					u8 mode)
{
	struct da9052_tsi_info *ts = &priv->tsi_info;
	ssize_t ret = 0;
	unsigned set_mask;
	if (mode == ECONOMY_MODE)
		set_mask = 0;
	else if (mode == FAST_MODE)
		set_mask = DA9052_ADCCONT_ADCMODE;
	else {
		pr_debug("%s: Invalid interval passed\n", __func__);
		return -EINVAL;
	}

	ret = priv->da9052->register_modify(priv->da9052, DA9052_ADCCONT_REG,
			DA9052_ADCCONT_ADCMODE, set_mask);
	if (ret) {
		pr_debug("%s: write_da9052_reg Failed\n", __func__);
		return ret;
	}

	switch (mode) {
	case ECONOMY_MODE:
		priv->tsi_reg_data_poll_interval =
			TSI_ECO_MODE_REG_DATA_PROCESSING_INTERVAL;
		priv->tsi_raw_data_poll_interval =
			TSI_ECO_MODE_RAW_DATA_PROCESSING_INTERVAL;
		break;
	case FAST_MODE:
		priv->tsi_reg_data_poll_interval =
			TSI_FAST_MODE_REG_DATA_PROCESSING_INTERVAL;
		priv->tsi_raw_data_poll_interval =
			TSI_FAST_MODE_RAW_DATA_PROCESSING_INTERVAL;
		break;
	}

	ts->tsi_penup_count =
		(u32)priv->tsi_pdata->pen_up_interval /
		priv->tsi_reg_data_poll_interval;

	return 0;
}

static ssize_t __devinit da9052_tsi_create_input_dev(struct input_dev **ip_dev,
							u8 n)
{
	u8 i;
	s32 ret;
	struct input_dev *dev = NULL;

	if (!n)
		return -EINVAL;

	for (i = 0; i < n; i++) {
		dev = input_allocate_device();
		if (!dev) {
			pr_err("%s:%s():memory allocation for \
					inputdevice failed\n", __FILE__,
								__func__);
			return -ENOMEM;
		}

		ip_dev[i] = dev;
		switch (i) {
		case TSI_INPUT_DEVICE_OFF:
			dev->name = DA9052_TSI_INPUT_DEV;
			dev->phys = "input(tsi)";
			break;
		default:
			break;
		}
	}
	dev->id.vendor = DA9052_VENDOR_ID;
	dev->id.product = DA9052_PRODUCT_ID;
	dev->id.bustype = BUS_RS232;
	dev->id.version = TSI_VERSION;
	dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	dev->evbit[0] = (BIT_MASK(EV_SYN) |
			BIT_MASK(EV_KEY) |
			BIT_MASK(EV_ABS));

	input_set_abs_params(dev, ABS_X, 0, DA9052_DISPLAY_X_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, 0, DA9052_DISPLAY_Y_MAX, 0, 0);
	input_set_abs_params(dev, ABS_PRESSURE, 0, DA9052_TOUCH_PRESSURE_MAX,
				0, 0);

	ret = input_register_device(dev);
	if (ret) {
		pr_err("%s: Could not register input device(touchscreen)!\n", __func__);
		ret = -EIO;
		goto fail;
	}
	return 0;

fail:
	for (; i-- != 0; )
		input_free_device(ip_dev[i]);
	return -EINVAL;
}

static ssize_t __devinit da9052_tsi_init_drv(struct da9052_ts_priv *priv, struct device *dev)
{
	u8 cnt = 0;
	ssize_t ret = 0;
	struct da9052_tsi_info  *ts = &priv->tsi_info;

	mutex_init(&ts->point_lock);
	init_timer(&ts->release_timer);
	ts->release_timer.data = (unsigned long)priv;
	ts->release_timer.function = release_timer_rtn;
	INIT_WORK(&ts->release_work, release_work_rtn);

	ts->ts_regulator = regulator_get(dev, "VDD_A");
	if (IS_ERR(ts->ts_regulator)) {
		dev_err(dev, "%s: error getting regulator VDD_A\n", __func__);
	} else {
		unsigned voltage = priv->tsi_pdata->tsi_supply_voltage;
		pr_info("%s: setting regulator VDD_A voltage to %u\n", __func__, voltage);
		regulator_set_voltage(ts->ts_regulator, voltage, voltage);
	}
	if ((DA9052_GPIO_PIN_3 != DA9052_GPIO_CONFIG_TSI) ||
		(DA9052_GPIO_PIN_4 != DA9052_GPIO_CONFIG_TSI) ||
		(DA9052_GPIO_PIN_5 != DA9052_GPIO_CONFIG_TSI) ||
		(DA9052_GPIO_PIN_6 != DA9052_GPIO_CONFIG_TSI) ||
		(DA9052_GPIO_PIN_7 != DA9052_GPIO_CONFIG_TSI)) {
		printk(KERN_ERR"DA9052_TSI: Configure DA9052 GPIO ");
		printk(KERN_ERR"pins for TSI\n");
		return -EINVAL;
	}

	ret = da9052_tsi_config_gpio(priv);

	ret = da9052_tsi_config_state(priv, TSI_IDLE);
	ts->tsi_conf.state = TSI_IDLE;

	da9052_tsi_config_measure_seq(priv, TSI_MODE_VALUE);

	da9052_tsi_config_skip_slots(priv, TSI_SLOT_SKIP_VALUE);

	da9052_tsi_config_delay(priv, TSI_DELAY_VALUE);

	da9052_tsi_set_sampling_mode(priv, DEFAULT_TSI_SAMPLING_MODE);


	ret = da9052_tsi_create_input_dev(ts->input_devs, NUM_INPUT_DEVS);
	if (ret) {
		pr_debug("%s: da9052_tsi_create_input_dev Failed\n", __func__);
		return ret;
	}

	init_tsi_threads(priv);
	ret = da9052_tsi_config_state(priv, TSI_AUTO_MODE);
	if (ret) {
		for (cnt = 0; cnt < NUM_INPUT_DEVS; cnt++) {
			if (ts->input_devs[cnt] != NULL)
				input_free_device(ts->input_devs[cnt]);
		}
	}

	return 0;
}

static const unsigned char * const cfg_strs[] = {
		[DA9052_4_WIRE] = "4-wire",
		[DA9052_5_WIRE_XYSXY_IO1] = "XYSXY_IO1",
		[DA9052_5_WIRE_YXSXY_IO1] = "YXSXY_IO1",
		[DA9052_5_WIRE_XYSXY_IO2] = "XYSXY_IO2",
		[DA9052_5_WIRE_YXSXY_IO2] = "YXSXY_IO2",
		[DA9052_5_WIRE_ADC_IO1] = "ADC_IO1",
		[DA9052_5_WIRE_ADC_IO2] = "ADC_IO2",
};

static s32 __devinit da9052_tsi_probe(struct platform_device *pdev)
{

	struct da9052_ts_priv *priv;
	struct da9052_tsi_platform_data *pdata = pdev->dev.platform_data;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	tsi_fifo_init(priv);
	priv->tsi_reg.tsi_state = WAIT_FOR_PEN_DOWN;
	priv->drop_samples = drop_samples;
	priv->da9052 = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, priv);
	priv->tsi_pdata = pdata;
	priv->config_index = (ts_mode <= DA9052_5_WIRE_ADC_IO2) ?
			ts_mode : pdata->config_index;

	if (priv->config_index) {
		if (priv->config_index >= DA9052_5_WIRE_ADC_IO1) {
			priv->datardy_nb.eve_type = ADC_EOM_EVE;
			priv->pd_nb.eve_type = GPI2_EVE;
		} else {
			priv->datardy_nb.eve_type = TSI_READY_EVE;
			priv->pd_nb.eve_type = PEN_DOWN_EVE;
		}
		priv->datardy_nb.call_back = &da9052_tsi_5w_data_ready_handler;
	} else {
		priv->datardy_nb.eve_type = TSI_READY_EVE;
		priv->datardy_nb.call_back = &da9052_tsi_data_ready_handler;
		priv->pd_nb.eve_type = PEN_DOWN_EVE;
	}
	priv->pd_nb.call_back = &da9052_tsi_pen_down_handler;
	if (da9052_tsi_init_drv(priv, &pdev->dev))
			return -EFAULT;


	printk(KERN_INFO "TSI Drv Successfully Inserted %s, %s\n",
		DA9052_TSI_DEVICE_NAME, cfg_strs[priv->config_index]);
	return 0;
}

static int __devexit da9052_tsi_remove(struct platform_device *pdev)
{
	struct da9052_ts_priv *priv = platform_get_drvdata(pdev);
	struct da9052_tsi_info *ts = &priv->tsi_info;
	s32 ret = 0, i = 0;

	if (!IS_ERR(ts->ts_regulator)) {
		if (ts->ts_reg_en) {
			ts->ts_reg_en = 0;
			regulator_disable(ts->ts_regulator);
		}
		regulator_put(ts->ts_regulator);
		ts->ts_regulator = NULL;
	}

	ret = da9052_tsi_config_state(priv, TSI_IDLE);
	if (!ret)
		return -EINVAL;

	if (ts->pd_reg_status) {
		priv->da9052->unregister_event_notifier(priv->da9052,
							&priv->pd_nb);
		ts->pd_reg_status = RESET;
	}

	if (ts->datardy_reg_status) {
		priv->da9052->unregister_event_notifier(priv->da9052,
							&priv->datardy_nb);
		ts->datardy_reg_status = RESET;
	}

	del_timer(&ts->release_timer);
	tsi_fifo_destroy(priv);
	for (i = 0; i < NUM_INPUT_DEVS; i++) {
		input_unregister_device(ts->input_devs[i]);
	}

	platform_set_drvdata(pdev, NULL);
	kfree(priv);
	pr_debug("%s: Removing %s\n", __func__, DA9052_TSI_DEVICE_NAME);

	return 0;
}

static struct platform_driver da9052_tsi_driver = {
	.probe		= da9052_tsi_probe,
	.remove		= __devexit_p(da9052_tsi_remove),
	.suspend	= da9052_tsi_suspend,
	.resume		= da9052_tsi_resume,
	.driver		= {
				.name	= DA9052_TSI_DEVICE_NAME,
				.owner	= THIS_MODULE,
			},
};

static int __init da9052_tsi_init(void)
{
	printk("DA9052 TSI Device Driver, v1.0\n");
	return platform_driver_register(&da9052_tsi_driver);
}
module_init(da9052_tsi_init);

static void __exit da9052_tsi_exit(void)
{
	printk(KERN_ERR "TSI Driver %s Successfully Removed \n",
					DA9052_TSI_DEVICE_NAME);
	return;

}
module_exit(da9052_tsi_exit);

MODULE_DESCRIPTION("Touchscreen driver for Dialog Semiconductor DA9052");
MODULE_AUTHOR("Dialog Semiconductor Ltd <dchen@diasemi.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
