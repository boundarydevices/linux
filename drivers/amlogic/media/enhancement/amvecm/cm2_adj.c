/*
 * cm2_adj for pq module
 *
 * Copyright (c) 2017 powerqin <yong.qin@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/module.h>

/* moudle headers */
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/vpp_regs.h"
#include "cm2_adj.h"

#define NUM_MATRIX_PARAM 7
#define NUM_SMTH_PARAM 11
static uint lpf_coef_matrix_param = NUM_MATRIX_PARAM;
static uint lpf_coef[NUM_MATRIX_PARAM] = {
	0, 16, 32, 32, 32, 16, 0
};

static uint color_key_pts_matrix_param = NUM_MATRIX_PARAM;
static uint color_key_pts[NUM_MATRIX_PARAM] = {
	4, 8, 12, 15, 19, 25, 30
};

static uint smth_coef_hue_matrix_param = NUM_MATRIX_PARAM;
static uint smth_coef_hue[NUM_SMTH_PARAM] = {
	0, 20, 40, 80, 110, 128, 110, 80, 40, 20, 0
};

static uint smth_coef_luma_matrix_param = NUM_MATRIX_PARAM;
static uint smth_coef_luma[NUM_SMTH_PARAM] = {
	40, 100, 105, 110, 115, 120, 115, 110, 85, 60, 40
};

static uint smth_coef_sat_matrix_param = NUM_MATRIX_PARAM;
static uint smth_coef_sat[NUM_SMTH_PARAM] = {
	40, 60, 85, 105, 115, 120, 115, 105, 85, 60, 30
};

module_param_array(lpf_coef, uint,
	&lpf_coef_matrix_param, 0664);
MODULE_PARM_DESC(lpf_coef, "\n lpf_coef\n");

module_param_array(color_key_pts, uint,
	&color_key_pts_matrix_param, 0664);
MODULE_PARM_DESC(color_key_pts, "\n color_key_pts\n");

module_param_array(smth_coef_hue, uint,
	&smth_coef_hue_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_hue_matrix_param, "\n smth_coef_hue\n");

module_param_array(smth_coef_luma, uint,
	&smth_coef_luma_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_luma_matrix_param, "\n smth_coef_luma\n");

module_param_array(smth_coef_sat, uint,
	&smth_coef_sat_matrix_param, 0664);
MODULE_PARM_DESC(smth_coef_sat_matrix_param, "\n smth_coef_sat\n");

/*static int lpf_coef[] = {
 *	0, 0, 32, 64, 32, 0, 0
 *};
 *static int color_key_pts[] = {
 *	4, 9, 12, 15, 19, 25, 31
 *};
 */

/*smth_coef= round([0.2 0.6 0.9 1 0.9 0.6 0.2]*128)*/
/*static int smth_coef[] = {26, 77, 115, 128, 115, 77, 26};*/

/*original value x100 */
static int huegain_via_sat5[7][5] = {
	{100, 100, 100, 100, 100},
	{30, 60, 80, 100, 100},
	{100, 100, 80, 50, 30},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
	{100, 100, 100, 100, 100},
};

/*original value x100 */
static int satgain_via_sat3[7][3] = {
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
	{100, 100, 100},
};

static int rsround(int val)
{
	if (val > 0)
		val = val + 50;
	else if (val < 0)
		val = val - 50;

	return val;
}

/**
 * [color_adj caculate lut table]
 * @param  inp_color     [0~6]
 * @param  inp_val       [-100~100]
 * @param  lpf_en        [1~0]
 * @param  lpf_coef      [tab]
 * @param  color_key_pts [tab]
 * @param  smth_coef     [tab]
 * @param  out_lut       [output lut]
 * @return               [fail, true]
 */
static void color_adj(int inp_color, int inp_val, int lpf_en,
				int *lpf_coef, int *color_key_pts,
				int *smth_coef, int *out_lut)
{
	/*int smth_win = 7;*/
	int inp_val2, temp;
	int smth_val[11];
	int x, k;
	int kpt;
	int varargin_1;

	inp_val2 = max(-128, min(127, inp_val));

	for (x = 0; x < NUM_SMTH_PARAM; x++) {
		if (inp_val2 > 0) {
			temp = ((smth_coef[x] * inp_val2 * 100/128) + 50)/100;
			smth_val[x] = temp;
		} else if (inp_val2 < 0) {
			temp = ((smth_coef[x] * inp_val2 * 100/128) - 50)/100;
			smth_val[x] = temp;
		} else {
			smth_val[x] = ((smth_coef[x] * inp_val2 * 100/128))/100;
		}
	}

	kpt = color_key_pts[inp_color];

	for (x = 0; x < NUM_SMTH_PARAM; x++) {
		inp_val2 = kpt + x - (NUM_SMTH_PARAM/2);
		if (inp_val2 < 0)
			inp_val2 = 32 - abs(inp_val2);

		if (inp_val2 > 31)
			inp_val2 -= 32;

		out_lut[(int)inp_val2] = 0;

		/*  reset to 0 before changing?? */
		out_lut[(int)inp_val2] += smth_val[x];
	}

	if (lpf_en) {
		for (x = 0; x < 32; x++) {
			inp_val2 = 0;
			for (k = 0; k < 7; k++) {
				varargin_1 = (x + k) - 3;
				if (varargin_1 < 0)
					varargin_1 = 32 - abs(varargin_1);

				if (varargin_1 > 31)
					varargin_1 -= 32;

				inp_val2 += lpf_coef[k] *
					out_lut[varargin_1];
			}
			out_lut[x] = inp_val2 / 128;
		}
	}
}

/**
 * [cm2_curve_update_hue description]
 *	0:purple, node 0-6, key pt as 4
 *	1:red, node 7-10, key pt as 9
 *	2: skin, node 11-14, key pt as 12
 *	3:yellow, node 15-17, key pt as 15
 *	4:green, node 18-22, key pt as 19
 *	5: cyan, node 23-28, key pt as 25
 *	6: blue, node 29-31, key pt as 29
 * @param colormode [description]
 * @param Adj_Hue_via_S[][32]  [description]
 */
static void cm2_curve_update_hue_by_hs(enum eCM2ColorMd colormode,
						char Adj_Hue_via_S[][32])
{
	unsigned int i, j, start = 0, end = 0;
	unsigned int val1[5] = {0}, val2[5] = {0};
	int temp, reg_node1, reg_node2;

	if (colormode == eCM2ColorMd_purple) {
		start = 0;
		end = 6;
	} else if (colormode == eCM2ColorMd_red) {
		start = 6;
		end = 10;
	} else if (colormode == eCM2ColorMd_skin) {
		start = 10;
		end = 14;
	} else if (colormode == eCM2ColorMd_yellow) {
		start = 14;
		end = 18;
	} else if (colormode == eCM2ColorMd_green) {
		start = 17;
		end = 23;
	} else if (colormode == eCM2ColorMd_cyan) {
		start = 23;
		end = 28;
	} else if (colormode == eCM2ColorMd_blue) {
		start = 27;
		end = 31;
	}

	reg_node1 = (CM2_ENH_COEF2_H00 - 0x100) % 8;
	reg_node2 = (CM2_ENH_COEF3_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		if (i > 31)
			i = i  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
		    val1[j] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);

			if (j == reg_node1) {
				/*curve 0,1*/
				val1[j] &= 0x0000ffff;
				temp = Adj_Hue_via_S[0][i];
				val1[j] |= (temp << 16) & 0x00ff0000;
				temp = Adj_Hue_via_S[1][i];
				val1[j] |= (temp << 24) & 0xff000000;
				continue;
			}
		}

		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val1[j]);
		}

		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
			val2[j] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
			if (j == reg_node2) {
				/*curve 2,3,4*/
				val2[j] &= 0xff000000;
				val2[j] |= Adj_Hue_via_S[2][i]
					& 0x000000ff;
				temp = Adj_Hue_via_S[3][i];
				val2[j] |= (temp << 8) & 0x0000ff00;
				temp = Adj_Hue_via_S[4][i];
				val2[j] |= (temp << 16) & 0x00ff0000;
				continue;
			}
			/*pr_info("0x%x, 0x%x\n", val1, val2);*/
		}

		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val2[j]);
		}
	}
}

static void cm2_curve_update_hue(enum eCM2ColorMd colormode,
						char *hue_lut)
{
	unsigned int i, j, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;

	if (colormode == eCM2ColorMd_purple) {
		start = 0;
		end = 6;
	} else if (colormode == eCM2ColorMd_red) {
		start = 6;
		end = 10;
	} else if (colormode == eCM2ColorMd_skin) {
		start = 10;
		end = 14;
	} else if (colormode == eCM2ColorMd_yellow) {
		start = 14;
		end = 18;
	} else if (colormode == eCM2ColorMd_green) {
		start = 17;
		end = 23;
	} else if (colormode == eCM2ColorMd_cyan) {
		start = 23;
		end = 28;
	} else if (colormode == eCM2ColorMd_blue) {
		start = 27;
		end = 31;
	}

	reg_node = (CM2_ENH_COEF1_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		if (i > 31)
			i = i  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
		    val1[j] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
			if (j == reg_node) {
				/*curve 0*/
				val1[j] &= 0xffffff00;
				temp = hue_lut[i];
				val1[j] |= (temp) & 0x000000ff;
				continue;
			}
		}
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				0x100 + i*8 + j);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val1[j]);
		}
	}
}

/**
 * [cm2_curve_update_luma description]
 *	0:purple, node 0-6, key pt as 4
 *	1:red, node 7-10, key pt as 9
 *	2: skin, node 11-14, key pt as 12
 *	3:yellow, node 15-17, key pt as 15
 *	4:green, node 18-22, key pt as 19
 *	5: cyan, node 23-28, key pt as 25
 *	6: blue, node 29-31, key pt as 29
 * @param colormode [description]
 * @param luma_lut  [description]
 */
static void cm2_curve_update_luma(enum eCM2ColorMd colormode,
						char *luma_lut)
{
	/*0:purple, node 0-6, key pt as 4*/
	/*1:red, node 7-10, key pt as 9*/
	/*2: skin, node 11-14, key pt as 12*/
	/*3:yellow, node 15-17, key pt as 15*/
	/*4:green, node 18-22, key pt as 19*/
	/*5: cyan, node 23-28, key pt as 25*/
	/*6: blue, node 29-31, key pt as 29*/
	unsigned int i, j, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;

	if (colormode == eCM2ColorMd_purple) {
		start = 0;
		end = 6;
	} else if (colormode == eCM2ColorMd_red) {
		start = 6;
		end = 10;
	} else if (colormode == eCM2ColorMd_skin) {
		start = 10;
		end = 14;
	} else if (colormode == eCM2ColorMd_yellow) {
		start = 14;
		end = 18;
	} else if (colormode == eCM2ColorMd_green) {
		start = 17;
		end = 23;
	} else if (colormode == eCM2ColorMd_cyan) {
		start = 23;
		end = 28;
	} else if (colormode == eCM2ColorMd_blue) {
		start = 27;
		end = 31;
	}

	reg_node = (CM2_ENH_COEF0_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		if (i > 31)
			i = i  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				CM2_ENH_COEF0_H00 + i*8 + j);
		    val1[j] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
			if (j == reg_node) {
				/*curve 0*/
				val1[j] &= 0xffffff00;
				temp = luma_lut[i];
				val1[j] |= (temp) & 0x000000ff;
				continue;
			}
		}
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				CM2_ENH_COEF0_H00 + i*8 + j);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val1[j]);
		}
	}
}

/**
 * [cm2_curve_update_sat description]
 *	0:purple, node 0-6, key pt as 4
 *	1:red, node 7-10, key pt as 9
 *	2: skin, node 11-14, key pt as 12
 *	3:yellow, node 15-17, key pt as 15
 *	4:green, node 18-22, key pt as 19
 *	5: cyan, node 23-28, key pt as 25
 *	6: blue, node 29-31, key pt as 29
 * @param colormode [description]
 * @param Adj_Sat_via_HS[3][32]  [description]
 */
static void cm2_curve_update_sat(enum eCM2ColorMd colormode,
						char Adj_Sat_via_HS[3][32])
{
	unsigned int i, j, start = 0, end = 0;
	unsigned int val1[5] = {0};
	int temp = 0, reg_node;

	if (colormode == eCM2ColorMd_purple) {
		start = 0;
		end = 6;
	} else if (colormode == eCM2ColorMd_red) {
		start = 6;
		end = 10;
	} else if (colormode == eCM2ColorMd_skin) {
		start = 10;
		end = 14;
	} else if (colormode == eCM2ColorMd_yellow) {
		start = 14;
		end = 18;
	} else if (colormode == eCM2ColorMd_green) {
		start = 17;
		end = 23;
	} else if (colormode == eCM2ColorMd_cyan) {
		start = 23;
		end = 28;
	} else if (colormode == eCM2ColorMd_blue) {
		start = 27;
		end = 31;
	}

	reg_node = (CM2_ENH_COEF0_H00 - 0x100) % 8;

	for (i = start; i <= end; i++) {
		if (i > 31)
			i = i  - 32;
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				CM2_ENH_COEF0_H00 + i*8 + j);
			val1[j] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
			if (j == reg_node) {
				val1[j] &= 0x000000ff;
				/*curve 0*/
				temp = Adj_Sat_via_HS[0][i];
				val1[j] |= (temp << 8) & 0x0000ff00;
				/*curve 1*/
				temp = Adj_Sat_via_HS[1][i];
				val1[j] |= (temp << 16) & 0x00ff0000;
				/*curve 2*/
				temp = Adj_Sat_via_HS[2][i];
				val1[j] |= (temp << 24) & 0xff000000;
				continue;
			}
		}
		for (j = 0; j < 5; j++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				CM2_ENH_COEF0_H00 + i*8 + j);
		    WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val1[j]);
			/*pr_info("0x%x,\n", val1);*/
		}
	}
}

/**
 * [cm2_luma adj cm2 Hue offset for each four pieces Saturation region]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_hue_by_hs(enum eCM2ColorMd colormode, int hue_val, int lpf_en)
{
	int inp_color = colormode;
	/*[-100, 100], color_adj will mapping to value [-128, 127]*/
	int inp_val = hue_val;
	int temp;
	int out_lut[32];
	char reg_CM2_Adj_Hue_via_S[5][32];
	/*int lpf_en = 0;*/
	int k, i;

	memset(out_lut, 0, sizeof(int)*32);
	memset(reg_CM2_Adj_Hue_via_S, 0, sizeof(char)*32*5);
	/*pr_info("color mode:%d, input val =%d\n", colormode, hue_val);*/

	color_adj(inp_color, inp_val, lpf_en, lpf_coef,
				color_key_pts, smth_coef_hue, out_lut);

	for (k = 0; k < 5; k++) {
		/*pr_info("\n Adj_Hue via %d\n", k);*/
		for (i = 0; i < 32; i++) {
			temp = out_lut[i] * huegain_via_sat5[inp_color][k];
			reg_CM2_Adj_Hue_via_S[k][i] = (char)(rsround(temp)/100);
			/*pr_info("%d  ", reg_CM2_Adj_Hue_via_S[k][i]);*/
		}
	}
	cm2_curve_update_hue_by_hs(colormode, reg_CM2_Adj_Hue_via_S);
	/*pr_info("\n ---end\n");*/
}

/**
 * [cm2_luma adj cm2 Hue offset for each four pieces Saturation region]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */

void cm2_hue(enum eCM2ColorMd colormode, int hue_val, int lpf_en)
{
	int inp_color = colormode;
	/*[-100, 100], color_adj will mapping to value [-128, 127]*/
	int inp_val = hue_val;
	int i;
	int out_lut[32];
	char reg_CM2_Adj_Hue_via_Hue[32];
	/*int lpf_en = 0;*/

	memset(out_lut, 0, sizeof(int)*32);
	memset(reg_CM2_Adj_Hue_via_Hue, 0, sizeof(char)*32);
	/*pr_info("color mode:%d, input val =%d\n", colormode, hue_val);*/

	color_adj(inp_color, inp_val, lpf_en, lpf_coef,
				color_key_pts, smth_coef_hue, out_lut);

	for (i = 0; i < 32; i++) {
		reg_CM2_Adj_Hue_via_Hue[i] = (char)out_lut[i];
		/*pr_info("%d  ", reg_CM2_Adj_Hue_via_S[k][i]);*/
	}
	cm2_curve_update_hue(colormode, reg_CM2_Adj_Hue_via_Hue);
	/*pr_info("\n ---end\n");*/
}

/**
 * [cm2_luma adj cm2 Luma offsets for Hue section]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_luma(enum eCM2ColorMd colormode, int luma_val, int lpf_en)
{
	char reg_CM2_Adj_Luma_via_Hue[32];
	int out_luma_lut[32];
	int i;
	int inp_color = colormode;
	int inp_val = luma_val;

	/*pr_info("colormode:%d, input val %d\n",colormode, luma_val);*/
	memset(reg_CM2_Adj_Luma_via_Hue, 0, sizeof(char)*32);
	memset(out_luma_lut, 0, sizeof(int)*32);

	color_adj(inp_color, inp_val, lpf_en, lpf_coef, color_key_pts,
				smth_coef_luma, out_luma_lut);

	for (i = 0; i < 32; i++) {
		reg_CM2_Adj_Luma_via_Hue[i] = (char)out_luma_lut[i];
		/*pr_info("%d,", out_luma_lut[i]);*/
	}

	cm2_curve_update_luma(colormode, reg_CM2_Adj_Luma_via_Hue);
	/*pr_info("\n---end\n");*/
}

/**
 * [cm2_sat adj cm2 saturation gain offset]
 * @param colormode [enum eCM2ColorMd]
 * @param sat_val   [-100 ~ 100]
 * @param lpf_en    [1:on 0:off]
 */
void cm2_sat(enum eCM2ColorMd colormode, int sat_val, int lpf_en)
{
	int inp_color = colormode;
	int inp_val = sat_val;

	char reg_CM2_Adj_Sat_via_HS[3][32];
	int out_sat_lut[32];
	int k, i;
	int temp;

	/*pr_info("colormode:%d, input val %d\n",colormode, sat_val);*/
	memset(reg_CM2_Adj_Sat_via_HS, 0, sizeof(char)*32*3);
	memset(out_sat_lut, 0, sizeof(int)*32);

	color_adj(inp_color, inp_val, lpf_en, lpf_coef, color_key_pts,
		smth_coef_sat, out_sat_lut);

	for (k = 0; k < 3; k++) {
		/*pr_info("\n Adj_sat %d\n", k);*/
		for (i = 0; i < 32; i++) {
			temp = out_sat_lut[i] * satgain_via_sat3[inp_color][k];
			reg_CM2_Adj_Sat_via_HS[k][i] =
				(char)(rsround(temp)/100);
			/*pr_info("%d  ", reg_CM2_Adj_Sat_via_HS[k][i]);*/
		}
	}
	cm2_curve_update_sat(colormode, reg_CM2_Adj_Sat_via_HS);
	/*pr_info("\n---end\n");*/
}


