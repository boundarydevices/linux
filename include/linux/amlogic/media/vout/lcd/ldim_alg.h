/*
 * include/linux/amlogic/media/vout/lcd/ldim_alg.h
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

#ifndef _INC_AML_LDIM_ALG_H_
#define _INC_AML_LDIM_ALG_H_

/*========================================*/
#define LD_STA_BIN_NUM 16
#define LD_STA_LEN_V 17
/*  support maximum 16x4 regions for statistics (16+1) */
#define LD_STA_LEN_H 25
/*  support maximum 16x4 regions for statistics (24+1)*/
#define LD_BLK_LEN_V 25
/*  support maximum 16 led of each side left/right(16+4+4+1)*/
#define LD_BLK_LEN_H 33
/*  support maximum 24 led of each side top/bot  (24+4+4+1)*/
#define LD_LUT_LEN 32
#define LD_BLKHMAX 32
#define LD_BLKVMAX 32

#define LD_BLKREGNUM 384  /* maximum support 24*16*/

struct LDReg_s {
	int reg_LD_pic_RowMax;            /*u13*/
	int reg_LD_pic_ColMax;            /*u13*/
	int reg_LD_pic_YUVsum[3];
	/* only output u16*3, (internal ACC will be u32x3)*/
	int reg_LD_pic_RGBsum[3];
	/* Statistic options */
	int reg_LD_STA_Vnum;
	/*u8: statistic region number of V, maximum to (STA_LEN_V-1)   (0~16)*/
	int reg_LD_STA_Hnum;
	/*u8: statistic region number of H, maximum to (STA_LEN_H-1)   (0~24)*/
	int reg_LD_BLK_Vnum;
	/*u8: Maximum to 16*/
	int reg_LD_BLK_Hnum;
	/*u8: Maximum to 24*/
	int reg_LD_STA1max_LPF;
	/*u1: STA1max statistics on [1 2 1]/4 filtered results*/
	int reg_LD_STA2max_LPF;
	/*u1: STA2max statistics on [1 2 1]/4 filtered results*/
	int reg_LD_STAhist_LPF;
	/*u1: STAhist statistics on [1 2 1]/4 filtered results*/
	int reg_LD_STA1max_Hdlt;
	/* u2: (2^x) extra pixels into Max calculation*/
	int reg_LD_STA1max_Vdlt;
	/* u4: extra pixels into Max calculation vertically*/
	int reg_LD_STA2max_Hdlt;
	/* u2: (2^x) extra pixels into Max calculation*/
	int reg_LD_STA2max_Vdlt;
	/* u4: extra pixels into Max calculation vertically*/
	int reg_LD_STA1max_Hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_LD_STA1max_Vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/
	int reg_LD_STA2max_Hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_LD_STA2max_Vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/

	int reg_LD_STAhist_mode;
		/*u3: histogram statistics on XX separately 20bits*16bins:
		 *0: R-only,1:G-only 2:B-only 3:Y-only;
		 *4: MAX(R,G,B), 5/6/7: R&G&B
		 */
	int reg_LD_STAhist_pix_drop_mode;
		/* u2:histogram statistics pixel drop mode:
		 * 0:no drop; 1:only statistic
		 * x%2=0; 2:only statistic x%4=0; 3: only statistic x%8=0;
		 */
	int reg_LD_STAhist_Hidx[LD_STA_LEN_H];  /* U12* STA_LEN_H*/
	int reg_LD_STAhist_Vidx[LD_STA_LEN_V];  /* u12x STA_LEN_V*/

	/***** FBC3 fw_hw_alg_frm *****/
	int reg_ldfw_BLmax;/* maximum BL value*/
	int reg_ldfw_blk_norm;
		/*u8: normalization gain for blk number,
		 *1/blk_num= norm>>(rs+8), norm = (1<<(rs+8))/blk_num
		 */
	int reg_ldfw_blk_norm_rs;/*u3: 0~7,  1/blk_num= norm>>(rs+8) */
	int reg_ldfw_sta_hdg_weight[8];
		/*  u8x8, weighting to each
		 * block's max to decide that block's ld.
		 */
	int reg_ldfw_sta_max_mode;
		/* u2: maximum selection for
		 *components: 0: r_max, 1: g_max, 2: b_max; 3: max(r,g,b)
		 */
	int reg_ldfw_sta_max_hist_mode;
		/* u2: mode of reference
		 * max/hist mode:0: MIN(max, hist), 1: MAX(max, hist)
		 * 2: (max+hist)/2, 3: (max(a,b)*3 + min(a,b))/4
		 */
	int reg_ldfw_hist_valid_rate;
		/* u8, norm to 512 as "1", if hist_matrix[i]>(rate*histavg)>>9*/
	int reg_ldfw_hist_valid_ofst;/*u8, hist valid bin upward offset*/
	int reg_ldfw_sedglit_RL;/*u1: single edge lit right/bottom mode*/
	int reg_ldfw_sf_thrd;/*u12: threshold of difference to enable the sf;*/
	int reg_ldfw_boost_gain;
		/* u8: boost gain for the region that
		 * is larger than the average, norm to 16 as "1"
		 */
	int reg_ldfw_tf_alpha_rate;
		/*u8: rate to SFB_BL_matrix from
		 *last frame difference;
		 */
	int reg_ldfw_tf_alpha_ofst;
		/*u8: ofset to alpha SFB_BL_matrix
		 *from last frame difference;
		 */
	int reg_ldfw_tf_disable_th;
		/*u8: 4x is the threshod to disable
		 *tf to the alpha (SFB_BL_matrix from last frame difference;
		 */
	int reg_ldfw_blest_acmode;
		/*u3: 0: est on BLmatrix;
		 *1: est on(BL-DC); 2: est on (BL-MIN);
		 *3: est on (BL-MAX) 4: 2048; 5:1024
		 */
	int reg_ldfw_sf_enable;
	/*u1: enable signal for spatial filter on the tbl_matrix*/
	int reg_ldfw_enable;
	int reg_ldfw_sta_hdg_vflt;
	int reg_ldfw_sta_norm;
	int reg_ldfw_sta_norm_rs;
	int reg_ldfw_tf_enable;
	int reg_LD_LUT_Hdg_TXLX[8][32];
	int reg_LD_LUT_Vdg_TXLX[8][32];
	int reg_LD_LUT_VHk_TXLX[8][32];
	int reg_LD_LUT_Id[16 * 24];
	int reg_LD_LUT_Hdg_LEXT_TXLX[8];
	int reg_LD_LUT_Vdg_LEXT_TXLX[8];
	int reg_LD_LUT_VHk_LEXT_TXLX[8];
	int reg_ldfw_boost_enable;
		/*u1: enable signal for Boost
		 *filter on the tbl_matrix
		 */
	int ro_ldfw_bl_matrix_avg;/*u12: read-only register for bl_matrix*/

	/* Backlit Modeling registers*/
	int BL_matrix[LD_BLKREGNUM];
	/* Define the RAM Matrix*/
	int reg_LD_BackLit_Xtlk;
	/*u1: 0 no block to block Xtalk model needed;   1: Xtalk model needed*/
	int reg_LD_BackLit_mode;
	/*u2: 0- LEFT/RIGHT Edge Lit; 1- Top/Bot Edge Lit; 2 - DirectLit
	 *modeled H/V independent; 3- DirectLit modeled HV Circle distribution
	 */
	int reg_LD_Reflect_Hnum;
	/*u3: numbers of band reflection considered in Horizontal direction;
	 *	0~4
	 */
	int reg_LD_Reflect_Vnum;
	/*u3: numbers of band reflection considered in Horizontal direction;
	 *	0~4
	 */
	int reg_LD_BkLit_curmod;
	/*u1: 0: H/V separately, 1 Circle distribution*/
	int reg_LD_BkLUT_Intmod;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	int reg_LD_BkLit_Intmod;
	/*u1: 0: linear interpolation, 1 cubical interpolation*/
	int reg_LD_BkLit_LPFmod;
	/*u3: 0: no LPF, 1:[1 14 1]/16;2:[1 6 1]/8; 3: [1 2 1]/4;
	 *	4:[9 14 9]/32  5/6/7: [5 6 5]/16;
	 */
	int reg_LD_BkLit_Celnum;
	/*u8: 0: 1920~ 61*/
	int reg_BL_matrix_AVG;
	/*u12: DC of whole picture BL to be subtract from BL_matrix
	 *	during modeling (Set by FW daynamically)
	 */
	int reg_BL_matrix_Compensate;
	/*u12: DC of whole picture BL to be compensated back to
	 *	Litfull after the model (Set by FW dynamically);
	 */
	int reg_LD_Reflect_Hdgr[20];
	/*20*u6:  cells 1~20 for H Gains of different dist of
	 * Left/Right;
	 */
	int reg_LD_Reflect_Vdgr[20];
	/*20*u6:  cells 1~20 for V Gains of different dist of Top/Bot;*/
	int reg_LD_Reflect_Xdgr[4];     /*4*u6:*/
	int reg_LD_Vgain;               /*u12*/
	int reg_LD_Hgain;               /*u12*/
	int reg_LD_Litgain;             /*u12*/
	int reg_LD_Litshft;
	/*u3   right shif of bits for the all Lit's sum*/
	int reg_LD_BkLit_valid[32];
	/*u1x32: valid bits for the 32 cell Bklit to contribut to current
	 *	position (refer to the backlit padding pattern)
	 */
	/* region division index 1 2 3 4 5(0) 6(1) 7(2)
	 *	8(3) 9(4)  10(5)11(6)12(7)13(8) 14(9)15(10) 16 17 18 19
	 */
	int reg_LD_BLK_Hidx[LD_BLK_LEN_H]; /* S14* BLK_LEN*/
	int reg_LD_BLK_Vidx[LD_BLK_LEN_V]; /* S14* BLK_LEN*/
	/* Define the RAM Matrix*/
	int reg_LD_LUT_Hdg[LD_LUT_LEN];  /*u10*/
	int reg_LD_LUT_Vdg[LD_LUT_LEN];  /*u10*/
	int reg_LD_LUT_VHk[LD_LUT_LEN];  /*u10*/
	/* VHk positive and negative side gain, normalized to 128
	 *	as "1" 20150428
	 */
	int reg_LD_LUT_VHk_pos[32];   /* u8*/
	int reg_LD_LUT_VHk_neg[32];   /* u8*/
	int reg_LD_LUT_HHk[32];
	/* u8 side gain for LED direction hdist gain for different LED*/
	/* VHo possitive and negative side offset, use with LS, (x<<LS)*/
	int reg_LD_LUT_VHo_pos[32];   /* s8*/
	int reg_LD_LUT_VHo_neg[32];   /* s8*/
	int reg_LD_LUT_VHo_LS;/* u3:0~6,left shift bits of VH0_pos/neg*/
	/* adding three cells for left boundary extend during
	 *	Cubic interpolation
	 */
	int reg_LD_LUT_Hdg_LEXT;
	int reg_LD_LUT_Vdg_LEXT;
	int reg_LD_LUT_VHk_LEXT;
	/* adding demo window mode for LD for RGB lut compensation*/
	int reg_LD_xlut_demo_roi_xstart;
	/* u14 start col index of the region of interest*/
	int reg_LD_xlut_demo_roi_xend;
	/* u14 end col index of the region of interest*/
	int reg_LD_xlut_demo_roi_ystart;
	/* u14 start row index of the region of interest*/
	int reg_LD_xlut_demo_roi_yend;
	/* u14 end row index of the region of interest*/
	int reg_LD_xlut_iroi_enable;
	/* u1: enable rgb LUT remapping inside regon of interest:
	 *	0: no rgb remapping; 1: enable rgb remapping
	 */
	int reg_LD_xlut_oroi_enable;
	/* u1: enable rgb LUT remapping outside regon of interest:
	 *	0: no rgb remapping; 1: enable rgb remapping
	 */
	/* Register used in RGB remapping*/
	int reg_LD_RGBmapping_demo;
	/*u2: 0 no demo mode 1: display BL_fulpel on RGB*/
	int reg_LD_X_LUT_interp_mode[3];
	/*U1 0: using linear interpolation between to neighbour LUT;
	 *	1: use the nearest LUT results
	 */
	int X_idx[1][16];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	int X_nrm[1][16];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	int X_lut[3][16][32];
	/* Changed to 16 Lits define 32 Bin LUT to save cost*/
	/* only do the Lit modleing on the AC part*/
	int X_lut2[3][16][16];
	int fw_LD_BLEst_ACmode;
	/*u2: 0: est on BLmatrix; 1: est on (BL-DC);
	 *	2: est on (BL-MIN); 3: est on (BL-MAX)
	 */
};

struct FW_DAT_s {
	/* for temporary Firmware algorithm */
	unsigned int *TF_BL_alpha;
	unsigned int *last_YUVsum;
	unsigned int *last_RGBsum;
	unsigned int *last_STA1_MaxRGB;
	unsigned int *SF_BL_matrix;
	unsigned int *TF_BL_matrix;
	unsigned int *TF_BL_matrix_2;
};

struct ldim_fw_para_s {
	/* header */
	unsigned int para_ver;
	unsigned int para_size;
	char ver_str[20];
	unsigned char ver_num;

	unsigned char hist_col;
	unsigned char hist_row;

	unsigned int fw_LD_ThSF_l;
	unsigned int fw_LD_ThTF_l;
	unsigned int boost_gain; /*norm 256 to 1,T960 finally use*/
	unsigned int TF_alpha; /*256;*/
	unsigned int lpf_gain;  /* [0~128~256], norm 128 as 1*/

	unsigned int boost_gain_neg;
	unsigned int alpha_delta;

	/*LPF tap: 0-lpf_res 41,1-lpf_res 114,...*/
	unsigned int lpf_res;    /* 1024/9*9 = 13,LPF_method=3 */
	unsigned int rgb_base;

	unsigned int ov_gain;
	/*unsigned int incr_dif_gain; //16 */

	unsigned int avg_gain;

	unsigned int fw_rgb_diff_th;
	unsigned int max_luma;
	unsigned int lmh_avg_TH;/*for woman flicker*/
	unsigned int fw_TF_sum_th;/*20180530*/

	unsigned int LPF_method;
	unsigned int LD_TF_STEP_TH;
	unsigned int TF_step_method;
	unsigned int TF_FRESH_BL;

	unsigned int TF_BLK_FRESH_BL;
	unsigned int side_blk_diff_th;
	unsigned int bbd_th;
	unsigned char bbd_detect_en;
	unsigned char diff_blk_luma_en;

	unsigned char Sf_bypass, Boost_light_bypass;
	unsigned char Lpf_bypass, Ld_remap_bypass;
	unsigned char black_frm;

	/* for debug print */
	unsigned char fw_hist_print;/*20180525*/
	unsigned int fw_print_frequent;/*20180606,print every 8 frame*/
	unsigned int Dbprint_lv;

	struct LDReg_s *nPRM;
	struct FW_DAT_s *FDat;
	unsigned int *bl_remap_curve; /* size: 16 */
	unsigned int *fw_LD_Whist;    /* size: 16 */

	void (*fw_alg_frm)(struct ldim_fw_para_s *fw_para,
		unsigned int *max_matrix, unsigned int *hist_matrix);
	void (*fw_alg_para_print)(struct ldim_fw_para_s *fw_para);
};
/* if struct ldim_fw_para_s changed, FW_PARA_VER must be update */
#define FW_PARA_VER    1

extern struct ldim_fw_para_s *aml_ldim_get_fw_para(void);

#endif
