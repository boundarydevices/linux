/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_ctl.h
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

#ifndef __TVIN_VDIN_CTL_H
#define __TVIN_VDIN_CTL_H


#include <linux/amlogic/media/vfm/vframe.h>

#include "vdin_drv.h"



/* *********************************************************************** */
/* *** enum definitions ********************************************* */
/* *********************************************************************** */
/*
 *YUV601:  SDTV BT.601            YCbCr (16~235, 16~240, 16~240)
 *YUV601F: SDTV BT.601 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *YUV709:  HDTV BT.709            YCbCr (16~235, 16~240, 16~240)
 *YUV709F: HDTV BT.709 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *RGBS:                       StudioRGB (16~235, 16~235, 16~235)
 *RGB:                              RGB ( 0~255,  0~255,  0~255)
 */
enum vdin_matrix_csc_e {
	VDIN_MATRIX_NULL = 0,
	VDIN_MATRIX_XXX_YUV601_BLACK,
	VDIN_MATRIX_RGB_YUV601,
	VDIN_MATRIX_GBR_YUV601,
	VDIN_MATRIX_BRG_YUV601,
	VDIN_MATRIX_YUV601_RGB,
	VDIN_MATRIX_YUV601_GBR,
	VDIN_MATRIX_YUV601_BRG,
	VDIN_MATRIX_RGB_YUV601F,
	VDIN_MATRIX_YUV601F_RGB,
	VDIN_MATRIX_RGBS_YUV601,
	VDIN_MATRIX_YUV601_RGBS,
	VDIN_MATRIX_RGBS_YUV601F,
	VDIN_MATRIX_YUV601F_RGBS,
	VDIN_MATRIX_YUV601F_YUV601,
	VDIN_MATRIX_YUV601_YUV601F,
	VDIN_MATRIX_RGB_YUV709,
	VDIN_MATRIX_YUV709_RGB,
	VDIN_MATRIX_YUV709_GBR,
	VDIN_MATRIX_YUV709_BRG,
	VDIN_MATRIX_RGB_YUV709F,
	VDIN_MATRIX_YUV709F_RGB,
	VDIN_MATRIX_RGBS_YUV709,
	VDIN_MATRIX_YUV709_RGBS,
	VDIN_MATRIX_RGBS_YUV709F,
	VDIN_MATRIX_YUV709F_RGBS,
	VDIN_MATRIX_YUV709F_YUV709,
	VDIN_MATRIX_YUV709_YUV709F,
	VDIN_MATRIX_YUV601_YUV709,
	VDIN_MATRIX_YUV709_YUV601,
	VDIN_MATRIX_YUV601_YUV709F,
	VDIN_MATRIX_YUV709F_YUV601,
	VDIN_MATRIX_YUV601F_YUV709,
	VDIN_MATRIX_YUV709_YUV601F,
	VDIN_MATRIX_YUV601F_YUV709F,
	VDIN_MATRIX_YUV709F_YUV601F,
	VDIN_MATRIX_RGBS_RGB,
	VDIN_MATRIX_RGB_RGBS,
};

/* *************************************************** */
/* *** structure definitions ************************* */
/* *************************************************** */
struct vdin_matrix_lup_s {
	unsigned int pre_offset0_1;
	unsigned int pre_offset2;
	unsigned int coef00_01;
	unsigned int coef02_10;
	unsigned int coef11_12;
	unsigned int coef20_21;
	unsigned int coef22;
	unsigned int post_offset0_1;
	unsigned int post_offset2;
};

struct vdin_stat_s {
	unsigned int   sum_luma;  /* VDIN_HIST_LUMA_SUM_REG */
	unsigned int   sum_pixel; /* VDIN_HIST_PIX_CNT_REG */
};

#ifdef CONFIG_AMLOGIC_LOCAL_DIMMING
struct ldim_max_s {
    /* general parameters */
	int ld_pic_rowmax;
	int ld_pic_colmax;
	int ld_stamax_hidx[11];  /* U12* 9 */
	int ld_stamax_vidx[11];  /* u12x 9 */
};
#endif

struct vdin_hist_cfg_s {
	unsigned int                pow;
	unsigned int                win_en;
	unsigned int                rd_en;
	unsigned int                hstart;
	unsigned int                hend;
	unsigned int                vstart;
	unsigned int                vend;
};

/* ************************************************************************ */
/* ******** GLOBAL FUNCTION CLAIM ******** */
/* ************************************************************************ */
extern void vdin_set_vframe_prop_info(struct vframe_s *vf,
		struct vdin_dev_s *devp);
extern void LDIM_Initial_2(int pic_h, int pic_v, int BLK_Vnum,
	int BLK_Hnum, int BackLit_mode, int ldim_bl_en, int ldim_hvcnt_bypass);
extern void vdin_get_format_convert(struct vdin_dev_s *devp);
extern enum vdin_format_convert_e vdin_get_format_convert_matrix0(
		struct vdin_dev_s *devp);
extern enum vdin_format_convert_e vdin_get_format_convert_matrix1(
		struct vdin_dev_s *devp);
extern void vdin_set_prob_xy(unsigned int offset, unsigned int x,
		unsigned int y, struct vdin_dev_s *devp);
extern void vdin_get_prob_rgb(unsigned int offset, unsigned int *r,
		unsigned int *g, unsigned int *b);
extern void vdin_set_all_regs(struct vdin_dev_s *devp);
extern void vdin_set_default_regmap(unsigned int offset);
extern void vdin_set_def_wr_canvas(struct vdin_dev_s *devp);
extern void vdin_hw_enable(unsigned int offset);
extern void vdin_hw_disable(unsigned int offset);
extern unsigned int vdin_get_field_type(unsigned int offset);
extern int vdin_vsync_reset_mif(int index);
extern void vdin_set_cutwin(struct vdin_dev_s *devp);
extern void vdin_set_decimation(struct vdin_dev_s *devp);
extern void vdin_fix_nonstd_vsync(struct vdin_dev_s *devp);
extern unsigned int vdin_get_meas_hcnt64(unsigned int offset);
extern unsigned int vdin_get_meas_vstamp(unsigned int offset);
extern unsigned int vdin_get_active_h(unsigned int offset);
extern unsigned int vdin_get_active_v(unsigned int offset);
extern unsigned int vdin_get_total_v(unsigned int offset);
extern unsigned int vdin_get_canvas_id(unsigned int offset);
extern void vdin_set_canvas_id(struct vdin_dev_s *devp,
			unsigned int rdma_enable, unsigned int canvas_id);
extern unsigned int vdin_get_chma_canvas_id(unsigned int offset);
extern void vdin_set_chma_canvas_id(struct vdin_dev_s *devp,
		unsigned int rdma_enable, unsigned int canvas_id);
extern void vdin_enable_module(unsigned int offset, bool enable);
extern void vdin_set_matrix(struct vdin_dev_s *devp);
void vdin_set_matrixs(struct vdin_dev_s *devp, unsigned char no,
		enum vdin_format_convert_e csc);
extern void vdin_set_matrix_blank(struct vdin_dev_s *devp);
extern void vdin_delay_line(unsigned short num, unsigned int offset);
extern void set_wr_ctrl(int h_pos, int v_pos, struct vdin_dev_s *devp);
extern bool vdin_check_cycle(struct vdin_dev_s *devp);
extern bool vdin_write_done_check(unsigned int offset,
		struct vdin_dev_s *devp);
extern bool vdin_check_vs(struct vdin_dev_s *devp);
extern void vdin_calculate_duration(struct vdin_dev_s *devp);
extern void vdin_output_ctl(unsigned int offset,
		unsigned int output_flag);
extern void vdin_wr_reverse(unsigned int offset, bool hreverse,
		bool vreverse);
extern void vdin_set_hvscale(struct vdin_dev_s *devp);
extern void vdin_set_bitdepth(struct vdin_dev_s *devp);
extern void vdin_set_cm2(unsigned int offset, unsigned int w,
		unsigned int h, unsigned int *data);
extern void vdin_bypass_isp(unsigned int offset);
extern void vdin_set_mpegin(struct vdin_dev_s *devp);
extern void vdin_force_gofiled(struct vdin_dev_s *devp);
extern void vdin_set_config(struct vdin_dev_s *devp);

#endif

