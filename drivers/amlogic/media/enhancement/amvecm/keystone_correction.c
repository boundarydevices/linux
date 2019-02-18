/*
 * drivers/amlogic/media/enhancement/amvecm/keystone_correction.c
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

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/vpp_keystone_regs.h"
#include "keystone_correction.h"

/*algotithm input parameters begin*/
unsigned int vks_input_width = 1920;
unsigned int vks_input_height = 1080;
unsigned int vks_output_width = 1920;
unsigned int vks_output_height = 1080;

/*case1:theta_angle=0, alph0_angle=alph1_angle=30*/
/*case2:theta_angle=30, alph0_angle=15, alph1_angle=20*/
/*case3:theta_angle=-45, alph0_angle=30, alph1_angle=20*/
signed int vks_theta_angle = 45;
module_param(vks_theta_angle, int, 0664);
MODULE_PARM_DESC(vks_theta_angle, "\n vks_theta_angle\n");

signed int vks_alph0_angle = 30;
module_param(vks_alph0_angle, int, 0664);
MODULE_PARM_DESC(vks_alph0_angle, "\n vks_alph0_angle\n");

signed int vks_alph1_angle = 20;
module_param(vks_alph1_angle, int, 0664);
MODULE_PARM_DESC(vks_alph1_angle, "\n vks_alph1_angle\n");

unsigned int vks_delta_bot;
/*algotithm input parameters end*/

/*control parameters start*/
unsigned int keystone_scaler_mode;
unsigned int reg_vks_en;
/*if scl_mode=0;4kinput,4k-outpu will underflow,*/
/*so vlsi suggest scl_mode=1 as default*/
unsigned int reg_vks_scl_mode0 = 1;
unsigned int reg_vks_scl_mode1 = 1;
unsigned int reg_vks_fill_mode = 1;
unsigned int reg_vks_row_inp_mode = 2;
unsigned int reg_vks_border_ext_mode0;
unsigned int reg_vks_border_ext_mode1;
unsigned int reg_vks_obuf_mode0 = 1;
unsigned int reg_vks_obuf_mode1 = 1;
unsigned int reg_vks_obuf_mrgn0 = 3;
unsigned int reg_vks_obuf_mrgn1 = 3;
unsigned int reg_vks_phs_qmode = 2;
/*control parameters end*/

/*macro define*/
#define OFFSET_RATIO	256
/*12bit frac*/
#define CAL_RATIO	4096
#define FRAC_BITS	12
#define ANGLE_RAGNE	90
/*0~89 angle*/
static unsigned int cos_table[90] = {
	4096,
	4095,
	4093,
	4090,
	4086,
	4080,
	4073,
	4065,
	4056,
	4045,
	4033,
	4020,
	4006,
	3991,
	3974,
	3956,
	3937,
	3917,
	3895,
	3872,
	3848,
	3823,
	3797,
	3770,
	3741,
	3712,
	3681,
	3649,
	3616,
	3582,
	3547,
	3510,
	3473,
	3435,
	3395,
	3355,
	3313,
	3271,
	3227,
	3183,
	3137,
	3091,
	3043,
	2995,
	2946,
	2896,
	2845,
	2793,
	2740,
	2687,
	2632,
	2577,
	2521,
	2465,
	2407,
	2349,
	2290,
	2230,
	2170,
	2109,
	2048,
	1985,
	1922,
	1859,
	1795,
	1731,
	1665,
	1600,
	1534,
	1467,
	1400,
	1333,
	1265,
	1197,
	1129,
	1060,
	990,
	921,
	851,
	781,
	711,
	640,
	570,
	499,
	428,
	356,
	285,
	214,
	142,
	71,
};

void exchange(unsigned int *a, unsigned int *b)
{
	unsigned int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void keystone_correction_process(void)
{
	unsigned int offset_bot, ratio_tb, delta_top, i;
	unsigned int step_top, step_bot, offset_top, temp_val;
	unsigned int reg_offset[17], reg_step[17], reg_vks_row_scl;
	unsigned int flag_top_large = 0;
	unsigned long index1, index2;

	/*config input h&w*/
	/*txlx new add,same addr,diferrent usage*/
	WRITE_VPP_REG_BITS(VPP_OUT_H_V_SIZE, vks_input_width, 16, 13);
	WRITE_VPP_REG_BITS(VPP_OUT_H_V_SIZE, vks_input_height, 0, 13);
	/*H-start&H-end*/
	WRITE_VPP_REG_BITS(VKS_IWIN_HSIZE, (vks_input_width - 1), 0, 14);
	WRITE_VPP_REG_BITS(VKS_IWIN_HSIZE, 0, 16, 14);
	/*V-start&V-end*/
	WRITE_VPP_REG_BITS(VKS_IWIN_VSIZE, (vks_input_height - 1), 0, 14);
	WRITE_VPP_REG_BITS(VKS_IWIN_VSIZE, 0, 16, 14);

	/*config output h&w*/
	WRITE_VPP_REG_BITS(VKS_OUT_WIN_SIZE, vks_output_height, 0, 14);
	WRITE_VPP_REG_BITS(VKS_OUT_WIN_SIZE, vks_output_width, 16, 14);

	/*calc ratio_tb*/
	index1 = abs(vks_alph0_angle + vks_theta_angle);
	index2 = abs(vks_alph1_angle - vks_theta_angle);
	if ((index1 >= ANGLE_RAGNE) || (index2 >= ANGLE_RAGNE)) {
		pr_info("out of angle range(%ld,%ld)\n", index1, index2);
		index1 = 0;
		index2 = 0;
	}
	ratio_tb = (cos_table[index1] * CAL_RATIO) / cos_table[index2];
	if (ratio_tb > CAL_RATIO) {
		ratio_tb = CAL_RATIO * CAL_RATIO / ratio_tb;
		flag_top_large = 1;
	}

	/* calc step_top & step_bot */
	if (reg_vks_scl_mode1 == 0) {
		step_bot = ((1 << 20) * vks_input_width +
			vks_output_width / 2) / vks_output_width;
		for (i = 0; i < FRAC_BITS; i++) {
			if (step_bot & (1 << (31 - i)))
				break;
			continue;
		}
		step_top = ((step_bot << i) / ratio_tb) << (FRAC_BITS - i);
	} else {
		step_bot = ((1 << 20) * vks_output_width +
			vks_input_width/2) / vks_input_width;
		for (i = 0; i < (FRAC_BITS - 1); i++) {
			if (step_bot & (1 << (31 - i)))
				break;
			continue;
		}
		step_top = ((step_bot >> (FRAC_BITS - i)) * ratio_tb) >> i;
	}
	/* calc offset_top & offset_bot */
	delta_top = (vks_output_width * (CAL_RATIO - ratio_tb)) /
		(2 * CAL_RATIO) + vks_delta_bot;
	offset_top = delta_top * OFFSET_RATIO;
	offset_bot = vks_delta_bot * OFFSET_RATIO;

	if (flag_top_large) {
		exchange(&step_top, &step_bot);
		exchange(&offset_top, &offset_bot);
	}
	/* calc step & offset */
	reg_offset[16] = offset_bot;/*may be 0*/
	reg_offset[0] = offset_top;
	reg_step[0] = step_top;
	reg_step[16] = step_bot;
	pr_info("step_top:0x%x,step_bot:0x%x,offset_top:0x%x,offset_bot:0x%x,ratio_tb:0x%x\n",
		step_top, step_bot, offset_top, offset_bot, ratio_tb);
	if (reg_vks_scl_mode1 == 0) {
		for (i = 0; i < 17; i++) {
			#if 0
			index1 = (1 << 26) / step_top;
			index2 = (1 << 26) / step_bot;
			if (index2 < index1)
				temp_val = index1 -
				(i - 0) * (index1 - index2) / 16;
			else
				temp_val = index1 +
				(i - 0) * (index2 - index1) / 16;
			index1 = (1 << 26) / temp_val;
			index2 = (1 << 30) - 1;
			#else
			index1 = (ulong)16 * step_top * step_bot;
			index2 = (ulong)16 * step_bot +
				i * (step_top - step_bot);
			temp_val = index1/index2;
			index1 = temp_val;
			index2 = (1 << 24) - 1;
			#endif
			reg_step[i] =  min(index1, index2);
			index1 = offset_top;
			index2 = offset_bot;
			if (index2 < index1)
				temp_val = index1 -
				(i - 0) * (index1 - index2) / 16;
			else
				temp_val = index1 +
				(i - 0) * (index2 - index1) / 16;
			index1 = (ulong)temp_val * reg_step[i] / (1<<20);
			index2 = 1 << 22;
			reg_offset[i] = min(index1, index2);
		}
	} else {
		for (i = 1; i < 16; i++) {
			reg_offset[i] = reg_offset[0] + (i * (reg_offset[16] -
				reg_offset[0]) + 8) / 16;
			reg_step[i]  = (reg_step[0] + (i * (reg_step[16] -
				reg_step[0]) + 8) / 16);
		}
	}

	/* config offset 0x09~0x19 */
	WRITE_VPP_REG(VKS_PARA_ADDR_PORT, 9);
	for (i = 0; i < 17; i++)
		WRITE_VPP_REG(VKS_PARA_DATA_PORT, reg_offset[i]);
	/* config step 0x1a~0x2a */
	WRITE_VPP_REG(VKS_PARA_ADDR_PORT, 0x1a);
	for (i = 0; i < 17; i++)
		WRITE_VPP_REG(VKS_PARA_DATA_PORT, reg_step[i]);

	/*calc row scl*/
	reg_vks_row_scl = ((1 << 23) + vks_input_height / 2) / vks_input_height;

	/*config fill value*/
	/*if the input format is yuv12bit,set to 0x00008080*/
	/*if the input format is yuv10bit,set to 0x00002020*/
	/*if the input format is RGB,set to 0x00000000*/
	WRITE_VPP_REG(VKS_FILL_VAL, 0x00002020);

	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_row_scl, 0, 16);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_phs_qmode, 16, 2);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_obuf_mrgn1, 18, 2);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_obuf_mrgn0, 20, 2);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_obuf_mode1, 22, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_obuf_mode0, 23, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_border_ext_mode1, 24, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_border_ext_mode0, 25, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_row_inp_mode, 26, 2);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_fill_mode, 28, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_scl_mode1, 29, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_scl_mode0, 30, 1);
	WRITE_VPP_REG_BITS(VKS_CTRL, reg_vks_en, 31, 1);
	if (reg_vks_en) {
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 29, 1);
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 3, 10, 2);
	} else {
		WRITE_VPP_REG_BITS(VPP_MISC, 0, 29, 1);
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0, 10, 2);
	}
	pr_info("%s done!\n", __func__);
}

void keystone_correction_config(enum vks_param_e vks_param, unsigned int val)
{
	switch (vks_param) {
	case VKS_INPUT_WIDTH:
		vks_input_width = val;
		break;
	case VKS_INPUT_HEIGHT:
		vks_input_height = val;
		break;
	case VKS_OUTPUT_WIDTH:
		vks_output_width = val;
		break;
	case VKS_OUTPUT_HEIGHT:
		vks_output_height = val;
		break;
	case VKS_THETA_ANGLE:
		vks_theta_angle = val;
		break;
	case VKS_ALPH0_ANGLE:
		vks_alph0_angle = val;
		break;
	case VKS_ALPH1_ANGLE:
		vks_alph1_angle = val;
		break;
	case VKS_DELTA_BOT:
		vks_delta_bot = val;
		break;
	case VKS_EN:
		reg_vks_en = val;
		break;
	case VKS_SCL_MODE0:
		reg_vks_scl_mode0 = val;
		break;
	case VKS_SCL_MODE1:
		reg_vks_scl_mode1 = val;
		break;
	case VKS_FILL_MODE:
		reg_vks_fill_mode = val;
		break;
	case VKS_ROW_INP_MODE:
		reg_vks_row_inp_mode = val;
		break;
	case VKS_BORDER_EXT_MODE0:
		reg_vks_border_ext_mode0 = val;
		break;
	case VKS_BORDER_EXT_MODE1:
		reg_vks_border_ext_mode1 = val;
		break;
	case VKS_OBUF_MODE0:
		reg_vks_obuf_mode0 = val;
		break;
	case VKS_OBUF_MODE1:
		reg_vks_obuf_mode1 = val;
		break;
	case VKS_OBUF_MRGN0:
		reg_vks_obuf_mrgn0 = val;
		break;
	case VKS_OBUF_MRGN1:
		reg_vks_obuf_mrgn1 = val;
		break;
	case VKS_PHS_QMODE:
		reg_vks_phs_qmode = val;
		break;
	default:
		pr_info("%s:unknown param!\n", __func__);
		break;
	}
}

void keystone_correction_status(void)
{
	pr_info("\t keystone param:\n");
	pr_info("vks_input_width(%d):%d\n", VKS_INPUT_WIDTH, vks_input_width);
	pr_info("vks_input_height(%d):%d\n",
		VKS_INPUT_HEIGHT, vks_input_height);
	pr_info("vks_output_width(%d):%d\n",
		VKS_OUTPUT_WIDTH, vks_output_width);
	pr_info("vks_output_height(%d):%d\n",
		VKS_OUTPUT_HEIGHT, vks_output_height);
	pr_info("vks_theta_angle(%d):%d\n", VKS_THETA_ANGLE, vks_theta_angle);
	pr_info("vks_alph0_angle(%d):%d\n", VKS_ALPH0_ANGLE, vks_alph0_angle);
	pr_info("vks_alph1_angle(%d):%d\n", VKS_ALPH1_ANGLE, vks_alph1_angle);
	pr_info("vks_delta_bot(%d):%d\n", VKS_DELTA_BOT, vks_delta_bot);
	pr_info("reg_vks_en(%d):%d\n", VKS_EN, reg_vks_en);
	pr_info("reg_vks_scl_mode0(%d):%d\n",
		VKS_SCL_MODE0, reg_vks_scl_mode0);
	pr_info("reg_vks_scl_mode1(%d):%d\n",
		VKS_SCL_MODE1, reg_vks_scl_mode1);
	pr_info("reg_vks_fill_mode(%d):%d\n",
		VKS_FILL_MODE, reg_vks_fill_mode);
	pr_info("reg_vks_row_inp_mode(%d):%d\n",
		VKS_ROW_INP_MODE, reg_vks_row_inp_mode);
	pr_info("reg_vks_border_ext_mode0(%d):%d\n",
		VKS_BORDER_EXT_MODE0, reg_vks_border_ext_mode0);
	pr_info("reg_vks_border_ext_mode1(%d):%d\n",
		VKS_BORDER_EXT_MODE1, reg_vks_border_ext_mode1);
	pr_info("reg_vks_obuf_mode0(%d):%d\n",
		VKS_OBUF_MODE0, reg_vks_obuf_mode0);
	pr_info("reg_vks_obuf_mode1(%d):%d\n",
		VKS_OBUF_MODE1, reg_vks_obuf_mode1);
	pr_info("reg_vks_obuf_mrgn0(%d):%d\n",
		VKS_OBUF_MRGN0, reg_vks_obuf_mrgn0);
	pr_info("reg_vks_obuf_mrgn1(%d):%d\n",
		VKS_OBUF_MRGN1, reg_vks_obuf_mrgn1);
	pr_info("reg_vks_phs_qmode(%d):%d\n", VKS_PHS_QMODE, reg_vks_phs_qmode);
}

void keystone_correction_regs(void)
{
	unsigned int reg, i;

	pr_info("----keystone regs start----\n");
	for (reg = VKS_CTRL; reg <= VKS_LBUF_SIZE; reg++)
		pr_info("[0x%x]reg:0x%x-0x%x\n",
			(0xFF900000 + (reg<<2)), reg, READ_VPP_REG(reg));
	pr_info("\t<vks offset regs>\n");
	for (i = 9; i < 0x1a; i++) {
		WRITE_VPP_REG(VKS_PARA_ADDR_PORT, i);
		reg = READ_VPP_REG(VKS_PARA_DATA_PORT);
		pr_info("reg:0x%x-0x%x\n", i, reg);
	}
	pr_info("\t<vks step regs>\n");
	for (i = 0x1a; i < 0x2b; i++) {
		WRITE_VPP_REG(VKS_PARA_ADDR_PORT, i);
		reg = READ_VPP_REG(VKS_PARA_DATA_PORT);
		pr_info("reg:0x%x-0x%x\n", i, reg);
	}
	pr_info("\t<vks Ycoeff regs>\n");
	for (i = 0x2b; i < 0x4c; i++) {
		WRITE_VPP_REG(VKS_PARA_ADDR_PORT, i);
		reg = READ_VPP_REG(VKS_PARA_DATA_PORT);
		pr_info("reg:0x%x-0x%x\n", i, reg);
	}
	pr_info("\t<vks Ccoeff regs>\n");
	for (i = 0x4c; i < 0x6d; i++) {
		WRITE_VPP_REG(VKS_PARA_ADDR_PORT, i);
		reg = READ_VPP_REG(VKS_PARA_DATA_PORT);
		pr_info("reg:0x%x-0x%x\n", i, reg);
	}
	pr_info("----keystone regs end----\n");
}

