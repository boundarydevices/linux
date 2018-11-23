/*
 * drivers/amlogic/media/dtv_demod/demod_func.c
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

#include "demod_func.h"
#include "amlfrontend.h"

#include <linux/dvb/aml_demod.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/delay.h>
/*#include "acf_filter_coefficient.h"*/
#include <linux/mutex.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>


#define M6D

/* static void __iomem * demod_meson_reg_map[4]; */


MODULE_PARM_DESC(debug_demod, "\n\t\t Enable frontend debug information");
static int debug_demod;
module_param(debug_demod, int, 0644);

MODULE_PARM_DESC(demod_mobile_power, "\n\t\t demod_mobile_power debug information");
static int demod_mobile_power = 100;
module_param(demod_mobile_power, int, 0644);

static struct mutex mp;
static struct mutex dtvpll_init_lock;
static int dtvpll_init;
/*static int dtmb_spectrum = 2;*/
#if defined DEMOD_FPGA_VERSION
static int fpga_version = 1;
#else
static int fpga_version = -1;
#endif

#if 0
/*register base address*/
unsigned int ddemod_reg_base;
unsigned int dd_hiu_reg_base;
unsigned int dtvafe_reg_base;	/* only txhd */

void dtvdemod_base_add_init(void)
{
	/* init value */
	ddemod_reg_base = IO_DEMOD_BASE;
	dd_hiu_reg_base = IO_HIU_BASE;
	dtvafe_reg_base = TXHD_TVFE_BASE;

	if (is_meson_gxtvbb_cpu()) {
		ddemod_reg_base = IO_DEMOD_BASE;
		dd_hiu_reg_base = IO_HIU_BASE;
	} else if (is_meson_txl_cpu()) {
		ddemod_reg_base = IO_DEMOD_BASE;
		dd_hiu_reg_base = IO_HIU_BASE;
	} else if (is_meson_txlx_cpu()) {
		ddemod_reg_base = TXLX_DEMOD_BASE;
		dd_hiu_reg_base = TXLX_IO_HIU_BASE;
	} else if (is_meson_gxlx_cpu()) {
		ddemod_reg_base = GXLX_DEMOD_BASE;
		dd_hiu_reg_base = IO_HIU_BASE;
	} else if (is_meson_txhd_cpu()) {
		ddemod_reg_base = TXHD_DEMOD_BASE;
		dd_hiu_reg_base = TXLX_IO_HIU_BASE;
		dtvafe_reg_base = TXHD_TVFE_BASE;
	} else {
		ddemod_reg_base = TXHD_DEMOD_BASE;
		dd_hiu_reg_base = TXLX_IO_HIU_BASE;
		dtvafe_reg_base = TXHD_TVFE_BASE;
	}

	PR_DBG(" dtv demod reg base address init =0x%x\n", ddemod_reg_base);
}
#endif

void dtvpll_lock_init(void)
{
	mutex_init(&dtvpll_init_lock);
}

void dtvpll_init_flag(int on)
{
	mutex_lock(&dtvpll_init_lock);
	dtvpll_init = on;
	mutex_unlock(&dtvpll_init_lock);
	pr_err("%s %d\n", __func__, on);
}

int get_dtvpll_init_flag(void)
{
	int val;

	mutex_lock(&dtvpll_init_lock);
	val = dtvpll_init;
	mutex_unlock(&dtvpll_init_lock);
	if (!val)
		pr_err("%s: %d\n", __func__, val);
	return val;
}


void adc_dpll_setup(int clk_a, int clk_b, int clk_sys, int dvb_mode)
{
	int unit, found, ena, enb, div2;
	int pll_m, pll_n, pll_od_a, pll_od_b, pll_xd_a, pll_xd_b;
	long freq_osc, freq_dco, freq_b, freq_a, freq_sys;
	long freq_b_act, freq_a_act, freq_sys_act, err_tmp, best_err;
	union adc_pll_cntl adc_pll_cntl;
	union adc_pll_cntl2 adc_pll_cntl2;
	union adc_pll_cntl3 adc_pll_cntl3;
	union adc_pll_cntl4 adc_pll_cntl4;
	union demod_dig_clk dig_clk_cfg;

	struct dfe_adcpll_para ddemod_pll;
	int sts_pll;

	if (is_ic_ver(IC_VER_TL1)) {
		dtvpll_init_flag(1);
		return;
	}

	adc_pll_cntl.d32 = 0;
	adc_pll_cntl2.d32 = 0;
	adc_pll_cntl3.d32 = 0;
	adc_pll_cntl4.d32 = 0;

	PR_DBG("target clk_a %d  clk_b %d\n", clk_a, clk_b);

	unit = 10000;		/* 10000 as 1 MHz, 0.1 kHz resolution. */
	freq_osc = 24 * unit;

	if (clk_a < 1000)
		freq_a = clk_a * unit;
	else
		freq_a = clk_a * (unit / 1000);

	if (clk_b < 1000)
		freq_b = clk_b * unit;
	else
		freq_b = clk_b * (unit / 1000);

	ena = clk_a > 0 ? 1 : 0;
	enb = clk_b > 0 ? 1 : 0;

	if (ena || enb)
		adc_pll_cntl3.b.enable = 1;
	adc_pll_cntl3.b.reset = 1;

	found = 0;
	best_err = 100 * unit;
	pll_od_a = 1;
	pll_od_b = 1;
	pll_n = 1;
	for (pll_m = 1; pll_m < 512; pll_m++) {
		/* for (pll_n=1; pll_n<=5; pll_n++) { */
		#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu())) {
			freq_dco = freq_osc * pll_m / pll_n / 2;
			/*txl add div2*/
			if (freq_dco < 700 * unit || freq_dco > 1000 * unit)
				continue;
		} else {
			freq_dco = freq_osc * pll_m / pll_n;
			if (freq_dco < 750 * unit || freq_dco > 1550 * unit)
				continue;
		}
		#else
		/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {*/
		if (get_ic_ver() >= IC_VER_TXL) {
			/*txl add div2*/
			freq_dco = freq_osc * pll_m / pll_n / 2;
			if (freq_dco < 700 * unit || freq_dco > 1000 * unit)
				continue;

		} else {
			freq_dco = freq_osc * pll_m / pll_n;
			if (freq_dco < 750 * unit || freq_dco > 1550 * unit)
				continue;

		}
		#endif
		pll_xd_a = freq_dco / (1 << pll_od_a) / freq_a;
		pll_xd_b = freq_dco / (1 << pll_od_b) / freq_b;

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		err_tmp = (freq_a_act - freq_a) * ena + (freq_b_act - freq_b) *
		    enb;

		if (err_tmp >= best_err)
			continue;

		adc_pll_cntl.b.pll_m = pll_m;
		adc_pll_cntl.b.pll_n = pll_n;
		adc_pll_cntl.b.pll_od0 = pll_od_b;
		adc_pll_cntl.b.pll_od1 = pll_od_a;
		adc_pll_cntl.b.pll_xd0 = pll_xd_b;
		adc_pll_cntl.b.pll_xd1 = pll_xd_a;
		#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu())) {
			adc_pll_cntl4.b.pll_od3 = 0;
			adc_pll_cntl.b.pll_od2 = 0;
		} else {
			adc_pll_cntl2.b.div2_ctrl =
				freq_dco > 1000 * unit ? 1 : 0;
		}
		#else
		/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {*/
		if (get_ic_ver() >= IC_VER_TXL) {
			adc_pll_cntl4.b.pll_od3 = 0;
			adc_pll_cntl.b.pll_od2 = 0;

		} else {
			adc_pll_cntl2.b.div2_ctrl =
				freq_dco > 1000 * unit ? 1 : 0;

		}
		#endif
		found = 1;
		best_err = err_tmp;
		/* } */
	}

	pll_m = adc_pll_cntl.b.pll_m;
	pll_n = adc_pll_cntl.b.pll_n;
	pll_od_b = adc_pll_cntl.b.pll_od0;
	pll_od_a = adc_pll_cntl.b.pll_od1;
	pll_xd_b = adc_pll_cntl.b.pll_xd0;
	pll_xd_a = adc_pll_cntl.b.pll_xd1;

	#if 0
	if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
		|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu()))
		div2 = 1;
	else
		div2 = adc_pll_cntl2.b.div2_ctrl;

	#else
	/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))*/
	if (get_ic_ver() >= IC_VER_TXL)
		div2 = 1;
	else
		div2 = adc_pll_cntl2.b.div2_ctrl;

	#endif
	/*
	 * p_adc_pll_cntl  =  adc_pll_cntl.d32;
	 * p_adc_pll_cntl2 = adc_pll_cntl2.d32;
	 * p_adc_pll_cntl3 = adc_pll_cntl3.d32;
	 * p_adc_pll_cntl4 = adc_pll_cntl4.d32;
	 */
	adc_pll_cntl3.b.reset = 0;
	/* *p_adc_pll_cntl3 = adc_pll_cntl3.d32; */
	if (!found) {
		PR_DBG(" ERROR can't setup %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_a / (unit / 1000));
	} else {
	#if 0
		if ((is_meson_txl_cpu()) || (is_meson_txlx_cpu())
			|| (is_meson_gxlx_cpu()) || (is_meson_txhd_cpu()))
			freq_dco = freq_osc * pll_m / pll_n / 2;
		else
			freq_dco = freq_osc * pll_m / pll_n;
	#else
		/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))*/
		if (get_ic_ver() >= IC_VER_TXL)
			freq_dco = freq_osc * pll_m / pll_n / 2;
		else
			freq_dco = freq_osc * pll_m / pll_n;
	#endif
		PR_DBG(" ADC PLL  M %3d   N %3d\n", pll_m, pll_n);
		PR_DBG(" ADC PLL DCO %ld kHz\n", freq_dco / (unit / 1000));

		PR_DBG(" ADC PLL XD %3d  OD %3d\n", pll_xd_b, pll_od_b);
		PR_DBG(" ADC PLL XD %3d  OD %3d\n", pll_xd_a, pll_od_a);

		freq_a_act = freq_dco / (1 << pll_od_a) / pll_xd_a;
		freq_b_act = freq_dco / (1 << pll_od_b) / pll_xd_b;

		PR_DBG(" B %7ld kHz %7ld kHz\n",
		       freq_b / (unit / 1000), freq_b_act / (unit / 1000));
		PR_DBG(" A %7ld kHz %7ld kHz\n",
		       freq_a / (unit / 1000), freq_a_act / (unit / 1000));

		if (clk_sys > 0) {
			dig_clk_cfg.b.demod_clk_en = 1;
			dig_clk_cfg.b.demod_clk_sel = 3;
			if (clk_sys < 1000)
				freq_sys = clk_sys * unit;
			else
				freq_sys = clk_sys * (unit / 1000);

			dig_clk_cfg.b.demod_clk_div = freq_dco / (1 + div2) /
			    freq_sys - 1;
			freq_sys_act = freq_dco / (1 + div2) /
			    (dig_clk_cfg.b.demod_clk_div + 1);
			PR_DBG(" SYS %7ld kHz div %d+1  %7ld kHz\n",
			       freq_sys / (unit / 1000),
			       dig_clk_cfg.b.demod_clk_div,
			       freq_sys_act / (unit / 1000));
		} else {
			dig_clk_cfg.b.demod_clk_en = 0;
		}

		/* *p_demod_dig_clk = dig_clk_cfg.d32; */
	}
#if 0
	if (is_meson_txlx_cpu()) {
		demod_set_demod_reg(TXLX_ADC_RESET_VALUE, TXLX_ADC_REG3);
		demod_set_demod_reg(adc_pll_cntl.d32, TXLX_ADC_REG1);
		demod_set_demod_reg(dig_clk_cfg.d32, TXLX_ADC_REG6);
		if (dvb_mode == Gxtv_Atsc)
			demod_set_demod_reg(0x507, TXLX_ADC_REG6);
		else
			demod_set_demod_reg(0x502, TXLX_ADC_REG6);
		demod_set_demod_reg(TXLX_ADC_REG3_VALUE, TXLX_ADC_REG3);
		/* debug */
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG1,
				demod_read_demod_reg(TXLX_ADC_REG1));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG2,
				demod_read_demod_reg(TXLX_ADC_REG2));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG3,
				demod_read_demod_reg(TXLX_ADC_REG3));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG4,
				demod_read_demod_reg(TXLX_ADC_REG4));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG5,
				demod_read_demod_reg(TXLX_ADC_REG5));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG6,
				demod_read_demod_reg(TXLX_ADC_REG6));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG7,
				demod_read_demod_reg(TXLX_ADC_REG7));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG8,
				demod_read_demod_reg(TXLX_ADC_REG8));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REG9,
				demod_read_demod_reg(TXLX_ADC_REG9));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REGB,
				demod_read_demod_reg(TXLX_ADC_REGB));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REGC,
				demod_read_demod_reg(TXLX_ADC_REGC));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REGD,
				demod_read_demod_reg(TXLX_ADC_REGD));
		PR_DBG("[adc][%x]%x\n", TXLX_ADC_REGE,
				demod_read_demod_reg(TXLX_ADC_REGE));
		PR_DBG("[demod][%x]%x\n", TXLX_DEMOD_REG1,
				demod_read_demod_reg(TXLX_DEMOD_REG1));
		PR_DBG("[demod][%x]%x\n", TXLX_DEMOD_REG2,
				demod_read_demod_reg(TXLX_DEMOD_REG2));
		PR_DBG("[demod][%x]%x\n", TXLX_DEMOD_REG3,
				demod_read_demod_reg(TXLX_DEMOD_REG3));
	} else if (is_meson_txl_cpu()) {
		demod_set_demod_reg(TXLX_ADC_RESET_VALUE, ADC_REG3);
		demod_set_demod_reg(adc_pll_cntl.d32, ADC_REG1);
		demod_set_demod_reg(dig_clk_cfg.d32, ADC_REG6);
		demod_set_demod_reg(0x502, ADC_REG6);
		demod_set_demod_reg(TXLX_ADC_REG3_VALUE, ADC_REG3);
		/* debug */
		PR_DBG("[adc][%x]%x\n", ADC_REG1,
				demod_read_demod_reg(ADC_REG1));
		PR_DBG("[adc][%x]%x\n", ADC_REG2,
				demod_read_demod_reg(ADC_REG2));
		PR_DBG("[adc][%x]%x\n", ADC_REG3,
				demod_read_demod_reg(ADC_REG3));
		PR_DBG("[adc][%x]%x\n", ADC_REG4,
				demod_read_demod_reg(ADC_REG4));
		PR_DBG("[adc][%x]%x\n", ADC_REG5,
				demod_read_demod_reg(ADC_REG5));
		PR_DBG("[adc][%x]%x\n", ADC_REG6,
				demod_read_demod_reg(ADC_REG6));
		PR_DBG("[adc][%x]%x\n", ADC_REG7,
				demod_read_demod_reg(ADC_REG7));
		PR_DBG("[adc][%x]%x\n", ADC_REG8,
				demod_read_demod_reg(ADC_REG8));
		PR_DBG("[adc][%x]%x\n", ADC_REG9,
				demod_read_demod_reg(ADC_REG9));
		PR_DBG("[adc][%x]%x\n", ADC_REGB,
				demod_read_demod_reg(ADC_REGB));
		PR_DBG("[adc][%x]%x\n", ADC_REGC,
				demod_read_demod_reg(ADC_REGC));
		PR_DBG("[adc][%x]%x\n", ADC_REGD,
				demod_read_demod_reg(ADC_REGD));
		PR_DBG("[adc][%x]%x\n", ADC_REGE,
				demod_read_demod_reg(ADC_REGE));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG0,
				demod_read_demod_reg(DEMOD_TOP_REG0));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG4,
				demod_read_demod_reg(DEMOD_TOP_REG4));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG8,
				demod_read_demod_reg(DEMOD_TOP_REG8));
	} else if (is_meson_gxlx_cpu()) {
		/*demod_set_demod_reg(TXLX_ADC_RESET_VALUE, ADC_REG3);*/
		/*demod_set_demod_reg(adc_pll_cntl.d32, ADC_REG1);*/
		demod_set_demod_reg(dig_clk_cfg.d32, ADC_REG6);
		demod_set_demod_reg(0x1000502, ADC_REG6);
		/*demod_set_demod_reg(TXLX_ADC_REG3_VALUE, ADC_REG3);*/
		/* debug */
		PR_DBG("gxlx[adc][%x]%x\n", ADC_REG1,
				demod_read_demod_reg(ADC_REG1));
		PR_DBG("[adc][%x]%x\n", ADC_REG2,
				demod_read_demod_reg(ADC_REG2));
		PR_DBG("[adc][%x]%x\n", ADC_REG3,
				demod_read_demod_reg(ADC_REG3));
		PR_DBG("[adc][%x]%x\n", ADC_REG4,
				demod_read_demod_reg(ADC_REG4));
		PR_DBG("[adc][%x]%x\n", ADC_REG5,
				demod_read_demod_reg(ADC_REG5));
		PR_DBG("[adc][%x]%x\n", ADC_REG6,
				demod_read_demod_reg(ADC_REG6));
		PR_DBG("[adc][%x]%x\n", ADC_REG7,
				demod_read_demod_reg(ADC_REG7));
		PR_DBG("[adc][%x]%x\n", ADC_REG8,
				demod_read_demod_reg(ADC_REG8));
		PR_DBG("[adc][%x]%x\n", ADC_REG9,
				demod_read_demod_reg(ADC_REG9));
		PR_DBG("[adc][%x]%x\n", ADC_REGB,
				demod_read_demod_reg(ADC_REGB));
		PR_DBG("[adc][%x]%x\n", ADC_REGC,
				demod_read_demod_reg(ADC_REGC));
		PR_DBG("[adc][%x]%x\n", ADC_REGD,
				demod_read_demod_reg(ADC_REGD));
		PR_DBG("[adc][%x]%x\n", ADC_REGE,
				demod_read_demod_reg(ADC_REGE));
		PR_DBG("[demod][%x]%x\n", GXLX_DEMOD_REG1,
				demod_read_demod_reg(GXLX_DEMOD_REG1));
		PR_DBG("[demod][%x]%x\n", GXLX_DEMOD_REG2,
				demod_read_demod_reg(GXLX_DEMOD_REG2));
		PR_DBG("[demod][%x]%x\n", GXLX_DEMOD_REG3,
				demod_read_demod_reg(GXLX_DEMOD_REG3));
	} else {
		demod_set_demod_reg(ADC_RESET_VALUE, ADC_REG3);	/* adc reset */
		demod_set_demod_reg(adc_pll_cntl.d32, ADC_REG1);
		demod_set_demod_reg(dig_clk_cfg.d32, ADC_REG6);
		demod_set_demod_reg(ADC_REG3_VALUE, ADC_REG3);
		/* debug */
		PR_DBG("[adc][%x]%x\n", ADC_REG1,
				demod_read_demod_reg(ADC_REG1));
		PR_DBG("[adc][%x]%x\n", ADC_REG2,
				demod_read_demod_reg(ADC_REG2));
		PR_DBG("[adc][%x]%x\n", ADC_REG3,
				demod_read_demod_reg(ADC_REG3));
		PR_DBG("[adc][%x]%x\n", ADC_REG4,
				demod_read_demod_reg(ADC_REG4));
		PR_DBG("[adc][%x]%x\n", ADC_REG6,
				demod_read_demod_reg(ADC_REG6));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG0,
				demod_read_demod_reg(DEMOD_TOP_REG0));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG4,
				demod_read_demod_reg(DEMOD_TOP_REG4));
		PR_DBG("[demod][%x]%x\n", DEMOD_TOP_REG8,
				demod_read_demod_reg(DEMOD_TOP_REG8));
	}
#endif
	ddemod_pll.adcpllctl = adc_pll_cntl.d32;
	ddemod_pll.demodctl = dig_clk_cfg.d32;
	ddemod_pll.atsc = 0;
	/*if ((dvb_mode == Gxtv_Atsc) && (is_meson_txlx_cpu()))*/
	if ((dvb_mode == Gxtv_Atsc) && !is_atsc_ver(IC_MD_NONE))
		ddemod_pll.atsc = 1;


	/* debug only */
	/*printk("adcpllctl=0x%x\n",adc_pll_cntl.d32);*/

	sts_pll = adc_set_pll_cntl(1, 0x0c, &ddemod_pll);
	if (sts_pll < 0) {
		/*set pll fail*/
		PR_ERR("%s:set pll fail! please check!\n", __func__);
	} else {
		dtvpll_init_flag(1);
	}

	debug_adc_pll();

}

void demod_set_adc_core_clk(int adc_clk, int sys_clk, int dvb_mode)
{
	adc_dpll_setup(25, adc_clk, sys_clk, dvb_mode);
}

void demod_set_cbus_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	PR_DBG("[cbus][write]%x\n", (IO_CBUS_PHY_BASE + (addr << 2)));
	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
}

unsigned int demod_read_cbus_reg(unsigned int addr)
{
/* return __raw_readl(CBUS_REG_ADDR(addr)); */
	unsigned int tmp;
	void __iomem *vaddr;

	vaddr = ioremap((IO_CBUS_PHY_BASE + (addr << 2)), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
/* tmp = aml_read_cbus(addr); */
	PR_DBG("[cbus][read]%x,data is %x\n",
	(IO_CBUS_PHY_BASE + (addr << 2)), tmp);
	return tmp;
}

void demod_set_ao_reg(unsigned int data, unsigned int addr)
{
	writel(data, gbase_aobus() + addr);
}

unsigned int demod_read_ao_reg(unsigned int addr)
{
	unsigned int tmp;

	tmp = readl(gbase_aobus() + addr);

	return tmp;
}


void demod_set_demod_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	mutex_lock(&mp);
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
}

void demod_set_tvfe_reg(unsigned int data, unsigned int addr)
{
	void __iomem *vaddr;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);
}

unsigned int demod_read_demod_reg(unsigned int addr)
{
	unsigned int tmp;
	void __iomem *vaddr;

	mutex_lock(&mp);
	vaddr = ioremap((addr), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
	mutex_unlock(&mp);

	return tmp;
}

void power_sw_hiu_reg(int on)
{
	if (on == PWR_ON) {
		if (is_ic_ver(IC_VER_TL1))
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG, 0);
		else
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
			(dd_tvafe_hiu_reg_read(HHI_DEMOD_MEM_PD_REG)
			& (~0x2fff)));
	} else {
		if (is_ic_ver(IC_VER_TL1))
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
				0xffffffff);
		else
			dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
			(dd_tvafe_hiu_reg_read
			(HHI_DEMOD_MEM_PD_REG) | 0x2fff));

	}
}
void power_sw_reset_reg(int en)
{
	if (en) {
		reset_reg_write(RESET_RESET0_LEVEL,
			(reset_reg_read(RESET_RESET0_LEVEL) & (~(0x1 << 8))));
	} else {
		reset_reg_write(RESET_RESET0_LEVEL,
			(reset_reg_read(RESET_RESET0_LEVEL) | (0x1 << 8)));

	}
}
void demod_power_switch(int pwr_cntl)
{
	int reg_data;
#if 1
/*if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {*/
if (is_ic_ver(IC_VER_TXLX) || is_ic_ver(IC_VER_TXHD)) {

	if (pwr_cntl == PWR_ON) {
		PR_DBG("[PWR]: Power on demod_comp %x,%x\n",
		       AO_RTI_GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_ISO0);
		/* Powerup demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data & (~(0x1 << 10))),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
		/*PR_DBG("[PWR]: Power on demod_comp %x,%x\n",*/
		/*      TXLX_HHI_DEMOD_MEM_PD_REG, TXLX_RESET0_LEVEL);*/
		/* Power up memory */
		power_sw_hiu_reg(PWR_ON);
		/* reset */
		power_sw_reset_reg(1); /*reset*/
	/*	msleep(20);*/

		/* remove isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
				  (~(0x3 << 14))), AO_RTI_GEN_PWR_ISO0);
		/* pull up reset */
		power_sw_reset_reg(0); /*reset*/
/* *P_RESET0_LEVEL |= (0x1<<8); */
	} else {
		PR_DBG("[PWR]: Power off demod_comp\n");
		/* add isolation */

			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
				  (0x3 << 14)), AO_RTI_GEN_PWR_ISO0);

		/* power down memory */
		power_sw_hiu_reg(PWR_OFF);
		/* power down demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data | (0x1 << 10)),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
	}
	/*} else if (is_meson_gxlx_cpu()) {*/
} else if (is_ic_ver(IC_VER_GXLX)) {

	PR_DBG("[PWR]: GXLX not support power switch,power mem\n");

		power_sw_hiu_reg(PWR_ON);
} else if  (is_ic_ver(IC_VER_TL1))  {

	if (pwr_cntl == PWR_ON) {
		PR_DBG("[PWR]: Power on demod_comp %x,%x\n",
		       AO_RTI_GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_ISO0);
		/* Powerup demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data & (~(0x1 << 10))),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
		/*PR_DBG("[PWR]: Power on demod_comp %x,%x\n",*/
		/*      TXLX_HHI_DEMOD_MEM_PD_REG, TXLX_RESET0_LEVEL);*/

		/* Power up memory */
		power_sw_hiu_reg(PWR_ON);
		/* reset */
		power_sw_reset_reg(1); /*reset*/
	/*	msleep(20);*/

		/* remove isolation */
		demod_set_ao_reg(
			(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
			  (~(0x3 << 14))), AO_RTI_GEN_PWR_ISO0);

		/* pull up reset */
		power_sw_reset_reg(0); /*reset*/
/* *P_RESET0_LEVEL |= (0x1<<8); */
	} else {
		PR_DBG("[PWR]: Power off demod_comp\n");

		/* add isolation */
		demod_set_ao_reg(
			(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
			  (0x3 << 14)), AO_RTI_GEN_PWR_ISO0);

		/* power down memory */
		power_sw_hiu_reg(PWR_OFF);
		/* power down demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data | (0x1 << 10)),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
	}
	/*} else if (is_meson_gxlx_cpu()) {*/
} else {
	if (pwr_cntl == PWR_ON) {
		PR_DBG("[PWR]: Power on demod_comp %x,%x\n",
		       AO_RTI_GEN_PWR_SLEEP0, AO_RTI_GEN_PWR_ISO0);
		/* Powerup demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data & (~(0x1 << 10))),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
		/*PR_DBG("[PWR]: Power on demod_comp %x,%x\n",*/
		/*       HHI_DEMOD_MEM_PD_REG, RESET0_LEVEL);*/

		/* Power up memory */
		power_sw_hiu_reg(PWR_ON);
		/* reset */
		power_sw_reset_reg(1); /*reset*/
	/*	msleep(20);*/
		/* remove isolation */
			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) &
				  (~(0x3 << 14))), AO_RTI_GEN_PWR_ISO0);
		/* pull up reset */
		power_sw_reset_reg(0); /*reset*/
/* *P_RESET0_LEVEL |= (0x1<<8); */
	} else {
		PR_DBG("[PWR]: Power off demod_comp\n");
		/* add isolation */

			demod_set_ao_reg(
				(demod_read_ao_reg(AO_RTI_GEN_PWR_ISO0) |
				  (0x3 << 14)), AO_RTI_GEN_PWR_ISO0);

		/* power down memory */
		power_sw_hiu_reg(PWR_OFF);
		/* power down demod_comb */
		reg_data = demod_read_ao_reg(AO_RTI_GEN_PWR_SLEEP0);
		demod_set_ao_reg((reg_data | (0x1 << 10)),
				 AO_RTI_GEN_PWR_SLEEP0);
		/* [10] power on */
	}
}

#endif
}
/* // 0 -DVBC J.83B, 1-DVBT, ISDBT, 2-ATSC,3-DTMB */
void demod_set_mode_ts(unsigned char dvb_mode)
{
	union demod_cfg0 cfg0;

	cfg0.b.adc_format = 1;
	cfg0.b.adc_regout = 1;
	if (dvb_mode == Gxtv_Dtmb) {
		cfg0.b.ts_sel = 1;
		cfg0.b.mode = 1;
		if (is_ic_ver(IC_VER_TL1)) {
			cfg0.b.adc_format = 0;
			cfg0.b.adc_regout = 0;
		}
	} else if (dvb_mode == Gxtv_Dvbt_Isdbt) {
		cfg0.b.ts_sel = 1<<1;
		cfg0.b.mode = 1<<1;
		cfg0.b.adc_format = 0;
		cfg0.b.adc_regout = 0;
	} else if (dvb_mode == Gxtv_Atsc) {
		cfg0.b.ts_sel = 1<<2;
		cfg0.b.mode = 1<<2;
		cfg0.b.adc_format = 0;
		if (!is_ic_ver(IC_VER_TL1)) {
			cfg0.b.adc_regout = 1;
			cfg0.b.adc_regadj = 2;
		}
	} else if (dvb_mode == Gxtv_Dvbc) {
		if (is_dvbc_ver(IC_DVBC_V2)) {
			cfg0.b.ts_sel = 2;
			cfg0.b.mode = 7;
			cfg0.b.adc_format = 1;
			cfg0.b.adc_regout = 0;
		} else {
			cfg0.b.ts_sel = 1<<3;
			cfg0.b.mode = 1<<3;
			cfg0.b.adc_format = 0;
			cfg0.b.adc_regout = 0;
		}
	}

	demod_write_reg(DEMOD_TOP_REG0, cfg0.d32);
}

void clocks_set_sys_defaults(unsigned char dvb_mode)
{
	union demod_cfg2 cfg2;
	int sts_pll;

	demod_power_switch(PWR_ON);

	tvafe_set_ddemod_default();
	demod_set_demod_default();
	sts_pll = adc_set_pll_cntl(1, 0x04, NULL);

	if (sts_pll < 0) {
		/*set pll fail*/
		PR_ERR("%s:set pll default fail! please check!\n", __func__);
	}

	demod_set_mode_ts(dvb_mode);
	cfg2.b.biasgen_en = 1;
	cfg2.b.en_adc = 1;
	demod_write_reg(DEMOD_TOP_REG8, cfg2.d32);
	PR_DBG("dvb_mode is %d\n", dvb_mode);
	debug_check_reg_val(REG_M_DEMOD, DEMOD_TOP_REG0);

}
#if 0
void dtmb_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;
	/* ary	demod_set_demod_reg(reg_data, reg_addr); */
	demod_set_reg_rlt(reg_addr, reg_data);
/* apb_write_reg(reg_addr,reg_data); */
}

unsigned int dtmb_read_reg(unsigned int reg_addr)
{
	if (!get_dtvpll_init_flag())
		return 0;
	/*ary return demod_read_demod_reg(reg_addr);*/
	return demod_read_reg_rlt(reg_addr);
}
#else
/* */

void dtmb_write_reg(int reg_addr, int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(reg_data, gbase_dtmb() + reg_addr);

	mutex_unlock(&mp);
/* apb_write_reg(reg_addr,reg_data); */
}

unsigned int dtmb_read_reg(unsigned int reg_addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_dtmb() + reg_addr);

	mutex_unlock(&mp);

	return tmp;
}

#endif
/* */
void dvbt_write_reg(unsigned int addr, unsigned int data)
{

	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_dvbt() + addr);

	mutex_unlock(&mp);
}

unsigned int dvbt_read_reg(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_dvbt() + addr);

	mutex_unlock(&mp);
/* printk("[demod][read]%x,data is %x\n",(addr),tmp); */
	return tmp;
}

void atsc_write_reg(unsigned int reg_addr, unsigned int reg_data)
{
	unsigned int data;

	if (!get_dtvpll_init_flag())
		return;

	data = (reg_addr & 0xffff) << 8 | (reg_data & 0xff);

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/
	writel(data, gbase_atsc());
	mutex_unlock(&mp);
}

unsigned int atsc_read_reg(unsigned int reg_addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	writel((reg_addr & 0xffff) << 8, gbase_atsc() + 4);
	tmp = readl(gbase_atsc());

	mutex_unlock(&mp);

	return tmp & 0xff;
}

/*TL1*/
void atsc_write_reg_v4(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_atsc() + (addr << 2));

	mutex_unlock(&mp);
}

unsigned int atsc_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_atsc() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;
}

unsigned int atsc_read_iqr_reg(void)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_atsc() + 8);

	mutex_unlock(&mp);

	PR_DBG("[atsc irq] is %x\n", tmp);
	return tmp & 0xffffffff;
}

#if 0
void demod_init_mutex(void)
{
	mutex_init(&mp);
}
#endif

int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_sys *demod_sys)
{
/* int adc_clk; */
/* demod_sta->tmp=Adc_mode; */
	unsigned char dvb_mode;
	int clk_adc, clk_dem;
	int nco_rate;
	/* int gpioDV_2;*/
	/*int gpiW_2;*/

	dvb_mode = demod_sta->dvb_mode;
	clk_adc = demod_sys->adc_clk;
	clk_dem = demod_sys->demod_clk;
	nco_rate = ((clk_adc / 1000) * 256) / 224 + 2;
	PR_DBG
	    ("demod_set_sys,clk_adc is %d,clk_demod is %d\n",
	     clk_adc, clk_dem);
	mutex_init(&mp);
	clocks_set_sys_defaults(dvb_mode);
	#if 0	/*use dtvdemod_set_agc_pinmux*/
	/* open dtv adc pinmux */
	if (is_meson_txlx_cpu() || is_meson_gxlx_cpu()) {
		PR_DBG("[R840][txlx_gxlx]set adc pinmux\n");
	} else if (is_meson_txhd_cpu()) {
		gpioDV_2 = demod_read_demod_reg(0xff634400 + (0x27 << 2));
		PR_DBG("[txhd_R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
		/* bit [11:8] */
		gpioDV_2 = gpioDV_2 & 0xfffff0ff;
		gpioDV_2 = gpioDV_2 | (0x1 << 8);

		demod_set_demod_reg(gpioDV_2, 0xff634400 + (0x27 << 2));
		PR_DBG("[R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
	} else if (is_meson_txl_cpu()) {
		gpioDV_2 = demod_read_demod_reg(0xc8834400 + (0x2e << 2));
		PR_DBG("[R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
		gpioDV_2 = gpioDV_2 | (0x1 << 22);
		gpioDV_2 = gpioDV_2 & ~(0x3 << 19);
		gpioDV_2 = gpioDV_2 & ~(0x1 << 23);
		gpioDV_2 = gpioDV_2 & ~(0x1 << 31);
		demod_set_demod_reg(gpioDV_2, 0xc8834400 + (0x2e << 2));
		PR_DBG("[R840]set adc pinmux,gpioDV_2 %x\n", gpioDV_2);
	} else {
		gpiW_2 = demod_read_demod_reg(0xc88344c4);
		gpiW_2 = gpiW_2 | (0x1 << 25);
		gpiW_2 = gpiW_2 & ~(0xd << 24);
		demod_set_demod_reg(gpiW_2, 0xc88344c4);
		PR_DBG("[R840]set adc pinmux,gpiW_2 %x\n", gpiW_2);
	}
	#endif
	/* set adc clk */
	demod_set_adc_core_clk(clk_adc, clk_dem, dvb_mode);
	/* init for dtmb */
	if (dvb_mode == Gxtv_Dtmb) {
		#if 0
		if (is_meson_txhd_cpu()) {
			/* demod_set_demod_reg(0x8, TXLX_DEMOD_REG4);*/
			demod_set_reg_rlt(TXLX_DEMOD_REG4, 0x8);
		} else if (!is_meson_txlx_cpu()) {
			/* open arbit */
			/* demod_set_demod_reg(0x8, DEMOD_TOP_REGC);*/
			demod_set_reg_rlt(GXBB_DEMOD_REG4, 0x8);
			PR_DBG("[open arbit]dtmb\n");
		}
		#else
		if (is_ic_ver(IC_VER_TL1)) {
			demod_write_reg(DEMOD_TOP_REGC, 0x11);
			front_write_reg_v4(0x20,
				((front_read_reg_v4(0x20) & ~0xff)
				| (nco_rate & 0xff)));

			front_write_reg_v4(0x20,
				(front_read_reg_v4(0x20) | (1 << 8)));
		} else {
			demod_write_reg(DEMOD_TOP_REGC, 0x8);
			PR_DBG("[open arbit]dtmb\n");
		}
		#endif
/*	} else if ((dvb_mode == Gxtv_Dvbt_Isdbt) && is_meson_txlx_cpu()) {*/
	} else if ((dvb_mode == Gxtv_Dvbt_Isdbt) && is_ic_ver(IC_VER_TXLX)) {
		#if 0
		/* open arbit */
		/*demod_set_demod_reg(0x8, TXLX_DEMOD_REG4);*/
		demod_set_reg_rlt(TXLX_DEMOD_REG4, 0x8);
		#else
		demod_write_reg(DEMOD_TOP_REGC, 0x8);
		#endif
		PR_DBG("[open arbit]dvbt,txlx\n");
	} else if (is_ic_ver(IC_VER_TL1) && (dvb_mode == Gxtv_Atsc)) {
		demod_write_reg(DEMOD_TOP_REGC, 0x11);
		front_write_reg_v4(0x20, ((front_read_reg_v4(0x20) & ~0xff)
				| (nco_rate & 0xff)));
		front_write_reg_v4(0x20, (front_read_reg_v4(0x20) | (1 << 8)));
	} else if (is_ic_ver(IC_VER_TL1) && (dvb_mode == Gxtv_Dvbc)) {
		nco_rate = ((clk_adc / 1000) * 256) / 250 + 2;
		demod_write_reg(DEMOD_TOP_REGC, 0x11);
		front_write_reg_v4(0x20, ((front_read_reg_v4(0x20) & ~0xff)
				| (nco_rate & 0xff)));
		front_write_reg_v4(0x20, (front_read_reg_v4(0x20) | (1 << 8)));
	}

	demod_sta->adc_freq = clk_adc;
	demod_sta->clk_freq = clk_dem;
	return 0;
}

//#ifdef HIGH_MEM
#if 0
int memorystart = 0x29c00000;//0x35100000;//0x1ef00000;0x9300000 7ca00000
#endif

/*TL1*/
void demod_set_sys_dtmb_v4(void)
{
	#if 0//move to clocks_set_sys_defaults
	int nco_rate;

	nco_rate = (24*256)/224+2;
	//app_apb_write_reg(0xf00*4,0x11 );
	demod_write_reg(DEMOD_TOP_REG0, 0x11);
	//app_apb_write_reg(0xf08*4,0x201);
	demod_write_reg(DEMOD_TOP_REG8, 0x201);
	//app_apb_write_reg(0xf0c*4,0x11);
	demod_write_reg(DEMOD_TOP_REGC, 0x11);
	//app_apb_write_reg(0xe20*4,
			//((app_apb_read_reg(0xe20*4) &~ 0xff)
			//| (nco_rate & 0xff)));

	front_write_reg_v4(0x20, ((front_read_reg_v4(0x20) & ~0xff)
			| (nco_rate & 0xff)));

	//app_apb_write_reg(0xe20*4,  (app_apb_read_reg(0xe20*4) | (1 << 8)));
	front_write_reg_v4(0x20, (front_read_reg_v4(0x20) | (1 << 8)));
	#endif

	//app_apb_write_reg(0x49,memorystart);
	//move to enter_mode()
	//dtmb_write_reg(DTMB_FRONT_MEM_ADDR, memorystart);

	#if 0//move to dtmb_all_reset()
	//app_apb_write_reg(0xe39,	(app_apb_read_reg(0xe39) | (1 << 30)));
	front_write_reg_v4(0x39,	(front_read_reg_v4(0x39) | (1 << 30)));


	//24M
	//app_apb_write_reg(0x25, 0x6aaaaa);
	//app_apb_write_reg(0x3e, 0x13196596);
	//app_apb_write_reg(0x5b, 0x50a30a25);
	dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
	dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);
	dtmb_write_reg(0x5b << 2, 0x50a30a25);
	#endif

	//25m
	//app_apb_write_reg(0x25, 0x62c1a5);
	//app_apb_write_reg(0x3e, 0x131a747d);
	//app_apb_write_reg(0x5b, 0x4d6a0a25);
	//dtmb_write_reg(0x25, 0x62c1a5);
	//dtmb_write_reg(0x3e, 0x131a747d);
	//dtmb_write_reg(0x5b, 0x4d6a0a25);
}

void demod_set_sys_atsc_v4(void)
{
	//int nco_rate;
	#if 0//move to gxtv_demod_atsc_set_frontend()
	union ATSC_DEMOD_REG_0X6A_BITS Val_0x6a;
	union ATSC_EQ_REG_0XA5_BITS Val_0xa5;
	union ATSC_CNTR_REG_0X20_BITS Val_0x20;
	union ATSC_DEMOD_REG_0X54_BITS  Val_0x54;
	union ATSC_DEMOD_REG_0X55_BITS	Val_0x55;
	union ATSC_DEMOD_REG_0X6E_BITS  Val_0x6e;
	#endif

	//PR_INFO("%s\n", __func__);
	//nco_rate = (24*256)/224+2;// 24 | 25
	//hardware_reset();
	//app_apb_write_reg(0xf0c, 0x10);
	//demod_write_reg(DEMOD_TOP_REGC, 0x10);//apb write enable

	#if 0//move to clocks_set_sys_defaults()
	//demod_front_init_cmd();
	//app_apb_write_reg(0xf00*4,0x44 );
	demod_write_reg(DEMOD_TOP_REG0, 0x44);
	//app_apb_write_reg(0xf08*4,0x201);
	demod_write_reg(DEMOD_TOP_REG8, 0x201);
	#endif

	#if 0//move to demod_set_sys()
	//app_apb_write_reg(0xf0c*4,0x11);
	demod_write_reg(DEMOD_TOP_REGC, 0x11);
	//app_apb_write_reg(0xe20*4,((app_apb_read_reg(0xe20*4) &~ 0xff)
			//| (nco_rate & 0xff)));
	front_write_reg_v4(0x20, ((front_read_reg_v4(0x20) & ~0xff)
			| (nco_rate & 0xff)));
	//app_apb_write_reg(0xe20*4,  (app_apb_read_reg(0xe20*4) | (1 << 8)));
	front_write_reg_v4(0x20, (front_read_reg_v4(0x20) | (1 << 8)));
	#endif

	#if 0//move to gxtv_demod_atsc_set_frontend()
	//set_cr_ck_rate();
	//Read Reg
	Val_0x6a.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X6A);
	Val_0xa5.bits = atsc_read_reg_v4(ATSC_EQ_REG_0XA5);

	//24M begin
	Val_0x54.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X54);
	Val_0x55.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X55);
	Val_0x6e.bits = atsc_read_reg_v4(ATSC_DEMOD_REG_0X6E);
	//24M end

	//Set Reg
	Val_0x6a.b.peak_thd = 0x6;//Let CCFO Quality over 6
	Val_0xa5.bits = 0x8c;//increase state 2 to state 3

	//24M begin
	Val_0x54.bits = 0x1aaaaa;//24m 5m if
	Val_0x55.bits = 0x3ae28d;//24m
	Val_0x6e.bits = 0x16e3600;
	//24M end

	//Write Reg
	atsc_write_reg_v4(ATSC_DEMOD_REG_0X6A, Val_0x6a.bits);
	atsc_write_reg_v4(ATSC_EQ_REG_0XA5, Val_0xa5.bits);

	//24M begin
	atsc_write_reg_v4(ATSC_DEMOD_REG_0X54, Val_0x54.bits);
	atsc_write_reg_v4(ATSC_DEMOD_REG_0X55, Val_0x55.bits);
	atsc_write_reg_v4(ATSC_DEMOD_REG_0X6E, Val_0x6e.bits);
	//24M end

	//atsc_reset();
	Val_0x20.bits = atsc_read_reg_v4(ATSC_CNTR_REG_0X20);
	Val_0x20.b.cpu_rst = 1;
	atsc_write_reg_v4(ATSC_CNTR_REG_0X20, Val_0x20.bits);
	Val_0x20.b.cpu_rst = 0;
	atsc_write_reg_v4(ATSC_CNTR_REG_0X20, Val_0x20.bits);
	usleep_range(5000, 5001);
	#endif
}

/*TL1*/
void set_j83b_filter_reg_v4(void)
{
	//j83_1
	qam_write_reg(0x40, 0x36333c0d);
	qam_write_reg(0x41, 0xa110d01);
	qam_write_reg(0x42, 0xf0e4ea7a);
	qam_write_reg(0x43, 0x3c0010);
	qam_write_reg(0x44, 0x7e0065);


	//j83_2
	qam_write_reg(0x45, 0xb3a1905);
	qam_write_reg(0x46, 0x1c396e07);
	qam_write_reg(0x47, 0x3801cc08);
	qam_write_reg(0x48, 0x10800a2);


	qam_write_reg(0x49, 0x53b1f03);
	qam_write_reg(0x4a, 0x18377407);
	qam_write_reg(0x4b, 0x3401cf0b);
	qam_write_reg(0x4c, 0x10d00a1);
}

#if 0
/*TL1*/
void dvbc_reg_initial_tmp_v4(struct aml_demod_dvbc *demod_dvbc)
{
	int clk_freq;
	int adc_freq;
	int ch_mode;
	int agc_mode;
	int ch_freq;
	int ch_if;
	int ch_bw;
	int symb_rate;
	int phs_cfg;
	int afifo_ctr;
	int max_frq_off, tmp;

	agc_mode  = 1;
	ch_if     = 5000;
	ch_bw     = 8000; // kHz
	//==important: Chip or FPGA Combo need to change=======
	clk_freq  = 250000;
	adc_freq  = 24000;
	ch_mode   = demod_dvbc->mode;
	symb_rate = demod_dvbc->symb_rate;
	ch_freq   = demod_dvbc->ch_freq;

	//printf("in dvbc_func, clk_freq is %d, adc_freq is %d\n",
		//clk_freq, adc_freq);
	PR_DBG("in dvbc_func, clk_freq is %d, adc_freq is %d\n",
		clk_freq, adc_freq);

	// disable irq
	//app_apb_write_reg(0xd0, 0);
	qam_write_reg(0x34, 0);

	// reset
	//dvbc_reset();
	//app_apb_write_reg(reg_reset,
		//app_apb_read_reg(reg_reset) & ~(1 << 4)); // disable fsm_en
	qam_write_reg(0x7,
		qam_read_reg(0x7) & ~(1 << 4)); // disable fsm_en
	//app_apb_write_reg(reg_reset,
		//app_apb_read_reg(reg_reset) & ~(1 << 0)); // Sw disable demod
	qam_write_reg(0x7,
		qam_read_reg(0x7) & ~(1 << 0)); // Sw disable demod
	//app_apb_write_reg(reg_reset,
		//app_apb_read_reg(reg_reset) | (1 << 0)); // Sw enable demod
	qam_write_reg(0x7,
		qam_read_reg(0x7) | (1 << 0)); // Sw enable demod


	//app_apb_write_reg(0x000, 0x00000000);  // QAM_STATUS
	qam_write_reg(0x000, 0x00000000);  // QAM_STATUS
	//app_apb_write_reg(reg_reset, 0x00000f00);  // QAM_GCTL0
	qam_write_reg(0x7, 0x00000f00);  // QAM_GCTL0
	//tmp = app_apb_read_reg(0x008);
	tmp = qam_read_reg(0x2);
	tmp = (tmp & 0xfffffff8) | ch_mode;
	//app_apb_write_reg(0x008, tmp);  // qam mode
	qam_write_reg(0x2, tmp);  // qam mode

	switch (ch_mode) {
	case 0: // 16 QAM
		#if 0
		app_apb_write_reg(0x1c4, 0xa2200);
		app_apb_write_reg(0x1c8, 0xc2b0c49);
		app_apb_write_reg(0x1cc, 0x2020000);
		app_apb_write_reg(0x1d4, 0xe9178);
		app_apb_write_reg(0x1d8, 0x1c100);
		app_apb_write_reg(0x1e8, 0x2ab7ff);
		app_apb_write_reg(0x24c, 0x641a180c);
		app_apb_write_reg(0x250, 0xc141400);
		#else
		qam_write_reg(0x71, 0xa2200);
		qam_write_reg(0x72, 0xc2b0c49);
		qam_write_reg(0x73, 0x2020000);
		qam_write_reg(0x75, 0xe9178);
		qam_write_reg(0x76, 0x1c100);
		qam_write_reg(0x7a, 0x2ab7ff);
		qam_write_reg(0x93, 0x641a180c);
		qam_write_reg(0x94, 0xc141400);
		#endif
		break;

	case 1: // 32 QAM
		#if 0
		app_apb_write_reg(0x1c4, 0x61200);
		app_apb_write_reg(0x1c8, 0x99301ae);
		app_apb_write_reg(0x1cc, 0x8080000);
		app_apb_write_reg(0x1d4, 0xbf10c);
		app_apb_write_reg(0x1d8, 0xa05c);
		app_apb_write_reg(0x1dc, 0x1000d6);
		app_apb_write_reg(0x1e8, 0x19a7ff);
		app_apb_write_reg(0x1f0, 0x111222);
		app_apb_write_reg(0x1f4, 0x2020305);
		app_apb_write_reg(0x1f8, 0x3000d0d);
		app_apb_write_reg(0x24c, 0x641f1d0c);
		app_apb_write_reg(0x250, 0xc1a1a00);
		#else
		qam_write_reg(0x71, 0x61200);
		qam_write_reg(0x72, 0x99301ae);
		qam_write_reg(0x73, 0x8080000);
		qam_write_reg(0x75, 0xbf10c);
		qam_write_reg(0x76, 0xa05c);
		qam_write_reg(0x77, 0x1000d6);
		qam_write_reg(0x7a, 0x19a7ff);
		qam_write_reg(0x7c, 0x111222);
		qam_write_reg(0x7d, 0x2020305);
		qam_write_reg(0x7e, 0x3000d0d);
		qam_write_reg(0x93, 0x641f1d0c);
		qam_write_reg(0x94, 0xc1a1a00);
		#endif
		break;

	case 2: // 64 QAM
		//app_apb_write_reg(0x150, 0x606050d);
		//		app_apb_write_reg(0x148, 0x346dc);
		break;

	case 3: // 128 QAM
		#if 0
		app_apb_write_reg(0x1c4, 0x2c200);
		app_apb_write_reg(0x1c8, 0xa6e0059);
		app_apb_write_reg(0x1cc,  0x8080000);
		app_apb_write_reg(0x1d4, 0xa70e9);
		app_apb_write_reg(0x1d8, 0x2013);
		app_apb_write_reg(0x1dc, 0x35068);
		app_apb_write_reg(0x1e0, 0xab100);
		app_apb_write_reg(0x1e8, 0xba7ff);
		app_apb_write_reg(0x1f0, 0x111222);
		app_apb_write_reg(0x1f4, 0x2020305);
		app_apb_write_reg(0x1f8, 0x3000d0d);
		app_apb_write_reg(0x24c, 0x642a240c);
		app_apb_write_reg(0x250, 0xc262600);
		#else
		qam_write_reg(0x71, 0x2c200);
		qam_write_reg(0x72, 0xa6e0059);
		qam_write_reg(0x73,  0x8080000);
		qam_write_reg(0x75, 0xa70e9);
		qam_write_reg(0x76, 0x2013);
		qam_write_reg(0x77, 0x35068);
		qam_write_reg(0x78, 0xab100);
		qam_write_reg(0x7a, 0xba7ff);
		qam_write_reg(0x7c, 0x111222);
		qam_write_reg(0x7d, 0x2020305);
		qam_write_reg(0x7e, 0x3000d0d);
		qam_write_reg(0x93, 0x642a240c);
		qam_write_reg(0x94, 0xc262600);
		#endif
		break;

	case 4: // 256 QAM
		#if 0
		app_apb_write_reg(0x270, 0x2a232100);
		app_apb_write_reg(0x15c, 0x606040d);
		#else
		qam_write_reg(0x9c, 0x2a232100);
		qam_write_reg(0x57, 0x606040d);
		#endif
		break;
	}

	//app_apb_write_reg(0x00c, 0xffff8ffe);  // adc_cnt, symb_cnt
	qam_write_reg(0x3, 0xffff8ffe);

	#if 0//impossible condition
	if (clk_freq == 0)
		afifo_ctr = 0;
	else
	#endif
		afifo_ctr = (adc_freq * 256 / clk_freq) + 2;

	#if 0//impossible condition
	if (afifo_ctr > 255)
		afifo_ctr = 255;
	#endif

	#if 0
	app_apb_write_reg(0x010, (afifo_ctr<<16) | 8000); // afifo, rs_cnt_cfg

	app_apb_write_reg(0xe20*4, afifo_ctr | 0x100);//front_end afifo nco rate
	app_apb_write_reg(0xe2f*4, 0x0);    //front_end sfifo out len
	app_apb_write_reg(0x188, 0x2000); //ffe/fbe taps 88/72

	app_apb_write_reg(0x020, 0x61b53e54);//modified by qiancheng by raymond
	#else
	qam_write_reg(0x4, (afifo_ctr<<16) | 8000); // afifo, rs_cnt_cfg

	//front_end afifo nco rate
	//front_write_reg_v4(0x20, afifo_ctr | 0x100);
	//front_write_reg_v4(0x2f, 0x0);    //front_end sfifo out len
	//dvbc_write_reg(0x188, 0x2000); //ffe/fbe taps 88/72

	qam_write_reg(0x8, 0x61b53e54); //modified by qiancheng by raymond
	#endif

	#if 0//impossible condition
	if (adc_freq == 0) {
		phs_cfg = 0;
	} else {
	#endif
		//  8*fo/fs*2^20 fo=36.125, fs = 28.57114, = 21d775
		phs_cfg = (1<<31) / adc_freq * ch_if / (1<<8);
	//}

	// printk("phs_cfg = %x\n", phs_cfg);
	//app_apb_write_reg(0x024,
		//0x4c000000 | (phs_cfg&0x7fffff));  // PHS_OFFSET, IF offset,
	qam_write_reg(0x9,
		0x4c000000 | (phs_cfg&0x7fffff));  // PHS_OFFSET, IF offset,

	#if 0//impossible condition
	if (adc_freq == 0) {
		max_frq_off = 0;
	} else {
	#endif
		// max_frq_off = (400KHz * 2^29) / (AD=25 * symbol_rate=68750)
		max_frq_off = (1<<29) / symb_rate;
		tmp = 40000000 / adc_freq;
		max_frq_off = tmp * max_frq_off;
	//}

	#if 0
	app_apb_write_reg(0x030, 0x245cf450); //MODIFIED BY QIANCHENG
	app_apb_write_reg(0x034,
		((adc_freq & 0xffff) << 16) | (symb_rate&0xffff));
	#else
	qam_write_reg(0xc, 0x245cf450); //MODIFIED BY QIANCHENG
	qam_write_reg(0xd,
		((adc_freq & 0xffff) << 16) | (symb_rate&0xffff));
	#endif

	//app_apb_write_reg(0x038, 0x00200000);  // TIM_SWEEP_RANGE 16000

	/************* hw state machine config **********/
	//apb_write_reg(0, 0x040, 0x003c); // configure symbol rate step step 0

	// modified 0x44 0x48
	#if 0
	// blind search, configure max symbol_rate      for 7218  fb=3.6M
	app_apb_write_reg(0x044, (symb_rate&0xffff)*256);
	// configure min symbol_rate fb = 6.95M
	app_apb_write_reg(0x048,
		((app_apb_read_reg(0x048)&~(0xff<<8))|3400*256));
	app_apb_write_reg(0x144, (app_apb_read_reg(0x144)&~(0x1<<28)));
	#else
	// blind search, configure max symbol_rate      for 7218  fb=3.6M
	qam_write_reg(0x011, (symb_rate&0xffff)*256);
	// configure min symbol_rate fb = 6.95M
	qam_write_reg(0x12,
		((qam_read_reg(0x12)&~(0xff<<8))|3400*256));
	qam_write_reg(0x51, (qam_read_reg(0x51)&~(0x1<<28)));
	#endif

	#if 0//impossible condition
	if ((agc_mode & 1) == 0)
		// freeze if agc
		//app_apb_write_reg(0x094,
			//app_apb_read_reg(0x94) | (0x1 << 10));
		// freeze if agc
		dvbc_write_reg(0x094, dvbc_read_reg(0x94) | (0x1 << 10));
	#endif

	if ((agc_mode & 2) == 0) // IF control
		qam_write_reg(0x25, qam_read_reg(0x25) | (0x1 << 13));
	//app_apb_write_reg(0x098, 0x9fcc8190);//AGC_IFGAIN_CTRL
	/*
	 * AGC_RFGAIN_CTRL 0x0e020800 by raymond,
	 * if Adjcent channel test, maybe it need change.
	 */
	//app_apb_write_reg(0x0a0, 0x0e03cd11);
	// IMQ, QAM Enable
	//app_apb_write_reg(reg_reset, app_apb_read_reg(reg_reset)|0x33);
	// IMQ, QAM Enable
	qam_write_reg(0x7, qam_read_reg(0x7)|0x33);

	// start hardware machine
	//dvbc_sw_reset(0x004, 4);
	#if 0
	app_apb_write_reg(reg_reset, app_apb_read_reg(reg_reset) | (1 << 4));
	app_apb_write_reg(0x0e8, (app_apb_read_reg(0x0e8)|(1<<2)));

	// clear irq status
	app_apb_read_reg(0xd4);

	// enable irq
	app_apb_write_reg(0xd0, 0x7fff<<3);
	#else
	qam_write_reg(0x7, qam_read_reg(0x7) | (1 << 4));
	qam_write_reg(0x3a, (qam_read_reg(0x3a)|(1<<2)));

	// clear irq status
	qam_read_reg(0x35);

	// enable irq
	qam_write_reg(0x34, 0x7fff<<3);
	#endif
}

/*TL1*/
void set_dvbc_reg_1_v4(void)
{
	//dvbc_1
	#if 0
	app_apb_write_reg(0x100, 0x3d3d0002);
	app_apb_write_reg(0x104, 0x747f0603);
	app_apb_write_reg(0x108, 0xf4130f7a);
	app_apb_write_reg(0x10c, 0x1f801d5);
	app_apb_write_reg(0x110, 0xc60063);
	#else
	dvbc_write_reg(0x100, 0x3d3d0002);
	dvbc_write_reg(0x104, 0x747f0603);
	dvbc_write_reg(0x108, 0xf4130f7a);
	dvbc_write_reg(0x10c, 0x1f801d5);
	dvbc_write_reg(0x110, 0xc60063);
	#endif
}

/*TL1*/
void set_dvbc_reg_2_v4(void)
{
	//dvbc_2
	#if 0
	app_apb_write_reg(0x114, 0xc381b06);
	app_apb_write_reg(0x118, 0x25346b0a);
	app_apb_write_reg(0x11c, 0x3101b90e);
	app_apb_write_reg(0x120, 0x17900e5);
	#else
	dvbc_write_reg(0x114, 0xc381b06);
	dvbc_write_reg(0x118, 0x25346b0a);
	dvbc_write_reg(0x11c, 0x3101b90e);
	dvbc_write_reg(0x120, 0x17900e5);
	#endif
}

/*TL1*/
void set_j83b_reg_1_v4(void)
{
	//j83_1
	#if 0
	app_apb_write_reg(0x100, 0x3e010201);
	app_apb_write_reg(0x104, 0x0b033c3a);
	app_apb_write_reg(0x108, 0xe2ecff0c);
	app_apb_write_reg(0x10c, 0x002a01f6);
	app_apb_write_reg(0x110, 0x0097006a);
	#else
	dvbc_write_reg(0x100, 0x3e010201);
	dvbc_write_reg(0x104, 0x0b033c3a);
	dvbc_write_reg(0x108, 0xe2ecff0c);
	dvbc_write_reg(0x10c, 0x002a01f6);
	dvbc_write_reg(0x110, 0x0097006a);
	#endif
}

/*TL1*/
void set_j83b_reg_2_v4(void)
{
	//j83_2
	#if 0
	app_apb_write_reg(0x114, 0xb3a1905);
	app_apb_write_reg(0x118, 0x1c396e07);
	app_apb_write_reg(0x11c, 0x3801cc08);
	app_apb_write_reg(0x120, 0x10800a2);
	#else
	dvbc_write_reg(0x114, 0xb3a1905);
	dvbc_write_reg(0x118, 0x1c396e07);
	dvbc_write_reg(0x11c, 0x3801cc08);
	dvbc_write_reg(0x120, 0x10800a2);
	#endif
}
#endif

void demod_set_reg(struct aml_demod_reg *demod_reg)
{
	if (fpga_version == 1) {
#if defined DEMOD_FPGA_VERSION
		fpga_write_reg(demod_reg->mode, demod_reg->addr,
				demod_reg->val);
#endif
	} else {
		switch (demod_reg->mode) {
		case AMLOGIC_DVBC_J83B:
			demod_reg->addr = (demod_reg->addr)>>2;
			break;
		case AMLOGIC_DVBT_ISDBT:
			demod_reg->addr = demod_reg->addr * 4 + DVBT_BASE;
			break;
		case AMLOGIC_DTMB:
			demod_reg->addr = gphybase_demod()
				+ DTMB_TOP_ADDR(demod_reg->addr);
			break;
		case AMLOGIC_ATSC:
			/* demod_reg->addr=ATSC_BASE; */
			break;
		case AMLOGIC_DEMOD_CFG:

			demod_reg->addr = demod_reg->addr * 4
				+ gphybase_demod() + DEMOD_CFG_BASE;
			break;
		case AMLOGIC_DEMOD_BASE:
			/* demod_reg->addr = demod_reg->addr + DEMOD_BASE;*/
			/*demod_reg->addr = demod_reg->addr + ddemod_reg_base;*/
			demod_reg->addr = demod_reg->addr + gphybase_demod();
			break;
		case AMLOGIC_DEMOD_FPGA:
			break;
		case AMLOGIC_DEMOD_OTHERS:
			/*not change: demod_reg->addr = demod_reg->addr;*/
			break;
		case AMLOGIC_DEMOD_COLLECT_DATA:
			break;
		}

		if (demod_reg->mode == AMLOGIC_ATSC)
			atsc_write_reg(demod_reg->addr, demod_reg->val);
		else if (demod_reg->mode == AMLOGIC_DEMOD_OTHERS)
			demod_set_cbus_reg(demod_reg->val, demod_reg->addr);
		else if (demod_reg->mode == AMLOGIC_DVBC_J83B)
			qam_write_reg(demod_reg->addr, demod_reg->val);
		else if (demod_reg->mode == AMLOGIC_DEMOD_COLLECT_DATA)
			apb_write_reg_collect(demod_reg->addr, demod_reg->val);
		else
			demod_set_demod_reg(demod_reg->val, demod_reg->addr);

	}
}

void demod_get_reg(struct aml_demod_reg *demod_reg)
{
	if (fpga_version == 1) {
		#if defined DEMOD_FPGA_VERSION
		demod_reg->val = fpga_read_reg(demod_reg->mode,
			demod_reg->addr);
		#endif
	} else {
		if (demod_reg->mode == AMLOGIC_DVBC_J83B)
			demod_reg->addr = (demod_reg->addr)>>2;
		else if (demod_reg->mode == AMLOGIC_DTMB)
			demod_reg->addr = gphybase_demod()
				+ DTMB_TOP_ADDR(demod_reg->addr);
		else if (demod_reg->mode == AMLOGIC_DVBT_ISDBT)
			demod_reg->addr = demod_reg->addr * 4 + DVBT_BASE;
		else if (demod_reg->mode == AMLOGIC_ATSC)
			;
		else if (demod_reg->mode == AMLOGIC_DEMOD_CFG)

			demod_reg->addr = demod_reg->addr * 4
				+ gphybase_demod() + DEMOD_CFG_BASE;
		else if (demod_reg->mode == AMLOGIC_DEMOD_BASE)
			/* demod_reg->addr = demod_reg->addr + DEMOD_BASE;*/
		/* demod_reg->addr = demod_reg->addr + ddemod_reg_base;*/
			demod_reg->addr = demod_reg->addr + gphybase_demod();
		else if (demod_reg->mode == AMLOGIC_DEMOD_FPGA)
			;
		else if (demod_reg->mode == AMLOGIC_DEMOD_OTHERS)
			/*demod_reg->addr = demod_reg->addr;*/
			;
		else if (demod_reg->mode == AMLOGIC_DEMOD_COLLECT_DATA)
			;

		if (demod_reg->mode == AMLOGIC_ATSC)
			demod_reg->val = atsc_read_reg(demod_reg->addr);
		else if (demod_reg->mode == AMLOGIC_DVBC_J83B)
			demod_reg->val = qam_read_reg(demod_reg->addr);
		else if (demod_reg->mode == AMLOGIC_DEMOD_OTHERS)
			demod_reg->val = demod_read_cbus_reg(demod_reg->addr);
		else if (demod_reg->mode == AMLOGIC_DEMOD_COLLECT_DATA)
			demod_reg->val = apb_read_reg_collect(demod_reg->addr);
		else
			demod_reg->val = demod_read_demod_reg(demod_reg->addr);
	}
}

void apb_write_reg_collect(unsigned int addr, unsigned int data)
{
	writel(data, ((void __iomem *)(phys_to_virt(addr))));
/* *(volatile unsigned int*)addr = data; */
}

unsigned long apb_read_reg_collect(unsigned long addr)
{
	unsigned long tmp;

	tmp = readl((void __iomem *)(phys_to_virt(addr)));

	return tmp & 0xffffffff;
}



void apb_write_reg(unsigned int addr, unsigned int data)
{
	demod_set_demod_reg(data, addr);
}

unsigned long apb_read_reg_high(unsigned long addr)
{
	return 0;
}

unsigned long apb_read_reg(unsigned long addr)
{
	return demod_read_demod_reg(addr);
}




int app_apb_read_reg(int addr)
{
	addr = DTMB_TOP_ADDR(addr);
	return (int)demod_read_demod_reg(addr);
}

int app_apb_write_reg(int addr, int data)
{
	addr = DTMB_TOP_ADDR(addr);
	demod_set_demod_reg(data, addr);
	return 0;
}
#if 0
void monitor_isdbt(void)
{
	int SNR;
	int SNR_SP = 500;
	int SNR_TPS = 0;
	int SNR_CP = 0;
	int timeStamp = 0;
	int SFO_residual = 0;
	int SFO_esti = 0;
	int FCFO_esti = 0;
	int FCFO_residual = 0;
	int AGC_Gain = 0;
	int RF_AGC = 0;
	int Signal_power = 0;
	int FECFlag = 0;
	int EQ_seg_ratio = 0;
	int tps_0 = 0;
	int tps_1 = 0;
	int tps_2 = 0;

	int time_stamp;
	int SFO;
	int FCFO;
	int timing_adj;
	int RS_CorrectNum;

	int cnt;
	int tmpAGCGain;

	tmpAGCGain = 0;
	cnt = 0;

/* app_apb_write_reg(0x8, app_apb_read_reg(0x8) & ~(1 << 17));*/
/* // TPS symbol index update : active high */
	time_stamp = app_apb_read_reg(0x07) & 0xffff;
	SNR = app_apb_read_reg(0x0a);
	FECFlag = (app_apb_read_reg(0x00) >> 11) & 0x3;
	SFO = app_apb_read_reg(0x47) & 0xfff;
	SFO_esti = app_apb_read_reg(0x60) & 0xfff;
	FCFO_esti = (app_apb_read_reg(0x60) >> 11) & 0xfff;
	FCFO = (app_apb_read_reg(0x26)) & 0xffffff;
	RF_AGC = app_apb_read_reg(0x0c) & 0x1fff;
	timing_adj = app_apb_read_reg(0x6f) & 0x1fff;
	RS_CorrectNum = app_apb_read_reg(0xc1) & 0xfffff;
	Signal_power = (app_apb_read_reg(0x1b)) & 0x1ff;
	EQ_seg_ratio = app_apb_read_reg(0x6e) & 0x3ffff;
	tps_0 = app_apb_read_reg(0x64);
	tps_1 = app_apb_read_reg(0x65);
	tps_2 = app_apb_read_reg(0x66) & 0xf;

	timeStamp = (time_stamp >> 8) * 68 + (time_stamp & 0x7f);
	SFO_residual = (SFO > 0x7ff) ? (SFO - 0x1000) : SFO;
	FCFO_residual = (FCFO > 0x7fffff) ? (FCFO - 0x1000000) : FCFO;
	/* RF_AGC          = (RF_AGC>0x3ff)? (RF_AGC - 0x800): RF_AGC; */
	FCFO_esti = (FCFO_esti > 0x7ff) ? (FCFO_esti - 0x1000) : FCFO_esti;
	SNR_CP = (SNR) & 0x3ff;
	SNR_TPS = (SNR >> 10) & 0x3ff;
	SNR_SP = (SNR >> 20) & 0x3ff;
	SNR_SP = (SNR_SP > 0x1ff) ? SNR_SP - 0x400 : SNR_SP;
	SNR_TPS = (SNR_TPS > 0x1ff) ? SNR_TPS - 0x400 : SNR_TPS;
	SNR_CP = (SNR_CP > 0x1ff) ? SNR_CP - 0x400 : SNR_CP;
	AGC_Gain = tmpAGCGain >> 4;
	tmpAGCGain = (AGC_Gain > 0x3ff) ? AGC_Gain - 0x800 : AGC_Gain;
	timing_adj = (timing_adj > 0xfff) ? timing_adj - 0x2000 : timing_adj;
	EQ_seg_ratio =
	    (EQ_seg_ratio > 0x1ffff) ? EQ_seg_ratio - 0x40000 : EQ_seg_ratio;

	PR_DBG
	    ("T %4x SP %3d TPS %3d CP %3d EQS %8x RSC %4d",
	     app_apb_read_reg(0xbf)
	     , SNR_SP, SNR_TPS, SNR_CP
/* ,EQ_seg_ratio */
	     , app_apb_read_reg(0x62)
	     , RS_CorrectNum);
	PR_DBG
	    ("SFO %4d FCFO %4d Vit %4x Timing %3d SigP %3x",
	    SFO_residual, FCFO_residual, RF_AGC, timing_adj,
	     Signal_power);
	PR_DBG
	    ("FEC %x RSErr %8x ReSyn %x tps %03x%08x",
	    FECFlag, app_apb_read_reg(0x0b)
	     , (app_apb_read_reg(0xc0) >> 20) & 0xff,
	     app_apb_read_reg(0x05) & 0xfff, app_apb_read_reg(0x04)
	    );
	PR_DBG("\n");
}
#endif
/* new api */
unsigned int demod_reg_get_abs_ddr(unsigned int reg_mode, unsigned int  reg_add)
{
	unsigned int base_add = 0;
	int ret = 0;

	switch (reg_mode) {
	case REG_M_DEMOD:
		/*base_add = ddemod_reg_base;*/
		base_add = gphybase_demodcfg();
		break;
	case REG_M_HIU:
		/*base_add  = dd_hiu_reg_base;*/
		base_add = gphybase_hiu();
		break;
#if 0
	case REG_M_TVAFE:
		base_add = dtvafe_reg_base;
		break;
#endif
	case REG_M_NONE:
	default:
		ret = -1;
		break;
	}

	if (ret < 0)
		return 0xffffffff;
	else
		return base_add + reg_add;
}


/*dvbc_write_reg -> apb_write_reg in dvbc_func*/
/*dvbc_read_reg -> apb_read_reg in dvbc_func*/
#if 0
void dvbc_write_reg(unsigned int addr, unsigned int data)
{
	demod_set_demod_reg(data, ddemod_reg_base + addr);
}
unsigned int dvbc_read_reg(unsigned int addr)
{
	return demod_read_demod_reg(ddemod_reg_base + addr);
}
#else
void dvbc_write_reg(unsigned int addr, unsigned int data)
{
	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_dvbc() + addr);

	mutex_unlock(&mp);
}
unsigned int dvbc_read_reg(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_dvbc() + addr);

	mutex_unlock(&mp);

	return tmp;
}

void dvbc_write_reg_v4(unsigned int addr, unsigned int data)
{

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_dvbc() + (addr << 2));

	mutex_unlock(&mp);
}
unsigned int dvbc_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_dvbc() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;
}

#endif
void demod_write_reg(unsigned int addr, unsigned int data)
{

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	if (is_ic_ver(IC_VER_TL1))
		writel(data, gbase_demod() + (addr << 2));
	else
		writel(data, gbase_demod() + addr);

	mutex_unlock(&mp);
}
unsigned int demod_read_reg(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	if (is_ic_ver(IC_VER_TL1))
		tmp = readl(gbase_demod() + (addr << 2));
	else
		tmp = readl(gbase_demod() + addr);

	mutex_unlock(&mp);

	return tmp;
}

/*TL1*/
void front_write_reg_v4(unsigned int addr, unsigned int data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_front() + (addr << 2));

	mutex_unlock(&mp);
}

unsigned int front_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;

	mutex_lock(&mp);

	tmp = readl(gbase_front() + (addr << 2));

	mutex_unlock(&mp);

	return tmp;

}

void  isdbt_write_reg_v4(unsigned int addr, unsigned int data)
{
	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_isdbt() + addr);

	mutex_unlock(&mp);
}

unsigned int isdbt_read_reg_v4(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_isdbt() + addr);

	mutex_unlock(&mp);

	return tmp;

}

/*dvbc v3:*/
void qam_write_reg(unsigned int reg_addr, unsigned int reg_data)
{
	if (!get_dtvpll_init_flag())
		return;

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(reg_data, gbase_dvbc() + (reg_addr << 2));

	mutex_unlock(&mp);

}

unsigned int qam_read_reg(unsigned int reg_addr)
{

	unsigned int tmp;

	if (!get_dtvpll_init_flag())
		return 0;


	mutex_lock(&mp);

	tmp = readl(gbase_dvbc() + (reg_addr << 2));

	mutex_unlock(&mp);

	return tmp;

}
#if 0
int dd_tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	demod_set_demod_reg(val, dd_hiu_reg_base + reg);

	return 0;
}
unsigned int dd_tvafe_hiu_reg_read(unsigned int addr)
{
	return demod_read_demod_reg(dd_hiu_reg_base + addr);
}
#else
int dd_tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	mutex_lock(&mp);

	writel(val, gbase_iohiu() + reg);

	mutex_unlock(&mp);

	return 0;
}
unsigned int dd_tvafe_hiu_reg_read(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_iohiu() + addr);

	mutex_unlock(&mp);

	return tmp;
}

#endif
int reset_reg_write(unsigned int reg, unsigned int val)
{
	mutex_lock(&mp);

	writel(val, gbase_reset() + reg);

	mutex_unlock(&mp);

	return 0;
}
unsigned int reset_reg_read(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_reset() + addr);

	mutex_unlock(&mp);

	return tmp;
}

#if 0
int dd_tvafe_reg_write(unsigned int reg, unsigned int val)
{
	demod_set_demod_reg(val, dtvafe_reg_base + reg);

	return 0;
}
unsigned int dd_tvafe_reg_read(unsigned int addr)
{
	return demod_read_demod_reg(dtvafe_reg_base + addr);
}
#endif

void demod_set_demod_default(void)
{
	if (is_ic_ver(IC_VER_TL1))
		return;

	#if 0
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu()) {
		demod_set_reg_rlt(GXBB_DEMOD_REG1, DEMOD_REG0_VALUE);
		demod_set_reg_rlt(GXBB_DEMOD_REG2, DEMOD_REG4_VALUE);
		demod_set_reg_rlt(GXBB_DEMOD_REG3, DEMOD_REG8_VALUE);

	} else if (is_meson_txlx_cpu() || is_meson_gxlx_cpu()
		|| is_meson_txhd_cpu()) {
		demod_set_reg_rlt(TXLX_DEMOD_REG1, DEMOD_REG0_VALUE);
		demod_set_reg_rlt(TXLX_DEMOD_REG2, DEMOD_REG4_VALUE);
		demod_set_reg_rlt(TXLX_DEMOD_REG3, DEMOD_REG8_VALUE);

	}
	#else
	demod_write_reg(DEMOD_TOP_REG0, DEMOD_REG0_VALUE);
	demod_write_reg(DEMOD_TOP_REG4, DEMOD_REG4_VALUE);
	demod_write_reg(DEMOD_TOP_REG8, DEMOD_REG8_VALUE);
	#endif

}


void debug_check_reg_val(unsigned int reg_mode, unsigned int reg)
{
	unsigned int regAddr;
	unsigned int val;
#if 0
	regAddr = demod_reg_get_abs_ddr(reg_mode, reg);

	if (regAddr == 0xffffffff) {
		PR_DBG("[reg=%x][mode=%d] is onthing!\n", reg, reg_mode);
		return;
	}
	val = demod_read_demod_reg(regAddr);
#endif
	switch (reg_mode) {
	case REG_M_DEMOD:
		val = demod_read_reg(reg);
		regAddr = gphybase_demodcfg() + reg;
		PR_DBG("[demod][%x]%x\n", regAddr, val);
		break;
#if 0
	case REG_M_TVAFE:
		regAddr = demod_reg_get_abs_ddr(reg_mode, reg);
		val = demod_read_demod_reg(regAddr);
		PR_DBG("[tafe][%x]%x\n", regAddr, val);
		break;
#endif
	case REG_M_HIU:
		val = dd_tvafe_hiu_reg_read(reg);
		regAddr = gphybase_hiu() + reg;
		PR_DBG("[adc][%x]%x\n", regAddr, val);
		break;
	default:
		PR_DBG("[reg=%x][mode=%d] is onthing!\n", reg, reg_mode);
		break;
	}
}

const unsigned int adc_check_tab_hiu[] = {
	REG_M_HIU, D_HHI_ADC_PLL_CNTL,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL2,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL4,
	REG_M_HIU, D_HHI_HDMI_CLK_CNTL,
	REG_M_HIU, D_HHI_DEMOD_CLK_CNTL,
	REG_M_HIU, D_HHI_DADC_CNTL,
	REG_M_HIU, D_HHI_DADC_CNTL2,
	REG_M_HIU, D_HHI_DADC_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL1,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL5,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL6,
	REG_M_HIU, D_HHI_VDAC_CNTL0,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,

};
const unsigned int adc_check_tab_gxtvbb[] = {
	REG_M_HIU, D_HHI_ADC_PLL_CNTL,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL2,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL3,
	REG_M_HIU, D_HHI_ADC_PLL_CNTL4,
/*	REG_M_HIU, D_HHI_HDMI_CLK_CNTL, */

	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,

};

const unsigned int adc_check_tab_demod_txlx[] = {
	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,
};
const unsigned int adc_check_tab_demod_gxbb[] = {
	REG_M_DEMOD, DEMOD_TOP_REG0,
	REG_M_DEMOD, DEMOD_TOP_REG4,
	REG_M_DEMOD, DEMOD_TOP_REG8,
	REG_M_DEMOD, DEMOD_TOP_REGC,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,
};
void debug_check_reg_table(const unsigned int *pTab)
{

	unsigned int cnt = 0;
	unsigned int add;
	unsigned int reg_mode;

	unsigned int pretect = 0;

	reg_mode = pTab[cnt++];
	add = pTab[cnt++];

	while ((reg_mode != TABLE_FLG_END) && (pretect < 100)) {
		debug_check_reg_val(reg_mode, add);
		reg_mode = pTab[cnt++];
		add = pTab[cnt++];
		pretect++;
	}

}
void debug_adc_pll(void)
{
	if (is_ic_ver(IC_VER_TXL)) {
		debug_check_reg_table(&adc_check_tab_hiu[0]);
		debug_check_reg_table(&adc_check_tab_demod_gxbb[0]);

	} else if (is_ic_ver(IC_VER_TXLX) || is_ic_ver(IC_VER_GXLX)
		|| is_ic_ver(IC_VER_TXHD)) {
		debug_check_reg_table(&adc_check_tab_hiu[0]);
		debug_check_reg_table(&adc_check_tab_demod_txlx[0]);

	} else {
		/* gxtvbb */
		debug_check_reg_table(&adc_check_tab_gxtvbb[0]);
	}

}


