/*
 * drivers/amlogic/media/vin/tvin/tvin_global.h
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

#ifndef __TVIN_GLOBAL_H
#define __TVIN_GLOBAL_H

/* #include <mach/io.h> */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>

#ifdef TVBUS_REG_ADDR
#define R_APB_REG(reg) aml_read_reg32(TVBUS_REG_ADDR(reg))
#define W_APB_REG(reg, val) aml_write_reg32(TVBUS_REG_ADDR(reg), val)
#define R_VBI_APB_REG(reg) aml_read_reg32(TVBUS_REG_ADDR(reg))
#define W_VBI_APB_REG(reg, val) aml_write_reg32(TVBUS_REG_ADDR(reg), val)
#define R_APB_BIT(reg, start, len) \
	aml_get_reg32_bits(TVBUS_REG_ADDR(reg), start, len)
#define W_APB_BIT(reg, val, start, len) \
	aml_set_reg32_bits(TVBUS_REG_ADDR(reg), val, start, len)
#define W_VBI_APB_BIT(reg, val, start, len) \
	aml_set_reg32_bits(TVBUS_REG_ADDR(reg), val, start, len)
#else
#if 1
extern int tvafe_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_vbi_reg_write(unsigned int reg, unsigned int val);
extern int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
extern int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
#else
static void __iomem *tvafe_reg_base;

static int tvafe_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(tvafe_reg_base+reg);
	return 0;
}

static int tvafe_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (tvafe_reg_base+reg));
	return 0;
}
#endif
static inline uint32_t R_APB_REG(uint32_t reg)
{
	unsigned int val;
	tvafe_reg_read(reg, &val);
	return val;
}

static inline void W_APB_REG(uint32_t reg,
				 const uint32_t val)
{
	tvafe_reg_write(reg, val);
}

static inline uint32_t R_VBI_APB_REG(uint32_t reg)
{
	unsigned int val = 0;

	tvafe_vbi_reg_read(reg, &val);
	return val;
}

static inline void W_VBI_APB_REG(uint32_t reg,
				 const uint32_t val)
{
	tvafe_vbi_reg_write(reg, val);
}

static inline void W_VBI_APB_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	W_VBI_APB_REG(reg, ((R_VBI_APB_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline void W_APB_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	W_APB_REG(reg, ((R_APB_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_APB_BIT(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((R_APB_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline void W_VCBUS(uint32_t reg, const uint32_t value)
{
	aml_write_vcbus(reg, value);
}

static inline uint32_t R_VCBUS(uint32_t reg)
{
	return aml_read_vcbus(reg);
}

static inline void W_VCBUS_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_VCBUS_BIT(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((aml_read_vcbus(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline uint32_t R_HIU_REG(uint32_t reg)
{
	unsigned int val;
	tvafe_hiu_reg_read(reg, &val);
	return val;
}

static inline void W_HIU_REG(uint32_t reg,
				 const uint32_t val)
{
	tvafe_hiu_reg_write(reg, val);
}

static inline void W_HIU_BIT(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	W_HIU_REG(reg, ((R_HIU_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t R_HIU_BIT(uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((R_HIU_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

/*
#define R_APB_REG(reg) READ_APB_REG(reg)
#define W_APB_REG(reg, val) WRITE_APB_REG(reg, val)
#define R_APB_BIT(reg, start, len) \
	READ_APB_REG_BITS(reg, start, len)
#define W_APB_BIT(reg, val, start, len) \
	WRITE_APB_REG_BITS(reg, val, start, len)
*/
#endif


static inline uint32_t rd(uint32_t offset,
							uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg+offset);
}

static inline void wr(uint32_t offset,
						uint32_t reg,
				 const uint32_t val)
{
	aml_write_vcbus(reg+offset, val);
}

static inline void wr_bits(uint32_t offset,
							uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	aml_write_vcbus(reg+offset, ((aml_read_vcbus(reg+offset) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t rd_bits(uint32_t offset,
							uint32_t reg,
				    const uint32_t start,
				    const uint32_t len)
{
	uint32_t val;

	val = ((aml_read_vcbus(reg+offset) >> (start)) & ((1L << (len)) - 1));

	return val;
}

/* ************************************************************************* */
/* *** enum definitions ********************************************* */
/* ************************************************************************* */

#define STATUS_ANTI_SHOCKING    3
#define MINIMUM_H_CNT           1400

#define ADC_REG_NUM             112
#define CVD_PART1_REG_NUM       64
#define CVD_PART1_REG_MIN       0x00
#define CVD_PART2_REG_NUM       144
#define CVD_PART2_REG_MIN       0x70
#define CVD_PART3_REG_NUM       7 /* 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa */
#define CVD_PART3_REG_0         0x87
#define CVD_PART3_REG_1         0x93
#define CVD_PART3_REG_2         0x94
#define CVD_PART3_REG_3         0x95
#define CVD_PART3_REG_4         0x96
#define CVD_PART3_REG_5         0xe6
#define CVD_PART3_REG_6         0xfa

/* #define ACD_REG_NUM1            0x32  //0x00-0x32 except 0x1E&0x31 */
/* #define ACD_REG_NUM2            0x39  //the sum of the part2 acd register */
#define ACD_REG_NUM            0xff/* the sum all of the acd register */
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
#define CRYSTAL_24M
/* #endif */
#ifndef CRYSTAL_24M
#define CRYSTAL_25M
#endif

#ifdef CRYSTAL_24M
#define CVD2_CHROMA_DTO_NTSC_M   0x262e8ba2
#define CVD2_CHROMA_DTO_NTSC_443 0x2f4abc24
#define CVD2_CHROMA_DTO_PAL_I    0x2f4abc24
#define CVD2_CHROMA_DTO_PAL_M    0x2623cd98
#define CVD2_CHROMA_DTO_PAL_CN   0x263566cf
#define CVD2_CHROMA_DTO_PAL_60   0x2f4abc24
#define CVD2_CHROMA_DTO_SECAM    0x2db7a328
#define CVD2_HSYNC_DTO_NTSC_M    0x24000000
#define CVD2_HSYNC_DTO_NTSC_443  0x24000000
#define CVD2_HSYNC_DTO_PAL_I     0x24000000
#define CVD2_HSYNC_DTO_PAL_M     0x24000000
#define CVD2_HSYNC_DTO_PAL_CN    0x24000000
#define CVD2_HSYNC_DTO_PAL_60    0x24000000
#define CVD2_HSYNC_DTO_SECAM     0x24000000
#define CVD2_DCRESTORE_ACCUM     0x98       /* [5:0] = 24(MHz) */
#endif

#ifdef CRYSTAL_25M
#define CVD2_CHROMA_DTO_NTSC_M   0x24a7904a
#define CVD2_CHROMA_DTO_NTSC_443 0x2d66772d
#define CVD2_CHROMA_DTO_PAL_I    0x2d66772d
#define CVD2_CHROMA_DTO_PAL_M    0x249d4040
#define CVD2_CHROMA_DTO_PAL_CN   0x24ae2541
#define CVD2_CHROMA_DTO_PAL_60   0x2d66772d
#define CVD2_CHROMA_DTO_SECAM    0x2be37de9
#define CVD2_HSYNC_DTO_NTSC_M    0x228f5c28
#define CVD2_HSYNC_DTO_NTSC_443  0x228f5c28
#define CVD2_HSYNC_DTO_PAL_I     0x228f5c28
#define CVD2_HSYNC_DTO_PAL_M     0x228f5c28
#define CVD2_HSYNC_DTO_PAL_CN    0x228f5c28
#define CVD2_HSYNC_DTO_PAL_60    0x228f5c28
#define CVD2_HSYNC_DTO_SECAM     0x228f5c28
#define CVD2_DCRESTORE_ACCUM     0x99       /* [5:0] = 25(MHz) */
#endif

#define TVAFE_SET_CVBS_PGA_EN
#ifdef TVAFE_SET_CVBS_PGA_EN
#define TVAFE_SET_CVBS_PGA_START    5
#define TVAFE_SET_CVBS_PGA_STEP     1
#define CVD2_DGAIN_MIDDLE           0x0200
#define CVD2_DGAIN_WINDOW           0x000F
#define CVD2_DGAIN_LIMITH (CVD2_DGAIN_MIDDLE + CVD2_DGAIN_WINDOW)
#define CVD2_DGAIN_LIMITL (CVD2_DGAIN_MIDDLE - CVD2_DGAIN_WINDOW)
#define CVD2_DGAIN_MAX		    0x0600
#define CVD2_DGAIN_MIN		    0x0100
#define PGA_DELTA_VAL			0x10
#endif

#define TVAFE_SET_CVBS_CDTO_EN
#ifdef TVAFE_SET_CVBS_CDTO_EN
#define TVAFE_SET_CVBS_CDTO_START   300
#define TVAFE_SET_CVBS_CDTO_STEP    0
#define HS_CNT_STANDARD             0x31380	/*0x17a00*/
#define CDTO_FILTER_FACTOR			1
#endif

enum tvin_sync_pol_e {
	TVIN_SYNC_POL_NULL = 0,
	TVIN_SYNC_POL_NEGATIVE,
	TVIN_SYNC_POL_POSITIVE,
};

enum tvin_color_space_e {
	TVIN_CS_RGB444 = 0,
	TVIN_CS_YUV444,
	TVIN_CS_YUV422_16BITS,
	TVIN_CS_YCbCr422_8BITS,
	TVIN_CS_MAX
};
/*vdin buffer control for tvin frontend*/
enum tvin_buffer_ctl_e {
	TVIN_BUF_NULL,
	TVIN_BUF_SKIP,
	TVIN_BUF_TMP,
	TVIN_BUF_RECYCLE_TMP,
};


/* ************************************************************************* */
/* *** structure definitions ********************************************* */
/* ************************************************************************* */
/* Hs_cnt        Pixel_Clk(Khz/10) */

struct tvin_format_s {
	/* Th in the unit of pixel */
	unsigned short         h_active;
	 /* Tv in the unit of line */
	unsigned short         v_active;
	/* Th in the unit of T, while 1/T = 24MHz or 27MHz or even 100MHz */
	unsigned short         h_cnt;
	/* Tolerance of h_cnt */
	unsigned short         h_cnt_offset;
	/* Tolerance of v_cnt */
	unsigned short         v_cnt_offset;
	/* Ths in the unit of T, while 1/T = 24MHz or 27MHz or even 100MHz */
	unsigned short         hs_cnt;
	/* Tolerance of hs_cnt */
	unsigned short         hs_cnt_offset;
	/* Th in the unit of pixel */
	unsigned short         h_total;
	/* Tv in the unit of line */
	unsigned short         v_total;
	/* h front proch */
	unsigned short         hs_front;
	 /* HS in the unit of pixel */
	unsigned short         hs_width;
	 /* HS in the unit of pixel */
	unsigned short         hs_bp;
	/* vs front proch in the unit of line */
	unsigned short         vs_front;
	 /* VS width in the unit of line */
	unsigned short         vs_width;
	/* vs back proch in the unit of line */
	unsigned short         vs_bp;
	enum tvin_sync_pol_e   hs_pol;
	enum tvin_sync_pol_e   vs_pol;
	enum tvin_scan_mode_e  scan_mode;
	/* (Khz/10) */
	unsigned short         pixel_clk;
	unsigned short         vbi_line_start;
	unsigned short         vbi_line_end;
	unsigned int           duration;
};

enum tvin_ar_b3_b0_val_e {
	TVIN_AR_14x9_LB_CENTER_VAL = 0x11,
	TVIN_AR_14x9_LB_TOP_VAL = 0x12,
	TVIN_AR_16x9_LB_TOP_VAL = 0x14,
	TVIN_AR_16x9_FULL_VAL = 0x17,
	TVIN_AR_4x3_FULL_VAL = 0x18,
	TVIN_AR_16x9_LB_CENTER_VAL = 0x1b,
	TVIN_AR_16x9_LB_CENTER1_VAL = 0x1d,
	TVIN_AR_14x9_FULL_VAL = 0x1e,
};

enum tvin_aspect_ratio_e {
	TVIN_ASPECT_NULL = 0,
	TVIN_ASPECT_1x1,
	TVIN_ASPECT_4x3_FULL,
	TVIN_ASPECT_14x9_FULL,
	TVIN_ASPECT_14x9_LB_CENTER,
	TVIN_ASPECT_14x9_LB_TOP,
	TVIN_ASPECT_16x9_FULL,
	TVIN_ASPECT_16x9_LB_CENTER,
	TVIN_ASPECT_16x9_LB_TOP,
	TVIN_ASPECT_MAX,
};

const char *tvin_aspect_ratio_str(enum tvin_aspect_ratio_e aspect_ratio);

enum tvin_hdr_eotf_e {
	EOTF_SDR,
	EOTF_HDR,
	EOTF_SMPTE_ST_2048,
	EOTF_HLG,
	EOTF_MAX,
};

enum tvin_hdr_state_e {
	HDR_STATE_NULL,
	HDR_STATE_GET,
	HDR_STATE_SET,
};

struct tvin_hdr_property_s {
	unsigned int x;/* max */
	unsigned int y;/* min */
};

struct tvin_hdr_data_s {
	enum tvin_hdr_eotf_e eotf:8;
	unsigned char metadata_id;
	unsigned char length;
	unsigned char reserved;
	struct tvin_hdr_property_s primaries[3];
	struct tvin_hdr_property_s white_points;
	struct tvin_hdr_property_s master_lum;/* max min lum */
	unsigned int mcll;
	unsigned int mfall;
};

struct tvin_hdr_info_s {
	struct tvin_hdr_data_s hdr_data;
	enum tvin_hdr_state_e hdr_state;
	unsigned int hdr_check_cnt;
};

enum tvin_cn_type_e {
	GRAPHICS,
	PHOTO,
	CINEMA,
	GAME,
};

struct tvin_latency_s {
	bool allm_mode;
	bool it_content;
	enum tvin_cn_type_e cn_type;
};

struct tvin_sig_property_s {
	enum tvin_trans_fmt	trans_fmt;
	enum tvin_color_fmt_e	color_format;
	/* for vdin matrix destination color fmt */
	enum tvin_color_fmt_e	dest_cfmt;
	enum tvin_aspect_ratio_e	aspect_ratio;
	unsigned int		dvi_info;
	unsigned short		scaling4h;	/* for vscaler */
	unsigned short		scaling4w;	/* for hscaler */
	unsigned int		hs;	/* for horizontal start cut window */
	unsigned int		he;	/* for horizontal end cut window */
	unsigned int		vs;	/* for vertical start cut window */
	unsigned int		ve;	/* for vertical end cut window */
	unsigned int		pre_vs;	/* for vertical start cut window */
	unsigned int		pre_ve;	/* for vertical end cut window */
	unsigned int		pre_hs;	/* for horizontal start cut window */
	unsigned int		pre_he;	/* for horizontal end cut window */
	unsigned int		decimation_ratio;	/* for decimation */
	unsigned int		colordepth; /* for color bit depth */
	unsigned int		vdin_hdr_Flag;
	enum tvin_color_fmt_range_e color_fmt_range;
	struct tvin_hdr_info_s hdr_info;
	bool dolby_vision;/*is signal dolby version*/
	bool low_latency;/*is low latency dolby mode*/
	uint8_t fps;
	unsigned int skip_vf_num;/*skip pre vframe num*/
	struct tvin_latency_s latency;
};

#define TVAFE_VF_POOL_SIZE			6 /* 8 */
#define VDIN_VF_POOL_MAX_SIZE		6 /* 8 */
#define TVHDMI_VF_POOL_SIZE			6 /* 8 */

#define BT656IN_ANCI_DATA_SIZE		0x4000 /* save anci data from bt656in */
#define CAMERA_IN_ANCI_DATA_SIZE	0x4000 /* save anci data from bt656in */


#endif /* __TVIN_GLOBAL_H */

