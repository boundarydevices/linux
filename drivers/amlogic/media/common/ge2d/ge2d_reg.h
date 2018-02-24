/*
 * drivers/amlogic/media/common/ge2d/ge2d_reg.h
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

#ifndef _GE2D_REG_H_
#define _GE2D_REG_H_

#define GE2D_GEN_CTRL0 0x18a0
#define GE2D_GEN_CTRL1 0x18a1
#define GE2D_GEN_CTRL2 0x18a2
#define GE2D_CMD_CTRL 0x18a3
#define GE2D_STATUS0 0x18a4
#define GE2D_STATUS1 0x18a5
#define GE2D_SRC1_DEF_COLOR 0x18a6
#define GE2D_SRC1_CLIPX_START_END 0x18a7
#define GE2D_SRC1_CLIPY_START_END 0x18a8
#define GE2D_SRC1_CANVAS 0x18a9
#define GE2D_SRC1_X_START_END 0x18aa
#define GE2D_SRC1_Y_START_END 0x18ab
#define GE2D_SRC1_LUT_ADDR 0x18ac
#define GE2D_SRC1_LUT_DAT 0x18ad
#define GE2D_SRC1_FMT_CTRL 0x18ae
#define GE2D_SRC2_DEF_COLOR 0x18af
#define GE2D_SRC2_CLIPX_START_END 0x18b0
#define GE2D_SRC2_CLIPY_START_END 0x18b1
#define GE2D_SRC2_X_START_END 0x18b2
#define GE2D_SRC2_Y_START_END 0x18b3
#define GE2D_DST_CLIPX_START_END 0x18b4
#define GE2D_DST_CLIPY_START_END 0x18b5
#define GE2D_DST_X_START_END 0x18b6
#define GE2D_DST_Y_START_END 0x18b7
#define GE2D_SRC2_DST_CANVAS 0x18b8
#define GE2D_VSC_START_PHASE_STEP 0x18b9
#define GE2D_VSC_PHASE_SLOPE 0x18ba
#define GE2D_VSC_INI_CTRL 0x18bb
#define GE2D_HSC_START_PHASE_STEP 0x18bc
#define GE2D_HSC_PHASE_SLOPE 0x18bd
#define GE2D_HSC_INI_CTRL 0x18be
#define GE2D_HSC_ADV_CTRL 0x18bf
#define GE2D_SC_MISC_CTRL 0x18c0
#define GE2D_VSC_NRND_POINT 0x18c1
#define GE2D_VSC_NRND_PHASE 0x18c2
#define GE2D_HSC_NRND_POINT 0x18c3
#define GE2D_HSC_NRND_PHASE 0x18c4
#define GE2D_MATRIX_PRE_OFFSET 0x18c5
#define GE2D_MATRIX_COEF00_01 0x18c6
#define GE2D_MATRIX_COEF02_10 0x18c7
#define GE2D_MATRIX_COEF11_12 0x18c8
#define GE2D_MATRIX_COEF20_21 0x18c9
#define GE2D_MATRIX_COEF22_CTRL 0x18ca
#define GE2D_MATRIX_OFFSET 0x18cb
#define GE2D_ALU_OP_CTRL 0x18cc
#define GE2D_ALU_CONST_COLOR 0x18cd
#define GE2D_SRC1_KEY 0x18ce
#define GE2D_SRC1_KEY_MASK 0x18cf
#define GE2D_SRC2_KEY 0x18d0
#define GE2D_SRC2_KEY_MASK 0x18d1
#define GE2D_DST_BITMASK 0x18d2
#define GE2D_DP_ONOFF_CTRL 0x18d3
#define GE2D_SCALE_COEF_IDX 0x18d4
#define GE2D_SCALE_COEF 0x18d5
#define GE2D_SRC_OUTSIDE_ALPHA 0x18d6
#define GE2D_ANTIFLICK_CTRL0 0x18d8
#define GE2D_ANTIFLICK_CTRL1 0x18d9
#define GE2D_ANTIFLICK_COLOR_FILT0 0x18da
#define GE2D_ANTIFLICK_COLOR_FILT1 0x18db
#define GE2D_ANTIFLICK_COLOR_FILT2 0x18dc
#define GE2D_ANTIFLICK_COLOR_FILT3 0x18dd
#define GE2D_ANTIFLICK_ALPHA_FILT0 0x18de
#define GE2D_ANTIFLICK_ALPHA_FILT1 0x18df
#define GE2D_ANTIFLICK_ALPHA_FILT2 0x18e0
#define GE2D_ANTIFLICK_ALPHA_FILT3 0x18e1
#define GE2D_SRC1_RANGE_MAP_Y_CTRL 0x18e3
#define GE2D_SRC1_RANGE_MAP_CB_CTRL 0x18e4
#define GE2D_SRC1_RANGE_MAP_CR_CTRL 0x18e5
#define GE2D_ARB_BURST_NUM 0x18e6
#define GE2D_TID_TOKEN 0x18e7
#define GE2D_GEN_CTRL3 0x18e8
#define GE2D_STATUS2 0x18e9
#define GE2D_GEN_CTRL4 0x18ea
#define GE2D_DST1_BADDR_CTRL  0x18f1
#define GE2D_DST1_STRIDE_CTRL 0x18f2
#define GE2D_SRC1_BADDR_CTRL  0x18f3
#define GE2D_SRC1_STRIDE_CTRL 0x18f4
#define GE2D_SRC2_BADDR_CTRL  0x18f5
#define GE2D_SRC2_STRIDE_CTRL 0x18f6
#define GE2D_GEN_CTRL5  0x18f1

#define VIU_OSD1_BLK0_CFG_W0 0x1a1b

#endif
