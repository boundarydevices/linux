/*
 * drivers/amlogic/media/dtv_demod/dvbt_v2.c
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

 /* Enables DVBv3 compatibility bits at the headers */
#define __DVB_CORE__	/*ary 2018-1-31*/

#include "demod_func.h"
#include <linux/dvb/aml_demod.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "acf_filter_coefficient.h"
#include <linux/mutex.h>

#if 0
#ifdef pr_dbg
undef pr_dbg
#endif

#define pr_dbg		aml_dbgdvbt

#endif

void apb_write_regb(unsigned long addr, int index, unsigned long data)
{
	/*to achieve write func*/
}
void dvbt_write_regb(unsigned long addr, int index, unsigned long data)
{
	/*to achieve write func*/
}



int dvbt_set_ch(struct aml_demod_sta *demod_sta,
		/*struct aml_demod_i2c *demod_i2c,*/
		struct aml_demod_dvbt *demod_dvbt)
{
	int ret = 0;
	u8_t demod_mode = 1;
	u8_t bw, sr, ifreq, agc_mode;
	u32_t ch_freq;

	bw = demod_dvbt->bw;
	sr = demod_dvbt->sr;
	ifreq = demod_dvbt->ifreq;
	agc_mode = demod_dvbt->agc_mode;
	ch_freq = demod_dvbt->ch_freq;
	demod_mode = demod_dvbt->dat0;
	if (ch_freq < 1000 || ch_freq > 900000000) {
		/* PR_DVBT("Error: Invalid Channel Freq option %d\n",*/
		/* ch_freq); */
		ch_freq = 474000;
		ret = -1;
	}

	/*if (demod_mode < 0 || demod_mode > 4) {*/
	if (demod_mode > 4) {
		/* PR_DVBT("Error: Invalid demod mode option %d\n",*/
		/* demod_mode); */
		demod_mode = 1;
		ret = -1;
	}

	/* demod_sta->dvb_mode  = 1; */
	demod_sta->ch_mode = 0;	/* TODO */
	demod_sta->agc_mode = agc_mode;
	demod_sta->ch_freq = ch_freq;
	demod_sta->dvb_mode = demod_mode;
	/*   if (demod_i2c->tuner == 1) */
	/*     demod_sta->ch_if = 36130;*/
	/* else if (demod_i2c->tuner == 2)*/
	/*     demod_sta->ch_if = 4570;*/
	/* else if (demod_i2c->tuner == 3)*/
	/*     demod_sta->ch_if = 4000;// It is nouse.(alan)*/
	/* else if (demod_i2c->tuner == 7)*/
	/*     demod_sta->ch_if = 5000;//silab 5000kHz IF*/

	demod_sta->ch_bw = (8 - bw) * 1000;
	demod_sta->symb_rate = 0;	/* TODO */

/* bw=0; */
	demod_mode = 1;
	/* for si2176 IF:5M   sr 28.57 */
	sr = 4;
	ifreq = 4;
	/*bw = convert_bandwidth(bw);*/
	PR_INFO("%s:1:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);

	/*bw = BANDWIDTH_AUTO;*/
	if (bw == BANDWIDTH_AUTO)
		demod_mode = 2;
#if 0
	PR_INFO("%s:2:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);
	bw = BANDWIDTH_8_MHZ;
	demod_mode = 1;

	PR_INFO("%s:3:bw=%d, demod_mode=%d\n", __func__, bw, demod_mode);
#endif
	ofdm_initial(bw,
			/* 00:8M 01:7M 10:6M 11:5M */
		     sr,
		     /* 00:45M 01:20.8333M 10:20.7M 11:28.57  100:24m */
		     ifreq,
		     /* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		     demod_mode - 1,
		     /* 00:DVBT,01:ISDBT */
		     1
		     /* 0: Unsigned, 1:TC */
	    );
	PR_DVBT("DVBT/ISDBT mode\n");

	return ret;
}

void ini_icfo_pn_index(int mode)
{				/* 00:DVBT,01:ISDBT */
	if (mode == 0) {
		dvbt_write_reg(0x3f8, 0x00000031);
		dvbt_write_reg(0x3fc, 0x00030000);
		dvbt_write_reg(0x3f8, 0x00000032);
		dvbt_write_reg(0x3fc, 0x00057036);
		dvbt_write_reg(0x3f8, 0x00000033);
		dvbt_write_reg(0x3fc, 0x0009c08d);
		dvbt_write_reg(0x3f8, 0x00000034);
		dvbt_write_reg(0x3fc, 0x000c90c0);
		dvbt_write_reg(0x3f8, 0x00000035);
		dvbt_write_reg(0x3fc, 0x001170ff);
		dvbt_write_reg(0x3f8, 0x00000036);
		dvbt_write_reg(0x3fc, 0x0014d11a);
	} else if (mode == 1) {
		dvbt_write_reg(0x3f8, 0x00000031);
		dvbt_write_reg(0x3fc, 0x00085046);
		dvbt_write_reg(0x3f8, 0x00000032);
		dvbt_write_reg(0x3fc, 0x0019a0e9);
		dvbt_write_reg(0x3f8, 0x00000033);
		dvbt_write_reg(0x3fc, 0x0024b1dc);
		dvbt_write_reg(0x3f8, 0x00000034);
		dvbt_write_reg(0x3fc, 0x003b3313);
		dvbt_write_reg(0x3f8, 0x00000035);
		dvbt_write_reg(0x3fc, 0x0048d409);
		dvbt_write_reg(0x3f8, 0x00000036);
		dvbt_write_reg(0x3fc, 0x00527509);
	}
}

static int coef[] = {
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0x3200, 0x1100, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0xfe00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0100, 0x0200, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x3900, 0x1300, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0xfc00, 0xf300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0x0300, 0x0900, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x2200, 0x0c00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0xfd00, 0xf700, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf900, 0xfe00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0x2100, 0x2100, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0xf900, 0x0000, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x2600, 0x2600, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0xee00, 0x0000, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0x0c00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x1700, 0x1700, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0xf400, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfb00, 0xfb00, 0x0000, 0x0000,
	0x0e00, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0400, 0x0400, 0x0000, 0x0000, 0x1100, 0x3200, 0x0000, 0x0000,
	0x0900, 0x0000, 0x0000, 0x0000, 0xfb00, 0xfe00, 0x0000, 0x0000,
	0x0700, 0x0700, 0x0000, 0x0000, 0xf600, 0x0000, 0x0000, 0x0000,
	0x0200, 0x0100, 0x0000, 0x0000, 0xfa00, 0xfa00, 0x0000, 0x0000,
	0x1300, 0x3900, 0x0000, 0x0000, 0x2e00, 0x0000, 0x0000, 0x0000,
	0xf300, 0xfc00, 0x0000, 0x0000, 0x0600, 0x0600, 0x0000, 0x0000,
	0x0900, 0x0300, 0x0000, 0x0000, 0xf700, 0xf700, 0x0000, 0x0000,
	0x0c00, 0x2200, 0x0000, 0x0000, 0x4c00, 0x0000, 0x0000, 0x0000,
	0xf700, 0xfd00, 0x0000, 0x0000, 0x0100, 0x0100, 0x0000, 0x0000,
	0xfe00, 0xf900, 0x0000, 0x0000, 0x0b00, 0x0400, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0200, 0x0700, 0x0000, 0x0000,
	0x4200, 0x0000, 0x0000, 0x0000, 0x0700, 0x0200, 0x0000, 0x0000,
	0xfc00, 0xfc00, 0x0000, 0x0000, 0x0400, 0x0b00, 0x0000, 0x0000
};


void tfd_filter_coff_ini(void)
{
	int i = 0;

	for (i = 0; i < 336; i++) {
		dvbt_write_reg(0x99 * 4, (i << 16) | coef[i]);
		dvbt_write_reg(0x03 * 4, (1 << 12));
	}
}

void ofdm_initial(int bandwidth,
		/* 00:8M 01:7M 10:6M 11:5M */
		int samplerate,
		/* 00:45M 01:20.8333M 10:20.7M 11:28.57 100: 24.00 */
		int IF,
		/* 000:36.13M 001:-5.5M 010:4.57M 011:4M 100:5M */
		int mode,
		/* 00:DVBT,01:ISDBT */
		int tc_mode
		/* 0: Unsigned, 1:TC */
		)
{
#if 1
	int tmp;
	int ch_if;
	int adc_freq;
	/*int memstart;*/
	PR_DVBT
	    ("[ofdm_initial]bandwidth is %d,samplerate is %d",
	     bandwidth, samplerate);
	PR_DVBT
	    ("IF is %d, mode is %d,tc_mode is %d\n",
	    IF, mode, tc_mode);
	switch (IF) {
	case 0:
		ch_if = 36130;
		break;
	case 1:
		ch_if = -5500;
		break;
	case 2:
		ch_if = 4570;
		break;
	case 3:
		ch_if = 4000;
		break;
	case 4:
		ch_if = 5000;
		break;
	default:
		ch_if = 4000;
		break;
	}
	switch (samplerate) {
	case 0:
		adc_freq = 45000;
		break;
	case 1:
		adc_freq = 20833;
		break;
	case 2:
		adc_freq = 20700;
		break;
	case 3:
		adc_freq = 28571;
		break;
	case 4:
		adc_freq = 24000;
		break;
	case 5:
		adc_freq = 25000;
		break;
	default:
		adc_freq = 28571;
		break;
	}

	dvbt_write_reg((0x02 << 2), 0x00800000);
	/* SW reset bit[23] ; write anything to zero */
	dvbt_write_reg((0x00 << 2), 0x00000000);

	dvbt_write_reg((0xe << 2), 0xffff);
	/* enable interrupt */

	if (mode == 0) {	/* DVBT */
		switch (samplerate) {
		case 0:
			dvbt_write_reg((0x08 << 2), 0x00005a00);
			break;	/* 45MHz */
		case 1:
			dvbt_write_reg((0x08 << 2), 0x000029aa);
			break;	/* 20.833 */
		case 2:
			dvbt_write_reg((0x08 << 2), 0x00002966);
			break;	/* 20.7   SAMPLERATE*512 */
		case 3:
			dvbt_write_reg((0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		case 4:
			dvbt_write_reg((0x08 << 2), 0x00003000);
			break;	/* 24 */
		case 5:
			dvbt_write_reg((0x08 << 2), 0x00003200);
			break;	/* 25 */
		default:
			dvbt_write_reg((0x08 << 2), 0x00003924);
			break;	/* 28.571 */
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			dvbt_write_reg((0x08 << 2), 0x0000580d);
			break;	/* 45MHz */
		case 1:
			dvbt_write_reg((0x08 << 2), 0x0000290d);
			break;	/* 20.833 = 56/7 * 20.8333 / (512/63)*512 */
		case 2:
			dvbt_write_reg((0x08 << 2), 0x000028da);
			break;	/* 20.7 */
		case 3:
			dvbt_write_reg((0x08 << 2), 0x0000383F);
			break;	/* 28.571  3863 */
		case 4:
			dvbt_write_reg((0x08 << 2), 0x00002F40);
			break;	/* 24 */
		default:
			dvbt_write_reg((0x08 << 2), 0x00003863);
			break;	/* 28.571 */
		}
	}
	/* memstart = 0x35400000;*/
	/* PR_DVBT("memstart is %x\n", memstart);*/
	/* dvbt_write_reg((0x10 << 2), memstart);*/
	/* 0x8f300000 */

	dvbt_write_reg((0x14 << 2), 0xe81c4ff6);
	/* AGC_TARGET 0xf0121385 */

	switch (samplerate) {
	case 0:
		dvbt_write_reg((0x15 << 2), 0x018c2df2);
		break;
	case 1:
		dvbt_write_reg((0x15 << 2), 0x0185bdf2);
		break;
	case 2:
		dvbt_write_reg((0x15 << 2), 0x0185bdf2);
		break;
	case 3:
		dvbt_write_reg((0x15 << 2), 0x0187bdf2);
		break;
	case 4:
		dvbt_write_reg((0x15 << 2), 0x0187bdf2);
		break;
	default:
		dvbt_write_reg((0x15 << 2), 0x0187bdf2);
		break;
	}
	if (tc_mode == 1)
		dvbt_write_regb((0x15 << 2), 11, 0);
	/* For TC mode. Notice, For ADC input is Unsigned,*/
	/* For Capture Data, It is TC. */
	dvbt_write_regb((0x15 << 2), 26, 1);
	/* [19:0] = [I , Q], I is high, Q is low. This bit is swap I/Q. */

	dvbt_write_reg((0x16 << 2), 0x00047f80);
	/* AGC_IFGAIN_CTRL */
	dvbt_write_reg((0x17 << 2), 0x00027f80);
	/* AGC_RFGAIN_CTRL */
	dvbt_write_reg((0x18 << 2), 0x00000190);
	/* AGC_IFGAIN_ACCUM */
	dvbt_write_reg((0x19 << 2), 0x00000190);
	/* AGC_RFGAIN_ACCUM */
	if (ch_if < 0)
		ch_if += adc_freq;
	if (ch_if > adc_freq)
		ch_if -= adc_freq;

	tmp = ch_if * (1 << 15) / adc_freq;
	dvbt_write_reg((0x20 << 2), tmp);

	dvbt_write_reg((0x21 << 2), 0x001ff000);
	/* DDC CS_FCFO_ADJ_CTRL */
	dvbt_write_reg((0x22 << 2), 0x00000000);
	/* DDC ICFO_ADJ_CTRL */
	dvbt_write_reg((0x23 << 2), 0x00004000);
	/* DDC TRACK_FCFO_ADJ_CTRL */

	dvbt_write_reg((0x27 << 2), (1 << 23)
	| (3 << 19) | (3 << 15) |  (1000 << 4) | 9);
	/* {8'd0,1'd1,4'd3,4'd3,11'd50,4'd9});//FSM_1 */
	dvbt_write_reg((0x28 << 2), (100 << 13) | 1000);
	/* {8'd0,11'd40,13'd50});//FSM_2 */
	dvbt_write_reg((0x29 << 2), (31 << 20) | (1 << 16) |
	(24 << 9) | (3 << 6) | 20);
	/* {5'd0,7'd127,1'd0,3'd0,7'd24,3'd5,6'd20}); */
	/*8K cannot sync*/
	dvbt_write_reg((0x29 << 2),
			dvbt_read_reg((0x29 << 2)) |
			0x7f << 9 | 0x7f << 20);

	if (mode == 0) {	/* DVBT */
		if (bandwidth == 0) {	/* 8M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x004ebf2e);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x00247551);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x00243999);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x002A0000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_8m();
				dvbt_write_reg((0x44 << 2),
					      0x0031ffcd);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 1) {	/* 7M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x0059ff10);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x0029aaa6);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x00296665);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x00300000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_7m();
				dvbt_write_reg((0x44 << 2),
					      0x00392491);
				break;	/* 28.57M */
			}
		} else if (bandwidth == 2) {	/* 6M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x00690000);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x00309c3e);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x002eaaaa);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x00380000);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_6m();
				dvbt_write_reg((0x44 << 2),
					      0x0042AA69);
				break;	/* 28.57M */
			}
		} else {	/* 5M */
			switch (samplerate) {
			case 0:
				ini_acf_iireq_src_45m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x007dfbe0);
				break;	/* 45M */
			case 1:
				ini_acf_iireq_src_207m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x003a554f);
				break;	/* 20.833M */
			case 2:
				ini_acf_iireq_src_207m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x0039f5c0);
				break;	/* 20.7M */
			case 3:
				ini_acf_iireq_src_2857m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			case 4:
				ini_acf_iireq_src_24m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x00433333);
				break;	/* 24M */
			default:
				ini_acf_iireq_src_2857m_5m();
				dvbt_write_reg((0x44 << 2),
					      0x004FFFFE);
				break;	/* 28.57M */
			}
		}
	} else {		/* ISDBT */
		switch (samplerate) {
		case 0:
			ini_acf_iireq_src_45m_6m();
			dvbt_write_reg((0x44 << 2), 0x00589800);
			break;
			/* 45M */
			/*SampleRate/(symbolRate)*2^20, */
			/*symbolRate = 512/63 for isdbt */
		case 1:
			ini_acf_iireq_src_207m_6m();
			dvbt_write_reg((0x44 << 2), 0x002903d4);
			break;	/* 20.833M */
		case 2:
			ini_acf_iireq_src_207m_6m();
			dvbt_write_reg((0x44 << 2), 0x00280ccc);
			break;	/* 20.7M */
		case 3:
			ini_acf_iireq_src_2857m_6m();
			dvbt_write_reg((0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		case 4:
			ini_acf_iireq_src_24m_6m();
			dvbt_write_reg((0x44 << 2), 0x002F4000);
			break;	/* 24M */
		default:
			ini_acf_iireq_src_2857m_6m();
			dvbt_write_reg((0x44 << 2), 0x00383fc8);
			break;	/* 28.57M */
		}
	}

	if (mode == 0)		/* DVBT */
		dvbt_write_reg((0x02 << 2),
			      (bandwidth << 20) | 0x10002);
	else			/* ISDBT */
		dvbt_write_reg((0x02 << 2), (1 << 20) | 0x1001a);
	/* {0x000,2'h1,20'h1_001a});    //For ISDBT , bandwidth should be 1,*/

	dvbt_write_reg((0x45 << 2), 0x00000000);
	/* SRC SFO_ADJ_CTRL */
	dvbt_write_reg((0x46 << 2), 0x02004000);
	/* SRC SFO_ADJ_CTRL */
	dvbt_write_reg((0x48 << 2), 0x000c0287);
	/* DAGC_CTRL1 */
	dvbt_write_reg((0x49 << 2), 0x00000005);
	/* DAGC_CTRL2 */
	dvbt_write_reg((0x4c << 2), 0x00000bbf);
	/* CCI_RP */
	dvbt_write_reg((0x4d << 2), 0x00000376);
	/* CCI_RPSQ */
	dvbt_write_reg((0x4e << 2), 0x0f0f1d09);
	/* CCI_CTRL */
	dvbt_write_reg((0x4f << 2), 0x00000000);
	/* CCI DET_INDX1 */
	dvbt_write_reg((0x50 << 2), 0x00000000);
	/* CCI DET_INDX2 */
	dvbt_write_reg((0x51 << 2), 0x00000000);
	/* CCI_NOTCH1_A1 */
	dvbt_write_reg((0x52 << 2), 0x00000000);
	/* CCI_NOTCH1_A2 */
	dvbt_write_reg((0x53 << 2), 0x00000000);
	/* CCI_NOTCH1_B1 */
	dvbt_write_reg((0x54 << 2), 0x00000000);
	/* CCI_NOTCH2_A1 */
	dvbt_write_reg((0x55 << 2), 0x00000000);
	/* CCI_NOTCH2_A2 */
	dvbt_write_reg((0x56 << 2), 0x00000000);
	/* CCI_NOTCH2_B1 */
	dvbt_write_reg((0x58 << 2), 0x00000885);
	/* MODE_DETECT_CTRL // 582 */
	if (mode == 0)		/* DVBT */
		dvbt_write_reg((0x5c << 2), 0x00001011);	/*  */
	else
		dvbt_write_reg((0x5c << 2), 0x00000753);
	/* ICFO_EST_CTRL ISDBT ICFO thres = 2 */

	dvbt_write_reg((0x5f << 2), 0x0ffffe10);
	/* TPS_FCFO_CTRL */
	dvbt_write_reg((0x61 << 2), 0x0000006c);
	/* FWDT ctrl */
	dvbt_write_reg((0x68 << 2), 0x128c3929);
	dvbt_write_reg((0x69 << 2), 0x91017f2d);
	/* 0x1a8 */
	dvbt_write_reg((0x6b << 2), 0x00442211);
	/* 0x1a8 */
	dvbt_write_reg((0x6c << 2), 0x01fc400a);
	/* 0x */
	dvbt_write_reg((0x6d << 2), 0x0030303f);
	/* 0x */
	dvbt_write_reg((0x73 << 2), 0xffffffff);
	/* CCI0_PILOT_UPDATE_CTRL */
	dvbt_write_reg((0x74 << 2), 0xffffffff);
	/* CCI0_DATA_UPDATE_CTRL */
	dvbt_write_reg((0x75 << 2), 0xffffffff);
	/* CCI1_PILOT_UPDATE_CTRL */
	dvbt_write_reg((0x76 << 2), 0xffffffff);
	/* CCI1_DATA_UPDATE_CTRL */

	tmp = mode == 0 ? 0x000001a2 : 0x00000da2;
	dvbt_write_reg((0x78 << 2), tmp);	/* FEC_CTR */

	dvbt_write_reg((0x7d << 2), 0x0000009d);
	dvbt_write_reg((0x7e << 2), 0x00004000);
	dvbt_write_reg((0x7f << 2), 0x00008000);

	dvbt_write_reg(((0x8b + 0) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 1) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 2) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 3) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 4) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 5) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 6) << 2), 0x20002000);
	dvbt_write_reg(((0x8b + 7) << 2), 0x20002000);

	dvbt_write_reg((0x93 << 2), 0x31);
	dvbt_write_reg((0x94 << 2), 0x00);
	dvbt_write_reg((0x95 << 2), 0x7f1);
	dvbt_write_reg((0x96 << 2), 0x20);

	dvbt_write_reg((0x98 << 2), 0x03f9115a);
	dvbt_write_reg((0x9b << 2), 0x000005df);

	dvbt_write_reg((0x9c << 2), 0x00100000);
	/* TestBus write valid, 0 is system clk valid */
	dvbt_write_reg((0x9d << 2), 0x01000000);
	/* DDR Start address */
	dvbt_write_reg((0x9e << 2), 0x02000000);
	/* DDR End   address */

	dvbt_write_regb((0x9b << 2), 7, 0);
	/* Enable Testbus dump to DDR */
	dvbt_write_regb((0x9b << 2), 8, 0);
	/* Run Testbus dump to DDR */

	dvbt_write_reg((0xd6 << 2), 0x00000003);
	/* apb_write_reg(DVBT_BASE+(0xd7<<2), 0x00000008); */
	dvbt_write_reg((0xd8 << 2), 0x00000120);
	dvbt_write_reg((0xd9 << 2), 0x01010101);

	ini_icfo_pn_index(mode);
	tfd_filter_coff_ini();

	calculate_cordic_para();
	msleep(20);
	/* delay_us(1); */

	dvbt_write_reg((0x02 << 2),
		      dvbt_read_reg((0x02 << 2)) | (1 << 0));
	dvbt_write_reg((0x02 << 2),
		      dvbt_read_reg((0x02 << 2)) | (1 << 24));
#endif
/* dvbt_check_status(); */
}

void calculate_cordic_para(void)
{
	dvbt_write_reg(0x0c, 0x00000040);
}

char *ofdm_fsm_name[] = { "    IDLE",
	"     AGC",
	"     CCI",
	"     ACQ",
	"    SYNC",
	"TRACKING",
	"  TIMING",
	" SP_SYNC",
	" TPS_DEC",
	"FEC_LOCK",
	"FEC_LOST"
};

void check_fsm_state(void)
{
	unsigned long tmp;

	tmp = dvbt_read_reg(0xa8);


	if ((tmp & 0xf) == 3) {
		dvbt_write_regb((0x9b << 2), 8, 1);
		/* Stop dump testbus; */
		dvbt_write_regb((0x0f << 2), 0, 1);
		tmp = dvbt_read_reg((0x9f << 2));

	}
}

void ofdm_read_all_regs(void)
{
	int i;
	unsigned long tmp;

	for (i = 0; i < 0xff; i++)
		tmp = dvbt_read_reg(0x00 + i * 4);
	/* printk("OFDM Reg (0x%x) is 0x%x\n", i, tmp); */

}

static int dvbt_get_status(struct aml_demod_sta *demod_sta)
{
	return dvbt_read_reg(0x0) >> 12 & 1;
}

static int dvbt_get_ber(struct aml_demod_sta *demod_sta)
{
/* PR_DVBT("[RSJ]per is %u\n",apb_read_reg(DVBT_BASE+(0xbf<<2))); */
	return dvbt_read_reg((0xbf << 2));
}

static int dvbt_get_snr(struct aml_demod_sta *demod_sta)
{
/* PR_DVBT("2snr is %u\n",((apb_read_reg(DVBT_BASE+(0x0a<<2)))>>20)&0x3ff); */
	return ((dvbt_read_reg((0x0a << 2))) >> 20) & 0x3ff;
	/*dBm: bit0~bit2=decimal */
}

static int dvbt_get_strength(struct aml_demod_sta *demod_sta)
{
/* int dbm = dvbt_get_ch_power(demod_sta, demod_i2c); */
/* return dbm; */
	return 0;
}

static int dvbt_get_ucblocks(struct aml_demod_sta *demod_sta)
{
	return 0;
/* return dvbt_get_per(); */
}

struct demod_status_ops *dvbt_get_status_ops(void)
{
	static struct demod_status_ops ops = {
		.get_status = dvbt_get_status,
		.get_ber = dvbt_get_ber,
		.get_snr = dvbt_get_snr,
		.get_strength = dvbt_get_strength,
		.get_ucblocks = dvbt_get_ucblocks,
	};

	return &ops;
}

