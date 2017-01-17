/*
 * drivers/amlogic/media/common/vpu/vpu_clk.h
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

#ifndef __VPU_CLK_H__
#define __VPU_CLK_H__

/* #define LIMIT_VPU_CLK_LOW */

/* ************************************************ */
/* VPU frequency table, important. DO NOT modify!! */
/* ************************************************ */

#define CLK_FPLL_FREQ    2000 /* MHz */

/* GXBB */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXBB     3
#define CLK_LEVEL_MAX_GXBB     8
/* GXTVBB */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXTVBB     3
#define CLK_LEVEL_MAX_GXTVBB     8
/* GXL */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXL     3
#define CLK_LEVEL_MAX_GXL     8
/* GXM */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_GXM     3
#define CLK_LEVEL_MAX_GXM     8
/* TXL */
/* freq max=666M, default=666M */
#define CLK_LEVEL_DFT_TXL     3
#define CLK_LEVEL_MAX_TXL     8

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
};

static unsigned int fclk_div_table[] = {
	4, /* mux 0 */
	3, /* mux 1 */
	5, /* mux 2 */
	7, /* mux 3 */
	2, /* invalid */
};

/* gxbb, gxtvbb, gxl, gxm, txl, fpll=2000M */
static unsigned int vpu_clk_table[10][3] = {
	/* frequency   clk_mux       div */
	{100000000,    FCLK_DIV5,    3}, /* 0 */
	{166667000,    FCLK_DIV3,    3}, /* 1 */
	{200000000,    FCLK_DIV5,    1}, /* 2 */
	{250000000,    FCLK_DIV4,    1}, /* 3 */
	{333333000,    FCLK_DIV3,    1}, /* 4 */
	{400000000,    FCLK_DIV5,    0}, /* 5 */
	{500000000,    FCLK_DIV4,    0}, /* 6 */
	{666667000,    FCLK_DIV3,    0}, /* 7 */
	{696000000,    GPLL_CLK,     0}, /* 8 */
	{850000000,    GPLL_CLK,     0}, /* 9 */ /* invalid */
};

/* ************************************************ */

#endif
