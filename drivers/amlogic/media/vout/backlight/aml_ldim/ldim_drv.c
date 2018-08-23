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
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include "ldim_drv.h"
#include "ldim_reg.h"

#define AML_LDIM_DEV_NAME            "aml_ldim"

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
struct LDReg_s nPRM;
struct FW_DAT_s FDat;
static struct ldim_fw_para_s ldim_fw_para;
static struct aml_ldim_driver_s ldim_driver;

static unsigned int ldim_hist_row = 1;
static unsigned int ldim_hist_col = 1;
static unsigned int ldim_blk_row = 1;
static unsigned int ldim_blk_col = 1;
static unsigned int ldim_data_min = LD_DATA_MIN;
static unsigned int ldim_brightness_level;
static unsigned long litgain = LD_DATA_MAX;

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

/*BL_matrix remap curve*/
static unsigned long bl_remap_curve[16] = {
	612, 654, 721, 851, 1001, 1181, 1339, 1516,
	1738, 1948, 2152, 2388, 2621, 2889, 3159, 3502
};
static unsigned long fw_LD_Whist[16] = {
	32, 64, 96, 128, 160, 192, 224, 256,
	288, 320, 352, 384, 416, 448, 480, 512
};

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

static void ldim_on_vs_brightness(void);
static void ldim_off_vs_brightness(void);
static void ldim_on_vs_arithmetic(void);
static void ldim_dump_histgram(void);
static void ldim_get_matrix_info_max_rgb(void);

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

static void ldim_db_para_print(struct LDReg_s *mLDReg)
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
		ldim_fw_para.rgb_base,
		ldim_fw_para.boost_gain,
		ldim_fw_para.lpf_res,
		ldim_fw_para.fw_LD_ThSF_l);

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

static void ldim_db_load_update(struct LDReg_s *mLDReg)
{
	int i;

	if (LDIM_DATA_FROM_DB == 1) {
		LDIMPR("ldim_db_load_update\n");
		/* beam model */
		ldim_fw_para.rgb_base = (unsigned long)ldim_db_para.rgb_base;
		ldim_fw_para.boost_gain =
			(unsigned long)ldim_db_para.boost_gain;
		ldim_fw_para.lpf_res = (unsigned long)ldim_db_para.lpf_res;
		ldim_fw_para.fw_LD_ThSF_l =
			(unsigned long)ldim_db_para.fw_ld_th_sf;

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

static void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		unsigned int BLK_Vnum, unsigned int BLK_Hnum)
{
	unsigned int resolution, resolution_region, blk_height, blk_width;
	unsigned int row_start, col_start;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();

	BLK_Vnum = (BLK_Vnum == 0) ? 1 : BLK_Vnum;
	BLK_Hnum = (BLK_Hnum == 0) ? 1 : BLK_Hnum;
	LDIMPR("%s: %d %d %d %d\n", __func__, pic_h, pic_v, BLK_Vnum, BLK_Hnum);

	ldim_fw_para.hist_col = BLK_Vnum;
	ldim_fw_para.hist_row = BLK_Hnum;

	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*Wr_reg(VDIN0_HIST_CTRL, 0x10d);*/

	switch (bl_drv->data->chip_type) {
	case BL_CHIP_TXLX:
		Wr_reg(LDIM_STTS_CTRL0, 7 << 2);
		ldim_set_matrix_ycbcr2rgb();
		ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);
		break;
	case BL_CHIP_GXTVBB:
		Wr_reg(LDIM_STTS_CTRL0, 3 << 3);/*4 mux to vpp_dout*/
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

	return IRQ_HANDLED;
}

static int brightness_vs_cnt;
static void ldim_on_vs_brightness(void)
{
	unsigned int size;
	unsigned int i;

	if (ldim_on_flag == 0)
		return;

	if (brightness_vs_cnt++ >= 30)
		brightness_vs_cnt = 0;

	if (ldim_func_bypass)
		return;

	if (ldim_fw_para.fw_alg_frm == NULL) {
		if (brightness_vs_cnt == 0)
			LDIMERR("%s: ldim_alg ko is not installed\n", __func__);
		return;
	}

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
			if (brightness_vs_cnt == 0) {
				ldim_get_matrix_info_max_rgb();
				ldim_dump_histgram();
			}
		}
	} else {
		if (brightness_vs_cnt == 0)
			LDIMERR("%s: device_bri_update is null\n", __func__);
	}
}

static void ldim_off_vs_brightness(void)
{
	unsigned int size;
	unsigned int i;
	int ret;

	if (ldim_on_flag == 0)
		return;

	if (brightness_vs_cnt++ >= 30)
		brightness_vs_cnt = 0;

	size = ldim_blk_row * ldim_blk_col;

	if (ldim_level_update) {
		ldim_level_update = 0;
		if (ldim_debug_print) {
			if (brightness_vs_cnt == 0)
				LDIMPR("%s: level update: 0x%lx\n", __func__,
					litgain);
		}
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
					if (brightness_vs_cnt == 0) {
						LDIMERR(
						"%s: device_bri_check error\n",
						__func__);
					}
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
			if (brightness_vs_cnt == 0) {
				ldim_get_matrix_info_max_rgb();
				ldim_dump_histgram();
			}
		}
	} else {
		if (brightness_vs_cnt == 0)
			LDIMERR("%s: device_bri_update is null\n", __func__);
	}
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
		if (ldim_fw_para.fw_alg_frm) {
			ldim_fw_para.fw_alg_frm(&ldim_fw_para,
				local_ldim_max_rgb, local_ldim_hist);
		}
	}

	kfree(local_ldim_hist);
	kfree(local_ldim_max);
	kfree(local_ldim_max_rgb);
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
		len += sprintf(buf+len, "\t%ld", bl_remap_curve[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_ld_remap_lut_print(int index)
{
	int i, j, len;
	char *buf;

	len = 32 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LDIMERR("print buf malloc error\n");
		return;
	}

	if (index == 0xff) {
		pr_info("LD_remap_lut:\n");
		for (i = 0; i < 16; i++) {
			pr_info("  %d:\n", i);
			len = 0;
			for (j = 0; j < 32; j++) {
				if (j == 16)
					len += sprintf(buf+len, "\n");
				len += sprintf(buf+len, "\t%d",
					LD_remap_lut[i][j]);
			}
			pr_info("%s\n", buf);
		}
	} else if (index < 16) {
		pr_info("LD_remap_lut %d:\n", index);
		len = 0;
		for (j = 0; j < 32; j++) {
			if (j == 16)
				len += sprintf(buf+len, "\n");
			len += sprintf(buf+len, "\t%d",
				LD_remap_lut[index][j]);
		}
		pr_info("%s\n", buf);
	} else {
		pr_info("LD_remap_lut invalid index %d\n", index);
	}
	pr_info("\n");

	kfree(buf);
}

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

	pr_info("current black_frm: %d\n", ldim_fw_para.black_frm);
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

	LDIMPR("%s\n", __func__);

	/* init ldim */
	ldim_stts_initial(ldim_config.hsize, ldim_config.vsize,
		ldim_hist_row, ldim_hist_col);
	LDIM_Initial(ldim_config.hsize, ldim_config.vsize,
		ldim_blk_row, ldim_blk_col, ldim_config.bl_mode, 1, 0);

	ldim_func_ctrl(0); /* default disable ldim function */

	if (ldim_driver.pinmux_ctrl)
		ldim_driver.pinmux_ctrl(1);
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
	.pinmux_flag = 0xff,
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
	"echo alg_info > /sys/class/aml_ldim/attr\n");
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
	len += sprintf(buf+len,
	"echo alpha_delta 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo boost_gain_neg 4 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo TF_alpha 256 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_gain 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_res 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo rgb_base 128 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo ov_gain 16 > /sys/class/aml_ldim/attr\n");
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
	"echo LD_remap_lut > /sys/class/aml_ldim/attr\n");

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
	int i, j;
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
		if (parm[16] != NULL) {
			for (i = 0; i < 16; i++) {
				if (kstrtoul(parm[i+1], 10,
					&bl_remap_curve[i]) < 0) {
					goto ldim_attr_store_end;
				}
			}

			ldim_bl_remap_curve_print();
			pr_info("\n********bl_remap_curve ok*********\n");
		}
	} else if (!strcmp(parm[0], "fw_LD_Whist")) {
		for (i = 0; i < 16; i++)
			pr_info("(%d),", (unsigned int)fw_LD_Whist[i]);
		if (parm[16] != NULL) {
			for (i = 0; i < 16; i++) {
				if (kstrtoul(parm[i+1], 10,
					&fw_LD_Whist[i]) < 0) {
					goto ldim_attr_store_end;
				}
			}

			for (i = 0; i < 16; i++)
				pr_info("(%d),", (unsigned int)fw_LD_Whist[i]);
			pr_info("\n********fw_LD_Whist ok*********\n");
		}
	} else if (!strcmp(parm[0], "LD_remap_lut")) {
		if (parm[33] != NULL) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_end;
			ldim_ld_remap_lut_print(i);
			for (j = 0; j < 32; j++) {
				if (kstrtouint(parm[j+2], 10,
					&LD_remap_lut[i][j]) < 0) {
					goto ldim_attr_store_end;
				}
			}

			ldim_ld_remap_lut_print(i);
			pr_info("\n********LD_remap_lut ok*********\n");
		} else if (parm[1] != NULL) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_end;
			ldim_ld_remap_lut_print(i);
		} else {
			ldim_ld_remap_lut_print(0xff);
		}
	} else if (!strcmp(parm[0], "Sf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.Sf_bypass = (unsigned char)val1;
		}
		pr_info("Sf_bypass = %d\n", ldim_fw_para.Sf_bypass);
	} else if (!strcmp(parm[0], "Boost_light_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.Boost_light_bypass = (unsigned char)val1;
		}
		pr_info("Boost_light_bypass = %d\n",
			ldim_fw_para.Boost_light_bypass);
	} else if (!strcmp(parm[0], "Lpf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.Lpf_bypass = (unsigned char)val1;
		}
		pr_info("Lpf_bypass = %d\n", ldim_fw_para.Lpf_bypass);
	} else if (!strcmp(parm[0], "Ld_remap_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.Ld_remap_bypass = (unsigned char)val1;
		}
		pr_info("Ld_remap_bypass = %d\n", ldim_fw_para.Ld_remap_bypass);
	} else if (!strcmp(parm[0], "ov_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.ov_gain = (unsigned int)val1;
		}
		pr_info("ov_gain = %d\n", ldim_fw_para.ov_gain);
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
			if (kstrtoul(parm[1], 10,
				&ldim_fw_para.fw_LD_ThSF_l) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("fw_LD_ThSF_l = %ld\n", ldim_fw_para.fw_LD_ThSF_l);
	} else if (!strcmp(parm[0], "fw_LD_ThTF_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10,
				&ldim_fw_para.fw_LD_ThTF_l) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("fw_LD_ThTF_l = %ld\n", ldim_fw_para.fw_LD_ThTF_l);
	} else if (!strcmp(parm[0], "boost_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.boost_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("boost_gain = %ld\n", ldim_fw_para.boost_gain);
	} else if (!strcmp(parm[0], "boost_gain_neg")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10,
				&ldim_fw_para.boost_gain_neg) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("boost_gain_neg = %ld\n", ldim_fw_para.boost_gain_neg);
	} else if (!strcmp(parm[0], "alpha_delta")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10,
				&ldim_fw_para.alpha_delta) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("alpha_delta = %ld\n", ldim_fw_para.alpha_delta);
	} else if (!strcmp(parm[0], "TF_alpha")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.TF_alpha) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("TF_alpha = %ld\n", ldim_fw_para.TF_alpha);
	} else if (!strcmp(parm[0], "lpf_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.lpf_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("lpf_gain = %ld\n", ldim_fw_para.lpf_gain);
	} else if (!strcmp(parm[0], "lpf_res")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.lpf_res) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("lpf_res = %ld\n", ldim_fw_para.lpf_res);
	} else if (!strcmp(parm[0], "rgb_base")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.rgb_base) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("rgb_base = %ld\n", ldim_fw_para.rgb_base);
	} else if (!strcmp(parm[0], "avg_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &ldim_fw_para.avg_gain) < 0)
				goto ldim_attr_store_end;
		}
		pr_info("avg_gain = %ld\n", ldim_fw_para.avg_gain);
	} else if (!strcmp(parm[0], "fw_rgb_diff_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.fw_rgb_diff_th = (unsigned int)val1;
		}
		pr_info("fw_rgb_diff_th = %d\n", ldim_fw_para.fw_rgb_diff_th);
	} else if (!strcmp(parm[0], "max_luma")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.max_luma = (unsigned int)val1;
		}
		pr_info("max_luma = %d\n", ldim_fw_para.max_luma);
	} else if (!strcmp(parm[0], "lmh_avg_TH")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.lmh_avg_TH = (unsigned int)val1;
		}
		pr_info("lmh_avg_TH = %d\n", ldim_fw_para.lmh_avg_TH);
	} else if (!strcmp(parm[0], "fw_TF_sum_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.fw_TF_sum_th = (unsigned int)val1;
		}
		pr_info("fw_TF_sum_th = %d\n", ldim_fw_para.fw_TF_sum_th);
	} else if (!strcmp(parm[0], "LPF_method")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.LPF_method = (unsigned int)val1;
		}
		pr_info("LPF_method = %d\n", ldim_fw_para.LPF_method);
	} else if (!strcmp(parm[0], "LD_TF_STEP_TH")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.LD_TF_STEP_TH = (unsigned int)val1;
		}
		pr_info("LD_TF_STEP_TH = %d\n", ldim_fw_para.LD_TF_STEP_TH);
	} else if (!strcmp(parm[0], "TF_step_method")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.TF_step_method = (unsigned int)val1;
		}
		pr_info("TF_step_method = %d\n", ldim_fw_para.TF_step_method);
	} else if (!strcmp(parm[0], "TF_FRESH_BL")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.TF_FRESH_BL = (unsigned int)val1;
		}
		pr_info("TF_FRESH_BL = %d\n", ldim_fw_para.TF_FRESH_BL);
	} else if (!strcmp(parm[0], "TF_BLK_FRESH_BL")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.TF_BLK_FRESH_BL = (unsigned int)val1;
		}
		pr_info("TF_BLK_FRESH_BL = %d\n", ldim_fw_para.TF_BLK_FRESH_BL);
	} else if (!strcmp(parm[0], "bbd_detect_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.bbd_detect_en = (unsigned char)val1;
		}
		pr_info("bbd_detect_en = %d\n", ldim_fw_para.bbd_detect_en);
	} else if (!strcmp(parm[0], "side_blk_diff_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.side_blk_diff_th = (unsigned int)val1;
		}
		pr_info("side_blk_diff_th = %d\n",
			ldim_fw_para.side_blk_diff_th);
	} else if (!strcmp(parm[0], "bbd_th")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.bbd_th = (unsigned int)val1;
		}
		pr_info("bbd_th = %d\n", ldim_fw_para.bbd_th);
	} else if (!strcmp(parm[0], "diff_blk_luma_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.diff_blk_luma_en = (unsigned char)val1;
		}
		pr_info("diff_blk_luma_en = %d\n",
			ldim_fw_para.diff_blk_luma_en);
	} else if (!strcmp(parm[0], "fw_hist_print")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.fw_hist_print = (unsigned char)val1;
		}
		pr_info("fw_hist_print = %d\n", ldim_fw_para.fw_hist_print);
	} else if (!strcmp(parm[0], "fw_print_frequent")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.fw_print_frequent = (unsigned int)val1;
		}
		pr_info("fw_print_frequent = %d\n",
			ldim_fw_para.fw_print_frequent);
	} else if (!strcmp(parm[0], "Dbprint_lv")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_end;
			ldim_fw_para.Dbprint_lv = (unsigned int)val1;
		}
		pr_info("Dbprint_lv = %d\n", ldim_fw_para.Dbprint_lv);
	} else if (!strcmp(parm[0], "alg_info")) {
		pr_info("ldim_alg_ver: %s\n", ldim_fw_para.ver_str);
		pr_info("ldim_fw_alg_frm: 0x%p\n\n", ldim_fw_para.fw_alg_frm);
		switch (ldim_fw_para.ver_num) {
		case 0:
			pr_info("fw_LD_ThSF_l         = %ld\n"
				"fw_LD_ThTF_l         = %ld\n"
				"boost_gain           = %ld\n"
				"TF_alpha             = %ld\n"
				"lpf_gain             = %ld\n"
				"lpf_res              = %ld\n"
				"rgb_base             = %ld\n\n",
				ldim_fw_para.fw_LD_ThSF_l,
				ldim_fw_para.fw_LD_ThTF_l,
				ldim_fw_para.boost_gain,
				ldim_fw_para.TF_alpha,
				ldim_fw_para.lpf_gain,
				ldim_fw_para.lpf_res,
				ldim_fw_para.rgb_base);
			break;
		case 1:
			pr_info("fw_LD_ThSF_l         = %ld\n"
				"fw_LD_ThTF_l         = %ld\n"
				"boost_gain           = %ld\n"
				"boost_gain_neg       = %ld\n"
				"alpha_delta          = %ld\n\n",
				ldim_fw_para.fw_LD_ThSF_l,
				ldim_fw_para.fw_LD_ThTF_l,
				ldim_fw_para.boost_gain,
				ldim_fw_para.boost_gain_neg,
				ldim_fw_para.alpha_delta);
			break;
		default:
			break;
		}
		pr_info("fw_rgb_diff_th       = %d\n"
			"max_luma             = %d\n"
			"lmh_avg_TH           = %d\n"
			"fw_TF_sum_th         = %d\n"
			"LPF_method           = %d\n"
			"LD_TF_STEP_TH        = %d\n"
			"TF_step_method       = %d\n"
			"TF_FRESH_BL          = %d\n\n",
			ldim_fw_para.fw_rgb_diff_th,
			ldim_fw_para.max_luma,
			ldim_fw_para.lmh_avg_TH,
			ldim_fw_para.fw_TF_sum_th,
			ldim_fw_para.LPF_method,
			ldim_fw_para.LD_TF_STEP_TH,
			ldim_fw_para.TF_step_method,
			ldim_fw_para.TF_FRESH_BL);
		pr_info("TF_BLK_FRESH_BL      = %d\n"
			"side_blk_diff_th     = %d\n"
			"bbd_th               = %d\n"
			"bbd_detect_en        = %d\n"
			"diff_blk_luma_en     = %d\n\n",
			ldim_fw_para.TF_BLK_FRESH_BL,
			ldim_fw_para.side_blk_diff_th,
			ldim_fw_para.bbd_th,
			ldim_fw_para.bbd_detect_en,
			ldim_fw_para.diff_blk_luma_en);
		pr_info("ov_gain              = %d\n"
			"avg_gain             = %ld\n"
			"litgain              = %ld\n\n",
			ldim_fw_para.ov_gain,
			ldim_fw_para.avg_gain,
			litgain);
		pr_info("Sf_bypass            = %d\n"
			"Boost_light_bypass   = %d\n"
			"Lpf_bypass           = %d\n"
			"Ld_remap_bypass      = %d\n\n",
			ldim_fw_para.Sf_bypass,
			ldim_fw_para.Boost_light_bypass,
			ldim_fw_para.Lpf_bypass,
			ldim_fw_para.Ld_remap_bypass);
		pr_info("fw_hist_print        = %d\n"
			"fw_print_frequent    = %d\n"
			"Dbprint_lv           = %d\n\n",
			ldim_fw_para.fw_hist_print,
			ldim_fw_para.fw_print_frequent,
			ldim_fw_para.Dbprint_lv);
	} else if (!strcmp(parm[0], "info")) {
		pr_info("ldim_drv_ver: %s\n", LDIM_DRV_VER);
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
		ldim_driver.dev_index = (unsigned char)para[0];
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

static struct ldim_fw_para_s ldim_fw_para = {
	.ver_str = "not installed",
	.ver_num = 0,

	.nPRM = &nPRM,
	.FDat = &FDat,
	.bl_remap_curve = bl_remap_curve,
	.fw_LD_Whist = fw_LD_Whist,

	.fw_LD_ThSF_l = 1600,
	.fw_LD_ThTF_l = 256,
	.boost_gain = 456, /*norm 256 to 1,T960 finally use*/
	.TF_alpha = 256, /*256;*/
	.lpf_gain = 128,  /* [0~128~256], norm 128 as 1*/
	.boost_gain_neg = 3,
	.alpha_delta = 255,/* to fix flicker */

	.lpf_res = 14,    /* 1024/9*9 = 13,LPF_method=3 */
	.rgb_base = 127,

	.ov_gain = 16,

	.LPF_method = 3,
	.LD_TF_STEP_TH = 100,
	.TF_step_method = 3,
	.TF_FRESH_BL = 8,

	.TF_BLK_FRESH_BL = 5,
	.side_blk_diff_th = 100,
	.bbd_th = 200,
	.bbd_detect_en = 1,
	.diff_blk_luma_en = 1,

	.fw_rgb_diff_th = 32760,
	.max_luma = 4060,
	.lmh_avg_TH = 200,/*for woman flicker*/
	.fw_TF_sum_th = 32760,/*20180530*/

	.avg_gain = LD_DATA_MAX,

	.Sf_bypass = 0,
	.Boost_light_bypass = 0,
	.Lpf_bypass = 0,
	.Ld_remap_bypass = 0,
	.black_frm = 0,

	/* debug print flag */
	.fw_hist_print = 0,
	.fw_print_frequent = 8,
	.Dbprint_lv = 0,

	.fw_alg_frm = NULL,
};

struct ldim_fw_para_s *aml_ldim_get_fw_para(void)
{
	return &ldim_fw_para;
}
EXPORT_SYMBOL(aml_ldim_get_fw_para);

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

#ifdef LDIM_DEBUG_INFO
	ldim_debug_print = 1;
#endif

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

	ret = aml_ldim_kzalloc(ldim_blk_row, ldim_blk_col);
	if (ret) {
		LDIMERR("%s failed\n", __func__);
		goto err;
	}

	memset(devp, 0, (sizeof(struct ldim_dev_s)));

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

	free_irq(bl_drv->res_ldim_vsync_irq->start, (void *)"ldim_vsync");
	switch (bl_drv->data->chip_type) {
	case BL_CHIP_GXTVBB:
		if (bl_drv->res_ldim_rdma_irq) {
			free_irq(bl_drv->res_ldim_rdma_irq->start,
				(void *)"ldim_rdma");
		}
		break;
	default:
		break;
	}

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

