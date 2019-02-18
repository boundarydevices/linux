/*
 * drivers/amlogic/media/enhancement/amvecm/keystone_correction.h
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

#ifndef _KEYSTONE_CORRECTION_H_
#define _KEYSTONE_CORRECTION_H_

#include <linux/types.h>

enum vks_param_e {
	VKS_INPUT_WIDTH = 0,
	VKS_INPUT_HEIGHT,
	VKS_OUTPUT_WIDTH,
	VKS_OUTPUT_HEIGHT,
	VKS_THETA_ANGLE,
	VKS_ALPH0_ANGLE,/*5*/
	VKS_ALPH1_ANGLE,
	VKS_DELTA_BOT,
	VKS_EN,
	VKS_SCL_MODE0,
	VKS_SCL_MODE1,/*10*/
	VKS_FILL_MODE,
	VKS_ROW_INP_MODE,
	VKS_BORDER_EXT_MODE0,
	VKS_BORDER_EXT_MODE1,
	VKS_OBUF_MODE0,/*15*/
	VKS_OBUF_MODE1,
	VKS_OBUF_MRGN0,
	VKS_OBUF_MRGN1,
	VKS_PHS_QMODE,
	VKS_PARAM_MAX,
};
/* extern function */
extern void keystone_correction_process(void);
extern void keystone_correction_config(enum vks_param_e vks_param,
	unsigned int val);
extern void keystone_correction_status(void);
extern void keystone_correction_regs(void);

/* extern parameters */
extern unsigned int vks_input_width;
extern unsigned int vks_input_height;
extern unsigned int vks_output_width;
extern unsigned int vks_output_height;
extern signed int vks_theta_angle;
extern signed int vks_alph0_angle;
extern signed int vks_alph1_angle;
extern unsigned int vks_delta_bot;
extern unsigned int reg_vks_en;
extern unsigned int reg_vks_scl_mode0;
extern unsigned int reg_vks_scl_mode1;
extern unsigned int reg_vks_fill_mode;
extern unsigned int reg_vks_row_inp_mode;
extern unsigned int reg_vks_border_ext_mode0;
extern unsigned int reg_vks_border_ext_mode1;
extern unsigned int reg_vks_obuf_mode0;
extern unsigned int reg_vks_obuf_mode1;
extern unsigned int reg_vks_obuf_mrgn0;
extern unsigned int reg_vks_obuf_mrgn1;
extern unsigned int reg_vks_phs_qmode;

#endif

