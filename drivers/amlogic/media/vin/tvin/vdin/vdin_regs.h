/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_regs.h
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

#ifndef __VDIN_REGS_H
#define __VDIN_REGS_H

/* mmc */
#define VPU_VDIN_ASYNC_HOLD_CTRL 0x2743
#define VPU_VDISP_ASYNC_HOLD_CTRL 0x2744
#define VPU_VPUARB2_ASYNC_HOLD_CTRL 0x2745
#define VPU_ARB_URG_CTRL 0x2747
#define VPU_WRARB_MODE_L2C1 0x27a2
#define VPU_ARB_DBG_STAT_L1C2	0x27b6
#define VDIN_DET_IDLE_BIT 8
#define VDIN_DET_IDLE_WIDTH 4
#define VDIN0_IDLE_MASK 0x05
#define VDIN1_IDLE_MASK 0x0a
#define VPU_WRARB_REQEN_SLV_L1C2 0x27af
#define VDIN0_REQ_EN_BIT 0
#define VDIN1_REQ_EN_BIT 1
#define VDIN_MISC_CTRL 0x2782
#define VDIN0_OUT_AFBCE_BIT 21
#define VDIN0_OUT_MIF_BIT 20
#define VDIN0_MIF_ENABLE_BIT 19
#define VDIN1_OUT_AFBCE_BIT 18
#define VDIN1_OUT_MIF_BIT 17
#define VDIN1_MIF_ENABLE_BIT 16
#define VDIN0_MIF_RST_BIT 3
#define VDIN1_MIF_RST_BIT 4
#define VDIN_MIF_RST_W 1
#define VDIN1_WR_CTRL2 0x129f
#define VDIN1_WR_CTRL 0x12a0
#define VDIN_COM_CTRL1 0x1282


/* #define VIU_SW_RESET 0x1a01 */
/* #define P_VIU_SW_RESET          VCBUS_REG_ADDR(VIU_SW_RESET) */

#define VPU_VD1_MMC_CTRL 0x2703
#define VPU_VD2_MMC_CTRL 0x2704
#define VPU_VD1_MMC_CTRL 0x2703
#define VPU_VD2_MMC_CTRL 0x2704
#define VPU_DI_IF1_MMC_CTRL 0x2705
#define VPU_DI_MEM_MMC_CTRL 0x2706
#define VPU_DI_INP_MMC_CTRL 0x2707
#define VPU_DI_MTNRD_MMC_CTRL 0x2708
#define VPU_DI_CHAN2_MMC_CTRL 0x2709
#define VPU_DI_MTNWR_MMC_CTRL 0x270a
#define VPU_DI_NRWR_MMC_CTRL 0x270b
#define VPU_DI_DIWR_MMC_CTRL 0x270c
#define VPU_TVD3D_MMC_CTRL 0x2710
#define VPU_TVDVBI_MMC_CTRL 0x2711

/* vpp */
#define VPP_VDO_MEAS_CTRL 0x1da8
#define VPP_POSTBLEND_VD1_H_START_END 0x1d1c
#define VPP_POSTBLEND_VD1_V_START_END 0x1d1d

/* VDIN0        8'h00 - 8'h7f */
/* VDIN1        8'h80 - 8'hef */
/* #define VDIN0_OFFSET            0x00 */
/* #define VDIN1_OFFSET            0x80 */

#define VDIN_SCALE_COEF_IDX    ((0x1200))/* + 0xd0100000) */
#define VDIN_SCALE_COEF        ((0x1201))/* + 0xd0100000) */
/* bit 31,   mpeg_to_vdin_sel, 0: mpeg source to NR directly,
 * 1: mpeg source pass through here
 */
/* bit 30,   mpeg_field info which can be written by software */
/* Bit 29,   force go_field, pulse signal */
/* Bit 28,   force go_line, pulse signal */
/* Bit 27,   enable mpeg_go_field input signal */
/* Bit 26:20, hold lines */
/* Bit 19,   delay go_field function enable */
/* Bit 18:12, delay go_field line number */
/* Bit 11:10, component2 output switch, 00: select component0 in,
 *  01: select component1 in, 10: select component2 in
 */
/* Bit 9:8, component1 output switch, 00: select component0 in,
 * 01: select component1 in, 10: select component2 in
 */
/* Bit 7:6, component0 output switch, 00: select component0 in,
 * 01: select component1 in, 10: select component2 in
 */
/* Bit 5,   input window selection function enable */
/* Bit 4, enable VDIN common data input,
 * otherwise there will be no video data input
 */
/* Bit 3:0 vdin selection, 1: mpeg_in from dram, 2: bt656 input,
 * 3: component input, 4: tvdecoder input, 5: hdmi rx input,
 *  6: digtial video input, 7: loopback from Viu1, 8: MIPI.
 */
#define VDIN_COM_CTRL0        ((0x1202))/* + 0xd0100000) */
/* Bit 28:16 active_max_pix_cnt, readonly */
/* Bit 12:0  active_max_pix_cnt_shadow, readonly */
#define VDIN_ACTIVE_MAX_PIX_CNT_STATUS   ((0x1203))/* + 0xd0100000) */
/* Bit 28:16 go_line_cnt, readonly */
/* Bit 12:0  active_line_cnt, readonly */
#define VDIN_LCNT_STATUS                 ((0x1204))/* + 0xd0100000) */
/* Readonly */
/* Bit [14:3] lfifo_buf_cnt */
/* Bit 2, vdin_direct_done status */
/* Bit 1, vdin_nr_done status */
/* Bit 0, field */
#define VDIN_COM_STATUS0                 ((0x1205))/* + 0xd0100000) */
/* Readonly */
/* Bit 31, vdi4 fifo overflow */
/* Bit 29:24, vdi3_asfifo_cnt */
/* Bit 23, vdi3 fifo overflow */
/* Bit 21:16, vdi3_asfifo_cnt */
/* Bit 15, vdi2 fifo overflow */
/* Bit 13:8, vdi2_asfifo_cnt */
/* Bit 7, vdi1 fifo overflow */
/* Bit 5:0, vdi1_asfifo_cnt */
#define VDIN_COM_STATUS1             ((0x1206))/* + 0xd0100000) */
/* Bit 28:16 go_line_cnt_shadow, readonly */
/* Bit 12:0  active_line_cnt_shadow, readonly */
#define VDIN_LCNT_SHADOW_STATUS        ((0x1207))/* + 0xd0100000) */
/* each 8bit asfifo_ctrl is following: */
/* Bit 7, DE  enable */
/* Bit 6, go field enable */
/* Bit 5, go line enable */
/* Bit 4, if true, negative active input vsync */
/* Bit 3, if true, negative active input hsync */
/* Bit 2, vsync soft reset fifo enable */
/* Bit 1, overflow status clear */
/* Bit 0 asfifo soft reset, level signal */
/* Bit 7:0 vdi1 asfifo_ctrl */
/* Bit 23:16 vdi2 asfifo_ctrl */
#define VDIN_ASFIFO_CTRL0        ((0x1208))/* + 0xd0100000) */
/* Bit 7:0 vdi3 asfifo_ctrl */
/* Bit 23:16 vdi4 asfifo_ctrl */
#define VDIN_ASFIFO_CTRL1        ((0x1209))/* + 0xd0100000) */
/* Bit 28:16 input width minus 1, after the window function */
/* Bit 12:0  output width minus 1 */
#define VDIN_WIDTHM1I_WIDTHM1O   ((0x120a))/* + 0xd0100000) */
/* Bit 20:17 prehsc_mode,
 * bit 3:2, prehsc odd line interp mode,
 * bit 1:0, prehsc even line interp mode,
 */
/* each 2bit, 00: pix0+pix1/2, average, 01: pix1, 10: pix0 */
/* Bit 16:15 sp422_mode, special mode for the component1 and component2,
 *  00: normal case, 01: 32 64 32, 10: 0 64 64 0, 11: 16 96 16
 */
/* Bit 14:8, hsc_ini_pixi_ptr, signed data,
 *  only useful when short_lineo_en is true
 */
/* Bit 7, prehsc_en */
/* Bit 6, hsc_en, */
/* Bit 5, hsc_short_lineo_en, short line output enable */
/* Bit 4, hsc_nearest_en */
/* Bit 3, hsc_phase0_always_en */
/* Bit 2:0, hsc_bank_length */
#define VDIN_SC_MISC_CTRL          ((0x120b))/* + 0xd0100000) */
/* Bit 28:24, integer portion */
/* Bit 23:0, fraction portion */
#define VDIN_HSC_PHASE_STEP        ((0x120c))/* + 0xd0100000) */
/* Bit 30:29    hscale rpt_p0_num */
/* Bit 28:24    hscale ini_rcv_num */
/* Bit 23:0     hscale ini_phase */
#define VDIN_HSC_INI_CTRL          ((0x120d))/* + 0xd0100000) */
/* Read only */
/* Bit 23, vdi7 fifo overflow */
/* Bit 21:16, vdi7_asfifo_cnt */
/* Bit 15, vdi6 fifo overflow */
/* Bit 13:8, vdi6_asfifo_cnt */
/* Bit 7, vdi5 fifo overflow */
/* Bit 5:0, vdi5_asfifo_cnt */
#define VDIN_COM_STATUS2           ((0x120e))/* + 0xd0100000) */
/* Bit 25:16 asfifo decimate control */
/* Bit 25, if true, decimation counter
 * sync with first valid DE in the field,
 */
/* otherwise the decimation counter
 * is not sync with external signal
 */
/* Bit 24, decimation de enable */
/* Bit 23:20, decimation phase,
 * which counter value use to decimate,
 */
/* Bit 19:16, decimation number, 0: not decimation,
 *  1: decimation 2, 2: decimation 3 ....
 */
/* Bit 7:0 vdi5 asfifo_ctrl */
#define VDIN_ASFIFO_CTRL2  ((0x120f))/* + 0xd0100000) */
/* Bit 7,  highlight_en */
/* Bit 6   probe_post, if true, probe pixel data after matrix,
 *  otherwise probe pixel data before matrix
 */
/* Bit 5:4  probe_sel, 00: select matrix 0, 01: select matrix 1,
 *  otherwise select nothing
 */
/* Bit 3:2, matrix coef idx selection, 00: select mat0, 01: select mat1,
 *  otherwise slect nothing
 */
/* Bit 1   mat1 conversion matrix enable */
/* Bit 0   mat0 conversion matrix enable */
#define VDIN_MATRIX_CTRL            ((0x1210))/* + 0xd0100000) */
/* Bit 28:16 coef00 */
/* Bit 12:0  coef01 */
#define VDIN_MATRIX_COEF00_01       ((0x1211))/* + 0xd0100000) */
/* Bit 28:16 coef02 */
/* Bit 12:0  coef10 */
#define VDIN_MATRIX_COEF02_10       ((0x1212))/* + 0xd0100000) */
/* Bit 28:16 coef11 */
/* Bit 12:0  coef12 */
#define VDIN_MATRIX_COEF11_12       ((0x1213))/* + 0xd0100000) */
/* Bit 28:16 coef20 */
/* Bit 12:0  coef21 */
#define VDIN_MATRIX_COEF20_21       ((0x1214))/* + 0xd0100000) */
/* BIt 18:16 conv_rs */
/* Bit 12:0  coef22 */
#define VDIN_MATRIX_COEF22          ((0x1215))/* + 0xd0100000) */
/* Bit 26:16 offset0 */
/* Bit 10:0  offset1 */
#define VDIN_MATRIX_OFFSET0_1       ((0x1216))/* + 0xd0100000) */
/* Bit 10:0  offset2 */
#define VDIN_MATRIX_OFFSET2         ((0x1217))/* + 0xd0100000) */
/* Bit 26:16 pre_offset0 */
/* Bit 10:0  pre_offset1 */
#define VDIN_MATRIX_PRE_OFFSET0_1   ((0x1218))/* + 0xd0100000) */
/* Bit 10:0  pre_offset2 */
#define VDIN_MATRIX_PRE_OFFSET2     ((0x1219))/* + 0xd0100000) */
/* 12:0 lfifo_buf_size */
#define VDIN_LFIFO_CTRL             ((0x121a))/* + 0xd0100000) */
#define VDIN_COM_GCLK_CTRL          ((0x121b))/* + 0xd0100000) */
/* 12:0 VDIN input interface width minus 1,
 * before the window function, after the de decimation
 */
#define VDIN_INTF_WIDTHM1   ((0x121c))/* + 0xd0100000) */
/* Bit 15          //default== 0, urgent_ctrl_en */
/* Bit 14          //default== 0, urgent_wr, if true for write buffer */
/* Bit 13          //default== 0, out_inv_en */
/* Bit 12          //default == 0, urgent_ini_value */
/* Bit 11:6        //default == 0, up_th  up threshold */
/* Bit 5:0         //default == 0, dn_th  dn threshold */
#define VDIN_LFIFO_URG_CTRL        ((0x121e))/* + 0xd0100000) */
/* Bit 8, 1: discard data before line fifo, 0: normal mode */
/* Bit 7:0 Write chroma canvas address */
#define VDIN_WR_CTRL2    ((0x121f))/* + 0xd0100000) */
/* Bit 31:30 hconv_mode, Applicable only to bit[13:12]=0 or 2.
 * 0: Output every even pixels' CbCr;
 */
/* 1: Output every odd pixels' CbCr; */
/* 2: Output an average value per even&odd pair of pixels; */
/* 3: Output all CbCr.
 * (This does NOT apply to bit[13:12]=0 -- 4:2:2 mode.)
 */
/* Bit 29 no_clk_gate: disable vid_wr_mif clock gating function. */
/* Bit 28 clear write response counter
 * in the vdin write memory interface
 */
/* Bit 27 eol_sel, 1: use eol as the line end indication,
 * 0: use width as line end indication in the
 * vdin write memory interface
 */
/* Bit 26 vcp_nr_en. Only used in VDIN0. NOT used in VDIN1. */
/* Bit 25 vcp_wr_en. Only used in VDIN0. NOT used in VDIN1. */
/* Bit 24 vcp_in_en. Only used in VDIN0. NOT used in VDIN1. */
/* Bit 23 vdin frame reset enble, if true,
 * it will provide frame reset during go_field(vsync) to
 *  the modules after that
 */
/* Bit 22 vdin line fifo soft reset enable, meaning,
 *  if true line fifo will reset during go_field (vsync)
 */
/* Bit 21 vdin direct write done status clear bit */
/* Bit 20 vdin NR write done status clear bit */
/* Bit 18 swap_cbcr. Applicable only to bit[13:12]=2.
 * 0: Output CbCr (NV12); 1: Output CrCb (NV21).
 */
/* Bit 17:16 vconv_mode, Applicable only to bit[13:12]=2.
 *  0: Output every even lines' CbCr;
 */
/* 1: Output every odd lines' CbCr; */
/* 2: Reserved; */
/* 3: Output all CbCr. */
/* Bit 13:12 vdin write format, 0: 4:2:2 to luma canvas,
 * 1: 4:4:4 to luma canvas,
 */
/* 2: Y to luma canvas, CbCr to chroma canvas.
 * For NV12/21, also define Bit 31:30, 17:16, and bit 18.
 */
/* Bit 11 vdin write canvas double buffer enable,
 * means the canvas address will be latched by vsync before using
 */
/* Bit 10 1: disable ctrl_reg write pulse which will reset internal counter.
 *  when bit 11 is 1, this bit should be 1.
 */
/* Bit 9 vdin write request urgent */
/* Bit 8 vdin write request enable */
/* Bit 7:0 Write luma canvas address */
#define VDIN_WR_CTRL                   ((0x1220))/* + 0xd0100000) */
/* Bit 29, if true, horizontal reverse */
/* Bit 28:16 start */
/* Bit 12:0  end */
#define VDIN_WR_H_START_END            ((0x1221))/* + 0xd0100000) */
/* Bit 29, if true, vertical reverse */
/* Bit 28:16 start */
/* Bit 12:0  end */
#define VDIN_WR_V_START_END            ((0x1222))/* + 0xd0100000) */
/* Bit 24:20, integer portion */
/* Bit 19:0, fraction portion */
#define VDIN_VSC_PHASE_STEP            ((0x1223))/* + 0xd0100000) */
/* Bit 23, vsc_en, vertical scaler enable */
/* Bit 21 vsc_phase0_always_en, when scale up, you have to set it to 1 */
/* Bit 20:16 ini skip_line_num */
/* Bit 15:0 vscaler ini_phase */
#define VDIN_VSC_INI_CTRL                 ((0x1224))/* + 0xd0100000) */
/* Bit 28:16, vshrink input height minus 1 */
/* Bit 12:0, scaler input height minus 1 */
#define VDIN_SCIN_HEIGHTM1                ((0x1225))/* + 0xd0100000) */
/* Bit 23:16, dummy component 0 */
/* Bit 15:8, dummy component 1 */
/* Bit 7:0, dummy component 2 */
#define VDIN_DUMMY_DATA                   ((0x1226))/* + 0xd0100000) */
/* Read only */
/* Bit 29:20 component 0 */
/* Bit 19:10 component 1 */
/* Bit 9:0 component 2 */
#define VDIN_MATRIX_PROBE_COLOR           ((0x1228))/* + 0xd0100000) */
/* Bit 23:16 component 0 */
/* Bit 15:8  component 1 */
/* Bit 7:0 component 2 */
#define VDIN_MATRIX_HL_COLOR              ((0x1229))/* + 0xd0100000) */
/* 28:16 probe x, postion */
/* 12:0  probe y, position */
#define VDIN_MATRIX_PROBE_POS             ((0x122a))/* + 0xd0100000) */
#define VDIN_CHROMA_ADDR_PORT             ((0x122b))/* + 0xd0100000) */
#define VDIN_CHROMA_DATA_PORT             ((0x122c))/* + 0xd0100000) */
#define VDIN_CM_BRI_CON_CTRL              ((0x122d))/* + 0xd0100000) */
/* Bit 17  clk_cyc_cnt_clr, if true, clear this register */
/* Bit 16 if true, use vpu clock to count one line,
 * otherwise use actually hsync to count line_cnt
 */
/* Bit 15:0   line width using vpu clk */
#define VDIN_GO_LINE_CTRL   ((0x122f))/* + 0xd0100000) */
/* Bit 31:24 hist_pix_white_th,
 * larger than this th is counted as white pixel
 */
/* Bit 23:16 hist_pix_black_th,
 * less than this th is counted as black pixel
 */
/* Bit 11    hist_34bin_only,   34 bin only mode, including white/black */
/* Bit 10:9  ldim_stts_din_sel, 00: from matrix0 dout,
 *  01: from vsc_dout, 10: from matrix1 dout, 11: form matrix1 din
 */
/* Bit 8     ldim_stts_en */
/* Bit 6:5   hist_dnlp_low
 * the real pixels in each bins got by
 *  VDIN_DNLP_HISTXX should multiple with 2^(dnlp_low+3)
 */
/* Bit 3:2   hist_din_sel    the source used for hist statistics.
 *  00: from matrix0 dout,  01: from vsc_dout,
 *  10: from matrix1 dout, 11: form matrix1 din
 */
/* Bit 1     hist_win_en     1'b0: hist used for full picture;
 * 1'b1: hist used for pixels within hist window
 */
/* Bit 0     hist_spl_en
 * 1'b0: disable hist readback; 1'b1: enable hist readback
 */
#define VDIN_HIST_CTRL   ((0x1230))/* + 0xd0100000) */
/* Bit 28:16 hist_hstart  horizontal start value to define hist window */
/* Bit 12:0  hist_hend    horizontal end value to define hist window */
#define VDIN_HIST_H_START_END     ((0x1231))/* + 0xd0100000) */
/* Bit 28:16 hist_vstart  vertical start value to define hist window */
/* Bit 12:0  hist_vend    vertical end value to define hist window */
#define VDIN_HIST_V_START_END            ((0x1232))/* + 0xd0100000) */
/* Bit 15:8  hist_max    maximum value */
/* Bit 7:0   hist_min    minimum value */
/* read only */
#define VDIN_HIST_MAX_MIN                ((0x1233))/* + 0xd0100000) */
/* Bit 31:0  hist_spl_rd */
/* counts for the total luma value */
/* read only */
#define VDIN_HIST_SPL_VAL                ((0x1234))/* + 0xd0100000) */
/* Bit 21:0  hist_spl_pixel_count */
/* counts for the total calculated pixels */
/* read only */
#define VDIN_HIST_SPL_PIX_CNT            ((0x1235))/* + 0xd0100000) */
/* Bit 31:0  hist_chroma_sum */
/* counts for the total chroma value */
/* read only */
#define VDIN_HIST_CHROMA_SUM             ((0x1236))/* + 0xd0100000) */
/* Bit 31:16 higher hist bin */
/* Bit 15:0  lower hist bin */
/* 0-255 are splited to 64 bins evenly, and VDIN_DNLP_HISTXX */
/* are the statistic number of pixels that within each bin. */
/* VDIN_DNLP_HIST00[15:0]  counts for the first  bin */
/* VDIN_DNLP_HIST00[31:16] counts for the second bin */
/* VDIN_DNLP_HIST01[15:0]  counts for the third  bin */
/* VDIN_DNLP_HIST01[31:16] counts for the fourth bin */
/* etc... */
/* read only */
#define VDIN_DNLP_HIST00    ((0x1237))/* + 0xd0100000) */
#define VDIN_DNLP_HIST01    ((0x1238))/* + 0xd0100000) */
#define VDIN_DNLP_HIST02    ((0x1239))/* + 0xd0100000) */
#define VDIN_DNLP_HIST03    ((0x123a))/* + 0xd0100000) */
#define VDIN_DNLP_HIST04    ((0x123b))/* + 0xd0100000) */
#define VDIN_DNLP_HIST05    ((0x123c))/* + 0xd0100000) */
#define VDIN_DNLP_HIST06    ((0x123d))/* + 0xd0100000) */
#define VDIN_DNLP_HIST07    ((0x123e))/* + 0xd0100000) */
#define VDIN_DNLP_HIST08    ((0x123f))/* + 0xd0100000) */
#define VDIN_DNLP_HIST09    ((0x1240))/* + 0xd0100000) */
#define VDIN_DNLP_HIST10    ((0x1241))/* + 0xd0100000) */
#define VDIN_DNLP_HIST11    ((0x1242))/* + 0xd0100000) */
#define VDIN_DNLP_HIST12    ((0x1243))/* + 0xd0100000) */
#define VDIN_DNLP_HIST13    ((0x1244))/* + 0xd0100000) */
#define VDIN_DNLP_HIST14    ((0x1245))/* + 0xd0100000) */
#define VDIN_DNLP_HIST15    ((0x1246))/* + 0xd0100000) */
#define VDIN_DNLP_HIST16    ((0x1247))/* + 0xd0100000) */
#define VDIN_DNLP_HIST17    ((0x1248))/* + 0xd0100000) */
#define VDIN_DNLP_HIST18    ((0x1249))/* + 0xd0100000) */
#define VDIN_DNLP_HIST19    ((0x124a))/* + 0xd0100000) */
#define VDIN_DNLP_HIST20    ((0x124b))/* + 0xd0100000) */
#define VDIN_DNLP_HIST21    ((0x124c))/* + 0xd0100000) */
#define VDIN_DNLP_HIST22    ((0x124d))/* + 0xd0100000) */
#define VDIN_DNLP_HIST23    ((0x124e))/* + 0xd0100000) */
#define VDIN_DNLP_HIST24    ((0x124f))/* + 0xd0100000) */
#define VDIN_DNLP_HIST25    ((0x1250))/* + 0xd0100000) */
#define VDIN_DNLP_HIST26    ((0x1251))/* + 0xd0100000) */
#define VDIN_DNLP_HIST27    ((0x1252))/* + 0xd0100000) */
#define VDIN_DNLP_HIST28    ((0x1253))/* + 0xd0100000) */
#define VDIN_DNLP_HIST29    ((0x1254))/* + 0xd0100000) */
#define VDIN_DNLP_HIST30    ((0x1255))/* + 0xd0100000) */
#define VDIN_DNLP_HIST31    ((0x1256))/* + 0xd0100000) */
/* Bit 31, local dimming statistic enable */
/* Bit 28, eol enable */
/* Bit 27:25, vertical line overlap number for max finding */
/* Bit 24:22, horizontal pixel overlap number,
 * 0: 17 pix, 1: 9 pix, 2: 5 pix, 3: 3 pix, 4: 0 pix
 */
/* Bit 20, 1,2,1 low pass filter enable before max/hist statistic */
/* Bit 19:16, region H/V position index,
 *  refer to VDIN_LDIM_STTS_HIST_SET_REGION
 */
/* Bit 15, 1: region read index auto increase per read to
 * VDIN_LDIM_STTS_HIST_READ_REGION
 */
/* Bit 6:0, region read index */
#define VDIN_LDIM_STTS_HIST_REGION_IDX ((0x1257))/* + 0xd0100000) */
/* Bit 28:0, if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h0:
 * read/write hvstart0
 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h1: read/write hend01 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h2: read/write vend01 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h3: read/write hend23 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h4: read/write vend23 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h5: read/write hend45 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h6: read/write vend45 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'd7: read/write hend67 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h8: read/write vend67 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h9: read/write hend89 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'ha: read/write vend89 */
/* hvstart0, Bit 28:16 row0 vstart, Bit 12:0 col0 hstart */
/* hend01, Bit 28:16 col1 hend, Bit 12:0 col0 hend */
/* vend01, Bit 28:16 row1 vend, Bit 12:0 row0 vend */
/* hend23, Bit 28:16 col3 hend, Bit 12:0 col2 hend */
/* vend23, Bit 28:16 row3 vend, Bit 12:0 row2 vend */
/* hend45, Bit 28:16 col5 hend, Bit 12:0 col4 hend */
/* vend45, Bit 28:16 row5 vend, Bit 12:0 row4 vend */
/* hend67, Bit 28:16 col7 hend, Bit 12:0 col6 hend */
/* vend67, Bit 28:16 row7 vend, Bit 12:0 row6 vend */
/* hend89, Bit 28:16 col9 hend, Bit 12:0 col8 hend */
/* vend89, Bit 28:16 row9 vend, Bit 12:0 row8 vend */
#define VDIN_LDIM_STTS_HIST_SET_REGION  ((0x1258))/* + 0xd0100000) */
/* REGION STATISTIC DATA READ OUT PORT, bit 29:20 max_comp2,
 * bit 19:10 max_comp1, bit 9:0 max_comp0
 */
#define VDIN_LDIM_STTS_HIST_READ_REGION ((0x1259))/* + 0xd0100000) */
/* Bit 18, reset bit, high active */
/* Bit 17, if true, widen hs/vs pulse */
/* Bit 16  vsync total counter always accumulating enable */
/* Bit 14:12, select hs/vs of video input channel to measure,
 * 0: no selection, 1: vdi1, 2: vid2, 3: vid3, 4:vid4,
 *  5:vdi5, 6:vid6, 7:vdi7, 8: vdi8
 */
/* Bit 11:4, vsync_span, define how many vsync span need to measure */
/* Bit 2:0  meas_hs_index, index to select which HS counter/range */
#define VDIN_MEAS_CTRL0  ((0x125a))/* + 0xd0100000) */
/* Read only */
/* 19:16     meas_ind_total_count_n,
 *  every number of sync_span vsyncs, this count add 1
 */
/* 15:0      high bit portion of vsync total counter */
#define VDIN_MEAS_VS_COUNT_HI  ((0x125b))/* + 0xd0100000) */
/* Read only */
/* 31:0, low bit portion of vsync total counter */
#define VDIN_MEAS_VS_COUNT_LO   ((0x125c))/* + 0xd0100000) */
/* according to the meas_hs_index in register VDIN_MEAS_CTRL0 */
/* meas_hs_index == 0, first hs range */
/* meas_hs_index == 1, second hs range */
/* meas_hs_index == 2, third hs range */
/* meas_hs_index == 3, fourth hs range */
/* bit 28:16 count_start */
/* bit 12:0 count_end */
#define VDIN_MEAS_HS_RANGE   ((0x125d))/* + 0xd0100000) */
/* Read only */
/* according to the meas_hs_index in register VDIN_MEAS_CTRL0, */
/* meas_hs_index == 0, first range hs counter, */
/* meas_hs_index == 1, second range hs counter */
/* meas_hs_index == 2, third range hs counter */
/* meas_hs_index == 3, fourth range hs counter */
/* 23:0 */
#define VDIN_MEAS_HS_COUNT     ((0x125e))/* + 0xd0100000) */
/* Bit 8      white_enable */
/* Bit 7:0    blkbar_white_level */
#define VDIN_BLKBAR_CTRL1     ((0x125f))/* + 0xd0100000) */
/* Bit 31:24 blkbar_black_level    threshold to judge a black point */
/* Bit 23:21 Reserved */
/* Bit 20:8  blkbar_hwidth         left and right region width */
/* Bit 7:5   blkbar_comp_sel
 * select yin or uin or vin to be the valid input
 */
/* Bit 4     blkbar_sw_statistic_en enable software statistic of
 * each block black points number
 */
/* Bit 3     blkbar_det_en */
/* Bit 2:1   blkbar_din_sel */
/* bit blkbar_det_top_en */
#define VDIN_BLKBAR_CTRL0    ((0x1260))/* + 0xd0100000) */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_hstart.        Left region start */
/* Bit 15:13 Reserved */
/* Bit 12:0  blkbar_hend.          Right region end */
#define VDIN_BLKBAR_H_START_END  ((0x1261))/* + 0xd0100000) */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_vstart */
/* Bit 15:13 Reserved */
/* Bit 12:0  blkbar_vend */
#define VDIN_BLKBAR_V_START_END  ((0x1262))/* + 0xd0100000) */
/* Bit 31:20 Reserved */
/* Bit 19:0  blkbar_cnt_threshold. threshold to
 * judge whether a block is totally black
 */
#define VDIN_BLKBAR_CNT_THRESHOLD  ((0x1263))/* + 0xd0100000) */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_row_th1.       //threshold of the top blackbar */
/* Bit 15:13 Reserved */
/* bit 12:0  blkbar_row_th2        //threshold of the bottom blackbar*/
#define VDIN_BLKBAR_ROW_TH1_TH2  ((0x1264))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_ind_left_start. horizontal start of
 * the left region in the current searching
 */
/* Bit 15:13 Reserved */
/* Bit 12:0  blkbar_ind_left_end.   horizontal end of
 *  the left region in the current searching
 */
#define VDIN_BLKBAR_IND_LEFT_START_END     ((0x1265))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_ind_right_start.horizontal start of
 * the right region in the current searching
 */
/* Bit 15:13 Reserved */
/* Bit 12:0  blkbar_ind_right_end.  horizontal end of
 *  the right region in the current searching
 */
#define VDIN_BLKBAR_IND_RIGHT_START_END    ((0x1266))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:20 Reserved */
/* Bit 19:0  blkbar_ind_left1_cnt.  Black pixel counter.
 * left part of the left region
 */
#define VDIN_BLKBAR_IND_LEFT1_CNT     ((0x1267))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:20 Reserved */
/* Bit 19:0  blkbar_ind_left2_cnt.  Black pixel counter.
 *  right part of the left region
 */
#define VDIN_BLKBAR_IND_LEFT2_CNT       ((0x1268))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:20 Reserved */
/* Bit 19:0  blkbar_ind_right1_cnt. Black pixel counter.
 * left part of the right region
 */
#define VDIN_BLKBAR_IND_RIGHT1_CNT      ((0x1269))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:20 Reserved */
/* Bit 19:0  blkbar_ind_right2_cnt. Black pixel counter.
 * right part of the right region
 */
#define VDIN_BLKBAR_IND_RIGHT2_CNT      ((0x126a))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:30 Resersed */
/* Bit 29    blkbar_ind_black_det_done.
 * LEFT/RIGHT Black detection done
 */
/* Bit 28:16 blkbar_top_pos.            Top black bar position */
/* Bit 15:13 Reserved. */
/* Bit 12:0  blkbar_bot_pos.            Bottom black bar position */
#define VDIN_BLKBAR_STATUS0    ((0x126b))/* + 0xd0100000) */
/* Readonly */
/* Bit 31:29 Reserved */
/* Bit 28:16 blkbar_left_pos.       Left black bar posiont */
/* Bit 15:13 Reserved */
/* Bit 12:0  blkbar_right_pos.      Right black bar position */
#define VDIN_BLKBAR_STATUS1      ((0x126c))/* + 0xd0100000) */
/* Bit 28:16 input window H start */
/* Bit 12:0  input window H end */
#define VDIN_WIN_H_START_END     ((0x126d))/* + 0xd0100000) */
/* Bit 28:16 input window V start */
/* Bit 12:0  input window V end */
#define VDIN_WIN_V_START_END     ((0x126e))/* + 0xd0100000) */
/* Bit 23:16 vdi8 asfifo_ctrl */
/* Bit 15:8 vdi7 asfifo_ctrl */
/* Bit 7:0 vdi6 asfifo_ctrl */
#define VDIN_ASFIFO_CTRL3        ((0x126f))/* + 0xd0100000) */
/* Bit 3:2 vshrk_clk2_ctrl */
/* Bit 1:0 vshrk_clk1_ctrl */
#define VDIN_COM_GCLK_CTRL2      ((0x1270))/* + 0xd0100000) */
/* Bit 27 vshrk_en */
/* Bit 26:25 vshrk_mode */
/* Bit 24 vshrk_lpf_mode */
/* Bit 23:0 vshrk_dummy */
#define VDIN_VSHRK_CTRL          ((0x1271))/* + 0xd0100000) */
#define VDIN_DNLP_HIST32         ((0x1272))/* + 0xd0100000) */
/* Read only */
/* Bit 7, vdi9 fifo overflow */
/* Bit 5:0, vdi9_asfifo_cnt */
#define VDIN_COM_STATUS3         ((0x1273))/* + 0xd0100000) */

/* dolby vdin regs */
#define VDIN_DOLBY_DSC_CTRL0                       0x1275
/*((0x1275  << 2) + 0xff900000)*/
#define VDIN_DOLBY_DSC_CTRL1                       0x1276
#define VDIN_DOLBY_DSC_CTRL2                       0x1277
#define VDIN_DOLBY_DSC_CTRL3                       0x1278
#define VDIN_DOLBY_AXI_CTRL0                       0x1279
#define VDIN_DOLBY_AXI_CTRL1                       0x127a
#define VDIN_DOLBY_AXI_CTRL2                       0x127b
#define VDIN_DOLBY_AXI_CTRL3                       0x127c
#define VDIN_DOLBY_DSC_STATUS0                     0x127d
#define VDIN_DOLBY_DSC_STATUS1                     0x127e
#define VDIN_DOLBY_DSC_STATUS2                     0x127f
#define VDIN_DOLBY_DSC_STATUS3                     0x121d


/*g12a new add begin*/
#define VDIN_HDR2_CTRL 0x1280
#define VDIN_HDR2_CLK_GATE 0x1281
#define VDIN_HDR2_MATRIXI_COEF00_01 0x1282
#define VDIN_HDR2_MATRIXI_COEF02_10 0x1283
#define VDIN_HDR2_MATRIXI_COEF11_12 0x1284
#define VDIN_HDR2_MATRIXI_COEF20_21 0x1285
#define VDIN_HDR2_MATRIXI_COEF22 0x1286
#define VDIN_HDR2_MATRIXI_COEF30_31 0x1287
#define VDIN_HDR2_MATRIXI_COEF32_40 0x1288
#define VDIN_HDR2_MATRIXI_COEF41_42 0x1289
#define VDIN_HDR2_MATRIXI_OFFSET0_1 0x128a
#define VDIN_HDR2_MATRIXI_OFFSET2 0x128b
#define VDIN_HDR2_MATRIXI_PRE_OFFSET0_1 0x128c
#define VDIN_HDR2_MATRIXI_PRE_OFFSET2 0x128d
#define VDIN_HDR2_MATRIXO_COEF00_01 0x128e
#define VDIN_HDR2_MATRIXO_COEF02_10 0x128f
#define VDIN_HDR2_MATRIXO_COEF11_12 0x1290
#define VDIN_HDR2_MATRIXO_COEF20_21 0x1291
#define VDIN_HDR2_MATRIXO_COEF22 0x1292
#define VDIN_HDR2_MATRIXO_COEF30_31 0x1293
#define VDIN_HDR2_MATRIXO_COEF32_40 0x1294
#define VDIN_HDR2_MATRIXO_COEF41_42 0x1295
#define VDIN_HDR2_MATRIXO_OFFSET0_1 0x1296
#define VDIN_HDR2_MATRIXO_OFFSET2 0x1297
#define VDIN_HDR2_MATRIXO_PRE_OFFSET0_1 0x1298
#define VDIN_HDR2_MATRIXO_PRE_OFFSET2 0x1299
#define VDIN_HDR2_MATRIXI_CLIP 0x129a
#define VDIN_HDR2_MATRIXO_CLIP 0x129b
#define VDIN_HDR2_CGAIN_OFFT 0x129c
#define VDIN_EOTF_LUT_ADDR_PORT 0x129e
#define VDIN_EOTF_LUT_DATA_PORT 0x129f
#define VDIN_OETF_LUT_ADDR_PORT 0x12a0
#define VDIN_OETF_LUT_DATA_PORT 0x12a1
#define VDIN_CGAIN_LUT_ADDR_PORT 0x12a2
#define VDIN_CGAIN_LUT_DATA_PORT 0x12a3
#define VDIN_HDR2_CGAIN_COEF0 0x12a4
#define VDIN_HDR2_CGAIN_COEF1 0x12a5
#define VDIN_OGAIN_LUT_ADDR_PORT 0x12a6
#define VDIN_OGAIN_LUT_DATA_PORT 0x12a7
#define VDIN_HDR2_ADPS_CTRL 0x12a8
#define VDIN_HDR2_ADPS_ALPHA0 0x12a9
#define VDIN_HDR2_ADPS_ALPHA1 0x12aa
#define VDIN_HDR2_ADPS_BETA0 0x12ab
#define VDIN_HDR2_ADPS_BETA1 0x12ac
#define VDIN_HDR2_ADPS_BETA2 0x12ad
#define VDIN_HDR2_ADPS_COEF0 0x12ae
#define VDIN_HDR2_ADPS_COEF1 0x12af
#define VDIN_HDR2_GMUT_CTRL 0x12b0
#define VDIN_HDR2_GMUT_COEF0 0x12b1
#define VDIN_HDR2_GMUT_COEF1 0x12b2
#define VDIN_HDR2_GMUT_COEF2 0x12b3
#define VDIN_HDR2_GMUT_COEF3 0x12b4
#define VDIN_HDR2_GMUT_COEF4 0x12b5
#define VDIN_HDR2_PIPE_CTRL1 0x12b6
#define VDIN_HDR2_PIPE_CTRL2 0x12b7
#define VDIN_HDR2_PIPE_CTRL3 0x12b8
#define VDIN_HDR2_PROC_WIN1 0x12b9
#define VDIN_HDR2_PROC_WIN2 0x12ba
#define VDIN_HDR2_MATRIXI_EN_CTRL 0x12bb
#define VDIN_HDR2_MATRIXO_EN_CTRL 0x12bc

/*g12a new add end*/

#define VDIN_WRARB_REQEN_SLV       0x12c1

/* #define VDIN_SCALE_COEF_IDX                        0x1200 */
/* #define VDIN_SCALE_COEF                            0x1201 */

/* #define VDIN_COM_CTRL0                             0x1202 */
/* used by other modules,indicates that MPEG input.
 *0: mpeg source to NR directly,
 *1: mpeg source pass through here
 */
#define MPEG_TO_VDIN_SEL_BIT            31
#define MPEG_TO_VDIN_SEL_WID            1
/* indicates MPEG field ID,written by software.
 *0: EVEN FIELD 1: ODD FIELD
 */
#define MPEG_FLD_BIT                    30
#define MPEG_FLD_WID                    1
/* enable external MPEG Go_field (VS) */
#define MPEG_GO_FLD_EN_BIT              27
#define MPEG_GO_FLD_EN_WID              1

/* vdin read enable after hold lines counting from delayed Go-field (VS). */
#define HOLD_LN_BIT                     20
#define HOLD_LN_WID                     7
#define DLY_GO_FLD_EN_BIT               19
#define DLY_GO_FLD_EN_WID               1
#define DLY_GO_FLD_LN_NUM_BIT           12
#define DLY_GO_FLD_LN_NUM_WID           7    /* delay go field lines */
/* 00: component0_in 01: component1_in 10: component2_in */
#define COMP2_OUT_SWT_BIT               10
#define COMP2_OUT_SWT_WID               2
/* 00: component0_in 01: component1_in 10: component2_in */
#define COMP1_OUT_SWT_BIT               8
#define COMP1_OUT_SWT_WID               2
/* 00: component0_in 01: component1_in 10: component2_in */
#define COMP0_OUT_SWT_BIT               6
#define COMP0_OUT_SWT_WID               2
#define INPUT_WIN_SEL_EN_BIT            5
#define INPUT_WIN_SEL_EN_WID            1
/* 0: no data input 1: common data input */
#define COMMON_DATA_IN_EN_BIT           4
#define COMMON_DATA_IN_EN_WID           1
/* 1: MPEG, 2: 656, 3: TVFE, 4: CVD2, 5: HDMI_Rx,6: DVIN otherwise: NULL
*7: loopback from VIU1, 8: MIPI csi2 in meson6
*/
#define VDIN_SEL_BIT                    0
#define VDIN_SEL_WID                    4
/* #define VDIN_ACTIVE_MAX_PIX_CNT_STATUS             0x1203 */
/* ~field_hold & prehsc input active max pixel
 * every line output of window
 */
#define ACTIVE_MAX_PIX_CNT_BIT          16
#define ACTIVE_MAX_PIX_CNT_WID          13
#define ACTIVE_MAX_PIX_CNT_SDW_BIT      0    /* latch by go_field */
#define ACTIVE_MAX_PIX_CNT_SDW_WID      13

/* #define VDIN_LCNT_STATUS                           0x1204 */
/* line count by force_go_line |sel_go_line :output of decimate */
#define GO_LN_CNT_BIT                   16
#define GO_LN_CNT_WID                   13
/* line  count prehsc input active max pixel
 * every active line output of window
 */
#define ACTIVE_LN_CNT_BIT               0
#define ACTIVE_LN_CNT_WID               13

/* #define VDIN_COM_STATUS0                        0x1205 */
#define LFIFO_BUF_CNT_BIT               3
#define LFIFO_BUF_CNT_WID               10   /* wren + read - */
#define DIRECT_DONE_STATUS_BIT          2
/* direct_done_clr_bit & reg_wpluse */
#define DIRECT_DONE_STATUS_WID          1
#define NR_DONE_STATUS_BIT              1
 /* nr_done_clr_bit & reg_wpluse */
#define NR_DONE_STATUS_WID              1
#define VDIN_FLD_EVEN_BIT               0
#define VDIN_FLD_EVEN_WID               1

/* #define VDIN_COM_STATUS1                        0x1206 */
#define FIFO4_OVFL_BIT                  31
#define FIFO4_OVFL_WID                  1
#define ASFIFO4_CNT_BIT                 24
#define ASFIFO4_CNT_WID                 6
#define FIFO3_OVFL_BIT                  23
#define FIFO3_OVFL_WID                  1
#define ASFIFO3_CNT_BIT                 16
#define ASFIFO3_CNT_WID                 6
#define FIFO2_OVFL_BIT                  15
#define FIFO2_OVFL_WID                  1
#define ASFIFO2_CNT_BIT                 8
#define ASFIFO2_CNT_WID                 6
#define FIFO1_OVFL_BIT                  7
#define FIFO1_OVFL_WID                  1
#define ASFIFO1_CNT_BIT                 0
#define ASFIFO1_CNT_WID                 6

/* #define VDIN_LCNT_SHADOW_STATUS                 0x1207 */
#define GO_LN_CNT_SDW_BIT               16
#define GO_LN_CNT_SDW_WID               13   /* latch by go_field */
#define ACTIVE_LN_CNT_SDW_BIT           0
#define ACTIVE_LN_CNT_SDW_WID           13   /* latch by go_field */

/* #define VDIN_ASFIFO_CTRL0                       0x1208 */
#define VDI2_ASFIFO_CTRL_BIT			16
#define VDI2_ASFIFO_CTRL_WID			8
#define ASFIFO2_DE_EN_BIT               23
#define ASFIFO2_DE_EN_WID               1
#define ASFIFO2_GO_FLD_EN_BIT           22
#define ASFIFO2_GO_FLD_EN_WID           1
#define ASFIFO2_GO_LN_EN_BIT            21
#define ASFIFO2_GO_LN_EN_WID            1
#define ASFIFO2_NEG_ACTIVE_IN_VS_BIT    20
#define ASFIFO2_NEG_ACTIVE_IN_VS_WID    1
#define ASFIFO2_NEG_ACTIVE_IN_HS_BIT    19
#define ASFIFO2_NEG_ACTIVE_IN_HS_WID    1
#define ASFIFO2_VS_SOFT_RST_FIFO_EN_BIT 18
#define ASFIFO2_VS_SOFT_RST_FIFO_EN_WID 1
#define ASFIFO2_OVFL_STATUS_CLR_BIT     17
#define ASFIFO2_OVFL_STATUS_CLR_WID     1
#define ASFIFO2_SOFT_RST_BIT            16
#define ASFIFO2_SOFT_RST_WID            1    /* write 1 & then 0 to reset */
#define VDI1_ASFIFO_CTRL_BIT			0
#define VDI1_ASFIFO_CTRL_WID			8
#define ASFIFO1_DE_EN_BIT               7
#define ASFIFO1_DE_EN_WID               1
#define ASFIFO1_GO_FLD_EN_BIT           6
#define ASFIFO1_GO_FLD_EN_WID           1
#define ASFIFO1_GO_LN_EN_BIT            5
#define ASFIFO1_GO_LN_EN_WID            1
#define ASFIFO1_NEG_ACTIVE_IN_VS_BIT    4
#define ASFIFO1_NEG_ACTIVE_IN_VS_WID    1
#define ASFIFO1_NEG_ACTIVE_IN_HS_BIT    3
#define ASFIFO1_NEG_ACTIVE_IN_HS_WID    1
#define ASFIFO1_VS_SOFT_RST_FIFO_EN_BIT 2
#define ASFIFO1_VS_SOFT_RST_FIFO_EN_WID 1
#define ASFIFO1_OVFL_STATUS_CLR_BIT     1
#define ASFIFO1_OVFL_STATUS_CLR_WID     1
#define ASFIFO1_SOFT_RST_BIT            0
#define ASFIFO1_SOFT_RST_WID            1    /* write 1 & then 0 to reset */

/* #define VDIN_ASFIFO_CTRL1                         0x1209 */
#define VDI4_ASFIFO_CTRL_BIT			16
#define VDI4_ASFIFO_CTRL_WID			8
#define ASFIFO4_DE_EN_BIT               23
#define ASFIFO4_DE_EN_WID               1
#define ASFIFO4_GO_FLD_EN_BIT           22
#define ASFIFO4_GO_FLD_EN_WID           1
#define ASFIFO4_GO_LN_EN_BIT            21
#define ASFIFO4_GO_LN_EN_WID            1
#define ASFIFO4_NEG_ACTIVE_IN_VS_BIT    20
#define ASFIFO4_NEG_ACTIVE_IN_VS_WID    1
#define ASFIFO4_NEG_ACTIVE_IN_HS_BIT    19
#define ASFIFO4_NEG_ACTIVE_IN_HS_WID    1
#define ASFIFO4_VS_SOFT_RST_FIFO_EN_BIT 18
#define ASFIFO4_VS_SOFT_RST_FIFO_EN_WID 1
#define ASFIFO4_OVFL_STATUS_CLR_BIT     17
#define ASFIFO4_OVFL_STATUS_CLR_WID     1
#define ASFIFO4_SOFT_RST_BIT            16
#define ASFIFO4_SOFT_RST_WID            1    /* write 1 & then 0 to reset */
#define VDI3_ASFIFO_CTRL_BIT			0
#define VDI3_ASFIFO_CTRL_WID			8
#define ASFIFO3_DE_EN_BIT               7
#define ASFIFO3_DE_EN_WID               1
#define ASFIFO3_GO_FLD_EN_BIT           6
#define ASFIFO3_GO_FLD_EN_WID           1
#define ASFIFO3_GO_LN_EN_BIT            5
#define ASFIFO3_GO_LN_EN_WID            1
#define ASFIFO3_NEG_ACTIVE_IN_VS_BIT    4
#define ASFIFO3_NEG_ACTIVE_IN_VS_WID    1
#define ASFIFO3_NEG_ACTIVE_IN_HS_BIT    3
#define ASFIFO3_NEG_ACTIVE_IN_HS_WID    1
#define ASFIFO3_VS_SOFT_RST_FIFO_EN_BIT 2
#define ASFIFO3_VS_SOFT_RST_FIFO_EN_WID 1
#define ASFIFO3_OVFL_STATUS_CLR_BIT     1
#define ASFIFO3_OVFL_STATUS_CLR_WID     1
#define ASFIFO3_SOFT_RST_BIT            0
#define ASFIFO3_SOFT_RST_WID            1    /* write 1 & then 0 to reset */

/* #define VDIN_WIDTHM1I_WIDTHM1O                  0x120a */
#define WIDTHM1I_BIT                    16
#define WIDTHM1I_WID                    13
#define WIDTHM1O_BIT                    0
#define WIDTHM1O_WID                    13

/* #define VDIN_SC_MISC_CTRL                       0x120b */
/* signed value for short line output */
#define PRE_HSCL_MODE_BIT               17
#define PRE_HSCL_MODE_WID               4
#define INIT_PIX_IN_PTR_BIT             8
#define INIT_PIX_IN_PTR_WID             7
#define INIT_PIX_IN_PTR_MSK             0x0000007f
#define PRE_HSCL_EN_BIT                 7
/* pre-hscaler: 1/2 coarse scale down */
#define PRE_HSCL_EN_WID                 1
#define HSCL_EN_BIT                     6
#define HSCL_EN_WID                     1    /* hscaler: fine scale down */
#define SHORT_LN_OUT_EN_BIT             5
#define SHORT_LN_OUT_EN_WID             1
/*when decimation timing located in between 2 input pixels,
 * decimate the nearest one
 */
#define HSCL_NEAREST_EN_BIT             4
#define HSCL_NEAREST_EN_WID             1
/* Start decimation from phase 0 for each line */
#define PHASE0_ALWAYS_EN_BIT            3
#define PHASE0_ALWAYS_EN_WID            1
/* filter pixel buf len (depth), max is 3 in IP design */
#define HSCL_BANK_LEN_BIT               0
#define HSCL_BANK_LEN_WID               3


/* #define VDIN_HSC_PHASE_STEP                     0x120c */
#define HSCL_PHASE_STEP_INT_BIT         24
#define HSCL_PHASE_STEP_INT_WID         5
#define HSCL_PHASE_STEP_FRA_BIT         0
#define HSCL_PHASE_STEP_FRA_WID         24

/* #define VDIN_HSC_INI_CTRL                          0x120d */
/* repeatedly decimation of pixel #0 of each line? */
#define HSCL_RPT_P0_NUM_BIT             29
#define HSCL_RPT_P0_NUM_WID             2
/* if rev>rpt_p0+1, then start decimation upon ini_phase? */
#define HSCL_INI_RCV_NUM_BIT            24
#define HSCL_INI_RCV_NUM_WID            5
/* which one every some pixels is decimated */
#define HSCL_INI_PHASE_BIT              0
#define HSCL_INI_PHASE_WID              24


/* #define VDIN_COM_STATUS2                           0x120e */
/* Read only */
#define VDI7_FIFO_OVFL_BIT              23  /* vdi7 fifo overflow */
#define VDI7_FIFO_OVFL_WID              1
#define VDI7_ASFIFO_CNT_BIT             16  /* vdi7_asfifo_cnt */
#define VDI7_ASFIFO_CNT_WID             6
#define VDI6_FIFO_OVFL_BIT              15  /* vdi6 fifo overflow */
#define VDI6_FIFO_OVFL_WID              1
#define VDI6_ASFIFO_CNT_BIT             8  /* vdi6_asfifo_cnt */
#define VDI6_ASFIFO_CNT_WID             6

#define VDI5_FIFO_OVFL_BIT              7  /* vdi5 fifo overflow */
#define VDI5_FIFO_OVFL_WID              1
#define VDI5_ASFIFO_CNT_BIT             0  /* vdi5_asfifo_cnt */
#define VDI5_ASFIFO_CNT_WID             6

/* #define VDIN_ASFIFO_CTRL2                          0x120f */
#define ASFIFO_DECIMATION_SYNC_WITH_DE_BIT        25
#define ASFIFO_DECIMATION_SYNC_WITH_DE_WID        1
#define ASFIFO_DECIMATION_DE_EN_BIT               24
#define ASFIFO_DECIMATION_DE_EN_WID               1
#define ASFIFO_DECIMATION_PHASE_BIT               20
 /* which counter value used to decimate */
#define ASFIFO_DECIMATION_PHASE_WID               4
#define ASFIFO_DECIMATION_NUM_BIT                 16
/* 0: not decimation, 1: decimation 2, 2: decimation 3 ... */
#define ASFIFO_DECIMATION_NUM_WID                 4
#define VDI5_ASFIFO_CTRL_BIT					  0
#define VDI5_ASFIFO_CTRL_WID					  8
#define ASFIFO5_DE_EN_BIT                         7
#define ASFIFO5_DE_EN_WID                         1
#define ASFIFO5_GO_FLD_EN_BIT                     6
#define ASFIFO5_GO_FLD_EN_WID                     1
#define ASFIFO5_GO_LN_EN_BIT                      5
#define ASFIFO5_GO_LN_EN_WID                      1
#define ASFIFO5_NEG_ACTIVE_IN_VS_BIT              4
#define ASFIFO5_NEG_ACTIVE_IN_VS_WID              1
#define ASFIFO5_NEG_ACTIVE_IN_HS_BIT              3
#define ASFIFO5_NEG_ACTIVE_IN_HS_WID              1
#define ASFIFO5_VS_SOFT_RST_FIFO_EN_BIT           2
#define ASFIFO5_VS_SOFT_RST_FIFO_EN_WID           1
#define ASFIFO5_OVFL_STATUS_CLR_BIT               1
#define ASFIFO5_OVFL_STATUS_CLR_WID               1
#define ASFIFO5_SOFT_RST_BIT                      0
/* write 1 & then 0 to reset */
#define ASFIFO5_SOFT_RST_WID                      1

/* #define VDIN_MATRIX_CTRL                        0x1210 */
#define VDIN_MATRIX0_BYPASS_BIT             9/* 1:bypass 0:pass */
#define VDIN_MATRIX0_BYPASS_WID             1
#define VDIN_MATRIX1_BYPASS_BIT             8
#define VDIN_MATRIX1_BYPASS_WID             1
#define VDIN_HIGHLIGHT_EN_BIT               7
#define VDIN_HIGHLIGHT_EN_WID               1
/* 1: probe pixel data after matrix, 0:probe pixel data before matrix */
#define VDIN_PROBE_POST_BIT                 6
#define VDIN_PROBE_POST_WID                 1
/* 00: select matrix0, 01: select matrix1,otherwise select nothing */
#define VDIN_PROBE_SEL_BIT                  4
#define VDIN_PROBE_SEL_WID                  2
/* 00: select mat0, 01: select mat1, otherwise slect nothing */
#define VDIN_MATRIX_COEF_INDEX_BIT          2
#define VDIN_MATRIX_COEF_INDEX_WID          2
/* Bit 1   mat1 conversion matrix enable */
#define VDIN_MATRIX1_EN_BIT                 1
#define VDIN_MATRIX1_EN_WID                 1
/* Bit 0   mat0 conversion matrix enable */
#define VDIN_MATRIX_EN_BIT                  0
#define VDIN_MATRIX_EN_WID                  1

/* #define VDIN_MATRIX_COEF00_01                   0x1211 */
#define MATRIX_C00_BIT                  16
#define MATRIX_C00_WID                  13   /* s2.10 */
#define MATRIX_C01_BIT                  0
#define MATRIX_C01_WID                  13   /* s2.10 */

/* #define VDIN_MATRIX_COEF02_10                   0x1212 */
#define MATRIX_C02_BIT                  16
#define MATRIX_C02_WID                  13   /* s2.10 */
#define MATRIX_C10_BIT                  0
#define MATRIX_C10_WID                  13   /* s2.10 */

/* #define VDIN_MATRIX_COEF11_12                   0x1213 */
#define MATRIX_C11_BIT                  16
#define MATRIX_C11_WID                  13   /* s2.10 */
#define MATRIX_C12_BIT                  0
#define MATRIX_C12_WID                  13   /* s2.10 */

/* #define VDIN_MATRIX_COEF20_21                   0x1214 */
#define MATRIX_C20_BIT                  16
#define MATRIX_C20_WID                  13   /* s2.10 */
#define MATRIX_C21_BIT                  0
#define MATRIX_C21_WID                  13   /* s2.10 */

/* #define VDIN_MATRIX_COEF22                      0x1215 */
#define MATRIX_C22_BIT                  0
#define MATRIX_C22_WID                  13   /* s2.10 */

/* #define VDIN_MATRIX_OFFSET0_1                   0x1216 */
#define MATRIX_OFFSET0_BIT              16
#define MATRIX_OFFSET0_WID              11   /* s8.2 */
#define MATRIX_OFFSET1_BIT              0
#define MATRIX_OFFSET1_WID              11   /* s8.2 */

/* #define VDIN_MATRIX_OFFSET2                     0x1217 */
#define MATRIX_OFFSET2_BIT              0
#define MATRIX_OFFSET2_WID              11   /* s8.2 */

/* #define VDIN_MATRIX_PRE_OFFSET0_1               0x1218 */
#define MATRIX_PRE_OFFSET0_BIT          16
#define MATRIX_PRE_OFFSET0_WID          11   /* s8.2 */
#define MATRIX_PRE_OFFSET1_BIT          0
#define MATRIX_PRE_OFFSET1_WID          11   /* s8.2 */

/* #define VDIN_MATRIX_PRE_OFFSET2                 0x1219 */
#define MATRIX_PRE_OFFSET2_BIT          0
#define MATRIX_PRE_OFFSET2_WID          11   /* s8.2 */

/* #define VDIN_LFIFO_CTRL                         0x121a */
#define LFIFO_BUF_SIZE_BIT              0
#define LFIFO_BUF_SIZE_WID              12

/* #define VDIN_COM_GCLK_CTRL                      0x121b */
#define COM_GCLK_BLKBAR_BIT             14
#define COM_GCLK_BLKBAR_WID             2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_HIST_BIT               12
#define COM_GCLK_HIST_WID               2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_LFIFO_BIT              10
#define COM_GCLK_LFIFO_WID              2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_MATRIX_BIT             8
#define COM_GCLK_MATRIX_WID             2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_HSCL_BIT               6
#define COM_GCLK_HSCL_WID               2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_PRE_HSCL_BIT           4
#define COM_GCLK_PRE_HSCL_WID           2    /* 00: auto, 01: off, 1x: on */
#define COM_GCLK_TOP_BIT                2
#define COM_GCLK_TOP_WID                2    /* 00: auto, 01: off, 1x: on */
/* Caution !!! never turn it off, otherwise no way to
 * wake up VDIN unless power reset
 */
#define COM_GCLK_REG_BIT                0
#define COM_GCLK_REG_WID                1    /* 0: auto,  1: off. Caution !!! */


/* #define VDIN_INTF_WIDTHM1                        0x121c */
/* before the cut window function, after the de decimation function */
#define VDIN_INTF_WIDTHM1_BIT           0
#define VDIN_INTF_WIDTHM1_WID           13

/* #define VDIN_LFIFO_URG_CTRL                       0x121e */
/*Bit 15          default== 0, urgent_ctrl_en
 *Bit 14          default== 0, urgent_wr, if true for write buffer
 *Bit 13          default== 0, out_inv_en
 *Bit 12          default == 0, urgent_ini_value
 *Bit 11:6        default == 0, up_th  up threshold
 *Bit 5:0         default == 0, dn_th  dn threshold
 */
#define VDIN_LFIFO_URG_CTRL_EN_BIT      15
#define VDIN_LFIFO_URG_CTRL_EN_WID      1
#define VDIN_LFIFO_URG_WR_EN_BIT        14
#define VDIN_LFIFO_URG_WR_EN_WID        1
#define VDIN_LFIFO_OUT_INV_EN_BIT       13
#define VDIN_LFIFO_OUT_INV_EN_WID       1
#define VDIN_LFIFO_URG_INI_BIT          12
#define VDIN_LFIFO_URG_INI_WID          1
#define VDIN_LFIFO_URG_UP_TH_BIT        6
#define VDIN_LFIFO_URG_UP_TH_WID        6
#define VDIN_LFIFO_URG_DN_TH_BIT        0
#define VDIN_LFIFO_URG_DN_TH_WID        6


/* #define VDIN_WR_CTRL2                           0x121f */
/*1: enable WR 10 bit mode, 0: disable WR 10 bit mode*/
#define VDIN_WR_10BIT_MODE_BIT		19
#define VDIN_WR_10BIT_MODE_WID		1
/* data_ext_en 1:send out data if req was interrupt by soft reset */
/* 0:normal mode */
#define VDIN_WR_DATA_EXT_EN_BIT		18
#define VDIN_WR_DATA_EXT_EN_WID		1
/*0: 1 word in 1burst, 1: 2 words in 1burst;
 *10: 4 words in 1burst; 11: reserved
 */
#define VDIN_WR_BURST_MODE_BIT		12
#define VDIN_WR_BURST_MODE_WID		4
/* 1: discard data before line fifo, 0: normal mode */
#define DISCARD_BEF_LINE_FIFO_BIT		8
#define DISCARD_BEF_LINE_FIFO_WID       1
/* Write chroma canvas address */
#define WRITE_CHROMA_CANVAS_ADDR_BIT	0
#define WRITE_CHROMA_CANVAS_ADDR_WID   8

/* #define VDIN_WR_CTRL                            0x1220 */

/* Applicable only bit[13:12]=0 or 10. */
/* 0: Output every even pixels' CbCr; */
/* 1: Output every odd pixels' CbCr; */
/* 10: Output an average value per even&odd pair of pixels; */
/* 11: Output all CbCr. (This does NOT apply to bit[13:12]=0 -- 4:2:2 mode.) */
#define HCONV_MODE_BIT                  30
#define HCONV_MODE_WID                  2
/* 1:disable vid_wr_mif clock gating function */
#define NO_CLOCK_GATE_BIT               29
#define NO_CLOCK_GATE_WID                1
#define WR_RESPONSE_CNT_CLR_BIT         28
#define WR_RESPONSE_CNT_CLR_WID         1
#define EOL_SEL_BIT                     27
#define EOL_SEL_WID                     1
#define VCP_NR_EN_BIT                   26/* ONLY VDIN0 */
#define VCP_NR_EN_WID                   1
#define VCP_WR_EN_BIT                   25/* ONLY VDIN0 */
#define VCP_WR_EN_WID                   1
#define VCP_IN_EN_BIT                   24/* ONLY VDIN0 */
#define VCP_IN_EN_WID                   1
/* #define WR_OUT_CTRL_BIT                 24 ? */
/* #define WR_OUT_CTRL_WID                 8    //directly send out */
#define FRAME_SOFT_RST_EN_BIT           23
#define FRAME_SOFT_RST_EN_WID           1
/* reset LFIFO on VS (Go_field) */
#define LFIFO_SOFT_RST_EN_BIT           22
#define LFIFO_SOFT_RST_EN_WID           1
#define DIRECT_DONE_CLR_BIT             21   /* used by other modules */
#define DIRECT_DONE_CLR_WID             1
#define NR_DONE_CLR_BIT                 20   /* used by other modules */
#define NR_DONE_CLR_WID                 1
/* only [13:12]=10;0 output cbcr(nv12);1 output cbcr(nv21) */
#define SWAP_CBCR_BIT                      18
#define SWAP_CBCR_WID                      1
/* 0: Output even lines' CbCr; 01: Output odd lines' CbCr;
 * 10: Reserved; 11: Output all CbCr.
 */
#define VCONV_MODE_BIT                     16
#define VCONV_MODE_WID                    2
/* 0: 422;1: 444;10:Y to luma canvas cbcr to chroma canvas for NV12/21 */
#define WR_FMT_BIT                      12
#define WR_FMT_WID                     2
/* vdin_wr_canvas = vdin_wr_canvas_dbuf_en ? wr_canvas_shadow :wr_canvas;  */
/* shadow is latch by go_field */
#define WR_CANVAS_DOUBLE_BUF_EN_BIT            11
#define WR_CANVAS_DOUBLE_BUF_EN_WID            1
#define WR_REQ_URGENT_BIT               9
#define WR_REQ_URGENT_WID               1    /* directly send out */
#define WR_REQ_EN_BIT                   8
#define WR_REQ_EN_WID                   1    /* directly send out */
#define WR_CANVAS_BIT                   0
#define WR_CANVAS_WID                   8

/* #define VDIN_WR_H_START_END                        0x1221 */

#define HORIZONTAL_REVERSE_BIT          29/* if true horizontal reverse */
#define HORIZONTAL_REVERSE_WID         1
#define WR_HSTART_BIT                   16
#define WR_HSTART_WID                   13   /* directly send out */
#define WR_HEND_BIT                     0
#define WR_HEND_WID                     13   /* directly send out */

/* #define VDIN_WR_V_START_END                        0x1222 */

#define VERTICAL_REVERSE_BIT          29/* if true vertical reverse */
#define VERTICAL_REVERSE_WID         1
#define WR_VSTART_BIT                   16
#define WR_VSTART_WID                   13   /* directly send out */
#define WR_VEND_BIT                     0
#define WR_VEND_WID                     13  /* directly send out */

/* #define VDIN_VSC_PHASE_STEP                       0x1223 */
#define INTEGER_PORTION_BIT            20
#define INTEGER_PORTION_WID           5
#define FRACTION_PORTION_BIT            0
#define FRACTION_PORTION_WID           20

/* #define VDIN_VSC_INI_CTRL                             0x1224 */
#define VSC_EN_BIT                                  23
#define VSC_EN_WID                                 1
#define VSC_PHASE0_ALWAYS_EN_BIT      21/* to be 1 when scale up */
#define VSC_PHASE0_ALWAYS_EN_WID     1
#define INI_SKIP_LINE_NUM_BIT                  16
#define INI_SKIP_LINE_NUM_WID                 5
#define VSCALER_INI_PHASE_BIT                0
#define VSCALER_INI_PHASE_WID               16

/* #define VDIN_SCIN_HEIGHTM1                          0x1225 */
/* Bit 12:0, scaler input height minus 1 */
#define SCALER_INPUT_HEIGHT_BIT            0
#define SCALER_INPUT_HEIGHT_WID           13

/* Bit 28:16, vshrk input height minus 1 */
#define VSHRK_INPUT_HEIGHT_BIT           16
#define VSHRK_INPUT_HEIGHT_WID           13

/* #define `define VDIN_DUMMY_DATA                0x1226 */
#define DUMMY_COMPONENT0_BIT                16
#define DUMMY_COMPONENT0_WID               8
#define DUMMY_COMPONENT1_BIT                8
#define DUMMY_COMPONENT1_WID               8
#define DUMMY_COMPONENT2_BIT                0
#define DUMMY_COMPONENT2_WID               8

/* #define VDIN_MATRIX_PROBE_COLOR           0x1228 */
/* Read only */
#define COMPONENT0_PROBE_COLOR_BIT                20
#define COMPONENT0_PROBE_COLOR_WID                10
#define COMPONENT1_PROBE_COLOR_BIT                10
#define COMPONENT1_PROBE_COLOR_WID                10
#define COMPONENT2_PROBE_COLOR_BIT                0
#define COMPONENT2_PROBE_COLOR_WID                10

/* #define VDIN_MATRIX_HL_COLOR                0x1229 */
#define COMPONENT0_HL_COLOR_BIT                   16
#define COMPONENT0_HL_COLOR_WID                   8
#define COMPONENT1_HL_COLOR_BIT                   8
#define COMPONENT1_HL_COLOR_WID                   8
#define COMPONENT2_HL_COLOR_BIT                   0
#define COMPONENT2_HL_COLOR_WID                   8

/* #define VDIN_MATRIX_PROBE_POS               0x122a */
#define PROBE_POS_X_BIT                           16
#define PROBE_POS_X_WID                           13
#define PROBE_POX_Y_BIT                           0
#define PROBE_POX_Y_WID                           13
/* #define VDIN_HIST_CTRL                             0x1230 */
/* Bit 10:9  ldim_stts_din_sel, 00: from matrix0 dout,
 *  01: from vsc_dout, 10: from matrix1 dout, 11: form matrix1 din
 */
#define LDIM_STTS_DIN_SEL_BIT                     9
#define LDIM_STTS_DIN_SEL_WID                     2
#define LDIM_STTS_EN_BIT                          8
#define LDIM_STTS_EN_WID                          1
/* 00: from matrix0 dout,  01: from vsc_dout,
 * 10: from matrix1 dout, 11: form matrix1 din
 */
#define HIST_DIN_SEL_BIT                          2
#define HIST_DIN_SEL_WID                          2

/* #define VDIN_CHROMA_ADDR_PORT	      0x122b */

/* #define VDIN_CHROMA_DATA_PORT	      0x122c */

/* #define VDIN_CM_BRI_CON_CTRL		      0x122d */
#define CM_TOP_EN_BIT				  28
#define CM_TOP_EN_WID				  1
#define BRI_CON_EN_BIT				  27
#define BRI_CON_EN_WID				  1
#define SED_YUVINVEN_BIT			  24
#define SED_YUVINVEN_WID			  3
#define REG_ADJ_BRI_BIT				  12
#define REG_ADJ_BRI_WID				  11
#define REG_ADJ_CON_BIT				  0
#define REG_ADJ_CON_WID				  12

/* #define VDIN_GO_LINE_CTRL		     0x122f */
#define CLK_CYC_CNT_CLR_BIT                       17
#define CLK_CYC_CNT_CLR_WID                       1
/* Bit 17  clk_cyc_cnt_clr, if true, clear this register */
#define LINE_CNT_SRC_SEL_BIT                      16
#define LINE_CNT_SRC_SEL_WID                      1
/* Bit 16 if true, use vpu clock to count one line,
 * otherwise use actually hsync to count line_cnt
 */
/* Bit 15:0   line width using vpu clk */
#define LINE_WID_USING_VPU_CLK_BIT                0
#define LINE_WID_USING_VPU_CLK_WID                16

/* #define VDIN_HIST_CTRL                             0x1230 */
/* the total pixels = VDIN_HISTXX*(2^(VDIN_HIST_POW+3)) */
#define HIST_POW_BIT                    5
#define HIST_POW_WID                    2
/* Bit 3:2   hist_din_sel    the source used for hist statistics.
 *  00: from matrix0 dout,  01: from vsc_dout,
 *  10: from matrix1 dout, 11: form matrix1 din
 */
#define HIST_HIST_DIN_SEL_BIT           2
#define HIST_HIST_DIN_SEL_WID           2
/* Histgram range: 0: full picture, 1: histgram window
 * defined by VDIN_HIST_H_START_END & VDIN_HIST_V_START_END
 */
#define HIST_WIN_EN_BIT                 1
#define HIST_WIN_EN_WID                 1
/* Histgram readback: 0: disable, 1: enable */
#define HIST_RD_EN_BIT                  0
#define HIST_RD_EN_WID                  1

/* #define VDIN_HIST_H_START_END                   0x1231 */
#define HIST_HSTART_BIT                 16
#define HIST_HSTART_WID                 13
#define HIST_HEND_BIT                   0
#define HIST_HEND_WID                   13

/* #define VDIN_HIST_V_START_END                   0x1232 */
#define HIST_VSTART_BIT                 16
#define HIST_VSTART_WID                 13
#define HIST_VEND_BIT                   0
#define HIST_VEND_WID                   13

/* #define VDIN_HIST_MAX_MIN                       0x1233 */
#define HIST_MAX_BIT                    8
#define HIST_MAX_WID                    8
#define HIST_MIN_BIT                    0
#define HIST_MIN_WID                    8

/* #define VDIN_HIST_SPL_VAL                       0x1234 */
#define HIST_LUMA_SUM_BIT               0
#define HIST_LUMA_SUM_WID               32

 /* the total calculated pixels */
/* #define VDIN_HIST_SPL_PIX_CNT                   0x1235 */
#define HIST_PIX_CNT_BIT                0
#define HIST_PIX_CNT_WID                22

/* the total chroma value */
/* #define VDIN_HIST_CHROMA_SUM                    0x1236 */
#define HIST_CHROMA_SUM_BIT             0
#define HIST_CHROMA_SUM_WID             32

/* #define VDIN_DNLP_HIST00                        0x1237 */
#define HIST_ON_BIN_01_BIT              16
#define HIST_ON_BIN_01_WID              16
#define HIST_ON_BIN_00_BIT              0
#define HIST_ON_BIN_00_WID              16

/* #define VDIN_DNLP_HIST01                        0x1238 */
#define HIST_ON_BIN_03_BIT              16
#define HIST_ON_BIN_03_WID              16
#define HIST_ON_BIN_02_BIT              0
#define HIST_ON_BIN_02_WID              16

/* #define VDIN_DNLP_HIST02                        0x1239 */
#define HIST_ON_BIN_05_BIT              16
#define HIST_ON_BIN_05_WID              16
#define HIST_ON_BIN_04_BIT              0
#define HIST_ON_BIN_04_WID              16

/* #define VDIN_DNLP_HIST03                        0x123a */
#define HIST_ON_BIN_07_BIT              16
#define HIST_ON_BIN_07_WID              16
#define HIST_ON_BIN_06_BIT              0
#define HIST_ON_BIN_06_WID              16

/* #define VDIN_DNLP_HIST04                        0x123b */
#define HIST_ON_BIN_09_BIT              16
#define HIST_ON_BIN_09_WID              16
#define HIST_ON_BIN_08_BIT              0
#define HIST_ON_BIN_08_WID              16

/* #define VDIN_DNLP_HIST05                        0x123c */
#define HIST_ON_BIN_11_BIT              16
#define HIST_ON_BIN_11_WID              16
#define HIST_ON_BIN_10_BIT              0
#define HIST_ON_BIN_10_WID              16

/* #define VDIN_DNLP_HIST06                        0x123d */
#define HIST_ON_BIN_13_BIT              16
#define HIST_ON_BIN_13_WID              16
#define HIST_ON_BIN_12_BIT              0
#define HIST_ON_BIN_12_WID              16

/* #define VDIN_DNLP_HIST07                        0x123e */
#define HIST_ON_BIN_15_BIT              16
#define HIST_ON_BIN_15_WID              16
#define HIST_ON_BIN_14_BIT              0
#define HIST_ON_BIN_14_WID              16

/* #define VDIN_DNLP_HIST08                        0x123f */
#define HIST_ON_BIN_17_BIT              16
#define HIST_ON_BIN_17_WID              16
#define HIST_ON_BIN_16_BIT              0
#define HIST_ON_BIN_16_WID              16

/* #define VDIN_DNLP_HIST09                        0x1240 */
#define HIST_ON_BIN_19_BIT              16
#define HIST_ON_BIN_19_WID              16
#define HIST_ON_BIN_18_BIT              0
#define HIST_ON_BIN_18_WID              16

/* #define VDIN_DNLP_HIST10                        0x1241 */
#define HIST_ON_BIN_21_BIT              16
#define HIST_ON_BIN_21_WID              16
#define HIST_ON_BIN_20_BIT              0
#define HIST_ON_BIN_20_WID              16

/* #define VDIN_DNLP_HIST11                        0x1242 */
#define HIST_ON_BIN_23_BIT              16
#define HIST_ON_BIN_23_WID              16
#define HIST_ON_BIN_22_BIT              0
#define HIST_ON_BIN_22_WID              16

/* #define VDIN_DNLP_HIST12                        0x1243 */
#define HIST_ON_BIN_25_BIT              16
#define HIST_ON_BIN_25_WID              16
#define HIST_ON_BIN_24_BIT              0
#define HIST_ON_BIN_24_WID              16

/* #define VDIN_DNLP_HIST13                        0x1244 */
#define HIST_ON_BIN_27_BIT              16
#define HIST_ON_BIN_27_WID              16
#define HIST_ON_BIN_26_BIT              0
#define HIST_ON_BIN_26_WID              16

/* #define VDIN_DNLP_HIST14                        0x1245 */
#define HIST_ON_BIN_29_BIT              16
#define HIST_ON_BIN_29_WID              16
#define HIST_ON_BIN_28_BIT              0
#define HIST_ON_BIN_28_WID              16

/* #define VDIN_DNLP_HIST15                        0x1246 */
#define HIST_ON_BIN_31_BIT              16
#define HIST_ON_BIN_31_WID              16
#define HIST_ON_BIN_30_BIT              0
#define HIST_ON_BIN_30_WID              16

/* #define VDIN_DNLP_HIST16                        0x1247 */
#define HIST_ON_BIN_33_BIT              16
#define HIST_ON_BIN_33_WID              16
#define HIST_ON_BIN_32_BIT              0
#define HIST_ON_BIN_32_WID              16

/* #define VDIN_DNLP_HIST17                        0x1248 */
#define HIST_ON_BIN_35_BIT              16
#define HIST_ON_BIN_35_WID              16
#define HIST_ON_BIN_34_BIT              0
#define HIST_ON_BIN_34_WID              16

/* #define VDIN_DNLP_HIST18                        0x1249 */
#define HIST_ON_BIN_37_BIT              16
#define HIST_ON_BIN_37_WID              16
#define HIST_ON_BIN_36_BIT              0
#define HIST_ON_BIN_36_WID              16

/* #define VDIN_DNLP_HIST19                        0x124a */
#define HIST_ON_BIN_39_BIT              16
#define HIST_ON_BIN_39_WID              16
#define HIST_ON_BIN_38_BIT              0
#define HIST_ON_BIN_38_WID              16

/* #define VDIN_DNLP_HIST20                        0x124b */
#define HIST_ON_BIN_41_BIT              16
#define HIST_ON_BIN_41_WID              16
#define HIST_ON_BIN_40_BIT              0
#define HIST_ON_BIN_40_WID              16

/* #define VDIN_DNLP_HIST21                        0x124c */
#define HIST_ON_BIN_43_BIT              16
#define HIST_ON_BIN_43_WID              16
#define HIST_ON_BIN_42_BIT              0
#define HIST_ON_BIN_42_WID              16

/* #define VDIN_DNLP_HIST22                        0x124d */
#define HIST_ON_BIN_45_BIT              16
#define HIST_ON_BIN_45_WID              16
#define HIST_ON_BIN_44_BIT              0
#define HIST_ON_BIN_44_WID              16

/* #define VDIN_DNLP_HIST23                        0x124e */
#define HIST_ON_BIN_47_BIT              16
#define HIST_ON_BIN_47_WID              16
#define HIST_ON_BIN_46_BIT              0
#define HIST_ON_BIN_46_WID              16

/* #define VDIN_DNLP_HIST24                        0x124f */
#define HIST_ON_BIN_49_BIT              16
#define HIST_ON_BIN_49_WID              16
#define HIST_ON_BIN_48_BIT              0
#define HIST_ON_BIN_48_WID              16

/* #define VDIN_DNLP_HIST25                        0x1250 */
#define HIST_ON_BIN_51_BIT              16
#define HIST_ON_BIN_51_WID              16
#define HIST_ON_BIN_50_BIT              0
#define HIST_ON_BIN_50_WID              16

/* #define VDIN_DNLP_HIST26                        0x1251 */
#define HIST_ON_BIN_53_BIT              16
#define HIST_ON_BIN_53_WID              16
#define HIST_ON_BIN_52_BIT              0
#define HIST_ON_BIN_52_WID              16

/* #define VDIN_DNLP_HIST27                        0x1252 */
#define HIST_ON_BIN_55_BIT              16
#define HIST_ON_BIN_55_WID              16
#define HIST_ON_BIN_54_BIT              0
#define HIST_ON_BIN_54_WID              16

/* #define VDIN_DNLP_HIST28                        0x1253 */
#define HIST_ON_BIN_57_BIT              16
#define HIST_ON_BIN_57_WID              16
#define HIST_ON_BIN_56_BIT              0
#define HIST_ON_BIN_56_WID              16

/* #define VDIN_DNLP_HIST29                        0x1254 */
#define HIST_ON_BIN_59_BIT              16
#define HIST_ON_BIN_59_WID              16
#define HIST_ON_BIN_58_BIT              0
#define HIST_ON_BIN_58_WID              16

/* #define VDIN_DNLP_HIST30                        0x1255 */
#define HIST_ON_BIN_61_BIT              16
#define HIST_ON_BIN_61_WID              16
#define HIST_ON_BIN_60_BIT              0
#define HIST_ON_BIN_60_WID              16

/* #define VDIN_DNLP_HIST31                        0x1256 */
#define HIST_ON_BIN_63_BIT              16
#define HIST_ON_BIN_63_WID              16
#define HIST_ON_BIN_62_BIT              0
#define HIST_ON_BIN_62_WID              16

/* #define VDIN_LDIM_STTS_HIST_REGION_IDX       0x1257 */
#define LOCAL_DIM_STATISTIC_EN_BIT          31
#define LOCAL_DIM_STATISTIC_EN_WID         1
#define EOL_EN_BIT                                          28
#define EOL_EN_WID                                        1
#define VLINE_OVERLAP_NUMBER_BIT        25
#define VLINE_OVERLAP_NUMBER_WID       3
/* 0: 17 pix, 1: 9 pix, 2: 5 pix, 3: 3 pix, 4: 0 pix */
#define HLINE_OVERLAP_NUMBER_BIT        22
#define HLINE_OVERLAP_NUMBER_WID       3
#define LPF_BEFORE_STATISTIC_EN_BIT    20
#define LPF_BEFORE_STATISTIC_EN_WID    1
/* region H/V position index, refer to VDIN_LDIM_STTS_HIST_SET_REGION */
#define BLK_HV_POS_IDXS_BIT                     16
#define BLK_HV_POS_IDXS_WID                    4
#define REGION_RD_INDEX_INC_BIT             15
#define REGION_RD_INDEX_INC_WID            1
#define REGION_RD_INDEX_BIT                      0
#define REGION_RD_INDEX_WID                     7

/* # VDIN_LDIM_STTS_HIST_SET_REGION                    0x1258 */
/* Bit 28:0, if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h0:
 * read/write hvstart0
 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h1: read/write hend01 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h2: read/write vend01 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h3: read/write hend23 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h4: read/write vend23 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h5: read/write hend45 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h6: read/write vend45 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'd7: read/write hend67 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h8: read/write vend67 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'h9: read/write hend89 */
/* if VDIN_LDIM_STTS_HIST_REGION_IDX[19:16] == 5'ha: read/write vend89 */
/* hvstart0, Bit 28:16 row0 vstart, Bit 12:0 col0 hstart */
/* hend01, Bit 28:16 col1 hend, Bit 12:0 col0 hend */
/* vend01, Bit 28:16 row1 vend, Bit 12:0 row0 vend */
/* hend23, Bit 28:16 col3 hend, Bit 12:0 col2 hend */
/* vend23, Bit 28:16 row3 vend, Bit 12:0 row2 vend */
/* hend45, Bit 28:16 col5 hend, Bit 12:0 col4 hend */
/* vend45, Bit 28:16 row5 vend, Bit 12:0 row4 vend */
/* hend67, Bit 28:16 col7 hend, Bit 12:0 col6 hend */
/* vend67, Bit 28:16 row7 vend, Bit 12:0 row6 vend */
/* hend89, Bit 28:16 col9 hend, Bit 12:0 col8 hend */
/* vend89, Bit 28:16 row9 vend, Bit 12:0 row8 vend */
#define HI_16_28_BIT                        16
#define HI_16_28_WID                       13
#define LOW_0_12_BIT                      0
#define LOW_0_12_WID                    13

/* #define VDIN_LDIM_STTS_HIST_READ_REGION           0x1259 */
/* REGION STATISTIC DATA READ OUT PORT, */
#define MAX_COMP2_BIT                   20
#define MAX_COMP2_WID                  10
#define MAX_COMP1_BIT                   10
#define MAX_COMP1_WID                  10
#define MAX_COMP0_BIT                   0
#define MAX_COMP0_WID                  10


/* #define VDIN_MEAS_CTRL0                            0x125a */
 /* write 1 & then 0 to reset */
#define MEAS_RST_BIT                    18
#define MEAS_RST_WID                    1
/* make hs ,vs at lest 12 pulse wide */
#define MEAS_WIDEN_HS_VS_EN_BIT         17
#define MEAS_WIDEN_HS_VS_EN_WID         1
 /* vsync total counter always accumulating enable */
#define MEAS_VS_TOTAL_CNT_EN_BIT        16
#define MEAS_VS_TOTAL_CNT_EN_WID        1
/* 0: null, 1: vdi1, 2: vdi2, 3: vdi3, 4:vdi4,
 * 5:vdi5,for m6 6:vdi6,7:vdi7 8:vdi8-isp
 */
#define MEAS_HS_VS_SEL_BIT              12
#define MEAS_HS_VS_SEL_WID              4
/* define how many VS span need to measure */
#define MEAS_VS_SPAN_BIT                4
#define MEAS_VS_SPAN_WID                8
/* select which HS counter/range */
#define MEAS_HS_INDEX_BIT               0
#define MEAS_HS_INDEX_WID               3



/* #define VDIN_MEAS_VS_COUNT_HI                      0x125b // read only */
/* after every VDIN_MEAS_VS_SPAN number of VS pulses,
 * VDIN_MEAS_IND_TOTAL_COUNT_N++
 */
#define MEAS_IND_VS_TOTAL_CNT_N_BIT     16
#define MEAS_IND_VS_TOTAL_CNT_N_WID     4
#define MEAS_VS_TOTAL_CNT_HI_BIT        0  /* vsync_total_counter[47:32] */
#define MEAS_VS_TOTAL_CNT_HI_WID        16



/* #define VDIN_MEAS_VS_COUNT_LO                      0x125c // read only */
#define MEAS_VS_TOTAL_CNT_LO_BIT        0  /* vsync_total_counter[31:0] */
#define MEAS_VS_TOTAL_CNT_LO_WID        32

/* 1st/2nd/3rd/4th hs range according to VDIN_MEAS_HS_INDEX */
/* #define VDIN_MEAS_HS_RANGE                         0x125d */
#define MEAS_HS_RANGE_CNT_START_BIT     16
#define MEAS_HS_RANGE_CNT_START_WID     13
#define MEAS_HS_RANGE_CNT_END_BIT       0
#define MEAS_HS_RANGE_CNT_END_WID       13

/* hs count as per 1st/2nd/3rd/4th hs range according to VDIN_MEAS_HS_INDEX */
/* #define VDIN_MEAS_HS_COUNT                         0x125e // read only */
#define MEAS_HS_CNT_BIT                 0
#define MEAS_HS_CNT_WID                 24

/* #define VDIN_BLKBAR_CTRL1                          0x125f */
#define BLKBAR_WHITE_EN_BIT             8
#define BLKBAR_WHITE_EN_WID             1
#define BLKBAR_WHITE_LVL_BIT            0
#define BLKBAR_WHITE_LVL_WID            8

/* #define VDIN_BLKBAR_CTRL0                       0x1260 */

 /* threshold to judge a black point */
#define BLKBAR_BLK_LVL_BIT              24
#define BLKBAR_BLK_LVL_WID              8

/* left and right region width */
#define BLKBAR_H_WIDTH_BIT              8
#define BLKBAR_H_WIDTH_WID              13
/* select yin or uin or vin to be the valid input */
#define BLKBAR_COMP_SEL_BIT             5
#define BLKBAR_COMP_SEL_WID             3
/* sw statistic of black pixels of each block,
 *1: search once, 0: search continuously till the exact edge
 */
#define BLKBAR_SW_STAT_EN_BIT           4
#define BLKBAR_SW_STAT_EN_WID           1
#define BLKBAR_DET_SOFT_RST_N_BIT       3
#define BLKBAR_DET_SOFT_RST_N_WID       1    /* write 0 & then 1 to reset */
/* 0: matrix_dout, 1: hscaler_dout, 2/3: pre-hscaler_din */
#define BLKBAR_DIN_SEL_BIT              1
#define BLKBAR_DIN_SEL_WID              2
/* blkbar_din_srdy blkbar_din_rrdy  enable */
#define BLKBAR_DET_TOP_EN_BIT           0
#define BLKBAR_DET_TOP_EN_WID           1

/* #define VDIN_BLKBAR_H_START_END                    0x1261 */
#define BLKBAR_HSTART_BIT               16
#define BLKBAR_HSTART_WID               13   /* Left region start */
#define BLKBAR_HEND_BIT                 0
#define BLKBAR_HEND_WID                 13   /* Right region end */

/* #define VDIN_BLKBAR_V_START_END                    0x1262 */
#define BLKBAR_VSTART_BIT               16
#define BLKBAR_VSTART_WID               13
#define BLKBAR_VEND_BIT                 0
#define BLKBAR_VEND_WID                 13

/* #define VDIN_BLKBAR_CNT_THRESHOLD                  0x1263 */
/* black pixel number threshold to judge whether a block is totally black */
#define BLKBAR_CNT_TH_BIT               0
#define BLKBAR_CNT_TH_WID               20

/* #define VDIN_BLKBAR_ROW_TH1_TH2                    0x1264 */
/* white pixel number threshold of black line on top */
#define BLKBAR_ROW_TH1_BIT              16
#define BLKBAR_ROW_TH1_WID              13
/* white pixel number threshold of black line on bottom */
#define BLKBAR_ROW_TH2_BIT              0
#define BLKBAR_ROW_TH2_WID              13

/* #define VDIN_BLKBAR_IND_LEFT_START_END             0x1265 */
#define BLKBAR_LEFT_HSTART_BIT          16
#define BLKBAR_LEFT_HSTART_WID          13
#define BLKBAR_LEFT_HEND_BIT            0
#define BLKBAR_LEFT_HEND_WID            13

/* #define VDIN_BLKBAR_IND_RIGHT_START_END            0x1266 */
#define BLKBAR_RIGHT_HSTART_BIT         16
#define BLKBAR_RIGHT_HSTART_WID         13
#define BLKBAR_RIGHT_HEND_BIT           0
#define BLKBAR_RIGHT_HEND_WID           13

/* #define VDIN_BLKBAR_IND_LEFT1_CNT                  0x1267 */
/* Black pixels at left part of the left region */
#define BLKBAR_LEFT1_CNT_BIT            0
#define BLKBAR_LEFT1_CNT_WID            20

/* #define VDIN_BLKBAR_IND_LEFT2_CNT                  0x1268 */
/* Black pixels at right part of the left region */
#define BLKBAR_LEFT2_CNT_BIT            0
#define BLKBAR_LEFT2_CNT_WID            20

/* #define VDIN_BLKBAR_IND_RIGHT1_CNT                 0x1269 */
/* Black pixels at right part of the left region */
#define BLKBAR_RIGHT1_CNT_BIT           0
#define BLKBAR_RIGHT1_CNT_WID           20

/* #define VDIN_BLKBAR_IND_RIGHT2_CNT                 0x126a */
/* Black pixels at right part of the right region */
#define BLKBAR_RIGHT2_CNT_BIT           0
#define BLKBAR_RIGHT2_CNT_WID           20

/* #define VDIN_BLKBAR_STATUS0                        0x126b */
/* LEFT/RIGHT Black Bar detection done */
#define BLKBAR_DET_DONE_BIT             29
#define BLKBAR_DET_DONE_WID             1
#define BLKBAR_TOP_POS_BIT              16
#define BLKBAR_TOP_POS_WID              13
#define BLKBAR_BTM_POS_BIT              0
#define BLKBAR_BTM_POS_WID              13

/* #define VDIN_BLKBAR_STATUS1                        0x126c */
#define BLKBAR_LEFT_POS_BIT             16
#define BLKBAR_LEFT_POS_WID             13
#define BLKBAR_RIGHT_POS_BIT            0
#define BLKBAR_RIGHT_POS_WID            13


/* #define VDIN_WIN_H_START_END                       0x126d */
#define INPUT_WIN_H_START_BIT            16
#define INPUT_WIN_H_START_WID            13
#define INPUT_WIN_H_END_BIT              0
#define INPUT_WIN_H_END_WID              13

/* #define VDIN_WIN_V_START_END                       0x126e */
#define INPUT_WIN_V_START_BIT            16
#define INPUT_WIN_V_START_WID            13
#define INPUT_WIN_V_END_BIT              0
#define INPUT_WIN_V_END_WID              13

/* Bit 15:8 vdi7 asfifo_ctrl */
/* Bit 7:0 vdi6 asfifo_ctrl */
/* #define VDIN_ASFIFO_CTRL3                                 0x126f */
#define VDI9_ASFIFO_CTRL_BIT		    24
#define VDI9_ASFIFO_CTRL_WID            8
#define VDI8_ASFIFO_CTRL_BIT		    16
#define VDI8_ASFIFO_CTRL_WID            8
#define VDI7_ASFIFO_CTRL_BIT            8
#define VDI7_ASFIFO_CTRL_WID            8
#define VDI6_ASFIFO_CTRL_BIT            0
#define VDI6_ASFIFO_CTRL_WID            8

#define VDIN_VSHRK_EN_BIT	27
#define VDIN_VSHRK_EN_WID	1
#define VDIN_VSHRK_LPF_MODE_BIT	24
#define VDIN_VSHRK_LPF_MODE_WID	1
#define VDIN_VSHRK_MODE_BIT	25
#define VDIN_VSHRK_MODE_WID	2

/* Bit 3:2 vshrk_clk2_ctrl */
/* Bit 1:0 vshrk_clk1_ctrl */
/* #define VDIN_COM_GCLK_CTRL2    ((0x1270  << 2) + 0xd0100000) */
/* Bit 27 vshrk_en */
/* Bit 26:25 vshrk_mode */
/* Bit 24 vshrk_lpf_mode */
/* Bit 23:0 vshrk_dummy */
/* #define VDIN_VSHRK_CTRL    ((0x1271  << 2) + 0xd0100000) */
/* #define VDIN_DNLP_HIST32    ((0x1272  << 2) + 0xd0100000) */
/* Read only */
/* Bit 7, vdi9 fifo overflow */
/* Bit 5:0, vdi9_asfifo_cnt */
/* #define VDIN_COM_STATUS3    ((0x1273  << 2) + 0xd0100000) */

#define VDIN_FORCEGOLINE_EN_BIT		28
#define VDIN_WRREQUEST_EN_BT		8
#define VDIN_WRCTRLREG_PAUSE_BIT		10

/*#define VDIN_INTF_WIDTHM1*/
#define VDIN_FIX_NONSTDVSYNC_BIT	24
#define VDIN_FIX_NONSTDVSYNC_WID	2

/*#define VPU_ARB_URG_CTRL*/
#define VDIN_LFF_URG_CTRL_BIT	8
#define VDIN_LFF_URG_CTRL_WID	1
#define VPP_OFF_URG_CTRL_BIT	6
#define VPP_OFF_URG_CTRL_WID	1

/*#define VDIN_COM_CTRL0*/
#define VDIN_COMMONINPUT_EN_BIT		4
#define VDIN_COMMONINPUT_EN_WID		1

/*#define VDIN_WR_CTRL*/
#define VDIN0_VCP_WR_EN_BIT	25
#define VDIN0_VCP_WR_EN_WID	1
#define VDIN0_DISABLE_CLOCKGATE_BIT	29
#define VDIN0_DISABLE_CLOCKGATE_WID	1
/*#define VDIN1_WR_CTRL*/
#define VDIN1_VCP_WR_EN_BIT	8
#define VDIN1_VCP_WR_EN_WID	1
#define VDIN1_DISABLE_CLOCKGATE_BIT	29
#define VDIN1_DISABLE_CLOCKGATE_WID	1


#endif /* __VDIN_REGS_H */
