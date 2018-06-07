/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_sm.c
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

/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* Amlogic Headers */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* Local Headers */
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"
#include "vdin_sm.h"
#include "vdin_ctl.h"
#include "vdin_drv.h"

/* Stay in TVIN_SIG_STATE_NOSIG for some
 * cycles => be sure TVIN_SIG_STATE_NOSIG
 */
#define NOSIG_MAX_CNT 8
/* Stay in TVIN_SIG_STATE_UNSTABLE for some
 * cycles => be sure TVIN_SIG_STATE_UNSTABLE
 */
#define UNSTABLE_MAX_CNT 2/* 4 */
/* Have signal for some cycles  => exit TVIN_SIG_STATE_NOSIG */
#define EXIT_NOSIG_MAX_CNT 2/* 1 */
/* No signal for some cycles  => back to TVAFE_STATE_NOSIG */
#define BACK_NOSIG_MAX_CNT 24 /* 8 */
/* Signal unstable for some cycles => exit TVAFE_STATE_STABLE */
#define EXIT_STABLE_MAX_CNT 1
/* Signal stable for some cycles  => back to TVAFE_STATE_STABLE */
/* must >=500ms,for new api function */
#define BACK_STABLE_MAX_CNT 50
#define EXIT_PRESTABLE_MAX_CNT 50
static struct tvin_sm_s sm_dev[VDIN_MAX_DEVS];

static int sm_print_nosig;
static int sm_print_notsup;
static int sm_print_unstable;
static int sm_print_fmt_nosig;
static int sm_print_fmt_chg;
static int sm_atv_prestable_fmt;
static int sm_print_prestable;

/*bit0:general debug bit;bit1:hdmirx color change*/
static unsigned int sm_debug_enable = 1;
module_param(sm_debug_enable, uint, 0664);
MODULE_PARM_DESC(sm_debug_enable,
		"enable/disable state machine debug message");

static int back_nosig_max_cnt = BACK_NOSIG_MAX_CNT;
static int atv_unstable_in_cnt = 45;
static int atv_unstable_out_cnt = 50;
static int hdmi_unstable_out_cnt = 1;
static int hdmi_stable_out_cnt = 1;/* 25; */
/* new add in gxtvbb@20160523,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *if atv_stable_out_cnt < 100,the signal state will change
 *after swich source to atv or after atv search
 */
static int atv_stable_out_cnt = 100;
/* new add in gxtvbb@20160613,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *ensure after fmt change,the new fmt can be detect in time!
 */
static int atv_stable_fmt_check_cnt = 10;
/* new add in gxtvbb@20160613,reason:
 * ensure vdin fmt can update when fmt is changed in menu
 */
static int atv_stable_fmt_check_enable;
/* new add in gxtvbb@20160523,reason:
 *gxtvbb add atv snow config,the config will affect signal detect.
 *ensure after prestable into stable,the state is really stable!
 */
static int atv_prestable_out_cnt = 100;
static int other_stable_out_cnt = EXIT_STABLE_MAX_CNT;
static int other_unstable_out_cnt = BACK_STABLE_MAX_CNT;
static int other_unstable_in_cnt = UNSTABLE_MAX_CNT;
static int nosig_in_cnt = NOSIG_MAX_CNT;
static int nosig2_unstable_cnt = EXIT_NOSIG_MAX_CNT;

#ifdef DEBUG_SUPPORT
module_param(back_nosig_max_cnt, int, 0664);
MODULE_PARM_DESC(back_nosig_max_cnt,
		"unstable enter nosignal state max count");

module_param(atv_unstable_in_cnt, int, 0664);
MODULE_PARM_DESC(atv_unstable_in_cnt, "atv_unstable_in_cnt");

module_param(atv_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_unstable_out_cnt, "atv_unstable_out_cnt");

module_param(hdmi_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(hdmi_unstable_out_cnt, "hdmi_unstable_out_cnt");

module_param(hdmi_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(hdmi_stable_out_cnt, "hdmi_stable_out_cnt");

module_param(atv_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_stable_out_cnt, "atv_stable_out_cnt");

module_param(atv_stable_fmt_check_cnt, int, 0664);
MODULE_PARM_DESC(atv_stable_fmt_check_cnt, "atv_stable_fmt_check_cnt");

module_param(atv_prestable_out_cnt, int, 0664);
MODULE_PARM_DESC(atv_prestable_out_cnt, "atv_prestable_out_cnt");

module_param(other_stable_out_cnt, int, 0664);
MODULE_PARM_DESC(other_stable_out_cnt, "other_stable_out_cnt");

module_param(other_unstable_out_cnt, int, 0664);
MODULE_PARM_DESC(other_unstable_out_cnt, "other_unstable_out_cnt");

module_param(other_unstable_in_cnt, int, 0664);
MODULE_PARM_DESC(other_unstable_in_cnt, "other_unstable_in_cnt");

module_param(nosig_in_cnt, int, 0664);
MODULE_PARM_DESC(nosig_in_cnt, "nosig_in_cnt");

module_param(nosig2_unstable_cnt, int, 0664);
MODULE_PARM_DESC(nosig2_unstable_cnt, "nosig2_unstable_cnt");
#endif

static int signal_status = TVIN_SIG_STATUS_NULL;
module_param(signal_status, int, 0664);
MODULE_PARM_DESC(signal_status, "signal_status");
enum tvin_color_fmt_range_e tvin_get_force_fmt_range(
	enum tvin_color_fmt_range_e fmt_range,
	enum tvin_color_fmt_e color_fmt)
{
	if (color_fmt == TVIN_YUV444 ||
		color_fmt == TVIN_YUV422) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_YUV_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_YUV_LIMIT;
	} else if (color_fmt == TVIN_RGB444) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_RGB_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_RGB_LIMIT;
	}
	return fmt_range;
}

/*
 * check hdmirx color format
 */
static void hdmirx_color_fmt_handler(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	enum tvin_port_e port = TVIN_PORT_NULL;
	enum tvin_color_fmt_e cur_color_fmt, pre_color_fmt;
	enum tvin_color_fmt_e cur_dest_color_fmt, pre_dest_color_fmt;
	struct tvin_sig_property_s *prop, *pre_prop;
	unsigned int vdin_hdr_flag, pre_vdin_hdr_flag;
	unsigned int vdin_fmt_range, pre_vdin_fmt_range;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}

	prop = &devp->prop;
	pre_prop = &devp->pre_prop;
	sm_ops = devp->frontend->sm_ops;
	port = devp->parm.port;

	if ((port < TVIN_PORT_HDMI0) || (port > TVIN_PORT_HDMI7))
		return;

	if ((devp->flags & VDIN_FLAG_DEC_STARTED) &&
		(sm_ops->get_sig_property)) {
		sm_ops->get_sig_property(devp->frontend, prop);

		cur_color_fmt = prop->color_format;
		pre_color_fmt = pre_prop->color_format;

		cur_dest_color_fmt = prop->dest_cfmt;
		pre_dest_color_fmt = pre_prop->dest_cfmt;

		vdin_hdr_flag = prop->vdin_hdr_Flag;
		pre_vdin_hdr_flag = pre_prop->vdin_hdr_Flag;

		if (color_range_force)
			prop->color_fmt_range =
			tvin_get_force_fmt_range(pre_prop->color_fmt_range,
			pre_prop->color_format);
		vdin_fmt_range = prop->color_fmt_range;
		pre_vdin_fmt_range = pre_prop->color_fmt_range;

		if ((cur_color_fmt != pre_color_fmt) ||
			(vdin_hdr_flag != pre_vdin_hdr_flag) ||
			(vdin_fmt_range != pre_vdin_fmt_range) ||
			(cur_dest_color_fmt != pre_dest_color_fmt)) {
			if (sm_debug_enable & (1 << 1))
				pr_info("[smr.%d] cur color fmt(%d->%d), hdr_flag(%d->%d), dest color fmt(%d->%d), csc_cfg:0x%x\n",
					devp->index,
					pre_color_fmt, cur_color_fmt,
					pre_vdin_hdr_flag, vdin_hdr_flag,
					pre_dest_color_fmt, cur_dest_color_fmt,
					devp->csc_cfg);
			vdin_get_format_convert(devp);
			devp->csc_cfg = 1;
		} else
			devp->csc_cfg = 0;
	}
}

/* check auto de to adjust vdin cutwindow */
void vdin_auto_de_handler(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_sig_property_s *prop;
	unsigned int cur_vs, cur_ve, pre_vs, pre_ve;
	unsigned int cur_hs, cur_he, pre_hs, pre_he;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}
	if (devp->auto_cutwindow_en == 0)
		return;
	prop = &devp->prop;
	sm_ops = devp->frontend->sm_ops;
	if ((devp->flags & VDIN_FLAG_DEC_STARTED) &&
		(sm_ops->get_sig_property)) {
		sm_ops->get_sig_property(devp->frontend, prop);
		cur_vs = prop->vs;
		cur_ve = prop->ve;
		cur_hs = prop->hs;
		cur_he = prop->he;
		pre_vs = prop->pre_vs;
		pre_ve = prop->pre_ve;
		pre_hs = prop->pre_hs;
		pre_he = prop->pre_he;
		if ((pre_vs != cur_vs) || (pre_ve != cur_ve) ||
			(pre_hs != cur_hs) || (pre_he != cur_he)) {
			pr_info("[smr.%d] pre_vs(%d->%d),pre_ve(%d->%d),pre_hs(%d->%d),pre_he(%d->%d),cutwindow_cfg:0x%x\n",
				devp->index, pre_vs, cur_vs, pre_ve, cur_ve,
				pre_hs, cur_hs, pre_he, cur_he,
				devp->cutwindow_cfg);
			devp->cutwindow_cfg = 1;
		} else {
			devp->cutwindow_cfg = 0;
		}
	}
}

void tvin_smr_init_counter(int index)
{
	sm_dev[index].state_cnt          = 0;
	sm_dev[index].exit_nosig_cnt     = 0;
	sm_dev[index].back_nosig_cnt     = 0;
	sm_dev[index].back_stable_cnt    = 0;
	sm_dev[index].exit_prestable_cnt = 0;
}

static void hdmirx_dv_check(struct vdin_dev_s *devp,
	struct tvin_sig_property_s *prop)
{
	/*check hdmiin dolby input*/
	if (prop->dolby_vision != devp->dv.dv_flag) {
		tvin_smr_init(devp->index);
		devp->dv.dv_flag = prop->dolby_vision;
	}
}

void reset_tvin_smr(unsigned int index)
{
	sm_dev[index].sig_status = TVIN_SIG_STATUS_NULL;
}

/*
 * tvin state machine routine
 *
 */
void tvin_smr(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_info_s *info;
	enum tvin_port_e port = TVIN_PORT_NULL;
	unsigned int unstb_in;
	struct tvin_sm_s *sm_p;
	struct tvin_frontend_s *fe;
	struct tvin_sig_property_s *prop, *pre_prop;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}

	if ((devp->flags & VDIN_FLAG_SM_DISABLE) ||
		(devp->flags & VDIN_FLAG_SUSPEND))
		return;

	sm_p = &sm_dev[devp->index];
	fe = devp->frontend;
	sm_ops = devp->frontend->sm_ops;
	info = &devp->parm.info;
	port = devp->parm.port;
	prop = &devp->prop;
	pre_prop = &devp->pre_prop;

	hdmirx_dv_check(devp, prop);

	switch (sm_p->state) {
	case TVIN_SM_STATUS_NOSIG:
		++sm_p->state_cnt;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if ((port == TVIN_PORT_CVBS3) &&
			(devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(1);
#endif
		if (sm_ops->nosig(devp->frontend)) {
			sm_p->exit_nosig_cnt = 0;
			if (sm_p->state_cnt >= nosig_in_cnt) {
				sm_p->state_cnt = nosig_in_cnt;
				info->status = TVIN_SIG_STATUS_NOSIG;
				info->fmt = TVIN_SIG_FMT_NULL;
				if (sm_debug_enable && !sm_print_nosig) {
					pr_info("[smr.%d] no signal\n",
							devp->index);
					sm_print_nosig = 1;
				}
				sm_print_unstable = 0;
			}
		} else {
			++sm_p->exit_nosig_cnt;
			if (sm_p->exit_nosig_cnt >= nosig2_unstable_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable)
					pr_info("[smr.%d] no signal --> unstable\n",
							devp->index);
				sm_print_nosig  = 0;
				sm_print_unstable = 0;
			}
		}
		break;

	case TVIN_SM_STATUS_UNSTABLE:
		++sm_p->state_cnt;
		if (sm_ops->nosig(devp->frontend)) {
			sm_p->back_stable_cnt = 0;
			++sm_p->back_nosig_cnt;
			if (sm_p->back_nosig_cnt >= sm_p->back_nosig_max_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_NOSIG;
				info->status = TVIN_SIG_STATUS_NOSIG;
				info->fmt = TVIN_SIG_FMT_NULL;
				if (sm_debug_enable)
					pr_info("[smr.%d] unstable --> no signal\n",
							devp->index);
				sm_print_nosig  = 0;
				sm_print_unstable = 0;
			}
		} else {
			sm_p->back_nosig_cnt = 0;
			if (sm_ops->fmt_changed(devp->frontend)) {
				sm_p->back_stable_cnt = 0;
				if (((port == TVIN_PORT_CVBS3) ||
					(port == TVIN_PORT_CVBS0)) &&
					devp->unstable_flag &&
					(devp->flags & VDIN_FLAG_SNOW_FLAG))
					/* UNSTABLE_ATV_MAX_CNT; */
					unstb_in = sm_p->atv_unstable_in_cnt;
				else
					unstb_in = other_unstable_in_cnt;
				if (sm_p->state_cnt >= unstb_in) {
					sm_p->state_cnt  = unstb_in;
					info->status = TVIN_SIG_STATUS_UNSTABLE;
					info->fmt = TVIN_SIG_FMT_NULL;
					if (sm_debug_enable &&
						!sm_print_unstable) {
						pr_info("[smr.%d] unstable\n",
								devp->index);
						sm_print_unstable = 1;
					}
					sm_print_nosig  = 0;
				}
			} else {
				++sm_p->back_stable_cnt;
				if (((port == TVIN_PORT_CVBS3) ||
					(port == TVIN_PORT_CVBS0)) &&
					(devp->flags & VDIN_FLAG_SNOW_FLAG))
					unstb_in = sm_p->atv_unstable_out_cnt;
				else if ((port >= TVIN_PORT_HDMI0) &&
						 (port <= TVIN_PORT_HDMI7))
					unstb_in = sm_p->hdmi_unstable_out_cnt;
				else
					unstb_in = other_unstable_out_cnt;
				 /* must wait enough time for cvd signal lock */
				if (sm_p->back_stable_cnt >= unstb_in) {
					sm_p->back_stable_cnt = 0;
					sm_p->state_cnt = 0;
					if (sm_ops->get_fmt &&
						sm_ops->get_sig_property) {
						info->fmt =
							sm_ops->get_fmt(fe);
						sm_ops->get_sig_property(fe,
							prop);
						info->cfmt = prop->color_format;
						memcpy(pre_prop, prop,
					sizeof(struct tvin_sig_property_s));
						devp->parm.info.trans_fmt =
							prop->trans_fmt;
						devp->parm.info.is_dvi =
							prop->dvi_info;
						devp->parm.info.fps =
							prop->fps;
					}
				} else
					info->fmt = TVIN_SIG_FMT_NULL;
				if (info->fmt == TVIN_SIG_FMT_NULL) {
					/* remove unsupport status */
					info->status = TVIN_SIG_STATUS_UNSTABLE;
					if (sm_debug_enable &&
						!sm_print_notsup) {
						pr_info("[smr.%d] unstable --> not support\n",
								devp->index);
						sm_print_notsup = 1;
					}
				} else {
					if (sm_ops->fmt_config)
						sm_ops->fmt_config(fe);
					tvin_smr_init_counter(devp->index);
					sm_p->state = TVIN_SM_STATUS_PRESTABLE;
					sm_atv_prestable_fmt = info->fmt;
					if (sm_debug_enable) {
						pr_info("[smr.%d]unstable-->prestable",
						devp->index);
						pr_info("and format is %d(%s)\n",
						info->fmt,
						tvin_sig_fmt_str(info->fmt));
					}
					sm_print_nosig  = 0;
					sm_print_unstable = 0;
					sm_print_fmt_nosig = 0;
					sm_print_fmt_chg = 0;
					sm_print_prestable = 0;
				}
			}
		}
		break;
	case TVIN_SM_STATUS_PRESTABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int prestable_out_cnt = 0;
		devp->unstable_flag = true;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if ((port == TVIN_PORT_CVBS3) &&
			(devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(0);
#endif
		if (sm_ops->nosig(devp->frontend)) {
			nosig = true;
			if (sm_debug_enable && !(sm_print_prestable&0x1)) {
				pr_info("[smr.%d] warning: no signal\n",
						devp->index);
				sm_print_prestable |= 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend)) {
			fmt_changed = true;
			if (sm_debug_enable && !(sm_print_prestable&0x2)) {
				pr_info("[smr.%d] warning: format changed\n",
						devp->index);
				sm_print_prestable |= (1<<1);
			}
		}

		if (nosig || fmt_changed) {
			++sm_p->state_cnt;
			if ((port == TVIN_PORT_CVBS3) &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG))
				prestable_out_cnt = atv_prestable_out_cnt;
			else
				prestable_out_cnt = other_stable_out_cnt;
			if (sm_p->state_cnt >= prestable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable)
					pr_info("[smr.%d] prestable --> unstable\n",
							devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_unstable = 0;
				sm_print_prestable = 0;
				break;
			}
		} else {
			sm_p->state_cnt = 0;

			if ((port == TVIN_PORT_CVBS3) &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG)) {
				++sm_p->exit_prestable_cnt;
				if (sm_p->exit_prestable_cnt <
					atv_prestable_out_cnt)
					break;
				else
					sm_p->exit_prestable_cnt = 0;
			}

			sm_p->state = TVIN_SM_STATUS_STABLE;
			info->status = TVIN_SIG_STATUS_STABLE;
			if (sm_debug_enable)
				pr_info("[smr.%d] %ums prestable --> stable\n",
						devp->index,
						jiffies_to_msecs(jiffies));
			sm_print_nosig  = 0;
			sm_print_notsup = 0;
			sm_print_prestable = 0;
		}
		break;
	}
	case TVIN_SM_STATUS_STABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int stable_out_cnt = 0;
		unsigned int stable_fmt = 0;
		devp->unstable_flag = true;

		if (sm_ops->nosig(devp->frontend)) {
			nosig = true;
			if (sm_debug_enable && !sm_print_fmt_nosig) {
				pr_info("[smr.%d] warning: no signal\n",
						devp->index);
				sm_print_fmt_nosig = 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend)) {
			fmt_changed = true;
			if (sm_debug_enable && !sm_print_fmt_chg) {
				pr_info("[smr.%d] warning: format changed\n",
						devp->index);
				sm_print_fmt_chg = 1;
			}
		}
		/* dynamic adjust cutwindow for atv test */
		if ((port >= TVIN_PORT_CVBS0) &&
			(port <= TVIN_PORT_CVBS3))
			vdin_auto_de_handler(devp);
		if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS3) &&
			devp->auto_ratio_en && sm_ops->get_sig_property)
			sm_ops->get_sig_property(devp->frontend, prop);
		/* hdmirx_color_fmt_handler(devp); */
#if 0
			if (sm_ops->pll_lock(devp->frontend)) {
				pll_lock = true;
			} else {
				pll_lock = false;
				if (sm_debug_enable)
					pr_info("[smr] warning: pll lock failed\n");
			}
#endif

		if (nosig || fmt_changed /* || !pll_lock */) {
			++sm_p->state_cnt;
			if (((port == TVIN_PORT_CVBS3) ||
				(port == TVIN_PORT_CVBS0)) &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG))
				stable_out_cnt = sm_p->atv_stable_out_cnt;
			else if ((port >= TVIN_PORT_HDMI0) &&
					(port <= TVIN_PORT_HDMI7))
				stable_out_cnt = hdmi_stable_out_cnt;
			else
				stable_out_cnt = other_stable_out_cnt;
			/*add for atv snow*/
			if ((sm_p->state_cnt >= atv_stable_fmt_check_cnt) &&
				(port == TVIN_PORT_CVBS3) &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG))
				atv_stable_fmt_check_enable = 1;
			if (sm_p->state_cnt >= stable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (sm_debug_enable)
					pr_info("[smr.%d] stable --> unstable\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_unstable = 0;
				sm_print_fmt_nosig = 0;
				sm_print_fmt_chg = 0;
				sm_print_prestable = 0;
				atv_stable_fmt_check_enable = 0;
			}
		} else {
			/*add for atv snow*/
			if ((port == TVIN_PORT_CVBS3) &&
				atv_stable_fmt_check_enable &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG) &&
				(sm_ops->get_fmt && sm_ops->get_sig_property)) {
				sm_p->state_cnt = 0;
				stable_fmt =
					sm_ops->get_fmt(fe);
				if ((sm_atv_prestable_fmt != stable_fmt) &&
					(stable_fmt != TVIN_SIG_FMT_NULL)) {
					sm_ops->get_sig_property(fe, prop);
					memcpy(pre_prop, prop,
					sizeof(struct tvin_sig_property_s));
					devp->parm.info.trans_fmt =
						prop->trans_fmt;
					devp->parm.info.is_dvi =
						prop->dvi_info;
					devp->parm.info.fps =
						prop->fps;
					info->fmt = stable_fmt;
					atv_stable_fmt_check_enable = 0;
					if (sm_debug_enable)
						pr_info("[smr.%d] stable fmt changed:0x%x-->0x%x\n",
							devp->index,
							sm_atv_prestable_fmt,
							stable_fmt);
					sm_atv_prestable_fmt = stable_fmt;
				}
			}
			sm_p->state_cnt = 0;
			hdmirx_color_fmt_handler(devp);
		}
		break;
	}
	case TVIN_SM_STATUS_NULL:
	default:
		sm_p->state = TVIN_SM_STATUS_NOSIG;
		break;
	}
	if (sm_p->sig_status != info->status) {
		sm_p->sig_status = info->status;
		wake_up(&devp->queue);
	}
	signal_status = sm_p->sig_status;
}

/*
 * tvin state machine routine init
 *
 */

void tvin_smr_init(int index)
{
	sm_dev[index].sig_status = TVIN_SIG_STATUS_NULL;
	sm_dev[index].state = TVIN_SM_STATUS_NULL;
	sm_dev[index].atv_stable_out_cnt = atv_stable_out_cnt;
	sm_dev[index].atv_unstable_in_cnt = atv_unstable_in_cnt;
	sm_dev[index].back_nosig_max_cnt = back_nosig_max_cnt;
	sm_dev[index].atv_unstable_out_cnt = atv_unstable_out_cnt;
	sm_dev[index].hdmi_unstable_out_cnt = hdmi_unstable_out_cnt;
	tvin_smr_init_counter(index);
}

enum tvin_sm_status_e tvin_get_sm_status(int index)
{
	return sm_dev[index].state;
}
EXPORT_SYMBOL(tvin_get_sm_status);

int tvin_get_av_status(void)
{
	if (tvin_get_sm_status(0) == TVIN_SM_STATUS_STABLE)
		return true;

	return false;
}
EXPORT_SYMBOL(tvin_get_av_status);

