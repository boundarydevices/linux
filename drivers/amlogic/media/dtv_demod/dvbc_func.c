/*
 * drivers/amlogic/media/dtv_demod/dvbc_func.c
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

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"
#include <linux/kthread.h>

/*#include "dvbc_func.h"*/

MODULE_PARM_DESC(debug_amldvbc, "\n\t\t Enable frontend demod debug information");
static int debug_amldvbc;
module_param(debug_amldvbc, int, 0644);

/*#define dprintk(a ...) do { if (debug_amldvbc) printk(a); } while (0)*/

/*move to dvbc_old.c*/
/* static struct task_struct *cci_task; */
/*int cciflag = 0;*/
struct timer_list mytimer;


static void dvbc_cci_timer(unsigned long data)
{
#if 0
	int count;
	int maxCCI_p, re, im, j, i, times, maxCCI, sum, sum1, reg_0xf0, tmp1,
	    tmp, tmp2, reg_0xa8, reg_0xac;
	int reg_0xa8_t, reg_0xac_t;

	count = 100;
	if ((((dvbc_read_reg(QAM_BASE + 0x18)) & 0x1) == 1)) {
		PR_DVBC("[cci]lock ");
		if (cciflag == 0) {
			dvbc_write_reg(QAM_BASE + 0xa8, 0);

			cciflag = 0;
		}
		PR_DVBC("\n");
		mdelay(500);
		mod_timer(&mytimer, jiffies + 2 * HZ);
		return;
	}
	if (cciflag == 1) {
		PR_DVBC("[cci]cciflag is 1,wait 20\n");
		mdelay(20000);
	}
	times = 300;
	tmp = 0x2be2be3;
	/*0x2ae4772;  IF = 6M, fs = 35M, dec2hex(round(8*IF/fs*2^25)) */
	tmp2 = 0x2000;
	tmp1 = 8;
	reg_0xa8 = 0xc0000000;	/* bypass CCI */
	reg_0xac = 0xc0000000;	/* bypass CCI */

	maxCCI = 0;
	maxCCI_p = 0;
	for (i = 0; i < times; i++) {
		/*reg_0xa8 = app_apb_read_reg(0xa8); */
		reg_0xa8_t = reg_0xa8 + tmp + i * tmp2;
		dvbc_write_reg(QAM_BASE + 0xa8, reg_0xa8_t);
		reg_0xac_t = reg_0xac + tmp - i * tmp2;
		dvbc_write_reg(QAM_BASE + 0xac, reg_0xac_t);
		sum = 0;
		sum1 = 0;
		for (j = 0; j < tmp1; j++) {
			/* msleep(20); */
			/* mdelay(20); */
			reg_0xf0 = dvbc_read_reg(QAM_BASE + 0xf0);
			re = (reg_0xf0 >> 24) & 0xff;
			im = (reg_0xf0 >> 16) & 0xff;
			if (re > 127)
				/*re = re - 256; */
				re = 256 - re;
			if (im > 127)
				/*im = im - 256; */
				im = 256 - im;

			sum += re + im;
			re = (reg_0xf0 >> 8) & 0xff;
			im = (reg_0xf0 >> 0) & 0xff;
			if (re > 127)
				/*re = re - 256; */
				re = 256 - re;
			if (im > 127)
				/*im = im - 256; */
				im = 256 - im;

			sum1 += re + im;
		}
		sum = sum / tmp1;
		sum1 = sum1 / tmp1;
		if (sum1 > sum) {
			sum = sum1;
			reg_0xa8_t = reg_0xac_t;
		}
		if (sum > maxCCI) {
			maxCCI = sum;
			if (maxCCI > 24)
				maxCCI_p = reg_0xa8_t & 0x7fffffff;
		}
		if ((sum < 24) && (maxCCI_p > 0))
			break;	/* stop CCI detect. */
	}

	if (maxCCI_p > 0) {
		dvbc_write_reg(QAM_BASE + 0xa8, maxCCI_p & 0x7fffffff);
		/* enable CCI */
		dvbc_write_reg(QAM_BASE + 0xac, maxCCI_p & 0x7fffffff);
		/* enable CCI */
		/*     if(dvbc.mode == 4) // 256QAM */
		dvbc_write_reg(QAM_BASE + 0x54, 0xa25705fa);
		/**/ cciflag = 1;
		mdelay(1000);
	} else {
		PR_DVBC
		    ("[cci] ------------  find NO CCI -------------------\n");
		cciflag = 0;
	}

	PR_DVBC("[cci][%s]--------------------------\n", __func__);
	mod_timer(&mytimer, jiffies + 2 * HZ);
	return;
/*      }*/
#endif
}

int dvbc_timer_init(void)
{
	PR_DVBC("%s\n", __func__);
	setup_timer(&mytimer, dvbc_cci_timer, (unsigned long)"Hello, world!");
	mytimer.expires = jiffies + 2 * HZ;
	add_timer(&mytimer);
	return 0;
}

void dvbc_timer_exit(void)
{
	PR_DVBC("%s\n", __func__);
	del_timer(&mytimer);
}
#if 0	/*ary*/
int dvbc_cci_task(void *data)
{
	int count;
	int maxCCI_p, re, im, j, i, times, maxCCI, sum, sum1, reg_0xf0, tmp1,
	    tmp, tmp2, reg_0xa8, reg_0xac;
	int reg_0xa8_t, reg_0xac_t;

	count = 100;
	while (1) {
		msleep(200);
		if ((((dvbc_read_reg(QAM_BASE + 0x18)) & 0x1) == 1)) {
			PR_DVBC("[cci]lock ");
			if (cciflag == 0) {
				dvbc_write_reg(QAM_BASE + 0xa8, 0);
				dvbc_write_reg(QAM_BASE + 0xac, 0);
				PR_DVBC("no cci ");
				cciflag = 0;
			}
			PR_DVBC("\n");
			msleep(500);
			continue;
		}

		if (cciflag == 1) {
			PR_DVBC("[cci]cciflag is 1,wait 20\n");
			msleep(20000);
		}
		times = 300;
		tmp = 0x2be2be3;
		/*0x2ae4772; IF = 6M,fs = 35M, dec2hex(round(8*IF/fs*2^25)) */
		tmp2 = 0x2000;
		tmp1 = 8;
		reg_0xa8 = 0xc0000000;	/* bypass CCI */
		reg_0xac = 0xc0000000;	/* bypass CCI */

		maxCCI = 0;
		maxCCI_p = 0;
		for (i = 0; i < times; i++) {
			/*reg_0xa8 = app_apb_read_reg(0xa8); */
			reg_0xa8_t = reg_0xa8 + tmp + i * tmp2;
			dvbc_write_reg(QAM_BASE + 0xa8, reg_0xa8_t);
			reg_0xac_t = reg_0xac + tmp - i * tmp2;
			dvbc_write_reg(QAM_BASE + 0xac, reg_0xac_t);
			sum = 0;
			sum1 = 0;
			for (j = 0; j < tmp1; j++) {
				/*         msleep(1); */
				reg_0xf0 = dvbc_read_reg(QAM_BASE + 0xf0);
				re = (reg_0xf0 >> 24) & 0xff;
				im = (reg_0xf0 >> 16) & 0xff;
				if (re > 127)
					/*re = re - 256; */
					re = 256 - re;
				if (im > 127)
					/*im = im - 256; */
					im = 256 - im;

				sum += re + im;

				re = (reg_0xf0 >> 8) & 0xff;
				im = (reg_0xf0 >> 0) & 0xff;
				if (re > 127)
					/*re = re - 256; */
					re = 256 - re;
				if (im > 127)
					/*im = im - 256; */
					im = 256 - im;

				sum1 += re + im;
			}
			sum = sum / tmp1;
			sum1 = sum1 / tmp1;
			if (sum1 > sum) {
				sum = sum1;
				reg_0xa8_t = reg_0xac_t;
			}
			if (sum > maxCCI) {
				maxCCI = sum;
				if (maxCCI > 24)
					maxCCI_p = reg_0xa8_t & 0x7fffffff;
			}

			if ((sum < 24) && (maxCCI_p > 0))
				break;	/* stop CCI detect. */
		}

		if (maxCCI_p > 0) {
			dvbc_write_reg(QAM_BASE + 0xa8, maxCCI_p & 0x7fffffff);
			/* enable CCI */
			dvbc_write_reg(QAM_BASE + 0xac, maxCCI_p & 0x7fffffff);
			/* enable CCI */
			/*     if(dvbc.mode == 4) // 256QAM */
			dvbc_write_reg(QAM_BASE + 0x54, 0xa25705fa);
			/**/ cciflag = 1;
			msleep(1000);
		} else {
			cciflag = 0;
		}

		PR_DVBC("[cci][%s]--------------------------\n", __func__);
	}
	return 0;
}

int dvbc_get_cci_task(void)
{
	if (cci_task)
		return 0;
	else
		return 1;
}

void dvbc_create_cci_task(void)
{
	int ret;

	/*dvbc_write_reg(QAM_BASE+0xa8, 0x42b2ebe3); // enable CCI */
	/* dvbc_write_reg(QAM_BASE+0xac, 0x42b2ebe3); // enable CCI */
/*     if(dvbc.mode == 4) // 256QAM*/
	/* dvbc_write_reg(QAM_BASE+0x54, 0xa25705fa); // */
	ret = 0;
	cci_task = kthread_create(dvbc_cci_task, NULL, "cci_task");
	if (ret != 0) {
		PR_DVBC("[%s]Create cci kthread error!\n", __func__);
		cci_task = NULL;
		return;
	}
	wake_up_process(cci_task);
	PR_DVBC("[%s]Create cci kthread and wake up!\n", __func__);
}

void dvbc_kill_cci_task(void)
{
	if (cci_task) {
		kthread_stop(cci_task);
		cci_task = NULL;
		PR_DVBC("[%s]kill cci kthread !\n", __func__);
	}
}
#endif


int dvbc_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_dvbc *demod_dvbc)
{
	int ret = 0;
	u16 symb_rate;
	u8 mode;
	u32 ch_freq;

	PR_DVBC("f=%d, s=%d, q=%d\n",
		demod_dvbc->ch_freq, demod_dvbc->symb_rate, demod_dvbc->mode);
/*ary no use	demod_i2c->tuner = 7;*/
	mode = demod_dvbc->mode;
	symb_rate = demod_dvbc->symb_rate;
	ch_freq = demod_dvbc->ch_freq;
	if (mode > 4) {
		PR_DVBC("Error: Invalid QAM mode option %d\n", mode);
		mode = 4;
		ret = -1;
	}

	if (symb_rate < 1000 || symb_rate > 7000) {
		PR_DVBC("Error: Invalid Symbol Rate option %d\n", symb_rate);
		symb_rate = 5361;
		ret = -1;
	}

	if (ch_freq < 1000 || ch_freq > 900000) {
		PR_DVBC("Error: Invalid Channel Freq option %d\n", ch_freq);
		ch_freq = 474000;
		ret = -1;
	}
	/* if (ret != 0) return ret; */
	demod_sta->dvb_mode = 0;
	demod_sta->ch_mode = mode;
	/* 0:16, 1:32, 2:64, 3:128, 4:256 */
	demod_sta->agc_mode = 1;
	/* 0:NULL, 1:IF, 2:RF, 3:both */
	demod_sta->ch_freq = ch_freq;
	/*ary no use demod_sta->tuner = demod_i2c->tuner;*/
#if 0	/*ary no use*/
	if (demod_i2c->tuner == 1)
		demod_sta->ch_if = 36130;	/* TODO  DCT tuner */
	else if (demod_i2c->tuner == 2)
		demod_sta->ch_if = 4570;	/* TODO  Maxlinear tuner */
	else if (demod_i2c->tuner == 7)
		/*   demod_sta->ch_if     = 5000; // TODO  Si2176 tuner */
#endif
	demod_sta->ch_bw = 8000;	/* TODO */
	if (demod_sta->ch_if == 0)
		demod_sta->ch_if = 5000;
	demod_sta->symb_rate = symb_rate;
	if ((!is_ic_ver(IC_VER_TL1)) && is_dvbc_ver(IC_DVBC_V3))
		demod_sta->adc_freq = demod_dvbc->dat0;

#if 0
	if (is_meson_txlx_cpu() || is_meson_gxlx_cpu())
		dvbc_reg_initial(demod_sta);
	else
		dvbc_reg_initial_old(demod_sta);
#endif
	if (is_dvbc_ver(IC_DVBC_V2))
		dvbc_reg_initial_old(demod_sta);
	else if (is_dvbc_ver(IC_DVBC_V3))
		dvbc_reg_initial(demod_sta);
	else
		PR_ERR("%s:not support %d\n", __func__, get_dvbc_ver());

	return ret;
}



