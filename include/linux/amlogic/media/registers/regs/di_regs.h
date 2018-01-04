/*
 * include/linux/amlogic/media/registers/regs/di_regs.h
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

#ifndef DI_REGS_HEADER_
#define DI_REGS_HEADER_

#define DI_POST_CTRL 0x1701
#define DI_POST_SIZE 0x1702
#define DI_CONTWR_X 0x17a0
#define DI_CONTWR_Y 0x17a1
#define DI_CONTWR_CTRL 0x17a2
#define DI_CONTPRD_X 0x17a3
#define DI_CONTPRD_Y 0x17a4
#define DI_CONTP2RD_X 0x17a5
#define DI_CONTP2RD_Y 0x17a6
#define DI_CONTRD_CTRL 0x17a7
#define DI_NRWR_X 0x17c0
#define DI_NRWR_Y 0x17c1
#define DI_NRWR_CTRL 0x17c2
#define DI_MTNWR_X 0x17c3
#define DI_MTNWR_Y 0x17c4
#define DI_MTNWR_CTRL 0x17c5
#define DI_DIWR_X 0x17c6
#define DI_DIWR_Y 0x17c7
#define DI_DIWR_CTRL 0x17c8
#define DI_MTNCRD_X 0x17c9
#define DI_MTNCRD_Y 0x17ca
#define DI_MTNPRD_X 0x17cb
#define DI_MTNPRD_Y 0x17cc
#define DI_MTNRD_CTRL 0x17cd
#define DI_INP_GEN_REG 0x17ce
#define DI_INP_CANVAS0 0x17cf
#define DI_INP_LUMA_X0 0x17d0
#define DI_INP_LUMA_Y0 0x17d1
#define DI_INP_CHROMA_X0 0x17d2
#define DI_INP_CHROMA_Y0 0x17d3
#define DI_INP_RPT_LOOP 0x17d4
#define DI_INP_LUMA0_RPT_PAT 0x17d5
#define DI_INP_CHROMA0_RPT_PAT 0x17d6
#define DI_INP_DUMMY_PIXEL 0x17d7
#define DI_INP_LUMA_FIFO_SIZE 0x17d8
#define DI_INP_RANGE_MAP_Y 0x17ba
#define DI_INP_RANGE_MAP_CB 0x17bb
#define DI_INP_RANGE_MAP_CR 0x17bc
#define DI_INP_GEN_REG2 0x1791
#define DI_INP_FMT_CTRL 0x17d9
#define DI_INP_FMT_W 0x17da
#define DI_MEM_GEN_REG 0x17db
#define DI_MEM_CANVAS0 0x17dc
#define DI_MEM_LUMA_X0 0x17dd
#define DI_MEM_LUMA_Y0 0x17de
#define DI_MEM_CHROMA_X0 0x17df
#define DI_MEM_CHROMA_Y0 0x17e0
#define DI_MEM_RPT_LOOP 0x17e1
#define DI_MEM_LUMA0_RPT_PAT 0x17e2
#define DI_MEM_CHROMA0_RPT_PAT 0x17e3
#define DI_MEM_DUMMY_PIXEL 0x17e4
#define DI_MEM_LUMA_FIFO_SIZE 0x17e5
#define DI_MEM_RANGE_MAP_Y 0x17bd
#define DI_MEM_RANGE_MAP_CB 0x17be
#define DI_MEM_RANGE_MAP_CR 0x17bf
#define DI_MEM_GEN_REG2 0x1792
#define DI_MEM_FMT_CTRL 0x17e6
#define DI_MEM_FMT_W 0x17e7
#define DI_IF1_GEN_REG 0x17e8
#define DI_IF1_CANVAS0 0x17e9
#define DI_IF1_LUMA_X0 0x17ea
#define DI_IF1_LUMA_Y0 0x17eb
#define DI_IF1_CHROMA_X0 0x17ec
#define DI_IF1_CHROMA_Y0 0x17ed
#define DI_IF1_RPT_LOOP 0x17ee
#define DI_IF1_LUMA0_RPT_PAT 0x17ef
#define DI_IF1_CHROMA0_RPT_PAT 0x17f0
#define DI_IF1_DUMMY_PIXEL 0x17f1
#define DI_IF1_LUMA_FIFO_SIZE 0x17f2
#define DI_IF1_RANGE_MAP_Y 0x17fc
#define DI_IF1_RANGE_MAP_CB 0x17fd
#define DI_IF1_RANGE_MAP_CR 0x17fe
#define DI_IF1_GEN_REG2 0x1790
#define DI_IF1_FMT_CTRL 0x17f3
#define DI_IF1_FMT_W 0x17f4
#define DI_CHAN2_GEN_REG 0x17f5
#define DI_CHAN2_CANVAS0 0x17f6
#define DI_CHAN2_LUMA_X0 0x17f7
#define DI_CHAN2_LUMA_Y0 0x17f8
#define DI_CHAN2_CHROMA_X0 0x17f9
#define DI_CHAN2_CHROMA_Y0 0x17fa
#define DI_CHAN2_RPT_LOOP 0x17fb
#define DI_CHAN2_LUMA0_RPT_PAT 0x17b0
#define DI_CHAN2_CHROMA0_RPT_PAT 0x17b1
#define DI_CHAN2_DUMMY_PIXEL 0x17b2
#define DI_CHAN2_LUMA_FIFO_SIZE 0x17b3
#define DI_CHAN2_RANGE_MAP_Y 0x17b4
#define DI_CHAN2_RANGE_MAP_CB 0x17b5
#define DI_CHAN2_RANGE_MAP_CR 0x17b6
#define DI_CHAN2_GEN_REG2 0x17b7
#define DI_CHAN2_FMT_CTRL 0x17b8
#define DI_CHAN2_FMT_W 0x17b9

#define VD1_IF0_GEN_REG3 0x1aa7
#define DI_IF1_GEN_REG3  0x20a7
#define DI_IF2_GEN_REG3  0x2022
#define DI_IF0_GEN_REG3  0x2042
#endif


