/*
 * drivers/amlogic/media/deinterlace/di_pps.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/amlogic/media/registers/regs/di_regs.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "di_pps.h"
#include "register.h"
#if 0
/* pps filter coefficients */
#define COEF_BICUBIC         0
#define COEF_3POINT_TRIANGLE 1
#define COEF_4POINT_TRIANGLE 2
#define COEF_BILINEAR        3
#define COEF_2POINT_BILINEAR 4
#define COEF_BICUBIC_SHARP   5
#define COEF_3POINT_TRIANGLE_SHARP   6
#define COEF_3POINT_BSPLINE  7
#define COEF_4POINT_BSPLINE  8
#define COEF_3D_FILTER       9
#define COEF_NULL            0xff
#define TOTAL_FILTERS        10

#define MAX_NONLINEAR_FACTOR    0x40


const u32 vpp_filter_coefs_bicubic_sharp[] = {
	3,
	33 | 0x8000,
	/* 0x01f80090, 0x01f80100, 0xff7f0200, 0xfe7f0300, */
	0x01fa008c, 0x01fa0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bicubic[] = {
	4,
	33,
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bilinear[] = {
	4,
	33,
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
	0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
	0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
	0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
	0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
	0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
	0x00404000
};

const u32 vpp_3d_filter_coefs_bilinear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

const u32 vpp_filter_coefs_3point_triangle[] = {
	3,
	33,
	0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
	0x3a420400, 0x39420500, 0x37430600, 0x36430700,
	0x35430800, 0x33450800, 0x32450900, 0x31450a00,
	0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
	0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
	0x26491100, 0x25491200, 0x24491300, 0x234a1300,
	0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
	0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
	0x1a4c1a00
};

/* point_num =4, filt_len =4, group_num = 64, [1 2 1] */
const u32 vpp_filter_coefs_4point_triangle[] = {
	4,
	33,
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};
/*
 *4th order (cubic) b-spline
 *filt_cubic point_num =4, filt_len =4, group_num = 64, [1 5 1]
 */
const u32 vpp_filter_coefs_4point_bspline[] = {
	4,
	33,
	0x15561500, 0x14561600, 0x13561700, 0x12561800,
	0x11551a00, 0x11541b00, 0x10541c00, 0x0f541d00,
	0x0f531e00, 0x0e531f00, 0x0d522100, 0x0c522200,
	0x0b522300, 0x0b512400, 0x0a502600, 0x0a4f2700,
	0x094e2900, 0x084e2a00, 0x084d2b00, 0x074c2c01,
	0x074b2d01, 0x064a2f01, 0x06493001, 0x05483201,
	0x05473301, 0x05463401, 0x04453601, 0x04433702,
	0x04423802, 0x03413a02, 0x03403b02, 0x033f3c02,
	0x033d3d03
};
/*3rd order (quadratic) b-spline*/
/*filt_quadratic, point_num =3, filt_len =3, group_num = 64, [1 6 1] */
const u32 vpp_filter_coefs_3point_bspline[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3c440000, 0x3a460000,
	0x38480000, 0x364a0000, 0x344b0100, 0x334c0100,
	0x314e0100, 0x304f0100, 0x2e500200, 0x2c520200,
	0x2a540200, 0x29540300, 0x27560300, 0x26570300,
	0x24580400, 0x23590400, 0x215a0500, 0x205b0500,
	0x1e5c0600, 0x1d5c0700, 0x1c5d0700, 0x1a5e0800,
	0x195e0900, 0x185e0a00, 0x175f0a00, 0x15600b00,
	0x14600c00, 0x13600d00, 0x12600e00, 0x11600f00,
	0x10601000
};
/*filt_triangle, point_num =3, filt_len =2.6, group_num = 64, [1 7 1] */
const u32 vpp_filter_coefs_3point_triangle_sharp[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3d430000, 0x3b450000,
	0x3a460000, 0x38480000, 0x37490000, 0x354b0000,
	0x344c0000, 0x324e0000, 0x314f0000, 0x2f510000,
	0x2e520000, 0x2c540000, 0x2b550000, 0x29570000,
	0x28580000, 0x265a0000, 0x245c0000, 0x235d0000,
	0x215f0000, 0x20600000, 0x1e620000, 0x1d620100,
	0x1b620300, 0x19630400, 0x17630600, 0x15640700,
	0x14640800, 0x12640a00, 0x11640b00, 0x0f650c00,
	0x0d660d00
};

const u32 vpp_filter_coefs_2point_binilear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

static const u32 *filter_table[] = {
	vpp_filter_coefs_bicubic,
	vpp_filter_coefs_3point_triangle,
	vpp_filter_coefs_4point_triangle,
	vpp_filter_coefs_bilinear,
	vpp_filter_coefs_2point_binilear,
	vpp_filter_coefs_bicubic_sharp,
	vpp_filter_coefs_3point_triangle_sharp,
	vpp_filter_coefs_3point_bspline,
	vpp_filter_coefs_4point_bspline,
	vpp_3d_filter_coefs_bilinear
};

static int chroma_filter_table[] = {
	COEF_BICUBIC, /* bicubic */
	COEF_3POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE, /* bilinear */
	COEF_2POINT_BILINEAR,
	COEF_3POINT_TRIANGLE, /* bicubic_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_triangle_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_bspline */
	COEF_4POINT_TRIANGLE, /* 4point_bspline */
	COEF_3D_FILTER		  /* can not change */
};


static unsigned int vert_scaler_filter = 0xff;
module_param(vert_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_scaler_filter, "vert_scaler_filter");

static unsigned int vert_chroma_scaler_filter = 0xff;
module_param(vert_chroma_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_chroma_scaler_filter, "vert_chroma_scaler_filter");

static unsigned int horz_scaler_filter = 0xff;
module_param(horz_scaler_filter, uint, 0664);
MODULE_PARM_DESC(horz_scaler_filter, "horz_scaler_filter");

bool pre_scaler_en = true;
module_param(pre_scaler_en, bool, 0664);
MODULE_PARM_DESC(pre_scaler_en, "pre_scaler_en");
#endif
unsigned int di_filt_coef0[] =   //bicubic
{
	0x00800000,
	0x007f0100,
	0xff7f0200,
	0xfe7f0300,
	0xfd7e0500,
	0xfc7e0600,
	0xfb7d0800,
	0xfb7c0900,
	0xfa7b0b00,
	0xfa7a0dff,
	0xf9790fff,
	0xf97711ff,
	0xf87613ff,
	0xf87416fe,
	0xf87218fe,
	0xf8701afe,
	0xf76f1dfd,
	0xf76d1ffd,
	0xf76b21fd,
	0xf76824fd,
	0xf76627fc,
	0xf76429fc,
	0xf7612cfc,
	0xf75f2ffb,
	0xf75d31fb,
	0xf75a34fb,
	0xf75837fa,
	0xf7553afa,
	0xf8523cfa,
	0xf8503ff9,
	0xf84d42f9,
	0xf84a45f9,
	0xf84848f8
};

unsigned int di_filt_coef1[] =  //2 point bilinear
{
	0x00800000,
	0x007e0200,
	0x007c0400,
	0x007a0600,
	0x00780800,
	0x00760a00,
	0x00740c00,
	0x00720e00,
	0x00701000,
	0x006e1200,
	0x006c1400,
	0x006a1600,
	0x00681800,
	0x00661a00,
	0x00641c00,
	0x00621e00,
	0x00602000,
	0x005e2200,
	0x005c2400,
	0x005a2600,
	0x00582800,
	0x00562a00,
	0x00542c00,
	0x00522e00,
	0x00503000,
	0x004e3200,
	0x004c3400,
	0x004a3600,
	0x00483800,
	0x00463a00,
	0x00443c00,
	0x00423e00,
	0x00404000
};

unsigned int di_filt_coef2[] =  //2 point bilinear, bank_length == 2
{
	 0x80000000,
	 0x7e020000,
	 0x7c040000,
	 0x7a060000,
	 0x78080000,
	 0x760a0000,
	 0x740c0000,
	 0x720e0000,
	 0x70100000,
	 0x6e120000,
	 0x6c140000,
	 0x6a160000,
	 0x68180000,
	 0x661a0000,
	 0x641c0000,
	 0x621e0000,
	 0x60200000,
	 0x5e220000,
	 0x5c240000,
	 0x5a260000,
	 0x58280000,
	 0x562a0000,
	 0x542c0000,
	 0x522e0000,
	 0x50300000,
	 0x4e320000,
	 0x4c340000,
	 0x4a360000,
	 0x48380000,
	 0x463a0000,
	 0x443c0000,
	 0x423e0000,
	 0x40400000
};


#define ZOOM_BITS       20
#define PHASE_BITS      16

static enum f2v_vphase_type_e top_conv_type = F2V_P2P;
static enum f2v_vphase_type_e bot_conv_type = F2V_P2P;
static unsigned int prehsc_en;
static unsigned int prevsc_en;

static const unsigned char f2v_420_in_pos_luma[F2V_TYPE_MAX] = {
0, 2, 0, 2, 0, 0, 0, 2, 0};
//static const unsigned char f2v_420_in_pos_chroma[F2V_TYPE_MAX] = {
//1, 5, 1, 5, 2, 2, 1, 5, 2};
static const unsigned char f2v_420_out_pos[F2V_TYPE_MAX] = {
0, 2, 2, 0, 0, 2, 0, 0, 0};

static void f2v_get_vertical_phase(unsigned int zoom_ratio,
	enum f2v_vphase_type_e type,
	unsigned char bank_length,
	struct pps_f2v_vphase_s *vphase)
{
	int offset_in, offset_out;

    /* luma */
	offset_in = f2v_420_in_pos_luma[type] << PHASE_BITS;
	offset_out = (f2v_420_out_pos[type] * zoom_ratio)
		>> (ZOOM_BITS - PHASE_BITS);

	vphase->rcv_num = bank_length;
	if (bank_length == 4 || bank_length == 3)
		vphase->rpt_num = 1;
	else
		vphase->rpt_num = 0;

	if (offset_in > offset_out) {
		vphase->rpt_num = vphase->rpt_num + 1;
		vphase->phase =
			((4 << PHASE_BITS) + offset_out - offset_in) >> 2;
	} else {
		while ((offset_in + (4 << PHASE_BITS)) <= offset_out) {
			if (vphase->rpt_num == 1)
				vphase->rpt_num = 0;
			else
				vphase->rcv_num++;
			offset_in += 4 << PHASE_BITS;
		}
		vphase->phase = (offset_out - offset_in) >> 2;
	}
}

/*
 * patch 1: inp scaler 0: di wr scaler
 * support: TM2
 * not support: SM1
 */
void di_pps_config(unsigned char path, int src_w, int src_h,
	int dst_w, int dst_h)
{
	struct pps_f2v_vphase_s vphase;

	int i;
	int hsc_en = 0, vsc_en = 0;
	int vsc_double_line_mode;
	unsigned int p_src_w, p_src_h;
	unsigned int vert_phase_step, horz_phase_step;
	unsigned char top_rcv_num, bot_rcv_num;
	unsigned char top_rpt_num, bot_rpt_num;
	unsigned short top_vphase, bot_vphase;
	unsigned char is_frame;
	int vert_bank_length = 4;

	unsigned int *filt_coef0 = di_filt_coef0;
	//unsigned int *filt_coef1 = di_filt_coef1;
	unsigned int *filt_coef2 = di_filt_coef2;

	vsc_double_line_mode = 0;

	if (src_h != dst_h)
		vsc_en = 1;
	if (src_w != dst_w)
		hsc_en = 1;
	/* config hdr size */
	Wr_reg_bits(DI_HDR_IN_HSIZE, dst_w, 0, 13);
	Wr_reg_bits(DI_HDR_IN_VSIZE, dst_h, 0, 13);
	p_src_w = (prehsc_en ? ((src_w+1) >> 1) : src_w);
	p_src_h = prevsc_en ? ((src_h+1) >> 1) : src_h;

	Wr(DI_SC_HOLD_LINE, 0x10);

	if (p_src_w > 2048) {
		//force vert bank length = 2
		vert_bank_length = 2;
		vsc_double_line_mode = 1;
	}

	//write vert filter coefs
	Wr(DI_SC_COEF_IDX, 0x0000);
	for (i = 0; i < 33; i++) {
		if (vert_bank_length == 2)
			Wr(DI_SC_COEF, filt_coef2[i]); //bilinear
		else
			Wr(DI_SC_COEF, filt_coef0[i]); //bicubic
	}

	//write horz filter coefs
	Wr(DI_SC_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++)
		Wr(DI_SC_COEF, filt_coef0[i]); //bicubic

	if (p_src_h > 2048)
		vert_phase_step = ((p_src_h << 18) / dst_h) << 2;
	else
		vert_phase_step = (p_src_h << 20) / dst_h;
	if (p_src_w > 2048)
		horz_phase_step = ((p_src_w << 18) / dst_w) << 2;
	else
		horz_phase_step = (p_src_w << 20) / dst_w;

	is_frame = ((top_conv_type == F2V_IT2P) ||
			(top_conv_type == F2V_IB2P) ||
			(top_conv_type == F2V_P2P));

	if (is_frame) {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type,
			vert_bank_length, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase  = vphase.phase;

		bot_rcv_num = 0;
		bot_rpt_num = 0;
		bot_vphase  = 0;
	} else {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type,
			vert_bank_length, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase = vphase.phase;

		f2v_get_vertical_phase(vert_phase_step, bot_conv_type,
			vert_bank_length, &vphase);
		bot_rcv_num = vphase.rcv_num;
		bot_rpt_num = vphase.rpt_num;
		bot_vphase = vphase.phase;
	}
	vert_phase_step = (vert_phase_step << 4);
	horz_phase_step = (horz_phase_step << 4);

	Wr(DI_SC_LINE_IN_LENGTH, src_w);
	Wr(DI_SC_PIC_IN_HEIGHT, src_h);
	Wr(DI_VSC_REGION12_STARTP,  0);
	Wr(DI_VSC_REGION34_STARTP, ((dst_h << 16) | dst_h));
	Wr(DI_VSC_REGION4_ENDP, (dst_h - 1));

	Wr(DI_VSC_START_PHASE_STEP, vert_phase_step);
	Wr(DI_VSC_REGION0_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION1_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION3_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION4_PHASE_SLOPE, 0);

	Wr(DI_VSC_PHASE_CTRL,
		((vsc_double_line_mode << 17) |
		(!is_frame) << 16) |
			(0 << 15) |
			(bot_rpt_num << 13) |
			(bot_rcv_num << 8) |
			(0 << 7) |
			(top_rpt_num << 5) |
			(top_rcv_num));
	Wr(DI_VSC_INI_PHASE, (bot_vphase << 16) | top_vphase);
	Wr(DI_HSC_REGION12_STARTP, 0);
	Wr(DI_HSC_REGION34_STARTP, (dst_w << 16) | dst_w);
	Wr(DI_HSC_REGION4_ENDP, dst_w - 1);

	Wr(DI_HSC_START_PHASE_STEP, horz_phase_step);
	Wr(DI_HSC_REGION0_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION1_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION3_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION4_PHASE_SLOPE, 0);

	Wr(DI_HSC_PHASE_CTRL, (1 << 21) |
			(4 << 16) | 0);
	Wr_reg_bits(DI_SC_TOP_CTRL, (path?3:0), 29, 2);
	Wr(DI_SC_MISC,
		(prevsc_en << 21) |
		(prehsc_en << 20) |      // prehsc_en
		(prevsc_en << 19) |      // prevsc_en
		(vsc_en << 18)    |      // vsc_en
		(hsc_en << 17)    |      // hsc_en
		((vsc_en | hsc_en) << 16)         |      // sc_top_en
		(1 << 15)         |      // vd1 sc out enable
		(0 << 12)         |      // horz nonlinear 4region enable
		(4 << 8)          |      // horz scaler bank length
		(0 << 5)          |      // vert scaler phase field mode enable
		(0 << 4)          |      // vert nonlinear 4region enable
		(vert_bank_length << 0)  // vert scaler bank length
	);

	pr_info("[pps] %s input %d %d output %d %d.\n",
		path?"pre":"post", src_w, src_h, dst_w, dst_h);
}
/*
 * 0x374e ~ 0x376d, 20 regs
 */
void dump_pps_reg(unsigned int base_addr)
{
	unsigned int i = 0x374e;

	pr_info("-----dump pps start-----\n");
	for (i = 0x374e; i < 0x376e; i++) {
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	}
	pr_info("-----dump pps end-----\n");
}
#if 0
static void
vpp_set_filters2(u32 process_3d_type, u32 width_in,
	u32 height_in,
	u32 wid_out,
	u32 hei_out,
	const struct vinfo_s *vinfo,
	u32 vpp_flags,
	struct vpp_frame_par_s *next_frame_par)
{
	u32 screen_width, screen_height;
	s32 start, end;
	s32 video_top, video_left, temp;
	u32 video_width, video_height;
	u32 ratio_x = 0;
	u32 ratio_y = 0;
	u32 tmp_ratio_y = 0;
	int temp_width;
	int temp_height;
	struct vppfilter_mode_s *filter = &next_frame_par->vpp_filter;
	u32 wide_mode;
	s32 height_shift = 0;
	u32 height_after_ratio;
	u32 aspect_factor;
	s32 ini_vphase;
	u32 w_in = width_in;
	u32 h_in = height_in;
	bool h_crop_enable = false, v_crop_enable = false;
	u32 width_out = wid_out;	/* vinfo->width; */
	u32 height_out = hei_out;	/* vinfo->height; */
	u32 aspect_ratio_out =
		(vinfo->aspect_ratio_den << 8) / vinfo->aspect_ratio_num;
	bool fill_match = true;
	u32 orig_aspect = 0;
	u32 screen_aspect = 0;
	bool skip_policy_check = true;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		if ((likely(w_in >
			(video_source_crop_left + video_source_crop_right)))
			&& (super_scaler == 0)) {
			w_in -= video_source_crop_left;
			w_in -= video_source_crop_right;
			h_crop_enable = true;
		}

		if ((likely(h_in >
			(video_source_crop_top + video_source_crop_bottom)))
			&& (super_scaler == 0)) {
			h_in -= video_source_crop_top;
			h_in -= video_source_crop_bottom;
			v_crop_enable = true;
		}
	} else {
		if (likely(w_in >
			(video_source_crop_left + video_source_crop_right))) {
			w_in -= video_source_crop_left;
			w_in -= video_source_crop_right;
			h_crop_enable = true;
		}

		if (likely(h_in >
			(video_source_crop_top + video_source_crop_bottom))) {
			h_in -= video_source_crop_top;
			h_in -= video_source_crop_bottom;
			v_crop_enable = true;
		}
	}

#ifndef TV_3D_FUNCTION_OPEN
	next_frame_par->vscale_skip_count = 0;
	next_frame_par->hscale_skip_count = 0;
#endif
	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->vscale_skip_count++;
	if (vpp_flags & VPP_FLAG_INTERLACE_OUT)
		height_shift++;

RESTART:

	aspect_factor = (vpp_flags & VPP_FLAG_AR_MASK) >> VPP_FLAG_AR_BITS;
	wide_mode = vpp_flags & VPP_FLAG_WIDEMODE_MASK;

	/* keep 8 bits resolution for aspect conversion */
	if (wide_mode == VIDEO_WIDEOPTION_4_3) {
		if (vpp_flags & VPP_FLAG_PORTRAIT_MODE)
			aspect_factor = 0x155;
		else
			aspect_factor = 0xc0;
		wide_mode = VIDEO_WIDEOPTION_NORMAL;
	} else if (wide_mode == VIDEO_WIDEOPTION_16_9) {
		if (vpp_flags & VPP_FLAG_PORTRAIT_MODE)
			aspect_factor = 0x1c7;
		else
			aspect_factor = 0x90;
		wide_mode = VIDEO_WIDEOPTION_NORMAL;
	} else if ((wide_mode >= VIDEO_WIDEOPTION_4_3_IGNORE)
			   && (wide_mode <= VIDEO_WIDEOPTION_4_3_COMBINED)) {
		if (aspect_factor != 0xc0)
			fill_match = false;

		orig_aspect = aspect_factor;
		screen_aspect = 0xc0;
	} else if ((wide_mode >= VIDEO_WIDEOPTION_16_9_IGNORE)
			   && (wide_mode <= VIDEO_WIDEOPTION_16_9_COMBINED)) {
		if (aspect_factor != 0x90)
			fill_match = false;

		orig_aspect = aspect_factor;
		screen_aspect = 0x90;
	}

	if ((aspect_factor == 0)
		|| (wide_mode == VIDEO_WIDEOPTION_FULL_STRETCH)
		|| (wide_mode == VIDEO_WIDEOPTION_NONLINEAR))
		aspect_factor = 0x100;
	else {
		aspect_factor =
			div_u64((unsigned long long)w_in * height_out *
					(aspect_factor << 8),
					width_out * h_in * aspect_ratio_out);
	}

	if (osd_layer_preblend)
		aspect_factor = 0x100;

	height_after_ratio = (h_in * aspect_factor) >> 8;

	/*
	 *if we have ever set a cropped display area for video layer
	 * (by checking video_layer_width/video_height), then
	 * it will override the input width_out/height_out for
	 * ratio calculations, a.k.a we have a window for video content
	 */
	if (osd_layer_preblend) {
		if ((osd_layer_width == 0) || (osd_layer_height == 0)) {
			video_top = 0;
			video_left = 0;
			video_width = width_out;
			video_height = height_out;

		} else {
			video_top = osd_layer_top;
			video_left = osd_layer_left;
			video_width = osd_layer_width;
			video_height = osd_layer_height;
		}
	} else {
		if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) &&
			next_frame_par->supscl_path == sup0_pp_sp1_scpath) {
			video_top = (video_layer_top >> next_frame_par->
				 supsc1_vert_ratio);
			video_height = (video_layer_height >> next_frame_par->
				 supsc1_vert_ratio);
			video_left = (video_layer_left >> next_frame_par->
				 supsc1_hori_ratio);
			video_width = (video_layer_width >> next_frame_par->
				 supsc1_hori_ratio);
		} else {
			video_top = video_layer_top;
			video_left = video_layer_left;
			video_width = video_layer_width;
			video_height = video_layer_height;
		}
		if ((video_top == 0) && (video_left == 0) && (video_width <= 1)
			&& (video_height <= 1)) {
			/* special case to do full screen display */
			video_width = width_out;
			video_height = height_out;
		} else {
			if ((video_layer_width < 16)
				&& (video_layer_height < 16)) {
				/*
				 *sanity check to move
				 *video out when the target size is too small
				 */
				video_width = width_out;
				video_height = height_out;
				video_left = width_out * 2;
			}
			video_top += video_layer_global_offset_y;
			video_left += video_layer_global_offset_x;
		}
	}

	/*aspect ratio match */
	if ((wide_mode >= VIDEO_WIDEOPTION_4_3_IGNORE)
		&& (wide_mode <= VIDEO_WIDEOPTION_16_9_COMBINED)
		&& orig_aspect) {
		if (vinfo->width && vinfo->height)
			aspect_ratio_out = (vinfo->height << 8) / vinfo->width;

		if ((video_height << 8) > (video_width * aspect_ratio_out)) {
			u32 real_video_height =
				(video_width * aspect_ratio_out) >> 8;

			video_top += (video_height - real_video_height) >> 1;
			video_height = real_video_height;
		} else {
			u32 real_video_width =
				(video_height << 8) / aspect_ratio_out;

			video_left += (video_width - real_video_width) >> 1;
			video_width = real_video_width;
		}

		if (!fill_match) {
			u32 screen_ratio_x, screen_ratio_y;

			screen_ratio_x = 1 << 18;
			screen_ratio_y = (orig_aspect << 18) / screen_aspect;

			switch (wide_mode) {
			case VIDEO_WIDEOPTION_4_3_LETTER_BOX:
			case VIDEO_WIDEOPTION_16_9_LETTER_BOX:
				screen_ratio_x = screen_ratio_y =
					max(screen_ratio_x, screen_ratio_y);
				break;
			case VIDEO_WIDEOPTION_4_3_PAN_SCAN:
			case VIDEO_WIDEOPTION_16_9_PAN_SCAN:
				screen_ratio_x = screen_ratio_y =
				min(screen_ratio_x, screen_ratio_y);
				break;
			case VIDEO_WIDEOPTION_4_3_COMBINED:
			case VIDEO_WIDEOPTION_16_9_COMBINED:
				screen_ratio_x = screen_ratio_y =
				((screen_ratio_x + screen_ratio_y) >> 1);
				break;
			default:
				break;
			}

			ratio_x = screen_ratio_x * w_in / video_width;
			ratio_y =
				screen_ratio_y * h_in / orig_aspect *
				screen_aspect / video_height;
		} else {
			screen_width = video_width * vpp_zoom_ratio / 100;
			screen_height = video_height * vpp_zoom_ratio / 100;

			ratio_x = (w_in << 18) / screen_width;
			ratio_y = (h_in << 18) / screen_height;
		}
	} else {
		screen_width = video_width * vpp_zoom_ratio / 100;
		screen_height = video_height * vpp_zoom_ratio / 100;

		ratio_x = (w_in << 18) / screen_width;
		if (ratio_x * screen_width < (w_in << 18))
			ratio_x++;

		ratio_y = (height_after_ratio << 18) / screen_height;
		if (super_debug)
			pr_info("height_after_ratio=%d,%d,%d,%d,%d\n",
				   height_after_ratio, ratio_x, ratio_y,
				   aspect_factor, wide_mode);

		if (wide_mode == VIDEO_WIDEOPTION_NORMAL) {
			ratio_x = ratio_y = max(ratio_x, ratio_y);
			ratio_y = (ratio_y << 8) / aspect_factor;
		} else if (wide_mode == VIDEO_WIDEOPTION_NORMAL_NOSCALEUP) {
			u32 r1, r2;

			r1 = max(ratio_x, ratio_y);
			r2 = (r1 << 8) / aspect_factor;

			if ((r1 < (1 << 18)) || (r2 < (1 << 18))) {
				if (r1 < r2) {
					ratio_x = 1 << 18;
					ratio_y =
					(ratio_x << 8) / aspect_factor;
				} else {
					ratio_y = 1 << 18;
					ratio_x = aspect_factor << 10;
				}
			} else {
				ratio_x = r1;
				ratio_y = r2;
			}
		}
	}

#if 0
	debug_video_left = video_left;
	debug_video_top = video_top;
	debug_video_width = video_width;
	debug_video_height = video_height;
	debug_ratio_x = ratio_x;
	debug_ratio_y = ratio_y;
	debug_wide_mode = wide_mode;
#endif

	/* vertical */
	ini_vphase = vpp_zoom_center_y & 0xff;

	next_frame_par->VPP_pic_in_height_ =
		h_in / (next_frame_par->vscale_skip_count + 1);

	/* screen position for source */
#ifdef TV_REVERSE
	start =
		video_top + (video_height + 1) / 2 - ((h_in << 17) +
				(vpp_zoom_center_y << 10) +
				(ratio_y >> 1)) / ratio_y;
	end = ((h_in << 18) + (ratio_y >> 1)) / ratio_y + start - 1;
	if (super_debug)
		pr_info("top:start =%d,%d,%d,%d  %d,%d,%d\n",
			start, end, video_top,
			video_height, h_in, ratio_y, vpp_zoom_center_y);
#else
	start =
		video_top + video_height / 2 - ((h_in << 17) +
		(vpp_zoom_center_y << 10)) / ratio_y;
	end = (h_in << 18) / ratio_y + start - 1;
	if (super_debug)
		pr_info("top:start =%d,%d,%d,%d  %d,%d,%d\n",
			start, end, video_top,
			video_height, h_in, ratio_y, vpp_zoom_center_y);
#endif

#ifdef TV_REVERSE
	if (reverse) {
		/* calculate source vertical clip */
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;

			} else
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;

		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;
			} else
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;
		}
		temp =
			next_frame_par->VPP_vd_end_lines_ -
			(video_height * ratio_y >> 18);
		next_frame_par->VPP_vd_start_lines_ = (temp >= 0) ? temp : 0;
	} else
#endif
	{
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;

			} else
				next_frame_par->VPP_vd_start_lines_ = 0;
			temp_height = min((video_top + video_height - 1),
			(vinfo->height - 1));
		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;

			} else
				next_frame_par->VPP_vd_start_lines_ = 0;
			temp_height = min((video_top + video_height - 1),
			(vinfo->height - 1)) - video_top + 1;
		}

		temp =
			next_frame_par->VPP_vd_start_lines_ +
			(temp_height * ratio_y >> 18);
		next_frame_par->VPP_vd_end_lines_ =
			(temp <= (h_in - 1)) ? temp : (h_in - 1);
	}

	if (v_crop_enable) {
		next_frame_par->VPP_vd_start_lines_ += video_source_crop_top;
		next_frame_par->VPP_vd_end_lines_ += video_source_crop_top;
	}

	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->VPP_vd_start_lines_ &= ~1;

	/*
	 *find overlapped region between
	 *[start, end], [0, height_out-1],
	 *[video_top, video_top+video_height-1]
	 */
	start = max(start, max(0, video_top));
	end = min(end, min((s32)(vinfo->height - 1),
		(s32)(video_top + video_height - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_vsc_startp = 0;

		next_frame_par->VPP_vsc_endp = 0;

	} else {
		next_frame_par->VPP_vsc_startp =
		(vpp_flags & VPP_FLAG_INTERLACE_OUT) ? (start >> 1) : start;

		next_frame_par->VPP_vsc_endp =
		(vpp_flags & VPP_FLAG_INTERLACE_OUT) ? (end >> 1) : end;
	}

	/* set filter co-efficients */
	tmp_ratio_y = ratio_y;
	ratio_y <<= height_shift;
	ratio_y = ratio_y / (next_frame_par->vscale_skip_count + 1);

	filter->vpp_vsc_start_phase_step = ratio_y << 6;

	f2v_get_vertical_phase(ratio_y, ini_vphase,
		next_frame_par->VPP_vf_ini_phase_,
		vpp_flags & VPP_FLAG_INTERLACE_OUT);

	/* horizontal */
	filter->vpp_hf_start_phase_slope = 0;
	filter->vpp_hf_end_phase_slope = 0;
	filter->vpp_hf_start_phase_step = ratio_x << 6;

	next_frame_par->VPP_hsc_linear_startp = next_frame_par->VPP_hsc_startp;
	next_frame_par->VPP_hsc_linear_endp = next_frame_par->VPP_hsc_endp;

	filter->vpp_hsc_start_phase_step = ratio_x << 6;
	next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;

	/* screen position for source */
#ifdef TV_REVERSE
	start =
		video_left + (video_width + 1) / 2 - ((w_in << 17) +
				(vpp_zoom_center_x << 10) +
				(ratio_x >> 1)) / ratio_x;
	end = ((w_in << 18) + (ratio_x >> 1)) / ratio_x + start - 1;
	if (super_debug)
		pr_info("left:start =%d,%d,%d,%d  %d,%d,%d\n",
			start, end, video_left,
			video_width, w_in, ratio_x, vpp_zoom_center_x);
#else
	start =
		video_left + video_width / 2 - ((w_in << 17) +
		(vpp_zoom_center_x << 10)) /
		ratio_x;
	end = (w_in << 18) / ratio_x + start - 1;
	if (super_debug)
		pr_info("left:start =%d,%d,%d,%d  %d,%d,%d\n",
			start, end, video_left,
			video_width, w_in, ratio_x, vpp_zoom_center_x);
#endif
	/* calculate source horizontal clip */
#ifdef TV_REVERSE
	if (reverse) {
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
		}
		temp = next_frame_par->VPP_hd_end_lines_ -
			(video_width * ratio_x >> 18);
		next_frame_par->VPP_hd_start_lines_ = (temp >= 0) ? temp : 0;
	} else
#endif
	{
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;

			} else
				next_frame_par->VPP_hd_start_lines_ = 0;
			temp_width = min((video_left + video_width - 1),
			(vinfo->width - 1));
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;

			} else
				next_frame_par->VPP_hd_start_lines_ = 0;
			temp_width = min((video_left + video_width - 1),
			(vinfo->width - 1)) - video_left + 1;
		}
		temp =
			next_frame_par->VPP_hd_start_lines_ +
			(temp_width * ratio_x >> 18);
		next_frame_par->VPP_hd_end_lines_ =
			(temp <= (w_in - 1)) ? temp : (w_in - 1);
	}

	if (h_crop_enable) {
		next_frame_par->VPP_hd_start_lines_ += video_source_crop_left;
		next_frame_par->VPP_hd_end_lines_ += video_source_crop_left;
	}

	next_frame_par->VPP_line_in_length_ =
		next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	/*
	 *find overlapped region between
	 * [start, end], [0, width_out-1],
	 * [video_left, video_left+video_width-1]
	 */
	start = max(start, max(0, video_left));
	end = min(end,
		min((s32)(vinfo->width - 1),
		(s32)(video_left + video_width - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_hsc_startp = 0;
		next_frame_par->VPP_hsc_endp = 0;
		/* avoid mif set wrong or di out size overflow */
		next_frame_par->VPP_hd_start_lines_ = 0;
		next_frame_par->VPP_hd_end_lines_ = 0;
	} else {
		next_frame_par->VPP_hsc_startp = start;

		next_frame_par->VPP_hsc_endp = end;
	}

	if ((wide_mode == VIDEO_WIDEOPTION_NONLINEAR) && (end > start)) {
		calculate_non_linear_ratio(ratio_x, end - start,
				next_frame_par);

		next_frame_par->VPP_hsc_linear_startp =
		next_frame_par->VPP_hsc_linear_endp = (start + end) / 2;
	}

	}

	if ((vf->type & VIDTYPE_COMPRESS) &&
		(vf->canvas0Addr != 0) &&
		(next_frame_par->vscale_skip_count > 1) &&
		(!next_frame_par->nocomp)) {
		pr_info("Try DW buffer for compressed frame scaling.\n");

		/* for VIDTYPE_COMPRESS, check if we can use double write
		 * buffer when primary frame can not be scaled.
		 */
		next_frame_par->nocomp = true;
		w_in = width_in = vf->width;
		h_in = height_in = vf->height;
		next_frame_par->hscale_skip_count = 0;
		next_frame_par->vscale_skip_count = 0;

		goto RESTART;
	}

	if ((skip_policy & 0xf0) && (skip_policy_check == true)) {
		skip_policy_check = false;
		if (skip_policy & 0x40) {
			next_frame_par->vscale_skip_count = skip_policy & 0xf;
			goto RESTART;
		} else if (skip_policy & 0x80) {
			if ((((vf->width >= 4096) &&
			(!(vf->type & VIDTYPE_COMPRESS))) ||
			(vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH))
			&& (next_frame_par->vscale_skip_count == 0)) {
				next_frame_par->vscale_skip_count =
				skip_policy & 0xf;
				goto RESTART;
			}
		}
	}

	filter->vpp_hsc_start_phase_step = ratio_x << 6;

	/* coeff selection before skip and apply pre_scaler */
	filter->vpp_vert_filter =
		coeff(vert_coeff_settings,
			filter->vpp_vsc_start_phase_step *
				(next_frame_par->vscale_skip_count + 1),
			1,
			((vf->type_original & VIDTYPE_TYPEMASK)
				!= VIDTYPE_PROGRESSIVE),
			vf->combing_cur_lev);
	filter->vpp_vert_coeff =
		filter_table[filter->vpp_vert_filter];

	/* when local interlace or AV or ATV */
	/* TODO: add 420 check for local */
	if (vert_chroma_filter_force_en || (vert_chroma_filter_en
	&& (((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)
	 && (((vf->type_original & VIDTYPE_TYPEMASK) != VIDTYPE_PROGRESSIVE) ||
	 (vf->height < vert_chroma_filter_limit)))
	|| (vf->source_type == VFRAME_SOURCE_TYPE_CVBS)
	|| (vf->source_type == VFRAME_SOURCE_TYPE_TUNER)))) {
		cur_vert_chroma_filter
			= chroma_filter_table[filter->vpp_vert_filter];
		filter->vpp_vert_chroma_coeff
			= filter_table[cur_vert_chroma_filter];
		filter->vpp_vert_chroma_filter_en = true;
	} else {
		cur_vert_chroma_filter = COEF_NULL;
		filter->vpp_vert_chroma_filter_en = false;
	}
	/* avoid hscaler fitler adjustion affect on picture shift*/
	filter->vpp_horz_filter =
		coeff(horz_coeff_settings,
			filter->vpp_hf_start_phase_step,
			next_frame_par->VPP_hf_ini_phase_,
			((vf->type_original & VIDTYPE_TYPEMASK)
				!= VIDTYPE_PROGRESSIVE),
			vf->combing_cur_lev);
	/*for gxl cvbs out index*/
	if ((vinfo->mode == VMODE_CVBS) && //DEBUG_TMP
		(filter->vpp_hf_start_phase_step == (1 << 24)))
		filter->vpp_horz_filter = COEF_BICUBIC_SHARP;
	filter->vpp_horz_coeff =
		filter_table[filter->vpp_horz_filter];

	/* apply line skip */
	if (next_frame_par->hscale_skip_count) {
		filter->vpp_hf_start_phase_step >>= 1;
		filter->vpp_hsc_start_phase_step >>= 1;
		next_frame_par->VPP_line_in_length_ >>= 1;
	}

	/*pre hsc&vsc in pps for scaler down*/
	if ((filter->vpp_hf_start_phase_step >= 0x2000000) &&
		(filter->vpp_vsc_start_phase_step >= 0x2000000) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_GXBB) &&
		pre_scaler_en) {
		filter->vpp_pre_vsc_en = 1;
		filter->vpp_vsc_start_phase_step >>= 1;
		ratio_y >>= 1;
		f2v_get_vertical_phase(ratio_y, ini_vphase,
		next_frame_par->VPP_vf_ini_phase_,
		vpp_flags & VPP_FLAG_INTERLACE_OUT);

	} else
		filter->vpp_pre_vsc_en = 0;

	if ((filter->vpp_hf_start_phase_step >= 0x2000000) &&
		(get_cpu_type() != MESON_CPU_MAJOR_ID_GXBB) &&
		pre_scaler_en) {
		filter->vpp_pre_hsc_en = 1;
		filter->vpp_hf_start_phase_step >>= 1;
		filter->vpp_hsc_start_phase_step >>= 1;
	} else
		filter->vpp_pre_hsc_en = 0;

	next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;

	/* overwrite filter setting for interlace output*/
	/* TODO: not reasonable when 4K input to 480i output */
	if (vpp_flags & VPP_FLAG_INTERLACE_OUT) {
		filter->vpp_vert_coeff = filter_table[COEF_BILINEAR];
		filter->vpp_vert_filter = COEF_BILINEAR;
	}

	/* force overwrite filter setting */
	if ((vert_scaler_filter >= COEF_BICUBIC) &&
		(vert_scaler_filter <= COEF_3D_FILTER)) {
		filter->vpp_vert_coeff = filter_table[vert_scaler_filter];
		filter->vpp_vert_filter = vert_scaler_filter;
	}
	if (vert_chroma_filter_force_en &&
		(vert_chroma_scaler_filter >= COEF_BICUBIC) &&
		(vert_chroma_scaler_filter <= COEF_3D_FILTER)) {
		cur_vert_chroma_filter = vert_chroma_scaler_filter;
			filter->vpp_vert_chroma_coeff
				= filter_table[cur_vert_chroma_filter];
			filter->vpp_vert_chroma_filter_en = true;
	} else {
		cur_vert_chroma_filter = COEF_NULL;
		filter->vpp_vert_chroma_filter_en = false;
	}

	if ((horz_scaler_filter >= COEF_BICUBIC) &&
		(horz_scaler_filter <= COEF_3D_FILTER)) {
		filter->vpp_horz_coeff = filter_table[horz_scaler_filter];
		filter->vpp_horz_filter = horz_scaler_filter;
	}

#ifdef	TV_3D_FUNCTION_OPEN
	/* final stage for 3D filter overwrite */
	if ((next_frame_par->vpp_3d_scale) && force_filter_mode) {
		filter->vpp_vert_coeff = filter_table[COEF_3D_FILTER];
		filter->vpp_vert_filter = COEF_3D_FILTER;
	}
#endif
	if ((last_vert_filter != filter->vpp_vert_filter) ||
		(last_horz_filter != filter->vpp_horz_filter)) {
		last_vert_filter = filter->vpp_vert_filter;
		last_horz_filter = filter->vpp_horz_filter;
		scaler_filter_cnt = 0;
	} else {
		scaler_filter_cnt++;
	}
	if ((scaler_filter_cnt >= scaler_filter_cnt_limit) &&
		((cur_vert_filter != filter->vpp_vert_filter) ||
		(cur_horz_filter != filter->vpp_horz_filter))) {
		video_property_notify(1);
		cur_vert_filter = filter->vpp_vert_filter;
		cur_horz_filter = filter->vpp_horz_filter;
		scaler_filter_cnt = scaler_filter_cnt_limit;
	}
	cur_skip_line = next_frame_par->vscale_skip_count;

#if HAS_VPU_PROT
	if (has_vpu_prot()) {
		if (get_prot_status()) {
			s32 tmp_height =
				(((s32) next_frame_par->VPP_vd_end_lines_ +
				  1) << 18) / tmp_ratio_y;
			s32 tmp_top = 0;
			s32 tmp_bottom = 0;

/* pr_info("height_out %d video_height %d\n", height_out, video_height); */
/* pr_info("vf1 %d %d %d %d vs %d %d\n", next_frame_par->VPP_hd_start_lines_,*/
/* next_frame_par->VPP_hd_end_lines_, */
/* next_frame_par->VPP_vd_start_lines_, next_frame_par->VPP_vd_end_lines_, */
/* next_frame_par->hscale_skip_count, next_frame_par->vscale_skip_count); */
			if ((s32) video_height > tmp_height) {
				tmp_top = (s32) video_top +
				(((s32) video_height - tmp_height) >> 1);
			} else
				tmp_top = (s32) video_top;
			tmp_bottom = tmp_top +
			(((s32) next_frame_par->VPP_vd_end_lines_ + 1) << 18) /
			(s32) tmp_ratio_y;
			if (tmp_bottom > (s32) height_out
				&& tmp_top < (s32) height_out) {
				s32 tmp_end =
				(s32) next_frame_par->VPP_vd_end_lines_ -
				((tmp_bottom -
				(s32) height_out) *
				(s32) tmp_ratio_y >> 18);
				if (tmp_end <
				(s32) next_frame_par->VPP_vd_end_lines_) {
					next_frame_par->VPP_vd_end_lines_ =
						tmp_end;
				}

			} else if (tmp_bottom > (s32) height_out
					   && tmp_top >= (s32) height_out)
				next_frame_par->VPP_vd_end_lines_ = 1;
			next_frame_par->VPP_vd_end_lines_ =
				next_frame_par->VPP_vd_end_lines_ -
				h_in / height_out;
			if ((s32) next_frame_par->VPP_vd_end_lines_ <
				(s32) next_frame_par->VPP_vd_start_lines_) {
				next_frame_par->VPP_vd_end_lines_ =
					next_frame_par->VPP_vd_start_lines_;
			}
			if ((s32) next_frame_par->VPP_hd_end_lines_ <
				(s32) next_frame_par->VPP_hd_start_lines_) {
				next_frame_par->VPP_hd_end_lines_ =
					next_frame_par->VPP_hd_start_lines_;
			}
/* pr_info("tmp_top %d tmp_bottom %d tmp_height %d\n",*/
/* tmp_top, tmp_bottom, tmp_height); */
/* pr_info("vf2 %d %d %d %d\n", next_frame_par->VPP_hd_start_lines_,*/
/* next_frame_par->VPP_hd_end_lines_, */
/* next_frame_par->VPP_vd_start_lines_, next_frame_par->VPP_vd_end_lines_); */
		}
	}
#endif
}
#endif

/*
 * di pre h scaling down function
 * only have h scaling down
 * support: sm1 tm2 ...
 * 0x37b0 ~ 0x37b5
 */
void di_inp_hsc_setting(uint32_t src_w, uint32_t dst_w)
{
	uint32_t  i;
	uint32_t  hsc_en;
	uint32_t horz_phase_step;
	int *filt_coef0 = di_filt_coef0;
	/*int *filt_coef1 = di_filt_coef1;*/
	/*int *filt_coef2 = di_filt_coef2;*/

	if (src_w == dst_w) {
		hsc_en = 0;
	} else {
		hsc_en = 1;
		/*write horz filter coefs*/
		RDMA_WR(DI_VIU_HSC_COEF_IDX, 0x0100);
		for (i = 0; i < 33; i++)
			RDMA_WR(DI_VIU_HSC_COEF, filt_coef0[i]); /*bicubic*/

		horz_phase_step = (src_w << 20) / dst_w;
		horz_phase_step = (horz_phase_step << 4);
		RDMA_WR(DI_VIU_HSC_WIDTHM1, (src_w-1)<<16 | (dst_w-1));
		RDMA_WR(DI_VIU_HSC_PHASE_STEP, horz_phase_step);
		RDMA_WR(DI_VIU_HSC_PHASE_CTRL, 0);
	}
	RDMA_WR(DI_VIU_HSC_CTRL,
		(4 << 20) |		/* initial receive number*/
		(0 << 12) |		/* initial pixel ptr*/
		(1 << 10) |		/* repeat first pixel number*/
		(0 << 8) |		/* sp422 mode*/
		(4 << 4) |      /* horz scaler bank length*/
		(0 << 2) |      /* phase0 always en*/
		(0 << 1) |      /* nearest_en*/
		(hsc_en<<0));	/* hsc_en*/
}

/*
 * 0x37b0 ~ 0x37b5
 */
void dump_hdownscler_reg(unsigned int base_addr)
{
	unsigned int i = 0x374e;

	pr_info("-----dump hdownscler start-----\n");
	for (i = 0x37b0; i < 0x37b5; i++) {
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, RDMA_RD(i));
	}
	pr_info("-----dump hdownscler end-----\n");
}


