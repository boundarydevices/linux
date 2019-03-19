/*
 * drivers/amlogic/media/common/vpu/vpu_ctrl.h
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

#ifndef __VPU_CTRL_H__
#define __VPU_CTRL_H__
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

/* #define LIMIT_VPU_CLK_LOW */

/* ************************************************ */
/* VPU frequency table, important. DO NOT modify!! */
/* ************************************************ */

#define CLK_FPLL_FREQ          2000 /* MHz */

/* GXBB */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXBB     7
#define CLK_LEVEL_MAX_GXBB     8

/* GXTVBB */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXTVBB   7
#define CLK_LEVEL_MAX_GXTVBB   8

/* GXL */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXL      7
#define CLK_LEVEL_MAX_GXL      8

/* GXM */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXM      7
#define CLK_LEVEL_MAX_GXM      8

/* TXLX */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_TXLX     7
#define CLK_LEVEL_MAX_TXLX     8

/* AXG */
/* freq max=250M, default=250M */
#define CLK_LEVEL_DFT_AXG      3
#define CLK_LEVEL_MAX_AXG      4

/* G12A */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_G12A     7
#define CLK_LEVEL_MAX_G12A     8

/* vpu clk setting */
enum vpu_mux_e {
	FCLK_DIV4 = 0,
	FCLK_DIV3,
	FCLK_DIV5,
	FCLK_DIV7,
	MPLL_CLK1,
	VID_PLL_CLK,
	VID2_PLL_CLK,
	GPLL_CLK,
	FCLK_DIV_MAX,
};

static struct fclk_div_s fclk_div_table_gxb[] = {
	/* id,         mux,  div */
	{FCLK_DIV4,    0,    4},
	{FCLK_DIV3,    1,    3},
	{FCLK_DIV5,    2,    5},
	{FCLK_DIV7,    3,    7},
	{FCLK_DIV_MAX, 8,    1},
};

static struct fclk_div_s fclk_div_table_g12a[] = {
	/* id,         mux,  div */
	{FCLK_DIV3,    0,    3},
	{FCLK_DIV4,    1,    4},
	{FCLK_DIV5,    2,    5},
	{FCLK_DIV7,    3,    7},
	{FCLK_DIV_MAX, 8,    1},
};

#define VPU_CLK_TOLERANCE    1000000 /* Hz */
static struct vpu_clk_s vpu_clk_table[] = {
	/* frequency   clk_mux       div */
	{100000000,    FCLK_DIV5,    3}, /* 0 */
	{166666667,    FCLK_DIV3,    3}, /* 1 */
	{200000000,    FCLK_DIV5,    1}, /* 2 */
	{250000000,    FCLK_DIV4,    1}, /* 3 */
	{333333333,    FCLK_DIV3,    1}, /* 4 */
	{400000000,    FCLK_DIV5,    0}, /* 5 */
	{500000000,    FCLK_DIV4,    0}, /* 6 */
	{666666667,    FCLK_DIV3,    0}, /* 7 */
	{696000000,    GPLL_CLK,     0}, /* 8 */
	{850000000,    GPLL_CLK,     0}, /* 9 */ /* invalid */
};

/* ************************************************ */


/* ******************************************************* */
/*              VPU memory power down table                */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_mem_pd_gxb[] = {
	/* vpu module,      reg,                  val,  bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3,  0,   2},
	{VPU_VIU_OSD2,      HHI_VPU_MEM_PD_REG0,  0x3,  2,   2},
	{VPU_VIU_VD1,       HHI_VPU_MEM_PD_REG0,  0x3,  4,   2},
	{VPU_VIU_VD2,       HHI_VPU_MEM_PD_REG0,  0x3,  6,   2},
	{VPU_VIU_CHROMA,    HHI_VPU_MEM_PD_REG0,  0x3,  8,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 10,   2},
	{VPU_VIU_SCALE,     HHI_VPU_MEM_PD_REG0,  0x3, 12,   2},
	{VPU_VIU_OSD_SCALE, HHI_VPU_MEM_PD_REG0,  0x3, 14,   2},
	{VPU_VIU_VDIN0,     HHI_VPU_MEM_PD_REG0,  0x3, 16,   2},
	{VPU_VIU_VDIN1,     HHI_VPU_MEM_PD_REG0,  0x3, 18,   2},
	{VPU_VIU_SRSCL,     HHI_VPU_MEM_PD_REG0,  0x3, 20,   2},
	{VPU_VIU_OSDSR,     HHI_VPU_MEM_PD_REG0,  0x3, 22,   2},
	{VPU_DI_PRE,        HHI_VPU_MEM_PD_REG0,  0x3, 26,   2},
	{VPU_DI_POST,       HHI_VPU_MEM_PD_REG0,  0x3, 28,   2},
	{VPU_SHARP,         HHI_VPU_MEM_PD_REG0,  0x3, 30,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG1,  0x3, 14,   2},
	{VPU_AFBC_DEC,      HHI_VPU_MEM_PD_REG1,  0x3, 16,   2},
	{VPU_VENCP,         HHI_VPU_MEM_PD_REG1,  0x3, 20,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG1,  0x3, 22,   2},
	{VPU_VENCI,         HHI_VPU_MEM_PD_REG1,  0x3, 24,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_gxtvbb[] = {
	/* vpu module,      reg,                  val,  bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3,  0,   2},
	{VPU_VIU_OSD2,      HHI_VPU_MEM_PD_REG0,  0x3,  2,   2},
	{VPU_VIU_VD1,       HHI_VPU_MEM_PD_REG0,  0x3,  4,   2},
	{VPU_VIU_VD2,       HHI_VPU_MEM_PD_REG0,  0x3,  6,   2},
	{VPU_VIU_CHROMA,    HHI_VPU_MEM_PD_REG0,  0x3,  8,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 10,   2},
	{VPU_VIU_SCALE,     HHI_VPU_MEM_PD_REG0,  0x3, 12,   2},
	{VPU_VIU_OSD_SCALE, HHI_VPU_MEM_PD_REG0,  0x3, 14,   2},
	{VPU_VIU_VDIN0,     HHI_VPU_MEM_PD_REG0,  0x3, 16,   2},
	{VPU_VIU_VDIN1,     HHI_VPU_MEM_PD_REG0,  0x3, 18,   2},
	{VPU_VIU_SRSCL,     HHI_VPU_MEM_PD_REG0,  0x3, 20,   2},
	{VPU_AFBC_DEC1,     HHI_VPU_MEM_PD_REG0,  0x3, 22,   2},
	{VPU_DI_PRE,        HHI_VPU_MEM_PD_REG0,  0x3, 26,   2},
	{VPU_DI_POST,       HHI_VPU_MEM_PD_REG0,  0x3, 28,   2},
	{VPU_SHARP,         HHI_VPU_MEM_PD_REG0,  0x3, 30,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG1,  0x3, 14,   2},
	{VPU_AFBC_DEC,      HHI_VPU_MEM_PD_REG1,  0x3, 16,   2},
	{VPU_VENCP,         HHI_VPU_MEM_PD_REG1,  0x3, 20,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG1,  0x3, 22,   2},
	{VPU_VENCI,         HHI_VPU_MEM_PD_REG1,  0x3, 24,   2},
	{VPU_LDIM_STTS,     HHI_VPU_MEM_PD_REG1,  0x3, 28,   2},
	{VPU_XVYCC_LUT,     HHI_VPU_MEM_PD_REG1,  0x3, 30,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_gxl[] = {
	/* vpu module,      reg,                  val,  bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3,  0,   2},
	{VPU_VIU_OSD2,      HHI_VPU_MEM_PD_REG0,  0x3,  2,   2},
	{VPU_VIU_VD1,       HHI_VPU_MEM_PD_REG0,  0x3,  4,   2},
	{VPU_VIU_VD2,       HHI_VPU_MEM_PD_REG0,  0x3,  6,   2},
	{VPU_VIU_CHROMA,    HHI_VPU_MEM_PD_REG0,  0x3,  8,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 10,   2},
	{VPU_VIU_SCALE,     HHI_VPU_MEM_PD_REG0,  0x3, 12,   2},
	{VPU_VIU_OSD_SCALE, HHI_VPU_MEM_PD_REG0,  0x3, 14,   2},
	{VPU_VIU_VDIN0,     HHI_VPU_MEM_PD_REG0,  0x3, 16,   2},
	{VPU_VIU_VDIN1,     HHI_VPU_MEM_PD_REG0,  0x3, 18,   2},
	{VPU_DI_PRE,        HHI_VPU_MEM_PD_REG0,  0x3, 26,   2},
	{VPU_DI_POST,       HHI_VPU_MEM_PD_REG0,  0x3, 28,   2},
	{VPU_SHARP,         HHI_VPU_MEM_PD_REG0,  0x3, 30,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG1,  0x3, 14,   2},
	{VPU_AFBC_DEC,      HHI_VPU_MEM_PD_REG1,  0x3, 16,   2},
	{VPU_VENCP,         HHI_VPU_MEM_PD_REG1,  0x3, 20,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG1,  0x3, 22,   2},
	{VPU_VENCI,         HHI_VPU_MEM_PD_REG1,  0x3, 24,   2},
	{VPU_VIU_WM,        HHI_VPU_MEM_PD_REG2,  0x3,  0,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_txl[] = {
	/* vpu module,      reg,                  val,  bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3,  0,   2},
	{VPU_VIU_OSD2,      HHI_VPU_MEM_PD_REG0,  0x3,  2,   2},
	{VPU_VIU_VD1,       HHI_VPU_MEM_PD_REG0,  0x3,  4,   2},
	{VPU_VIU_VD2,       HHI_VPU_MEM_PD_REG0,  0x3,  6,   2},
	{VPU_VIU_CHROMA,    HHI_VPU_MEM_PD_REG0,  0x3,  8,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 10,   2},
	{VPU_VIU_SCALE,     HHI_VPU_MEM_PD_REG0,  0x3, 12,   2},
	{VPU_VIU_OSD_SCALE, HHI_VPU_MEM_PD_REG0,  0x3, 14,   2},
	{VPU_VIU_VDIN0,     HHI_VPU_MEM_PD_REG0,  0x3, 16,   2},
	{VPU_VIU_VDIN1,     HHI_VPU_MEM_PD_REG0,  0x3, 18,   2},
	{VPU_DI_PRE,        HHI_VPU_MEM_PD_REG0,  0x3, 26,   2},
	{VPU_DI_POST,       HHI_VPU_MEM_PD_REG0,  0x3, 28,   2},
	{VPU_SHARP,         HHI_VPU_MEM_PD_REG0,  0x3, 30,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG1,  0x3, 14,   2},
	{VPU_AFBC_DEC,      HHI_VPU_MEM_PD_REG1,  0x3, 16,   2},
	{VPU_VENCP,         HHI_VPU_MEM_PD_REG1,  0x3, 20,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG1,  0x3, 22,   2},
	{VPU_VENCI,         HHI_VPU_MEM_PD_REG1,  0x3, 24,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_txlx[] = {
	/* vpu module,      reg,                  val,  bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3,  0,   2},
	{VPU_VIU_OSD2,      HHI_VPU_MEM_PD_REG0,  0x3,  2,   2},
	{VPU_VIU_VD1,       HHI_VPU_MEM_PD_REG0,  0x3,  4,   2},
	{VPU_VIU_VD2,       HHI_VPU_MEM_PD_REG0,  0x3,  6,   2},
	{VPU_VIU_CHROMA,    HHI_VPU_MEM_PD_REG0,  0x3,  8,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 10,   2},
	{VPU_VIU_SCALE,     HHI_VPU_MEM_PD_REG0,  0x3, 12,   2},
	{VPU_VIU_OSD_SCALE, HHI_VPU_MEM_PD_REG0,  0x3, 14,   2},
	{VPU_VIU_VDIN0,     HHI_VPU_MEM_PD_REG0,  0x3, 16,   2},
	{VPU_VIU_VDIN1,     HHI_VPU_MEM_PD_REG0,  0x3, 18,   2},
	{VPU_VIU_SRSCL,     HHI_VPU_MEM_PD_REG0,  0x3, 20,   2},
	{VPU_AFBC_DEC1,     HHI_VPU_MEM_PD_REG0,  0x3, 22,   2},
	{VPU_DI_PRE,        HHI_VPU_MEM_PD_REG0,  0x3, 26,   2},
	{VPU_DI_POST,       HHI_VPU_MEM_PD_REG0,  0x3, 28,   2},
	{VPU_SHARP,         HHI_VPU_MEM_PD_REG0,  0x3, 30,   2},
	{VPU_VKSTONE,       HHI_VPU_MEM_PD_REG1,  0x3,  4,   2},
	{VPU_DOLBY_CORE3,   HHI_VPU_MEM_PD_REG1,  0x3,  6,   2},
	{VPU_DOLBY0,        HHI_VPU_MEM_PD_REG1,  0x3,  8,   2},
	{VPU_DOLBY1A,       HHI_VPU_MEM_PD_REG1,  0x3, 10,   2},
	{VPU_DOLBY1B,       HHI_VPU_MEM_PD_REG1,  0x3, 12,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG1,  0x3, 14,   2},
	{VPU_AFBC_DEC,      HHI_VPU_MEM_PD_REG1,  0x3, 16,   2},
	{VPU_OSD_AFBCD,     HHI_VPU_MEM_PD_REG1,  0x3, 18,   2},
	{VPU_VENCP,         HHI_VPU_MEM_PD_REG1,  0x3, 20,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG1,  0x3, 22,   2},
	{VPU_VENCI,         HHI_VPU_MEM_PD_REG1,  0x3, 24,   2},
	{VPU_LDIM_STTS,     HHI_VPU_MEM_PD_REG1,  0x3, 28,   2},
	{VPU_XVYCC_LUT,     HHI_VPU_MEM_PD_REG1,  0x3, 30,   2},
	{VPU_VIU_WM,        HHI_VPU_MEM_PD_REG2,  0x3,  0,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_axg[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VIU_OSD1,      HHI_VPU_MEM_PD_REG0,  0x3, 0,   2},
	{VPU_VIU_OFIFO,     HHI_VPU_MEM_PD_REG0,  0x3, 2,   2},
	{VPU_VPU_ARB,       HHI_VPU_MEM_PD_REG0,  0x3, 4,   2},
	{VPU_VENCL,         HHI_VPU_MEM_PD_REG0,  0x3, 6,   2},
	{VPU_MOD_MAX,       VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_g12a[] = {
	/* vpu module,      reg,                   val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_g12b[] = {
	/* vpu module,      reg,                   val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_tl1[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2_OSD1,       HHI_VPU_MEM_PD_REG1, 0x3,  0,   2},
	{VPU_VIU2_OFIFO,      HHI_VPU_MEM_PD_REG1, 0x3,  2,   2},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LS_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2_OSD_ROT,    HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0xf, 24,   4},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4, 0x3,  2,   2},
	{VPU_AFBCE,           HHI_VPU_MEM_PD_REG4, 0x3,  4,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

static struct vpu_ctrl_s vpu_mem_pd_sm1[] = {
	/* vpu module,        reg,                 val,  bit, len */
	{VPU_VIU_OSD1,        HHI_VPU_MEM_PD_REG0, 0x3,  0,   2},
	{VPU_VIU_OSD2,        HHI_VPU_MEM_PD_REG0, 0x3,  2,   2},
	{VPU_VIU_VD1,         HHI_VPU_MEM_PD_REG0, 0x3,  4,   2},
	{VPU_VIU_VD2,         HHI_VPU_MEM_PD_REG0, 0x3,  6,   2},
	{VPU_VIU_CHROMA,      HHI_VPU_MEM_PD_REG0, 0x3,  8,   2},
	{VPU_VIU_OFIFO,       HHI_VPU_MEM_PD_REG0, 0x3, 10,   2},
	{VPU_VIU_SCALE,       HHI_VPU_MEM_PD_REG0, 0x3, 12,   2},
	{VPU_VIU_OSD_SCALE,   HHI_VPU_MEM_PD_REG0, 0x3, 14,   2},
	{VPU_VIU_VDIN0,       HHI_VPU_MEM_PD_REG0, 0x3, 16,   2},
	{VPU_VIU_VDIN1,       HHI_VPU_MEM_PD_REG0, 0x3, 18,   2},
	{VPU_VIU_SRSCL,       HHI_VPU_MEM_PD_REG0, 0x3, 20,   2},
	{VPU_AFBC_DEC1,       HHI_VPU_MEM_PD_REG0, 0x3, 22,   2},
	{VPU_VIU_DI_SCALE,    HHI_VPU_MEM_PD_REG0, 0x3, 24,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG0, 0x3, 26,   2},
	{VPU_DI_POST,         HHI_VPU_MEM_PD_REG0, 0x3, 28,   2},
	{VPU_SHARP,           HHI_VPU_MEM_PD_REG0, 0x3, 30,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG1, 0xf,  0,   4},
	{VPU_VKSTONE,         HHI_VPU_MEM_PD_REG1, 0x3,  4,   2},
	{VPU_DOLBY_CORE3,     HHI_VPU_MEM_PD_REG1, 0x3,  6,   2},
	{VPU_DOLBY0,          HHI_VPU_MEM_PD_REG1, 0x3,  8,   2},
	{VPU_DOLBY1A,         HHI_VPU_MEM_PD_REG1, 0x3, 10,   2},
	{VPU_DOLBY1B,         HHI_VPU_MEM_PD_REG1, 0x3, 12,   2},
	{VPU_VPU_ARB,         HHI_VPU_MEM_PD_REG1, 0x3, 14,   2},
	{VPU_AFBC_DEC,        HHI_VPU_MEM_PD_REG1, 0x3, 16,   2},
	{VPU_VD2_SCALE,       HHI_VPU_MEM_PD_REG1, 0x3, 18,   2},
	{VPU_VENCP,           HHI_VPU_MEM_PD_REG1, 0x3, 20,   2},
	{VPU_VENCL,           HHI_VPU_MEM_PD_REG1, 0x3, 22,   2},
	{VPU_VENCI,           HHI_VPU_MEM_PD_REG1, 0x3, 24,   2},
	{VPU_LS_STTS,         HHI_VPU_MEM_PD_REG1, 0x3, 26,   2},
	{VPU_LDIM_STTS,       HHI_VPU_MEM_PD_REG1, 0x3, 28,   2},
	{VPU_VD2_OSD2_SCALE,  HHI_VPU_MEM_PD_REG1, 0x3, 30,   2},
	{VPU_VIU_WM,          HHI_VPU_MEM_PD_REG2, 0x3,  0,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG2, 0x3,  2,   2},
	{VPU_VIU_OSD3,        HHI_VPU_MEM_PD_REG2, 0x3,  4,   2},
	{VPU_VIU_OSD4,        HHI_VPU_MEM_PD_REG2, 0x3,  6,   2},
	{VPU_MAIL_AFBCD,      HHI_VPU_MEM_PD_REG2, 0x3,  8,   2},
	{VPU_VD1_SCALE,       HHI_VPU_MEM_PD_REG2, 0x3, 10,   2},
	{VPU_OSD_BLD34,       HHI_VPU_MEM_PD_REG2, 0x3, 12,   2},
	{VPU_PRIME_DOLBY_RAM, HHI_VPU_MEM_PD_REG2, 0x3, 14,   2},
	{VPU_VD2_OFIFO,       HHI_VPU_MEM_PD_REG2, 0x3, 16,   2},
	{VPU_DS,              HHI_VPU_MEM_PD_REG2, 0x3, 18,   2},
	{VPU_LUT3D,           HHI_VPU_MEM_PD_REG2, 0x3, 20,   2},
	{VPU_VIU2,            HHI_VPU_MEM_PD_REG2, 0x3, 22,   2},
	{VPU_DI_PRE,          HHI_VPU_MEM_PD_REG2, 0xf, 24,   4},
	{VPU_RDMA,            HHI_VPU_MEM_PD_REG2, 0x3, 30,   2},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3_SM1, 0x3,  0,  16},
	{VPU_TCON,            HHI_VPU_MEM_PD_REG3_SM1, 0x3, 16,  16},
	{VPU_AXI_WR1,         HHI_VPU_MEM_PD_REG4_SM1, 0x3,  0,   2},
	{VPU_AXI_WR0,         HHI_VPU_MEM_PD_REG4_SM1, 0x3,  2,   2},
	{VPU_MOD_MAX,         VPU_REG_END,         0,    0,   0},
};

/* ******************************************************* */
/*                 VPU clock gate table                    */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_clk_gate_gxb[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  16,   1},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_DI,            DI_CLKG_CTRL,         1,  26,   3},
	{VPU_DI,            DI_CLKG_CTRL,         1,  24,   1},
	{VPU_DI,            DI_CLKG_CTRL,         1,  17,   4},
	{VPU_DI,            DI_CLKG_CTRL,         1,   0,   2},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,  16,  16},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   6,   8},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,   2},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_clk_gate_gxl[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  16,   2},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_DI,            DI_CLKG_CTRL,         1,  26,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,  24,   1},
	{VPU_DI,            DI_CLKG_CTRL,         1,  17,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,   0,   2},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,  30},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_clk_gate_txl[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  16,   1},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_DI,            DI_CLKG_CTRL,         1,  26,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,  24,   1},
	{VPU_DI,            DI_CLKG_CTRL,         1,  17,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,   0,   2},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,  30},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_clk_gate_txlx[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  16,   1},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,  30},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_clk_gate_axg[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1},
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,  16,  16},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   6,   8},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,   2},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

static struct vpu_ctrl_s vpu_clk_gate_g12a[] = {
	/* vpu module,      reg,                  val, bit, len */
	{VPU_VPU_TOP,       VPU_CLK_GATE,         1,   1,   1}, /*vpu_sys_clk*/
	{VPU_VPU_CLKB,      VPU_CLK_GATE,         1,  18,   1},
	{VPU_CLK_B_REG_LATCH, VPU_CLK_GATE,       1,  17,   1},
	{VPU_CLK_VIB,       VPU_CLK_GATE,         1,  16,   1},
	{VPU_RDMA,          VPU_CLK_GATE,         1,  15,   1}, /*rdma_clk*/
	{VPU_VLOCK,         VPU_CLK_GATE,         1,  14,   1},
	{VPU_MISC,          VPU_CLK_GATE,         1,   6,   1}, /*hs,vs intr*/
	{VPU_VENCP,         VPU_CLK_GATE,         1,   3,   1},
	{VPU_VENCP,         VPU_CLK_GATE,         1,   0,   1},
	{VPU_VENCL,         VPU_CLK_GATE,         1,   4,   2},
	{VPU_VENCI,         VPU_CLK_GATE,         1,  10,   2},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN0,     VDIN0_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,  24,   6},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   4,  18},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL,  1,   1,   1},
	{VPU_VIU_VDIN1,     VDIN1_COM_GCLK_CTRL2, 1,   0,   4},
	{VPU_DI,            DI_CLKG_CTRL,         1,  26,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,  24,   1},
	{VPU_DI,            DI_CLKG_CTRL,         1,  17,   5},
	{VPU_DI,            DI_CLKG_CTRL,         1,   0,   2},
	{VPU_VPP,           VPP_GCLK_CTRL0,       1,   2,  30},
	{VPU_VPP,           VPP_GCLK_CTRL1,       1,   0,  12},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,  18,   8},
	{VPU_VPP,           VPP_SC_GCLK_CTRL,     1,   2,  10},
	{VPU_VPP,           VPP_XVYCC_GCLK_CTRL,  1,   0,  18},
	{VPU_MAX,           VPU_REG_END,          0,   0,   0},
};

/* ******************************************************* */
/*                 VPU_HDMI ISO                            */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_hdmi_iso_gxb[] = {
	/* reg,                val, bit, len */
	{AO_RTI_GEN_PWR_SLEEP0,  1,   9,   1},
	{VPU_REG_END,            0,   0,   0},
};

static struct vpu_ctrl_s vpu_hdmi_iso_sm1[] = {
	/* reg,                val, bit, len */
	{AO_RTI_GEN_PWR_ISO0,    1,   8,   1},
	{VPU_REG_END,            0,   0,   0},
};

/* ******************************************************* */
/*                 VPU module init table                   */
/* ******************************************************* */
static struct vpu_ctrl_s vpu_module_init_gxm[] = {
	/* 0, reg,                       val,        bit, len */
	{0,   DOLBY_CORE1_CLKGATE_CTRL,  0x55555555, 0,   32},
	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 0x55555555, 0,   32},
	{0,   DOLBY_CORE3_CLKGATE_CTRL,  0x55555555, 0,   32},
	{0,   VPU_REG_END,               0,          0,   0},
};

static struct vpu_ctrl_s vpu_module_init_txlx[] = {
	/* 0, reg,                       val, bit, len */
	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   10,  2},
	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   2,   2},
	{0,   DOLBY_TV_CLKGATE_CTRL,     1,   4,   2},
	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   10,  2},
	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   2,   2},
	{0,   DOLBY_CORE2A_CLKGATE_CTRL, 1,   4,   2},
	{0,   DOLBY_CORE3_CLKGATE_CTRL,  0,   1,   1},
	{0,   DOLBY_CORE3_CLKGATE_CTRL,  1,   2,   2},
	{0,   VPU_REG_END,               0,   0,   0},
};

/* ******************************************************* */
/*                 VPU reset table                         */
/* ******************************************************* */
static struct vpu_reset_s vpu_reset_gx[] = {
	/* reg,             mask */
	{RESET0_LEVEL,      ((1<<5) | (1<<10) | (1<<19) | (1<<13))},
	{RESET1_LEVEL,      (1<<5)},
	{RESET2_LEVEL,      (1<<15)},
	{RESET4_LEVEL,      ((1<<6) | (1<<7) | (1<<13) | (1<<5) |
				(1<<9) | (1<<4) | (1<<12))},
	{RESET7_LEVEL,      (1<<7)},
	{VPU_REG_END,       0},
};

static struct vpu_reset_s vpu_reset_txlx[] = {
	/* reg,             mask */
	{RESET0_LEVEL_TXLX, ((1<<5) | (1<<10) | (1<<19) | (1<<13))},
	{RESET1_LEVEL_TXLX, (1<<5)},
	{RESET2_LEVEL_TXLX, (1<<15)},
	{RESET3_LEVEL_TXLX, ((1<<6) | (1<<7) | (1<<13) | (1<<5) |
				(1<<9) | (1<<4) | (1<<12))},
	{RESET7_LEVEL_TXLX, (1<<7)},
	{VPU_REG_END,       0},
};

static struct vpu_reset_s vpu_reset_tl1[] = {
	/* reg,             mask */
	{RESET0_LEVEL_TXLX, ((1<<5) | (1<<10) | (1<<19) | (1<<13))},
	{RESET1_LEVEL_TXLX, ((1<<5) | (1<<4))},
	{RESET2_LEVEL_TXLX, (1<<15)},
	{RESET4_LEVEL_TXLX, ((1<<6) | (1<<7) | (1<<13) | (1<<5) |
				(1<<9) | (1<<4) | (1<<12))},
	{RESET7_LEVEL_TXLX, (1<<7)},
	{VPU_REG_END,       0},
};


#endif
