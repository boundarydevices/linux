/*
 * drivers/amlogic/media/vin/tvin/tvafe/tvafe_general.h
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

#ifndef _TVAFE_GENERAL_H
#define _TVAFE_GENERAL_H

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "tvafe_cvd.h"
#include <linux/amlogic/cpu_version.h>

/************************************************* */
/* *** macro definitions *********************** */
/* ************************************ */

/* reg need in tvafe_general.c */
#define HHI_VDAC_CNTL0			0xbd
#define HHI_VDAC_CNTL1			0xbe
#define HHI_VIID_CLK_DIV		0x4a
#define HHI_VIID_CLK_CNTL		0x4b
#define HHI_VIID_DIVIDER_CNTL	0x4c
#define HHI_VID_CLK_CNTL2		0x65
#define HHI_VID_DIVIDER_CNTL	0x66

#define RESET1_REGISTER				0x1102

#define VENC_VDAC_DACSEL0			0x1b78
#define P_VENC_VDAC_DACSEL0		VCBUS_REG_ADDR(VENC_VDAC_DACSEL0)
#define VENC_VDAC_DACSEL1			0x1b79
#define P_VENC_VDAC_DACSEL1		VCBUS_REG_ADDR(VENC_VDAC_DACSEL1)
#define VENC_VDAC_DACSEL2			0x1b7a
#define P_VENC_VDAC_DACSEL2		VCBUS_REG_ADDR(VENC_VDAC_DACSEL2)
#define VENC_VDAC_DACSEL3			0x1b7b
#define P_VENC_VDAC_DACSEL3		VCBUS_REG_ADDR(VENC_VDAC_DACSEL3)
#define VENC_VDAC_DACSEL4			0x1b7c
#define P_VENC_VDAC_DACSEL4		VCBUS_REG_ADDR(VENC_VDAC_DACSEL4)
#define VENC_VDAC_DACSEL5			0x1b7d
#define P_VENC_VDAC_DACSEL5		VCBUS_REG_ADDR(VENC_VDAC_DACSEL5)
#define VENC_VDAC_SETTING			0x1b7e
#define P_VENC_VDAC_SETTING		VCBUS_REG_ADDR(VENC_VDAC_SETTING)
#define VENC_VDAC_TST_VAL			0x1b7f
#define P_VENC_VDAC_TST_VAL		VCBUS_REG_ADDR(VENC_VDAC_TST_VAL)
#define VENC_VDAC_DAC0_GAINCTRL			0x1bf0
#define P_VENC_VDAC_DAC0_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC0_GAINCTRL)
#define VENC_VDAC_DAC0_OFFSET			0x1bf1
#define P_VENC_VDAC_DAC0_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC0_OFFSET)
#define VENC_VDAC_DAC1_GAINCTRL			0x1bf2
#define P_VENC_VDAC_DAC1_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC1_GAINCTRL)
#define VENC_VDAC_DAC1_OFFSET			0x1bf3
#define P_VENC_VDAC_DAC1_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC1_OFFSET)
#define VENC_VDAC_DAC2_GAINCTRL			0x1bf4
#define P_VENC_VDAC_DAC2_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC2_GAINCTRL)
#define VENC_VDAC_DAC2_OFFSET			0x1bf5
#define P_VENC_VDAC_DAC2_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC2_OFFSET)
#define VENC_VDAC_DAC3_GAINCTRL			0x1bf6
#define P_VENC_VDAC_DAC3_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC3_GAINCTRL)
#define VENC_VDAC_DAC3_OFFSET			0x1bf7
#define P_VENC_VDAC_DAC3_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC3_OFFSET)
#define VENC_VDAC_DAC4_GAINCTRL			0x1bf8
#define P_VENC_VDAC_DAC4_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC4_GAINCTRL)
#define VENC_VDAC_DAC4_OFFSET			0x1bf9
#define P_VENC_VDAC_DAC4_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC4_OFFSET)
#define VENC_VDAC_DAC5_GAINCTRL			0x1bfa
#define P_VENC_VDAC_DAC5_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC5_GAINCTRL)
#define VENC_VDAC_DAC5_OFFSET			0x1bfb
#define P_VENC_VDAC_DAC5_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC5_OFFSET)
#define VENC_VDAC_FIFO_CTRL			0x1bfc
#define P_VENC_VDAC_FIFO_CTRL		VCBUS_REG_ADDR(VENC_VDAC_FIFO_CTRL)

#define HHI_VAFE_CLKXTALIN_CNTL		0x8f
#define P_HHI_VAFE_CLKXTALIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKXTALIN_CNTL)
#define HHI_VAFE_CLKOSCIN_CNTL		0x90
#define P_HHI_VAFE_CLKOSCIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKOSCIN_CNTL)
#define HHI_VAFE_CLKIN_CNTL			0x91
#define P_HHI_VAFE_CLKIN_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKIN_CNTL)
#define HHI_TVFE_AUTOMODE_CLK_CNTL	0x92
#define P_HHI_TVFE_AUTOMODE_CLK_CNTL CBUS_REG_ADDR(HHI_TVFE_AUTOMODE_CLK_CNTL)
#define HHI_VAFE_CLKPI_CNTL			0x93
#define P_HHI_VAFE_CLKPI_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKPI_CNTL)

#define HHI_VPU_CLK_CNTL			0x6f

#define HHI_CADC_CNTL				0x20
#define HHI_CADC_CNTL2				0x21
#define HHI_CADC_CNTL3				0x22
#define HHI_CADC_CNTL4				0x23
#define HHI_CADC_CNTL5				0x24
#define HHI_CADC_CNTL6				0x25

#define HHI_DADC_CNTL				0x27
#define HHI_DADC_CNTL2				0x28
#define HHI_DADC_RDBK0_I			0x29
#define HHI_DADC_CNTL3				0x2a
#define HHI_DADC_CNTL4				0x2b

#define HHI_DEMOD_CLK_CNTL			0x74

#define HHI_ADC_PLL_CNTL			0xaa
#define P_HHI_ADC_PLL_CNTL			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL)
#define HHI_ADC_PLL_CNTL2			0xab
#define P_HHI_ADC_PLL_CNTL2			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL2)
#define HHI_ADC_PLL_CNTL3			0xac
#define P_HHI_ADC_PLL_CNTL3			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL3)
#define HHI_ADC_PLL_CNTL4			0xad
#define P_HHI_ADC_PLL_CNTL4			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL4)
#define HHI_ADC_PLL_CNTL5			0x9e
#define P_HHI_ADC_PLL_CNTL5			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL5)
#define HHI_ADC_PLL_CNTL6			0x9f
#define P_HHI_ADC_PLL_CNTL6			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL6)
#define HHI_ADC_PLL_CNTL1			0xaf
#define P_HHI_ADC_PLL_CNTL1			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL1)
#define HHI_GCLK_OTHER              0x54

#define HHI_ADC_PLL_CNTL0_TL1		0xb0
#define HHI_ADC_PLL_CNTL1_TL1		0xb1
#define HHI_ADC_PLL_CNTL2_TL1		0xb2
#define HHI_ADC_PLL_CNTL3_TL1		0xb3
#define HHI_ADC_PLL_CNTL4_TL1		0xb4
#define HHI_ADC_PLL_CNTL5_TL1		0xb5
#define HHI_ADC_PLL_CNTL6_TL1		0xb6

/* adc pll ctl, atv demod & tvafe use the same adc module*/
/* module index: atv demod:0x01; tvafe:0x2*/
#define ADC_EN_ATV_DEMOD	0x1
#define ADC_EN_TVAFE		0x2
#define ADC_EN_DTV_DEMOD	0x4
#define ADC_EN_DTV_DEMODPLL	0xC

#define LOG_ADC_CAL
/* #define LOG_VGA_EDID */
/* ************************************************** */
/* *** enum definitions ***************************** */
/* ************************************************** */
enum tvafe_adc_ch_e {
	TVAFE_ADC_CH_NULL = 0,
	TVAFE_ADC_CH_0 = 1,
	TVAFE_ADC_CH_1 = 2,
	TVAFE_ADC_CH_2 = 3,
	TVAFE_ADC_CH_3 = 4,
};

enum tvafe_cpu_type {
	CPU_TYPE_GXTVBB = 0,
	CPU_TYPE_TXL  = 1,
	CPU_TYPE_TXLX  = 2,
	CPU_TYPE_TXHD = 3,
	CPU_TYPE_GXLX = 4,
	CPU_TYPE_TL1 = 5,
};

struct meson_tvafe_data {
	enum tvafe_cpu_type cpu_id;
	const char *name;
};

struct tvafe_clkgate_type {
	/* clktree */
	unsigned int clk_gate_state;
	struct clk *vdac_clk_gate;
};

/* ********************************************* */
/* ******** function claims ******************** */
/* ********************************************* */
extern int  tvafe_set_source_muxing(enum tvin_port_e port,
			struct tvafe_pin_mux_s *pinmux);
extern void tvafe_set_regmap(struct am_regs_s *p);
extern void tvafe_init_reg(struct tvafe_cvd2_s *cvd2,
		struct tvafe_cvd2_mem_s *mem, enum tvin_port_e port,
		struct tvafe_pin_mux_s *pinmux);
extern void tvafe_set_apb_bus_err_ctrl(void);
extern void tvafe_enable_module(bool enable);
extern void tvafe_enable_avout(enum tvin_port_e port, bool enable);

/* vdac ctrl,adc/dac ref signal,cvbs out signal*/
/* module index: atv demod:0x01; dtv demod:0x02; tvafe:0x4; dac:0x8*/
void vdac_enable(bool on, unsigned int module_sel);
extern void adc_set_pll_reset(void);
extern int tvafe_adc_get_pll_flag(void);
extern int tvafe_cpu_type(void);
extern void tvafe_clk_gate_ctrl(int status);

extern struct mutex pll_mutex;
extern bool tvafe_dbg_enable;

#endif  /* _TVAFE_GENERAL_H */

