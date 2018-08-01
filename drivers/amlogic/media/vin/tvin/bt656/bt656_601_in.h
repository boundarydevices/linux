/*
 * drivers/amlogic/media/vin/tvin/bt656/bt656_601_in.h
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

#ifndef __BT656_601_INPUT_H
#define __BT656_601_INPUT_H

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include <linux/amlogic/cpu_version.h>

#define BT656PR(fmt, args...)    pr_info("amvdec_bt656in: "fmt"", ## args)
#define BT656ERR(fmt, args...)   pr_err("amvdec_bt656in: error: "fmt"", ## args)


#define BT656_1                0xd0048000//0xd0048000~0xd004_ffff
#define BT656_2                0xd0050000//0xd0050000~0xd005_ffff

/* registers */
#define BT_CTRL                0x00
#define BT_VBISTART            0x01
#define BT_VBIEND              0x02
#define BT_FIELDSADR           0x03
#define BT_LINECTRL            0x04
#define BT_VIDEOSTART          0x05
#define BT_VIDEOEND            0x06
#define BT_SLICELINE0          0x07
#define BT_SLICELINE1          0x08
#define BT_PORT_CTRL           0x09
#define BT_SWAP_CTRL           0x0a
#define BT_AFIFO_CTRL          0x0d
#define BT_601_CTRL0           0x0e
#define BT_601_CTRL1           0x0f
#define BT_601_CTRL2           0x10
#define BT_601_CTRL3           0x11
#define BT_FIELD_LUMA          0x12
#define BT_RAW_CTRL            0x13
#define BT_STATUS              0x14
#define BT_INT_CTRL            0x15
#define BT_VLINE_STATUS        0x17
#define BT_ERR_CNT             0x19
#define BT_LCNT_STATUS         0x1a
#define BT_PCNT_STATUS         0x1c
#define BT_DELAY_CTRL          0x1d

/* #define VDIN_WR_V_START_END    0x1222 */
#define RESET1_REGISTER        0x1102
#define HHI_CSI_PHY_CNTL0      0x10d3
#define HHI_CSI_PHY_CNTL1      0x10d4
#define HHI_CSI_PHY_CNTL2      0x10d5

/* bit defines */
/*BT656 MACRO */
/* #define BT_CTRL 0x0 */
/* Soft reset */
#define BT_SOFT_RESET       31
/* Sync fifo soft  reset_n at system clock domain.
 * Level reset. 0 = reset. 1 : normal mode.
 */
/* 30 The reset of syncfifo output */
#define BT_SYSCLOCK_RESET    30
/* Sync fifo soft reset_n at bt656 clock domain.
 * Level reset.  0 = reset.  1 : normal mode.
 */
/* 29 The reset of syncfifo input */
#define BT_656CLOCK_RESET    29
/*0: use 3 port send raw data;
 * 1: use 1 port send raw data;
 */
#define BT_RAW_ISP           27
/* 25:26 VDIN VS selection.   00 :  SOF.  01: EOF.
 *  10: vbi start point.  11 : vbi end point.
 */
#define BT_VSYNC_SEL         25
/* 24:23 VDIN HS selection.  00 : EAV.  01: SAV.
 * 10:  EOL.  11: SOL
 */
#define BT_HSYNC_SEL          23
/* Camera_mode */
#define BT_CAMERA_MODE        22
/* 16 Update all status register. */
#define BT_UPDATE_ST_SEL      16
/* 15 Format select of YCbCr422 to YCbCr444. */
#define BT_COLOR_REPEAT       15
/* 14 Input format and output format.
 *0:YCbCr;1:565RGB;
 *2:1byte raw;3:2byte raw;
 */
#define BT_VIDEO_MODE         14
/*12 Go to ntsc/pal state.*/
#define BT_AUTO_FMT           12
/*11 Go to DVP state.*/
#define BT_PROG_MODE          11
/* 9 Go to RAW state. */
#define BT_RAW_MODE           9
/* 1: enable bt656 clock. 0:
 * disable bt656 clock.
 */
#define BT_CLOCK_ENABLE        7
/* 6 clk27_phase Reverse of clk27. */
#define BT_PHASE_REVERSE       6
/*5 Reset after error occurs. */
#define BT_AUTO_COVER_ERROR    5
/* 2 Use data or port control */
#define BT_REF_MODE            2
/* 0 bt_data_en	*/
#define BT_EN_BIT              0

/* #define BT_LINECTRL  0x04 */
/* [31]	soft_video_active
 * 0: control by vs/hs/field;
 * 1: control by cnt and sw;
 */
#define BT_SW_VACTIVE          31
/* [30:16] The end of active pixel*/
#define BT_HEND                16
/* [14:0] The start of active pixel*/
#define BT_HOFFSET             0

/* #define BT_VIDEOSTART 0x05 */
/* [28:16]	video1_start The start of active line, field1*/
#define BT_F1_VSTART           16
/*	[12:0]	video0_start The start of active line, field0*/
#define BT_F0_VSTART            0

/* #define BT_VIDEOEND   0x06 */
/* [28:16]	video1_end The end of active line, field1*/
#define BT_F1_VEND             16
/* [12:0]	video0_end The end of active line, field0*/
#define BT_F0_VEND              0

/* #define BT_SOFT_RESET           31	 Soft reset */
/* #define BT_JPEG_START           30 */
/* #define BT_JPEG_IGNORE_BYTES    18	//20:18 */
/* #define BT_JPEG_IGNORE_LAST     17 */
/* #define BT_UPDATE_ST_SEL        16 */
/* #define BT_COLOR_REPEAT         15 */
/* #define BT_VIDEO_MODE           13	// 14:13 */
/* #define BT_AUTO_FMT             12 */
/* #define BT_PROG_MODE            11 */
/* #define BT_JPEG_MODE            10 */
/* 1 : xclk27 is input.     0 : xclk27 is output. */
#define BT_XCLK27_EN_BIT        9
#define BT_FID_EN_BIT           8	/* 1 : enable use FID port. */
/* 1 : external xclk27      0 : internal clk27. */
#define BT_CLK27_SEL_BIT        7
/* 1 : no inverted          0 : inverted. */
/* #define BT_CLK27_PHASE_BIT      6 */
/* 1 : auto cover error by hardware. */
/* #define BT_ACE_MODE_BIT         5 */
/* 1 : no ancillay flag     0 : with ancillay flag. */
#define BT_SLICE_MODE_BIT       4
/* 1 : ntsc                 0 : pal. */
#define BT_FMT_MODE_BIT         3
/* 1 : from bit stream.     0 : from ports. */
#define BT_REF_MODE_BIT         2
/* 1 : BT656 model          0 : SAA7118 mode. */
#define BT_MODE_BIT             1

/* #define BT_PORT_CTRL   0x09 */
/* 26 data_endian
 * 0: afifo_wdata = {data1,data2}
 * 1: afifo_wdata = {data2,data1}
 */
#define BT_DATA_ENDIAN     26
/* 25 clk_inv_sel	Reverse clk in*/
#define BT_CLK_REVERSE     25
/* 24 dual_edge_clk_en Use dual edge clock*/
#define BT_CLK_EDGE_EN      24
/* 23 port_active_hmode
 * 0: active data when hsync is low;
 * 1: active data when hsync is high;
 */
#define BT_ACTIVE_HMODE      23
/* 22 vref_fro_vs_only	0: use reference reverse vblank
 * 1£ºuse vsync reverse vblank;
 */
#define BT_VREF_VS           22
/* 21 fid_hsvs_falling	For 601 format:
 *0: do not use falling edge of vsync
 *1:use falling edge of vsync
 */
#define BT_HSVS_FALLING      21
/* 20 fid_hsvs_rising	For 601 format:
 *0: do not use rising edge of vsync
 *1:use rising edge of vsync
 */
#define BT_HSVS_RISING       20
/*19	fid_hsvs_pcnt	For 601 format:
 *0£ºuse hs/vs generate field
 *1£ºuse pixel cnt generate field;
 */
#define BT_FID_HSVS_PCNT      19
/*[18:16]	hsync_delay	Skew of hsync*/
#define BT_HSYNC_DELAY        16
/*[15:13]	vsync_delay	Skew of vsync*/
#define BT_VSYNC_DELAY        13
/*[12:10]	fid_delay	Skew of field*/
#define BT_FID_DELAY         10
/*9	bt_10bto8b When use 8bits data port:
 *0£º[9:2]£»
 *1£º[9:2] + [1]£
 */
#define BT_10BTO8B            9
/*8	bt_d8b	0£ºuse 10bits data port£»
 *1£ºuse 8bits data port;
 */
#define BT_D8B               8
/*7	idq_phase	Reverse de port*/
#define BT_IDQ_PHASE         7
/*6	idq_en	Enable de port*/
#define BT_IDQ_EN            6
/*5	fid_hsvs	For 601 format:
 *0: use hs/vs generate fid;
 *1: use field port directly;
 */
#define BT_FID_HSVS          5
/*4	fid_phase	Reverse field port*/
#define BT_FID_PHASE         4
/*1	vsync_phase	Reverse vsync port*/
#define BT_HSYNC_PHASE       1
/*0	hsync_phase	Reverse hsync port*/
#define BT_VSYNC_PHASE       0

/* registers rw */
#define BT656_MAX_DEVS             2

static void __iomem *bt656_reg_base[BT656_MAX_DEVS];

static int bt656_reg_read(u32 index, unsigned int reg, unsigned int *val)
{
	if (index <= BT656_MAX_DEVS) {
		*val = readl(bt656_reg_base[index-1]+reg);
		return 0;
	} else
		return -1;
}

static int bt656_reg_write(u32 index, unsigned int reg, unsigned int val)
{
	if (index <= BT656_MAX_DEVS) {
		writel(val, (bt656_reg_base[index-1]+reg));
		return 0;
	} else
		return -1;
}

static inline uint32_t bt656_rd(u32 index, uint32_t reg)
{
	int val = 0;

	bt656_reg_read(index, (reg<<2), &val);
	return val;
}

static inline void bt656_wr(u32 index, uint32_t reg, const uint32_t val)
{
	bt656_reg_write(index, (reg<<2), val);
}

static inline void bt656_wr_bits(u32 index, uint32_t reg,
		const uint32_t value, const uint32_t start, const uint32_t len)
{
	bt656_wr(index, reg, ((bt656_rd(index, reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t bt656_rd_bits(u32 index, uint32_t reg,
		const uint32_t start, const uint32_t len)
{
	uint32_t val;

	val = ((bt656_rd(index, reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

enum am656_status_e {
	TVIN_AM656_STOP,
	TVIN_AM656_RUNNING,
};

struct am656in_dev_s {
	int                     index;
	dev_t                   devt;
	struct cdev             cdev;
	struct device           *dev;
	/* void __iomem            *reg_base; */
	unsigned int            overflow_cnt;
	unsigned int            skip_vdin_frame_count;
	enum am656_status_e     dec_status;
	struct vdin_parm_s      para;
	struct tvin_frontend_s  frontend;
	/* clktree */
	struct clk              *bt656_clk;
	struct clk              *gate_bt656;
	struct clk              *gate_bt656_pclk;
};

#endif

