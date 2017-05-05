/*
 * drivers/amlogic/media/osd_ext/osd_clone.c
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
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#include <linux/amlogic/media/ge2d/ge2d.h>
#endif


/* Local Headers */
#include <osd/osd_io.h>
#include <osd/osd_log.h>
#include <osd/osd_canvas.h>
#include "osd_clone.h"
#include "osd_hw.h"

#ifdef OSD_EXT_GE2D_CLONE_SUPPORT
struct osd_ext_clone_s {
	bool inited;
	int angle;
	int pan;
	struct config_para_ex_s ge2d_config;
	struct ge2d_context_s *ge2d_context;
};

static DEFINE_MUTEX(osd_ext_clone_mutex);
static struct osd_ext_clone_s s_osd_ext_clone;

static void osd_clone_process(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	struct canvas_s cs, cd;
#endif
	u32 cs_addr  = 0, cs_width = 0, cs_height = 0;
	u32 cd_addr = 0, cd_width = 0, cd_height = 0;

	u32 x0 = 0;
	u32 y0 = 0;
	u32 y1 = 0;
	unsigned char x_rev = 0;
	unsigned char y_rev = 0;
	unsigned char xy_swap = 0;
	struct config_para_ex_s *ge2d_config = &s_osd_ext_clone.ge2d_config;
	struct ge2d_context_s *context = s_osd_ext_clone.ge2d_context;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	if (get_cpu_type() != MESON_CPU_MAJOR_ID_AXG) {
		canvas_read(OSD1_CANVAS_INDEX, &cs);
		canvas_read(OSD2_CANVAS_INDEX, &cd);
		cs_addr = cs.addr;
		cs_width = cs.width;
		cs_height = cs.height;
		cd_addr = cd.addr;
		cd_width = cd.width;
		cd_height = cd.height;
	} else {
		osd_ext_get_info(OSD1, &cs_addr,
			&cs_width, &cs_height);
		osd_ext_get_info(OSD2, &cs_addr,
			&cs_width, &cs_height);
	}
#else
	osd_ext_get_info(OSD1, &cs_addr,
		&cs_width, &cs_height);
	osd_ext_get_info(OSD2, &cs_addr,
		&cs_width, &cs_height);
#endif
	if (s_osd_ext_clone.pan == 1) {
		y0 = cs_height / 2;
		y1 = cd_height / 2;
	}

	if (s_osd_ext_clone.angle == 1) {
		xy_swap = 1;
		x_rev = 1;
	} else if (s_osd_ext_clone.angle == 2) {
		x_rev = 1;
		y_rev = 1;
	} else if (s_osd_ext_clone.angle == 3) {
		xy_swap = 1;
		y_rev = 1;
	}

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;

	ge2d_config->src_planes[0].addr = cs_addr;
	ge2d_config->src_planes[0].w = cs_width / 4;
	ge2d_config->src_planes[0].h = cs_height;

	ge2d_config->dst_planes[0].addr = cd_addr;
	ge2d_config->dst_planes[0].w = cd_width / 4;
	ge2d_config->dst_planes[0].h = cd_height;

	ge2d_config->src_para.canvas_index = OSD1_CANVAS_INDEX;
	ge2d_config->src_para.mem_type = CANVAS_OSD0;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S32_ARGB;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = cs_width / 4;
	ge2d_config->src_para.height = cs_height;

	ge2d_config->dst_para.canvas_index = OSD3_CANVAS_INDEX;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S32_ARGB;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = cd_width / 4;
	ge2d_config->dst_para.height = cd_height;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.x_rev = x_rev;
	ge2d_config->dst_para.y_rev = y_rev;
	ge2d_config->dst_xy_swap = xy_swap;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		osd_log_err("++ osd clone ge2d config error.\n");
		return;
	}

	stretchblt(context, x0, y0, cs_width / 4, cs_height / 2,
			x0, y1, cd_width / 4, cd_height / 2);
}

void osd_ext_clone_update_pan(int pan)
{
	if (!s_osd_ext_clone.inited)
		return;

	mutex_lock(&osd_ext_clone_mutex);
	s_osd_ext_clone.pan = pan;
	mutex_unlock(&osd_ext_clone_mutex);
	osd_clone_process();
}

void osd_ext_clone_set_angle(int angle)
{
	mutex_lock(&osd_ext_clone_mutex);
	s_osd_ext_clone.angle = angle;
	mutex_unlock(&osd_ext_clone_mutex);
}

int osd_ext_clone_task_start(void)
{
	if (s_osd_ext_clone.inited) {
		osd_log_info("osd_ext_clone_task already started.\n");
		return 0;
	}

	osd_log_info("osd_ext_clone_task start.\n");

	if (s_osd_ext_clone.ge2d_context == NULL)
		s_osd_ext_clone.ge2d_context = create_ge2d_work_queue();

	memset(&s_osd_ext_clone.ge2d_config,
			0, sizeof(struct config_para_ex_s));
	s_osd_ext_clone.inited = true;

	return 0;
}

void osd_ext_clone_task_stop(void)
{
	if (!s_osd_ext_clone.inited) {
		osd_log_info("osd_ext_clone_task already stopped.\n");
		return;
	}

	osd_log_info("osd_ext_clone_task stop.\n");

	if (s_osd_ext_clone.ge2d_context) {
		destroy_ge2d_work_queue(s_osd_ext_clone.ge2d_context);
		s_osd_ext_clone.ge2d_context = NULL;
	}

	s_osd_ext_clone.inited = false;
}
#endif
