/*
 * drivers/amlogic/media/dtv_demod/dvbc_v3.c
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
#include <linux/delay.h>
#include <linux/dvb/aml_demod.h>
#include "demod_func.h"

/*#define dprintk(a ...)	aml_dbgdvbc_reg(a)*/


#if 0
void qam_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;

	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		demod_set_demod_reg(reg_data, TXLX_QAM_BASE + (reg_addr << 2));
	else if (is_meson_gxlx_cpu())
		demod_set_demod_reg(reg_data, GXLX_QAM_BASE + (reg_addr << 2));
	else
		demod_set_demod_reg(reg_data, ddemod_reg_base + QAM_BASE
						+ (reg_addr << 2));
}

unsigned long qam_read_reg(int reg_addr)
{
	if (!get_dtvpll_init_flag())
		return 0;
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu())
		return demod_read_demod_reg(TXLX_QAM_BASE + (reg_addr << 2));
	else if (is_meson_gxlx_cpu())
		return demod_read_demod_reg(GXLX_QAM_BASE + (reg_addr << 2));
	else
		return demod_read_demod_reg(ddemod_reg_base + QAM_BASE
						+ (reg_addr << 2));
}

#endif


u32 dvbc_get_status(void)
{
/*      PR_DVBC("c4 is %x\n",dvbc_read_reg(QAM_BASE+0xc4));*/
	return qam_read_reg(0x31) & 0xf;
}

static u32 dvbc_get_ch_power(void)
{
	u32 tmp;
	u32 ad_power;
	u32 agc_gain;
	u32 ch_power;

	tmp = qam_read_reg(0x27);

	ad_power = (tmp >> 22) & 0x1ff;
	agc_gain = (tmp >> 0) & 0x7ff;

	ad_power = ad_power >> 4;
	/* ch_power = lookuptable(agc_gain) + ad_power; TODO */
	ch_power = (ad_power & 0xffff) + ((agc_gain & 0xffff) << 16);

	return ch_power;
}

static u32 dvbc_get_snr(void)
{
	u32 tmp, snr;

	tmp = qam_read_reg(0x5) & 0xfff;
	snr = tmp * 100 / 32;	/* * 1e2 */

	return snr;
}

static u32 dvbc_get_ber(void)
{
	u32 rs_ber;
	u32 rs_packet_len;

	rs_packet_len = qam_read_reg(0x4) & 0xffff;
	rs_ber = qam_read_reg(0x5) >> 12 & 0xfffff;

	/* rs_ber = rs_ber / 204.0 / 8.0 / rs_packet_len; */
	if (rs_packet_len == 0)
		rs_ber = 1000000;
	else
		rs_ber = rs_ber * 613 / rs_packet_len;	/* 1e-6 */

	return rs_ber;
}

static u32 dvbc_get_per(void)
{
	u32 rs_per;
	u32 rs_packet_len;
	u32 acc_rs_per_times;

	rs_packet_len = qam_read_reg(0x4) & 0xffff;
	rs_per = qam_read_reg(0x6) >> 16 & 0xffff;

	acc_rs_per_times = qam_read_reg(0x33) & 0xffff;
	/*rs_per = rs_per / rs_packet_len; */

	if (rs_packet_len == 0)
		rs_per = 10000;
	else
		rs_per = 10000 * rs_per / rs_packet_len;	/* 1e-4 */

	/*return rs_per; */
	return acc_rs_per_times;
}

static u32 dvbc_get_symb_rate(void)
{
	u32 tmp;
	u32 adc_freq;
	u32 symb_rate;

	adc_freq = qam_read_reg(0xd) >> 16 & 0xffff;
	tmp = qam_read_reg(0x2e);

	if ((tmp >> 15) == 0)
		symb_rate = 0;
	else
		symb_rate = 10 * (adc_freq << 12) / (tmp >> 15);
	/* 1e4 */

	return symb_rate;
}

static int dvbc_get_freq_off(void)
{
	int tmp;
	int symb_rate;
	int freq_off;

	symb_rate = dvbc_get_symb_rate();
	tmp = qam_read_reg(0x38) & 0x3fffffff;
	if (tmp >> 29 & 1)
		tmp -= (1 << 30);

	freq_off = ((tmp >> 16) * 25 * (symb_rate >> 10)) >> 3;

	return freq_off;
}



void qam_auto_scan(int auto_qam_enable)
{
	if (auto_qam_enable) {
		qam_write_reg(0xc, 0x235cf459);
		/*qam_write_reg(0xe, 0x2d82d);*/
		qam_write_reg(0xe, 0x400);/*reduce range to 0.05m*/
		qam_write_reg(0x4e, 0x12010012);
	} else
		qam_write_reg(0x4e, 0x12000012);

}
static unsigned int get_adc_freq(void)
{
	return 24000;
}

void dvbc_reg_initial(struct aml_demod_sta *demod_sta)
{
	u32 clk_freq;
	u32 adc_freq;
	/*ary no use u8 tuner;*/
	u8 ch_mode;
	u8 agc_mode;
	u32 ch_freq;
	u16 ch_if;
	u16 ch_bw;
	u16 symb_rate;
	u32 phs_cfg;
	int afifo_ctr;
	int max_frq_off, tmp, adc_format;

	clk_freq = demod_sta->clk_freq;	/* kHz */
	/*no use adc_freq = demod_sta->adc_freq;*/	/* kHz */
	adc_freq  = get_adc_freq();/*24000*/;
	adc_format = 1;
	/*ary no use tuner = demod_sta->tuner;*/
	ch_mode = demod_sta->ch_mode;
	agc_mode = demod_sta->agc_mode;
	ch_freq = demod_sta->ch_freq;	/* kHz */
	ch_if = demod_sta->ch_if;	/* kHz */
	ch_bw = demod_sta->ch_bw;	/* kHz */
	symb_rate = demod_sta->symb_rate;	/* k/sec */
	PR_DVBC("ch_if is %d,  %d,  %d,  %d, %d %d\n",
		ch_if, ch_mode, ch_freq, ch_bw, symb_rate, adc_freq);
/*    ch_mode=4;*/
	/* disable irq */
	qam_write_reg(0x34, 0);

	/* reset */
	/*dvbc_reset(); */
	qam_write_reg(0x7, qam_read_reg(0x7) & ~(1 << 4));
	/* disable fsm_en */
	qam_write_reg(0x7, qam_read_reg(0x7) & ~(1 << 0));
	/* Sw disable demod */
	qam_write_reg(0x7, qam_read_reg(0x7) | (1 << 0));
	/* Sw enable demod */
	qam_write_reg(0x0, 0x0);
	/* QAM_STATUS */
	qam_write_reg(0x7, 0x00000f00);
	/* QAM_GCTL0 */
	qam_write_reg(0x2, (qam_read_reg(0x2) & ~7) | (ch_mode & 7));
	/* qam mode */

	switch (ch_mode) {
	case 0: /*16qam*/
		qam_write_reg(0x71, 0x000a2200);

		if (is_ic_ver(IC_VER_TL1))
			qam_write_reg(0x72, 0xc2b0c49);
		else
			qam_write_reg(0x72, 0x0c2b04a9);

		qam_write_reg(0x73, 0x02020000);
		qam_write_reg(0x75, 0x000e9178);
		qam_write_reg(0x76, 0x0001c100);
		qam_write_reg(0x7a, 0x002ab7ff);
		qam_write_reg(0x93, 0x641a180c);
		qam_write_reg(0x94, 0x0c141400);
		break;
	case 1:/*32qam*/
		qam_write_reg(0x71, 0x00061200);
		qam_write_reg(0x72, 0x099301ae);
		qam_write_reg(0x73, 0x08080000);
		qam_write_reg(0x75, 0x000bf10c);
		qam_write_reg(0x76, 0x0000a05c);
		qam_write_reg(0x77, 0x001000d6);
		qam_write_reg(0x7a, 0x0019a7ff);
		qam_write_reg(0x7c, 0x00111222);

		if (is_ic_ver(IC_VER_TL1))
			qam_write_reg(0x7d, 0x2020305);
		else
			qam_write_reg(0x7d, 0x05050505);

		qam_write_reg(0x7e, 0x03000d0d);
		qam_write_reg(0x93, 0x641f1d0c);
		qam_write_reg(0x94, 0x0c1a1a00);
		break;
	case 2:/*64qam*/
		break;
	case 3:/*128qam*/
		qam_write_reg(0x71, 0x0002c200);
		qam_write_reg(0x72, 0x0a6e0059);
		qam_write_reg(0x73, 0x08080000);
		qam_write_reg(0x75, 0x000a70e9);
		qam_write_reg(0x76, 0x00002013);
		qam_write_reg(0x77, 0x00035068);
		qam_write_reg(0x78, 0x000ab100);

		if (is_ic_ver(IC_VER_TL1))
			qam_write_reg(0x7a, 0xba7ff);
		else
			qam_write_reg(0x7a, 0x002ba7ff);

		qam_write_reg(0x7c, 0x00111222);

		if (is_ic_ver(IC_VER_TL1))
			qam_write_reg(0x7d, 0x2020305);
		else
			qam_write_reg(0x7d, 0x05050505);

		qam_write_reg(0x7e, 0x03000d0d);
		qam_write_reg(0x93, 0x642a240c);
		qam_write_reg(0x94, 0x0c262600);
		break;
	case 4:
		if (is_ic_ver(IC_VER_TL1)) {
			qam_write_reg(0x9c, 0x2a232100);
			qam_write_reg(0x57, 0x606040d);
		}
		break;
	}
	/*dvbc_write_reg(QAM_BASE+0x00c, 0xfffffffe);*/
	/* // adc_cnt, symb_cnt*/
	qam_write_reg(0x3, 0xffff8ffe);
	/* adc_cnt, symb_cnt    by raymond 20121213 */
	if (clk_freq == 0)
		afifo_ctr = 0;
	else
		afifo_ctr = (adc_freq * 256 / clk_freq) + 2;
	if (afifo_ctr > 255)
		afifo_ctr = 255;
	qam_write_reg(0x4, (afifo_ctr << 16) | 8000);
	/* afifo, rs_cnt_cfg */

	/*dvbc_write_reg(QAM_BASE+0x020, 0x21353e54);*/
	 /* // PHS_reset & TIM_CTRO_ACCURATE  sw_tim_select=0*/
	/*dvbc_write_reg(QAM_BASE+0x020, 0x21b53e54);*/
	 /* //modified by qiancheng*/
	qam_write_reg(0x8, 0x61b53e54);
	/*modified by qiancheng by raymond 20121208  0x63b53e54 for cci */
	/*  dvbc_write_reg(QAM_BASE+0x020, 0x6192bfe2);*/
	/* //modifed by ligg 20130613 auto symb_rate scan*/
	if (adc_freq == 0)
		phs_cfg = 0;
	else
		phs_cfg = (1 << 31) / adc_freq * ch_if / (1 << 8);
	/*  8*fo/fs*2^20 fo=36.125, fs = 28.57114, = 21d775 */
	/* PR_DVBC("phs_cfg = %x\n", phs_cfg); */
	qam_write_reg(0x9, 0x4c000000 | (phs_cfg & 0x7fffff));
	/* PHS_OFFSET, IF offset, */

	if (adc_freq == 0) {
		max_frq_off = 0;
	} else {
		max_frq_off = (1 << 29) / symb_rate;
		/* max_frq_off = (400KHz * 2^29) /  */
		/*   (AD=28571 * symbol_rate=6875) */
		tmp = 40000000 / adc_freq;
		max_frq_off = tmp * max_frq_off;
	}
	PR_DVBC("max_frq_off is %x,\n", max_frq_off);

	if (is_ic_ver(IC_VER_TL1))
		qam_write_reg(0xc, 0x245cf450); //MODIFIED BY QIANCHENG
	else
		qam_write_reg(0xb, max_frq_off & 0x3fffffff);
	/* max frequency offset, by raymond 20121208 */

	/*dvbc_write_reg(QAM_BASE+0x030, 0x011bf400);*/
	/* // TIM_CTL0 start speed is 0,  when know symbol rate*/
	/*rsj//qam_write_reg(0xc, 0x235cf451);*/
	/*MODIFIED BY QIANCHENG */
/*      dvbc_write_reg(QAM_BASE+0x030, 0x245bf451);*/
 /* //modified by ligg 20130613 --auto symb_rate scan*/
	qam_write_reg(0xd,
	((adc_freq & 0xffff) << 16) | (symb_rate & 0xffff));
	/*dvbc_write_reg(QAM_BASE + (0xE << 2), 0x00400000);*/
	/* TIM_SWEEP_RANGE 16000 */

/************* hw state machine config **********/
	/*dvbc_write_reg(QAM_BASE + (0x10 << 2), 0x003c);*/
/* configure symbol rate step step 0*/

	/* modified 0x44 0x48 */
	qam_write_reg(0x11, (symb_rate & 0xffff) * 256);
	/* blind search, configure max symbol_rate      for 7218  fb=3.6M */
	/*dvbc_write_reg(QAM_BASE+0x048, 3600*256);*/
	/* // configure min symbol_rate fb = 6.95M*/
	qam_write_reg(0x12, (qam_read_reg(0x12) & ~(0xff<<8)) | 3400 * 256);

	if (is_ic_ver(IC_VER_TL1))
		qam_write_reg(0x51, (qam_read_reg(0x51)&~(0x1<<28)));
	/* configure min symbol_rate fb = 6.95M */

	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff6f); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xfffffd68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffffff68); // threshold */
	/*dvbc_write_reg(QAM_BASE+0x0c0, 0xffff2f67);*/
	 /* // threshold for skyworth*/
	/* dvbc_write_reg(QAM_BASE+0x0c0, 0x061f2f67); // by raymond 20121208 */
	/* dvbc_write_reg(QAM_BASE+0x0c0, 0x061f2f66);*/
	 /* // by raymond 20121213, remove it to every constellation*/
/************* hw state machine config **********/
	/* agc control */
	/* dvbc_write_reg(QAM_BASE+0x094, 0x7f800d2c);// AGC_CTRL  ALPS tuner */
	/* dvbc_write_reg(QAM_BASE+0x094, 0x7f80292c);     // Pilips Tuner */
	if ((agc_mode & 1) == 0)
		/* freeze if agc */
		qam_write_reg(0x25,
		qam_read_reg(0x25) | (0x1 << 10));
	if ((agc_mode & 2) == 0) {
		/* IF control */
		/*freeze rf agc */
		qam_write_reg(0x25,
		qam_read_reg(0x25) | (0x1 << 13));
	}
	/*Maxlinear Tuner */
	/*dvbc_write_reg(QAM_BASE+0x094, 0x7f80292d); */
	/*dvbc_write_reg(QAM_BASE + 0x098, 0x9fcc8190);*/
	/* AGC_IFGAIN_CTRL */
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e028c00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e03cc00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e028700);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800 now*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0e03cd00);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800*/
	/*dvbc_write_reg(QAM_BASE+0x0a0, 0x0603cd11);*/
	/* // AGC_RFGAIN_CTRL 0x0e020800 by raymond,*/
	/* if Adjcent channel test, maybe it need change.*/
	/*20121208 ad invert*/
	/*rsj//qam_write_reg(0x28, 0x0603cd10);*/
	if (!is_ic_ver(IC_VER_TL1))
		qam_write_reg(0x28,
		qam_read_reg(0x28) | (adc_format << 27));
	/* AGC_RFGAIN_CTRL 0x0e020800 by raymond,*/
	/* if Adjcent channel test, maybe it need change.*/
	/* 20121208 ad invert,20130221, suit for two path channel.*/

	qam_write_reg(0x7, qam_read_reg(0x7) | 0x33);
	/* IMQ, QAM Enable */

	/* start hardware machine */
	/*dvbc_sw_reset(0x004, 4); */
	qam_write_reg(0x7, qam_read_reg(0x7) | (1 << 4));
	qam_write_reg(0x3a, qam_read_reg(0x3a) | (1 << 2));

	/* clear irq status */
	qam_read_reg(0x35);

	/* enable irq */
	qam_write_reg(0x34, 0x7fff << 3);
#if 1
	/*if (is_meson_txlx_cpu()) {*/
	if (is_ic_ver(IC_VER_TXLX)) {
		/*my_tool setting j83b mode*/
		qam_write_reg(0x7, 0x10f33);
		/*j83b filter para*/
		qam_write_reg(0x40, 0x3f010201);
		qam_write_reg(0x41, 0x0a003a3b);
		qam_write_reg(0x42, 0xe1ee030e);
		qam_write_reg(0x43, 0x002601f2);
		qam_write_reg(0x44, 0x009b006b);
		qam_write_reg(0x45, 0xb3a1905);
		qam_write_reg(0x46, 0x1c396e07);
		qam_write_reg(0x47, 0x3801cc08);
		qam_write_reg(0x48, 0x10800a2);
		qam_write_reg(0x12, 0x50e1000);
		qam_write_reg(0x30, 0x41f2f69);
		/*j83b_symbolrate(please see register doc)*/
		qam_write_reg(0x4d, 0x23d125f7);
		/*for phase noise case 256qam*/
		qam_write_reg(0x9c, 0x2a232100);
		qam_write_reg(0x57, 0x606040d);
		/*for phase noise case 64qam*/
		qam_write_reg(0x54, 0x606050d);
		qam_write_reg(0x52, 0x346dc);
		qam_auto_scan(1);
	}
#endif
	if (!is_ic_ver(IC_VER_TL1)) {
		qam_write_reg(0x7, 0x10f23);
		qam_write_reg(0x3a, 0x0);
		qam_write_reg(0x7, 0x10f33);
		qam_write_reg(0x3a, 0x4);
	}
/*auto track*/
	/*      dvbc_set_auto_symtrack(); */
}

u32 dvbc_set_auto_symtrack(void)
{
	if (is_ic_ver(IC_VER_TL1))
		return 0;

	qam_write_reg(0xc, 0x245bf45c);	/*open track */
	qam_write_reg(0x8, 0x61b2bf5c);
	qam_write_reg(0x11, (7000 & 0xffff) * 256);
	qam_write_reg(0xe, 0x00220000);
	qam_write_reg(0x7, qam_read_reg(0x7) & ~(1 << 0));
	/* Sw disable demod */
	qam_write_reg(0x7, qam_read_reg(0x7) | (1 << 0));
	/* Sw enable demod */
	return 0;
}



int dvbc_status(struct aml_demod_sta *demod_sta,
		struct aml_demod_sts *demod_sts)
{
	int ftmp, tmp;

	struct dvb_frontend *fe = aml_get_fe();

	demod_sts->ch_sts = qam_read_reg(0x6);
	demod_sts->ch_pow = dvbc_get_ch_power();
	demod_sts->ch_snr = dvbc_get_snr();
	demod_sts->ch_ber = dvbc_get_ber();
	demod_sts->ch_per = dvbc_get_per();
	demod_sts->symb_rate = dvbc_get_symb_rate();
	demod_sts->freq_off = dvbc_get_freq_off();
	/*demod_sts->dat0 = dvbc_read_reg(QAM_BASE+0x28); */
/*    demod_sts->dat0 = tuner_get_ch_power(demod_i2c);*/
	demod_sts->dat1 = tuner_get_ch_power(fe);
#if 1

	ftmp = demod_sts->ch_sts;
	PR_DVBC("[dvbc debug] ch_sts is %x\n", ftmp);
	ftmp = demod_sts->ch_snr;
	ftmp /= 100;
	PR_DVBC("snr %d dB ", ftmp);
	ftmp = demod_sts->ch_ber;
	PR_DVBC("ber %.d ", ftmp);
	tmp = demod_sts->ch_per;
	PR_DVBC("per %d ", tmp);
	ftmp = demod_sts->symb_rate;
	PR_DVBC("srate %.d ", ftmp);
	ftmp = demod_sts->freq_off;
	PR_DVBC("freqoff %.d kHz ", ftmp);
	tmp = demod_sts->dat1;
	/*change %lu to %u*/
	PR_DVBC("strength %ddb  0xe0 status is %u ,b4 status is %u", tmp,
		(qam_read_reg(0x38) & 0xffff),
		(qam_read_reg(0x2d) & 0xffff));
	/*change %lu to %u*/
	PR_DVBC("dagc_gain is %u ", qam_read_reg(0x29) & 0x7f);
	tmp = demod_sts->ch_pow;
	PR_DVBC("power is %ddb\n", (tmp & 0xffff));

#endif

	return 0;
}

void dvbc_enable_irq(int dvbc_irq)
{
	u32 mask;

	/* clear status */
	qam_read_reg(0x35);
	/* enable irq */
	mask = qam_read_reg(0x34);
	mask |= (1 << dvbc_irq);
	qam_write_reg(0x34, mask);
}


void dvbc_init_reg_ext(void)
{
	/*ary move from amlfrontend.c */
	qam_write_reg(0x7, 0xf33);

	if (is_ic_ver(IC_VER_TL1)) {
		qam_write_reg(0x12, 0x50e1000);
		qam_write_reg(0x30, 0x41f2f69);
	}
}

u32 dvbc_get_ch_sts(void)
{
	return qam_read_reg(0x6);
}
u32 dvbc_get_qam_mode(void)
{
	return qam_read_reg(0x2) & 7;
}
