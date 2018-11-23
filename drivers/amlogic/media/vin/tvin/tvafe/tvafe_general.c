/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.c
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

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
/*#include <mach/am_regs.h>*/

/*#include <mach/am_regs.h>*/

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_debug.h"
#include "tvafe_general.h"
/***************************Local defines**************************/
/* calibration validation function enable/disable */
/* #define TVAFE_ADC_CAL_VALIDATION */

/* edid config reg value */
#define TVAFE_EDID_CONFIG	0x03804050/* 0x03800050 */
#define HHI_ATV_DMD_SYS_CLK_CNTL	0xf3
#define VAFE_CLK_EN				23
#define VAFE_CLK_EN_WIDTH		1
#define VAFE_CLK_SELECT			24
#define VAFE_CLK_SELECT_WIDTH	2



static unsigned int adc_pll_chg;

/* TOP */
static const unsigned int cvbs_top_reg_default[][2] = {
	{TVFE_DVSS_MUXCTRL, 0x07000008,},
	{TVFE_DVSS_MUXVS_REF, 0x00000000,},
	{TVFE_DVSS_MUXCOAST_V, 0x00000000,},
	{TVFE_DVSS_SEP_HVWIDTH, 0x00000000,},
	{TVFE_DVSS_SEP_HPARA, 0x00000000,},
	{TVFE_DVSS_SEP_VINTEG, 0x00000000,},
	{TVFE_DVSS_SEP_H_THR, 0x00000000,},
	{TVFE_DVSS_SEP_CTRL, 0x00000000,},
	{TVFE_DVSS_GEN_WIDTH, 0x00000000,},
	{TVFE_DVSS_GEN_PRD, 0x00000000,},
	{TVFE_DVSS_GEN_COAST, 0x00000000,},
	{TVFE_DVSS_NOSIG_PARA, 0x00000000,},
	{TVFE_DVSS_NOSIG_PLS_TH, 0x00000000,},
	/* TVFE_DVSS_NOSIG_PLS_TH */
	{TVFE_DVSS_GATE_H, 0x00000000,},
	{TVFE_DVSS_GATE_V, 0x00000000,},
	{TVFE_DVSS_INDICATOR1, 0x00000000,},
	{TVFE_DVSS_INDICATOR2, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL1, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL2, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL3, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL4, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL5, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL6, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL7, 0x00000000,},
	{TVFE_SYNCTOP_SPOL_MUXCTRL, 0x00000009,},
	/* TVFE_SYNCTOP_SPOL_MUXCTRL */
	{TVFE_SYNCTOP_INDICATOR1_HCNT, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR1_HCNT */
	{TVFE_SYNCTOP_INDICATOR2_VCNT, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR2_VCNT */
	{TVFE_SYNCTOP_INDICATOR3, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR3 */
	{TVFE_SYNCTOP_SFG_MUXCTRL1, 0x00000000,},
	/* TVFE_SYNCTOP_SFG_MUXCTRL1 */
	{TVFE_SYNCTOP_SFG_MUXCTRL2, 0x00330000,},
	/* TVFE_SYNCTOP_SFG_MUXCTRL2 */
	{TVFE_SYNCTOP_INDICATOR4, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR4 */
	{TVFE_SYNCTOP_SAM_MUXCTRL, 0x00082001,},
	/* TVFE_SYNCTOP_SAM_MUXCTRL */
	{TVFE_MISC_WSS1_MUXCTRL1, 0x00000000,},
	/* TVFE_MISC_WSS1_MUXCTRL1 */
	{TVFE_MISC_WSS1_MUXCTRL2, 0x00000000,},
	/* TVFE_MISC_WSS1_MUXCTRL2 */
	{TVFE_MISC_WSS2_MUXCTRL1, 0x00000000,},
	/* TVFE_MISC_WSS2_MUXCTRL1 */
	{TVFE_MISC_WSS2_MUXCTRL2, 0x00000000,},
	/* TVFE_MISC_WSS2_MUXCTRL2 */
	{TVFE_MISC_WSS1_INDICATOR1, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR1 */
	{TVFE_MISC_WSS1_INDICATOR2, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR2 */
	{TVFE_MISC_WSS1_INDICATOR3, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR3 */
	{TVFE_MISC_WSS1_INDICATOR4, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR4 */
	{TVFE_MISC_WSS1_INDICATOR5, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR5 */
	{TVFE_MISC_WSS2_INDICATOR1, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR1 */
	{TVFE_MISC_WSS2_INDICATOR2, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR2 */
	{TVFE_MISC_WSS2_INDICATOR3, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR3 */
	{TVFE_MISC_WSS2_INDICATOR4, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR4 */
	{TVFE_MISC_WSS2_INDICATOR5, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR5 */
	{TVFE_AP_MUXCTRL1, 0x00000000,},
	{TVFE_AP_MUXCTRL2, 0x00000000,},
	{TVFE_AP_MUXCTRL3, 0x00000000,},
	{TVFE_AP_MUXCTRL4, 0x00000000,},
	{TVFE_AP_MUXCTRL5, 0x00000000,},
	{TVFE_AP_INDICATOR1, 0x00000000,},
	{TVFE_AP_INDICATOR2, 0x00000000,},
	{TVFE_AP_INDICATOR3, 0x00000000,},
	{TVFE_AP_INDICATOR4, 0x00000000,},
	{TVFE_AP_INDICATOR5, 0x00000000,},
	{TVFE_AP_INDICATOR6, 0x00000000,},
	{TVFE_AP_INDICATOR7, 0x00000000,},
	{TVFE_AP_INDICATOR8, 0x00000000,},
	{TVFE_AP_INDICATOR9, 0x00000000,},
	{TVFE_AP_INDICATOR10, 0x00000000,},
	{TVFE_AP_INDICATOR11, 0x00000000,},
	{TVFE_AP_INDICATOR12, 0x00000000,},
	{TVFE_AP_INDICATOR13, 0x00000000,},
	{TVFE_AP_INDICATOR14, 0x00000000,},
	{TVFE_AP_INDICATOR15, 0x00000000,},
	{TVFE_AP_INDICATOR16, 0x00000000,},
	{TVFE_AP_INDICATOR17, 0x00000000,},
	{TVFE_AP_INDICATOR18, 0x00000000,},
	{TVFE_AP_INDICATOR19, 0x00000000,},
	{TVFE_BD_MUXCTRL1, 0x00000000,},
	{TVFE_BD_MUXCTRL2, 0x00000000,},
	{TVFE_BD_MUXCTRL3, 0x00000000,},
	{TVFE_BD_MUXCTRL4, 0x00000000,},
	{TVFE_CLP_MUXCTRL1, 0x00000000,},
	{TVFE_CLP_MUXCTRL2, 0x00000000,},
	{TVFE_CLP_MUXCTRL3, 0x00000000,},
	{TVFE_CLP_MUXCTRL4, 0x00000000,},
	{TVFE_CLP_INDICATOR1, 0x00000000,},
	{TVFE_BPG_BACKP_H, 0x00000000,},
	{TVFE_BPG_BACKP_V, 0x00000000,},
	{TVFE_DEG_H, 0x00000000,},
	{TVFE_DEG_VODD, 0x00000000,},
	{TVFE_DEG_VEVEN, 0x00000000,},
	{TVFE_OGO_OFFSET1, 0x00000000,},
	{TVFE_OGO_GAIN1, 0x00000000,},
	{TVFE_OGO_GAIN2, 0x00000000,},
	{TVFE_OGO_OFFSET2, 0x00000000,},
	{TVFE_OGO_OFFSET3, 0x00000000,},
	{TVFE_VAFE_CTRL, 0x00000000,},
	{TVFE_VAFE_STATUS, 0x00000000,},
#ifdef CRYSTAL_25M
	{TVFE_TOP_CTRL, 0x30c4e6c
	/*0xc4f64 0x00004B60*/,},
	/* TVFE_TOP_CTRL */
#else/* 24M */
	{TVFE_TOP_CTRL, 0x30c4f64
	/*0xc4f64 0x00004B60*/,},
	/* TVFE_TOP_CTRL */
#endif
	/*enable in tvafe_dec_open or avdetect avplug in*/
	{TVFE_CLAMP_INTF, 0x00000666,},
	{TVFE_RST_CTRL, 0x00000000,},
	{TVFE_EXT_VIDEO_AFE_CTRL_MUX1, 0x00000000,},
	/* TVFE_EXT_VIDEO_AFE_CTRL_MUX1 */
	{TVFE_EDID_CONFIG, TVAFE_EDID_CONFIG,},
	/* TVFE_EDID_CONFIG */
	{TVFE_EDID_RAM_ADDR, 0x00000000,},
	{TVFE_EDID_RAM_WDATA, 0x00000000,},
	{TVFE_EDID_RAM_RDATA, 0x00000000,},
	{TVFE_APB_ERR_CTRL_MUX1, 0x8fff8fff,},
	/* TVFE_APB_ERR_CTRL_MUX1 */
	{TVFE_APB_ERR_CTRL_MUX2, 0x00008fff,},
	/* TVFE_APB_ERR_CTRL_MUX2 */
	{TVFE_APB_INDICATOR1, 0x00000000,},
	{TVFE_APB_INDICATOR2, 0x00000000,},
	{TVFE_ADC_READBACK_CTRL, 0x80140003,},
	/* TVFE_ADC_READBACK_CTRL */
	{TVFE_ADC_READBACK_INDICATOR, 0x00000000,},
	/* TVFE_ADC_READBACK_INDICATOR */
	{TVFE_INT_CLR, 0x00000000,},
	{TVFE_INT_MSKN, 0x00000000,},
	{TVFE_INT_INDICATOR1, 0x00000000,},
	{TVFE_INT_SET, 0x00000000,},
	/* {TVFE_CHIP_VERSION                      ,0x00000000,}, */
	/* TVFE_CHIP_VERSION */
	{TVFE_FREERUN_GEN_WIDTH, 0x00000000,},/* TVFE_FREERUN_GEN_WIDTH */
	{TVFE_FREERUN_GEN_PRD,  0x00000000,},/* TVFE_FREERUN_GEN_PRD */
	{TVFE_FREERUN_GEN_COAST, 0x00000000,},/* TVFE_FREERUN_GEN_COAST */
	{TVFE_FREERUN_GEN_CTRL, 0x00000000,},/* TVFE_FREERUN_GEN_CTRL */

	{TVFE_AAFILTER_CTRL1,   0x00100000,},
	/* TVFE_AAFILTER_CTRL1 bypass all */
	{TVFE_AAFILTER_CTRL2,   0x00000000,},
	/* TVFE_AAFILTER_CTRL2 */
	{TVFE_AAFILTER_CTRL3,   0x00000000,},
	/* TVFE_AAFILTER_CTRL3 */
	{TVFE_AAFILTER_CTRL4,   0x00000000,},
	/* TVFE_AAFILTER_CTRL4 */
	{TVFE_AAFILTER_CTRL5,   0x00000000,},
	/* TVFE_AAFILTER_CTRL5 */

	{TVFE_SOG_MON_CTRL1,   0x00000000,},/* TVFE_SOG_MON_CTRL1 */
	{TVFE_ADC_READBACK_CTRL1,   0x00000000,},/* TVFE_ADC_READBACK_CTRL1 */
	{TVFE_ADC_READBACK_CTRL2,   0x00000000,},/* TVFE_ADC_READBACK_CTRL2 */
#ifdef CRYSTAL_25M
	{TVFE_AFC_CTRL1,	 0x85730459,},/* TVFE_AFC_CTRL1 */
	{TVFE_AFC_CTRL2,	 0x342fa9ed,},/* TVFE_AFC_CTRL2 */
	{TVFE_AFC_CTRL3,	 0x2a02396,},/* TVFE_AFC_CTRL3 */
	{TVFE_AFC_CTRL4,	 0xfefbff14,},/* TVFE_AFC_CTRL4 */
	{TVFE_AFC_CTRL5,		0x0,},/* TVFE_AFC_CTRL5 */
#else/* for 24M */
	{TVFE_AFC_CTRL1,   0x893904d2,},
	/* TVFE_AFC_CTRL1 */
	{TVFE_AFC_CTRL2,   0xf4b9ac9,},
	/* TVFE_AFC_CTRL2 */
	{TVFE_AFC_CTRL3,   0x1fd8c36,},
	/* TVFE_AFC_CTRL3 */
	{TVFE_AFC_CTRL4,   0x2de6d04f,},
	/* TVFE_AFC_CTRL4 */
	{TVFE_AFC_CTRL5,          0x4,},
	/* TVFE_AFC_CTRL5 */
#endif
	{0xFFFFFFFF, 0x00000000,}
};

/*
 * tvafe cvd2 video poaition reg setting
 */
static enum tvafe_adc_ch_e tvafe_adc_pin_muxing(
					enum tvafe_adc_pin_e pin)
{
	enum tvafe_adc_ch_e ret = TVAFE_ADC_CH_NULL;

	if (tvafe_cpu_type() == CPU_TYPE_TXL ||
		tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		tvafe_pr_info("[tvafe]%s:pin:%d\n",
			__func__, (unsigned int)pin);
		if (pin == TVAFE_CVBS_IN0) {

			W_APB_BIT(TVFE_VAFE_CTRL1, 1,
				VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
			W_APB_BIT(TVFE_VAFE_CTRL2, 3, 4, 3);
			ret = TVAFE_ADC_CH_0;

		} else if (pin == TVAFE_CVBS_IN1) {

			W_APB_BIT(TVFE_VAFE_CTRL1, 2,
				VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
			W_APB_BIT(TVFE_VAFE_CTRL2, 5, 4, 3);
			ret = TVAFE_ADC_CH_1;

		} else if (pin == TVAFE_CVBS_IN2) {

			W_APB_BIT(TVFE_VAFE_CTRL1, 3,
				VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
			W_APB_BIT(TVFE_VAFE_CTRL2, 6, 4, 3);
			ret = TVAFE_ADC_CH_2;

		} else if (pin == TVAFE_CVBS_IN3) {

			/* atv demod data for cvd2 */
			W_APB_REG(TVFE_ATV_DMD_CLP_CTRL, 0x1300010);
			W_APB_BIT(TVFE_VAFE_CTRL2, 6, 4, 3);
			ret = TVAFE_ADC_CH_3;
		}

	}
	return ret;
}

/*
 * tvafe pin mux setting for input source
 */
int tvafe_set_source_muxing(enum tvin_port_e port,
		struct tvafe_pin_mux_s *pinmux)
{
	int ret = 0;

	switch (port) {

	case TVIN_PORT_CVBS0:
		tvafe_adc_pin_muxing(pinmux->pin[CVBS_IN0]);
		break;
	case TVIN_PORT_CVBS1:
		tvafe_adc_pin_muxing(pinmux->pin[CVBS_IN1]);
		break;
	case TVIN_PORT_CVBS2:
		tvafe_adc_pin_muxing(pinmux->pin[CVBS_IN2]);
		break;
	case TVIN_PORT_CVBS3:
		tvafe_adc_pin_muxing(pinmux->pin[CVBS_IN3]);
		break;
	default:
		ret = -EFAULT;
		break;
	}
	if (ret == 0)
		tvafe_pr_info("%s set pin mux to port:0x%x ok.\n",
		__func__, port);
	else
		tvafe_pr_info("%s set pin mux error!!!!!.\n",
		__func__);

	return ret;
}

void tvafe_set_regmap(struct am_regs_s *p)
{
	unsigned short i;

for (i = 0; i < p->length; i++) {
	switch (p->am_reg[i].type) {
	case REG_TYPE_PHY:
		#ifdef PQ_DEBUG_EN
		    tvafe_pr_info("%s: bus type: phy..\n", __func__);
		#endif
		break;
	case REG_TYPE_CBUS:
		if (p->am_reg[i].mask == 0xffffffff)
			aml_write_cbus(p->am_reg[i].addr, p->am_reg[i].val);
		else
			aml_write_cbus(p->am_reg[i].addr,
			(aml_read_cbus(p->am_reg[i].addr) &
			(~(p->am_reg[i].mask))) |
			(p->am_reg[i].val & p->am_reg[i].mask));
		#ifdef PQ_DEBUG_EN
					tvafe_pr_info("%s: cbus: Reg0x%x(%u)=0x%x(%u)val=%x(%u)mask=%x(%u)\n",
				__func__, p->am_reg[i].addr, p->am_reg[i].addr,
					(p->am_reg[i].val & p->am_reg[i].mask),
					(p->am_reg[i].val & p->am_reg[i].mask),
					p->am_reg[i].val, p->am_reg[i].val,
					p->am_reg[i].mask, p->am_reg[i].mask);
		#endif
		break;
	case REG_TYPE_APB:
		if (p->am_reg[i].mask == 0xffffffff)
			W_APB_REG(p->am_reg[i].addr<<2, p->am_reg[i].val);
		else
			W_APB_REG(p->am_reg[i].addr<<2,
			(R_APB_REG(p->am_reg[i].addr<<2) &
			(~(p->am_reg[i].mask))) |
			(p->am_reg[i].val & p->am_reg[i].mask));
		#ifdef PQ_DEBUG_EN
					tvafe_pr_info("%s: apb: Reg0x%x(%u)=0x%x(%u)val=%x(%u)mask=%x(%u)\n",
				__func__, p->am_reg[i].addr, p->am_reg[i].addr,
					(p->am_reg[i].val & p->am_reg[i].mask),
					(p->am_reg[i].val & p->am_reg[i].mask),
					p->am_reg[i].val, p->am_reg[i].val,
					p->am_reg[i].mask, p->am_reg[i].mask);
		#endif
		break;
	default:
	    #ifdef PQ_DEBUG_EN
		tvafe_pr_info("%s: bus type error!!!bustype = 0x%x................\n",
				__func__, p->am_reg[i].type);
	    #endif
		break;
		}
	}
}

/*
 * tvafe init cvbs setting with pal-i
 */
static void tvafe_set_cvbs_default(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, enum tvin_port_e port,
				struct tvafe_pin_mux_s *mux)
{
	unsigned int i = 0;

	/**disable auto mode clock**/
	if (tvafe_cpu_type() != CPU_TYPE_TL1)
		W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);

	/*config adc*/
	if (port == TVIN_PORT_CVBS3) {
		if (tvafe_cpu_type() == CPU_TYPE_TXL ||
			tvafe_cpu_type() == CPU_TYPE_TXLX) {
			/** DADC CNTL for LIF signal input **/
			W_HIU_REG(HHI_DADC_CNTL, 0x00102038);
			W_HIU_REG(HHI_DADC_CNTL2, 0x00000406);
			W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);
		} else if (tvafe_cpu_type() == CPU_TYPE_TXHD) {
			/** DADC CNTL for LIF signal input **/
			W_HIU_REG(HHI_DADC_CNTL, 0x00102038);
			W_HIU_REG(HHI_DADC_CNTL2, 0x00000401);
			W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);
		} else if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			/** DADC CNTL for LIF signal input **/
			W_HIU_REG(HHI_DADC_CNTL, 0x0030303c);
			W_HIU_REG(HHI_DADC_CNTL2, 0x00003480);
			W_HIU_REG(HHI_DADC_CNTL3, 0x08300b83);
		} else {
			/** DADC CNTL for LIF signal input **/
			W_HIU_REG(HHI_DADC_CNTL, 0x1411036);
			W_HIU_REG(HHI_DADC_CNTL2, 0x0);
			W_HIU_REG(HHI_DADC_CNTL3, 0x430036);
			W_HIU_REG(HHI_DADC_CNTL4, 0x80600240);
		}
	} else {
		if (tvafe_cpu_type() == CPU_TYPE_TXL ||
			tvafe_cpu_type() == CPU_TYPE_TXLX) {
			W_HIU_REG(HHI_CADC_CNTL, 0x02000A08);
			W_HIU_REG(HHI_CADC_CNTL2, 0x04007B05);
		} else if (tvafe_cpu_type() == CPU_TYPE_TXHD) {
			W_HIU_REG(HHI_DADC_CNTL, 0x00102038);
			W_HIU_REG(HHI_DADC_CNTL2, 0x00000400);
			W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);
		} else if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			W_HIU_REG(HHI_DADC_CNTL, 0x0030303c);
			W_HIU_REG(HHI_DADC_CNTL2, 0x00003400);
			W_HIU_REG(HHI_DADC_CNTL3, 0x08300b83);
		}
	}
	/** enable tv_decoder mem clk **/
	W_HIU_BIT(HHI_VPU_CLK_CNTL, 1, 28, 1);

	/** write top register **/
	i = 0;
	while (cvbs_top_reg_default[i][0] != 0xFFFFFFFF) {
		W_APB_REG(cvbs_top_reg_default[i][0],
			cvbs_top_reg_default[i][1]);
		i++;
	}
	if (tvafe_cpu_type() == CPU_TYPE_TXL ||
		tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			if (port == TVIN_PORT_CVBS3) {
				W_APB_REG(TVFE_VAFE_CTRL0, 0x000d0710);
				W_APB_REG(TVFE_VAFE_CTRL1, 0x00003000);
				W_APB_REG(TVFE_VAFE_CTRL2, 0x1fe09e31);
			} else if ((port == TVIN_PORT_CVBS1) ||
					(port == TVIN_PORT_CVBS2)) {
				W_APB_REG(TVFE_VAFE_CTRL0, 0x00490710);
				W_APB_REG(TVFE_VAFE_CTRL1, 0x0000110e);
				W_APB_REG(TVFE_VAFE_CTRL2, 0x1fe09fd3);
			}
		} else {
			W_APB_REG(TVFE_VAFE_CTRL0, 0x00090b00);
			W_APB_REG(TVFE_VAFE_CTRL1, 0x00000110);
			W_APB_REG(TVFE_VAFE_CTRL2, 0x0010ef93);
			if (tvafe_cpu_type() == CPU_TYPE_TXHD) {
				if (port == TVIN_PORT_CVBS3) {
					/*enable fitler for atv/dtv*/
					W_APB_BIT(TVFE_VAFE_CTRL0, 1,
					VAFE_FILTER_EN_BIT, VAFE_FILTER_EN_WID);
					/*increase current*/
					W_APB_BIT(TVFE_VAFE_CTRL0, 2,
						VAFE_FILTER_BIAS_ADJ_BIT,
						VAFE_FILTER_BIAS_ADJ_WID);
					/*increase band for atv/dtv*/
					W_APB_BIT(TVFE_VAFE_CTRL0, 7,
					VAFE_BW_SEL_BIT, VAFE_BW_SEL_WID);
					W_APB_BIT(TVFE_VAFE_CTRL0, 0x10,
						VAFE_FILTER_RESV_BIT,
						VAFE_FILTER_RESV_WID);
					/*disable pga for atv/dtv*/
					W_APB_BIT(TVFE_VAFE_CTRL1, 0,
					VAFE_PGA_EN_BIT, VAFE_PGA_EN_WID);
					/*config from vlsi-xiaoniu for atv/dtv*/
					/*disable afe buffer(bit0),*/
					/*enable vafe buffer(bit28)*/
					W_APB_REG(TVFE_VAFE_CTRL2, 0x1010eeb0);
				/*W_APB_BIT(TVFE_VAFE_CTRL2, 1, 28, 1);*/
				/*W_APB_BIT(TVFE_VAFE_CTRL2, 0, 0, 1);*/
				} else if ((port == TVIN_PORT_CVBS1) ||
					(port == TVIN_PORT_CVBS2)) {
					W_APB_BIT(TVFE_VAFE_CTRL0, 1,
					VAFE_FILTER_EN_BIT, VAFE_FILTER_EN_WID);
					W_APB_BIT(TVFE_VAFE_CTRL1, 1,
					VAFE_PGA_EN_BIT, VAFE_PGA_EN_WID);
					/*enable Vref buffer*/
					W_APB_BIT(TVFE_VAFE_CTRL2, 1, 28, 1);
					/*enable afe buffer*/
					W_APB_BIT(TVFE_VAFE_CTRL2, 1, 0, 1);
				}
			}
		}

#if (defined(CONFIG_ADC_DOUBLE_SAMPLING_FOR_CVBS) && defined(CRYSTAL_24M))
		if ((port != TVIN_PORT_CVBS3) && (port != TVIN_PORT_CVBS0)) {
			W_APB_REG(TVFE_TOP_CTRL, 0x010c4d6c);
			W_APB_REG(TVFE_AAFILTER_CTRL1, 0x00012721);
			W_APB_REG(TVFE_AAFILTER_CTRL2, 0x1304fcfa);
			W_APB_REG(TVFE_AFC_CTRL1, 0x893904d2);
			W_APB_REG(TVFE_AFC_CTRL2, 0x0f4b9ac9);
			W_APB_REG(TVFE_AFC_CTRL3, 0x01fd8c36);
			W_APB_REG(TVFE_AFC_CTRL4, 0x2de6d04f);
			W_APB_REG(TVFE_AFC_CTRL5, 0x00000004);
		} else
#endif
		{
			W_APB_REG(TVFE_AAFILTER_CTRL1, 0x00182222);
			W_APB_REG(TVFE_AAFILTER_CTRL2, 0x252b39c6);
			W_APB_REG(TVFE_AFC_CTRL1, 0x05730459);
			W_APB_REG(TVFE_AFC_CTRL2, 0xf4b9ac9);
			W_APB_REG(TVFE_AFC_CTRL3, 0x1fd8c36);
			W_APB_REG(TVFE_AFC_CTRL4, 0x2de6d04f);
			W_APB_REG(TVFE_AFC_CTRL5, 0x00000004);
		}
	}
	/* init some variables  */
	cvd2->vd_port = port;

	/* set cvd2 default format to pal-i */
	tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);

}

/* add for dtv demod */
void tvafe_set_ddemod_default(void)
{
	if (tvafe_cpu_type() == CPU_TYPE_TXL ||
		tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_GXLX) {
		W_HIU_REG(HHI_DADC_CNTL, 0x00102038);
		W_HIU_REG(HHI_DADC_CNTL2, 0x00000406);
		W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);

		/*W_HIU_REG(HHI_VDAC_CNTL0, 0x00000200);*/
		W_HIU_BIT(HHI_VDAC_CNTL0, 1, 9, 1);

	} else if (tvafe_cpu_type() == CPU_TYPE_TXHD) {
		W_HIU_REG(HHI_DADC_CNTL, 0x00102038);
		W_HIU_REG(HHI_DADC_CNTL2, 0x00000401);
		W_HIU_REG(HHI_DADC_CNTL3, 0x00082183);

		/*W_HIU_REG(HHI_VDAC_CNTL0, 0x00000200);*/
		W_HIU_BIT(HHI_VDAC_CNTL0, 1, 9, 1);

		/*enable fitler */
		/*config from vlsi-xiaoniu */
		W_APB_REG(TVFE_VAFE_CTRL0, 0x000d0710);
		W_APB_REG(TVFE_VAFE_CTRL1, 0x0);
		W_APB_REG(TVFE_VAFE_CTRL2, 0x1010eeb0);
	}

}
EXPORT_SYMBOL(tvafe_set_ddemod_default);

void tvafe_enable_avout(enum tvin_port_e port, bool enable)
{
	if (tvafe_cpu_type() == CPU_TYPE_TXL ||
		tvafe_cpu_type() == CPU_TYPE_TXLX ||
		tvafe_cpu_type() == CPU_TYPE_TXHD ||
		tvafe_cpu_type() == CPU_TYPE_TL1) {
		if (enable) {
			tvafe_clk_gate_ctrl(1);
			if (port == TVIN_PORT_CVBS3) {
				vdac_enable(1, 0x1);
				/* clock delay control */
				W_HIU_BIT(HHI_VIID_CLK_DIV, 1, 19, 1);
				/* vdac_clock_mux form atv demod */
				W_HIU_BIT(HHI_VID_CLK_CNTL2, 1, 8, 1);
				W_HIU_BIT(HHI_VID_CLK_CNTL2, 1, 4, 1);
				/* vdac_clk gated clock control */
				W_VCBUS_BIT(VENC_VDAC_DACSEL0, 1, 5, 1);
			} else {
				W_APB_REG(TVFE_ATV_DMD_CLP_CTRL, 0);
				vdac_enable(1, 0x4);
			}
		} else {
			if (port == TVIN_PORT_CVBS3)
				vdac_enable(0, 0x1);
			else
				vdac_enable(0, 0x4);
			tvafe_clk_gate_ctrl(0);
		}
	}
}

int adc_set_pll_cntl(bool on, unsigned int module_sel, void *pDtvPara)
{
	unsigned int adc_pll_lock_cnt = 0;
	int ret = 0;	/* 0: success; -x: failed*/

	struct dfe_adcpll_para *pDpara = pDtvPara;	/*only for dtv demod*/

	if (!on) {
		mutex_lock(&pll_mutex);
		adc_pll_chg &= ~module_sel;
		mutex_unlock(&pll_mutex);
		if (tvafe_dbg_enable)
			tvafe_pr_info("\n%s: init flag on:%d,module:0x%x,flag:0x%x\n",
				__func__, on, module_sel, adc_pll_chg);
		return ret;
	}
	switch (module_sel) {
	case ADC_EN_ATV_DEMOD: /* atv demod */
		if (adc_pll_chg & (ADC_EN_TVAFE | ADC_EN_DTV_DEMOD)) {
			ret = -1;
			tvafe_pr_info("%s:ADEMOD fail!:%d\n",
				__func__, adc_pll_chg);
			break;
		}
		if (adc_pll_chg & ADC_EN_ATV_DEMOD) {
			tvafe_pr_info("%s:ADEMOD ATV had done!:%d\n",
				__func__, adc_pll_chg);
			break;
		}
		mutex_lock(&pll_mutex);
		if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			do {
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x012004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x312004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL1_TL1, 0x05400000);
				W_HIU_REG(HHI_ADC_PLL_CNTL2_TL1, 0xe1800000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3_TL1, 0x48681c00);
				W_HIU_REG(HHI_ADC_PLL_CNTL4_TL1, 0x88770290);
				W_HIU_REG(HHI_ADC_PLL_CNTL5_TL1, 0x39272000);
				W_HIU_REG(HHI_ADC_PLL_CNTL6_TL1, 0x56540000);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x111104e0);

				udelay(100);
				adc_pll_lock_cnt++;
			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL0_TL1, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		} else {
			do {
			if (tvafe_cpu_type() == CPU_TYPE_TXL ||
				tvafe_cpu_type() == CPU_TYPE_TXLX ||
				tvafe_cpu_type() == CPU_TYPE_TXHD) {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL, 0x30f14250);
				W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
				/*0x5ba00380 from pll;0x5ba00384 clk*/
				/*form crystal*/
				W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x5ba00384);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02913004);
				W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
				W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
			} else {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca2a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x2933800);
				W_HIU_REG(HHI_ADC_PLL_CNTL, 0xe0644220);
				W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x34e0bf84);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a2a2110);
				/* TVFE reset */
				W_HIU_BIT(RESET1_REGISTER, 1, 7, 1);
			}
			udelay(100);
			adc_pll_lock_cnt++;
			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		}
		adc_pll_chg |= ADC_EN_ATV_DEMOD;
		mutex_unlock(&pll_mutex);
		if (adc_pll_lock_cnt == 10)
			tvafe_pr_info("%s: adc pll lock fail!!!\n", __func__);
		if (tvafe_dbg_enable)
			tvafe_pr_info("\n%s: on:%d,module:0x%x,flag:0x%x...\n",
				__func__, on, module_sel, adc_pll_chg);
		break;
	case ADC_EN_TVAFE: /* tvafe */
		if (adc_pll_chg & (ADC_EN_ATV_DEMOD | ADC_EN_DTV_DEMOD)) {
			ret = -2;
			tvafe_pr_info("%s:AFE fail!!!:%d\n",
				__func__, adc_pll_chg);
			break;
		}
		mutex_lock(&pll_mutex);
		if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			do {
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x012004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x312004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL1_TL1, 0x05400000);
				W_HIU_REG(HHI_ADC_PLL_CNTL2_TL1, 0xe0800000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3_TL1, 0x48681c00);
				W_HIU_REG(HHI_ADC_PLL_CNTL4_TL1, 0x88770290);
				W_HIU_REG(HHI_ADC_PLL_CNTL5_TL1, 0x39272000);
				W_HIU_REG(HHI_ADC_PLL_CNTL6_TL1, 0x56540000);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x111104e0);

				udelay(100);
				adc_pll_lock_cnt++;
			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL0_TL1, 31, 1) &&
				(adc_pll_lock_cnt < 10));
			tvafe_pr_info("b0=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL0_TL1));
			tvafe_pr_info("b1=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL1_TL1));
			tvafe_pr_info("b2=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL2_TL1));
			tvafe_pr_info("b3=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL3_TL1));
			tvafe_pr_info("b4=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL4_TL1));
			tvafe_pr_info("b5=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL5_TL1));
			tvafe_pr_info("b6=0x%x",
				R_HIU_REG(HHI_ADC_PLL_CNTL6_TL1));

		} else {
			do {
			if (tvafe_cpu_type() == CPU_TYPE_TXL ||
				tvafe_cpu_type() == CPU_TYPE_TXLX) {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL, 0x30f14250);
				W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
				/*0x5ba00380 from pll;0x5ba00384 clk*/
				/*form crystal*/
				W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x5ba00384);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02913004);
				W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
				W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
			} else if (tvafe_cpu_type() == CPU_TYPE_TXHD) {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL, 0x30f14250);
				W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
				/*0x5ba00380 from pll;0x5ba00385 clk*/
				/*form crystal*/
				W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x5ba00385);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02913004);
				W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
				W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
			} else {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca2a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x2933800);
				W_HIU_REG(HHI_ADC_PLL_CNTL, 0xe0644220);
				W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x34e0bf84);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a2a2110);
				/* TVFE reset */
				W_HIU_BIT(RESET1_REGISTER, 1, 7, 1);
			}
			udelay(100);
			adc_pll_lock_cnt++;
			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		}
		adc_pll_chg |= ADC_EN_TVAFE;
		mutex_unlock(&pll_mutex);
		if (adc_pll_lock_cnt == 10)
			tvafe_pr_info("%s: adc pll lock fail!!!\n", __func__);
		if (tvafe_dbg_enable)
			tvafe_pr_info("\n%s: on:%d,module:0x%x,flag:0x%x...\n",
				__func__, on, module_sel, adc_pll_chg);

		break;
	case ADC_EN_DTV_DEMOD: /* dtv demod default*/
		if (adc_pll_chg & (ADC_EN_ATV_DEMOD | ADC_EN_TVAFE)) {
			ret = -3;
			tvafe_pr_info("%s:DDEMOD fail!:%d\n",
				__func__, adc_pll_chg);
			break;
		}
		mutex_lock(&pll_mutex);
		if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			do {
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x012004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x312004e0);
				W_HIU_REG(HHI_ADC_PLL_CNTL1_TL1, 0x05400000);
				W_HIU_REG(HHI_ADC_PLL_CNTL2_TL1, 0xe1800000);
				W_HIU_REG(HHI_ADC_PLL_CNTL3_TL1, 0x48681c00);
				W_HIU_REG(HHI_ADC_PLL_CNTL4_TL1, 0x88770290);
				W_HIU_REG(HHI_ADC_PLL_CNTL5_TL1, 0x39272000);
				W_HIU_REG(HHI_ADC_PLL_CNTL6_TL1, 0x56540000);
				W_HIU_REG(HHI_ADC_PLL_CNTL0_TL1, 0x111104e0);

				udelay(100);
				adc_pll_lock_cnt++;
			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL0_TL1, 31, 1) &&
				(adc_pll_lock_cnt < 10));
		} else if (tvafe_cpu_type() == CPU_TYPE_TXL ||
			tvafe_cpu_type() == CPU_TYPE_TXLX ||
			tvafe_cpu_type() == CPU_TYPE_TXHD) {
			do {
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL,  0x5d414260);
				W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
				if (tvafe_cpu_type() == CPU_TYPE_TXL) {

					W_HIU_REG(HHI_ADC_PLL_CNTL2,
						0x5ba00384);
				} else {

					W_HIU_REG(HHI_ADC_PLL_CNTL2,
						0x5ba00385);
				}
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02913004);
				W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
				W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
				/* reset*/
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);

				udelay(100);
				adc_pll_lock_cnt++;

			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL, 31, 1)
				&& (adc_pll_lock_cnt < 10));

		} else if (tvafe_cpu_type() == CPU_TYPE_GXLX) {

			W_HIU_REG(HHI_ADC_PLL_CNTL1, 0x22000442);
			W_HIU_REG(HHI_ADC_PLL_CNTL5, 0x00034a00);
			W_HIU_REG(HHI_ADC_PLL_CNTL6, 0x00005000);
			adc_pll_lock_cnt = 1;
		}

		adc_pll_chg |= ADC_EN_DTV_DEMOD;
		mutex_unlock(&pll_mutex);
		if (adc_pll_lock_cnt >= 10)
			tvafe_pr_info("%s: adc pll lock fail!!!\n", __func__);
		if (tvafe_dbg_enable)
			tvafe_pr_info("\n%s: on:%d,module:0x%x,flag:0x%x...\n",
				__func__, on, module_sel, adc_pll_chg);
		break;
	case ADC_EN_DTV_DEMODPLL: /* dtv demod default*/

		if (adc_pll_chg & (ADC_EN_ATV_DEMOD | ADC_EN_TVAFE)) {
			ret = -4;
			tvafe_pr_info("%s:DMODPLL fail!!!:%d\n",
						__func__, adc_pll_chg);
			break;
		}

		if (pDpara == NULL) {
			ret = -5;
			tvafe_pr_info("%s: DTV para is NULL\n", __func__);
			break;
		}
		mutex_lock(&pll_mutex);

		if (tvafe_cpu_type() == CPU_TYPE_TXL ||
			tvafe_cpu_type() == CPU_TYPE_TXLX ||
			tvafe_cpu_type() == CPU_TYPE_TXHD) {
			do {
				/*reset*/
				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xca6a2110);
				W_HIU_REG(HHI_ADC_PLL_CNTL,  pDpara->adcpllctl);
				if (pDpara->atsc)
					W_HIU_REG(HHI_DEMOD_CLK_CNTL, 0x507);
				else
					W_HIU_REG(HHI_DEMOD_CLK_CNTL, 0x502);

				W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x4a6a2110);

				udelay(100);
				adc_pll_lock_cnt++;

			} while (!R_HIU_BIT(HHI_ADC_PLL_CNTL, 31, 1)
				&& (adc_pll_lock_cnt < 10));

		} else if (tvafe_cpu_type() == CPU_TYPE_GXLX) {
			W_HIU_REG(HHI_DEMOD_CLK_CNTL, 0x1000502);

			adc_pll_lock_cnt = 1;
		} else {
			/*is_meson_gxtvbb_cpu()*/
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x8a2a2110);/*reset*/
			W_HIU_REG(HHI_ADC_PLL_CNTL,  pDpara->adcpllctl);
			W_HIU_REG(HHI_DEMOD_CLK_CNTL, pDpara->demodctl);
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x0a2a2110);

			adc_pll_lock_cnt = 1;
		}

		adc_pll_chg |= ADC_EN_DTV_DEMOD;
		mutex_unlock(&pll_mutex);
		if (adc_pll_lock_cnt == 10)
			tvafe_pr_info("%s: adc pll lock fail!!!\n", __func__);
		if (tvafe_dbg_enable)
			tvafe_pr_info("\n%s: on:%d,module:0x%x,flag:0x%x...\n",
				__func__, on, module_sel, adc_pll_chg);
		break;

	default:
		tvafe_pr_err("%s:module: 0x%x wrong module index !! ",
			__func__, module_sel);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(adc_set_pll_cntl);

void adc_set_pll_reset(void)
{
	adc_pll_chg = 0;
}

int tvafe_adc_get_pll_flag(void)
{
	unsigned int ret = 0;

	if (!mutex_trylock(&pll_mutex)) {
		tvafe_pr_info("%s trylock pll_mutex fail.\n", __func__);
		return 0;
	}
	ret = adc_pll_chg;
	mutex_unlock(&pll_mutex);
	return ret;
}
EXPORT_SYMBOL(tvafe_adc_get_pll_flag);

/*
 * tvafe init the whole module
 */
static bool enableavout = true;
module_param(enableavout, bool, 0644);
MODULE_PARM_DESC(enableavout, "disable av out when load adc reg");
void tvafe_init_reg(struct tvafe_cvd2_s *cvd2,
	struct tvafe_cvd2_mem_s *mem, enum tvin_port_e port,
	struct tvafe_pin_mux_s *pinmux)
{
	unsigned int module_sel = ADC_EN_TVAFE;

	if (port == TVIN_PORT_CVBS3)
		module_sel = ADC_EN_ATV_DEMOD;
	else if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS2))
		module_sel = ADC_EN_TVAFE;

	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3)) {

#ifdef CRYSTAL_25M
	if (tvafe_cpu_type() != CPU_TYPE_TL1)
		W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0x703);/* can't write !!! */
#endif

#if (defined(CONFIG_ADC_DOUBLE_SAMPLING_FOR_CVBS) && defined(CRYSTAL_24M))
		if ((port != TVIN_PORT_CVBS3)
			&& (port != TVIN_PORT_CVBS0)) {
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xa92a2110);
			W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02973800);
			W_HIU_REG(HHI_ADC_PLL_CNTL, 0x08664220);
			W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x34e0bf80);
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x292a2110);
		} else
#endif
		adc_set_pll_cntl(1, module_sel, NULL);

		tvafe_set_cvbs_default(cvd2, mem, port, pinmux);
		/*turn on/off av out*/
		tvafe_enable_avout(port, enableavout);
		/* CDAC_CTRL_RESV2<1>=0 */
	}

	/* must reload mux if you change adc reg table!!! */
	tvafe_set_source_muxing(port, pinmux);

	tvafe_pr_info("%s ok.\n", __func__);

}

/*
 * tvafe set APB bus register accessing error exception
 */
void tvafe_set_apb_bus_err_ctrl(void)
{
	W_APB_REG(TVFE_APB_ERR_CTRL_MUX1, 0x8fff8fff);
	W_APB_REG(TVFE_APB_ERR_CTRL_MUX2, 0x00008fff);
}

/*
 * tvafe reset the whole module
 */
static void tvafe_reset_module(void)
{
	pr_info("tvafe_reset_module.\n");
	W_APB_BIT(TVFE_RST_CTRL, 1, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
	W_APB_BIT(TVFE_RST_CTRL, 0, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
	/*reset vdin asynchronous fifo*/
	/*for greenscreen on repeatly power on/off*/
	W_APB_BIT(TVFE_RST_CTRL, 1, SAMPLE_OUT_RST_BIT, SAMPLE_OUT_RST_WID);
	W_APB_BIT(TVFE_RST_CTRL, 0, SAMPLE_OUT_RST_BIT, SAMPLE_OUT_RST_WID);
}

/*
 * tvafe power control of the module
 */
void tvafe_enable_module(bool enable)
{
	/* enable */

	/* main clk up */
	if (tvafe_cpu_type() == CPU_TYPE_TL1) {
		W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 1,
			VAFE_CLK_SELECT, VAFE_CLK_SELECT_WIDTH);
		W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 1,
			VAFE_CLK_EN, VAFE_CLK_EN_WIDTH);
	} else {
		W_HIU_REG(HHI_VAFE_CLKXTALIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKOSCIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKPI_CNTL, 0x100);
		W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0x100);
	}
	/* tvfe power up */
	W_APB_BIT(TVFE_TOP_CTRL, 1, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 3, TVFE_ADC_CLK_DIV_BIT, TVFE_ADC_CLK_DIV_WID);

	/*reset module*/
	tvafe_reset_module();

	/* disable */
	if (!enable) {
		W_APB_BIT(TVFE_VAFE_CTRL0, 0,
			VAFE_FILTER_EN_BIT, VAFE_FILTER_EN_WID);
		W_APB_BIT(TVFE_VAFE_CTRL1, 0,
			VAFE_PGA_EN_BIT, VAFE_PGA_EN_WID);
		/*disable Vref buffer*/
		W_APB_BIT(TVFE_VAFE_CTRL2, 0, 28, 1);
		/*disable afe buffer*/
		W_APB_BIT(TVFE_VAFE_CTRL2, 0, 0, 1);

		/* tvfe power down */
		W_APB_BIT(TVFE_TOP_CTRL, 0, COMP_CLK_ENABLE_BIT,
				COMP_CLK_ENABLE_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, TVFE_ADC_CLK_DIV_BIT,
			TVFE_ADC_CLK_DIV_WID);

		/* main clk down */
		if (tvafe_cpu_type() == CPU_TYPE_TL1) {
			W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 0,
				VAFE_CLK_SELECT, VAFE_CLK_SELECT_WIDTH);
			W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 0,
				VAFE_CLK_EN, VAFE_CLK_EN_WIDTH);
		} else {
			W_HIU_REG(HHI_VAFE_CLKXTALIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKOSCIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKPI_CNTL, 0);
			W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);
		}
	}
}

