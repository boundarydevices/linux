/*
 * drivers/amlogic/media/vout/backlight/aml_ldim/ldim_drv.c
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
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include "ldim_drv.h"
#include "ldim_func.h"
#include "ldim_reg.h"
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>

#define AML_LDIM_DEV_NAME            "aml_ldim"

#ifndef MAX
#define MAX(a, b)   ((a > b) ? a:b)
#endif
#ifndef MIN
#define MIN(a, b)   ((a < b) ? a:b)
#endif

#ifndef ABS
#define ABS(a)   ((a < 0) ? (-a):a)
#endif

#define FRM_NUM_DBG 5

const char ldim_dev_id[] = "ldim-dev";
unsigned char ldim_debug_print;

struct ldim_dev_s {
	struct cdev   cdev;
	struct device *dev;
	dev_t aml_ldim_devno;
	struct class *aml_ldim_clsp;
	struct cdev *aml_ldim_cdevp;
};
static struct ldim_dev_s ldim_dev;
static struct ldim_param_s ldim_db_para;
struct LDReg nPRM;
struct FW_DAT FDat;

static unsigned int ldim_hist_row = 1;
static unsigned int ldim_hist_col = 1;
static unsigned int ldim_blk_row = 1;
static unsigned int ldim_blk_col = 1;
static unsigned int ldim_data_min = LD_DATA_MIN;
static unsigned int ldim_brightness_level;

static unsigned char ldim_on_flag;
static unsigned char LDIM_DATA_FROM_DB, db_print_flag;
static unsigned char ldim_func_en, ldim_remap_en;
static unsigned char ldim_func_bypass; /* for lcd bist pattern */
static unsigned char ldim_brightness_bypass;
static unsigned char ldim_level_update;
static unsigned char ldim_test_en;

static spinlock_t  ldim_isr_lock;
static spinlock_t  rdma_ldim_isr_lock;

static struct workqueue_struct *ldim_queue;
static struct work_struct ldim_on_vs_work;
static struct work_struct ldim_off_vs_work;

static unsigned int ldim_irq_cnt;
static unsigned int rdma_ldim_irq_cnt;

/* ************************************************* */
static unsigned long val_1[16] = {512, 512, 512, 512, 546, 546,
	585, 630, 745, 819, 910, 1170, 512, 512, 512, 512};
static unsigned long bin_1[16] = {4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4};/* 0~32 */
static unsigned long val_2[16] = {3712, 3712, 3712, 3712, 3823, 3823,
	3647, 3455, 3135, 3007, 2879, 2623, 3750, 3800, 4000, 4055};
static unsigned long bin_2[16] = {29, 29, 29, 29, 29, 29,
	25, 22, 17, 15, 13, 28, 28, 27, 26, 25};


/* *************** alg parameters ******************* */
#define LD_FW_ALG_FRM_V0     0 /* gxtvbb, txlx local dimming, 201806 */
#define LD_FW_ALG_FRM_V1     1 /* txlx local contrast, 2017.10 round */
#define LD_FW_ALG_FRM_SEL    LD_FW_ALG_FRM_V0

#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
static unsigned long fw_LD_ThSF_l = 1600;
static unsigned long fw_LD_ThTF_l = 32;/* note*/
static unsigned long boost_gain = 2;
static unsigned long boost_gain_neg = 3;
static unsigned long alpha_delta = 255;/* to fix flicker */
#define LD_FW_ALG_FRM_VER    "V1 - 20180612"

#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
static unsigned long fw_LD_ThSF_l = 1600;
static unsigned long fw_LD_ThTF_l = 256;
static unsigned long boost_gain = 456; /*norm 256 to 1,T960 finally use*/
static unsigned long TF_alpha = 256; /*256;*/
static unsigned long lpf_gain = 128;  /* [0~128~256], norm 128 as 1*/
#define LD_FW_ALG_FRM_VER    "V0 - 20180628"
#endif /* LD_FW_ALG_FRM_V0 */

/*LPF tap: 0-lpf_res 41,1-lpf_res 114,...*/
static unsigned long lpf_res = 14;    /* 1024/9*9 = 13,LPF_method=3 */
static unsigned long rgb_base = 127;

static unsigned int ov_gain = 16;
static unsigned int incr_dif_gain = 16;

static unsigned int LPF_method = 3;
static unsigned int LD_TF_STEP_TH = 100;
static unsigned int TF_step_method = 3;
static unsigned int TF_FRESH_BL = 8;

static unsigned int TF_BLK_FRESH_BL = 5;
static unsigned int side_blk_diff_th = 100;
static unsigned int bbd_th = 200;
static unsigned char bbd_detect_en = 1;
static unsigned char diff_blk_luma_en = 1;

static unsigned int fw_rgb_diff_th = 32760;
static unsigned int max_luma = 4060;
static unsigned int lmh_avg_TH = 200;/*for woman flicker*/
static unsigned int fw_TF_sum_th = 32760;/*20180530*/

static unsigned long avg_gain = LD_DATA_MAX;
static unsigned long litgain = LD_DATA_MAX;
/* *************** alg parameters ******************* */

/*BL_matrix remap curve*/
static unsigned long bl_remap_curve[16] = {
	612, 654, 721, 851, 1001, 1181, 1339, 1516,
	1738, 1948, 2152, 2388, 2621, 2889, 3159, 3502
};
static unsigned long fw_LD_Whist[16] = {
	32, 64, 96, 128, 160, 192, 224, 256,
	288, 320, 352, 384, 416, 448, 480, 512
};
static unsigned char Sf_bypass, Boost_light_bypass;
static unsigned char Lpf_bypass, Ld_remap_bypass;
static unsigned char black_frm;

static unsigned char fw_hist_print;/*20180525*/
static unsigned char Debug;/*20180606*/
static unsigned int db_cnt;
static unsigned int fw_print_frequent = 8;/*20180606,print every 8 frame*/
static unsigned int Dbprint_lv;

static unsigned int ldim_hist_en;
module_param(ldim_hist_en, uint, 0664);
MODULE_PARM_DESC(ldim_hist_en, "ldim_hist_en");

static unsigned int ldim_avg_update_en;
module_param(ldim_avg_update_en, uint, 0664);
MODULE_PARM_DESC(ldim_avg_update_en, "ldim_avg_update_en");

static unsigned int ldim_matrix_update_en;
module_param(ldim_matrix_update_en, uint, 0664);
MODULE_PARM_DESC(ldim_matrix_update_en, "ldim_matrix_update_en");

static unsigned int ldim_alg_en;
module_param(ldim_alg_en, uint, 0664);
MODULE_PARM_DESC(ldim_alg_en, "ldim_alg_en");

static unsigned int ldim_top_en;
module_param(ldim_top_en, uint, 0664);
MODULE_PARM_DESC(ldim_top_en, "ldim_top_en");

static struct aml_ldim_driver_s ldim_driver;
static void ldim_on_vs_brightness(void);
static void ldim_off_vs_brightness(void);
static void ldim_on_vs_arithmetic(void);
static void ldim_update_setting(void);
static void ldim_get_matrix_info_max_rgb(void);
static void ldim_bl_remap_curve(int slop_gain);
static void ldim_bl_remap_curve_print(void);
static struct ldim_config_s ldim_config = {
	.hsize = 3840,
	.vsize = 2160,
	.row = 1,
	.col = 1,
	.bl_mode = 1,
};

static void ldim_on_update_brightness(struct work_struct *work)
{
	ldim_stts_read_region(ldim_hist_row, ldim_hist_col);
	ldim_on_vs_arithmetic();
	ldim_on_vs_brightness();
}

static void ldim_off_update_brightness(struct work_struct *work)
{
	ldim_off_vs_brightness();
}

void LDIM_WR_32Bits(unsigned int addr, unsigned int data)
{
#ifdef CONFIG_VSYNC_RDMA
	VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, addr);
	VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);
#else
	Wr(LDIM_BL_ADDR_PORT, addr);
	Wr(LDIM_BL_DATA_PORT, data);
#endif
}

unsigned int LDIM_RD_32Bits(unsigned int addr)
{
	Wr(LDIM_BL_ADDR_PORT, addr);
	return Rd(LDIM_BL_DATA_PORT);
}

void LDIM_wr_reg_bits(unsigned int addr, unsigned int val,
				unsigned int start, unsigned int len)
{
	unsigned int data;

	data = LDIM_RD_32Bits(addr);
	data = (data & (~((1 << len) - 1)<<start))  |
		((val & ((1 << len) - 1)) << start);
	LDIM_WR_32Bits(addr, data);
}

void LDIM_WR_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 32 / size_t;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		data = data | ((pData[i] & mask) << (size_t *cnt));
		cnt++;
		if (cnt == subCnt) {
			Wr(LDIM_BL_DATA_PORT, data);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		Wr(LDIM_BL_DATA_PORT, data);
}
void LDIM_RD_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 32 / size_t;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		cnt++;
		if (cnt == subCnt) {
			data = Rd(LDIM_BL_DATA_PORT);
			pData[i-1] = data & mask;
			pData[i] = mask & (data >> size_t);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = Rd(LDIM_BL_DATA_PORT);
}
void LDIM_RD_BASE_LUT_2(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1 << size_t)-1;
	subCnt = 2;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		cnt++;
		if (cnt == subCnt) {
			data = Rd(LDIM_BL_DATA_PORT);
			pData[i-1] = data & mask;
			pData[i] = mask & (data >> size_t);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = Rd(LDIM_BL_DATA_PORT);
}


static void ldim_bl_remap_curve(int slop_gain)
{
	int i = 0, ii = 0;
	int t_bl_remap_curve[16] = {0};
	int bl_max = 0, mid_value = 0;

	LDIMPR("%s:\n", __func__);
	for (i = 0; i < 16; i++) {
		ii = (i + 1) * 256;
		mid_value = ii * ii >> 17;
		t_bl_remap_curve[i] = mid_value * slop_gain + 512;
	}
	bl_max = t_bl_remap_curve[15];

	for (i = 0; i < 16; i++)
		bl_remap_curve[i] = t_bl_remap_curve[i] * 4095 / bl_max;
}

static void ldim_bl_remap_curve_print(void)
{
	int i = 0, len;
	char *buf;

	len = 16 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		return;
	}

	pr_info("bl_remap_curve:\n");
	len = 0;
	for (i = 0; i < 16; i++)
		len += sprintf(buf+len, "\t%ld\n", bl_remap_curve[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
static void ld_fw_alg_frm_txlx(struct LDReg *nPRM1, struct FW_DAT *FDat1,
	unsigned int *max_matrix, unsigned int *hist_matrix)
{
	/* Notes, nPRM will be set here in SW algorithm too */
	int dif, blkRow, blkCol, k, m, n;
	unsigned long sum;
	int adpt_alp;
	unsigned int avg, alpha = 256, Bmin, Bmax, bl_value;
	unsigned int Vnum  = (nPRM1->reg_LD_BLK_Vnum);
	unsigned int Hnum  = (nPRM1->reg_LD_BLK_Hnum);
	unsigned int *tBL_matrix;
	unsigned int BLmax = 4096;   /* maximum BL value */
	int stride = ldim_hist_col;
	int RGBmax, Histmx, maxNB, curNB = 0;
	unsigned int fw_blk_num  = Vnum*Hnum;
	unsigned int fw_LD_Thist = ((fw_blk_num*5)>>2);
	unsigned int fw_pic_size =
			(nPRM1->reg_LD_pic_RowMax)*(nPRM1->reg_LD_pic_ColMax);
	int fw_LD_ThSF = 1600;
	unsigned int fw_LD_ThTF = 32;
	unsigned int fw_LD_BLEst_ACmode = 1;
	unsigned int fw_hist_mx;
	unsigned int SF_sum = 0, TF_sum = 0, dif_sum = 0;
	int gain_neg_delta = 0;
	int pr_flag = 0;

	fw_LD_ThSF = fw_LD_ThSF_l;
	fw_LD_ThTF = fw_LD_ThTF_l;
	tBL_matrix = FDat1->TF_BL_matrix_2;

	/* first 8 frame & every 2 min print*/
	pr_flag = (db_cnt < 8) || (db_cnt % 120 == 0);
	if (Dbprint_lv == 1) {
		if (pr_flag) {
			pr_info("input:\n"
				"Vnum = [%4d]\n"
				"Hnum = [%4d]\n"
				"reg_LD_pic_RowMax = [%4d]\n"
				"reg_LD_pic_ColMax = [%4d]\n\n"
				"stride(ldim_hist_col) = [%4d]\n\n",
				Vnum, Hnum,
				nPRM1->reg_LD_pic_RowMax,
				nPRM1->reg_LD_pic_ColMax,
				ldim_hist_col);
			if (fw_hist_print) {
				for (blkRow = 0; blkRow < Vnum; blkRow++) {
					for (blkCol = 0; blkCol < Hnum;
						blkCol++) {
						pr_info("(%d,%d,%d); ",
						max_matrix[blkRow*3*stride +
							blkCol*3 + 0],
						max_matrix[blkRow*3*stride +
							blkCol*3 + 1],
						max_matrix[blkRow*3*stride +
							blkCol*3 + 2]);
					}
					pr_info("\n");
				}
				pr_info("\n\n");
			}
		}
	}

	/* calculate the current frame */
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			RGBmax = MAX(MAX(max_matrix[blkRow*3*stride +
				blkCol*3 + 0],
			max_matrix[blkRow*3*stride + blkCol*3 + 1]),
			max_matrix[blkRow*3*stride + blkCol*3 + 2]);
			/* Consider the sitrogram */
			Histmx = 0;
			for (k = 0; k < 16; k++) {
				Histmx += (hist_matrix[blkRow*LD_STA_BIN_NUM*
					stride + blkCol*LD_STA_BIN_NUM + k] *
					fw_LD_Whist[k]);
				}
			fw_hist_mx =
				((Histmx>>8)*fw_LD_Thist*2/(fw_pic_size>>8));
			/* further debug */
			tBL_matrix[blkRow*Hnum + blkCol] =
				((BLmax*MIN(fw_hist_mx, RGBmax))>>10);
				nPRM1->BL_matrix[blkCol*Vnum +
					blkRow] = tBL_matrix[blkRow*Hnum +
						blkCol];
		}
	}

	if (Dbprint_lv == 1) {
		if (pr_flag) {
			pr_info("BL_0[%d * %d] info(LD_STA_BIN_NUM: %d): [\n ",
				Vnum, Hnum, LD_STA_BIN_NUM);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d) ",
					tBL_matrix[blkRow*Hnum + blkCol]);
				}
				pr_info(" ;\n ");
			}
			pr_info(" ]\n\n ");
		}
	}

	/* Spatial Filter the BackLits */
	sum = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			maxNB = 0;
			for (m =  -1; m < 2; m++) {
				for (n =  -1; n < 2; n++) {
					if ((m == 0) && (n == 0))
						curNB =
						tBL_matrix[blkRow*Hnum +
						blkCol];
					else if (((blkRow+m) >= 1) &&
						((blkRow+m) <= Vnum) &&
						((blkCol+n) >= 1) &&
						((blkCol+n) <= Hnum))
						maxNB = MAX(maxNB, tBL_matrix[
							blkRow*Hnum + blkCol]);
				}
			}
			/* SF matrix */
			FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] =
				MAX(curNB, (maxNB-fw_LD_ThSF));
			sum += FDat1->SF_BL_matrix[blkRow*Hnum + blkCol];
			/* for SF_BL_matrix average calculation */
		}
	}

	if (Dbprint_lv == 1) {
		if (pr_flag) {
			pr_info("BL_SF[%d * %d] info: [\n ", Vnum, Hnum);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d) ",
					FDat1->SF_BL_matrix[
						blkRow*Hnum + blkCol]);
				}
				pr_info(" ;\n ");
			}
			pr_info(" ]\n\n ");
		}
	}

	/* boost the bright region lights a little bit. */
	avg = ((sum*7/fw_blk_num)>>3);
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			dif = (FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] - avg);
			if (dif > 0)
				FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] +=
					(boost_gain*dif);
			if (dif < 0) {
				gain_neg_delta = -boost_gain_neg*dif;
				if (FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] >
					gain_neg_delta) {
					FDat1->SF_BL_matrix[
						blkRow*Hnum + blkCol] -=
						gain_neg_delta;
				} else {
					FDat1->SF_BL_matrix[
						blkRow*Hnum + blkCol] = 0;
				}
			}
			if (FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] > 4095)
				FDat1->SF_BL_matrix[blkRow*Hnum + blkCol]
					= 4095;
			if (FDat1->SF_BL_matrix[blkRow*Hnum + blkCol] < 0)
				FDat1->SF_BL_matrix[blkRow*Hnum + blkCol]
					= 0; /*clip*/
		}
	}
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			SF_sum += FDat1->SF_BL_matrix[blkRow*Hnum + blkCol];
			TF_sum += FDat1->TF_BL_matrix[blkRow*Hnum + blkCol];
		}
	}

	if (Dbprint_lv == 1) {
		if (pr_flag) {
			pr_info("BL_boost_SF [%d * %d] info:\n",
				Vnum, Hnum);
			pr_info("boost_gain: %ld,%ld:\n"
				"luma_avg = %d, SF_sum= %d, TF_sum = %d\n\n ",
				boost_gain, boost_gain_neg,
				avg, SF_sum, TF_sum);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d) ",
					FDat1->SF_BL_matrix[
						blkRow*Hnum + blkCol]);
				}
				pr_info(" ;\n");
			}
			pr_info(" ]\n\n");
		}
	}

	/* Temperary filter */
	sum = 0; Bmin = 4096; Bmax = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			/* Optimization needed here */
			adpt_alp = ABS((FDat1->SF_BL_matrix[
				blkRow*Hnum + blkCol]) -
				(FDat1->TF_BL_matrix[
				blkRow*Hnum + blkCol]));

			dif_sum = ABS(SF_sum - TF_sum);
			if (dif_sum > fw_TF_sum_th)
				alpha = 256-alpha_delta;
			else
				alpha = MIN(256, fw_LD_ThTF);

			FDat1->TF_BL_alpha[blkRow*Hnum + blkCol] = alpha;

			/* get the temporary filtered BL_value */
			bl_value = (((256-alpha) * (FDat1->TF_BL_matrix[
				blkRow*Hnum + blkCol]) +
				alpha*(FDat1->SF_BL_matrix[
				blkRow*Hnum + blkCol]) +
				128) >> 8);

			/* Set the TF_BL_matrix to the BL_matrix */
			if (nPRM1->reg_LD_BackLit_mode == 1)
				nPRM1->BL_matrix[blkCol*Vnum +
					blkRow] = bl_value;
			else
				nPRM1->BL_matrix[blkRow*Hnum +
					blkCol] = bl_value;

			/* Get the TF_BL_matrix */
			FDat1->TF_BL_matrix[blkRow*Hnum + blkCol] = bl_value;

			/* leave the Delayed version for next frame */
			for (k = 0; k < 3; k++)
				FDat1->last_STA1_MaxRGB[blkRow*3*stride +
				blkCol*3 + k] =
					max_matrix[blkRow*3*stride +
						blkCol*3 + k];

			/* get the sum/min/max */
			sum += bl_value;
			Bmin = MIN(Bmin, bl_value);
			Bmax = MAX(Bmax, bl_value);
		}
	}

	if (Dbprint_lv == 1) {
		if (pr_flag) {
			pr_info("BL_TF(final out)[%d*%d] info(alpha:%d,): [\n",
				Vnum, Hnum, alpha);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d) ",
					FDat1->TF_BL_matrix[
						blkRow*Hnum + blkCol]);
				}
				pr_info(" ;\n ");
			}
			pr_info(" ]\n\n ");
		}
	}

	db_cnt++;
	if (db_cnt > 4095)
		db_cnt = 0;

	/* set the DC reduction for the BL_modeling */
	if (fw_LD_BLEst_ACmode == 0)
		nPRM1->reg_BL_matrix_AVG = 0;
	else if (fw_LD_BLEst_ACmode == 1)
		nPRM1->reg_BL_matrix_AVG = (sum/fw_blk_num);
	else if (fw_LD_BLEst_ACmode == 2)
		nPRM1->reg_BL_matrix_AVG = Bmin;
	else if (fw_LD_BLEst_ACmode == 3)
		nPRM1->reg_BL_matrix_AVG = Bmax;
	else if (fw_LD_BLEst_ACmode == 4)
		nPRM1->reg_BL_matrix_AVG = 2048;
	else
		nPRM1->reg_BL_matrix_AVG = 1024;
	nPRM1->reg_BL_matrix_Compensate = nPRM1->reg_BL_matrix_AVG;
}

#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
static unsigned long  black_time_cnt; /*black_display statue */
static void ld_fw_alg_frm_gxtvbb(struct LDReg *nPRM1, struct FW_DAT *FDat1,
	unsigned int *max_matrix, unsigned int *hist_matrix)
{
	/* Notes, nPRM will be set here in SW algorithm too */
	int dif, blkRow, blkCol, k, m, n, i, tmp;
	unsigned long sum;
	unsigned int avg, alpha = 256, Bmin, Bmax; /*local here*/
	unsigned int bl_value, bl_valuex128;
	static unsigned int  Map_bl_matrix[192];
	static unsigned int  Tf_bl_matrix_map[192];
	static unsigned int  TF_bl_matrix_t[384]; /*max=16*24*/
	static unsigned int  Map_bl_matrix_Compensate;
	static unsigned int  Map_bl_matrix_AVG;
	/*consistent with ldim hist_row*/
	unsigned int Vnum  = (nPRM1->reg_LD_BLK_Vnum);
	/*consistent with ldim hist_col*/
	unsigned int Hnum  = (nPRM1->reg_LD_BLK_Hnum);
	unsigned int *tBL_matrix;
	unsigned int BLmax = 4096;
	int stride = ldim_hist_col;/* inherit from ldim_hist_col */
	int RGBmax, Histmx, maxNB, curNB = 0;
	unsigned int fw_blk_num  = Vnum * Hnum;
	unsigned int fw_LD_Thist = ((fw_blk_num * 5) >> 2);
	/*consistent with ldim picture size: ldim_pic_row * ldim_pic_col*/
	unsigned int fw_pic_size =
			(nPRM1->reg_LD_pic_RowMax) * (nPRM1->reg_LD_pic_ColMax);
	int fw_LD_ThSF = 1600;
	unsigned int fw_LD_ThTF = 32;
	static unsigned int  Tf_luma_avg_frm[FRM_NUM_DBG] = {0, 0, 0, 0, 0};
	/* note!  when Hnum,Vnum changed.
	 * make sure Hnum*Vnum<= 16 or change 16 to other num
	 */
	static unsigned int  Tf_blkLuma_avg[FRM_NUM_DBG][16];
	static unsigned int  Tf_bl_matrix[FRM_NUM_DBG][16];
	static unsigned int  Tf_diff_frm_luma[FRM_NUM_DBG] = {0, 0, 0, 0, 0};
	static unsigned int  Tf_diff_blk_luma[FRM_NUM_DBG][16];
	unsigned int fw_LD_BLEst_ACmode = 1;
	int num = 0, mm = 0, nn = 0;
	unsigned int fw_hist_mx;
	unsigned int SF_sum = 0, TF_sum = 0;
	int int_x, rmd_x, norm;
	int left,  right, bl_value_map;
	int Luma_avg = 0, blkLuma_avg = 0, lmh_avg = 0, blk_sum = 0;
	int diff_frm_luma = 0, diff_blk_luma = 0;
	int diff_blk_matrix = 0, remap_value = 0;
	int dif_r = 0, dif_g = 0, dif_b = 0, adpt_alp = 0, dif_sum = 0;
	int tvalue = 0, min_diff = 0;
	int Bmax_lpf = 0;
	int frm_rgbmax = 0, black_blk_num = 0;
	int tdiff = 0, mx_diff = 0;

	/* for debug */
	unsigned int pr_flag = 0;
	static unsigned int frm_cnt;

	/*patch for fast luma change. */
	int step = 0;
	static unsigned int TF_bl_matrix_t2[16]; /*>Vnum*Hnum*/
	static unsigned int TF_bl_matrix_t3[16]; /*>Vnum*Hnum*/
	int diff_blk_matrix_tf_avg = 0;
	int Tf_diff_frm_luma_tf_avg = 0;
	int side_blk_diff = 0;

	fw_LD_ThSF = fw_LD_ThSF_l;
	fw_LD_ThTF = fw_LD_ThTF_l;

	tBL_matrix = FDat1->TF_BL_matrix_2;

	/* first 8 frame & every 8(fw_print_frequent) frames print once*/
	pr_flag = (frm_cnt < 8) || (frm_cnt % fw_print_frequent == 0);

	if ((Dbprint_lv == 1) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("input: Vnum = [%4d],Hnum = [%4d],\n"
			"RowMax=[%4d],ColMax=[%4d],ldim_hist_col=[%4d]\n",
				Vnum, Hnum,
				nPRM1->reg_LD_pic_RowMax,
				nPRM1->reg_LD_pic_ColMax,
				ldim_hist_col);
			if (fw_hist_print) {
				for (blkRow = 0; blkRow < Vnum; blkRow++) {
					for (blkCol = 0; blkCol < Hnum;
						blkCol++) {
						pr_info("(%d,%d,%d)\n ",
						max_matrix[blkRow*3*stride +
							blkCol*3 + 0],
						max_matrix[blkRow*3*stride +
							blkCol*3 + 1],
						max_matrix[blkRow*3*stride +
							blkCol*3 + 2]);
					}
					pr_info(";\n");
				}
				pr_info("];\n");
			}
		}
	}

	/* store 5 frames Luma_avg */
	for (i = 0; i < (FRM_NUM_DBG - 1); i++) {
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				tmp = blkRow * Hnum + blkCol;
				Tf_blkLuma_avg[i][tmp] =
					Tf_blkLuma_avg[i+1][tmp];
				Tf_bl_matrix[i][tmp] =
					Tf_bl_matrix[i+1][tmp];
				Tf_diff_blk_luma[i][tmp] =
					Tf_diff_blk_luma[i+1][tmp];
			}
		}
		Tf_luma_avg_frm[i] = Tf_luma_avg_frm[i+1];
		Tf_diff_frm_luma[i] = Tf_diff_frm_luma[i+1];
	}

	if ((Dbprint_lv == 2) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-pre:5 frames info:\n ");
			for (i = 0; i < FRM_NUM_DBG; i++) {
				for (blkRow = 0; blkRow < Vnum; blkRow++) {
					for (blkCol = 0; blkCol < Hnum;
						blkCol++) {
						tmp = blkRow * Hnum + blkCol;
						pr_info("Tf_blkLuma_avg[%d][%4d],Tf_bl_matrix[%d][%4d],Tf_diff_blk_luma[%d][%4d]\n",
						i,
						Tf_blkLuma_avg[i][tmp],
						i,
						Tf_bl_matrix[i][tmp],
						i,
						Tf_diff_blk_luma[i][tmp]);
					}
					pr_info(" ;\n ");
				}
				pr_info("];\n ");
			}
		}
	}

	frm_rgbmax = 0; black_blk_num = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			RGBmax = MAX(MAX(max_matrix[blkRow * 3 * stride +
				blkCol * 3], max_matrix[blkRow * 3 * stride +
				blkCol * 3 + 1]), max_matrix[blkRow * 3 *
				stride + blkCol * 3 + 2]);
			if ((RGBmax == 0) &&
				(hist_matrix[blkRow * LD_STA_BIN_NUM * stride +
				blkCol * LD_STA_BIN_NUM + 0] >= 1030800))
				black_blk_num++;
			frm_rgbmax = MAX(frm_rgbmax, RGBmax);
		}
	}

	/* calculate the current frame */
	sum = 0; Luma_avg = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			RGBmax = MAX(MAX(max_matrix[blkRow * 3 * stride +
				blkCol * 3], max_matrix[blkRow * 3 * stride +
				blkCol * 3 + 1]), max_matrix[blkRow * 3 *
				stride + blkCol * 3 + 2]);
			if (frm_rgbmax != 0) {
				if (RGBmax < rgb_base)
					RGBmax = rgb_base;
			}
			/* Consider the histogram */
			Histmx = 0;
			blkLuma_avg = 0;
			blk_sum = 0;
			for (k = 0; k < 16; k++) {
				Histmx += (hist_matrix[blkRow * LD_STA_BIN_NUM *
					stride + blkCol * LD_STA_BIN_NUM + k] *
					fw_LD_Whist[k]);
				blk_sum += hist_matrix[blkRow * LD_STA_BIN_NUM *
					stride + blkCol * LD_STA_BIN_NUM + k];
				blkLuma_avg += hist_matrix[blkRow *
					LD_STA_BIN_NUM * stride + blkCol *
					LD_STA_BIN_NUM + k] * k * 32;
			}
			Tf_blkLuma_avg[FRM_NUM_DBG - 1][tmp] =
				((blkLuma_avg + (blk_sum >> 1)) /
				(blk_sum + 1));
			Luma_avg += Tf_blkLuma_avg[FRM_NUM_DBG - 1][tmp];
			fw_hist_mx = ((Histmx >> 8) * fw_LD_Thist * 2 /
				(fw_pic_size >> 8));
			/* further debug */
			tBL_matrix[tmp] =
				((BLmax * MIN(fw_hist_mx, RGBmax)) >> 10);
		}
	}

	if ((Dbprint_lv == 3) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-0: BL_0[%d * %d]:\n ", Vnum, Hnum);
			pr_info(
			" LD_STA_BIN_NUM=%d, rgb_base=%ld, frm_rgbmax=%d [\n ",
				LD_STA_BIN_NUM,
				rgb_base,
				frm_rgbmax);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					tBL_matrix[blkRow*Hnum + blkCol]);
				}
				pr_info(" ;\n ");
			}
			pr_info("];\n ");
		}
	}

	/*To solve black/white woman flicker*/
	lmh_avg = (Luma_avg / (Vnum * Hnum));
	Tf_luma_avg_frm[FRM_NUM_DBG - 1] = lmh_avg;

	/* Spatial Filter the BackLits */
	sum = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			maxNB = 0;
			for (m =  -1; m < 2; m++) {
				for (n =  -1; n < 2; n++) {
					if ((m == 0) && (n == 0)) {
						curNB = tBL_matrix[tmp];
					} else if (((blkRow + m) >= 0) &&
						((blkRow + m) < Vnum) &&
						((blkCol + n) >= 0) &&
						((blkCol+n) < Hnum)) {
						maxNB = MAX(maxNB,
						tBL_matrix[(blkRow + m) * Hnum
							+ blkCol + n]);
					}
				}
			}
			/* SF matrix */
			if (Sf_bypass == 1) {
				FDat1->SF_BL_matrix[tmp] = tBL_matrix[tmp];
			} else {
				FDat1->SF_BL_matrix[tmp] =
					MAX(curNB, (maxNB - fw_LD_ThSF));
			}
			sum += FDat1->SF_BL_matrix[tmp];
			/* for SF_BL_matrix average calculation */
		}
	}

	if ((Dbprint_lv == 4) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-1: BL_SF:\n ");
			pr_info("Sf_bypass=%d, fw_LD_ThSF=%d, sum=%ld [\n ",
				Sf_bypass,
				fw_LD_ThSF,
				sum);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
						FDat1->SF_BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(" ;\n ");
			}
			pr_info("];\n ");
		}
	}

	/*side black bar detect*/
	avg = sum / fw_blk_num;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			dif = avg - (FDat1->SF_BL_matrix[tmp]);
			dif = ABS(dif);

			if ((blkCol == 0) &&
				(dif > bbd_th) &&
				bbd_detect_en) {
				/*1-0*/
				side_blk_diff =
					FDat1->SF_BL_matrix[tmp + 1] -
					FDat1->SF_BL_matrix[tmp];
				if (side_blk_diff > side_blk_diff_th) {
					FDat1->SF_BL_matrix[tmp] =
						FDat1->SF_BL_matrix[tmp + 1] -
						side_blk_diff_th; /*8-9*/
				}
			}
			if ((blkCol == (Hnum - 1)) &&
				(dif > bbd_th) &&
				bbd_detect_en) {
				side_blk_diff = FDat1->SF_BL_matrix[tmp - 1] -
					FDat1->SF_BL_matrix[tmp];
				if (side_blk_diff > side_blk_diff_th) {
					FDat1->SF_BL_matrix[tmp] =
						FDat1->SF_BL_matrix[tmp - 1] -
						side_blk_diff_th;
				}
			}
		}
	}

	if ((Dbprint_lv == 13) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-2.5: BL_SF_bbd:\n ");
			pr_info(
			" bbd_detect_en=%d, bbd_th=%d, side_blk_diff=%d,avg=%d\n ",
				bbd_detect_en,
				bbd_th,
				side_blk_diff,
				avg);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					FDat1->SF_BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info(" ];\n ");
		}
	}

	/* boost the bright region lights a little bit. */
	avg = ((sum * 7 / fw_blk_num) >> 3);
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			dif = (FDat1->SF_BL_matrix[tmp] - avg);

			FDat1->SF_BL_matrix[tmp] += 0;
			if (Boost_light_bypass == 0) {
				FDat1->SF_BL_matrix[tmp] =
					(FDat1->SF_BL_matrix[tmp] *
					boost_gain + 64) >> 7;
			}
			if (FDat1->SF_BL_matrix[tmp] > 4095)
				FDat1->SF_BL_matrix[tmp] = 4095;
		}
	}

	if ((Dbprint_lv == 5) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-2: BL_SF_boost:\n ");
			pr_info(
			" Boost_light_bypass=%d, boost_gain=%ld, avg=%d [\n ",
				Boost_light_bypass,
				boost_gain,
				avg);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					FDat1->SF_BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info(" ];\n ");
		}
	}

	SF_sum = 0;
	TF_sum = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			SF_sum += FDat1->SF_BL_matrix[blkRow * Hnum + blkCol];
			TF_sum += FDat1->TF_BL_matrix[blkRow * Hnum + blkCol];
		}
	}

	/* LPF  Only for Xiaomi 8 Led ,here Vnum = 1;*/
	Bmin = 4096; Bmax_lpf = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			sum = 0;
			if (LPF_method == 0) {/* 5*5 lpf_res=1024/25=41 */
				for (m = -2; m < 3; m++) {
					for (n = -2; n < 3; n++) {
						/* be careful here */
						mm = MIN(MAX(blkRow + m, 0),
							Vnum-1);
						nn = MIN(MAX(blkCol + n, 0),
							Hnum-1);
						num = MIN(MAX((mm * Hnum + nn),
							0), (Vnum * Hnum - 1));
						sum += FDat1->SF_BL_matrix[num];
					}
				}
			} else if (LPF_method == 1) {/*3*3,lpf_res=1024/9=114*/
				for (m = -1; m < 2; m++) {
					for (n = -1; n < 2; n++) {
						/* be careful here */
						mm = MIN(MAX(blkRow + m, 0),
							Vnum-1);
						nn = MIN(MAX(blkCol + n, 0),
							Hnum-1);
						num = MIN(MAX((mm * Hnum + nn),
							0),
							(Vnum * Hnum - 1));
						sum += FDat1->SF_BL_matrix[num];
					}
				}
			} else if (LPF_method == 2) {/*7*7,lpf_res=1024/49=21*/
				for (m = -3; m < 4; m++) {
					for (n = -3; n < 4; n++) {
						/* be careful here */
						mm = MIN(MAX(blkRow + m, 0),
							Vnum-1);
						nn = MIN(MAX(blkCol + n, 0),
							Hnum-1);
						num = MIN(MAX((mm * Hnum + nn),
							0),
							(Vnum * Hnum - 1));
						sum += FDat1->SF_BL_matrix[num];
					}
				}
			} else if (LPF_method == 3) {/*9*9,lpf_res=1024/81=13*/
				for (m = -4; m < 5; m++) {
					for (n = -4; n < 5; n++) {
						/* be careful here */
						mm = MIN(MAX(blkRow + m, 0),
							Vnum-1);
						nn = MIN(MAX(blkCol + n, 0),
							Hnum-1);
						num = MIN(MAX((mm * Hnum + nn),
							0),
							(Vnum * Hnum - 1));
						sum += FDat1->SF_BL_matrix[num];
					}
				}
			}

			if (Lpf_bypass == 0) {
				FDat1->SF_BL_matrix[tmp] =
				(((sum * lpf_res >> 10) * lpf_gain) >> 7);
			}

			/*check if sum overflow*/
			if (Dbprint_lv == 6)
				pr_info("step 3.1: sum=%ld:\n ", sum);
			tvalue = FDat1->SF_BL_matrix[tmp];
			tvalue = (tvalue * ov_gain + 16) >> 4;
			min_diff = tvalue - FDat1->SF_BL_matrix[tmp];
			Bmin = MIN(Bmin, min_diff);

			Bmax_lpf = MAX(Bmax_lpf,
				Tf_bl_matrix[FRM_NUM_DBG-2][tmp]);

			if (FDat1->SF_BL_matrix[tmp] > 4095)
				FDat1->SF_BL_matrix[tmp] = 4095;
		}
	}

	if ((Dbprint_lv == 6) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step 3.1: BL_SF_lpf:\n ");
			pr_info(" LPF_method=%d, Lpf_bypass=%d, lpf_res=%ld, lpf_gain=%ld\n",
				LPF_method,
				Lpf_bypass,
				lpf_res,
				lpf_gain);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					FDat1->SF_BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
			pr_info("Bmax_lpf=%d, Bmin=%d\n ", Bmax_lpf, Bmin);
		}
	}


	/*hivicast00082*/
	if (lmh_avg > lmh_avg_TH) {
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				tmp = blkRow * Hnum + blkCol;
				tdiff = max_luma - FDat1->SF_BL_matrix[tmp];
				mx_diff = MAX(tdiff, 0);

				FDat1->SF_BL_matrix[tmp] =
					FDat1->SF_BL_matrix[tmp] +
					((mx_diff + 1) >> 1);
			}
		}
	}
	if ((Dbprint_lv == 7) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-3.2:BL_SF(hivicast00082:\n ");
			pr_info(" max_luma=%d, mx_diff=%d,lmh_avg_TH=%d\n ",
				max_luma,
				mx_diff,
				lmh_avg_TH);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					FDat1->SF_BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
		}
	}

	/* Temperary filter */
	sum = 0; Bmin = 4096; Bmax = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			tmp = blkRow * Hnum + blkCol;
			/* Optimization needed here */
			dif_r = FDat1->last_STA1_MaxRGB[blkRow * 3 * stride
				+ blkCol * 3] - max_matrix[blkRow * 3 * stride
				+ blkCol * 3];
			dif_r = ABS(dif_r);
			dif_g = FDat1->last_STA1_MaxRGB[blkRow * 3 * stride
				+ blkCol * 3 + 1] - max_matrix[blkRow * 3
				* stride + blkCol * 3 + 1];
			dif_g = ABS(dif_g);
			dif_b = FDat1->last_STA1_MaxRGB[blkRow * 3 * stride
				+ blkCol * 3 + 2] - max_matrix[blkRow * 3
				* stride + blkCol * 3 + 2];
			dif_b = ABS(dif_b);
			adpt_alp = (FDat1->SF_BL_matrix[tmp]) -
				(FDat1->TF_BL_matrix[tmp]);
			adpt_alp = ABS(adpt_alp);

			dif_sum = SF_sum - TF_sum;
			dif_sum = ABS(dif_sum);
			/*note: T968&T960 only use current BL_matrix:
			 *TF_alpha =256,fw_LD_ThTF=256
			 *20180612
			 */
			/*note: manual set TF_alpha no useful anymore*/
			TF_alpha = 1 << TF_FRESH_BL;
			if (dif_sum > fw_rgb_diff_th) /* 32760 */
				alpha = TF_alpha; /* 256 */
			else
				alpha = MIN(TF_alpha, fw_LD_ThTF);
			bl_valuex128 = 0;

			FDat1->TF_BL_alpha[tmp] = alpha;

			/*4.1: TF: get the temporary filtered BL_value */
			bl_value = (((TF_alpha - alpha) *
				(FDat1->TF_BL_matrix[tmp]) +
				alpha * (FDat1->SF_BL_matrix[tmp]) +
				(TF_alpha >> 1)) >> TF_FRESH_BL);
				/*2^8 = TF_alpha*/
			if (bl_value > 4095)
				bl_value = 4095;
			/* for debug */
			TF_bl_matrix_t[tmp] = bl_value;

			/*4.2: remap: ld_curve map*/
			int_x = bl_value >> 8;
			rmd_x = bl_value % 256;
			norm  = 256;
			if (int_x == 0)
				left = 0;
			else
				left = bl_remap_curve[int_x - 1];

			right = bl_remap_curve[int_x];

			if (Ld_remap_bypass == 1) {
				bl_value_map = bl_value;
			} else {
				bl_value_map = left +
					(((right-left) * rmd_x + 128) >> 8);
			}
			bl_value_map = (bl_value_map > 4095) ? 4095 :
				bl_value_map;

			Tf_bl_matrix_map[tmp] = bl_value_map;

			/*4.3 tiir optimize */
			diff_frm_luma = Tf_luma_avg_frm[FRM_NUM_DBG - 1] -
				Tf_luma_avg_frm[FRM_NUM_DBG - 2];
			diff_frm_luma = ABS(diff_frm_luma);
			Tf_diff_frm_luma[FRM_NUM_DBG - 1] = diff_frm_luma;
			if (Tf_luma_avg_frm[FRM_NUM_DBG - 1] <= 1) {
				/* Tf_bl_matrix[FRM_NUM_DBG - 2][tmp] =
				 *	Bmax_lpf>>1;
				 */
				Tf_bl_matrix[FRM_NUM_DBG - 2][tmp] =
					Tf_bl_matrix[FRM_NUM_DBG - 2][tmp];
				/* do nothing. need to optimise */
			}
			if (diff_frm_luma < 10) {/*same video scene*/
				diff_blk_luma =
					Tf_blkLuma_avg[FRM_NUM_DBG - 1][tmp] -
					Tf_blkLuma_avg[FRM_NUM_DBG - 2][tmp];
				diff_blk_matrix = bl_value_map -
					Tf_bl_matrix[FRM_NUM_DBG-2][tmp];
				diff_blk_matrix = ABS(diff_blk_matrix);
				Tf_diff_blk_luma[FRM_NUM_DBG - 1][tmp] =
					diff_blk_matrix;

				/*Debug print local value*/
				if (Dbprint_lv == 200) {
					if ((db_cnt % 4) == 0) {
						/*4 frames print once*/
						pr_info(
						"##4.3_tiir-1: diff_frm_luma <10\n"
						"diff_blk_matrix[%4d]\n"
						"bl_value_map[%4d]\n"
						"Tf_bl_matrix[%4d]\n",
						diff_blk_matrix,
						bl_value_map,
						Tf_bl_matrix[FRM_NUM_DBG - 2]
							[tmp]);
					}
				}
				if ((diff_blk_matrix > 50) &&
					(diff_blk_luma_en)) {
					if (bl_value_map >=
					Tf_bl_matrix[FRM_NUM_DBG-2][tmp]) {
						bl_value_map =
						Tf_bl_matrix[FRM_NUM_DBG-2]
							[tmp] +
						((diff_blk_matrix + 16) >> 5);
					} else { /* any difference ? */
						bl_value_map =
						Tf_bl_matrix[FRM_NUM_DBG-2]
							[tmp] -
						((diff_blk_matrix + 16) >> 5);
					}
					if (Dbprint_lv == 2) {
						/* just for confirm */
						pr_info("##--2: diff_blk_matrix >50\n");
					}

					if ((Dbprint_lv == 200) &&
						((db_cnt % 4) == 0)) {
						/*4 frames print once*/
						pr_info(
						"##4.3_tiir-2: diff_blk_matrix > 50\n"
						"bl_value_map= [%4d]\n"
						"blkRow * Hnum + blkCol = %d(cnt)\n"
						"Tf_bl_matrix[FRM_NUM_DBG-1][cnt])= [%4d]\n"
						"Tf_bl_matrix[FRM_NUM_DBG-2][cnt])= [%4d]\n"
						"Tf_bl_matrix[FRM_NUM_DBG-3][cnt])= [%4d]\n"
						"Tf_bl_matrix[FRM_NUM_DBG-4][cnt])= [%4d]\n"
						"Tf_bl_matrix[FRM_NUM_DBG-5][cnt])= [%4d]\n",
						bl_value_map,
						tmp,
						Tf_bl_matrix[FRM_NUM_DBG - 1]
							[tmp],
						Tf_bl_matrix[FRM_NUM_DBG - 2]
							[tmp],
						Tf_bl_matrix[FRM_NUM_DBG - 3]
							[tmp],
						Tf_bl_matrix[FRM_NUM_DBG - 4]
							[tmp],
						Tf_bl_matrix[FRM_NUM_DBG - 5]
							[tmp]);
					}
				} else
					bl_value_map = bl_value_map;

				if (bl_value_map > 4095)
					bl_value_map = 4095;
				if (bl_value_map <= 0)
					bl_value_map = 0;
			}

			/* for debug */
			TF_bl_matrix_t3[tmp] = bl_value_map;

			/*4.4 patch for fast luma change.20180621*/
			diff_frm_luma = Tf_luma_avg_frm[FRM_NUM_DBG - 1] -
				Tf_luma_avg_frm[FRM_NUM_DBG - 2];/*0~512*/
			/*current*/
			Tf_diff_frm_luma[FRM_NUM_DBG - 1] = diff_frm_luma;
			Tf_diff_frm_luma_tf_avg =
				(Tf_diff_frm_luma[FRM_NUM_DBG - 1] +
				Tf_diff_frm_luma[FRM_NUM_DBG - 2] +
				Tf_diff_frm_luma[FRM_NUM_DBG - 3] +
				Tf_diff_frm_luma[FRM_NUM_DBG - 4]) * 2;
			/*(/4*8,12bit)4 frames avg. note: FRM_NUM_DBG >=4*/

			diff_blk_matrix = bl_value_map -
				Tf_bl_matrix[FRM_NUM_DBG-2][tmp];
			diff_blk_matrix_tf_avg =
				(Tf_diff_blk_luma[FRM_NUM_DBG - 2][tmp] +
				Tf_diff_blk_luma[FRM_NUM_DBG - 3][tmp] +
				Tf_diff_blk_luma[FRM_NUM_DBG - 4][tmp] +
				Tf_diff_blk_luma[FRM_NUM_DBG - 5][tmp]) >> 2;
			/*make sure FRM_NUM_DBG > 5 */

			if ((dif_sum < fw_rgb_diff_th) &&
				(ABS(diff_frm_luma*8) > LD_TF_STEP_TH)) {

				if (TF_step_method == 1) {
					step = diff_frm_luma * 8;/*12bit*/
				/* dark --> bright, limit increase step,u12*/
					if (step > LD_TF_STEP_TH)
						step = LD_TF_STEP_TH;/*12bit*/
				/*bright --> dark, limit decrease step,u12*/
					else if (step < (-LD_TF_STEP_TH))
						step = -(LD_TF_STEP_TH);
					bl_value_map =
					(FDat1->TF_BL_matrix[tmp]) + step;
				} else if (TF_step_method == 2) {
					step =
					(Tf_diff_frm_luma[FRM_NUM_DBG - 1] +
					Tf_diff_frm_luma[FRM_NUM_DBG - 2] +
					Tf_diff_frm_luma[FRM_NUM_DBG - 3] +
					Tf_diff_frm_luma[FRM_NUM_DBG - 4]) * 2;
			/*(/4*8,12bit)4 frames avg. note: FRM_NUM_DBG >=4*/

				/* dark --> bright, limit increase step,u12*/
					if (step > LD_TF_STEP_TH)
						step = LD_TF_STEP_TH ;/*12bit*/
				/*bright --> dark, limit decrease step,u12*/
					else if (step < (-LD_TF_STEP_TH))
						step = -(LD_TF_STEP_TH);

					bl_value_map =
					(FDat1->TF_BL_matrix[tmp]) + step;
				} else if (TF_step_method == 3) {
					diff_blk_matrix = bl_value_map -
					Tf_bl_matrix[FRM_NUM_DBG-2][tmp];

					if (diff_blk_matrix > 0) {
						bl_value_map =
						Tf_bl_matrix[FRM_NUM_DBG-2][tmp]
						+ ((diff_blk_matrix +
						(1 << (TF_BLK_FRESH_BL-1))) >>
							TF_BLK_FRESH_BL);
					} else {
						bl_value_map =
						Tf_bl_matrix[FRM_NUM_DBG-2][tmp]
						- ((ABS(diff_blk_matrix) +
						(1 << (TF_BLK_FRESH_BL-1))) >>
							TF_BLK_FRESH_BL);
					}
				} else
					bl_value_map = bl_value_map;
			}

			if (Dbprint_lv == 100 && pr_flag) {
				pr_info("new TF filter:row[%d],col[%d]:dif_sum=%d(th(%d))\n",
					blkRow, blkCol, dif_sum,
					fw_rgb_diff_th);
				pr_info("diff_frm_luma:%4d,step:%4d, LD_TF_STEP_TH:%4d (pre_bl[%4d] => bl[%4d])\n\n",
					diff_frm_luma,
					step, LD_TF_STEP_TH,
					(FDat1->TF_BL_matrix[tmp]),
					bl_value_map);
			}

			if (Dbprint_lv == 100 && pr_flag &&
				(TF_step_method == 3)) {
				pr_info("new TF filter:row[%d],col[%d]:dif_sum=%d(th(%d))\n",
					blkRow, blkCol, dif_sum,
					fw_rgb_diff_th);
				pr_info("diff_frm_luma:%4d,diff_blk_matrix:%4d, TF_BLK_FRESH_BL:%4d (pre_bl[%4d] => bl[%4d])\n\n",
					diff_frm_luma,
					diff_blk_matrix, TF_BLK_FRESH_BL,
					Tf_bl_matrix[FRM_NUM_DBG-2][tmp],
					bl_value_map);
			}

			/* debug */
			TF_bl_matrix_t2[tmp] = bl_value_map;

			remap_value = bl_value_map;
			if (remap_value > 4095)
				remap_value = 4095;
			if (nPRM1->reg_LD_BackLit_mode == 1) {
				nPRM1->BL_matrix[blkCol * Vnum + blkRow] =
					bl_value_map;
				Map_bl_matrix[blkCol * Vnum + blkRow] =
					remap_value;
			} else {
				nPRM1->BL_matrix[tmp] = bl_value_map;
				Map_bl_matrix[tmp] = remap_value;
			}

			/* Get the TF_BL_matrix */
			FDat1->TF_BL_matrix[tmp] = bl_value_map;
			Tf_bl_matrix[FRM_NUM_DBG-1][tmp] = bl_value_map;

			/* leave the Delayed version for next frame */
			for (k = 0; k < 3; k++) {
				FDat1->last_STA1_MaxRGB[blkRow * 3 * stride +
					blkCol * 3 + k] =
						max_matrix[blkRow * 3 * stride +
						blkCol * 3 + k];
			}
			/* get the sum/min/max */
			sum += bl_value_map;
			Bmin = MIN(Bmin, bl_value_map);
			Bmax = MAX(Bmax, bl_value_map);
		}
	}

	if ((Dbprint_lv == 8) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-4.1: BL_TF: [\n ");
			pr_info("alpha =%d, dif_sum =%d\n ", alpha, dif_sum);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					TF_bl_matrix_t[blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n");
		}
	}

	if ((Dbprint_lv == 9) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-4.2: BL_remap: [\n ");
			pr_info("Ld_remap_bypass =%d\n ", Ld_remap_bypass);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					Tf_bl_matrix_map[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
		}
	}

	if ((Dbprint_lv == 10) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-4.3: BL_remap_patch_old: [\n ");
			pr_info(" diff_frm_luma=%d,\n ", diff_frm_luma);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					TF_bl_matrix_t3[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
		}
	}

	if ((Dbprint_lv == 11) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step-4.4: BL_remap_patch_new: [\n ");
			pr_info(" diff_frm_luma=%d, dif_sum=%d, LD_TF_STEP_TH=%d, TF_step_method=%d\n ",
				diff_frm_luma,
				dif_sum,
				LD_TF_STEP_TH,
				TF_step_method);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
						TF_bl_matrix_t2
						[blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
		}
	}
	if ((Dbprint_lv == 12) || (Dbprint_lv == 1024)) {
		if (pr_flag) {
			pr_info("step 5: BL_TF_optimise(final BL_matrix):\n ");
			pr_info("frm_print_cnt = %d, diff_frm_luma =%d, diff_blk_matrix =%d\n ",
				fw_print_frequent,
				diff_frm_luma,
				diff_blk_matrix);
			for (blkRow = 0; blkRow < Vnum; blkRow++) {
				for (blkCol = 0; blkCol < Hnum; blkCol++) {
					pr_info("(%4d)\n",
					nPRM1->BL_matrix[
						blkRow * Hnum + blkCol]);
				}
				pr_info(";\n ");
			}
			pr_info("];\n ");
		}
	}

	db_cnt++;
	if (db_cnt > 4095)
		db_cnt = 0;

	frm_cnt++;
	if (frm_cnt > 4095)
		frm_cnt = 0;

	/* set the DC reduction for the BL_modeling */
	if (fw_LD_BLEst_ACmode == 0)
		nPRM1->reg_BL_matrix_AVG = 0;
	else if (fw_LD_BLEst_ACmode == 1) {
		/*nPRM->reg_BL_matrix_AVG = (sum/fw_blk_num);*/
		nPRM1->reg_BL_matrix_AVG = ((sum / fw_blk_num) *
			avg_gain + 2048) >> 12;
		Map_bl_matrix_AVG = ((sum / fw_blk_num) *
			avg_gain + 2048) >> 12;
		Map_bl_matrix_Compensate = ((sum / fw_blk_num) *
			avg_gain + 2048) >> 12;
	} else if (fw_LD_BLEst_ACmode == 2)
		nPRM1->reg_BL_matrix_AVG = Bmin;
	else if (fw_LD_BLEst_ACmode == 3)
		nPRM1->reg_BL_matrix_AVG = Bmax;
	else if (fw_LD_BLEst_ACmode == 4)
		nPRM1->reg_BL_matrix_AVG = 2048;
	else
		nPRM1->reg_BL_matrix_AVG = 1024;

	nPRM1->reg_BL_matrix_Compensate = nPRM1->reg_BL_matrix_AVG;

	/*black_display statue */
	if (Hnum == 10) {
		if (frm_rgbmax < 10) {
			black_time_cnt++;
			if (black_time_cnt > 120)
				black_frm = 1;
		} else {
			black_time_cnt = 0;
			black_frm = 0;
			ldim_level_update = 1;
		}
	} else {
		if (frm_rgbmax < 10) {
			black_frm = 1;
		} else {
			black_frm = 0;
			ldim_level_update = 1;
		}
	}
	/*note: black_frm should co-work with brightness controlling(spi)*/
}
#endif

void LDIM_WR_BASE_LUT_DRT(int base, int *pData, int len)
{
	int i;
	int addr;

	addr  = base;/*(base<<4)*/
#ifdef CONFIG_VSYNC_RDMA
	VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, addr);
	for (i = 0; i < len; i++)
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, pData[i]);
#else
	Wr(LDIM_BL_ADDR_PORT, addr);
	for (i = 0; i < len; i++)
		Wr(LDIM_BL_DATA_PORT, pData[i]);
#endif
}

static void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		unsigned int BLK_Vnum, unsigned int BLK_Hnum)
{
	unsigned int resolution, resolution_region, blk_height, blk_width;
	unsigned int row_start, col_start;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	BLK_Vnum = (BLK_Vnum == 0) ? 1 : BLK_Vnum;
	BLK_Hnum = (BLK_Hnum == 0) ? 1 : BLK_Hnum;
	LDIMPR("%s: %d %d %d %d\n", __func__, pic_h, pic_v, BLK_Vnum, BLK_Hnum);

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*Wr(VDIN0_HIST_CTRL, 0x10d);*/

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		Wr(LDIM_STTS_CTRL0, 7 << 2);
		ldim_set_matrix_ycbcr2rgb();
		ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);
		break;
	case BL_CHIP_GXTVBB:
		Wr(LDIM_STTS_CTRL0, 3<<3);/*4 mux to vpp_dout*/
		ldim_set_matrix_ycbcr2rgb();
		/*ldim_set_matrix_rgb2ycbcr(0);*/
		ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);
		break;
	default:
		break;
	}

	resolution_region = 0;

	blk_height = (pic_v - 8) / BLK_Vnum;
	blk_width = (pic_h - 8) / BLK_Hnum;
	row_start = (pic_v - (blk_height * BLK_Vnum)) >> 1;
	col_start = (pic_h - (blk_width * BLK_Hnum)) >> 1;
	ldim_set_region(0, blk_height, blk_width,
		row_start, col_start, BLK_Hnum);
}

static void ldim_db_para_print(struct LDReg *mLDReg)
{
	int i, len;
	char *buf;

	len = 32 * 10 + 10;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		return;
	}

	LDIMPR("rgb_base = %ld\n"
		"boost_gain = %ld\n"
		"lpf_res = %ld\n"
		"fw_LD_ThSF = %ld\n\n",
		rgb_base,
		boost_gain,
		lpf_res,
		fw_LD_ThSF_l);

	LDIMPR("ld_vgain = %d\n"
		"ld_hgain = %d\n"
		"ld_litgain = %d\n\n",
		mLDReg->reg_LD_Vgain,
		mLDReg->reg_LD_Hgain,
		mLDReg->reg_LD_Litgain);

	LDIMPR("ld_lut_vdg_lext = %d\n"
		"ld_lut_hdg_lext = %d\n"
		"ld_lut_vhk_lext = %d\n\n",
		mLDReg->reg_LD_LUT_Vdg_LEXT,
		mLDReg->reg_LD_LUT_Hdg_LEXT,
		mLDReg->reg_LD_LUT_VHk_LEXT);

	len = 0;
	LDIMPR("ld_lut_hdg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_Hdg[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vdg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_Vdg[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vhk:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_VHk[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vhk_pos:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_VHk_pos[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vhk_neg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_VHk_neg[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_hhk:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_HHk[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vho_pos:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_VHo_pos[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	len = 0;
	LDIMPR("ld_lut_vho_neg:\n");
	for (i = 0; i < 32; i++) {
		len += sprintf(buf+len, "\t%d",
			mLDReg->reg_LD_LUT_VHo_neg[i]);
		if (i % 8 == 7)
			len += sprintf(buf+len, "\n");
	}
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_db_load_update(struct LDReg *mLDReg)
{
	int i;

	if (LDIM_DATA_FROM_DB == 1) {
		LDIMPR("ldim_db_load_update\n");
		/* beam model */
		rgb_base = (unsigned long)ldim_db_para.rgb_base;
		boost_gain = (unsigned long)ldim_db_para.boost_gain;
		lpf_res = (unsigned long)ldim_db_para.lpf_res;
		fw_LD_ThSF_l = (unsigned long)ldim_db_para.fw_ld_th_sf;

		/* beam curve */
		mLDReg->reg_LD_Vgain = ldim_db_para.ld_vgain;
		mLDReg->reg_LD_Hgain = ldim_db_para.ld_hgain;
		mLDReg->reg_LD_Litgain = ldim_db_para.ld_litgain;

		mLDReg->reg_LD_LUT_Vdg_LEXT = ldim_db_para.ld_lut_vdg_lext;
		mLDReg->reg_LD_LUT_Hdg_LEXT = ldim_db_para.ld_lut_hdg_lext;
		mLDReg->reg_LD_LUT_VHk_LEXT = ldim_db_para.ld_lut_vhk_lext;

		for (i = 0; i < 32; i++) {
			mLDReg->reg_LD_LUT_Hdg[i] = ldim_db_para.ld_lut_hdg[i];
			mLDReg->reg_LD_LUT_Vdg[i] = ldim_db_para.ld_lut_vdg[i];
			mLDReg->reg_LD_LUT_VHk[i] = ldim_db_para.ld_lut_vhk[i];
		}

		/* beam shape minor adjustment */
		for (i = 0; i < 32; i++) {
			mLDReg->reg_LD_LUT_VHk_pos[i] =
				ldim_db_para.ld_lut_vhk_pos[i];
			mLDReg->reg_LD_LUT_VHk_neg[i] =
				ldim_db_para.ld_lut_vhk_neg[i];
			mLDReg->reg_LD_LUT_HHk[i] =
				ldim_db_para.ld_lut_hhk[i];
			mLDReg->reg_LD_LUT_VHo_pos[i] =
				ldim_db_para.ld_lut_vho_pos[i];
			mLDReg->reg_LD_LUT_VHo_neg[i] =
				ldim_db_para.ld_lut_vho_neg[i];
		}

		/* remapping */
		/*ldim_db_para.lit_idx_th;*/
		/*ldim_db_para.comp_gain;*/
	}

	if (db_print_flag == 1)
		ldim_db_para_print(mLDReg);
}

static void LDIM_Initial_GXTVBB(unsigned int ldim_bl_en,
		unsigned int ldim_hvcnt_bypass)
{
	unsigned int i, j, k;
	unsigned int data;
	unsigned int *arrayTmp;

	arrayTmp = kcalloc(1536, sizeof(unsigned int), GFP_KERNEL);
	if (arrayTmp == NULL) {
		LDIMERR("%s malloc error\n", __func__);
		return;
	}

	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data & (~(3<<4));
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	/*change here: gBLK_Hidx_LUT: s14*19 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_HIDX_BASE,
			nPRM.reg_LD_BLK_Hidx, 16, LD_BLK_LEN_H);
	/* change here: gBLK_Vidx_LUT: s14*19 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE,
			nPRM.reg_LD_BLK_Vidx, 16, LD_BLK_LEN_V);
	/* change here: gHDG_LUT: u10*32  */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE,
			nPRM.reg_LD_LUT_Hdg, 16, LD_LUT_LEN);
	/* change here: gVDG_LUT: u10*32 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE,
			nPRM.reg_LD_LUT_Vdg, 16, LD_LUT_LEN);
	/* change here: gVHk_LUT: u10*32  */
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE,
			nPRM.reg_LD_LUT_VHk, 16, LD_LUT_LEN);
	/* reg_LD_LUT_VHk_pos[32]/reg_LD_LUT_VHk_neg[32]: u8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHk_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHk_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE, arrayTmp, 8, 64);
	/* reg_LD_LUT_VHo_pos[32]/reg_LD_LUT_VHo_neg[32]: s8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHo_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHo_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE, arrayTmp, 8, 64);
	/* reg_LD_LUT_HHk[32]:u8 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HHK_BASE, nPRM.reg_LD_LUT_HHk, 8, 32);
	/*gLD_REFLECT_DGR_LUT: u6 * (20+20+4) */
	for (i = 0; i < 20; i++)
		arrayTmp[i] = nPRM.reg_LD_Reflect_Hdgr[i];
	for (i = 0; i < 20; i++)
		arrayTmp[20+i] = nPRM.reg_LD_Reflect_Vdgr[i];
	for (i = 0; i < 4; i++)
		arrayTmp[40+i] = nPRM.reg_LD_Reflect_Xdgr[i];
	LDIM_WR_BASE_LUT(REG_LD_REFLECT_DGR_BASE, arrayTmp, 8, 44);
	/* X_lut: 12 * 3*16*32 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 16; j++)
			for (k = 0; k < 32; k++)
				arrayTmp[16*32*i+32*j+k] = nPRM.X_lut[i][j][k];
	LDIM_WR_BASE_LUT(REG_LD_RGB_LUT_BASE, arrayTmp, 16, 32*16*3);
	/* X_nrm: 4 * 16 */
	LDIM_WR_BASE_LUT(REG_LD_RGB_NRMW_BASE, nPRM.X_nrm[0], 4, 16);
	/*  X_idx: 12*16  */
	/*LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM.X_idx[0], 12, 16);*/
	LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM.X_idx[0], 16, 16);
	/*  gMatrix_LUT: u12*LD_BLKREGNUM  */
	LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE, nPRM.BL_matrix, LD_BLKREGNUM);
	/*  LD_FRM_SIZE  */
	data = ((nPRM.reg_LD_pic_RowMax&0xfff)<<16) |
		(nPRM.reg_LD_pic_ColMax&0xfff);
	LDIM_WR_32Bits(REG_LD_FRM_SIZE, data);
	/* LD_RGB_MOD */
	data = ((nPRM.reg_LD_RGBmapping_demo & 0x1) << 19) |
		((nPRM.reg_LD_X_LUT_interp_mode[2] & 0x1) << 18) |
		((nPRM.reg_LD_X_LUT_interp_mode[1] & 0x1) << 17) |
		((nPRM.reg_LD_X_LUT_interp_mode[0] & 0x1) << 16) |
		((nPRM.reg_LD_BkLit_LPFmod & 0x7) << 12) |
		((nPRM.reg_LD_Litshft  & 0x7) << 8) |
		((nPRM.reg_LD_BackLit_Xtlk & 0x1) << 7) |
		((nPRM.reg_LD_BkLit_Intmod & 0x1) << 6) |
		((nPRM.reg_LD_BkLUT_Intmod & 0x1) << 5) |
		((nPRM.reg_LD_BkLit_curmod & 0x1) << 4) |
		((nPRM.reg_LD_BackLit_mode & 0x3));
	LDIM_WR_32Bits(REG_LD_RGB_MOD, data);
	/* LD_BLK_HVNUM  */
	data = ((nPRM.reg_LD_Reflect_Vnum & 0x7) << 20) |
		((nPRM.reg_LD_Reflect_Hnum & 0x7) << 16) |
		((nPRM.reg_LD_BLK_Vnum & 0x3f) << 8) |
		((nPRM.reg_LD_BLK_Hnum & 0x3f));
	LDIM_WR_32Bits(REG_LD_BLK_HVNUM, data);
	/* REG_LD_FRM_HBLAN_VHOLS  */
	data = ((nPRM.reg_LD_LUT_VHo_LS & 0x7) << 16) |
		((6 & 0x1fff)) ;  /*frm_hblank_num */
	LDIM_WR_32Bits(REG_LD_FRM_HBLAN_VHOLS, data);
	/* LD_HVGAIN */
	data = ((nPRM.reg_LD_Vgain & 0xfff) << 16) |
		(nPRM.reg_LD_Hgain & 0xfff);
	LDIM_WR_32Bits(REG_LD_HVGAIN, data);
	/*  LD_LIT_GAIN_COMP */
	data = ((nPRM.reg_LD_Litgain & 0xfff) << 16) |
		(nPRM.reg_BL_matrix_Compensate & 0xfff);
	LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	/*  LD_BKLIT_VLD  */
	data = 0;
	for (i = 0; i < 32; i++)
		if (nPRM.reg_LD_BkLit_valid[i])
			data = data | (1<<i);
	LDIM_WR_32Bits(REG_LD_BKLIT_VLD, data);
	/* LD_BKLIT_PARAM */
	data = ((nPRM.reg_LD_BkLit_Celnum & 0xff) << 16) |
		(nPRM.reg_BL_matrix_AVG & 0xfff);
	LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);
	/* REG_LD_LUT_XDG_LEXT */
	data = ((nPRM.reg_LD_LUT_Vdg_LEXT & 0x3ff) << 20) |
		((nPRM.reg_LD_LUT_VHk_LEXT & 0x3ff) << 10) |
		(nPRM.reg_LD_LUT_Hdg_LEXT & 0x3ff);
	LDIM_WR_32Bits(REG_LD_LUT_XDG_LEXT, data);

	/* LD_FRM_RST_POS */
	data = (16<<16) | (3); /* h=16,v=3 :ldim_param_frm_rst_pos */
	LDIM_WR_32Bits(REG_LD_FRM_RST_POS, data);
	/* LD_FRM_BL_START_POS */
	data = (16<<16) | (4); /* ldim_param_frm_bl_start_pos; */
	LDIM_WR_32Bits(REG_LD_FRM_BL_START_POS, data);

	/* REG_LD_XLUT_DEMO_ROI_XPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_xend & 0x1fff) << 16) |
		(nPRM.reg_LD_xlut_demo_roi_xstart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_XPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_YPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_yend & 0x1fff) << 16) |
		(nPRM.reg_LD_xlut_demo_roi_ystart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_YPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_CTRL */
	data = ((nPRM.reg_LD_xlut_oroi_enable & 0x1) << 1) |
		(nPRM.reg_LD_xlut_iroi_enable & 0x1);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_CTRL, data);

	/* REG_LD_MISC_CTRL0 {ram_clk_gate_en,2'h0,ldlut_ram_sel,ram_clk_sel,
	 * reg_hvcnt_bypass,reg_ldim_bl_en,soft_bl_start,reg_soft_rst)
	 */
	data = (0 << 1) | (ldim_bl_en << 2) |
		(ldim_hvcnt_bypass << 3) | (3 << 4) | (1 << 8);
	/* ldim_param_misc_ctrl0; */
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	kfree(arrayTmp);
}

static int LDIM_Update_Matrix(int NewBlMatrix[], int BlMatrixNum)
{
	int data;

	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	if (data & (1 << 12)) {  /*bl_ram_mode=1;*/
		if (LDIM_RD_32Bits(REG_LD_BLMAT_RAM_MISC) & 0x10000)
			/*Previous Matrix is not used*/
			goto Previous_Matrix;
		else {
			LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
				NewBlMatrix, BlMatrixNum);
			LDIM_WR_32Bits(REG_LD_BLMAT_RAM_MISC,
				(BlMatrixNum & 0x1ff) | (1 << 16));
			/*set Matrix update ready*/

			return 0;
		}
	} else {  /*bl_ram_mode=0*/
		/*set ram_clk_sel=0, ram_bus_sel = 0*/
		data = data & (~(3 << 9));
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			NewBlMatrix, BlMatrixNum);
		data = data | (3 << 9); /*set ram_clk_sel=1, ram_bus_sel = 1*/
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

		return 0;
	}

Previous_Matrix:
	return 1;
}

static void LDIM_Initial_TXLX(unsigned int ldim_bl_en,
		unsigned int ldim_hvcnt_bypass)
{
	unsigned int i;
	unsigned int data;
	unsigned int *arrayTmp;

	arrayTmp = kcalloc(1536, sizeof(unsigned int), GFP_KERNEL);
	if (arrayTmp == NULL) {
		LDIMERR("%s malloc error\n", __func__);
		return;
	}

	/*  LD_FRM_SIZE  */
	data = ((nPRM.reg_LD_pic_RowMax & 0xfff)<<16) |
		(nPRM.reg_LD_pic_ColMax & 0xfff);
	LDIM_WR_32Bits(REG_LD_FRM_SIZE, data);

	/* LD_RGB_MOD */
	data = ((0 & 0xfff)                               << 20) |
		((nPRM.reg_LD_RGBmapping_demo & 0x1)      << 19) |
		((nPRM.reg_LD_X_LUT_interp_mode[2] & 0x1) << 18) |
		((nPRM.reg_LD_X_LUT_interp_mode[1] & 0x1) << 17) |
		((nPRM.reg_LD_X_LUT_interp_mode[0] & 0x1) << 16) |
		((0 & 0x1) << 15) |
		((nPRM.reg_LD_BkLit_LPFmod & 0x7) << 12) |
		((nPRM.reg_LD_Litshft & 0x7)      << 8)  |
		((nPRM.reg_LD_BackLit_Xtlk & 0x1) << 7)  |
		((nPRM.reg_LD_BkLit_Intmod & 0x1) << 6)  |
		((nPRM.reg_LD_BkLUT_Intmod & 0x1) << 5)  |
		((nPRM.reg_LD_BkLit_curmod & 0x1) << 4)  |
		((nPRM.reg_LD_BackLit_mode & 0x3));
	LDIM_WR_32Bits(REG_LD_RGB_MOD, data);

	/* LD_BLK_HVNUM  */
	data = ((nPRM.reg_LD_Reflect_Vnum & 0x7)  << 20) |
		((nPRM.reg_LD_Reflect_Hnum & 0x7) << 16) |
		((nPRM.reg_LD_BLK_Vnum & 0x3f)    <<  8) |
		((nPRM.reg_LD_BLK_Hnum & 0x3f));
	LDIM_WR_32Bits(REG_LD_BLK_HVNUM, data);

	/* LD_HVGAIN */
	data = ((nPRM.reg_LD_Vgain & 0xfff) << 16) |
		(nPRM.reg_LD_Hgain & 0xfff);
	LDIM_WR_32Bits(REG_LD_HVGAIN, data);

	/*  LD_BKLIT_VLD  */
	data = 0;
	for (i = 0; i < 32; i++)
		if (nPRM.reg_LD_BkLit_valid[i])
			data = data | (1 << i);
	LDIM_WR_32Bits(REG_LD_BKLIT_VLD, data);

	/* LD_BKLIT_PARAM */
	data = ((nPRM.reg_LD_BkLit_Celnum & 0xff) << 16) |
		(nPRM.reg_BL_matrix_AVG & 0xfff);
	LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);

	/*  LD_LIT_GAIN_COMP */
	data = ((nPRM.reg_LD_Litgain & 0xfff) << 16) |
		(nPRM.reg_BL_matrix_Compensate & 0xfff);
	LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);

	/* LD_FRM_RST_POS */
	data = (1 << 16) | (5); /* h=1,v=5 :ldim_param_frm_rst_pos */
	LDIM_WR_32Bits(REG_LD_FRM_RST_POS, data);

	/* LD_FRM_BL_START_POS */
	data = (1 << 16) | (6); /* ldim_param_frm_bl_start_pos; */
	LDIM_WR_32Bits(REG_LD_FRM_BL_START_POS, data);

	/* REG_LD_FRM_HBLAN_VHOLS  */
	data = ((nPRM.reg_LD_LUT_VHo_LS & 0x7) << 16) |
		((6 & 0x1fff)) ;  /*frm_hblank_num */
	LDIM_WR_32Bits(REG_LD_FRM_HBLAN_VHOLS, data);

	/* REG_LD_XLUT_DEMO_ROI_XPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_xend & 0x1fff) << 16) |
		(nPRM.reg_LD_xlut_demo_roi_xstart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_XPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_YPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_yend & 0x1fff) << 16) |
		(nPRM.reg_LD_xlut_demo_roi_ystart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_YPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_CTRL */
	data = ((nPRM.reg_LD_xlut_oroi_enable & 0x1) << 1) |
		(nPRM.reg_LD_xlut_iroi_enable & 0x1);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_CTRL, data);

	/*LD_BLMAT_RAM_MISC*/
	LDIM_WR_32Bits(REG_LD_BLMAT_RAM_MISC, 384 & 0x1ff);

	/*  X_idx: 12*16  */
	LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM.X_idx[0], 16, 16);

	/* X_nrm[16]: 4 * 16 */
	LDIM_WR_BASE_LUT(REG_LD_RGB_NRMW_BASE_TXLX, nPRM.X_nrm[0], 4, 16);

	/*reg_LD_BLK_Hidx[33]: 14*33 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_HIDX_BASE_TXLX,
			nPRM.reg_LD_BLK_Hidx, 16, 33);

	/* reg_LD_BLK_Vidx[25]: 14*25 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE_TXLX,
			nPRM.reg_LD_BLK_Vidx, 16, 25);

	/* reg_LD_LUT_VHk_pos[32]/reg_LD_LUT_VHk_neg[32]: u8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHk_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHk_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE_TXLX, arrayTmp, 8, 64);

	/* reg_LD_LUT_VHo_pos[32]/reg_LD_LUT_VHo_neg[32]: s8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHo_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHo_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE_TXLX, arrayTmp, 8, 64);

	/* reg_LD_LUT_HHk[32]:u8 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HHK_BASE_TXLX, nPRM.reg_LD_LUT_HHk, 8, 32);

	/*reg_LD_Reflect_Hdgr[20],reg_LD_Reflect_Vdgr[20],
	 *	reg_LD_Reflect_Xdgr[4]
	 */
	for (i = 0; i < 20; i++)
		arrayTmp[i] = nPRM.reg_LD_Reflect_Hdgr[i];
	for (i = 0; i < 20; i++)
		arrayTmp[20+i] = nPRM.reg_LD_Reflect_Vdgr[i];
	for (i = 0; i < 4; i++)
		arrayTmp[40+i] = nPRM.reg_LD_Reflect_Xdgr[i];
	LDIM_WR_BASE_LUT(REG_LD_REFLECT_DGR_BASE_TXLX, arrayTmp, 8, 44);

	/*reg_LD_LUT_Hdg_LEXT[8]/reg_LD_LUT_Vdg_LEXT[8]/reg_LD_LUT_VHk_LEXT[8]*/
	for (i = 0; i < 8; i++)
		arrayTmp[i] = (nPRM.reg_LD_LUT_Hdg_LEXT_TXLX[i] & 0x3ff) |
			((nPRM.reg_LD_LUT_VHk_LEXT_TXLX[i] & 0x3ff) << 10) |
			((nPRM.reg_LD_LUT_Vdg_LEXT_TXLX[i] & 0x3ff) << 20);
	LDIM_WR_BASE_LUT_DRT(REG_LD_LUT_LEXT_BASE_TXLX, arrayTmp, 8);

	/*reg_LD_LUT_Hdg[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE_TXLX,
		nPRM.reg_LD_LUT_Hdg_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_Vdg[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE_TXLX,
		nPRM.reg_LD_LUT_Vdg_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_VHk[8][32]: u10*8*32*/
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE_TXLX,
		nPRM.reg_LD_LUT_VHk_TXLX[0], 16, 8*32);

	/*reg_LD_LUT_Id[16][24]: u3*16*24=u3*384 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_ID_BASE_TXLX, nPRM.reg_LD_LUT_Id, 4, 384);

	/*enable the CBUS configure the RAM*/
	/*LD_MISC_CTRL0  {reg_blmat_ram_mode,
	 *1'h0,ram_bus_sel,ram_clk_sel,ram_clk_gate_en,
	 *2'h0,reg_hvcnt_bypass,reg_demo_synmode,reg_ldbl_synmode,
	 *reg_ldim_bl_en,soft_bl_start,reg_soft_rst)
	 */
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = (data & (~(3 << 9))) | (1 << 8);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	/*X_lut[3][16][16]*/
	LDIM_WR_BASE_LUT_DRT(REG_LD_RGB_LUT_BASE, nPRM.X_lut2[0][0], 3*16*16);
	data = 0 | (0 << 1) | ((ldim_bl_en & 0x1) << 2) |
		(ldim_hvcnt_bypass << 5) | (1 << 8) |
		(3 << 9) | (1 << 12);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	LDIM_Update_Matrix(nPRM.BL_matrix, 16 * 24);

	kfree(arrayTmp);
}

static void LDIM_Initial(unsigned int pic_h, unsigned int pic_v,
		unsigned int BLK_Vnum, unsigned int BLK_Hnum,
		unsigned int BackLit_mode, unsigned int ldim_bl_en,
		unsigned int ldim_hvcnt_bypass)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	LDIMPR("%s: %d %d %d %d %d %d %d\n",
		__func__, pic_h, pic_v, BLK_Vnum, BLK_Hnum,
		BackLit_mode, ldim_bl_en, ldim_hvcnt_bypass);

	ldim_matrix_update_en = ldim_bl_en;
	LD_ConLDReg(&nPRM);
	/* config params begin */
	/* configuration of the panel parameters */
	nPRM.reg_LD_pic_RowMax = pic_v;
	nPRM.reg_LD_pic_ColMax = pic_h;
	/* Maximum to BLKVMAX  , Maximum to BLKHMAX */
	nPRM.reg_LD_BLK_Vnum     = BLK_Vnum;
	nPRM.reg_LD_BLK_Hnum     = BLK_Hnum;
	nPRM.reg_LD_BackLit_mode = BackLit_mode;
	/*config params end */
	ld_fw_cfg_once(&nPRM);
	ldim_db_load_update(&nPRM);

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		LDIM_Initial_TXLX(ldim_bl_en, ldim_hvcnt_bypass);
		break;
	case BL_CHIP_GXTVBB:
		LDIM_Initial_GXTVBB(ldim_bl_en, ldim_hvcnt_bypass);
		break;
	default:
		break;
	}
}

static int ldim_open(struct inode *inode, struct file *file)
{
	struct ldim_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct ldim_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int ldim_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long ldim_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	/*********local dimming ioctl************/
	case LDIM_IOC_PARA:
		if (!LDIM_DATA_FROM_DB)
			return -EINVAL;

		if (copy_from_user(&ldim_db_para, (void __user *)arg,
			sizeof(struct ldim_param_s)))
			ret = -EFAULT;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ldim_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = ldim_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations ldim_fops = {
	.owner          = THIS_MODULE,
	.open           = ldim_open,
	.release        = ldim_release,
	.unlocked_ioctl = ldim_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ldim_compat_ioctl,
#endif
};

static void ldim_dump_histgram(void)
{
	unsigned int i, j, k, len;
	unsigned int *p = NULL;
	char *buf;

	p = kcalloc(2048, sizeof(unsigned int), GFP_KERNEL);
	if (p == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = 16 * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(p);
		return;
	}

	memcpy(p, ldim_driver.hist_matrix,
		ldim_hist_row*ldim_hist_col*16*sizeof(unsigned int));

	for (i = 0; i < ldim_hist_row; i++) {
		pr_info("%s: row %d:\n", __func__, i);
		for (j = 0; j < ldim_hist_col; j++) {
			len = sprintf(buf, "col %d:\n", j);
			for (k = 0; k < 16; k++) {
				len += sprintf(buf+len, "\t0x%x",
					*(p+i*16*ldim_hist_col+j*16+k));
				if (k == 7)
					len += sprintf(buf+len, "\n");
			}
			pr_info("%s\n\n", buf);
		}
		msleep(20);
	}

	kfree(buf);
	kfree(p);
}

static irqreturn_t rdma_ldim_intr(int irq, void *dev_id)
{
	ulong flags;

	/*LDIMPR("*********rdma_ldim_intr start*********\n");*/
	spin_lock_irqsave(&rdma_ldim_isr_lock, flags);

	if (ldim_hist_en) {
		/*schedule_work(&ldim_on_vs_work);*/
		queue_work(ldim_queue, &ldim_on_vs_work);
	}
	rdma_ldim_irq_cnt++;
	if (rdma_ldim_irq_cnt > 0xfffffff)
		rdma_ldim_irq_cnt = 0;
	spin_unlock_irqrestore(&rdma_ldim_isr_lock, flags);
	/*LDIMPR("*********rdma_ldim_intr end*********\n");*/
	return IRQ_HANDLED;
}

static irqreturn_t ldim_vsync_isr(int irq, void *dev_id)
{
	ulong flags;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	if (ldim_on_flag == 0)
		return IRQ_HANDLED;

	spin_lock_irqsave(&ldim_isr_lock, flags);

	if (ldim_func_en) {
		if (ldim_avg_update_en)
			ldim_update_setting();

		switch (bl_drv->data->chip_type) {
		case BL_CHIP_TXLX:
			if (ldim_hist_en) {
				/*schedule_work(&ldim_on_vs_work);*/
				queue_work(ldim_queue, &ldim_on_vs_work);
			}
			break;
		case BL_CHIP_GXTVBB:
			break;
		default:
			break;
		}
	} else {
		/*schedule_work(&ldim_off_vs_work);*/
		queue_work(ldim_queue, &ldim_off_vs_work);
	}

	ldim_irq_cnt++;
	if (ldim_irq_cnt > 0xfffffff)
		ldim_irq_cnt = 0;

	spin_unlock_irqrestore(&ldim_isr_lock, flags);
	/*	LDIMPR("***ldim_vsync_isr end****\n");*/

	return IRQ_HANDLED;
}

static void ldim_update_gxtvbb(void)
{
	unsigned int data;

	if (ldim_avg_update_en) {
		/* LD_BKLIT_PARAM */
		data = LDIM_RD_32Bits(REG_LD_BKLIT_PARAM);
		data = (data&(~0xfff)) | (nPRM.reg_BL_matrix_AVG&0xfff);
		LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);

		/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = (data&(~0xfff)) |
			(nPRM.reg_BL_matrix_Compensate & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	}
	if (ldim_matrix_update_en) {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data & (~(3<<4));
		data = data | (1<<2);
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

		/* gMatrix_LUT: s12*100 ==> max to 8*8 enum ##r/w ram method*/
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&(nPRM.BL_matrix[0]), ldim_blk_row*ldim_blk_col);

		/*data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);*/
		data = data | (3<<4);
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	} else {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data & (~(1<<2));
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	}
	/* disable the CBUS configure the RAM */

}

static void ldim_update_txlx(void)
{
	unsigned int data;

	if (ldim_avg_update_en) {
		/* LD_BKLIT_PARAM */
		data = LDIM_RD_32Bits(REG_LD_BKLIT_PARAM);
		data = (data&(~0xfff)) | (nPRM.reg_BL_matrix_AVG&0xfff);
		LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);

		/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = (data&(~0xfff)) |
			(nPRM.reg_BL_matrix_Compensate & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	}

	if (ldim_matrix_update_en)
		LDIM_Update_Matrix(nPRM.BL_matrix, ldim_blk_row*ldim_blk_col);

}

static void ldim_update_setting(void)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		ldim_update_txlx();
		break;
	case BL_CHIP_GXTVBB:
		ldim_update_gxtvbb();
		break;
	default:
		break;
	}
}

static void ldim_update_matrix(unsigned int mode)
{
	unsigned int data;
	int bl_matrix[8] = {0};
	unsigned int reg_BL_matrix_Compensate = 0x0;
	int bl_matrix_1[8] = {0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_1 = 0xfff;
	int bl_matrix_2[8] = {0xfff, 0xfff, 0xfff, 0x000, 0x000, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_2 = 0xbff;
	int bl_matrix_3[8] = {0, 0, 0, 0xfff, 0, 0, 0, 0};
	unsigned int reg_BL_matrix_Compensate_3 = 0x1ff;
	int bl_matrix_4[8] = {0xfff, 0xfff, 0xfff, 0, 0xfff, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_4 = 0xdff;

	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data & (~(3<<4));
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	/* gMatrix_LUT: s12*100 ==> max to 8*8 enum ##r/w ram method*/
	if (mode == 0) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix[0], 8);
		/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data|(reg_BL_matrix_Compensate&0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 1) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_1[0], 8);
	/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_1 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 2) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_2[0], 8);
	/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data|(reg_BL_matrix_Compensate_2 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 3) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_3[0], 8);
	/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_3 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 4) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_4[0], 8);
	/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_4 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	}
	/* disable the CBUS configure the RAM */
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data | (3<<4);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
}

static void ldim_on_vs_brightness(void)
{
	unsigned int size;
	unsigned int i;

	if (ldim_on_flag == 0)
		return;

	if (ldim_func_bypass)
		return;

	size = ldim_blk_row * ldim_blk_col;

	if (ldim_test_en) {
		for (i = 0; i < size; i++) {
			ldim_driver.local_ldim_matrix[i] =
				(unsigned short)nPRM.BL_matrix[i];
			ldim_driver.ldim_matrix_buf[i] =
				ldim_driver.ldim_test_matrix[i];
		}
	} else {
		for (i = 0; i < size; i++) {
			ldim_driver.local_ldim_matrix[i] =
				(unsigned short)nPRM.BL_matrix[i];
			ldim_driver.ldim_matrix_buf[i] =
				(unsigned short)
				(((nPRM.BL_matrix[i] * litgain)
				+ (LD_DATA_MAX / 2)) >> LD_DATA_DEPTH);
		}
	}

	if (ldim_driver.device_bri_update) {
		ldim_driver.device_bri_update(ldim_driver.ldim_matrix_buf,
			size);
		if (ldim_driver.static_pic_flag == 1) {
			ldim_get_matrix_info_max_rgb();
			ldim_dump_histgram();
		}
	} else {
		/*LDIMERR("%s: device_bri_update is null\n", __func__);*/
	}
}

static void ldim_off_vs_brightness(void)
{
	unsigned int size;
	unsigned int i;
	int ret;

	if (ldim_on_flag == 0)
		return;

	size = ldim_blk_row * ldim_blk_col;

	if (ldim_level_update) {
		ldim_level_update = 0;
		if (ldim_debug_print)
			LDIMPR("%s: level update: 0x%lx\n", __func__, litgain);
		for (i = 0; i < size; i++) {
			ldim_driver.local_ldim_matrix[i] =
				(unsigned short)nPRM.BL_matrix[i];
			ldim_driver.ldim_matrix_buf[i] =
				(unsigned short)(litgain);
		}
	} else {
		if (ldim_driver.device_bri_check) {
			ret = ldim_driver.device_bri_check();
			if (ret) {
				if (ldim_debug_print) {
					LDIMERR("%s: device_bri_check error\n",
						__func__);
				}
				ldim_level_update = 1;
			}
		}
		return;
	}

	if (ldim_driver.device_bri_update) {
		ldim_driver.device_bri_update(ldim_driver.ldim_matrix_buf,
			size);
		if (ldim_driver.static_pic_flag == 1) {
			ldim_get_matrix_info_max_rgb();
			ldim_dump_histgram();
		}
	} else {
		/*LDIMERR("%s: device_bri_update is null\n", __func__);*/
	}
}

static inline void ld_fw_alg_frm(struct LDReg *nPRM1, struct FW_DAT *FDat1,
	unsigned int *max_matrix, unsigned int *hist_matrix)
{
#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
	ld_fw_alg_frm_txlx(nPRM1, FDat1, max_matrix, hist_matrix);
#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
	ld_fw_alg_frm_gxtvbb(nPRM1, FDat1, max_matrix, hist_matrix);
#endif
}

static void ldim_on_vs_arithmetic(void)
{
	unsigned int *local_ldim_hist = NULL;
	unsigned int *local_ldim_max = NULL;
	unsigned int *local_ldim_max_rgb = NULL;
	unsigned int i;

	if (ldim_top_en == 0)
		return;
	local_ldim_hist = kcalloc(ldim_hist_row*ldim_hist_col*16,
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_hist == NULL)
		return;

	local_ldim_max = kcalloc(ldim_hist_row*ldim_hist_col,
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max == NULL) {
		kfree(local_ldim_hist);
		return;
	}
	local_ldim_max_rgb = kcalloc(ldim_hist_row*ldim_hist_col*3,
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max_rgb == NULL) {
		kfree(local_ldim_hist);
		kfree(local_ldim_max);
		return;
	}
	/* spin_lock_irqsave(&ldim_isr_lock, flags); */
	memcpy(local_ldim_hist, ldim_driver.hist_matrix,
		ldim_hist_row*ldim_hist_col*16*sizeof(unsigned int));
	memcpy(local_ldim_max, ldim_driver.max_rgb,
		ldim_hist_row*ldim_hist_col*sizeof(unsigned int));
	for (i = 0; i < ldim_hist_row*ldim_hist_col; i++) {
		(*(local_ldim_max_rgb+i*3)) =
			(*(local_ldim_max+i))&0x3ff;
		(*(local_ldim_max_rgb+i*3+1)) =
			(*(local_ldim_max+i))>>10&0x3ff;
		(*(local_ldim_max_rgb+i*3+2)) =
			(*(local_ldim_max+i))>>20&0x3ff;
	}
	if (ldim_alg_en) {
		ld_fw_alg_frm(&nPRM, &FDat,
			local_ldim_max_rgb, local_ldim_hist);
	}

	kfree(local_ldim_hist);
	kfree(local_ldim_max);
	kfree(local_ldim_max_rgb);
}

static void ldim_get_matrix_info_TF(void)
{
	unsigned int i, j, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	ldim_matrix_t = kcalloc(ldim_hist_row*ldim_hist_col,
		sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_t == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(ldim_matrix_t);
		return;
	}

	memcpy(ldim_matrix_t, &FDat.TF_BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t%4d",
				ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}
static void ldim_get_matrix_info_SF(void)
{
	unsigned int i, j, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	ldim_matrix_t = kcalloc(ldim_hist_row*ldim_hist_col,
		sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_t == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(ldim_matrix_t);
		return;
	}

	memcpy(ldim_matrix_t, &FDat.SF_BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t%4d",
				ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}
static void ldim_get_matrix_info_4(void)
{
	unsigned int i, j, k, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	ldim_matrix_t = kcalloc(ldim_hist_row*ldim_hist_col,
		16*sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_t == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = 3 * ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(ldim_matrix_t);
		return;
	}

	memcpy(ldim_matrix_t, &FDat.last_STA1_MaxRGB[0],
		ldim_blk_col*ldim_blk_row*3*sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\tcol %d:", ldim_blk_col);
			for (k = 0; k < 3; k++) {
				len += sprintf(buf+len, "\t%4d",
					ldim_matrix_t[3*ldim_blk_col*i+j*3+k]);
			}
			len += sprintf(buf+len, "\n");
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}
static void ldim_get_matrix_info_alpha(void)
{
	unsigned int i, j, len;
	unsigned int *ldim_matrix_t = NULL;
	char *buf;

	ldim_matrix_t = kcalloc(ldim_hist_row*ldim_hist_col,
		sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_t == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(ldim_matrix_t);
		return;
	}

	memcpy(ldim_matrix_t, &FDat.TF_BL_alpha[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t%4d",
				ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	kfree(buf);
	kfree(ldim_matrix_t);
}

static void ldim_get_matrix_info_max_rgb(void)
{
	unsigned int i, j, len;
	unsigned int *p = NULL;
	char *buf;

	p = kcalloc(ldim_blk_col*ldim_blk_row,
		sizeof(unsigned int), GFP_KERNEL);
	if (p == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}

	len = ldim_blk_col * 30 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(p);
		return;
	}

	memcpy(p, ldim_driver.max_rgb,
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	pr_info("%s max_rgb:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t(R:%4d, G:%4d, B:%4d)",
				(*(p + j + i*ldim_blk_col))&0x3ff,
				(*(p + j + i*ldim_blk_col)>>10)&0x3ff,
				(*(p + j + i*ldim_blk_col)>>20)&0x3ff);
			if ((j + 1) % 4 == 0)
				len += sprintf(buf+len, "\n\n");
		}
		pr_info("%s\n\n", buf);
		msleep(20);
	}

	kfree(buf);
	kfree(p);
}

static void ldim_get_matrix(unsigned int *data, unsigned int reg_sel)
{
	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		LDIM_RD_BASE_LUT(REG_LD_BLK_VIDX_BASE, data, 16, 32);
	else if (reg_sel == 3)
		ldim_get_matrix_info_TF();
	else if (reg_sel == 4)
		ldim_get_matrix_info_SF();
	else if (reg_sel == 5)
		ldim_get_matrix_info_4();
	else if (reg_sel == 6)
		ldim_get_matrix_info_alpha();
	else if (reg_sel == 7)
		ldim_get_matrix_info_max_rgb();
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_HDG_BASE, data, 10, 32);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_VHK_BASE, data, 10, 32);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_VDG_BASE, data, 10, 32);
}

static void ldim_get_matrix_info(void)
{
	unsigned int i, j, len;
	unsigned short *ldim_matrix_t = NULL;
	unsigned short *ldim_matrix_spi_t = NULL;
	char *buf;

	ldim_matrix_t = kcalloc(ldim_hist_row*ldim_hist_col,
		sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_t == NULL) {
		LDIMERR("ldim_matrix_t malloc error\n");
		return;
	}
	ldim_matrix_spi_t = kcalloc(ldim_hist_row*ldim_hist_col,
		sizeof(unsigned int), GFP_KERNEL);
	if (ldim_matrix_spi_t == NULL) {
		LDIMERR("ldim_matrix_spi_t malloc error\n");
		kfree(ldim_matrix_t);
		return;
	}

	len = ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		kfree(ldim_matrix_t);
		kfree(ldim_matrix_spi_t);
		return;
	}

	memcpy(ldim_matrix_t, &ldim_driver.local_ldim_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned short));
	memcpy(ldim_matrix_spi_t,
		&ldim_driver.ldim_matrix_buf[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned short));

	pr_info("%s:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t%4d",
				ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	pr_info("current black_frm: %d\n", black_frm);
	pr_info("spi transfer_matrix:\n");
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t%4d",
				ldim_matrix_spi_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}
	pr_info("\n");

	kfree(buf);
	kfree(ldim_matrix_t);
	kfree(ldim_matrix_spi_t);
}

static void ldim_nPRM_bl_matrix_info(void)
{
	unsigned int i, j, len;
	unsigned int ldim_matrix_t[LD_BLKREGNUM] = {0};
	char *buf;

	len = ldim_blk_col * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		return;
	}

	memcpy(&ldim_matrix_t[0], &nPRM.BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned short));

	pr_info("%s and spi info:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		len = 0;
		for (j = 0; j < ldim_blk_col; j++) {
			len += sprintf(buf+len, "\t0x%x",
				ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("%s\n", buf);
		msleep(20);
	}

	kfree(buf);
}
static void ldim_set_matrix(unsigned int *data, unsigned int reg_sel,
		unsigned int cnt)
{
	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE, data, 16, 32);
	else if (reg_sel == 2)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE, data, 16, 32);
	else if (reg_sel == 3)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE, data, 16, 4);
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE, data, 16, cnt);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE, data, 16, cnt);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE, data, 16, cnt);
}

static void ldim_remap_ctrl(unsigned char status)
{
	unsigned int temp;

	temp = ldim_matrix_update_en;
	if (status) {
		ldim_matrix_update_en = 0;
		LDIM_WR_32Bits(0x0a, 0x706);
		msleep(20);
		ldim_matrix_update_en = temp;
	} else {
		ldim_matrix_update_en = 0;
		LDIM_WR_32Bits(0x0a, 0x600);
		msleep(20);
		ldim_matrix_update_en = temp;
	}
	LDIMPR("%s: %d\n", __func__, status);
}

static void ldim_func_ctrl(unsigned char status)
{
	if (status) {
		/* enable other flag */
		ldim_top_en = 1;
		ldim_hist_en = 1;
		ldim_alg_en = 1;
		/* enable update */
		ldim_avg_update_en = 1;
		/*ldim_matrix_update_en = 1;*/

		ldim_remap_ctrl(ldim_remap_en);
	} else {
		/* disable remap */
		ldim_remap_ctrl(0);
		/* disable update */
		ldim_avg_update_en = 0;
		/*ldim_matrix_update_en = 0;*/
		/* disable other flag */
		ldim_top_en = 0;
		ldim_hist_en = 0;
		ldim_alg_en = 0;

		/* refresh system brightness */
		ldim_level_update = 1;
	}
	LDIMPR("%s: %d\n", __func__, status);
}

static int ldim_on_init(void)
{
	int ret = 0;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	LDIMPR("%s\n", __func__);

	/* init ldim */
	ldim_stts_initial(ldim_config.hsize, ldim_config.vsize,
		ldim_blk_row, ldim_blk_col);
	LDIM_Initial(ldim_config.hsize, ldim_config.vsize,
		ldim_blk_row, ldim_blk_col, ldim_config.bl_mode, 1, 0);

	ldim_func_ctrl(0); /* default disable ldim function */

	if (ldim_driver.pinmux_ctrl)
		ldim_driver.pinmux_ctrl(ldim_drv->ldev_conf->pinmux_name);
	ldim_on_flag = 1;
	ldim_level_update = 1;

	return ret;
}

static int ldim_power_on(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	ldim_func_ctrl(ldim_func_en);

	if (ldim_driver.device_power_on)
		ldim_driver.device_power_on();
	ldim_on_flag = 1;
	ldim_level_update = 1;

	return ret;
}
static int ldim_power_off(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	ldim_on_flag = 0;
	if (ldim_driver.device_power_off)
		ldim_driver.device_power_off();

	ldim_func_ctrl(0);

	return ret;
}

static int ldim_set_level(unsigned int level)
{
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	unsigned int level_max, level_min;

	ldim_brightness_level = level;

	if (ldim_brightness_bypass)
		return 0;

	level_max = bl_drv->bconf->level_max;
	level_min = bl_drv->bconf->level_min;

	level = ((level - level_min) * (LD_DATA_MAX - ldim_data_min)) /
		(level_max - level_min) + ldim_data_min;
	level &= 0xfff;
	litgain = (unsigned long)level;
	ldim_level_update = 1;

	return 0;
}

static void ldim_test_ctrl(int flag)
{
	if (flag) /* when enable lcd bist pattern, bypass ldim function */
		ldim_func_bypass = 1;
	else
		ldim_func_bypass = 0;
	LDIMPR("%s: ldim_func_bypass = %d\n", __func__, ldim_func_bypass);
}

static struct aml_ldim_driver_s ldim_driver = {
	.valid_flag = 0, /* default invalid, active when bl_ctrl_method=ldim */
	.dev_index = 0xff,
	.static_pic_flag = 0,
	.ldim_conf = &ldim_config,
	.ldev_conf = NULL,
	.ldim_matrix_buf = NULL,
	.hist_matrix = NULL,
	.max_rgb = NULL,
	.ldim_test_matrix = NULL,
	.local_ldim_matrix = NULL,
	.init = ldim_on_init,
	.power_on = ldim_power_on,
	.power_off = ldim_power_off,
	.set_level = ldim_set_level,
	.test_ctrl = ldim_test_ctrl,
	.config_print = NULL,
	.pinmux_ctrl = NULL,
	.pwm_vs_update = NULL,
	.device_power_on = NULL,
	.device_power_off = NULL,
	.device_bri_update = NULL,
	.device_bri_check = NULL,
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void)
{
	return &ldim_driver;
}

static ssize_t ldim_attr_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len,
	"\necho hist > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo maxrgb > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo matrix > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo histgram_ldim > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_matrix_get 7 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_matrix_info > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_matrix_get 0/1/2/3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_enable > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_disable > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo info > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo alg_para > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_mode 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_set 0 0xfff > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_set_all 0xfff > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_get > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo fw_LD_ThSF_l 1600 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo fw_LD_ThTF_l 32 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo boost_gain 4 > /sys/class/aml_ldim/attr\n");
#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
	len += sprintf(buf+len,
	"echo alpha_delta 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo boost_gain_neg 4 > /sys/class/aml_ldim/attr\n");
#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
	len += sprintf(buf+len,
	"echo TF_alpha 256 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_gain 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_res 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo rgb_base 128 > /sys/class/aml_ldim/attr\n");
#endif

	len += sprintf(buf+len,
	"echo ov_gain 16 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo incr_dif_gain 16 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo avg_gain 2048 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo litgain 4096 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo LPF_method 3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo LD_TF_STEP_TH 100 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo TF_step_method 3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo TF_FRESH_BL 8 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo TF_BLK_FRESH_BL 5 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo side_blk_diff_th 100 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo bbd_th 200 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo bbd_detect_en 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo diff_blk_luma_en 1 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo bl_remap_curve > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo fw_LD_Whist > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo Sf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Boost_light_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Lpf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Ld_remap_bypass 0 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo fw_hist_print 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Debug 1 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo fw_print_frequent 8 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Dbprint_lv 1 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo ldim_init 1920 1080 8 2 0 1 0 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_0 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_1 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_2 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_3 512 4 3712 29 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_4 546 4 3823 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_5 546 4 3823 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_6 585 4 3647 25 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_7 630 4 3455 22 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_8 745 4 3135 17 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_9 819 4 3007 15 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_10 910 4 2879 13 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_11 1170 4 2623 28 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_12 512 4 3750 28 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_13 512 4 3800 27 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_14 512 4 4000 26 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_15 512 4 4055 25 > /sys/class/aml_ldim/attr\n");

	return len;
}
static ssize_t ldim_attr_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t len)
{
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[47] = {NULL};
	char str[3] = {' ', '\n', '\0'};
	unsigned int size;
	int i;
	char *pr_buf;
	ssize_t pr_len = 0;

	unsigned long pic_h = 0, pic_v = 0;
	unsigned long blk_vnum = 0, blk_hnum = 0, hist_row = 0, hist_col = 0;
	unsigned long backlit_mod = 0, ldim_bl_en = 0, ldim_hvcnt_bypass = 0;
	unsigned long val1 = 0, val2 = 0;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (buf_orig == NULL) {
		LDIMERR("buf malloc error\n");
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "histgram_ldim")) {
		ldim_dump_histgram();
	} else if (!strcmp(parm[0], "maxrgb")) {
		ldim_get_matrix_info_max_rgb();
	} else if (!strcmp(parm[0], "hist")) {
		ldim_dump_histgram();
	} else if (!strcmp(parm[0], "matrix")) {
		ldim_get_matrix_info();
	} else if (!strcmp(parm[0], "db_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			LDIM_DATA_FROM_DB = (unsigned char)val1;
			ldim_db_load_update(&nPRM);
		}
		pr_info("LDIM_DATA_FROM_DB = %d\n", LDIM_DATA_FROM_DB);
	} else if (!strcmp(parm[0], "db_print")) {
		db_print_flag = 1;
	} else if (!strcmp(parm[0], "dump_db")) {
		ldim_db_para_print(&nPRM);
	} else if (!strcmp(parm[0], "ldim_init")) {
		if (parm[7] != NULL) {
			if (kstrtoul(parm[1], 10, &pic_h) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 10, &pic_v) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[3], 10, &blk_vnum) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[4], 10, &blk_hnum) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[5], 10, &backlit_mod) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[6], 10, &ldim_bl_en) < 0)
				return -EINVAL;
			if (kstrtoul(parm[7], 10, &ldim_hvcnt_bypass) < 0)
				goto ldim_attr_store_end;

			pr_info("ldim init param:%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
				pic_h, pic_v, blk_vnum, blk_hnum,
				backlit_mod, ldim_bl_en, ldim_hvcnt_bypass);
			ldim_blk_row = blk_vnum;
			ldim_blk_col = blk_hnum;
			ldim_config.row = ldim_blk_row;
			ldim_config.col = ldim_blk_col;
			ldim_config.bl_mode = (unsigned char)backlit_mod;
			LDIM_Initial(pic_h, pic_v, blk_vnum, blk_hnum,
				backlit_mod, ldim_bl_en, ldim_hvcnt_bypass);
			pr_info("**************ldim init ok*************\n");
		}
	} else if (!strcmp(parm[0], "ldim_stts_init")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &pic_h) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 10, &pic_v) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[3], 10, &hist_row) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[4], 10, &hist_col) < 0)
				goto ldim_attr_store_end;

			pr_info("****ldim init param:%lu,%lu,%lu,%lu********\n",
				pic_h, pic_v, hist_row, hist_col);
			ldim_hist_row = hist_row;
			ldim_hist_col = hist_col;
			ldim_stts_initial(pic_h, pic_v, hist_row, hist_col);
			pr_info("************ldim stts init ok*************\n");
		}
	} else if (!strcmp(parm[0], "remap")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			if (val1) {
				if (ldim_func_en) {
					ldim_remap_en = 1;
					ldim_remap_ctrl(1);
				} else {
					pr_info(
					"error: ldim_func is disabled\n");
				}
			} else {
				ldim_remap_en = 0;
				ldim_remap_ctrl(0);
			}
		}
		pr_info("ldim_remap_en: %d\n", ldim_remap_en);
	} else if (!strcmp(parm[0], "ldim_matrix_get")) {
		unsigned int data[32] = {0};
		unsigned int k, g, reg_sel = 0;

		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			reg_sel = (unsigned int)val1;

			pr_buf = kzalloc(sizeof(char) * 200, GFP_KERNEL);
			if (!pr_buf) {
				LDIMERR("buf malloc error\n");
				goto ldim_attr_store_end;
			}
			ldim_get_matrix(&data[0], reg_sel);
			if ((reg_sel == 0) || (reg_sel == 1)) {
				pr_info("******ldim matrix info start******\n");
				for (k = 0; k < 4; k++) {
					pr_len = 0;
					for (g = 0; g < 8; g++) {
						pr_len += sprintf(pr_buf+pr_len,
							"\t%d", data[8*k+g]);
					}
					pr_info("%s\n", pr_buf);
				}
				pr_info("*******ldim matrix info end*******\n");
			}
			kfree(pr_buf);
		}
	} else if (!strcmp(parm[0], "ldim_matrix_set")) {
		unsigned int data_set[32] = {0};
		unsigned int reg_sel_1 = 0, k1, cnt1 = 0;
		unsigned long temp_set[32] = {0};

		if (parm[2] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 10, &val2) < 0)
				goto ldim_attr_store_end;

			reg_sel_1 = val1;
			cnt1 = val2;
			for (k1 = 0; k1 < cnt1; k1++) {
				if (parm[k1+2] != NULL) {
					temp_set[k1] = kstrtoul(parm[k1+2],
						10, &temp_set[k1]);
					data_set[k1] =
						(unsigned int)temp_set[k1];
				}
			}
			ldim_set_matrix(&data_set[0], reg_sel_1, cnt1);
			pr_info("***********ldim matrix set over***********\n");
		}
	} else if (!strcmp(parm[0], "ldim_matrix_info")) {
		ldim_get_matrix_info();
		pr_info("**************ldim matrix info over*************\n");
	} else if (!strcmp(parm[0], "ldim_nPRM_bl_matrix_info")) {
		ldim_nPRM_bl_matrix_info();
		pr_info("**************ldim matrix(nPRM) info over*********\n");
	} else if (!strcmp(parm[0], "ldim_enable")) {
		ldim_func_en = 1;
		ldim_func_ctrl(1);
		pr_info("**************ldim enable ok*************\n");
	} else if (!strcmp(parm[0], "ldim_disable")) {
		ldim_func_en = 0;
		ldim_func_ctrl(0);
		pr_info("**************ldim disable ok*************\n");
	} else if (!strcmp(parm[0], "data_min")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;

			ldim_data_min = (unsigned int)val1;
			ldim_set_level(ldim_brightness_level);
			pr_info("*****ldim brightness data_min update******\n");
		}
	} else if (!strcmp(parm[0], "test_mode")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_test_en = (unsigned char)val1;
		}
		LDIMPR("test_mode: %d\n", ldim_test_en);
	} else if (!strcmp(parm[0], "test_set")) {
		if (parm[2] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 16, &val2) < 0)
				goto ldim_attr_store_end;

			size = ldim_blk_row * ldim_blk_col;
			if (val1 < size) {
				ldim_driver.ldim_test_matrix[val1] =
					(unsigned short)val2;
				LDIMPR("set test_matrix[%d] = 0x%03x\n",
					(unsigned int)val1, (unsigned int)val2);
			} else {
				LDIMERR("invalid index for test_matrix: %d\n",
					(unsigned int)val1);
			}
		}
	} else if (!strcmp(parm[0], "test_set_all")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &val1) < 0)
				goto ldim_attr_store_end;

			for (i = 0; i < ldim_blk_row * ldim_blk_col; i++) {
				ldim_driver.ldim_test_matrix[i] =
					(unsigned short)val1;
			}
			LDIMPR("set all test_matrix to 0x%03x\n",
				(unsigned int)val1);
		}
	} else if (!strcmp(parm[0], "test_get")) {
		LDIMPR("get test_mode: %d, test_matrix:\n", ldim_test_en);
		size = ldim_blk_row * ldim_blk_col;
		for (i = 0; i < size; i++)
			pr_info("0x%03x\t", ldim_driver.ldim_test_matrix[i]);
		pr_info("\n");
	} else if (!strcmp(parm[0], "rs")) {
		unsigned int reg_addr = 0, reg_val;

		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &val1) < 0)
				goto ldim_attr_store_end;
			reg_addr = (unsigned int)val1;

			reg_val = LDIM_RD_32Bits(reg_addr);
			pr_info("reg_addr: 0x%x=0x%x\n", reg_addr, reg_val);
		}
	} else if (!strcmp(parm[0], "ws")) {
		unsigned int reg_addr = 0, reg_val = 0;

		if (parm[2] != NULL) {
			if (kstrtoul(parm[1], 16, &val1) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 16, &val2) < 0)
				goto ldim_attr_store_end;
			reg_addr = (unsigned int)val1;
			reg_val = (unsigned int)val2;

			LDIM_WR_32Bits(reg_addr, reg_val);
			pr_info("reg_addr: 0x%x=0x%x, readback: 0x%x\n",
				reg_addr, reg_val, LDIM_RD_32Bits(reg_addr));
		}
	} else if (!strcmp(parm[0], "update_matrix")) {
		unsigned int mode = 0;

		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			mode  = (unsigned int)val1;

			ldim_update_matrix(mode);
			pr_info("mode: %d\n", mode);
		}
	} else if (!strcmp(parm[0], "bl_remap_curve")) {
		ldim_bl_remap_curve_print();
	} else if (!strcmp(parm[0], "fw_LD_Whist")) {
		for (i = 0; i < 16; i++)
			pr_info("(%d),", (unsigned int)fw_LD_Whist[i]);
		if (parm[16] != NULL) {
			if (kstrtoul(parm[1], 10, &fw_LD_Whist[0]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[2], 10, &fw_LD_Whist[1]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[3], 10, &fw_LD_Whist[2]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[4], 10, &fw_LD_Whist[3]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[5], 10, &fw_LD_Whist[4]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[6], 10, &fw_LD_Whist[5]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[7], 10, &fw_LD_Whist[6]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[8], 10, &fw_LD_Whist[7]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[9], 10, &fw_LD_Whist[8]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[10], 10, &fw_LD_Whist[9]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[11], 10, &fw_LD_Whist[10]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[12], 10, &fw_LD_Whist[11]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[13], 10, &fw_LD_Whist[12]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[14], 10, &fw_LD_Whist[13]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[15], 10, &fw_LD_Whist[14]) < 0)
				goto ldim_attr_store_end;
			if (kstrtoul(parm[16], 10, &fw_LD_Whist[15]) < 0)
				goto ldim_attr_store_end;

			for (i = 0; i < 16; i++)
				pr_info("(%d),", (unsigned int)fw_LD_Whist[i]);
			pr_info("\n********fw_LD_Whist ok*********\n");
		}
	} else if (!strcmp(parm[0], "Sf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Sf_bypass = (unsigned char)val1;
		}
		pr_info("Sf_bypass = %d\n", Sf_bypass);
	} else if (!strcmp(parm[0], "Boost_light_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Boost_light_bypass = (unsigned char)val1;
		}
		pr_info("Boost_light_bypass = %d\n", Boost_light_bypass);
	} else if (!strcmp(parm[0], "Lpf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Lpf_bypass = (unsigned char)val1;
		}
		pr_info("Lpf_bypass = %d\n", Lpf_bypass);
	} else if (!strcmp(parm[0], "Ld_remap_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Ld_remap_bypass = (unsigned char)val1;
		}
		pr_info("Ld_remap_bypass = %d\n", Ld_remap_bypass);
	} else if (!strcmp(parm[0], "slp_gain")) {
		unsigned int slop_gain = 0;

		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;

			slop_gain = (unsigned int)val1;
			ldim_bl_remap_curve(slop_gain);
			pr_info("slp_gain = %d\n", slop_gain);
			ldim_bl_remap_curve_print();
		}
	} else if (!strcmp(parm[0], "ov_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ov_gain = (unsigned int)val1;
		}
		pr_info("ov_gain = %d\n", ov_gain);
	} else if (!strcmp(parm[0], "incr_dif_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			incr_dif_gain = (unsigned int)val1;
		}
		pr_info("incr_dif_gain = %d\n", incr_dif_gain);
	} else if (!strcmp(parm[0], "litgain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &litgain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("litgain = %ld\n", litgain);
	} else if (!strcmp(parm[0], "brightness_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_brightness_bypass = (unsigned char)val1;
			if (ldim_brightness_bypass == 0)
				ldim_set_level(ldim_brightness_level);
		}
		pr_info("brightness_bypass = %d\n", ldim_brightness_bypass);
	} else if (!strcmp(parm[0], "fw_LD_ThSF_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &fw_LD_ThSF_l) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("fw_LD_ThSF_l = %ld\n", fw_LD_ThSF_l);
	} else if (!strcmp(parm[0], "fw_LD_ThTF_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &fw_LD_ThTF_l) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("fw_LD_ThTF_l = %ld\n", fw_LD_ThTF_l);
	} else if (!strcmp(parm[0], "boost_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &boost_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("boost_gain = %ld\n", boost_gain);
#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
	} else if (!strcmp(parm[0], "boost_gain_neg")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &boost_gain_neg) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("boost_gain_neg = %ld\n", boost_gain_neg);
	} else if (!strcmp(parm[0], "alpha_delta")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &alpha_delta) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("alpha_delta = %ld\n", alpha_delta);
#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
	} else if (!strcmp(parm[0], "TF_alpha")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &TF_alpha) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("TF_alpha = %ld\n", TF_alpha);
	} else if (!strcmp(parm[0], "lpf_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &lpf_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("lpf_gain = %ld\n", lpf_gain);
	} else if (!strcmp(parm[0], "lpf_res")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &lpf_res) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("lpf_res = %ld\n", lpf_res);
	} else if (!strcmp(parm[0], "rgb_base")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &rgb_base) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("rgb_base = %ld\n", rgb_base);
#endif
	} else if (!strcmp(parm[0], "avg_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &avg_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("avg_gain = %ld\n", avg_gain);
	} else if (!strcmp(parm[0], "fw_rgb_diff_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			fw_rgb_diff_th = (unsigned int)val1;
		}
		pr_info("fw_rgb_diff_th = %d\n", fw_rgb_diff_th);
	} else if (!strcmp(parm[0], "max_luma")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			max_luma = (unsigned int)val1;
		}
		pr_info("max_luma = %d\n", max_luma);
	} else if (!strcmp(parm[0], "lmh_avg_TH")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			lmh_avg_TH = (unsigned int)val1;
		}
		pr_info("lmh_avg_TH = %d\n", lmh_avg_TH);
	} else if (!strcmp(parm[0], "fw_TF_sum_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			fw_TF_sum_th = (unsigned int)val1;
		}
		pr_info("fw_TF_sum_th = %d\n", fw_TF_sum_th);
	} else if (!strcmp(parm[0], "LPF_method")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			LPF_method = (unsigned int)val1;
		}
		pr_info("LPF_method = %d\n", LPF_method);
	} else if (!strcmp(parm[0], "LD_TF_STEP_TH")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			LD_TF_STEP_TH = (unsigned int)val1;
		}
		pr_info("LD_TF_STEP_TH = %d\n", LD_TF_STEP_TH);
	} else if (!strcmp(parm[0], "TF_step_method")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			TF_step_method = (unsigned int)val1;
		}
		pr_info("TF_step_method = %d\n", TF_step_method);
	} else if (!strcmp(parm[0], "TF_FRESH_BL")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			TF_FRESH_BL = (unsigned int)val1;
		}
		pr_info("TF_FRESH_BL = %d\n", TF_FRESH_BL);
	} else if (!strcmp(parm[0], "TF_BLK_FRESH_BL")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			TF_BLK_FRESH_BL = (unsigned int)val1;
		}
		pr_info("TF_BLK_FRESH_BL = %d\n", TF_BLK_FRESH_BL);
	} else if (!strcmp(parm[0], "bbd_detect_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			bbd_detect_en = (unsigned char)val1;
		}
		pr_info("bbd_detect_en = %d\n", bbd_detect_en);
	} else if (!strcmp(parm[0], "side_blk_diff_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			side_blk_diff_th = (unsigned int)val1;
		}
		pr_info("side_blk_diff_th = %d\n", side_blk_diff_th);
	} else if (!strcmp(parm[0], "bbd_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			bbd_th = (unsigned int)val1;
		}
		pr_info("bbd_th = %d\n", bbd_th);
	} else if (!strcmp(parm[0], "diff_blk_luma_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			diff_blk_luma_en = (unsigned char)val1;
		}
		pr_info("diff_blk_luma_en = %d\n", diff_blk_luma_en);
	} else if (!strcmp(parm[0], "fw_hist_print")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			fw_hist_print = (unsigned char)val1;
		}
		pr_info("fw_hist_print = %d\n", fw_hist_print);
	} else if (!strcmp(parm[0], "Debug")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Debug = (unsigned char)val1;
		}
		pr_info("Debug = %d\n", Debug);
	} else if (!strcmp(parm[0], "fw_print_frequent")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			fw_print_frequent = (unsigned int)val1;
		}
		pr_info("fw_print_frequent = %d\n", fw_print_frequent);
	} else if (!strcmp(parm[0], "Dbprint_lv")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			Dbprint_lv = (unsigned int)val1;
		}
		pr_info("Dbprint_lv = %d\n", Dbprint_lv);
	} else if (!strcmp(parm[0], "alg_para")) {
		pr_info("ldim_alg_ver         = %s\n",
			LD_FW_ALG_FRM_VER);
#if (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V1)
		pr_info("fw_LD_ThSF_l         = %ld\n"
			"fw_LD_ThTF_l         = %ld\n"
			"boost_gain           = %ld\n"
			"boost_gain_neg       = %ld\n"
			"alpha_delta          = %ld\n\n",
			fw_LD_ThSF_l,
			fw_LD_ThTF_l,
			boost_gain,
			boost_gain_neg,
			alpha_delta);
#elif (LD_FW_ALG_FRM_SEL == LD_FW_ALG_FRM_V0)
		pr_info("fw_LD_ThSF_l         = %ld\n"
			"fw_LD_ThTF_l         = %ld\n"
			"boost_gain           = %ld\n"
			"TF_alpha             = %ld\n"
			"lpf_gain             = %ld\n"
			"lpf_res              = %ld\n"
			"rgb_base             = %ld\n\n",
			fw_LD_ThSF_l,
			fw_LD_ThTF_l,
			boost_gain,
			TF_alpha,
			lpf_gain,
			lpf_res,
			rgb_base);
#endif
		pr_info("fw_rgb_diff_th       = %d\n"
			"max_luma             = %d\n"
			"lmh_avg_TH           = %d\n"
			"fw_TF_sum_th         = %d\n"
			"LPF_method           = %d\n"
			"LD_TF_STEP_TH        = %d\n"
			"TF_step_method       = %d\n"
			"TF_FRESH_BL          = %d\n\n",
			fw_rgb_diff_th,
			max_luma,
			lmh_avg_TH,
			fw_TF_sum_th,
			LPF_method,
			LD_TF_STEP_TH,
			TF_step_method,
			TF_FRESH_BL);
		pr_info("TF_BLK_FRESH_BL      = %d\n"
			"side_blk_diff_th     = %d\n"
			"bbd_th               = %d\n"
			"bbd_detect_en        = %d\n"
			"diff_blk_luma_en     = %d\n\n",
			TF_BLK_FRESH_BL,
			side_blk_diff_th,
			bbd_th,
			bbd_detect_en,
			diff_blk_luma_en);
		pr_info("ov_gain              = %d\n"
			"incr_dif_gain        = %d\n"
			"avg_gain             = %ld\n"
			"litgain              = %ld\n\n",
			ov_gain,
			incr_dif_gain,
			avg_gain,
			litgain);
		pr_info("Sf_bypass            = %d\n"
			"Boost_light_bypass   = %d\n"
			"Lpf_bypass           = %d\n"
			"Ld_remap_bypass      = %d\n\n",
			Sf_bypass,
			Boost_light_bypass,
			Lpf_bypass,
			Ld_remap_bypass);
		pr_info("fw_hist_print        = %d\n"
			"Debug                = %d\n"
			"fw_print_frequent    = %d\n"
			"Dbprint_lv           = %d\n\n",
			fw_hist_print,
			Debug,
			fw_print_frequent,
			Dbprint_lv);
	} else if (!strcmp(parm[0], "info")) {
		pr_info("ldim_drv_ver          = %s\n",
			LDIM_DRV_VER);
		ldim_driver.config_print();
		pr_info("\nldim_blk_row          = %d\n"
			"ldim_blk_col          = %d\n"
			"ldim_hist_row         = %d\n"
			"ldim_hist_col         = %d\n"
			"ldim_bl_mode          = %d\n"
			"dev_index             = %d\n\n",
			ldim_blk_row, ldim_blk_col,
			ldim_hist_row, ldim_hist_col,
			ldim_config.bl_mode,
			ldim_driver.dev_index);
		pr_info("ldim_on_flag          = %d\n"
			"ldim_func_en          = %d\n"
			"ldim_remap_en         = %d\n"
			"ldim_func_bypass      = %d\n"
			"ldim_test_en          = %d\n"
			"ldim_avg_update_en    = %d\n"
			"ldim_matrix_update_en = %d\n"
			"ldim_alg_en           = %d\n"
			"ldim_top_en           = %d\n"
			"ldim_hist_en          = %d\n"
			"ldim_data_min         = %d\n"
			"ldim_data_max         = %d\n"
			"ldim_irq_cnt          = %d\n\n",
			ldim_on_flag, ldim_func_en, ldim_remap_en,
			ldim_func_bypass, ldim_test_en,
			ldim_avg_update_en, ldim_matrix_update_en,
			ldim_alg_en, ldim_top_en, ldim_hist_en,
			ldim_data_min, LD_DATA_MAX,
			ldim_irq_cnt);
		pr_info("nPRM.reg_LD_BLK_Hnum   = %d\n"
			"nPRM.reg_LD_BLK_Vnum   = %d\n"
			"nPRM.reg_LD_pic_RowMax = %d\n"
			"nPRM.reg_LD_pic_ColMax = %d\n",
			nPRM.reg_LD_BLK_Hnum, nPRM.reg_LD_BLK_Vnum,
			nPRM.reg_LD_pic_RowMax, nPRM.reg_LD_pic_ColMax);
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_debug_print = (unsigned char)val1;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else
		pr_info("no support cmd!!!\n");

	kfree(buf_orig);
	return len;

ldim_attr_store_end:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t ldim_func_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_func_en);

	return ret;
}

static ssize_t ldim_func_en_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming function: %s\n", (val ? "enable" : "disable"));
	ldim_func_en = val ? 1 : 0;
	ldim_func_ctrl(ldim_func_en);

	return count;
}

static struct class_attribute aml_ldim_class_attrs[] = {
	__ATTR(attr, 0644, ldim_attr_show, ldim_attr_store),
	__ATTR(func_en, 0644,
		ldim_func_en_show, ldim_func_en_store),
	__ATTR_NULL,
};

int aml_ldim_get_config_dts(struct device_node *child)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int para[5];
	int ret;

	if (child == NULL) {
		LDIMERR("child device_node is null\n");
		return -1;
	}

	/* default setting */
	ldim_config.hsize = vinfo->width;
	ldim_config.vsize = vinfo->height;

	/* get row & col from dts */
	ret = of_property_read_u32_array(child, "bl_ldim_region_row_col",
			para, 2);
	if (ret) {
		LDIMERR("failed to get bl_ldim_region_row_col\n");
	} else {
		if ((para[0] * para[1]) > LD_BLKREGNUM) {
			LDIMERR("region row*col(%d*%d) is out of support\n",
				para[0], para[1]);
		} else {
			ldim_blk_row = para[0];
			ldim_blk_col = para[1];
			ldim_hist_row = ldim_blk_row;
			ldim_hist_col = ldim_blk_col;
			ldim_config.row = ldim_blk_row;
			ldim_config.col = ldim_blk_col;
		}
	}
	LDIMPR("get region row = %d, col = %d\n", ldim_blk_row, ldim_blk_col);

	/* get bl_mode from dts */
	ret = of_property_read_u32(child, "bl_ldim_mode", &para[0]);
	if (ret)
		LDIMERR("failed to get bl_ldim_mode\n");
	else
		ldim_config.bl_mode = (unsigned char)para[0];
	LDIMPR("get bl_mode = %d\n", ldim_config.bl_mode);

	/* get ldim_dev_index from dts */
	ret = of_property_read_u32(child, "ldim_dev_index", &para[0]);
	if (ret)
		LDIMERR("failed to get ldim_dev_index\n");
	else
		ldim_driver.dev_index = para[0];
	LDIMPR("get ldim_dev_index = %d\n", ldim_driver.dev_index);

	return 0;
}

int aml_ldim_get_config_unifykey(unsigned char *buf)
{
	unsigned char *p;
	struct vinfo_s *vinfo = get_current_vinfo();

	/* default setting */
	ldim_config.hsize = vinfo->width;
	ldim_config.vsize = vinfo->height;

	p = buf;

	/* ldim: 24byte */
	/* get bl_ldim_region_row_col 4byte*/
	ldim_blk_row = *(p + LCD_UKEY_BL_LDIM_ROW);
	ldim_blk_col = *(p + LCD_UKEY_BL_LDIM_COL);
	ldim_hist_row = ldim_blk_row;
	ldim_hist_col = ldim_blk_col;
	ldim_config.row = ldim_blk_row;
	ldim_config.col = ldim_blk_col;
	LDIMPR("get region row = %d, col = %d\n", ldim_blk_row, ldim_blk_col);

	/* get bl_ldim_mode 1byte*/
	ldim_config.bl_mode = *(p + LCD_UKEY_BL_LDIM_MODE);
	LDIMPR("get bl_mode = %d\n", ldim_config.bl_mode);

	/* get ldim_dev_index 1byte*/
	ldim_driver.dev_index = *(p + LCD_UKEY_BL_LDIM_DEV_INDEX);
	LDIMPR("get dev_index = %d\n", ldim_driver.dev_index);

	return 0;
}

static int aml_ldim_kzalloc(unsigned int ldim_blk_row1,
	unsigned int ldim_blk_col1)
{
	ldim_driver.ldim_matrix_buf = kzalloc(
		(sizeof(unsigned short) * (ldim_blk_row1 * ldim_blk_col1)),
		GFP_KERNEL);
	if (ldim_driver.ldim_matrix_buf == NULL) {
		LDIMERR("ldim_driver ldim_matrix_buf malloc error\n");
		goto err0;
	}
	ldim_driver.hist_matrix = kzalloc((sizeof(unsigned int) *
		(ldim_blk_row1 * ldim_blk_col1 * 16)), GFP_KERNEL);
	if (ldim_driver.hist_matrix == NULL) {
		LDIMERR("ldim_driver hist_matrix malloc error\n");
		goto err1;
	}
	ldim_driver.max_rgb = kzalloc((sizeof(unsigned int) *
		(ldim_blk_row1 * ldim_blk_col1)), GFP_KERNEL);
	if (ldim_driver.max_rgb == NULL) {
		LDIMERR("ldim_driver max_rgb malloc error\n");
		goto err2;
	}
	ldim_driver.ldim_test_matrix = kzalloc((sizeof(unsigned short) *
		(ldim_blk_row1 * ldim_blk_col1)), GFP_KERNEL);
	if (ldim_driver.ldim_test_matrix == NULL) {
		LDIMERR("ldim_driver ldim_test_matrix malloc error\n");
		goto err3;
	}
	ldim_driver.local_ldim_matrix = kzalloc((sizeof(unsigned short) *
		(ldim_blk_row1 * ldim_blk_col1)), GFP_KERNEL);
	if (ldim_driver.local_ldim_matrix == NULL) {
		LDIMERR("ldim_driver local_ldim_matrix malloc error\n");
		goto err4;
	}
	FDat.SF_BL_matrix = kzalloc((sizeof(unsigned int) *
		(ldim_blk_row1 * ldim_blk_col1)), GFP_KERNEL);
	if (FDat.SF_BL_matrix == NULL) {
		LDIMERR("ldim_driver FDat.SF_BL_matrix malloc error\n");
		goto err5;
	}
	FDat.last_STA1_MaxRGB = kzalloc((sizeof(unsigned int) *
		ldim_blk_row1 * ldim_blk_col1 * 3), GFP_KERNEL);
	if (FDat.last_STA1_MaxRGB == NULL) {
		LDIMERR("ldim_driver FDat.last_STA1_MaxRGB malloc error\n");
		goto err6;
	}
	FDat.TF_BL_matrix = kzalloc((sizeof(unsigned int) *
		ldim_blk_row1 * ldim_blk_col1), GFP_KERNEL);
	if (FDat.TF_BL_matrix == NULL) {
		LDIMERR("ldim_driver FDat.TF_BL_matrix malloc error\n");
		goto err7;
	}
	FDat.TF_BL_matrix_2 = kzalloc((sizeof(unsigned int) *
		ldim_blk_row1 * ldim_blk_col1), GFP_KERNEL);
	if (FDat.TF_BL_matrix_2 == NULL) {
		LDIMERR("ldim_driver FDat.TF_BL_matrix_2 malloc error\n");
		goto err8;
	}
	FDat.TF_BL_alpha = kzalloc((sizeof(unsigned int) *
		ldim_blk_row1 * ldim_blk_col1), GFP_KERNEL);
	if (FDat.TF_BL_alpha == NULL) {
		LDIMERR("ldim_driver FDat.TF_BL_alpha malloc error\n");
		goto err9;
	}

	return 0;

err9:
	kfree(FDat.TF_BL_matrix_2);
err8:
	kfree(FDat.TF_BL_matrix);
err7:
	kfree(FDat.last_STA1_MaxRGB);
err6:
	kfree(FDat.SF_BL_matrix);
err5:
	kfree(ldim_driver.local_ldim_matrix);
err4:
	kfree(ldim_driver.ldim_test_matrix);
err3:
	kfree(ldim_driver.max_rgb);
err2:
	kfree(ldim_driver.hist_matrix);
err1:
	kfree(ldim_driver.ldim_matrix_buf);
err0:
	return -1;
}

int aml_ldim_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i;
	unsigned int ldim_vsync_irq = 0, ldim_rdma_irq = 0;
	struct ldim_dev_s *devp = &ldim_dev;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	/* function flag */
	ldim_on_flag = 0;
	ldim_func_en = 0;
	ldim_remap_en = 0;
	ldim_func_bypass = 0;
	ldim_test_en = 0;

	ldim_brightness_level = 0;
	ldim_level_update = 0;

	/* alg flag */
	ldim_avg_update_en = 0;
	ldim_matrix_update_en = 0;
	ldim_alg_en = 0;
	ldim_top_en = 0;
	ldim_hist_en = 0;
	Dbprint_lv = 0;
	db_cnt = 0;

	memset(devp, 0, (sizeof(struct ldim_dev_s)));
	nPRM.val_1 = &val_1[0];
	nPRM.bin_1 = &bin_1[0];
	nPRM.val_2 = &val_2[0];
	nPRM.bin_2 = &bin_2[0];

	ret = aml_ldim_kzalloc(ldim_blk_row, ldim_blk_col);
	if (ret) {
		LDIMERR("%s failed\n", __func__);
		goto err;
	}

	ret = alloc_chrdev_region(&devp->aml_ldim_devno, 0, 1,
		AML_LDIM_DEVICE_NAME);
	if (ret < 0) {
		LDIMERR("failed to alloc major number\n");
		ret = -ENODEV;
		goto err;
	}

	devp->aml_ldim_clsp = class_create(THIS_MODULE, "aml_ldim");
	if (IS_ERR(devp->aml_ldim_clsp)) {
		ret = PTR_ERR(devp->aml_ldim_clsp);
		return ret;
	}
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->aml_ldim_clsp,
				&aml_ldim_class_attrs[i]) < 0)
			goto err1;
	}

	devp->aml_ldim_cdevp = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!devp->aml_ldim_cdevp) {
		ret = -ENOMEM;
		goto err2;
	}

	/* connect the file operations with cdev */
	cdev_init(devp->aml_ldim_cdevp, &ldim_fops);
	devp->aml_ldim_cdevp->owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(devp->aml_ldim_cdevp, devp->aml_ldim_devno, 1);
	if (ret) {
		LDIMERR("failed to add device\n");
		goto err3;
	}

	devp->dev = device_create(devp->aml_ldim_clsp, NULL,
		devp->aml_ldim_devno, NULL, AML_LDIM_CLASS_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto err4;
	}

	ldim_queue = create_workqueue("ldim workqueue");
	if (!ldim_queue) {
		LDIMERR("ldim_queue create failed\n");
		ret = -1;
		goto err;
	}
	INIT_WORK(&ldim_on_vs_work, ldim_on_update_brightness);
	INIT_WORK(&ldim_off_vs_work, ldim_off_update_brightness);

	spin_lock_init(&ldim_isr_lock);
	spin_lock_init(&rdma_ldim_isr_lock);

	bl_drv->res_ldim_vsync_irq =
		platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!bl_drv->res_ldim_vsync_irq) {
		ret = -ENODEV;
		LDIMERR("ldim_vsync_irq resource error\n");
		goto err;
	}
	ldim_vsync_irq = bl_drv->res_ldim_vsync_irq->start;
	LDIMPR("ldim_vsync_irq: %d\n", ldim_vsync_irq);
	if (request_irq(ldim_vsync_irq, ldim_vsync_isr, IRQF_SHARED,
		"ldim_vsync", (void *)"ldim_vsync"))
		LDIMERR("can't request ldim_vsync_irq\n");
	else
		LDIMPR("request ldim_vsync_irq successful\n");

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_GXTVBB:
		bl_drv->res_ldim_rdma_irq = platform_get_resource(pdev,
			IORESOURCE_IRQ, 1);
		if (!bl_drv->res_ldim_rdma_irq) {
			ret = -ENODEV;
			LDIMERR("ldim_rdma_irq resource error\n");
			goto err;
		}
		ldim_rdma_irq = bl_drv->res_ldim_rdma_irq->start;
		LDIMPR("ldim_rdma_irq: %d\n", ldim_rdma_irq);
		if (request_irq(ldim_rdma_irq, rdma_ldim_intr, IRQF_SHARED,
			"ldim_rdma", (void *)"ldim_rdma"))
			LDIMERR("can't request ldim_rdma_irq\n");
		else
			LDIMPR("request ldim_rdma_irq successful\n");
		break;
	default:
		break;
	}

	ldim_driver.valid_flag = 1;

	LDIMPR("%s ok\n", __func__);
	return 0;

err4:
	cdev_del(&devp->cdev);
err3:
	kfree(devp->aml_ldim_cdevp);
err2:
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->aml_ldim_clsp,
			&aml_ldim_class_attrs[i]);
	}
	class_destroy(devp->aml_ldim_clsp);
err1:
	unregister_chrdev_region(devp->aml_ldim_devno, 1);
err:
	return ret;
}

int aml_ldim_remove(void)
{
	unsigned int i;
	struct ldim_dev_s *devp = &ldim_dev;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	kfree(FDat.SF_BL_matrix);
	kfree(FDat.TF_BL_matrix);
	kfree(FDat.TF_BL_matrix_2);
	kfree(FDat.last_STA1_MaxRGB);
	kfree(FDat.TF_BL_alpha);
	kfree(ldim_driver.ldim_matrix_buf);
	kfree(ldim_driver.hist_matrix);
	kfree(ldim_driver.max_rgb);
	kfree(ldim_driver.ldim_test_matrix);
	kfree(ldim_driver.local_ldim_matrix);

	free_irq(bl_drv->res_ldim_rdma_irq->start, (void *)"ldim_rdma");
	free_irq(bl_drv->res_ldim_vsync_irq->start, (void *)"ldim_vsync");

	cdev_del(devp->aml_ldim_cdevp);
	kfree(devp->aml_ldim_cdevp);
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->aml_ldim_clsp,
				&aml_ldim_class_attrs[i]);
	}
	class_destroy(devp->aml_ldim_clsp);
	unregister_chrdev_region(devp->aml_ldim_devno, 1);

	LDIMPR("%s ok\n", __func__);
	return 0;
}

