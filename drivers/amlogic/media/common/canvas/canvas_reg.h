/*
 * drivers/amlogic/media/common/canvas/canvas_reg.h
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

#ifndef CANVAS_REG_HEADER_
#define CANVAS_REG_HEADER_

/*
 *register base.
 */
#define S_DMC_REG_BASE_OFFSET (0x6000)
#define DC_CAV_LUT_DATAL (0x08 << 2)
#define DC_CAV_LUT_DATAH (0x09 << 2)
#define DC_CAV_LUT_ADDR  (0x0a << 2)
#define DC_CAV_LUT_RDATAL  (0x0b << 2)
#define DC_CAV_LUT_RDATAH  (0x0c << 2)

/* M8M2/ */
#define DC_CAV_LUT_DATAL_M8M2 (0x12 << 2)
#define DC_CAV_LUT_DATAH_M8M2 (0x13 << 2)
#define DC_CAV_LUT_ADDR_M8M2  (0x14 << 2)
#define DC_CAV_LUT_RDATAL_M8M2  (0x15 << 2)
#define DC_CAV_LUT_RDATAH_M8M2  (0x16 << 2)

#define CANVAS_ADDR_LMASK       0x1fffffff
#define CANVAS_WIDTH_LMASK      0x7
#define CANVAS_WIDTH_LWID       3
#define CANVAS_WIDTH_LBIT       29

#define CANVAS_WIDTH_HMASK      0x1ff
#define CANVAS_WIDTH_HBIT       0
#define CANVAS_HEIGHT_MASK      0x1fff
#define CANVAS_HEIGHT_BIT       9
#define CANVAS_YWRAP            (1<<23)
#define CANVAS_XWRAP            (1<<22)
#define CANVAS_ADDR_NOWRAP      0x00
#define CANVAS_ADDR_WRAPX       0x01
#define CANVAS_ADDR_WRAPY       0x02
#define CANVAS_BLKMODE_MASK     3
#define CANVAS_BLKMODE_BIT      24
#define CANVAS_BLKMODE_LINEAR   0x00
#define CANVAS_BLKMODE_32X32    0x01
#define CANVAS_BLKMODE_64X32    0x02

#define CANVAS_LUT_INDEX_BIT    0
#define CANVAS_LUT_INDEX_MASK   0x7
#define CANVAS_LUT_WR_EN        (0x2 << 8)
#define CANVAS_LUT_RD_EN        (0x1 << 8)

#define MMC_PHY_CTRL              0x1380

/****************logo relative part **************************/
#define ASSIST_MBOX1_CLR_REG VDEC_ASSIST_MBOX1_CLR_REG
#define ASSIST_MBOX1_MASK VDEC_ASSIST_MBOX1_MASK
#define RESET_PSCALE        (1<<4)
#define RESET_IQIDCT        (1<<2)
#define RESET_MC            (1<<3)
#define MEM_BUFCTRL_MANUAL      (1<<1)
#define MEM_BUFCTRL_INIT        (1<<0)
#define MEM_LEVEL_CNT_BIT       18
#define MEM_FIFO_CNT_BIT        16
#define MEM_FILL_ON_LEVEL       (1<<10)
#define MEM_CTRL_EMPTY_EN       (1<<2)
#define MEM_CTRL_FILL_EN        (1<<1)
#define MEM_CTRL_INIT           (1<<0)
#define CANVAS_WRITE(x...)
#define CANVAS_READ(x...)

#endif
