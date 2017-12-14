/*
 * drivers/amlogic/media/osd/osd_progressbar.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/of_fdt.h>
#include <linux/console.h>
#include <linux/amlogic/ge2d/ge2d.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include "osd_canvas.h"
#include "osd_fb.h"
#include "osd_hw.h"
#include "osd_io.h"
#include "osd_reg.h"

struct src_dst_info_s {
	struct rectangle_s  src_rect;
	struct rectangle_s  dst_rect;
	unsigned int		color;
};

struct osd_progress_bar_s {
	struct ge2d_context_s  *ge2d_context;
	const struct vinfo_s *vinfo;
	struct src_dst_info_s  op_info;
	u32 bar_border;
	u32 bar_width;
	u32 bar_height;
};

static struct osd_progress_bar_s progress_bar;
static struct config_para_s ge2d_config;
static struct ge2d_context_s *ge2d_context;

static int init_fb1_first(const struct vinfo_s *vinfo)
{
	struct osd_ctl_s  osd_ctl;
	const struct color_bit_define_s  *color;
	u32 reg = 0, data32 = 0;
	size_t osd_size;
	void __iomem *osd_vaddr;

	osd_ctl.index = 1;
	color = &default_color_format_array[31];

	osd_ctl.addr = get_fb_rmem_paddr(osd_ctl.index);
	osd_vaddr = get_fb_rmem_vaddr(osd_ctl.index);
	osd_size = get_fb_rmem_size(osd_ctl.index);

	osd_ctl.xres = vinfo->width;
	osd_ctl.yres = vinfo->height;
	osd_ctl.xres_virtual = osd_ctl.xres;
	osd_ctl.yres_virtual = osd_ctl.yres;
	osd_ctl.disp_start_x = 0;
	osd_ctl.disp_end_x = osd_ctl.xres - 1;
	osd_ctl.disp_start_y = (vinfo->height * 9) / 10;
	osd_ctl.disp_end_y = osd_ctl.yres - 1;

	reg = osd_ctl.index == 0 ? VIU_OSD1_BLK0_CFG_W0 : VIU_OSD2_BLK0_CFG_W0;
	data32 = VSYNCOSD_RD_MPEG_REG(reg) & (~(0xf<<8));
	data32 |=  color->hw_blkmode << 8; /* osd_blk_mode */
	VSYNCOSD_WR_MPEG_REG(reg, data32);

	memset(osd_vaddr, 0, osd_size);
	pr_debug("addr is 0x%08x, xres is %d, yres is %d\n",
			osd_ctl.addr, osd_ctl.xres, osd_ctl.yres);
	osd_setup_hw(osd_ctl.index,
		&osd_ctl,
		0,
		0,
		osd_ctl.xres,
		osd_ctl.yres,
		osd_ctl.xres_virtual,
		osd_ctl.yres_virtual,
		osd_ctl.disp_start_x,
		osd_ctl.disp_start_y,
		osd_ctl.disp_end_x,
		osd_ctl.disp_end_y,
		osd_ctl.addr,
		NULL,
		color);

	return 0;
}

int osd_show_progress_bar(u32 percent)
{
	static u32 progress;
	u32 step = 1;
	/* wait_queue_head_t  wait_head; */
	struct osd_fb_dev_s *fb_dev;
	struct ge2d_context_s  *context = progress_bar.ge2d_context;
	struct src_dst_info_s  *op_info = &progress_bar.op_info;

	if (context == NULL) {
		/* osd_init_progress_bar(); */
		pr_debug("context is NULL\n");
		return -1;
	}
	fb_dev = gp_fbdev_list[1];
	if (fb_dev == NULL) {
		pr_debug("fb1 should exit!!!");
		return -EFAULT;
	}

	while (progress < percent) {
		pr_debug("progress is %d, x: [%d], y: [%d], w: [%d], h: [%d]\n",
			progress, op_info->dst_rect.x, op_info->dst_rect.y,
			op_info->dst_rect.w, op_info->dst_rect.h);


		fillrect(context, op_info->dst_rect.x,
			op_info->dst_rect.y,
			op_info->dst_rect.w,
			op_info->dst_rect.h,
			op_info->color);

		/* wait_event_interruptible_timeout(wait_head,0,4); */
		progress += step;
		op_info->dst_rect.x += op_info->dst_rect.w;
		op_info->color -= (0xff*step/100) << 16;
	}

	if (percent == 100) {
		progress = 0;
		osd_blank(1, fb_dev->fb_info);
		destroy_ge2d_work_queue(progress_bar.ge2d_context);
	}

	return 0;
}
EXPORT_SYMBOL(osd_show_progress_bar);

int osd_init_progress_bar(void)
{
	struct src_dst_info_s  *op_info = &progress_bar.op_info;
	const struct vinfo_s *vinfo = progress_bar.vinfo;
	struct osd_fb_dev_s *fb_dev;
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	struct canvas_s cs;
#endif
	u32 cs_addr, cs_width, cs_height;
	struct config_para_s *cfg = &ge2d_config;
	struct ge2d_context_s *context = ge2d_context;
	u32 step = 1;

	if (osd_hw.osd_meson_dev.has_ver == OSD_SIMPLE)
		return 0;

	memset(&progress_bar, 0, sizeof(struct osd_progress_bar_s));

	vinfo = get_current_vinfo();

	progress_bar.bar_border =
		(((vinfo->field_height ?
		vinfo->field_height :
		vinfo->height) * 4 / 720)>>2)<<2;
	progress_bar.bar_width =
		(((vinfo->width * 200 / 1280)>>2)<<2) + progress_bar.bar_border;
	progress_bar.bar_height =
		(((vinfo->field_height ?
		vinfo->field_height :
		vinfo->height) * 32 / 720) >> 2) << 2;

	if (!init_fb1_first(vinfo)) {
		fb_dev = gp_fbdev_list[1];
		if (fb_dev == NULL) {
			pr_debug("fb1 should exit!!!");
			return -EFAULT;
		}
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
		canvas_read(OSD2_CANVAS_INDEX, &cs);
		cs_addr = cs.addr;
		cs_width = cs.width / 4;
		cs_height = cs.height;
#else
		cs_addr = 0;
		cs_width = 0;
		cs_height = 0;
#endif
		context = create_ge2d_work_queue();
		if (!context) {
			pr_debug("create work queue error\n");
			return -EFAULT;
		}

		memset(cfg, 0, sizeof(struct config_para_s));
		cfg->src_dst_type = OSD1_OSD1;
		cfg->src_format = GE2D_FORMAT_S32_ARGB;
		cfg->src_planes[0].addr = cs_addr;
		cfg->src_planes[0].w = cs_width;
		cfg->src_planes[0].h = cs_height;
		cfg->dst_planes[0].addr = cs_addr;
		cfg->dst_planes[0].w = cs_width;
		cfg->dst_planes[0].h = cs_height;

		if (ge2d_context_config(context, cfg) < 0) {
			pr_debug("ge2d config error.\n");
			return -EFAULT;
		}

		if (context == NULL) {
			pr_debug("ge2d_context is NULL!!!!!!\n");
			return -EFAULT;
		}
		progress_bar.ge2d_context = context;
		pr_debug("progress bar setup ge2d device OK\n");
		/* show fb1 */
		console_lock();
		osd_blank(0, fb_dev->fb_info);
		console_unlock();
		op_info->color = 0x555555ff;
		op_info->dst_rect.x =
			(vinfo->width / 2) - progress_bar.bar_width;
		op_info->dst_rect.y = 0;
		op_info->dst_rect.w = progress_bar.bar_width * 2;
		op_info->dst_rect.h = progress_bar.bar_height;

		pr_debug("fill==dst:%d-%d-%d-%d\n",
			op_info->dst_rect.x, op_info->dst_rect.y,
			op_info->dst_rect.w, op_info->dst_rect.h);

		fillrect(context, op_info->dst_rect.x,
			op_info->dst_rect.y,
			op_info->dst_rect.w,
			op_info->dst_rect.h,
			op_info->color);
	} else {
		pr_debug("fb1 init failed, exit!!!");
		return -EFAULT;
	}

	/* initial op info before draw actrualy */
	op_info->dst_rect.x += progress_bar.bar_border;
	op_info->dst_rect.y += progress_bar.bar_border;
	op_info->dst_rect.w =
		(progress_bar.bar_width - progress_bar.bar_border)
		* 2 * step/100;
	op_info->dst_rect.h =
		progress_bar.bar_height - progress_bar.bar_border * 2;
	op_info->color = 0xffffff;

	return 0;
}
EXPORT_SYMBOL(osd_init_progress_bar);

