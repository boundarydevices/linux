/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_ctl.c
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

#include <linux/module.h>
/* #include <mach/am_regs.h> */
/* #include <mach/register.h> */
/* #include <mach/cpu.h> */
/* #include <linux/amlogic/media/canvas/canvas.h> */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/delay.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
/* #include "../../../../common/drivers/amlogic/amports/ve_regs.h" */

/* #define VDIN_MEAS_24M_1MS 24000 */
/* #define VDIN_MEAS_51M_1MS 50000 */

#define TVIN_MAX_PIXCLK 20000

static short max_hactive = 4096;
module_param(max_hactive, short, 0664);
MODULE_PARM_DESC(max_hactive, "the max hactive of vdin");

/*
 * protection for vga vertical de adjustment,if vertical blanking too short
 * mybe too short to process one field data
 */
static short vbp_offset = 15;
module_param(vbp_offset, short, 0664);
MODULE_PARM_DESC(vbp_offset, "the mix lines after vsync");

/* black bar det enable/disable test */
static bool black_bar_enable;
module_param(black_bar_enable, bool, 0664);
MODULE_PARM_DESC(black_bar_enable, "black bar enable/disable");

static bool hist_bar_enable;
module_param(hist_bar_enable, bool, 0664);
MODULE_PARM_DESC(hist_bar_enable, "hist bar enable/disable");

static int color_convert;
module_param(color_convert, int, 0664);
MODULE_PARM_DESC(color_convert, "color_convert");

static unsigned int max_undone_cnt = 60;
module_param(max_undone_cnt, uint, 0644);
MODULE_PARM_DESC(max_undone_cnt, "the max vdin undone cnt to reset vpp");

static unsigned int use_frame_rate;
module_param(use_frame_rate, uint, 0644);
MODULE_PARM_DESC(use_frame_rate, "use frame rate to cal duraton");

static bool cm_enable = 1;
module_param(cm_enable, bool, 0644);
MODULE_PARM_DESC(cm_enable, "cm_enable");

static bool rgb_info_enable;
module_param(rgb_info_enable, bool, 0644);
MODULE_PARM_DESC(rgb_info_enable, "rgb_info_enable");

static unsigned int rgb_info_x;
module_param(rgb_info_x, uint, 0644);
MODULE_PARM_DESC(rgb_info_x, "rgb_info_x");

static unsigned int rgb_info_y;
module_param(rgb_info_y, uint, 0644);
MODULE_PARM_DESC(rgb_info_y, "rgb_info_y");

static unsigned int rgb_info_r;
module_param(rgb_info_r, uint, 0644);
MODULE_PARM_DESC(rgb_info_r, "rgb_info_r");

static unsigned int rgb_info_g;
module_param(rgb_info_g, uint, 0644);
MODULE_PARM_DESC(rgb_info_g, "rgb_info_g");

static unsigned int rgb_info_b;
module_param(rgb_info_b, uint, 0644);
MODULE_PARM_DESC(rgb_info_b, "rgb_info_b");

static unsigned int vdin_ldim_max_en = 1;
module_param(vdin_ldim_max_en, uint, 0644);
MODULE_PARM_DESC(vdin_ldim_max_en, "vdin_ldim_max_en");

/*0: 1 word in 1burst, 1: 2 words in 1burst;
 * 2: 4 words in 1burst;
 */
static unsigned int vdin_wr_burst_mode = 2;
module_param(vdin_wr_burst_mode, uint, 0644);
MODULE_PARM_DESC(vdin_wr_burst_mode, "vdin_wr_burst_mode");

static unsigned int delay_line_num;
module_param(delay_line_num, uint, 0644);
MODULE_PARM_DESC(delay_line_num, "delay_line_num");

bool enable_reset;
module_param(enable_reset, bool, 0664);
MODULE_PARM_DESC(enable_reset, "enable_reset");
static int vsync_reset_mask;
module_param(vsync_reset_mask, int, 0664);
MODULE_PARM_DESC(vsync_reset_mask, "vsync_reset_mask");

static int vdin_det_idle_wait = 100;
module_param(vdin_det_idle_wait, int, 0664);
MODULE_PARM_DESC(vdin_det_idle_wait, "vdin_det_idle_wait");

int try_count;
int try_count_max = 3;
module_param(try_count_max, int, 0664);
MODULE_PARM_DESC(try_count_max, "try_count_max");


unsigned int vpu_reg_27af = 0x3;

/***************************Local defines**********************************/
#define BBAR_BLOCK_THR_FACTOR           3
#define BBAR_LINE_THR_FACTOR            7

#define VDIN_MUX_NULL                   0
#define VDIN_MUX_MPEG                   1
#define VDIN_MUX_656                    2
#define VDIN_MUX_TVFE                   3
#define VDIN_MUX_CVD2                   4
#define VDIN_MUX_HDMI                   5
#define VDIN_MUX_DVIN                   6

#define VDIN_MUX_VIU                    7
#define VDIN_MUX_MIPI                   8
#define VDIN_MUX_ISP					9
#define VDIN_MUX_656_B                  10

#define VDIN_MAP_Y_G                    0
#define VDIN_MAP_BPB                    1
#define VDIN_MAP_RCR                    2

#define MEAS_MUX_NULL                   0
#define MEAS_MUX_656                    1
#define MEAS_MUX_TVFE                   2
#define MEAS_MUX_CVD2                   3
#define MEAS_MUX_HDMI                   4
#define MEAS_MUX_DVIN                   5
#define MEAS_MUX_DTV                    6
#define MEAS_MUX_ISP                    8
#define MEAS_MUX_656_B                  9

#define MEAS_MUX_VIU                    6

/* check hcnt/vcnt after N*vs. */
#define VDIN_WAIT_VALID_VS      2
/* ignore n*vs which have wrong data. */
#define VDIN_IGNORE_VS_CNT      20
/* the diff value between normal/bad data */
#define VDIN_MEAS_HSCNT_DIFF    0x50
/* the diff value between normal/bad data */
#define VDIN_MEAS_VSCNT_DIFF    0x50

#ifndef VDIN_DEBUG
#undef pr_info
#define pr_info(fmt, ...)
#endif

static void vdin0_wr_mif_reset(void)
{
	if (vsync_reset_mask & 0x08) {
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, FRAME_SOFT_RST_EN_BIT, 1);
		W_VCBUS_BIT(VDIN_COM_CTRL0, 1, 28, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, FRAME_SOFT_RST_EN_BIT, 1);
	} else {
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, 10, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, 2, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_RST_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_MIF_RST_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, 2, 1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, 8, 1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, 10, 1);
	}
};

static void vdin1_wr_mif_reset(void)
{
	if (vsync_reset_mask & 0x08) {
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, FRAME_SOFT_RST_EN_BIT, 1);
		W_VCBUS_BIT(VDIN_COM_CTRL1, 1, 28, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, FRAME_SOFT_RST_EN_BIT, 1);
	} else {
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, 10, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_MIF_RST_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN1_MIF_RST_BIT, 1);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, 8, 1);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, 10, 1);
	}
}

/***************************Local Structures**********************************/
static struct vdin_matrix_lup_s vdin_matrix_lup[] = {
	{0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x0400200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV601 */
	/* 0     0.257  0.504  0.098     16 */
	/* 0    -0.148 -0.291  0.439    128 */
	/* 0     0.439 -0.368 -0.071    128 */
	{0x00000000, 0x00000000, 0x01070204, 0x00641f68, 0x1ed601c2, 0x01c21e87,
		0x00001fb7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_GBR_YUV601 */
	/* 0	    0.504  0.098  0.257     16 */
	/* 0      -0.291  0.439 -0.148    128 */
	/* 0	   -0.368 -0.071  0.439    128 */
	{0x00000000, 0x00000000, 0x02040064, 0x01071ed6, 0x01c21f68, 0x1e871fb7,
		0x000001c2, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_BRG_YUV601 */
	/* 0	    0.098  0.257  0.504     16 */
	/* 0       0.439 -0.148 -0.291    128 */
	/* 0      -0.071  0.439 -0.368    128 */
	{0x00000000, 0x00000000, 0x00640107, 0x020401c2, 0x1f681ed6, 0x1fb701c2,
		0x00001e87, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGB */
	/* -16     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_GBR */
	/* -16     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0	     0 */
	/* -128     1.164  0	  1.596      0 */
	{0x07c00600, 0x00000600, 0x04a81e70, 0x1cbf04a8, 0x08120000, 0x04a80000,
		0x00000662, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_BRG */
	/* -16     1.164  2.018  0          0 */
	/* -128     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	{0x07c00600, 0x00000600, 0x04a80812, 0x000004a8, 0x00000662, 0x04a81e70,
		0x00001cbf, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV601F */
	/* 0     0.299  0.587  0.114      0 */
	/* 0    -0.169 -0.331  0.5      128 */
	/* 0     0.5   -0.419 -0.081    128 */
	{0x00000000, 0x00000000, 0x01320259, 0x00751f53, 0x1ead0200, 0x02001e53,
		0x00001fad, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGB */
	/* 0     1      0      1.402      0 */
	/* -128     1     -0.344 -0.714      0 */
	/* -128     1      1.772  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x059c0400, 0x1ea01d25, 0x04000717,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV601 */
	/* -16     0.299  0.587  0.114     16 */
	/* -16    -0.173 -0.339  0.511    128 */
	/* -16     0.511 -0.429 -0.083    128 */
	{0x07c007c0, 0x000007c0, 0x01320259, 0x00751f4f, 0x1ea5020b, 0x020b1e49,
		0x00001fab, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGBS */
	/* -16     1      0      1.371     16 */
	/* -128     1     -0.336 -0.698     16 */
	/* -128     1      1.733  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x057c0400, 0x1ea81d35, 0x040006ef,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV601F */
	/* -16     0.348  0.683  0.133      0 */
	/* -16    -0.197 -0.385  0.582    128 */
	/* -16     0.582 -0.488 -0.094    128 */
	{0x07c007c0, 0x000007c0, 0x016402bb, 0x00881f36, 0x1e760254, 0x02541e0c,
		0x00001fa0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGBS */
	/* 0     0.859  0      1.204     16 */
	/* -128     0.859 -0.295 -0.613     16 */
	/* -128     0.859  1.522  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x04d10370, 0x1ed21d8c, 0x03700617,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV601F_YUV601 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV601F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV709 */
	/* 0     0.183  0.614  0.062     16 */
	/* 0    -0.101 -0.338  0.439    128 */
	/* 0     0.439 -0.399 -0.04     128 */
	{0x00000000, 0x00000000, 0x00bb0275, 0x003f1f99, 0x1ea601c2, 0x01c21e67,
		0x00001fd7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGB */
	/* -16     1.164  0      1.793      0 */
	/* -128     1.164 -0.213 -0.534      0 */
	/* -128     1.164  2.115  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x072c04a8, 0x1f261ddd, 0x04a80876,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_GBR */
	/* -16	1.164 -0.213 -0.534	 0 */
	/* -128	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	{0x07c00600, 0x00000600, 0x04a81f26, 0x1ddd04a8, 0x08760000, 0x04a80000,
		0x0000072c, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_BRG */
	/* -16	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	/* -128	1.164 -0.213 -0.534	 0 */
	{0x07c00600, 0x00000600, 0x04a80876, 0x000004a8, 0x0000072c, 0x04a81f26,
		0x00001ddd, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV709F */
	/* 0     0.213  0.715  0.072      0 */
	/* 0    -0.115 -0.385  0.5      128 */
	/* 0     0.5   -0.454 -0.046    128 */
	{0x00000000, 0x00000000, 0x00da02dc, 0x004a1f8a, 0x1e760200, 0x02001e2f,
		0x00001fd1, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGB */
	/* 0     1      0      1.575      0 */
	/* -128     1     -0.187 -0.468      0 */
	/* -128     1      1.856  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x064d0400, 0x1f411e21, 0x0400076d,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV709 */
	/* -16     0.213  0.715  0.072     16 */
	/* -16    -0.118 -0.394  0.511    128 */
	/* -16     0.511 -0.464 -0.047    128 */
	{0x07c007c0, 0x000007c0, 0x00da02dc, 0x004a1f87, 0x1e6d020b, 0x020b1e25,
		0x00001fd0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGBS */
	/* -16     1      0      1.54      16 */
	/* -128     1     -0.183 -0.459     16 */
	/* -128     1      1.816  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x06290400, 0x1f451e2a, 0x04000744,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV709F */
	/* -16     0.248  0.833  0.084      0 */
	/* -16    -0.134 -0.448  0.582    128 */
	/* -16     0.582 -0.529 -0.054    128 */
	{0x07c007c0, 0x000007c0, 0x00fe0355, 0x00561f77, 0x1e350254, 0x02541de2,
		0x00001fc9, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGBS */
	/* 0     0.859  0      1.353     16 */
	/* -128     0.859 -0.161 -0.402     16 */
	/* -128     0.859  1.594  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x05690370, 0x1f5b1e64, 0x03700660,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV709F_YUV709 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV709F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709 */
	/* -16     1     -0.115 -0.207     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x07c00600, 0x00000600, 0x04001f8a, 0x1f2c0000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601 */
	/* -16     1      0.100  0.192     16 */
	/* -128     0      0.990 -0.110    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x07c00600, 0x00000600, 0x04000066, 0x00c50000, 0x03f61f8f, 0x00001fb6,
		0x000003f0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709F */
	/* -16     1.164 -0.134 -0.241      0 */
	/* -128     0      1.160  0.129    128 */
	/* -128     0      0.085  1.167    128 */
	{0x07c00600, 0x00000600, 0x04a81f77, 0x1f090000, 0x04a40084, 0x00000057,
		0x000004ab, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601 */
	/* 0     0.859  0.088  0.169     16 */
	/* -128     0      0.869 -0.097    128 */
	/* -128     0     -0.063  0.864    128 */
	{0x00000600, 0x00000600, 0x0370005a, 0x00ad0000, 0x037a1f9d, 0x00001fbf,
		0x00000375, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709 */
	/* 0     0.859 -0.101 -0.182     16 */
	/* -128     0      0.894  0.100    128 */
	/* -128     0      0.066  0.900    128 */
	{0x00000600, 0x00000600, 0x03701f99, 0x1f460000, 0x03930066, 0x00000044,
		0x0000039a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601F */
	/* -16     1.164  0.116  0.223      0 */
	/* -128     0      1.128 -0.126    128 */
	/* -128     0     -0.082  1.120    128 */
	{0x07c00600, 0x00000600, 0x04a80077, 0x00e40000, 0x04831f7f, 0x00001fac,
		0x0000047b, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709F */
	/* 0     1     -0.118 -0.212     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x00000600, 0x00000600, 0x04001f87, 0x1f270000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601F */
	/* 0     1      0.102  0.196      0 */
	/* -128     0      0.990 -0.111    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x00000600, 0x00000600, 0x04000068, 0x00c90000, 0x03f61f8e, 0x00001fb6,
		0x000003f0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGBS_RGB */
	/* -16     1.164  0      0          0 */
	/* -16     0      1.164  0          0 */
	/* -16     0      0      1.164      0 */
	{0x07c007c0, 0x000007c0, 0x04a80000, 0x00000000, 0x04a80000, 0x00000000,
		0x000004a8, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_RGBS */
	/* 0     0.859  0      0         16 */
	/* 0     0      0.859  0         16 */
	/* 0     0      0      0.859     16 */
	{0x00000000, 0x00000000, 0x03700000, 0x00000000, 0x03700000, 0x00000000,
		0x00000370, 0x00400040, 0x00000040,},
};

/***************************Local function**********************************/

void vdin_get_format_convert(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e	format_convert;

	if (devp->prop.color_format == devp->prop.dest_cfmt) {
		switch (devp->prop.color_format) {
		case TVIN_YUV422:
		case TVIN_YUYV422:
		case TVIN_YVYU422:
		case TVIN_UYVY422:
		case TVIN_VYUY422:
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;
			break;
		case TVIN_YUV444:
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
			break;
		case TVIN_RGB444:
			format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
			break;
		default:
			format_convert = VDIN_FORMAT_CONVERT_MAX;
			break;
		}
	} else {
		switch (devp->prop.color_format) {
		case TVIN_YUV422:
		case TVIN_YUV444:
		case TVIN_YUYV422:
		case TVIN_YVYU422:
		case TVIN_UYVY422:
		case TVIN_VYUY422:
			if (devp->prop.dest_cfmt == TVIN_NV21)
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV21;
			else if (devp->prop.dest_cfmt == TVIN_NV12)
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV12;
			else
				format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;
			break;
		case TVIN_RGB444:
			if (devp->prop.dest_cfmt == TVIN_YUV444)
				format_convert = VDIN_FORMAT_CONVERT_RGB_YUV444;
			else if (devp->prop.dest_cfmt == TVIN_NV21)
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV21;
			else if (devp->prop.dest_cfmt == TVIN_NV12)
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV12;
			else
				format_convert = VDIN_FORMAT_CONVERT_RGB_YUV422;
			break;
		default:
			format_convert = VDIN_FORMAT_CONVERT_MAX;
		break;
		}
	}
	devp->format_convert = format_convert;
}

enum vdin_format_convert_e vdin_get_format_convert_matrix0(
		struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert = VDIN_FORMAT_CONVERT_MAX;

	if (devp->prop.dest_cfmt == TVIN_NV21)
		format_convert = VDIN_FORMAT_CONVERT_RGB_NV21;
	else if (devp->prop.dest_cfmt == TVIN_NV12)
		format_convert = VDIN_FORMAT_CONVERT_RGB_NV12;
	else if (devp->prop.dest_cfmt == TVIN_RGB444)
		format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
	else if (devp->prop.dest_cfmt == TVIN_YUV444)
		format_convert = VDIN_FORMAT_CONVERT_RGB_YUV444;
	else
		format_convert = VDIN_FORMAT_CONVERT_RGB_YUV422;
	return format_convert;
}

enum vdin_format_convert_e vdin_get_format_convert_matrix1(
		struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert = VDIN_FORMAT_CONVERT_MAX;

	switch (devp->prop.color_format) {
	case TVIN_YUV422:
	case TVIN_YUYV422:
	case TVIN_YVYU422:
	case TVIN_UYVY422:
	case TVIN_VYUY422:
	case TVIN_YUV444:
		format_convert = VDIN_FORMAT_CONVERT_YUV_RGB;
	break;
	case TVIN_RGB444:
		format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
		break;
	default:
		format_convert = VDIN_FORMAT_CONVERT_MAX;
	break;
	}
	return format_convert;
}

void vdin_get_prob_rgb(unsigned int offset,
		unsigned int *r, unsigned int *g, unsigned int *b)
{
	*b = rgb_info_b = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR, 0, 10)
			<< 8) >> 10);
	*g = rgb_info_g = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR, 10, 10)
			<< 8) >> 10);
	*r =  rgb_info_r = ((rd_bits(offset, VDIN_MATRIX_PROBE_COLOR, 20, 10)
			<< 8) >> 10);
}

static void vdin_set_meas_mux(unsigned int offset, enum tvin_port_e port_,
		enum bt_path_e bt_path)
{
	/* unsigned int offset = devp->addr_offset; */
	unsigned int meas_mux = MEAS_MUX_NULL;

	switch ((port_)>>8) {
	case 0x01: /* mpeg */
		meas_mux = MEAS_MUX_NULL;
		break;
	case 0x02: /* 656 */
		if (is_meson_gxbb_cpu() && (bt_path == BT_PATH_GPIO_B))
		meas_mux = MEAS_MUX_656_B;
		else
			meas_mux = MEAS_MUX_656;
		break;
	case 0x04: /* VGA */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x08: /* COMPONENT */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x10: /* CVBS */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x20: /* SVIDEO */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x40: /* hdmi */
		meas_mux = MEAS_MUX_HDMI;
		break;
	case 0x80: /* dvin */
		meas_mux = MEAS_MUX_DVIN;
		break;
	case 0xc0:/* viu */
		meas_mux = MEAS_MUX_VIU;
		break;
	case 0x100:/* dtv mipi */
		meas_mux = MEAS_MUX_DTV;
		break;
	case 0x200:/* isp */
		meas_mux = MEAS_MUX_ISP;
		break;
	default:
		meas_mux = MEAS_MUX_NULL;
		break;
	}
	/* set VDIN_MEAS in accumulation mode */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1,
			MEAS_VS_TOTAL_CNT_EN_BIT, MEAS_VS_TOTAL_CNT_EN_WID);
	/* set VPP_VDO_MEAS in accumulation mode */
	wr_bits(0, VPP_VDO_MEAS_CTRL, 1, 8, 1);
	/* set VPP_MEAS in latch-on-falling-edge mode */
	wr_bits(0, VPP_VDO_MEAS_CTRL, 1, 9, 1);
	/* set VDIN_MEAS mux */
	wr_bits(offset, VDIN_MEAS_CTRL0, meas_mux,
			MEAS_HS_VS_SEL_BIT, MEAS_HS_VS_SEL_WID);
	/* manual reset VDIN_MEAS & VPP_VDO_MEAS at the same time,
	 * rst = 1 & 0
	 */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1, MEAS_RST_BIT, MEAS_RST_WID);
	W_VCBUS_BIT(VPP_VDO_MEAS_CTRL, 1, 10, 1);
	wr_bits(offset, VDIN_MEAS_CTRL0, 0, MEAS_RST_BIT, MEAS_RST_WID);
	W_VCBUS_BIT(VPP_VDO_MEAS_CTRL, 0, 10, 1);
}


static inline void vdin_set_top(unsigned int offset,
		enum tvin_port_e port,
		enum tvin_color_fmt_e input_cfmt, unsigned int h,
		enum bt_path_e bt_path)
{
	/* unsigned int offset = devp->addr_offset; */
	unsigned int vdin_mux = VDIN_MUX_NULL;
	unsigned int vdin_data_bus_0 = VDIN_MAP_Y_G;
	unsigned int vdin_data_bus_1 = VDIN_MAP_BPB;
	unsigned int vdin_data_bus_2 = VDIN_MAP_RCR;

	/* [28:16]         top.input_width_m1   = h-1 */
	/* [12: 0]         top.output_width_m1  = h-1 */
	wr(offset, VDIN_WIDTHM1I_WIDTHM1O, ((h-1)<<16)|(h-1));
	switch ((port)>>8) {
	case 0x01: /* mpeg */
		vdin_mux = VDIN_MUX_MPEG;
		wr_bits(offset, VDIN_ASFIFO_CTRL0, 0xe0,
				VDI1_ASFIFO_CTRL_BIT, VDI1_ASFIFO_CTRL_WID);
		break;
	case 0x02: /* bt656 */
		if (is_meson_gxbb_cpu() && (bt_path == BT_PATH_GPIO_B)) {
		vdin_mux = VDIN_MUX_656_B;
		wr_bits(offset, VDIN_ASFIFO_CTRL3, 0xe4,
				VDI9_ASFIFO_CTRL_BIT, VDI9_ASFIFO_CTRL_WID);
		} else {
		vdin_mux = VDIN_MUX_656;
		wr_bits(offset, VDIN_ASFIFO_CTRL0, 0xe4,
				VDI1_ASFIFO_CTRL_BIT, VDI1_ASFIFO_CTRL_WID);
		}
		break;
	case 0x04: /* VGA */
		vdin_mux = VDIN_MUX_TVFE;
		wr_bits(offset, VDIN_ASFIFO_CTRL0, 0xe4,
				VDI2_ASFIFO_CTRL_BIT, VDI2_ASFIFO_CTRL_WID);
		/* In the order of RGB for further
		 * RGB->YUV601 or RGB->YUV709 conversion
		 */
		vdin_data_bus_0 = VDIN_MAP_RCR;
		vdin_data_bus_1 = VDIN_MAP_Y_G;
		vdin_data_bus_2 = VDIN_MAP_BPB;
		break;
	case 0x08: /* COMPONENT */
		vdin_mux = VDIN_MUX_TVFE;
		wr_bits(offset, VDIN_ASFIFO_CTRL0, 0xe4,
				VDI2_ASFIFO_CTRL_BIT, VDI2_ASFIFO_CTRL_WID);
		break;
	case 0x10: /* CVBS */
		vdin_mux = VDIN_MUX_CVD2;
		wr_bits(offset, VDIN_ASFIFO_CTRL1, 0xe4,
				VDI3_ASFIFO_CTRL_BIT, VDI3_ASFIFO_CTRL_WID);
		break;
	case 0x20: /* SVIDEO */
		vdin_mux = VDIN_MUX_CVD2;
		wr_bits(offset, VDIN_ASFIFO_CTRL1, 0xe4,
				VDI3_ASFIFO_CTRL_BIT, VDI3_ASFIFO_CTRL_WID);
		break;
	case 0x40: /* hdmi */
		vdin_mux = VDIN_MUX_HDMI;
		wr_bits(offset, VDIN_ASFIFO_CTRL1, 0xe4,
				VDI4_ASFIFO_CTRL_BIT, VDI4_ASFIFO_CTRL_WID);
		break;
	case 0x80: /* dvin */
		vdin_mux = VDIN_MUX_DVIN;
		wr_bits(offset, VDIN_ASFIFO_CTRL2, 0xe4,
				VDI5_ASFIFO_CTRL_BIT, VDI5_ASFIFO_CTRL_WID);
		break;
	case 0xa0:
	case 0xc0: /* viu */
		vdin_mux = VDIN_MUX_VIU;
		if (port == TVIN_PORT_VIDEO)
			wr_bits(offset, VDIN_ASFIFO_CTRL3, 0xe4,
				VDI6_ASFIFO_CTRL_BIT, VDI6_ASFIFO_CTRL_WID);
		else
			wr_bits(offset, VDIN_ASFIFO_CTRL3, 0xf4,
				VDI6_ASFIFO_CTRL_BIT, VDI6_ASFIFO_CTRL_WID);
		break;
	case 0x100:/* mipi in mybe need modify base on truth */
		vdin_mux = VDIN_MUX_MIPI;
		wr_bits(offset, VDIN_ASFIFO_CTRL3, 0xe0,
				VDI7_ASFIFO_CTRL_BIT, VDI7_ASFIFO_CTRL_WID);
		break;
	case 0x200:
		vdin_mux = VDIN_MUX_ISP;
		wr_bits(offset, VDIN_ASFIFO_CTRL3, 0xe4,
				VDI8_ASFIFO_CTRL_BIT, VDI8_ASFIFO_CTRL_WID);
		break;
	default:
		vdin_mux = VDIN_MUX_NULL;
		break;
	}
	switch (input_cfmt) {
	case TVIN_YVYU422:
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_BPB;
		break;
	case TVIN_UYVY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	case TVIN_VYUY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	default:
		break;
	}
	wr_bits(offset, VDIN_COM_CTRL0, vdin_mux, VDIN_SEL_BIT, VDIN_SEL_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_0,
			COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_1,
			COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_2,
			COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
}

/*
 *  this function will set the bellow parameters of devp:
 *  1.h_active
 *  2.v_active
 */
#define HDMI_DE_REPEAT_DONE_FLAG	0xF0
#define DECIMATION_REAL_RANGE		0x0F
void vdin_set_decimation(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int new_clk = 0;
	bool decimation_in_frontend = false;

	if (devp->prop.decimation_ratio & HDMI_DE_REPEAT_DONE_FLAG) {
		decimation_in_frontend = true;
		pr_info("decimation_in_frontend\n");
	}
	devp->prop.decimation_ratio = devp->prop.decimation_ratio &
			DECIMATION_REAL_RANGE;

	new_clk = devp->fmt_info_p->pixel_clk /
			(devp->prop.decimation_ratio + 1);
	pr_info("%s decimation_ratio=%u,new_clk=%u.\n",
		__func__, devp->prop.decimation_ratio, new_clk);

	devp->h_active = devp->fmt_info_p->h_active /
			(devp->prop.decimation_ratio + 1);
	devp->v_active = devp->fmt_info_p->v_active;

	if ((devp->prop.decimation_ratio) && (!decimation_in_frontend))	{
		/* ratio */
		wr_bits(offset, VDIN_ASFIFO_CTRL2,
			devp->prop.decimation_ratio,
			ASFIFO_DECIMATION_NUM_BIT, ASFIFO_DECIMATION_NUM_WID);
		/* en */
		wr_bits(offset, VDIN_ASFIFO_CTRL2, 1,
			ASFIFO_DECIMATION_DE_EN_BIT,
			ASFIFO_DECIMATION_DE_EN_WID);
		/* manual reset, rst = 1 & 0 */
		wr_bits(offset, VDIN_ASFIFO_CTRL2, 1,
				ASFIFO_DECIMATION_SYNC_WITH_DE_BIT,
				ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
		wr_bits(offset, VDIN_ASFIFO_CTRL2, 0,
				ASFIFO_DECIMATION_SYNC_WITH_DE_BIT,
				ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
	}

	/* output_width_m1 */
	wr_bits(offset, VDIN_INTF_WIDTHM1, (devp->h_active - 1),
			VDIN_INTF_WIDTHM1_BIT, VDIN_INTF_WIDTHM1_WID);
}

void vdin_fix_nonstd_vsync(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	wr_bits(offset, VDIN_INTF_WIDTHM1, 3, 24, 2);
}

/*
 *  this function will set the bellow parameters of devp:
 *  1.h_active
 *  2.v_active
 */
void vdin_set_cutwin(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int he = 0, ve = 0;

	if (((devp->prop.hs) || (devp->prop.he) ||
		(devp->prop.vs) || (devp->prop.ve)) &&
		(devp->h_active > (devp->prop.hs + devp->prop.he)) &&
		(devp->v_active > (devp->prop.vs + devp->prop.ve))
	   ) {
		devp->h_active -= (devp->prop.he + devp->prop.hs);
		devp->v_active -= (devp->prop.ve + devp->prop.vs);
		he = devp->prop.hs + devp->h_active - 1;
		ve = devp->prop.vs + devp->v_active - 1;

		wr(offset, VDIN_WIN_H_START_END,
				(devp->prop.hs << INPUT_WIN_H_START_BIT) |
				(he << INPUT_WIN_H_END_BIT));
		wr(offset, VDIN_WIN_V_START_END,
				(devp->prop.vs << INPUT_WIN_V_START_BIT) |
				(ve << INPUT_WIN_V_END_BIT));
		wr_bits(offset, VDIN_COM_CTRL0, 1,
				INPUT_WIN_SEL_EN_BIT, INPUT_WIN_SEL_EN_WID);
		pr_info("%s enable cutwin hs=%d, he=%d,  vs=%d, ve=%d\n",
			__func__,
			devp->prop.hs, devp->prop.he,
			devp->prop.vs, devp->prop.ve);
	} else {
		pr_info("%s disable cutwin!!! hs=%d, he=%d,  vs=%d, ve=%d\n",
			__func__,
			devp->prop.hs, devp->prop.he,
			devp->prop.vs, devp->prop.ve);
	}

}

void vdin_set_config(struct vdin_dev_s *devp)
{
	if (is_meson_gxbb_cpu() || is_meson_gxm_cpu() || is_meson_gxl_cpu())
		/* max pixel clk of vdin for gxbb/gxm/gxl */
		devp->vdin_max_pixelclk = 248832000; /* 2160p30hz*/
	else
		devp->vdin_max_pixelclk = 497664000; /* 2160p60hz*/
}


static inline void vdin_set_color_matrix1(unsigned int offset,
		struct tvin_format_s *tvin_fmt_p,
		enum vdin_format_convert_e format_convert,
		enum tvin_port_e port,
		enum tvin_color_fmt_range_e color_fmt_range,
		unsigned int vdin_hdr_flag)
{
	/* unsigned int offset = devp->addr_offset; */
	enum vdin_matrix_csc_e    matrix_csc = VDIN_MATRIX_NULL;
	struct vdin_matrix_lup_s *matrix_tbl;
	struct tvin_format_s *fmt_info = tvin_fmt_p;

	switch (format_convert)	{
	case VDIN_MATRIX_XXX_YUV_BLACK:
		matrix_csc = VDIN_MATRIX_XXX_YUV601_BLACK;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		if ((port >= TVIN_PORT_HDMI0) &&
			(port <= TVIN_PORT_HDMI7)) {
			if (color_fmt_range == TVIN_RGB_FULL) {
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGB_YUV709;
			} else {
				matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
			}
		} else
			matrix_csc = VDIN_MATRIX_RGB_YUV709F;
		break;
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		matrix_csc = VDIN_MATRIX_BRG_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
		matrix_csc = VDIN_MATRIX_GBR_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		if ((port >= TVIN_PORT_HDMI0) &&
				(port <= TVIN_PORT_HDMI7)) {
			if (color_fmt_range == TVIN_RGB_FULL) {
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGB_YUV709;
			} else {
				matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
			}
		} else
			matrix_csc = VDIN_MATRIX_RGB_YUV709F;
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
			((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_RGB;
		else
			matrix_csc = VDIN_MATRIX_YUV601_RGB;
		break;
	case VDIN_FORMAT_CONVERT_YUV_GBR:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_GBR;
		else
			matrix_csc = VDIN_MATRIX_YUV601_GBR;
		break;
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_BRG;
		else
			matrix_csc = VDIN_MATRIX_YUV601_BRG;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   ) {
			if (color_fmt_range != TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709_YUV709F;
		} else {
			if (color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV601F_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_YUV601_YUV709F;
		}
		if (vdin_hdr_flag == 1)
			matrix_csc = VDIN_MATRIX_NULL;

		break;
	default:
		matrix_csc = VDIN_MATRIX_NULL;
		break;
	}

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
				VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];
		/*select matrix1 post probe and postion(200,100)*/
		wr_bits(offset, VDIN_MATRIX_CTRL, 1,
				VDIN_PROBE_POST_BIT, VDIN_PROBE_POST_WID);
		wr_bits(offset, VDIN_MATRIX_CTRL, 1,
				VDIN_PROBE_SEL_BIT, VDIN_PROBE_SEL_WID);
		wr(offset, VDIN_MATRIX_PROBE_POS, 0xc812);
		/*coefficient index select matrix1*/
		wr_bits(offset, VDIN_MATRIX_CTRL, 1,
			VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);
		wr(offset,
			VDIN_MATRIX_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_MATRIX_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_MATRIX_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_MATRIX_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_MATRIX_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_MATRIX_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_MATRIX_COEF22, matrix_tbl->coef22);
		wr(offset, VDIN_MATRIX_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_MATRIX_OFFSET2, matrix_tbl->post_offset2);
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
			VDIN_MATRIX1_BYPASS_BIT, VDIN_MATRIX1_BYPASS_WID);
		wr_bits(offset, VDIN_MATRIX_CTRL, 1,
			VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
	}
}

static inline void vdin_set_color_matrix0(unsigned int offset,
		struct tvin_format_s *tvin_fmt_p,
		enum vdin_format_convert_e format_convert,
		enum tvin_port_e port,
		enum tvin_color_fmt_range_e color_fmt_range,
		unsigned int vdin_hdr_flag)
{
	enum vdin_matrix_csc_e    matrix_csc = VDIN_MATRIX_NULL;
	struct vdin_matrix_lup_s *matrix_tbl;
	struct tvin_format_s *fmt_info = tvin_fmt_p;

	switch (format_convert)	{
	case VDIN_MATRIX_XXX_YUV_BLACK:
		matrix_csc = VDIN_MATRIX_XXX_YUV601_BLACK;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		if ((port >= TVIN_PORT_HDMI0) &&
			(port <= TVIN_PORT_HDMI7)) {
			if (color_fmt_range == TVIN_RGB_FULL) {
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGB_YUV709;
			} else {
				matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
			}
		} else
			matrix_csc = VDIN_MATRIX_RGB_YUV709F;
		break;
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
		matrix_csc = VDIN_MATRIX_GBR_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		matrix_csc = VDIN_MATRIX_BRG_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		if ((port >= TVIN_PORT_HDMI0) &&
			(port <= TVIN_PORT_HDMI7)) {
			if (color_fmt_range == TVIN_RGB_FULL) {
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGB_YUV709;
			} else {
				matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
				if (vdin_hdr_flag == 1)
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
			}
		} else
			matrix_csc = VDIN_MATRIX_RGB_YUV709F;
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_RGB;
		else
			matrix_csc = VDIN_MATRIX_YUV601_RGB;
		break;
	case VDIN_FORMAT_CONVERT_YUV_GBR:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_GBR;
		else
			matrix_csc = VDIN_MATRIX_YUV601_GBR;
		break;
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   )
			matrix_csc = VDIN_MATRIX_YUV709_BRG;
		else
			matrix_csc = VDIN_MATRIX_YUV601_BRG;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		if (((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) &&
			(fmt_info->v_active >= 720)) || /* 720p & above */
		    ((fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED)  &&
			(fmt_info->v_active >= 540))    /* 1080i & above */
		   ) {
			if (color_fmt_range != TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709_YUV709F;
		} else {
			if (color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV601F_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_YUV601_YUV709F;
		}
		if (vdin_hdr_flag == 1)
			matrix_csc = VDIN_MATRIX_NULL;

		break;
	default:
		matrix_csc = VDIN_MATRIX_NULL;
		break;
	}

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
				VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
			VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset,
			VDIN_MATRIX_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset,
			VDIN_MATRIX_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_MATRIX_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_MATRIX_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_MATRIX_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_MATRIX_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_MATRIX_COEF22, matrix_tbl->coef22);
		wr(offset, VDIN_MATRIX_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_MATRIX_OFFSET2, matrix_tbl->post_offset2);
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
			VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr_bits(offset, VDIN_MATRIX_CTRL, 1,
			VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
	}
}
void vdin_set_matrix(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert_matrix0;
	enum vdin_format_convert_e format_convert_matrix1;
	unsigned int offset = devp->addr_offset;

	if (rgb_info_enable == 0) {
		/* vdin_set_color_matrix1(devp->addr_offset, devp->fmt_info_p,
		 * devp->format_convert);
		 * disable matrix1
		 */
		wr_bits(offset, VDIN_MATRIX_CTRL, 0,
				VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
		vdin_set_color_matrix0(devp->addr_offset, devp->fmt_info_p,
				devp->format_convert,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_Flag);
	} else {
		format_convert_matrix0 = vdin_get_format_convert_matrix0(devp);
		format_convert_matrix1 = vdin_get_format_convert_matrix1(devp);
		vdin_set_color_matrix1(devp->addr_offset, devp->fmt_info_p,
				format_convert_matrix1,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_Flag);
		vdin_set_color_matrix0(devp->addr_offset, devp->fmt_info_p,
				devp->format_convert,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_Flag);
		/* set xy */
		wr_bits(offset, VDIN_MATRIX_PROBE_POS, rgb_info_y, 0, 13);
		wr_bits(offset, VDIN_MATRIX_PROBE_POS, rgb_info_x, 16, 13);
		/* 1:probe pixel data after matrix */
		wr_bits(offset, VDIN_MATRIX_CTRL, 1, 6, 1);
		/* 1:select matrix 1 */
		wr_bits(offset, VDIN_MATRIX_CTRL, 1, 4, 2);
	}
}

void vdin_set_matrixs(struct vdin_dev_s *devp, unsigned char id,
		enum vdin_format_convert_e csc)
{
	switch (id) {
	case 0:
		vdin_set_color_matrix0(devp->addr_offset,
				devp->fmt_info_p, csc,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_Flag);
		break;
	case 1:
		vdin_set_color_matrix1(devp->addr_offset,
				devp->fmt_info_p, csc,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_Flag);
		break;
	default:
		break;
	}
}

void vdin_set_prob_xy(unsigned int offset,
		unsigned int x, unsigned int y, struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert_matrix0;
	enum vdin_format_convert_e format_convert_matrix1;

	/* set matrix */
	rgb_info_enable = 1;
	format_convert_matrix0 = vdin_get_format_convert_matrix0(devp);
	format_convert_matrix1 = vdin_get_format_convert_matrix1(devp);
	vdin_set_color_matrix1(devp->addr_offset, devp->fmt_info_p,
			format_convert_matrix1,
			devp->parm.port,
			devp->prop.color_fmt_range,
			devp->prop.vdin_hdr_Flag);
	vdin_set_color_matrix0(devp->addr_offset, devp->fmt_info_p,
			format_convert_matrix0,
			devp->parm.port,
			devp->prop.color_fmt_range,
			devp->prop.vdin_hdr_Flag);
	/* set position */
	rgb_info_x = x;
	if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		rgb_info_y = y/2;
	else
		rgb_info_y = y;
/* #if defined(VDIN_V1) */
	wr_bits(offset, VDIN_MATRIX_PROBE_POS, rgb_info_y, 0, 13);
	wr_bits(offset, VDIN_MATRIX_PROBE_POS, rgb_info_x, 16, 13);
	/* 1:probe pixel data after matrix */
	wr_bits(offset, VDIN_MATRIX_CTRL, 1, 6, 1);
	/* 1:select matrix 1 */
	wr_bits(offset, VDIN_MATRIX_CTRL, 1, 4, 2);
/* #endif */
}

void vdin_set_matrix_blank(struct vdin_dev_s *devp)
{
	vdin_set_color_matrix0(devp->addr_offset,
			devp->fmt_info_p,
			VDIN_MATRIX_XXX_YUV_BLACK,
			devp->parm.port,
			devp->prop.color_fmt_range,
			devp->prop.vdin_hdr_Flag);
}
static inline void vdin_set_bbar(unsigned int offset, unsigned int v,
		unsigned int h)
{
	unsigned int region_width = 1, block_thr = 0, line_thr = 0;

	while ((region_width<<1) < h)
		region_width <<= 1;

	block_thr = (region_width>>1) * v;
	/* bblk=(bpix>thr) */
	block_thr = block_thr - (block_thr >> BBAR_BLOCK_THR_FACTOR);
	 /* bln=!(wpix>=thr) */
	line_thr  = h >> BBAR_LINE_THR_FACTOR;

	/* region_width */
	wr_bits(offset, VDIN_BLKBAR_CTRL0,
			region_width, BLKBAR_H_WIDTH_BIT, BLKBAR_H_WIDTH_WID);
	/* win_he */
	wr_bits(offset, VDIN_BLKBAR_H_START_END,
			(h - 1), BLKBAR_HEND_BIT, BLKBAR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_BLKBAR_V_START_END,
			(v - 1), BLKBAR_VEND_BIT, BLKBAR_VEND_WID);
	/* bblk_thr_on_bpix */
	wr_bits(offset, VDIN_BLKBAR_CNT_THRESHOLD,
			block_thr, BLKBAR_CNT_TH_BIT, BLKBAR_CNT_TH_WID);
	/* blnt_thr_on_wpix */
	wr_bits(offset, VDIN_BLKBAR_ROW_TH1_TH2,
			line_thr, BLKBAR_ROW_TH1_BIT, BLKBAR_ROW_TH1_WID);
	/* blnb_thr_on_wpix */
	wr_bits(offset, VDIN_BLKBAR_ROW_TH1_TH2,
			line_thr, BLKBAR_ROW_TH2_BIT, BLKBAR_ROW_TH2_WID);
	/* en */
	wr_bits(offset, VDIN_BLKBAR_CTRL0,
			1, BLKBAR_DET_TOP_EN_BIT, BLKBAR_DET_TOP_EN_WID);
	/* manual reset, rst = 0 & 1, raising edge mode */
	wr_bits(offset, VDIN_BLKBAR_CTRL0, 0,
			BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
	wr_bits(offset, VDIN_BLKBAR_CTRL0, 1,
			BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
}

static inline void vdin_set_histogram(unsigned int offset, unsigned int hs,
		unsigned int he, unsigned int vs, unsigned int ve)
{
	unsigned int pixel_sum = 0, record_len = 0, hist_pow = 0;

	if ((hs < he) && (vs < ve)) {
		pixel_sum = (he - hs + 1) * (ve - vs + 1);
		record_len = 0xffff<<3;
		while ((pixel_sum > record_len) && (hist_pow < 3)) {
			hist_pow++;
			record_len <<= 1;
		}
		/* #ifdef CONFIG_MESON2_CHIP */
		/* pow */
		wr_bits(offset, VDIN_HIST_CTRL, hist_pow,
				HIST_POW_BIT, HIST_POW_WID);
		/* win_hs */
		wr_bits(offset, VDIN_HIST_H_START_END, hs,
				HIST_HSTART_BIT, HIST_HSTART_WID);
		/* win_he */
		wr_bits(offset, VDIN_HIST_H_START_END, he,
				HIST_HEND_BIT, HIST_HEND_WID);
		/* win_vs */
		wr_bits(offset, VDIN_HIST_V_START_END, vs,
				HIST_VSTART_BIT, HIST_VSTART_WID);
		/* win_ve */
		wr_bits(offset, VDIN_HIST_V_START_END, ve,
				HIST_VEND_BIT, HIST_VEND_WID);
	}
}

static inline void vdin_set_hist_mux(struct vdin_dev_s *devp)
{
	enum tvin_port_e port = TVIN_PORT_NULL;

	port = devp->parm.port;

	if ((port < TVIN_PORT_HDMI0) || (port > TVIN_PORT_HDMI7))
		return;
	/* use 11: form matrix1 din */
	wr_bits(devp->addr_offset, VDIN_HIST_CTRL, 3,
			HIST_HIST_DIN_SEL_BIT, HIST_HIST_DIN_SEL_WID);
}

static inline void vdin_set_wr_ctrl(unsigned int offset, unsigned int v,
		unsigned int h, enum vdin_format_convert_e format_convert,
		unsigned int color_depth_mode, unsigned int source_bitdeth)
{
	unsigned int write_format444 = 0, swap_cbcr = 0;
	/* unsigned int def_canvas_id = offset?
	 * vdin_canvas_ids[1][0]:vdin_canvas_ids[0][0];
	 */

	switch (format_convert)	{
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		write_format444 = 0;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		write_format444 = 2;
		swap_cbcr = 1;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		write_format444 = 2;
		swap_cbcr = 0;
		break;
	default:
		write_format444 = 1;
		break;
	}
	/* yuv422 full pack mode for 10bit */
	if (((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		color_depth_mode && (source_bitdeth > 8))
		write_format444 = 3;

	/* win_he */
	wr_bits(offset, VDIN_WR_H_START_END, (h - 1), WR_HEND_BIT, WR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_WR_V_START_END, (v - 1), WR_VEND_BIT, WR_VEND_WID);
	/* hconv_mode */
	wr_bits(offset, VDIN_WR_CTRL, 0, HCONV_MODE_BIT, HCONV_MODE_WID);
	/* vconv_mode */
	wr_bits(offset, VDIN_WR_CTRL, 0, VCONV_MODE_BIT, VCONV_MODE_WID);
	if (write_format444 == 2) {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WR_CTRL, swap_cbcr,
				SWAP_CBCR_BIT, SWAP_CBCR_WID);
		/* output even lines's cbcr */
		wr_bits(offset, VDIN_WR_CTRL, 0,
				VCONV_MODE_BIT, VCONV_MODE_WID);
		wr_bits(offset, VDIN_WR_CTRL, 2,
				HCONV_MODE_BIT, HCONV_MODE_WID);
		/* chroma canvas */
		/* wr_bits(offset, VDIN_WR_CTRL2, def_canvas_id+1,
		 * WRITE_CHROMA_CANVAS_ADDR_BIT,
		 * WRITE_CHROMA_CANVAS_ADDR_WID);
		 */
	} else if (write_format444 == 1) {
		/* output all cbcr */
		wr_bits(offset, VDIN_WR_CTRL, 3,
				VCONV_MODE_BIT, VCONV_MODE_WID);
	} else {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WR_CTRL, 0, SWAP_CBCR_BIT, SWAP_CBCR_WID);
		/* chroma canvas */
		/* wr_bits(offset, VDIN_WR_CTRL2, 0,
		 *  WRITE_CHROMA_CANVAS_ADDR_BIT,
		 */
		/* WRITE_CHROMA_CANVAS_ADDR_WID); */
	}
	/* format444 */
	wr_bits(offset, VDIN_WR_CTRL, write_format444, WR_FMT_BIT, WR_FMT_WID);
/*
 *	canvas_id
 *	wr_bits(offset, VDIN_WR_CTRL,
 *	def_canvas_id, WR_CANVAS_BIT, WR_CANVAS_WID);
 */
	/* req_urgent */
	wr_bits(offset, VDIN_WR_CTRL, 1, WR_REQ_URGENT_BIT, WR_REQ_URGENT_WID);
	/* req_en */
	wr_bits(offset, VDIN_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	/* dis ctrl reg wpulse */
	/* if (is_meson_g9tv_cpu() || is_meson_m8_cpu() ||
	 *	is_meson_m8m2_cpu() || is_meson_gxbb_cpu() ||
	 *	is_meson_m8b_cpu())
	 */
	wr_bits(offset, VDIN_WR_CTRL, 1, 10, 1);
	/*  swap the 2 64bits word in 128 words */
	/*if (is_meson_gxbb_cpu())*/
	wr_bits(offset, VDIN_WR_CTRL, 1, 19, 1);
}
#if 0
void set_wr_ctrl(int h_pos, int v_pos, struct vdin_dev_s *devp)
{
	enum tvin_sig_fmt_e fmt = devp->parm.info.fmt;
	unsigned int offset = devp->addr_offset;
	unsigned int ve = 0;
	const struct tvin_format_s *fmt_info = tvin_get_fmt_info(fmt);

	if (!fmt_info) {
		pr_err("[tvafe..] %s: error,fmt is null!!!\n", __func__);
		return;
	}

/* disable cut window */
	ve = fmt_info->v_active;
	wr_bits(offset, VDIN_COM_CTRL0, 0,
			INPUT_WIN_SEL_EN_BIT, INPUT_WIN_SEL_EN_WID);
	if (h_pos + fmt_info->hs_bp < 0) {
		unsigned int w_s = abs(h_pos + fmt_info->hs_bp);

		w_s = (1 + (w_s>>3))<<3;
		wr_bits(offset, VDIN_WR_H_START_END, w_s,
				WR_HSTART_BIT, WR_HSTART_WID);
	} else {
		wr_bits(offset, VDIN_WR_H_START_END, 0,
				WR_HSTART_BIT, WR_HSTART_WID);
	}
	if (v_pos + fmt_info->vs_bp - vbp_offset < 0) {
		wr_bits(offset, VDIN_WR_V_START_END,
				abs(v_pos + fmt_info->vs_bp - vbp_offset),
				WR_VSTART_BIT, WR_VSTART_WID);
	} else if (v_pos > fmt_info->vs_front) {
		/* config write window and cut window
		 * when v pos > v front porch
		 */
		wr(offset, VDIN_WIN_V_START_END,
			((v_pos - fmt_info->vs_front)<<INPUT_WIN_V_START_BIT) |
			ve << INPUT_WIN_V_END_BIT);
		wr(offset, VDIN_WIN_H_START_END,
				(0 << INPUT_WIN_H_START_BIT) |
				((devp->h_active - 1) << INPUT_WIN_H_END_BIT));
		wr_bits(offset, VDIN_COM_CTRL0, 1,
				INPUT_WIN_SEL_EN_BIT, INPUT_WIN_SEL_EN_WID);
		ve -= v_pos - fmt_info->vs_front;
	} else {
		wr_bits(offset, VDIN_WR_V_START_END, 0,
				WR_VSTART_BIT, WR_VSTART_WID);
	}
	vdin_set_wr_ctrl(devp->addr_offset, ve, devp->h_active,
			devp->format_convert);
}
#endif
/***************************global function**********************************/


unsigned int vdin_get_meas_hcnt64(unsigned int offset)
{
	return rd_bits(offset, VDIN_MEAS_HS_COUNT,
			MEAS_HS_CNT_BIT, MEAS_HS_CNT_WID);
}
unsigned int vdin_get_meas_vstamp(unsigned int offset)
{
	return rd(offset, VDIN_MEAS_VS_COUNT_LO);
}

unsigned int vdin_get_active_h(unsigned int offset)
{
	return rd_bits(offset, VDIN_ACTIVE_MAX_PIX_CNT_STATUS,
			ACTIVE_MAX_PIX_CNT_SDW_BIT, ACTIVE_MAX_PIX_CNT_SDW_WID);
}

unsigned int vdin_get_active_v(unsigned int offset)
{
	return rd_bits(offset, VDIN_LCNT_SHADOW_STATUS,
			ACTIVE_LN_CNT_SDW_BIT, ACTIVE_LN_CNT_SDW_WID);
}

unsigned int vdin_get_total_v(unsigned int offset)
{
	return rd_bits(offset, VDIN_LCNT_SHADOW_STATUS,
			GO_LN_CNT_SDW_BIT, GO_LN_CNT_SDW_WID);
}

void vdin_set_canvas_id(struct vdin_dev_s *devp, unsigned int rdma_enable,
			unsigned int canvas_id)
{
#ifdef CONFIG_AML_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle,
			VDIN_WR_CTRL+devp->addr_offset,
			canvas_id, WR_CANVAS_BIT, WR_CANVAS_WID);
	else
#endif
		wr_bits(devp->addr_offset, VDIN_WR_CTRL, canvas_id,
			WR_CANVAS_BIT, WR_CANVAS_WID);
}
unsigned int vdin_get_canvas_id(unsigned int offset)
{
	return rd_bits(offset, VDIN_WR_CTRL, WR_CANVAS_BIT, WR_CANVAS_WID);
}

void vdin_set_chma_canvas_id(struct vdin_dev_s *devp, unsigned int rdma_enable,
		unsigned int canvas_id)
{
#ifdef CONFIG_AML_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle,
				VDIN_WR_CTRL2+devp->addr_offset,
				canvas_id, WRITE_CHROMA_CANVAS_ADDR_BIT,
				WRITE_CHROMA_CANVAS_ADDR_WID);
	else
#endif
		wr_bits(devp->addr_offset, VDIN_WR_CTRL2,  canvas_id,
				WRITE_CHROMA_CANVAS_ADDR_BIT,
				WRITE_CHROMA_CANVAS_ADDR_WID);
}
unsigned int vdin_get_chma_canvas_id(unsigned int offset)
{
	return rd_bits(offset, VDIN_WR_CTRL2, WRITE_CHROMA_CANVAS_ADDR_BIT,
			WRITE_CHROMA_CANVAS_ADDR_WID);
}

/* reset default writing cavnas register */
void vdin_set_def_wr_canvas(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int def_canvas;

	def_canvas = vdin_canvas_ids[devp->index][0];

	/* [31:24]       write.out_ctrl         = 0x0b */
	/* [   23]       write.frame_rst_on_vs  = 1 */
	/* [   22]       write.lfifo_rst_on_vs  = 1 */
	/* [   21]       write.clr_direct_done  = 0 */
	/* [   20]       write.clr_nr_done      = 0 */
	/* [   12]       write.format444        = 1/(422, 444) */
	/* [   11]       write.canvas_latch_en  = 0 */
	/* [    9]       write.req_urgent       = 0 ***sub_module.enable*** */
	/* [    8]       write.req_en           = 0 ***sub_module.enable*** */
	/* [ 7: 0]       write.canvas           = 0 */
	if (enable_reset)
		wr(offset, VDIN_WR_CTRL, (0x0b401000 | def_canvas));
	else
		wr(offset, VDIN_WR_CTRL, (0x0bc01000 | def_canvas));
}

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
void vdin_set_ldim_max_init(unsigned int offset,
		int pic_h, int pic_v, int blk_vnum, int blk_hnum)
{
	int k;
	struct ldim_max_s ldimmax;
	int ldim_pic_rowmax = 1080;
	int ldim_pic_colmax = 1920;
	int ldim_blk_vnum = 2;
	int ldim_blk_hnum = 8;

	ldim_pic_rowmax = pic_v;
	ldim_pic_colmax = pic_h;
	ldim_blk_vnum = blk_vnum; /* 8; */
	ldim_blk_hnum = blk_hnum; /* 2; */
	ldimmax.ld_stamax_hidx[0] = 0;

	/* check ic type */
	if (!is_meson_gxtvbb_cpu())
		return;

	pr_info("\n****************vdin_set_ldim_max_init:hidx start********\n");
	for (k = 1; k < 11; k++) {
		ldimmax.ld_stamax_hidx[k] =
			((ldim_pic_colmax + ldim_blk_hnum - 1)/ldim_blk_hnum)*k;
		if (ldimmax.ld_stamax_hidx[k] > 4095)
			ldimmax.ld_stamax_hidx[k] = 4095; /* clip U12 */
		if (ldimmax.ld_stamax_hidx[k] == ldim_pic_colmax)
			ldimmax.ld_stamax_hidx[k] = ldim_pic_colmax - 1;
		pr_info("%d\t", ldimmax.ld_stamax_hidx[k]);
	}
	pr_info("\n****************vdin_set_ldim_max_init:hidx end*********\n");
	ldimmax.ld_stamax_vidx[0] = 0;
	pr_info("\n***********vdin_set_ldim_max_init:vidx start************\n");
	for (k = 1; k < 11; k++) {

		ldimmax.ld_stamax_vidx[k] = ((ldim_pic_rowmax +
				ldim_blk_vnum - 1)/ldim_blk_vnum)*k;
		if (ldimmax.ld_stamax_vidx[k] > 4095)
			ldimmax.ld_stamax_vidx[k] = 4095;  /* clip to U12 */
		if (ldimmax.ld_stamax_vidx[k] == ldim_pic_rowmax)
			ldimmax.ld_stamax_vidx[k] = ldim_pic_rowmax - 1;
		pr_info("%d\t", ldimmax.ld_stamax_vidx[k]);
	}
	pr_info("\n*******vdin_set_ldim_max_init:vidx end*******\n");
	wr(offset, VDIN_LDIM_STTS_HIST_REGION_IDX,
			(1 << LOCAL_DIM_STATISTIC_EN_BIT)  |
			(0 << EOL_EN_BIT)                  |
			(2 << VLINE_OVERLAP_NUMBER_BIT)    |
			(1 << HLINE_OVERLAP_NUMBER_BIT)    |
			(1 << LPF_BEFORE_STATISTIC_EN_BIT) |
			(1 << REGION_RD_INDEX_INC_BIT)
		);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 0,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[0]<<16|ldimmax.ld_stamax_hidx[0]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 1,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_hidx[2]<<16|ldimmax.ld_stamax_hidx[1]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 2,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[2]<<16|ldimmax.ld_stamax_vidx[1]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 3,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_hidx[4]<<16|ldimmax.ld_stamax_hidx[3]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 4,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[4]<<16|ldimmax.ld_stamax_vidx[3]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 5,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_hidx[6]<<16|ldimmax.ld_stamax_hidx[5]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 6,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[6]<<16|ldimmax.ld_stamax_vidx[5]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 7,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_hidx[8]<<16|ldimmax.ld_stamax_hidx[7]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 8,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[8]<<16|ldimmax.ld_stamax_vidx[7]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 9,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_hidx[10]<<16|ldimmax.ld_stamax_hidx[9]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 10,
		BLK_HV_POS_IDXS_BIT, BLK_HV_POS_IDXS_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
		ldimmax.ld_stamax_vidx[10]<<16|ldimmax.ld_stamax_vidx[9]);
	wr_bits(offset, VDIN_HIST_CTRL, 3, 9, 2);
	wr_bits(offset, VDIN_HIST_CTRL, 1, 8, 1);
}
#endif

void vdin_set_vframe_prop_info(struct vframe_s *vf,
		struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	struct vframe_bbar_s bbar = {0};
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	/*int i;*/
#endif
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,
	 * HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   );
	 */
	vf->prop.hist.hist_pow   = rd_bits(offset, VDIN_HIST_CTRL,
			HIST_POW_BIT, HIST_POW_WID);
	vf->prop.hist.luma_sum   = rd(offset, VDIN_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,
	 * HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID );
	 */
	vf->prop.hist.chroma_sum = rd(offset, VDIN_HIST_CHROMA_SUM);
	vf->prop.hist.pixel_sum  = rd_bits(offset, VDIN_HIST_SPL_PIX_CNT,
			HIST_PIX_CNT_BIT, HIST_PIX_CNT_WID);
	vf->prop.hist.height     = rd_bits(offset, VDIN_HIST_V_START_END,
			HIST_VEND_BIT, HIST_VEND_WID) -
		rd_bits(offset, VDIN_HIST_V_START_END,
				HIST_VSTART_BIT, HIST_VSTART_WID)+1;
	vf->prop.hist.width      = rd_bits(offset, VDIN_HIST_H_START_END,
			HIST_HEND_BIT, HIST_HEND_WID) -
		rd_bits(offset, VDIN_HIST_H_START_END,
				HIST_HSTART_BIT, HIST_HSTART_WID)+1;
	vf->prop.hist.luma_max   = rd_bits(offset, VDIN_HIST_MAX_MIN,
			HIST_MAX_BIT, HIST_MAX_WID);
	vf->prop.hist.luma_min   = rd_bits(offset, VDIN_HIST_MAX_MIN,
			HIST_MIN_BIT, HIST_MIN_WID);
	vf->prop.hist.gamma[0]   = rd_bits(offset, VDIN_DNLP_HIST00,
			HIST_ON_BIN_00_BIT, HIST_ON_BIN_00_WID);
	vf->prop.hist.gamma[1]   = rd_bits(offset, VDIN_DNLP_HIST00,
			HIST_ON_BIN_01_BIT, HIST_ON_BIN_01_WID);
	vf->prop.hist.gamma[2]   = rd_bits(offset, VDIN_DNLP_HIST01,
			HIST_ON_BIN_02_BIT, HIST_ON_BIN_02_WID);
	vf->prop.hist.gamma[3]   = rd_bits(offset, VDIN_DNLP_HIST01,
			HIST_ON_BIN_03_BIT, HIST_ON_BIN_03_WID);
	vf->prop.hist.gamma[4]   = rd_bits(offset, VDIN_DNLP_HIST02,
			HIST_ON_BIN_04_BIT, HIST_ON_BIN_04_WID);
	vf->prop.hist.gamma[5]   = rd_bits(offset, VDIN_DNLP_HIST02,
			HIST_ON_BIN_05_BIT, HIST_ON_BIN_05_WID);
	vf->prop.hist.gamma[6]   = rd_bits(offset, VDIN_DNLP_HIST03,
			HIST_ON_BIN_06_BIT, HIST_ON_BIN_06_WID);
	vf->prop.hist.gamma[7]   = rd_bits(offset, VDIN_DNLP_HIST03,
			HIST_ON_BIN_07_BIT, HIST_ON_BIN_07_WID);
	vf->prop.hist.gamma[8]   = rd_bits(offset, VDIN_DNLP_HIST04,
			HIST_ON_BIN_08_BIT, HIST_ON_BIN_08_WID);
	vf->prop.hist.gamma[9]   = rd_bits(offset, VDIN_DNLP_HIST04,
			HIST_ON_BIN_09_BIT, HIST_ON_BIN_09_WID);
	vf->prop.hist.gamma[10]  = rd_bits(offset, VDIN_DNLP_HIST05,
			HIST_ON_BIN_10_BIT, HIST_ON_BIN_10_WID);
	vf->prop.hist.gamma[11]  = rd_bits(offset, VDIN_DNLP_HIST05,
			HIST_ON_BIN_11_BIT, HIST_ON_BIN_11_WID);
	vf->prop.hist.gamma[12]  = rd_bits(offset, VDIN_DNLP_HIST06,
			HIST_ON_BIN_12_BIT, HIST_ON_BIN_12_WID);
	vf->prop.hist.gamma[13]  = rd_bits(offset, VDIN_DNLP_HIST06,
			HIST_ON_BIN_13_BIT, HIST_ON_BIN_13_WID);
	vf->prop.hist.gamma[14]  = rd_bits(offset, VDIN_DNLP_HIST07,
			HIST_ON_BIN_14_BIT, HIST_ON_BIN_14_WID);
	vf->prop.hist.gamma[15]  = rd_bits(offset, VDIN_DNLP_HIST07,
			HIST_ON_BIN_15_BIT, HIST_ON_BIN_15_WID);
	vf->prop.hist.gamma[16]  = rd_bits(offset, VDIN_DNLP_HIST08,
			HIST_ON_BIN_16_BIT, HIST_ON_BIN_16_WID);
	vf->prop.hist.gamma[17]  = rd_bits(offset, VDIN_DNLP_HIST08,
			HIST_ON_BIN_17_BIT, HIST_ON_BIN_17_WID);
	vf->prop.hist.gamma[18]  = rd_bits(offset, VDIN_DNLP_HIST09,
			HIST_ON_BIN_18_BIT, HIST_ON_BIN_18_WID);
	vf->prop.hist.gamma[19]  = rd_bits(offset, VDIN_DNLP_HIST09,
			HIST_ON_BIN_19_BIT, HIST_ON_BIN_19_WID);
	vf->prop.hist.gamma[20]  = rd_bits(offset, VDIN_DNLP_HIST10,
			HIST_ON_BIN_20_BIT, HIST_ON_BIN_20_WID);
	vf->prop.hist.gamma[21]  = rd_bits(offset, VDIN_DNLP_HIST10,
			HIST_ON_BIN_21_BIT, HIST_ON_BIN_21_WID);
	vf->prop.hist.gamma[22]  = rd_bits(offset, VDIN_DNLP_HIST11,
			HIST_ON_BIN_22_BIT, HIST_ON_BIN_22_WID);
	vf->prop.hist.gamma[23]  = rd_bits(offset, VDIN_DNLP_HIST11,
			HIST_ON_BIN_23_BIT, HIST_ON_BIN_23_WID);
	vf->prop.hist.gamma[24]  = rd_bits(offset, VDIN_DNLP_HIST12,
			HIST_ON_BIN_24_BIT, HIST_ON_BIN_24_WID);
	vf->prop.hist.gamma[25]  = rd_bits(offset, VDIN_DNLP_HIST12,
			HIST_ON_BIN_25_BIT, HIST_ON_BIN_25_WID);
	vf->prop.hist.gamma[26]  = rd_bits(offset, VDIN_DNLP_HIST13,
			HIST_ON_BIN_26_BIT, HIST_ON_BIN_26_WID);
	vf->prop.hist.gamma[27]  = rd_bits(offset, VDIN_DNLP_HIST13,
			HIST_ON_BIN_27_BIT, HIST_ON_BIN_27_WID);
	vf->prop.hist.gamma[28]  = rd_bits(offset, VDIN_DNLP_HIST14,
			HIST_ON_BIN_28_BIT, HIST_ON_BIN_28_WID);
	vf->prop.hist.gamma[29]  = rd_bits(offset, VDIN_DNLP_HIST14,
			HIST_ON_BIN_29_BIT, HIST_ON_BIN_29_WID);
	vf->prop.hist.gamma[30]  = rd_bits(offset, VDIN_DNLP_HIST15,
			HIST_ON_BIN_30_BIT, HIST_ON_BIN_30_WID);
	vf->prop.hist.gamma[31]  = rd_bits(offset, VDIN_DNLP_HIST15,
			HIST_ON_BIN_31_BIT, HIST_ON_BIN_31_WID);
	vf->prop.hist.gamma[32]  = rd_bits(offset, VDIN_DNLP_HIST16,
			HIST_ON_BIN_32_BIT, HIST_ON_BIN_32_WID);
	vf->prop.hist.gamma[33]  = rd_bits(offset, VDIN_DNLP_HIST16,
			HIST_ON_BIN_33_BIT, HIST_ON_BIN_33_WID);
	vf->prop.hist.gamma[34]  = rd_bits(offset, VDIN_DNLP_HIST17,
			HIST_ON_BIN_34_BIT, HIST_ON_BIN_34_WID);
	vf->prop.hist.gamma[35]  = rd_bits(offset, VDIN_DNLP_HIST17,
			HIST_ON_BIN_35_BIT, HIST_ON_BIN_35_WID);
	vf->prop.hist.gamma[36]  = rd_bits(offset, VDIN_DNLP_HIST18,
			HIST_ON_BIN_36_BIT, HIST_ON_BIN_36_WID);
	vf->prop.hist.gamma[37]  = rd_bits(offset, VDIN_DNLP_HIST18,
			HIST_ON_BIN_37_BIT, HIST_ON_BIN_37_WID);
	vf->prop.hist.gamma[38]  = rd_bits(offset, VDIN_DNLP_HIST19,
			HIST_ON_BIN_38_BIT, HIST_ON_BIN_38_WID);
	vf->prop.hist.gamma[39]  = rd_bits(offset, VDIN_DNLP_HIST19,
			HIST_ON_BIN_39_BIT, HIST_ON_BIN_39_WID);
	vf->prop.hist.gamma[40]  = rd_bits(offset, VDIN_DNLP_HIST20,
			HIST_ON_BIN_40_BIT, HIST_ON_BIN_40_WID);
	vf->prop.hist.gamma[41]  = rd_bits(offset, VDIN_DNLP_HIST20,
			HIST_ON_BIN_41_BIT, HIST_ON_BIN_41_WID);
	vf->prop.hist.gamma[42]  = rd_bits(offset, VDIN_DNLP_HIST21,
			HIST_ON_BIN_42_BIT, HIST_ON_BIN_42_WID);
	vf->prop.hist.gamma[43]  = rd_bits(offset, VDIN_DNLP_HIST21,
			HIST_ON_BIN_43_BIT, HIST_ON_BIN_43_WID);
	vf->prop.hist.gamma[44]  = rd_bits(offset, VDIN_DNLP_HIST22,
			HIST_ON_BIN_44_BIT, HIST_ON_BIN_44_WID);
	vf->prop.hist.gamma[45]  = rd_bits(offset, VDIN_DNLP_HIST22,
			HIST_ON_BIN_45_BIT, HIST_ON_BIN_45_WID);
	vf->prop.hist.gamma[46]  = rd_bits(offset, VDIN_DNLP_HIST23,
			HIST_ON_BIN_46_BIT, HIST_ON_BIN_46_WID);
	vf->prop.hist.gamma[47]  = rd_bits(offset, VDIN_DNLP_HIST23,
			HIST_ON_BIN_47_BIT, HIST_ON_BIN_47_WID);
	vf->prop.hist.gamma[48]  = rd_bits(offset, VDIN_DNLP_HIST24,
			HIST_ON_BIN_48_BIT, HIST_ON_BIN_48_WID);
	vf->prop.hist.gamma[49]  = rd_bits(offset, VDIN_DNLP_HIST24,
			HIST_ON_BIN_49_BIT, HIST_ON_BIN_49_WID);
	vf->prop.hist.gamma[50]  = rd_bits(offset, VDIN_DNLP_HIST25,
			HIST_ON_BIN_50_BIT, HIST_ON_BIN_50_WID);
	vf->prop.hist.gamma[51]  = rd_bits(offset, VDIN_DNLP_HIST25,
			HIST_ON_BIN_51_BIT, HIST_ON_BIN_51_WID);
	vf->prop.hist.gamma[52]  = rd_bits(offset, VDIN_DNLP_HIST26,
			HIST_ON_BIN_52_BIT, HIST_ON_BIN_52_WID);
	vf->prop.hist.gamma[53]  = rd_bits(offset, VDIN_DNLP_HIST26,
			HIST_ON_BIN_53_BIT, HIST_ON_BIN_53_WID);
	vf->prop.hist.gamma[54]  = rd_bits(offset, VDIN_DNLP_HIST27,
			HIST_ON_BIN_54_BIT, HIST_ON_BIN_54_WID);
	vf->prop.hist.gamma[55]  = rd_bits(offset, VDIN_DNLP_HIST27,
			HIST_ON_BIN_55_BIT, HIST_ON_BIN_55_WID);
	vf->prop.hist.gamma[56]  = rd_bits(offset, VDIN_DNLP_HIST28,
			HIST_ON_BIN_56_BIT, HIST_ON_BIN_56_WID);
	vf->prop.hist.gamma[57]  = rd_bits(offset, VDIN_DNLP_HIST28,
			HIST_ON_BIN_57_BIT, HIST_ON_BIN_57_WID);
	vf->prop.hist.gamma[58]  = rd_bits(offset, VDIN_DNLP_HIST29,
			HIST_ON_BIN_58_BIT, HIST_ON_BIN_58_WID);
	vf->prop.hist.gamma[59]  = rd_bits(offset, VDIN_DNLP_HIST29,
			HIST_ON_BIN_59_BIT, HIST_ON_BIN_59_WID);
	vf->prop.hist.gamma[60]  = rd_bits(offset, VDIN_DNLP_HIST30,
			HIST_ON_BIN_60_BIT, HIST_ON_BIN_60_WID);
	vf->prop.hist.gamma[61]  = rd_bits(offset, VDIN_DNLP_HIST30,
			HIST_ON_BIN_61_BIT, HIST_ON_BIN_61_WID);
	vf->prop.hist.gamma[62]  = rd_bits(offset, VDIN_DNLP_HIST31,
			HIST_ON_BIN_62_BIT, HIST_ON_BIN_62_WID);
	vf->prop.hist.gamma[63]  = rd_bits(offset, VDIN_DNLP_HIST31,
			HIST_ON_BIN_63_BIT, HIST_ON_BIN_63_WID);

	/* fetch bbar info */
	bbar.top        = rd_bits(offset, VDIN_BLKBAR_STATUS0,
			BLKBAR_TOP_POS_BIT, BLKBAR_TOP_POS_WID);
	bbar.bottom     = rd_bits(offset, VDIN_BLKBAR_STATUS0,
			BLKBAR_BTM_POS_BIT,   BLKBAR_BTM_POS_WID);
	bbar.left       = rd_bits(offset, VDIN_BLKBAR_STATUS1,
			BLKBAR_LEFT_POS_BIT, BLKBAR_LEFT_POS_WID);
	bbar.right      = rd_bits(offset, VDIN_BLKBAR_STATUS1,
			BLKBAR_RIGHT_POS_BIT, BLKBAR_RIGHT_POS_WID);
	if (bbar.top > bbar.bottom) {
		bbar.top = 0;
		bbar.bottom = vf->height - 1;
	}
	if (bbar.left > bbar.right) {
		bbar.left = 0;
		bbar.right = vf->width - 1;
	}

	/* Update Histgram windown with detected BlackBar window */
	if (hist_bar_enable)
		vdin_set_histogram(offset, 0, vf->width - 1, 0, vf->height - 1);
	else
		vdin_set_histogram(offset, bbar.left, bbar.right,
				bbar.top, bbar.bottom);

	if (black_bar_enable) {
		vf->prop.bbar.top        = bbar.top;
		vf->prop.bbar.bottom     = bbar.bottom;
		vf->prop.bbar.left       = bbar.left;
		vf->prop.bbar.right      = bbar.right;
	} else
		memset(&vf->prop.bbar, 0, sizeof(struct vframe_bbar_s));

	/* fetch meas info - For M2 or further chips only, not for M1 chip */
	vf->prop.meas.vs_stamp = devp->stamp;
	vf->prop.meas.vs_cycle = devp->cycle;
#if 0
/*#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING*/
	/* get ldim max */
	if (vdin_ldim_max_en && is_meson_gxtvbb_cpu()) {
		wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 0,
			REGION_RD_INDEX_BIT, REGION_RD_INDEX_WID);
		for (i = 0; i < 100; i++)
			vf->prop.hist.ldim_max[i] = rd(offset,
				VDIN_LDIM_STTS_HIST_READ_REGION);
	}
#endif
}

static inline ulong vdin_reg_limit(ulong val, ulong wid)
{
	if (val < (1<<wid))
		return val;
	else
		return (1<<wid)-1;
}



void vdin_set_all_regs(struct vdin_dev_s *devp)
{

	/* matrix sub-module */
	vdin_set_matrix(devp);

	/* bbar sub-module */
	vdin_set_bbar(devp->addr_offset, devp->v_active, devp->h_active);
#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
	/* ldim sub-module */
	/* vdin_set_ldim_max_init(devp->addr_offset, 1920, 1080, 8, 2); */
	vdin_set_ldim_max_init(devp->addr_offset, devp->h_active,
				devp->v_active, 8, 2);
#endif
	/* hist sub-module */
	vdin_set_histogram(devp->addr_offset, 0,
			devp->h_active - 1, 0, devp->v_active - 1);
	/* hist mux selecttion */
	vdin_set_hist_mux(devp);
	/* write sub-module */
	vdin_set_wr_ctrl(devp->addr_offset, devp->v_active,
			devp->h_active, devp->format_convert,
			devp->color_depth_mode, devp->source_bitdepth);
	/* top sub-module */
	vdin_set_top(devp->addr_offset, devp->parm.port,
			devp->prop.color_format, devp->h_active,
			devp->bt_path);
	vdin_set_meas_mux(devp->addr_offset, devp->parm.port,
			devp->bt_path);
}

void vdin_delay_line(unsigned short num, unsigned int offset)
{
	wr_bits(offset, VDIN_COM_CTRL0, num,
			DLY_GO_FLD_LN_NUM_BIT, DLY_GO_FLD_LN_NUM_WID);
	if (num)
		wr_bits(offset, VDIN_COM_CTRL0, 1,
			DLY_GO_FLD_EN_BIT, DLY_GO_FLD_EN_WID);
	else
		wr_bits(offset, VDIN_COM_CTRL0, 0,
			DLY_GO_FLD_EN_BIT, DLY_GO_FLD_EN_WID);
}

void vdin_set_default_regmap(unsigned int offset)
{
	unsigned int def_canvas_id;
	/* unsigned int offset = devp->addr_offset; */

	/* [   31]        mpeg.en               = 0 ***sub_module.enable*** */
	/* [   30]        mpeg.even_fld         = 0/(odd, even) */



	/* [26:20]         top.hold_ln          = 0    //8 */
	/* [   19]      vs_dly.en               = 0 ***sub_module.enable*** */
	/* [18:12]      vs_dly.dly_ln           = 0 */
	/* [11:10]         map.comp2            = 2/(comp0, comp1, comp2) */
	/* [ 9: 8]         map.comp1            = 1/(comp0, comp1, comp2) */
	/* [ 7: 6]         map.comp0            = 0/(comp0, comp1, comp2) */


	/* [    4]         top.datapath_en      = 1 */
	/* [ 3: 0] top.mux = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin) */
	wr(offset, VDIN_COM_CTRL0, 0x00000910);
	vdin_delay_line(delay_line_num, offset);
	/* [   23] asfifo_tvfe.de_en            = 1 */
	/* [   22] asfifo_tvfe.vs_en            = 1 */
	/* [   21] asfifo_tvfe.hs_en            = 1 */
	/* [   20] asfifo_tvfe.vs_inv = 0/(positive-active, negative-active) */
	/* [   19] asfifo_tvfe.hs_inv = 0/(positive-active, negative-active) */
	/* [   18] asfifo_tvfe.rst_on_vs        = 1 */
	/* [   17] asfifo_tvfe.clr_ov_flag      = 0 */
	/* [   16] asfifo_tvfe.rst              = 0 */
	/* [    7]  asfifo_656.de_en            = 1 */
	/* [    6]  asfifo_656.vs_en            = 1 */
	/* [    5]  asfifo_656.hs_en            = 1 */
	/* [    4]  asfifo_656.vs_inv = 0/(positive-active, negative-active) */
	/* [    3]  asfifo_656.hs_inv = 0/(positive-active, negative-active) */
	/* [    2]  asfifo_656.rst_on_vs        = 0 */
	/* [    1]  asfifo_656.clr_ov_flag      = 0 */
	/* [    0]  asfifo_656.rst              = 0 */
	/* wr(offset, VDIN_ASFIFO_CTRL0, 0x00000000); */
	/* [   23] asfifo_hdmi.de_en            = 1 */
	/* [   22] asfifo_hdmi.vs_en            = 1 */
	/* [   21] asfifo_hdmi.hs_en            = 1 */
	/* [   20] asfifo_hdmi.vs_inv = 0/(positive-active, negative-active) */
	/* [   19] asfifo_hdmi.hs_inv = 0/(positive-active, negative-active) */
	/* [   18] asfifo_hdmi.rst_on_vs        = 1 */
	/* [   17] asfifo_hdmi.clr_ov_flag      = 0 */
	/* [   16] asfifo_hdmi.rst              = 0 */
	/* [    7] asfifo_cvd2.de_en            = 1 */
	/* [    6] asfifo_cvd2.vs_en            = 1 */
	/* [    5] asfifo_cvd2.hs_en            = 1 */
	/* [    4] asfifo_cvd2.vs_inv = 0/(positive-active, negative-active) */
	/* [    3] asfifo_cvd2.hs_inv = 0/(positive-active, negative-active) */
	/* [    2] asfifo_cvd2.rst_on_vs        = 1 */
	/* [    1] asfifo_cvd2.clr_ov_flag      = 0 */
	/* [    0] asfifo_cvd2.rst              = 0 */
	/* wr(offset, VDIN_ASFIFO_CTRL1, 0x00000000); */
	/* [28:16]         top.input_width_m1   = 0 */
	/* [12: 0]         top.output_width_m1  = 0 */
	wr(offset, VDIN_WIDTHM1I_WIDTHM1O, 0x00000000);
	/* [14: 8]         hsc.init_pix_in_ptr  = 0 */
	/* [    7]         hsc.phsc_en          = 0 */
	/* [    6]         hsc.en               = 0 ***sub_module.enable*** */
	/* [    5]         hsc.short_ln_en      = 1 */
	/* [    4]         hsc.nearest_en       = 0 */
	/* [    3]         hsc.phase0_always    = 1 */
	/* [ 2: 0] hsc.filt_dep         = 0/(DEPTH4,DEPTH1, DEPTH2, DEPTH3) */
	/* wr(offset, VDIN_SC_MISC_CTRL, 0x00000028); */
	/* [28:24]         hsc.phase_step_int   = 0 <u5.0> */
	/* [23: 0]         hsc.phase_step_fra   = 0 <u0.24> */
	/* wr(offset, VDIN_HSC_PHASE_STEP, 0x00000000); */
	/* [30:29] hsc.repeat_pix0_num  = 1 // ?
	 * to confirm pix0 is always used
	 */
	/* [28:24] hsc.ini_receive_num  = 4 // ?
	 * to confirm pix0 is always used
	 */
	/* [23: 0]         hsc.ini_phase        = 0 */
	/* wr(offset, VDIN_HSC_INI_CTRL, 0x24000000); */


	/* [   25]  decimation.rst              = 0 */
	/* [   24]  decimation.en = 0 ***sub_module.enable*** */
	/* [23:20]  decimation.phase            = 0 */
	/* [19:16]  decimation.ratio            = 0/(1, 1/2, ..., 1/16) */
	/* [    7] asfifo_dvin.de_en            = 1 */
	/* [    6] asfifo_dvin.vs_en            = 1 */
	/* [    5] asfifo_dvin.hs_en            = 1 */
	/* [    4] asfifo_dvin.vs_inv = 0/(positive-active, negative-active) */
	/* [    3] asfifo_dvin.hs_inv = 0/(positive-active, negative-active) */
	/* [    2] asfifo_dvin.rst_on_vs        = 1 */
	/* [    1] asfifo_dvin.clr_ov_flag      = 0 */
	/* [    0] asfifo_dvin.rst              = 0 */
	wr(offset, VDIN_ASFIFO_CTRL2, 0x00000000);
	/* Bit 15:8 vdi7 asfifo_ctrl */
	/* Bit 7:0 vdi6 asfifo_ctrl */
	/* wr(offset, VDIN_ASFIFO_CTRL3, 0x00000000); */


	/* [    0]      matrix.en               = 0 ***sub_module.enable*** */
	wr(offset, VDIN_MATRIX_CTRL, 0x00000000);
	/* [28:16]      matrix.coef00           = 0 <s2.10> */
	/* [12: 0]      matrix.coef01           = 0 <s2.10> */
	wr(offset, VDIN_MATRIX_COEF00_01, 0x00000000);
	/* [28:16]      matrix.coef02           = 0 <s2.10> */
	/* [12: 0]      matrix.coef10           = 0 <s2.10> */
	wr(offset, VDIN_MATRIX_COEF02_10, 0x00000000);
	/* [28:16]      matrix.coef11           = 0 <s2.10> */
	/* [12: 0]      matrix.coef12           = 0 <s2.10> */
	wr(offset, VDIN_MATRIX_COEF11_12, 0x00000000);
	/* [28:16]      matrix.coef20           = 0 <s2.10> */
	/* [12: 0]      matrix.coef21           = 0 <s2.10> */
	wr(offset, VDIN_MATRIX_COEF20_21, 0x00000000);
	/* [12: 0]      matrix.coef22           = 0 <s2.10> */
	wr(offset, VDIN_MATRIX_COEF22, 0x00000000);
	/* [26:16]      matrix.offset0          = 0 <s8.2> */
	/* [10: 0]      matrix.ofsset1          = 0 <s8.2> */
	wr(offset, VDIN_MATRIX_OFFSET0_1, 0x00000000);
	/* [10: 0]      matrix.ofsset2          = 0 <s8.2> */
	wr(offset, VDIN_MATRIX_OFFSET2, 0x00000000);
	/* [26:16]      matrix.pre_offset0      = 0 <s8.2> */
	/* [10: 0]      matrix.pre_ofsset1      = 0 <s8.2> */
	wr(offset, VDIN_MATRIX_PRE_OFFSET0_1, 0x00000000);
	/* [10: 0]      matrix.pre_ofsset2      = 0 <s8.2> */
	wr(offset, VDIN_MATRIX_PRE_OFFSET2, 0x00000000);
	/* [11: 0]       write.lfifo_buf_size   = 0x100 */
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu())
		wr(offset, VDIN_LFIFO_CTRL,     0x00000f00);
	else
		wr(offset, VDIN_LFIFO_CTRL,     0x00000780);
	/* [15:14]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [13:12]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [11:10]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [ 9: 8]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [ 7: 6]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [ 5: 4]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [ 3: 2]     clkgate.bbar             = 0/(auto, off, on, on) */
	/* [    0]     clkgate.bbar             = 0/(auto, off!!!!!!!!) */
	wr(offset, VDIN_COM_GCLK_CTRL, 0x00000000);


	/* [12: 0]  decimation.output_width_m1  = 0 */
	wr(offset, VDIN_INTF_WIDTHM1, 0x00000000);


	def_canvas_id = offset ? vdin_canvas_ids[1][0]:vdin_canvas_ids[0][0];

	/* [31:24]       write.out_ctrl         = 0x0b */
	/* [   23]       write.frame_rst_on_vs  = 1 */
	/* [   22]       write.lfifo_rst_on_vs  = 1 */
	/* [   21]       write.clr_direct_done  = 0 */
	/* [   20]       write.clr_nr_done      = 0 */
	/* [   12]       write.format444        = 1/(422, 444) */
	/* [   11]       write.canvas_latch_en  = 0 */
	/* [    9]       write.req_urgent       = 0 ***sub_module.enable*** */
	/* [    8]       write.req_en           = 0 ***sub_module.enable*** */
	/* [ 7: 0]       write.canvas           = 0 */
	if (enable_reset)
		wr(offset, VDIN_WR_CTRL, (0x0b401000 | def_canvas_id));
	else
		wr(offset, VDIN_WR_CTRL, (0x0bc01000 | def_canvas_id));

	/* [8]   discard data before line fifo= 0  normal mode */
	/* [7:0] write chroma addr = 1 */
	wr_bits(offset, VDIN_WR_CTRL2, def_canvas_id+1,
			WRITE_CHROMA_CANVAS_ADDR_BIT,
			WRITE_CHROMA_CANVAS_ADDR_WID);
	wr_bits(offset, VDIN_WR_CTRL2, 0,
		DISCARD_BEF_LINE_FIFO_BIT, DISCARD_BEF_LINE_FIFO_WID);
	wr_bits(offset, VDIN_WR_CTRL2, vdin_wr_burst_mode,
		VDIN_WR_BURST_MODE_BIT, VDIN_WR_BURST_MODE_WID);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		wr_bits(offset, VDIN_WR_CTRL2, 1,
			VDIN_WR_DATA_EXT_EN_BIT, VDIN_WR_DATA_EXT_EN_WID);
	else
		wr_bits(offset, VDIN_WR_CTRL2, 0,
			VDIN_WR_DATA_EXT_EN_BIT, VDIN_WR_DATA_EXT_EN_WID);
	/* [20:25] integer = 0 */
	/* [0:19] fraction = 0 */
	wr(offset, VDIN_VSC_PHASE_STEP, 0x0000000);

	/* disable hscale&pre hscale */
	wr_bits(offset, VDIN_SC_MISC_CTRL, 0, PRE_HSCL_EN_BIT, PRE_HSCL_EN_WID);
	wr_bits(offset, VDIN_SC_MISC_CTRL, 0, HSCL_EN_BIT, HSCL_EN_WID);

	/* Bit 23, vsc_en, vertical scaler enable */
	/* Bit 21 vsc_phase0_always_en, when scale up,
	 * you have to set it to 1
	 */
	/* Bit 20:16 ini skip_line_num */
	/* Bit 15:0 vscaler ini_phase */
	wr(offset, VDIN_VSC_INI_CTRL, 0x000000);
	/* Bit 12:0, scaler input height minus 1 */
	wr(offset, VDIN_SCIN_HEIGHTM1, 0x00000);
	/* Bit 23:16, dummy component 0 */
	/* Bit 15:8, dummy component 1 */
	/* Bit 7:0, dummy component 2 */
	wr(offset, VDIN_DUMMY_DATA, 0x8080);
	/* Bit 23:16 component 0 */
	/* Bit 15:8  component 1 */
	/* Bit 7:0 component 2 */
	wr(offset, VDIN_MATRIX_HL_COLOR, 0x000000);
	/* 28:16 probe x, postion */
	/* 12:0  probe y, position */
	wr(offset, VDIN_MATRIX_PROBE_POS, 0x00000000);
	/* Bit 31, local dimming statistic enable */
	/* Bit 28, eol enable */
	/* Bit 27:25, vertical line overlap number for max finding */
	/* Bit 24:22, horizontal pixel overlap number,
	 * 0: 17 pix, 1: 9 pix, 2: 5 pix, 3: 3 pix, 4: 0 pix
	 */
	/* Bit 20, 1,2,1 low pass filter enable before max/hist statistic */
	/* Bit 19:16, region H/V position index,
	 * refer to VDIN_LDIM_STTS_HIST_SET_REGION
	 */
	/* Bit 15, 1: region read index auto increase
	 * per read to VDIN_LDIM_STTS_HIST_READ_REGION
	 */
	/* Bit 6:0, region read index */
	if (is_meson_gxtvbb_cpu())
		wr(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 0x00000000);
	/* [27:16] write.output_hs        = 0 */
	/* [11: 0] write.output_he        = 0 */
	wr(offset, VDIN_WR_H_START_END, 0x00000000);
	/* [27:16] write.output_vs        = 0 */
	/* [11: 0] write.output_ve        = 0 */
	wr(offset, VDIN_WR_V_START_END, 0x00000000);
	/* [ 6: 5] hist.pow              = 0 */
	/* [ 3: 2] hist.mux = 0/(matrix_out, hsc_out, phsc_in) */
	/* [    1] hist.win_en           = 1 */
	/* [    0] hist.read_en          = 1 */
	wr(offset, VDIN_HIST_CTRL, 0x00000003);
	/* [28:16] hist.win_hs           = 0 */
	/* [12: 0] hist.win_he           = 0 */
	wr(offset, VDIN_HIST_H_START_END, 0x00000000);
	/* [28:16] hist.win_vs           = 0 */
	/* [12: 0] hist.win_ve           = 0 */
	wr(offset, VDIN_HIST_V_START_END, 0x00000000);


	/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
	/* if (is_meson_gxbb_cpu()) */
	/* ; */
	/* else */
	/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */

	/* [   18] meas.rst              = 0 */
	/* [   17] meas.widen_hs_vs_en   = 1 */
	/* [   16] meas.vs_cnt_accum_en  = 0 */
	/* [14:12] meas.mux = 0/(null, 656, tvfe, cvd2, hdmi, dvin) */
	/* [11: 4] meas.vs_span_m1       = 0 */
	/* [ 2: 0] meas.hs_ind           = 0 */
	wr(offset, VDIN_MEAS_CTRL0, 0x00020000);
	/* HS range0: Line #112 ~ Line #175 */
	/* [28:16] meas.hs_range_start  = 112*/
	/* [12: 0] meas.hs_range_end = 175 */
	wr(offset, VDIN_MEAS_HS_RANGE, 0x007000af);



	/* [    8]  bbar.white_en         = 0 */
	/* [ 7: 0]  bbar.white_thr        = 0 */
	wr(offset, VDIN_BLKBAR_CTRL1, 0x00000000);




	/* [20: 8] bbar.region_width     = 0 */
	/* [ 7: 5] bbar.src_on_v         = 0/(Y, sU, sV, U, V) */
	/* [    4] bbar.search_one_step  = 0 */
	/* [    3] bbar.raising_edge_rst = 0 */

	/* [ 2: 1] bbar.mux = 0/(matrix_out, hsc_out, phsc_in) */
	/* [    0] bbar.en = 0 ***sub_module.enable*** */
	wr(offset, VDIN_BLKBAR_CTRL0, 0x14000000);
	/* [28:16]  bbar.win_hs           = 0 */
	/* [12: 0]  bbar.win_he           = 0 */
	wr(offset, VDIN_BLKBAR_H_START_END, 0x00000000);
	/* [28:16] bbar.win_vs           = 0 */
	/* [12: 0] bbar.win_ve           = 0 */
	wr(offset, VDIN_BLKBAR_V_START_END, 0x00000000);
	/* [19: 0] bbar.bblk_thr_on_bpix = 0 */
	wr(offset, VDIN_BLKBAR_CNT_THRESHOLD, 0x00000000);
	/* [28:16]  bbar.blnt_thr_on_wpix = 0 */
	/* [12: 0]  bbar.blnb_thr_on_wpix = 0 */
	wr(offset, VDIN_BLKBAR_ROW_TH1_TH2, 0x00000000);


	/* [28:16] input_win.hs               = 0 */
	/* [12: 0] input_win.he               = 0 */
	wr(offset, VDIN_WIN_H_START_END, 0x00000000);
	/* [28:16] input_win.vs               = 0 */
	/* [12: 0] input_win.ve               = 0 */
	wr(offset, VDIN_WIN_V_START_END, 0x00000000);

}

void vdin_hw_enable(unsigned int offset)
{
	/* enable video data input */
	/* [    4]  top.datapath_en  = 1 */
	wr_bits(offset, VDIN_COM_CTRL0, 1, 4, 1);

	/* mux input */
	/* [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin) */
	wr_bits(offset, VDIN_COM_CTRL0, 0, 0, 4);

	/* enable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Enable blackbar clock       = 00/(auto, off, on, on) */
	/* [13:12]  Enable histogram clock      = 00/(auto, off, on, on) */
	/* [11:10]  Enable line fifo1 clock     = 00/(auto, off, on, on) */
	/* [ 9: 8]  Enable matrix clock         = 00/(auto, off, on, on) */
	/* [ 7: 6]  Enable hscaler clock        = 00/(auto, off, on, on) */
	/* [ 5: 4]  Enable pre hscaler clock    = 00/(auto, off, on, on) */
	/* [ 3: 2]  Enable clock0               = 00/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	wr(offset, VDIN_COM_GCLK_CTRL, 0x0);
}


void vdin_hw_disable(unsigned int offset)
{
	/* disable cm2 */
	wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	/* disable video data input */
	/* [    4]  top.datapath_en  = 0 */
	wr_bits(offset, VDIN_COM_CTRL0, 0, 4, 1);

	/* mux null input */
	/* [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin) */
	wr_bits(offset, VDIN_COM_CTRL0, 0, 0, 4);
	wr(offset, VDIN_COM_CTRL0, 0x00000910);
	vdin_delay_line(delay_line_num, offset);
	if (enable_reset)
		wr(offset, VDIN_WR_CTRL, 0x0b401000);
	else
		wr(offset, VDIN_WR_CTRL, 0x0bc01000);

	/* disable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Disable blackbar clock      = 01/(auto, off, on, on) */
	/* [13:12]  Disable histogram clock     = 01/(auto, off, on, on) */
	/* [11:10]  Disable line fifo1 clock    = 01/(auto, off, on, on) */
	/* [ 9: 8]  Disable matrix clock        = 01/(auto, off, on, on) */
	/* [ 7: 6]  Disable hscaler clock       = 01/(auto, off, on, on) */
	/* [ 5: 4]  Disable pre hscaler clock   = 01/(auto, off, on, on) */
	/* [ 3: 2]  Disable clock0              = 01/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	wr(offset, VDIN_COM_GCLK_CTRL, 0x5554);
}

/* get current vsync field type 0:top 1 bottom */
unsigned int vdin_get_field_type(unsigned int offset)
{
	return rd_bits(offset, VDIN_COM_STATUS0, 0, 1);
}

int vdin_vsync_reset_mif(int index)
{
	int i;
#if 0
	int start_line = aml_read_vcbus(VDIN_LCNT_STATUS) & 0xfff;
#endif
	if (!enable_reset)
		return 0;
	if (index == 0) {
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, 25, 1); /* vdin->vdin mif wr en */
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, 29, 1); /* clock gate */
		/* wr req en */
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2, vpu_reg_27af &
						(~(1 << VDIN0_REQ_EN_BIT)));
		vpu_reg_27af &= (~VDIN0_REQ_EN_BIT);
		if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2, VDIN_DET_IDLE_BIT,
				VDIN_DET_IDLE_WIDTH) & VDIN0_IDLE_MASK) {
			vdin0_wr_mif_reset();
		} else {
			for (i = 0; i < vdin_det_idle_wait; i++) {
				if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2,
					VDIN_DET_IDLE_BIT, VDIN_DET_IDLE_WIDTH)
							& VDIN0_IDLE_MASK) {
					vdin0_wr_mif_reset();
					break;
				}
			}
			if (i >= vdin_det_idle_wait)
				pr_info("============== !!! idle wait timeout\n");
		}

		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af | (1 << VDIN0_REQ_EN_BIT));

		W_VCBUS_BIT(VDIN_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, 29, 1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, 25, 1);

		vpu_reg_27af |= VDIN0_REQ_EN_BIT;
	} else if (index == 1) {
		W_VCBUS_BIT(VDIN1_WR_CTRL2, 1, 8, 1); /* vdin->vdin mif wr en */
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, 29, 1); /* clock gate */
		/* wr req en */
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af & (~(1 << VDIN1_REQ_EN_BIT)));
		vpu_reg_27af &= (~(1 << VDIN1_REQ_EN_BIT));
		if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2, VDIN_DET_IDLE_BIT,
				VDIN_DET_IDLE_WIDTH) & VDIN1_IDLE_MASK) {
			vdin1_wr_mif_reset();
		} else {
			for (i = 0; i < vdin_det_idle_wait; i++) {
				if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2,
					VDIN_DET_IDLE_BIT, VDIN_DET_IDLE_WIDTH)
							& VDIN1_IDLE_MASK) {
					vdin1_wr_mif_reset();
					break;
				}
			}
		}
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af | (1 << VDIN1_REQ_EN_BIT));
		vpu_reg_27af |= (1 << VDIN1_REQ_EN_BIT);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, 29, 1);
		W_VCBUS_BIT(VDIN1_WR_CTRL2, 0, 8, 1);
	}
#if 0 /* TODO: if start or end line > 0, should drop this frame! */
	if ((aml_read_vcbus(VDIN_LCNT_STATUS) & 0xfff) > 0) {
		pr_info("============== !!! line counter = %d -> %d !!!\n",
		start_line, aml_read_vcbus(VDIN_LCNT_STATUS) & 0xff);
	}
#endif
	return vsync_reset_mask & 0x08;
}

void vdin_enable_module(unsigned int offset, bool enable)
{
	if (enable)	{
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */
		/* vdin_hw_enable(offset); */
		/* todo: check them */
	} else {
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000000); */
		vdin_hw_disable(offset);
	}
}
#if 0
bool vdin_write_done_check(unsigned int offset, struct vdin_dev_s *devp)
{

	if (rd_bits(offset, VDIN_COM_STATUS0,
			DIRECT_DONE_STATUS_BIT, DIRECT_DONE_STATUS_WID)) {
		wr_bits(offset, VDIN_WR_CTRL, 1,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		wr_bits(offset, VDIN_WR_CTRL, 0,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		devp->abnormal_cnt = 0;
	} else if ((vdin_get_active_v(offset) >= devp->v_active) &&
			(vdin_get_active_h(offset) >= devp->h_active)
		)
		devp->abnormal_cnt++;
	else
		devp->abnormal_cnt = 0;

	if (devp->abnormal_cnt > max_undone_cnt) {
		devp->abnormal_cnt = 0;
		devp->flags |= VDIN_FLAG_FORCE_UNSTABLE;
	}
	/* check the event */
	if (get_foreign_affairs(FOREIGN_AFFAIRS_02)) {
		/* clean the event flag */
		rst_foreign_affairs(FOREIGN_AFFAIRS_02);
		/* notify api to stop vdin */
		devp->flags |= VDIN_FLAG_FORCE_UNSTABLE;
	}
}
#else
bool vdin_write_done_check(unsigned int offset, struct vdin_dev_s *devp)
{

	if (rd_bits(offset, VDIN_COM_STATUS0,
			DIRECT_DONE_STATUS_BIT, DIRECT_DONE_STATUS_WID)) {
		wr_bits(offset, VDIN_WR_CTRL, 1,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		wr_bits(offset, VDIN_WR_CTRL, 0,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		return true;
	}
	devp->abnormal_cnt++;
	return false;
}

#endif
/* check invalid vs to avoid screen flicker */
bool vdin_check_vs(struct vdin_dev_s *devp)
{
	bool ret = false;
	unsigned int dh = 0, dv = 0;

	if ((devp->parm.port < TVIN_PORT_COMP0) ||
		(devp->parm.port > TVIN_PORT_COMP7))
		return ret;

	/* check vs after n*vs avoid unstable signal after TVIN_IOC_START_DEC*/
	if (devp->vs_cnt_valid++ >= VDIN_WAIT_VALID_VS)
		devp->vs_cnt_valid = VDIN_WAIT_VALID_VS;

	/* check hcnt64/cycle to find format changed */
	if (devp->hcnt64 < devp->hcnt64_tag)
		dh = devp->hcnt64_tag - devp->hcnt64;
	else
		dh = devp->hcnt64 - devp->hcnt64_tag;
	if (devp->cycle < devp->cycle_tag)
		dv = devp->cycle_tag - devp->cycle;
	else
		dv = devp->cycle - devp->cycle_tag;
	if ((dh > VDIN_MEAS_HSCNT_DIFF) || (dv > VDIN_MEAS_VSCNT_DIFF)) {
		devp->hcnt64_tag = devp->hcnt64;
		devp->cycle_tag  = devp->cycle;
		if (devp->vs_cnt_valid >= VDIN_WAIT_VALID_VS)
			devp->vs_cnt_ignore = VDIN_IGNORE_VS_CNT;
	}

	/* Do not send data of format changed to video buffer */
	if (devp->vs_cnt_ignore) {
		devp->vs_cnt_ignore--;
		ret = true;
	}

	return ret;
}
/*
 * cycle = delta_stamp = ((1/fps)/(1/msr_clk))*(vsync_span+1)
 * msr_clk/fps unit is HZ
 * vsync_span(0x125a[11:4]) usually should be 0;
 */
bool vdin_check_cycle(struct vdin_dev_s *devp)
{
	unsigned int stamp, cycle;

	stamp = vdin_get_meas_vstamp(devp->addr_offset);
	if (stamp < devp->stamp)
		cycle = 0xffffffff - devp->stamp + stamp + 1;
	else
		cycle = stamp - devp->stamp;

	if (cycle <= (devp->msr_clk_val/1000))
		return true;
	devp->stamp = stamp;
	devp->cycle  = cycle;
	return false;
}
void vdin_calculate_duration(struct vdin_dev_s *devp)
{
	unsigned int last_field_type, cycle_phase;
	struct vframe_s *curr_wr_vf = NULL;
	/* enum tvin_sig_fmt_e fmt = devp->parm.info.fmt; */
	const struct tvin_format_s *fmt_info = devp->fmt_info_p;
	/* unsigned int frame_rate =
	 * (VDIN_CRYSTAL + (devp->cycle>>3))/devp->cycle;
	 */
	enum tvin_port_e port = devp->parm.port;

	curr_wr_vf = &devp->curr_wr_vfe->vf;
	last_field_type = devp->curr_field_type;
	cycle_phase = devp->msr_clk_val/96000;
	if (cycle_phase == 0) {
		cycle_phase = 250;
		pr_info("%s:cycle_phase is 0!!!!", __func__);
	}
#ifdef VDIN_DYNAMIC_DURATION
	devp->curr_wr_vf->duration =
		(devp->cycle + cycle_phase/2) / cycle_phase;
#else
	if ((use_frame_rate == 1) &&
		((port >= TVIN_PORT_HDMI0) && (port <= TVIN_PORT_HDMI7))) {
		curr_wr_vf->duration =
			(devp->cycle + cycle_phase/2) / cycle_phase;
	} else {
		if (!fmt_info->duration)
			curr_wr_vf->duration =
				(devp->cycle + cycle_phase/2) / cycle_phase;
	}
#endif
	/* for 2D->3D mode & interlaced format, double top field
	 * duration to match software frame lock
	 */
#ifdef VDIN_DYNAMIC_DURATION
	if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
			(last_field_type & VIDTYPE_INTERLACE))
		curr_wr_vf->duration <<= 1;
#else
	if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
			(last_field_type & VIDTYPE_INTERLACE)) {
		if (!fmt_info->duration)
			curr_wr_vf->duration = ((devp->cycle << 1) +
				cycle_phase/2) / cycle_phase;
		else
			curr_wr_vf->duration = fmt_info->duration << 1;
	}
#endif
}
void vdin_output_ctl(unsigned int offset, unsigned int output_nr_flag)
{
	wr_bits(offset, VDIN_WR_CTRL, 1, VCP_IN_EN_BIT, VCP_IN_EN_WID);
	if (output_nr_flag) {
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	} else {
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	}
}

/*
 *just for horizontal down scale src_w is origin width,
 *just dst_w is width after scale down
 */
void vdin_set_hscale(unsigned int offset, unsigned int src_w,
		unsigned int dst_w)
{

	unsigned int filt_coef0[] =  { /* bicubic */
		0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300, 0xfd7e0500,
		0xfc7e0600,	0xfb7d0800, 0xfb7c0900, 0xfa7b0b00, 0xfa7a0dff,
		0xf9790fff, 0xf97711ff,	0xf87613ff, 0xf87416fe, 0xf87218fe,
		0xf8701afe, 0xf76f1dfd, 0xf76d1ffd,	0xf76b21fd, 0xf76824fd,
		0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,	0xf75d31fb,
		0xf75a34fb, 0xf75837fa, 0xf7553afa, 0xf8523cfa, 0xf8503ff9,
		0xf84d42f9, 0xf84a45f9, 0xf84848f8
	};
/*
 *	unsigned int filt_coef1[] =  {//2 point bilinear
 *		0x00800000, 0x007e0200, 0x007c0400, 0x007a0600, 0x00780800,
 *		0x00760a00,	0x00740c00, 0x00720e00, 0x00701000, 0x006e1200,
 *		 0x006c1400, 0x006a1600,0x00681800, 0x00661a00, 0x00641c00,
 *		 0x00621e00, 0x00602000, 0x005e2200,0x005c2400, 0x005a2600,
 *		 0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,0x00503000,
 *		 0x004e3200, 0x004c3400, 0x004a3600, 0x00483800, 0x00463a00,
 *		0x00443c00, 0x00423e00, 0x00404000
 *	};
 *
 *	unsigned int filt_coef2[] = { //2 point bilinear, bank_length == 2
 *		0x80000000, 0x7e020000, 0x7c040000, 0x7a060000, 0x78080000,
 *		0x760a0000,	0x740c0000, 0x720e0000, 0x70100000, 0x6e120000,
 *		0x6c140000, 0x6a160000,	0x68180000, 0x661a0000, 0x641c0000,
 *		0x621e0000, 0x60200000, 0x5e220000,	0x5c240000, 0x5a260000,
 *		 0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,0x50300000,
 *		  0x4e320000, 0x4c340000, 0x4a360000, 0x48380000, 0x463a0000,
 *		0x443c0000, 0x423e0000, 0x40400000
 *	};
 */
	int horz_phase_step, i;

	if (!dst_w) {
		pr_err("[vdin..]%s parameter dst_w error.\n", __func__);
		return;
	}
	/* disable hscale&pre hscale */
	wr_bits(offset, VDIN_SC_MISC_CTRL, 0, PRE_HSCL_EN_BIT, PRE_HSCL_EN_WID);
	wr_bits(offset, VDIN_SC_MISC_CTRL, 0, HSCL_EN_BIT, HSCL_EN_WID);
	/* write horz filter coefs */
	wr(offset, VDIN_SCALE_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++)
		wr(offset, VDIN_SCALE_COEF, filt_coef0[i]); /* bicubic */

	if (src_w >> 12) {/* for src_w >= 4096, avoid data overflow. */
		horz_phase_step = (src_w << 18) / dst_w;
		horz_phase_step = (horz_phase_step << 6);
	} else {
		horz_phase_step = (src_w << 20) / dst_w;
		horz_phase_step = (horz_phase_step << 4);
	}

	wr(offset, VDIN_WIDTHM1I_WIDTHM1O, ((src_w - 1) << WIDTHM1I_BIT) |
			(dst_w  - 1)
		      );

	wr(offset, VDIN_HSC_PHASE_STEP, horz_phase_step);
	/* hsc_p0_num */
	wr(offset, VDIN_HSC_INI_CTRL, (1 << HSCL_RPT_P0_NUM_BIT) |
			(4 << HSCL_INI_RCV_NUM_BIT) |/* hsc_ini_rcv_num */
			(0 << HSCL_INI_PHASE_BIT)/* hsc_ini_phase */
		      );

	wr(offset, VDIN_SC_MISC_CTRL,
			(0 << INIT_PIX_IN_PTR_BIT) |
			(0 << PRE_HSCL_EN_BIT) |/* pre_hscale_en */
			(1 << HSCL_EN_BIT) |/* hsc_en */
			(1 << SHORT_LN_OUT_EN_BIT) |/* short_lineo_en */
			(1 << HSCL_NEAREST_EN_BIT) |/* nearest_en */
			(0 << PHASE0_ALWAYS_EN_BIT) |/* phase0_always_en */
			(4 << HSCL_BANK_LEN_BIT)/* hsc_bank_length */
		      );
}

/*
 *just for veritical scale src_w is origin height,
 *just dst_h is the height after scale
 */
void vdin_set_vscale(unsigned int offset, unsigned int src_h,
		unsigned int dst_h)
{
	int veri_phase_step, tmp;

	if (!dst_h) {
		pr_err("[vdin..]%s parameter dst_h error.\n", __func__);
		return;
	}
	/* disable vscale */
	wr_bits(offset, VDIN_VSC_INI_CTRL, 0, VSC_EN_BIT, VSC_EN_WID);

	veri_phase_step = (src_h << 20) / dst_h;
	tmp = veri_phase_step >> 25;
	if (tmp) {
		pr_err("[vdin..]%s error. cannot be divided more than 31.9999.\n",
				__func__);
		return;
	}
	wr(offset, VDIN_VSC_PHASE_STEP, veri_phase_step);
	if (!(veri_phase_step>>20)) {/* scale up the bit should be 1 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 1,
			VSC_PHASE0_ALWAYS_EN_BIT, VSC_PHASE0_ALWAYS_EN_WID);
		/* scale phase is 0 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 0,
				VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	} else {
		wr_bits(offset, VDIN_VSC_INI_CTRL, 0,
			VSC_PHASE0_ALWAYS_EN_BIT, VSC_PHASE0_ALWAYS_EN_WID);
		/* scale phase is 0x8000 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 0x8000,
				VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	}
	/* skip 0 line in the beginning */
	wr_bits(offset, VDIN_VSC_INI_CTRL, 0,
			INI_SKIP_LINE_NUM_BIT, INI_SKIP_LINE_NUM_WID);

	wr(offset, VDIN_SCIN_HEIGHTM1, src_h - 1);
	wr(offset, VDIN_DUMMY_DATA, 0x008080);
	/* enable vscale */
	wr_bits(offset, VDIN_VSC_INI_CTRL, 1, VSC_EN_BIT, VSC_EN_WID);

}

void vdin_set_hvscale(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	if ((devp->prop.scaling4w < devp->h_active) &&
		(devp->prop.scaling4w > 0)) {
		vdin_set_hscale(offset, devp->h_active, devp->prop.scaling4w);
		devp->h_active = devp->prop.scaling4w;
	} else if (devp->h_active > max_hactive) {
		vdin_set_hscale(offset, devp->h_active, max_hactive);
		devp->h_active = max_hactive;
	}
	pr_info("[vdin.%d] dst hactive:%u,", devp->index, devp->h_active);
	if ((devp->prop.scaling4h < devp->v_active) &&
		(devp->prop.scaling4h > 0)) {
		vdin_set_vscale(offset, devp->v_active, devp->prop.scaling4h);
		devp->v_active = devp->prop.scaling4h;
	}
	pr_info(" dst vactive:%u.\n", devp->v_active);
}

void vdin_set_bitdepth(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	switch (devp->color_depth_config) {
	case 8:
		devp->source_bitdepth = 8;
		wr_bits(offset, VDIN_WR_CTRL2, 0,
			VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		break;
	case 10:
		devp->source_bitdepth = 10;
		wr_bits(offset, VDIN_WR_CTRL2, 1,
			VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		break;
	case 0:
		/* vdin_bit_depth is set to 0 by defaut, in this case,
		 * devp->source_bitdepth is controlled by colordepth
		 */
		if (devp->prop.colordepth == 8) {
			devp->source_bitdepth = 8;
			wr_bits(offset, VDIN_WR_CTRL2, 0,
				VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		} else if ((devp->prop.colordepth == 10) &&
		(devp->color_depth_support & VDIN_WR_COLOR_DEPTH_10BIT)) {
			devp->source_bitdepth = 10;
			wr_bits(offset, VDIN_WR_CTRL2, 1,
				VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		} else {
			devp->source_bitdepth = 8;
			wr_bits(offset, VDIN_WR_CTRL2, 0,
				VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		}
		break;
	default:
		devp->source_bitdepth = 8;
		wr_bits(offset, VDIN_WR_CTRL2, 0,
			VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
		break;
	}
}

void vdin_wr_reverse(unsigned int offset, bool hreverse, bool vreverse)
{
	if (hreverse)
		wr_bits(offset, VDIN_WR_H_START_END, 1,
				HORIZONTAL_REVERSE_BIT, HORIZONTAL_REVERSE_WID);
	else
		wr_bits(offset, VDIN_WR_H_START_END, 0,
				HORIZONTAL_REVERSE_BIT, HORIZONTAL_REVERSE_WID);
	if (vreverse)
		wr_bits(offset, VDIN_WR_V_START_END, 1,
				VERTICAL_REVERSE_BIT, VERTICAL_REVERSE_WID);
	else
		wr_bits(offset, VDIN_WR_V_START_END, 0,
				VERTICAL_REVERSE_BIT, VERTICAL_REVERSE_WID);
}

void vdin_bypass_isp(unsigned int offset)
{
	wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
			VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
			VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
}

void vdin_set_cm2(unsigned int offset, unsigned int w,
		unsigned int h, unsigned int *cm2)
{
	unsigned int i = 0, j = 0, start_addr = 0x100;

	if (!cm_enable)
		return;
	wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	for (i = 0; i < 160; i++) {
		j = i/5;
		wr(offset, VDIN_CHROMA_ADDR_PORT, start_addr+(j<<3)+(i%5));
		wr(offset, VDIN_CHROMA_DATA_PORT, cm2[i]);
	}
	for (i = 0; i < 28; i++) {
		wr(offset, VDIN_CHROMA_ADDR_PORT, 0x200+i);
		wr(offset, VDIN_CHROMA_DATA_PORT, cm2[160+i]);
	}
	/*config cm2 frame size*/
	wr(offset, VDIN_CHROMA_ADDR_PORT, 0x205);
	wr(offset, VDIN_CHROMA_DATA_PORT, h<<16|w);
	wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 1, CM_TOP_EN_BIT, CM_TOP_EN_WID);
}

void vdin0_output_ctl(unsigned int offset, unsigned int output_nr_flag)
{
	wr_bits(offset, VDIN_WR_CTRL, 1, VCP_IN_EN_BIT, VCP_IN_EN_WID);
	if (output_nr_flag) {
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	} else {
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	}
}
void vdin_set_mpegin(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
	/* if (is_meson_gxbb_cpu()) */
	/* ; */
	/* else */
	/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */

	wr(offset, VDIN_COM_CTRL0, 0x80000911);
	vdin_delay_line(delay_line_num, offset);
	wr(offset, VDIN_COM_GCLK_CTRL, 0x0);

	wr(offset, VDIN_INTF_WIDTHM1, devp->h_active-1);
	wr(offset, VDIN_WR_CTRL2, 0x0);

	wr(offset, VDIN_HIST_CTRL, 0x3);
	wr(offset, VDIN_HIST_H_START_END, devp->h_active-1);
	wr(offset, VDIN_HIST_V_START_END, devp->v_active-1);
	vdin0_output_ctl(offset, 1);
}

void vdin_force_gofiled(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	wr_bits(offset, VDIN_COM_CTRL0, 1, 28, 1);
	wr_bits(offset, VDIN_COM_CTRL0, 0, 28, 1);
}

