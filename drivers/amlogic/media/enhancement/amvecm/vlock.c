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
#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif
#include "arch/vpp_regs.h"
#include "vlock.h"
#include "amvecm_vlock_regmap.h"
#include "amcm.h"

/* video lock */
/* 0:off;
 * 1:auto enc;
 * 2:auto pll;
 * 4:manual pll;
 * 8:manual_enc mode(only support lvds/vx1)
 */
enum VLOCK_MD vlock_mode = VLOCK_MODE_MANUAL_PLL;
unsigned int vlock_en = 1;
/*
 *0:only support 50->50;60->60;24->24;30->30;
 *1:support 24/30/50/60/100/120 mix,such as 50->60;
 */
static unsigned int vlock_adapt;
static unsigned int vlock_dis_cnt_limit = 2;
static unsigned int vlock_delta_limit = 2;
/*limit vlock pll m adj*/
static unsigned int vlock_pll_m_limit = 1;
/*for 24MHZ clock, 50hz input, delta value (10) of(0x3011-0x3012) == 0.001HZ */
static unsigned int vlock_delta_cnt_limit = 10;
/*hdmi support enable default,cvbs support not good,need debug with vlsi*/
static unsigned int vlock_support = (VLOCK_SUPPORT_HDMI | VLOCK_SUPPORT_CVBS);
static unsigned int vlock_enc_stable_flag;
static unsigned int vlock_pll_stable_cnt;
static unsigned int vlock_pll_adj_limit;
static unsigned int vlock_pll_val_last;
static unsigned int vlock_intput_type;
/* [reg(0x3009) x linemax_num)]>>24 is the limit line of enc mode
 * change from 4 to 3,for 4 may cause shake issue for 60.3hz input
 */
static signed int vlock_line_limit = 3;
static unsigned int vlock_enc_adj_limit;
/* 0x3009 default setting for 2 line(1080p-output) is 0x8000 */
static unsigned int vlock_capture_limit = 0x10000/*0x8000*/;
static unsigned int vlock_debug;
static unsigned int vlock_dynamic_adjust = 1;

static unsigned int vlock_sync_limit_flag;
static unsigned int vlock_state = VLOCK_STATE_NULL;/*1/2/3:vlock step*/
static enum vframe_source_type_e pre_source_type =
		VFRAME_SOURCE_TYPE_OTHERS;
static enum vframe_source_mode_e pre_source_mode =
		VFRAME_SOURCE_MODE_OTHERS;
static unsigned int pre_input_freq;
static unsigned int pre_output_freq;
static unsigned int vlock_dis_cnt;
static bool vlock_vmode_changed;
static unsigned int vlock_vmode_change_status;
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
static unsigned int hhi_pll_reg_m;
static unsigned int hhi_pll_reg_frac;

static struct stvlock_sig_sts vlock;

/*static unsigned int hhi_pll_reg_vlock_ctl;*/
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
	/**val = readl(amvecm_hiu_reg_base+((reg - 0x1000)<<2));*/
	*val = aml_read_hiubus(reg);
	/*pr_info("vlock rd hiu reg:0x%x,0x%x\n", (reg - 0x1000), *val);*/
	return 0;
}

int amvecm_hiu_reg_write(unsigned int reg, unsigned int val)
{
	/*writel(val, (amvecm_hiu_reg_base+((reg - 0x1000)<<2)));*/
	/*pr_info("vlock rd hiu reg:0x%x,0x%x\n", (reg - 0x1000), val);*/
	aml_write_hiubus(reg, val);
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

u32 vlock_get_panel_pll_m(void)
{
	u32 val;

	amvecm_hiu_reg_read(hhi_pll_reg_m, &val);
	return val;
}

void vlock_set_panel_pll_m(u32 val)
{
	amvecm_hiu_reg_write(hhi_pll_reg_m, val);
}

u32 vlock_get_panel_pll_frac(void)
{
	u32 val;

	amvecm_hiu_reg_read(hhi_pll_reg_frac, &val);
	return val;
}

void vlock_set_panel_pll_frac(u32 val)
{
	amvecm_hiu_reg_write(hhi_pll_reg_frac, val);
}


/*returen 1: use phase lock*/
int phase_lock_check(void)
{
	unsigned int ret = 0;

	ret = READ_VPP_REG_BITS(VPU_VLOCK_RO_LCK_FRM, 17, 1);
	return ret;
}

static unsigned int vlock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	unsigned int duration = vf->duration;

	if ((vf->source_type != VFRAME_SOURCE_TYPE_CVBS) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_HDMI))
		ret_hz = 0;
	else if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI) &&
		(vlock_support & VLOCK_SUPPORT_HDMI)) {
		if (duration != 0)
			ret_hz = (96000 + duration/2)/duration;
	} else if ((vf->source_type == VFRAME_SOURCE_TYPE_CVBS) &&
		(vlock_support & VLOCK_SUPPORT_CVBS)) {
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

static unsigned int vlock_check_output_hz(unsigned int sync_duration_num,
	unsigned int sync_duration_den)
{
	unsigned int ret_hz = 0;
	unsigned int tempHz;

	tempHz = (sync_duration_num*100)/sync_duration_den;

	switch (tempHz) {
	case 2400:
		ret_hz = 24;
		break;
	case 3000:
		ret_hz = 30;
		break;
	case 5000:
		ret_hz = 50;
		break;
	case 6000:
		ret_hz = 60;
		break;
	case 10000:
		ret_hz = 100;
		break;
	case 12000:
		ret_hz = 120;
		break;
	default:
		ret_hz = 0;
		break;
	}

	if ((ret_hz == 0) && (vlock_debug & VLOCK_DEBUG_INFO))
		pr_info("sync_duration_num:%d\n", sync_duration_num);

	return ret_hz;
}
/*vlock is support eq_after gxbb,but which is useful only for tv chip
 *after gxl,the enable/disable reg_bit is changed
 */
static void vlock_enable(bool enable)
{
	unsigned int tmp_value;

	if (is_meson_gxtvbb_cpu()) {
		if (vlock_mode & VLOCK_MODE_MANUAL_PLL) {
			amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);
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
		if (vlock_mode & VLOCK_MODE_MANUAL_PLL)
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5, 0, 3, 1);
		else
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL5,
				enable, 3, 1);
	} else if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
		/*reset*/
		if (!(vlock_mode & VLOCK_MODE_MANUAL_PLL)) {
			/*reset*/
			/*WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);*/
			/*WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);*/
		}

		if (!enable) {
			/*amvecm_hiu_reg_write_bits(*/
			/*	HHI_HDMI_PLL_VLOCK_CNTL, 0, 0, 3);*/

			/*WRITE_VPP_REG(VPU_VLOCK_CTRL, 0);*/
		}
	}

	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info(">>>[%s] (%d)\n", __func__, enable);
}
static void vlock_hw_reinit(struct vlock_regs_s *vlock_regs, unsigned int len)
{
	unsigned int i;

	if ((vlock_debug & VLOCK_DEBUG_FLUSH_REG_DIS) || (vlock_regs == NULL))
		return;
	for (i = 0; i < len; i++)
		WRITE_VPP_REG(vlock_regs[i].addr, vlock_regs[i].val);

	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info("[%s]\n", __func__);
}
static void vlock_setting(struct vframe_s *vf,
		unsigned int input_hz, unsigned int output_hz)
{
	unsigned int freq_hz = 0;
	unsigned int reg_value = 0, hiu_reg_value, hiu_reg_value_2;
	unsigned int hiu_m_val = 0, hiu_frac_val = 0, temp_value;

	if (vlock_debug & VLOCK_DEBUG_INFO) {
		pr_info(">>>[%s]\n", __func__);
		pr_info("inputHz:%d,outputHz:%d\n", input_hz, output_hz);
		pr_info("type_original:0x%x\n", vf->type_original);
	}
	amvecm_hiu_reg_write(HHI_VID_LOCK_CLK_CNTL, 0x80);
	if (IS_ENC_MODE(vlock_mode)) {
		/*init default config for enc mode*/
		vlock_hw_reinit(vlock_enc_setting, VLOCK_DEFAULT_REG_SIZE);
		/*vlock line limit*/
		/*WRITE_VPP_REG(VPU_VLOCK_OUTPUT0_CAPT_LMT,*/
		/*	vlock_capture_limit);*/

		/* VLOCK_CNTL_EN disable */
		vlock_enable(0);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN enable */
		if ((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
			VLOCK_MODE_MANUAL_SOFT_ENC))) {
			/*auto vlock off*/
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				0, 13, 1);
			WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 1, 14, 1);
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				pre_enc_max_pixel, 0, 13);
		} else {
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				1, 13, 1);
			WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 0, 14, 1);
		}
		/* enable to adjust enc */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 30, 1);
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1) {
			/*clear accum1 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
			/*clear accum0 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
		}
		/*@20180314 new add*/
		/*
		 *set input & output freq
		 *bit0~7:input freq
		 *bit8~15:output freq
		 */
		if ((vf->type_original & VIDTYPE_TYPEMASK) &&
			!(vlock_mode & VLOCK_MODE_MANUAL_SOFT_ENC)) {
			/*tl1 fix i problem*/
			if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1)
				input_hz = input_hz >> 1;
			else
				WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
						1, 28, 1);
		}
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
		WRITE_VPP_REG_BITS(VPU_VLOCK_OROW_OCOL_MAX,
			temp_value + 1, 0, 14);
		temp_value = READ_VPP_REG(enc_max_pixel_addr);
		WRITE_VPP_REG_BITS(VPU_VLOCK_OROW_OCOL_MAX,
			temp_value + 1, 16, 14);
		WRITE_VPP_REG_BITS(VPU_VLOCK_ADJ_EN_SYNC_CTRL,
			vlock_latch_en_cnt, 8, 8);
		WRITE_VPP_REG_BITS(enc_video_mode_addr, 1, 15, 1);
	}
	if ((vlock_mode & (VLOCK_MODE_AUTO_PLL |
		VLOCK_MODE_MANUAL_PLL))) {
		/* av pal in,1080p60 hdmi out as default */
		vlock_hw_reinit(vlock_pll_setting, VLOCK_DEFAULT_REG_SIZE);
		/*
		 *set input & output freq
		 *bit0~7:input freq
		 *bit8~15:output freq
		 */
		if (vf->type_original & VIDTYPE_TYPEMASK) {
			if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1)
				input_hz = input_hz >> 1;
			else
				WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
						1, 28, 1);
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
						0, 28, 1);
		}
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
		/*amvecm_hiu_reg_read(hhi_pll_reg_m, &hiu_reg_value);*/
		/*amvecm_hiu_reg_read(hhi_pll_reg_frac,*/
		/*	&hiu_reg_value_2);*/
		hiu_reg_value = vlock_get_panel_pll_m();
		hiu_reg_value_2 = vlock_get_panel_pll_frac();

		if (vlock_debug & VLOCK_DEBUG_INFO) {
			pr_info("hhi_pll_reg_m:0x%x\n", hiu_reg_value);
			pr_info("hhi_pll_reg_frac:0x%x\n", hiu_reg_value_2);
		}

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			hiu_m_val = hiu_reg_value & 0xff;
			/*discard low 5 bits*/
			hiu_frac_val = (hiu_reg_value_2 >> 5) & 0xfff;
			reg_value = (hiu_m_val << 12) + hiu_frac_val;
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
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
		} else {
			pr_info("err: m f value\n");
		}
		if (vlock_debug & VLOCK_DEBUG_INFO) {
			pr_info("hiu_m_val=0x%x\n", hiu_m_val);
			pr_info("hiu_frac_val=0x%x\n", hiu_frac_val);
		}
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, reg_value, 0, 21);
		/*vlock_pll_adj_limit = (reg_value * 0x8000) >> 24;*/
		vlock_pll_adj_limit = reg_value >> VLOCK_PLL_ADJ_LIMIT;
		vlock_pll_val_last = ((reg_value & 0x3fff000) << 4) |
			(reg_value & 0x3fff);

		/*enable vlock to adj pll*/
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 0, 13, 1);
		/* disable to adjust enc */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* VLOCK_CNTL_EN enable */
		vlock_enable(1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 29, 1);
	}

	/*initial phase lock setting*/
	if (vlock.dtdata->vlk_phlock_en) {
		vlock_hw_reinit(vlock_pll_phase_setting, VLOCK_PHASE_REG_SIZE);
		/*disable pll lock*/
		/*WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 3, 1);*/

		/*enable pll mode and enc mode phase lock*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 3, 0, 2);

		/*reset*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
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

	/*enable vlock*/
	WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 31, 1);
}
void vlock_vmode_check(void)
{
	const struct vinfo_s *vinfo;
	unsigned int t0, t1;

	if (vlock_en == 0)
		return;

	vinfo = get_current_vinfo();
	vlock_vmode_changed = 0;
	if ((vlock_vmode_change_status == VOUT_EVENT_MODE_CHANGE) ||
		(pre_hiu_reg_m == 0)) {
		if (vlock_mode & (VLOCK_MODE_MANUAL_PLL |
			VLOCK_MODE_AUTO_PLL)) {
			/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &t0);*/
			/*amvecm_hiu_reg_read(hhi_pll_reg_m, &t1);*/
			t0 = vlock_get_panel_pll_m();
			t1 = vlock_get_panel_pll_frac();
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				pre_hiu_reg_frac = (t0 >> 5) & 0xfff;
				pre_hiu_reg_m = t1 & 0xff;
			} else {
				pre_hiu_reg_frac = t0 & 0xfff;
				pre_hiu_reg_m = t1 & 0x1ff;
			}
		}
		if ((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
			VLOCK_MODE_AUTO_ENC |
			VLOCK_MODE_MANUAL_SOFT_ENC))) {
			#if 0
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
			#endif
			pre_enc_max_line = READ_VPP_REG(enc_max_line_addr);
			pre_enc_max_pixel = READ_VPP_REG(enc_max_pixel_addr);
			vlock_capture_limit = ((1 << 12) * vlock_line_limit) /
				vinfo->vtotal + 1;
			vlock_capture_limit <<= 12;
		}
		vlock_vmode_change_status = 0;
		vlock_vmode_changed = 1;
	}
}
static void vlock_disable_step1(void)
{
	unsigned int m_reg_value, tmp_value, enc_max_line, enc_max_pixel;

	/* VLOCK_CNTL_EN disable */
	vlock_enable(0);
	vlock_vmode_check();
	if ((vlock_mode & (VLOCK_MODE_MANUAL_PLL |
		VLOCK_MODE_AUTO_PLL |
		VLOCK_MODE_MANUAL_SOFT_ENC))) {
		if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
			#if 0
			amvecm_hiu_reg_read(hhi_pll_reg_frac,
				&tmp_value);
			m_reg_value = (tmp_value >> 5) & 0xfff;
			if (m_reg_value != pre_hiu_reg_frac) {
				tmp_value = (tmp_value & 0xfffff000) |
					(pre_hiu_reg_frac & 0xfff);
				amvecm_hiu_reg_write(hhi_pll_reg_frac,
					tmp_value);
				pr_info("restore f value=0x%x\n", tmp_value);
			}
			amvecm_hiu_reg_read(hhi_pll_reg_m, &tmp_value);
			m_reg_value = tmp_value & 0xff;
			if ((m_reg_value != pre_hiu_reg_m) &&
				(pre_hiu_reg_m != 0)) {
				tmp_value = (tmp_value & 0xffffff00) |
					(pre_hiu_reg_m & 0xff);
				amvecm_hiu_reg_write(hhi_pll_reg_m, tmp_value);
				pr_info("restore m value=0x%x\n", tmp_value);
			}
			#endif

			#if 1
			/*restore the orginal pll setting*/
			/*amvecm_hiu_reg_read(hhi_pll_reg_m, &tmp_value);*/
			tmp_value = vlock_get_panel_pll_m();
			m_reg_value = tmp_value & 0xff;
			if (m_reg_value != (vlock.val_m & 0xff))
				vlock_set_panel_pll_m(vlock.val_m);
				/*amvecm_hiu_reg_write(hhi_pll_reg_m,*/
				/*	vlock.val_m);*/

			/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &tmp_value);*/
			tmp_value = vlock_get_panel_pll_frac();
			m_reg_value = tmp_value & 0x1ffff;
			if (m_reg_value != (vlock.val_frac & 0xfff))
				vlock_set_panel_pll_frac(vlock.val_frac);
				/*amvecm_hiu_reg_write(hhi_pll_reg_frac,*/
				/*	vlock.val_frac);*/
			pr_info("restore orignal m,f value\n");
			#endif
		} else {
			/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &tmp_value);*/
			tmp_value = vlock_get_panel_pll_frac();
			m_reg_value = tmp_value & 0xfff;
			if (m_reg_value != pre_hiu_reg_frac) {
				tmp_value = (tmp_value & 0xfffff000) |
					(pre_hiu_reg_frac & 0xfff);
				/*amvecm_hiu_reg_write(hhi_pll_reg_frac,*/
				/*	tmp_value);*/
				vlock_set_panel_pll_frac(tmp_value);
			}
			/*amvecm_hiu_reg_read(hhi_pll_reg_m, &tmp_value);*/
			tmp_value = vlock_get_panel_pll_m();
			m_reg_value = tmp_value & 0x1ff;
			if ((m_reg_value != pre_hiu_reg_m) &&
				(pre_hiu_reg_m != 0)) {
				tmp_value = (tmp_value & 0xfffffe00) |
					(pre_hiu_reg_m & 0x1ff);
				/*amvecm_hiu_reg_write(hhi_pll_reg_m, */
				/*tmp_value);*/
				vlock_set_panel_pll_m(tmp_value);
			}
		}
	}
	if ((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
		VLOCK_MODE_AUTO_ENC |
		VLOCK_MODE_MANUAL_SOFT_ENC))) {
		err_accum = 0;
		WRITE_VPP_REG_BITS(enc_video_mode_adv_addr, 0, 14, 1);
		enc_max_line = READ_VPP_REG(enc_max_line_addr);
		enc_max_pixel = READ_VPP_REG_BITS(enc_max_line_switch_addr,
			0, 13);
		if (!(vlock_debug & VLOCK_DEBUG_ENC_LINE_ADJ_DIS) &&
			(enc_max_line != pre_enc_max_line) &&
			(pre_enc_max_line != 0) && (enc_max_line_addr != 0))
			WRITE_VPP_REG(enc_max_line_addr, pre_enc_max_line);
		if (!(vlock_debug & VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS) &&
			(enc_max_pixel != pre_enc_max_pixel) &&
			(enc_max_line_switch_addr != 0))
			WRITE_VPP_REG_BITS(enc_max_line_switch_addr,
				0x1fff, 0, 13);
	}
	vlock_dis_cnt = vlock_dis_cnt_limit;
	pre_source_type = VFRAME_SOURCE_TYPE_OTHERS;
	pre_source_mode = VFRAME_SOURCE_MODE_OTHERS;
	pre_input_freq = 0;
	pre_output_freq = 0;
	vlock_state = VLOCK_STATE_DISABLE_STEP1_DONE;
	if (vlock_mode & VLOCK_MODE_MANUAL_MIX_PLL_ENC) {
		vlock_mode &= ~VLOCK_MODE_MANUAL_MIX_PLL_ENC;
		vlock_mode |= VLOCK_MODE_MANUAL_PLL;
	}
	vlock_pll_stable_cnt = 0;
	vlock_enc_stable_flag = 0;
	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info(">>>[%s]\n", __func__);
}

static bool vlock_disable_step2(void)
{
	unsigned int temp_val;
	bool ret = false;

	/* need delay to disable follow regs(vlsi suggest!!!) */
	if (vlock_dis_cnt > 0)
		vlock_dis_cnt--;
	else if (vlock_dis_cnt == 0) {
		if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
			amvecm_hiu_reg_write_bits(
				HHI_HDMI_PLL_VLOCK_CNTL, 0x4, 0, 3);
			amvecm_hiu_reg_write_bits(
				HHI_HDMI_PLL_VLOCK_CNTL, 0x0, 0, 3);
		}

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
		if (is_meson_gxtvbb_cpu()) {
			amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &temp_val);
			if (((temp_val >> 21) & 0x3) != 0)
				amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6,
					0, 21, 2);
		}
		ret = true;

		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info(">>>[%s]\n", __func__);
	}

	return ret;
}
static void vlock_enable_step1(struct vframe_s *vf, struct vinfo_s *vinfo,
	unsigned int input_hz, unsigned int output_hz)
{
	if (vinfo->vtotal && output_hz)
		vlock_enc_adj_limit = (XTAL_VLOCK_CLOCK * vlock_line_limit) /
			(vinfo->vtotal * output_hz);
	vlock_setting(vf, input_hz, output_hz);
	if (vlock_debug & VLOCK_DEBUG_INFO) {
		pr_info("%s:vmode/source_type/source_mode/input_freq/output_freq:\n",
			__func__);
		pr_info("\t%d/%d/%d/%d=>%d/%d/%d/%d\n",
			pre_source_type, pre_source_mode,
			pre_input_freq, pre_output_freq,
			vf->source_type, vf->source_mode,
			input_hz, output_hz);
	}
	pre_source_type = vf->source_type;
	pre_source_mode = vf->source_mode;
	pre_input_freq = input_hz;
	pre_output_freq = output_hz;
	vlock_sync_limit_flag = 0;
	vlock_vmode_changed = 0;
	vlock_dis_cnt = 0;
	/*vlock_state = VLOCK_STATE_ENABLE_STEP1_DONE;*/
	vlock_pll_stable_cnt = 0;
	vlock_log_cnt = 0;
	vlock_enc_stable_flag = 0;

	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info(">>>[%s]\n", __func__);
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
	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info(">>>[%s]\n", __func__);
}

static void vlock_enable_step3_soft_enc(void)
{
	unsigned int ia, oa, tmp_value;
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

	if ((ia == 0) || (oa == 0)) {
		vlock_state = VLOCK_STATE_ENABLE_FORCE_RESET;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s:vlock enc work abnormal! force reset vlock\n",
				__func__);
		return;
	}

	/*check enc adj limit*/
	tmp_value = abs(ia - oa);
	if (tmp_value > vlock_enc_adj_limit) {
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s:vlock enc abs[%d](ia[%d]-oa[%d]) over limit,don't do adj\n",
				 __func__, tmp_value, ia, oa);
		return;
	}

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

	tfrac = nT0 - (line_adj << 14);

	pixel_adj = (tfrac * pre_enc_max_pixel + 8192) >> 14;

	while ((pixel_adj < 0) && (pre_enc_max_pixel > 0)) {
		line_adj = line_adj - 1;
		pixel_adj = pre_enc_max_pixel + pixel_adj;
	}

	while (((pixel_adj + pre_enc_max_pixel) > 0x1fff) &&
		(pre_enc_max_pixel > 0)) {
		line_adj = line_adj + 1;
		pixel_adj = pixel_adj - pre_enc_max_pixel;
	}

	if (line_adj > vlock_line_limit)
		line_adj = vlock_line_limit;
	else if (line_adj < -vlock_line_limit)
		line_adj = -vlock_line_limit;

	pixel_adj &= 0xfffffffe;/*clean bit0*/

	if (vlock_enc_stable_flag++ > VLOCK_ENC_STABLE_CNT)
		vlock_enc_stable_flag = VLOCK_ENC_STABLE_CNT;
	if ((vlock_enc_stable_flag < VLOCK_ENC_STABLE_CNT) &&
		(!(vlock_debug & VLOCK_DEBUG_ENC_LINE_ADJ_DIS)))
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT,
			pre_enc_max_line + line_adj);
	if (!(vlock_debug & VLOCK_DEBUG_ENC_PIXEL_ADJ_DIS))
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
	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info(">>>[%s]\n", __func__);
}

/*check pll adj value (0x3020),otherwise may cause blink*/
static void vlock_pll_adj_limit_check(unsigned int *pll_val)
{
	unsigned int m_reg_value, pll_cur, pll_last, pll_ret;

	m_reg_value = *pll_val;

	if (m_reg_value != 0) {
		pll_cur = ((m_reg_value & 0x3fff0000) >> 4) |
			(m_reg_value & 0x3fff);
		pll_last = ((vlock_pll_val_last & 0x3fff0000) >> 4) |
			(vlock_pll_val_last & 0x3fff);
		if (abs(pll_cur - pll_last) > vlock_pll_adj_limit) {
			if (pll_cur > pll_last)
				pll_ret = pll_last + vlock_pll_adj_limit;
			else
				pll_ret = pll_last - vlock_pll_adj_limit;
			*pll_val = ((pll_ret & 0x3fff000) << 4) |
				(pll_ret & 0x3fff);
		}
	}
}

static void vlock_enable_step3_pll(void)
{
	unsigned int m_reg_value, tmp_value, abs_val;
	unsigned int ia, oa, abs_cnt;
	unsigned int pre_m, new_m, tar_m, org_m;

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
		#if 0
		vlock_log[vlock_log_cnt]->pll_frac =
			(vlock_pll_val_last & 0xfff) >> 2;
		vlock_log[vlock_log_cnt]->pll_m =
			(vlock_pll_val_last >> 16) & 0x1ff;
		#else
		/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &tmp_value);*/
		tmp_value = vlock_get_panel_pll_frac();
		vlock_log[vlock_log_cnt]->pll_frac = tmp_value;

		/*amvecm_hiu_reg_read(hhi_pll_reg_m, &tmp_value);*/
		tmp_value = vlock_get_panel_pll_m();
		vlock_log[vlock_log_cnt]->pll_m = tmp_value;
		#endif
		vlock_reg_get();
		vlock_log_cnt++;
	}
	if (m_reg_value == 0) {
		vlock_state = VLOCK_STATE_ENABLE_FORCE_RESET;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s:vlock pll work abnormal! force reset vlock\n",
				__func__);
		return;
	}
	/*check adjust delta limit*/
	if (vlock.dtdata->vlk_hwver < vlock_hw_ver2)
		vlock_pll_adj_limit_check(&m_reg_value);

	/*vlsi suggest config:don't enable load signal,
	 *on gxtvbb this load signal will effect SSG,
	 *which may result in flashes black
	 */
	if (is_meson_gxtvbb_cpu()) {
		amvecm_hiu_reg_read(HHI_HDMI_PLL_CNTL6, &tmp_value);
		if (((tmp_value >> 21) & 0x3) != 2)
			amvecm_hiu_reg_write_bits(HHI_HDMI_PLL_CNTL6, 2, 21, 2);
	}

	/* add delta count check
	 *for interlace input need div 2
	 *0:progressive type
	 *1:interlace type
	 */
	if (vlock_intput_type &&
		(vlock.dtdata->vlk_hwver < vlock_hw_ver2))
		ia = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST) / 2;
	else
		ia = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
	oa = READ_VPP_REG(VPU_VLOCK_RO_VS_O_DIST);
	abs_cnt = abs(ia - oa);
	if (abs_cnt > (oa / 3)) {
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s:vlock input cnt abnormal!!\n", __func__);
		return;
	}
	/*frac*/
	/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &tmp_value);*/
	tmp_value = vlock_get_panel_pll_frac();
	if (vlock.dtdata->vlk_hwver < vlock_hw_ver2) {
		abs_val = abs(((m_reg_value & 0xfff) >> 2) -
			(tmp_value & 0xfff));
		if ((abs_val >= vlock_log_delta_frac) &&
			(vlock_log_delta_en&(1<<3)))
			pr_info("vlock frac delta:%d(0x%x,0x%x)\n",
				abs_val, ((m_reg_value & 0xfff) >> 2),
				(tmp_value & 0xfff));
		if ((abs_val >= vlock_delta_limit) &&
			(abs_cnt > vlock_delta_cnt_limit)) {
			tmp_value = (tmp_value & 0xfffff000) |
				((m_reg_value & 0xfff) >> 2);
			/*amvecm_hiu_reg_write(hhi_pll_reg_frac, tmp_value);*/
			vlock_set_panel_pll_frac(tmp_value);
			vlock_pll_val_last &= 0xffff0000;
			vlock_pll_val_last |= (m_reg_value & 0xfff);
		}
		/*check stable by diff frac*/
		if ((abs_val < (2 * vlock_delta_limit)) &&
			(abs_cnt < vlock_enc_adj_limit))
			vlock_pll_stable_cnt++;
		else
			vlock_pll_stable_cnt = 0;
	} else {
		abs_val = abs((tmp_value & 0x1ffff) -
			((m_reg_value & 0xfff) << 5));

		if (abs_val > (50 << 5))
			tmp_value = ((tmp_value & 0xfffe0000) |
				(((tmp_value & 0x1ffff) +
				((m_reg_value & 0xfff) << 5)) >> 1));
		else
			tmp_value = (tmp_value & 0xfffe0000) |
						((m_reg_value & 0xfff) << 5);

		/*16:0*/
		/*amvecm_hiu_reg_write(hhi_pll_reg_frac, tmp_value);*/
		vlock_set_panel_pll_frac(tmp_value);
	}

	/*m*/
	/*amvecm_hiu_reg_read(hhi_pll_reg_m, &tmp_value);*/
	tmp_value = vlock_get_panel_pll_m();
	if (vlock.dtdata->vlk_hwver < vlock_hw_ver2) {
		abs_val = abs(((m_reg_value >> 16) & 0xff) -
			(pre_hiu_reg_m & 0xff));
		if ((abs_val > vlock_log_delta_m) &&
			(vlock_log_delta_en&(1<<4)))
			pr_info("vlock m delta:%d(0x%x,0x%x)\n",
				abs_val, ((m_reg_value >> 16) & 0x1ff),
				(tmp_value & 0x1ff));
		if ((abs_val <= vlock_pll_m_limit) &&
			(((m_reg_value >> 16) & 0x1ff) !=
				(tmp_value & 0x1ff)) &&
			(abs_cnt > vlock_delta_cnt_limit)) {
			tmp_value = (tmp_value & 0xfffffe00) |
				((m_reg_value >> 16) & 0x1ff);
			/*amvecm_hiu_reg_write(hhi_pll_reg_m, tmp_value);*/
			vlock_set_panel_pll_m(tmp_value);
			vlock_pll_val_last &= 0x0000ffff;
			vlock_pll_val_last |= (m_reg_value & 0xffff0000);
		}
	} else {
		pre_m = (tmp_value & 0xff);
		new_m = ((m_reg_value >> 16) & 0x1ff);
		org_m = (vlock.val_m & 0xff);
		if (pre_m != new_m) {
			if (vlock_debug & VLOCK_DEBUG_INFO)
				pr_info("vlock m chg: pre=0x%x, report=0x%x\n",
				pre_m, new_m);

			if (new_m > pre_m) {
				tar_m = ((pre_m + 1) <
					(org_m + 1))?(pre_m + 1):(org_m + 1);
			} else {
				/*tar_m < pre_m*/
				tar_m = ((pre_m - 1) <
					(org_m - 1))?(org_m - 1):(pre_m - 1);
			}
			tmp_value = (tmp_value & 0xffffff00) + tar_m;
			/*amvecm_hiu_reg_write(hhi_pll_reg_m, tmp_value);*/
			vlock_set_panel_pll_m(tmp_value);
		}
	}

	/*check stable by diff m*/
	if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
		if (((m_reg_value >> 16) & 0xff) != (tmp_value & 0xff))
			vlock_pll_stable_cnt = 0;
	} else {
		if (((m_reg_value >> 16) & 0x1ff) != (tmp_value & 0x1ff))
			vlock_pll_stable_cnt = 0;
	}
}
/* won't change this function internal seqence,
 * if really need change,please be carefull and check with vlsi
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
	output_hz = vlock_check_output_hz(vinfo->sync_duration_num,
		vinfo->sync_duration_den);
	vlock_dis_cnt_no_vf = 0;
	if (vecm_latch_flag & FLAG_VLOCK_EN) {
		vlock_enable_step1(vf, vinfo, input_hz, output_hz);
		vlock_state = VLOCK_STATE_ENABLE_STEP1_DONE;
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
		if (vlock_vmode_change_status == VOUT_EVENT_MODE_CHANGE_PRE) {
			vlock_enable(0);
			if (vlock_debug & VLOCK_DEBUG_INFO)
				pr_info("[%s]auto disable vlock module for vmode change pre case!!!\n",
					__func__);
			return;
		}
		vlock_vmode_check();
		if ((vf->source_type != pre_source_type) ||
			(vf->source_mode != pre_source_mode) ||
			(input_hz != pre_input_freq) ||
			(output_hz != pre_output_freq) ||
			vlock_vmode_changed ||
			(vlock_state == VLOCK_STATE_ENABLE_FORCE_RESET)) {
			vlock_enable_step1(vf, vinfo, input_hz, output_hz);
			vlock_state = VLOCK_STATE_ENABLE_STEP1_DONE;
		}
		if ((vlock_sync_limit_flag < 10) &&
			(vlock_state >= VLOCK_STATE_ENABLE_STEP1_DONE)) {
			vlock_sync_limit_flag++;
			if ((vlock_sync_limit_flag <= 3) &&
				((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
				VLOCK_MODE_MANUAL_PLL)))) {
				/*reset*/
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
				/*clear first 3 frame internal cnt*/
				WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM0, 0);
				WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM1, 0);
				if (vlock_debug & VLOCK_DEBUG_INFO)
					pr_info("amve_vlock_process-0\n");
			}
			if ((vlock_sync_limit_flag == 4) &&
				((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
				VLOCK_MODE_MANUAL_PLL)))) {
				/*release reset*/
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
				WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
				if (vlock_debug & VLOCK_DEBUG_INFO)
					pr_info("amve_vlock_process-1\n");
			}
		}

		if ((vlock_sync_limit_flag == 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP1_DONE)) {
			/*input_vs_cnt =*/
			/*READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,*/
			/*	0, 28);*/
			input_vs_cnt = XTAL_VLOCK_CLOCK/input_hz;
			/*tl1 not need */
			if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) &&
				vf->type_original & VIDTYPE_TYPEMASK)
				input_vs_cnt = input_vs_cnt << 1;

			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
					input_vs_cnt*125/100);
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
					input_vs_cnt*70/100);

			/*cal accum1 value*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
			/*cal accum0 value*/
			//WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
			vlock_state = VLOCK_STATE_ENABLE_STEP2_DONE;

			/*
			 * tl1 auto pll,swich clk need after
			 *several frames
			 */
			if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
				if (IS_AUTO_MODE(vlock_mode))
					amvecm_hiu_reg_write_bits(
					HHI_HDMI_PLL_VLOCK_CNTL, 0x7, 0, 3);
				else if (IS_MANUAL_MODE(vlock_mode))
					amvecm_hiu_reg_write_bits(
					HHI_HDMI_PLL_VLOCK_CNTL, 0x6, 0, 3);
			}

			if (vlock_debug & VLOCK_DEBUG_INFO)
				pr_info("amve_vlock_process-2\n");
		} else if (vlock_dynamic_adjust &&
			(vlock_sync_limit_flag > 5) &&
			(vlock_state == VLOCK_STATE_ENABLE_STEP2_DONE) &&
			(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
			((vlock_mode & (VLOCK_MODE_MANUAL_PLL |
			VLOCK_MODE_MANUAL_ENC |
			VLOCK_MODE_MANUAL_SOFT_ENC)))) {
			if (vlock_mode & VLOCK_MODE_MANUAL_ENC)
				vlock_enable_step3_enc();
			else if (vlock_mode & VLOCK_MODE_MANUAL_PLL)
				vlock_enable_step3_pll();
			else if (vlock_mode & VLOCK_MODE_MANUAL_SOFT_ENC)
				vlock_enable_step3_soft_enc();
			/*check stable*/
			if ((vinfo->fr_adj_type != VOUT_FR_ADJ_HDMI) &&
				(vinfo->width <= 1920) &&
				!(vlock_debug & VLOCK_DEBUG_PLL2ENC_DIS) &&
				(vlock_mode &
				VLOCK_MODE_MANUAL_MIX_PLL_ENC) &&
				(vlock_pll_stable_cnt >
				VLOCK_PLL_STABLE_LIMIT)) {
				vlock_mode &= ~VLOCK_MODE_MANUAL_MIX_PLL_ENC;
				vlock_mode |= VLOCK_MODE_MANUAL_SOFT_ENC;
				vlock_state = VLOCK_STATE_ENABLE_FORCE_RESET;
				if (vlock_debug & VLOCK_DEBUG_INFO)
					pr_info("[%s]force reset for switch pll to enc mode!!!\n",
					__func__);
			}
		}
		if (((vlock_mode & (VLOCK_MODE_AUTO_PLL |
			VLOCK_MODE_AUTO_ENC))) &&
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
		((vlock_mode & (VLOCK_MODE_MANUAL_PLL |
		VLOCK_MODE_MANUAL_ENC |
		VLOCK_MODE_MANUAL_SOFT_ENC)))) {
		if (vlock_mode & VLOCK_MODE_MANUAL_ENC)
			vlock_enable_step3_enc();
		else if (vlock_mode & VLOCK_MODE_MANUAL_PLL)
			vlock_enable_step3_pll();
		else if (vlock_mode & VLOCK_MODE_MANUAL_SOFT_ENC)
			vlock_enable_step3_soft_enc();
	}
}

void vlock_clear_frame_counter(void)
{
	vlock.frame_cnt_in = 0;
	vlock.frame_cnt_no = 0;
	vlock_log_cnt = 0;
}

void vlock_set_en(bool en)
{
	/*if (vlock.dtdata->vlk_support)*/
		vlock_en = en;
}

void vlock_status_init(void)
{
	/*config vlock mode*/
	/*todo:txlx & g9tv support auto pll,*/
	/*but support not good,need vlsi support optimize*/
	vlock_mode = VLOCK_MODE_MANUAL_PLL;
	if (is_meson_gxtvbb_cpu() ||
		is_meson_txl_cpu() || is_meson_txlx_cpu()
		|| is_meson_tl1_cpu())
		vlock_en = 1;
	else
		vlock_en = 0;

	/*initial pll register address*/
	if (is_meson_tl1_cpu()) {
		hhi_pll_reg_m = HHI_TCON_PLL_CNTL0;
		hhi_pll_reg_frac = HHI_TCON_PLL_CNTL1;
		/*hhi_pll_reg_vlock_ctl = HHI_HDMI_PLL_VLOCK_CNTL;*/
	} else if (get_cpu_type() >=
		MESON_CPU_MAJOR_ID_GXL) {
		hhi_pll_reg_m = HHI_HDMI_PLL_CNTL;
		hhi_pll_reg_frac = HHI_HDMI_PLL_CNTL2;
	} else {
		hhi_pll_reg_m = HHI_HDMI_PLL_CNTL;
		hhi_pll_reg_frac = HHI_HDMI_PLL_CNTL2;
	}

	/*initial enc register address*/
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

	/*back up orignal pll value*/
	vlock.val_m = vlock_get_panel_pll_m();
	vlock.val_frac = vlock_get_panel_pll_frac();

	vlock.fsm_sts = VLOCK_STATE_NULL;
	vlock.fsm_prests = VLOCK_STATE_NULL;
	vlock.vf_sts = false;
	vlock.vmd_chg = false;
	vlock.md_support = false;
	/* vlock.phlock_percent = phlock_percent; */
	vlock_clear_frame_counter();


	pr_info("%s vlock_en:%d\n", __func__, vlock_en);
}

void vlock_dt_match_init(struct vecm_match_data_s *pdata)
{
	vlock.dtdata = pdata;
	/*vlock_en = vlock.dtdata.vlk_support;*/
	pr_info("vlock dt support: %d\n", vlock.dtdata->vlk_support);
	pr_info("vlock dt new_fsm: %d\n", vlock.dtdata->vlk_new_fsm);
	pr_info("vlock dt hwver: %d\n", vlock.dtdata->vlk_hwver);
	pr_info("vlock dt phlock_en: %d\n", vlock.dtdata->vlk_phlock_en);
}

void vlock_set_phase(u32 percent)
{
	u32 vs_i_val = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
	/*u32 vs_o_val = READ_VPP_REG(VPU_VLOCK_RO_VS_O_DIST);*/
	u32 data = 0;

	if (!vlock.dtdata->vlk_phlock_en)
		return;

	if (percent > 100) {
		pr_info("percent val err:%d\n", percent);
		return;
	}

	vlock.phlock_percent = percent;
	data = (vs_i_val * (100 + vlock.phlock_percent))/200;
	WRITE_VPP_REG(VPU_VLOCK_LOOP1_PHSDIF_TGT, data);
	pr_info("LOOP1_PHSDIF_TGT:0x%x\n", data);

	/*reset*/
	data = READ_VPP_REG(VPU_VLOCK_CTRL);
	data |= 1 << 2;
	data |= 1 << 5;
	WRITE_VPP_REG(VPU_VLOCK_CTRL, data);
	data &= ~(1 << 2);
	data &= ~(1 << 5);
	WRITE_VPP_REG(VPU_VLOCK_CTRL, data);
}

void vlock_set_phase_en(u32 en)
{
	if (en)
		vlock.dtdata->vlk_phlock_en = true;
	else
		vlock.dtdata->vlk_phlock_en = false;
	pr_info("vlock phlock_en=%d\n", en);
}

void vlock_phaselock_check(struct stvlock_sig_sts *pvlock,
		struct vframe_s *vf)
{
	/*vs_i*/
	u32 ia = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
	u32 val;
	static u32 cnt = 48;

	if (vlock.dtdata->vlk_phlock_en) {
		if (cnt++ > 50) {
			ia = READ_VPP_REG(VPU_VLOCK_RO_VS_I_DIST);
			val = (ia * (100 + vlock.phlock_percent))/200;
			WRITE_VPP_REG(VPU_VLOCK_LOOP1_PHSDIF_TGT, val);
			cnt = 0;
			#if 0
			/*reset*/
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
			#endif
		}
	}
}

bool vlock_get_phlock_flag(void)
{
	u32 flag;
	u32 sts;

	if (!vlock.dtdata->vlk_phlock_en)
	return false;

	flag = READ_VPP_REG(VPU_VLOCK_RO_LCK_FRM) >> 17;
	flag = flag&0x01;

	if (vlock.dtdata->vlk_new_fsm)
		sts = vlock.fsm_sts;
	else
		sts = vlock_state;

	if (flag && (sts == VLOCK_STATE_ENABLE_STEP2_DONE))
		return true;
	else
		return false;
}

bool vlock_get_vlock_flag(void)
{
	u32 flag;
	u32 sts;

	flag = READ_VPP_REG(VPU_VLOCK_RO_LCK_FRM) >> 16;
	flag = flag&0x01;

	if (vlock.dtdata->vlk_new_fsm)
		sts = vlock.fsm_sts;
	else
		sts = vlock_state;

	if (flag && (sts == VLOCK_STATE_ENABLE_STEP2_DONE))
		return true;
	else
		return false;
}

void vlock_enc_timing_monitor(void)
{
	static unsigned int pre_line, pre_pixel;
	unsigned int cur_line, cur_pixel;
	unsigned int val;

	val = READ_VPP_REG(VPU_VLOCK_RO_LINE_PIX_ADJ);
	cur_pixel = (val & 0x0000ffff);
	cur_line = (val >> 16) & 0x0000ffff;

	if ((cur_line != pre_line) || (cur_pixel != pre_pixel)) {
		pr_info("vlock line=0x%x pixel=0x%x\n",
			cur_line, cur_pixel);
		pre_line = cur_line;
		pre_pixel = cur_pixel;
	}
}

#if 1

u32 vlock_fsm_check_support(struct stvlock_sig_sts *pvlock,
		struct vframe_s *vf)
{
	u32 ret = true;

	if (((pvlock->input_hz != pvlock->output_hz) && (vlock_adapt == 0)) ||
		(pvlock->input_hz == 0) || (pvlock->output_hz == 0) ||
		(((vf->type_original & VIDTYPE_TYPEMASK)
			!= VIDTYPE_PROGRESSIVE) &&
			is_meson_txlx_package_962E())) {

		if (vlock_debug & VLOCK_DEBUG_INFO) {
			pr_info("[%s] for no support case!!!\n",
				__func__);
			pr_info("input_hz:%d, output_hz:%d\n",
				pvlock->input_hz, pvlock->output_hz);
			pr_info("type_original:0x%x\n", vf->type_original);
		}
		ret = false;
	}

	if (vlock_vmode_change_status == VOUT_EVENT_MODE_CHANGE_PRE) {
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("[%s] for vmode change pre case!!!\n",
				__func__);
		ret = false;
	}

	return ret;
}

u32 vlock_fsm_input_check(struct stvlock_sig_sts *vlock, struct vframe_s *vf)
{
	u32 ret = 0;
	u32 vframe_sts;
	struct vinfo_s *vinfo = NULL;

	if (vf == NULL)
		vframe_sts = false;
	else
		vframe_sts = true;

	vlock_vmode_check();

	if (vf != NULL) {
		vinfo = get_current_vinfo();
		vlock->input_hz = vlock_check_input_hz(vf);
		vlock->output_hz = vlock_check_output_hz(
			vinfo->sync_duration_num, vinfo->sync_duration_den);
	}

	/*check vf exist status*/
	if (vlock->vf_sts != vframe_sts) {
		vlock->vf_sts = vframe_sts;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("vlock vfsts chg %d\n", vframe_sts);
		ret = 1;
	} else if (vlock_vmode_change_status) {
		/*check video mode status*/
		vlock->vmd_chg = true;
		ret = 1;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("vlock vmode chg\n");
	}

	if (vlock->vf_sts)
		vlock->md_support = vlock_fsm_check_support(vlock, vf);

	return ret;
}

u32 vlock_fsm_to_en_func(struct stvlock_sig_sts *pvlock,
		struct vframe_s *vf)
{
	u32 ret = 0;
	struct vinfo_s *vinfo;

	if ((vf->source_type != pre_source_type) ||
		(vf->source_mode != pre_source_mode) ||
		(pvlock->input_hz != pre_input_freq) ||
		(pvlock->output_hz != pre_output_freq) ||
		vlock_vmode_changed ||
		(pvlock->fsm_sts ==
		VLOCK_STATE_ENABLE_FORCE_RESET)) {

		/*back up orignal pll value*/
		/*amvecm_hiu_reg_read(hhi_pll_reg_m, &vlock.val_m);*/
		/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &vlock.val_frac);*/
		vlock.val_m = vlock_get_panel_pll_m();
		vlock.val_frac = vlock_get_panel_pll_frac();
		if (vlock_debug & VLOCK_DEBUG_INFO) {
			pr_info("HIU pll m[0x%x]=0x%x\n",
				hhi_pll_reg_m, vlock.val_m);
			pr_info("HIU pll f[0x%x]=0x%x\n",
				hhi_pll_reg_frac, vlock.val_frac);
		}
		vinfo = get_current_vinfo();
		vlock_enable_step1(vf, vinfo,
		pvlock->input_hz, pvlock->output_hz);
		ret = 1;
	}

	return ret;
}

u32 vlock_fsm_en_step1_func(struct stvlock_sig_sts *pvlock,
		struct vframe_s *vf)
{
	u32 ret = 0;
	u32 input_vs_cnt;

	if ((pvlock->frame_cnt_in <= 3) &&
		((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
		VLOCK_MODE_MANUAL_PLL)))) {
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 5, 1);
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 2, 1);
		/*clear first 3 frame internal cnt*/
		WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM0, 0);
		WRITE_VPP_REG(VPU_VLOCK_OVWRITE_ACCUM1, 0);
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s -0\n", __func__);
	} else if ((pvlock->frame_cnt_in == 4) &&
		((vlock_mode & (VLOCK_MODE_MANUAL_ENC |
		VLOCK_MODE_MANUAL_PLL)))) {
		/*cal accum0 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);
		/*cal accum1 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s -1\n", __func__);
	} else if (pvlock->frame_cnt_in == 5) {
		/*input_vs_cnt =*/
		/*READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,*/
		/*	0, 28);*/
		input_vs_cnt = XTAL_VLOCK_CLOCK/pvlock->input_hz;
		/*tl1 not need */
		if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TL1) &&
			vf->type_original & VIDTYPE_TYPEMASK)
			input_vs_cnt = input_vs_cnt << 1;

		WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
				input_vs_cnt*125/100);
		WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
				input_vs_cnt*70/100);

		/*cal accum1 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 2, 1);
		/*cal accum0 value*/
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 5, 1);

		/*
		 * tl1 auto pll,swich clk need after
		 *several frames
		 */
		if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
			if (IS_AUTO_MODE(vlock_mode))
				amvecm_hiu_reg_write_bits(
				HHI_HDMI_PLL_VLOCK_CNTL, 0x7, 0, 3);
			else if (IS_MANUAL_MODE(vlock_mode))
				amvecm_hiu_reg_write_bits(
				HHI_HDMI_PLL_VLOCK_CNTL, 0x6, 0, 3);
		}

		ret = 1;
		if (vlock_debug & VLOCK_DEBUG_INFO)
			pr_info("%s -2\n", __func__);
	}

	return ret;
}

u32 vlock_fsm_en_step2_func(struct stvlock_sig_sts *pvlock,
		struct vframe_s *vf)
{
	u32 ret = 0;

	if (vlock_dynamic_adjust &&
		(cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) &&
		(IS_MANUAL_MODE(vlock_mode))) {
		if (IS_MANUAL_ENC_MODE(vlock_mode))
			vlock_enable_step3_enc();
		else if (IS_MANUAL_PLL_MODE(vlock_mode))
			vlock_enable_step3_pll();
		else if (IS_MANUAL_SOFTENC_MODE(vlock_mode))
			vlock_enable_step3_soft_enc();
	}

	if (IS_ENC_MODE(vlock_mode))
		vlock_enc_timing_monitor();

	/*check phase*/
	vlock_phaselock_check(pvlock, vf);
	return ret;
}


void vlock_fsm_monitor(struct vframe_s *vf)
{
	u32 changed;

	changed = vlock_fsm_input_check(&vlock, vf);
	switch (vlock.fsm_sts) {
	case VLOCK_STATE_NULL:
		if (vlock.vf_sts) {
			/*have frame in*/
			if (vlock.frame_cnt_in++ >= 20) {
				/*vframe input valid*/
				if (vlock.md_support) {
					if (vlock_fsm_to_en_func(&vlock, vf)) {
						vlock_clear_frame_counter();
						vlock.fsm_sts =
						VLOCK_STATE_ENABLE_STEP1_DONE;
					} else {
						/*error waitting here*/
						vlock_clear_frame_counter();
					}
				}
			}
		} else if (vlock.vmd_chg) {
			vlock_clear_frame_counter();
			vlock.vmd_chg = false;
			vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP2_DONE;
		} else {
			/*disabled and waitting here*/
			vlock_clear_frame_counter();
		}
		break;

	case VLOCK_STATE_ENABLE_STEP1_DONE:
		if (vlock.vf_sts) {
			vlock.frame_cnt_in++;
			if (vlock_fsm_en_step1_func(&vlock, vf))
				vlock.fsm_sts = VLOCK_STATE_ENABLE_STEP2_DONE;
		} else if (vlock.vmd_chg) {
			vlock_clear_frame_counter();
			vlock.vmd_chg = false;
			vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP1_DONE;
		} else {
			vlock.frame_cnt_in = 0;
			if (vlock.frame_cnt_no++ > vlock_dis_cnt_no_vf_limit) {
				/*go to disable state*/
				vlock_clear_frame_counter();
				vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP1_DONE;
			}
		}
		break;

	case VLOCK_STATE_ENABLE_STEP2_DONE:
		if (vlock.vf_sts) {
			if (!vlock.md_support) {
				/*function not support*/
				vlock_clear_frame_counter();
				vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP1_DONE;
			} else if (vecm_latch_flag & FLAG_VLOCK_DIS) {
				/*disable vlock by vecm cmd*/
				vlock_disable_step1();
				vlock_disable_step2();
				vlock_en = 0;
				vecm_latch_flag &= ~FLAG_VLOCK_DIS;
				if (vlock_debug & VLOCK_DEBUG_INFO)
					pr_info("[%s] vlock dis\n", __func__);
				vlock_clear_frame_counter();
				vlock.fsm_sts = VLOCK_STATE_NULL;
			} else {
				/*normal mode*/
				vlock.frame_cnt_no = 0;
				vlock_fsm_en_step2_func(&vlock, vf);
			}
		} else if (vlock.vmd_chg) {
			vlock_clear_frame_counter();
			vlock.vmd_chg = false;
			vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP1_DONE;
		} else {
			/*no frame input*/
			if (vlock.frame_cnt_no++ > vlock_dis_cnt_no_vf_limit) {
				/*go to disable state*/
				vlock_clear_frame_counter();
				vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP1_DONE;
			}
		}
		break;

	case VLOCK_STATE_DISABLE_STEP1_DONE:
		vlock_disable_step1();
		if (vlock_disable_step2())
			vlock.fsm_sts = VLOCK_STATE_NULL;
		else
			vlock.fsm_sts = VLOCK_STATE_DISABLE_STEP2_DONE;
		break;

	case VLOCK_STATE_DISABLE_STEP2_DONE:
		if (vlock_disable_step2())
			vlock.fsm_sts = VLOCK_STATE_NULL;
		break;

	default:
		pr_info("err state %d\n", vlock.fsm_sts);
		break;
	}

	/*capture log*/
	if (((vlock_mode & (VLOCK_MODE_AUTO_PLL |
		VLOCK_MODE_AUTO_ENC))) &&
		vlock_log_en && (vlock_log_cnt < vlock_log_size) &&
		(vlock_debug & VLOCK_DEBUG_AUTO_MODE_LOG_EN)) {
		vlock_reg_get();
		vlock_log_cnt++;
	}

	if ((vlock_debug & VLOCK_DEBUG_INFO) &&
			vlock.fsm_sts != vlock.fsm_prests) {
		pr_info(">>> %s fsm %d to %d\n", __func__,
			vlock.fsm_prests, vlock.fsm_sts);
		vlock.fsm_prests = vlock.fsm_sts;
	}
}
#endif

/*new packed separeted from amvecm_on_vs,avoid the influencec of repeate call,
 *which may affect vlock process
 */
void vlock_process(struct vframe_s *vf)
{
	if (probe_ok == 0 || !vlock_en)
		return;

	if (vlock_debug & VLOCK_DEBUG_FSM_DIS)
		return;

	/* todo:vlock processs only for tv chip */
	if (vlock.dtdata->vlk_new_fsm)
		vlock_fsm_monitor(vf);
	else {
		if (vf != NULL)
			amve_vlock_process(vf);
		else
			amve_vlock_resume();
	}
}
EXPORT_SYMBOL(vlock_process);

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
	case VLOCK_PLL_M_LIMIT:
		vlock_pll_m_limit = val;
		break;
	case VLOCK_DELTA_CNT_LIMIT:
		vlock_delta_cnt_limit = val;
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
	case VLOCK_SUPPORT:
		vlock_support = val;
		break;
	default:
		pr_info("%s:unknown vlock param:%d\n", __func__, sel);
		break;
	}

}
void vlock_status(void)
{
	struct vinfo_s *vinfo;

	pr_info("\n current vlock parameters status:\n");
	pr_info("vlock driver version :  %s\n", VLOCK_VER);
	pr_info("vlock_mode:%d\n", vlock_mode);
	pr_info("vlock_en:%d\n", vlock_en);
	pr_info("vlock_adapt:%d\n", vlock_adapt);
	pr_info("vlock_dis_cnt_limit:%d\n", vlock_dis_cnt_limit);
	pr_info("vlock_delta_limit:%d\n", vlock_delta_limit);
	pr_info("vlock_pll_m_limit:%d\n", vlock_pll_m_limit);
	pr_info("vlock_delta_cnt_limit:%d\n", vlock_delta_cnt_limit);
	pr_info("vlock_debug:0x%x\n", vlock_debug);
	pr_info("vlock_dynamic_adjust:%d\n", vlock_dynamic_adjust);
	pr_info("vlock_state:%d\n", vlock_state);
	pr_info("vecm_latch_flag:0x%x\n", vecm_latch_flag);
	pr_info("vlock_sync_limit_flag:%d\n", vlock_sync_limit_flag);
	pr_info("pre_hiu_reg_m:0x%x\n", pre_hiu_reg_m);
	pr_info("pre_hiu_reg_frac:0x%x\n", pre_hiu_reg_frac);
	pr_info("pre_enc_max_line:0x%x\n", pre_enc_max_line);
	pr_info("pre_enc_max_pixel:0x%x\n", pre_enc_max_pixel);
	pr_info("vlock_dis_cnt:%d\n", vlock_dis_cnt);
	pr_info("vlock_dis_cnt_no_vf:%d\n", vlock_dis_cnt_no_vf);
	pr_info("vlock_dis_cnt_no_vf_limit:%d\n", vlock_dis_cnt_no_vf_limit);
	pr_info("enc_max_line_addr:0x%x\n", enc_max_line_addr);
	pr_info("enc_max_pixel_addr:0x%x\n", enc_max_pixel_addr);
	pr_info("enc_video_mode_addr:0x%x\n", enc_video_mode_addr);
	pr_info("enc_max_line_switch_addr:0x%x\n", enc_max_line_switch_addr);
	pr_info("vlock_capture_limit:0x%x\n", vlock_capture_limit);
	pr_info("vlock_line_limit:%d\n", vlock_line_limit);
	pr_info("vlock_pll_stable_cnt:%d\n", vlock_pll_stable_cnt);
	pr_info("vlock_enc_adj_limit:%d\n", vlock_enc_adj_limit);
	pr_info("vlock_support:%d\n", vlock_support);
	pr_info("vlock_enc_stable_flag:%d\n", vlock_enc_stable_flag);
	pr_info("vlock_intput_type:%d\n", vlock_intput_type);
	pr_info("vlock_pll_adj_limit:%d\n", vlock_pll_adj_limit);
	pr_info("vlock_pll_val_last:%d\n", vlock_pll_val_last);
	pr_info("vlk_fsm_sts:%d(2 is working)\n", vlock.fsm_sts);
	pr_info("vlk_support:%d\n", vlock.dtdata->vlk_support);
	pr_info("vlk_new_fsm:%d\n", vlock.dtdata->vlk_new_fsm);
	pr_info("vlk_phlock_en:%d\n", vlock.dtdata->vlk_phlock_en);
	pr_info("vlk_hwver:%d\n", vlock.dtdata->vlk_hwver);
	pr_info("phlock flag:%d\n", vlock_get_phlock_flag());
	pr_info("vlock flag:%d\n", vlock_get_vlock_flag());
	pr_info("phase:%d\n", vlock.phlock_percent);
	vinfo = get_current_vinfo();
	pr_info("vinfo sync_duration_num:%d\n", vinfo->sync_duration_num);
	pr_info("vinfo sync_duration_den:%d\n", vinfo->sync_duration_den);
	pr_info("vinfo video_clk:%d\n", vinfo->video_clk);
	pr_info("vframe input_hz:%d\n", vlock.input_hz);
	pr_info("vframe output_hz:%d\n", vlock.output_hz);
}

void vlock_reg_dump(void)
{
	unsigned int addr;
	unsigned int val;

	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000); addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(0xd0100000+(addr<<2)), addr,
			READ_VPP_REG(addr));

	if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2) {
		for (addr = (0x3021); addr <= (0x302b); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
		amvecm_hiu_reg_read(HHI_HDMI_PLL_VLOCK_CNTL, &val);
		pr_info("HIU [0x%04x]=0x%08x\n", HHI_HDMI_PLL_VLOCK_CNTL, val);
	}
	/*amvecm_hiu_reg_read(hhi_pll_reg_m, &val);*/
	val = vlock_get_panel_pll_m();
	pr_info("HIU pll m[0x%04x]=0x%08x\n", hhi_pll_reg_m, val);
	/*amvecm_hiu_reg_read(hhi_pll_reg_frac, &val);*/
	val = vlock_get_panel_pll_frac();
	pr_info("HIU pll f[0x%04x]=0x%08x\n", hhi_pll_reg_frac, val);

	/*back up orignal pll value*/
	pr_info("HIU pll m[0x%x]=0x%x\n", hhi_pll_reg_m, vlock.val_m);
	pr_info("HIU pll f[0x%x]=0x%x\n", hhi_pll_reg_frac, vlock.val_frac);

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
	if (vlock.dtdata->vlk_hwver >= vlock_hw_ver2)
		return;
	/*
	 *1:fromhdmi rx ,
	 *2:from tv-decoder,
	 *3:from dvin,
	 *4:from dvin,
	 *5:from 2nd bt656
	 */
	vlock_intput_type = type & VIDTYPE_TYPEMASK;
	if ((vlock_intput_type == VIDTYPE_PROGRESSIVE) ||
		(vlock_mode & VLOCK_MODE_MANUAL_SOFT_ENC))
		return;
	if (vlock_intput_type == VIDTYPE_INTERLACE_TOP) {
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

#ifdef CONFIG_AMLOGIC_LCD
#define VLOCK_LCD_RETRY_MAX    100
void vlock_lcd_param_work(struct work_struct *p_work)
{
	unsigned int param[LCD_VLOCK_PARAM_NUM] = {0};
	int i = 0;

	while (i++ < VLOCK_LCD_RETRY_MAX) {
		aml_lcd_notifier_call_chain(LCD_EVENT_VLOCK_PARAM, &param);
		if (param[0] & LCD_VLOCK_PARAM_BIT_UPDATE) {
			if (param[0] & LCD_VLOCK_PARAM_BIT_VALID) {
				vlock_en = param[1];
				vlock_mode = param[2];
				vlock_pll_m_limit = param[3];
				vlock_line_limit = param[4];

				if (vlock_mode &
					VLOCK_MODE_MANUAL_MIX_PLL_ENC) {
					vlock_mode &=
						~VLOCK_MODE_MANUAL_MIX_PLL_ENC;
					vlock_mode |= VLOCK_MODE_MANUAL_PLL;
				}
			}
			break;
		}
		msleep(20);
	}
}
#endif

void vlock_param_config(struct device_node *node)
{
	unsigned int val;
	int ret;

	ret = of_property_read_u32(node, "vlock_en", &val);
	if (ret)
		pr_info("Can't find  vlock_en.\n");
	else
		vlock_en = val;
	ret = of_property_read_u32(node, "vlock_mode", &val);
	if (ret)
		pr_info("Can't find  vlock_mode.\n");
	else
		vlock_mode = val;
	ret = of_property_read_u32(node, "vlock_pll_m_limit", &val);
	if (ret)
		pr_info("Can't find  vlock_pll_m_limit.\n");
	else
		vlock_pll_m_limit = val;
	ret = of_property_read_u32(node, "vlock_line_limit", &val);
	if (ret)
		pr_info("Can't find  vlock_line_limit.\n");
	else
		vlock_line_limit = val;

#ifdef CONFIG_AMLOGIC_LCD
	schedule_work(&aml_lcd_vlock_param_work);
#endif

	if (vlock_mode & VLOCK_MODE_MANUAL_MIX_PLL_ENC) {
		vlock_mode &= ~VLOCK_MODE_MANUAL_MIX_PLL_ENC;
		vlock_mode |= VLOCK_MODE_MANUAL_PLL;
	}
	pr_info("param_config vlock_en:%d\n", vlock_en);
}

int vlock_notify_callback(struct notifier_block *block, unsigned long cmd,
	void *para)
{
	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	if (!vinfo) {
		pr_info("current vinfo NULL\n");
		return -1;
	}
	if (vlock_debug & VLOCK_DEBUG_INFO)
		pr_info("current vmode=%s, vinfo w=%d,h=%d, cmd: 0x%lx\n",
			vinfo->name, vinfo->width, vinfo->height, cmd);
	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE_PRE:
	case VOUT_EVENT_MODE_CHANGE:
		vlock_vmode_change_status = cmd;
		break;
	default:
		break;
	}
	return 0;
}

static int __init phlock_phase_config(char *str)
{
	unsigned char *ptr = str;

	pr_info("%s: bootargs is %s.\n", __func__, str);
	if (strstr(ptr, "1"))
		vlock.phlock_percent = 99;
	else
		vlock.phlock_percent = 40;

	return 0;
}
__setup("video_reverse=", phlock_phase_config);

/*video lock end*/

