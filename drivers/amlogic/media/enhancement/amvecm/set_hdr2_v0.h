/*
 * drivers/amlogic/media/enhancement/amvecm/set_hdr2_v0.h
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

#include <linux/types.h>

#ifndef MAX
#define MAX(x1, x2) (double)(x1 > x2 ? x1 : x2)
#endif

#ifndef POW
#define POW(x1, x2) (int64_t)(pow((double)x1, (double)x2))
#endif

#ifndef LOG2
#define LOG2(x) (int)(x == 0 ? 0 : log2((long long)x))
#endif

#define FLTZERO 0xfc000/*float zero*/

#define peak_out  10000/* luma out*/
#define peak_in   1000/* luma in*/
/* 0:hdr10 peak_in input to hdr10 peak_out,*/
/*1:hdr peak_in->gamma,2:gamma->hdr peak out*/
#define fmt_io       2

#define precision  14/* freeze*/
/*input data bitwidth : 12 (VD1 OSD1 VD2)*/
/*10 (VDIN & DI)*/
#define IE_BW      12
#define OE_BW      12/*same IE_BW*/
#define O_BW       32/*freeze*/
#define maxbit     33/*freeze*/
#define OGAIN_BW   12/*freeze*/

int64_t FloatRev(int64_t iA);
int64_t FloatCon(int64_t iA, int MOD);

enum hdr_module_sel {
	VD1_HDR = 0x1,
	VD2_HDR = 0x2,
	OSD1_HDR = 0x4,
	VDIN0_HDR = 0x8,
	VDIN1_HDR = 0x10,
	DI_HDR = 0x20,
	HDR_MAX
};

enum hdr_matrix_sel {
	HDR_IN_MTX = 0x1,
	HDR_GAMUT_MTX = 0x2,
	HDR_OUT_MTX = 0x4,
	HDR_MTX_MAX
};

enum hdr_lut_sel {
	HDR_EOTF_LUT = 0x1,
	HDR_OOTF_LUT = 0x2,
	HDR_OETF_LUT = 0x4,
	HDR_CGAIN_LUT = 0x8,
	HDR_LUT_MAX
};

enum hdr_process_sel {
	HDR_BYPASS = 0x1,
	HDR_SDR = 0x2,
	SDR_HDR = 0x4,
	HLG_BYPASS = 0x8,
	HLG_SDR = 0x10,
	HLG_HDR = 0x20,
	SDR_HLG = 0X40,
	HDRPLUS_SDR = 0x80,
	HDR_p_MAX
};


#define MTX_ON  1
#define MTX_OFF 0

#define MTX_ONLY  1
#define HDR_ONLY  0

#define LUT_ON  1
#define LUT_OFF 0

#define HDR2_EOTF_LUT_SIZE 143
#define HDR2_OOTF_LUT_SIZE 149
#define HDR2_OETF_LUT_SIZE 149
#define HDR2_CGAIN_LUT_SIZE 65

struct hdr_proc_mtx_param_s {
	int mtx_only;
	int mtx_in[15];
	int mtx_gamut[9];
	int mtx_cgain[15];
	int mtx_ogain[15];
	int mtx_out[15];
	unsigned int mtx_on;
	enum hdr_process_sel p_sel;
};

struct hdr_proc_lut_param_s {
	int64_t eotf_lut[143];
	int64_t oetf_lut[149];
	int64_t ogain_lut[149];
	int64_t cgain_lut[65];
	unsigned int lut_on;
	unsigned int bitdepth;
	unsigned int cgain_en;
};

typedef int64_t(*MenuFun)(int64_t);
void eotf_float_gen(int64_t *o_out, MenuFun eotf);
void oetf_float_gen(int64_t *bin_e, MenuFun oetf);
void nolinear_lut_gen(int64_t *bin_c, MenuFun cgain);
extern void hdr_func(enum hdr_module_sel module_sel,
	enum hdr_process_sel hdr_process_select);
/*G12A vpp matrix*/
enum vpp_matrix_e {
	VD1_MTX = 0x1,
	POST2_MTX = 0x2,
	POST_MTX = 0x4
};

enum mtx_csc_e {
	MATRIX_NULL = 0,
	MATRIX_RGB_YUV601 = 0x1,
	MATRIX_RGB_YUV601F = 0x2,
	MATRIX_RGB_YUV709 = 0x3,
	MATRIX_RGB_YUV709F = 0x4,
	MATRIX_YUV601_RGB = 0x10,
	MATRIX_YUV601_YUV601F = 0x11,
	MATRIX_YUV601_YUV709 = 0x12,
	MATRIX_YUV601_YUV709F = 0x13,
	MATRIX_YUV601F_RGB = 0x14,
	MATRIX_YUV601F_YUV601 = 0x15,
	MATRIX_YUV601F_YUV709 = 0x16,
	MATRIX_YUV601F_YUV709F = 0x17,
	MATRIX_YUV709_RGB = 0x20,
	MATRIX_YUV709_YUV601 = 0x21,
	MATRIX_YUV709_YUV601F = 0x22,
	MATRIX_YUV709_YUV709F = 0x23,
	MATRIX_YUV709F_RGB = 0x24,
	MATRIX_YUV709F_YUV601 = 0x25,
	MATRIX_YUV709F_YUV709 = 0x26,
	MATRIX_BT2020YUV_BT2020RGB = 0x40,
	MATRIX_BT2020RGB_709RGB,
	MATRIX_BT2020RGB_CUSRGB,
};

extern void mtx_setting(enum vpp_matrix_e mtx_sel,
	enum mtx_csc_e mtx_csc,
	int mtx_on);

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define _VSYNC_WR_MPEG_REG(adr, val) WRITE_VCBUS_REG(adr, val)
#define _VSYNC_RD_MPEG_REG(adr) READ_VCBUS_REG(adr)
#define _VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
			WRITE_VCBUS_REG_BITS(adr, val, start, len)
#else
extern int _VSYNC_WR_MPEG_REG_BITS(u32 adr,
			u32 val, u32 start, u32 len);
extern u32 _VSYNC_RD_MPEG_REG(u32 adr);
extern int _VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif
