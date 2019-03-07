/*
 * drivers/amlogic/media/enhancement/amprime_sl/amprime_sl_hw.c
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


#include <linux/jiffies.h>
#include<linux/ktime.h>	/*gettimeofday*/
#include<linux/timekeeping.h>	/*gettimeofday*/

#include <linux/amlogic/media/amvecm/amvecm.h>	/*WRITE_VPP_REG*/
#include "amprime_sl.h"

/*======================================*/
/*#define USE_TASKLET	1*/

static inline void wbits_PRIMESL_CTRL0(
	unsigned int primesl_en, unsigned int gclk_ctrl,
	unsigned int reg_gclk_ctrl, unsigned int inv_y_ratio,
	unsigned int inv_chroma_ratio, unsigned int clip_en,
	unsigned int legacy_mode_en)
{
	union PRIMESL_CTRL0_BITS v;

	v.b.primesl_en = primesl_en;
	v.b.gclk_ctrl = gclk_ctrl;
	v.b.reg_gclk_ctrl = reg_gclk_ctrl;
	v.b.inv_y_ratio = inv_y_ratio;
	v.b.inv_chroma_ratio = inv_chroma_ratio;
	v.b.clip_en = clip_en;
	v.b.legacy_mode_en = legacy_mode_en;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL0, v.d32);
}
static inline void wbits_PRIMESL_CTRL1(unsigned int footroom,
				unsigned int l_headroom)
{
	union PRIMESL_CTRL1_BITS v;

	v.b.footroom = footroom;
	v.b.l_headroom = l_headroom;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL1, v.d32);
}
static inline void wbits_PRIMESL_CTRL2(unsigned int c_headroom)
{
	union PRIMESL_CTRL2_BITS v;

	v.b.c_headroom = c_headroom;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL2, v.d32);
}
static inline void wbits_PRIMESL_CTRL3(unsigned int mua,
				unsigned int mub)
{
	union PRIMESL_CTRL3_BITS v;

	v.b.mua = mua;
	v.b.mub = mub;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL3, v.d32);
}
static inline void wbits_PRIMESL_CTRL4(unsigned int oct_7_0,
				unsigned int oct_7_1)
{
	union PRIMESL_CTRL4_BITS v;

	v.b.oct_7_0 = oct_7_0;
	v.b.oct_7_1 = oct_7_1;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL4, v.d32);
}
static inline void wbits_PRIMESL_CTRL5(unsigned int oct_7_2,
				unsigned int oct_7_3)
{
	union PRIMESL_CTRL5_BITS v;

	v.b.oct_7_2 = oct_7_2;
	v.b.oct_7_3 = oct_7_3;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL5, v.d32);
}
static inline void wbits_PRIMESL_CTRL6(unsigned int oct_7_4,
				unsigned int oct_7_5)
{
	union PRIMESL_CTRL6_BITS v;

	v.b.oct_7_4 = oct_7_4;
	v.b.oct_7_5 = oct_7_5;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL6, v.d32);
}
static inline void wbits_PRIMESL_CTRL7(unsigned int oct_7_6)
{
	union PRIMESL_CTRL7_BITS v;

	v.b.oct_7_6 = oct_7_6;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL7, v.d32);
}
static inline void wbits_PRIMESL_CTRL8(unsigned int d_lut_threshold_3_0,
				unsigned int d_lut_threshold_3_1)
{
	union PRIMESL_CTRL8_BITS v;

	v.b.d_lut_threshold_3_0 = d_lut_threshold_3_0;
	v.b.d_lut_threshold_3_1 = d_lut_threshold_3_1;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL8, v.d32);
}
static inline void wbits_PRIMESL_CTRL9(unsigned int d_lut_threshold_3_2)
{
	union PRIMESL_CTRL9_BITS v;

	v.b.d_lut_threshold_3_2 = d_lut_threshold_3_2;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL9, v.d32);
}
static inline void wbits_PRIMESL_CTRL10(unsigned int d_lut_step_4_0,
				unsigned int d_lut_step_4_1,
				unsigned int d_lut_step_4_2,
				unsigned int d_lut_step_4_3)
{
	union PRIMESL_CTRL10_BITS v;

	v.b.d_lut_step_4_0 = d_lut_step_4_0;
	v.b.d_lut_step_4_1 = d_lut_step_4_1;
	v.b.d_lut_step_4_2 = d_lut_step_4_2;
	v.b.d_lut_step_4_3 = d_lut_step_4_3;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL10, v.d32);
}
static inline void wbits_PRIMESL_CTRL11(unsigned int rgb2yuv_9_1,
				unsigned int rgb2yuv_9_0)
{
	union PRIMESL_CTRL11_BITS v;

	v.b.rgb2yuv_9_1 = rgb2yuv_9_1;
	v.b.rgb2yuv_9_0 = rgb2yuv_9_0;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL11, v.d32);
}
static inline void wbits_PRIMESL_CTRL12(unsigned int rgb2yuv_9_3,
				unsigned int rgb2yuv_9_2)
{
	union PRIMESL_CTRL12_BITS v;

	v.b.rgb2yuv_9_3 = rgb2yuv_9_3;
	v.b.rgb2yuv_9_2 = rgb2yuv_9_2;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL12, v.d32);
}
static inline void wbits_PRIMESL_CTRL13(unsigned int rgb2yuv_9_5,
				unsigned int rgb2yuv_9_4)
{
	union PRIMESL_CTRL13_BITS v;

	v.b.rgb2yuv_9_5 = rgb2yuv_9_5;
	v.b.rgb2yuv_9_4 = rgb2yuv_9_4;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL13, v.d32);
}
static inline void wbits_PRIMESL_CTRL14(unsigned int rgb2yuv_9_7,
				unsigned int rgb2yuv_9_6)
{
	union PRIMESL_CTRL14_BITS v;

	v.b.rgb2yuv_9_7 = rgb2yuv_9_7;
	v.b.rgb2yuv_9_6 = rgb2yuv_9_6;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL14, v.d32);
}
static inline void wbits_PRIMESL_CTRL15(unsigned int rgb2yuv_9_8)
{
	union PRIMESL_CTRL15_BITS v;

	v.b.rgb2yuv_9_8 = rgb2yuv_9_8;
	_VSYNC_WR_MPEG_REG(PRIMESL_CTRL15, v.d32);
}

#define NOT_USR_WR_T	1

void slctr_set_rgb2yuv(const int *pTab)/*9byte*/
{
#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL11(pTab[1], pTab[0]);
	wbits_PRIMESL_CTRL12(pTab[3], pTab[2]);
	wbits_PRIMESL_CTRL13(pTab[5], pTab[4]);
	wbits_PRIMESL_CTRL14(pTab[7], pTab[6]);
	wbits_PRIMESL_CTRL15(pTab[8]);

#else
	int i;

	for (i = 0; i < 9; i++)
		WR_T(PRIME_RGB2YUV_9_0+i, (unsigned int)pTab[i]);
#endif
}

void slctr_set_lutstep(const unsigned int *pTab)/*4byte*/
{

#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL10(pTab[0], pTab[1], pTab[2], pTab[3]);

#else
	int i;

	for (i = 0; i < 4; i++)
		WR_T(PRIME_D_LUT_STEP_4_0+i, pTab[i]);

#endif
}
void slctr_set_lutthrd(const unsigned int *pTab)/*3byte*/
{

#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL8(pTab[0], pTab[1]);
	wbits_PRIMESL_CTRL9(pTab[2]);
#else
	int i;

	for (i = 0; i < 3; i++)
		WR_T(PRIME_D_LUT_THRESHOLD_3_0+i, pTab[i]);

#endif
}
void slctr_set_oct(const int *pTab)/*7byte*/
{

#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL4(pTab[0], pTab[1]);
	wbits_PRIMESL_CTRL5(pTab[2], pTab[3]);
	wbits_PRIMESL_CTRL6(pTab[4], pTab[5]);
	wbits_PRIMESL_CTRL7(pTab[6]);
#else
	int i;

	for (i = 0; i < 7; i++)
		WR_T(PRIME_OCT_7_0+i, (unsigned int)pTab[i]);
#endif
}
void slctr_set_lut_c(const int *pTab)/*7byte*/
{
	int i;

	_VSYNC_WR_MPEG_REG(PRIMESL_LUTC_ADDR_PORT, 0);
	for (i = 0; i < 65; i++)
		_VSYNC_WR_MPEG_REG(PRIMESL_LUTC_DATA_PORT, pTab[i]);
}

void slctr_set_lut_p(const int *pTab)/*7byte*/
{
	int i;

	_VSYNC_WR_MPEG_REG(PRIMESL_LUTP_ADDR_PORT, 0);
	for (i = 0; i < 65; i++)
		_VSYNC_WR_MPEG_REG(PRIMESL_LUTP_DATA_PORT, pTab[i]);
}

void slctr_set_lut_d(const int *pTab)/*7byte*/
{
	int i;

	_VSYNC_WR_MPEG_REG(PRIMESL_LUTD_ADDR_PORT, 0);
	for (i = 0; i < 65; i++)
		_VSYNC_WR_MPEG_REG(PRIMESL_LUTD_DATA_PORT, pTab[i]);
}

void prime_sl_set_reg(const struct prime_sl_t *pS)
{
	wbits_PRIMESL_CTRL1(pS->footroom, pS->l_headroom);
	wbits_PRIMESL_CTRL2(pS->c_headroom);
	wbits_PRIMESL_CTRL3(pS->mua, pS->mub);
	slctr_set_oct(&pS->oct[0]);
	slctr_set_lutthrd(&pS->d_lut_threshold[0]);
	slctr_set_lutstep(&pS->d_lut_step[0]);
	slctr_set_rgb2yuv(&pS->rgb2yuv[0]);
	slctr_set_lut_c(&pS->lut_c[0]);
	slctr_set_lut_p(&pS->lut_p[0]);
	slctr_set_lut_d(&pS->lut_d[0]);

	wbits_PRIMESL_CTRL0(1, 0, 0, pS->inv_y_ratio,
		pS->inv_chroma_ratio, 0, 0);
}
void prime_sl_close(void)
{
	wbits_PRIMESL_CTRL0(0, 0, 0, 0, 0, 0, 0);
}

