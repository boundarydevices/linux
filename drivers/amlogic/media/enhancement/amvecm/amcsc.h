/*
 * drivers/amlogic/media/enhancement/amvecm/amcsc.h
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

#ifndef AM_CSC_H
#define AM_CSC_H

/* white balance value */
extern void ve_ogo_param_update(void);
extern struct tcon_rgb_ogo_s video_rgb_ogo;

enum vpp_matrix_sel_e {
	VPP_MATRIX_0 = 0,	/* OSD convert matrix - new from GXL */
	VPP_MATRIX_1,		/* vd1 matrix before post-blend */
	VPP_MATRIX_2,		/* post matrix */
	VPP_MATRIX_3,		/* xvycc matrix */
	VPP_MATRIX_4,		/* in video eotf - new from GXL */
	VPP_MATRIX_5,		/* in osd eotf - new from GXL */
	VPP_MATRIX_6		/* vd2 matrix before pre-blend */
};
#define NUM_MATRIX 6

/* matrix names */
#define VPP_MATRIX_OSD		VPP_MATRIX_0
#define VPP_MATRIX_VD1		VPP_MATRIX_1
#define VPP_MATRIX_POST		VPP_MATRIX_2
#define VPP_MATRIX_XVYCC	VPP_MATRIX_3
#define VPP_MATRIX_EOTF		VPP_MATRIX_4
#define VPP_MATRIX_OSD_EOTF	VPP_MATRIX_5
#define VPP_MATRIX_VD2		VPP_MATRIX_6

/*	osd->eotf->matrix5->oetf->matrix0-+->post blend*/
/*		->vadj2->matrix2->eotf->matrix4->oetf->matrix3*/
/*	video1->cm->lut->vadj1->matrix1-^*/
/*			  video2->matrix6-^*/

#define CSC_ON              1
#define CSC_OFF             0

enum vpp_lut_sel_e {
	VPP_LUT_OSD_EOTF = 0,
	VPP_LUT_OSD_OETF,
	VPP_LUT_EOTF,
	VPP_LUT_OETF,
	VPP_LUT_INV_EOTF
};
#define NUM_LUT 5

/* matrix registers */
struct matrix_s {
	u16 pre_offset[3];
	u16 matrix[3][3];
	u16 offset[3];
	u16 right_shift;
};

enum mtx_en_e {
	POST_MTX_EN = 0,
	VD2_MTX_EN = 4,
	VD1_MTX_EN,
	XVY_MTX_EN,
	OSD1_MTX_EN
};

#define POST_MTX_EN_MASK (1 << POST_MTX_EN)
#define VD2_MTX_EN_MASK  (1 << VD2_MTX_EN)
#define VD1_MTX_EN_MASK  (1 << VD1_MTX_EN)
#define XVY_MTX_EN_MASK  (1 << XVY_MTX_EN)
#define OSD1_MTX_EN_MASK (1 << OSD1_MTX_EN)

#define HDR_SUPPORT		(1 << 2)
#define HLG_SUPPORT		(1 << 3)

#define LUT_289_SIZE	289
extern unsigned int lut_289_mapping[LUT_289_SIZE];
extern int dnlp_en;
/*extern int cm_en;*/

extern unsigned int vecm_latch_flag;
extern signed int vd1_contrast_offset;
extern signed int saturation_offset;
extern uint sdr_mode;
extern uint hdr_flag;
extern int video_rgb_ogo_xvy_mtx_latch;
extern int video_rgb_ogo_xvy_mtx;
extern int tx_op_color_primary;
extern uint cur_csc_type;


extern int amvecm_matrix_process(
	struct vframe_s *vf, struct vframe_s *vf_rpt, int flags);
extern int amvecm_hdr_dbg(u32 sel);

extern u32 get_video_enabled(void);
extern void get_hdr_source_type(void);

/*hdr*/
/*#define DBG_BUF_SIZE (1024)*/

struct hdr_cfg_t {
	unsigned int en_osd_lut_100;
};
struct hdr_data_t {
	struct hdr_cfg_t hdr_cfg;

	/*debug_fs*/
	struct dentry *dbg_root;
	/*char dbg_buf[DBG_BUF_SIZE];*/

};

extern void hdr_init(struct hdr_data_t *phdr_data);
extern void hdr_exit(void);
extern void hdr_set_cfg_osd_100(int val);

#endif /* AM_CSC_H */

