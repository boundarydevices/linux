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
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "linux/amlogic/media/amvecm/ve.h"

#define VLOCK_VER "Ref.2019/1/24"

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
	VLOCK_PLL_M_LIMIT,
	VLOCK_DELTA_CNT_LIMIT,
	VLOCK_DEBUG,
	VLOCK_DYNAMIC_ADJUST,
	VLOCK_STATE,
	VLOCK_SYNC_LIMIT_FLAG,
	VLOCK_DIS_CNT_NO_VF_LIMIT,
	VLOCK_LINE_LIMIT,
	VLOCK_SUPPORT,
	VLOCK_PARAM_MAX,
};

struct stvlock_sig_sts {
	u32 fsm_sts;
	u32 fsm_prests;
	u32 vf_sts;
	u32 vmd_chg;
	u32 frame_cnt_in;
	u32 frame_cnt_no;
	u32 input_hz;
	u32 output_hz;
	bool md_support;
	u32 phlock_percent;
	struct vecm_match_data_s *dtdata;
	u32 val_frac;
	u32 val_m;
};
extern void amve_vlock_process(struct vframe_s *vf);
extern void amve_vlock_resume(void);
extern void vlock_param_set(unsigned int val, enum vlock_param_e sel);
extern void vlock_status(void);
extern void vlock_reg_dump(void);
extern void vlock_log_start(void);
extern void vlock_log_stop(void);
extern void vlock_log_print(void);
extern int phase_lock_check(void);

#define VLOCK_STATE_NULL 0
#define VLOCK_STATE_ENABLE_STEP1_DONE 1
#define VLOCK_STATE_ENABLE_STEP2_DONE 2
#define VLOCK_STATE_DISABLE_STEP1_DONE 3
#define VLOCK_STATE_DISABLE_STEP2_DONE 4
#define VLOCK_STATE_ENABLE_FORCE_RESET 5

/* video lock */
enum VLOCK_MD {
	VLOCK_MODE_AUTO_ENC = 0x01,
	VLOCK_MODE_AUTO_PLL = 0x02,
	VLOCK_MODE_MANUAL_PLL = 0x04,
	VLOCK_MODE_MANUAL_ENC = 0x08,
	VLOCK_MODE_MANUAL_SOFT_ENC = 0x10,
	VLOCK_MODE_MANUAL_MIX_PLL_ENC = 0x20,
};

#define IS_MANUAL_MODE(md)	(md & \
				(VLOCK_MODE_MANUAL_PLL | \
				VLOCK_MODE_MANUAL_ENC |	\
				VLOCK_MODE_MANUAL_SOFT_ENC))

#define IS_AUTO_MODE(md)	(md & \
				(VLOCK_MODE_AUTO_PLL | \
				VLOCK_MODE_AUTO_ENC))

#define IS_PLL_MODE(md)	(md & \
				(VLOCK_MODE_MANUAL_PLL | \
				VLOCK_MODE_AUTO_PLL))

#define IS_ENC_MODE(md)	(md & \
				(VLOCK_MODE_MANUAL_ENC | \
				VLOCK_MODE_MANUAL_SOFT_ENC | \
				VLOCK_MODE_AUTO_ENC))

#define IS_AUTO_PLL_MODE(md) (md & \
					VLOCK_MODE_AUTO_PLL)

#define IS_MANUAL_ENC_MODE(md) (md & \
				VLOCK_MODE_MANUAL_ENC)

#define IS_MANUAL_PLL_MODE(md) (md & \
				VLOCK_MODE_MANUAL_PLL)

#define IS_MANUAL_SOFTENC_MODE(md) (md & \
				VLOCK_MODE_MANUAL_SOFT_ENC)


#define XTAL_VLOCK_CLOCK   24000000/*vlock use xtal clock*/

#define VLOCK_SUPPORT_HDMI (1 << 0)
#define VLOCK_SUPPORT_CVBS (1 << 1)

/*10s for 60hz input,vlock pll stabel cnt limit*/
#define VLOCK_PLL_STABLE_LIMIT	600
#define VLOCK_ENC_STABLE_CNT	180/*vlock enc stabel cnt limit*/
#define VLOCK_PLL_ADJ_LIMIT 9/*vlock pll adj limit(0x300a default)*/

/*vlock_debug mask*/
#define VLOCK_DEBUG_INFO (0x1)
#define VLOCK_DEBUG_FLUSH_REG_DIS (0x2)
#define VLOCK_DEBUG_ENC_LINE_ADJ_DIS (0x4)
#define VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS (0x8)
#define VLOCK_DEBUG_AUTO_MODE_LOG_EN (0x10)
#define VLOCK_DEBUG_PLL2ENC_DIS (0x20)
#define VLOCK_DEBUG_FSM_DIS (0x40)

/* 0:enc;1:pll;2:manual pll */
extern unsigned int vlock_mode;
extern unsigned int vlock_en;
extern unsigned int vecm_latch_flag;
/*extern void __iomem *amvecm_hiu_reg_base;*/
extern unsigned int probe_ok;

extern void lcd_ss_enable(bool flag);
extern unsigned int lcd_ss_status(void);
extern int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int amvecm_hiu_reg_write(unsigned int reg, unsigned int val);
extern void vdin_vlock_input_sel(unsigned int type,
	enum vframe_source_type_e source_type);
extern void vlock_param_config(struct device_node *node);
#ifdef CONFIG_AMLOGIC_LCD
extern struct work_struct aml_lcd_vlock_param_work;
extern void vlock_lcd_param_work(struct work_struct *p_work);
#endif
extern int vlock_notify_callback(struct notifier_block *block,
	unsigned long cmd, void *para);
#endif
extern void vlock_status_init(void);
extern void vlock_dt_match_init(struct vecm_match_data_s *pdata);
extern void vlock_set_en(bool en);
extern void vlock_set_phase(u32 percent);
extern void vlock_set_phase_en(u32 en);

