/*
 * drivers/amlogic/media/enhancement/amvecm/vlock.h
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

#ifndef __AM_VLOCK_H
#define __AM_VLOCK_H

#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/ve.h"

#define VLOCK_REG_NUM	33

struct vlock_log_s {
	unsigned int pll_m;
	unsigned int pll_frac;
	signed int line_num_adj;
	unsigned int enc_line_max;
	signed int pixel_num_adj;
	unsigned int enc_pixel_max;
	signed int nT0;
	signed int vdif_err;
	signed int err_sum;
	signed int margin;
	unsigned int vlock_regs[VLOCK_REG_NUM];
};

enum vlock_param_e {
	VLOCK_EN = 0x0,
	VLOCK_ADAPT,
	VLOCK_MODE,
	VLOCK_DIS_CNT_LIMIT,
	VLOCK_DELTA_LIMIT,
	VLOCK_DEBUG,
	VLOCK_DYNAMIC_ADJUST,
	VLOCK_STATE,
	VLOCK_SYNC_LIMIT_FLAG,
	VLOCK_DIS_CNT_NO_VF_LIMIT,
	VLOCK_LINE_LIMIT,
	VLOCK_PARAM_MAX,
};

extern void amve_vlock_process(struct vframe_s *vf);
extern void amve_vlock_resume(void);
extern void vlock_param_set(unsigned int val, enum vlock_param_e sel);
extern void vlock_status(void);
extern void vlock_reg_dump(void);
extern void vlock_log_start(void);
extern void vlock_log_stop(void);
extern void vlock_log_print(void);


#define VLOCK_STATE_NULL 0
#define VLOCK_STATE_ENABLE_STEP1_DONE 1
#define VLOCK_STATE_ENABLE_STEP2_DONE 2
#define VLOCK_STATE_DISABLE_STEP1_DONE 3
#define VLOCK_STATE_DISABLE_STEP2_DONE 4
#define VLOCK_STATE_ENABLE_FORCE_RESET 5

/* video lock */
#define VLOCK_MODE_AUTO_ENC 0
#define VLOCK_MODE_AUTO_PLL 1
#define VLOCK_MODE_MANUAL_PLL 2
#define VLOCK_MODE_MANUAL_ENC 3
#define VLOCK_MODE_MANUAL_SOFT_ENC 4

#define XTAL_VLOCK_CLOCK   24000000/*vlock use xtal clock*/

/*vlock_debug mask*/
#define VLOCK_DEBUG_INFO (1 << 0)
#define VLOCK_DEBUG_FLUSH_REG_DIS (1 << 1)
#define VLOCK_DEBUG_ENC_LINE_ADJ_DIS (1 << 2)
#define VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS (1 << 3)
#define VLOCK_DEBUG_AUTO_MODE_LOG_EN (1 << 4)
#define VLOCK_DEBUG_PLL2ENC_DIS (1 << 5)

/* 0:enc;1:pll;2:manual pll */
extern unsigned int vlock_mode;
extern unsigned int vlock_en;
extern unsigned int vecm_latch_flag;
extern void __iomem *amvecm_hiu_reg_base;
extern unsigned int probe_ok;

extern void lcd_ss_enable(bool flag);
extern unsigned int lcd_ss_status(void);
extern int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int amvecm_hiu_reg_write(unsigned int reg, unsigned int val);
extern void vdin_vlock_input_sel(unsigned int type,
	enum vframe_source_type_e source_type);
#endif

