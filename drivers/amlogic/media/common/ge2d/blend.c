/*
 * drivers/amlogic/media/common/ge2d/blend.c
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
/* Linux Headers */
#include <linux/types.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/ge2d/ge2d.h>

void blend(struct ge2d_context_s *wq,
	   int src_x, int src_y, int src_w, int src_h,
	   int src2_x, int src2_y, int src2_w, int src2_h,
	   int dst_x, int dst_y, int dst_w, int dst_h,
	   int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = (op >> 8) & 0xff;
	if (ge2d_cmd_cfg->alpha_blend_mode >= BLENDOP_LOGIC) {
		ge2d_cmd_cfg->alpha_logic_op =
			ge2d_cmd_cfg->alpha_blend_mode - BLENDOP_LOGIC;
		ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	}

	ge2d_cmd_cfg->wait_done_flag   = 1;

	ge2d_wq_add_work(wq);
}
EXPORT_SYMBOL(blend);

void blend_noblk(struct ge2d_context_s *wq,
		 int src_x, int src_y, int src_w, int src_h,
		 int src2_x, int src2_y, int src2_w, int src2_h,
		 int dst_x, int dst_y, int dst_w, int dst_h,
		 int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;
	if (get_cpu_type() != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = (op >> 8) & 0xff;
	if (ge2d_cmd_cfg->alpha_blend_mode >= BLENDOP_LOGIC) {
		ge2d_cmd_cfg->alpha_logic_op =
			ge2d_cmd_cfg->alpha_blend_mode - BLENDOP_LOGIC;
		ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	}

	ge2d_cmd_cfg->wait_done_flag = 0;

	ge2d_wq_add_work(wq);
}
EXPORT_SYMBOL(blend_noblk);
void blend_noalpha(struct ge2d_context_s *wq,
	   int src_x, int src_y, int src_w, int src_h,
	   int src2_x, int src2_y, int src2_w, int src2_h,
	   int dst_x, int dst_y, int dst_w, int dst_h,
	   int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_SET;

	ge2d_cmd_cfg->wait_done_flag   = 1;

	ge2d_wq_add_work(wq);
}
EXPORT_SYMBOL(blend_noalpha);


void blend_noalpha_noblk(struct ge2d_context_s *wq,
	   int src_x, int src_y, int src_w, int src_h,
	   int src2_x, int src2_y, int src2_w, int src2_h,
	   int dst_x, int dst_y, int dst_w, int dst_h,
	   int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_SET;

	ge2d_cmd_cfg->wait_done_flag   = 0;

	ge2d_wq_add_work(wq);
}
EXPORT_SYMBOL(blend_noalpha_noblk);
