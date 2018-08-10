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
		PR_DBG("[demod][%x]%x\n", DEMOD_REG1,
				demod_read_demod_reg(DEMOD_REG1));
		PR_DBG("[demod][%x]%x\n", DEMOD_REG2,
				demod_read_demod_reg(DEMOD_REG2));
		PR_DBG("[demod][%x]%x\n", DEMOD_REG3,
				demod_read_demod_reg(DEMOD_REG3));
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
		PR_DBG("[demod][%x]%x\n", DEMOD_REG1,
				demod_read_demod_reg(DEMOD_REG1));
		PR_DBG("[demod][%x]%x\n", DEMOD_REG2,
				demod_read_demod_reg(DEMOD_REG2));
		PR_DBG("[demod][%x]%x\n", DEMOD_REG3,
				demod_read_demod_reg(DEMOD_REG3));
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
/* printk("[demod][read]%x,data is %x\n",(addr),tmp); */
	return tmp;
}

void power_sw_hiu_reg(int on)
{
	if (on == PWR_ON) {
		dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
		(dd_tvafe_hiu_reg_read(HHI_DEMOD_MEM_PD_REG) & (~0x2fff)));
	} else {
		dd_tvafe_hiu_reg_write(HHI_DEMOD_MEM_PD_REG,
		(dd_tvafe_hiu_reg_read(HHI_DEMOD_MEM_PD_REG) | 0x2fff));

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
	} else if (dvb_mode == Gxtv_Dvbt_Isdbt) {
		cfg0.b.ts_sel = 1<<1;
		cfg0.b.mode = 1<<1;
		cfg0.b.adc_format = 0;
		cfg0.b.adc_regout = 0;
	} else if (dvb_mode == Gxtv_Atsc) {
		cfg0.b.ts_sel = 1<<2;
		cfg0.b.mode = 1<<2;
		cfg0.b.adc_format = 0;
		cfg0.b.adc_regout = 1;
		cfg0.b.adc_regadj = 2;
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

	demod_write_reg(DEMOD_REG1, cfg0.d32);

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

	demod_write_reg(DEMOD_REG3, cfg2.d32);
	PR_DBG("dvb_mode is %d\n", dvb_mode);
	debug_check_reg_val(REG_M_DEMOD, DEMOD_REG1);

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

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_dvbt() + addr);

	mutex_unlock(&mp);
}

unsigned int dvbt_read_reg(unsigned int addr)
{
	unsigned int tmp;

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




int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_sys *demod_sys)
{
/* int adc_clk; */
/* demod_sta->tmp=Adc_mode; */
	unsigned char dvb_mode;
	int clk_adc, clk_dem;
	/* int gpioDV_2;*/
	/*int gpiW_2;*/

	dvb_mode = demod_sta->dvb_mode;
	clk_adc = demod_sys->adc_clk;
	clk_dem = demod_sys->demod_clk;
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
			/* demod_set_demod_reg(0x8, DEMOD_REG4);*/
			demod_set_reg_rlt(GXBB_DEMOD_REG4, 0x8);
			PR_DBG("[open arbit]dtmb\n");
		}
		#else
		demod_write_reg(DEMOD_REG4, 0x8);
		PR_DBG("[open arbit]dtmb\n");
		#endif
/*	} else if ((dvb_mode == Gxtv_Dvbt_Isdbt) && is_meson_txlx_cpu()) {*/
	} else if ((dvb_mode == Gxtv_Dvbt_Isdbt) && is_ic_ver(IC_VER_TXLX)) {
		#if 0
		/* open arbit */
		/*demod_set_demod_reg(0x8, TXLX_DEMOD_REG4);*/
		demod_set_reg_rlt(TXLX_DEMOD_REG4, 0x8);
		#else
		demod_write_reg(DEMOD_REG4, 0x8);
		#endif
		PR_DBG("[open arbit]dvbt,txlx\n");
	}
	demod_sta->adc_freq = clk_adc;
	demod_sta->clk_freq = clk_dem;
	return 0;
}

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

#endif
void demod_write_reg(unsigned int addr, unsigned int data)
{

	mutex_lock(&mp);
	/* printk("[demod][write]%x,data is %x\n",(addr),data);*/

	writel(data, gbase_demod() + addr);

	mutex_unlock(&mp);
}
unsigned int demod_read_reg(unsigned int addr)
{
	unsigned int tmp;

	mutex_lock(&mp);

	tmp = readl(gbase_demod() + addr);

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
	#if 0
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu()) {
		demod_set_reg_rlt(GXBB_DEMOD_REG1, DEMOD_REG1_VALUE);
		demod_set_reg_rlt(GXBB_DEMOD_REG2, DEMOD_REG2_VALUE);
		demod_set_reg_rlt(GXBB_DEMOD_REG3, DEMOD_REG3_VALUE);

	} else if (is_meson_txlx_cpu() || is_meson_gxlx_cpu()
		|| is_meson_txhd_cpu()) {
		demod_set_reg_rlt(TXLX_DEMOD_REG1, DEMOD_REG1_VALUE);
		demod_set_reg_rlt(TXLX_DEMOD_REG2, DEMOD_REG2_VALUE);
		demod_set_reg_rlt(TXLX_DEMOD_REG3, DEMOD_REG3_VALUE);

	}
	#else
	demod_write_reg(DEMOD_REG1, DEMOD_REG1_VALUE);
	demod_write_reg(DEMOD_REG2, DEMOD_REG2_VALUE);
	demod_write_reg(DEMOD_REG3, DEMOD_REG3_VALUE);

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

	REG_M_DEMOD, DEMOD_REG1,
	REG_M_DEMOD, DEMOD_REG2,
	REG_M_DEMOD, DEMOD_REG3,
	REG_M_DEMOD, DEMOD_REG4,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,

};

const unsigned int adc_check_tab_demod_txlx[] = {
	REG_M_DEMOD, DEMOD_REG1,
	REG_M_DEMOD, DEMOD_REG2,
	REG_M_DEMOD, DEMOD_REG3,
	REG_M_DEMOD, DEMOD_REG4,
	/* end */
	TABLE_FLG_END, TABLE_FLG_END,
};
const unsigned int adc_check_tab_demod_gxbb[] = {
	REG_M_DEMOD, DEMOD_REG1,
	REG_M_DEMOD, DEMOD_REG2,
	REG_M_DEMOD, DEMOD_REG3,
	REG_M_DEMOD, DEMOD_REG4,
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


