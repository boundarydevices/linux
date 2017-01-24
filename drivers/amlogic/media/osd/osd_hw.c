/*
 * drivers/amlogic/media/osd/osd_hw.c
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
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/irqreturn.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/kthread.h>
/* Android Headers */

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#ifdef CONFIG_AMLOGIC_VECM
#include <linux/amlogic/amvecm/ve.h>
#endif

#ifdef CONFIG_AMLOGIC_VIDEO
#include <linux/amlogic/amports/video.h>
#endif
/* Local Headers */
#include "osd_canvas.h"
#include "osd_prot.h"
#include "osd_antiflicker.h"
#include "osd_clone.h"
#include "osd_log.h"
#include "osd_reg.h"
#include "osd_io.h"
#include "osd_backup.h"

#include "osd_hw.h"
#include "osd_hw_def.h"

#ifdef CONFIG_AMLOGIC_VSYNC_FIQ_ENABLE
#define FIQ_VSYNC
#endif
#define VOUT_ENCI	1
#define VOUT_ENCP	2
#define VOUT_ENCT	3
#define OSD_TYPE_TOP_FIELD 0
#define OSD_TYPE_BOT_FIELD 1

#define WAIT_AFBC_READY_COUNT 100

struct hw_para_s osd_hw;
static DEFINE_MUTEX(osd_mutex);
static DECLARE_WAIT_QUEUE_HEAD(osd_vsync_wq);

static bool vsync_hit;
static bool osd_update_window_axis;
static int osd_afbc_dec_enable;
static void osd_clone_pan(u32 index, u32 yoffset, int debug_flag);

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
/* sync fence relative varible. */
static int timeline_created;
static struct sw_sync_timeline *timeline;
static u32 cur_streamline_val;
/* thread control part */
struct kthread_worker buffer_toggle_worker;
struct task_struct *buffer_toggle_thread;
struct kthread_work buffer_toggle_work;
struct list_head post_fence_list;
struct mutex post_fence_list_lock;
static void osd_pan_display_fence(struct osd_fence_map_s *fence_map);
#endif

static int pxp_mode;

static __nosavedata int use_h_filter_mode = -1;
static __nosavedata int use_v_filter_mode = -1;

static unsigned int osd_h_filter_mode = 1;
module_param(osd_h_filter_mode, uint, 0664);
MODULE_PARM_DESC(osd_h_filter_mode, "osd_h_filter_mode");

static unsigned int osd_v_filter_mode = 1;
module_param(osd_v_filter_mode, uint, 0664);
MODULE_PARM_DESC(osd_v_filter_mode, "osd_v_filter_mode");

static unsigned int osd_auto_adjust_filter = 1;
module_param(osd_auto_adjust_filter, uint, 0664);
MODULE_PARM_DESC(osd_auto_adjust_filter, "osd_auto_adjust_filter");

static int osd_init_hw_flag;
static int osd_logo_index = 1;
module_param(osd_logo_index, int, 0664);
MODULE_PARM_DESC(osd_logo_index, "osd_logo_index");

module_param(osd_afbc_dec_enable, int, 0664);
MODULE_PARM_DESC(osd_afbc_dec_enable, "osd_afbc_dec_enable");

static u32 osd_vpp_misc;
static u32 osd_vpp_misc_mask = OSD_RELATIVE_BITS;
module_param(osd_vpp_misc, uint, 0444);
MODULE_PARM_DESC(osd_vpp_misc, "osd_vpp_misc");

static unsigned int osd_filter_coefs_bicubic_sharp[] = {
	0x01fa008c, 0x01fa0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

static unsigned int osd_filter_coefs_bicubic[] = { /* bicubic	coef0 */
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300, 0xfd7e0500, 0xfc7e0600,
	0xfb7d0800, 0xfb7c0900, 0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe, 0xf76f1dfd, 0xf76d1ffd,
	0xf76b21fd, 0xf76824fd, 0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa, 0xf8523cfa, 0xf8503ff9,
	0xf84d42f9, 0xf84a45f9, 0xf84848f8
};

static unsigned int osd_filter_coefs_bilinear[] = { /* 2 point bilinear	coef1 */
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600, 0x00780800, 0x00760a00,
	0x00740c00, 0x00720e00, 0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00, 0x00602000, 0x005e2200,
	0x005c2400, 0x005a2600, 0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600, 0x00483800, 0x00463a00,
	0x00443c00, 0x00423e00, 0x00404000
};

static unsigned int osd_filter_coefs_2point_binilear[] = {
	/* 2 point bilinear, bank_length == 2	coef2 */
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000, 0x78080000, 0x760a0000,
	0x740c0000, 0x720e0000, 0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000, 0x60200000, 0x5e220000,
	0x5c240000, 0x5a260000, 0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000, 0x48380000, 0x463a0000,
	0x443c0000, 0x423e0000, 0x40400000
};

/* filt_triangle, point_num =3, filt_len =2.6, group_num = 64 */
static unsigned int osd_filter_coefs_3point_triangle_sharp[] = {
	0x40400000, 0x3e420000, 0x3d430000, 0x3b450000,
	0x3a460000, 0x38480000, 0x37490000, 0x354b0000,
	0x344c0000, 0x324e0000, 0x314f0000, 0x2f510000,
	0x2e520000, 0x2c540000, 0x2b550000, 0x29570000,
	0x28580000, 0x265a0000, 0x245c0000, 0x235d0000,
	0x215f0000, 0x20600000, 0x1e620000, 0x1d620100,
	0x1b620300, 0x19630400, 0x17630600, 0x15640700,
	0x14640800, 0x12640a00, 0x11640b00, 0x0f650c00,
	0x0d660d00
};

static unsigned int osd_filter_coefs_3point_triangle[] = {
	0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
	0x3a420400, 0x39420500, 0x37430600, 0x36430700,
	0x35430800, 0x33450800, 0x32450900, 0x31450a00,
	0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
	0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
	0x26491100, 0x25491200, 0x24491300, 0x234a1300,
	0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
	0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
	0x1a4c1a00
};

static unsigned int osd_filter_coefs_4point_triangle[] = {
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};

/* 4th order (cubic) b-spline */
/* filt_cubic point_num =4, filt_len =4, group_num = 64 */
static unsigned int vpp_filter_coefs_4point_bspline[] = {
	0x15561500, 0x14561600, 0x13561700, 0x12561800,
	0x11551a00, 0x11541b00, 0x10541c00, 0x0f541d00,
	0x0f531e00, 0x0e531f00, 0x0d522100, 0x0c522200,
	0x0b522300, 0x0b512400, 0x0a502600, 0x0a4f2700,
	0x094e2900, 0x084e2a00, 0x084d2b00, 0x074c2c01,
	0x074b2d01, 0x064a2f01, 0x06493001, 0x05483201,
	0x05473301, 0x05463401, 0x04453601, 0x04433702,
	0x04423802, 0x03413a02, 0x03403b02, 0x033f3c02,
	0x033d3d03
};

/* filt_quadratic, point_num =3, filt_len =3, group_num = 64 */
static unsigned int osd_filter_coefs_3point_bspline[] = {
	0x40400000, 0x3e420000, 0x3c440000, 0x3a460000,
	0x38480000, 0x364a0000, 0x344b0100, 0x334c0100,
	0x314e0100, 0x304f0100, 0x2e500200, 0x2c520200,
	0x2a540200, 0x29540300, 0x27560300, 0x26570300,
	0x24580400, 0x23590400, 0x215a0500, 0x205b0500,
	0x1e5c0600, 0x1d5c0700, 0x1c5d0700, 0x1a5e0800,
	0x195e0900, 0x185e0a00, 0x175f0a00, 0x15600b00,
	0x14600c00, 0x13600d00, 0x12600e00, 0x11600f00,
	0x10601000
};

static unsigned int *filter_table[] = {
	osd_filter_coefs_bicubic_sharp,
	osd_filter_coefs_bicubic,
	osd_filter_coefs_bilinear,
	osd_filter_coefs_2point_binilear,
	osd_filter_coefs_3point_triangle_sharp,
	osd_filter_coefs_3point_triangle,
	osd_filter_coefs_4point_triangle,
	vpp_filter_coefs_4point_bspline,
	osd_filter_coefs_3point_bspline
};

#ifdef CONFIG_AMLOGIC_VECM
static bool osd_hdr_on;
#endif

static void osd_vpu_power_on(void)
{
#ifdef CONFIG_AMLOGIC_VPU
	switch_vpu_mem_pd_vmod(VPU_VIU_OSD1, VPU_MEM_POWER_ON);
	switch_vpu_mem_pd_vmod(VPU_VIU_OSD2, VPU_MEM_POWER_ON);
	switch_vpu_mem_pd_vmod(VPU_VIU_OSD_SCALE, VPU_MEM_POWER_ON);
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)) {
		switch_vpu_mem_pd_vmod(VPU_AFBC_DEC,
			VPU_MEM_POWER_ON);
	}
#endif
}

#ifdef CONFIG_AMLOGIC_SUPERSCALER
static void osd_super_scale_mem_power_on(void)
{
#ifdef CONFIG_AMLOGIC_VPU
	switch_vpu_mem_pd_vmod(VPU_VIU_OSDSR, VPU_MEM_POWER_ON);
#endif
}

static void osd_super_scale_mem_power_off(void)
{
#ifdef CONFIG_AMLOGIC_VPU
	switch_vpu_mem_pd_vmod(VPU_VIU_OSDSR, VPU_MEM_POWER_DOWN);
#endif
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
static inline  int  find_buf_num(u32 yres, u32 yoffset)
{
	int n = yres;
	int i;

	for (i = 0; i < OSD_MAX_BUF_NUM; i++) {
		/* find current addr position. */
		if (yoffset  < (n))
			break;
		n += yres;
	}
	return i;
}

/* next we will process two osd layer in toggle buffer. */
static void osd_toggle_buffer(struct kthread_work *work)
{
	struct osd_fence_map_s *data, *next;
	struct list_head saved_list;

	mutex_lock(&post_fence_list_lock);
	saved_list = post_fence_list;
	list_replace_init(&post_fence_list, &saved_list);
	mutex_unlock(&post_fence_list_lock);
	list_for_each_entry_safe(data, next, &saved_list, list) {
		osd_pan_display_fence(data);
		if (data->in_fence)
			sync_fence_put(data->in_fence);
		if (data->in_fd > 0)
			__close_fd(data->files, data->in_fd);
		list_del(&data->list);
		kfree(data);
	}
}

static int out_fence_create(int *release_fence_fd, u32 *val, u32 buf_num)
{
	/* the first time create out_fence_fd==0 */
	/* sw_sync_timeline_inc  will release fence and it's sync point */
	struct sync_pt *outer_sync_pt;
	struct sync_fence *outer_fence;
	int out_fence_fd = -1;

	out_fence_fd = get_unused_fd_flags(O_CLOEXEC);
	/* no file descriptor could be used. Error. */
	if (out_fence_fd < 0)
		return -1;
	if (!timeline_created) { /* timeline has not been created */
		timeline = sw_sync_timeline_create("osd_timeline");
		cur_streamline_val = 1;
		if (timeline == NULL)
			return -1;
		init_kthread_worker(&buffer_toggle_worker);
		buffer_toggle_thread = kthread_run(kthread_worker_fn,
				&buffer_toggle_worker, "aml_buf_toggle");
		init_kthread_work(&buffer_toggle_work, osd_toggle_buffer);
		timeline_created = 1;
	}
	/* install fence map; first ,the simplest. */
	cur_streamline_val++;
	*val = cur_streamline_val;
	outer_sync_pt = sw_sync_pt_create(timeline, *val);
	if (outer_sync_pt == NULL)
		goto error_ret;
	/* fence object will be released when no point */
	outer_fence = sync_fence_create("osd_fence_out", outer_sync_pt);
	if (outer_fence == NULL) {
		sync_pt_free(outer_sync_pt); /* free sync point. */
		goto error_ret;
	}
	sync_fence_install(outer_fence, out_fence_fd);
	*release_fence_fd = out_fence_fd;
	return out_fence_fd;

error_ret:
	cur_streamline_val--; /* pt or fence fail,restore timeline value. */
	osd_log_err("fence obj create fail\n");
	put_unused_fd(out_fence_fd);
	return -1;
}

int osd_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
		     s32 in_fence_fd)
{
	int out_fence_fd = -1;
	int buf_num = 0;
	struct osd_fence_map_s *fence_map =
		kzalloc(sizeof(struct osd_fence_map_s), GFP_KERNEL);

	buf_num = find_buf_num(yres, yoffset);
	if (!fence_map) {
		osd_log_err("could not allocate osd_fence_map\n");
		return -ENOMEM;
	}
	mutex_lock(&post_fence_list_lock);
	fence_map->fb_index = index;
	fence_map->buf_num = buf_num;
	fence_map->yoffset = yoffset;
	fence_map->xoffset = xoffset;
	fence_map->yres = yres;
	fence_map->in_fd = in_fence_fd;
	fence_map->in_fence = sync_fence_fdget(in_fence_fd);
	fence_map->files = current->files;
	fence_map->out_fd =
		out_fence_create(&out_fence_fd, &fence_map->val, buf_num);
	list_add_tail(&fence_map->list, &post_fence_list);
	mutex_unlock(&post_fence_list_lock);
	queue_kthread_work(&buffer_toggle_worker, &buffer_toggle_work);

	return  out_fence_fd;
}

static int osd_wait_buf_ready(struct osd_fence_map_s *fence_map)
{
	s32 ret = -1;
	struct sync_fence *buf_ready_fence = NULL;

	if (fence_map->in_fd <= 0) {
		ret = -1;
		return ret;
	}
	buf_ready_fence = fence_map->in_fence;
	if (buf_ready_fence == NULL) {
		ret = -1;/* no fence ,output directly. */
		return ret;
	}
	ret = sync_fence_wait(buf_ready_fence, -1);
	if (ret < 0) {
		osd_log_err("Sync Fence wait error:%d\n", ret);
		osd_log_err("-----wait buf idx:[%d] ERROR\n"
			"-----on screen buf idx:[%d]\n",
			fence_map->buf_num, find_buf_num(fence_map->yres,
			osd_hw.pandata[fence_map->fb_index].y_start));
	} else
		ret = 1;
	return ret;
}

#else
int osd_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
		     s32 in_fence_fd)
{
	osd_log_err("osd_sync_request not supported\n");
	return -5566;
}
#endif

void osd_update_3d_mode(void)
{
	/* only called by vsync irq or rdma irq */
	if (osd_hw.mode_3d[OSD1].enable)
		osd1_update_disp_3d_mode();
	if (osd_hw.mode_3d[OSD2].enable)
		osd2_update_disp_3d_mode();
}

static inline void wait_vsync_wakeup(void)
{
	vsync_hit = true;
	wake_up_interruptible(&osd_vsync_wq);
}

void osd_update_vsync_hit(void)
{
	if (!vsync_hit) {
#ifdef FIQ_VSYNC
		fiq_bridge_pulse_trigger(&osd_hw.fiq_handle_item);
#else
		wait_vsync_wakeup();
#endif
	}
}

static void osd_update_interlace_mode(void)
{
	/* only called by vsync irq or rdma irq */
	unsigned int fb0_cfg_w0, fb1_cfg_w0;
	unsigned int scan_line_number = 0;
	unsigned int odd_even;

	spin_lock_irqsave(&osd_lock, lock_flags);
	fb0_cfg_w0 = VSYNCOSD_RD_MPEG_REG(VIU_OSD1_BLK0_CFG_W0);
	fb1_cfg_w0 = VSYNCOSD_RD_MPEG_REG(VIU_OSD2_BLK0_CFG_W0);
	if (osd_reg_read(ENCP_VIDEO_MODE) & (1 << 12)) {
		/* 1080I */
		scan_line_number = ((osd_reg_read(ENCP_INFO_READ))
				& 0x1fff0000) >> 16;
		if ((osd_hw.pandata[OSD1].y_start % 2) == 0) {
			if (scan_line_number >= 562) {
				/* bottom field, odd lines*/
				odd_even = OSD_TYPE_BOT_FIELD;
			} else {
				/* top field, even lines*/
				odd_even = OSD_TYPE_TOP_FIELD;
			}
		} else {
			if (scan_line_number >= 562) {
				/* top field, even lines*/
				odd_even = OSD_TYPE_TOP_FIELD;
			} else {
				/* bottom field, odd lines*/
				odd_even = OSD_TYPE_BOT_FIELD;
			}
		}
	} else {
		if ((osd_hw.pandata[OSD1].y_start % 2) == 1) {
			odd_even = (osd_reg_read(ENCI_INFO_READ) & (1 << 29)) ?
				OSD_TYPE_TOP_FIELD : OSD_TYPE_BOT_FIELD;
		} else {
			odd_even = (osd_reg_read(ENCI_INFO_READ) & (1 << 29)) ?
				OSD_TYPE_BOT_FIELD : OSD_TYPE_TOP_FIELD;
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	/* when RDMA enabled, top/bottom fields changed in next vsync */
	odd_even = (odd_even == OSD_TYPE_TOP_FIELD) ?
		OSD_TYPE_BOT_FIELD : OSD_TYPE_TOP_FIELD;
#endif
	fb0_cfg_w0 &= ~1;
	fb1_cfg_w0 &= ~1;
	fb0_cfg_w0 |= odd_even;
	fb1_cfg_w0 |= odd_even;
	VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W0, fb0_cfg_w0);
	VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W0, fb1_cfg_w0);
	spin_unlock_irqrestore(&osd_lock, lock_flags);
}

void osd_update_scan_mode(void)
{
	/* only called by vsync irq or rdma irq */
	unsigned int output_type = 0;

	output_type = osd_reg_read(VPU_VIU_VENC_MUX_CTRL) & 0x3;
	osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	switch (output_type) {
	case VOUT_ENCP:
		if (osd_reg_read(ENCP_VIDEO_MODE) & (1 << 12)) /* 1080i */
			osd_hw.scan_mode = SCAN_MODE_INTERLACE;
		break;
	case VOUT_ENCI:
		if (osd_reg_read(ENCI_VIDEO_EN) & 1)
			osd_hw.scan_mode = SCAN_MODE_INTERLACE;
		break;
	}
	if (osd_hw.free_scale_enable[OSD1])
		osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	if (osd_hw.osd_afbcd[OSD1].enable)
		osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	if (osd_hw.scan_mode == SCAN_MODE_INTERLACE)
		osd_update_interlace_mode();
}

static inline void walk_through_update_list(void)
{
	u32  i, j;

	for (i = 0; i < HW_OSD_COUNT; i++) {
		j = 0;
		while (osd_hw.updated[i] && j < 32) {
			if (osd_hw.updated[i] & (1 << j)) {
				osd_hw.reg[i][j].update_func();
				remove_from_update_list(i, j);
			}
			j++;
		}
	}
}

/*************** for GXL/GXM hardware alpha bug workaround ***************/
u32 osd_get_hw_reset_flag(void)
{
	u32 hw_reset_flag = HW_RESET_NONE;
	u32 cpu_type;

	cpu_type = get_cpu_type();
	/* check hw version */
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_GXTVBB:
		if (osd_hw.osd_afbcd[OSD1].enable)
			hw_reset_flag |= HW_RESET_AFBCD_REGS;
		break;
	case MESON_CPU_MAJOR_ID_GXM:
		/* same bit, but gxm only reset hardware, not top reg*/
		if (osd_hw.osd_afbcd[OSD1].enable)
			hw_reset_flag |= HW_RESET_AFBCD_HARDWARE;
#ifndef CONFIG_AMLOGIC_VECM
		break;
#else
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_TXL:
		if (((hdr_osd_reg.viu_osd1_matrix_ctrl & 0x00000001)
			!= 0x0) ||
			((hdr_osd_reg.viu_osd1_eotf_ctl & 0x80000000)
			!= 0) ||
			((hdr_osd_reg.viu_osd1_oetf_ctl & 0xe0000000)
			!= 0)) {
			hw_reset_flag |= HW_RESET_OSD1_REGS;
			osd_hdr_on = true;
		} else if (osd_hdr_on) {
			hw_reset_flag |= HW_RESET_OSD1_REGS;
			osd_hdr_on = false;
		}
		break;
#endif
	default:
		hw_reset_flag = HW_RESET_NONE;
		break;
	}
	return hw_reset_flag;
}

void osd_hw_reset(void)
{
	/* only called by vsync irq or rdma irq */
	u32 backup_mask;
	u32 reset_bit =
		osd_get_hw_reset_flag();

	backup_mask = is_backup();
	osd_hw.hw_reset_flag = reset_bit;
	if (reset_bit == HW_RESET_NONE)
		return;
	spin_lock_irqsave(&osd_lock, lock_flags);
	if ((reset_bit & HW_RESET_OSD1_REGS)
		&& !(backup_mask & HW_RESET_OSD1_REGS))
		reset_bit &= ~HW_RESET_OSD1_REGS;

	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)
		&& (reset_bit & HW_RESET_AFBCD_REGS)
		&& !(backup_mask & HW_RESET_AFBCD_REGS))
		reset_bit &= ~HW_RESET_AFBCD_REGS;

#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	/* if not osd rdma, don't reset osd1 for hdr */
	reset_bit &= ~HW_RESET_OSD1_REGS;
	backup_mask &= ~HW_RESET_OSD1_REGS;

	VSYNCOSD_IRQ_WR_MPEG_REG(
		VIU_SW_RESET, reset_bit);
	VSYNCOSD_IRQ_WR_MPEG_REG(
		VIU_SW_RESET, 0);

	if (reset_bit & HW_RESET_OSD1_REGS) {
		/* restore osd regs */
		int i;
		u32 addr;
		u32 base = VIU_OSD1_CTRL_STAT;

		for (i = 0; i < OSD_REG_BACKUP_COUNT; i++) {
			addr = osd_reg_backup[i];
			VSYNCOSD_IRQ_WR_MPEG_REG(
				addr, osd_backup[addr - base]);
		}
	}

	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB)
		&& (reset_bit & HW_RESET_AFBCD_REGS)) {
		/* restore osd afbcd regs */
		int i;
		u32 addr;
		u32 value;
		u32 base = OSD1_AFBCD_ENABLE;

		for (i = 0; i < OSD_AFBC_REG_BACKUP_COUNT; i++) {
			addr = osd_afbc_reg_backup[i];
			value = osd_afbc_backup[addr - base];
			if (addr == OSD1_AFBCD_ENABLE)
				value |= 0x100;
			VSYNCOSD_IRQ_WR_MPEG_REG(
				addr, value);
		}
	}
#else
	osd_rdma_reset_and_flush(reset_bit);
#endif
	spin_unlock_irqrestore(&osd_lock, lock_flags);
	/* maybe change reset bit */
	osd_hw.hw_reset_flag = reset_bit;
}

static int notify_to_amvideo(void)
{
	u32 para[2];

	para[0] = osd_vpp_misc;
	para[1] = osd_vpp_misc_mask;
	pr_debug(
		"osd notify_to_amvideo vpp misc:0x%08x, mask:0x%08x\n",
		para[0], para[1]);
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
#ifdef CONFIG_AMLOGIC_VIDEO
	amvideo_notifier_call_chain(
		AMVIDEO_UPDATE_OSD_MODE,
		(void *)&para[0]);
#endif
#endif
	return 0;
}
/*************** end of GXL/GXM hardware alpha bug workaround ***************/

#ifdef FIQ_VSYNC
static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	wait_vsync_wakeup();
	return IRQ_HANDLED;
}

static void osd_fiq_isr(void)
#else
static irqreturn_t vsync_isr(int irq, void *dev_id)
#endif
{
#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_update_scan_mode();
	/* go through update list */
	walk_through_update_list();
	osd_update_3d_mode();
	osd_update_vsync_hit();
	osd_hw_reset();
#else
	osd_rdma_interrupt_done_clear();
#endif
#ifndef FIQ_VSYNC
	return IRQ_HANDLED;
#endif
}


void osd_set_pxp_mode(u32 mode)
{
	pxp_mode = mode;
}
void osd_set_afbc(u32 enable)
{
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM))
		osd_hw.osd_afbcd[OSD1].enable = enable;
}

u32 osd_get_afbc(void)
{
	return osd_hw.osd_afbcd[OSD1].enable;
}

u32 osd_get_reset_status(void)
{
	return osd_hw.hw_reset_flag;
}

void osd_wait_vsync_hw(void)
{
	unsigned long timeout;

	vsync_hit = false;

	if (pxp_mode)
		timeout = msecs_to_jiffies(50);
	else
		timeout = HZ;
	wait_event_interruptible_timeout(osd_vsync_wq, vsync_hit, timeout);
}

s32 osd_wait_vsync_event(void)
{
	vsync_hit = false;
	/* waiting for 10ms. */
	wait_event_interruptible_timeout(osd_vsync_wq, vsync_hit, 1);

	return 0;
}

static int is_interlaced(struct vinfo_s *vinfo)
{
	if (vinfo->mode == VMODE_CVBS)
		return 1;
	if (vinfo->height != vinfo->field_height)
		return 1;
	else
		return 0;
}


int osd_set_scan_mode(u32 index)
{
	struct vinfo_s *vinfo;
	u32 data32 = 0x0;
	int real_scan_mode;
	s32 y_end = 0;

	osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	vinfo = get_current_vinfo();
	if (vinfo) {
		osd_hw.scale_workaround = 0;
		if (osd_auto_adjust_filter) {
			osd_h_filter_mode = 1;
			osd_v_filter_mode = 1;
		}
		if (is_interlaced(vinfo)) {
			osd_hw.scan_mode = real_scan_mode = SCAN_MODE_INTERLACE;
			y_end = osd_hw.free_src_data[index].y_end;
			if ((vinfo->width == 720)
				&& (vinfo->height == 480)) {
				if (osd_hw.free_scale_mode[index]) {
					osd_hw.field_out_en = 1;
					switch (y_end) {
					case 719:
						osd_hw.bot_type = 2;
						break;
					case 1079:
						osd_hw.bot_type = 3;
						break;
					default:
						osd_hw.bot_type = 2;
						break;
					}
				}
				if (osd_auto_adjust_filter) {
					osd_h_filter_mode = 6;
					osd_v_filter_mode = 6;
				}
			} else if ((vinfo->width == 720)
				&& (vinfo->height == 576)) {
				if (osd_hw.free_scale_mode[index]) {
					osd_hw.field_out_en = 1;
					switch (y_end) {
					case 719:
						osd_hw.bot_type = 2;
						break;
					case 1079:
						osd_hw.bot_type = 2;
						break;
					default:
						osd_hw.bot_type = 2;
						break;
					}
				}
				if (osd_auto_adjust_filter) {
					osd_h_filter_mode = 6;
					osd_v_filter_mode = 6;
				}

			} else if ((vinfo->width == 1920)
				&& (vinfo->height == 1080)) {
				if (osd_hw.free_scale_mode[index]) {
					osd_hw.field_out_en = 1;
					switch (y_end) {
					case 719:
						osd_hw.bot_type = 1;
						break;
					case 1079:
						osd_hw.bot_type = 2;
						break;
					default:
						osd_hw.bot_type = 1;
						break;
					}
				}
			}
		} else {
			if (((vinfo->width == 3840)
				&& (vinfo->height == 2160))
				|| ((vinfo->width == 4096)
				&& (vinfo->height == 2160))) {
				if ((osd_hw.fb_for_4k2k)
					&& (osd_hw.free_scale_enable[index]))
					if (!(is_meson_gxtvbb_cpu() ||
						is_meson_gxm_cpu()))
						osd_hw.scale_workaround = 1;
				osd_hw.field_out_en = 0;
			} else if (((vinfo->width == 720)
				&& (vinfo->height == 480))
				|| ((vinfo->width == 720)
				&& (vinfo->height == 576))) {
				if (osd_auto_adjust_filter) {
					osd_h_filter_mode = 6;
					osd_v_filter_mode = 6;
				}
				if (osd_hw.free_scale_mode[index])
					osd_hw.field_out_en = 0;
			} else {
				if (osd_hw.free_scale_mode[index])
					osd_hw.field_out_en = 0;
			}
		}
	}
	if (osd_hw.free_scale_enable[index])
		osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	if (osd_hw.osd_afbcd[OSD1].enable)
		osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	if (index == OSD2) {
		if (real_scan_mode == SCAN_MODE_INTERLACE)
			return 1;
		data32 = (VSYNCOSD_RD_MPEG_REG(VIU_OSD2_BLK0_CFG_W0) & 3) >> 1;
	} else
		data32 = (VSYNCOSD_RD_MPEG_REG(VIU_OSD1_BLK0_CFG_W0) & 3) >> 1;
	if (data32 == osd_hw.scan_mode)
		return 1;
	else
		return 0;
}

void  osd_set_gbl_alpha_hw(u32 index, u32 gbl_alpha)
{
	/* normalized */
	if (gbl_alpha == 0xff)
		gbl_alpha = 0x100;
	if (osd_hw.gbl_alpha[index] != gbl_alpha) {
		osd_hw.gbl_alpha[index] = gbl_alpha;
		add_to_update_list(index, OSD_GBL_ALPHA);
		osd_wait_vsync_hw();
	}
}

u32 osd_get_gbl_alpha_hw(u32  index)
{
	return osd_hw.gbl_alpha[index];
}

void osd_set_color_key_hw(u32 index, u32 color_index, u32 colorkey)
{
	u8 r = 0, g = 0, b = 0, a = (colorkey & 0xff000000) >> 24;
	u32 data32;

	colorkey &= 0x00ffffff;
	switch (color_index) {
	case COLOR_INDEX_16_655:
		r = (colorkey >> 10 & 0x3f) << 2;
		g = (colorkey >> 5 & 0x1f) << 3;
		b = (colorkey & 0x1f) << 3;
		break;
	case COLOR_INDEX_16_844:
		r = colorkey >> 8 & 0xff;
		g = (colorkey >> 4 & 0xf) << 4;
		b = (colorkey & 0xf) << 4;
		break;
	case COLOR_INDEX_16_565:
		r = (colorkey >> 11 & 0x1f) << 3;
		g = (colorkey >> 5 & 0x3f) << 2;
		b = (colorkey & 0x1f) << 3;
		break;
	case COLOR_INDEX_24_888_B:
		b = colorkey >> 16 & 0xff;
		g = colorkey >> 8 & 0xff;
		r = colorkey & 0xff;
		break;
	case COLOR_INDEX_24_RGB:
	case COLOR_INDEX_YUV_422:
		r = colorkey >> 16 & 0xff;
		g = colorkey >> 8 & 0xff;
		b = colorkey & 0xff;
		break;
	}
	data32 = r << 24 | g << 16 | b << 8 | a;
	if (osd_hw.color_key[index] != data32) {
		osd_hw.color_key[index] = data32;
		osd_log_dbg2("bpp:%d--r:0x%x g:0x%x b:0x%x ,a:0x%x\n",
				color_index, r, g, b, a);
		add_to_update_list(index, OSD_COLOR_KEY);
		osd_wait_vsync_hw();
	}
}
void  osd_srckey_enable_hw(u32  index, u8 enable)
{
	if (enable != osd_hw.color_key_enable[index]) {
		osd_hw.color_key_enable[index] = enable;
		add_to_update_list(index, OSD_COLOR_KEY_ENABLE);
		osd_wait_vsync_hw();
	}
}

void osd_set_color_mode(u32 index, const struct color_bit_define_s *color)
{
	if (color != osd_hw.color_info[index]) {
		osd_hw.color_info[index] = color;
		add_to_update_list(index, OSD_COLOR_MODE);
	}
}

void osd_update_disp_axis_hw(
	u32 index,
	u32 display_h_start,
	u32 display_h_end,
	u32 display_v_start,
	u32 display_v_end,
	u32 xoffset,
	u32 yoffset,
	u32 mode_change)
{
	struct pandata_s disp_data;
	struct pandata_s pan_data;

	if (index == OSD2)
		return;

	if (osd_hw.color_info[index] == NULL)
		return;
	disp_data.x_start = display_h_start;
	disp_data.y_start = display_v_start;
	disp_data.x_end = display_h_end;
	disp_data.y_end = display_v_end;
	pan_data.x_start = xoffset;
	pan_data.x_end = xoffset + (display_h_end - display_h_start);
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
	pan_data.y_start = osd_hw.pandata[index].y_start;
	pan_data.y_end = osd_hw.pandata[index].y_start +
					(display_v_end - display_v_start);
#else
	pan_data.y_start = yoffset;
	pan_data.y_end = yoffset + (display_v_end - display_v_start);
#endif
	/* if output mode change then reset pan ofFfset. */
	memcpy(&osd_hw.pandata[index], &pan_data, sizeof(struct pandata_s));
	memcpy(&osd_hw.dispdata[index], &disp_data, sizeof(struct pandata_s));
	spin_lock_irqsave(&osd_lock, lock_flags);
	if (mode_change) /* modify pandata . */
		osd_hw.reg[index][OSD_COLOR_MODE].update_func();
	osd_hw.reg[index][DISP_GEOMETRY].update_func();
	spin_unlock_irqrestore(&osd_lock, lock_flags);
	osd_wait_vsync_hw();
}

void osd_setup_hw(u32 index,
		  struct osd_ctl_s *osd_ctl,
		  u32 xoffset,
		  u32 yoffset,
		  u32 xres,
		  u32 yres,
		  u32 xres_virtual,
		  u32 yres_virtual,
		  u32 disp_start_x,
		  u32 disp_start_y,
		  u32 disp_end_x,
		  u32 disp_end_y,
		  u32 fbmem,
		  phys_addr_t *afbc_fbmem,
		  const struct color_bit_define_s *color)
{
	struct pandata_s disp_data;
	struct pandata_s pan_data;
	int update_color_mode = 0;
	int update_geometry = 0;
	u32 w = (color->bpp * xres_virtual + 7) >> 3;
	u32 i;

	pan_data.x_start = xoffset;
	pan_data.y_start = yoffset;
	disp_data.x_start = disp_start_x;
	disp_data.y_start = disp_start_y;
	if (likely(osd_hw.free_scale_enable[OSD1] && index == OSD1)) {
		if (!(osd_hw.free_scale_mode[OSD1])) {
			osd_log_info(
				"osd[%d] osd_setup_hw scale mode is error %d\n",
				index, osd_hw.free_scale_mode[OSD1]);
		} else {
			pan_data.x_end = xoffset + (disp_end_x - disp_start_x);
			pan_data.y_end = yoffset + (disp_end_y - disp_start_y);
			disp_data.x_end = disp_end_x;
			disp_data.y_end = disp_end_y;
		}
	} else {
		pan_data.x_end = xoffset + (disp_end_x - disp_start_x);
		pan_data.y_end = yoffset + (disp_end_y - disp_start_y);
		disp_data.x_end = disp_end_x;
		disp_data.y_end = disp_end_y;
	}
	if (osd_hw.fb_gem[index].addr != fbmem
		|| osd_hw.fb_gem[index].width != w
		|| osd_hw.fb_gem[index].height != yres_virtual) {
		osd_hw.fb_gem[index].addr = fbmem;
		osd_hw.fb_gem[index].width = w;
		osd_hw.fb_gem[index].height = yres_virtual;
		if (index == OSD1 &&
				osd_hw.osd_afbcd[OSD1].enable == ENABLE &&
				afbc_fbmem != NULL) {
			osd_hw.osd_afbcd[index].frame_width = xres;
			/* osd_hw.osd_afbcd[index].frame_height =
			 *	ALIGN_CALC(yres, 16) +
			 *	ALIGN_CALC(yres, 64) / 64;
			 */
			osd_hw.osd_afbcd[index].frame_height = yres;
			for (i = 0; i < OSD_MAX_BUF_NUM; i++)
				osd_hw.osd_afbcd[index].addr[i] =
					(u32)afbc_fbmem[i];
			if (pxp_mode)
				osd_hw.osd_afbcd[index].phy_addr =
					osd_hw.osd_afbcd[index].addr[0];
			else
				osd_hw.osd_afbcd[index].phy_addr = 0;
			/* we need update geometry
			 * and color mode for afbc mode
			 * update_geometry = 1;
			 * update_color_mode = 1;
			 */
			if (xres <= 128)
				osd_hw.osd_afbcd[index].conv_lbuf_len = 32;
			else if (xres <= 256)
				osd_hw.osd_afbcd[index].conv_lbuf_len = 64;
			else if (xres <= 512)
				osd_hw.osd_afbcd[index].conv_lbuf_len = 128;
			else if (xres <= 1024)
				osd_hw.osd_afbcd[index].conv_lbuf_len = 256;
			else if (xres <= 2048)
				osd_hw.osd_afbcd[index].conv_lbuf_len = 512;
			else
				osd_hw.osd_afbcd[index].conv_lbuf_len = 1024;
		}
		osd_log_info("osd[%d] canvas.idx =0x%x\n",
			index, osd_hw.fb_gem[index].canvas_idx);
		osd_log_info("osd[%d] canvas.addr=0x%x\n",
			index, osd_hw.fb_gem[index].addr);
		osd_log_info("osd[%d] canvas.width=%d\n",
			index, osd_hw.fb_gem[index].width);
		osd_log_info("osd[%d] canvas.height=%d\n",
			index, osd_hw.fb_gem[index].height);
		osd_log_info("osd[%d] frame.width=%d\n",
			index, xres);
		osd_log_info("osd[%d] frame.height=%d\n",
			index, yres);
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
		canvas_config(osd_hw.fb_gem[index].canvas_idx,
			osd_hw.fb_gem[index].addr,
			osd_hw.fb_gem[index].width,
			osd_hw.fb_gem[index].height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
#endif
	}

	/* need always set color mode for osd2 */
	if ((color != osd_hw.color_info[index]) || (index == OSD2)) {
		update_color_mode = 1;
		osd_hw.color_info[index] = color;
	}
	/* osd blank only control by /sys/class/graphcis/fbx/blank */
#if 0
	if (osd_hw.enable[index] == DISABLE) {
		osd_hw.enable[index] = ENABLE;
		add_to_update_list(index, OSD_ENABLE);
	}
#endif

	if (memcmp(&pan_data, &osd_hw.pandata[index],
				sizeof(struct pandata_s)) != 0 ||
	    memcmp(&disp_data, &osd_hw.dispdata[index],
		    sizeof(struct pandata_s)) != 0) {
		update_geometry = 1;
		memcpy(&osd_hw.pandata[index], &pan_data,
				sizeof(struct pandata_s));
		memcpy(&osd_hw.dispdata[index], &disp_data,
				sizeof(struct pandata_s));
	}
	spin_lock_irqsave(&osd_lock, lock_flags);
	if (update_color_mode)
		osd_hw.reg[index][OSD_COLOR_MODE].update_func();
	if (update_geometry)
		osd_hw.reg[index][DISP_GEOMETRY].update_func();
	spin_unlock_irqrestore(&osd_lock, lock_flags);

	if (osd_hw.antiflicker_mode)
		osd_antiflicker_update_pan(yoffset, yres);
	if (osd_hw.clone[index])
		osd_clone_pan(index, yoffset, 0);
#ifdef CONFIG_AM_FB_EXT
	osd_ext_clone_pan(index);
#endif
	osd_wait_vsync_hw();
}

void osd_setpal_hw(u32 index,
		   unsigned int regno,
		   unsigned int red,
		   unsigned int green,
		   unsigned int blue,
		   unsigned int transp
		  )
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		return;

	if (regno < 256) {
		u32 pal;

		pal = ((red   & 0xff) << 24) |
		      ((green & 0xff) << 16) |
		      ((blue  & 0xff) <<  8) |
		      (transp & 0xff);
		spin_lock_irqsave(&osd_lock, lock_flags);
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_COLOR_ADDR + REG_OFFSET * index,
				regno);
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_COLOR + REG_OFFSET * index, pal);
		spin_unlock_irqrestore(&osd_lock, lock_flags);
	}
}

void osd_get_order_hw(u32 index, u32 *order)
{
	*order = osd_hw.order & 0x3;
}

void osd_set_order_hw(u32 index, u32 order)
{
	if ((order != OSD_ORDER_01) && (order != OSD_ORDER_10))
		return;
	osd_hw.order = order;
	add_to_update_list(index, OSD_CHANGE_ORDER);
	osd_wait_vsync_hw();
}

/* osd free scale mode */
static void osd_set_free_scale_enable_mode1(u32 index, u32 enable)
{
	unsigned int h_enable = 0;
	unsigned int v_enable = 0;
	int ret = 0;

	h_enable = (enable & 0xffff0000 ? 1 : 0);
	v_enable = (enable & 0xffff ? 1 : 0);
	osd_hw.free_scale[index].h_enable = h_enable;
	osd_hw.free_scale[index].v_enable = v_enable;
	osd_hw.free_scale_enable[index] = enable;
	if (osd_hw.free_scale_enable[index]) {
		ret = osd_set_scan_mode(index);
		spin_lock_irqsave(&osd_lock, lock_flags);
		if (ret)
			osd_hw.reg[index][OSD_COLOR_MODE].update_func();
		osd_hw.reg[index][OSD_FREESCALE_COEF].update_func();
		osd_hw.reg[index][DISP_GEOMETRY].update_func();
		osd_hw.reg[index][DISP_FREESCALE_ENABLE].update_func();
		osd_hw.reg[index][OSD_ENABLE].update_func();
		spin_unlock_irqrestore(&osd_lock, lock_flags);
	} else {
		ret = osd_set_scan_mode(index);
		spin_lock_irqsave(&osd_lock, lock_flags);
		if (ret)
			osd_hw.reg[index][OSD_COLOR_MODE].update_func();
		osd_hw.reg[index][DISP_GEOMETRY].update_func();
		osd_hw.reg[index][DISP_FREESCALE_ENABLE].update_func();
		osd_hw.reg[index][OSD_ENABLE].update_func();
		spin_unlock_irqrestore(&osd_lock, lock_flags);
	}
	osd_wait_vsync_hw();
}

void osd_set_free_scale_enable_hw(u32 index, u32 enable)
{
	if (osd_hw.free_scale_mode[index])
		osd_set_free_scale_enable_mode1(index, enable);
	else if (enable)
		osd_log_info(
			"osd[%d] free_scale_enable_hw mode is error %d\n",
			index, osd_hw.free_scale_mode[index]);
}

void osd_get_free_scale_enable_hw(u32 index, u32 *free_scale_enable)
{
	*free_scale_enable = osd_hw.free_scale_enable[index];
}

void osd_set_free_scale_mode_hw(u32 index, u32 freescale_mode)
{
	osd_hw.free_scale_mode[index] = freescale_mode;
}

void osd_get_free_scale_mode_hw(u32 index, u32 *freescale_mode)
{
	*freescale_mode = osd_hw.free_scale_mode[index];
}

void osd_set_4k2k_fb_mode_hw(u32 fb_for_4k2k)
{
	osd_hw.fb_for_4k2k = fb_for_4k2k;
}

void osd_get_free_scale_width_hw(u32 index, u32 *free_scale_width)
{
	*free_scale_width = osd_hw.free_src_data[index].x_end -
		osd_hw.free_src_data[index].x_start + 1;
}

void osd_get_free_scale_height_hw(u32 index, u32 *free_scale_height)
{
	*free_scale_height = osd_hw.free_src_data[index].y_end -
		osd_hw.free_src_data[index].y_start + 1;
}

void osd_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	*x0 = osd_hw.free_src_data[index].x_start;
	*y0 = osd_hw.free_src_data[index].y_start;
	*x1 = osd_hw.free_src_data[index].x_end;
	*y1 = osd_hw.free_src_data[index].y_end;
}

void osd_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_hw.free_src_data[index].x_start = x0;
	osd_hw.free_src_data[index].y_start = y0;
	osd_hw.free_src_data[index].x_end = x1;
	osd_hw.free_src_data[index].y_end = y1;
}

void osd_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	*x0 = osd_hw.scaledata[index].x_start;
	*x1 = osd_hw.scaledata[index].x_end;
	*y0 = osd_hw.scaledata[index].y_start;
	*y1 = osd_hw.scaledata[index].y_end;
}

void osd_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_hw.scaledata[index].x_start = x0;
	osd_hw.scaledata[index].x_end = x1;
	osd_hw.scaledata[index].y_start = y0;
	osd_hw.scaledata[index].y_end = y1;
}

void osd_get_window_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	struct vinfo_s *vinfo;
	s32 height;

	vinfo = get_current_vinfo();
	if (vinfo) {
		if (is_interlaced(vinfo)) {
			height = osd_hw.free_dst_data[index].y_end -
				osd_hw.free_dst_data[index].y_start + 1;
			height *= 2;
			*y0 = osd_hw.free_dst_data[index].y_start * 2;
			*y1 = height + *y0 - 1;
		} else {
			*y0 = osd_hw.free_dst_data[index].y_start;
			*y1 = osd_hw.free_dst_data[index].y_end;
		}
	} else {
		*y0 = osd_hw.free_dst_data[index].y_start;
		*y1 = osd_hw.free_dst_data[index].y_end;
	}
	*x0 = osd_hw.free_dst_data[index].x_start;
	*x1 = osd_hw.free_dst_data[index].x_end;
}

void osd_set_window_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	mutex_lock(&osd_mutex);
	if (vinfo) {
		if (is_interlaced(vinfo)) {
			osd_hw.free_dst_data[index].y_start = y0 / 2;
			osd_hw.free_dst_data[index].y_end = y1 / 2;
		} else {
			osd_hw.free_dst_data[index].y_start = y0;
			osd_hw.free_dst_data[index].y_end = y1;
		}
	} else {
		osd_hw.free_dst_data[index].y_start = y0;
		osd_hw.free_dst_data[index].y_end = y1;
	}
	osd_hw.free_dst_data[index].x_start = x0;
	osd_hw.free_dst_data[index].x_end = x1;
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	osd_hw.cursor_dispdata[index].x_start = x0;
	osd_hw.cursor_dispdata[index].x_end = x1;
	osd_hw.cursor_dispdata[index].y_start = y0;
	osd_hw.cursor_dispdata[index].y_end = y1;
#endif
	if (osd_hw.free_dst_data[index].y_end >= 2159) {
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)
			osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0x002020ff);
		else if (get_cpu_type() ==
			MESON_CPU_MAJOR_ID_GXTVBB)
			osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0xff);
		else
			osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0x008080ff);
	}
	osd_update_window_axis = true;
	mutex_unlock(&osd_mutex);
}

void osd_get_block_windows_hw(u32 index, u32 *windows)
{
	/*
	 * memcpy(windows, osd_hw.block_windows[index],
	 *      sizeof(osd_hw.block_windows[index]));
	 */
}

void osd_set_block_windows_hw(u32 index, u32 *windows)
{
	/*
	 * memcpy(osd_hw.block_windows[index], windows,
	 *      sizeof(osd_hw.block_windows[index]));
	 */
	add_to_update_list(index, DISP_GEOMETRY);
	osd_wait_vsync_hw();
}

void osd_get_block_mode_hw(u32 index, u32 *mode)
{
	*mode = osd_hw.block_mode[index];
}

void osd_set_block_mode_hw(u32 index, u32 mode)
{
	/* osd_hw.block_mode[index] = mode; */
	add_to_update_list(index, DISP_GEOMETRY);
	osd_wait_vsync_hw();
}

void osd_enable_3d_mode_hw(u32 index, u32 enable)
{
	spin_lock_irqsave(&osd_lock, lock_flags);
	osd_hw.mode_3d[index].enable = enable;
	spin_unlock_irqrestore(&osd_lock, lock_flags);
	if (enable) {
		/* when disable 3d mode ,we should return to stardard state. */
		osd_hw.mode_3d[index].left_right = OSD_LEFT;
		osd_hw.mode_3d[index].l_start = osd_hw.pandata[index].x_start;
		osd_hw.mode_3d[index].l_end = (osd_hw.pandata[index].x_end +
					osd_hw.pandata[index].x_start) >> 1;
		osd_hw.mode_3d[index].r_start = osd_hw.mode_3d[index].l_end + 1;
		osd_hw.mode_3d[index].r_end = osd_hw.pandata[index].x_end;
		osd_hw.mode_3d[index].origin_scale.h_enable =
					osd_hw.scale[index].h_enable;
		osd_hw.mode_3d[index].origin_scale.v_enable =
					osd_hw.scale[index].v_enable;
		osd_set_2x_scale_hw(index, 1, 0);
	} else {
		osd_set_2x_scale_hw(index,
				osd_hw.mode_3d[index].origin_scale.h_enable,
				osd_hw.mode_3d[index].origin_scale.v_enable);
	}
}

void osd_enable_hw(u32 index, u32 enable)
{
	int i = 0;
	int count = (pxp_mode == 1)?3:WAIT_AFBC_READY_COUNT;

	if (index == 0) {
		osd_log_info("osd[%d] enable: %d (%s)\n",
				index, enable, current->comm);
	} else {
		osd_log_info("osd[%d] enable: %d (%s)\n",
				index, enable, current->comm);
	}

	/* reset viu 31bit ?? */
	if (index == OSD1 &&
			!osd_hw.enable[index] &&
			osd_hw.osd_afbcd[index].enable && enable) {
		spin_lock_irqsave(&osd_lock, lock_flags);
		osd_reg_write(VIU_SW_RESET, 0x80000000);
		osd_reg_write(VIU_SW_RESET, 0);
		spin_unlock_irqrestore(&osd_lock, lock_flags);
		osd_afbc_dec_enable = 0;

		add_to_update_list(index, OSD_COLOR_MODE);
		add_to_update_list(index, OSD_GBL_ALPHA);
		add_to_update_list(index, DISP_GEOMETRY);
		osd_wait_vsync_hw();
	}

	while ((index == 0) && osd_hw.osd_afbcd[index].enable &&
			(osd_hw.osd_afbcd[index].phy_addr == 0) &&
			enable && (i < count)) {
		osd_wait_vsync_hw();
		i++;
	}
	if (i > 0)
		osd_log_info("osd[%d]: wait %d vsync first buffer ready.\n",
			index, i);
	osd_hw.enable[index] = enable;
	add_to_update_list(index, OSD_ENABLE);
	osd_wait_vsync_hw();
}

void osd_set_2x_scale_hw(u32 index, u16 h_scale_enable, u16 v_scale_enable)
{
	osd_log_info("osd[%d] set scale, h_scale: %s, v_scale: %s\n",
		     index, h_scale_enable ? "ENABLE" : "DISABLE",
		     v_scale_enable ? "ENABLE" : "DISABLE");
	osd_log_info("osd[%d].scaledata: %d %d %d %d\n",
		     index,
		     osd_hw.scaledata[index].x_start,
		     osd_hw.scaledata[index].x_end,
		     osd_hw.scaledata[index].y_start,
		     osd_hw.scaledata[index].y_end);
	osd_log_info("osd[%d].pandata: %d %d %d %d\n",
		     index,
		     osd_hw.pandata[index].x_start,
		     osd_hw.pandata[index].x_end,
		     osd_hw.pandata[index].y_start,
		     osd_hw.pandata[index].y_end);
	osd_hw.scale[index].h_enable = h_scale_enable;
	osd_hw.scale[index].v_enable = v_scale_enable;
	spin_lock_irqsave(&osd_lock, lock_flags);
	osd_hw.reg[index][DISP_SCALE_ENABLE].update_func();
	osd_hw.reg[index][DISP_GEOMETRY].update_func();
	spin_unlock_irqrestore(&osd_lock, lock_flags);
	osd_wait_vsync_hw();
}

void osd_get_flush_rate_hw(u32 *break_rate)
{
	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	*break_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
}

void osd_set_antiflicker_hw(u32 index, struct vinfo_s *vinfo, u32 yres)
{
	if (is_interlaced(vinfo)) {
		osd_hw.antiflicker_mode = 1;
		osd_antiflicker_task_start();
		osd_antiflicker_enable(1);
		osd_antiflicker_update_pan(osd_hw.pandata[index].y_start, yres);
	} else {
		if (osd_hw.antiflicker_mode)
			osd_antiflicker_task_stop();
		osd_hw.antiflicker_mode = 0;
	}
}

void osd_get_antiflicker_hw(u32 index, u32 *on_off)
{
	*on_off = osd_hw.antiflicker_mode;
}

void osd_clone_pan(u32 index, u32 yoffset, int debug_flag)
{
	s32 offset = 0;
	u32 index_buffer = 0;
	s32 osd0_buffer_number = 0;
	s32 height_osd1 = 0;

	if (yoffset != 0) {
		index_buffer = osd_hw.fb_gem[index].height / yoffset;
		if (index_buffer == 3)
			osd0_buffer_number = 1;
		else if (index_buffer == 1)
			osd0_buffer_number = 2;
	} else
		osd0_buffer_number = 0;
	osd_clone_get_virtual_yres(&height_osd1);
	if (osd_hw.clone[index]) {
		offset = osd0_buffer_number * height_osd1;
		osd_hw.pandata[OSD2].y_start = offset;
		osd_hw.pandata[OSD2].y_end = offset + height_osd1 - 1;
		if (osd_hw.angle[OSD2]) {
			if (debug_flag)
				osd_log_dbg("++ osd_clone_pan start when enable clone\n");
			osd_clone_update_pan(osd0_buffer_number);
		}
		add_to_update_list(OSD2, DISP_GEOMETRY);
	}
}

void osd_set_angle_hw(u32 index, u32 angle, u32  virtual_osd1_yres,
		      u32 virtual_osd2_yres)
{
#ifndef OSD_GE2D_CLONE_SUPPORT
	osd_log_err("++ osd_clone depends on GE2D module!\n");
	return;
#endif
	if (angle > 4) {
		osd_log_err("++ invalid angle: %d\n", angle);
		return;
	}
	osd_log_info("++ virtual_osd1_yres is %d, virtual_osd2_yres is %d!\n",
		     virtual_osd1_yres, virtual_osd2_yres);
	osd_clone_set_virtual_yres(virtual_osd1_yres, virtual_osd2_yres);
	if (osd_hw.clone[index] == 0) {
		osd_log_info("++ set osd[%d]->angle: %d->%d\n",
				index, osd_hw.angle[index], angle);
		osd_clone_set_angle(angle);
		osd_hw.angle[index] = angle;
	} else if (!((osd_hw.angle[index] == 0) || (angle == 0))) {
		osd_log_info("++ set osd[%d]->angle: %d->%d\n",
				index, osd_hw.angle[index], angle);
		osd_clone_set_angle(angle);
		osd_hw.angle[index] = angle;
		osd_clone_pan(index, osd_hw.pandata[OSD1].y_start, 1);
	}
}

void osd_get_angle_hw(u32 index, u32 *angle)
{
	*angle = osd_hw.angle[index];
}

void osd_set_clone_hw(u32 index, u32 clone)
{
	int ret = -1;

	osd_log_info("++ set osd[%d]->clone: %d->%d\n",
			index, osd_hw.clone[index], clone);
	osd_hw.clone[index] = clone;
	if (osd_hw.clone[index]) {
		if (osd_hw.angle[index]) {
			osd_hw.color_info[index] = osd_hw.color_info[OSD1];
			ret = osd_clone_task_start();
			if (ret)
				osd_clone_pan(index,
					osd_hw.pandata[OSD1].y_start, 1);
			else
				osd_log_err("++ start clone error\n");
		}
	} else {
		if (osd_hw.angle[index])
			osd_clone_task_stop();
	}
	add_to_update_list(index, OSD_COLOR_MODE);
}

void osd_set_update_pan_hw(u32 index)
{
	osd_clone_pan(index, osd_hw.pandata[OSD1].y_start, 1);
}

void osd_get_clone_hw(u32 index, u32 *clone)
{
	*clone = osd_hw.clone[index];
}

void osd_set_reverse_hw(u32 index, u32 reverse)
{
	char *str[4] = {"NONE", "ALL", "X_REV", "Y_REV"};

	osd_hw.osd_reverse[index] = reverse;
	pr_info("set osd%d reverse as %s\n", index, str[reverse]);
	add_to_update_list(index, DISP_OSD_REVERSE);
	osd_wait_vsync_hw();
}

void osd_get_reverse_hw(u32 index, u32 *reverse)
{
	*reverse = osd_hw.osd_reverse[index];
}

void osd_switch_free_scale(
	u32 pre_index, u32 pre_enable, u32 pre_scale,
	u32 next_index, u32 next_enable, u32 next_scale)
{
	unsigned int h_enable = 0;
	unsigned int v_enable = 0;
	int i = 0;
	int count = (pxp_mode == 1)?3:WAIT_AFBC_READY_COUNT;

	osd_log_info("osd[%d] enable: %d scale:0x%x (%s)\n",
		pre_index, pre_enable, pre_scale, current->comm);
	osd_log_info("osd[%d] enable: %d scale:0x%x (%s)\n",
		next_index, next_enable, next_scale, current->comm);
	if (osd_hw.free_scale_mode[pre_index]
		|| osd_hw.free_scale_mode[next_index]) {
		while ((next_index == OSD1)
			&& osd_hw.osd_afbcd[next_index].enable
			&& (osd_hw.osd_afbcd[next_index].phy_addr == 0)
			&& next_enable && (i < count)) {
			osd_wait_vsync_hw();
			i++;
		}
		if (i > 0)
			osd_log_info("osd[%d]: wait %d vsync first buffer ready.\n",
				next_index, i);
		h_enable = (pre_scale & 0xffff0000 ? 1 : 0);
		v_enable = (pre_scale & 0xffff ? 1 : 0);
		osd_hw.free_scale[pre_index].h_enable = h_enable;
		osd_hw.free_scale[pre_index].v_enable = v_enable;
		osd_hw.free_scale_enable[pre_index] = pre_scale;
		osd_hw.enable[pre_index] = pre_enable;

		h_enable = (next_scale & 0xffff0000 ? 1 : 0);
		v_enable = (next_scale & 0xffff ? 1 : 0);
		osd_hw.free_scale[next_index].h_enable = h_enable;
		osd_hw.free_scale[next_index].v_enable = v_enable;
		osd_hw.free_scale_enable[next_index] = next_scale;
		osd_hw.enable[next_index] = next_enable;

		osd_set_scan_mode(next_index);
		spin_lock_irqsave(&osd_lock, lock_flags);
		if (next_index == OSD1
			&& osd_hw.osd_afbcd[next_index].enable
			&& next_enable) {
			osd_reg_write(VIU_SW_RESET, 0x80000000);
			osd_reg_write(VIU_SW_RESET, 0);
			osd_afbc_dec_enable = 0;
			osd_hw.reg[next_index][OSD_GBL_ALPHA].update_func();
		}

		osd_hw.reg[pre_index][OSD_COLOR_MODE].update_func();
		if (pre_scale)
			osd_hw.reg[pre_index][OSD_FREESCALE_COEF].update_func();
		osd_hw.reg[pre_index][DISP_GEOMETRY].update_func();
		osd_hw.reg[pre_index][DISP_FREESCALE_ENABLE].update_func();
		osd_hw.reg[pre_index][OSD_ENABLE].update_func();

		osd_hw.reg[next_index][OSD_COLOR_MODE].update_func();
		if (next_scale)
			osd_hw.reg[next_index]
				[OSD_FREESCALE_COEF].update_func();
		osd_hw.reg[next_index][DISP_GEOMETRY].update_func();
		osd_hw.reg[next_index][DISP_FREESCALE_ENABLE].update_func();
		osd_hw.reg[next_index][OSD_ENABLE].update_func();

		spin_unlock_irqrestore(&osd_lock, lock_flags);
		osd_wait_vsync_hw();
	} else {
		osd_enable_hw(pre_index, pre_enable);
		osd_enable_hw(next_index, next_enable);
	}
}

void osd_get_urgent(u32 index, u32 *urgent)
{
	*urgent = osd_hw.urgent[index];
}

void osd_set_urgent(u32 index, u32 urgent)
{
	osd_hw.urgent[index] = urgent;
	add_to_update_list(index, OSD_FIFO);
	osd_wait_vsync_hw();
}

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
static void osd_pan_display_fence(struct osd_fence_map_s *fence_map)
{
	s32 ret = 1;
	long diff_x, diff_y;
	u32 index = fence_map->fb_index;
	u32 xoffset = fence_map->xoffset;
	u32 yoffset = fence_map->yoffset;

	if (index >= 2)
		return;
	if (timeline_created) { /* out fence created success. */
		ret = osd_wait_buf_ready(fence_map);
		if (ret < 0)
			osd_log_dbg("fence wait ret %d\n", ret);
	}
	if (ret) {
		if (xoffset != osd_hw.pandata[index].x_start
		    || yoffset != osd_hw.pandata[index].y_start) {
			spin_lock_irqsave(&osd_lock, lock_flags);
			diff_x = xoffset - osd_hw.pandata[index].x_start;
			diff_y = yoffset - osd_hw.pandata[index].y_start;
			osd_hw.pandata[index].x_start += diff_x;
			osd_hw.pandata[index].x_end   += diff_x;
			osd_hw.pandata[index].y_start += diff_y;
			osd_hw.pandata[index].y_end   += diff_y;
			if (index == OSD1 &&
				osd_hw.osd_afbcd[index].enable == ENABLE) {
				/* osd_hw.osd_afbcd[index].phy_addr =
				 *	(osd_hw.pandata[index].y_start /
				 *	osd_hw.osd_afbcd[index].frame_height) *
				 *	osd_hw.osd_afbcd[index].frame_height *
				 *  osd_hw.fb_gem[index].width;
				 *  osd_hw.osd_afbcd[index].phy_addr +=
				 *  osd_hw.fb_gem[index].addr;
				 */
				osd_hw.osd_afbcd[index].phy_addr =
					osd_hw.osd_afbcd[index].addr
					[osd_hw.pandata[index].y_start /
					osd_hw.osd_afbcd[index].frame_height];
			}
			osd_hw.reg[index][DISP_GEOMETRY].update_func();
			if (osd_hw.free_scale_enable[index]
					&& osd_update_window_axis) {
				osd_hw.reg[index][DISP_FREESCALE_ENABLE]
					.update_func();
				osd_update_window_axis = false;
			}
			spin_unlock_irqrestore(&osd_lock, lock_flags);
			osd_wait_vsync_hw();
		}
	}
	if (timeline_created) {
		if (ret)
			sw_sync_timeline_inc(timeline, 1);
		else
			osd_log_err("------NOT signal out_fence ERROR\n");
	}
#ifdef CONFIG_AM_FB_EXT
	if (ret)
		osd_ext_clone_pan(index);
#endif
	osd_log_dbg2("offset[%d-%d]x[%d-%d]y[%d-%d]\n",
		    xoffset, yoffset,
		    osd_hw.pandata[index].x_start,
		    osd_hw.pandata[index].x_end,
		    osd_hw.pandata[index].y_start,
		    osd_hw.pandata[index].y_end);
}
#endif

void osd_pan_display_hw(u32 index, unsigned int xoffset, unsigned int yoffset)
{
	long diff_x, diff_y;

#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	if (index >= 1)
#else
	if (index >= 2)
#endif
		return;
	if (xoffset != osd_hw.pandata[index].x_start
	    || yoffset != osd_hw.pandata[index].y_start) {
		diff_x = xoffset - osd_hw.pandata[index].x_start;
		diff_y = yoffset - osd_hw.pandata[index].y_start;
		osd_hw.pandata[index].x_start += diff_x;
		osd_hw.pandata[index].x_end   += diff_x;
		osd_hw.pandata[index].y_start += diff_y;
		osd_hw.pandata[index].y_end   += diff_y;
		add_to_update_list(index, DISP_GEOMETRY);
		osd_wait_vsync_hw();
	}
#ifdef CONFIG_AM_FB_EXT
	osd_ext_clone_pan(index);
#endif
	osd_log_dbg2("offset[%d-%d]x[%d-%d]y[%d-%d]\n",
		    xoffset, yoffset,
		    osd_hw.pandata[index].x_start,
		    osd_hw.pandata[index].x_end,
		    osd_hw.pandata[index].y_start,
		    osd_hw.pandata[index].y_end);
}

static  void  osd1_update_disp_scale_enable(void)
{
	if (osd_hw.scale[OSD1].h_enable)
		VSYNCOSD_SET_MPEG_REG_MASK(VIU_OSD1_BLK0_CFG_W0, 3 << 12);
	else
		VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD1_BLK0_CFG_W0, 3 << 12);
	if (osd_hw.scan_mode != SCAN_MODE_INTERLACE) {
		if (osd_hw.scale[OSD1].v_enable)
			VSYNCOSD_SET_MPEG_REG_MASK(
				VIU_OSD1_BLK0_CFG_W0,
				1 << 14);
		else
			VSYNCOSD_CLR_MPEG_REG_MASK(
				VIU_OSD1_BLK0_CFG_W0,
				1 << 14);
	}
	remove_from_update_list(OSD1, DISP_SCALE_ENABLE);
}

static  void  osd2_update_disp_scale_enable(void)
{
	if (osd_hw.scale[OSD2].h_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
		VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0, 3 << 12);
#else
		VSYNCOSD_SET_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0, 3 << 12);
#endif
	} else
		VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0, 3 << 12);
	if (osd_hw.scan_mode != SCAN_MODE_INTERLACE) {
		if (osd_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
			VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0,
					1 << 14);
#else
			VSYNCOSD_SET_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0,
					1 << 14);
#endif
		} else
			VSYNCOSD_CLR_MPEG_REG_MASK(VIU_OSD2_BLK0_CFG_W0,
					1 << 14);
	}
	remove_from_update_list(OSD2, DISP_SCALE_ENABLE);
}

#ifdef CONFIG_AMLOGIC_SUPERSCALER
static void osd_super_scale_enable(u32 index)
{
	u32 data32 = 0x0;
	u32 src_w = 0, src_h = 0;

	osd_super_scale_mem_power_on();

	/* enable osd scaler path */
	if (index == OSD1)
		data32 = 8;
	else {
		data32 = 1;       /* select osd2 input */
		data32 |= 1 << 3; /* enable osd scaler path */
	}
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, data32);
	/* enable osd super scaler */
	data32 = (1 << 0)
		 | (1 << 1)
		 | (1 << 2);
	VSYNCOSD_WR_MPEG_REG(OSDSR_CTRL_MODE, data32);
	/* config osd super scaler setting */
	VSYNCOSD_WR_MPEG_REG(OSDSR_UK_GRAD2DDIAG_LIMIT, 0xffffff);
	VSYNCOSD_WR_MPEG_REG(OSDSR_UK_GRAD2DADJA_LIMIT, 0xffffff);
	VSYNCOSD_WR_MPEG_REG(OSDSR_UK_BST_GAIN, 0x7a7a3a50);
	/* config osd super scaler input size */
	src_w = osd_hw.free_src_data[OSD1].x_end -
		osd_hw.free_src_data[OSD1].x_start + 1;
	src_h = osd_hw.free_src_data[OSD1].y_end -
		osd_hw.free_src_data[OSD1].y_start + 1;
	data32 = (src_w & 0x1fff) | (src_h & 0x1fff) << 16;
	VSYNCOSD_WR_MPEG_REG(OSDSR_HV_SIZEIN, data32);
	/* config osd super scaler output size */
	data32 = ((osd_hw.free_dst_data[index].x_end & 0xfff) |
		  (osd_hw.free_dst_data[index].x_start & 0xfff) << 16);
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_H_START_END, data32);
	data32 = ((osd_hw.free_dst_data[index].y_end & 0xfff) |
		  (osd_hw.free_dst_data[index].y_start & 0xfff) << 16);
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_V_START_END, data32);
}

static void osd_super_scale_disable(void)
{
	/* disable osd scaler path */
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, 0);
	/* disable osd super scaler */
	VSYNCOSD_WR_MPEG_REG(OSDSR_HV_SIZEIN, 0);
	VSYNCOSD_WR_MPEG_REG(OSDSR_CTRL_MODE, 0);
	osd_super_scale_mem_power_off();
}
#endif
static void osd1_update_disp_freescale_enable(void)
{
	int hf_phase_step, vf_phase_step;
	int src_w, src_h, dst_w, dst_h;
	int bot_ini_phase;
	int vsc_ini_rcv_num, vsc_ini_rpt_p0_num;
	int vsc_bot_rcv_num = 0, vsc_bot_rpt_p0_num = 0;
	int hsc_ini_rcv_num, hsc_ini_rpt_p0_num;
	int hf_bank_len = 4;
	int vf_bank_len = 0;
	u32 data32 = 0x0;

	if (osd_hw.scale_workaround)
		vf_bank_len = 2;
	else
		vf_bank_len = 4;
	if (osd_hw.bot_type == 1) {
		vsc_bot_rcv_num = 4;
		vsc_bot_rpt_p0_num = 1;
	} else if (osd_hw.bot_type == 2) {
		vsc_bot_rcv_num = 6;
		vsc_bot_rpt_p0_num = 2;
	} else if (osd_hw.bot_type == 3) {
		vsc_bot_rcv_num = 8;
		vsc_bot_rpt_p0_num = 3;
	}
	hsc_ini_rcv_num = hf_bank_len;
	vsc_ini_rcv_num = vf_bank_len;
	hsc_ini_rpt_p0_num =
		(hf_bank_len / 2 - 1) > 0 ? (hf_bank_len / 2 - 1) : 0;
	vsc_ini_rpt_p0_num =
		(vf_bank_len / 2 - 1) > 0 ? (vf_bank_len / 2 - 1) : 0;

	src_w = osd_hw.free_src_data[OSD1].x_end -
		osd_hw.free_src_data[OSD1].x_start + 1;
	src_h = osd_hw.free_src_data[OSD1].y_end -
		osd_hw.free_src_data[OSD1].y_start + 1;
	dst_w = osd_hw.free_dst_data[OSD1].x_end -
		osd_hw.free_dst_data[OSD1].x_start + 1;
	dst_h = osd_hw.free_dst_data[OSD1].y_end -
		osd_hw.free_dst_data[OSD1].y_start + 1;
    #ifdef CONFIG_AMLOGIC_SUPERSCALER
	/* super scaler mode */
	if (osd_hw.free_scale_mode[OSD1] & 0x2) {
		if (osd_hw.free_scale_enable[OSD1])
			osd_super_scale_enable(OSD1);
		else
			osd_super_scale_disable();
		remove_from_update_list(OSD1, DISP_FREESCALE_ENABLE);
		return;
	}
    #endif
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD1]) {
		/* enable osd scaler */
		if (osd_hw.free_scale_mode[OSD1] & 0x1) {
			data32 |= 1 << 2; /* enable osd scaler */
			data32 |= 1 << 3; /* enable osd scaler path */
			VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, data32);
		}
	} else {
		/* disable osd scaler path */
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, 0);
	}
	hf_phase_step = (src_w << 18) / dst_w;
	hf_phase_step = (hf_phase_step << 6);
	vf_phase_step = (src_h << 20) / dst_h;
	if (osd_hw.field_out_en)   /* interface output */
		bot_ini_phase = ((vf_phase_step / 2) >> 4);
	else
		bot_ini_phase = 0;
	vf_phase_step = (vf_phase_step << 4);
	/* config osd scaler in/out hv size */
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD1]) {
		data32 = (((src_h - 1) & 0x1fff)
				| ((src_w - 1) & 0x1fff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCI_WH_M1, data32);
		data32 = ((osd_hw.free_dst_data[OSD1].x_end & 0xfff) |
			  (osd_hw.free_dst_data[OSD1].x_start & 0xfff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_H_START_END, data32);
		data32 = ((osd_hw.free_dst_data[OSD1].y_end & 0xfff) |
			  (osd_hw.free_dst_data[OSD1].y_start & 0xfff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_V_START_END, data32);
	}
	data32 = 0x0;
	if (osd_hw.free_scale[OSD1].v_enable) {
		data32 |= (vf_bank_len & 0x7)
			| ((vsc_ini_rcv_num & 0xf) << 3)
			| ((vsc_ini_rpt_p0_num & 0x3) << 8);
		if (osd_hw.field_out_en)
			data32 |= ((vsc_bot_rcv_num & 0xf) << 11)
				| ((vsc_bot_rpt_p0_num & 0x3) << 16)
				| (1 << 23);
		if (osd_hw.scale_workaround)
			data32 |= 1 << 21;
		data32 |= 1 << 24;
	}
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_VSC_CTRL0, data32);
	data32 = 0x0;
	if (osd_hw.free_scale[OSD1].h_enable) {
		data32 |= (hf_bank_len & 0x7)
			| ((hsc_ini_rcv_num & 0xf) << 3)
			| ((hsc_ini_rpt_p0_num & 0x3) << 8);
		data32 |= 1 << 22;
	}
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_HSC_CTRL0, data32);
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD1]) {
		data32 |= (bot_ini_phase & 0xffff) << 16;
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_HSC_PHASE_STEP,
				hf_phase_step, 0, 28);
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_HSC_INI_PHASE, 0, 0, 16);
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_VSC_PHASE_STEP,
				vf_phase_step, 0, 28);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_VSC_INI_PHASE, data32);
	}
	remove_from_update_list(OSD1, DISP_FREESCALE_ENABLE);
}

static void osd1_update_coef(void)
{
	int i;
	bool need_update_coef = false;
	int hf_coef_wren = 1;
	int vf_coef_wren = 1;
	int *hf_coef, *vf_coef;

	if (osd_hw.scale_workaround) {
		if (use_v_filter_mode != 3) {
			use_v_filter_mode = 3;
			need_update_coef = true;
		} else
			need_update_coef = false;
	} else {
		if (use_v_filter_mode != osd_v_filter_mode) {
			use_v_filter_mode = osd_v_filter_mode;
			need_update_coef = true;
		} else
			need_update_coef = false;
	}
	if (need_update_coef) {
		vf_coef = filter_table[use_v_filter_mode];
		if (vf_coef_wren) {
			osd_reg_set_bits(VPP_OSD_SCALE_COEF_IDX, 0x0000, 0, 9);
			for (i = 0; i < 33; i++)
				osd_reg_write(VPP_OSD_SCALE_COEF, vf_coef[i]);
		}
	}
	need_update_coef = false;
	if (use_h_filter_mode != osd_h_filter_mode) {
		use_h_filter_mode = osd_h_filter_mode;
		need_update_coef = true;
	}
	hf_coef = filter_table[use_h_filter_mode];
	if (need_update_coef) {
		if (hf_coef_wren) {
			osd_reg_set_bits(VPP_OSD_SCALE_COEF_IDX, 0x0100, 0, 9);
			for (i = 0; i < 33; i++)
				osd_reg_write(VPP_OSD_SCALE_COEF, hf_coef[i]);
		}
	}
	remove_from_update_list(OSD1, OSD_FREESCALE_COEF);
}

static void osd2_update_disp_freescale_enable(void)
{
	int hf_phase_step, vf_phase_step;
	int src_w, src_h, dst_w, dst_h;
	int bot_ini_phase;
	int vsc_ini_rcv_num, vsc_ini_rpt_p0_num;
	int vsc_bot_rcv_num = 6, vsc_bot_rpt_p0_num = 2;
	int hsc_ini_rcv_num, hsc_ini_rpt_p0_num;
	int hf_bank_len = 4;
	int vf_bank_len = 4;
	u32 data32 = 0x0;

	if (osd_hw.scale_workaround)
		vf_bank_len = 2;
	hsc_ini_rcv_num = hf_bank_len;
	vsc_ini_rcv_num = vf_bank_len;
	hsc_ini_rpt_p0_num =
		(hf_bank_len / 2 - 1) > 0 ? (hf_bank_len / 2 - 1) : 0;
	vsc_ini_rpt_p0_num =
		(vf_bank_len / 2 - 1) > 0 ? (vf_bank_len / 2 - 1) : 0;

	src_w = osd_hw.free_src_data[OSD2].x_end -
		osd_hw.free_src_data[OSD2].x_start + 1;
	src_h = osd_hw.free_src_data[OSD2].y_end -
		osd_hw.free_src_data[OSD2].y_start + 1;
	dst_w = osd_hw.free_dst_data[OSD2].x_end -
		osd_hw.free_dst_data[OSD2].x_start + 1;
	dst_h = osd_hw.free_dst_data[OSD2].y_end -
		osd_hw.free_dst_data[OSD2].y_start + 1;
    #ifdef CONFIG_AMLOGIC_SUPERSCALER
	/* super scaler mode */
	if (osd_hw.free_scale_mode[OSD2] & 0x2) {
		if (osd_hw.free_scale_enable[OSD2])
			osd_super_scale_enable(OSD2);
		else
			osd_super_scale_disable();
		remove_from_update_list(OSD2, DISP_FREESCALE_ENABLE);
		return;
    #endif
	/* config osd sc control reg */
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD2]) {
		/* enable osd scaler */
		if (osd_hw.free_scale_mode[OSD2] & 0x1) {
			data32 |= 1;      /* select osd2 input */
			data32 |= 1 << 2; /* enable osd scaler */
			data32 |= 1 << 3; /* enable osd scaler path */
			VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, data32);
		}
	} else {
		/* disable osd scaler path */
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SC_CTRL0, 0);
	}
	hf_phase_step = (src_w << 18) / dst_w;
	hf_phase_step = (hf_phase_step << 6);
	vf_phase_step = (src_h << 20) / dst_h;
	if (osd_hw.field_out_en)   /* interface output */
		bot_ini_phase = ((vf_phase_step / 2) >> 4);
	else
		bot_ini_phase = 0;
	vf_phase_step = (vf_phase_step << 4);
	/* config osd scaler in/out hv size */
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD2]) {
		data32 = (((src_h - 1) & 0x1fff)
				| ((src_w - 1) & 0x1fff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCI_WH_M1, data32);
		data32 = ((osd_hw.free_dst_data[OSD2].x_end & 0xfff) |
			  (osd_hw.free_dst_data[OSD2].x_start & 0xfff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_H_START_END, data32);
		data32 = ((osd_hw.free_dst_data[OSD2].y_end & 0xfff) |
			  (osd_hw.free_dst_data[OSD2].y_start & 0xfff) << 16);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_SCO_V_START_END, data32);
	}
	data32 = 0x0;
	if (osd_hw.free_scale[OSD2].h_enable) {
		data32 |= (hf_bank_len & 0x7)
			| ((hsc_ini_rcv_num & 0xf) << 3)
			| ((hsc_ini_rpt_p0_num & 0x3) << 8);
		data32 |= 1 << 22;
	}
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_HSC_CTRL0, data32);
	data32 = 0x0;
	if (osd_hw.free_scale[OSD2].v_enable) {
		data32 |= (vf_bank_len & 0x7)
			| ((vsc_ini_rcv_num & 0xf) << 3)
			| ((vsc_ini_rpt_p0_num & 0x3) << 8);
		if (osd_hw.field_out_en)   /* interface output */
			data32 |= ((vsc_bot_rcv_num & 0xf) << 11)
				| ((vsc_bot_rpt_p0_num & 0x3) << 16)
				| (1 << 23);
		if (osd_hw.scale_workaround)
			data32 |= 1 << 21;
		data32 |= 1 << 24;
	}
	VSYNCOSD_WR_MPEG_REG(VPP_OSD_VSC_CTRL0, data32);
	data32 = 0x0;
	if (osd_hw.free_scale_enable[OSD2]) {
		data32 |= (bot_ini_phase & 0xffff) << 16;
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_HSC_PHASE_STEP,
				hf_phase_step, 0, 28);
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_HSC_INI_PHASE, 0, 0, 16);
		VSYNCOSD_WR_MPEG_REG_BITS(VPP_OSD_VSC_PHASE_STEP,
				vf_phase_step, 0, 28);
		VSYNCOSD_WR_MPEG_REG(VPP_OSD_VSC_INI_PHASE, data32);
	}
	remove_from_update_list(OSD2, DISP_FREESCALE_ENABLE);
}

static void osd2_update_coef(void)
{
	int i;
	bool need_update_coef = false;
	int hf_coef_wren = 1;
	int vf_coef_wren = 1;
	int *hf_coef, *vf_coef;

	if (osd_hw.scale_workaround) {
		if (use_v_filter_mode != 3) {
			use_v_filter_mode = 3;
			need_update_coef = true;
		} else
			need_update_coef = false;
	} else {
		if (use_v_filter_mode != osd_v_filter_mode) {
			use_v_filter_mode = osd_v_filter_mode;
			need_update_coef = true;
		} else
			need_update_coef = false;
	}
	vf_coef = filter_table[use_v_filter_mode];
	if (need_update_coef) {
		if (vf_coef_wren) {
			osd_reg_set_bits(VPP_OSD_SCALE_COEF_IDX, 0x0000, 0, 9);
			for (i = 0; i < 33; i++)
				osd_reg_write(VPP_OSD_SCALE_COEF, vf_coef[i]);
		}
	}
	need_update_coef = false;
	if (use_h_filter_mode != osd_h_filter_mode) {
		use_h_filter_mode = osd_h_filter_mode;
		need_update_coef = true;
	}
	hf_coef = filter_table[use_h_filter_mode];
	if (need_update_coef) {
		if (hf_coef_wren) {
			osd_reg_set_bits(VPP_OSD_SCALE_COEF_IDX, 0x0100, 0, 9);
			for (i = 0; i < 33; i++)
				osd_reg_write(VPP_OSD_SCALE_COEF, hf_coef[i]);
		}
	}
	remove_from_update_list(OSD2, OSD_FREESCALE_COEF);
}

static void osd1_update_color_mode(void)
{
	u32  data32 = 0;

	if (osd_hw.color_info[OSD1] != NULL) {
		data32 = (osd_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= VSYNCOSD_RD_MPEG_REG(VIU_OSD1_BLK0_CFG_W0)
			& 0x30007040;
		data32 |= osd_hw.fb_gem[OSD1].canvas_idx << 16;
		/* if (!osd_hw.rotate[OSD1].on_off) */
		data32 |= OSD_DATA_LITTLE_ENDIAN << 15;
		data32 |= osd_hw.color_info[OSD1]->hw_colormat << 2;
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXTVBB) {
			if (osd_hw.color_info[OSD1]->color_index <
				COLOR_INDEX_YUV_422)
				data32 |= 1 << 7; /* yuv enable */
		}
		/* osd_blk_mode */
		data32 |=  osd_hw.color_info[OSD1]->hw_blkmode << 8;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W0, data32);
		/*
		 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK1_CFG_W0, data32);
		 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK2_CFG_W0, data32);
		 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK3_CFG_W0, data32);
		 */
		if (osd_hw.osd_afbcd[OSD1].enable) {
			/* only the RGBA32 mode */
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_MODE,
				(3 << 24) |
				(4 << 16) | /* hold_line_num */
				(0xe4 << 8) |
				(1 << 6) |
				(1 << 5) |
				(0x15 << 0)); /* pixel_packing_fmt */
			/* TODO: add pixel_packing_fmt setting */
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_CONV_CTRL,
				(osd_hw.osd_afbcd[OSD1].conv_lbuf_len
				& 0xffff));
			/* afbc mode RGBA32 -> RGB24
			 * data32 = (0x1b << 24) |
			 *	(osd_hw.osd_afbcd[OSD1].phy_addr & 0xffffff);
			 * VSYNCOSD_WR_MPEG_REG(
			 *	OSD1_AFBCD_CHROMA_PTR,
			 *	data32);
			 */
			VSYNCOSD_WR_MPEG_REG_BITS(
				OSD1_AFBCD_CHROMA_PTR,
				0xe4, 24, 8);
			/*
			 * data32 = (0xe4 << 24) |
			 *	(osd_hw.osd_afbcd[OSD1].phy_addr & 0xffffff);
			 * VSYNCOSD_WR_MPEG_REG(
			 *	OSD1_AFBCD_CHROMA_PTR,
			 *	data32);
			 */
		}
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
			enum color_index_e idx =
				osd_hw.color_info[OSD1]->color_index;
			if (idx >= COLOR_INDEX_32_BGRX
			    && idx <= COLOR_INDEX_32_XRGB)
				VSYNCOSD_WR_MPEG_REG_BITS(
					VIU_OSD1_CTRL_STAT2,
					0x1ff, 6, 9);
			else
				VSYNCOSD_WR_MPEG_REG_BITS(
					VIU_OSD1_CTRL_STAT2,
					0, 6, 9);
		}
	}
	remove_from_update_list(OSD1, OSD_COLOR_MODE);
}
static void osd2_update_color_mode(void)
{
	u32 data32 = 0;

	if (osd_hw.color_info[OSD2] != NULL) {
		data32 = (osd_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= VSYNCOSD_RD_MPEG_REG(VIU_OSD2_BLK0_CFG_W0)
			& 0x30007040;
		data32 |= osd_hw.fb_gem[OSD2].canvas_idx << 16;
		/* if (!osd_hw.rotate[OSD2].on_off) */
		data32 |= OSD_DATA_LITTLE_ENDIAN << 15;
		data32 |= osd_hw.color_info[OSD2]->hw_colormat << 2;
		if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXTVBB) {
			if (osd_hw.color_info[OSD2]->color_index <
				COLOR_INDEX_YUV_422)
				data32 |= 1 << 7; /* yuv enable */
		}
		/* osd_blk_mode */
		data32 |=  osd_hw.color_info[OSD2]->hw_blkmode << 8;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W0, data32);
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
			enum color_index_e idx =
				osd_hw.color_info[OSD2]->color_index;
			if (idx >= COLOR_INDEX_32_BGRX
			    && idx <= COLOR_INDEX_32_XRGB)
				VSYNCOSD_WR_MPEG_REG_BITS(
					VIU_OSD2_CTRL_STAT2,
					0x1ff, 6, 9);
			else
				VSYNCOSD_WR_MPEG_REG_BITS(
					VIU_OSD2_CTRL_STAT2,
					0, 6, 9);
		}
	}
	remove_from_update_list(OSD2, OSD_COLOR_MODE);
}

static void osd1_update_enable(void)
{
	if (((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)) &&
		(osd_hw.enable[OSD1] == ENABLE)) {
		if ((VSYNCOSD_RD_MPEG_REG(VPU_RDARB_MODE_L1C2) &
			(1 << 16)) == 0) {
			VSYNCOSD_WR_MPEG_REG_BITS(VPU_RDARB_MODE_L1C2,
				1, 16, 1);
		}
	}

	if (osd_hw.enable[OSD1] == ENABLE)
		osd_vpp_misc |= VPP_OSD1_POSTBLEND;
	else
		osd_vpp_misc &= ~VPP_OSD1_POSTBLEND;

	if (osd_hw.enable[OSD1] == ENABLE) {
		VSYNCOSD_SET_MPEG_REG_MASK(
			VIU_OSD1_CTRL_STAT, 1 << 0);
		VSYNCOSD_SET_MPEG_REG_MASK(VPP_MISC,
			VPP_OSD1_POSTBLEND | VPP_POSTBLEND_EN);
		notify_to_amvideo();
	} else {
		notify_to_amvideo();
		VSYNCOSD_CLR_MPEG_REG_MASK(
			VPP_MISC,
			VPP_OSD1_POSTBLEND);
		VSYNCOSD_CLR_MPEG_REG_MASK(
			VIU_OSD1_CTRL_STAT, 1 << 0);

	}
	if ((osd_hw.osd_afbcd[OSD1].enable == ENABLE) &&
		((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
		(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM))) {
		if (osd_hw.enable[OSD1] == ENABLE) {
			if (!osd_afbc_dec_enable &&
					osd_hw.osd_afbcd[OSD1].phy_addr != 0) {
				VSYNCOSD_WR_MPEG_REG(
					OSD1_AFBCD_ENABLE,
					0x8100);
				osd_afbc_dec_enable = 1;
			}
			VSYNCOSD_WR_MPEG_REG_BITS(
				VIU_OSD1_CTRL_STAT2,
				1, 15, 1);

		} else {
			if (osd_afbc_dec_enable) {
				VSYNCOSD_WR_MPEG_REG(
					OSD1_AFBCD_ENABLE,
					0x8000);
				osd_afbc_dec_enable = 0;
			}
			VSYNCOSD_WR_MPEG_REG_BITS(
				VIU_OSD1_CTRL_STAT2,
				0, 15, 1);
		}
		if ((VSYNCOSD_RD_MPEG_REG(VIU_MISC_CTRL1) &
			(0xff << 8)) != 0x9000) {
			VSYNCOSD_WR_MPEG_REG_BITS(
				VIU_MISC_CTRL1,
				0x90, 8, 8);
		}
	}
	remove_from_update_list(OSD1, OSD_ENABLE);
}

static void osd2_update_enable(void)
{
	if (osd_hw.enable[OSD2] == ENABLE)
		osd_vpp_misc |= VPP_OSD2_POSTBLEND;
	else
		osd_vpp_misc &= ~VPP_OSD2_POSTBLEND;

	if (osd_hw.enable[OSD2] == ENABLE) {
		VSYNCOSD_SET_MPEG_REG_MASK(
			VIU_OSD2_CTRL_STAT,
			1 << 0);
		VSYNCOSD_SET_MPEG_REG_MASK(
			VPP_MISC,
			VPP_OSD2_POSTBLEND
			| VPP_POSTBLEND_EN);
		notify_to_amvideo();
	} else {
		notify_to_amvideo();
		VSYNCOSD_CLR_MPEG_REG_MASK(VPP_MISC,
			VPP_OSD2_POSTBLEND);
		VSYNCOSD_CLR_MPEG_REG_MASK(
			VIU_OSD2_CTRL_STAT,
			1 << 0);
	}
	remove_from_update_list(OSD2, OSD_ENABLE);
}

static void osd1_update_disp_osd_reverse(void)
{
	if (osd_hw.osd_reverse[OSD1] == REVERSE_TRUE)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 3, 28, 2);
	else if (osd_hw.osd_reverse[OSD1] == REVERSE_X)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 1, 28, 2);
	else if (osd_hw.osd_reverse[OSD1] == REVERSE_Y)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 2, 28, 2);
	else
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD1_BLK0_CFG_W0, 0, 28, 2);
	remove_from_update_list(OSD1, DISP_OSD_REVERSE);
}

static void osd2_update_disp_osd_reverse(void)
{
	if (osd_hw.osd_reverse[OSD2] == REVERSE_TRUE)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 3, 28, 2);
	else if (osd_hw.osd_reverse[OSD2] == REVERSE_X)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 1, 28, 2);
	else if (osd_hw.osd_reverse[OSD2] == REVERSE_Y)
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 2, 28, 2);
	else
		VSYNCOSD_WR_MPEG_REG_BITS(VIU_OSD2_BLK0_CFG_W0, 0, 28, 2);
	remove_from_update_list(OSD2, DISP_OSD_REVERSE);
}
static void osd1_update_disp_osd_rotate(void)
{
	remove_from_update_list(OSD1, DISP_OSD_ROTATE);
}

static void osd2_update_disp_osd_rotate(void)
{
	remove_from_update_list(OSD2, DISP_OSD_ROTATE);
}


static void osd1_update_color_key(void)
{
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_TCOLOR_AG0, osd_hw.color_key[OSD1]);
	remove_from_update_list(OSD1, OSD_COLOR_KEY);
}

static void osd2_update_color_key(void)
{
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_TCOLOR_AG0, osd_hw.color_key[OSD2]);
	remove_from_update_list(OSD2, OSD_COLOR_KEY);
}

static void osd1_update_color_key_enable(void)
{
	u32  data32;

	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD1_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_hw.color_key_enable[OSD1] << 6);
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W0, data32);
	/*
	 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK1_CFG_W0, data32);
	 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK2_CFG_W0, data32);
	 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK3_CFG_W0, data32);
	 */
	remove_from_update_list(OSD1, OSD_COLOR_KEY_ENABLE);
}

static void osd2_update_color_key_enable(void)
{
	u32  data32;

	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD2_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_hw.color_key_enable[OSD2] << 6);
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W0, data32);
	remove_from_update_list(OSD2, OSD_COLOR_KEY_ENABLE);
}
static void osd1_update_gbl_alpha(void)
{
	u32  data32;

	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD1_CTRL_STAT);
	data32 &= ~(0x1ff << 12);
	data32 |= osd_hw.gbl_alpha[OSD1] << 12;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_CTRL_STAT, data32);
	remove_from_update_list(OSD1, OSD_GBL_ALPHA);
}
static void osd2_update_gbl_alpha(void)
{
	u32  data32;

	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD2_CTRL_STAT);
	data32 &= ~(0x1ff << 12);
	data32 |= osd_hw.gbl_alpha[OSD2] << 12;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_CTRL_STAT, data32);
	remove_from_update_list(OSD2, OSD_GBL_ALPHA);
}
static void osd2_update_order(void)
{
	switch (osd_hw.order) {
	case  OSD_ORDER_01:
#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
		osd_reg_clr_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
#endif
		osd_vpp_misc &= ~(VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	case  OSD_ORDER_10:
#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
		osd_reg_set_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
#endif
		osd_vpp_misc |= (VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	default:
		break;
	}
	notify_to_amvideo();
	remove_from_update_list(OSD2, OSD_CHANGE_ORDER);
}
static void osd1_update_order(void)
{
	switch (osd_hw.order) {
	case  OSD_ORDER_01:
#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
		osd_reg_clr_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
#endif
		osd_vpp_misc &= ~(VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	case  OSD_ORDER_10:
#ifndef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
		osd_reg_set_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
#endif
		osd_vpp_misc |= (VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	default:
		break;
	}
	notify_to_amvideo();
	remove_from_update_list(OSD1, OSD_CHANGE_ORDER);
}

#if 0
static void osd_block_update_disp_geometry(u32 index)
{
	u32 data32;
	u32 data_w1, data_w2, data_w3, data_w4;
	u32 coef[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1} };
	u32 xoff, yoff;
	u32 i;
	u32 block_num;

	block_num = 4;
	switch (osd_hw.block_mode[index] & HW_OSD_BLOCK_LAYOUT_MASK) {
	case HW_OSD_BLOCK_LAYOUT_HORIZONTAL:
		yoff = ((osd_hw.pandata[index].y_end & 0x1fff) -
			(osd_hw.pandata[index].y_start & 0x1fff) + 1) >> 2;
		data_w1 = (osd_hw.pandata[index].x_start & 0x1fff) |
			  (osd_hw.pandata[index].x_end & 0x1fff) << 16;
		data_w3 = (osd_hw.dispdata[index].x_start & 0xfff) |
			  (osd_hw.dispdata[index].x_end & 0xfff) << 16;
		for (i = 0; i < block_num; i++) {
			if (i == 3) {
				data_w2 = ((osd_hw.pandata[index].y_start
					+ yoff * i) & 0x1fff)
					  | (osd_hw.pandata[index].y_end
							& 0x1fff) << 16;
				data_w4 = ((osd_hw.dispdata[index].y_start
					+ yoff * i) & 0xfff)
					  | (osd_hw.dispdata[index].y_end
							& 0xfff) << 16;
			} else {
				data_w2 = ((osd_hw.pandata[index].y_start
					+ yoff * i) & 0x1fff)
					  | ((osd_hw.pandata[index].y_start
					+ yoff * (i + 1) - 1) & 0x1fff) << 16;
				data_w4 = ((osd_hw.dispdata[index].y_start
					+ yoff * i) & 0xfff)
					  | ((osd_hw.dispdata[index].y_start
					+ yoff * (i + 1) - 1) & 0xfff) << 16;
			}
			if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) |
					((((((data32 >> 16) & 0xfff) + 1) >> 1)
						- 1) << 16);
			}
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);
			osd_hw.block_windows[index][i << 1] = data_w1;
			osd_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_VERTICAL:
		xoff = ((osd_hw.pandata[index].x_end & 0x1fff)
			- (osd_hw.pandata[index].x_start & 0x1fff) + 1) >> 2;
		data_w2 = (osd_hw.pandata[index].y_start & 0x1fff) |
			  (osd_hw.pandata[index].y_end & 0x1fff) << 16;
		data_w4 = (osd_hw.dispdata[index].y_start & 0xfff) |
			  (osd_hw.dispdata[index].y_end & 0xfff) << 16;
		if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = data_w4;
			data_w4 = ((data32 & 0xfff) >> 1)
				| ((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1)
					<< 16);
		}
		for (i = 0; i < block_num; i++) {
			data_w1 = ((osd_hw.pandata[index].x_start + xoff * i)
					& 0x1fff)
				  | ((osd_hw.pandata[index].x_start
					+ xoff * (i + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_hw.dispdata[index].x_start + xoff * i)
					& 0xfff)
				  | ((osd_hw.dispdata[index].x_start
					+ xoff * (i + 1) - 1) & 0xfff) << 16;
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);
			osd_hw.block_windows[index][i << 1] = data_w1;
			osd_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_GRID:
		xoff = ((osd_hw.pandata[index].x_end & 0x1fff) -
			(osd_hw.pandata[index].x_start & 0x1fff) + 1) >> 1;
		yoff = ((osd_hw.pandata[index].y_end & 0x1fff) -
			(osd_hw.pandata[index].y_start & 0x1fff) + 1) >> 1;
		for (i = 0; i < block_num; i++) {
			data_w1 = ((osd_hw.pandata[index].x_start
				+ xoff * coef[i][0]) & 0x1fff)
				  | ((osd_hw.pandata[index].x_start
				+ xoff * (coef[i][0] + 1) - 1) & 0x1fff) << 16;
			data_w2 = ((osd_hw.pandata[index].y_start
				+ yoff * coef[i][1]) & 0x1fff)
				  | ((osd_hw.pandata[index].y_start
				+ yoff * (coef[i][1] + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_hw.dispdata[index].x_start
				+ xoff * coef[i][0]) & 0xfff)
				  | ((osd_hw.dispdata[index].x_start
				+ xoff * (coef[i][0] + 1) - 1) & 0xfff) << 16;
			data_w4 = ((osd_hw.dispdata[index].y_start
				+ yoff * coef[i][1]) & 0xfff)
				  | ((osd_hw.dispdata[index].y_start
				+ yoff * (coef[i][1] + 1) - 1) & 0xfff) << 16;
			if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1)
					| ((((((data32 >> 16) & 0xfff)
						+ 1) >> 1) - 1) << 16);
			}
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);
			osd_hw.block_windows[index][i << 1] = data_w1;
			osd_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_CUSTOMER:
		for (i = 0; i < block_num; i++) {
			if (((osd_hw.block_windows[index][i << 1] >> 16)
						& 0x1fff) >
			    osd_hw.pandata[index].x_end) {
				osd_hw.block_windows[index][i << 1] =
					(osd_hw.block_windows[index][i << 1]
						& 0x1fff)
					| ((osd_hw.pandata[index].x_end
						& 0x1fff) << 16);
			}
			data_w1 = osd_hw.block_windows[index][i << 1]
					& 0x1fff1fff;
			data_w2 = ((osd_hw.pandata[index].y_start & 0x1fff) +
				   (osd_hw.block_windows[index][(i << 1) + 1]
					& 0x1fff))
				  | (((osd_hw.pandata[index].y_start & 0x1fff)
					<< 16)
				+ (osd_hw.block_windows[index][(i << 1) + 1]
					& 0x1fff0000));
			data_w3 = (osd_hw.dispdata[index].x_start
					+ (data_w1 & 0xfff))
				  | (((osd_hw.dispdata[index].x_start & 0xfff)
					<< 16) + (data_w1 & 0xfff0000));
			data_w4 = (osd_hw.dispdata[index].y_start
				+ (osd_hw.block_windows[index][(i << 1) + 1]
					& 0xfff))
				  | (((osd_hw.dispdata[index].y_start & 0xfff)
					<< 16)
				+ (osd_hw.block_windows[index][(i << 1) + 1]
					& 0xfff0000));
			if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1)
					| ((((((data32 >> 16) & 0xfff) + 1)
						>> 1) - 1) << 16);
			}
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);
		}
		break;
	default:
		osd_log_err("ERROR block_mode: 0x%x\n",
				osd_hw.block_mode[index]);
		break;
	}
}
#endif

static void osd1_2x_scale_update_geometry(void)
{
	u32 data32;

	data32 = (osd_hw.scaledata[OSD1].x_start & 0x1fff) |
		 (osd_hw.scaledata[OSD1].x_end & 0x1fff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1, data32);

	if (osd_hw.osd_afbcd[OSD1].enable) {
		data32 = (osd_hw.scaledata[OSD1].x_end & 0x1fff) |
			(osd_hw.scaledata[OSD1].x_start & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_HSCOPE, data32);
		data32 = (osd_hw.scaledata[OSD1].y_end & 0x1fff) |
			(osd_hw.scaledata[OSD1].y_start & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_VSCOPE, data32);
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_HDR_PTR,
			osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_FRAME_PTR,
			osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
		data32 = (0xe4 << 24) |
			(osd_hw.osd_afbcd[OSD1].phy_addr & 0xffffff);
		VSYNCOSD_WR_MPEG_REG(
			OSD1_AFBCD_CHROMA_PTR,
			data32);
	}

	data32 = ((osd_hw.scaledata[OSD1].y_start
			+ osd_hw.pandata[OSD1].y_start) & 0x1fff)
		 | ((osd_hw.scaledata[OSD1].y_end
			+ osd_hw.pandata[OSD1].y_start) & 0x1fff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2, data32);
	/* adjust display x-axis */
	if (osd_hw.scale[OSD1].h_enable) {
		data32 = (osd_hw.dispdata[OSD1].x_start & 0xfff)
			 | ((osd_hw.dispdata[OSD1].x_start
				+ (osd_hw.scaledata[OSD1].x_end
				- osd_hw.scaledata[OSD1].x_start) * 2 + 1)
					 & 0xfff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3, data32);
		if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = ((osd_hw.dispdata[OSD1].y_start >> 1) & 0xfff)
				 | (((((osd_hw.dispdata[OSD1].y_start
					+ (osd_hw.scaledata[OSD1].y_end
					- osd_hw.scaledata[OSD1].y_start) * 2)
					+ 1) >> 1) - 1) & 0xfff) << 16;
		} else {
			data32 = (osd_hw.dispdata[OSD1].y_start & 0xfff)
				 | (((osd_hw.dispdata[OSD1].y_start
					+ (osd_hw.scaledata[OSD1].y_end
					- osd_hw.scaledata[OSD1].y_start) * 2))
						 & 0xfff) << 16;
		}
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4, data32);
	}
	/* adjust display y-axis */
	if (osd_hw.scale[OSD1].v_enable) {
		data32 = (osd_hw.dispdata[OSD1].x_start & 0xfff)
			 | ((osd_hw.dispdata[OSD1].x_start
				+ (osd_hw.scaledata[OSD1].x_end
				- osd_hw.scaledata[OSD1].x_start) * 2 + 1)
					 & 0xfff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3, data32);
		if (osd_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = ((osd_hw.dispdata[OSD1].y_start >> 1) & 0xfff)
				 | (((((osd_hw.dispdata[OSD1].y_start
					+ (osd_hw.scaledata[OSD1].y_end
					- osd_hw.scaledata[OSD1].y_start) * 2)
					+ 1) >> 1) - 1) & 0xfff) << 16;
		} else {
			data32 = (osd_hw.dispdata[OSD1].y_start & 0xfff)
				 | (((osd_hw.dispdata[OSD1].y_start
					+ (osd_hw.scaledata[OSD1].y_end
					- osd_hw.scaledata[OSD1].y_start) * 2))
						 & 0xfff) << 16;
		}
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4, data32);
	}
}

static void osd1_basic_update_disp_geometry(void)
{
	u32 data32;

	data32 = (osd_hw.dispdata[OSD1].x_start & 0xfff)
		| (osd_hw.dispdata[OSD1].x_end & 0xfff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W3, data32);
	if (osd_hw.scan_mode == SCAN_MODE_INTERLACE)
		data32 = ((osd_hw.dispdata[OSD1].y_start >> 1) & 0xfff)
			| ((((osd_hw.dispdata[OSD1].y_end + 1)
			>> 1) - 1) & 0xfff) << 16;
	else
		data32 = (osd_hw.dispdata[OSD1].y_start & 0xfff)
			| (osd_hw.dispdata[OSD1].y_end
			& 0xfff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W4, data32);

	if (osd_hw.osd_afbcd[OSD1].enable)
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_SIZE_IN,
			((osd_hw.osd_afbcd[OSD1].frame_width
			& 0xffff) << 16) |
			((osd_hw.osd_afbcd[OSD1].frame_height
			& 0xffff) << 0));

	/* enable osd 2x scale */
	if (osd_hw.scale[OSD1].h_enable || osd_hw.scale[OSD1].v_enable) {
		osd1_2x_scale_update_geometry();
	} else if (osd_hw.free_scale_enable[OSD1]
		   && (osd_hw.free_src_data[OSD1].x_end > 0)
		   && (osd_hw.free_src_data[OSD1].y_end > 0)) {
		/* enable osd free scale */
		data32 = (osd_hw.free_src_data[OSD1].x_start & 0x1fff) |
			 (osd_hw.free_src_data[OSD1].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1, data32);
		if (osd_hw.osd_afbcd[OSD1].enable) {
			data32 =
				(osd_hw.free_src_data[OSD1].x_end & 0x1fff) |
				(osd_hw.free_src_data[OSD1].x_start & 0x1fff)
				<< 16;
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_HSCOPE, data32);
			data32 =
				(osd_hw.free_src_data[OSD1].y_end & 0x1fff) |
				(osd_hw.free_src_data[OSD1].y_start & 0x1fff)
				<< 16;
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_VSCOPE, data32);
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_HDR_PTR,
				osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_FRAME_PTR,
				osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
			data32 = (0xe4 << 24) |
				(osd_hw.osd_afbcd[OSD1].phy_addr & 0xffffff);
			VSYNCOSD_WR_MPEG_REG(
				OSD1_AFBCD_CHROMA_PTR,
				data32);
		}
		data32 = ((osd_hw.free_src_data[OSD1].y_start
			+ osd_hw.pandata[OSD1].y_start) & 0x1fff)
			| ((osd_hw.free_src_data[OSD1].y_end
			+ osd_hw.pandata[OSD1].y_start) & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2, data32);
	} else {
		/* normal mode */
		data32 = (osd_hw.pandata[OSD1].x_start & 0x1fff)
			| (osd_hw.pandata[OSD1].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1, data32);
		if (osd_hw.osd_afbcd[OSD1].enable) {
			u32 virtual_y_start, virtual_y_end;

			data32 = (osd_hw.pandata[OSD1].x_end & 0x1fff)
				| (osd_hw.pandata[OSD1].x_start & 0x1fff) << 16;
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_HSCOPE, data32);
			virtual_y_start = osd_hw.pandata[OSD1].y_start %
				osd_hw.osd_afbcd[OSD1].frame_height;
			virtual_y_end = osd_hw.pandata[OSD1].y_end -
				osd_hw.pandata[OSD1].y_start + virtual_y_start;
			data32 = (virtual_y_end & 0x1fff) |
				 (virtual_y_start & 0x1fff) << 16;
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_PIXEL_VSCOPE, data32);
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_HDR_PTR,
				osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
			VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_FRAME_PTR,
				osd_hw.osd_afbcd[OSD1].phy_addr >> 4);
			data32 = (0xe4 << 24) |
				(osd_hw.osd_afbcd[OSD1].phy_addr & 0xffffff);
			VSYNCOSD_WR_MPEG_REG(
				OSD1_AFBCD_CHROMA_PTR,
				data32);
		}
		data32 = (osd_hw.pandata[OSD1].y_start & 0x1fff)
			| (osd_hw.pandata[OSD1].y_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W2, data32);
	}

	if (osd_hw.osd_afbcd[OSD1].enable &&
		!osd_afbc_dec_enable &&
		osd_hw.osd_afbcd[OSD1].phy_addr != 0) {
		VSYNCOSD_WR_MPEG_REG(OSD1_AFBCD_ENABLE, 0x8100);
		osd_afbc_dec_enable = 1;
	}
	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD1_CTRL_STAT);
	data32 &= ~0x1ff00e;
	data32 |= osd_hw.gbl_alpha[OSD1] << 12;
	/* data32 |= HW_OSD_BLOCK_ENABLE_0; */
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_CTRL_STAT, data32);
}

static void osd1_update_disp_geometry(void)
{
	if ((osd_hw.block_mode[OSD1]) &&
		(get_cpu_type() <= MESON_CPU_MAJOR_ID_GXBB)) {
		/* multi block */
		/*
		 * osd_block_update_disp_geometry(OSD1);
		 * data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD1_CTRL_STAT);
		 * data32 &= 0xfffffff0;
		 * data32 |= (osd_hw.block_mode[OSD1]
		 * & HW_OSD_BLOCK_ENABLE_MASK);
		 * VSYNCOSD_WR_MPEG_REG(VIU_OSD1_CTRL_STAT, data32);
		 */
		osd_log_info(
			"osd1_update_disp_geometry: not support block mode\n");
	} else {
		/* single block */
		osd1_basic_update_disp_geometry();
	}
	remove_from_update_list(OSD1, DISP_GEOMETRY);
}

static void osd2_update_disp_geometry(void)
{
	u32 data32;

	data32 = (osd_hw.dispdata[OSD2].x_start & 0xfff)
		| (osd_hw.dispdata[OSD2].x_end & 0xfff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W3, data32);
	if (osd_hw.scan_mode == SCAN_MODE_INTERLACE)
		data32 = (osd_hw.dispdata[OSD2].y_start & 0xfff)
			| ((((osd_hw.dispdata[OSD2].y_end + 1
			- osd_hw.dispdata[OSD2].y_start) >> 1)
			+ osd_hw.dispdata[OSD2].y_start - 1)
				& 0xfff) << 16;
	else
		data32 = (osd_hw.dispdata[OSD2].y_start & 0xfff)
			| (osd_hw.dispdata[OSD2].y_end & 0xfff) << 16;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W4, data32);
	if (osd_hw.scale[OSD2].h_enable || osd_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
		data32 = (osd_hw.pandata[OSD2].x_start & 0x1fff)
			| (osd_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_hw.pandata[OSD2].y_start & 0x1fff)
			| (osd_hw.pandata[OSD2].y_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W2, data32);
#else
		data32 = (osd_hw.scaledata[OSD2].x_start & 0x1fff) |
			 (osd_hw.scaledata[OSD2].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
		data32 = ((osd_hw.scaledata[OSD2].y_start
				+ osd_hw.pandata[OSD2].y_start) & 0x1fff)
			 | ((osd_hw.scaledata[OSD2].y_end
				+ osd_hw.pandata[OSD2].y_start) & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W2, data32);
#endif
	} else if (osd_hw.free_scale_enable[OSD2]
		   && (osd_hw.free_src_data[OSD2].x_end > 0)
		   && (osd_hw.free_src_data[OSD2].y_end > 0)) {
		/* enable osd free scale */
		data32 = (osd_hw.free_src_data[OSD2].x_start & 0x1fff)
			| (osd_hw.free_src_data[OSD2].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
		data32 = ((osd_hw.free_src_data[OSD2].y_start
			+ osd_hw.pandata[OSD2].y_start) & 0x1fff)
			| ((osd_hw.free_src_data[OSD2].y_end
			+ osd_hw.pandata[OSD2].y_start) & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W2, data32);
	} else {
		data32 = (osd_hw.pandata[OSD2].x_start & 0x1fff)
			| (osd_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_hw.pandata[OSD2].y_start & 0x1fff)
			| (osd_hw.pandata[OSD2].y_end & 0x1fff) << 16;
		VSYNCOSD_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W2, data32);
	}
	data32 = VSYNCOSD_RD_MPEG_REG(VIU_OSD2_CTRL_STAT);
	data32 &= ~0x1ff000;
	data32 |= osd_hw.gbl_alpha[OSD2] << 12;
	/* data32 |= HW_OSD_BLOCK_ENABLE_0; */
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_CTRL_STAT, data32);
	remove_from_update_list(OSD2, DISP_GEOMETRY);
}

static void osd1_update_disp_3d_mode(void)
{
	/*step 1 . set pan data */
	/* only called by vsync irq or rdma irq */
	u32  data32;

	if (osd_hw.mode_3d[OSD1].left_right == OSD_LEFT) {
		data32 = (osd_hw.mode_3d[OSD1].l_start & 0x1fff)
			| (osd_hw.mode_3d[OSD1].l_end & 0x1fff) << 16;
		VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_hw.mode_3d[OSD1].r_start & 0x1fff)
			| (osd_hw.mode_3d[OSD1].r_end & 0x1fff) << 16;
		VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD1_BLK0_CFG_W1, data32);
	}
	osd_hw.mode_3d[OSD1].left_right ^= 1;
}
static void osd2_update_disp_3d_mode(void)
{
	/* only called by vsync irq or rdma irq */
	u32  data32;

	if (osd_hw.mode_3d[OSD2].left_right == OSD_LEFT) {
		data32 = (osd_hw.mode_3d[OSD2].l_start & 0x1fff)
			| (osd_hw.mode_3d[OSD2].l_end & 0x1fff) << 16;
		VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_hw.mode_3d[OSD2].r_start & 0x1fff)
			| (osd_hw.mode_3d[OSD2].r_end & 0x1fff) << 16;
		VSYNCOSD_IRQ_WR_MPEG_REG(VIU_OSD2_BLK0_CFG_W1, data32);
	}
	osd_hw.mode_3d[OSD2].left_right ^= 1;
}

static void osd1_update_fifo(void)
{
	u32 data32;

	data32 = osd_hw.urgent[OSD1] & 1;
	data32 |= 4 << 5; /* hold_fifo_lines */
	/* burst_len_sel: 3=64 */
	data32 |= 3  << 10;
	/* fifo_depth_val: 32*8=256 */
	data32 |= 32 << 12;
	/* if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) */
	/*
	 * bit 23:22, fifo_ctrl
	 * 00 : for 1 word in 1 burst
	 * 01 : for 2 words in 1 burst
	 * 10 : for 4 words in 1 burst
	 * 11 : reserved
	 */
	data32 |= 2 << 22;
	/* bit 28:24, fifo_lim */
	data32 |= 2 << 24;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD1_FIFO_CTRL_STAT, data32);
	remove_from_update_list(OSD1, OSD_FIFO);
}

static void osd2_update_fifo(void)
{
	u32 data32;

	data32 = osd_hw.urgent[OSD2] & 1;
	data32 |= 4 << 5; /* hold_fifo_lines */
	/* burst_len_sel: 3=64 */
	data32 |= 3  << 10;
	/* fifo_depth_val: 32*8=256 */
	data32 |= 32 << 12;
	/* if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) */
	/*
	 * bit 23:22, fifo_ctrl
	 * 00 : for 1 word in 1 burst
	 * 01 : for 2 words in 1 burst
	 * 10 : for 4 words in 1 burst
	 * 11 : reserved
	 */
	data32 |= 2 << 22;
	/* bit 28:24, fifo_lim */
	data32 |= 2 << 24;
	VSYNCOSD_WR_MPEG_REG(VIU_OSD2_FIFO_CTRL_STAT, data32);
	remove_from_update_list(OSD2, OSD_FIFO);
}

void osd_init_scan_mode(void)
{
#define	VOUT_ENCI	1
#define	VOUT_ENCP	2
#define	VOUT_ENCT	3
	unsigned int output_type = 0;

	output_type = osd_reg_read(VPU_VIU_VENC_MUX_CTRL) & 0x3;
	osd_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	switch (output_type) {
	case VOUT_ENCP:
		if (osd_reg_read(ENCP_VIDEO_MODE) & (1 << 12)) /* 1080i */
			osd_hw.scan_mode = SCAN_MODE_INTERLACE;
		break;
	case VOUT_ENCI:
		if (osd_reg_read(ENCI_VIDEO_EN) & 1)
			osd_hw.scan_mode = SCAN_MODE_INTERLACE;
		break;
	}
}

void osd_init_hw(u32 logo_loaded)
{
	u32 group, idx, data32;
	int err_num = 0;

	osd_vpu_power_on();

	if (get_cpu_type() ==
		MESON_CPU_MAJOR_ID_GXTVBB)
		backup_regs_init(HW_RESET_AFBCD_REGS);
	else if (get_cpu_type() ==
		MESON_CPU_MAJOR_ID_GXM)
		backup_regs_init(HW_RESET_OSD1_REGS);
	else if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
		&& (get_cpu_type() <= MESON_CPU_MAJOR_ID_TXL))
		backup_regs_init(HW_RESET_OSD1_REGS);
	else
		backup_regs_init(HW_RESET_NONE);

	recovery_regs_init();
	for (group = 0; group < HW_OSD_COUNT; group++)
		for (idx = 0; idx < HW_REG_INDEX_MAX; idx++)
			osd_hw.reg[group][idx].update_func =
				hw_func_array[group][idx];
	osd_hw.updated[OSD1] = 0;
	osd_hw.updated[OSD2] = 0;
	osd_hw.urgent[OSD1] = 1;
	osd_hw.urgent[OSD2] = 1;
#ifdef CONFIG_AMLOGIC_VECM
	osd_hdr_on = false;
#endif
	osd_hw.hw_reset_flag = HW_RESET_NONE;

	/* here we will init default value ,these value only set once . */
	if (!logo_loaded) {
		/* init vpu fifo control register */
		data32 = osd_reg_read(VPP_OFIFO_SIZE);
		if ((get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) ||
			(get_cpu_type() == MESON_CPU_MAJOR_ID_GXM))
			data32 |= 0xfff;
		else
			data32 |= 0x77f;
		osd_reg_write(VPP_OFIFO_SIZE, data32);
		data32 = 0x08080808;
		osd_reg_write(VPP_HOLD_LINES, data32);

		/* init osd fifo control register
		 * set DDR request priority to be urgent
		 */
		data32 = 1;
		data32 |= 4 << 5;  /* hold_fifo_lines */
		/* burst_len_sel: 3=64 */
		data32 |= 3  << 10;
		/* fifo_depth_val: 32*8=256 */
		data32 |= 32 << 12;
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
			/*
			 * bit 23:22, fifo_ctrl
			 * 00 : for 1 word in 1 burst
			 * 01 : for 2 words in 1 burst
			 * 10 : for 4 words in 1 burst
			 * 11 : reserved
			 */
			data32 |= 2 << 22;
			/* bit 28:24, fifo_lim */
			data32 |= 2 << 24;
		}
		osd_reg_write(VIU_OSD1_FIFO_CTRL_STAT, data32);
		osd_reg_write(VIU_OSD2_FIFO_CTRL_STAT, data32);
		osd_reg_set_mask(VPP_MISC, VPP_POSTBLEND_EN);
		osd_reg_clr_mask(VPP_MISC, VPP_PREBLEND_EN);
		osd_vpp_misc =
			osd_reg_read(VPP_MISC) & OSD_RELATIVE_BITS;
		osd_vpp_misc &=
			~(VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND);
		notify_to_amvideo();
		osd_reg_clr_mask(VPP_MISC,
			VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND);
		/* just disable osd to avoid booting hang up */
		data32 = 0x1 << 0;
		data32 |= OSD_GLOBAL_ALPHA_DEF << 12;
		osd_reg_write(VIU_OSD1_CTRL_STAT, data32);
		osd_reg_write(VIU_OSD2_CTRL_STAT, data32);
	}
	osd_vpp_misc =
		osd_reg_read(VPP_MISC) & OSD_RELATIVE_BITS;
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	osd_vpp_misc |= (VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	notify_to_amvideo();
	osd_reg_set_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_hw.order = OSD_ORDER_10;
#else
	osd_vpp_misc &= ~(VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	notify_to_amvideo();
	osd_reg_clr_mask(VPP_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_hw.order = OSD_ORDER_01;
#endif
	osd_hw.enable[OSD2] = osd_hw.enable[OSD1] = DISABLE;
	osd_hw.fb_gem[OSD1].canvas_idx = OSD1_CANVAS_INDEX;
	osd_hw.fb_gem[OSD2].canvas_idx = OSD2_CANVAS_INDEX;
	osd_hw.gbl_alpha[OSD1] = OSD_GLOBAL_ALPHA_DEF;
	osd_hw.gbl_alpha[OSD2] = OSD_GLOBAL_ALPHA_DEF;
	osd_hw.color_info[OSD1] = NULL;
	osd_hw.color_info[OSD2] = NULL;
	osd_hw.color_key[OSD1] = osd_hw.color_key[OSD2] = 0xffffffff;
	osd_hw.free_scale_enable[OSD1] = osd_hw.free_scale_enable[OSD2] = 0;
	osd_hw.scale[OSD1].h_enable = osd_hw.scale[OSD1].v_enable = 0;
	osd_hw.scale[OSD2].h_enable = osd_hw.scale[OSD2].v_enable = 0;
	osd_hw.mode_3d[OSD2].enable = osd_hw.mode_3d[OSD1].enable = 0;
	osd_hw.block_mode[OSD1] = osd_hw.block_mode[OSD2] = 0;
	osd_hw.free_scale[OSD1].h_enable = 0;
	osd_hw.free_scale[OSD1].h_enable = 0;
	osd_hw.free_scale[OSD2].v_enable = 0;
	osd_hw.free_scale[OSD2].v_enable = 0;
	osd_hw.osd_reverse[OSD1] = REVERSE_FALSE;
	osd_hw.osd_reverse[OSD2] = REVERSE_FALSE;
	/*
	 * osd_hw.rotation_pandata[OSD1].x_start = 0;
	 * osd_hw.rotation_pandata[OSD1].y_start = 0;
	 * osd_hw.rotation_pandata[OSD2].x_start = 0;
	 * osd_hw.rotation_pandata[OSD2].y_start = 0;
	 */
	osd_hw.antiflicker_mode = 0;
	osd_hw.color_key_enable[OSD1] = 0;
	osd_hw.color_key_enable[OSD2] = 0;
	osd_hw.free_src_data[OSD1].x_start = 0;
	osd_hw.free_src_data[OSD1].x_end = 0;
	osd_hw.free_src_data[OSD1].y_start = 0;
	osd_hw.free_src_data[OSD1].y_end = 0;
	osd_hw.free_src_data[OSD2].x_start = 0;
	osd_hw.free_src_data[OSD2].x_end = 0;
	osd_hw.free_src_data[OSD2].y_start = 0;
	osd_hw.free_src_data[OSD2].y_end = 0;
	osd_hw.free_scale_mode[OSD1] = 0;
	osd_hw.free_scale_mode[OSD2] = 1;
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXM)
		osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0x002020ff);
	else if (get_cpu_type() ==
		MESON_CPU_MAJOR_ID_GXTVBB)
		osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0xff);
	else
		osd_reg_write(VPP_OSD_SC_DUMMY_DATA, 0x008080ff);
	/* osd_hw.osd_afbcd[OSD1].enable = 0;
	 * osd_hw.osd_afbcd[OSD2].enable = 0;
	 */
	/* memset(osd_hw.rotate, 0, sizeof(struct osd_rotate_s)); */

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
	INIT_LIST_HEAD(&post_fence_list);
	mutex_init(&post_fence_list_lock);
#endif
#ifdef FIQ_VSYNC
	osd_hw.fiq_handle_item.handle = vsync_isr;
	osd_hw.fiq_handle_item.key = (u32)vsync_isr;
	osd_hw.fiq_handle_item.name = "osd_vsync";
	if (register_fiq_bridge_handle(&osd_hw.fiq_handle_item))
#else
	err_num = request_irq(int_viu_vsync, &vsync_isr,
				IRQF_SHARED, "osd-vsync", osd_setup_hw);
	if (err_num)
#endif
		osd_log_err("can't request irq for vsync,err_num=%d\n",
			-err_num);
#ifdef FIQ_VSYNC
	request_fiq(INT_VIU_VSYNC, &osd_fiq_isr);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_rdma_enable(1);
#endif

	osd_init_hw_flag = 1;
}

#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
void osd_cursor_hw(u32 index, s16 x, s16 y, s16 xstart, s16 ystart, u32 osd_w,
		   u32 osd_h)
{
	struct pandata_s disp_tmp;

	if (index != 1)
		return;

	osd_log_dbg2("cursor: x=%d, y=%d, x0=%d, y0=%d, w=%d, h=%d\n",
			x, y, xstart, ystart, osd_w, osd_h);

	if (osd_hw.free_scale_mode[OSD1]) {
		if (osd_hw.free_scale_enable[OSD1])
			memcpy(&disp_tmp, &osd_hw.cursor_dispdata[OSD1],
					sizeof(struct pandata_s));
		else
			memcpy(&disp_tmp, &osd_hw.dispdata[OSD1],
					sizeof(struct pandata_s));
	} else
		memcpy(&disp_tmp, &osd_hw.dispdata[OSD1],
				sizeof(struct pandata_s));
	if (osd_hw.scale[OSD2].h_enable && (osd_hw.scaledata[OSD2].x_start > 0)
	    && (osd_hw.scaledata[OSD2].x_end > 0)) {
		x = x * osd_hw.scaledata[OSD2].x_end /
			osd_hw.scaledata[OSD2].x_start;
		if (osd_hw.scaledata[OSD2].x_end >
				osd_hw.scaledata[OSD2].x_start) {
			disp_tmp.x_start = osd_hw.dispdata[OSD1].x_start *
				osd_hw.scaledata[OSD2].x_end /
				osd_hw.scaledata[OSD2].x_start;
			disp_tmp.x_end = osd_hw.dispdata[OSD1].x_end *
				osd_hw.scaledata[OSD2].x_end /
				osd_hw.scaledata[OSD2].x_start;
		}
	}
	if (osd_hw.scale[OSD2].v_enable && (osd_hw.scaledata[OSD2].y_start > 0)
	    && (osd_hw.scaledata[OSD2].y_end > 0)) {
		y = y * osd_hw.scaledata[OSD2].y_end /
			osd_hw.scaledata[OSD2].y_start;
		if (osd_hw.scaledata[OSD2].y_end >
				osd_hw.scaledata[OSD2].y_start) {
			disp_tmp.y_start = osd_hw.dispdata[OSD1].y_start *
				osd_hw.scaledata[OSD2].y_end /
				osd_hw.scaledata[OSD2].y_start;
			disp_tmp.y_end = osd_hw.dispdata[OSD1].y_end *
				osd_hw.scaledata[OSD2].y_end /
					 osd_hw.scaledata[OSD2].y_start;
		}
	}
	x += xstart;
	y += ystart;
	/*
	 * Use pandata to show a partial cursor when it is at the edge because
	 * the registers can't have negative values and because we need to
	 * manually clip the cursor when it is past the edge.  The edge is
	 * hardcoded to the OSD0 area.
	 */
	osd_hw.dispdata[OSD2].x_start = x;
	osd_hw.dispdata[OSD2].y_start = y;
	if (x <  disp_tmp.x_start) {
		/* if negative position, set osd to 0,y and pan. */
		if ((disp_tmp.x_start - x) < osd_w) {
			osd_hw.pandata[OSD2].x_start = disp_tmp.x_start - x;
			osd_hw.pandata[OSD2].x_end = osd_w - 1;
		}
		osd_hw.dispdata[OSD2].x_start = 0;
	} else {
		osd_hw.pandata[OSD2].x_start = 0;
		if (x + osd_w > disp_tmp.x_end) {
			/*
			 * if past positive edge,
			 * set osd to inside of the edge and pan.
			 */
			if (x < disp_tmp.x_end)
				osd_hw.pandata[OSD2].x_end = disp_tmp.x_end - x;
		} else
			osd_hw.pandata[OSD2].x_end = osd_w - 1;
	}
	if (y < disp_tmp.y_start) {
		if ((disp_tmp.y_start - y) < osd_h) {
			osd_hw.pandata[OSD2].y_start = disp_tmp.y_start - y;
			osd_hw.pandata[OSD2].y_end = osd_h - 1;
		}
		osd_hw.dispdata[OSD2].y_start = 0;
	} else {
		osd_hw.pandata[OSD2].y_start = 0;
		if (y + osd_h > disp_tmp.y_end) {
			if (y < disp_tmp.y_end)
				osd_hw.pandata[OSD2].y_end = disp_tmp.y_end - y;
		} else
			osd_hw.pandata[OSD2].y_end = osd_h - 1;
	}
	osd_hw.dispdata[OSD2].x_end = osd_hw.dispdata[OSD2].x_start +
		osd_hw.pandata[OSD2].x_end - osd_hw.pandata[OSD2].x_start;
	osd_hw.dispdata[OSD2].y_end = osd_hw.dispdata[OSD2].y_start +
		osd_hw.pandata[OSD2].y_end - osd_hw.pandata[OSD2].y_start;
	add_to_update_list(OSD2, OSD_COLOR_MODE);
	add_to_update_list(OSD2, DISP_GEOMETRY);
}
#endif /* CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR */

void  osd_suspend_hw(void)
{
	osd_hw.reg_status_save = osd_reg_read(VPP_MISC) & OSD_RELATIVE_BITS;
	osd_vpp_misc &= ~OSD_RELATIVE_BITS;
	notify_to_amvideo();
	osd_reg_clr_mask(VPP_MISC, OSD_RELATIVE_BITS);
	/* VSYNCOSD_CLR_MPEG_REG_MASK(VPP_MISC, OSD_RELATIVE_BITS); */
	osd_log_info("osd_suspended\n");
}

void osd_resume_hw(void)
{
	if (osd_hw.reg_status_save &
		(VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND))
		osd_hw.reg_status_save |= VPP_POSTBLEND_EN;
	osd_vpp_misc = osd_hw.reg_status_save & OSD_RELATIVE_BITS;
	notify_to_amvideo();
	osd_reg_set_mask(VPP_MISC, osd_hw.reg_status_save);
	/* VSYNCOSD_SET_MPEG_REG_MASK(VPP_MISC, osd_hw.reg_status_save); */
	osd_log_info("osd_resumed\n");
}

void osd_shutdown_hw(void)
{
#ifdef CONFIG_VSYNC_RDMA
	enable_rdma(0);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_rdma_enable(0);
#endif
	pr_info("osd_shutdown\n");
}

#ifdef CONFIG_HIBERNATION
static unsigned int fb0_cfg_w0_save;
static u32 __nosavedata free_scale_enable[HW_OSD_COUNT];

void osd_realdata_save_hw(void)
{
	free_scale_enable[OSD1] = osd_hw.free_scale_enable[OSD1];
	free_scale_enable[OSD2] = osd_hw.free_scale_enable[OSD2];
}

void osd_realdata_restore_hw(void)
{
	osd_hw.free_scale_enable[OSD1] = free_scale_enable[OSD1];
	osd_hw.free_scale_enable[OSD2] = free_scale_enable[OSD2];
}

void  osd_freeze_hw(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	enable_rdma(0);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_rdma_enable(0);
	if (get_backup_reg(VIU_OSD1_BLK0_CFG_W0,
		&fb0_cfg_w0_save) != 0)
		fb0_cfg_w0_save =
			osd_reg_read(VIU_OSD1_BLK0_CFG_W0);
#else
	fb0_cfg_w0_save = osd_reg_read(VIU_OSD1_BLK0_CFG_W0);
#endif
	pr_debug("osd_freezed\n");
}
void osd_thaw_hw(void)
{
	pr_debug("osd_thawed\n");
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_rdma_enable(2);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	enable_rdma(1);
#endif
}
void osd_restore_hw(void)
{
	osd_reg_write(VIU_OSD1_BLK0_CFG_W0, fb0_cfg_w0_save);
#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_VSYNC_RDMA
	osd_rdma_enable(2);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	enable_rdma(1);
#endif
	canvas_config(osd_hw.fb_gem[0].canvas_idx,
			osd_hw.fb_gem[0].addr,
			osd_hw.fb_gem[0].width,
			osd_hw.fb_gem[0].height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	pr_debug("osd_restored\n");
}
#endif

int osd_get_logo_index(void)
{
	return osd_logo_index;
}

void osd_set_logo_index(int index)
{
	osd_logo_index = index;
}

int osd_get_init_hw_flag(void)
{
	return osd_init_hw_flag;
}

void osd_get_hw_para(struct hw_para_s **para)
{
	*para = &osd_hw;
}
