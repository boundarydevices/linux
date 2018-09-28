/*
 * drivers/amlogic/media/osd/osd_antiflicker.c
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
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#include <linux/amlogic/media/ge2d/ge2d.h>
#endif
/* Local Headers */
#include "osd_canvas.h"
#include "osd_antiflicker.h"
#include "osd_log.h"
#include "osd_io.h"
#include "osd_hw.h"


#ifdef OSD_GE2D_ANTIFLICKER_SUPPORT
struct osd_antiflicker_s {
	bool inited;
	u32 yoffset;
	u32 yres;
	struct config_para_ex_s ge2d_config;
	struct ge2d_context_s *ge2d_context;
};

static DEFINE_MUTEX(osd_antiflicker_mutex);
static struct osd_antiflicker_s ge2d_osd_antiflicker;

void osd_antiflicker_enable(u32 enable)
{
	ge2d_antiflicker_enable(ge2d_osd_antiflicker.ge2d_context, enable);
}

static int osd_antiflicker_process(void)
{
	int ret = -1;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	struct canvas_s cs, cd;
#endif
	u32 cs_addr = 0, cs_width = 0, cs_height = 0;
	u32 cd_addr = 0, cd_width = 0, cd_height = 0;
	u32 x0 = 0;
	u32 y0 = 0;
	u32 y1 = 0;
	u32 yres = 0;
	struct config_para_ex_s *ge2d_config = NULL;
	struct ge2d_context_s *context = NULL;

	ge2d_config = &ge2d_osd_antiflicker.ge2d_config;
	context = ge2d_osd_antiflicker.ge2d_context;

	mutex_lock(&osd_antiflicker_mutex);
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	if (osd_hw.osd_meson_dev.cpu_id != MESON_CPU_MAJOR_ID_AXG) {
		canvas_read(OSD1_CANVAS_INDEX, &cs);
		canvas_read(OSD1_CANVAS_INDEX, &cd);
		cs_addr = cs.addr;
		cs_width = cs.width;
		cs_height = cs.height;
		cd_addr = cd.addr;
		cd_width = cd.width;
		cd_height = cd.height;
	} else {
		osd_get_info(OSD1, &cs_addr,
			&cs_width, &cs_height);
		osd_get_info(OSD2, &cs_addr,
			&cs_width, &cs_height);
	}
#else
	osd_get_info(OSD1, &cs_addr,
		&cs_width, &cs_height);
	osd_get_info(OSD2, &cs_addr,
		&cs_width, &cs_height);
#endif

	if (ge2d_osd_antiflicker.yoffset > 0) {
		y0 = ge2d_osd_antiflicker.yoffset;
		y1 = ge2d_osd_antiflicker.yoffset;
	}

	yres = cs_height / ge2d_osd_antiflicker.yres;
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;

	ge2d_config->src_planes[0].addr = cs_addr;
	ge2d_config->src_planes[0].w = cs_width / 4;
	ge2d_config->src_planes[0].h = cs_height;

	ge2d_config->dst_planes[0].addr = cd_addr;
	ge2d_config->dst_planes[0].w = cd_width / 4;
	ge2d_config->dst_planes[0].h = cd_height;

	ge2d_config->src_para.canvas_index = OSD1_CANVAS_INDEX;
	ge2d_config->src_para.mem_type = CANVAS_OSD0;
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

	ge2d_config->dst_para.canvas_index = OSD1_CANVAS_INDEX;
	ge2d_config->dst_para.mem_type = CANVAS_OSD0;
	ge2d_config->dst_para.format = GE2D_FORMAT_S32_ARGB;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = cd_width / 4;
	ge2d_config->dst_para.height = cd_height;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_xy_swap = 0;

	ret = ge2d_context_config_ex(context, ge2d_config);
	mutex_unlock(&osd_antiflicker_mutex);

	if (ret < 0) {
		osd_log_info("++ osd antiflicker ge2d config ex error.\n");
		return ret;
	}

	stretchblt(context, x0, y0, cs_width / 4, (cs_height / yres), x0, y1,
		   cd_width / 4, (cd_height / yres));
	return ret;
}

void osd_antiflicker_update_pan(u32 yoffset, u32 yres)
{
	int ret = -1;

	if (!ge2d_osd_antiflicker.inited)
		return;

	mutex_lock(&osd_antiflicker_mutex);
	ge2d_osd_antiflicker.yoffset = yoffset;
	ge2d_osd_antiflicker.yres = yres;
	mutex_unlock(&osd_antiflicker_mutex);
#if 0
	osd_antiflicker_process();
	osd_antiflicker_process_2();
#endif
	ret = osd_antiflicker_process();

	if (ret < 0)
		osd_antiflicker_task_stop();
}

int osd_antiflicker_task_start(void)
{
	if (ge2d_osd_antiflicker.inited) {
		osd_log_info("osd_antiflicker_task already started.\n");
		return 0;
	}

	osd_log_info("osd_antiflicker_task start.\n");

	if (ge2d_osd_antiflicker.ge2d_context == NULL)
		ge2d_osd_antiflicker.ge2d_context = create_ge2d_work_queue();

	memset(&ge2d_osd_antiflicker.ge2d_config,
			0, sizeof(struct config_para_ex_s));
	ge2d_osd_antiflicker.inited = true;

	return 0;
}

void osd_antiflicker_task_stop(void)
{
	if (!ge2d_osd_antiflicker.inited) {
		osd_log_info("osd_antiflicker_task already stopped.\n");
		return;
	}

	osd_log_info("osd_antiflicker_task stop.\n");

	if (ge2d_osd_antiflicker.ge2d_context) {
		destroy_ge2d_work_queue(ge2d_osd_antiflicker.ge2d_context);
		ge2d_osd_antiflicker.ge2d_context = NULL;
	}

	ge2d_osd_antiflicker.inited = false;
}
#endif
