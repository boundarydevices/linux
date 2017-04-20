/*
 * Color Management
 * include/linux/amlogic/media/amvecm/cm.h
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _TVOUT_CM_H
#define _TVOUT_CM_H

/* ******************************************************************* */
/* *** enum definitions ********************************************* */
/* ******************************************************************* */

enum cm_region_idx_e {
	CM_REGION_IDX_0 = 0,
	CM_REGION_IDX_1,
	CM_REGION_IDX_2,
	CM_REGION_IDX_3,
	CM_REGION_IDX_4,
	CM_REGION_IDX_5,
	CM_REGION_IDX_6,
	CM_REGION_IDX_7,
};

enum cm_sat_shape_e {
	CM_SAT_SHAPE_RIGHT_BIGGEST = 0,
	CM_SAT_SHAPE_LEFT_BIGGEST,
};

enum cm_hue_shape_e {
	CM_HUE_SHAPE_LEFT_MORE = 0,
	CM_HUE_SHAPE_RIGHT_MORE,
};

enum cm_demo_pos_e {
	CM_DEMO_POS_TOP = 0,
	CM_DEMO_POS_BOTTOM,
	CM_DEMO_POS_LEFT,
	CM_DEMO_POS_RIGHT,
};

enum cm_sat_sel_e {
	CM_SAT_SEL_U2_V2 = 0,
	CM_SAT_SEL_UV_MAX,
};

enum cm_csc_e {
	CM_CSC_601 = 0,
	CM_CSC_709,
	CM_CSC_FULL_601,
	CM_CSC_FULL_709,
};

/* ******************************************************************* */
/* *** struct definitions ********************************************* */
/* ******************************************************************* */

struct cm_region_s {
	enum cm_region_idx_e region_idx;
	/* sym */
	unsigned char       sym_en;
	/* sat - top */
	unsigned char       sat_en;
	unsigned char       sat_central_en;
	enum cm_sat_shape_e sat_shape;
	unsigned char       sat_gain;
	unsigned char       sat_inc;
	/* sat - lum */
	unsigned char       sat_lum_h_slope;
	unsigned char       sat_lum_l_slope;
	unsigned char       sat_lum_h;
	unsigned char       sat_lum_l;
	/* sat - sat */
	unsigned char       sat_sat_h_slope;
	unsigned char       sat_sat_l_slope;
	unsigned char       sat_sat_h;
	unsigned char       sat_sat_l;
	/* hue - top */
	unsigned char       hue_en;
	unsigned char       hue_central_en;
	enum cm_hue_shape_e hue_shape;
	unsigned char       hue_gain;
	unsigned char       hue_clockwise;
	unsigned char       hue_shf_ran;
	unsigned short      hue_shf_sta;
	/* hue - lum */
	unsigned char       hue_lum_h_slope;
	unsigned char       hue_lum_l_slope;
	unsigned char       hue_lum_h;
	unsigned char       hue_lum_l;
	/* hue - sat */
	unsigned char       hue_sat_h_slope;
	unsigned char       hue_sat_l_slope;
	unsigned char       hue_sat_h;
	unsigned char       hue_sat_l;
};

struct cm_top_s {
	unsigned char       chroma_en;
	enum cm_sat_sel_e   sat_sel;
	unsigned char       uv_adj_en;
	unsigned char       rgb_to_hue_en;
	enum cm_csc_e       csc_sel;
};

struct cm_cbar_s {
	unsigned char en;
	unsigned char wid;
	unsigned char cr;
	unsigned char cb;
	unsigned char y;
};
struct cm_demo_s {
	unsigned char       en;
	enum cm_demo_pos_e  pos;
	unsigned char       hlight_adj;
	unsigned short      wid;
	struct cm_cbar_s   cbar;
};

struct cm_regmap_s {
	unsigned long reg[50];
};

enum reg_bus_type_e {
	REG_TYPE_PHY = 0,
	REG_TYPE_CBUS = 1,
	REG_TYPE_APB = 2,
	REG_TYPE_AXI = 3,
	REG_TYPE_AHB = 4,
	REG_TYPE_MPEG = 5,
	REG_TYPE_INDEX_VPPCHROMA = 6,
	REG_TYPE_INDEX_GAMMA = 7,
	VALUE_TYPE_CONTRAST_BRIGHTNESS = 8,
	REG_TYPE_INDEX_VPP_COEF = 9,
	REG_TYPE_VCBUS = 10,
	REG_TYPE_MAX,
};

/* Register table structure */
struct am_reg_s {
	unsigned int type; /* 32-bits; 0: CBUS; 1: APB BUS... */
	unsigned int addr; /* 32-bits; Register address */
	unsigned int mask; /* 32-bits; Valid bits */
	unsigned int  val; /* 32-bits; Register Value */
};
#define am_reg_size 900
struct am_regs_s {
	unsigned int    length; /* Length of total am_reg */
	struct am_reg_s am_reg[am_reg_size];
};

#endif  /* _TVOUT_CM_H */
