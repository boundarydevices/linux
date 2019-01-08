/*
 * drivers/amlogic/media/deinterlace/register.h
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

#ifndef __MACH_DEINTERLACE_REG_ADDR_H_
#define __MACH_DEINTERLACE_REG_ADDR_H_
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/registers/regs/di_regs.h>
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/registers/regs/vdin_regs.h>

#define Wr(adr, val) aml_write_vcbus(adr, val)
#define Rd(adr) aml_read_vcbus(adr)
#define Wr_reg_bits(adr, val, start, len)  \
		aml_vcbus_update_bits(adr, ((1<<len)-1)<<start, val<<start)
#define Rd_reg_bits(adr, start, len)  \
		((aml_read_vcbus(adr)&(((1UL<<len)-1UL)<<start))>>start)

unsigned int RDMA_WR(unsigned int adr, unsigned int val);
unsigned int RDMA_RD(unsigned int adr);
unsigned int RDMA_WR_BITS(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len);
unsigned int RDMA_RD_BITS(unsigned int adr, unsigned int start,
		unsigned int len);
void DI_Wr(unsigned int addr, unsigned int val);
void DI_Wr_reg_bits(unsigned int adr, unsigned int val,
		unsigned int start, unsigned int len);
void DI_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val);
void DI_VSYNC_WR_MPEG_REG_BITS(unsigned int addr,
	unsigned int val, unsigned int start, unsigned int len);

#define HHI_VPU_CLKB_CNTL	0x83

#define VPU_WRARB_REQEN_SLV_L1C1	0x2795
#define VPU_RDARB_REQEN_SLV_L1C1	0x2791
#define VPU_ARB_DBG_STAT_L1C1		0x27b4

#define VPU_WRARB_REQEN_SLV_L1C1_TL1	0x2055
#define VPU_RDARB_REQEN_SLV_L1C1_TL1	0x2051
#define VPU_ARB_DBG_STAT_L1C1_TL1		0x205a


#define VIUB_SW_RESET					0x2001
#define VIUB_SW_RESET0					0x2002
#define VIUB_MISC_CTRL0					0x2006
		/* 0xd0108018 */
#define VIUB_GCLK_CTRL0					0x2007
#define VIUB_GCLK_CTRL1					0x2008
#define VIUB_GCLK_CTRL2					0x2009
#define VIUB_GCLK_CTRL3					0x200a
/* g12a add debug reg */
#define DI_DBG_CTRL						0x200b
#define DI_DBG_CTRL1					0x200c
#define DI_DBG_SRDY_INF					0x200d
#define DI_DBG_RRDY_INF				0x200e
/* txl add if2 */
#define DI_IF2_GEN_REG					0x2010
#define DI_IF2_CANVAS0					0x2011
#define DI_IF2_LUMA_X0					0x2012
#define DI_IF2_LUMA_Y0					0x2013
#define DI_IF2_CHROMA_X0				0x2014
#define DI_IF2_CHROMA_Y0				0x2015
#define DI_IF2_RPT_LOOP					0x2016
#define DI_IF2_LUMA0_RPT_PAT			0x2017
#define DI_IF2_CHROMA0_RPT_PAT			0x2018
#define DI_IF2_DUMMY_PIXEL				0x2019
#define DI_IF2_LUMA_FIFO_SIZE			0x201a
#define DI_IF2_RANGE_MAP_Y				0x201b
#define DI_IF2_RANGE_MAP_CB				0x201c
#define DI_IF2_RANGE_MAP_CR				0x201d
#define DI_IF2_GEN_REG2					0x201e
#define DI_IF2_FMT_CTRL					0x201f
#define DI_IF2_FMT_W					0x2020
#define DI_IF2_URGENT_CTRL				0x2021
#define DI_IF2_GEN_REG3					0x2022
/*txl new add end*/

/* g12 new added */
/* IF0 MIF */
#define DI_IF0_GEN_REG					0x2030
#define DI_IF0_CANVAS0					0x2031
#define DI_IF0_LUMA_X0					0x2032
#define DI_IF0_LUMA_Y0					0x2033
#define DI_IF0_CHROMA_X0				0x2034
#define DI_IF0_CHROMA_Y0				0x2035
#define DI_IF0_REPEAT_LOOP				0x2036
#define DI_IF0_LUMA0_RPT_PAT			0x2037
#define DI_IF0_CHROMA0_RPT_PAT			0x2038
#define DI_IF0_DUMMY_PIXEL				0x2039
#define DI_IF0_LUMA_FIFO_SIZE			0x203A
#define DI_IF0_RANGE_MAP_Y				0x203B
#define DI_IF0_RANGE_MAP_CB				0x203C
#define DI_IF0_RANGE_MAP_CR				0x203D
#define DI_IF0_GEN_REG2					0x203E
#define DI_IF0_FMT_CTRL					0x203F
#define DI_IF0_FMT_W					0x2040
#define DI_IF0_FMT_W					0x2040
#define DI_IF0_URGENT_CTRL				0x2041
#define DI_IF0_GEN_REG3					0x2042
/* AXI ARB */
#define DI_RDARB_MODE_L1C1				0x2050
#define DI_RDARB_REQEN_SLV_L1C1			0x2051
#define DI_RDARB_WEIGH0_SLV_L1C1		0x2052
#define DI_RDARB_WEIGH1_SLV_L1C1		0x2053
#define DI_WRARB_MODE_L1C1				0x2054
#define DI_WRARB_REQEN_SLV_L1C1			0x2055
#define DI_WRARB_WEIGH0_SLV_L1C1		0x2056
#define DI_WRARB_WEIGH1_SLV_L1C1		0x2057
#define DI_RDWR_ARB_STATUS_L1C1			0x2058
#define DI_ARB_DBG_CTRL_L1C1			0x2059
#define DI_ARB_DBG_STAT_L1C1			0x205a
#define DI_RDARB_UGT_L1C1				0x205b
#define DI_RDARB_LIMT0_L1C1				0x205c
#define DI_WRARB_UGT_L1C1				0x205d
#define DI_PRE_GL_CTRL					0x20ab
#define DI_PRE_GL_THD					0x20ac
#define DI_POST_GL_CTRL					0x20ad
#define DI_POST_GL_THD					0x20ae

#define DI_SUB_RDARB_MODE				0x37c0
#define DI_SUB_RDARB_REQEN_SLV			0x37c1
#define DI_SUB_RDARB_WEIGH0_SLV			0x37c2
#define DI_SUB_RDARB_WEIGH1_SLV			0x37c3
#define DI_SUB_RDARB_UGT				0x37c4
#define DI_SUB_RDARB_LIMT0				0x37c5
#define DI_SUB_WRARB_MODE				0x37c6
#define DI_SUB_WRARB_REQEN_SLV			0x37c7
#define DI_SUB_WRARB_WEIGH0_SLV			0x37c8
#define DI_SUB_WRARB_WEIGH1_SLV			0x37c9
#define DI_SUB_WRARB_UGT				0x37ca
#define DI_SUB_RDWR_ARB_STATUS			0x37cb
#define DI_SUB_ARB_DBG_CTRL				0x37cc
#define DI_SUB_ARB_DBG_STAT				0x37cd
#define CONTRD_CTRL1					0x37d0
#define CONTRD_CTRL2					0x37d1
#define CONTRD_SCOPE_X					0x37d2
#define CONTRD_SCOPE_Y					0x37d3
#define CONTRD_RO_STAT					0x37d4
#define CONT2RD_CTRL1					0x37d5
#define CONT2RD_CTRL2					0x37d6
#define CONT2RD_SCOPE_X					0x37d7
#define CONT2RD_SCOPE_Y					0x37d8
#define CONT2RD_RO_STAT					0x37d9
#define MTNRD_CTRL1						0x37da
#define MTNRD_CTRL2						0x37db
#define MTNRD_SCOPE_X					0x37dc
#define MTNRD_SCOPE_Y					0x37dd
#define MTNRD_RO_STAT					0x37de
#define MCVECRD_CTRL1					0x37df
#define MCVECRD_CTRL2					0x37e0
#define MCVECRD_SCOPE_X					0x37e1
#define MCVECRD_SCOPE_Y					0x37e2
#define MCVECRD_RO_STAT					0x37e3
#define MCINFRD_CTRL1					0x37e4
#define MCINFRD_CTRL2					0x37e5
#define MCINFRD_SCOPE_X					0x37e6
#define MCINFRD_SCOPE_Y					0x37e7
#define MCINFRD_RO_STAT					0x37e8
#define CONTWR_X						0x37e9
#define CONTWR_Y						0x37ea
#define CONTWR_CTRL						0x37eb
#define CONTWR_CAN_SIZE					0x37ec
#define MTNWR_X							0x37ed
#define MTNWR_Y							0x37ee
#define MTNWR_CTRL						0x37ef
#define MTNWR_CAN_SIZE					0x37f0
#define MCVECWR_X						0x37f1
#define MCVECWR_Y						0x37f2
#define MCVECWR_CTRL					0x37f3
#define MCVECWR_CAN_SIZE				0x37f4
#define MCINFWR_X						0x37f5
#define MCINFWR_Y						0x37f6
#define MCINFWR_CTRL					0x37f7
#define MCINFWR_CAN_SIZE				0x37f8
/* DI SCALE */
#define DI_SCO_FIFO_CTRL				0x374e
#define DI_SC_TOP_CTRL					0x374f
#define DI_SC_DUMMY_DATA				0x3750
#define DI_SC_LINE_IN_LENGTH			0x3751
#define DI_SC_PIC_IN_HEIGHT				0x3752
#define DI_SC_COEF_IDX					0x3753
#define DI_SC_COEF						0x3754
#define DI_VSC_REGION12_STARTP			0x3755
#define DI_VSC_REGION34_STARTP			0x3756
#define DI_VSC_REGION4_ENDP				0x3757
#define DI_VSC_START_PHASE_STEP			0x3758
#define DI_VSC_REGION0_PHASE_SLOPE		0x3759
#define DI_VSC_REGION1_PHASE_SLOPE		0x375a
#define DI_VSC_REGION3_PHASE_SLOPE		0x375b
#define DI_VSC_REGION4_PHASE_SLOPE		0x375c
#define DI_VSC_PHASE_CTRL				0x375d
#define DI_VSC_INI_PHASE				0x375e
#define DI_HSC_REGION12_STARTP			0x3760
#define DI_HSC_REGION34_STARTP			0x3761
#define DI_HSC_REGION4_ENDP				0x3762
#define DI_HSC_START_PHASE_STEP			0x3763
#define DI_HSC_REGION0_PHASE_SLOPE		0x3764
#define DI_HSC_REGION1_PHASE_SLOPE		0x3765
#define DI_HSC_REGION3_PHASE_SLOPE		0x3766
#define DI_HSC_REGION4_PHASE_SLOPE		0x3767
#define DI_HSC_PHASE_CTRL				0x3768
#define DI_SC_MISC						0x3769
#define DI_HSC_PHASE_CTRL1				0x376a
#define DI_HSC_INI_PAT_CTRL				0x376b
#define DI_SC_GCLK_CTRL					0x376c
#define DI_SC_HOLD_LINE					0x376d
/* NR DOWNSAMPLE */
#define NRDSWR_X						0x37f9
#define NRDSWR_Y						0x37fa
#define NRDSWR_CTRL						0x37fb
#define NRDSWR_CAN_SIZE					0x37fc
#define NR_DS_BUF_SIZE_REG				0x3740
#define NR_DS_CTRL						0x3741
#define NR_DS_OFFSET					0x3742
#define NR_DS_BLD_COEF					0x3743
/* di */
#define DI_IF1_URGENT_CTRL                  (0x20a3)  /*  << 2 + 0xd0100000*/
/* bit15, auto enable; bit14, canvas write mode ;7:4, high threshold ;3:0 ,
 * low threshold  for di inp chroma path
 * bit31, auto enable; bit30, canvas write mode ;23:20, high threshold ;19:16 ,
 * low threshold  for di inp luma path
 */
#define DI_INP_URGENT_CTRL                  (0x20a4)  /*  << 2 + 0xd0100000*/
/* bit15, auto enable; bit14, canvas write mode ;7:4, high threshold ;3:0 ,
 * low threshold  for di mem chroma path
 * bit31, auto enable; bit30, canvas write mode ;23:20, high threshold ;19:16 ,
 * low threshold  for di mem luma path
 */
#define DI_MEM_URGENT_CTRL                  (0x20a5)  /*  << 2 + 0xd0100000*/
/* bit15, auto enable; bit14, canvas write mode ;7:4, high threshold ;3:0 ,
 * low threshold  for di chan2 chroma path
 * bit31, auto enable; bit30, canvas write mode ;23:20, high threshold ;19:16 ,
 * low threshold  for di chan2 luma path
 */
#define DI_CHAN2_URGENT_CTRL                (0x20a6)  /*  << 2 + 0xd0100000*/

#define DI_PRE_CTRL                        ((0x1700)) /* << 2) + 0xd0100000) */
/* bit 31,      cbus_pre_frame_rst */
/* bit 30,      cbus_pre_soft_rst */
/* bit 29,      pre_field_num */
/* bit 27:26,   mode_444c422 */
/* bit 25,      di_cont_read_en */
/* bit 24:23,   mode_422c444 */
/* bit 22,      mtn_after_nr */
/* bit 21:16,   pre_hold_fifo_lines */
/* bit 15,      nr_wr_by */
/* bit 14,      use_vdin_go_line */
/* bit 13,      di_prevdin_en */
/* bit 12,      di_pre_viu_link */
/* bit 11,      di_pre_repeat */
/* bit 10,      di_pre_drop_1st */
/* bit  9,      di_buf2_en */
/* bit  8,      di_chan2_en */
/* bit  7,      prenr_hist_en */
/* bit  6,      chan2_hist_en */
/* bit  5,      hist_check_en */
/* bit  4,      check_after_nr */
/* bit  3,      check222p_en */
/* bit  2,      check322p_en */
/* bit  1,      mtn_en */
/* bit  0,      nr_en */
/* #define DI_POST_CTRL                      ((0x1701)) */
/* bit 31,      cbus_post_frame_rst */
/* bit 30,      cbus_post_soft_rst */
/* bit 29,      post_field_num */
/* bit 21:16,   post_hold_fifo_lines */
/* bit 13,      prepost_link */
/* bit 12,      di_post_viu_link */
/* bit 11,      di_post_repeat */
/* bit 10,      di_post_drop_1st */
/* bit  9,      mif0_to_vpp_en */
/* bit  8,      di_vpp_out_en */
/* bit  7,      di_wr_bk_en */
/* bit  6,      di_mux_en */
/* bit  5,      di_blend_en */
/* bit  4,      di_mtnp_read_en */
/* bit  3,      di_mtn_buf_en */
/* bit  2,      di_ei_en */
/* bit  1,      di_buf1_en */
/* bit  0,      di_buf0_en */
/* #define DI_POST_SIZE                      ((0x1702)) */
/* bit 28:16,    vsize1post */
/* bit 12:0,     hsize1post */
#define DI_PRE_SIZE                       ((0x1703)) /* << 2) + 0xd0100000) */
/* bit 28:16,    vsize1pre */
/* bit 12:0,     hsize1pre */
#define DI_EI_CTRL0                       ((0x1704)) /* << 2) + 0xd0100000) */
/* bit 23:16,    ei0_filter[2:+]  abs_diff_left>filter &&
 * ...right>filter && ...top>filter && ...bot>filter -> filter
 */
/* bit 15:8,     ei0_threshold[2:+] */
/* bit 3,        ei0_vertical */
/* bit 2,        ei0_bpscf2 */
/* bit 1,        ei0_bpsfar1 */
#define DI_EI_CTRL1                       ((0x1705)) /* << 2) + 0xd0100000) */
/* bit 31:24,    ei0_diff */
/* bit 23:16,    ei0_angle45 */
/* bit 15:8,     ei0_peak */
/* bit 7:0,      ei0_cross */
#define DI_EI_CTRL2                       ((0x1706)) /* << 2) + 0xd0100000) */
/* bit 31:24,    ei0_close2 */
/* bit 23:16,    ei0_close1 */
/* bit 15:8,     ei0_far2 */
/* bit 7:0,      ei0_far1 */
#define DI_NR_CTRL0                       ((0x1707)) /* << 2) + 0xd0100000) */
/* bit 26,       nr_cue_en */
/* bit 25,       nr2_en */
#define DI_NR_CTRL1                       ((0x1708)) /* << 2) + 0xd0100000) */
/* bit 31:30,    mot_p1txtcore_mode */
/* bit 29:24,    mot_p1txtcore_clmt */
/* bit 21:16,    mot_p1txtcore_ylmt */
/* bit 15:8,     mot_p1txtcore_crate */
/* bit 7:0,      mot_p1txtcore_yrate */
#define DI_NR_CTRL2                       ((0x1709)) /* << 2) + 0xd0100000) */
/* bit 29:24,    mot_curtxtcore_clmt */
/* bit 21:16,    mot_curtxtcore_ylmt */
/* bit 15:8,     mot_curtxtcore_crate */
/* bit 7:0,      mot_curtxtcore_yrate */
/* `define DI_NR_CTRL3               8'h0a */
/* no use */
/* `define DI_MTN_CTRL               8'h0b */
/* no use */
#define DI_MTN_CTRL1                      ((0x170c)) /* << 2) + 0xd0100000) */
/* bit 13 ,      me enable */
/* bit 12 ,      me autoenable */
/* bit 11:8,		mtn_paramtnthd */
/* bit 7:0,      mtn_parafltthd */
#define DI_BLEND_CTRL                     ((0x170d)) /* << 2) + 0xd0100000) */
/* bit 31,      blend_1_en */
/* bit 30,      blend_mtn_lpf */
/* bit 28,      post_mb_en */
/* bit 27,      blend_mtn3p_max */
/* bit 26,      blend_mtn3p_min */
/* bit 25,      blend_mtn3p_ave */
/* bit 24,      blend_mtn3p_maxtb */
/* bit 23,      blend_mtn_flt_en */
/* bit 22,      blend_data_flt_en */
/* bit 21:20,   blend_top_mode */
/* bit 19,      blend_reg3_enable */
/* bit 18,      blend_reg2_enable */
/* bit 17,      blend_reg1_enable */
/* bit 16,      blend_reg0_enable */
/* bit 15:14,   blend_reg3_mode */
/* bit 13:12,   blend_reg2_mode */
/* bit 11:10,   blend_reg1_mode */
/* bit 9:8,     blend_reg0_mode */
/* bit 7:0,     kdeint */
/* `define DI_BLEND_CTRL1            8'h0e */
/* no use */
/* `define DI_BLEND_CTRL2            8'h0f */
/* no use */
#define DI_ARB_CTRL                       ((0x170f)) /* << 2) + 0xd0100000) */
/* bit 31:26,			di_arb_thd1 */
/* bit 25:20,			di_arb_thd0 */
/* bit 19,			di_arb_tid_mode */
/* bit 18,			di_arb_arb_mode */
/* bit 17,			di_arb_acq_en */
/* bit 16,			di_arb_disable_clk */
/* bit 15:0,			di_arb_req_en */
#define DI_BLEND_REG0_X                   ((0x1710)) /* << 2) + 0xd0100000) */
/* bit 27:16,   blend_reg0_startx */
/* bit 11:0,    blend_reg0_endx */
#define DI_BLEND_REG0_Y                   ((0x1711)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG1_X                   ((0x1712)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG1_Y                   ((0x1713)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG2_X                   ((0x1714)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG2_Y                   ((0x1715)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG3_X                   ((0x1716)) /* << 2) + 0xd0100000) */
#define DI_BLEND_REG3_Y                   ((0x1717)) /* << 2) + 0xd0100000) */
#define DI_CLKG_CTRL                      ((0x1718)) /* << 2) + 0xd0100000) */
/* bit 31:24,   pre_gclk_ctrl     no clk gate control. if ==1,
 * module clk is not gated (always on). [3] for pulldown,[2]
 * for mtn_1,[1] for mtn_0,[0] for nr
 * bit 23:16,   post_gclk_ctrl    no clk gate control. [4]
 * for ei_1, [3] for ei_0,[2] for ei_top, [1] for blend_1, [0] for blend_0
 * bit 1,       di_gate_all       clk shut down. if ==1 ,
 * all di clock shut down
 * bit 0,       di_no_clk_gate    no clk gate control.
 * if di_gated_all==0 and di_no_clk_gate ==1, all di clock is always working.
 */
#define DI_EI_CTRL3                       ((0x1719)) /* << 2) + 0xd0100000) */
/* bit 31,      reg_ei_1 */
/* bit 30,      reg_demon_en */
/* bit 26:24,   reg_demon_mux */
/* bit 23:20,   reg_right_win */
/* bit 19:16,   reg_left_win */
/* bit 7:4,     reg_ei_sadm_quatize_margin */
/* bit 1:0,     reg_ei_sad_relative_mode */
#define DI_EI_CTRL4                       ((0x171a)) /* << 2) + 0xd0100000) */
/* bit 29,      reg_ei_caldrt_ambliike2_biasvertical */
/* bit 28:24,   reg_ei_caldrt_addxla2list_drtmax */
/* bit 22:20,   reg_ei_caldrt_addxla2list_signm0th */
/* bit 19,      reg_ei_caldrt_addxla2list_mode */
/* bit 18:16,   reg_ei_signm_sad_cor_rate */
/* bit 15:12,   reg_ei_signm_sadi_cor_rate */
/* bit 11:6,    reg_ei_signm_sadi_cor_ofst */
/* bit 5:0,     reg_ei_signm_sad_ofst */
#define DI_EI_CTRL5                       ((0x171b)) /* << 2) + 0xd0100000) */
/* bit 30:28,   reg_ei_caldrt_cnflcctchk_frcverthrd */
/* bit 26:24,   reg_ei_caldrt_cnflctchk_mg */
/* bit 23:22,   reg_ei_caldrt_cnflctchk_ws */
/* bit 21,      reg_ei_caldrt_cnflctchk_en */
/* bit 20,      reg_ei_caldrt_verfrc_final_en */
/* bit 19,      reg_ei_caldrt_verfrc_retimflt_en */
/* bit 18:16,   reg_ei_caldrt_verftc_eithratemth */
/* bit 15,      reg_ei_caldrt_verfrc_retiming_en */
/* bit 14:12,   reg_ei_caldrt_verfrc_bothratemth */
/* bit 11:9,    reg_ei_caldrt_ver_thrd */
/* bit 8:4,     reg_ei_caldrt_addxla2list_drtmin */
/* bit 3:0,     reg_ei_caldrt_addxla2list_drtlimit */
#define DI_EI_CTRL6                       ((0x171c)) /* << 2) + 0xd0100000) */
/* bit 31:24,   reg_ei_caldrt_abext_sad12thhig */
/* bit 23:16,   reg_ei_caldrt_abext_sad00thlow */
/* bit 15:8,    reg_ei_caldrt_abext_sad12thlow */
/* bit 6:4,     reg_ei_caldrt_abext_ratemth */
/* bit 2:0,     reg_ei_caldrt_abext_drtthrd */
#define DI_EI_CTRL7                       ((0x171d)) /* << 2) + 0xd0100000) */
/* bit 29,      reg_ei_caldrt_xlanopeak_codien */
/* bit 28:24,   reg_ei_caldrt_xlanopeak_drtmax */
/* bit 23,      reg_ei_caldrt_xlanopeak_en */
/* bit 28:24,   reg_ei_caldrt_abext_monotrnd_alpha */
/* bit 28:24,   reg_ei_caldrt_abext_mononum12_thrd */
/* bit 28:24,   reg_ei_caldrt_abext_mononum00_thrd */
/* bit 28:24,   reg_ei_caldrt_abext_sad00rate */
/* bit 28:24,   reg_ei_caldrt_abext_sad12rate */
/* bit 28:24,   reg_ei_caldrt_abext_sad00thhig */
#define DI_EI_CTRL8                       ((0x171e)) /* << 2) + 0xd0100000) */
/* bit 30:28,   reg_ei_assign_headtail_magin */
/* bit 26:24,   reg_ei_retime_lastcurpncnfltchk_mode */
/* bit 22:21,   reg_ei_retime_lastcurpncnfltchk_drtth */
/* bit 20,      reg_ei_caldrt_histchk_cnfid */
/* bit 19:16,   reg_ei_caldrt_histchk_thrd */
/* bit 15,      reg_ei_caldrt_histchk_abext */
/* bit 14,      reg_ei_caldrt_histchk_npen */
/* bit 13:11,   reg_ei_caldrt_amblike2_drtmg */
/* bit 10:8,    reg_ei_caldrt_amblike2_valmg */
/* bit 7:4,     reg_ei_caldrt_amblike2_alpha */
/* bit 3:0,     reg_ei_caldrt_amblike2_drtth */
#define DI_EI_CTRL9                       ((0x171f)) /* << 2) + 0xd0100000) */
/* bit 31:28,   reg_ei_caldrt_hcnfcheck_frcvert_xla_th3 */
/* bit 27,      reg_ei_caldrt_hcnfcheck_frcvert_xla_en */
/* bit 26:24,   reg_ei_caldrt_conf_drtth */
/* bit 23:20,   reg_ei_caldrt_conf_absdrtth */
/* bit 19:18,   reg_ei_caldrt_abcheck_mode1 */
/* bit 17:16,   reg_ei_caldrt_abcheck_mode0 */
/* bit 15:12,   reg_ei_caldrt_abcheck_drth1 */
/* bit 11:8,    reg_ei_caldrt_abcheck_drth0 */
/* bit 6:4,     reg_ei_caldrt_abpnchk1_th */
/* bit 1,       reg_ei_caldrt_abpnchk1_en */
/* bit 0,       reg_ei_caldrt_abpnchk0_en */
#define DI_EI_CTRL10                      ((0x1793)) /* << 2) + 0xd0100000) */
/* bit 31:28,   reg_ei_caldrt_hstrrgchk_drtth */
/* bit 27:24,   reg_ei_caldrt_hstrrgchk_frcverthrd */
/* bit 23:20,   reg_ei_caldrt_hstrrgchk_mg */
/* bit 19,      reg_ei_caldrt_hstrrgchk_1sidnul */
/* bit 18,      reg_ei_caldrt_hstrrgchk_excpcnf */
/* bit 17:16,   reg_ei_caldrt_hstrrgchk_ws */
/* bit 15,      reg_ei_caldrt_hstrrgchk_en */
/* bit 14:13,   reg_ei_caldrt_hpncheck_mode */
/* bit 12,      reg_ei_caldrt_hpncheck_mute */
/* bit 11:9,    reg_ei_caldrt_hcnfcheck_mg2 */
/* bit 8:6,     reg_ei_caldrt_hcnfcheck_mg1 */
/* bit 5:4,     reg_ei_caldrt_hcnfcheck_mode */
/* bit 3:0,     reg_ei_caldrt_hcnfcheck_mg2 */
#define DI_EI_CTRL11                      ((0x179e)) /* << 2) + 0xd0100000) */
/* bit 30:29,   reg_ei_amb_detect_mode */
/* bit 28:24,   reg_ei_amb_detect_winth */
/* bit 23:21,   reg_ei_amb_decide_rppth */
/* bit 20:19,   reg_ei_retime_lastmappncnfltchk_drtth */
/* bit 18:16,   reg_ei_retime_lastmappncnfltchk_mode */
/* bit 15:14,   reg_ei_retime_lastmapvertfrcchk_mode */
/* bit 13:12,   reg_ei_retime_lastvertfrcchk_mode */
/* bit 11:8,    reg_ei_retime_lastpnchk_drtth */
/* bit 6,       reg_ei_retime_lastpnchk_en */
/* bit 5:4,     reg_ei_retime_mode */
/* bit 3,       reg_ei_retime_last_en */
/* bit 2,       reg_ei_retime_ab_en */
/* bit 1,       reg_ei_caldrt_hstrvertfrcchk_en */
/* bit 0,       reg_ei_caldrt_hstrrgchk_mode */
#define DI_EI_CTRL12                      ((0x179f)) /* << 2) + 0xd0100000) */
/* bit 31:28,   reg_ei_drtdelay2_lmt */
/* bit 27:26,   reg_ei_drtdelay2_notver_lrwin */
/* bit 25:24,   reg_ei_drtdelay_mode */
/* bit 23,      reg_ei_drtdelay2_mode */
/* bit 22:20,   reg_ei_assign_xla_signm0th */
/* bit 19,      reg_ei_assign_pkbiasvert_en */
/* bit 18,      reg_ei_assign_xla_en */
/* bit 17:16,   reg_ei_assign_xla_mode */
/* bit 15:12,   reg_ei_assign_nlfilter_magin */
/* bit 11:8,    reg_ei_localsearch_maxrange */
/* bit 7:4,     reg_ei_xla_drtth */
/* bit 3:0,     reg_ei_flatmsad_thrd */
#define DI_EI_CTRL13                      ((0x17a8)) /* << 2) + 0xd0100000) */
/* bit 27:24,   reg_ei_int_drt2x_chrdrt_limit */
/* bit 23:20,   reg_ei_int_drt16x_core */
/* bit 19:16,   reg_ei_int_drtdelay2_notver_cancv */
/* bit 15:8,    reg_ei_int_drtdelay2_notver_sadth */
/* bit 7:0,     reg_ei_int_drtdelay2_vlddrt_sadth */
#define DI_EI_XWIN0                       ((0x1798)) /* << 2) + 0xd0100000) */
/* bit 27:16,   ei_xend0 */
/* bit 11:0,    ei_xstart0 */
#define DI_EI_XWIN1                       ((0x1799)) /* << 2) + 0xd0100000) */
/* DEINTERLACE mode check. */
#define DI_MC_REG0_X                      ((0x1720)) /* << 2) + 0xd0100000) */
/* bit 27:16,   mc_reg0_start_x */
/* bit 11:0,    mc_reg0_end_x */
#define DI_MC_REG0_Y                      ((0x1721)) /* << 2) + 0xd0100000) */
#define DI_MC_REG1_X                      ((0x1722)) /* << 2) + 0xd0100000) */
#define DI_MC_REG1_Y                      ((0x1723)) /* << 2) + 0xd0100000) */
#define DI_MC_REG2_X                      ((0x1724)) /* << 2) + 0xd0100000) */
#define DI_MC_REG2_Y                      ((0x1725)) /* << 2) + 0xd0100000) */
#define DI_MC_REG3_X                      ((0x1726)) /* << 2) + 0xd0100000) */
#define DI_MC_REG3_Y                      ((0x1727)) /* << 2) + 0xd0100000) */
#define DI_MC_REG4_X                      ((0x1728)) /* << 2) + 0xd0100000) */
#define DI_MC_REG4_Y                      ((0x1729)) /* << 2) + 0xd0100000) */
#define DI_MC_32LVL0                      ((0x172a)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mc_reg2_32lvl */
/* bit 23:16,   mc_reg1_32lvl */
/* bit 15:8,    mc_reg0_32lvl */
/* bit 7:0,     field_32lvl */
#define DI_MC_32LVL1                      ((0x172b)) /* << 2) + 0xd0100000) */
/* bit 15:8,    mc_reg3_32lvl */
/* bit 7:0,     mc_reg4_32lvl */
#define DI_MC_22LVL0                      ((0x172c)) /* << 2) + 0xd0100000) */
/* bit 31:16,   mc_reg0_22lvl */
/* bit 15:0,    field_22lvl */
#define DI_MC_22LVL1                      ((0x172d)) /* << 2) + 0xd0100000) */
/* bit 31:16,   mc_reg2_22lvl */
/* bit 15:0,    mc_reg1_22lvl */
#define DI_MC_22LVL2                      ((0x172e)) /* << 2) + 0xd0100000) */
/* bit 31:16,   mc_reg4_22lvl */
/* bit 15:0,    mc_reg3_22lvl */
#define DI_MC_CTRL                        ((0x172f)) /* << 2) + 0xd0100000) */
/* bit 4,       mc_reg4_en */
/* bit 3,       mc_reg3_en */
/* bit 2,       mc_reg2_en */
/* bit 1,       mc_reg1_en */
/* bit 0,       mc_reg0_en */
#define DI_INTR_CTRL                      ((0x1730)) /* << 2) + 0xd0100000) */
#define DI_INFO_ADDR                      ((0x1731)) /* << 2) + 0xd0100000) */
#define DI_INFO_DATA                      ((0x1732)) /* << 2) + 0xd0100000) */
#define DI_PRE_HOLD                       ((0x1733)) /* << 2) + 0xd0100000) */
#define DI_MTN_1_CTRL1                    ((0x1740)) /* << 2) + 0xd0100000) */
/* bit 31,      mtn_1_en */
/* bit 30,      mtn_init */
/* bit 29,      di2nr_txt_en */
/* bit 28,      di2nr_txt_mode */
/* bit 27:24,   mtn_def */
/* bit 23:16,   mtn_adp_yc */
/* bit 15:8,    mtn_adp_2c */
/* bit 7:0,     mtn_adp_2y */
#define DI_MTN_1_CTRL2                    ((0x1741)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_ykinter */
/* bit 23:16,   mtn_ckinter */
/* bit 15:8,    mtn_ykintra */
/* bit  7:0,    mtn_ckintra */
#define DI_MTN_1_CTRL3                    ((0x1742)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_tyrate */
/* bit 23:16,   mtn_tcrate */
/* bit 15: 8,   mtn_mxcmby */
/* bit  7: 0,   mtn_mxcmbc */
#define DI_MTN_1_CTRL4                    ((0x1743)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_tcorey */
/* bit 23:16,   mtn_tcorec */
/* bit 15: 8,   mtn_minth */
/* bit  7: 0,   mtn_maxth */
#define DI_MTN_1_CTRL5                    ((0x1744)) /* << 2) + 0xd0100000) */
/* bit 31:28,   mtn_m1b_extnd */
/* bit 27:24,   mtn_m1b_errod */
/* bit 19:18,   mtn_replace_cbyy */
/* bit 17:16,   mtn_replace_ybyc */
/* bit 15: 8,   mtn_core_ykinter */
/* bit  7: 0,   mtn_core_ckinter */
#define DI_MTN_1_CTRL6                    ((0x17a9)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_m1b_extnd */
/* bit 23:16,   mtn_m1b_errod */
/* bit 15: 8,   mtn_core_ykinter */
/* bit  7: 0,   mtn_core_ckinter */
#define DI_MTN_1_CTRL7                    ((0x17aa)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_core_mxcmby */
/* bit 23:16,   mtn_core_mxcmbc */
/* bit 15: 8,   mtn_core_y */
/* bit  7: 0,   mtn_core_c */
#define DI_MTN_1_CTRL8                    ((0x17ab)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_fcore_ykinter */
/* bit 23:16,   mtn_fcore_ckinter */
/* bit 15: 8,   mtn_fcore_ykintra */
/* bit  7: 0,   mtn_fcore_ckintra */
#define DI_MTN_1_CTRL9                    ((0x17ac)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_fcore_2yrate */
/* bit 23:16,   mtn_fcore_2crate */
/* bit 15: 8,   mtn_fcore_y */
/* bit  7: 0,   mtn_fcore_c */
#define DI_MTN_1_CTRL10                   ((0x17ad)) /* << 2) + 0xd0100000) */
/* bit 27:24,   mtn_motfld0 */
/* bit 19:16,   mtn_stlfld0 */
/* bit 11: 8,   mtn_motfld1 */
/* bit  3: 0,   mtn_stlfld1 */
#define DI_MTN_1_CTRL11                   ((0x17ae)) /* << 2) + 0xd0100000) */
/* bit 27:24,   mtn_smotevn */
/* bit 20:16,   mtn_smotodd */
/* bit 11: 8,   mtn_sstlevn */
/* bit  4: 0,   mtn_sstlodd */
#define DI_MTN_1_CTRL12                   ((0x17af)) /* << 2) + 0xd0100000) */
/* bit 31:24,   mtn_mgain */
/* bit 17:16,   mtn_mmode */
/* bit 15: 8,   mtn_sthrd */
/* bit  4: 0,   mtn_sgain */
/* // DET 3D REG DEFINE BEGIN //// */
/* // 8'h34~8'h3f */
#define DET3D_MOTN_CFG                    ((0x1734)) /* << 2) + 0xd0100000) */
/* Bit 16,	reg_det3d_intr_en	        Det3d interrupt enable
 * Bit 9:8,	reg_Det3D_Motion_Mode
 * U2  Different mode for Motion Calculation of Luma and Chroma:
 *		0: MotY, 1: (2*MotY + (MotU + MotV))/4;
 *		2: Max(MotY, MotU,MotV); 3:Max(MotY, (MotU+MotV)/2)
 * Bit 7:4,	reg_Det3D_Motion_Core_Rate	U4  K Rate to Edge (HV) details
 * for coring of Motion Calculations, normalized to 32
 * Bit 3:0,	reg_Det3D_Motion_Core_Thrd
 * U4  2X: static coring value for Motion Detection.
 */
#define DET3D_CB_CFG                      ((0x1735)) /* << 2) + 0xd0100000) */
/* Bit 7:4,	reg_Det3D_ChessBd_NHV_ofst
 * U4,  Noise immune offset for NON-Horizotnal or vertical combing detection.
 *
 * Bit 3:0,	reg_Det3D_ChessBd_HV_ofst
 * U4,  Noise immune offset for Horizotnal or vertical combing detection.
 */
#define DET3D_SPLT_CFG                    ((0x1736)) /* << 2) + 0xd0100000) */
/* Bit 7:4,	reg_Det3D_SplitValid_ratio
 * U4,  Ratio between max_value and the avg_value of
 * the edge mapping for split line valid detection.
 * The smaller of this value, the easier of the split line detected.
 * Bit 3:0,	reg_Det3D_AvgIdx_ratio	    U4,Ratio to the avg_value of the
 * edge mapping for split line position estimation.
 */
/* The smaller of this value,
 * the more samples will be added to the estimation.
 */
#define DET3D_HV_MUTE                     ((0x1737)) /* << 2) + 0xd0100000) */
/* Bit 23:20, reg_Det3D_Edge_Ver_Mute	U4  X2: Horizontal pixels to be mute
 * from H/V Edge calculation Top and Bottom border part.
 * Bit 19:16, reg_Det3D_Edge_Hor_Mute	U4  X2: Horizontal pixels to be mute
 * from H/V Edge calculation Left and right border part.
 * Bit 15:12, reg_Det3D_ChessBd_Ver_Mute	U4  X2: Horizontal pixels to
 * be mute from ChessBoard statistics calculation in middle part
 * Bit 11:8,	 reg_Det3D_ChessBd_Hor_Mute	U4  X2: Horizontal pixels to
 * be mute from ChessBoard statistics calculation in middle part
 * Bit 7:4,	 reg_Det3D_STA8X8_Ver_Mute	U4  1X: Vertical pixels to be
 * mute from 8x8 statistics calculation in each block.
 * Bit 3:0,	 reg_Det3D_STA8X8_Hor_Mute	U4  1X: Horizontal pixels to
 * be mute from 8x8 statistics calculation in each block.
 */
#define DET3D_MAT_STA_P1M1                ((0x1738)) /* << 2) + 0xd0100000) */
/* Bit 31:24, reg_Det3D_STA8X8_P1_K0_R8	U8  SAD to SAI ratio to decide P1,
 * normalized to 256 (0.8)
 * Bit 23:16, reg_Det3D_STA8X8_P1_K1_R7	U8  SAD to ENG ratio to decide P1,
 * normalized to 128 (0.5)
 * Bit 15:8,	 reg_Det3D_STA8X8_M1_K0_R6
 * U8  SAD to SAI ratio to decide M1, normalized to 64  (1.1)
 * Bit 7:0,	 reg_Det3D_STA8X8_M1_K1_R6
 * U8  SAD to ENG ratio to decide M1, normalized to 64  (0.8)
 */
#define DET3D_MAT_STA_P1TH                ((0x1739)) /* << 2) + 0xd0100000) */
/* Bit 23:16, reg_Det3D_STAYUV_P1_TH_L4	U8  SAD to ENG Thrd offset to
 * decide P1, X16         (100)
 * Bit 15:8,	 reg_Det3D_STAEDG_P1_TH_L4	U8  SAD to ENG Thrd
 * offset to decide P1, X16         (80)
 * Bit 7:0,	 reg_Det3D_STAMOT_P1_TH_L4	U8  SAD to ENG Thrd
 * offset to decide P1, X16         (48)
 */
#define DET3D_MAT_STA_M1TH                ((0x173a)) /* << 2) + 0xd0100000) */
/* Bit 23:16, reg_Det3D_STAYUV_M1_TH_L4	U8  SAD to
 * ENG Thrd offset to decide M1, X16         (100)
 * Bit 15:8,	 reg_Det3D_STAEDG_M1_TH_L4
 * U8  SAD to ENG Thrd offset to decide M1, X16         (80)
 * Bit 7:0,	 reg_Det3D_STAMOT_M1_TH_L4
 * U8  SAD to ENG Thrd offset to decide M1, X16         (64)
 */
#define DET3D_MAT_STA_RSFT                ((0x173b)) /* << 2) + 0xd0100000) */
/* Bit 5:4,	 reg_Det3D_STAYUV_RSHFT	    U2  YUV statistics SAD and SAI
 * calculation result right shift bits to accommodate the 12bits clipping:
 *		0: mainly for images <=720x480:
 *		1: mainly for images <=1366x768:
 *		2: mainly
 * for images <=1920X1080: 2; 3: other higher resolutions
 * Bit 3:2,	 reg_Det3D_STAEDG_RSHFT	    U2  Horizontal and Vertical Edge
 * Statistics SAD and SAI calculation result right shift bits to
 * accommodate the 12bits clipping:
 *		0: mainly for images <=720x480: 1: mainly for images <=1366x768:
 *		2: mainly for images <=1920X1080: 2; 3: other higher resolutions
 * Bit 1:0,	 reg_Det3D_STAMOT_RSHFT	    U2  Motion SAD and SAI
 * calculation result right shift bits to accommodate the 12bits clipping:
 *		0: mainly for images <=720x480: 1: mainly for images <=1366x768:
 *		2: mainly for images <=1920X1080: 2; 3: other higher resolutions
 */
#define DET3D_MAT_SYMTC_TH                ((0x173c)) /* << 2) + 0xd0100000) */
/* Bit 31:24, reg_Det3D_STALUM_symtc_Th	  U8  threshold to decide
 * if the Luma statistics is TB or LR symmetric.
 * Bit 23:16, reg_Det3D_STACHR_symtc_Th	  U8  threshold to decide
 * if the Chroma (UV) statistics is TB or LR symmetric.
 * Bit 15:8,	 reg_Det3D_STAEDG_symtc_Th	  U8  threshold to
 * decide if the Horizontal and Vertical Edge statistics is TB or LR symmetric.
 * Bit 7:0,	 reg_Det3D_STAMOT_symtc_Th	  U8  threshold to
 * decide if the Motion statistics is TB or LR symmetric.
 */
#define DET3D_RO_DET_CB_HOR               ((0x173d)) /* << 2) + 0xd0100000) */
/* Bit 31:16, RO_Det3D_ChessBd_NHor_value    U16  X64: number of Pixels
 * of Horizontally Surely NOT matching Chessboard pattern.
 * Bit 15:0,	 RO_Det3D_ChessBd_Hor_value	    U16  X64: number of
 * Pixels of Horizontally Surely matching Chessboard pattern.
 */
#define DET3D_RO_DET_CB_VER               ((0x173e)) /* << 2) + 0xd0100000) */
/* Bit 31:16, RO_Det3D_ChessBd_NVer_value	U16  X64: number of
 * Pixels of Vertically Surely NOT matching Chessboard pattern.
 * Bit 15:0,	 RO_Det3D_ChessBd_Ver_value	    U16  X64: number
 * of Pixels of Vertically Surely matching Chessboard pattern.
 */
#define DET3D_RO_SPLT_HT                  ((0x173f)) /* << 2) + 0xd0100000) */
/* Bit 24,	 RO_Det3D_Split_HT_valid	U1  horizontal LR split border
 * detected valid signal for top half picture
 * Bit 20:16, RO_Det3D_Split_HT_pxnum	U5  number of pixels included for the
 * LR split position estimation for top half picture
 * Bit 9:0,	 RO_Det3D_Split_HT_idxX4	S10  X4: horizontal pixel
 * shifts of LR split position to the (ColMax/2) for top half picture
 * // DET 3D REG DEFINE END ////
 * // NR2 REG DEFINE BEGIN////
 */
#define NR2_MET_NM_CTRL                   ((0x1745)) /* << 2) + 0xd0100000) */
/* Bit 28,	   reg_NM_reset	     Reset to the status of the Loop filter.
 * Bit 27:24,   reg_NM_calc_length	  Length mode of the Noise
 * measurement sample number for statistics.
 *		0:  256 samples;    1: 512 samples;    2: 1024 samples;
 * ¡­X: 2^(8+x) samples
 * Bit 23:20,   reg_NM_inc_step	      Loop filter input gain increase step.
 * Bit 19:16,   reg_NM_dec_step	      Loop filter input gain decrease step.
 * Bit 15:8,	   reg_NM_YHPmot_thrd	  Luma channel HP portion motion
 * for condition of pixels included in Luma Noise measurement.
 * Bit 7:0,	   reg_NM_CHPmot_thrd	  Chroma channel HP portion motion
 * for condition of pixels included in Chroma Noise measurement.
 */
#define NR2_MET_NM_YCTRL                  ((0x1746)) /* << 2) + 0xd0100000) */
/* Bit 31:28,   reg_NM_YPLL_target	      Target rate of
 * NM_Ynoise_thrd to mean of the Luma Noise
 * Bit 27:24,   reg_NM_YLPmot_thrd	      Luma channel LP
 * portion motion for condition of pixels included in Luma Noise measurement.
 * Bit 23:16,   reg_NM_YHPmot_thrd_min	  Minimum threshold for
 * Luma channel HP portion motion to decide whether the pixel
 * will be included in Luma noise measurement.
 * Bit 15:8,	   reg_NM_YHPmot_thrd_max	  Maximum threshold for Luma
 * channel HP portion motion to decide whether the pixel will be included in
 * Luma noise measurement.
 * Bit 7:0,	   reg_NM_Ylock_rate	      Rate to decide whether the
 * Luma noise measurement is lock or not.
 */
#define NR2_MET_NM_CCTRL                  ((0x1747)) /* << 2) + 0xd0100000) */
/* Bit 31:28,	reg_NM_CPLL_target
 * Target rate of NM_Cnoise_thrd to mean of the Chroma Noise
 * Bit 27:24,	reg_NM_CLPmot_thrd	     Chroma channel LP portion motion
 * for condition of pixels included in Chroma Noise measurement.
 * Bit 23:16,	reg_NM_CHPmot_thrd_min	 Minimum threshold for Chroma channel
 * HP portion motion to decide whether the pixel will be
 * included in Chroma noise measurement.
 * Bit 15:8,	    reg_NM_CHPmot_thrd_max	 Maximum threshold for Chroma
 * channel HP portion motion to decide whether the pixel will be included in
 * Chroma noise measurement.
 * Bit 7:0,	    reg_NM_Clock_rate	     Rate to decide whether the Chroma
 * noise measurement is lock or not;
 */
#define NR2_MET_NM_TNR                    ((0x1748)) /* << 2) + 0xd0100000) */
/* Bit 25,	    ro_NM_TNR_Ylock	         Read-only register to tell
 * ifLuma channel noise measurement is locked or not.
 * Bit 24,	    ro_NM_TNR_Clock	         Read-only register to tell
 * if Chroma channel noise measurement is locked or not.
 * Bit 23:12,	ro_NM_TNR_Ylevel	     Read-only register to give Luma
 * channel noise level. It was 16x of pixel difference in 8 bits of YHPmot.
 * Bit 11:0,	ro_NM_TNR_ClevelRead-only register to give Chroma channel noise
 * level.It was 16x of pixel difference in 8 bits of CHPmot.
 */
#define NR2_MET_NMFRM_TNR_YLEV            ((0x1749)) /* << 2) + 0xd0100000) */
/* Bit 28:0,	ro_NMFrm_TNR_Ylevel	         Frame based Read-only register
 * to give Luma channel noise level within one frame/field.
 */
#define NR2_MET_NMFRM_TNR_YCNT            ((0x174a)) /* << 2) + 0xd0100000) */
/* Bit 23:0,	ro_NMFrm_TNR_Ycount	         Number ofLuma channel pixels
 * included in Frame/Field based noise level measurement.
 */
#define NR2_MET_NMFRM_TNR_CLEV            ((0x174b)) /* << 2) + 0xd0100000) */
/* Bit 28:0,	ro_NMFrm_TNR_Clevel	         Frame based Read-only register
 * to give Chroma channel noise level within one frame/field.
 */
#define NR2_MET_NMFRM_TNR_CCNT            ((0x174c)) /* << 2) + 0xd0100000) */
/* Bit 23:0,	ro_NMFrm_TNR_Ccount	         Number of Chroma channel pixels
 * included in Frame/Field based noise level measurement.
 */
#define NR2_3DEN_MODE                     ((0x174d)) /* << 2) + 0xd0100000) */
/* Bit 6:4,	Blend_3dnr_en_r */
/* Bit 2:0,	Blend_3dnr_en_l */
#define NR2_IIR_CTRL                      ((0x174e)) /* << 2) + 0xd0100000) */
/* Bit 15:14, reg_LP_IIR_8bit_mode	LP IIR membitwidth mode:
 *		0: 10bits will be store in memory;
 *		1: 9bits will be store in memory;
 *		2: 8bits will be store in memory;
 *		3: 7bits will be store in memory;
 * Bit 13:12, reg_LP_IIR_mute_mode	Mode for the LP IIR mute,
 * Bit 11:8,reg_LP_IIR_mute_thrd Threshold of LP IIR mute to avoid ghost:
 * Bit 7:6,	 reg_HP_IIR_8bit_mode	IIR membitwidth mode:
 *		0: 10bits will be store in memory;
 *		1: 9bits will be store in memory;
 *		2: 8bits will be store in memory;
 *		3: 7bits will be store in memory;
 * Bit 5:4,	reg_HP_IIR_mute_mode	Mode for theLP IIR mute
 * Bit 3:0,reg_HP_IIR_mute_thrd	Threshold of HP IIR mute to avoid ghost
 */
#define NR2_SW_EN                         ((0x174f)) /* << 2) + 0xd0100000) */
/* Bit 17:8,	Clk_gate_ctrl
 * Bit 7,	Cfr_enable
 * Bit 5,	Det3d_en
 * Bit 4,	Nr2_proc_en
 * Bit 0,	Nr2_sw_en
 */
#define NR2_FRM_SIZE                      ((0x1750)) /* << 2) + 0xd0100000) */
/* Bit 27:16,  Frm_heigh	Frame/field height */
/* Bit 11: 0,  Frm_width	Frame/field width */
#define NR2_SNR_SAD_CFG                   ((0x1751)) /* << 2) + 0xd0100000) */
/* Bit 12,	reg_MATNR_SNR_SAD_CenRPL	U1, Enable signal for Current
 * pixel position SAD to be replaced by SAD_min.0: do not replace Current pixel
 * position SAD by SAD_min;1: do replacements
 * Bit 11:8,	reg_MATNR_SNR_SAD_coring	Coring value of the intra-frame
 * SAD. sum = (sum - reg_MATNR_SNR_SAD_coring);
 * sum = (sum<0) ? 0: (sum>255)? 255: sum;
 * Bit 6:5,	reg_MATNR_SNR_SAD_WinMod	Unsigned, Intra-frame SAD
 * matching window mode:0: 1x1; 1: [1 1 1] 2: [1 2 1]; 3: [1 2 2 2 1];
 * Bit 4:0,	Sad_coef_num	            Sad coeffient
 */
#define NR2_MATNR_SNR_OS                  ((0x1752)) /* << 2) + 0xd0100000) */
/* Bit 7:4,	reg_MATNR_SNR_COS	    SNR Filter overshoot control
 * margin for UV channel (X2 to u10 scale)
 * Bit 3:0,	reg_MATNR_SNR_YOS	    SNR Filter overshoot control
 * margin for luma channel (X2 to u10 scale)
 */
#define NR2_MATNR_SNR_NRM_CFG             ((0x1753)) /* << 2) + 0xd0100000) */
/* Bit 23:16,	reg_MATNR_SNR_NRM_ofst	Edge based SNR
 *		boosting normalization offset to SAD_max ;
 * Bit 15:8,	    reg_MATNR_SNR_NRM_max
 *		Edge based SNR boosting normalization Max value
 * Bit 7:0,	    reg_MATNR_SNR_NRM_min
 *		Edge based SNR boosting normalization Min value
 */
#define NR2_MATNR_SNR_NRM_GAIN            ((0x1754)) /* << 2) + 0xd0100000) */
/* Bit 15:8,	reg_MATNR_SNR_NRM_Cgain	Edge based SNR boosting
 *		normalization Gain for Chrm channel (norm 32 as 1)
 * Bit 7:0,	reg_MATNR_SNR_NRM_Ygain	Edge based SNR boosting
 *		normalization Gain for Luma channel (norm 32 as 1)
 */
#define NR2_MATNR_SNR_LPF_CFG             ((0x1755)) /* << 2) + 0xd0100000) */
/* Bit 23:16,reg_MATNR_SNRLPF_SADmaxTH	U8,  Threshold to SADmax to use TNRLPF
 * to replace SNRLPF. i.e.if (SAD_max<reg_MATNR_SNRLPF_SADmaxTH)
 * SNRLPF_yuv[k] = TNRLPF_yuv[k];
 * Bit 13:11,reg_MATNR_SNRLPF_Cmode
 * LPF based SNR filtering mode on CHRM channel:
 *		0: gradient LPF [1 1]/2, 1: gradient LPF [2 1 1]/4;
 *		2: gradient LPF [3 3 2]/8; 3: gradient LPF [5 4 4 3]/16;
 *		4: TNRLPF;  5 : CurLPF3x3_yuv[];
 *		6: CurLPF3o3_yuv[]  7: CurLPF3x5_yuv[]
 * Bit 10:8,	reg_MATNR_SNRLPF_Ymode
 * LPF based SNR filtering mode on LUMA channel:
 *		0: gradient LPF //Bit [1 1]/2, 1: gradient LPF [2 1 1]/4;
 *		2: gradient LPF [3 3 2]/8;3: gradient LPF [5 4 4 3]/16;
 *		4: TNRLPF;               5 : CurLPF3x3_yuv[];
 *		6: CurLPF3o3_yuv[]         7: CurLPF3x5_yuv[]
 * Bit 7:4,	reg_MATNR_SNRLPF_SADmin3TH	Offset threshold to SAD_min to
 * Discard SAD_min3 corresponding pixel in LPF SNR filtering. (X8 to u8 scale)
 * Bit 3:0,	reg_MATNR_SNRLPF_SADmin2TH	Offset threshold to SAD_min to
 * Discard SAD_min2 corresponding pixel in LPF SNR filtering. (X8 to u8 scale)
 */
#define NR2_MATNR_SNR_USF_GAIN           ((0x1756)) /* << 2) + 0xd0100000) */
/* Bit 15:8,	reg_MATNR_SNR_USF_Cgain
 * Un-sharp (HP) compensate back Chrm portion gain, (norm 64 as 1)
 * Bit 7:0,	reg_MATNR_SNR_USF_Ygain
 * Un-sharp (HP) compensate back Luma portion gain, (norm 64 as 1)
 */
#define NR2_MATNR_SNR_EDGE2B             ((0x1757)) /* << 2) + 0xd0100000) */
/* Bit 15:8,	reg_MATNR_SNR_Edge2Beta_ofst	U8,
 * Offset for Beta based on Edge.
 * Bit 7:0,	reg_MATNR_SNR_Edge2Beta_gain	U8.
 * Gain to SAD_min for Beta based on Edge. (norm 16 as 1)
 */
#define NR2_MATNR_BETA_EGAIN             ((0x1758)) /* << 2) + 0xd0100000) */
/* Bit 15:8,	reg_MATNR_CBeta_Egain	U8,
 * Gain to Edge based Beta for Chrm channel. (normalized to 32 as 1)
 * Bit 7:0,	reg_MATNR_YBeta_Egain	U8,
 * Gain to Edge based Beta for Luma channel. (normalized to 32 as 1)
 */
#define NR2_MATNR_BETA_BRT               ((0x1759)) /* << 2) + 0xd0100000) */
/* Bit 31:28,	reg_MATNR_beta_BRT_limt_hi	U4,
 * Beta adjustment based on Brightness high side Limit. (X16 to u8 scale)
 * Bit 27:24,	reg_MATNR_beta_BRT_slop_hi	U4,
 * Beta adjustment based on Brightness high side slope. Normalized to 16 as 1
 * Bit 23:16,	reg_MATNR_beta_BRT_thrd_hi	U8,
 * Beta adjustment based on Brightness high threshold.(u8 scale)
 * Bit 15:12,	reg_MATNR_beta_BRT_limt_lo	U4,
 * Beta adjustment based on Brightness low side Limit. (X16 to u8 scale)
 * Bit 11:8,	    reg_MATNR_beta_BRT_slop_lo	U4,
 * Beta adjustment based on Brightness low side slope. Normalized to 16 as 1
 * Bit 7:0,	    reg_MATNR_beta_BRT_thrd_lo	U8,
 * Beta adjustment based on Brightness low threshold.(u8 scale)
 */
#define NR2_MATNR_XBETA_CFG              ((0x175a)) /* << 2) + 0xd0100000) */
/* Bit 19:18,	reg_MATNR_CBeta_use_mode	U2,
 * Beta options (mux) from beta_motion and beta_edge for Chrm channel;
 * Bit 17:16,	reg_MATNR_YBeta_use_mode	U2,
 * Beta options (mux) from beta_motion and beta_edge for Luma channel;
 * Bit 15: 8,	reg_MATNR_CBeta_Ofst	    U8,
 * Offset to Beta for Chrm channel.(after beta_edge and beta_motion mux)
 * Bit  7: 0,	reg_MATNR_YBeta_Ofst	    U8,
 * Offset to Beta for Luma channel.(after beta_edge and beta_motion mux)
 */
#define NR2_MATNR_YBETA_SCL              ((0x175b)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_YBeta_scale_min	U8,
 * Final step Beta scale low limit for Luma channel;
 * Bit 23:16,	reg_MATNR_YBeta_scale_max	U8,
 * Final step Beta scale high limit for Luma channe;
 * Bit 15: 8,	reg_MATNR_YBeta_scale_gain	U8,
 * Final step Beta scale Gain for Luma channel (normalized 32 to 1);
 * Bit 7 : 0,	reg_MATNR_YBeta_scale_ofst	S8,
 * Final step Beta scale offset for Luma channel ;
 */
#define NR2_MATNR_CBETA_SCL              ((0x175c)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_CBeta_scale_min
 * Final step Beta scale low limit for Chrm channel.Similar to Y
 * Bit 23:16,	reg_MATNR_CBeta_scale_max	U8,
 * Final step Beta scale high limit for Chrm channel.Similar to Y
 * Bit 15: 8,	reg_MATNR_CBeta_scale_gain	U8,
 * Final step Beta scale Gain for Chrm channel Similar to Y
 * Bit  7: 0,	reg_MATNR_CBeta_scale_ofst	S8,
 * Final step Beta scale offset for Chrm channel Similar to Y
 */
#define NR2_SNR_MASK                     ((0x175d)) /* << 2) + 0xd0100000) */
/* Bit 20:0,	SAD_MSK	                 Valid signal in the 3x7 SAD surface */
#define NR2_SAD2NORM_LUT0                ((0x175e)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_SAD2Norm_LUT_3
 *		SAD convert normal LUT node 3
 * Bit 23:16,	reg_MATNR_SAD2Norm_LUT_2
 *		SAD convert normal LUT node 2
 * Bit 15: 8,	reg_MATNR_SAD2Norm_LUT_1
 *		SAD convert normal LUT node 1
 * Bit  7: 0,	reg_MATNR_SAD2Norm_LUT_0
 *		SAD convert normal LUT node 0
 */
#define NR2_SAD2NORM_LUT1                ((0x175f)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_SAD2Norm_LUT_7
 *		SAD convert normal LUT node 7
 * Bit 23:16,	reg_MATNR_SAD2Norm_LUT_6
 *		SAD convert normal LUT node 6
 * Bit 15: 8,	reg_MATNR_SAD2Norm_LUT_5
 *		SAD convert normal LUT node 5
 * Bit  7: 0,	reg_MATNR_SAD2Norm_LUT_4
 *		SAD convert normal LUT node 4
 */
#define NR2_SAD2NORM_LUT2                ((0x1760)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_SAD2Norm_LUT_11
 *		SAD convert normal LUT node 11
 * Bit 23:16,	reg_MATNR_SAD2Norm_LUT_10
 *		SAD convert normal LUT node 10
 * Bit 15: 8,	reg_MATNR_SAD2Norm_LUT_9
 *		SAD convert normal LUT node 9
 * Bit  7: 0,	reg_MATNR_SAD2Norm_LUT_8
 *		SAD convert normal LUT node 8
 */
#define NR2_SAD2NORM_LUT3                ((0x1761)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_SAD2Norm_LUT_15 SAD convert normal LUT node 15 */
/* Bit 23:16,	reg_MATNR_SAD2Norm_LUT_14 SAD convert normal LUT node 14 */
/* Bit 15:8,	reg_MATNR_SAD2Norm_LUT_13 SAD convert normal LUT node 13 */
/* Bit 7:0,	reg_MATNR_SAD2Norm_LUT_12 SAD convert normal LUT node 12 */
#define NR2_EDGE2BETA_LUT0               ((0x1762)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Edge2Beta_LUT_3 Edge convert beta LUT node 3 */
/* Bit 23:16,	reg_MATNR_Edge2Beta_LUT_2 Edge convert beta LUT node 2 */
/* Bit 15: 8,	reg_MATNR_Edge2Beta_LUT_1 Edge convert beta LUT node 1 */
/* Bit  7: 0,	reg_MATNR_Edge2Beta_LUT_0 Edge convert beta LUT node 0 */
#define NR2_EDGE2BETA_LUT1               ((0x1763)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Edge2Beta_LUT_7 Edge convert beta LUT node 7 */
/* Bit 23:16,	reg_MATNR_Edge2Beta_LUT_6 Edge convert beta LUT node 6 */
/* Bit 15: 8,	reg_MATNR_Edge2Beta_LUT_5 Edge convert beta LUT node 5 */
/* Bit  7: 0,	reg_MATNR_Edge2Beta_LUT_4 Edge convert beta LUT node 4 */
#define NR2_EDGE2BETA_LUT2               ((0x1764)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Edge2Beta_LUT_11 Edge convert beta LUT node 11 */
/* Bit 23:16,	reg_MATNR_Edge2Beta_LUT_10 Edge convert beta LUT node 10 */
/* Bit 15: 8,	reg_MATNR_Edge2Beta_LUT_9  Edge convert beta LUT node 9 */
/* Bit  7: 0,	reg_MATNR_Edge2Beta_LUT_8  Edge convert beta LUT node 8 */
#define NR2_EDGE2BETA_LUT3               ((0x1765)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Edge2Beta_LUT_15 Edge convert beta LUT node 15 */
/* Bit 23:16,	reg_MATNR_Edge2Beta_LUT_14 Edge convert beta LUT node 14 */
/* Bit 15: 8,	reg_MATNR_Edge2Beta_LUT_13 Edge convert beta LUT node 13 */
/* Bit  7: 0,	reg_MATNR_Edge2Beta_LUT_12 Edge convert beta LUT node 12 */
#define NR2_MOTION2BETA_LUT0             ((0x1766)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Mot2Beta_LUT_3 Motion convert beta LUT node 3 */
/* Bit 23:16,	reg_MATNR_Mot2Beta_LUT_2 Motion convert beta LUT node 2 */
/* Bit 15: 8,	reg_MATNR_Mot2Beta_LUT_1 Motion convert beta LUT node 1 */
/* Bit  7: 0,	reg_MATNR_Mot2Beta_LUT_0 Motion convert beta LUT node 0 */
#define NR2_MOTION2BETA_LUT1             ((0x1767)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Mot2Beta_LUT_7 Motion convert beta LUT node 7 */
/* Bit 23:16,	reg_MATNR_Mot2Beta_LUT_6 Motion convert beta LUT node 6 */
/* Bit 15: 8,	reg_MATNR_Mot2Beta_LUT_5 Motion convert beta LUT node 5 */
/* Bit  7: 0,	reg_MATNR_Mot2Beta_LUT_4 Motion convert beta LUT node 4 */
#define NR2_MOTION2BETA_LUT2             ((0x1768)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Mot2Beta_LUT_11 Motion convert beta LUT node 11 */
/* Bit 23:16,	reg_MATNR_Mot2Beta_LUT_10 Motion convert beta LUT node 10 */
/* Bit 15: 8,	reg_MATNR_Mot2Beta_LUT_9  Motion convert beta LUT node 9 */
/* Bit  7: 0,	reg_MATNR_Mot2Beta_LUT_8  Motion convert beta LUT node 8 */
#define NR2_MOTION2BETA_LUT3             ((0x1769)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_Mot2Beta_LUT_15 Motion convert beta LUT node 15 */
/* Bit 23:16,	reg_MATNR_Mot2Beta_LUT_14 Motion convert beta LUT node 14 */
/* Bit 15: 8,	reg_MATNR_Mot2Beta_LUT_13 Motion convert beta LUT node 13 */
/* Bit  7: 0,	reg_MATNR_Mot2Beta_LUT_12 Motion convert beta LUT node 12 */
#define NR2_MATNR_MTN_CRTL               ((0x176a)) /* << 2) + 0xd0100000) */
/* Bit 25:24,	reg_MATNR_Vmtn_use_mode	    Motion_yuvV channel motion selection
 * mode:	0: Vmot;
 *			1:Ymot/2 + (Umot+Vmot)/4;
 *			2:Ymot/2 + max(Umot,Vmot)/2;
 *			3: max(Ymot,Umot, Vmot)
 * Bit 21:20,	reg_MATNR_Umtn_use_mode	    Motion_yuvU channel motion selection
 * mode:	0:Umot;
 *			1:Ymot/2 + (Umot+Vmot)/4;
 *			2:Ymot/2 + max(Umot,Vmot)/2;
 *			3: max(Ymot,Umot, Vmot)
 * Bit 17:16,	reg_MATNR_Ymtn_use_mode	 Motion_yuvLuma channel motion selection
 * mode:	0:  Ymot,
 *			1: Ymot/2 + (Umot+Vmot)/4;
 *			2: Ymot/2 + max(Umot,Vmot)/2;
 *			3:  max(Ymot,Umot, Vmot)
 * Bit 13:12,	reg_MATNR_mtn_txt_mode
 * Texture detection mode for adaptive coring of HP motion
 * Bit  9: 8,	reg_MATNR_mtn_cor_mode
 * Coring selection mode based on texture detection;
 * Bit  6: 4,	reg_MATNR_mtn_hpf_mode
 * video mode of current and previous frame/field for MotHPF_yuv[k] calculation:
 * Bit  2: 0,	reg_MATNR_mtn_lpf_mode	LPF video mode of current and previous
 * frame/field for MotLPF_yuv[k] calculation:
 */
#define NR2_MATNR_MTN_CRTL2              ((0x176b)) /* << 2) + 0xd0100000) */
/* Bit 18:16,	reg_MATNR_iir_BS_Ymode	    IIR TNR filter Band split filter
 * mode for Luma LPF result generation (Cur and Prev);
 * Bit 15: 8,	reg_MATNR_mtnb_alpLP_Cgain	Scale of motion_brthp_uv to
 * motion_brtlp_uv, normalized to 32 as 1
 * Bit  7: 0,	reg_MATNR_mtnb_alpLP_Ygain	Scale of motion_brthp_y to
 * motion_brtlp_y, normalized to 32 as 1
 */
#define NR2_MATNR_MTN_COR                ((0x176c)) /* << 2) + 0xd0100000) */
/* Bit 15:12,	reg_MATNR_mtn_cor_Cofst	    Coring Offset for Chroma Motion.
 * Bit 11: 8,	reg_MATNR_mtn_cor_Cgain	    Gain to texture based coring for
 * Chroma Motion. Normalized to 16 as 1
 * Bit  7: 4,	reg_MATNR_mtn_cor_Yofst	    Coring Offset for Luma Motion.
 * Bit  3: 0,	reg_MATNR_mtn_cor_Ygain	    Gain to texture based coring for
 * Luma Motion. Normalized to 16 as 1
 */
#define NR2_MATNR_MTN_GAIN               ((0x176d)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_mtn_hp_Cgain	Gain to MotHPF_yuv[k] Chrm channel for
 * motion calculation, normalized to 64 as 1
 * Bit 23:16,	reg_MATNR_mtn_hp_Ygain	Gain to MotHPF_yuv[k] Luma channel for
 * motion calculation, normalized to 64 as 1
 * Bit 15: 8,	reg_MATNR_mtn_lp_Cgain	Gain to MotLPF_yuv[k] Chrm channel for
 * motion calculation, normalized to 32 as 1
 * Bit  7: 0,	reg_MATNR_mtn_lp_Ygain	Gain to MotLPF_yuv[k] Luma channel for
 * motion calculation, normalized to 32 as 1
 */
#define NR2_MATNR_DEGHOST                ((0x176e)) /* << 2) + 0xd0100000) */
/* Bit 8,	reg_MATNR_DeGhost_En
 * Enable signal for DeGhost function:0: disable; 1: enable
 * Bit 7:4,	reg_MATNR_DeGhost_COS
 * DeGhost Overshoot margin for UV channel, (X2 to u10 scale)
 * Bit 3:0,	reg_MATNR_DeGhost_YOS
 * DeGhost Overshoot margin for Luma channel, (X2 to u10 scale)
 */
#define NR2_MATNR_ALPHALP_LUT0           ((0x176f)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaLP_LUT_3
 * Matnr low-pass filter alpha LUT node 3
 */
/* Bit 23:16,	reg_MATNR_AlphaLP_LUT_2
 * Matnr low-pass filter alpha LUT node 2
 */
/* Bit 15: 8,	reg_MATNR_AlphaLP_LUT_1
 * Matnr low-pass filter alpha LUT node 1
 */
/* Bit  7: 0,	reg_MATNR_AlphaLP_LUT_0
 *Matnr low-pass filter alpha LUT node 0
 */
#define NR2_MATNR_ALPHALP_LUT1           ((0x1770)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaLP_LUT_7
 * Matnr low-pass filter alpha LUT node 7
 */
/* Bit 23:16,	reg_MATNR_AlphaLP_LUT_6
 * Matnr low-pass filter alpha LUT node 6
 */
/* Bit 15: 8,	reg_MATNR_AlphaLP_LUT_5
 * Matnr low-pass filter alpha LUT node 5
 */
/* Bit  7: 0,	reg_MATNR_AlphaLP_LUT_4
 * Matnr low-pass filter alpha LUT node 4
 */
#define NR2_MATNR_ALPHALP_LUT2           ((0x1771)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaLP_LUT_11
 * Matnr low-pass filter alpha LUT node 11
 */
/* Bit 23:16,	reg_MATNR_AlphaLP_LUT_10
 * Matnr low-pass filter alpha LUT node 10
 */
/* Bit 15: 8,	reg_MATNR_AlphaLP_LUT_9
 * Matnr low-pass filter alpha LUT node 9
 */
/* Bit  7: 0,	reg_MATNR_AlphaLP_LUT_8
 * Matnr low-pass filter alpha LUT node 8
 */
#define NR2_MATNR_ALPHALP_LUT3           ((0x1772)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaLP_LUT_15
 * Matnr low-pass filter alpha LUT node 15
 */
/* Bit 23:16,	reg_MATNR_AlphaLP_LUT_14
 * Matnr low-pass filter alpha LUT node 14
 */
/* Bit 15: 8,	reg_MATNR_AlphaLP_LUT_13
 * Matnr low-pass filter alpha LUT node 13
 */
/* Bit  7: 0,	reg_MATNR_AlphaLP_LUT_12
 * Matnr low-pass filter alpha LUT node 12
 */
#define NR2_MATNR_ALPHAHP_LUT0           ((0x1773)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaHP_LUT_3
 * Matnr high-pass filter alpha LUT node 3
 */
/* Bit 23:16,	reg_MATNR_AlphaHP_LUT_2
 * Matnr high-pass filter alpha LUT node 2
 */
/* Bit 15: 8,	reg_MATNR_AlphaHP_LUT_1
 * Matnr high-pass filter alpha LUT node 1
 */
/* Bit  7: 0,	reg_MATNR_AlphaHP_LUT_0
 * Matnr high-pass filter alpha LUT node 0
 */
#define NR2_MATNR_ALPHAHP_LUT1           ((0x1774)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaHP_LUT_7
 * Matnr high-pass filter alpha LUT node 7
 */
/* Bit 23:16,	reg_MATNR_AlphaHP_LUT_6
 * Matnr high-pass filter alpha LUT node 6
 */
/* Bit 15: 8,	reg_MATNR_AlphaHP_LUT_5
 * Matnr high-pass filter alpha LUT node 5
 */
/* Bit  7: 0,	reg_MATNR_AlphaHP_LUT_4
 * Matnr high-pass filter alpha LUT node 4
 */
#define NR2_MATNR_ALPHAHP_LUT2           ((0x1775)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaHP_LUT_11
 * Matnr high-pass filter alpha LUT node 11
 */
/* Bit 23:16,	reg_MATNR_AlphaHP_LUT_10
 * Matnr high-pass filter alpha LUT node 10
 */
/* Bit 15: 8,	reg_MATNR_AlphaHP_LUT_9
 * Matnr high-pass filter alpha LUT node 9
 */
/* Bit  7: 0,	reg_MATNR_AlphaHP_LUT_8
 * Matnr high-pass filter alpha LUT node 8
 */
#define NR2_MATNR_ALPHAHP_LUT3           ((0x1776)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_MATNR_AlphaHP_LUT_15
 * Matnr high-pass filter alpha LUT node 15
 */
/* Bit 23:16,	reg_MATNR_AlphaHP_LUT_14
 * Matnr high-pass filter alpha LUT node 14
 */
/* Bit 15: 8,	reg_MATNR_AlphaHP_LUT_13
 * Matnr high-pass filter alpha LUT node 13
 */
/* Bit  7: 0,	reg_MATNR_AlphaHP_LUT_12
 *Matnr high-pass filter alpha LUT node 12
 */
#define NR2_MATNR_MTNB_BRT               ((0x1777)) /* << 2) + 0xd0100000) */
/* Bit 31:28,	reg_MATNR_mtnb_BRT_limt_hi	Motion adjustment based on
 * Brightness high side Limit. (X16 to u8 scale)
 */
/* Bit 27:24,	reg_MATNR_mtnb_BRT_slop_hi	Motion adjustment based on
 * Brightness high side slope. Normalized to 16 as 1
 */
/* Bit 23:16,	reg_MATNR_mtnb_BRT_thrd_hi	Motion adjustment based on
 * Brightness high threshold.(u8 scale)
 */
/* Bit 15:12,	reg_MATNR_mtnb_BRT_limt_lo	Motion adjustment based on
 * Brightness low side Limit. (X16 to u8 scale)
 */
/* Bit 11: 8,	reg_MATNR_mtnb_BRT_slop_lo	Motion adjustment based on
 * Brightness low side slope. Normalized to 16 as 1
 */
/* Bit  7: 0,	reg_MATNR_mtnb_BRT_thrd_lo	Motion adjustment based on
 * Brightness low threshold.(u8 scale)
 */
#define NR2_CUE_MODE                     ((0x1778)) /* << 2) + 0xd0100000) */
/* Bit 9,	Cue_enable_r	        Cue right half frame enable */
/* Bit 8,	Cue_enable_l            Cue left half frame enable */
/* Bit 6:4,	reg_CUE_CON_RPLC_mode	U3, CUE pixel chroma replace mode; */
/* Bit 2:0,	reg_CUE_CHRM_FLT_mode	U3, CUE improvement filter mode, */
#define NR2_CUE_CON_MOT_TH               ((0x1779)) /* << 2) + 0xd0100000) */
/* Bit 31:24,	reg_CUE_CON_Cmot_thrd2	U8,  Motion Detection threshold of
 * up/down two rows,  Chroma channel in Chroma Up-sampling Error (CUE)
 * Detection (tighter).
 */
/* Bit 23:16,	reg_CUE_CON_Ymot_thrd2	U8,  Motion Detection threshold of
 * up/mid/down three rows,  Luma channel in Chroma Up-sampling Error (CUE)
 * Detection (tighter).
 */
/* Bit 15: 8,	reg_CUE_CON_Cmot_thrd	U8,  Motion Detection threshold of
 * up/down two rows, Chroma channel in Chroma Up-sampling Error (CUE) Detection.
 */
/* Bit  7: 0,	reg_CUE_CON_Ymot_thrd	U8,  Motion Detection threshold of
 * up/mid/down three rows, Luma channel in
 * Chroma Up-sampling Error (CUE) Detection.
 */
#define NR2_CUE_CON_DIF0                 ((0x177a)) /* << 2) + 0xd0100000) */
/* Bit 15:8,	reg_CUE_CON_difP1_thrd	    U8,  P1 field Intra-Field top/below
 * line chroma difference threshold,
 */
/* Bit 7:0,	reg_CUE_CON_difCur_thrd	    U8,  Current Field/Frame Intra-Field
 * up/down line chroma difference threshold,
 */
#define NR2_CUE_CON_DIF1                 ((0x177b)) /* << 2) + 0xd0100000) */
/* Bit 19:16,	reg_CUE_CON_rate0	    U4,  The Krate to decide CUE by
 * relationship between CUE_diflG and CUE_difEG
 */
/* Bit 15: 8,	reg_CUE_CON_difEG_thrd	U8,  Theshold to the difference between
 * current Field/Frame middle line to down line color channel(CUE_difEG).
 */
/* Bit  7: 0,	reg_CUE_CON_diflG_thrd	U8,  Threshold to the difference between
 * P1 field top line to current Field/Frame down line color channel (CUE_diflG).
 */
#define NR2_CUE_CON_DIF2                 ((0x177c)) /* << 2) + 0xd0100000) */
/* Bit 19:16,	reg_CUE_CON_rate1	    U4,  The Krate to decide CUE by
 * relationship between CUE_difnC and CUE_difEC
 */
/* Bit 15: 8,	reg_CUE_CON_difEC_thrd	U8,  Theshold to the difference between
 * current Field/Frame middle line to up line color channel(CUE_difEC).
 */
/* Bit  7: 0,	reg_CUE_CON_difnC_thrd	U8,  Threshold to the difference between
 * P1 field bot line to current Field/Frame up line color channel (CUE_difnC).
 */
#define NR2_CUE_CON_DIF3                 ((0x177d)) /* << 2) + 0xd0100000) */
/* Bit 19:16,	reg_CUE_CON_rate2	    U4,  The Krate to decide CUE by
 * relationship between CUE_difP1 and CUE_difEP1
 */
/* Bit 15: 8,	reg_CUE_CON_difEP1_thrd	U8,  Inter-Field top/below line to
 * current field/frame middle line chroma difference (CUE_difEP1) threshold.
 */
/* Bit  7: 0,	reg_CUE_CON_difP1_thrd2	U8,  P1 field Intra-Field top/below line
 * chroma difference threshold (tighter),
 */
/* change from txlx */
#define DI_EI_DRT_CTRL                  ((0x1778))

#define DI_EI_DRT_PIXTH                 ((0x1779))

#define DI_EI_DRT_CORRPIXTH             ((0x177a))

#define DI_EI_DRT_RECTG_WAVE            ((0x177b))

#define DI_EI_DRT_PIX_DIFFTH            ((0x177c))

#define DI_EI_DRT_UNBITREND_TH          ((0x177d))

#define NR2_CUE_PRG_DIF                  ((0x177e)) /* << 2) + 0xd0100000) */
/* Bit 20,	    reg_CUE_PRG_Enable	    Enable bit for progressive video CUE
 * detection.If interlace input video,
 */
/* Bit 19:16,	reg_CUE_PRG_rate	    U3,  The Krate to decide CUE by
 * relationship between CUE_difCur and (CUE_difEC+CUE_difEG)
 */
/* Bit 15: 8,	reg_CUE_PRG_difCEG_thrd	U8, Current Frame Intra-Field up-mid and
 * mid-down line chroma difference threshold
 * for progressive video CUE detection,
 */
/* Bit  7: 0,	reg_CUE_PRG_difCur_thrd	U8,  Current Frame Intra-Field up/down
 * line chroma difference threshold,
 */
#define NR2_CONV_MODE                    ((0x177f)) /* << 2) + 0xd0100000) */
/* Bit 3:2,	Conv_c444_mode
 * The format convert mode about 422 to 444 when data read out line buffer
 */
/* Bit 1:0,	Conv_c422_mode
 * the format convert mode about 444 to 422 when data write to line buffer
 */
/* // NR2 REG DEFINE END //// */
/* // DET 3D REG DEFINE BEGIN //// */
/* for gxlx */
#define DI_EI_DRT_CTRL_GXLX                  ((0x2028))

#define DI_EI_DRT_PIXTH_GXLX                 ((0x2029))

#define DI_EI_DRT_CORRPIXTH_GXLX             ((0x202a))

#define DI_EI_DRT_RECTG_WAVE_GXLX            ((0x202b))

#define DI_EI_DRT_PIX_DIFFTH_GXLX            ((0x202c))

#define DI_EI_DRT_UNBITREND_TH_GXLX          ((0x202d))
#define DET3D_RO_SPLT_HB                 ((0x1780)) /* << 2) + 0xd0100000) */
/* Bit 24,	    RO_Det3D_Split_HB_valid
 * U1   horizontal LR split border detected valid signal for top half picture
 */
/* Bit 20:16,	RO_Det3D_Split_HB_pxnum	    U5   number of pixels included for
 * the LR split position estimation for top half picture
 */
/* Bit  9: 0,	RO_Det3D_Split_HB_idxX4	    S10  X4: horizontal pixel shifts of
 * LR split position to the (ColMax/2) for top half picture
 */
#define DET3D_RO_SPLT_VL                 ((0x1781)) /* << 2) + 0xd0100000) */
/* Bit 24,	    RO_Det3D_Split_VL_valid	    U1   horizontal LR split
 * border detected valid signal for top half picture
 */
/* Bit 20:16,	RO_Det3D_Split_VL_pxnum	    U5   number of pixels included for
 * the LR split position estimation for top half picture
 */
/* Bit  9: 0,	RO_Det3D_Split_VL_idxX4	    S10  X4: horizontal pixel shifts of
 * LR split position to the (ColMax/2) for top half picture
 */
#define DET3D_RO_SPLT_VR                 ((0x1782)) /* << 2) + 0xd0100000) */
/* Bit 24   ,	RO_Det3D_Split_VR_valid	    U1   horizontal LR split border
 * detected valid signal for top half picture
 */
/* Bit 20:16,	RO_Det3D_Split_VR_pxnum	    U5   number of pixels included for
 * the LR split position estimation for top half picture
 */
/* Bit  9: 0,	RO_Det3D_Split_VR_idxX4	    S10  X4: horizontal pixel shifts of
 * LR split position to the (ColMax/2) for top half picture
 */
#define DET3D_RO_MAT_LUMA_LR             ((0x1783)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Luma_LR_score	 S2*8  LUMA statistics left right
 * decision score for each band (8bands vertically),
 */
/* it can be -1/0/1:-1: most likely not LR symmetric 0: not sure
 * 1: most likely LR symmetric
 */
/* Bit 7:0,	RO_Luma_LR_symtc	 U1*8  Luma statistics left
 * right pure symmetric for each band (8bands vertically),
 */
/* it can be 0/1: 0: not sure 1: most likely LR is pure symmetric */
/* Bit 4:0,	RO_Luma_LR_sum	     S5  Total score of 8x8 Luma
 * statistics for LR like decision,
 */
/* the larger this score, the more confidence that this is a LR 3D video.
 * It is sum of  RO_Luma_LR_score[0~7]
 */
#define DET3D_RO_MAT_LUMA_TB             ((0x1784)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Luma_TB_score	 S2*8  LUMA statistics Top/Bottom
 * decision score for each band (8bands Horizontally),
 */
/* Bit 7:0,	RO_Luma_TB_symtc	 Luma statistics Top/Bottompure
 * symmetric for each band (8bands Horizontally),
 */
/* Bit 4:0,	RO_Luma_TB_sum
 * Total score of 8x8 Luma statistics for TB like decision,
 */
#define DET3D_RO_MAT_CHRU_LR             ((0x1785)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_ChrU_LR_score	S2*8  LUMA statistics left right
 * decision score for each band (8bands vertically),
 */
/* Bit 7:0,	RO_ChrU_LR_symtc	CHRU statistics left right pure
 * symmetric for each band (8bands vertically),
 */
/* Bit 4:0,	RO_ChrU_LR_sum
 * Total score of 8x8 ChrU statistics for LR like decision,
 */
#define DET3D_RO_MAT_CHRU_TB             ((0x1786)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_ChrU_TB_score	S2*8  CHRU statistics Top/Bottom
 * decision score for each band (8bands Horizontally)
 */
/* Bit 7:0,	RO_ChrU_TB_symtc	CHRU statistics Top/Bottompure symmetric
 * for each band (8bands Horizontally)
 */
/* Bit 4:0,	RO_ChrU_TB_sum
 * Total score of 8x8 ChrU statistics for TB like decision
 */
#define DET3D_RO_MAT_CHRV_LR             ((0x1787)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_ChrV_LR_score	S2*8  CHRUstatistics left right decision
 * score for each band (8bands vertically)
 */
/* Bit 7:0,	RO_ChrV_LR_symtc	CHRV statistics left right pure
 * symmetric for each band (8bands vertically)
 */
/* Bit 4:0,	RO_ChrV_LR_sum
 * Total score of 8x8 ChrV statistics for LR like decision
 */
#define DET3D_RO_MAT_CHRV_TB             ((0x1788)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_ChrV_TB_score	CHRV statistics Top/Bottom decision
 * score for each band (8bands Horizontally)
 */
/* Bit 7:0,	RO_ChrV_TB_symtc	CHRV statistics Top/Bottompure
 * symmetric for each band (8bands Horizontally)
 */
/* Bit 4:0,	RO_ChrV_TB_sum	    Total score of 8x8 ChrV statistics
 * for TB like decision
 */
#define DET3D_RO_MAT_HEDG_LR             ((0x1789)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Hedg_LR_score	Horizontal Edge statistics left right
 * decision score for each band (8bands vertically)
 */
/* Bit 7:0,	RO_Hedg_LR_symtc	Horizontal Edge statistics left right
 * pure symmetric for each band (8bands vertically)
 */
/* Bit 4:0,	RO_Hedg_LR_sum	    Total score of 8x8 Hedg statistics for
 * LR like decision
 */
#define DET3D_RO_MAT_HEDG_TB             ((0x178a)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Hedg_TB_score	Horizontal Edge statistics Top/Bottom
 * decision score for each band (8bands Horizontally)
 */
/* Bit 7:0,	RO_Hedg_TB_symtc	Horizontal Edge statistics
 * Top/Bottompure symmetric for each band (8bands Horizontally)
 */
/* Bit 4:0,	RO_Hedg_TB_sum
 * Total score of 8x8 Hedg statistics for TB like decision
 */
#define DET3D_RO_MAT_VEDG_LR             ((0x178b)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Vedg_LR_score	Vertical Edge statistics left right
 * decision score for each band (8bands vertically)
 */
/* Bit 7:0,	RO_Vedg_LR_symtc	Vertical Edge statistics left right
 * pure symmetric for each band (8bands vertically)
 */
/* Bit 4:0,	RO_Vedg_LR_sum
 * Total score of 8x8 Vedg statistics for LR like decision
 */
#define DET3D_RO_MAT_VEDG_TB             ((0x178c)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Vedg_TB_score	Vertical Edge statistics Top/Bottom
 * decision score for each band (8bands Horizontally)
 */
/* Bit 7:0,	RO_Vedg_TB_symtc	Vertical Edge statistics Top/Bottompure
 * symmetric for each band (8bands Horizontally)
 */
/* Bit 4:0,	RO_Vedg_TB_sum
 * Total score of 8x8 Vedg statistics for TB like decision
 */
#define DET3D_RO_MAT_MOTN_LR             ((0x178d)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Motn_LR_score	Motion statistics left right decision
 * score for each band (8bands vertically)
 */
/* Bit 7:0,	RO_Motn_LR_symtc	Motion statistics left right pure
 * symmetric for each band (8bands vertically)
 */
/* Bit 4:0,	RO_Motn_LR_sum	    Total score of 8x8 Motion statistics for
 * LR like decision
 */
#define DET3D_RO_MAT_MOTN_TB             ((0x178e)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Motn_TB_score	Motion statistics Top/Bottom decision
 * score for each band (8bands Horizontally)
 */
/* Bit 7:0,	RO_Motn_TB_symtc	Motion statistics Top/Bottompure
 * symmetric for each band (8bands Horizontally)
 */
/* Bit 4:0,	RO_Motn_TB_sum
 * Total score of 8x8 Motion statistics for TB like decision
 */
#define DET3D_RO_FRM_MOTN                ((0x178f)) /* << 2) + 0xd0100000) */
/* Bit 15:0,	RO_Det3D_Frame_Motion	U16  frame based motion value sum for
 * still image decision in FW.
 */
/* mat ram read enter addr */
#define DET3D_RAMRD_ADDR_PORT            ((0x179a)) /* << 2) + 0xd0100000) */
#define DET3D_RAMRD_DATA_PORT            ((0x179b)) /* << 2) + 0xd0100000) */
#define NR2_CFR_PARA_CFG0                ((0x179c)) /* << 2) + 0xd0100000) */
/* Bit 8,	reg_CFR_CurDif_luma_mode	Current Field Top/Bot line Luma
 * difference calculation mode
 */
/* Bit 7:6,	reg_MACFR_frm_phase	        U2  This will be a field based
 * phase register that need to be set by FW phase to phase:
 */
/* this will be calculated based on dbdr_phase of the
 * specific line of this frame.
 */
/* u1: dbdr_phase=1, center line is DB in current line;  dbdr_phase=2, center
 * line is Dr in current line;
 */
/* Bit 5:4,	reg_CFR_CurDif_tran_mode	U2  Current Field Top/Bot line
 * Luma/Chroma transition level calculation mode,
 */
/* Bit 3:2,	reg_CFR_alpha_mode	        U2  Alpha selection mode for
 * CFR block from curAlp and motAlp i.e.
 *		0: motAlp; 1: (motAlp+curAlp)/2;
 *		2: min(motAlp,curAlp); 3: max(motAlp,curAlp);
 */
/* Bit 1:0,	reg_CFR_Motion_Luma_mode	U2  LumaMotion Calculation
 * mode for MA-CFR.
 *		0: top/bot Lumma motion;
 *		1: middle Luma Motion
 *		2: top/bot + middle motion;
 *		3: max(top/tot motion, middle motion)
 */
#define NR2_CFR_PARA_CFG1                ((0x179d)) /* << 2) + 0xd0100000) */
/* Bit 23:16,	reg_CFR_alpha_gain	    gain to map muxed curAlp and motAlp
 * to alpha that will be used for final blending.
 */
/* Bit 15: 8,	reg_CFR_Motion_ofst	    Offset to Motion to calculate the
 * motAlp, e,g:motAlp= reg_CFR_Motion_ofst- Motion;This register can be seen as
 * the level of motion that we consider it at moving.
 */
/* Bit  7: 0,	reg_CFR_CurDif_gain
 * gain to CurDif to map to alpha, normalized to 32;
 */
/* // DET 3D REG DEFINE END //// */
#define DI_NR_1_CTRL0                    ((0x1794)) /* << 2) + 0xd0100000) */
#define DI_NR_1_CTRL1                    ((0x1795)) /* << 2) + 0xd0100000) */
#define DI_NR_1_CTRL2                    ((0x1796)) /* << 2) + 0xd0100000) */
#define DI_NR_1_CTRL3                    ((0x1797)) /* << 2) + 0xd0100000) */
#define DI_CONTWR_X                      ((0x17a0)) /* << 2) + 0xd0100000) */
#define DI_CONTWR_Y                      ((0x17a1)) /* << 2) + 0xd0100000) */
#define DI_CONTWR_CTRL                   ((0x17a2)) /* << 2) + 0xd0100000) */
#define DI_CONTPRD_X                     ((0x17a3)) /* << 2) + 0xd0100000) */
#define DI_CONTPRD_Y                     ((0x17a4)) /* << 2) + 0xd0100000) */
#define DI_CONTP2RD_X                    ((0x17a5)) /* << 2) + 0xd0100000) */
#define DI_CONTP2RD_Y                    ((0x17a6)) /* << 2) + 0xd0100000) */
#define DI_CONTRD_CTRL                   ((0x17a7)) /* << 2) + 0xd0100000) */
#define DI_NRWR_X                        ((0x17c0)) /* << 2) + 0xd0100000) */
#define DI_NRWR_Y                        ((0x17c1)) /* << 2) + 0xd0100000) */
/* bit 31:30				nrwr_words_lim */
/* bit 29				nrwr_rev_y */
/* bit 28:16				nrwr_start_y */
/* bit 15				nrwr_ext_en */
/* bit 14		Nrwr bit10 mode */
/* bit 12:0				nrwr_end_y */
#define DI_NRWR_CTRL                     ((0x17c2)) /* << 2) + 0xd0100000) */
/* bit 31				pending_ddr_wrrsp_diwr */
/* bit 30				nrwr_reg_swap */
/* bit 29:26				nrwr_burst_lim */
/* bit 25				nrwr_canvas_syncen */
/* bit 24				nrwr_no_clk_gate */
/* bit 23:22				nrwr_rgb_mode
 *					0:422 to one canvas;
 *					1:4:4:4 to one canvas;
 */
/* bit 21:20				nrwr_hconv_mode */
/* bit 19:18				nrwr_vconv_mode */
/* bit 17				nrwr_swap_cbcr */
/* bit 16				nrwr_urgent */
/* bit 15:8				nrwr_canvas_index_chroma */
/* bit 7:0				nrwr_canvas_index_luma */
#define DI_MTNWR_X                       ((0x17c3)) /* << 2) + 0xd0100000) */
#define DI_MTNWR_Y                       ((0x17c4)) /* << 2) + 0xd0100000) */
#define DI_MTNWR_CTRL                    ((0x17c5)) /* << 2) + 0xd0100000) */
#define DI_DIWR_X                        ((0x17c6)) /* << 2) + 0xd0100000) */
#define DI_DIWR_Y                        ((0x17c7)) /* << 2) + 0xd0100000) */
/* bit 31:30				diwr_words_lim */
/* bit 29				diwr_rev_y */
/* bit 28:16				diwr_start_y */
/* bit 15				diwr_ext_en */
/* bit 12:0				diwr_end_y */
#define DI_DIWR_CTRL                     ((0x17c8)) /* << 2) + 0xd0100000) */
/* bit 31				pending_ddr_wrrsp_diwr */
/* bit 30				diwr_reg_swap */
/* bit 29:26				diwr_burst_lim */
/* bit 25				diwr_canvas_syncen */
/* bit 24				diwr_no_clk_gate */
/* bit 23:22				diwr_rgb_mode  0:422 to one canvas;
 * 1:4:4:4 to one canvas;
 */
/* bit 21:20				diwr_hconv_mode */
/* bit 19:18				diwr_vconv_mode */
/* bit 17				diwr_swap_cbcr */
/* bit 16				diwr_urgent */
/* bit 15:8				diwr_canvas_index_chroma */
/* bit 7:0				diwr_canvas_index_luma */
/* `define DI_MTNCRD_X               8'hc9 */
/* `define DI_MTNCRD_Y               8'hca */
#define DI_MTNPRD_X                      ((0x17cb)) /* << 2) + 0xd0100000) */
#define DI_MTNPRD_Y                      ((0x17cc)) /* << 2) + 0xd0100000) */
#define DI_MTNRD_CTRL                    ((0x17cd)) /* << 2) + 0xd0100000) */
#define DI_INP_GEN_REG                   ((0x17ce)) /* << 2) + 0xd0100000) */
#define DI_INP_CANVAS0                   ((0x17cf)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_X0                   ((0x17d0)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_Y0                   ((0x17d1)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA_X0                 ((0x17d2)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA_Y0                 ((0x17d3)) /* << 2) + 0xd0100000) */
#define DI_INP_RPT_LOOP                  ((0x17d4)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA0_RPT_PAT             ((0x17d5)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA0_RPT_PAT           ((0x17d6)) /* << 2) + 0xd0100000) */
#define DI_INP_DUMMY_PIXEL               ((0x17d7)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_FIFO_SIZE            ((0x17d8)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_Y               ((0x17ba)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_CB              ((0x17bb)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_CR              ((0x17bc)) /* << 2) + 0xd0100000) */
#define DI_INP_GEN_REG2                  ((0x1791)) /* << 2) + 0xd0100000) */
#define DI_INP_FMT_CTRL                  ((0x17d9)) /* << 2) + 0xd0100000) */
#define DI_INP_FMT_W                     ((0x17da)) /* << 2) + 0xd0100000) */
#define DI_MEM_GEN_REG                   ((0x17db)) /* << 2) + 0xd0100000) */
#define DI_MEM_CANVAS0                   ((0x17dc)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_X0                   ((0x17dd)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_Y0                   ((0x17de)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA_X0                 ((0x17df)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA_Y0                 ((0x17e0)) /* << 2) + 0xd0100000) */
#define DI_MEM_RPT_LOOP                  ((0x17e1)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA0_RPT_PAT             ((0x17e2)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA0_RPT_PAT           ((0x17e3)) /* << 2) + 0xd0100000) */
#define DI_MEM_DUMMY_PIXEL               ((0x17e4)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_FIFO_SIZE            ((0x17e5)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_Y               ((0x17bd)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_CB              ((0x17be)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_CR              ((0x17bf)) /* << 2) + 0xd0100000) */
#define DI_MEM_GEN_REG2                  ((0x1792)) /* << 2) + 0xd0100000) */
#define DI_MEM_FMT_CTRL                  ((0x17e6)) /* << 2) + 0xd0100000) */
#define DI_MEM_FMT_W                     ((0x17e7)) /* << 2) + 0xd0100000) */
/* #define DI_IF1_GEN_REG                   ((0x17e8)) + 0xd0100000) */
#define DI_IF1_CANVAS0                   ((0x17e9)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_X0                   ((0x17ea)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_Y0                   ((0x17eb)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA_X0                 ((0x17ec)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA_Y0                 ((0x17ed)) /* << 2) + 0xd0100000) */
#define DI_IF1_RPT_LOOP                  ((0x17ee)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA0_RPT_PAT             ((0x17ef)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA0_RPT_PAT           ((0x17f0)) /* << 2) + 0xd0100000) */
#define DI_IF1_DUMMY_PIXEL               ((0x17f1)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_FIFO_SIZE            ((0x17f2)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_Y               ((0x17fc)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_CB              ((0x17fd)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_CR              ((0x17fe)) /* << 2) + 0xd0100000) */
#define DI_IF1_GEN_REG2                  ((0x1790)) /* << 2) + 0xd0100000) */
#define DI_IF1_FMT_CTRL                  ((0x17f3)) /* << 2) + 0xd0100000) */
#define DI_IF1_FMT_W                     ((0x17f4)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_GEN_REG                 ((0x17f5)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CANVAS0                 ((0x17f6)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_X0                 ((0x17f7)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_Y0                 ((0x17f8)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA_X0               ((0x17f9)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA_Y0               ((0x17fa)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RPT_LOOP                ((0x17fb)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA0_RPT_PAT           ((0x17b0)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA0_RPT_PAT         ((0x17b1)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_DUMMY_PIXEL             ((0x17b2)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_FIFO_SIZE          ((0x17b3)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_Y             ((0x17b4)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_CB            ((0x17b5)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_CR            ((0x17b6)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_GEN_REG2                ((0x17b7)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_FMT_CTRL                ((0x17b8)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_FMT_W                   ((0x17b9)) /* << 2) + 0xd0100000) */
#define DI_CANVAS_URGENT0                ((0x170a)) /* << 2) + 0xd0100000) */
#define DI_CANVAS_URGENT1                ((0x170b)) /* << 2) + 0xd0100000) */
#define DI_MTN_CTRL                      ((0x170b)) /* << 2) + 0xd0100000) */
#define DI_CANVAS_URGENT2                ((0x170e)) /* << 2) + 0xd0100000) */

#define VD1_IF0_GEN_REG				    0x1a50
/* ((0x1a50  << 2) + 0xd0100000) */
#define VD1_IF0_LUMA_FIFO_SIZE          0x1a63
/* ((0x1a63  << 2) + 0xd0100000) */

#define VIU_VD1_FMT_CTRL			    0x1a68
/* ((0x1a68  << 2) + 0xd0100000) */
/* Bit 31    it true, disable clock, otherwise enable clock        */
/* Bit 30    soft rst bit */
/* Bit 28    if true, horizontal formatter use repeating to */
/* generete pixel, otherwise use bilinear interpolation */
/* Bit 27:24 horizontal formatter initial phase */
/* Bit 23    horizontal formatter repeat pixel 0 enable */
/* Bit 22:21 horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1 */
/* Bit 20    horizontal formatter enable */
/* Bit 19    if true, always use phase0 while vertical formater, */
/* meaning always repeat data, no interpolation */
/* Bit 18    if true, disable vertical formatter chroma repeat last line */
/* Bit 17    veritcal formatter dont need repeat */
/* line on phase0, 1: enable, 0: disable */
/* Bit 16    veritcal formatter repeat line 0 enable */
/* Bit 15:12 vertical formatter skip line num at the beginning */
/* Bit 11:8  vertical formatter initial phase */
/* Bit 7:1   vertical formatter phase step (3.4) */
/* Bit 0     vertical formatter enable */
#define VIU_VD1_FMT_W				    0x1a69
/* ((0x1a69  << 2) + 0xd0100000) */
/* Bit 27:16  horizontal formatter width */
/* Bit 11:0   vertical formatter width */


#define VD1_IF0_GEN_REG2		0x1a6d

#define DI_INP_GEN_REG3                 0x20a8
		/* 0xd01082a0 */
/*bit9:8	bit mode: 0 = 8bits, 1=10bits 422,  2 = 10bits 444 */
#define DI_MEM_GEN_REG3                 0x20a9
		/* 0xd01082a4 */
/*bit9:8	bit mode: 0 = 8bits, 1=10bits 422,  2 = 10bits 444 */
#define DI_CHAN2_GEN_REG3               0x20aa
		/* 0xd01082a8 */
/* dnr  Base Addr: 0xd0100000 */
#define DNR_CTRL                         ((0x2d00))
/* Bit 31:17,        reserved */
/* Bit 16,            reg_dnr_en,
 * dnr enable                  . unsigned  , default = 1
 */
/* Bit 15,            reg_dnr_db_vdbstep                          ,
 * vdb step, 0: 4, 1: 8        . unsigned  , default = 1
 */
/* Bit 14,            reg_dnr_db_vdbprten                         ,
 * vdb protectoin enable       . unsigned  , default = 1
 */
/* Bit 13,            reg_dnr_gbs_difen                           ,
 * enable dif (between LR and LL/RR) condition for gbs stat..
 * unsigned  , default = 0
 */
/* Bit 12,            reg_dnr_luma_en                             ,
 * enable ycbcr2luma module    . unsigned  , default = 1
 */
/* Bit 11:10,        reg_dnr_db_mod                              ,
 * deblocking mode, 0: disable, 1: horizontal deblocking, 2: vertical
 * deblocking, 3: horizontal & vertical deblocking. unsigned  , default = 3
 */
/* Bit  9,            reg_dnr_db_chrmen                           ,
 * enable chroma deblocking    . unsigned  , default = 1
 */
/* Bit  8,            reg_dnr_hvdif_mod                           ,
 * 0: calc. difs by original Y, 1: by new luma. unsigned  , default = 1
 */
/* Bit  7,            reserved */
/* Bit  6: 4,        reg_dnr_demo_lften                          ,
 * b0: Y b1:U b2:V             . unsigned  , default = 7
 */
/* Bit  3,            reserved */
/* Bit  2: 0,        reg_dnr_demo_rgten                          ,
 * b0: Y b1:U b2:V             . unsigned  , default = 7
 */
#define DNR_HVSIZE                       ((0x2d01))
/* Bit 31:29,        reserved */
/* Bit 28:16,        reg_dnr_hsize                               ,
 * hsize                       . unsigned  , default = 0
 */
/* Bit 15:13,        reserved */
/* Bit 12: 0,        reg_dnr_vsize                               ,
 * vsize                       . unsigned  , default = 0
 */
#define DNR_DBLK_BLANK_NUM                         ((0x2d02))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_dblk_hblank_num                         ,
 * deblock hor blank num       . unsigned  , default = 16
 */
/* Bit  7: 0,        reg_dblk_vblank_num                         ,
 * deblock ver blank num       . unsigned  , default = 45
 */
#define DNR_BLK_OFFST                              ((0x2d03))
/* Bit 31: 7,        reserved */
/* Bit  6: 4,        reg_dnr_hbofst                              ,
 * horizontal block offset may provide by software calc.. unsigned  ,
 * default = 0
 */
/* Bit  3,            reserved */
/* Bit  2: 0,        reg_dnr_vbofst                              ,
 * vertical block offset may provide by software calc.. unsigned  , default = 0
 */
#define DNR_GBS                                    ((0x2d04))
/* Bit 31: 2,        reserved */
/* Bit  1: 0,        reg_dnr_gbs                                 ,
 * global block strength may update by software calc.. unsigned  , default = 0
 */
#define DNR_HBOFFST_STAT                           ((0x2d05))
/* Bit 31:24,        reg_dnr_hbof_difthd                         ,
 * dif threshold (>=) between LR and LL/RR. unsigned  , default = 2
 */
/* Bit 23:16,        reg_dnr_hbof_edgethd                        ,
 * edge threshold (<=) for LR  . unsigned  , default = 32
 */
/* Bit 15: 8,        reg_dnr_hbof_flatthd                        ,
 * flat threshold (>=) for LR  . unsigned  , default = 0
 */
/* Bit  7,            reserved */
/* Bit  6: 4,        reg_dnr_hbof_delta                          ,
 * delta for weighted bin accumulator. unsigned  , default = 1
 */
/* Bit  3,            reserved */
/* Bit  2: 0,        reg_dnr_hbof_statmod                        ,
 * statistic mode for horizontal block offset, 0: count flags for 8-bin,
 * 1: count LRs for 8-bin, 2: count difs for 8-bin,
 * 3: count weighted flags for 8-bin, 4: count flags for first 32-bin,
 * 5: count LRs for first 32-bin, 6 or 7: count difs for first 32-bin.
 * unsigned  , default = 2
 */
#define DNR_VBOFFST_STAT                           ((0x2d06))
/* Bit 31:24,        reg_dnr_vbof_difthd                         ,
 * dif threshold (>=) between Up and Dw. unsigned  , default = 1
 */
/* Bit 23:16,        reg_dnr_vbof_edgethd                        ,
 * edge threshold (<=) for Up/Dw. unsigned  , default = 16
 */
/* Bit 15: 8,        reg_dnr_vbof_flatthd                        ,
 * flat threshold (>=) for Up/Dw. unsigned  , default = 0
 */
/* Bit  7,            reserved */
/* Bit  6: 4,        reg_dnr_vbof_delta                          ,
 * delta for weighted bin accumulator. unsigned  , default = 1
 */
/* Bit  3,            reserved */
/* Bit  2: 0,        reg_dnr_vbof_statmod                        ,
 * statistic mode for vertical block offset, 0: count flags for 8-bin,
 * 1: count Ups for 8-bin, 2: count difs for 8-bin, 3: count weighted
 * flags for 8-bin, 4: count flags for first 32-bin, 5: count Ups for
 * first 32-bin, 6 or 7: count difs for first 32-bin. unsigned  , default = 2
 */
#define DNR_GBS_STAT                               ((0x2d07))
/* Bit 31:24,        reg_dnr_gbs_edgethd                         ,
 * edge threshold (<=) for LR  . unsigned  , default = 32
 */
/* Bit 23:16,        reg_dnr_gbs_flatthd                         ,
 * flat threshold (>=) for LR  . unsigned  , default = 0
 */
/* Bit 15: 8,        reg_dnr_gbs_varthd                          ,
 * variation threshold (<=) for Lvar/Rvar. unsigned  , default = 16
 */
/* Bit  7: 0,        reg_dnr_gbs_difthd                          ,
 * dif threshold (>=) between LR and LL/RR. unsigned  , default = 2
 */
#define DNR_STAT_X_START_END                       ((0x2d08))
/* Bit 31:30,        reserved */
/* Bit 29:16,        reg_dnr_stat_xst    . unsigned  , default = 24 */
/* Bit 15:14,        reserved */
/* Bit 13: 0,        reg_dnr_stat_xed    . unsigned  , default = HSIZE - 25 */
#define DNR_STAT_Y_START_END                       ((0x2d09))
/* Bit 31:30,        reserved */
/* Bit 29:16,        reg_dnr_stat_yst    . unsigned  , default = 24 */
/* Bit 15:14,        reserved */
/* Bit 13: 0,        reg_dnr_stat_yed    . unsigned  , default = VSIZE - 25 */
#define DNR_LUMA                                   ((0x2d0a))
/* Bit 31:27,        reserved */
/* Bit 26:24,        reg_dnr_luma_sqrtshft                       ,
 * left shift for fast squart of chroma, [0, 4]. unsigned  , default = 2
 */
/* Bit 23:21,        reserved */
/* Bit 20:16,        reg_dnr_luma_sqrtoffst                      ,
 * offset for fast squart of chroma. signed    , default = 0
 */
/* Bit 15,            reserved */
/* Bit 14:12,        reg_dnr_luma_wcmod                          ,
 * theta related to warm/cool segment line, 0: 0, 1: 45, 2: 90, 3: 135,
 * 4: 180, 5: 225, 6: 270, 7: 315. . unsigned  , default = 3
 */
/* Bit 11: 8,        reg_dnr_luma_cshft                          ,
 * shift for calc. delta part, 0~8,  . unsigned  , default = 8
 */
/* Bit  7: 6,        reserved */
/* Bit  5: 0,        reg_dnr_luma_cgain                          ,
 * final gain for delta part, 32 normalized to "1". unsigned  , default = 4
 */
#define DNR_DB_YEDGE_THD                           ((0x2d0b))
/* Bit 31:24,        reg_dnr_db_yedgethd0                        ,
 * edge threshold0 for luma    . unsigned  , default = 12
 */
/* Bit 23:16,        reg_dnr_db_yedgethd1                        ,
 * edge threshold1 for luma    . unsigned  , default = 15
 */
/* Bit 15: 8,        reg_dnr_db_yedgethd2                        ,
 * edge threshold2 for luma    . unsigned  , default = 18
 */
/* Bit  7: 0,        reg_dnr_db_yedgethd3                        ,
 * edge threshold3 for luma    . unsigned  , default = 25
 */
#define DNR_DB_CEDGE_THD                           ((0x2d0c))
/* Bit 31:24,        reg_dnr_db_cedgethd0                        ,
 * edge threshold0 for chroma  . unsigned  , default = 12
 */
/* Bit 23:16,        reg_dnr_db_cedgethd1                        ,
 * edge threshold1 for chroma  . unsigned  , default = 15
 */
/* Bit 15: 8,        reg_dnr_db_cedgethd2                        ,
 * edge threshold2 for chroma  . unsigned  , default = 18
 */
/* Bit  7: 0,        reg_dnr_db_cedgethd3                        ,
 * edge threshold3 for chroma  . unsigned  , default = 25
 */
#define DNR_DB_HGAP                                ((0x2d0d))
/* Bit 31:24,        reserved */
/* Bit 23:16,        reg_dnr_db_hgapthd                          ,
 * horizontal gap thd (<=) for very sure blockiness . unsigned  , default = 8
 */
/* Bit 15: 8,        reg_dnr_db_hgapdifthd                       ,
 * dif thd between hgap and lft/rgt hdifs. unsigned  , default = 1
 */
/* Bit  7: 1,        reserved */
/* Bit  0,            reg_dnr_db_hgapmod                          ,
 * horizontal gap calc. mode, 0: just use current col x,
 * 1: find max between (x-1, x, x+1) . unsigned  , default = 0
 */
#define DNR_DB_HBS                                 ((0x2d0e))
/* Bit 31: 6,        reserved */
/* Bit  5: 4,        reg_dnr_db_hbsup                            ,
 * horizontal bs up value      . unsigned  , default = 1
 */
/* Bit  3: 2,        reg_dnr_db_hbsmax                           ,
 * max value of hbs for global control. unsigned  , default = 3
 */
/* Bit  1: 0,        reg_dnr_db_hgbsthd                          ,
 * gbs thd (>=) for hbs calc.  . unsigned  , default = 1
 */
#define DNR_DB_HACT                                ((0x2d0f))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_dnr_db_hactthd0                         ,
 * thd0 of hact, for block classification. unsigned  , default = 10
 */
/* Bit  7: 0,        reg_dnr_db_hactthd1                         ,
 * thd1 of hact, for block classification. unsigned  , default = 32
 */
#define DNR_DB_YHDELTA_GAIN                        ((0x2d10))
/* Bit 31:27,        reserved */
/* Bit 26:24,        reg_dnr_db_yhdeltagain1                     ,
 * (p1-q1) gain for Y's delta calc. when bs=1, normalized 8 as "1" .
 * unsigned  , default = 2
 */
/* Bit 23,            reserved */
/* Bit 22:20,        reg_dnr_db_yhdeltagain2                     ,
 * (p1-q1) gain for Y's delta calc. when bs=2, normalized 8 as "1" .
 * unsigned  , default = 0
 */
/* Bit 19,            reserved */
/* Bit 18:16,        reg_dnr_db_yhdeltagain3                     ,
 * (p1-q1) gain for Y's delta calc. when bs=3, normalized 8 as "1" .
 * unsigned  , default = 0
 */
/* Bit 15,            reserved */
/* Bit 14: 8,        reg_dnr_db_yhdeltaadjoffst                  ,
 * offset for adjust Y's hdelta (-64, 63). signed    , default = 0
 */
/* Bit  7: 6,        reserved */
/* Bit  5: 0,        reg_dnr_db_yhdeltaadjgain                   ,
 * gain for adjust Y's hdelta, normalized 32 as "1" . unsigned  , default = 32
 */
#define DNR_DB_YHDELTA2_GAIN                       ((0x2d11))
/* Bit 31:30,        reserved */
/* Bit 29:24,        reg_dnr_db_yhdelta2gain2                    ,
 * gain for bs=2's adjust Y's hdelta2, normalized 64 as "1" .
 * unsigned  , default = 8
 */
/* Bit 23:21,        reserved */
/* Bit 20:16,        reg_dnr_db_yhdelta2offst2                   ,
 * offset for bs=2's adjust Y's hdelta2 (-16, 15). signed    , default = 0
 */
/* Bit 15:14,        reserved */
/* Bit 13: 8,        reg_dnr_db_yhdelta2gain3                    ,
 * gain for bs=3's adjust Y's hdelta2, normalized 64 as "1" .
 * unsigned , default = 4
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_db_yhdelta2offst3                   ,
 * offset for bs=3's adjust Y's hdelta2 (-16, 15). signed    , default = 0
 */
#define DNR_DB_CHDELTA_GAIN                        ((0x2d12))
/* Bit 31:27,        reserved */
/* Bit 26:24,        reg_dnr_db_chdeltagain1                     ,
 * (p1-q1) gain for UV's delta calc. when bs=1, normalized 8 as "1".
 * unsigned  , default = 2
 */
/* Bit 23,            reserved */
/* Bit 22:20,        reg_dnr_db_chdeltagain2                     ,
 * (p1-q1) gain for UV's delta calc. when bs=2,
 * normalized 8 as "1". unsigned  , default = 0
 */
/* Bit 19,            reserved */
/* Bit 18:16,        reg_dnr_db_chdeltagain3                     ,
 * (p1-q1) gain for UV's delta calc. when bs=3, normalized 8 as "1".
 * unsigned  , default = 0
 */
/* Bit 15,            reserved */
/* Bit 14: 8,        reg_dnr_db_chdeltaadjoffst                  ,
 * offset for adjust UV's hdelta (-64, 63). signed    , default = 0
 */
/* Bit  7: 6,        reserved */
/* Bit  5: 0,        reg_dnr_db_chdeltaadjgain                   ,
 * gain for adjust UV's hdelta, normalized 32 as "1". unsigned  , default = 32
 */
#define DNR_DB_CHDELTA2_GAIN                       ((0x2d13))
/* Bit 31:30,        reserved */
/* Bit 29:24,        reg_dnr_db_chdelta2gain2                    ,
 * gain for bs=2's adjust UV's hdelta2, normalized 64 as "1" .
 * unsigned  , default = 8
 */
/* Bit 23:21,        reserved */
/* Bit 20:16,        reg_dnr_db_chdelta2offst2                   ,
 * offset for bs=2's adjust UV's hdelta2 (-16, 15). signed    , default = 0
 */
/* Bit 15:14,        reserved */
/* Bit 13: 8,        reg_dnr_db_chdelta2gain3                    ,
 * gain for bs=2's adjust UV's hdelta2, normalized 64 as "1" .
 * unsigned , default = 4
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_db_chdelta2offst3                   ,
 * offset for bs=2's adjust UV's hdelta2 (-16, 15). signed    , default = 0
 */
#define DNR_DB_YC_VEDGE_THD                        ((0x2d14))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_dnr_db_yvedgethd                        ,
 * special Y's edge thd for vdb. unsigned  , default = 12
 */
/* Bit  7: 0,        reg_dnr_db_cvedgethd                        ,
 * special UV's edge thd for vdb. unsigned  , default = 12
 */
#define DNR_DB_VBS_MISC                            ((0x2d15))
/* Bit 31:24,        reg_dnr_db_vgapthd                          ,
 * vertical gap thd (<=) for very sure blockiness . unsigned  , default = 8
 */
/* Bit 23:16,        reg_dnr_db_vactthd                          ,
 * thd of vact, for block classification . unsigned  , default = 10
 */
/* Bit 15: 8,        reg_dnr_db_vgapdifthd                       ,
 * dif thd between vgap and vact. unsigned  , default = 4
 */
/* Bit  7: 4,        reserved */
/* Bit  3: 2,        reg_dnr_db_vbsmax                           ,
 * max value of vbs for global control. unsigned  , default = 2
 */
/* Bit  1: 0,        reg_dnr_db_vgbsthd                          ,
 * gbs thd (>=) for vbs calc.  . unsigned  , default = 1
 */
#define DNR_DB_YVDELTA_GAIN                        ((0x2d16))
/* Bit 31:30,        reserved */
/* Bit 29:24,        reg_dnr_db_yvdeltaadjgain                   ,
 * gain for adjust Y's vdelta, normalized 32 as "1". unsigned  , default = 32
 */
/* Bit 23,            reserved */
/* Bit 22:16,        reg_dnr_db_yvdeltaadjoffst                  ,
 * offset for adjust Y's vdelta (-64, 63). signed    , default = 0
 */
/* Bit 15:14,        reserved */
/* Bit 13: 8,        reg_dnr_db_yvdelta2gain                     ,
 * gain for adjust Y's vdelta2, normalized 64 as "1". unsigned  , default = 8
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_db_yvdelta2offst                    ,
 * offset for adjust Y's vdelta2 (-16, 15). signed    , default = 0
 */
#define DNR_DB_CVDELTA_GAIN                        ((0x2d17))
/* Bit 31:30,        reserved */
/* Bit 29:24,        reg_dnr_db_cvdeltaadjgain                   ,
 * gain for adjust UV's vdelta, normalized 32 as "1". unsigned  , default = 32
 */
/* Bit 23,            reserved */
/* Bit 22:16,        reg_dnr_db_cvdeltaadjoffst                  ,
 * offset for adjust UV's vdelta (-64, 63). signed    , default = 0
 */
/* Bit 15:14,        reserved */
/* Bit 13: 8,        reg_dnr_db_cvdelta2gain                     ,
 * gain for adjust UV's vdelta2, normalized 64 as "1". unsigned  , default = 8
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_db_cvdelta2offst                    ,
 * offset for adjust UV's vdelta2 (-16, 15). signed    , default = 0
 */
#define DNR_RO_GBS_STAT_LR                         ((0x2d18))
/* Bit 31: 0,        ro_gbs_stat_lr
 * . unsigned  , default = 0
 */
#define DNR_RO_GBS_STAT_LL                         ((0x2d19))
/* Bit 31: 0,        ro_gbs_stat_ll
 * . unsigned  , default = 0
 */
#define DNR_RO_GBS_STAT_RR                         ((0x2d1a))
/* Bit 31: 0,        ro_gbs_stat_rr
 * . unsigned  , default = 0
 */
#define DNR_RO_GBS_STAT_DIF                        ((0x2d1b))
/* Bit 31: 0,        ro_gbs_stat_dif
 * . unsigned  , default = 0
 */
#define DNR_RO_GBS_STAT_CNT                        ((0x2d1c))
/* Bit 31: 0,        ro_gbs_stat_cnt
 * . unsigned  , default = 0
 */
#define DNR_RO_HBOF_STAT_CNT_0                     ((0x2d1d))
/* Bit 31: 0,        ro_hbof_stat_cnt0
 * . unsigned  , default = 0
 */
#define DNR_RO_HBOF_STAT_CNT_1                     ((0x2d1e))
/* Bit 31: 0,        ro_hbof_stat_cnt1
 * . unsigned  , default = 0
 */
#define DNR_RO_HBOF_STAT_CNT_2                     ((0x2d1f))
/* Bit 31: 0,        ro_hbof_stat_cnt2
 * . unsigned  , default = 0
 */
#define DNR_RO_HBOF_STAT_CNT_3                     ((0x2d20))
/* Bit 31: 0,        ro_hbof_stat_cnt3  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_4                     ((0x2d21))
/* Bit 31: 0,        ro_hbof_stat_cnt4  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_5                     ((0x2d22))
/* Bit 31: 0,        ro_hbof_stat_cnt5  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_6                     ((0x2d23))
/* Bit 31: 0,        ro_hbof_stat_cnt6  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_7                     ((0x2d24))
/* Bit 31: 0,        ro_hbof_stat_cnt7  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_8                     ((0x2d25))
/* Bit 31: 0,        ro_hbof_stat_cnt8  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_9                     ((0x2d26))
/* Bit 31: 0,        ro_hbof_stat_cnt9  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_10                    ((0x2d27))
/* Bit 31: 0,        ro_hbof_stat_cnt10 . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_11                    ((0x2d28))
/* Bit 31: 0,        ro_hbof_stat_cnt11 . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_12                    ((0x2d29))
/* Bit 31: 0,        ro_hbof_stat_cnt12 . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_13                    ((0x2d2a))
/* Bit 31: 0,        ro_hbof_stat_cnt13 . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_14                    ((0x2d2b))
/* Bit 31: 0,        ro_hbof_stat_cnt14  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_15                    ((0x2d2c))
/* Bit 31: 0,        ro_hbof_stat_cnt15  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_16                    ((0x2d2d))
/* Bit 31: 0,        ro_hbof_stat_cnt16  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_17                    ((0x2d2e))
/* Bit 31: 0,        ro_hbof_stat_cnt17  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_18                    ((0x2d2f))
/* Bit 31: 0,        ro_hbof_stat_cnt18  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_19                    ((0x2d30))
/* Bit 31: 0,        ro_hbof_stat_cnt19  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_20                    ((0x2d31))
/* Bit 31: 0,        ro_hbof_stat_cnt20  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_21                    ((0x2d32))
/* Bit 31: 0,        ro_hbof_stat_cnt21  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_22                    ((0x2d33))
/* Bit 31: 0,        ro_hbof_stat_cnt22  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_23                    ((0x2d34))
/* Bit 31: 0,        ro_hbof_stat_cnt23  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_24                    ((0x2d35))
/* Bit 31: 0,        ro_hbof_stat_cnt24  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_25                    ((0x2d36))
/* Bit 31: 0,        ro_hbof_stat_cnt25  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_26                    ((0x2d37))
/* Bit 31: 0,        ro_hbof_stat_cnt26  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_27                    ((0x2d38))
/* Bit 31: 0,        ro_hbof_stat_cnt27  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_28                    ((0x2d39))
/* Bit 31: 0,        ro_hbof_stat_cnt28  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_29                    ((0x2d3a))
/* Bit 31: 0,        ro_hbof_stat_cnt29  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_30                    ((0x2d3b))
/* Bit 31: 0,        ro_hbof_stat_cnt30  . unsigned  , default = 0 */
#define DNR_RO_HBOF_STAT_CNT_31                    ((0x2d3c))
/* Bit 31: 0,        ro_hbof_stat_cnt31  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_0                     ((0x2d3d))
/* Bit 31: 0,        ro_vbof_stat_cnt0  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_1                     ((0x2d3e))
/* Bit 31: 0,        ro_vbof_stat_cnt1  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_2                     ((0x2d3f))
/* Bit 31: 0,        ro_vbof_stat_cnt2  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_3                     ((0x2d40))
/* Bit 31: 0,        ro_vbof_stat_cnt3  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_4                     ((0x2d41))
/* Bit 31: 0,        ro_vbof_stat_cnt4  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_5                     ((0x2d42))
/* Bit 31: 0,        ro_vbof_stat_cnt5  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_6                     ((0x2d43))
/* Bit 31: 0,        ro_vbof_stat_cnt6  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_7                     ((0x2d44))
/* Bit 31: 0,        ro_vbof_stat_cnt7  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_8                     ((0x2d45))
/* Bit 31: 0,        ro_vbof_stat_cnt8  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_9                     ((0x2d46))
/* Bit 31: 0,        ro_vbof_stat_cnt9  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_10                    ((0x2d47))
/* Bit 31: 0,        ro_vbof_stat_cnt10  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_11                    ((0x2d48))
/* Bit 31: 0,        ro_vbof_stat_cnt11  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_12                    ((0x2d49))
/* Bit 31: 0,        ro_vbof_stat_cnt12  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_13                    ((0x2d4a))
/* Bit 31: 0,        ro_vbof_stat_cnt13  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_14                    ((0x2d4b))
/* Bit 31: 0,        ro_vbof_stat_cnt14  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_15                    ((0x2d4c))
/* Bit 31: 0,        ro_vbof_stat_cnt15  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_16                    ((0x2d4d))
/* Bit 31: 0,        ro_vbof_stat_cnt16  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_17                    ((0x2d4e))
/* Bit 31: 0,        ro_vbof_stat_cnt17  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_18                    ((0x2d4f))
/* Bit 31: 0,        ro_vbof_stat_cnt18  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_19                    ((0x2d50))
/* Bit 31: 0,        ro_vbof_stat_cnt19  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_20                    ((0x2d51))
/* Bit 31: 0,        ro_vbof_stat_cnt20  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_21                    ((0x2d52))
/* Bit 31: 0,        ro_vbof_stat_cnt21  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_22                    ((0x2d53))
/* Bit 31: 0,        ro_vbof_stat_cnt22  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_23                    ((0x2d54))
/* Bit 31: 0,        ro_vbof_stat_cnt23  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_24                    ((0x2d55))
/* Bit 31: 0,        ro_vbof_stat_cnt24  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_25                    ((0x2d56))
/* Bit 31: 0,        ro_vbof_stat_cnt25  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_26                    ((0x2d57))
/* Bit 31: 0,        ro_vbof_stat_cnt26  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_27                    ((0x2d58))
/* Bit 31: 0,        ro_vbof_stat_cnt27  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_28                    ((0x2d59))
/* Bit 31: 0,        ro_vbof_stat_cnt28  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_29                    ((0x2d5a))
/* Bit 31: 0,        ro_vbof_stat_cnt29  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_30                    ((0x2d5b))
/* Bit 31: 0,        ro_vbof_stat_cnt30  . unsigned  , default = 0 */
#define DNR_RO_VBOF_STAT_CNT_31                    ((0x2d5c))
/* Bit 31: 0,        ro_vbof_stat_cnt31  . unsigned  , default = 0 */
#define DNR_DM_CTRL                                ((0x2d60))
/* Bit 31:13,        reserved */
/* Bit 12,            reg_dnr_dm_fedgeflg_en                      ,
 * enable edge flag calc. of each frame. unsigned  , default = 1
 */
/* Bit 11,            reg_dnr_dm_fedgeflg_cl                      ,
 * clear frame edge flag if needed. unsigned  , default = 1
 */
/* Bit 10,            reg_dnr_dm_fedgeflg_df                      ,
 * user defined edge when reg_dnr_dm_fedgeflg_en=0, default = 1
 */
/* Bit  9,            reg_dnr_dm_en                               ,
 * enable demosquito function  . unsigned  , default = 1
 */
/* Bit  8,            reg_dnr_dm_chrmen                           ,
 * enable chrome processing for demosquito. unsigned  , default = 1
 */
/* Bit  7: 6,        reg_dnr_dm_level                            ,
 * demosquito level            . unsigned  , default = 3
 */
/* Bit  5: 4,        reg_dnr_dm_leveldw0                         ,
 * level down when gbs is small. unsigned  , default = 1
 */
/* Bit  3: 2,        reg_dnr_dm_leveldw1                         ,
 * level down for no edge/flat blocks. unsigned  , default = 1
 */
/* Bit  1: 0,        reg_dnr_dm_gbsthd                           ,
 * small/large threshold for gbs (<=). unsigned  , default = 0
 */
#define DNR_DM_NR_BLND                             ((0x2d61))
/* Bit 31:25,        reserved */
/* Bit 24,            reg_dnr_dm_defalpen                         ,
 * enable user define alpha for dm & nr blend. unsigned  , default = 0
 */
/* Bit 23:16,        reg_dnr_dm_defalp                           ,
 * user define alpha for dm & nr blend if enable. unsigned  , default = 0
 */
/* Bit 15:14,        reserved */
/* Bit 13: 8,        reg_dnr_dm_alpgain                          ,
 * gain for nr/dm alpha, normalized 32 as "1". unsigned  , default = 32
 */
/* Bit  7: 0,        reg_dnr_dm_alpoffst                         ,
 * (-128, 127), offset for nr/dm alpha. signed    , default = 0
 */
#define DNR_DM_RNG_THD                             ((0x2d62))
/* Bit 31:24,        reserved */
/* Bit 23:16,        reg_dnr_dm_rngminthd  . unsigned  , default = 2 */
/* Bit 15: 8,        reg_dnr_dm_rngmaxthd  . unsigned  , default = 64 */
/* Bit  7: 0,        reg_dnr_dm_rngdifthd  . unsigned  , default = 4 */
#define DNR_DM_RNG_GAIN_OFST                       ((0x2d63))
/* Bit 31:14,        reserved */
/* Bit 13: 8,        reg_dnr_dm_rnggain                          ,
 * normalized 16 as "1"        . unsigned  , default = 16
 */
/* Bit  7: 6,        reserved */
/* Bit  5: 0,        reg_dnr_dm_rngofst  . unsigned  , default = 0 */
#define DNR_DM_DIR_MISC                            ((0x2d64))
/* Bit 31:30,        reserved */
/* Bit 29,            reg_dnr_dm_diralpen     . unsigned  , default = 1 */
/* Bit 28:24,        reg_dnr_dm_diralpgain    . unsigned  , default = 0 */
/* Bit 23:22,        reserved */
/* Bit 21:16,        reg_dnr_dm_diralpofst    . unsigned  , default = 0 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_diralpmin     . unsigned  , default = 0 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_diralpmax     . unsigned  , default = 31 */
#define DNR_DM_COR_DIF                             ((0x2d65))
/* Bit 31: 4,        reserved */
/* Bit  3: 1,        reg_dnr_dm_cordifshft    . unsigned  , default = 3 */
/* Bit  0,            reg_dnr_dm_cordifmod                        ,
 * 0:use max dir dif as cordif, 1: use max3x3 - min3x3 as cordif.
 * unsigned  , default = 1
 */
#define DNR_DM_FLT_THD                             ((0x2d66))
/* Bit 31:24,        reg_dnr_dm_fltthd00                         ,
 * block flat threshold0 for block average difference when gbs is small,
 * for flat block detection. unsigned  , default = 4
 */
/* Bit 23:16,        reg_dnr_dm_fltthd01                         ,
 * block flat threshold1 for block average difference when gbs is small,
 * for flat block detection. unsigned  , default = 6
 */
/* Bit 15: 8,        reg_dnr_dm_fltthd10                         ,
 * block flat threshold0 for block average difference when gbs is large,
 * for flat block detection. unsigned  , default = 9
 */
/* Bit  7: 0,        reg_dnr_dm_fltthd11                         ,
 * block flat threshold1 for block average difference when gbs is large,
 * for flat block detection. unsigned  , default = 12
 */
#define DNR_DM_VAR_THD                             ((0x2d67))
/* Bit 31:24,        reg_dnr_dm_varthd00                         ,
 * block variance threshold0 (>=) when gbs is small, for flat block
 * detection. unsigned  , default = 2
 */
/* Bit 23:16,        reg_dnr_dm_varthd01                         ,
 * block variance threshold1 (<=) when gbs is small, for flat block
 * detection. unsigned  , default = 15
 */
/* Bit 15: 8,        reg_dnr_dm_varthd10                         ,
 * block variance threshold0 (>=) when gbs is large, for flat block
 * detection. unsigned  , default = 3
 */
/* Bit  7: 0,        reg_dnr_dm_varthd11                         ,
 * block variance threshold1 (<=) when gbs is large, for flat block
 * detection. unsigned  , default = 24
 */
#define DNR_DM_EDGE_DIF_THD                        ((0x2d68))
/* Bit 31:24,        reg_dnr_dm_edgethd0                         ,
 * block edge threshold (<=) when gbs is small, for flat block detection.
 * unsigned  , default = 32
 */
/* Bit 23:16,        reg_dnr_dm_edgethd1                         ,
 * block edge threshold (<=) when gbs is large, for flat block detection.
 * unsigned  , default = 48
 */
/* Bit 15: 8,        reg_dnr_dm_difthd0                          ,
 * block dif threshold (<=) when gbs is small, for flat block detection.
 * unsigned  , default = 48
 */
/* Bit  7: 0,        reg_dnr_dm_difthd1                          ,
 * block dif threshold (<=) when gbs is large, for flat block detection.
 * unsigned  , default = 64
 */
#define DNR_DM_AVG_THD                             ((0x2d69))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_dnr_dm_avgthd0                          ,
 * block average threshold (>=), for flat block detection.
 * unsigned ,default = 160
 */
/* Bit  7: 0,        reg_dnr_dm_avgthd1                          ,
 * block average threshold (<=), for flat block detection. unsigned
 * , default = 128
 */
#define DNR_DM_AVG_VAR_DIF_THD                     ((0x2d6a))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_dnr_dm_avgdifthd                        ,
 * block average dif threshold (<) between cur and up block, for flat
 * block detection. unsigned  , default = 12
 */
/* Bit  7: 0,        reg_dnr_dm_vardifthd                        ,
 * block variance dif threshold (>=) between cur and up block, for flat
 * block detection. unsigned  , default = 1
 */
#define DNR_DM_VAR_EDGE_DIF_THD2                   ((0x2d6b))
/* Bit 31:24,        reserved */
/* Bit 23:16,        reg_dnr_dm_varthd2, block variance threshold (>=),
 * for edge block detection.unsigned, default = 24
 */
/* Bit 15: 8,        reg_dnr_dm_edgethd2                         ,
 * block edge threshold (>=), for edge block detection. unsigned  , default = 40
 */
/* Bit  7: 0,        reg_dnr_dm_difthd2                          ,
 * block dif threshold (>=), for edge block detection. unsigned  , default = 80
 */
#define DNR_DM_DIF_FLT_MISC                        ((0x2d6c))
/* Bit 31:28,        reg_dnr_dm_ldifoob                          ,
 * pre-defined large dif when pixel out of blocks. unsigned  , default = 0
 */
/* Bit 27:24,        reg_dnr_dm_bdifoob                          ,
 * pre-defined block dif when pixel out of blocks;. unsigned  , default = 0
 */
/* Bit 23:16,        reg_dnr_dm_fltalp                           ,
 * pre-defined alpha for dm and nr blending, when block is flat
 * with mos.. unsigned  , default = 200
 */
/* Bit 15:12,        reserved */
/* Bit 11: 8,        reg_dnr_dm_fltminbdif                       ,
 * pre-defined min block dif for dm filter,
 * when block is flat with mos.. unsigned  , default = 12
 */
/* Bit  7,            reserved */
/* Bit  6: 2,        reg_dnr_dm_difnormgain                      ,
 * gain for pixel dif normalization for dm filter,
 * normalized 16 as "1". unsigned  , default = 16
 */
/* Bit  1,            reg_dnr_dm_difnormen                        ,
 * enable pixel dif normalization for dm filter. unsigned  , default = 1
 */
/* Bit  0,            reg_dnr_dm_difupden                         ,
 * enable block dif update using max of left, cur, right difs.
 * unsigned  , default = 0
 */
#define DNR_DM_SDIF_LUT0_2                         ((0x2d6d))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_sdiflut0                         ,
 * normally 0-16               . unsigned  , default = 16
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_sdiflut1                         ,
 * normally 0-16               . unsigned  , default = 14
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_sdiflut2                         ,
 * normally 0-16               . unsigned  , default = 13
 */
#define DNR_DM_SDIF_LUT3_5                         ((0x2d6e))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_sdiflut3                         ,
 * normally 0-16               . unsigned  , default = 10
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_sdiflut4                         ,
 * normally 0-16               . unsigned  , default = 7
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_sdiflut5                         ,
 * normally 0-16               . unsigned  , default = 5
 */
#define DNR_DM_SDIF_LUT6_8                         ((0x2d6f))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_sdiflut6                         ,
 * normally 0-16               . unsigned  , default = 3
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_sdiflut7                         ,
 * normally 0-16               . unsigned  , default = 1
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_sdiflut8                         ,
 * normally 0-16               . unsigned  , default = 0
 */
#define DNR_DM_LDIF_LUT0_2                         ((0x2d70))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_ldiflut0                         ,
 * normally 0-16               . unsigned  , default = 0
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_ldiflut1                         ,
 * normally 0-16               . unsigned  , default = 4
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_ldiflut2                         ,
 * normally 0-16               . unsigned  , default = 12
 */
#define DNR_DM_LDIF_LUT3_5                         ((0x2d71))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_ldiflut3                         ,
 * normally 0-16               . unsigned  , default = 14
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_ldiflut4                         ,
 * normally 0-16               . unsigned  , default = 15
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_ldiflut5                         ,
 * normally 0-16               . unsigned  , default = 16
 */
#define DNR_DM_LDIF_LUT6_8                         ((0x2d72))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_ldiflut6                         ,
 * normally 0-16               . unsigned  , default = 16
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_ldiflut7                         ,
 * normally 0-16               . unsigned  , default = 16
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_ldiflut8                         ,
 * normally 0-16               . unsigned  , default = 16
 */
#define DNR_DM_DIF2NORM_LUT0_2                     ((0x2d73))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_dif2normlut0                     ,
 * normally 0-16               . unsigned  , default = 16
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_dif2normlut1                     ,
 * normally 0-16               . unsigned  , default = 5
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_dif2normlut2                     ,
 * normally 0-16               . unsigned  , default = 3
 */
#define DNR_DM_DIF2NORM_LUT3_5                     ((0x2d74))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_dif2normlut3                     ,
 * normally 0-16               . unsigned  , default = 2
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_dif2normlut4                     ,
 * normally 0-16               . unsigned  , default = 2
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_dif2normlut5                     ,
 * normally 0-16               . unsigned  , default = 1
 */
#define DNR_DM_DIF2NORM_LUT6_8                     ((0x2d75))
/* Bit 31:21,        reserved */
/* Bit 20:16,        reg_dnr_dm_dif2normlut6                     ,
 * normally 0-16               . unsigned  , default = 1
 */
/* Bit 15:13,        reserved */
/* Bit 12: 8,        reg_dnr_dm_dif2normlut7                     ,
 * normally 0-16               . unsigned  , default = 1
 */
/* Bit  7: 5,        reserved */
/* Bit  4: 0,        reg_dnr_dm_dif2normlut8                     ,
 * normally 0-16               . unsigned  , default = 1
 */
#define DNR_DM_GMS_THD                             ((0x2d76))
/* Bit 31:16,        reserved */
/* Bit 15: 8,        reg_gms_stat_thd0  . unsigned  , default = 0 */
/* Bit  7: 0,        reg_gms_stat_thd1  . unsigned  , default = 128 */
#define DNR_RO_DM_GMS_STAT_CNT                     ((0x2d77))
/* Bit 31: 0,        ro_dm_gms_stat_cnt  . unsigned  , default = 0 */
#define DNR_RO_DM_GMS_STAT_MS                      ((0x2d78))
/* Bit 31: 0,        ro_dm_gms_stat_ms  . unsigned  , default = 0 */
/* txl added */
#define DECOMB_DET_VERT_CON0				(0x2d80)
#define DECOMB_DET_VERT_CON1				(0x2d81)
#define DECOMB_DET_EDGE_CON0				(0x2d82)
#define DECOMB_DET_EDGE_CON1				(0x2d83)
#define DECOMB_PARA							(0x2d84)
#define DECOMB_BLND_CON0					(0x2d85)
#define DECOMB_BLND_CON1					(0x2d86)
#define DECOMB_YC_THRD						(0x2d87)
#define DECOMB_MTN_GAIN_OFST				(0x2d88)
#define DECOMB_CMB_SEL_GAIN_OFST			(0x2d89)
#define DECOMB_WIND00						(0x2d8a)
#define DECOMB_WIND01						(0x2d8b)
#define DECOMB_WIND10						(0x2d8c)
#define DECOMB_WIND11						(0x2d8d)
#define DECOMB_MODE							(0x2d8e)
#define DECOMB_FRM_SIZE						(0x2d8f)
#define DECOMB_HV_BLANK						(0x2d90)
#define NR2_POLAR3_MODE						(0x2d98)
#define NR2_POLAR3_THRD						(0x2d99)
#define NR2_POLAR3_PARA0					(0x2d9a)
#define NR2_POLAR3_PARA1					(0x2d9b)
#define NR2_POLAR3_CTRL						(0x2d9c)
#define NR2_RO_POLAR3_NUMOFPIX				(0x2d9d)
#define NR2_RO_POLAR3_SMOOTHMV				(0x2d9e)
#define NR2_RO_POLAR3_M1					(0x2d9f)
#define NR2_RO_POLAR3_P1					(0x2da0)
#define NR2_RO_POLAR3_M2					(0x2da1)
#define NR2_RO_POLAR3_P2					(0x2da2)
#define NR2_RO_POLAR3_32					(0x2da3)
/* txl end */


#define VPU_VD1_MMC_CTRL					(0x2703)
#define VPU_VD2_MMC_CTRL					(0x2704)
#define VPU_DI_IF1_MMC_CTRL					(0x2705)
#define VPU_DI_MEM_MMC_CTRL					(0x2706)
#define VPU_DI_INP_MMC_CTRL					(0x2707)
#define VPU_DI_MTNRD_MMC_CTRL				(0x2708)
#define VPU_DI_CHAN2_MMC_CTRL				(0x2709)
#define VPU_DI_MTNWR_MMC_CTRL				(0x270a)
#define VPU_DI_NRWR_MMC_CTRL				(0x270b)
#define VPU_DI_DIWR_MMC_CTRL				(0x270c)

#define MCDI_PD_22_CHK_WND0_X				(0x2f59)
#define MCDI_PD_22_CHK_WND0_Y				(0x2f5a)
#define MCDI_PD_22_CHK_WND1_X				(0x2f5b)
#define MCDI_PD_22_CHK_WND1_Y				(0x2f5c)
#define MCDI_PD_22_CHK_FLG_CNT				(0x2f5e)
/* mc di */
/* //=================================================================//// */
/* // memc di core 0 */
/* //=================================================================//// */
#define MCDI_HV_SIZEIN                             ((0x2f00))
/* Bit 31:29, reserved */
/* Bit 28:16, reg_mcdi_hsize
 * image horizontal size (number of cols)   default=1024
 */
/* Bit 15:13, reserved */
/* Bit 12: 0, reg_mcdi_vsize
 * image vertical size   (number of rows)   default=1024
 */
#define MCDI_HV_BLKSIZEIN                          ((0x2f01))
/* Bit    31, reg_mcdi_vrev					 default = 0 */
/* Bit    30, reg_mcdi_hrev					 default = 0 */
/* Bit 29:28, reserved */
/* Bit 27:16, reg_mcdi_blkhsize
 * image horizontal blk size (number of cols)   default=1024
 */
/* Bit 15:13, reserved */
/* Bit 11: 0, reg_mcdi_blkvsize
 * image vertical blk size   (number of rows)   default=1024
 */
#define MCDI_BLKTOTAL                              ((0x2f02))
/* Bit 31:24, reserved */
/* Bit 23: 0, reg_mcdi_blktotal */
#define MCDI_MOTINEN                               ((0x2f03))
/* Bit 31: 2, reserved */
/* Bit     1, reg_mcdi_motionrefen.
 * enable motion refinement of MA, default = 1
 */
/* Bit     0, reg_mcdi_motionparadoxen.
 * enable motion paradox detection, default = 1
 */
#define MCDI_CTRL_MODE                             ((0x2f04))
/* Bit 31:28, reserved */
/* Bit 27:26, reg_mcdi_lmvlocken
 * 0:disable, 1: use max Lmv, 2: use no-zero Lmv,
 * lmv lock enable mode, default = 2
 */
/* Bit 25,    reg_mcdi_reldetrptchken */
/* 0: unable; 1: enable, enable repeat pattern
 * check (not repeat mv detection) in rel det part, default = 1
 */
/* Bit 24,    reg_mcdi_reldetgmvpd22chken */
/* 0: unable; 1: enable, enable pull-down 22 mode
 * check in gmv lock mode for rel det, default = 1
 */
/* Bit 23,    reg_mcdi_pd22chken */
/* 0: unable; 1: enable, enable pull-down 22
 * mode check (lock) function, default = 1
 */
/* Bit 22,    reg_mcdi_reldetlpfen */
/* 0: unable; 1: enable, enable det value lpf, default = 1 */
/* Bit 21,    reg_mcdi_reldetlmvpd22chken */
/* 0: unable; 1: enable, enable pull-down 22
 * mode check in lmv lock mode for rel det, default = 1
 */
/* Bit 20,    reg_mcdi_reldetlmvdifchken */
/* 0: unable; 1: enable, enable lmv dif check
 * in lmv lock mode for rel det, default = 1
 */
/* Bit 19,    reg_mcdi_reldetgmvdifchken */
/* 0: unable; 1: enable, enable lmv dif check in
 * lmv lock mode for rel det, default = 1
 */
/* Bit 18,    reg_mcdi_reldetpd22chken */
/* 0: unable; 1: enable, enable pull-down 22
 * mode check for rel det refinement, default = 1
 */
/* Bit 17,    reg_mcdi_reldetfrqchken */
/* 0: unable; 1: enable, enable mv frequency check in rel det, default = 1 */
/* Bit 16,    reg_mcdi_qmeen */
/* 0: unable; 1: enable, enable quarter motion estimation, defautl = 1 */
/* Bit 15,    reg_mcdi_refrptmven */
/* 0: unable; 1: enable, use repeat mv in refinement, default = 1 */
/* Bit 14,    reg_mcdi_refgmven */
/* 0: unable; 1: enable, use gmv in refinement, default = 1 */
/* Bit 13,    reg_mcdi_reflmven */
/* 0: unable; 1: enable, use lmvs in refinement, default = 1 */
/* Bit 12,    reg_mcdi_refnmven */
/* 0: unable; 1: enable, use neighoring mvs in refinement, default = 1 */
/* Bit 11,    reserved */
/* Bit 10,    reg_mcdi_referrfrqchken */
/* 0: unable; 1: enable, enable mv frquency
 * check while finding min err in ref, default = 1
 */
/* Bit 9,     reg_mcdi_refen */
/* 0: unable; 1: enable, enable mv refinement, default = 1
 */
/* Bit 8,     reg_mcdi_horlineen */
/* 0: unable; 1: enable,enable horizontal lines
 * detection by sad map, default = 1
 */
/* Bit 7,     reg_mcdi_highvertfrqdeten */
/* 0: unable; 1: enable, enable high vertical
 * frequency pattern detection, default = 1
 */
/* Bit 6,     reg_mcdi_gmvlocken */
/* 0: unable; 1: enable, enable gmv lock mode, default = 1 */
/* Bit 5,     reg_mcdi_rptmven */
/* 0: unable; 1: enable, enable repeat pattern detection, default = 1 */
/* Bit 4,     reg_mcdi_gmven */
/* 0: unable; 1: enable, enable global motion estimation, default = 1 */
/* Bit 3,     reg_mcdi_lmven */
/* 0: unable; 1: enable, enable line mv estimation for hme, default = 1 */
/* Bit 2,     reg_mcdi_chkedgeen */
/* 0: unable; 1: enable, enable check edge function, default = 1 */
/* Bit 1,     reg_mcdi_txtdeten */
/* 0: unable; 1: enable, enable texture detection, default = 1 */
/* Bit 0,     reg_mcdi_memcen */
/* 0: unable; 1: enable, enable of memc di, default = 1 */
#define MCDI_UNI_MVDST                             ((0x2f05))
/* Bit 31:20, reserved */
/* Bit 19:17, reg_mcdi_unimvdstabsseg0
 * segment0 for uni-mv abs, default = 1
 */
/* Bit 16:12, reg_mcdi_unimvdstabsseg1
 * segment1 for uni-mv abs, default = 15
 */
/* Bit 11: 8, reg_mcdi_unimvdstabsdifgain0
 * 2/2, gain0 of uni-mv abs dif for segment0, normalized 2 to '1', default = 2
 */
/* Bit  7: 5, reg_mcdi_unimvdstabsdifgain1
 * 2/2, gain1 of uni-mv abs dif for segment1, normalized 2 to '1', default = 2
 */
/* Bit  4: 2, reg_mcdi_unimvdstabsdifgain2
 * 2/2, gain2 of uni-mv abs dif beyond segment1,normalized 2 to '1', default = 2
 */
/* Bit  1: 0, reg_mcdi_unimvdstsgnshft
 * shift for neighboring distance of uni-mv, default = 0
 */
#define MCDI_BI_MVDST                              ((0x2f06))
/* Bit 31:20, reserved */
/* Bit 19:17, reg_mcdi_bimvdstabsseg0
 * segment0 for bi-mv abs, default = 1
 */
/* Bit 16:12, reg_mcdi_bimvdstabsseg1
 * segment1 for bi-mv abs, default = 9
 */
/* Bit 11: 8, reg_mcdi_bimvdstabsdifgain0
 * 6/2, gain0 of bi-mv abs dif for segment0, normalized 2 to '1', default = 6
 */
/* Bit  7: 5, reg_mcdi_bimvdstabsdifgain1
 * 3/2, gain1 of bi-mvabs dif for segment1, normalized 2 to '1', default = 3
 */
/* Bit  4: 2, reg_mcdi_bimvdstabsdifgain2
 * 2/2, gain2 of bi-mvabs dif beyond segment1, normalized 2 to '1', default = 2
 */
/* Bit  1: 0, reg_mcdi_bimvdstsgnshft
 * shift for neighboring distance of bi-mv, default = 0
 */
#define MCDI_SAD_GAIN                              ((0x2f07))
/* Bit 31:19, reserved */
/* Bit 18:17, reg_mcdi_unisadcorepxlgain
 * uni-sad core pixels gain, default = 3
 */
/* Bit 16,    reg_mcdi_unisadcorepxlnormen
 * enable uni-sad core pixels normalization, default = 0
 */
/* Bit 15:11, reserved */
/* Bit 10: 9, reg_mcdi_bisadcorepxlgain
 * bi-sad core pixels gain, default = 3
 */
/* Bit  8,    reg_mcdi_bisadcorepxlnormen
 * enable bi-sad core pixels normalization, default = 1
 */
/* Bit  7: 3, reserved */
/* Bit  2: 1, reg_mcdi_biqsadcorepxlgain
 * bi-qsad core pixels gain, default = 3
 */
/* Bit  0,    reg_mcdi_biqsadcorepxlnormen
 * enable bi-qsad core pixels normalization, default = 1
 */
#define MCDI_TXT_THD                               ((0x2f08))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_txtminmaxdifthd,
 * min max dif threshold (>=) for texture detection, default = 24
 */
/* Bit 15: 8, reg_mcdi_txtmeandifthd,
 * mean dif threshold (<) for texture detection, default = 9
 */
/* Bit  7: 3, reserved */
/* Bit  2: 0, reg_mcdi_txtdetthd,
 * texture detecting threshold, 0~4, default = 2
 */
#define MCDI_FLT_MODESEL                           ((0x2f09))
/* Bit 31	 reserved */
/* Bit 30:28, reg_mcdi_flthorlineselmode
 * mode for horizontal line detecting flat calculation,default = 1,same as below
 */
/* Bit 27	 reserved */
/* Bit 26:24, reg_mcdi_fltgmvselmode
 * mode for gmv flat calculation, default = 4, same as below
 */
/* Bit 23,	 reserved */
/* Bit 22:20, reg_mcdi_fltsadselmode
 * mode for sad flat calculation, default = 2, same as below
 */
/* Bit 19,	 reserved */
/* Bit 18:16, reg_mcdi_fltbadwselmode
 * mode for badw flat calculation, default = 3, same as below
 */
/* Bit 15,	 reserved */
/* Bit 14:12, reg_mcdi_fltrptmvselmode
 * mode for repeat mv flat calculation, default = 4, same as below
 */
/* Bit 11,	 reserved */
/* Bit 10: 8, reg_mcdi_fltbadrelselmode
 * mode for bad rel flat calculation, default = 4, same as below
 */
/* Bit  7,	 reserved */
/* Bit  6: 4, reg_mcdi_fltcolcfdselmode
 * mode for col cfd flat calculation, default = 2, same as below
 */
/* Bit  3,	 reserved */
/* Bit  2: 0, reg_mcdi_fltpd22chkselmode
 * mode for pd22 check flat calculation, default = 2, #
 * 0:cur dif h, 1: cur dif v, 2: pre dif h, 3: pre dif v,
 * 4: cur flt, 5: pre flt, 6: cur+pre, 7: max all(cur,pre)
 */
#define MCDI_CHK_EDGE_THD                          ((0x2f0a))
/* Bit 23:28, reserved. */
/* Bit 27:24, reg_mcdi_chkedgedifsadthd.
 * thd (<=) for sad dif check, 0~8, default = 1
 */
/* Bit 23:16, reserved. */
/* Bit 15:12, reg_mcdi_chkedgemaxedgethd.
 * max drt of edge, default = 15
 */
/* Bit 11: 8, reg_mcdi_chkedgeminedgethd.
 * min drt of edge, default = 2
 */
/* Bit     7, reserved. */
/* Bit  6: 0, reg_mcdi_chkedgevdifthd.
 * thd for vertical dif in check edge, default = 14
 */
#define MCDI_CHK_EDGE_GAIN_OFFST                   ((0x2f0b))
/* Bit 31:24, reserved. */
/* Bit 23:20, reg_mcdi_chkedgedifthd1.
 * thd1 for edge dif check (<=), default = 4
 */
/* Bit 19:16, reg_mcdi_chkedgedifthd0.
 * thd0 for edge dif check (>=), default = 15
 */
/* Bit   :15, reserved. */
/* Bit 14:10, reg_mcdi_chkedgechklen.
 * total check length for edge check, 1~24 (>0), default = 24
 */
/* Bit  9: 8, reg_mcdi_chkedgeedgesel.
 * final edge select mode, 0: original start edge, 1: lpf start edge,
 * 2: orignal start+end edge, 3: lpf start+end edge, default = 1
 */
/* Bit  7: 3, reg_mcdi_chkedgesaddstgain.
 * distance gain for sad calc while getting edges, default = 4
 */
/* Bit     2, reg_mcdi_chkedgechkmode.
 * edge used in check mode, 0: original edge, 1: lpf edge, defautl = 1
 */
/* Bit     1, reg_mcdi_chkedgestartedge.
 * edge mode for start edge, 0: original edge, 1: lpf edge, defautl = 0
 */
/* Bit     0, reg_mcdi_chkedgeedgelpf.
 * edge lpf mode, 0:[0,2,4,2,0], 1:[1,2,2,2,1], default = 0
 */
#define MCDI_LMV_RT                                ((0x2f0c))
/* BIt 31:15, reserved */
/* Bit 14:12, reg_mcdi_lmvvalidmode
 * valid mode for lmv calc., 100b:use char det, 010b: use flt,001b: use hori flg
 */
/* Bit 11:10, reg_mcdi_lmvgainmvmode
 * four modes of mv selection for lmv weight calucluation, default = 1
 */
/* 0: cur(x-3), lst(x-1,x,x+1); 1: cur(x-4,x-3), lst(x,x+1);
 * 2: cur(x-5,x-4,x-3), lst(x-1,x,x+1,x+2,x+3);
 * 3: cur(x-6,x-5,x-4,x-3), lst(x-1,x,x+1,x+2);
 */
/* Bit  9,    reg_mcdi_lmvinitmode
 * initial lmvs at first row of input field, 0: initial value = 0;
 * 1: initial = 32 (invalid), default = 0
 */
/* Bit  8,    reserved */
/* Bit  7: 4, reg_mcdi_lmvrt0      ratio of max mv, default = 5 */
/* Bit  3: 0, reg_mcdi_lmvrt1      ratio of second max mv, default = 5 */
#define MCDI_LMV_GAINTHD                           ((0x2f0d))
/* Bit 31:24, reg_mcdi_lmvvxmaxgain    max gain of lmv weight, default = 96 */
/* Bit 23,    reserved */
/* Bit 22:20, reg_mcdi_lmvdifthd0
 * dif threshold 0 (<) for small lmv, default = 1
 */
/* Bit 19:17, reg_mcdi_lmvdifthd1
 * dif threshold 1 (<) for median lmv, default = 2
 */
/* Bit 16:14, reg_mcdi_lmvdifthd2
 * dif threshold 2 (<) for large lmv, default = 3
 */
/* Bit 13: 8, reg_mcdi_lmvnumlmt
 * least/limit number of (total number - max0), default = 20
 */
/* Bit  7: 0, reg_mcdi_lmvfltthd
 * flt cnt thd (<) for lmv, default = 9
 */
#define MCDI_RPTMV_THD0                            ((0x2f0e))
/* Bit 31:25, reg_mcdi_rptmvslpthd2
 * slope thd (>=) between i and i+3/i-3 (i+4/i-4), default = 64
 */
/* Bit 24:20, reg_mcdi_rptmvslpthd1
 * slope thd (>=) between i and i+2/i-2, default = 4
 */
/* Bit 19:10, reg_mcdi_rptmvampthd2
 * amplitude thd (>=) between max and min, when count cycles, default = 300
 */
/* Bit  9: 0, reg_mcdi_rptmvampthd1
 * amplitude thd (>=) between average of max and min, default = 400
 */
#define MCDI_RPTMV_THD1                            ((0x2f0f))
/* Bit 31:28, reserved */
/* Bit 27:25, reg_mcdi_rptmvcyccntthd
 * thd (>=) of total cycles count, default = 2
 */
/* Bit 24:21, reg_mcdi_rptmvcycdifthd
 * dif thd (<) of cycles length, default = 3
 */
/* Bit 20:18, reg_mcdi_rptmvcycvldthd
 * thd (>) of valid cycles number, default = 1
 */
/* Bit 17:15, reg_mcdi_rptmvhalfcycminthd
 * min length thd (>=) of half cycle, default = 2
 */
/* Bit 14:11, reg_mcdi_rptmvhalfcycdifthd
 * neighboring half cycle length dif thd (<), default = 5
 */
/* Bit 10: 8, reg_mcdi_rptmvminmaxcntthd
 * least number of valid max and min, default = 2
 */
/* Bit  7: 5, reg_mcdi_rptmvcycminthd
 * min length thd (>=) of cycles, default = 2
 */
/* Bit  4: 0, reg_mcdi_rptmvcycmaxthd
 * max length thd (<) of cycles, default = 17
 */
#define MCDI_RPTMV_THD2                            ((0x2f10))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_rptmvhdifthd0
 * higher hdif thd (>=) (vertical edge) for rpt detection, default = 8
 */
/* Bit 15: 8, reg_mcdi_rptmvhdifthd1
 * hdif thd (>=) (slope edge) for rpt detection, default = 4
 */
/* Bit  7: 0, reg_mcdi_rptmvvdifthd
 * vdif thd (>=) (slope edge) for rpt detection, default = 1
 */
#define MCDI_RPTMV_SAD                             ((0x2f11))
/* Bit 31:26, reserved */
/* Bit 25:16, reg_mcdi_rptmvsaddifthdgain
 * 7x3x(16/16), gain for sad dif thd in rpt mv detection,
 * 0~672, normalized 16 as '1', default = 336
 */
/* Bit 15:10, reserved */
/* Bit  9: 0, reg_mcdi_rptmvsaddifthdoffst
 * offset for sad dif thd in rpt mv detection, -512~511, default = 16
 */
#define MCDI_RPTMV_FLG                             ((0x2f12))
/* Bit 31:18,  reserved */
/* Bit 17:16,  reg_mcdi_rptmvmode
 * select mode of mvs for repeat motion estimation, 0: hmv,
 * 1: qmv/2, 2 or 3: qmv/4, default = 2
 */
/* Bit 15: 8,  reg_mcdi_rptmvflgcntthd
 * thd (>=) of min count number for rptmv of whole field,
 * for rptmv estimation, default = 64
 */
/* Bit  7: 5,  reserved */
/* Bit  4: 0,  reg_mcdi_rptmvflgcntrt
 * 4/32, ratio for repeat mv flag count, normalized 32 as '1', set 31 to 32,
 */
#define MCDI_RPTMV_GAIN                            ((0x2f13))
/* Bit 31:24, reg_mcdi_rptmvlftgain
 * up repeat mv gain for hme, default = 96
 */
/* Bit 23:16, reg_mcdi_rptmvuplftgain
 * up left repeat mv gain for hme, default = 32
 */
/* Bit 15: 8, reg_mcdi_rptmvupgain
 * up repeat mv gain for hme, default = 64
 */
/* Bit  7: 0, reg_mcdi_rptmvuprightgain
 * up right repeat mv gain for hme, default = 32
 */
#define MCDI_GMV_RT                                ((0x2f14))
/* Bit 31,    reserved */
/* Bit 30:24, reg_mcdi_gmvmtnrt0
 * ratio 0 for motion senario, set 127 to 128, normalized 128 as '1',default =32
 */
/* Bit 23,    reserved */
/* Bit 22:16, reg_mcdi_gmvmtnrt1
 * ratio 1 for motion senario, set 127 to 128 normalized 128 as '1',default = 56
 */
/* Bit 15,    reserved */
/* Bit 14: 8, reg_mcdi_gmvstlrt0
 * ratio 0 for still senario, set 127 to 128,normalized 128 as '1', default = 56
 */
/* Bit  7,    reserved */
/* Bit  6: 0, reg_mcdi_gmvstlrt1
 * ratio 1 for still senario, set 127 to 128, normalized 128 as '1',default = 80
 */
#define MCDI_GMV_GAIN                              ((0x2f15))
/* Bit 31:25, reg_mcdi_gmvzeromvlockrt0
 * ratio 0 for locking zero mv, set 127 to 128,
 * normalized 128 as '1', default = 100
 */
/* Bit 24:18, reg_mcdi_gmvzeromvlockrt1
 * ratio 1 for locking zero mv, set 127 to 128,
 * normalized 128 as '1', default = 112
 */
/* Bit 17:16, reg_mcdi_gmvvalidmode
 * valid mode for gmv calc., 10b: use flt, 01b: use hori flg, default = 3
 */
/* Bit 15: 8, reg_mcdi_gmvvxgain
 * gmv's vx gain when gmv locked for hme, default = 0
 */
/* Bit  7: 0, reg_mcdi_gmvfltthd
 * flat thd (<) for gmv calc. default = 3
 */
#define MCDI_HOR_SADOFST                           ((0x2f16))
/* Bit 31:25, reserved */
/* Bit 24:16, reg_mcdi_horsaddifthdgain
 * 21*1/8, gain/divisor for sad dif threshold in hor line detection,
 * normalized 8 as '1', default = 21
 */
/* Bit 15: 8, reg_mcdi_horsaddifthdoffst
 * offset for sad dif threshold in hor line detection, -128~127, default = 0
 */
/* Bit  7: 0, reg_mcdi_horvdifthd
 * threshold (>=) of vertical dif of next block for
 * horizontal line detection, default = 24
 */
#define MCDI_REF_MV_NUM                            ((0x2f17))
/* Bit 31: 2, reserved */
/* Bit  1: 0, reg_mcdi_refmcmode.         motion compensated mode used in
 * refinement, 0: pre, 1: next, 2: (pre+next)/2, default = 0
 */
#define MCDI_REF_BADW_THD_GAIN                     ((0x2f18))
/* Bit 31:28, reserved */
/* Bit 27:24, reg_mcdi_refbadwcnt2gain.
 * gain for badwv count num==3, default = 6
 */
/* Bit 23:20, reg_mcdi_refbadwcnt1gain.
 * gain for badwv count num==2, default = 3
 */
/* Bit 19:16, reg_mcdi_refbadwcnt0gain.
 * gain for badwv count num==1, default = 1
 */
/* Bit 15:12, reg_mcdi_refbadwthd3.
 * threshold 3 for detect badweave with largest average luma, default = 4
 */
/* Bit 11: 8, reg_mcdi_refbadwthd2.
 * threshold 2 for detect badweave with third smallest average luma, default = 3
 */
/* Bit  7: 4, reg_mcdi_refbadwthd1.
 * threshold 1 for detect badweave with second smallest average luma,default = 2
 */
/* Bit  3: 0, reg_mcdi_refbadwthd0.
 * threshold 0 for detect badweave with smallest average luma, default = 1
 */
#define MCDI_REF_BADW_SUM_GAIN                     ((0x2f19))
/* Bit 31:13, reserved */
/* Bit 12: 8, reg_mcdi_refbadwsumgain0.
 * sum gain for r channel, 0~16, default = 8
 */
/* Bit  7: 5, reserved */
/* Bit     4, reg_mcdi_refbadwcalcmode.
 * mode for badw calculation, 0:sum, 1:max, default = 0
 */
/* Bit  3: 0, reserved */
#define MCDI_REF_BS_THD_GAIN                       ((0x2f1a))
/* Bit 31:28, reg_mcdi_refbsudgain1.
 * up & down block stregth gain1, normalized to 8 as '1', default = 2
 */
/* Bit 27:24, reg_mcdi_refbsudgain0.
 * up & down block stregth gain0, normalized to 8 as '1', default = 4
 */
/* Bit 23:19, reserved */
/* Bit 18:16, reg_mcdi_refbslftgain.
 * left block strength gain, default = 0
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_refbsthd1.
 * threshold 1 for detect block stregth in refinment, default = 16
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_refbsthd0.
 * threshold 0 for detect block stregth in refinment, default = 8
 */
#define MCDI_REF_ERR_GAIN0                         ((0x2f1b))
/* Bit    31, reserved */
/* Bit 30:24, reg_mcdi_referrnbrdstgain.
 * neighoring mv distances gain for err calc. in ref,
 * normalized to 8 as '1', default = 48
 */
/* Bit 23:20, reserved */
/* Bit 19:16, reg_mcdi_referrbsgain.
 * bs gain for err calc. in ref, normalized to 8 as '1', default = 4
 */
/* Bit    15, reserved */
/* Bit 14: 8, reg_mcdi_referrbadwgain.
 * badw gain for err calc. in ref, normalized to 8 as '1', default = 64
 */
/* Bit  7: 4, reserved */
/* Bit  3: 0, reg_mcdi_referrsadgain.
 * sad gain for err calc. in ref, normalized to 8 as '1', default = 4
 */
#define MCDI_REF_ERR_GAIN1                         ((0x2f1c))
/* Bit 31:20, reserved */
/* Bit 19:16, reg_mcdi_referrchkedgegain.
 * check edge gain for err calc. in ref, normalized to 8 as '1', default = 4
 */
/* Bit 15:12, reserved */
/* Bit 11: 8, reg_mcdi_referrlmvgain.
 * (locked) lmv gain for err calc. in ref, normalized to 8 as '1', default = 0
 */
/* Bit  7: 4, reserved */
/* Bit  3: 0, reg_mcdi_referrgmvgain.
 * (locked) gmv gain for err calc. in ref, normalized to 8 as '1', default = 0
 */
#define MCDI_REF_ERR_FRQ_CHK                       ((0x2f1d))
/* Bit 31:28, reserved */
/* Bit 27:24, reg_mcdi_referrfrqgain.
 * gain for mv frquency, normalized to 4 as '1', default = 10
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_referrfrqmax.
 * max gain for mv frquency check, default = 31
 */
/* Bit    15, reserved */
/* Bit 14:12, reg_mcdi_ref_errfrqmvdifthd2.
 * mv dif threshold 2 (<) for mv frquency check, default = 3
 */
/* Bit    11, reserved */
/* Bit 10: 8, reg_mcdi_ref_errfrqmvdifthd1.
 * mv dif threshold 1 (<) for mv frquency check, default = 2
 */
/* Bit     7, reserved */
/* Bit  6: 4, reg_mcdi_ref_errfrqmvdifthd0.
 * mv dif threshold 0 (<) for mv frquency check, default = 1
 */
/* Bit  3: 0, reserved */
#define MCDI_QME_LPF_MSK                           ((0x2f1e))
/* Bit 31:28, reserved */
/* Bit 27:24, reg_mcdi_qmechkedgelpfmsk0.
 * lpf mask0 for chk edge in qme, 0~8, msk1 = (8-msk0),
 * normalized to 8 as '1', default = 7
 */
/* Bit 23:20, reserved */
/* Bit 19:16, reg_mcdi_qmebslpfmsk0.
 * lpf mask0 for bs in qme, 0~8, msk1 = (8-msk0),
 * normalized to 8 as '1', default = 7
 */
/* Bit 15:12, reserved */
/* Bit 11: 8, reg_mcdi_qmebadwlpfmsk0.
 * lpf mask0 for badw in qme, 0~8, msk1 = (8-msk0),
 * normalized to 8 as '1', default = 7
 */
/* Bit  7: 4, reserved */
/* Bit  3: 0, reg_mcdi_qmesadlpfmsk0.
 * lpf mask0 for sad in qme, 0~8, msk1 = (8-msk0),
 * normalized to 8 as '1', default = 7
 */
#define MCDI_REL_DIF_THD_02                        ((0x2f1f))
/* Bit 31:24, reserved. */
/* Bit 23:16, reg_mcdi_reldifthd2.
 * thd (<) for (hdif+vdif), default = 9
 */
/* Bit 15: 8, reg_mcdi_reldifthd1.
 * thd (<) for (vdif), default = 5
 */
/* Bit  7: 0, reg_mcdi_reldifthd0.
 * thd (>=) for (hdif-vdif), default = 48
 */
#define MCDI_REL_DIF_THD_34                        ((0x2f20))
/* Bit 31:16, reserved. */
/* Bit 15: 8, reg_mcdi_reldifthd4.
 * thd (<) for (hdif), default = 255
 */
/* Bit  7: 0, reg_mcdi_reldifthd3.
 * thd (>=) for (vdif-hdif), default = 48
 */
#define MCDI_REL_BADW_GAIN_OFFST_01                ((0x2f21))
/* Bit 31:24, reg_mcdi_relbadwoffst1.
 * offset for badw adj, for flat block, -128~127, default = 0
 */
/* Bit 23:16, reg_mcdi_relbadwgain1.
 * gain for badw adj, for flat block, default = 128
 */
/* Bit 15: 8, reg_mcdi_relbadwoffst0.
 * offset for badw adj, for vertical block, -128~127, default = 0
 */
/* Bit  7: 0, reg_mcdi_relbadwgain0.
 * gain for badw adj, for vertical block, default = 160
 */
#define MCDI_REL_BADW_GAIN_OFFST_23                ((0x2f22))
/* Bit 31:24, reg_mcdi_relbadwoffst3.
 * offset for badw adj, for other block, -128~127, default = 0
 */
/* Bit 23:16, reg_mcdi_relbadwgain3.
 * gain for badw adj, for other block, default = 48
 */
/* Bit 15: 8, reg_mcdi_relbadwoffst2.
 * offset for badw adj, for horizontal block, -128~127, default = 0
 */
/* Bit  7: 0, reg_mcdi_relbadwgain2.
 * gain for badw adj, for horizontal block, default = 48
 */
#define MCDI_REL_BADW_THD_GAIN_OFFST               ((0x2f23))
/* Bit 31:23, reserved. */
/* Bit 22:16, reg_mcdi_relbadwoffst.
 * offset for badw thd adj, -64~63, default = 0
 */
/* Bit 15: 8, reserved. */
/* Bit  7: 0, reg_mcdi_relbadwthdgain.
 * gain0 for badw thd adj, normalized to 16 as '1', default = 16
 */
#define MCDI_REL_BADW_THD_MIN_MAX                  ((0x2f24))
/* Bit 31:18, reserved. */
/* Bit 17: 8, reg_mcdi_relbadwthdmax.
 * max for badw thd adj, default = 256
 */
/* Bit  7: 0, reg_mcdi_relbadwthdmin.
 * min for badw thd adj, default = 16
 */
#define MCDI_REL_SAD_GAIN_OFFST_01                 ((0x2f25))
/* Bit 31:24, reg_mcdi_relsadoffst1.
 * offset for sad adj, for flat block, -128~127, default = 0
 */
/* Bit 23:20, reserved. */
/* Bit 19:16, reg_mcdi_relsadgain1.
 * gain for sad adj, for flat block, normalized to 8 as '1', default = 8
 */
/* Bit 15: 8, reg_mcdi_relsadoffst0.
 * offset for sad adj, for vertical block, -128~127, default = 0
 */
/* Bit  7: 4, reserved. */
/* Bit  3: 0, reg_mcdi_relsadgain0.
 * gain for sad adj, for vertical block, normalized to 8 as '1', default = 6
 */
#define MCDI_REL_SAD_GAIN_OFFST_23                 ((0x2f26))
/* Bit 31:24, reg_mcdi_relsadoffst3.
 * offset for sad adj, for other block, -128~127, default = 0
 */
/* Bit 23:20, reserved. */
/* Bit 19:16, reg_mcdi_relsadgain3.
 * gain for sad adj, for other block, normalized to 8 as '1', default = 8
 */
/* Bit 15: 8, reg_mcdi_relsadoffst2.
 * offset for sad adj, for horizontal block, -128~127, default = 0
 */
/* Bit  7: 4, reserved. */
/* Bit  3: 0, reg_mcdi_relsadgain2.
 * gain for sad adj, for horizontal block, normalized to 8 as '1', default = 12
 */
#define MCDI_REL_SAD_THD_GAIN_OFFST                ((0x2f27))
/* Bit 31:24, reserved. */
/* Bit 23:16, reg_mcdi_relsadoffst.
 * offset for sad thd adj, -128~127, default = 0
 */
/* Bit 15:10, reserved. */
/* Bit  9: 0, reg_mcdi_relsadthdgain.
 * gain for sad thd adj, 21*2/16, normalized to 16 as '1', default = 42
 */
#define MCDI_REL_SAD_THD_MIN_MAX                   ((0x2f28))
/* Bit 31:27, reserved. */
/* Bit 26:16, reg_mcdi_relsadthdmax.
 * max for sad thd adj, 21*32, default = 672
 */
/* Bit 15: 9, reserved. */
/* Bit  8: 0, reg_mcdi_relsadthdmin.
 * min for sad thd adj, 21*2, default = 42
 */
#define MCDI_REL_DET_GAIN_00                       ((0x2f29))
/* Bit 31:21, reserved. */
/* Bit 20:16, reg_mcdi_reldetbsgain0.
 * gain0 (gmv locked) for bs, for det. calc. normalized to 16 as '1',
 * default = 8
 */
/* Bit 15:14, reserved. */
/* Bit 13: 8, reg_mcdi_reldetbadwgain0.
 * gain0 (gmv locked) for badw, for det. calc.
 * normalized to 16 as '1', default = 12
 */
/* Bit  7: 5, reserved. */
/* Bit  4: 0, reg_mcdi_reldetsadgain0.
 * gain0 (gmv locked) for qsad, for det. calc.
 * normalized to 16 as '1', default = 8
 */
#define MCDI_REL_DET_GAIN_01                       ((0x2f2a))
/* Bit 31:14, reserved. */
/* Bit 12: 8, reg_mcdi_reldetchkedgegain0.
 * gain0 (gmv locked) for chk_edge, for det. calc.
 * normalized to 16 as '1', default = 2
 */
/* Bit     7, reserved. */
/* Bit  6: 0, reg_mcdi_reldetnbrdstgain0.
 * gain0 (gmv locked) for neighoring dist, for det.
 * calc. normalized to 16 as '1', default = 24
 */
#define MCDI_REL_DET_GAIN_10                       ((0x2f2b))
/* Bit 31:21, reserved. */
/* Bit 20:16, reg_mcdi_reldetbsgain1.
 * gain1 (lmv locked) for bs, for det. calc. normalized
 * to 16 as '1', default = 0
 */
/* Bit 15:14, reserved. */
/* Bit 13: 8, reg_mcdi_reldetbadwgain1.
 * gain1 (lmv locked) for badw, for det. calc.
 * normalized to 16 as '1', default = 8
 */
/* Bit  7: 5, reserved. */
/* Bit  4: 0, reg_mcdi_reldetsadgain1.
 * gain1 (lmv locked) for qsad, for det. calc.
 * normalized to 16 as '1', default = 8
 */
#define MCDI_REL_DET_GAIN_11                       ((0x2f2c))
/* Bit 31:14, reserved. */
/* Bit 12: 8, reg_mcdi_reldetchkedgegain1.
 * gain1 (lmv locked) for chk_edge, for det. calc.
 * normalized to 16 as '1', default = 0
 */
/* Bit     7, reserved. */
/* Bit  6: 0, reg_mcdi_reldetnbrdstgain1.
 * gain1 (lmv locked) for neighoring dist, for det.
 * calc. normalized to 16 as '1', default = 24
 */
#define MCDI_REL_DET_GAIN_20                       ((0x2f2d))
/* Bit 31:21, reserved. */
/* Bit 20:16, reg_mcdi_reldetbsgain2.
 * gain2 (no locked) for bs, for det. calc.normalized to 16 as '1', default = 12
 */
/* Bit 15:14, reserved. */
/* Bit 13: 8, reg_mcdi_reldetbadwgain2.
 * gain2 (no locked) for badw, for det. calc.
 * normalized to 16 as '1',default = 32
 */
/* Bit  7: 5, reserved. */
/* Bit  4: 0, reg_mcdi_reldetsadgain2.
 * gain2 (no locked) for qsad, for det. calc.
 * normalized to 16 as '1', default = 16
 */
#define MCDI_REL_DET_GAIN_21                       ((0x2f2e))
/* Bit 31:26, reserved */
/* Bit 25:16, reg_mcdi_reldetoffst.
 * offset for rel calculation, for det. calc. -512~511,  default = 0
 */
/* Bit 15:14, reserved. */
/* Bit 12: 8, reg_mcdi_reldetchkedgegain2.
 * gain2 (no locked) for chk_edge, for det. calc.
 * normalized to 16 as '1', default = 10
 */
/* Bit     7, reserved. */
/* Bit  6: 0, reg_mcdi_reldetnbrdstgain2.
 * gain2 (no locked) for neighoring dist, for det. calc.
 * normalized to 16 as '1', default = 32
 */
#define MCDI_REL_DET_GMV_DIF_CHK                   ((0x2f2f))
/* Bit 31:24, reserved. */
/* Bit 23:16, reg_mcdi_reldetgmvfltthd.
 * flat thd (>=) for gmv lock decision, default = 0
 */
/* Bit    15, reserved. */
/* Bit 14:12, reg_mcdi_reldetgmvdifthd.
 * dif thd (>=) for current mv different from gmv for gmv dif check,
 * actually used in Lmv lock check, default = 3
 */
/* Bit    11, reserved. */
/* Bit 10: 8, reg_mcdi_reldetgmvdifmin.
 * min mv dif for gmv dif check, default = 1, note: dif between
 * reg_mcdi_rel_det_gmv_dif_max and reg_mcdi_rel_det_gmv_dif_min
 * should be; 0,1,3,7, not work for others
 */
/* Bit  7: 4, reg_mcdi_reldetgmvdifmax.
 * max mv dif for gmv dif check, default = 4
 */
/* Bit  3: 1, reserved */
/* Bit     0, reg_mcdi_reldetgmvdifmvmode.
 * mv mode used for gmv dif check, 0: use refmv, 1: use qmv, default = 0
 */
#define MCDI_REL_DET_LMV_DIF_CHK                   ((0x2f30))
/* Bit 31:24, reserved. */
/* Bit 23:16, reg_mcdi_reldetlmvfltthd.
 * flat thd (>=) for lmv lock decision, default = 12
 */
/* Bit 15:14, reserved. */
/* Bit 13:12, reg_mcdi_reldetlmvlockchkmode.
 * lmv lock check mode, 0:cur Lmv, 1: cur & (last | next),
 * 2: last & cur & next Lmv, default = 1
 */
/* Bit    11, reserved. */
/* Bit 10: 8, reg_mcdi_reldetlmvdifmin.
 * min mv dif for lmv dif check, default = 1, note: dif between
 * reg_mcdi_rel_det_lmv_dif_max and reg_mcdi_rel_det_lmv_dif_min should be;
 * 0,1,3,7, not work for others
 */
/* Bit  7: 4, reg_mcdi_reldetlmvdifmax.
 * max mv dif for lmv dif check, default = 4
 */
/* Bit  3: 1, reserved */
/* Bit     0, reg_mcdi_reldetlmvdifmvmode.
 * mv mode used for lmv dif check, 0: use refmv, 1: use qmv, default = 0
 */
#define MCDI_REL_DET_FRQ_CHK                       ((0x2f31))
/* Bit 31:12, reserved. */
/* Bit 11: 8, reg_mcdi_reldetfrqgain.
 * gain for frequency check, normalized to 4 as '1', default = 10
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetfrqmax.
 * max value for frequency check, default = 31
 */
#define MCDI_REL_DET_PD22_CHK                      ((0x2f32))
/* Bit 31:18, reserved. */
/* Bit 17: 8, reg_mcdi_reldetpd22chkoffst.
 * offset for pd22 check happened, default = 512
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetpd22chkgain.
 * gain for pd22 check happened, normalized to 8 as '1', default = 12
 */
#define MCDI_REL_DET_RPT_CHK_ROW                   ((0x2f33))
/* Bit 31:27, reserved */
/* Bit 26:16, reg_mcdi_reldetrptchkendrow.
 * end row (<) number for repeat check, default = 2047
 */
/* Bit 15:11, reserved */
/* Bit 10: 0, reg_mcdi_reldetrptchkstartrow.
 * start row (>=) number for repeat check, default = 0
 */
#define MCDI_REL_DET_RPT_CHK_GAIN_QMV              ((0x2f34))
/* Bit 31:30, reserved */
/* Bit 29:24, reg_mcdi_reldetrptchkqmvmax.
 * max thd (<) of abs qmv for repeat check, default = 15,
 * note that quarter mv's range is -63~63
 */
/* Bit 23:22, reserved */
/* Bit 21:16, reg_mcdi_reldetrptchkqmvmin.
 * min thd (>=) of abs qmv for repeat check, default = 10,
 * note that quarter mv's range is -63~63
 */
/* Bit    15, reserved/ */
/* Bit 14: 4, reg_mcdi_reldetrptchkoffst.
 * offset for repeat check, default = 512
 */
/* Bit  3: 0, reg_mcdi_reldetrptchkgain.
 * gain for repeat check, normalized to 8 as '1', default = 4
 */
#define MCDI_REL_DET_RPT_CHK_THD_0                 ((0x2f35))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_reldetrptchkzerosadthd.
 * zero sad thd (<) for repeat check, default = 255
 */
/* Bit 15:14, reserved. */
/* Bit 13: 8, reg_mcdi_reldetrptchkzerobadwthd.
 * zero badw thd (>=) for repeat check, default = 16
 */
/* Bit  7: 4, reserved */
/* Bit  3: 0, reg_mcdi_reldetrptchkfrqdifthd.
 * frequency dif thd (<) for repeat check, 0~10, default = 5
 */
#define MCDI_REL_DET_RPT_CHK_THD_1                 ((0x2f36))
/* Bit 31:16, reserved */
/* Bit 15: 8, reg_mcdi_reldetrptchkvdifthd.
 * vertical dif thd (<) for repeat check, default = 16
 */
/* Bit  7: 0, reg_mcdi_reldetrptchkhdifthd.
 * horizontal dif thd (>=) for repeat check, default = 16
 */
#define MCDI_REL_DET_LPF_DIF_THD                   ((0x2f37))
/* Bit 31:24, reg_mcdi_reldetlpfdifthd3.
 * hdif thd (<) for lpf selection of horizontal block, default = 9
 */
/* Bit 23:16, reg_mcdi_reldetlpfdifthd2.
 * vdif-hdif thd (>=) for lpf selection of horizontal block, default = 48
 */
/* Bit 15: 8, reg_mcdi_reldetlpfdifthd1.
 * vdif thd (<) for lpf selection of vertical block, default = 9
 */
/* Bit  7: 0, reg_mcdi_reldetlpfdifthd0.
 * hdif-vdif thd (>=) for lpf selection of vertical block, default = 48
 */
#define MCDI_REL_DET_LPF_MSK_00_03                 ((0x2f38))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_reldetlpfmsk03.
 * det lpf mask03 for gmv/lmv locked mode, 0~16, default = 1
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_reldetlpfmsk02.
 * det lpf mask02 for gmv/lmv locked mode, 0~16, default = 1
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_reldetlpfmsk01.
 * det lpf mask01 for gmv/lmv locked mode, 0~16, default = 5
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetlpfmsk00.
 * det lpf mask00 for gmv/lmv locked mode, 0~16, default = 8
 */
#define MCDI_REL_DET_LPF_MSK_04_12                 ((0x2f39))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_reldetlpfmsk12.
 * det lpf mask12 for vertical blocks, 0~16, default = 0
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_reldetlpfmsk11.
 * det lpf mask11 for vertical blocks, 0~16, default = 0
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_reldetlpfmsk10.
 * det lpf mask10 for vertical blocks, 0~16, default = 16
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetlpfmsk04.
 * det lpf mask04 for gmv/lmv locked mode, 0~16, default = 1
 */
#define MCDI_REL_DET_LPF_MSK_13_21                 ((0x2f3a))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_reldetlpfmsk21.
 * det lpf mask21 for horizontal blocks, 0~16, default = 6
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_reldetlpfmsk20.
 * det lpf mask20 for horizontal blocks, 0~16, default = 8
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_reldetlpfmsk14.
 * det lpf mask14 for vertical blocks, 0~16, default = 0
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetlpfmsk13.
 * det lpf mask13 for vertical blocks, 0~16, default = 0
 */
#define MCDI_REL_DET_LPF_MSK_22_30                 ((0x2f3b))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_reldetlpfmsk30.
 * det lpf mask30 for other blocks, 0~16, default = 16
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_reldetlpfmsk24.
 * det lpf mask24 for horizontal blocks, 0~16, default = 1
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_reldetlpfmsk23.
 * det lpf mask23 for horizontal blocks, 0~16, default = 0
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetlpfmsk22.
 * det lpf mask22 for horizontal blocks, 0~16, default = 1
 */
#define MCDI_REL_DET_LPF_MSK_31_34                 ((0x2f3c))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_reldetlpfmsk34.
 * det lpf mask34 for other blocks, 0~16, default = 0
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_reldetlpfmsk33.
 * det lpf mask33 for other blocks, 0~16, default = 0
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_reldetlpfmsk32.
 * det lpf mask32 for other blocks, 0~16, default = 0
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_reldetlpfmsk31.
 * det lpf mask31 for other blocks, 0~16, default = 0
 */
/* Note: there are four group lpf masks from addr 37~3b,
 * each group sum equal to 16.
 */
#define MCDI_REL_DET_MIN                           ((0x2f3d))
/* Bit 31: 7, reserved */
/* Bit  6: 0, reg_mcdi_reldetmin.
 * min of detected value, default = 16
 */
#define MCDI_REL_DET_LUT_0_3                       ((0x2f3e))
/* Bit 31:24, reg_mcdi_reldetmaplut3.               default = 8 */
/* Bit 23:16, reg_mcdi_reldetmaplut2.               default = 4 */
/* Bit 15: 8, reg_mcdi_reldetmaplut1.               default = 2 */
/* Bit  7: 0, reg_mcdi_reldetmaplut0.               default = 0 */
#define MCDI_REL_DET_LUT_4_7                       ((0x2f3f))
/* Bit 31:24, reg_mcdi_reldetmaplut7.               default = 64 */
/* Bit 23:16, reg_mcdi_reldetmaplut6.               default = 48 */
/* Bit 15: 8, reg_mcdi_reldetmaplut5.               default = 32 */
/* Bit  7: 0, reg_mcdi_reldetmaplut4.               default = 16 */
#define MCDI_REL_DET_LUT_8_11                      ((0x2f40))
/* Bit 31:24, reg_mcdi_reldetmaplut11.              default = 160 */
/* Bit 23:16, reg_mcdi_reldetmaplut10.              default = 128 */
/* Bit 15: 8, reg_mcdi_reldetmaplut9.               default = 96 */
/* Bit  7: 0, reg_mcdi_reldetmaplut8.               default = 80 */
#define MCDI_REL_DET_LUT_12_15                     ((0x2f41))
/* Bit 31:24, reg_mcdi_reldetmaplut15.              default = 255 */
/* Bit 23:16, reg_mcdi_reldetmaplut14.              default = 240 */
/* Bit 15: 8, reg_mcdi_reldetmaplut13.              default = 224 */
/* Bit  7: 0, reg_mcdi_reldetmaplut12.              default = 192 */
#define MCDI_REL_DET_COL_CFD_THD                   ((0x2f42))
/* Bit 31:24, reg_mcdi_reldetcolcfdfltthd.
 * thd for flat smaller than (<) of column cofidence, default = 5
 */
/* Bit 23:16, reg_mcdi_reldetcolcfdthd1.
 * thd for rel larger than (>=) in rel calc.
 * mode col confidence without gmv locking, default = 160
 */
/* Bit 15: 8, reg_mcdi_reldetcolcfdthd0.
 * thd for rel larger than (>=) in rel calc.
 * mode col confidence when gmv locked, default = 100
 */
/* Bit  7: 2, reg_mcdi_reldetcolcfdbadwthd.
 * thd for badw larger than (>=) in qbadw calc.
 * mode of column cofidence, default = 16
 */
/* Bit     1, reserved */
/* Bit     0, reg_mcdi_reldetcolcfdcalcmode.        calc.
 * mode for column cofidence, 0: use rel, 1: use qbadw, default = 0
 */
#define MCDI_REL_DET_COL_CFD_AVG_LUMA              ((0x2f43))
/* Bit 31:24, reg_mcdi_reldetcolcfdavgmin1.
 * avg luma min1 (>=) for column cofidence, valid between 16~235, default = 235
 */
/* Bit 23:16, reg_mcdi_reldetcolcfdavgmax1.
 * avg luma max1 (<)  for column cofidence, valid between 16~235, default = 235
 */
/* Bit 15: 8, reg_mcdi_reldetcolcfdavgmin0.
 * avg luma min0 (>=) for column cofidence, valid between 16~235, default = 16
 */
/* Bit  7: 0, reg_mcdi_reldetcolcfdavgmax0.
 * avg luma max0 (<)  for column cofidence, valid between 16~235, default = 21
 */
#define MCDI_REL_DET_BAD_THD_0                     ((0x2f44))
/* Bit 31:16, reserved */
/* Bit 15: 8, reg_mcdi_reldetbadsadthd.
 * thd (>=) for bad sad, default = 120 (480/4)
 */
/* Bit  7: 6, reserved */
/* Bit  5: 0, reg_mcdi_reldetbadbadwthd.
 * thd (>=) for bad badw, 0~42, default = 12
 */
#define MCDI_REL_DET_BAD_THD_1                     ((0x2f45))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_reldetbadrelfltthd.
 * thd (>=) of flat for bad rel detection, default = 4
 */
/* Bit 15: 8, reg_mcdi_reldetbadrelthd1.
 * thd (>=) for bad rel without gmv/lmv locked, default = 160
 */
/* Bit  7: 0, reg_mcdi_reldetbadrelthd0.
 * thd (>=) for bad rel with gmv/lmv locked, default = 120
 */
#define MCDI_PD22_CHK_THD                          ((0x2f46))
/* Bit 31:25, reserved */
/* Bit 24:16, reg_mcdi_pd22chksaddifthd.
 * sad dif thd (>=) for (pd22chksad - qsad) for pd22 check, default = 64
 */
/* Bit 15:14, reserved */
/* Bit 13: 8, reg_mcdi_pd22chkqmvthd.
 * thd (>=) of abs qmv for pd22 check, default = 2
 */
/* Bit  7: 0, reg_mcdi_pd22chkfltthd.
 * thd (>=) of flat for pd22 check, default = 4
 */
#define MCDI_PD22_CHK_GAIN_OFFST_0                 ((0x2f47))
/* Bit 31:24, reg_mcdi_pd22chkedgeoffst0.
 * offset0 of pd22chkedge from right film22 phase, -128~127, default = 0
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_pd22chkedgegain0.
 * gain0 of pd22chkedge from right film22 phase,
 * normalized to 16 as '1', default = 16
 */
/* Bit 15:12, reserved */
/* Bit 11: 8, reg_mcdi_pd22chkbadwoffst0.
 * offset0 of pd22chkbadw from right film22 phase, -8~7, default = 0
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_pd22chkbadwgain0.
 * gain0 of pd22chkbadw from right film22 phase,
 * normalized to 16 as '1', default = 8
 */
#define MCDI_PD22_CHK_GAIN_OFFST_1                 ((0x2f48))
/* Bit 31:24, reg_mcdi_pd22chkedgeoffst1.
 * offset1 of pd22chkedge from right film22 phase, -128~127, default = 0
 */
/* Bit 23:21, reserved */
/* Bit 20:16, reg_mcdi_pd22chkedgegain1.
 * gain1 of pd22chkedge from right film22 phase,
 * normalized to 16 as '1', default = 16
 */
/* Bit 15:12, reserved */
/* Bit 11: 8, reg_mcdi_pd22chkbadwoffst1.
 * offset1 of pd22chkbadw from right film22 phase, -8~7, default = 0
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_pd22chkbadwgain1.
 * gain1 of pd22chkbadw from right film22 phase,
 * normalized to 16 as '1', default = 12
 */
#define MCDI_LMV_LOCK_CNT_THD_GAIN                 ((0x2f49))
/* Bit 31:20, reserved */
/* Bit 19:16, reg_mcdi_lmvlockcntmax.
 * max lmv lock count number, default = 6
 */
/* Bit 15:12, reg_mcdi_lmvlockcntoffst.
 * offset for lmv lock count, -8~7, default =  0
 */
/* Bit 11: 8, reg_mcdi_lmvlockcntgain.
 * gain for lmv lock count, normalized 8 as '1', 15 is set to 16, default = 8
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_lmvlockcntthd.
 * lmv count thd (>=) before be locked, 1~31, default = 4
 */
#define MCDI_LMV_LOCK_ABS_DIF_THD                  ((0x2f4a))
/* Bit 31:27, reserved */
/* Bit 26:24, reg_mcdi_lmvlockdifthd2.
 * lmv dif thd for third part, before locked, default = 1
 */
/* Bit    23, reserved */
/* Bit 22:20, reg_mcdi_lmvlockdifthd1.
 * lmv dif thd for second part, before locked, default = 1
 */
/* Bit    19, reserved */
/* Bit 18:16, reg_mcdi_lmvlockdifthd0.
 * lmv dif thd for first part, before locked, default = 1
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_lmvlockabsmax.
 * max abs (<) of lmv to be locked, default = 24
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_lmvlockabsmin.
 * min abs (>=) of lmv to be locked, default = 1
 */
#define MCDI_LMV_LOCK_ROW                          ((0x2f4b))
/* Bit 31:27, reserved */
/* Bit 26:16, reg_mcdi_lmvlockendrow.
 * end row (<) for lmv lock, default = 2047
 */
/* Bit 15:11, reserved */
/* Bit 10: 0, reg_mcdi_lmvlockstartrow.
 * start row (>=) for lmv lock, default = 0
 */
#define MCDI_LMV_LOCK_RT_MODE                      ((0x2f4c))
/* Bit 31:27, reserved */
/* Bit 26:24, reg_mcdi_lmvlockextmode.
 * extend lines for lmv lock check, check how many lines
 * for lmv locking, default = 2
 */
/* Bit 23:16, reg_mcdi_lmvlockfltcntrt.
 * ratio of flt cnt for lock check, normalized 256 as '1',
 * 255 is set to 256, default = 32
 */
/* Bit 15: 8, reg_mcdi_lmvlocklmvcntrt1.
 * ratio when use non-zero lmv for lock check,
 * normalized 256 as '1', 255 is set to 256, default = 48
 */
/* Bit  7: 0, reg_mcdi_lmvlocklmvcntrt0.
 * ratio when use max lmv for lock check, normalized 256 as '1',
255 is set to 256, default = 106
*/
#define MCDI_GMV_LOCK_CNT_THD_GAIN                 ((0x2f4d))
/* Bit 31:20, reserved */
/* Bit 19:16, reg_mcdi_gmvlockcntmax.
 * max gmv lock count number, default = 6
 */
/* Bit 15:12, reg_mcdi_gmvlockcntoffst.
 * offset for gmv lock count, -8~7, default =  0
 */
/* Bit 11: 8, reg_mcdi_gmvlockcntgain.
 * gain for gmv lock count, normalized 8 as '1', 15 is set to 16, default = 8
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_gmvlockcntthd.
 * gmv count thd (>=) before be locked, 1~31, default = 4
 */
#define MCDI_GMV_LOCK_ABS_DIF_THD                  ((0x2f4e))
/* Bit 31:27, reserved */
/* Bit 26:24, reg_mcdi_gmvlockdifthd2.
 * gmv dif thd for third part, before locked, default = 3
 */
/* Bit    23, reserved */
/* Bit 22:20, reg_mcdi_gmvlockdifthd1.
 * gmv dif thd for second part, before locked, default = 2
 */
/* Bit    19, reserved */
/* Bit 18:16, reg_mcdi_gmvlockdifthd0.
 * gmv dif thd for first part, before locked, default = 1
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_gmvlockabsmax.
 * max abs of gmv to be locked, default = 15
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_gmvlockabsmin.
 * min abs of gmv to be locked, default = 1
 */
#define MCDI_HIGH_VERT_FRQ_DIF_THD                 ((0x2f4f))
/* Bit 31: 0, reg_mcdi_highvertfrqfldavgdifthd.
 * high_vert_frq field average luma dif thd (>=), 3*Blk_Width*Blk_Height,
 * set by software, default = 103680
 */
#define MCDI_HIGH_VERT_FRQ_DIF_DIF_THD             ((0x2f50))
/* Bit 31: 0, reg_mcdi_highvertfrqfldavgdifdifthd.
 * high_vert_frq field average luma dif's dif thd (<), 3*Blk_Width*Blk_Height,
 * set by software, default = 103680
 */
#define MCDI_HIGH_VERT_FRQ_RT_GAIN                 ((0x2f51))
/* Bit 31:20, reserved */
/* Bit 19:16, reg_mcdi_highvertfrqcntthd.
 * high_vert_frq count thd (>=) before locked, 1~31, default = 4
 */
/* Bit 15: 8, reg_mcdi_highvertfrqbadsadrt.
 * ratio for high_vert_frq bad sad count, normalized 256 as '1',
 * 255 is set to 256, default = 24
 */
/* Bit  7: 0, reg_mcdi_highvertfrqbadbadwrt.
 * ratio for high_vert_frq badw count, normalized 256 as '1',
 * 255 is set to 256, default = 130
 */
#define MCDI_MOTION_PARADOX_THD                    ((0x2f52))
/* Bit 31:29, reserved */
/* Bit 28:24, reg_mcdi_motionparadoxcntthd.
 * motion paradox count thd (>=) before locked, 1~31, default = 4
 */
/* Bit 23:22, reserved */
/* Bit 21:16, reg_mcdi_motionparadoxgmvthd.
 * abs gmv thd (<) of motion paradox, 0~32, note that 32
 * means invalid gmv, be careful, default = 32
 */
/* Bit 15: 0, reserved */
#define MCDI_MOTION_PARADOX_RT                     ((0x2f53))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_motionparadoxbadsadrt.
 * ratio for field bad sad count of motion paradox,
 * normalized 256 as '1', 255 is set to 256, default = 24
 */
/* Bit 15: 8, reg_mcdi_motionparadoxbadrelrt.
 * ratio for field bad reliabilty count of motion paradox,
 * normalized 256 as '1', 255 is set to 256, default = 120
 */
/* Bit  7: 0, reg_mcdi_motionparadoxmtnrt.
 * ratio for field motion count of motion paradox,
 * normalized 256 as '1', 255 is set to 256, default = 218
 */
#define MCDI_MOTION_REF_THD                        ((0x2f54))
/* Bit 31:24, reserved */
/* Bit 23:20, reg_mcdi_motionrefoffst.
 * motion ref additive offset, default = 15
 */
/* Bit 19:16, reg_mcdi_motionrefgain.
 * motion ref gain, normalized 8 as '1', default = 8
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_motionrefrptmvthd.
 * abs thd (>=) of rpt mv (0~31, 32 means invalid) for motion ref, default = 1
 */
/* Bit  7: 2, reg_mcdi_motionrefqmvthd.
 * min thd (>=) of abs qmv for motion ref,
 * note that quarter mv's range is -63~63, default = 2
 */
/* Bit  1: 0, reg_mcdi_motionreflpfmode.
 * Mv and (8 x repeat flg) 's lpf mode of motion refinement,
 * 0: no lpf, 1: [1 2 1], 2: [1 2 2 2 1], default = 1
 */
#define MCDI_REL_COL_REF_RT                        ((0x2f55))
/* Bit 31: 8, reserved */
/* Bit  7: 0, reg_mcdi_relcolrefrt.
 * ratio for column cofidence level against column number,
 * for refinement, default = 135
 */
#define MCDI_PD22_CHK_THD_RT                       ((0x2f56))
/* Bit 31:27, reserved */
/* Bit 26:16, reg_mcdi_pd22chkfltcntrt.
 * ratio for flat count of field pulldown 22 check, normalized 2048 as '1',
 * 2047 is set to 2048, default = 1
 */
/* Bit 15: 8, reg_mcdi_pd22chkcntrt.  ratio of pulldown 22 check count,
 * normalized 256 as '1', 255 is set to 256, default = 100
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_pd22chkcntthd.
 * thd (>=) for pd22 count before locked, 1~31, default = 4
 */
#define MCDI_CHAR_DET_DIF_THD                      ((0x2f57))
/* Bit 31:24, reserved */
/* Bit 23:16, reg_mcdi_chardetminmaxdifthd.
 * thd (>=) for dif between min and max value, default = 64
 */
/* Bit 15: 8, reg_mcdi_chardetmaxdifthd.
 * thd (<) for dif between max value, default = 17
 */
/* Bit  7: 0, reg_mcdi_chardetmindifthd.
 * thd (<) for dif between min value, default = 17
 */
#define MCDI_CHAR_DET_CNT_THD                      ((0x2f58))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_chardettotcntthd.
 * thd (>=) for total count, 0~21, default = 18
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_chardetmaxcntthd.
 * thd (>=) for max count, 0~21, default = 1
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_chardetmincntthd.
 * thd (>=) for min count, 0~21, default = 1
 */
#define MCDI_FIELD_MV                              ((0x2f60))
/* Bit 31:24, reg_mcdi_pd22chkcnt */
/* Bit 23:16, reg_mcdi_fieldgmvcnt */
/* Bit    15, reg_mcdi_pd22chkflg */
/* Bit    14, reg_mcdi_fieldgmvlock */
/* Bit 13: 8, reg_mcdi_fieldrptmv.	           last field rpt mv */
/* Bit  7: 6, reserved */
/* Bit  5: 0, reg_mcdi_fieldgmv.                    last field gmv */
#define MCDI_FIELD_HVF_PRDX_CNT                    ((0x2f61))
/* Bit 31:24, reg_mcdi_motionparadoxcnt. */
/* Bit 23:17, reserved */
/* Bit    16, reg_mcdi_motionparadoxflg. */
/* Bit 15: 8, reg_mcdi_highvertfrqcnt. */
/* Bit  7: 4, reserved */
/* Bit  3: 2, reg_mcdi_highvertfrqphase. */
/* Bit     1, reserved */
/* Bit     0, reg_mcdi_highvertfrqflg. */
#define MCDI_FIELD_LUMA_AVG_SUM_0                  ((0x2f62))
/* Bit 31: 0, reg_mcdi_fld_luma_avg_sum0. */
#define MCDI_FIELD_LUMA_AVG_SUM_1                  ((0x2f63))
/* Bit 31: 0, reg_mcdi_fld_luma_avg_sum1. */
#define MCDI_YCBCR_BLEND_CRTL                      ((0x2f64))
/* Bit 31:16, reserved */
/* Bit 15: 8, reg_mcdi_ycbcrblendgain.
 * ycbcr blending gain for cbcr in ycbcr. default = 0
 */
/* Bit  7: 2, reserved. */
/* Bit  1: 0, reg_mcdi_ycbcrblendmode.
 * 0:y+cmb(cb,cr), 1:med(r,g,b), 2:max(r,g,b), default = 2
 */
#define MCDI_MCVECWR_CANVAS_SIZE                   ((0x2f65))
#define MCDI_MCVECRD_CANVAS_SIZE                   ((0x2f66))
#define MCDI_MCINFOWR_CANVAS_SIZE                  ((0x2f67))
#define MCDI_MCINFORD_CANVAS_SIZE                  ((0x2f68))
#define MCDI_LMVLCKSTEXT_0                         ((0x2f69))
#define MCDI_LMVLCKSTEXT_1                         ((0x2f6a))
#define MCDI_LMVLCKEDEXT_0                         ((0x2f6b))
#define MCDI_LMVLCKEDEXT_1                         ((0x2f6c))
#define MCDI_MCVECWR_X                             ((0x2f92))
#define MCDI_MCVECWR_Y                             ((0x2f93))
#define MCDI_MCVECWR_CTRL                          ((0x2f94))
#define MCDI_MCVECRD_X                             ((0x2f95))
#define MCDI_MCVECRD_Y                             ((0x2f96))
#define MCDI_MCVECRD_CTRL                          ((0x2f97))
#define MCDI_MCINFOWR_X                            ((0x2f98))
#define MCDI_MCINFOWR_Y                            ((0x2f99))
#define MCDI_MCINFOWR_CTRL                         ((0x2f9a))
#define MCDI_MCINFORD_X                            ((0x2f9b))
#define MCDI_MCINFORD_Y                            ((0x2f9c))
#define MCDI_MCINFORD_CTRL                         ((0x2f9d))
/* === MC registers ============================================ */
#define MCDI_MC_CRTL                               ((0x2f70))
/* Bit 31: 9, reserved */
/* Bit     8, reg_mcdi_mcpreflg.
 * flag to use previous field for MC, 0:forward field,
 * 1: previous field, default = 1
 */
/* Bit     7, reg_mcdi_mcrelrefbycolcfden.
 * enable rel refinement by column cofidence in mc blending, default = 1
 */
/* Bit  6: 5, reg_mcdi_mclpfen.
 * enable mc pixles/rel lpf, 0:disable, 1: lpf rel,
 * 2: lpf mc pxls, 3: lpf both rel and mc pxls, default = 0
 */
/* Bit  4: 2, reg_mcdi_mcdebugmode.
 * enable mc debug mode, 0:disable, 1: split left/right,
 * 2: split top/bottom, 3: debug mv, 4: debug rel, default = 0
 */
/* Bit  1: 0, reg_mcdi_mcen.
 * mcdi enable mode, 0:disable, 1: blend with ma, 2: full mc, default = 1
 */
#define MCDI_MC_LPF_MSK_0                          ((0x2f71))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_mclpfmsk02.
 * mc lpf coef. 2 for pixel 0 of current block,normalized 16 as '1', default = 0
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_mclpfmsk01.
 * mc lpf coef. 1 for pixel 0 of current block,normalized 16 as '1', default = 9
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_mclpfmsk00.
 * mc lpf coef. 0 for pixel 0 of current block,normalized 16 as '1', default = 7
 */
#define MCDI_MC_LPF_MSK_1                          ((0x2f72))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_mclpfmsk12.
 * mc lpf coef. 2 for pixel 1 of current block,
 * 0~16, normalized 16 as '1', default = 0
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_mclpfmsk11.
 * mc lpf coef. 1 for pixel 1 of current block, 0~16,
 * normalized 16 as '1', default = 11
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_mclpfmsk10.
 * mc lpf coef. 0 for pixel 1 of current block, 0~16,
 * normalized 16 as '1', default = 5
 */
#define MCDI_MC_LPF_MSK_2                          ((0x2f73))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_mclpfmsk22.
 * mc lpf coef. 2 for pixel 2 of current block, 0~16,
 * normalized 16 as '1', default = 1
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_mclpfmsk21.
 * mc lpf coef. 1 for pixel 2 of current block, 0~16,
 * normalized 16 as '1', default = 14
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_mclpfmsk20.
 * mc lpf coef. 0 for pixel 2 of current block, 0~16,
 * normalized 16 as '1', default = 1
 */
#define MCDI_MC_LPF_MSK_3                          ((0x2f74))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_mclpfmsk32.
 * mc lpf coef. 2 for pixel 3 of current block, 0~16,
 * normalized 16 as '1', default = 5
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_mclpfmsk31.
 * mc lpf coef. 1 for pixel 3 of current block, 0~16,
 * normalized 16 as '1', default = 11
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_mclpfmsk30.
 * mc lpf coef. 0 for pixel 3 of current block, 0~16,
 * normalized 16 as '1', default = 0
 */
#define MCDI_MC_LPF_MSK_4                          ((0x2f75))
/* Bit 31:21, reserved */
/* Bit 20:16, reg_mcdi_mclpfmsk42.
 * mc lpf coef. 2 for pixel 4 of current block, 0~16,
 * normalized 16 as '1', default = 7
 */
/* Bit 15:13, reserved */
/* Bit 12: 8, reg_mcdi_mclpfmsk41.
 * mc lpf coef. 1 for pixel 4 of current block, 0~16,
 * normalized 16 as '1', default = 9
 */
/* Bit  7: 5, reserved */
/* Bit  4: 0, reg_mcdi_mclpfmsk40.
 * mc lpf coef. 0 for pixel 4 of current block, 0~16,
 * normalized 16 as '1', default = 0
 */
#define MCDI_MC_REL_GAIN_OFFST_0                   ((0x2f76))
/* Bit 31:26, reserved */
/* Bit    25, reg_mcdi_mcmotionparadoxflg.
 * flag of motion paradox, initial with 0 and read from software, default = 0
 */
/* Bit    24, reg_mcdi_mchighvertfrqflg.
 * flag of high vert frq, initial with 0 and read from software, default = 0
 */
/* Bit 23:16, reg_mcdi_mcmotionparadoxoffst.
 * offset (rel + offset) for rel (MC blending coef.)
 * refinement if motion paradox
 * detected before MC blending before MC blending, default = 128
 */
/* Bit 15:12, reserved */
/* Bit 11: 8, reg_mcdi_mcmotionparadoxgain.
 * gain for rel (MC blending coef.)
 * refinement if motion paradox detected before MC
 * blending, normalized 8 as '1', set 15 to 16, default = 8
 */
/* Bit  7: 4, reg_mcdi_mchighvertfrqoffst.          minus offset
 * (alpha - offset) for motion (MA blending coef.) refinement if high vertical
 * frequency detected before MA blending, default = 15
 */
/* Bit  3: 0, reg_mcdi_mchighvertfrqgain.
 * gain for motion (MA blending coef.) refinement if high vertical frequency
 * detected before MA blending, normalized 8 as '1', set 15 to 16, default = 8
 */
#define MCDI_MC_REL_GAIN_OFFST_1                   ((0x2f77))
/* Bit 31:24, reg_mcdi_mcoutofboundrayoffst.
 * offset (rel + offset) for rel (MC blending coef.) refinement
 * if MC pointed out
 * of boundray before MC blending before MC blending, default = 255
 */
/* Bit 23:20, reserved*/
/* Bit 19:16, reg_mcdi_mcoutofboundraygain.
 * gain for rel (MC blending coef.) refinement
 * if MC pointed out of boundray before
 * MC blending, normalized 8 as '1', set 15 to 16, default = 8
 */
/* Bit 15: 8, reg_mcdi_mcrelrefbycolcfdoffst.
 * offset (rel + offset) for rel (MC blending coef.) refinement
 * if motion paradox
 * detected before MC blending before MC blending, default = 255
 */
/* Bit  7: 4, reserved. */
/* Bit  3: 0, reg_mcdi_mcrelrefbycolcfdgain.
 * gain for rel (MC blending coef.) refinement
 * if column cofidence failed before MC
 * blending, normalized 8 as '1', set 15 to 16, default = 8
 */
#define MCDI_MC_COL_CFD_0                          ((0x2f78))
/* Bit 31: 0, mcdi_mc_col_cfd_0.
 * column cofidence value 0 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_1                          ((0x2f79))
/* Bit 31: 0, mcdi_mc_col_cfd_1.
 * column cofidence value 1 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_2                          ((0x2f7a))
/* Bit 31: 0, mcdi_mc_col_cfd_2.
 * column cofidence value 2 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_3                          ((0x2f7b))
/* Bit 31: 0, mcdi_mc_col_cfd_3.
 * column cofidence value 3 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_4                          ((0x2f7c))
/* Bit 31: 0, mcdi_mc_col_cfd_4.
 * column cofidence value 4 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_5                          ((0x2f7d))
/* Bit 31: 0, mcdi_mc_col_cfd_5.
 * column cofidence value 5 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_6                          ((0x2f7e))
/* Bit 31: 0, mcdi_mc_col_cfd_6.
 * column cofidence value 6 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_7                          ((0x2f7f))
/* Bit 31: 0, mcdi_mc_col_cfd_7.
 * column cofidence value 7 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_8                          ((0x2f80))
/* Bit 31: 0, mcdi_mc_col_cfd_8.
 * column cofidence value 8 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_9                          ((0x2f81))
/* Bit 31: 0, mcdi_mc_col_cfd_9.
 * column cofidence value 9 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_10                         ((0x2f82))
/* Bit 31: 0, mcdi_mc_col_cfd_10.
 * column cofidence value 10 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_11                         ((0x2f83))
/* Bit 31: 0, mcdi_mc_col_cfd_11.
 * column cofidence value 11 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_12                         ((0x2f84))
/* Bit 31: 0, mcdi_mc_col_cfd_12.
 * column cofidence value 12 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_13                         ((0x2f85))
/* Bit 31: 0, mcdi_mc_col_cfd_13.
 * column cofidence value 13 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_14                         ((0x2f86))
/* Bit 31: 0, mcdi_mc_col_cfd_14.
 * column cofidence value 14 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_15                         ((0x2f87))
/* Bit 31: 0, mcdi_mc_col_cfd_15.
 * column cofidence value 15 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_16                         ((0x2f88))
/* Bit 31: 0, mcdi_mc_col_cfd_16.
 * column cofidence value 16 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_17                         ((0x2f89))
/* Bit 31: 0, mcdi_mc_col_cfd_17.
 * column cofidence value 17 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_18                         ((0x2f8a))
/* Bit 31: 0, mcdi_mc_col_cfd_18.
 * column cofidence value 18 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_19                         ((0x2f8b))
/* Bit 31: 0, mcdi_mc_col_cfd_19.
 * column cofidence value 19 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_20                         ((0x2f8c))
/* Bit 31: 0, mcdi_mc_col_cfd_20.
 * column cofidence value 20 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_21                         ((0x2f8d))
/* Bit 31: 0, mcdi_mc_col_cfd_21.
 * column cofidence value 21 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_22                         ((0x2f8e))
/* Bit 31: 0, mcdi_mc_col_cfd_22.
 * column cofidence value 22 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_23                         ((0x2f8f))
/* Bit 31: 0, mcdi_mc_col_cfd_23.
 * column cofidence value 23 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_24                         ((0x2f90))
/* Bit 31: 0, mcdi_mc_col_cfd_24.
 * column cofidence value 24 read from software. initial = 0
 */
#define MCDI_MC_COL_CFD_25                         ((0x2f91))
/* Bit 31: 0, mcdi_mc_col_cfd_25.
 * column cofidence value 25 read from software. initial = 0
 */
/* ======= PRE RO Registers ==================================== */
#define MCDI_RO_FLD_LUMA_AVG_SUM                   ((0x2fa0))
/* Bit 31: 0, ro_mcdi_fldlumaavgsum.
 * block's luma avg sum of current filed (block based). initial = 0
 */
#define MCDI_RO_GMV_VLD_CNT                        ((0x2fa1))
/* Bit 31: 0, ro_mcdi_gmvvldcnt.
 * valid gmv's count of pre one filed (block based). initial = 0
 */
#define MCDI_RO_RPT_FLG_CNT                        ((0x2fa2))
/* Bit 31: 0, ro_mcdi_rptflgcnt.
 * repeat mv's count of pre one filed (block based). initial = 0
 */
#define MCDI_RO_FLD_BAD_SAD_CNT                    ((0x2fa3))
/* Bit 31: 0, ro_mcdi_fldbadsadcnt.
 * bad sad count of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_FLD_BAD_BADW_CNT                   ((0x2fa4))
/* Bit 31: 0, ro_mcdi_fldbadbadwcnt.
 * bad badw count of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_FLD_BAD_REL_CNT                    ((0x2fa5))
/* Bit 31: 0, ro_mcdi_fldbadrelcnt.
 * bad rel count of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_FLD_MTN_CNT                        ((0x2fa6))
/* Bit 31: 0, ro_mcdi_fldmtncnt.
 * motion count of whole pre one field (pixel based). initial = 0
 */
#define MCDI_RO_FLD_VLD_CNT                        ((0x2fa7))
/* Bit 31: 0, ro_mcdi_fldvldcnt.
 * valid motion count of whole pre one field (pixel based). initial = 0
 */
#define MCDI_RO_FLD_PD_22_PRE_CNT                  ((0x2fa8))
/* Bit 31: 0, ro_mcdi_fldpd22precnt.
 * prevoius pd22 check count of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_FLD_PD_22_FOR_CNT                  ((0x2fa9))
/* Bit 31: 0, ro_mcdi_fldpd22forcnt.
 * forward pd22 check count of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_FLD_PD_22_FLT_CNT                  ((0x2faa))
/* Bit 31: 0, ro_mcdi_fldpd22fltcnt.
 * flat count (for pd22 check) of whole pre one field (block based). initial = 0
 */
#define MCDI_RO_HIGH_VERT_FRQ_FLG                  ((0x2fab))
/* Bit 31:16, reserved. */
/* Bit 15: 8, ro_mcdi_highvertfrqcnt.
 * high vertical frequency count till prevoius one field. initial = 0
 */
/* Bit  7: 3, reserved. */
/* Bit  2: 1, ro_mcdi_highvertfrqphase.
 * high vertical frequency phase of prevoius one field. initial = 2
 */
/* Bit     0, ro_mcdi_highvertfrqflg.
 * high vertical frequency flag of prevoius one field. initial = 0
 */
#define MCDI_RO_GMV_LOCK_FLG                       ((0x2fac))
/* Bit 31:16, reserved. */
/* Bit 15: 8, ro_mcdi_gmvlckcnt.
 * global mv lock count till prevoius one field. initial = 0
 */
/* Bit  7: 2, ro_mcdi_gmv.
 * global mv of prevoius one field. -31~31, initial = 32 (invalid value)
 */
/* Bit     1, ro_mcdi_zerogmvlckflg.
 * zero global mv lock flag of prevoius one field. initial = 0
 */
/* Bit     0, ro_mcdi_gmvlckflg.
 * global mv lock flag of prevoius one field. initial = 0
 */
#define MCDI_RO_RPT_MV                             ((0x2fad))
/* Bit 5: 0, ro_mcdi_rptmv.
 * repeate mv of prevoius one field. -31~31, initial = 32 (invalid value)
 */
#define MCDI_RO_MOTION_PARADOX_FLG                 ((0x2fae))
/* Bit 31:16, reserved. */
/* Bit 15: 8, ro_mcdi_motionparadoxcnt.
 * motion paradox count till prevoius one field. initial = 0
 */
/* Bit  7: 1, reserved. */
/* Bit     0, ro_mcdi_motionparadoxflg.
 * motion paradox flag of prevoius one field. initial = 0
 */
#define MCDI_RO_PD_22_FLG                          ((0x2faf))
/* Bit 31:16, reserved. */
/* Bit 15: 8, ro_mcdi_pd22cnt.
 * pull down 22 count till prevoius one field. initial = 0
 */
/* Bit  7: 1, reserved. */
/* Bit     0, ro_mcdi_pd22flg.
 * pull down 22 flag of prevoius one field. initial = 0
 */
#define MCDI_RO_COL_CFD_0                          ((0x2fb0))
/* Bit 31: 0, ro_mcdi_col_cfd_0.
 * column cofidence value 0. initial = 0
 */
#define MCDI_RO_COL_CFD_1                          ((0x2fb1))
/* Bit 31: 0, ro_mcdi_col_cfd_1.
 * column cofidence value 1. initial = 0
 */
#define MCDI_RO_COL_CFD_2                          ((0x2fb2))
/* Bit 31: 0, ro_mcdi_col_cfd_2.
 * column cofidence value 2. initial = 0
 */
#define MCDI_RO_COL_CFD_3                          ((0x2fb3))
/* Bit 31: 0, ro_mcdi_col_cfd_3.
 * column cofidence value 3. initial = 0
 */
#define MCDI_RO_COL_CFD_4                          ((0x2fb4))
/* Bit 31: 0, ro_mcdi_col_cfd_4.
 * column cofidence value 4. initial = 0
 */
#define MCDI_RO_COL_CFD_5                          ((0x2fb5))
/* Bit 31: 0, ro_mcdi_col_cfd_5.
 * column cofidence value 5. initial = 0
 */
#define MCDI_RO_COL_CFD_6                          ((0x2fb6))
/* Bit 31: 0, ro_mcdi_col_cfd_6.      column cofidence value 6. initial = 0 */
#define MCDI_RO_COL_CFD_7                          ((0x2fb7))
/* Bit 31: 0, ro_mcdi_col_cfd_7.      column cofidence value 7. initial = 0 */
#define MCDI_RO_COL_CFD_8                          ((0x2fb8))
/* Bit 31: 0, ro_mcdi_col_cfd_8.      column cofidence value 8. initial = 0 */
#define MCDI_RO_COL_CFD_9                          ((0x2fb9))
/* Bit 31: 0, ro_mcdi_col_cfd_9.      column cofidence value 9. initial = 0 */
#define MCDI_RO_COL_CFD_10                         ((0x2fba))
/* Bit 31: 0, ro_mcdi_col_cfd_10.     column cofidence value 10. initial = 0 */
#define MCDI_RO_COL_CFD_11                         ((0x2fbb))
/* Bit 31: 0, ro_mcdi_col_cfd_11.     column cofidence value 11. initial = 0 */
#define MCDI_RO_COL_CFD_12                         ((0x2fbc))
/* Bit 31: 0, ro_mcdi_col_cfd_12.     column cofidence value 12. initial = 0 */
#define MCDI_RO_COL_CFD_13                         ((0x2fbd))
/* Bit 31: 0, ro_mcdi_col_cfd_13.     column cofidence value 13. initial = 0 */
#define MCDI_RO_COL_CFD_14                         ((0x2fbe))
/* Bit 31: 0, ro_mcdi_col_cfd_14.     column cofidence value 14. initial = 0 */
#define MCDI_RO_COL_CFD_15                         ((0x2fbf))
/* Bit 31: 0, ro_mcdi_col_cfd_15.     column cofidence value 15. initial = 0 */
#define MCDI_RO_COL_CFD_16                         ((0x2fc0))
/* Bit 31: 0, ro_mcdi_col_cfd_16.     column cofidence value 16. initial = 0 */
#define MCDI_RO_COL_CFD_17                         ((0x2fc1))
/* Bit 31: 0, ro_mcdi_col_cfd_17.     column cofidence value 17. initial = 0 */
#define MCDI_RO_COL_CFD_18                         ((0x2fc2))
/* Bit 31: 0, ro_mcdi_col_cfd_18.     column cofidence value 18. initial = 0 */
#define MCDI_RO_COL_CFD_19                         ((0x2fc3))
/* Bit 31: 0, ro_mcdi_col_cfd_19.     column cofidence value 19. initial = 0 */
#define MCDI_RO_COL_CFD_20                         ((0x2fc4))
/* Bit 31: 0, ro_mcdi_col_cfd_20.     column cofidence value 20. initial = 0 */
#define MCDI_RO_COL_CFD_21                         ((0x2fc5))
/* Bit 31: 0, ro_mcdi_col_cfd_21.     column cofidence value 21. initial = 0 */
#define MCDI_RO_COL_CFD_22                         ((0x2fc6))
/* Bit 31: 0, ro_mcdi_col_cfd_22.     column cofidence value 22. initial = 0 */
#define MCDI_RO_COL_CFD_23                         ((0x2fc7))
/* Bit 31: 0, ro_mcdi_col_cfd_23.     column cofidence value 23. initial = 0 */
#define MCDI_RO_COL_CFD_24                         ((0x2fc8))
/* Bit 31: 0, ro_mcdi_col_cfd_24.     column cofidence value 24. initial = 0 */
#define MCDI_RO_COL_CFD_25                         ((0x2fc9))
/* Bit 31: 0, ro_mcdi_col_cfd_25.     column cofidence value 25. initial = 0 */


#define DIPD_COMB_CTRL0					0x2fd0
/* Bit 31: 24, cmb_v_dif_min */
/* Bit 23: 16, cmb_v_dif_max */
/* Bit 15:  8, cmb_crg_mi */
/* Bit  7:  0, cmb_crg_max */
#define DIPD_COMB_CTRL1					0x2fd1
/* Bit 31: 31, pd_check_en */
/* Bit 29: 24, cmb_wv_min3 */
/* Bit 21: 16, cmb_wv_min2 */
/* Bit 13:  8, cmb_wv_min1 */
/* Bit  5:  0, cmb_wv_min0 */
#define DIPD_COMB_CTRL2					0x2fd2
/* Bit 31: 28, cmb_wnd_cnt1 */
/* Bit 25: 20, ccnt_cmmin1 */
/* Bit 19: 16, ccnt_mtmin */
/* Bit 13:  8, ccnt_cmmin */
/* Bit  5:  0, cmb_wv_min4 */
#define DIPD_COMB_CTRL3					0x2fd3
/* Bit 31: 31, cmb32spcl */
/* Bit 17: 12, cmb_wnd_mthd */
/* Bit 11:  4, cmb_abs_nocmb */
/* Bit  3:  0, cnt_minlen */
#define DIPD_COMB_CTRL4					0x2fd4
/* Bit 30: 30, flm_stamtn_en */
/* Bit 29: 28, in_horflt */
/* Bit 27: 20, alpha */
/* Bit 19: 16, thtran_ctmtd */
/* Bit 15:  8, htran_mnth1 */
/* Bit  7:  0, htran_mnth0 */
#define DIPD_COMB_CTRL5					0x2fd5
/* Bit 31: 24, fld_mindif */
/* Bit 23: 16, frm_mindif */
/* Bit 13:  8, flm_smp_mtn_cnt */
/* Bit  7:  0, flm_smp_mtn_thd */
#define DIPD_RO_COMB_0					0x2fd6
#define DIPD_RO_COMB_1					0x2fd7
#define DIPD_RO_COMB_2					0x2fd8
#define DIPD_RO_COMB_3					0x2fd9
#define DIPD_RO_COMB_4					0x2fda
#define DIPD_RO_COMB_5					0x2fdb
#define DIPD_RO_COMB_6					0x2fdc
#define DIPD_RO_COMB_7					0x2fdd
#define DIPD_RO_COMB_8					0x2fde
#define DIPD_RO_COMB_9					0x2fdf
#define DIPD_RO_COMB_10					0x2fe0
#define DIPD_RO_COMB_11					0x2fe1
#define DIPD_RO_COMB_12					0x2fe2
#define DIPD_RO_COMB_13					0x2fe3
#define DIPD_RO_COMB_14					0x2fe4
#define DIPD_RO_COMB_15					0x2fe5
#define DIPD_RO_COMB_16					0x2fe6
#define DIPD_RO_COMB_17					0x2fe7
#define DIPD_RO_COMB_18					0x2fe8
#define DIPD_RO_COMB_19					0x2fe9
#define DIPD_RO_COMB_20					0x2fea
#define DIPD_COMB_CTRL6					0x2feb
/* nr3 */
#define NR3_MODE					0x2ff0
		/* d010bfc0 */
#define NR3_COOP_PARA					0x2ff1
#define NR3_CNOOP_GAIN					0x2ff2
#define NR3_YMOT_PARA					0x2ff3
#define NR3_CMOT_PARA					0x2ff4
#define NR3_SUREMOT_YGAIN				0x2ff5
#define NR3_SUREMOT_CGAIN				0x2ff6

#endif
