/*
 * drivers/amlogic/media/enhancement/amvecm/vlock.c
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
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/io.h>
#include "arch/vpp_regs.h"
#include "vlock.h"
#include "amvecm_vlock_regmap.h"
#include "amcm.h"

/* video lock */
/* 0:enc;1:pll;
 * 2:manual pll;
 * 3:manual_enc mode(only support lvds/vx1)
 */
unsigned int vlock_mode = VLOCK_MODE_MANUAL_PLL;
unsigned int vlock_en = 1;
/*
 *0:only support 50->50;60->60;24->24;30->30;
 *1:support 24/30/50/60/100/120 mix,such as 50->60;
 */
static unsigned int vlock_adapt;
static unsigned int vlock_dis_cnt_limit = 2;
static unsigned int vlock_delta_limit = 2;
/* [reg(0x3009) x linemax_num)]>>24 is the limit line of enc mode */
static signed int vlock_line_limit = 4;
/* 0x3009 default setting for 2 line(1080p-output) is 0x8000 */
static unsigned int vlock_capture_limit = 0x8000;
static unsigned int vlock_debug;
static unsigned int vlock_dynamic_adjust = 1;

static unsigned int vlock_sync_limit_flag;
static unsigned int vlock_state = VLOCK_STATE_NULL;/*1/2/3:vlock step*/
static enum vmode_e pre_vmode = VMODE_INIT_NULL;
static enum vframe_source_type_e pre_source_type =
		VFRAME_SOURCE_TYPE_OTHERS;
static enum vframe_source_mode_e pre_source_mode =
		VFRAME_SOURCE_MODE_OTHERS;
static unsigned int pre_input_freq;
static unsigned int pre_output_freq;
static unsigned int vlock_dis_cnt;
static char pre_vout_mode[64];
static bool vlock_vmode_changed;
static unsigned int pre_hiu_reg_m;
static unsigned int pre_hiu_reg_frac;
static signed int pre_enc_max_line;
static signed int pre_enc_max_pixel;
static unsigned int enc_max_line_addr;
static unsigned int enc_max_pixel_addr;
static unsigned int enc_video_mode_addr;
static unsigned int enc_video_mode_adv_addr;
static unsigned int enc_max_line_switch_addr;

static unsigned int vlock_dis_cnt_no_vf;
static unsigned int vlock_dis_cnt_no_vf_limit = 5;

static unsigned int vlock_log_cnt;/*cnt base: vlock_log_s*/
static unsigned int vlock_log_size = 60;/*size base: vlock_log_s*/
static unsigned int vlock_log_delta_frac = 100;
static unsigned int vlock_log_delta_ivcnt = 0xff;
static unsigned int vlock_log_delta_ovcnt = 0xff;
static unsigned int vlock_log_delta_vcnt = 0xff;
static unsigned int vlock_log_last_ivcnt;
static unsigned int vlock_log_last_ovcnt;
static unsigned int vlock_log_delta_m;
static unsigned int vlock_log_delta_en;
module_param(vlock_log_size, uint, 0664);
MODULE_PARM_DESC(vlock_log_size, "\n vlock_log_size\n");
module_param(vlock_log_cnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_cnt, "\n vlock_log_cnt\n");
module_param(vlock_log_delta_frac, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_frac, "\n vlock_log_delta_frac\n");
module_param(vlock_log_delta_m, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_m, "\n vlock_log_delta_m\n");
module_param(vlock_log_delta_en, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_en, "\n vlock_log_delta_en\n");

module_param(vlock_log_delta_ivcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_ivcnt, "\n vlock_log_delta_ivcnt\n");

module_param(vlock_log_delta_ovcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_ovcnt, "\n vlock_log_delta_ovcnt\n");

module_param(vlock_log_delta_vcnt, uint, 0664);
MODULE_PARM_DESC(vlock_log_delta_vcnt, "\n vlock_log_delta_vcnt\n");

static unsigned int vlock_frac;
module_param(vlock_frac, uint, 0664);
MODULE_PARM_DESC(vlock_frac, "\n vlock_frac\n");
static unsigned int vlock_frac_delta;
module_param(vlock_frac_delta, uint, 0664);
MODULE_PARM_DESC(vlock_frac_delta, "\n vlock_frac_delta\n");

static unsigned int vlock_latch_en_cnt = 20;
module_param(vlock_latch_en_cnt, uint, 0664);
MODULE_PARM_DESC(vlock_latch_en_cnt, "\n vlock_latch_en_cnt\n");

static unsigned int vlock_log_en;
struct vlock_log_s **vlock_log;

static signed int err_accum;
static unsigned int last_i_vsync;

int amvecm_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(amvecm_hiu_reg_base+((reg - 0x1000)<<2));
	return 0;
}

int amvecm_hiu_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (amvecm_hiu_reg_base+((reg - 0x1000)<<2)));
	return 0;
}
static int amvecm_hiu_reg_write_bits(unsigned int reg, unsigned int value,
		unsigned int start, unsigned int len)
{
	unsigned int rd_val;

	amvecm_hiu_reg_read(reg, &rd_val);
	amvecm_hiu_reg_write(reg, ((rd_val &
	     ~(((1L << (len)) - 1) << (start))) |
	    (((value) & ((1L << (len)) - 1)) << (start))));
	return 0;
}


static unsigned int vlock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	unsigned int duration = vf->duration;

	if ((vf->source_type != VFRAME_SOURCE_TYPE_CVBS) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_HDMI))
		ret_hz = 0;
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (duration != 0)
			ret_hz = (96000 + duration/2)/duration;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if (vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
			ret_hz = 60;
		else if ((vf->source_mode == VFRAME_SOURCE_MODE_PAL) ||
			(vf->source_mode == VFRAME_SOURCE_MODE_SECAM))
			ret_hz = 50;
		else
			ret_hz = 0;
	}
	return ret_hz;
}

static unsigned int vlock_check_output_hz(unsigned int sync_duration_num)
{
	unsigned int ret_hz = 0;

	switch (sync_duration_num) {
	case 24:
		ret_hz = 24;
		break;
	case 30:
		ret_hz = 30;
		break;
	case 50:
		ret_hz = 50;
		break;
	case 60:
		ret_hz = 60;
		break;
	case 100:
		ret_hz = 100;
		break;
	case 120:
		ret_hz = 120;
		break;
	default:
		ret_hz = 0;
		break;
	}
	return ret_hz;
}
/*vlock is support eq_after gxbb,but which is useful only for tv chip
 *after gxl,the enable/disable reg_bit is changed
 */
static void vlock_enable(bool enable)
{
	unsigned int tmp_value;

	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);

	if (is_meson_gxtvbb_cpu()) {
		if (vlock_mode == VLOCK_MODE_MANUAL_PLL) {
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 0, 20, 1);
			/*vlsi suggest config:don't enable load signal,
			 *on gxtvbb this load signal will effect SSG,
			 *which may result in flashes black
			 */
			if (is_meson_gxtvbb_cpu() &&
				(((tmp_value >> 21) & 0x3) != 2))
				amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6,
					2, 21, 2);
		} else
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6,
				enable, 20, 1);
	} else if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		if (vlock_mode == VLOCK_MODE_MANUAL_PLL)
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5, 0, 3, 1);
		else
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5,
				enable, 3, 1);
	}
}
static void vlock_setting(struct vframe_s *vf,
		unsigned int input_hz, unsigned int output_hz)
{
	unsigned int freq_hz = 0, hiu_reg_value_2_addr = HHI_HDMI_PLL_CNTL2;
	unsigned int reg_value = 0, hiu_reg_value, hiu_reg_value_2;
	unsigned int hiu_m_val, hiu_frac_val, temp_value;

	amvecm_hiu_reg_write(HHI_VID_LOCK_CLK_CNTL, 0x80);
	if ((vlock_mode == VLOCK_MODE_AUTO_ENC) ||
		(vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
		(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)) {
		if ((vlock_debug & VLOCK_DEBUG_FLUSH_REG_DIS) == 0)
			am_set_regmap(&vlock_enc_setting);
		/*vlock line limit*/
		WRITE_VPP_REG(VPU_VLOCK_OUTPUT0_CAPT_LMT, vlock_capture_limit);
		/* VLOCK_CNTL_EN disable */
		vlock_enable(0);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN enable */
		if ((vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
			(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)) {
			/*auto vlock off*/
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				0, 13, 1);
			WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 1, 14, 1);
		} else {
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				1, 13, 1);
			WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 0, 14, 1);
		}
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 30, 1);
		/*clear accum1 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
		/*clear accum0 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
		/*@20180314 new add*/
		/*
		 *set input & output freq
		 *bit0~7:input freq
		 *bit8~15:output freq
		 */
		if ((vf->type_original & VIDTYPE_TYPEMASK) &&
			(vlock_mode != VLOCK_MODE_MANUAL_SOFT_ENC))
			input_hz = input_hz >> 1;
		freq_hz = input_hz | (output_hz << 8);
		WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, freq_hz, 0, 16);
		/*
		 *Ifrm_cnt_mod:0x3001(bit23~16);
		 *(output_freq/input_freq)*Ifrm_cnt_mod must be integer
		 */
		if (vlock_adapt == 0)
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 1, 16, 8);
		else
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				input_hz, 16, 8);
		temp_value = READ_VPP_REG(enc_max_line_addr);
		WRITE_VPP_REG_BITS(VPU_VLOCK_OROW_OCOL_MAX, temp_value, 0, 14);
		temp_value = READ_VPP_REG(enc_max_pixel_addr);
		WRITE_VPP_REG_BITS(VPU_VLOCK_OROW_OCOL_MAX, temp_value, 16, 14);
		WRITE_VPP_REG_BITS(VPU_VLOCK_ADJ_EN_SYNC_CTRL,
			vlock_latch_en_cnt, 8, 8);
		WRITE_VPP_REG_BITS(enc_video_mode_addr, 0, 15, 1);
	}
	if ((vlock_mode == VLOCK_MODE_AUTO_PLL) ||
		(vlock_mode == VLOCK_MODE_MANUAL_PLL)) {
		/* av pal in,1080p60 hdmi out as default */
		if ((vlock_debug & VLOCK_DEBUG_FLUSH_REG_DIS) == 0)
			am_set_regmap(&vlock_pll_setting);
		/*
		 *set input & output freq
		 *bit0~7:input freq
		 *bit8~15:output freq
		 */
		if (vf->type_original & VIDTYPE_TYPEMASK)
			input_hz = input_hz >> 1;
		freq_hz = input_hz | (output_hz << 8);
		WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, freq_hz, 0, 16);
		/*
		 *Ifrm_cnt_mod:0x3001(bit23~16);
		 *(output_freq/input_freq)*Ifrm_cnt_mod must be integer
		 */
		if (vlock_adapt == 0)
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 1, 16, 8);
		else
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				input_hz, 16, 8);
		/*set PLL M_INT;PLL M_frac*/
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,9),12,9); */
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &hiu_reg_value);
		amvecm_hiu_reg_read(hiu_reg_value_2_addr,
			&hiu_reg_value_2);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
			hiu_m_val = hiu_reg_value & 0x1FF;
			hiu_frac_val = hiu_reg_value_2 & 0x3FF;
			if (hiu_reg_value_2 & 0x800) {
				hiu_m_val -= 1;
				if (hiu_reg_value_2 & 0x400)
					hiu_m_val -= 1;
				hiu_frac_val = 0x400 -
					((~(hiu_frac_val - 1)) & 0x3ff);
			} else if (hiu_reg_value_2 & 0x400) {
				hiu_m_val += 1;
			}
			reg_value = (hiu_m_val << 12)
				+ (hiu_frac_val << 2);
		}
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, reg_value, 0, 21);

		/*enable vlock to adj pll*/
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* VLOCK_CNTL_EN enable */
		vlock_enable(1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 29, 1);
	}
	/* vlock module output goes to which module */
	switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
	case 0:/* ENCL */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 26, 2);
		break;
	case 1:/* ENCI */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 26, 2);
		break;
	case 2: /* ENCP */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 26, 2);
		break;
	default:
		break;
	}
	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS))
		/* Input Vsync source select from tv-decoder */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 16, 3);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		/* Input Vsync source select from hdmi-rx */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 16, 3);
	WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 31, 1);
}
void vlock_vmode_check(void)
{
	const struct vinfo_s *vinfo;
	unsigned int tmp_value, hiu_reg_addr;
	char cur_vout_mode[64];

	if (vlock_en == 0)
		return;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
	else
		hiu_reg_addr = HHI_HDMI_PLL_CNTL2;
	vinfo = get_current_vinfo();
	vlock_vmode_changed = 0;
	memset(cur_vout_mode, 0, sizeof(cur_vout_mode));
	if ((vinfo->name == NULL) ||
		(strlen(vinfo->name) > sizeof(cur_vout_mode)))
		return;
	strcpy(cur_vout_mode, vinfo->name);

	if (strcmp(cur_vout_mode, pre_vout_mode) != 0) {
		if ((vlock_mode == VLOCK_MODE_MANUAL_PLL) ||
			(vlock_mode == VLOCK_MODE_AUTO_PLL)) {
			amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
			pre_hiu_reg_frac = tmp_value & 0xfff;
			amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
			pre_hiu_reg_m = tmp_value & 0x1ff;
		}
		if ((vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
			(vlock_mode == VLOCK_MODE_AUTO_ENC) ||
			(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)) {
			switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL,
				0, 2)) {
			case 0:
				enc_max_line_addr = ENCL_VIDEO_MAX_LNCNT;
				enc_max_pixel_addr = ENCL_VIDEO_MAX_PXCNT;
				enc_video_mode_addr = ENCL_VIDEO_MODE;
				enc_video_mode_adv_addr = ENCL_VIDEO_MODE_ADV;
				enc_max_line_switch_addr =
					ENCL_MAX_LINE_SWITCH_POINT;
				break;
			#if 0 /*enc mode not adapt to ENCP and ENCT*/
			case 2:
				enc_max_line_addr = ENCP_VIDEO_MAX_LNCNT;
				enc_max_pixel_addr = ENCP_VIDEO_MAX_PXCNT;
				enc_video_mode_addr = ENCP_VIDEO_MODE;
				enc_max_line_switch_addr =
					ENCP_MAX_LINE_SWITCH_POINT;
				break;
			case 3:
				enc_max_line_addr = ENCT_VIDEO_MAX_LNCNT;
				enc_max_pixel_addr = ENCT_VIDEO_MAX_PXCNT;
				enc_video_mode_addr = ENCT_VIDEO_MODE;
				enc_max_line_switch_addr =
					ENCT_MAX_LINE_SWITCH_POINT;
				break;
			#endif
			default:
				enc_max_line_addr = ENCL_VIDEO_MAX_LNCNT;
				enc_max_pixel_addr = ENCL_VIDEO_MAX_PXCNT;
				enc_video_mode_addr = ENCL_VIDEO_MODE;
				enc_video_mode_adv_addr = ENCL_VIDEO_MODE_ADV;
				enc_max_line_switch_addr =
					ENCL_MAX_LINE_SWITCH_POINT;
				break;
			}
			pre_enc_max_line = READ_VPP_REG(enc_max_line_addr);
			pre_enc_max_pixel = READ_VPP_REG(enc_max_pixel_addr);
			vlock_capture_limit = ((1 << 12) * vlock_line_limit) /
				vinfo->vtotal + 1;
			vlock_capture_limit <<= 12;
		}
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("[%s]:vout mode changed:%s==>%s\n",
				__func__, pre_vout_mode, cur_vout_mode);
		memset(pre_vout_mode, 0, sizeof(pre_vout_mode));
		strcpy(pre_vout_mode, cur_vout_mode);
		vlock_vmode_changed = 1;
	}
}
static void vlock_disable_step1(void)
{
	unsigned int m_reg_value, tmp_value, enc_max_line, enc_max_pixel;
	unsigned int hiu_reg_addr;

	/* VLOCK_CNTL_EN disable */
	vlock_enable(0);
	vlock_vmode_check();
	if ((vlock_mode == VLOCK_MODE_MANUAL_PLL) ||
			(vlock_mode == VLOCK_MODE_AUTO_PLL)) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
			hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
		else
			hiu_reg_addr = HHI_HDMI_PLL_CNTL2;
		amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
		m_reg_value = tmp_value & 0xfff;
		if ((m_reg_value != pre_hiu_reg_frac) &&
			(pre_hiu_reg_m != 0)) {
			tmp_value = (tmp_value & 0xfffff000) |
				(pre_hiu_reg_frac & 0xfff);
			amvecm_hiu_reg_write(hiu_reg_addr, tmp_value);
		}
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
		m_reg_value = tmp_value & 0x1ff;
		if ((m_reg_value != pre_hiu_reg_m) &&
			(pre_hiu_reg_m != 0)) {
			tmp_value = (tmp_value & 0xfffffe00) |
				(pre_hiu_reg_m & 0x1ff);
			amvecm_hiu_reg_write(HHI_HDMI_PLL_CNTL, tmp_value);
		}
	}
	if ((vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
		(vlock_mode == VLOCK_MODE_AUTO_ENC) ||
		(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)) {
		err_accum = 0;
		WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 0, 14, 1);
		enc_max_line = READ_VPP_REG(enc_max_line_addr);
		enc_max_pixel = READ_VPP_REG(enc_max_pixel_addr);
		if (!(vlock_debug & VLOCK_DEBUG_ENC_LINE_ADJ_DIS) &&
			(enc_max_line != pre_enc_max_line) &&
			(pre_enc_max_line != 0) && (enc_max_line_addr != 0))
			WRITE_VPP_REG(enc_max_line_addr, pre_enc_max_line);
		if (!(vlock_debug & VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS) &&
			(enc_max_pixel != pre_enc_max_pixel) &&
			(enc_max_line_addr != 0) && (enc_max_pixel_addr != 0)) {
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				0x1fff, 0, 13);
			WRITE_VPP_REG(enc_max_pixel_addr, pre_enc_max_pixel);
		}
	}
	vlock_dis_cnt = vlock_dis_cnt_limit;
	memset(pre_vout_mode, 0, sizeof(pre_vout_mode));
	pre_vmode = VMODE_INIT_NULL;
	pre_source_type = VFRAME_SOURCE_TYPE_OTHERS;
	pre_source_mode = VFRAME_SOURCE_MODE_OTHERS;
	pre_input_freq = 0;
	pre_output_freq = 0;
	vlock_state = VLOCK_STATE_DISABLE_STEP1_DONE;
}

static void vlock_disable_step2(void)
{
	unsigned int temp_val;
	/* need delay to disable follow regs(vlsi suggest!!!) */
	if (vlock_dis_cnt > 0)
		vlock_dis_cnt--;
	if (vlock_dis_cnt == 0) {
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT,
				0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* disable vid_lock_en */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 31, 1);
		vlock_state = VLOCK_STATE_DISABLE_STEP2_DONE;
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &temp_val);
		if (is_meson_gxtvbb_cpu() && (((temp_val >> 21) & 0x3) != 0))
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 0, 21, 2);
	}
}
static void vlock_enable_step1(struct vframe_s *vf, struct vinfo_s *vinfo,
	unsigned int input_hz, unsigned int output_hz)
{
	vlock_setting(vf, input_hz, output_hz);
	if (vlock_debug & VLOCK_DEBUG_INFO) {
		pr_info("%s:vmode/source_type/source_mode/input_freq/output_freq:\n",
			__func__);
		pr_info("\t%d/%d/%d/%d/%d=>%d/%d/%d/%d/%d\n",
			pre_vmode, pre_source_type, pre_source_mode,
			pre_input_freq, pre_output_freq,
			vinfo->mode, vf->source_type, vf->source_mode,
			input_hz, output_hz);
	}
	pre_vmode = vinfo->mode;
	pre_source_type = vf->source_type;
	pre_source_mode = vf->source_mode;
	pre_input_freq = input_hz;
	pre_output_freq = output_hz;
	vlock_sync_limit_flag = 0;
	vlock_vmode_changed = 0;
	vlock_dis_cnt = 0;
	vlock_state = VLOCK_STATE_ENABLE_STEP1_DONE;
	vlock_log_cnt = 0;
}

void vlock_log_start(void)
{
	unsigned int i;

	vlock_log = kmalloc_array(vlock_log_size, sizeof(struct vlock_log_s *),
		GFP_KERNEL);
	if (vlock_log == NULL)
		return;
	for (i = 0; i < vlock_log_size; i++) {
		vlock_log[i] = kmalloc(sizeof(struct vlock_log_s), GFP_KERNEL);
		if (vlock_log[i] == NULL) {
			pr_info("%s:vlock_log[%d] kmalloc fail!!!\n",
				__func__, i);
			return;
		}
	}
	vlock_log_en = 1;
	pr_info("%s done\n", __func__);
}
void vlock_log_stop(void)
{
	unsigned int i;

	for (i = 0; i < vlock_log_size; i++)
		kfree(vlock_log[i]);
	kfree(vlock_log);
	vlock_log_en = 0;
	pr_info("%s done\n", __func__);
}
void vlock_log_print(void)
{
	unsigned int i, j;

	if (vlock_log[0] == NULL)
		return;
	for (i = 0; i < vlock_log_size; i++) {
		pr_info("\n*******[%d]pll_m:0x%x,pll_frac:0x%x*******\n",
			i, vlock_log[i]->pll_m, vlock_log[i]->pll_frac);
		pr_info("*******enc_line_max:0x%x,line_num_adj:%d*******\n",
			vlock_log[i]->enc_line_max, vlock_log[i]->line_num_adj);
		pr_info("*******enc_pixel_max:0x%x,pixel_num_adj:%d*******\n",
			vlock_log[i]->enc_pixel_max,
			vlock_log[i]->pixel_num_adj);
		pr_info("*******nT0:%d(0x%x),vdif_err:%d(0x%x),err_sum:%d(0x%x),margin:%d(0x%x)*******\n",
			vlock_log[i]->nT0, vlock_log[i]->nT0,
			vlock_log[i]->vdif_err, vlock_log[i]->vdif_err,
			vlock_log[i]->err_sum, vlock_log[i]->err_sum,
			vlock_log[i]->margin, vlock_log[i]->margin);
		for (j = 0; j < VLOCK_REG_NUM;) {
			if ((j%8 == 0) && ((j + 7) < VLOCK_REG_NUM)) {
				pr_info("0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x\n",
					vlock_log[i]->vlock_regs[j],
					vlock_log[i]->vlock_regs[j+1],
					vlock_log[i]->vlock_regs[j+2],
					vlock_log[i]->vlock_regs[j+3],
					vlock_log[i]->vlock_regs[j+4],
					vlock_log[i]->vlock_regs[j+5],
					vlock_log[i]->vlock_regs[j+6],
					vlock_log[i]->vlock_regs[j+7]);
				j += 8;
			} else {
				pr_info("0x%08x\t",
					vlock_log[i]->vlock_regs[j]);
				j++;
			}

		}
	}
	pr_info("%s done\n", __func__);
}
void vlock_reg_get(void)
{
	unsigned int i;

	for (i = 0; i < VLOCK_REG_NUM; i++)
		vlock_log[vlock_log_cnt]->vlock_regs[i] =
			READ_VPP_REG(0x3000 + i);
}
static void vlock_enable_step3_enc(void)
{
	unsigned int line_num = 0, enc_max_line = 0, polity_line_num = 0;
	unsigned int pixel_num = 0, enc_max_pixel = 0, polity_pixel_num = 0;

	/*vlock pixel num adjust*/
	if (!(vlock_debug & VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS)) {
		polity_pixel_num = READ_VPP_REG_BITS(VPU_VLOCK_RO_LINE_PIX_ADJ,
			13, 1);
		pixel_num = READ_VPP_REG_BITS(VPU_VLOCK_RO_LINE_PIX_ADJ, 0, 14);
		if (polity_pixel_num) {
			pixel_num = (~(pixel_num - 1)) & 0x3fff;
			enc_max_pixel = pre_enc_max_pixel - pixel_num;
		} else {
			enc_max_pixel = pre_enc_max_pixel + pixel_num;
		}
		if (enc_max_pixel > 0x1fff)
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				pixel_num, 0, 13);
		else
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				enc_max_pixel, 0, 13);
	}
	/*vlock line num adjust*/
	if (!(vlock_debug & VLOCK_DEBUG_ENC_LINE_ADJ_DIS)) {
		polity_line_num = READ_VPP_REG_BITS(VPU_VLOCK_RO_LINE_PIX_ADJ,
			29, 1);
		line_num = READ_VPP_REG_BITS(VPU_VLOCK_RO_LINE_PIX_ADJ, 16, 14);
		if (polity_line_num) {
			line_num = (~(line_num - 1)) & 0x3fff;
			enc_max_line = pre_enc_max_line - line_num;
		} else {
			enc_max_line = pre_enc_max_line + line_num;
		}
		if (enc_max_pixel > 0x1fff)
			enc_max_line += 1;
		WRITE_VPP_REG(enc_max_line_addr, enc_max_line);
	}

	if (vlock_log_en && (vlock_log_cnt < vlock_log_size)) {
		vlock_log[vlock_log_cnt]->enc_line_max = enc_max_line;
		vlock_log[vlock_log_cnt]->line_num_adj = line_num;
		vlock_log[vlock_log_cnt]->enc_pixel_max = enc_max_pixel;
		vlock_log[vlock_log_cnt]->pixel_num_adj = pixel_num;
		vlock_reg_get();
		vlock_log_cnt++;
	}
}

static void vlock_enable_step3_soft_enc(void)
{
	unsigned int ia, oa;
	signed int margin;
	signed int pixel_adj;
	signed int err, vdif_err;
	signed int nT0, tfrac;
	signed int line_adj;

	signed int reg_errclip_rate =
		READ_VPP_REG_BITS(VPU_VLOCK_LOOP0_CTRL0, 24, 8);
	signed int reg_accum_lmt = READ_VPP_REG(VPU_VLOCK_LOOP0_ACCUM_LMT);
	signed int reg_capt_gain =
		READ_VPP_REG_BITS(VPU_VLOCK_LOOP0_CTRL0, 0, 8);
	signed int reg_capt_rs =
		READ_VPP_REG_BITS(VPU_VLOCK_LOOP0_CTRL0, 8, 4);
	signed int reg_capt_lmt = READ_VPP_REG(VPU_VLOCK_OUTPUT0_CAPT_LMT);

	if (last_i_vsync == 0) {
		last_i_vsync = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
		return;
	}
	ia = (READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST) + last_i_vsync + 1) / 2;
	oa = READ_VPP_REG(VPU_VLOCK_RO_VS_O_DIST);

	if (ia < oa)
		margin = ia >> 8;
	else
		margin = oa >> 8;

	margin *= reg_errclip_rate;
	margin  = (margin >> 26) ? 0x3ffffff : margin;
	margin  = margin << 5;
	margin  = (margin >> 28) ? 0xfffffff : margin;

	err = oa - ia;

	if ((err < margin) && (err > -margin))
		err = err;
	else
		err = 0;

	vdif_err = err_accum + err;
	vdif_err = (vdif_err >  (1<<30)) ?  (1<<30) :
	(vdif_err < -(1<<30)) ? -(1<<30) : vdif_err;

	err_accum = (vdif_err >  reg_accum_lmt) ?  reg_accum_lmt :
	(vdif_err < -reg_accum_lmt) ? -reg_accum_lmt : vdif_err;

	nT0 = (err_accum * reg_capt_gain) >> reg_capt_rs;

	nT0 = -nT0;

	nT0 = (nT0 >  reg_capt_lmt) ?  reg_capt_lmt :
	(nT0 < -reg_capt_lmt) ? -reg_capt_lmt : nT0;

	nT0 = (nT0 * pre_enc_max_line) >> 10;

	line_adj = nT0 >> 14;

	if (line_adj > vlock_line_limit)
		line_adj = vlock_line_limit;
	else if (line_adj < -vlock_line_limit)
		line_adj = -vlock_line_limit;

	tfrac = nT0 - (line_adj << 14);

	pixel_adj = (tfrac * pre_enc_max_pixel + 8192) >> 14;

	if ((pixel_adj + pre_enc_max_pixel) > 0x1fff) {
		line_adj = line_adj + 1;
		pixel_adj = pixel_adj - pre_enc_max_pixel;
	}

	pixel_adj &= 0xfffffffe;/*clean bit0*/

	WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, pre_enc_max_line + line_adj);
	WRITE_VPP_REG(ENCL_MAX_LINE_SWITCH_POINT,
		pre_enc_max_pixel + pixel_adj);

	last_i_vsync = READ_VPP_REG(0x3011);

	if (vlock_log_en && (vlock_log_cnt < vlock_log_size)) {
		vlock_log[vlock_log_cnt]->enc_line_max = pre_enc_max_line;
		vlock_log[vlock_log_cnt]->line_num_adj = line_adj;
		vlock_log[vlock_log_cnt]->enc_pixel_max = pre_enc_max_pixel;
		vlock_log[vlock_log_cnt]->pixel_num_adj = pixel_adj;
		vlock_log[vlock_log_cnt]->nT0 = nT0;
		vlock_log[vlock_log_cnt]->vdif_err = err;
		vlock_log[vlock_log_cnt]->err_sum = err_accum;
		vlock_log[vlock_log_cnt]->margin = margin;
		vlock_reg_get();
		vlock_log_cnt++;
	}
}

static void vlock_enable_step3_pll(void)
{
	unsigned int m_reg_value, tmp_value, abs_val, hiu_reg_addr;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		hiu_reg_addr = HHI_HDMI_PLL_CNTL1;
	else
		hiu_reg_addr = HHI_HDMI_PLL_CNTL2;

	/*vs_i*/
	tmp_value = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
	abs_val = abs(vlock_log_last_ivcnt - tmp_value);
	if ((abs_val > vlock_log_delta_ivcnt) && (vlock_log_delta_en&(1<<0)))
		pr_info("%s: abs_ivcnt over 0x%x:0x%x(last:0x%x,cur:0x%x)\n",
			__func__, vlock_log_delta_ivcnt,
			abs_val, vlock_log_last_ivcnt, tmp_value);
	vlock_log_last_ivcnt = tmp_value;
	/*vs_o*/
	tmp_value = READ_VPP_REG(VPU_VLOCK_RO_VS_O_DIST);
	abs_val = abs(vlock_log_last_ovcnt - tmp_value);
	if ((abs_val > vlock_log_delta_ovcnt) && (vlock_log_delta_en&(1<<1)))
		pr_info("%s: abs_ovcnt over 0x%x:0x%x(last:0x%x,cur:0x%x)\n",
			__func__, vlock_log_delta_ovcnt,
			abs_val, vlock_log_last_ivcnt, tmp_value);
	vlock_log_last_ovcnt = tmp_value;
	/*delta_vs*/
	abs_val = abs(vlock_log_last_ovcnt - vlock_log_last_ivcnt);
	if ((abs_val > vlock_log_delta_vcnt) && (vlock_log_delta_en&(1<<2)))
		pr_info("%s: abs_vcnt over 0x%x:0x%x(ivcnt:0x%x,ovcnt:0x%x)\n",
			__func__, vlock_log_delta_vcnt,
			abs_val, vlock_log_last_ivcnt, vlock_log_last_ovcnt);

	m_reg_value = READ_VPP_REG(VPU_VLOCK_RO_M_INT_FRAC);
	if (vlock_log_en && (vlock_log_cnt < vlock_log_size)) {
		vlock_log[vlock_log_cnt]->pll_frac = (m_reg_value & 0xfff) >> 2;
		vlock_log[vlock_log_cnt]->pll_m = (m_reg_value >> 16) & 0x1ff;
		vlock_reg_get();
		vlock_log_cnt++;
	}
	if (m_reg_value == 0) {
		vlock_state = VLOCK_STATE_ENABLE_FORCE_RESET;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s:vlock work abnormal! force reset vlock\n",
				__func__);
		return;
	}
	/*vlsi suggest config:don't enable load signal,
	 *on gxtvbb this load signal will effect SSG,
	 *which may result in flashes black
	 */
	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);
	if (is_meson_gxtvbb_cpu() && (((tmp_value >> 21) & 0x3) != 2))
		amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 2, 21, 2);
	/*frac*/
	amvecm_hiu_reg_read(hiu_reg_addr, &tmp_value);
	abs_val = abs(((m_reg_value & 0xfff) >> 2) - (tmp_value & 0xfff));
	if ((abs_val >= vlock_log_delta_frac) && (vlock_log_delta_en&(1<<3)))
		pr_info("vlock frac delta:%d(0x%x,0x%x)\n",
			abs_val, ((m_reg_value & 0xfff) >> 2),
			(tmp_value & 0xfff));
	if (abs_val >= vlock_delta_limit) {
		tmp_value = (tmp_value & 0xfffff000) |
			((m_reg_value & 0xfff) >> 2);
		amvecm_hiu_reg_write(hiu_reg_addr, tmp_value);
	}
	/*m*/
	amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL, &tmp_value);
	abs_val = abs(((m_reg_value >> 16) & 0x1ff) - (tmp_value & 0x1ff));
	if ((abs_val > vlock_log_delta_m) && (vlock_log_delta_en&(1<<4)))
		pr_info("vlock m delta:%d(0x%x,0x%x)\n",
			abs_val, ((m_reg_value >> 16) & 0x1ff),
			(tmp_value & 0x1ff));
	if (((m_reg_value >> 16) & 0x1ff) != (tmp_value & 0x1ff)) {
		tmp_value = (tmp_value & 0xfffffe00) |
			((m_reg_value >> 16) & 0x1ff);
		amvecm_hiu_reg_write(HHI_HDMI_PLL_CNTL, tmp_value);
	}
}
/* won't change this function internal seqence,
 * if really need change,please be carefull
 */
void amve_vlock_process(struct vframe_s *vf)
{
	struct vinfo_s *vinfo;
	unsigned int input_hz, output_hz, input_vs_cnt;

	if (vecm_latch_flag & FLAG_VLOCK_DIS) {
		vlock_disable_step1();
		vlock_en = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_DIS;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("[%s]disable vlock module!!!\n", __func__);
		return;
	}
	vinfo = get_current_vinfo();
	input_hz = vlock_check_input_hz(vf);
	output_hz = vlock_check_output_hz(vinfo->sync_duration_num);
	vlock_dis_cnt_no_vf = 0;
	if (vecm_latch_flag & FLAG_VLOCK_EN) {
		vlock_enable_step1(vf, vinfo, input_hz, output_hz);
		vlock_en = 1;
		vecm_latch_flag &= ~FLAG_VLOCK_EN;
	}
	if (vlock_state == VLOCK_STATE_DISABLE_STEP1_DONE) {
		vlock_disable_step2();
		return;
	}
	if (vlock_en == 1) {
		/*@20170728 new add work aound method:
		 **disable vlock for interlace input && hdmitx output
		 */
		if (((input_hz != output_hz) && (vlock_adapt == 0)) ||
			(input_hz == 0) || (output_hz == 0) ||
			(((vf->type_original & VIDTYPE_TYPEMASK)
				!= VIDTYPE_PROGRESSIVE) &&
				is_meson_txlx_package_962E())) {
			if ((vlock_state != VLOCK_STATE_DISABLE_STEP2_DONE) &&
				(vlock_state != VLOCK_STATE_NULL))
				vlock_disable_step1();
			if (vlock_debug & VLOCK_DEBUG_INFO)
				pr_info("[%s]auto disable vlock module for no support case!!!\n",
					__func__);
			return;
		}
		vlock_vmode_check();
		if ((vinfo->mode != pre_vmode) ||
			(vf->source_type != pre_source_type) ||
			(vf->source_mode != pre_source_mode) ||
			(input_hz != pre_input_freq) ||
			(output_hz != pre_output_freq) ||
			vlock_vmode_changed ||
			(vlock_state == VLOCK_STATE_ENABLE_FORCE_RESET))
			vlock_enable_step1(vf, vinfo, input_hz, output_hz);
		if ((vlock_sync_limit_flag < 10) &&
			(vlock_state >= VLOCK_STATE_ENABLE_STEP1_DONE)) {
			vlock_sync_limit_flag++;
			if ((vlock_sync_limit_flag <= 3) &&
				(vlock_mode == VLOCK_MODE_MANUAL_ENC)) {
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
				WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM0, 0);
				WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM1, 0);
			}
			if ((vlock_sync_limit_flag == 4) &&
				(vlock_mode == VLOCK_MODE_MANUAL_ENC)) {
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
			}
		}
		if ((vlock_sync_limit_flag == 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP1_DONE)) {
			/*input_vs_cnt =*/
			/*READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,*/
			/*	0, 28);*/
			input_vs_cnt = XTAL_VLOCK_CLOCK/input_hz;
			if (vf->type_original & VIDTYPE_TYPEMASK)
				input_vs_cnt = input_vs_cnt << 1;
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
					input_vs_cnt*125/100);
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
					input_vs_cnt*70/100);
			/*cal accum1 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
			/*cal accum0 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
			vlock_state = VLOCK_STATE_ENABLE_STEP2_DONE;
		} else if (vlock_dynamic_adjust &&
			(vlock_sync_limit_flag > 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP2_DONE) &&
			(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
			((vlock_mode == VLOCK_MODE_MANUAL_PLL) ||
			(vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
			(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC))) {
			if (vlock_mode == VLOCK_MODE_MANUAL_ENC)
				vlock_enable_step3_enc();
			else if (vlock_mode == VLOCK_MODE_MANUAL_PLL)
				vlock_enable_step3_pll();
			else if (vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)
				vlock_enable_step3_soft_enc();
		}
		if (((vlock_mode == VLOCK_MODE_AUTO_PLL) ||
			(vlock_mode == VLOCK_MODE_AUTO_ENC)) &&
			vlock_log_en && (vlock_log_cnt < vlock_log_size) &&
			(vlock_debug & VLOCK_DEBUG_AUTO_MODE_LOG_EN)) {
			vlock_reg_get();
			vlock_log_cnt++;
		}
	}
}

void amve_vlock_resume(void)
{
	if ((vlock_en == 0) || (vlock_state ==
		VLOCK_STATE_DISABLE_STEP2_DONE) ||
		(vlock_state == VLOCK_STATE_NULL))
		return;
	if (vlock_state == VLOCK_STATE_DISABLE_STEP1_DONE) {
		vlock_disable_step2();
		return;
	}
	vlock_dis_cnt_no_vf++;
	if ((vlock_state != VLOCK_STATE_DISABLE_STEP2_DONE) &&
		(vlock_dis_cnt_no_vf > vlock_dis_cnt_no_vf_limit)) {
		vlock_disable_step1();
		vlock_dis_cnt_no_vf = 0;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("[%s]auto disable vlock module for no vframe & run disable step1.!!!\n",
				__func__);
	}
	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info("[%s]auto disable vlock module for no vframe!!!\n",
			__func__);
	if (vlock_dynamic_adjust &&
		(vlock_sync_limit_flag > 5) &&
		(vlock_state == VLOCK_STATE_ENABLE_STEP2_DONE) &&
		(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
		((vlock_mode == VLOCK_MODE_MANUAL_PLL) ||
		(vlock_mode == VLOCK_MODE_MANUAL_ENC) ||
		(vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC))) {
		if (vlock_mode == VLOCK_MODE_MANUAL_ENC)
			vlock_enable_step3_enc();
		else if (vlock_mode == VLOCK_MODE_MANUAL_PLL)
			vlock_enable_step3_pll();
		else if (vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC)
			vlock_enable_step3_soft_enc();
	}
}
void vlock_param_set(unsigned int val, enum vlock_param_e sel)
{
	switch (sel) {
	case VLOCK_EN:
		vlock_en = val;
		break;
	case VLOCK_ADAPT:
		vlock_adapt = val;
		break;
	case VLOCK_MODE:
		vlock_mode = val;
		break;
	case VLOCK_DIS_CNT_LIMIT:
		vlock_dis_cnt_limit = val;
		break;
	case VLOCK_DELTA_LIMIT:
		vlock_delta_limit = val;
		break;
	case VLOCK_DEBUG:
		vlock_debug = val;
		break;
	case VLOCK_DYNAMIC_ADJUST:
		vlock_dynamic_adjust = val;
		break;
	case VLOCK_DIS_CNT_NO_VF_LIMIT:
		vlock_dis_cnt_no_vf_limit = val;
		break;
	case VLOCK_LINE_LIMIT:
		vlock_line_limit = val;
		break;
	default:
		pr_info("%s:unknown vlock param:%d\n", __func__, sel);
		break;
	}

}
void vlock_status(void)
{
	pr_info("\n current vlock parameters status:\n");
	pr_info("vlock_mode:%d\n", vlock_mode);
	pr_info("vlock_en:%d\n", vlock_en);
	pr_info("vlock_adapt:%d\n", vlock_adapt);
	pr_info("vlock_dis_cnt_limit:%d\n", vlock_dis_cnt_limit);
	pr_info("vlock_delta_limit:%d\n", vlock_delta_limit);
	pr_info("vlock_debug:0x%x\n", vlock_debug);
	pr_info("vlock_dynamic_adjust:%d\n", vlock_dynamic_adjust);
	pr_info("vlock_state:%d\n", vlock_state);
	pr_info("vlock_sync_limit_flag:%d\n", vlock_sync_limit_flag);
	pr_info("pre_vmode:%d\n", pre_vmode);
	pr_info("pre_hiu_reg_m:0x%x\n", pre_hiu_reg_m);
	pr_info("pre_hiu_reg_frac:0x%x\n", pre_hiu_reg_frac);
	pr_info("pre_enc_max_line:0x%x\n", pre_enc_max_line);
	pr_info("pre_enc_max_pixel:0x%x\n", pre_enc_max_pixel);
	pr_info("vlock_dis_cnt:%d\n", vlock_dis_cnt);
	pr_info("pre_vout_mode:%s\n", pre_vout_mode);
	pr_info("vlock_dis_cnt_no_vf:%d\n", vlock_dis_cnt_no_vf);
	pr_info("vlock_dis_cnt_no_vf_limit:%d\n", vlock_dis_cnt_no_vf_limit);
	pr_info("enc_max_line_addr:0x%x\n", enc_max_line_addr);
	pr_info("enc_max_pixel_addr:0x%x\n", enc_max_pixel_addr);
	pr_info("enc_video_mode_addr:0x%x\n", enc_video_mode_addr);
	pr_info("enc_max_line_switch_addr:0x%x\n", enc_max_line_switch_addr);
	pr_info("vlock_capture_limit:0x%x\n", vlock_capture_limit);
	pr_info("vlock_line_limit:%d\n", vlock_line_limit);
}
void vlock_reg_dump(void)
{
	unsigned int addr;

	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000); addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(0xd0100000+(addr<<2)), addr,
			READ_VPP_REG(addr));
}
/*work around method for vlock process hdmirx input interlace source.@20170803
 **for interlace input,TOP and BOT have one line delta,
 **Which may cause vlock output jitter.
 **The work around method is that changing vlock input select.
 **So input vsync cnt may be stable,
 **However the input hz should be div 2 as vlock input setting
 */
void vdin_vlock_input_sel(unsigned int type,
	enum vframe_source_type_e source_type)
{
	type = type & VIDTYPE_TYPEMASK;
	if ((type == 0) || (vlock_mode == VLOCK_MODE_MANUAL_SOFT_ENC))
		return;
	if (type == VIDTYPE_INTERLACE_TOP) {
		if ((source_type == VFRAME_SOURCE_TYPE_TUNER) ||
			(source_type == VFRAME_SOURCE_TYPE_CVBS))
			/* Input Vsync source select from tv-decoder */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 16, 3);
		else if (source_type == VFRAME_SOURCE_TYPE_HDMI)
			/* Input Vsync source select from hdmi-rx */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 16, 3);
	} else
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 7, 16, 3);
}
EXPORT_SYMBOL(vdin_vlock_input_sel);
/*video lock end*/

