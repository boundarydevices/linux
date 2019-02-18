/*
 * drivers/amlogic/media/enhancement/amvecm/bitdepth.c
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
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/vpp_dolbyvision_regs.h"
#include "bitdepth.h"


/*u2s_mode:0:true 12bit;1:false 12bit*/
static unsigned int u2s_mode;
module_param(u2s_mode, uint, 0664);
MODULE_PARM_DESC(u2s_mode, "\n u2s_mode\n");

/*u2s_mode:0:bypass;1:enable*/
static unsigned int dolby_core1_en;
module_param(dolby_core1_en, uint, 0664);
MODULE_PARM_DESC(dolby_core1_en, "\n dolby_core1_en\n");

static unsigned int dolby_core2_en;
module_param(dolby_core2_en, uint, 0664);
MODULE_PARM_DESC(dolby_core2_en, "\n dolby_core2_en\n");

static unsigned int vd2_en;
module_param(vd2_en, uint, 0664);
MODULE_PARM_DESC(vd2_en, "\n vd2_en\n");

static unsigned int osd2_en;
module_param(osd2_en, uint, 0664);
MODULE_PARM_DESC(osd2_en, "\n osd2_en\n");

static unsigned int dolby_core1_ext_mode;
module_param(dolby_core1_ext_mode, uint, 0664);
MODULE_PARM_DESC(dolby_core1_ext_mode, "\n dolby_core1_ext_mode\n");

static unsigned int dolby_core2_ext_mode;
module_param(dolby_core2_ext_mode, uint, 0664);
MODULE_PARM_DESC(dolby_core2_ext_mode, "\n dolby_core2_ext_mode\n");

static unsigned int vd2_ext_mode;
module_param(vd2_ext_mode, uint, 0664);
MODULE_PARM_DESC(vd2_ext_mode, "\n vd2_ext_mode\n");

static unsigned int osd2_ext_mode;
module_param(osd2_ext_mode, uint, 0664);
MODULE_PARM_DESC(osd2_ext_mode, "\n osd2_ext_mode\n");

/**/
static unsigned int vpp_dpath_sel0;
module_param(vpp_dpath_sel0, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel0, "\n vpp_dpath_sel0\n");

static unsigned int vpp_dpath_sel1;
module_param(vpp_dpath_sel1, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel1, "\n vpp_dpath_sel1\n");

static unsigned int vpp_dpath_sel2;
module_param(vpp_dpath_sel2, uint, 0664);
MODULE_PARM_DESC(vpp_dpath_sel2, "\n vpp_dpath_sel2\n");

static unsigned int vpp_dolby3_en;
module_param(vpp_dolby3_en, uint, 0664);
MODULE_PARM_DESC(vpp_dolby3_en, "\n vpp_dolby3_en\n");

static unsigned int core1_input_data_conv_mode;
module_param(core1_input_data_conv_mode, uint, 0664);
MODULE_PARM_DESC(core1_input_data_conv_mode, "\n core1_input_data_conv_mode\n");

static unsigned int core1_output_data_conv_mode;
module_param(core1_output_data_conv_mode, uint, 0664);
MODULE_PARM_DESC(core1_output_data_conv_mode, "\n core1_output_data_conv_mode\n");

static unsigned int vd1_input_clip_mode;
module_param(vd1_input_clip_mode, uint, 0664);
MODULE_PARM_DESC(vd1_input_clip_mode, "\n vd1_input_clip_mode\n");

static unsigned int vd2_input_clip_mode;
module_param(vd2_input_clip_mode, uint, 0664);
MODULE_PARM_DESC(vd2_input_clip_mode, "\n vd2_input_clip_mode\n");

static unsigned int vpp_output_clip_mode;
module_param(vpp_output_clip_mode, uint, 0664);
MODULE_PARM_DESC(vpp_output_clip_mode, "\n vpp_output_clip_mode\n");

static unsigned int vpp_dith_en;
module_param(vpp_dith_en, uint, 0664);
MODULE_PARM_DESC(vpp_dith_en, "\n vpp_dith_en\n");

/*U12toU10:1:cut low 2bit;0:cut high 2bit*/
static unsigned int vpp_dith_mode;
module_param(vpp_dith_mode, uint, 0664);
MODULE_PARM_DESC(vpp_dith_mode, "\n vpp_dith_mode\n");

static unsigned int dolby3_path_sel;
module_param(dolby3_path_sel, uint, 0664);
MODULE_PARM_DESC(dolby3_path_sel, "\n dolby3_path_sel\n");

/*u2s: scaler:>>n;offset:-n*/
void vpp_set_pre_u2s(enum data_conv_mode_e conv_mode)
{
	/*U12-->U10*/
	if (conv_mode == U12_TO_U10)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x2000, 16, 14);
	/*U12-->S12*/
	else if (conv_mode == U12_TO_S12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x800, 16, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0, 16, 14);
}
void vpp_set_post_u2s(enum data_conv_mode_e conv_mode)
{
	/*U12-->U10*/
	if (conv_mode == U12_TO_U10)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x2000, 16, 14);
	/*U12-->S12*/
	else if (conv_mode == U12_TO_S12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x800, 16, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0, 16, 14);
}
/*s2u: scaler:<<n;offset:+n*/
void vpp_set_pre_s2u(enum data_conv_mode_e conv_mode)
{
	/*U10-->U12*/
	if (conv_mode == U10_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x2000, 0, 14);
	/*S12-->U12*/
	else if (conv_mode == S12_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0x800, 0, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0, 0, 14);
}
void vpp_set_post_s2u(enum data_conv_mode_e conv_mode)
{
	/*U10-->U12*/
	if (conv_mode == U10_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x2000, 0, 14);
	/*S12-->U12*/
	else if (conv_mode == S12_TO_U12)
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0x800, 0, 14);
	else
		WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0, 0, 14);
}
static void vpp_blackext_en(bool flag)
{
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, flag, 3, 1);
}
static void vpp_chroma_coring_en(bool flag)
{
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, flag, 4, 1);
}
static void vpp_bluestretch_en(bool flag)
{
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, flag, 0, 1);
}
static void vpp_vadj1_en(bool flag)
{
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, flag, 0, 1);
}
static void vpp_vadj2_en(bool flag)
{
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, flag, 2, 1);
}

/*skip:1-->skip;0-->un skip*/
void vpp_skip_pps(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 0, 1);
}
void vpp_skip_eotf_matrix3(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 1, 1);
}
void vpp_skip_vadj2_matrix3(bool skip)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, skip, 2, 1);
}
/*bypass:1-->bypass;0-->un bypass*/
void vpp_bypas_core1(bool bypass)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, bypass, 16, 1);
}
void vpp_bypas_core2(bool bypass)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, bypass, 18, 1);
}
/*enable:1-->enable;0-->un enable*/
void vpp_enable_vd2(bool enable)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, enable, 17, 1);
}
void vpp_enable_osd2(bool enable)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, enable, 19, 1);
}
/*extend mode,only work when dolby_core1 bypass:*/
/*0:extend 2bit 0 in low bits*/
/*1:extend 2bit 0 in high bits*/
void vpp_extend_mode_core1(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 20, 1);
}
void vpp_extend_mode_core2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 22, 1);
}
void vpp_extend_mode_vd2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 21, 1);
}
void vpp_extend_mode_osd2(bool mode)
{
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, mode, 23, 1);
}
/**/
void vpp_vd1_if_bits_mode(enum vd_if_bits_mode_e bits_mode)
{
	u32 reg = ((is_meson_g12a_cpu() ||
		is_meson_g12b_cpu()) ?
		G12_VD1_IF0_GEN_REG3 :
		VD1_IF0_GEN_REG3);
	WRITE_VPP_REG_BITS(reg, (bits_mode & 0x3), 8, 2);
}
void vpp_vd2_if_bits_mode(enum vd_if_bits_mode_e bits_mode)
{
	u32 reg = ((is_meson_g12a_cpu() ||
		is_meson_g12b_cpu()) ?
		G12_VD2_IF0_GEN_REG3 :
		VD2_IF0_GEN_REG3);
	WRITE_VPP_REG_BITS(reg, (bits_mode & 0x3), 8, 2);
}
void vpp_enable_dither(bool enable)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, enable, 12, 1);
}
/*mode:0:cut high 2 bits;1:cut low 2 bits*/
void vpp_dither_bits_comp_mode(bool mode)
{
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, mode, 14, 1);
}

/*g12a new add begin*/
/*0:axird to vd1;1:axird to afbc1*/
void vpp_set_vd1_mux0(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 12, 2);
}
/*0:axird to vd2;1:axird to afbc2*/
void vpp_set_vd2_mux0(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 12, 2);
}

/*vd1 mif config:0:vd1_mif to vpp;1:use afbc_mif*/
void vpp_set_vd1_mux1(bool flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 10, 1);
}
void vpp_set_vd2_mux1(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 10, 1);
}

/*0:vd1/afbc to vpp;1:vd1/afbc to di*/
void vpp_set_vd1_mux2(bool flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 8, 1);
}
/*0:afbc1 to di;1:afbc2 to di*/
void vpp_set_vd2_mux2(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 8, 1);
}

/*0:afbc to vpp;1:afbc to di*/
void vpp_set_vd1_mux3(bool flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 9, 1);
}
void vpp_set_vd2_mux3(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 9, 1);
}

/*0:afbc/vd1 to vd1;1:osd3 to vd1*/
void vpp_set_vd1_mux4(bool flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 11, 1);
}
/*0:afbc1/vd2 to vd2;1:osd4 to vd2*/
void vpp_set_vd2_mux4(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 11, 1);
}

/*0:vd1 to dolby;2:vd1 to primel*/
void vpp_set_vd1_mux5(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 14, 2);
}
/*0:afbc2 to vd2;2:osd4 to vd2*/
void vpp_set_vd2_mux5(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 14, 2);
}

/*0:vd1 gclk enable 0x55: disbale */
void vpp_set_vd1_gate(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD1_AFBCD0_MISC_CTRL, flag, 0, 8);
}
/*0:vd2 gclk enable 0x55: disbale */
void vpp_set_vd2_gate(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD2_AFBCD1_MISC_CTRL, flag, 0, 8);
}

/*blend src mux==>0:close;1:vd1;2:vd2:3:osd1;4:osd2*/
void vpp_set_vd1_preblend_mux(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD1_BLEND_SRC_CTRL, flag, 0, 4);
}
/*blend src mux==>0:close;1:vd1;2:vd2:3:osd1;4:osd2*/
void vpp_set_vd1_postblend_mux(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD1_BLEND_SRC_CTRL, flag, 8, 4);
}
/*if vpp_misc 0x1d26[7]=1 0x1dfb[16] must set 1*/
void vpp_set_vd1_postblend_en(bool flag)
{
	WRITE_VPP_REG_BITS(VD1_BLEND_SRC_CTRL, flag, 16, 1);
}

/*blend src mux==>0:close;1:vd1;2:vd2:3:osd1;4:osd2*/
void vpp_set_vd2_preblend_mux(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD2_BLEND_SRC_CTRL, flag, 0, 4);
}
/*blend src mux==>0:close;1:vd1;2:vd2:3:osd1;4:osd2*/
void vpp_set_vd2_postblend_mux(unsigned int flag)
{
	WRITE_VPP_REG_BITS(VD2_BLEND_SRC_CTRL, flag, 8, 4);
}
void vpp_set_vd2_postblend_en(bool flag)
{
	WRITE_VPP_REG_BITS(VD2_BLEND_SRC_CTRL, flag, 16, 1);
	if (flag)
		WRITE_VPP_REG_BITS(VD2_BLEND_SRC_CTRL, 1, 20, 1);
}

/*data ext mod==>0:data << 2;1:data << 0*/
void vpp_set_vd1_ext_mod(bool flag)
{
	WRITE_VPP_REG_BITS(DOLBY_PATH_CTRL, flag, 4, 1);
}
/*vd1 dolby bypass==>0:no bypass;1:bypass*/
void vpp_set_vd1_bypass_dolby(bool flag)
{
	WRITE_VPP_REG_BITS(DOLBY_PATH_CTRL, flag, 0, 1);
}

/*data ext mod==>0:data << 2;1:data << 0*/
void vpp_set_vd2_ext_mod(bool flag)
{
	WRITE_VPP_REG_BITS(DOLBY_PATH_CTRL, flag, 5, 1);
}
/*vd2 dolby bypass==>0:no bypass;1:bypass*/
void vpp_set_vd2_bypass_dolby(bool flag)
{
	WRITE_VPP_REG_BITS(DOLBY_PATH_CTRL, flag, 1, 1);
}

void vpp_set_12bit_datapath_g12a(void)
{
	/*after this step vd1 output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step vd1 output data is U12,*/
	vpp_set_vd1_ext_mod(0);
	vpp_set_vd1_bypass_dolby(1);

	if (is_meson_g12a_cpu() || is_meson_g12b_cpu()) {
		/*vd1 mux config*/
		vpp_set_vd1_mux0(0);
		vpp_set_vd1_mux1(0);
		vpp_set_vd1_mux4(0);
		vpp_set_vd1_mux2(0);
		vpp_set_vd1_mux5(0);
		vpp_set_vd1_preblend_mux(0);
		vpp_set_vd1_postblend_mux(0);
		vpp_set_vd1_postblend_en(1);
		vpp_set_vd1_gate(0x0);
		vpp_set_vd2_preblend_mux(0);
		vpp_set_vd2_postblend_mux(0);
		vpp_set_vd2_postblend_en(0);
		vpp_set_vd2_ext_mod(0);
		vpp_set_vd2_bypass_dolby(1);
		vpp_set_vd2_gate(0x0);
	}
}

/*g12a new add end*/

void vpp_set_12bit_datapath1(void)
{
	/*after this step output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);
	vpp_vd2_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step output data is U12*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_extend_mode_vd2(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,*/
	/*after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U12*/
	vpp_set_pre_s2u(U10_TO_U12);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U12_TO_S12);
	vpp_skip_vadj2_matrix3(0);

	/*skip eotf & oetf*/
	vpp_skip_eotf_matrix3(1);
	vpp_set_post_s2u(S12_TO_U12);
}

/*  u12 to postblending, u10 to post matirx */
void vpp_set_12bit_datapath2(void)
{
	/*after this step output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);
	vpp_vd2_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step output data is U12*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_extend_mode_vd2(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,*/
	/*after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U12*/
	vpp_set_pre_s2u(U10_TO_U12);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U12_TO_U10);
	vpp_skip_vadj2_matrix3(0);

	/*skip eotf & oetf*/
	vpp_skip_eotf_matrix3(0);
	vpp_set_post_s2u(U_TO_S_NULL);
}

void vpp_set_10bit_datapath1(void)
{
	/*after this step vd1 output data is U10*/
	vpp_vd1_if_bits_mode(BIT_MODE_10BIT_422);

	/*after this step vd1 output data is U12,*/
	/*only work when dolby_core1 bypass,high 10bit is valid*/
	vpp_extend_mode_core1(EXTMODE_LOW);
	vpp_bypas_core1(1);
	/* vpp_enable_vd2(0); */

	/*don't skip pps & super scaler*/
	vpp_skip_pps(0);
	vpp_set_pre_u2s(U_TO_S_NULL);

	/*enable vpp dither after preblend,*/
	/*after this step output data is U10*/
	vpp_dither_bits_comp_mode(1);
	vpp_enable_dither(1);

	/*after this step output data is U10*/
	vpp_set_pre_s2u(U_TO_S_NULL);

	/*don't skip vadj2 & post_matrix & wb*/
	vpp_set_post_u2s(U_TO_S_NULL);
	vpp_skip_vadj2_matrix3(0);

	/*don't skip eotf & oetf*/
	vpp_skip_eotf_matrix3(0);
	vpp_set_post_s2u(U_TO_S_NULL);

	if (is_meson_txhd_cpu()) {
		WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, 0, 11, 1);
		WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, 1, 9, 1);
		WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, 1, 10, 1);
	}
}

void  vpp_set_datapath(void)
{
	unsigned int w_data;

	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core1_en, 16, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, vd2_en, 17, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core2_en, 18, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, osd2_en, 19, 1);

	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core1_ext_mode, 20, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, vd2_ext_mode, 21, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, dolby_core2_ext_mode, 22, 1);
	WRITE_VPP_REG_BITS(VIU_MISC_CTRL1, osd2_ext_mode, 23, 1);

	/*if oetf_osd bypass ,the output can be 10/12bits*/
	/*bit3:1:10bit;0:12bit*/
	/*WRITE_VPP_REG_BITS(VIU_OSD1_CTRL_STAT, 0x0,  3, 1);*/

	/*config sr4 position:0:before postblend,1:after postblend*/
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 5, 1);

	/*config vpp dolby contrl*/
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel0, 0, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel1, 1, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dpath_sel2, 2, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dolby3_en, 3, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, core1_input_data_conv_mode, 6, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, core1_output_data_conv_mode, 7, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vd1_input_clip_mode, 8, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vd2_input_clip_mode, 9, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_output_clip_mode, 10, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dith_en, 12, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, vpp_dith_mode, 14, 1);
	WRITE_VPP_REG_BITS(VPP_DOLBY_CTRL, dolby3_path_sel, 16, 1);

	/*config u2s & s2u*/
	if (u2s_mode)
		w_data = (2 << 12) | (2 << 28);
	else
		w_data = (1 << 11) | ((1 << 11) << 16);

	WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA0, w_data, 0, 32);
	WRITE_VPP_REG_BITS(VPP_DAT_CONV_PARA1, w_data, 0, 32);

	/*config clipping after VKeystone*/
	WRITE_VPP_REG_BITS(VPP_CLIP_MISC0, 0xffffffff, 0, 32);
	WRITE_VPP_REG_BITS(VPP_CLIP_MISC1, 0x0, 0, 32);
}

void vpp_bitdepth_config(unsigned int bitdepth)
{
	if (bitdepth == 10)
		vpp_set_10bit_datapath1();
	else if (bitdepth == 12)
		vpp_set_12bit_datapath1();
	else if (bitdepth == 122)
		vpp_set_12bit_datapath2();
	else if (bitdepth == 123)
		vpp_set_12bit_datapath_g12a();
	else
		vpp_set_datapath();
}
void vpp_datapath_config(unsigned int node, unsigned int param1,
	unsigned int param2)
{
	switch (node) {
	case VD1_IF:
		vpp_vd1_if_bits_mode(param1);
		break;
	case VD2_IF:
		vpp_vd2_if_bits_mode(param1);
		break;
	case CORE1_EXTMODE:
		/*after this step vd1 output data is U12,*/
		/*only work when dolby_core1 bypass,high 10bit is valid*/
		vpp_extend_mode_core1(param1);
		vpp_bypas_core1(param2);
		break;
	case PRE_BLEDN_SWITCH:
		vpp_skip_pps(param1);
		break;
	case DITHER:
		/*enable vpp dither after preblend,*/
		/*after this step output data is U10*/
		vpp_dither_bits_comp_mode(param1);
		vpp_enable_dither(param2);
		break;
	case PRE_U2S:
		vpp_set_pre_u2s(param1);
		break;
	case PRE_S2U:
		vpp_set_pre_s2u(param1);
		break;
	case POST_BLEDN_SWITCH:
		break;
	case WATER_MARK_SWITCH:
		vpp_skip_vadj2_matrix3(param1);
		break;
	case GAIN_OFFSET_SWITCH:
		vpp_skip_eotf_matrix3(param1);
		break;
	case POST_U2S:
		vpp_set_post_u2s(param1);
		break;
	case POST_S2U:
		vpp_set_post_s2u(param1);
		break;
	case CHROMA_CORING:
		vpp_chroma_coring_en(param1);
		break;
	case BLACK_EXT:
		vpp_blackext_en(param1);
		break;
	case BLUESTRETCH:
		vpp_bluestretch_en(param1);
		break;
	case VADJ1:
		vpp_vadj1_en(param1);
		break;
	case VADJ2:
		vpp_vadj2_en(param1);
		break;
	default:
		pr_info("%s: unsupport cmd!\n", __func__);
		break;
	}
	pr_info("%s: node=%d,param1=%d,param2=%d\n", __func__,
		node, param1, param2);
}

void vpp_datapath_status(void)
{
	enum vd_if_bits_mode_e vd1_out_format, vd2_out_format;
	unsigned int core1_ext_mode, core1_bypass, pre_blend_switch;
	unsigned int dither_mode, dither_enable;
	unsigned int pre_u2s, pre_s2u, post_u2s, post_s2u;
	unsigned int watermark_switch, gainoff_switch;
	unsigned int chroma_coring_en, black_ext_en, bluestretch_en;
	unsigned int vadj1_en, vadj2_en;

	if (is_meson_g12a_cpu() || is_meson_g12b_cpu()) {
		vd1_out_format =
			READ_VPP_REG_BITS(G12_VD1_IF0_GEN_REG3, 8, 2);
		vd2_out_format =
			READ_VPP_REG_BITS(G12_VD2_IF0_GEN_REG3, 8, 2);
	} else {
		vd1_out_format =
			READ_VPP_REG_BITS(VD1_IF0_GEN_REG3, 8, 2);
		vd2_out_format =
			READ_VPP_REG_BITS(VD2_IF0_GEN_REG3, 8, 2);
	}

	core1_ext_mode = READ_VPP_REG_BITS(VIU_MISC_CTRL1, 20, 1);
	core1_bypass = READ_VPP_REG_BITS(VIU_MISC_CTRL1, 16, 1);
	pre_blend_switch = READ_VPP_REG_BITS(VPP_DOLBY_CTRL, 0, 1);
	dither_mode = READ_VPP_REG_BITS(VPP_DOLBY_CTRL, 14, 1);
	dither_enable = READ_VPP_REG_BITS(VPP_DOLBY_CTRL, 12, 1);
	pre_u2s = READ_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 16, 14);
	pre_s2u = READ_VPP_REG_BITS(VPP_DAT_CONV_PARA0, 0, 14);
	post_u2s = READ_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 16, 14);
	post_s2u = READ_VPP_REG_BITS(VPP_DAT_CONV_PARA1, 0, 14);
	watermark_switch = READ_VPP_REG_BITS(VPP_DOLBY_CTRL, 2, 1);
	gainoff_switch = READ_VPP_REG_BITS(VPP_DOLBY_CTRL, 1, 1);
	black_ext_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 3, 1);
	chroma_coring_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 4, 1);
	bluestretch_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 1);
	vadj1_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1);
	vadj2_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 2, 1);
	pr_info("vd1_out_format(%d):%d(0:8bit;1:10bit_yuv422;2:10bit_yuv444,3:10bit_yuv422_fullpack)\n",
		VD1_IF, vd1_out_format);
	pr_info("vd2_out_format(%d):%d(0:8bit;1:10bit_yuv422;2:10bit_yuv444,3:10bit_yuv422_fullpack)\n",
		VD2_IF, vd2_out_format);
	pr_info("core1_ext_mode(%d):%d(0:extend 2bit 0 in low bits;1:extend 2bit 0 in high bits)\n",
		CORE1_EXTMODE, core1_ext_mode);
	pr_info("core1_bypass(%d):%d(0:no bypass;1:bypass)\n",
		CORE1_EXTMODE, core1_bypass);
	pr_info("pre_blend_switch(%d):%d(0:pre_blend to dither;1:pre_blend to pre u2s)\n",
		PRE_BLEDN_SWITCH, pre_blend_switch);
	pr_info("dither_mode(%d):%d(0:cut high 2 bits;1:cut low 2 bits)\n",
		DITHER, dither_mode);
	pr_info("dither_enable(%d):%d(0:disable;1:enable)\n",
		DITHER, dither_enable);
	pr_info("watermark_switch(%d):%d(0:water_mark to post u2s;1:water_mark to Vkeystone)\n",
		WATER_MARK_SWITCH, watermark_switch);
	pr_info("gainoff_switch(%d):%d(0:gainoff to EOTF;1:gainoff to post s2u)\n",
		GAIN_OFFSET_SWITCH, gainoff_switch);
	pr_info("pre_u2s(%d):0x%x(0:NULL;0x2000:U12_TO_U10;0x800:U12_TO_S12)\n",
		PRE_U2S, pre_u2s);
	pr_info("post_u2s(%d):0x%x(0:NULL;0x2000:U12_TO_U10;0x800:U12_TO_S12)\n",
		POST_U2S, post_u2s);
	pr_info("pre_s2u(%d):0x%x(0:NULL;0x2000:U10_TO_U12;0x800:S12_TO_U12)\n",
		PRE_S2U, pre_s2u);
	pr_info("post_s2u(%d):0x%x(0:NULL;0x2000:U10_TO_U12;0x800:S12_TO_U12)\n",
		POST_S2U, post_s2u);
	pr_info("chroma_coring(%d):%d(0:disable;1:enable)\n",
		CHROMA_CORING, chroma_coring_en);
	pr_info("black_ext_en(%d):%d(0:disable;1:enable)\n",
		BLACK_EXT, black_ext_en);
	pr_info("bluestretch_en(%d):%d(0:disable;1:enable)\n",
		BLUESTRETCH, bluestretch_en);
	pr_info("vadj1_en(%d):%d(0:disable;1:enable)\n",
		VADJ1, vadj1_en);
	pr_info("vadj2_en(%d):%d(0:disable;1:enable)\n",
		VADJ2, vadj2_en);
}

