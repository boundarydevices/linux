/*
 * drivers/amlogic/media/video_sink/video_keeper.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
/*#include <linux/amlogic/gpio-amlogic.h>*/
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#ifdef CONFIG_GE2D_KEEP_FRAME
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>

#include "video_priv.h"
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/utils/vdec_reg.h>

#define MEM_NAME "video-keeper"
static DEFINE_MUTEX(video_keeper_mutex);

static unsigned long keep_y_addr, keep_u_addr, keep_v_addr;
static int keep_video_on;
static int keep_video_pip_on;
static int keep_id;
static int keep_head_id;
static int keep_pip_id;
static int keep_pip_head_id;
static int keep_el_id;
static int keep_el_head_id;
static int keep_pip_el_id;
static int keep_pip_el_head_id;

#define Y_BUFFER_SIZE   0x400000	/* for 1920*1088 */
#define U_BUFFER_SIZE   0x100000	/* compatible with NV21 */
#define V_BUFFER_SIZE   0x80000

#define RESERVE_CLR_FRAME

static inline ulong keep_phy_addr(unsigned long addr)
{
	return addr;
}

#ifdef CONFIG_GE2D_KEEP_FRAME
static int display_canvas_y_dup;
static int display_canvas_u_dup;
static int display_canvas_v_dup;
static struct ge2d_context_s *ge2d_video_context;

static int ge2d_videotask_init(void)
{
	const char *keep_owner = "keepframe";

	if (ge2d_video_context == NULL)
		ge2d_video_context = create_ge2d_work_queue();

	if (ge2d_video_context == NULL) {
		pr_info("create_ge2d_work_queue video task failed\n");
		return -1;
	}
	if (!display_canvas_y_dup)
		display_canvas_y_dup = canvas_pool_map_alloc_canvas(keep_owner);
	if (!display_canvas_u_dup)
		display_canvas_u_dup = canvas_pool_map_alloc_canvas(keep_owner);
	if (!display_canvas_v_dup)
		display_canvas_v_dup = canvas_pool_map_alloc_canvas(keep_owner);
	pr_info("create_ge2d_work_queue video task ok\n");

	return 0;
}



static int ge2d_videotask_release(void)
{
	if (ge2d_video_context) {
		destroy_ge2d_work_queue(ge2d_video_context);
		ge2d_video_context = NULL;
	}
	if (display_canvas_y_dup)
		canvas_pool_map_free_canvas(display_canvas_y_dup);
	if (display_canvas_u_dup)
		canvas_pool_map_free_canvas(display_canvas_u_dup);
	if (display_canvas_v_dup)
		canvas_pool_map_free_canvas(display_canvas_v_dup);

	return 0;
}

static int ge2d_store_frame_YUV444(u32 cur_index)
{
	u32 y_index, des_index, src_index;
	struct canvas_s cs, cd;
	ulong yaddr;
	u32 ydupindex;
	struct config_para_ex_s ge2d_config;

	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));

	ydupindex = display_canvas_y_dup;

	pr_info("ge2d_store_frame_YUV444 cur_index:s:0x%x\n", cur_index);
	/* pr_info("ge2d_store_frame cur_index:d:0x%x\n", canvas_tab[0]); */
	y_index = cur_index & 0xff;
	canvas_read(y_index, &cs);

	yaddr = keep_phy_addr(keep_y_addr);
	canvas_config(ydupindex,
		(ulong) yaddr,
		cs.width, cs.height,
		CANVAS_ADDR_NOWRAP, cs.blkmode);

	canvas_read(ydupindex, &cd);
	src_index = y_index;
	des_index = ydupindex;

	pr_info("ge2d_canvas_dup ADDR srcy[0x%lx] des[0x%lx]\n", cs.addr,
	       cd.addr);

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	ge2d_config.src_planes[0].addr = cs.addr;
	ge2d_config.src_planes[0].w = cs.width;
	ge2d_config.src_planes[0].h = cs.height;

	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;

	ge2d_config.src_para.canvas_index = src_index;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = GE2D_FORMAT_M24_YUV444;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = cs.width;
	ge2d_config.src_para.height = cs.height;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.canvas_index = des_index;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = GE2D_FORMAT_M24_YUV444;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = cs.width;
	ge2d_config.dst_para.height = cs.height;

	if (ge2d_context_config_ex(ge2d_video_context, &ge2d_config) < 0) {
		pr_info("ge2d_context_config_ex failed\n");
		return -1;
	}

	stretchblt_noalpha(ge2d_video_context,
		0, 0, cs.width, cs.height,
		0, 0, cs.width, cs.height);

	return 0;
}

/* static u32 canvas_tab[1]; */
static int ge2d_store_frame_NV21(u32 cur_index)
{
	u32 y_index, u_index, des_index, src_index;
	struct canvas_s cs0, cs1, cd;
	ulong yaddr, uaddr;
	u32 ydupindex, udupindex;
	struct config_para_ex_s ge2d_config;

	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));

	ydupindex = display_canvas_y_dup;
	udupindex = display_canvas_u_dup;

	pr_info("ge2d_store_frame_NV21 cur_index:s:0x%x\n", cur_index);

	/* pr_info("ge2d_store_frame cur_index:d:0x%x\n", canvas_tab[0]); */
	yaddr = keep_phy_addr(keep_y_addr);
	uaddr = keep_phy_addr(keep_u_addr);

	y_index = cur_index & 0xff;
	u_index = (cur_index >> 8) & 0xff;

	canvas_read(y_index, &cs0);
	canvas_read(u_index, &cs1);
	canvas_config(ydupindex,
		(ulong) yaddr,
		cs0.width, cs0.height,
		CANVAS_ADDR_NOWRAP, cs0.blkmode);
	canvas_config(udupindex,
		(ulong) uaddr,
		cs1.width, cs1.height,
		CANVAS_ADDR_NOWRAP, cs1.blkmode);

	canvas_read(ydupindex, &cd);
	src_index = ((y_index & 0xff) | ((u_index << 8) & 0x0000ff00));
	des_index = ((ydupindex & 0xff) | ((udupindex << 8) & 0x0000ff00));

	pr_info("ge2d_store_frame d:0x%x\n", des_index);

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	ge2d_config.src_planes[0].addr = cs0.addr;
	ge2d_config.src_planes[0].w = cs0.width;
	ge2d_config.src_planes[0].h = cs0.height;
	ge2d_config.src_planes[1].addr = cs1.addr;
	ge2d_config.src_planes[1].w = cs1.width;
	ge2d_config.src_planes[1].h = cs1.height;

	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;

	ge2d_config.src_para.canvas_index = src_index;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = GE2D_FORMAT_M24_NV21;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = cs0.width;
	ge2d_config.src_para.height = cs0.height;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.canvas_index = des_index;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = GE2D_FORMAT_M24_NV21;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = cs0.width;
	ge2d_config.dst_para.height = cs0.height;

	if (ge2d_context_config_ex(ge2d_video_context, &ge2d_config) < 0) {
		pr_info("ge2d_context_config_ex failed\n");
		return -1;
	}

	stretchblt_noalpha(ge2d_video_context,
		0, 0, cs0.width, cs0.height,
		0, 0, cs0.width, cs0.height);

	return 0;
}

/* static u32 canvas_tab[1]; */
static int ge2d_store_frame_YUV420(u32 cur_index)
{
	u32 y_index, u_index, v_index;
	struct canvas_s cs, cd;
	ulong yaddr, uaddr, vaddr;
	u32 ydupindex, udupindex, vdupindex;
	struct config_para_ex_s ge2d_config;

	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));

	ydupindex = display_canvas_y_dup;
	udupindex = display_canvas_u_dup;
	vdupindex = display_canvas_v_dup;

	pr_info("ge2d_store_frame_YUV420 cur_index:s:0x%x\n", cur_index);
	/* operation top line */
	/* Y data */
	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	y_index = cur_index & 0xff;
	canvas_read(y_index, &cs);
	ge2d_config.src_planes[0].addr = cs.addr;
	ge2d_config.src_planes[0].w = cs.width;
	ge2d_config.src_planes[0].h = cs.height;
	ge2d_config.src_planes[1].addr = 0;
	ge2d_config.src_planes[1].w = 0;
	ge2d_config.src_planes[1].h = 0;
	ge2d_config.src_planes[2].addr = 0;
	ge2d_config.src_planes[2].w = 0;
	ge2d_config.src_planes[2].h = 0;

	yaddr = keep_phy_addr(keep_y_addr);
	canvas_config(ydupindex,
		(ulong) yaddr,
		cs.width, cs.height,
		CANVAS_ADDR_NOWRAP, cs.blkmode);

	canvas_read(ydupindex, &cd);
	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;
	ge2d_config.dst_planes[1].addr = 0;
	ge2d_config.dst_planes[1].w = 0;
	ge2d_config.dst_planes[1].h = 0;
	ge2d_config.dst_planes[2].addr = 0;
	ge2d_config.dst_planes[2].w = 0;
	ge2d_config.dst_planes[2].h = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_key.key_color = 0;

	ge2d_config.src_para.canvas_index = y_index;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = GE2D_FMT_S8_Y;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = cs.width;
	ge2d_config.src_para.height = cs.height;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.canvas_index = ydupindex;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = GE2D_FMT_S8_Y;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = cs.width;
	ge2d_config.dst_para.height = cs.height;

	if (ge2d_context_config_ex(ge2d_video_context, &ge2d_config) < 0) {
		pr_info("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(ge2d_video_context,
		0, 0, cs.width, cs.height,
		0, 0, cs.width, cs.height);

	/* U data */
	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	u_index = (cur_index >> 8) & 0xff;
	canvas_read(u_index, &cs);
	ge2d_config.src_planes[0].addr = cs.addr;
	ge2d_config.src_planes[0].w = cs.width;
	ge2d_config.src_planes[0].h = cs.height;
	ge2d_config.src_planes[1].addr = 0;
	ge2d_config.src_planes[1].w = 0;
	ge2d_config.src_planes[1].h = 0;
	ge2d_config.src_planes[2].addr = 0;
	ge2d_config.src_planes[2].w = 0;
	ge2d_config.src_planes[2].h = 0;

	uaddr = keep_phy_addr(keep_u_addr);
	canvas_config(udupindex,
		(ulong) uaddr,
		cs.width, cs.height,
		CANVAS_ADDR_NOWRAP, cs.blkmode);

	canvas_read(udupindex, &cd);
	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;
	ge2d_config.dst_planes[1].addr = 0;
	ge2d_config.dst_planes[1].w = 0;
	ge2d_config.dst_planes[1].h = 0;
	ge2d_config.dst_planes[2].addr = 0;
	ge2d_config.dst_planes[2].w = 0;
	ge2d_config.dst_planes[2].h = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_key.key_color = 0;

	ge2d_config.src_para.canvas_index = u_index;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = GE2D_FMT_S8_CB;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = cs.width;
	ge2d_config.src_para.height = cs.height;

	ge2d_config.dst_para.canvas_index = udupindex;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = GE2D_FMT_S8_CB;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = cs.width;
	ge2d_config.dst_para.height = cs.height;

	if (ge2d_context_config_ex(ge2d_video_context, &ge2d_config) < 0) {
		pr_info("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(ge2d_video_context,
		0, 0, cs.width, cs.height,
		0, 0, cs.width, cs.height);

	/* operation top line */
	/* V data */
	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;

	v_index = (cur_index >> 16) & 0xff;
	canvas_read(v_index, &cs);
	ge2d_config.src_planes[0].addr = cs.addr;
	ge2d_config.src_planes[0].w = cs.width;
	ge2d_config.src_planes[0].h = cs.height;
	ge2d_config.src_planes[1].addr = 0;
	ge2d_config.src_planes[1].w = 0;
	ge2d_config.src_planes[1].h = 0;
	ge2d_config.src_planes[2].addr = 0;
	ge2d_config.src_planes[2].w = 0;
	ge2d_config.src_planes[2].h = 0;

	vaddr = keep_phy_addr(keep_v_addr);
	canvas_config(vdupindex,
		(ulong) vaddr,
		cs.width, cs.height,
		CANVAS_ADDR_NOWRAP, cs.blkmode);

	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;
	ge2d_config.dst_planes[1].addr = 0;
	ge2d_config.dst_planes[1].w = 0;
	ge2d_config.dst_planes[1].h = 0;
	ge2d_config.dst_planes[2].addr = 0;
	ge2d_config.dst_planes[2].w = 0;
	ge2d_config.dst_planes[2].h = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_key.key_color = 0;

	ge2d_config.src_para.canvas_index = v_index;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = GE2D_FMT_S8_CR;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = cs.width;
	ge2d_config.src_para.height = cs.height;

	ge2d_config.dst_para.canvas_index = vdupindex;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = GE2D_FMT_S8_CR;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = cs.width;
	ge2d_config.dst_para.height = cs.height;

	if (ge2d_context_config_ex(ge2d_video_context, &ge2d_config) < 0) {
		pr_info("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(ge2d_video_context,
		0, 0, cs.width, cs.height,
		0, 0, cs.width, cs.height);
	return 0;
}

static void ge2d_keeplastframe_block(int cur_index, int format)
{
	u32 y_index, u_index, v_index;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	u32 y_index2, u_index2, v_index2;
#endif

	video_module_lock();

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	y_index = disp_canvas_index[0][0];
	y_index2 = disp_canvas_index[1][0];
	u_index = disp_canvas_index[0][1];
	u_index2 = disp_canvas_index[1][1];
	v_index = disp_canvas_index[0][2];
	v_index2 = disp_canvas_index[1][2];
#else
	/*
	 *cur_index = READ_VCBUS_REG(VD1_IF0_CANVAS0 +
	 *	get_video_cur_dev->viu_off);
	 */
	y_index = cur_index & 0xff;
	u_index = (cur_index >> 8) & 0xff;
	v_index = (cur_index >> 16) & 0xff;
#endif

	switch (format) {
	case GE2D_FORMAT_M24_YUV444:
		ge2d_store_frame_YUV444(cur_index);
		canvas_update_addr(y_index, keep_phy_addr(keep_y_addr));
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		canvas_update_addr(y_index2, keep_phy_addr(keep_y_addr));
#endif
		break;
	case GE2D_FORMAT_M24_NV21:
		ge2d_store_frame_NV21(cur_index);
		canvas_update_addr(y_index, keep_phy_addr(keep_y_addr));
		canvas_update_addr(u_index, keep_phy_addr(keep_u_addr));
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		canvas_update_addr(y_index2, keep_phy_addr(keep_y_addr));
		canvas_update_addr(u_index2, keep_phy_addr(keep_u_addr));
#endif
		break;
	case GE2D_FORMAT_M24_YUV420:
		ge2d_store_frame_YUV420(cur_index);
		canvas_update_addr(y_index, keep_phy_addr(keep_y_addr));
		canvas_update_addr(u_index, keep_phy_addr(keep_u_addr));
		canvas_update_addr(v_index, keep_phy_addr(keep_v_addr));
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		canvas_update_addr(y_index2, keep_phy_addr(keep_y_addr));
		canvas_update_addr(u_index2, keep_phy_addr(keep_u_addr));
		canvas_update_addr(v_index2, keep_phy_addr(keep_v_addr));
#endif
		break;
	default:
		break;
	}
	video_module_unlock();

}
#endif

#define FETCHBUF_SIZE (64*1024) /*DEBUG_TMP*/
static int canvas_dup(ulong dst, ulong src_paddr, ulong size)
{
	void *src_addr = codec_mm_phys_to_virt(src_paddr);
	void *dst_addr = codec_mm_phys_to_virt(dst);

	if (src_paddr && dst && src_addr && dst_addr) {
		dma_addr_t dma_addr = 0;
		memcpy(dst_addr, src_addr, size);
		dma_addr = dma_map_single(get_video_device(), dst_addr,
			size, DMA_TO_DEVICE);
		dma_unmap_single(get_video_device(), dma_addr,
			FETCHBUF_SIZE, DMA_TO_DEVICE);
		return 1;
	}

	return 0;
}

#ifdef RESERVE_CLR_FRAME
static int free_alloced_keep_buffer(void)
{
	/*pr_info("free_alloced_keep_buffer %p.%p.%p\n",
		(void *)keep_y_addr, (void *)keep_u_addr, (void *)keep_v_addr);
		*/
	if (keep_y_addr) {
		codec_mm_free_for_dma(MEM_NAME, keep_y_addr);
		keep_y_addr = 0;
	}

	if (keep_u_addr) {
		codec_mm_free_for_dma(MEM_NAME, keep_u_addr);
		keep_u_addr = 0;
	}

	if (keep_v_addr) {
		codec_mm_free_for_dma(MEM_NAME, keep_v_addr);
		keep_v_addr = 0;
	}
	return 0;
}

static int alloc_keep_buffer(void)
{
	int flags = CODEC_MM_FLAGS_DMA |
		CODEC_MM_FLAGS_FOR_VDECODER;
#ifndef CONFIG_GE2D_KEEP_FRAME
	/*
	 *	if not used ge2d.
	 *	need CPU access.
	 */
	flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_FOR_VDECODER;
#endif
	if ((flags & CODEC_MM_FLAGS_FOR_VDECODER) &&
		codec_mm_video_tvp_enabled())
		/*TVP TODO for MULTI*/
		flags |= CODEC_MM_FLAGS_TVP;

	if (!keep_y_addr) {
		keep_y_addr = codec_mm_alloc_for_dma(
			MEM_NAME,
			PAGE_ALIGN(Y_BUFFER_SIZE)/PAGE_SIZE, 0, flags);
		if (!keep_y_addr) {
			pr_err("%s: failed to alloc y addr\n", __func__);
			goto err1;
		}
	}

	if (!keep_u_addr) {
		keep_u_addr = codec_mm_alloc_for_dma(
			MEM_NAME,
			PAGE_ALIGN(U_BUFFER_SIZE)/PAGE_SIZE, 0, flags);
		if (!keep_u_addr) {
			pr_err("%s: failed to alloc u addr\n", __func__);
			goto err1;
		}
	}

	if (!keep_v_addr) {
		keep_v_addr = codec_mm_alloc_for_dma(
			MEM_NAME,
			PAGE_ALIGN(V_BUFFER_SIZE)/PAGE_SIZE, 0, flags);
		if (!keep_v_addr) {
			pr_err("%s: failed to alloc v addr\n", __func__);
			goto err1;
		}
	}
	pr_info("alloced keep buffer yaddr=%p,u_addr=%p,v_addr=%p,tvp=%d\n",
		(void *)keep_y_addr,
		(void *)keep_u_addr,
		(void *)keep_v_addr,
		codec_mm_video_tvp_enabled());
	return 0;

 err1:
	free_alloced_keep_buffer();
	return -ENOMEM;
}

/*
 *flags,used per bit:
 *deflaut free alloced keeper buffer.
 *0x1: free scatters keeper..
 *0x2:
 */
void try_free_keep_video(int flags)
{
	int free_scatter_keeper = flags & 0x1;

	if (keep_video_on || free_scatter_keeper) {
		/*pr_info("disbled keep video before free keep buffer.\n");*/
		keep_video_on = 0;
		if (!get_video_enabled()) {
			/*if not disable video,changed to 2 for */
			pr_info("disbled video for next before free keep buffer!\n");
			_video_set_disable(VIDEO_DISABLE_FORNEXT);
		} else if (get_video_enabled()) {
			safe_disble_videolayer();
		}
	}
	mutex_lock(&video_keeper_mutex);
	video_keeper_new_frame_notify();
	free_alloced_keep_buffer();
	mutex_unlock(&video_keeper_mutex);
}
EXPORT_SYMBOL(try_free_keep_video);
#endif

void try_free_keep_videopip(int flags)
{
	int free_scatter_keeper = flags & 0x1;

	if (keep_video_pip_on || free_scatter_keeper) {
		/*pr_info("disbled keep video before free keep buffer.\n");*/
		keep_video_pip_on = 0;
		if (!get_videopip_enabled()) {
			/*if not disable video,changed to 2 for */
			pr_info("disbled videopip for next before free keep buffer!\n");
			_videopip_set_disable(VIDEO_DISABLE_FORNEXT);
		} else if (get_videopip_enabled()) {
			safe_disble_videolayer2();
		}
	}
	mutex_lock(&video_keeper_mutex);
	video_pip_keeper_new_frame_notify();
	free_alloced_keep_buffer();
	mutex_unlock(&video_keeper_mutex);
}
EXPORT_SYMBOL(try_free_keep_videopip);

static void video_keeper_update_keeper_mem(
	void *mem_handle, int type,
	int *id)
{
	int ret;
	int old_id = *id;

	if (!mem_handle)
		return;
	ret = codec_mm_keeper_mask_keep_mem(mem_handle,
		type);
	if (ret > 0) {
		if (old_id > 0 && ret != old_id) {
			/*wait 80 ms for vsync post.*/
			codec_mm_keeper_unmask_keeper(old_id, 120);
		}
		*id = ret;
	}
}

static int video_keeper_frame_keep_locked(
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf_el)
{
	int type = MEM_TYPE_CODEC_MM;
	if (cur_dispbuf->type & VIDTYPE_SCATTER)
		type = MEM_TYPE_CODEC_MM_SCATTER;
	video_keeper_update_keeper_mem(
		cur_dispbuf->mem_handle,
		type,
		&keep_id);
	video_keeper_update_keeper_mem(
		cur_dispbuf->mem_head_handle,
		MEM_TYPE_CODEC_MM,
		&keep_head_id);
	if (cur_dispbuf_el) {
		if (cur_dispbuf->type & VIDTYPE_SCATTER)
			type = MEM_TYPE_CODEC_MM_SCATTER;
		else
			type = MEM_TYPE_CODEC_MM;
		video_keeper_update_keeper_mem(
			cur_dispbuf_el->mem_handle,
			type,
			&keep_el_id);
		video_keeper_update_keeper_mem(
			cur_dispbuf_el->mem_head_handle,
			MEM_TYPE_CODEC_MM,
			&keep_el_head_id);
	}
	return (keep_id + keep_head_id) > 0;
}

static int video_pip_keeper_frame_keep_locked(
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf_el)
{
	int type = MEM_TYPE_CODEC_MM;

	if (cur_dispbuf) {
		if (cur_dispbuf->type & VIDTYPE_SCATTER)
			type = MEM_TYPE_CODEC_MM_SCATTER;
		video_keeper_update_keeper_mem(
			cur_dispbuf->mem_handle,
			type,
			&keep_pip_id);
		video_keeper_update_keeper_mem(
			cur_dispbuf->mem_head_handle,
			MEM_TYPE_CODEC_MM,
			&keep_pip_head_id);
	}
	if (cur_dispbuf_el) {
		if (cur_dispbuf_el->type & VIDTYPE_SCATTER)
			type = MEM_TYPE_CODEC_MM_SCATTER;
		else
			type = MEM_TYPE_CODEC_MM;
		video_keeper_update_keeper_mem(
			cur_dispbuf_el->mem_handle,
			type,
			&keep_pip_el_id);
		video_keeper_update_keeper_mem(
			cur_dispbuf_el->mem_head_handle,
			MEM_TYPE_CODEC_MM,
			&keep_pip_el_head_id);
	}
	return (keep_pip_id + keep_pip_head_id) > 0;
}

/*
 * call in irq.
 *don't used mutex
 */
void video_keeper_new_frame_notify(void)
{
	if (keep_video_on) {
		pr_info("new frame show, free keeper\n");
		keep_video_on = 0;
	}
	if (keep_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_id, 120);
		keep_id = -1;
	}
	if (keep_head_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_head_id, 120);
		keep_head_id = -1;
	}

	if (keep_el_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_el_id, 120);
		keep_el_id = -1;
	}
	if (keep_el_head_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_el_head_id, 120);
		keep_el_head_id = -1;
	}
	return;
}

void video_pip_keeper_new_frame_notify(void)
{
	if (keep_video_pip_on) {
		pr_info("new frame show, pip free keeper\n");
		keep_video_pip_on = 0;
	}
	if (keep_pip_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_pip_id, 120);
		keep_pip_id = -1;
	}
	if (keep_pip_head_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_pip_head_id, 120);
		keep_pip_head_id = -1;
	}
	if (keep_pip_el_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_pip_el_id, 120);
		keep_pip_el_id = -1;
	}
	if (keep_pip_el_head_id > 0) {
		/*wait 80 ms for vsync post.*/
		codec_mm_keeper_unmask_keeper(keep_pip_el_head_id, 120);
		keep_pip_el_head_id = -1;
	}

}


static unsigned int vf_keep_current_locked(
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf_el)
{
	u32 cur_index;
	u32 y_index, u_index, v_index;
	struct canvas_s cs0, cs1, cs2, cd;
	int ret;

	if (!cur_dispbuf) {
		pr_info("keep exit without cur_dispbuf\n");
		return 0;
	}

	if (get_video_debug_flags() &
		DEBUG_FLAG_TOGGLE_SKIP_KEEP_CURRENT) {
		pr_info("keep exit is skip current\n");
		return 0;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
	ext_frame_capture_poll(1); /*pull  if have capture end frame */
#endif

	if (get_blackout_policy()) {
		pr_info("keep exit is skip current\n");
		return 0;
	}

	if (VSYNC_RD_MPEG_REG(DI_IF1_GEN_REG) & 0x1) {
		pr_info("keep exit is di\n");
		return 0;
	}

	ret = video_keeper_frame_keep_locked(
		cur_dispbuf,
		cur_dispbuf_el);
	if (ret) {
		/*keeped ok with codec keeper!*/
		keep_video_on = 1;
		return 1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (codec_mm_video_tvp_enabled()) {
		pr_info("keep exit is TVP\n");
		return 0;
	}
#endif

	if (cur_dispbuf->type & VIDTYPE_COMPRESS) {
		/* todo: duplicate compressed video frame */
		pr_info("keep exit is skip VIDTYPE_COMPRESS\n");
		return -1;
	}
	cur_index = READ_VCBUS_REG(VD1_IF0_CANVAS0 +
		get_video_cur_dev()->viu_off);
	y_index = cur_index & 0xff;
	u_index = (cur_index >> 8) & 0xff;
	v_index = (cur_index >> 16) & 0xff;
	canvas_read(y_index, &cd);

	if ((cd.width * cd.height) <= 2048 * 1088
		&& !keep_y_addr) {
		alloc_keep_buffer();
	}
	if (!keep_y_addr
		|| (cur_dispbuf->type & VIDTYPE_VIU_422)
		== VIDTYPE_VIU_422) {
		/* no support VIDTYPE_VIU_422... */
		pr_info("%s:no support VIDTYPE_VIU_422\n", __func__);
		return -1;
	}

	if (get_video_debug_flags() & DEBUG_FLAG_BLACKOUT) {
		pr_info("%s keep_y_addr=%p %x\n",
			__func__, (void *)keep_y_addr,
			canvas_get_addr(y_index));
	}

	if ((cur_dispbuf->type & VIDTYPE_VIU_422) == VIDTYPE_VIU_422) {
		return -1;
		/* no VIDTYPE_VIU_422 type frame need keep,avoid memcpy crash*/
/*
		if ((Y_BUFFER_SIZE < (cd.width * cd.height))) {
			pr_info("[%s::%d]data > buf size: %x,%x,%x, %x,%x\n",
				__func__, __LINE__, Y_BUFFER_SIZE,
				U_BUFFER_SIZE, V_BUFFER_SIZE,
				cd.width, cd.height);
			return -1;
		}
		if (keep_phy_addr(keep_y_addr) != canvas_get_addr(y_index) &&
			canvas_dup(keep_phy_addr(keep_y_addr),
			canvas_get_addr(y_index),
			(cd.width) * (cd.height))) {
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
			canvas_update_addr(disp_canvas_index[0][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[1][0],
				keep_phy_addr(keep_y_addr));
#else
			canvas_update_addr(y_index,
				keep_phy_addr(keep_y_addr));
#endif
			if (get_video_debug_flags() & DEBUG_FLAG_BLACKOUT)
				pr_info("%s: VIDTYPE_VIU_422\n", __func__);
		}
*/
	} else if ((cur_dispbuf->type & VIDTYPE_VIU_444) == VIDTYPE_VIU_444) {
		if ((Y_BUFFER_SIZE < (cd.width * cd.height))) {
			pr_info
			    ("[%s::%d] error:data>buf size: %x,%x,%x, %x,%x\n",
			     __func__, __LINE__, Y_BUFFER_SIZE,
			     U_BUFFER_SIZE, V_BUFFER_SIZE,
				cd.width, cd.height);
			return -1;
		}
#ifdef CONFIG_GE2D_KEEP_FRAME
		ge2d_keeplastframe_block(cur_index, GE2D_FORMAT_M24_YUV444);
#else
		if (keep_phy_addr(keep_y_addr) != canvas_get_addr(y_index) &&
			canvas_dup(keep_phy_addr(keep_y_addr),
			canvas_get_addr(y_index),
			(cd.width) * (cd.height))) {
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
			canvas_update_addr(disp_canvas_index[0][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[1][0],
				keep_phy_addr(keep_y_addr));
#else
			canvas_update_addr(y_index,
				keep_phy_addr(keep_y_addr));
#endif
		}
#endif
		if (get_video_debug_flags() & DEBUG_FLAG_BLACKOUT)
			pr_info("%s: VIDTYPE_VIU_444\n", __func__);
	} else if ((cur_dispbuf->type & VIDTYPE_VIU_NV21) == VIDTYPE_VIU_NV21) {
		canvas_read(y_index, &cs0);
		canvas_read(u_index, &cs1);
		if ((Y_BUFFER_SIZE < (cs0.width * cs0.height))
			|| (U_BUFFER_SIZE < (cs1.width * cs1.height))) {
			pr_info("## [%s::%d] error: yuv data size larger",
				__func__, __LINE__);
			return -1;
		}
#ifdef CONFIG_GE2D_KEEP_FRAME
		ge2d_keeplastframe_block(cur_index, GE2D_FORMAT_M24_NV21);
#else
		if (keep_phy_addr(keep_y_addr) != canvas_get_addr(y_index) &&
			canvas_dup(keep_phy_addr(keep_y_addr),
			canvas_get_addr(y_index),
			(cs0.width * cs0.height))
			&& canvas_dup(keep_phy_addr(keep_u_addr),
			canvas_get_addr(u_index),
			(cs1.width * cs1.height))) {
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
			canvas_update_addr(disp_canvas_index[0][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[1][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[0][1],
				keep_phy_addr(keep_u_addr));
			canvas_update_addr(disp_canvas_index[1][1],
				keep_phy_addr(keep_u_addr));
#else
			canvas_update_addr(y_index,
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(u_index,
				keep_phy_addr(keep_u_addr));
#endif
		}
#endif
		if (get_video_debug_flags() & DEBUG_FLAG_BLACKOUT)
			pr_info("%s: VIDTYPE_VIU_NV21\n", __func__);
	} else {
		canvas_read(y_index, &cs0);
		canvas_read(u_index, &cs1);
		canvas_read(v_index, &cs2);

		if ((Y_BUFFER_SIZE < (cs0.width * cs0.height))
			|| (U_BUFFER_SIZE < (cs1.width * cs1.height))
			|| (V_BUFFER_SIZE < (cs2.width * cs2.height))) {
			pr_info("## [%s::%d] error: yuv data size larger than buf size: %x,%x,%x, %x,%x, %x,%x, %x,%x,\n",
				__func__, __LINE__, Y_BUFFER_SIZE,
				U_BUFFER_SIZE, V_BUFFER_SIZE, cs0.width,
				cs0.height, cs1.width, cs1.height, cs2.width,
				cs2.height);
			return -1;
		}
#ifdef CONFIG_GE2D_KEEP_FRAME
		ge2d_keeplastframe_block(cur_index, GE2D_FORMAT_M24_YUV420);
#else
		if (keep_phy_addr(keep_y_addr) != canvas_get_addr(y_index) &&
			/*must not the same address */
			canvas_dup(keep_phy_addr(keep_y_addr),
			canvas_get_addr(y_index),
			(cs0.width * cs0.height))
			&& canvas_dup(keep_phy_addr(keep_u_addr),
			canvas_get_addr(u_index),
			(cs1.width * cs1.height))
			&& canvas_dup(keep_phy_addr(keep_v_addr),
			canvas_get_addr(v_index),
			(cs2.width * cs2.height))) {
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
			canvas_update_addr(disp_canvas_index[0][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[1][0],
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(disp_canvas_index[0][1],
				keep_phy_addr(keep_u_addr));
			canvas_update_addr(disp_canvas_index[1][1],
				keep_phy_addr(keep_u_addr));
			canvas_update_addr(disp_canvas_index[0][2],
				keep_phy_addr(keep_v_addr));
			canvas_update_addr(disp_canvas_index[1][2],
				keep_phy_addr(keep_v_addr));
#else
			canvas_update_addr(y_index,
				keep_phy_addr(keep_y_addr));
			canvas_update_addr(u_index,
				keep_phy_addr(keep_u_addr));
			canvas_update_addr(v_index,
				keep_phy_addr(keep_v_addr));
#endif
		}

		if (get_video_debug_flags() & DEBUG_FLAG_BLACKOUT)
			pr_info("%s: VIDTYPE_VIU_420\n", __func__);
#endif
	}
	keep_video_on = 1;
	pr_info("%s: keep video on with keep\n", __func__);
	return 1;

}

unsigned int vf_keep_pip_current_locked(
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf_el)
{
	//u32 cur_index;
	//u32 y_index, u_index, v_index;
	//struct canvas_s cs0, cs1, cs2, cd;
	int ret;

	if (!cur_dispbuf) {
		pr_info("keep pip exit without cur_dispbuf\n");
		return 0;
	}

	if (get_video_debug_flags() &
		DEBUG_FLAG_TOGGLE_SKIP_KEEP_CURRENT) {
		pr_info("flag: keep pip exit is skip current\n");
		return 0;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
	ext_frame_capture_poll(1); /*pull  if have capture end frame */
#endif

	if (get_blackout_pip_policy()) {
		pr_info("policy: keep exit is skip current\n");
		return 0;
	}

	ret = video_pip_keeper_frame_keep_locked(
		cur_dispbuf,
		cur_dispbuf_el);

	if (ret) {
		/*keeped ok with codec keeper!*/
		pr_info("keep pip buffer on!\n");
		keep_video_pip_on = 1;
		return 1;
	}

	keep_video_pip_on = 0;
	return 0;
}

unsigned int vf_keep_current(
	struct vframe_s *cur_dispbuf,
	struct vframe_s *cur_dispbuf2)
{
	unsigned int ret;

	mutex_lock(&video_keeper_mutex);
	ret = vf_keep_current_locked(
		cur_dispbuf, cur_dispbuf2);
	mutex_unlock(&video_keeper_mutex);
	return ret;
}

int __init video_keeper_init(void)
{
#ifdef CONFIG_GE2D_KEEP_FRAME
	/* video_frame_getmem(); */
	ge2d_videotask_init();
#endif
	return 0;
}
void __exit video_keeper_exit(void)
{
#ifdef CONFIG_GE2D_KEEP_FRAME
	ge2d_videotask_release();
#endif
}

