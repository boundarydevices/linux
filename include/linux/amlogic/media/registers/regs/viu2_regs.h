/*
 * include/linux/amlogic/media/registers/regs/viu2_regs.h
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

#ifndef VIU2_REGS_HEADER_
#define VIU2_REGS_HEADER_

#define VIU2_ADDR_START 0x1e00
#define VIU2_ADDR_END 0x1eff
#define VIU2_SW_RESET 0x1e01
#define VIU2_OSD1_CTRL_STAT 0x1e10
#define VIU2_OSD1_CTRL_STAT2 0x1e2d
#define VIU2_OSD1_COLOR_ADDR 0x1e11
#define VIU2_OSD1_COLOR 0x1e12
#define VIU2_OSD1_TCOLOR_AG0 0x1e17
#define VIU2_OSD1_TCOLOR_AG1 0x1e18
#define VIU2_OSD1_TCOLOR_AG2 0x1e19
#define VIU2_OSD1_TCOLOR_AG3 0x1e1a
#define VIU2_OSD1_BLK0_CFG_W0 0x1e1b
#define VIU2_OSD1_BLK1_CFG_W0 0x1e1f
#define VIU2_OSD1_BLK2_CFG_W0 0x1e23
#define VIU2_OSD1_BLK3_CFG_W0 0x1e27
#define VIU2_OSD1_BLK0_CFG_W1 0x1e1c
#define VIU2_OSD1_BLK1_CFG_W1 0x1e20
#define VIU2_OSD1_BLK2_CFG_W1 0x1e24
#define VIU2_OSD1_BLK3_CFG_W1 0x1e28
#define VIU2_OSD1_BLK0_CFG_W2 0x1e1d
#define VIU2_OSD1_BLK1_CFG_W2 0x1e21
#define VIU2_OSD1_BLK2_CFG_W2 0x1e25
#define VIU2_OSD1_BLK3_CFG_W2 0x1e29
#define VIU2_OSD1_BLK0_CFG_W3 0x1e1e
#define VIU2_OSD1_BLK1_CFG_W3 0x1e22
#define VIU2_OSD1_BLK2_CFG_W3 0x1e26
#define VIU2_OSD1_BLK3_CFG_W3 0x1e2a
#define VIU2_OSD1_BLK0_CFG_W4 0x1e13
#define VIU2_OSD1_BLK1_CFG_W4 0x1e14
#define VIU2_OSD1_BLK2_CFG_W4 0x1e15
#define VIU2_OSD1_BLK3_CFG_W4 0x1e16
#define VIU2_OSD1_FIFO_CTRL_STAT 0x1e2b
#define VIU2_OSD1_TEST_RDDATA 0x1e2c
#define VIU2_OSD1_PROT_CTRL 0x1e2e
#define VIU2_OSD2_CTRL_STAT 0x1e30
#define VIU2_OSD2_CTRL_STAT2 0x1e4d
#define VIU2_OSD2_COLOR_ADDR 0x1e31
#define VIU2_OSD2_COLOR 0x1e32
#define VIU2_OSD2_HL1_H_START_END 0x1e33
#define VIU2_OSD2_HL1_V_START_END 0x1e34
#define VIU2_OSD2_HL2_H_START_END 0x1e35
#define VIU2_OSD2_HL2_V_START_END 0x1e36
#define VIU2_OSD2_TCOLOR_AG0 0x1e37
#define VIU2_OSD2_TCOLOR_AG1 0x1e38
#define VIU2_OSD2_TCOLOR_AG2 0x1e39
#define VIU2_OSD2_TCOLOR_AG3 0x1e3a
#define VIU2_OSD2_BLK0_CFG_W0 0x1e3b
#define VIU2_OSD2_BLK1_CFG_W0 0x1e3f
#define VIU2_OSD2_BLK2_CFG_W0 0x1e43
#define VIU2_OSD2_BLK3_CFG_W0 0x1e47
#define VIU2_OSD2_BLK0_CFG_W1 0x1e3c
#define VIU2_OSD2_BLK1_CFG_W1 0x1e40
#define VIU2_OSD2_BLK2_CFG_W1 0x1e44
#define VIU2_OSD2_BLK3_CFG_W1 0x1e48
#define VIU2_OSD2_BLK0_CFG_W2 0x1e3d
#define VIU2_OSD2_BLK1_CFG_W2 0x1e41
#define VIU2_OSD2_BLK2_CFG_W2 0x1e45
#define VIU2_OSD2_BLK3_CFG_W2 0x1e49
#define VIU2_OSD2_BLK0_CFG_W3 0x1e3e
#define VIU2_OSD2_BLK1_CFG_W3 0x1e42
#define VIU2_OSD2_BLK2_CFG_W3 0x1e46
#define VIU2_OSD2_BLK3_CFG_W3 0x1e4a
#define VIU2_OSD2_BLK0_CFG_W4 0x1e64
#define VIU2_OSD2_BLK1_CFG_W4 0x1e65
#define VIU2_OSD2_BLK2_CFG_W4 0x1e66
#define VIU2_OSD2_BLK3_CFG_W4 0x1e67
#define VIU2_OSD2_FIFO_CTRL_STAT 0x1e4b
#define VIU2_OSD2_TEST_RDDATA 0x1e4c
#define VIU2_OSD2_PROT_CTRL 0x1e4e
#define VIU2_VD1_IF0_GEN_REG 0x1e50
#define VIU2_VD1_IF0_CANVAS0 0x1e51
#define VIU2_VD1_IF0_CANVAS1 0x1e52
#define VIU2_VD1_IF0_LUMA_X0 0x1e53
#define VIU2_VD1_IF0_LUMA_Y0 0x1e54
#define VIU2_VD1_IF0_CHROMA_X0 0x1e55
#define VIU2_VD1_IF0_CHROMA_Y0 0x1e56
#define VIU2_VD1_IF0_LUMA_X1 0x1e57
#define VIU2_VD1_IF0_LUMA_Y1 0x1e58
#define VIU2_VD1_IF0_CHROMA_X1 0x1e59
#define VIU2_VD1_IF0_CHROMA_Y1 0x1e5a
#define VIU2_VD1_IF0_RPT_LOOP 0x1e5b
#define VIU2_VD1_IF0_LUMA0_RPT_PAT 0x1e5c
#define VIU2_VD1_IF0_CHROMA0_RPT_PAT 0x1e5d
#define VIU2_VD1_IF0_LUMA1_RPT_PAT 0x1e5e
#define VIU2_VD1_IF0_CHROMA1_RPT_PAT 0x1e5f
#define VIU2_VD1_IF0_LUMA_PSEL 0x1e60
#define VIU2_VD1_IF0_CHROMA_PSEL 0x1e61
#define VIU2_VD1_IF0_DUMMY_PIXEL 0x1e62
#define VIU2_VD1_IF0_LUMA_FIFO_SIZE 0x1e63
#define VIU2_VD1_IF0_RANGE_MAP_Y 0x1e6a
#define VIU2_VD1_IF0_RANGE_MAP_CB 0x1e6b
#define VIU2_VD1_IF0_RANGE_MAP_CR 0x1e6c
#define VIU2_VD1_IF0_GEN_REG2 0x1e6d
#define VIU2_VD1_IF0_PROT_CNTL 0x1e6e
#define VIU2_VD1_FMT_CTRL 0x1e68
#define VIU2_VD1_FMT_W 0x1e69

#endif

