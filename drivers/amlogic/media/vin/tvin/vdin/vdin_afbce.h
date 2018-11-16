/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_afbce.h
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

#ifndef __VDIN_AFBCE_H__
#define __VDIN_AFBCE_H__
#include "vdin_drv.h"
#include "vdin_ctl.h"

#define AFBCE_ENABLE  0x41a0
//Bit   31:13,    reserved
//Bit   12 ,      reg_clk_en       default = 1,
//Bit   11:9,     reserved
//Bit   8,        enc_enable       default = 0
//Bit   7:1,      reserved
//Bit   0,        enc_frm_start    default = 0,

#define AFBCE_MODE   0x41a1
//Bit   31:29,    soft_rst         default = 0 ,the use as go_field
//Bit   28,       reserved
//Bit   27:26,    rev_mode    default = 0 , reverse mode
//Bit   25:24,    mif_urgent  default = 3 , info mif and data mif urgent
//Bit   23,       reserved
//Bit   22:16,    hold_line_num    default = 4, 0: burst1 1:burst2 2:burst4
//Bit   15:14,    burst_mode       default = 1, 0: burst1 1:burst2 2:burst4
//Bit   13:0,     reserved

#define AFBCE_SIZE_IN  0x41a2
//Bit   31:29,    reserved
//Bit   28:16     hsize_in  default = 1920,pic horz size in.unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vsize_in  default = 1080,pic vert size in.unit: pixel

#define AFBCE_BLK_SIZE_IN   0x41a3
//Bit   31:29,    reserved
//Bit   28:16     hblk_size default = 60 , pic horz size in.unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vblk_size default = 270, pic vert size in.unit: pixel

#define AFBCE_HEAD_BADDR    0x41a4
//Bit   31:0,   head_baddr         unsigned, default = 32'h00;

#define AFBCE_MIF_SIZE      0x41a5
//Bit   31:30,  reserved
//Bit   29:28,  ddr_blk_size       unsigned, default = 32'h128;
//Bit   27,     reserved
//Bit   26:24,  cmd_blk_size       unsigned, default = 32'h128;
//Bit   23:21,  reserved
//Bit   20:16,  uncmp_size         unsigned, default = 32'h128;
//Bit   15:13,  reserved
//Bit   12:0,   mmu_page_size      unsigned, default = 32'h4096;

#define AFBCE_PIXEL_IN_HOR_SCOPE    0x41a6
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_h     unsigned, default = 1919
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_h     unsigned, default = 0

#define AFBCE_PIXEL_IN_VER_SCOPE    0x41a7
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_v     unsigned, default = 1079
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_v     unsigned, default = 0

#define AFBCE_CONV_CTRL             0x41a8
//Bit   31:12,   reserved
//Bit   11: 0,   lbuf_depth default = 256, unit=16 pixel need to set = 2^n

#define AFBCE_MIF_HOR_SCOPE         0x41a9
//Bit   31:26,   reserved
//Bit   25:16,   blk_end_h         unsigned, default = 0
//Bit   15:10,   reserved
//Bit   9:0,     blk_bgn_h         unsigned, default = 59

#define AFBCE_MIF_VER_SCOPE         0x41aa
//Bit   31:28,   reserved
//Bit   27:16,   blk_end_v         unsigned, default = 0
//Bit   15:12,   reserved
//Bit   11:0,    blk_bgn_v         unsigned, default = 269

#define AFBCE_STAT1                 0x41ab
//Bit   31,     ro_frm_end_pulse1   unsigned, default = 0;frame end status
//Bit   30:0,   ro_dbg_top_info1    unsigned, default = 0;

#define AFBCE_STAT2                 0x41ac
//Bit   31,     reserved            unsigned, default = 0;frame end status
//Bit   30:0,   ro_dbg_top_info2    unsigned, default = 0

#define AFBCE_FORMAT                0x41ad
//Bit 31:12        reserved
//Bit 11            reserved
//Bit 10           reg_inp_yuv  default = 1
//  input is with yuv instead of rgb: 0: rgb, 1:yuv
//Bit  9           reg_inp_422  default = 0
//  input is with yuv422 instead of 444. 0: yuv444/yuv420; 1:yuv422
//Bit  8           reg_inp_420  default = 1
//  input is with yuv420 instead of 444. 0: yuv444/yuv422; 1:yuv420
//Bit  7: 4        reg_bly      default = 10  luma bitwidth
//Bit  3: 0        reg_blc      default = 10  chroma bitwidth

#define AFBCE_MODE_EN               0x41ae
//Bit 31:28        reserved
//Bit 27:26        reserved
//Bit 25           reg_adpt_interleave_ymode
//  RW, default = 0  force 0 to disable it: no  HW implementation
//Bit 24           reg_adpt_interleave_cmode
//  RW, default = 0  force 0 to disable it: not HW implementation
//Bit 23           reg_adpt_yinterleave_luma_ride
//  RW, default = 1  vertical interleave piece luma reorder ride;
//  0: no reorder ride; 1: w/4 as ride
//Bit 22           reg_adpt_yinterleave_chrm_ride
//  RW, default = 1  vertical interleave piece chroma reorder ride;
//  0: no reorder ride; 1: w/2 as ride
//Bit 21           reg_adpt_xinterleave_luma_ride
//  RW, default = 1  vertical interleave piece luma reorder ride;
//  0: no reorder ride; 1: w/4 as ride
//Bit 20           reg_adpt_xinterleave_chrm_ride
//  RW, default = 1  vertical interleave piece chroma reorder ride;
//  0: no reorder ride; 1: w/2 as ride
//Bit 19            reserved
//Bit 18           reg_disable_order_mode_i_6
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 17           reg_disable_order_mode_i_5
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 16           reg_disable_order_mode_i_4
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 15           reg_disable_order_mode_i_3
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 14           reg_disable_order_mode_i_2
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 13           reg_disable_order_mode_i_1
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 12           reg_disable_order_mode_i_0
//  RW, default = 0  disable order mode0~6: each mode with one
//  disable bit: 0: no disable, 1: disable
//Bit 11            reserved
//Bit 10           reg_minval_yenc_en
//  RW, default = 0  force disable,
//  final decision to remove this ws 1% performance loss
//Bit  9           reg_16x4block_enable
//  RW, default = 0  block as mission, but permit 16x4 block
//Bit  8           reg_uncompress_split_mode
//  RW, default = 0  0: no split; 1: split
//Bit  7: 6        reserved
//Bit  5           reg_input_padding_uv128
//  RW, default = 0  input picture 32x4
//  block gap mode: 0:  pad uv=0; 1: pad uv=128
//Bit  4           reg_dwds_padding_uv128
//  RW, default = 0  downsampled image for double write 32x gap mode
//  0:  pad uv=0; 1: pad uv=128
//Bit  3: 1        reg_force_order_mode_value
//  RW, default = 0  force order mode 0~7
//Bit  0           reg_force_order_mode_en
//  RW, default = 0  force order mode enable
//  0: no force; 1: forced to force_value

#define AFBCE_DWSCALAR      0x41af
//Bit 31: 8        reserved
//Bit  7: 6        reg_dwscalar_w0
//  RW, default = 3  horizontal 1st step scalar mode
//  0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6)
//  pixel kept; 2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  5: 4        reg_dwscalar_w1
//  RW, default = 0  horizontal 2nd step scalar mode
//  0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6) pixel kept;
//  2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  3: 2        reg_dwscalar_h0
//  RW, default = 2  vertical 1st step scalar mode
//  0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6) pixel kept
//  2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  1: 0        reg_dwscalar_h1
//  RW, default = 3  vertical 2nd step scalar mode
//  0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6) pixel kept
//  2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg

#define AFBCE_DEFCOLOR_1    0x41b0
//Bit 31:24        reserved
//Bit 23:12        reg_enc_defalutcolor_3
//  RW, default = 4095  Picture wise default color value in [Y Cb Cr]
//Bit 11: 0        reg_enc_defalutcolor_0
//  RW, default = 4095  Picture wise default color value in [Y Cb Cr]

#define AFBCE_DEFCOLOR_2    0x41b1
//Bit 31:24        reserved
//Bit 23:12        reg_enc_defalutcolor_2
//  RW, default = 2048  wise default color value in [Y Cb Cr]
//Bit 11: 0        reg_enc_defalutcolor_1
//  RW, default = 2048  wise default color value in [Y Cb Cr]

#define AFBCE_QUANT_ENABLE     0x41b2
//Bit 31:10        reserved
//Bit  9: 8        reg_bcleav_ofst
//  RW, default = 0  bcleave ofset to get lower range,
//  especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1
//  RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0
//  RW, default = 0  enable for quant to get some lossy

#define AFBCE_IQUANT_LUT_1       0x41b3
#define AFBCE_IQUANT_LUT_2       0x41b4
#define AFBCE_IQUANT_LUT_3       0x41b5
#define AFBCE_IQUANT_LUT_4       0x41b6
#define AFBCE_RQUANT_LUT_1       0x41b7
#define AFBCE_RQUANT_LUT_2       0x41b8
#define AFBCE_RQUANT_LUT_3       0x41b9
#define AFBCE_RQUANT_LUT_4       0x41ba

#define AFBCE_YUV_FORMAT_CONV_MODE  0x41bb
//Bit 31: 8        reserved
//Bit  7            reserved
//Bit  6: 4        reg_444to422_mode  RW, default = 0
//Bit  3            reserved
//Bit  2: 0        reg_422to420_mode  RW, default = 0

#define AFBCE_DUMMY_DATA            0x41bc
//Bit 31:30        reserved
//Bit 29: 0        reg_dummy_data RW, default = 0x00080200

#define AFBCE_CLR_FLAG              0x41bd
//Bit 31:0         reg_afbce_clr_flag  default = 0

#define AFBCE_STA_FLAGT             0x41be
//Bit 31:0         ro_afbce_sta_flag   default = 0

#define AFBCE_MMU_NUM               0x41bf
//Bit 31:16        reserved
//Bit 15: 0        ro_frm_mmu_num      default = 0

#define AFBCE_MMU_RMIF_CTRL1        0x41c0
//Bit 31:26 reserved
//Bit 25:24 reg_sync_sel   default = 0, axi canvas id sync with frm rst
//Bit 23:16 reg_canvas_id  default = 0, axi canvas id num
//Bit 15    reserved
//Bit 14:12 reg_cmd_intr_len
//  default = 1, interrupt send cmd when how many series axi cmd
//Bit 11:10 reg_cmd_req_size
//  default = 1, how many room fifo have,
//  then axi send series req, 0=16 1=32 2=24 3=64
//Bit 9:8   reg_burst_len
//  default = 2, burst type: 0-single 1-bst2 2-bst4
//Bit 7     reg_swap_64bit
//  default = 0, 64bits of 128bit swap enable
//Bit 6     reg_little_endian
//  default = 0, big endian enable
//Bit 5     reg_y_rev  default = 0, vertical reverse enable
//Bit 4     reg_x_rev  default = 0, horizontal reverse enable
//Bit 3     reserved
//Bit 2:0   reg_pack_mode
//  default = 3, 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit

#define AFBCE_MMU_RMIF_CTRL2      0x41c1
//Bit 31:30 reg_sw_rst        // unsigned , default = 0,
//Bit 29:24 reserved
//Bit 23:18 reg_gclk_ctrl
//Bit 17    reserved
//Bit 16:0  reg_urgent_ctrl   // unsigned , default = 0, urgent control reg

#define AFBCE_MMU_RMIF_CTRL3      0x41c2
//Bit 31:17 reserved
//Bit 16    reg_acc_mode      // unsigned , default = 1
//Bit 15:13 reserved
//Bit 12:0  reg_stride        // unsigned , default = 4096

#define AFBCE_MMU_RMIF_CTRL4      0x41c3
//Bit 31:0  reg_baddr        // unsigned , default = 0

#define AFBCE_MMU_RMIF_SCOPE_X    0x41c4
//Bit 31:29 reserved
//Bit 28:16 reg_x_end  default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_x_start default = 0, the canvas hor start pixel position

#define AFBCE_MMU_RMIF_SCOPE_Y    0x41c5
//Bit 31:29 reserved
//Bit 28:16 reg_y_end  default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_y_start default = 0, the canvas ver start pixel positio

#define AFBCE_MMU_RMIF_RO_STAT    0x41c6

extern void vdin_write_mif_or_afbce_init(struct vdin_dev_s *devp);
extern void vdin_write_mif_or_afbce(struct vdin_dev_s *devp,
	enum vdin_output_mif_e sel);
extern void vdin_afbce_update(struct vdin_dev_s *devp);
extern void vdin_afbce_config(struct vdin_dev_s *devp);
extern void vdin_afbce_maptable_init(struct vdin_dev_s *devp);
extern void vdin_afbce_set_next_frame(struct vdin_dev_s *devp,
	unsigned int rdma_enable, struct vf_entry *vfe);
extern void vdin_afbce_clear_writedown_flag(struct vdin_dev_s *devp);
extern int vdin_afbce_read_writedown_flag(void);
extern void vdin_afbce_hw_disable(void);
extern void vdin_afbce_hw_enable(void);
extern void vdin_afbce_hw_disable_rdma(struct vdin_dev_s *devp);
extern void vdin_afbce_hw_enable_rdma(struct vdin_dev_s *devp);
extern void vdin_afbce_soft_reset(void);
#endif

