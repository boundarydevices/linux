/*
 * drivers/amlogic/media/enhancement/amvecm/amve.c
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

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "amve.h"
#include "amve_gamma_table.h"
#include <linux/io.h>
#include "dnlp_cal.h"

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (amve_debug&0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\
/* #define pr_amve_error(fmt, args...) */
/* printk(KERN_##(KERN_INFO) "AMVECM: " fmt, ## args) */

#define GAMMA_RETRY        1000

/* 0: Invalid */
/* 1: Valid */
/* 2: Updated in 2D mode */
/* 3: Updated in 3D mode */
unsigned long flags;

/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESONG9TV) */
#define NEW_DNLP_IN_SHARPNESS 2
#define NEW_DNLP_IN_VPP 1

unsigned int dnlp_sel = NEW_DNLP_IN_SHARPNESS;
module_param(dnlp_sel, int, 0664);
MODULE_PARM_DESC(dnlp_sel, "dnlp_sel");
/* #endif */

static int amve_debug;
module_param(amve_debug, int, 0664);
MODULE_PARM_DESC(amve_debug, "amve_debug");

struct ve_hist_s video_ve_hist;

unsigned int vpp_log[128][10];

struct tcon_gamma_table_s video_gamma_table_r;
struct tcon_gamma_table_s video_gamma_table_g;
struct tcon_gamma_table_s video_gamma_table_b;
struct tcon_gamma_table_s video_gamma_table_r_adj;
struct tcon_gamma_table_s video_gamma_table_g_adj;
struct tcon_gamma_table_s video_gamma_table_b_adj;
struct tcon_gamma_table_s video_gamma_table_ioctl_set;

struct tcon_rgb_ogo_s video_rgb_ogo = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};

#define FLAG_LVDS_FREQ_SW1       (1 <<  6)

int dnlp_en;/* 0:disabel;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");
static int dnlp_status = 1;/* 0:done;1:todo */

int dnlp_en_2;/* 0:disabel;1:enable */
module_param(dnlp_en_2, int, 0664);
MODULE_PARM_DESC(dnlp_en_2, "\n enable or disable dnlp\n");

static int frame_lock_freq;
module_param(frame_lock_freq, int, 0664);
MODULE_PARM_DESC(frame_lock_freq, "frame_lock_50");

static int video_rgb_ogo_mode_sw;
module_param(video_rgb_ogo_mode_sw, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_mode_sw,
		"enable/disable video_rgb_ogo_mode_sw");

int video_rgb_ogo_xvy_mtx;
module_param(video_rgb_ogo_xvy_mtx, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_xvy_mtx,
		"enable/disable video_rgb_ogo_xvy_mtx");

int video_rgb_ogo_xvy_mtx_latch;

static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */

/* 3d sync parts begin */
unsigned int sync_3d_h_start;
unsigned int sync_3d_h_end;
unsigned int sync_3d_v_start = 10;
unsigned int sync_3d_v_end = 20;
unsigned int sync_3d_polarity;
unsigned int sync_3d_out_inv;
unsigned int sync_3d_black_color = 0x008080;/* yuv black */
/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo;
/* 3d sync parts end */

unsigned int contrast_adj_sel;/*0:vdj1, 1:vd1 mtx rgb contrast*/
module_param(contrast_adj_sel, uint, 0664);
MODULE_PARM_DESC(contrast_adj_sel, "\n contrast_adj_sel\n");

/*gxlx adaptive sr level*/
static unsigned int sr_adapt_level;
module_param(sr_adapt_level, uint, 0664);
MODULE_PARM_DESC(sr_adapt_level, "\n sr_adapt_level\n");

/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
static void ve_hist_gamma_tgt(struct vframe_s *vf)
{
	int ave_luma;
	struct vframe_prop_s *p = &vf->prop;

	video_ve_hist.sum    = p->hist.vpp_luma_sum;
	video_ve_hist.width  = p->hist.vpp_width;
	video_ve_hist.height = p->hist.vpp_height;

	video_ve_hist.ave =
		video_ve_hist.sum/(video_ve_hist.height*
				video_ve_hist.width);
	if (((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) &&
		(is_meson_gxtvbb_cpu())) ||
		cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		ave_luma = video_ve_hist.ave;
		ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
		video_ve_hist.ave = ave_luma*255/(235-16);
		if (video_ve_hist.ave > 255)
			video_ve_hist.ave = 255;
	}
}

void ve_hist_gamma_reset(void)
{
	if ((video_ve_hist.height != 0) ||
		(video_ve_hist.width != 0))
		memset(&video_ve_hist, 0, sizeof(struct ve_hist_s));
}

void ve_dnlp_load_reg(void)
{
	int i;

	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
					ve_dnlp_reg[i]);
		} else if (is_meson_tl1_cpu()) {
			for (i = 0; i < 32; i++)
				WRITE_VPP_REG(SHARP1_DNLP_00 + i,
					ve_dnlp_reg_v2[i]);
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
					ve_dnlp_reg[i]);
		}
	} else {
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
				ve_dnlp_reg[i]);
	}

}

static void ve_dnlp_load_def_reg(void)
{
	int i;

	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
					ve_dnlp_reg[i]);
		} else if (is_meson_tl1_cpu()) {
			for (i = 0; i < 32; i++)
				WRITE_VPP_REG(SHARP1_DNLP_00 + i,
					ve_dnlp_reg_v2[i]);
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
					ve_dnlp_reg[i]);
		}
	} else {
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
				ve_dnlp_reg_def[i]);
	}
}

void ve_on_vs(struct vframe_s *vf)
{

	if (dnlp_en_2 || ve_en) {
		/* calculate dnlp target data */
		if (ve_dnlp_calculate_tgtx(vf)) {
			/* calculate dnlp low-pass-filter data */
			ve_dnlp_calculate_lpf();
			/* calculate dnlp reg data */
			ve_dnlp_calculate_reg();
			/* load dnlp reg data */
			ve_dnlp_load_reg();
		}
	}
	ve_hist_gamma_tgt(vf);

	/* sharpness process */
	sharpness_process(vf);

	/* */
#if 0
	/* comment for duration algorithm is not based on panel vsync */
	if (vf->prop.meas.vs_cycle && !frame_lock_nosm) {
		if ((vecm_latch_flag & FLAG_LVDS_FREQ_SW1) &&
		(vf->duration >= 1920 - 19) && (vf->duration <= 1920 + 19))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				true,
				lock_range_50hz_fast,
				lock_range_50hz_slow);
		if ((!(vecm_latch_flag & FLAG_LVDS_FREQ_SW1)) &&
		(vf->duration >= 1600 - 5) && (vf->duration <= 1600 + 13))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				false,
				lock_range_60hz_fast,
				lock_range_60hz_slow);
	}
#endif
}

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */

void vpp_enable_lcd_gamma_table(void)
{
	VSYNC_WR_MPEG_REG_BITS(L_GAMMA_CNTL_PORT, 1, GAMMA_EN, 1);
}

void vpp_disable_lcd_gamma_table(void)
{
	VSYNC_WR_MPEG_REG_BITS(L_GAMMA_CNTL_PORT, 0, GAMMA_EN, 1);
}

void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				0, GAMMA_EN, 1);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	VSYNC_WR_MPEG_REG_BITS(L_GAMMA_CNTL_PORT,
					gamma_en, GAMMA_EN, 1);

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

u16 gamma_data_r[256] = {0};
u16 gamma_data_g[256] = {0};
u16 gamma_data_b[256] = {0};
void vpp_get_lcd_gamma_table(u32 rgb_mask)
{
	int i;
	int cnt = 0;

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;
	pr_info("read gamma begin\n");
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	for (i = 0; i < 256; i++) {
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_RD) |
						(0x0 << H_AUTO_INC) |
						(0x1 << rgb_mask)	|
						(i << HADR));

		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << RD_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		if (rgb_mask == H_SEL_R)
			gamma_data_r[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_G)
			gamma_data_g[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_B)
			gamma_data_b[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));
	pr_info("read gamma over\n");
}
void amve_write_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

#define COEFF_NORM(a) ((int)((((a) * 2048.0) + 1) / 2))
#define MATRIX_5x3_COEF_SIZE 24

static int RGB709_to_YUV709l_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.181873),	COEFF_NORM(0.611831),	COEFF_NORM(0.061765),
	COEFF_NORM(-0.100251),	COEFF_NORM(-0.337249),	COEFF_NORM(0.437500),
	COEFF_NORM(0.437500),	COEFF_NORM(-0.397384),	COEFF_NORM(-0.040116),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	64, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static int bypass_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p)
{
	int m[24];
	int i;
	struct vinfo_s *vinfo = get_current_vinfo();
	/* write to registers */
	if (video_rgb_ogo_xvy_mtx) {
		if (video_rgb_ogo_xvy_mtx_latch & MTX_BYPASS_RGB_OGO) {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_BYPASS_RGB_OGO;
		} else if (video_rgb_ogo_xvy_mtx_latch & MTX_RGB2YUVL_RGB_OGO) {
			memcpy(m, RGB709_to_YUV709l_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_RGB2YUVL_RGB_OGO;
		} else
			memcpy(m, bypass_coeff, sizeof(int) * 24);

		m[3] = p->r_gain * m[3] / COEFF_NORM(1.0);
		m[4] = p->g_gain * m[4] / COEFF_NORM(1.0);
		m[5] = p->b_gain * m[5] / COEFF_NORM(1.0);
		m[6] = p->r_gain * m[6] / COEFF_NORM(1.0);
		m[7] = p->g_gain * m[7] / COEFF_NORM(1.0);
		m[8] = p->b_gain * m[8] / COEFF_NORM(1.0);
		m[9] = p->r_gain * m[9] / COEFF_NORM(1.0);
		m[10] = p->g_gain * m[10] / COEFF_NORM(1.0);
		m[11] = p->b_gain * m[11] / COEFF_NORM(1.0);

		if (vinfo->viu_color_fmt == COLOR_FMT_RGB444) {
			m[18] = (p->r_pre_offset + m[18] + 1024)
				* p->r_gain / COEFF_NORM(1.0)
				- p->r_gain + p->r_post_offset;
			m[19] = (p->g_pre_offset + m[19] + 1024)
				* p->g_gain / COEFF_NORM(1.0)
				- p->g_gain + p->g_post_offset;
			m[20] = (p->b_pre_offset + m[20] + 1024)
				* p->b_gain / COEFF_NORM(1.0)
				- p->b_gain + p->b_post_offset;
		} else {
			m[0] = p->r_gain * p->r_pre_offset / COEFF_NORM(1.0) +
				p->r_post_offset;
			m[1] = p->g_gain * p->g_pre_offset / COEFF_NORM(1.0) +
				p->g_post_offset;
			m[2] = p->b_gain * p->b_pre_offset / COEFF_NORM(1.0) +
				p->b_post_offset;
		}

		for (i = 18; i < 21; i++) {
			if (m[i] > 1023)
				m[i] = 1023;
			if (m[i] < -1024)
				m[i] = -1024;
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_EN_CTRL,
				p->en, 0, 1);
			WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET0_1,
				((m[0] & 0xfff) << 16)
				| (m[1] & 0xfff));
			WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET2,
				m[2] & 0xfff);
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF00_01,
				((m[3] & 0x1fff) << 16)
				| (m[4] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF02_10,
				((m[5]	& 0x1fff) << 16)
				| (m[6] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF11_12,
				((m[7] & 0x1fff) << 16)
				| (m[8] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF20_21,
				((m[9] & 0x1fff) << 16)
				| (m[10] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF22,
				m[11] & 0x1fff);
			if (m[21]) {
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF13_14,
					((m[12] & 0x1fff) << 16)
					| (m[13] & 0x1fff));
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF15_25,
					((m[14] & 0x1fff) << 16)
					| (m[17] & 0x1fff));
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF23_24,
					((m[15] & 0x1fff) << 16)
					| (m[16] & 0x1fff));
			}
			WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET0_1,
				((m[18] & 0xfff) << 16)
				| (m[19] & 0xfff));
			WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET2,
				m[20] & 0xfff);
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
				m[21], 3, 2);
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
				m[22], 5, 3);
			return;
		}

		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, p->en, 6, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);

		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
			((m[0] & 0xfff) << 16)
			| (m[1] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
			m[2] & 0xfff);
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
			((m[3] & 0x1fff) << 16)
			| (m[4] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
			((m[5]	& 0x1fff) << 16)
			| (m[6] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
			((m[7] & 0x1fff) << 16)
			| (m[8] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
			((m[9] & 0x1fff) << 16)
			| (m[10] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF22,
			m[11] & 0x1fff);
		if (m[21]) {
			WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
				((m[12] & 0x1fff) << 16)
				| (m[13] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
				((m[14] & 0x1fff) << 16)
				| (m[17] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
				((m[15] & 0x1fff) << 16)
				| (m[16] & 0x1fff));
		}
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
			((m[18] & 0xfff) << 16)
			| (m[19] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
			m[20] & 0xfff);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[21], 3, 2);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[22], 5, 3);
	} else {
		/*for txlx and txhd, pre_offset and post_offset become 13 bit*/
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				((p->en << 31) & 0x80000000) |
				((p->r_gain << 16) & 0x07ff0000) |
				((p->g_gain <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				((p->b_gain << 16) & 0x07ff0000) |
				((p->r_post_offset <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
				((p->g_post_offset << 16) & 0x1fff0000) |
				((p->b_post_offset <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
				((p->r_pre_offset  << 16) & 0x1fff0000) |
				((p->g_pre_offset  <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
				((p->b_pre_offset  <<  0) & 0x00001fff));
		} else {
		/*txl and before txl, and tl1 10bit path offset is 11bit*/
			WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				((p->en << 31) & 0x80000000) |
				((p->r_gain << 16) & 0x07ff0000) |
				((p->g_gain <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				((p->b_gain << 16) & 0x07ff0000) |
				((p->r_post_offset <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
				((p->g_post_offset << 16) & 0x07ff0000) |
				((p->b_post_offset <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
				((p->r_pre_offset  << 16) & 0x07ff0000) |
				((p->g_pre_offset  <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
				((p->b_pre_offset  <<  0) & 0x000007ff));
		}
	}
}

void ve_enable_dnlp(void)
{
	ve_en = 1;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
			WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 1, 0, 1);
		else if (is_meson_tl1_cpu())
			WRITE_VPP_REG_BITS(SHARP1_DNLP_EN, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 1, 0, 1);
	} else
		/* #endif */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				1, DNLP_EN_BIT, DNLP_EN_WID);
}

void ve_disable_dnlp(void)
{
	ve_en = 0;
	if (dnlp_sel == NEW_DNLP_IN_SHARPNESS)
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
			WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 0, 0, 1);
		else if (is_meson_tl1_cpu())
			WRITE_VPP_REG_BITS(SHARP1_DNLP_EN, 0, 0, 1);
		else
			WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 0, 0, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				0, DNLP_EN_BIT, DNLP_EN_WID);
}

void ve_set_dnlp_2(void)
{
	ulong i = 0;
	/* clear historic luma sum */
	if (dnlp_insmod_ok == 0)
		return;
	*ve_dnlp_luma_sum_copy = 0;
	/* init tgt & lpf */
	for (i = 0; i < 64; i++) {
		ve_dnlp_tgt_copy[i] = i << 2;
		ve_dnlp_lpf[i] = (ulong)ve_dnlp_tgt_copy[i] << ve_dnlp_rt;
	}
	/* calculate dnlp reg data */
	ve_dnlp_calculate_reg();
	/* load dnlp reg data */
	/*ve_dnlp_load_reg();*/
	ve_dnlp_load_def_reg();
}

unsigned int ve_get_vs_cnt(void)
{
	return READ_VPP_REG(VPP_VDO_MEAS_VS_COUNT_LO);
}

void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool         lock50,
			  unsigned int range_fast,
			  unsigned int range_slow)
{
	unsigned int vtotal_ori = READ_VPP_REG(ENCL_VIDEO_MAX_LNCNT);
	unsigned int vtotal     = lock50 ? 1349 : 1124;
	unsigned int stamp_in   = READ_VPP_REG(VDIN_MEAS_VS_COUNT_LO);
	unsigned int stamp_out  = ve_get_vs_cnt();
	unsigned int phase      = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	int step = 0, i = 0;
	/* get phase */
	if (stamp_out < stamp)
		phase = 0xffffffff - stamp + stamp_out + 1;
	else
		phase = stamp_out - stamp;
	while (phase >= cycle)
		phase -= cycle;
	/* 225~315 degree => tune fast panel output */
	if ((phase > ((cycle * 5) >> 3)) && (phase < ((cycle * 7) >> 3))) {
		vtotal -= range_slow;
		step = 1;
	} else if ((phase > (cycle >> 3)) && (phase < ((cycle * 3) >> 3))) {
		/* 45~135 degree => tune slow panel output */
		vtotal += range_slow;
		step = -1;
	} else if (phase >= ((cycle * 7) >> 3)) {
		/* 315~360 degree => tune fast panel output */
		vtotal -= range_fast;
		step = + 2;
	} else if (phase <= (cycle >> 3)) {
		/* 0~45 degree => tune slow panel output */
		vtotal += range_fast;
		step = -2;
	} else {/* 135~225 degree => keep still */
		vtotal = vtotal_ori;
		step = 0;
	}
	if (vtotal != vtotal_ori)
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, vtotal);
	if (cnt) {
		cnt--;
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, cnt); */
		assist_cnt = cnt;
		if (cnt) {
			vpp_log[cnt][0] = stamp;
			vpp_log[cnt][1] = stamp_in;
			vpp_log[cnt][2] = stamp_out;
			vpp_log[cnt][3] = cycle;
			vpp_log[cnt][4] = phase;
			vpp_log[cnt][5] = vtotal;
			vpp_log[cnt][6] = step;
		} else {
			for (i = 127; i > 0; i--) {
				pr_amve_dbg("Ti=%10u Tio=%10u To=%10u CY=%6u ",
					vpp_log[i][0],
					vpp_log[i][1],
					vpp_log[i][2],
					vpp_log[i][3]);
				pr_amve_dbg("PH =%10u Vt=%4u S=%2d\n",
					vpp_log[i][4],
					vpp_log[i][5],
					vpp_log[i][6]);
			}
		}
	}

}

void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height|(width << 16);

	if (READ_VPP_REG(VPP_VE_H_V_SIZE) != vpp_size)
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
}

void ve_dnlp_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_NEW_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_NEW_DNLP;
		ve_set_v3_dnlp(&dnlp_curve_param_load);
	}
	if (dnlp_en && dnlp_status) {
		dnlp_status = 0;
		ve_set_dnlp_2();
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	} else if (dnlp_en == 0) {
		dnlp_status = 1;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
}

void ve_lcd_gamma_process(void)
{
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_EN) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_EN;
		vpp_enable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_enable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_DIS) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_DIS;
		vpp_disable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_disable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_R) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
		vpp_set_lcd_gamma_table(video_gamma_table_r.data, H_SEL_R);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_G) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
		vpp_set_lcd_gamma_table(video_gamma_table_g.data, H_SEL_G);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_B) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
		vpp_set_lcd_gamma_table(video_gamma_table_b.data, H_SEL_B);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_RGB_OGO) {
		vecm_latch_flag &= ~FLAG_RGB_OGO;
		if (video_rgb_ogo_mode_sw) {
			if (video_rgb_ogo.en) {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r_adj.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g_adj.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b_adj.data,
						H_SEL_B);
			} else {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b.data,
						H_SEL_B);
			}
			pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
		} else {
			vpp_set_rgb_ogo(&video_rgb_ogo);
			pr_amve_dbg("\n[amve..] set vpp_set_rgb_ogo OK!!!\n");
		}
	}
}
void lvds_freq_process(void)
{
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TV)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TVD)) */
/*  lvds freq 50Hz/60Hz */
/* if (frame_lock_freq == 1){//50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* } */
/* else if (frame_lock_freq == 2){//60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if(READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* else if (frame_lock_freq == 0){ */
/*  lvds freq 50Hz/60Hz */
/* if (vecm_latch_flag & FLAG_LVDS_FREQ_SW){  //50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* }else{	 //60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* } */
/* #endif */
}

void ve_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_DNLP;
}

void ve_new_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_NEW_DNLP;
}

static void video_data_limitation(int *val)
{
	if (*val > 1023)
		*val = 1023;
	if (*val < 0)
		*val = 0;
}

static void video_lookup(struct tcon_gamma_table_s *tbl, int *val)
{
	unsigned int idx = (*val) >> 2, mod = (*val) & 3;

	if (idx < 255)
		*val = tbl->data[idx] +
		(((tbl->data[idx + 1] - tbl->data[idx]) * mod + 2) >> 2);
	else
		*val = tbl->data[idx] +
		(((1023 - tbl->data[idx]) * mod + 2) >> 2);
}

static void video_set_rgb_ogo(void)
{
	int i = 0, r = 0, g = 0, b = 0;

	for (i = 0; i < 256; i++) {
		r = video_curve_2d2.data[i];
		g = video_curve_2d2.data[i];
		b = video_curve_2d2.data[i];
		/* Pre_offset */
		r += video_rgb_ogo.r_pre_offset;
		g += video_rgb_ogo.g_pre_offset;
		b += video_rgb_ogo.b_pre_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Gain */
		r  *= video_rgb_ogo.r_gain;
		r >>= 10;
		g  *= video_rgb_ogo.g_gain;
		g >>= 10;
		b  *= video_rgb_ogo.b_gain;
		b >>= 10;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Post_offset */
		r += video_rgb_ogo.r_post_offset;
		g += video_rgb_ogo.g_post_offset;
		b += video_rgb_ogo.b_post_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		video_lookup(&video_curve_2d2_inv, &r);
		video_lookup(&video_curve_2d2_inv, &g);
		video_lookup(&video_curve_2d2_inv, &b);
		/* Get gamma_ogo = curve_2d2_inv_ogo * gamma */
		video_lookup(&video_gamma_table_r, &r);
		video_lookup(&video_gamma_table_g, &g);
		video_lookup(&video_gamma_table_b, &b);
		/* Save gamma_ogo */
		video_gamma_table_r_adj.data[i] = r;
		video_gamma_table_g_adj.data[i] = g;
		video_gamma_table_b_adj.data[i] = b;
	}
}

void ve_ogo_param_update(void)
{
	if (video_rgb_ogo.en > 1)
		video_rgb_ogo.en = 1;
	if (video_rgb_ogo.r_pre_offset > 1023)
		video_rgb_ogo.r_pre_offset = 1023;
	if (video_rgb_ogo.r_pre_offset < -1024)
		video_rgb_ogo.r_pre_offset = -1024;
	if (video_rgb_ogo.g_pre_offset > 1023)
		video_rgb_ogo.g_pre_offset = 1023;
	if (video_rgb_ogo.g_pre_offset < -1024)
		video_rgb_ogo.g_pre_offset = -1024;
	if (video_rgb_ogo.b_pre_offset > 1023)
		video_rgb_ogo.b_pre_offset = 1023;
	if (video_rgb_ogo.b_pre_offset < -1024)
		video_rgb_ogo.b_pre_offset = -1024;
	if (video_rgb_ogo.r_gain > 2047)
		video_rgb_ogo.r_gain = 2047;
	if (video_rgb_ogo.g_gain > 2047)
		video_rgb_ogo.g_gain = 2047;
	if (video_rgb_ogo.b_gain > 2047)
		video_rgb_ogo.b_gain = 2047;
	if (video_rgb_ogo.r_post_offset > 1023)
		video_rgb_ogo.r_post_offset = 1023;
	if (video_rgb_ogo.r_post_offset < -1024)
		video_rgb_ogo.r_post_offset = -1024;
	if (video_rgb_ogo.g_post_offset > 1023)
		video_rgb_ogo.g_post_offset = 1023;
	if (video_rgb_ogo.g_post_offset < -1024)
		video_rgb_ogo.g_post_offset = -1024;
	if (video_rgb_ogo.b_post_offset > 1023)
		video_rgb_ogo.b_post_offset = 1023;
	if (video_rgb_ogo.b_post_offset < -1024)
		video_rgb_ogo.b_post_offset = -1024;
	if (video_rgb_ogo_mode_sw)
		video_set_rgb_ogo();

	vecm_latch_flag |= FLAG_RGB_OGO;
}

/* sharpness process begin */
void sharpness_process(struct vframe_s *vf)
{
	return;
}
/* sharpness process end */

/*for gxbbtv rgb contrast adj in vd1 matrix */
void vpp_vd1_mtx_rgb_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int con_minus_value, rgb_con_en;

	if ((cont_val > 1023) || (cont_val < -1024))
		return;
	cont_val = cont_val + 1024;
	/*close rgb contrast protect*/
	WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 0, 1);
	/*VPP_VADJ_CTRL bit 1 on for rgb contrast adj*/
	rgb_con_en = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1);
	if (!rgb_con_en)
		WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1, 1);

	/*select full or limit range setting*/
	con_minus_value = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 4, 10);
	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (con_minus_value != 64)
			WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 64, 4, 10);
	} else {
		if (con_minus_value != 0)
			WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 4, 10);
	}

	vd1_contrast = (READ_VPP_REG(XVYCC_VD1_RGB_CTRST) & 0xf000ffff) |
					(cont_val << 16);

	WRITE_VPP_REG(XVYCC_VD1_RGB_CTRST, vd1_contrast);
}

/*for gxbbtv contrast adj in vadj1*/
void vpp_vd_adj1_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int vdj1_ctl;

	if ((cont_val > 1023) || (cont_val < -1024))
		return;
	cont_val = ((cont_val + 1024) >> 3);
	/*VPP_VADJ_CTRL bit 1 off for contrast adj*/
	vdj1_ctl = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1);
	if (is_meson_gxtvbb_cpu()) {
		if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
			if (!vdj1_ctl)
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1, 1);
		} else {
			if (vdj1_ctl)
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1, 1);
		}
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_TL1) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0x7ff00) |
						(cont_val << 0);
		WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_contrast);
		return;
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x3ff00) |
						(cont_val << 0);
	} else {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x1ff00) |
						(cont_val << 0);
	}
	WRITE_VPP_REG(VPP_VADJ1_Y, vd1_contrast);
}

void vpp_vd_adj1_brightness(signed int bri_val, struct vframe_s *vf)
{
	signed int vd1_brightness;

	signed int ao0 =  -64;
	signed int ao1 = -512;
	signed int ao2 = -512;
	unsigned int a01 =    0, a_2 =    0;
	/* enable vd0_csc */

	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	if (bri_val > 1023 || bri_val < -1024)
		return;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_TL1) {
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0xff) |
			(bri_val << 8);

		WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_brightness);
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		bri_val = bri_val >> 1;
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y) & 0xff) |
			(bri_val << 8);

		WRITE_VPP_REG(VPP_VADJ1_Y, vd1_brightness);
	} else {
		if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_CVBS) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_COMP))
			vd1_brightness = bri_val;
		else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if ((((vf->signal_type >> 29) & 0x1) == 1) &&
				(((vf->signal_type >> 16) & 0xff) == 9)) {
				bri_val += ao0;
				if (bri_val < -1024)
					bri_val = -1024;
				vd1_brightness = bri_val;
			} else
				vd1_brightness = bri_val;
		} else {
			bri_val += ao0;
			if (bri_val < -1024)
				bri_val = -1024;
			vd1_brightness = bri_val;
		}

		a01 = ((vd1_brightness << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<	0) & 0x00000fff);
		/*p01 = ((po0 << 16) & 0x0fff0000) |*/
				/*((po1 <<  0) & 0x00000fff);*/
		/*p_2 = ((po2 <<	0) & 0x00000fff);*/

		WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
		WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
	}
}


/* brightness/contrast adjust process begin */
static void vd1_brightness_contrast(signed int brightness,
		signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu()) {
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
	} else {
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01, gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10, gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12, gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21, gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22, gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2, p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
}

void amvecm_bricon_process(signed int bri_val,
		signed int cont_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_BRI) {
		vecm_latch_flag &= ~FLAG_VADJ1_BRI;
		vpp_vd_adj1_brightness(bri_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_brightness OK!!!\n");
		if (amve_debug&0x100)
			pr_info("\n[amve..]%s :brightness:%d!!!\n",
				__func__, bri_val);
	}

	if (vecm_latch_flag & FLAG_VADJ1_CON) {
		vecm_latch_flag &= ~FLAG_VADJ1_CON;
		if (contrast_adj_sel)
			vpp_vd1_mtx_rgb_contrast(cont_val, vf);
		else
			vpp_vd_adj1_contrast(cont_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_contrast OK!!!\n");
		if (amve_debug&0x100)
			pr_info("\n[amve..]%s :contrast:%d!!!\n",
				__func__, cont_val);
	}

	if (0) { /* vecm_latch_flag & FLAG_BRI_CON) { */
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(bri_val, cont_val);
		pr_amve_dbg("\n[amve..] set vd1_brightness_contrast OK!!!\n");
	}
}
/* brightness/contrast adjust process end */

void amvecm_color_process(signed int sat_val,
		signed int hue_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_COLOR) {
		vecm_latch_flag &= ~FLAG_VADJ1_COLOR;
		vpp_vd_adj1_saturation_hue(sat_val, hue_val, vf);
		if (amve_debug&0x100)
			pr_info("\n[amve..]%s :saturation:%d,hue:%d!!!\n",
				__func__, sat_val, hue_val);
	}
}
/* saturation/hue adjust process end */

/* 3d process begin */
void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				sync_3d_black_color&0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}
void amvecm_3d_sync_process(void)
{

	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}
/* 3d process end */

/*gxlx sr adaptive param*/
#define SR_SD_SCALE_LEVEL 0x1
#define SR_HD_SCALE_LEVEL 0x2
#define SR_4k_LEVEL 0x4
#define SR_CVBS_LEVEL 0x8
#define SR_NOSCALE_LEVEL 0x10
static void amve_sr_reg_setting(unsigned int adaptive_level)
{
	if (adaptive_level & SR_SD_SCALE_LEVEL)
		am_set_regmap(&sr1reg_sd_scale);
	else if (adaptive_level & SR_HD_SCALE_LEVEL)
		am_set_regmap(&sr1reg_hd_scale);
	else if (adaptive_level & SR_4k_LEVEL) {
		amvecm_sharpness_enable(1);/*peaking*/
		amvecm_sharpness_enable(3);/*lcti*/
		amvecm_sharpness_enable(5);/*drtlpf theta*/
		amvecm_sharpness_enable(7);/*debanding*/
		amvecm_sharpness_enable(9);/*dejaggy*/
		amvecm_sharpness_enable(11);/*dering*/
		amvecm_sharpness_enable(13);/*drlpf*/
	} else if (adaptive_level & SR_CVBS_LEVEL)
		am_set_regmap(&sr1reg_cvbs);
	else if (adaptive_level & SR_NOSCALE_LEVEL)
		am_set_regmap(&sr1reg_hv_noscale);
}
void amve_sharpness_adaptive_setting(struct vframe_s *vf,
	 unsigned int sps_h_en, unsigned int sps_v_en)
{
	static unsigned int adaptive_level = 1;
	unsigned int cur_level;
	unsigned int width, height;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vf == NULL)
		return;
	if (vinfo->mode == VMODE_CVBS)
		cur_level = SR_CVBS_LEVEL;
	else {
		width = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;

		if ((sps_h_en == 1) && (sps_v_en == 1)) {
			/*super scaler h and v scale up x2 */
			if ((height >= 2160) && (width >= 3840))
				cur_level = SR_4k_LEVEL;
			else if ((height >= 720) && (width >= 1280))
				cur_level = SR_HD_SCALE_LEVEL;
			else
				cur_level = SR_SD_SCALE_LEVEL;
		} else {
			/*1. super scaler no up scale */
			/*2. super scaler h up scale x2*/
			if ((height >= 2160) && (width >= 3840))
				cur_level = SR_4k_LEVEL;
			else
				cur_level = SR_NOSCALE_LEVEL;
		}
	}

	if (adaptive_level == cur_level)
		return;

	amve_sr_reg_setting(cur_level);
	adaptive_level = cur_level;
	sr_adapt_level = adaptive_level;
	pr_amve_dbg("\n[amcm..]sr_adapt_level = %d :1->sd;2->hd;4->4k\n",
			sr_adapt_level);

}
/*gxlx sr init*/
void amve_sharpness_init(void)
{
	am_set_regmap(&sr1reg_sd_scale);
}

static int overscan_timing = TIMING_MAX;
module_param(overscan_timing, uint, 0664);
MODULE_PARM_DESC(overscan_timing, "\n overscan_control\n");

static int overscan_screen_mode = 0xff;
module_param(overscan_screen_mode, uint, 0664);
MODULE_PARM_DESC(overscan_screen_mode, "\n overscan_screen_mode\n");

static int overscan_disable;
module_param(overscan_disable, uint, 0664);
MODULE_PARM_DESC(overscan_disable, "\n overscan_disable\n");

void amvecm_fresh_overscan(struct vframe_s *vf)
{
	unsigned int height = 0;
	unsigned int cur_overscan_timing = 0;

	if (overscan_disable)
		return;
	if (is_dolby_vision_on())
		return;
	if (overscan_table[0].load_flag) {
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
		if (height <= 576)
			cur_overscan_timing = TIMING_SD;
		else if (height <= 720)
			cur_overscan_timing = TIMING_HD;
		else if (height <= 1088)
			cur_overscan_timing = TIMING_FHD;
		else
			cur_overscan_timing = TIMING_UHD;

		overscan_timing = cur_overscan_timing;
		overscan_screen_mode =
			overscan_table[overscan_timing].screen_mode;

		vf->pic_mode.AFD_enable =
			overscan_table[overscan_timing].afd_enable;
		/*local play screen mode set by decoder*/
		if (overscan_table[0].source == SOURCE_MPEG)
			vf->pic_mode.screen_mode = 0xff;
		else
			vf->pic_mode.screen_mode =
				overscan_table[overscan_timing].screen_mode;

		vf->pic_mode.hs = overscan_table[overscan_timing].hs;
		vf->pic_mode.he = overscan_table[overscan_timing].he;
		vf->pic_mode.vs = overscan_table[overscan_timing].vs;
		vf->pic_mode.ve = overscan_table[overscan_timing].ve;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
	}
}

void amvecm_reset_overscan(void)
{
	if (overscan_disable)
		return;
	if (overscan_timing != TIMING_MAX) {
		overscan_timing = TIMING_MAX;
		if ((overscan_table[0].source != SOURCE_DTV) &&
			(overscan_table[0].source != SOURCE_MPEG)) {
			overscan_table[0].load_flag = 0;
			overscan_screen_mode = 0xff;
		}
	}
}

static int P3dlut_tab[289] = {0};
static int P3dlut_regtab[291] = {0};
/*------------------------------------------------*/
/*pLut3D[]: 17*17*17*3*12bit*/
int vpp_set_lut3d(int enable, int bLut3DLoad,
	int *pLut3D, int bLut3DCheck)
{
	int i;
	uint32_t dwTemp, wRgb[3];

	if (bLut3DLoad) {
		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
		WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0|(0<<31));
		for (i = 0; i < 17*17*17; i++) {
			//{comp0, comp1, comp2}
			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
				((pLut3D[i*3+1]&0xfff)<<16)|
				(pLut3D[i*3+2]&0xfff));
			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
				(pLut3D[i*3+0]&0xfff)); /*MSB*/
		}
		pr_info("%s: Lut3d load ok!!\n", __func__);
	}
	if (bLut3DCheck) {
		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
		WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0|(1<<31));
		for (i = 0; i < 17*17*17; i++) {
			dwTemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			wRgb[2] = dwTemp & 0xfff;
			wRgb[1] = (dwTemp>>16)&0xfff;
			dwTemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			wRgb[0] = dwTemp & 0xfff;
			if (i < 97) {
				P3dlut_regtab[i*3+2] = wRgb[2];
				P3dlut_regtab[i*3+1] = wRgb[1];
				P3dlut_regtab[i*3+0] = wRgb[0];
			}
			if (wRgb[0] != pLut3D[i*3+0]) {
				pr_info("%s:Error: Lut3d check error at R[%d]\n",
					__func__, i);
				return 1;
			}
			if (wRgb[1] != pLut3D[i*3+1]) {
				pr_info("%s:Error: Lut3d check error at G[%d]\n",
					__func__, i);
				return 1;
			}
			if (wRgb[2] != pLut3D[i*3+2]) {
				pr_info("%s:\n", __func__);
				return 1;
			}
		}
		pr_info("%s: Lut3d check ok!!\n", __func__);
	}

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, 7, 4, 3);/*reg_lut3d_extnd_en[6:4]*/
	WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, enable&0x1, 0, 1);/*reg_lut3d_en*/
	pr_info("%s: Lut3d set done!!\n", __func__);
	return 0;
}

static void ycbcr2rgbpc_nb(int *R, int *G, int *B,
	int Y, int Cb, int Cr, int bitdepth)
{
	int y = 0;
	int cb = 0;
	int cr = 0;
	int r = 0;
	int g = 0;
	int b = 0;
	int norm = (1<<bitdepth)-1;

	y  = Y  - (1<<(bitdepth-4));
	cb = Cb - (1<<(bitdepth-1));
	cr = Cr - (1<<(bitdepth-1));

	r = (298 * y + 408 * cr) / 256;
	g = (298 * y - 208 * cr - 100 * cb) / 256;
	b = (298 * y + 516 * cb) / 256;

	if (r > norm)
		r = norm;
	if (g > norm)
		g = norm;
	if (b > norm)
		b = norm;

	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;

	*R = r;
	*G = g;
	*B = b;
}

/*table: use for yuv->rgb*/
void vpp_lut3d_table_init(int *pLut3D, int bitdepth)
{
	int d0, d1, d2, ncmp;
	unsigned int i;
	int step[3]; /*steps of each input components lut-nodes*/
	int max_val = (1 << bitdepth) - 1;
	/*step*/
	for (ncmp = 0; ncmp < 3; ncmp++)
		step[ncmp] = (1 << (bitdepth - 4));

	/*initialize the lut3d ad same input and output;*/
	for (d0 = 0; d0 < 17; d0++) {
		for (d1 = 0; d1 < 17; d1++) {
			for (d2 = 0; d2 < 17; d2++) {
				pLut3D[d0*17*17*3+d1*17*3+d2*3+0] =
					(d0*step[0] < max_val) ?
					d0*step[0] : max_val;/* 1st components*/
				pLut3D[d0*17*17*3+d1*17*3+d2*3+1] =
					(d1*step[1] < max_val) ?
					d1*step[1] : max_val;/*2nd components*/
				pLut3D[d0*17*17*3+d1*17*3+d2*3+2] =
					(d2*step[2] < max_val) ?
					d2*step[2] : max_val;/*3rd components*/
				ycbcr2rgbpc_nb(
					&pLut3D[d0*17*17*3+d1*17*3+d2*3+0],
					&pLut3D[d0*17*17*3+d1*17*3+d2*3+1],
					&pLut3D[d0*17*17*3+d1*17*3+d2*3+2],
					pLut3D[d0*17*17*3+d1*17*3+d2*3+0],
					pLut3D[d0*17*17*3+d1*17*3+d2*3+1],
					pLut3D[d0*17*17*3+d1*17*3+d2*3+2],
					bitdepth);
			}
		}
	}
	for (i = 0; i < 289; i++)
		P3dlut_tab[i] = pLut3D[i];
}

void dump_plut3d_table(void)
{
	unsigned int i, j;

	pr_info("*****dump_plut3d_table:pLut3D[0]~pLut3D[288]*****\n");
	for (i = 0; i < 17; i++) {
		pr_info("*****dump pLut3D:17* %d*****\n", i);
		for (j = 0; j < 17; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_tab[i*17+j]);
		}
	}
}

void dump_plut3d_reg_table(void)
{
	unsigned int i, j;

	pr_info("*****dump_plut3d_reg_table:[0]~[288]*****\n");
	for (i = 0; i < 17; i++) {
		pr_info("*****dump pLut3D regtab:17* %d*****\n", i);
		for (j = 0; j < 17; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_regtab[i*17+j]);
		}
	}
}


