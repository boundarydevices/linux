/*
 * drivers/amlogic/media/deinterlace/deinterlace_mtn.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include "register.h"
#include "deinterlace.h"
#include "deinterlace_mtn.h"

#define MAX_NUM_DI_REG 32
#define GXTVBB_REG_START 12
static unsigned int combing_setting_registers[MAX_NUM_DI_REG] = {
	DI_MTN_1_CTRL1,
	DI_MTN_1_CTRL2,
	DI_MTN_1_CTRL3,
	DI_MTN_1_CTRL4,
	DI_MTN_1_CTRL5,
	DI_MTN_1_CTRL6,
	DI_MTN_1_CTRL7,
	DI_MTN_1_CTRL8,
	DI_MTN_1_CTRL9,
	DI_MTN_1_CTRL10,
	DI_MTN_1_CTRL11,
	DI_MTN_1_CTRL12,
	/* below reg are gxtvbb only, offset defined in GXTVBB_REG_START */
	MCDI_REL_DET_PD22_CHK,
	MCDI_PD22_CHK_THD,
	MCDI_PD22_CHK_THD_RT,
	NR2_MATNR_DEGHOST,
	0
};

/* decide the levels based on glb_mot[0:4]
 * or with patches like Hist, smith trig logic
 * level:    0->1  1->2   2->3   3->4
 * from low motion to high motion level
 * take 720x480 as example
 */
static unsigned int combing_glb_mot_thr_LH[4] = {1440, 2880, 4760, 9520};

/* >> 4 */
static unsigned int combing_glbmot_radprat[4] = {32, 48, 64, 80};
static unsigned int num_glbmot_radprat = 4;
module_param_array(combing_glbmot_radprat, uint, &num_glbmot_radprat, 0664);

/* level:    0<-1  1<-2   2<-3   3<-4
 * from high motion to low motion level
 */
static unsigned int combing_glb_mot_thr_HL[4] = {720, 1440, 2880, 5760};

static unsigned int num_glb_mot_thr_LH = 4;
module_param_array(combing_glb_mot_thr_LH, uint, &num_glb_mot_thr_LH, 0664);
static unsigned int num_glb_mot_thr_HL = 4;
module_param_array(combing_glb_mot_thr_HL, uint, &num_glb_mot_thr_HL, 0664);

int last_lev = -1;
static int force_lev = 0xff;
module_param_named(combing_force_lev, force_lev, int, 0664);
static int dejaggy_flag = -1;
module_param_named(combing_dejaggy_flag, dejaggy_flag, int, 0664);
int dejaggy_enable = 1;
module_param_named(combing_dejaggy_enable, dejaggy_enable, int, 0664);
static uint num_dejaggy_setting = 5;
/* 0:off 1:1-14-1 2:1-6-1 3:3-10-3 4:100% */
/* current setting dejaggy always on when interlace source */
static int combing_dejaggy_setting[5] = {1, 1, 1, 2, 3};
module_param_array(combing_dejaggy_setting, uint,
	&num_dejaggy_setting, 0664);
#ifdef CONFIG_AM_ATVDEMOD
static int atv_snr_val = 30;
module_param_named(atv_snr_val, atv_snr_val, int, 0664);
static int atv_snr_cnt;
module_param_named(atv_snr_cnt, atv_snr_cnt, int, 0664);
static int atv_snr_cnt_limit = 30;
module_param_named(atv_snr_cnt_limit, atv_snr_cnt_limit, int, 0664);
#endif

static struct combing_param_s cmb_param;

static unsigned int combing_setting_masks[MAX_NUM_DI_REG] = {
	0x0fffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x0003ff1f,
	0x01ff3fff,
	0x07ffff1f,
	0x000001ff,
	0
};

static unsigned int combing_pure_still_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090708,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000199
};

static unsigned int combing_bias_static_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090708,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000166
};


static unsigned int combing_normal_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200a0a,
	0x01000880,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000153
};

static unsigned int combing_bias_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200101,
	0x01200440,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,
	0x0001ff0c, /* 0x0001FF12 */
	0x00400204, /* 0x00200204 */
	0x00016404, /* 0x00012002 */
	0x00000142
};

static unsigned int combing_very_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200101,
	0x01200440,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080304,
	0x40020a04,  /* 0x60000404,*/
	0x0001ff0c, /* 0x0001FF12 */
	0x00400204, /* 0x00200204 */
	0x00016404, /* 0x00012002 */
	0x00000131
};
/*special for resolution test file*/
static unsigned int combing_resolution_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x141a3a62,
	0x15200a0a,
	0x01800880,
	0x74200d0d,
	0x0d5a1520,
	0x0a800480,
	0x1a1a2662,
	0x0d200302,
	0x01010101,
	0x06090708,
	0x40020a04,
	0x0001ff0c,
	0x00400204,
	0x00016404,
	0x00000131
};

static unsigned int num_combing_setting_registers = MAX_NUM_DI_REG;
module_param_array(combing_setting_registers, uint,
	&num_combing_setting_registers, 0664);
static unsigned int num_combing_setting_masks = MAX_NUM_DI_REG;
module_param_array(combing_setting_masks, uint,
	&num_combing_setting_masks, 0664);
static unsigned int num_combing_pure_still_setting = MAX_NUM_DI_REG;
module_param_array(combing_pure_still_setting, uint,
	&num_combing_pure_still_setting, 0664);
static unsigned int num_combing_bias_static_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_static_setting, uint,
	&num_combing_bias_static_setting, 0664);
static unsigned int num_combing_normal_setting = MAX_NUM_DI_REG;
module_param_array(combing_normal_setting, uint, &num_combing_normal_setting,
	0664);
static unsigned int num_combing_bias_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_motion_setting, uint,
	&num_combing_bias_motion_setting, 0664);
static unsigned int num_combing_very_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_very_motion_setting, uint,
	&num_combing_very_motion_setting, 0664);
/* 0: pure still; 1: bias static; 2: normal; 3: bias motion, 4: very motion */

static unsigned int (*combing_setting_values[6])[MAX_NUM_DI_REG] = {
	&combing_pure_still_setting,
	&combing_bias_static_setting,
	&combing_normal_setting,
	&combing_bias_motion_setting,
	&combing_very_motion_setting,
	&combing_resolution_setting
};

void adpative_combing_config(unsigned int width, unsigned int height,
		enum vframe_source_type_e src_type,
		bool prog, enum tvin_sig_fmt_e fmt)
{
	int i = 0;

	for (i = 0; i < 4; i++) {
		combing_glb_mot_thr_LH[i] =
			((width * combing_glbmot_radprat[i] + 8) >> 4);
		combing_glb_mot_thr_HL[i] =
			combing_glb_mot_thr_LH[i] - width;
	}
	last_lev = -1;
	cmb_param.width = width;
	cmb_param.height = height;
	cmb_param.src_type = src_type;
	cmb_param.fmt = fmt;
	cmb_param.prog_flag = prog;
	di_apply_reg_cfg(0);
}

void adpative_combing_exit(void)
{
	if (is_meson_gxtvbb_cpu() && dejaggy_enable) {
		dejaggy_flag = -1;
		DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC, 0, 3, 1);
	}
}
unsigned int adp_set_level(unsigned int diff, unsigned int field_diff_num)
{
	unsigned int rst = 0;
	char tlog[] = "LHM";

	if (diff <= combing_glb_mot_thr_LH[0])
		rst = 0;
	else if (diff >= combing_glb_mot_thr_LH[3])
		rst = 1;
	else
		rst = 2;
	pr_info("%s rst=%u.", __func__, rst);
	if (cmb_adpset_cnt > 0) {
		pr_info("\field-num=%04d frame-num=%04d lvl=%c\n",
			field_diff_num, diff, tlog[rst]);
		cmb_adpset_cnt--;
	}

	return rst;
}

unsigned int adp_set_mtn_ctrl3(unsigned int diff, unsigned int dlvel)
{
	int istp = 0;
	int idats = 0;
	int idatm = 0;
	int idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[2];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[2];
	else {
		rst = 0x1520;
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_pure_still_setting[2] >> 8) & 0xff;
		idatm = (combing_very_motion_setting[2] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[2]) & 0xff;
		idatm = (combing_very_motion_setting[2]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0)
	 *pr_info("mtn_ctrl3=%8x\n", rst);
	 */

	return rst;
}


int cmb_num_rat_ctl4 = 64; /* 0~255 */
module_param(cmb_num_rat_ctl4, int, 0644);
MODULE_PARM_DESC(cmb_num_rat_ctl4, "cmb_num_rat_ctl4");

int cmb_rat_ctl4_minthd = 64;
module_param(cmb_rat_ctl4_minthd, int, 0644);
MODULE_PARM_DESC(cmb_rat_ctl4_minthd, "cmb_rat_ctl4_minthd");


unsigned int adp_set_mtn_ctrl4(unsigned int diff, unsigned int dlvel,
	unsigned int height, int cmb_cnt)
{
	int hHeight = height;
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[3];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[3];
	else {
			rst = 1;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[3] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			idatr = idatr >> 1;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3]) & 0xff;
			idatm = (combing_very_motion_setting[3]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst << 8) | (idatr & 0xff);
	}

		istp = ((cmb_num_rat_ctl4 * hHeight + 128) >> 8);
		if (cmb_adpset_cnt > 0)
			pr_info("mtn_ctrl4=%8x %03d (%03d)\n",
				rst, istp, cmb_cnt);
	if (cmb_cnt > istp) {
		istp = 64 * (hHeight - cmb_cnt) / (hHeight - istp + 1);
		if (istp < 4)
			istp = 4;

		idatm = 1;
		idats = (rst >> 16) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
				idatr = idatr >> 1; /*color*/
		if (idatr < (cmb_rat_ctl4_minthd >> 1))
			idatr = (cmb_rat_ctl4_minthd >> 1);
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = (rst >> 8) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < 4)
			idatr = 4;
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = rst & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < cmb_rat_ctl4_minthd)
			idatr = cmb_rat_ctl4_minthd;
		idatm = (idatm<<8) | (idatr & 0xff);

		rst = idatm;

		if (cmb_adpset_cnt > 0)
			pr_info("%03d (%03d)=%8x\n",
				cmb_cnt, hHeight, rst);
	}

	return rst;
}

unsigned int adp_set_mtn_ctrl7(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[6];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[6];
	else {
			rst = 10;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[6] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6]) & 0xff;
			idatm = (combing_very_motion_setting[6]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0) {
	 *	pr_info("mtn_ctrl7=%8x\n", rst);
	 *}
	 */
	return rst;
}
static unsigned int small_local_mtn = 70;
module_param(small_local_mtn, uint, 0644);
MODULE_PARM_DESC(small_local_mtn, "small_local_mtn");

unsigned int adp_set_mtn_ctrl10(unsigned int diff, unsigned int dlvel,
	unsigned int *frame_diff_avg)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (*frame_diff_avg < small_local_mtn)
		rst = combing_very_motion_setting[9];
	else if (dlvel == 0)
		rst = combing_pure_still_setting[9];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[9];
	else {
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_very_motion_setting[9] >> 24) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 24) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9] >> 16) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 16) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9] >> 8) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9]) & 0xff;
		idatm = (combing_pure_still_setting[9]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}

	if (cmb_adpset_cnt > 0) {
		pr_info("mtn_ctr10=0x%08x (frame_dif_avg=%03d)\n",
			rst, *frame_diff_avg);
	}
	return rst;
}

unsigned int adp_set_mtn_ctrl11(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[10];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[10];
	else {
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_pure_still_setting[10] >> 24) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 24) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10] >> 16) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 16) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10] >> 8) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10]) & 0xff;
		idatm = (combing_very_motion_setting[10]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0) {
	 *	pr_info("mtn_ctr11=%8x\n", rst);
	 *}
	 */
	return rst;
}

static void set_combing_regs(int lvl, int bit_mode)
{
	int i;
	unsigned int ndat = 0;

	for (i = 0; i < MAX_NUM_DI_REG; i++) {
		if ((combing_setting_registers[i] == 0)
		|| (combing_setting_masks[i] == 0))
			break;
		if (bit_mode != 10 &&
			combing_setting_registers[i] == NR2_MATNR_DEGHOST)
			break;
		else if (i < GXTVBB_REG_START) {
			/* TODO: need change to check if
			 * register only in GCTVBB
			 */
			ndat = (*combing_setting_values[lvl])[i];
			DI_Wr(combing_setting_registers[i], ndat);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
			DI_Wr(combing_setting_registers[i],
				((*combing_setting_values[lvl])[i] &
				combing_setting_masks[i]) |
				(Rd(
					combing_setting_registers[i])
					& ~combing_setting_masks[i]));
	}
}

static int di_debug_readreg;
module_param(di_debug_readreg, int, 0644);
MODULE_PARM_DESC(di_debug_readreg, "di_debug_readreg");
static int dejaggy_4p = true;
module_param_named(dejaggy_4p, dejaggy_4p, int, 0664);

int adaptive_combing_fixing(
	unsigned int field_diff,
	unsigned int frame_diff,
	int cur_lev, int bit_mode,
	int cmb_cnt, int like_pulldown22_flag,
	unsigned int *frame_diff_avg)
{
	unsigned int glb_mot_avg2;
	unsigned int glb_mot_avg3;
	unsigned int glb_mot_avg5;

	unsigned int diff = 0;
	unsigned int wt_dat = 0;
	unsigned int dlvl = 0;
	static unsigned int pre_dat[5];
	bool prt_flg = (cmb_adpset_cnt > 0);
	unsigned int i = 0;
	static unsigned int pre_num;
	unsigned int crt_num = field_diff;
	unsigned int drat = 0;
	static int still_field_count;
	static int glb_mot[5] = {0, 0, 0, 0, 0};

	if (pre_num > crt_num)
		diff = pre_num - crt_num;
	else
		diff = crt_num - pre_num;

	if (diff >= cmb_param.width)
		field_diff_rate = 0;
	else {
		drat = (diff << 8) / (cmb_param.width + 1);
		if (drat > 255)
			field_diff_rate = 0;
		else
			field_diff_rate = 256 - drat;
	}
	pre_num = crt_num;

	if (di_debug_readreg > 1) {
		for (i = 0; i < 12; i++) {
			wt_dat = Rd(combing_setting_registers[i]);
			pr_info("mtn_ctrl%02d = 0x%08x\n",
				i+1, wt_dat);
		}
		pr_info("\n");
		di_debug_readreg--;
	}

	glb_mot[4] = glb_mot[3];
	glb_mot[3] = glb_mot[2];
	glb_mot[2] = glb_mot[1];
	glb_mot[1] = glb_mot[0];
	glb_mot[0] = frame_diff;
	glb_mot_avg5 =
		(glb_mot[0] + glb_mot[1] + glb_mot[2] + glb_mot[3] +
		 glb_mot[4]) / 5;
	glb_mot_avg3 = (glb_mot[0] + glb_mot[1] + glb_mot[2]) / 3;
	glb_mot_avg2 = (glb_mot[0] + glb_mot[1]) / 2;

	if (glb_mot[0] > combing_glb_mot_thr_LH[0])
		still_field_count = 0;
	else
	if (still_field_count < 16)
		still_field_count++;
	if (glb_mot_avg3 > combing_glb_mot_thr_LH[min(cur_lev, 3)]) {
		if (cur_lev < 4)
			cur_lev++;
	} else {
		if (glb_mot_avg5 <
		    combing_glb_mot_thr_HL[max(cur_lev - 1, 0)]) {
			if (cur_lev <= 1 && still_field_count > 5)
				cur_lev = 0;
			else
				cur_lev = max(cur_lev - 1, 1);
		}
	}
	if ((force_lev >= 0) & (force_lev < 6))
		cur_lev = force_lev;
	if (cur_lev != last_lev) {
		set_combing_regs(cur_lev, bit_mode);
		if (pr_pd & 0x400)
			pr_dbg("\t%5d: from %d to %d: di_mtn_1_ctrl1 = %08x\n",
		field_count, last_lev, cur_lev, Rd(DI_MTN_1_CTRL1));
		last_lev = cur_lev;
	}

	if ((force_lev > 5) && (glb_mot[1] != glb_mot[0])) {
		dlvl = adp_set_level(glb_mot[0], field_diff);
		diff = glb_mot[0];
		pre_dat[0] = Rd(DI_MTN_1_CTRL3);
		if (prt_flg)
			pr_info("%s dlevel=%d.\n", __func__, dlvl);
		wt_dat = adp_set_mtn_ctrl3(diff, dlvl);
		if (pre_dat[0] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL3, wt_dat);
			pre_dat[0] = wt_dat;
			if (prt_flg)
				pr_info("set mtn03 0x%08x.\n", wt_dat);
		}

		pre_dat[1] = Rd(DI_MTN_1_CTRL4);
		wt_dat = adp_set_mtn_ctrl4(diff, dlvl, cmb_param.height,
				cmb_cnt);
		if (pre_dat[1] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL4, wt_dat);
			if (prt_flg)
				pr_info("set mtn04 %08x -> %08x.\n",
				pre_dat[1], wt_dat);
			pre_dat[1] = wt_dat;
		}

		pre_dat[2] = Rd(DI_MTN_1_CTRL7);
		wt_dat = adp_set_mtn_ctrl7(diff, dlvl);
		if (pre_dat[2] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL7, wt_dat);
			pre_dat[2] = wt_dat;
			if (prt_flg)
				pr_info("set mtn07 0x%08x.\n", wt_dat);
		}

		pre_dat[3] = Rd(DI_MTN_1_CTRL10);
		wt_dat = adp_set_mtn_ctrl10(diff, dlvl, frame_diff_avg);
		if (pre_dat[3] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL10, wt_dat);
			pre_dat[3] = wt_dat;
			if (prt_flg)
				pr_info("set mtn10 0x%08x.\n", wt_dat);
		}

		pre_dat[4] = Rd(DI_MTN_1_CTRL11);
		wt_dat = adp_set_mtn_ctrl11(diff, dlvl);
		if (pre_dat[4] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL11, wt_dat);
			pre_dat[4] = wt_dat;
			if (prt_flg)
				pr_info("set mtn11 0x%08x.\n\n", wt_dat);
		}
	}

	if (is_meson_gxtvbb_cpu() && dejaggy_enable) {
		/* only enable dejaggy for interlace */
		if (cmb_param.prog_flag &&
			!dejaggy_4p) {
			if (dejaggy_flag != -1) {
				dejaggy_flag = -1;
				DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC,
					0, 3, 1);
			}
		} else {
			if ((dejaggy_flag == -1)
			|| ((Rd(SRSHARP0_SHARP_SR2_CTRL) & (1 << 24)) == 0)) {
				/* enable dejaggy module */
				DI_Wr_reg_bits(SRSHARP0_SHARP_SR2_CTRL,
					1, 24, 1);
				/* first time set default */
				DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ2_PRC,
					0xff, 24, 8);
				DI_Wr(SRSHARP0_SHARP_DEJ1_PRC,
					(0xff<<24)|(0xd1<<16)|(0xe<<8)|0x31);
				DI_Wr(
					SRSHARP0_SHARP_DEJ2_MISC, 0x30);
				DI_Wr(
					SRSHARP0_SHARP_DEJ1_MISC, 0x02f4);
				dejaggy_flag = 0;
			}
			if (dejaggy_enable) {
				/* dejaggy alpha according to motion level */
				dejaggy_flag =
					combing_dejaggy_setting[cur_lev];
				/* TODO: check like_pulldown22_flag and ATV
				 * noise_level
				 */
				#ifdef CONFIG_AM_ATVDEMOD
				if ((aml_atvdemod_get_snr_ex() < atv_snr_val)
			&& cmb_param.src_type == VFRAME_SOURCE_TYPE_TUNER) {
					if (atv_snr_cnt++ > atv_snr_cnt_limit)
						dejaggy_flag += 3;
				} else if (atv_snr_cnt)
					atv_snr_cnt = 0;
				#endif
				if (like_pulldown22_flag && (cur_lev > 2))
					dejaggy_flag += 1;
				/* overwrite dejaggy alpha */
				if (dejaggy_enable >= 2)
					dejaggy_flag = dejaggy_enable;
				if (dejaggy_flag > 4)
					dejaggy_flag = 4;
				if (dejaggy_flag)
					DI_Wr_reg_bits(
						SRSHARP0_SHARP_DEJ1_MISC,
						(1<<3)|dejaggy_flag, 0, 4);
				else
					DI_Wr_reg_bits(
						SRSHARP0_SHARP_DEJ1_MISC,
						0, 3, 1);
			} else
				dejaggy_flag = 0;
		}
	} else if (is_meson_gxtvbb_cpu()) {
		dejaggy_flag = -1;
		DI_Wr_reg_bits(SRSHARP0_SHARP_DEJ1_MISC, 0, 3, 1);
	}
	return cur_lev;
}

static reg_cfg_t *reg_cfg_head;

/* new pre and post di setting */
reg_cfg_t di_default_pre = {
	NULL,
	((1 << VFRAME_SOURCE_TYPE_OTHERS) |
	 (1 << VFRAME_SOURCE_TYPE_TUNER) |
	 (1 << VFRAME_SOURCE_TYPE_CVBS) |
	 (1 << VFRAME_SOURCE_TYPE_COMP) |
	 (1 << VFRAME_SOURCE_TYPE_HDMI)
	),
	0,
	0,
	{
		(
			(TVIN_SIG_FMT_COMP_480P_60HZ_D000 << 16) |
			TVIN_SIG_FMT_CVBS_SECAM),
		0
	},
	{
		{DI_EI_CTRL3,  0x0000013, 0, 27},
		{DI_EI_CTRL4, 0x151b3084, 0, 31},
		{DI_EI_CTRL5, 0x5273204f, 0, 31},
		{DI_EI_CTRL6, 0x50232815, 0, 31},
		{DI_EI_CTRL7, 0x2fb56650, 0, 31},
		{DI_EI_CTRL8, 0x230019a4, 0, 31},
		{DI_EI_CTRL9, 0x7cb9bb33, 0, 31},
/* #define DI_EI_CTRL10 */
		{0x1793, 0x0842c6a9, 0, 31},
/* #define DI_EI_CTRL11 */
		{0x179e, 0x486ab07a, 0, 31},
/* #define DI_EI_CTRL12 */
		{0x179f, 0xdb0c2503, 0, 32},
/* #define DI_EI_CTRL13 */
		{0x17a8, 0x0f021414, 0, 31},
		{ 0 },
	}
};
reg_cfg_t di_default_post = {
	NULL,
	((1 << VFRAME_SOURCE_TYPE_OTHERS) |
	 (1 << VFRAME_SOURCE_TYPE_TUNER) |
	 (1 << VFRAME_SOURCE_TYPE_CVBS) |
	 (1 << VFRAME_SOURCE_TYPE_COMP) |
	 (1 << VFRAME_SOURCE_TYPE_HDMI)
	),
	1,
	2,
	{
		(
			(TVIN_SIG_FMT_COMP_480P_60HZ_D000 << 16) |
			TVIN_SIG_FMT_CVBS_SECAM),
		0
	},
	{
		{DI_MTN_1_CTRL1,		 0, 30, 1},
		{DI_MTN_1_CTRL1, 0x0202015,  0, 27},
		{DI_MTN_1_CTRL2, 0x141a2062, 0, 31},
		{DI_MTN_1_CTRL3, 0x1520050a, 0, 31},
		{DI_MTN_1_CTRL4, 0x08800840, 0, 31},
		{DI_MTN_1_CTRL5, 0x74200d0d, 0, 31},
/* #define DI_MTN_1_CTRL6 */
		{DI_MTN_1_CTRL6, 0x0d5a1520, 0, 31},
/* #define DI_MTN_1_CTRL7 */
		{DI_MTN_1_CTRL7, 0x0a0a0201, 0, 31},
/* #define DI_MTN_1_CTRL8 */
		{DI_MTN_1_CTRL8, 0x1a1a2662, 0, 31},
/* #define DI_MTN_1_CTRL9 */
		{DI_MTN_1_CTRL9, 0x0d200302, 0, 31},
/* #define DI_MTN_1_CTRL10 */
		{DI_MTN_1_CTRL10, 0x02020606, 0, 31},
/* #define DI_MTN_1_CTRL11 */
		{DI_MTN_1_CTRL11, 0x05080304, 0, 31},
/* #define DI_MTN_1_CTRL12 */
		{DI_MTN_1_CTRL12, 0x40020a04, 0, 31},
		{ 0 },
	}
};

void di_add_reg_cfg(reg_cfg_t *reg_cfg)
{
	reg_cfg->next = reg_cfg_head;
	reg_cfg_head = reg_cfg;
}

void di_add_reg_cfg_init(void)
{
	di_add_reg_cfg(&di_default_pre);
	di_add_reg_cfg(&di_default_post);
}
static int use_reg_cfg = 1;
MODULE_PARM_DESC(use_reg_cfg, "\n use_reg_cfg\n");
module_param(use_reg_cfg, uint, 0664);

void di_apply_reg_cfg(unsigned char pre_post_type)
{
	reg_cfg_t *reg_cfg = reg_cfg_head;
	int ii;
	unsigned char set_flag = 0;

	if (!use_reg_cfg)
		return;
	while (reg_cfg) {
		if ((pre_post_type == reg_cfg->pre_post_type) &&
			((1 << cmb_param.src_type) &
			reg_cfg->source_types_enable)) {
			if (cmb_param.src_type ==
			VFRAME_SOURCE_TYPE_OTHERS &&
			(reg_cfg->dtv_definition_type != 2)) {
				/* if:dtv stand definition
				 * else if:high definition
				 */
				if ((cmb_param.height<<1) < 720 &&
				reg_cfg->dtv_definition_type)
					set_flag = 1;
				else if ((cmb_param.height<<1) >= 720
				&& (!reg_cfg->dtv_definition_type))
					set_flag = 1;
			} else {
				for (ii = 0; ii < FMT_MAX_NUM; ii++) {
					if (reg_cfg->
					sig_fmt_range[ii] == 0)
						break;
					else if (
					(cmb_param.fmt >=
					((reg_cfg->sig_fmt_range[ii]
					>> 16) & 0xffff))
					&& (cmb_param.fmt <=
					(reg_cfg->sig_fmt_range[ii] &
					0xffff))) {
						set_flag = 1;
						break;
					}
				}
			}
		}
		if (set_flag) {
			for (ii = 0; ii < REG_SET_MAX_NUM; ii++) {
				if (reg_cfg->reg_set[ii].adr == 0)
					break;
				if (pre_post_type) {
					DI_VSYNC_WR_MPEG_REG_BITS(
					reg_cfg->reg_set[ii].adr,
					reg_cfg->reg_set[ii].val,
					reg_cfg->reg_set[ii].start,
					reg_cfg->reg_set[ii].len);
				} else {
					RDMA_WR_BITS(
					reg_cfg->reg_set[ii].adr,
					reg_cfg->reg_set[ii].val,
					reg_cfg->reg_set[ii].start,
					reg_cfg->reg_set[ii].len);
				}
			}
			break;
		}
		reg_cfg = reg_cfg->next;
	}
}
