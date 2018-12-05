/*
 * drivers/amlogic/media/osd_ext/osd_hw.c
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
#include <linux/spinlock.h>
#include <linux/irqreturn.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>

/* Android Headers */
#include <sw_sync.h>
#include <sync.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/amports/vframe_receiver.h>
#endif

/* Local Headers */
#include <osd/osd_log.h>
#include <osd/osd_io.h>
#include <osd/osd_reg.h>
#include <osd/osd_canvas.h>
#include <osd/osd_hw.h>


#include "osd_prot.h"
#include "osd_clone.h"
#include "osd_hw_def.h"

#ifdef CONFIG_AML_VSYNC_FIQ_ENABLE
#define FIQ_VSYNC
#endif

static DECLARE_WAIT_QUEUE_HEAD(osd_ext_vsync_wq);
static bool vsync_hit;
static bool osd_ext_vf_need_update;
static struct hw_para_s *osd_hw;

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
/* add sync fence relative varible here. */
/* we will limit all fence relative code in this driver file. */
static int ext_timeline_created;
static struct  sw_sync_timeline *ext_timeline;
static u32 ext_cur_streamline_val;
/* thread control part */
struct kthread_worker ext_buffer_toggle_worker;
struct task_struct *ext_buffer_toggle_thread;
struct kthread_work ext_buffer_toggle_work;
struct list_head ext_post_fence_list;
struct mutex ext_post_fence_list_lock;

static void osd_ext_pan_display_fence(struct osd_fence_map_s *fence_map);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#define PROVIDER_NAME "osd_ext"
static struct vframe_s vf;
static struct vframe_provider_s osd_ext_vf_prov;
static unsigned char osd_ext_vf_prov_init;
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
static int g_vf_visual_width;
static int g_vf_width;
static int g_vf_height;
#endif
static unsigned int filt_coef0[] = { /* bicubic */
	0x00800000,
	0x007f0100,
	0xff7f0200,
	0xfe7f0300,
	0xfd7e0500,
	0xfc7e0600,
	0xfb7d0800,
	0xfb7c0900,
	0xfa7b0b00,
	0xfa7a0dff,
	0xf9790fff,
	0xf97711ff,
	0xf87613ff,
	0xf87416fe,
	0xf87218fe,
	0xf8701afe,
	0xf76f1dfd,
	0xf76d1ffd,
	0xf76b21fd,
	0xf76824fd,
	0xf76627fc,
	0xf76429fc,
	0xf7612cfc,
	0xf75f2ffb,
	0xf75d31fb,
	0xf75a34fb,
	0xf75837fa,
	0xf7553afa,
	0xf8523cfa,
	0xf8503ff9,
	0xf84d42f9,
	0xf84a45f9,
	0xf84848f8
};

static unsigned int filt_coef1[] = { /* 2 point bilinear */
	0x00800000,
	0x007e0200,
	0x007c0400,
	0x007a0600,
	0x00780800,
	0x00760a00,
	0x00740c00,
	0x00720e00,
	0x00701000,
	0x006e1200,
	0x006c1400,
	0x006a1600,
	0x00681800,
	0x00661a00,
	0x00641c00,
	0x00621e00,
	0x00602000,
	0x005e2200,
	0x005c2400,
	0x005a2600,
	0x00582800,
	0x00562a00,
	0x00542c00,
	0x00522e00,
	0x00503000,
	0x004e3200,
	0x004c3400,
	0x004a3600,
	0x00483800,
	0x00463a00,
	0x00443c00,
	0x00423e00,
	0x00404000
};

static unsigned int filt_coef2[] = { /* 2 point bilinear, bank_length == 2 */
	0x80000000,
	0x7e020000,
	0x7c040000,
	0x7a060000,
	0x78080000,
	0x760a0000,
	0x740c0000,
	0x720e0000,
	0x70100000,
	0x6e120000,
	0x6c140000,
	0x6a160000,
	0x68180000,
	0x661a0000,
	0x641c0000,
	0x621e0000,
	0x60200000,
	0x5e220000,
	0x5c240000,
	0x5a260000,
	0x58280000,
	0x562a0000,
	0x542c0000,
	0x522e0000,
	0x50300000,
	0x4e320000,
	0x4c340000,
	0x4a360000,
	0x48380000,
	0x463a0000,
	0x443c0000,
	0x423e0000,
	0x40400000
};

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
static inline int find_ext_buf_num(u32 yres, u32 yoffset)
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

static void osd_ext_toggle_buffer(struct kthread_work *work)
{
	struct osd_fence_map_s *data, *next;
	struct list_head saved_list;

	mutex_lock(&ext_post_fence_list_lock);
	saved_list = ext_post_fence_list;
	list_replace_init(&ext_post_fence_list, &saved_list);
	mutex_unlock(&ext_post_fence_list_lock);

	list_for_each_entry_safe(data, next, &saved_list, list) {
		osd_ext_pan_display_fence(data);

		if ((data->in_fence) && (data->in_fd > 0)) {
			__close_fd(data->files, data->in_fd);
			sync_fence_put(data->in_fence);
		}

		list_del(&data->list);
		kfree(data);
	}
}

static int out_ext_fence_create(int *release_fence_fd, u32 *val, u32 buf_num)
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

	if (!ext_timeline_created) { /* timeline has not been created */
		ext_timeline = sw_sync_timeline_create("osd_ext_timeline");
		ext_cur_streamline_val = 1;

		if (ext_timeline == NULL)
			return -1;

		init_kthread_worker(&ext_buffer_toggle_worker);
		ext_buffer_toggle_thread =
			kthread_run(kthread_worker_fn,
					&ext_buffer_toggle_worker,
					"aml_buf_ext_toggle");
		init_kthread_work(&ext_buffer_toggle_work,
				osd_ext_toggle_buffer);
		ext_timeline_created = 1;
	}

	/* install fence map; first ,the simplest. */
	ext_cur_streamline_val++;
	*val = ext_cur_streamline_val;

	outer_sync_pt = sw_sync_pt_create(ext_timeline, *val);

	if (outer_sync_pt == NULL)
		goto error_ret;

	outer_fence = sync_fence_create("osd_ext_fence_out", outer_sync_pt);
	if (outer_fence == NULL) {
		sync_pt_free(outer_sync_pt); /* free sync point. */
		goto error_ret;
	}

	sync_fence_install(outer_fence, out_fence_fd);
	osd_log_dbg(MODULE_FENCE, "---------------------------------------\n");
	osd_log_dbg(MODULE_FENCE, "return out fence fd:%d\n", out_fence_fd);
	*release_fence_fd = out_fence_fd;
	return out_fence_fd;

error_ret:
	ext_cur_streamline_val--;
	/* pt or fence fail,restore timeline value. */
	osd_log_err("fence obj create fail\n");
	put_unused_fd(out_fence_fd);
	return -1;

}

int osd_ext_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
			 s32 in_fence_fd)
{
	int out_fence_fd = -1;
	int buf_num = 0;
	struct osd_fence_map_s *fence_map = NULL;

	fence_map = kzalloc(sizeof(struct osd_fence_map_s), GFP_KERNEL);
	if (!fence_map)
		return -ENOMEM;

	buf_num = find_ext_buf_num(yres, yoffset);

	mutex_lock(&ext_post_fence_list_lock);
	fence_map->fb_index = index;
	fence_map->buf_num = buf_num;
	fence_map->yoffset = yoffset;
	fence_map->xoffset = xoffset;
	fence_map->yres = yres;
	fence_map->in_fd = in_fence_fd;
	fence_map->in_fence = sync_fence_fdget(in_fence_fd);
	fence_map->files = current->files;

	fence_map->out_fd = out_ext_fence_create(&out_fence_fd, &fence_map->val,
			    buf_num);
	list_add_tail(&fence_map->list, &ext_post_fence_list);
	mutex_unlock(&ext_post_fence_list_lock);

	queue_kthread_work(&ext_buffer_toggle_worker, &ext_buffer_toggle_work);

	return  out_fence_fd;
}

static int osd_ext_wait_buf_ready(struct osd_fence_map_s *fence_map)
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
	if (ret < 0)
		osd_log_err("Sync Fence wait error:%d\n", ret);
	else
		ret = 1;

	return ret;
}
#else
int osd_ext_sync_request(u32 index, u32 yres, u32 xoffset, u32 yoffset,
			 s32 in_fence_fd)
{
	osd_log_err("osd_ext_sync_request not supported\n");
	return -5566;
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
static struct vframe_s *osd_ext_vf_peek(void *arg)
{
	if ((osd_ext_vf_need_update && (vf.width > 0) && (vf.height > 0)))
		return &vf;
	else
		return NULL;
}

static struct vframe_s *osd_ext_vf_get(void *arg)
{
	if (osd_ext_vf_need_update) {
		vf_ext_light_unreg_provider(&osd_ext_vf_prov);
		osd_ext_vf_need_update = false;
		return &vf;
	}

	return NULL;
}

static const struct vframe_operations_s osd_ext_vf_provider = {
	.peek = osd_ext_vf_peek,
	.get = osd_ext_vf_get,
	.put = NULL,
};
#endif


static inline void osd_ext_update_3d_mode(int enable_osd1, int enable_osd2)
{
	if (enable_osd1)
		osd1_update_disp_3d_mode();

	if (enable_osd2)
		osd2_update_disp_3d_mode();
}

static inline void wait_ext_vsync_wakeup(void)
{
	vsync_hit = true;
	wake_up_interruptible(&osd_ext_vsync_wq);
}

static inline void walk_through_update_list(void)
{
	u32 i, j;

	for (i = 0; i < HW_OSD_COUNT; i++) {
		j = 0;
		while (osd_ext_hw.updated[i] && j < 32) {
			if (osd_ext_hw.updated[i] & (1 << j)) {
				osd_ext_hw.reg[i][j].update_func();
				remove_from_update_list(i, j);
			}
			j++;
		}
	}
}

#ifdef FIQ_VSYNC
static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	wait_ext_vsync_wakeup();

	return IRQ_HANDLED;
}
#endif

#ifdef FIQ_VSYNC
static void osd_ext_fiq_isr(void)
#else
static irqreturn_t vsync_isr(int irq, void *dev_id)
#endif
{
#define	VOUT_ENCI	1
#define	VOUT_ENCP	2
#define	VOUT_ENCT	3
	unsigned int fb0_cfg_w0, fb1_cfg_w0;
	unsigned int odd_even;
	unsigned int scan_line_number = 0;
	unsigned char output_type = 0;

	output_type = osd_reg_read(VPU_VIU_VENC_MUX_CTRL) & 0xc;
	osd_ext_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	switch (output_type) {
	case VOUT_ENCP:
		if (osd_reg_read(ENCP_VIDEO_MODE) & (1 << 12)) /* 1080i */
			osd_ext_hw.scan_mode = SCAN_MODE_INTERLACE;

		break;

	case VOUT_ENCI:
		if (osd_reg_read(ENCI_VIDEO_EN) & 1)
			osd_ext_hw.scan_mode = SCAN_MODE_INTERLACE;

		break;
	}


	if (osd_ext_hw.free_scale_enable[OSD1])
		osd_ext_hw.scan_mode = SCAN_MODE_PROGRESSIVE;

	if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
		fb0_cfg_w0 = osd_reg_read(VIU2_OSD1_BLK0_CFG_W0);
		fb1_cfg_w0 = osd_reg_read(VIU2_OSD1_BLK0_CFG_W0 + REG_OFFSET);

		if (osd_reg_read(ENCP_VIDEO_MODE) & (1 << 12)) {
			/* 1080I */
			scan_line_number = ((osd_reg_read(ENCP_INFO_READ))
					& 0x1fff0000) >> 16;
			if ((osd_ext_hw.pandata[OSD1].y_start % 2) == 0) {
				if (scan_line_number >= 562) {
					/* bottom field, odd lines */
					odd_even = 1;
				} else {
					/* top field, even lines */
					odd_even = 0;
				}
			} else {
				if (scan_line_number >= 562) {
					/* top field, even lines */
					odd_even = 0;
				} else {
					/* bottom field, odd lines */
					odd_even = 1;
				}
			}
		} else {
			if ((osd_ext_hw.pandata[OSD1].y_start % 2) == 0)
				odd_even = osd_reg_read(ENCI_INFO_READ) & 1;
			else
				odd_even = !(osd_reg_read(ENCI_INFO_READ) & 1);
		}

		fb0_cfg_w0 &= ~1;
		fb1_cfg_w0 &= ~1;
		fb0_cfg_w0 |= odd_even;
		fb1_cfg_w0 |= odd_even;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W0, fb0_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK1_CFG_W0, fb0_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK2_CFG_W0, fb0_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK3_CFG_W0, fb0_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK1_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK2_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		osd_reg_write(VIU2_OSD1_BLK3_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
	}

	/* go through update list */
	walk_through_update_list();
	osd_ext_update_3d_mode(osd_ext_hw.mode_3d[OSD1].enable,
			       osd_ext_hw.mode_3d[OSD2].enable);

	if (!vsync_hit) {
#ifdef FIQ_VSYNC
		fiq_bridge_pulse_trigger(&osd_ext_hw.fiq_handle_item);
#else
		wait_ext_vsync_wakeup();
#endif
	}

#ifndef FIQ_VSYNC
	return IRQ_HANDLED;
#endif
}

void osd_ext_wait_vsync_hw(void)
{
	vsync_hit = false;

	wait_event_interruptible_timeout(osd_ext_vsync_wq,
		vsync_hit, msecs_to_jiffies(1000));
}

s32 osd_ext_wait_vsync_event(void)
{
	vsync_hit = false;
	wait_event_interruptible_timeout(osd_ext_vsync_wq,
		vsync_hit, msecs_to_jiffies(1000));
	return 0;
}

/* the return stride unit is 128bit(16bytes) */
static u32 line_stride_calc(
		u32 fmt_mode,
		u32 hsize,
		u32 stride_align_32bytes)
{
	u32 line_stride = 0;

	/* 2-bit LUT */
	if (fmt_mode == 0)
		line_stride = ((hsize<<1)+127)>>7;
	/* 4-bit LUT */
	else if (fmt_mode == 1)
		line_stride = ((hsize<<2)+127)>>7;
	/* 8-bit LUT */
	else if (fmt_mode == 2)
		line_stride = ((hsize<<3)+127)>>7;
	/* 4:2:2, 32-bit per 2 pixels */
	else if (fmt_mode == 3)
		line_stride = ((((hsize+1)>>1)<<5)+127)>>7;
	/* 16-bit LUT */
	else if (fmt_mode == 4)
		line_stride = ((hsize<<4)+127)>>7;
	/* 32-bit LUT */
	else if (fmt_mode == 5)
		line_stride = ((hsize<<5)+127)>>7;
	/* 24-bit LUT */
	else if (fmt_mode == 7)
		line_stride = ((hsize<<4)+(hsize<<3)+127)>>7;
	/* need wr ddr is 32bytes aligned */
	if (stride_align_32bytes)
		line_stride = ((line_stride+1)>>1)<<1;
	else
		line_stride = line_stride;
	return line_stride;
}

static void osd_ext_update_phy_addr(u32 index)
{
	u32 line_stride, fmt_mode;

	fmt_mode =
		osd_ext_hw.color_info[index]->hw_colormat << 2;
	line_stride = line_stride_calc(fmt_mode,
		osd_ext_hw.fb_gem[index].width, 1);
	VSYNCOSD_WR_MPEG_REG(
		VIU_OSD1_BLK1_CFG_W4,
		osd_ext_hw.fb_gem[index].addr);
	VSYNCOSD_WR_MPEG_REG_BITS(
		VIU_OSD1_BLK2_CFG_W4,
		line_stride,
		0, 12);
}


void osd_ext_set_gbl_alpha_hw(u32 index, u32 gbl_alpha)
{
	if (osd_ext_hw.gbl_alpha[index] != gbl_alpha) {

		osd_ext_hw.gbl_alpha[index] = gbl_alpha;
		add_to_update_list(index, OSD_GBL_ALPHA);

		osd_ext_wait_vsync_hw();
	}
}

u32 osd_ext_get_gbl_alpha_hw(u32 index)
{
	return osd_ext_hw.gbl_alpha[index];
}

void osd_ext_set_color_key_hw(u32 index, u32 color_index, u32 colorkey)
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

	if (osd_ext_hw.color_key[index] != data32) {
		osd_ext_hw.color_key[index] = data32;
		osd_log_info("bpp:%d--r:0x%x g:0x%x b:0x%x ,a:0x%x\n",
			     color_index, r, g, b, a);
		add_to_update_list(index, OSD_COLOR_KEY);

		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_srckey_enable_hw(u32 index, u8 enable)
{
	if (enable != osd_ext_hw.color_key_enable[index]) {
		osd_ext_hw.color_key_enable[index] = enable;
		add_to_update_list(index, OSD_COLOR_KEY_ENABLE);

		osd_ext_wait_vsync_hw();
	}

}

void osd_ext_update_disp_axis_hw(
	u32 display_h_start,
	u32 display_h_end,
	u32 display_v_start,
	u32 display_v_end,
	u32 xoffset,
	u32 yoffset,
	u32 mode_change,
	u32 index)
{
	struct pandata_s disp_data;
	struct pandata_s pan_data;

	if (osd_ext_hw.color_info[index] == NULL)
		return;

	if (mode_change)	/* modify pandata . */
		add_to_update_list(index, OSD_COLOR_MODE);

	disp_data.x_start = display_h_start;
	disp_data.y_start = display_v_start;
	disp_data.x_end = display_h_end;
	disp_data.y_end = display_v_end;

	pan_data.x_start = xoffset;
	pan_data.x_end = xoffset + (display_h_end - display_h_start);
	pan_data.y_start = yoffset;
	pan_data.y_end = yoffset + (display_v_end - display_v_start);

	/* if output mode change then reset pan ofFfset. */
	memcpy(&osd_ext_hw.pandata[index], &pan_data,
			sizeof(struct pandata_s));
	memcpy(&osd_ext_hw.dispdata[index], &disp_data,
			sizeof(struct pandata_s));
	add_to_update_list(index, DISP_GEOMETRY);
	osd_ext_wait_vsync_hw();
}

void osd_ext_setup(struct osd_ctl_s *osd_ext_ctl,
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
		   const struct color_bit_define_s *color,
		   int index)
{
	int cpu_type;
	u32 w = (color->bpp * xres_virtual + 7) >> 3;
	struct pandata_s disp_data;
	struct pandata_s pan_data;

	pan_data.x_start = xoffset;
	pan_data.y_start = yoffset;

	disp_data.x_start = disp_start_x;
	disp_data.y_start = disp_start_y;

	if (likely(osd_ext_hw.free_scale_enable[OSD1] && index == OSD1)) {
		pan_data.x_end = xoffset + (disp_end_x - disp_start_x);
		pan_data.y_end = yoffset + (disp_end_y - disp_start_y);
		disp_data.x_end = disp_end_x;
		disp_data.y_end = disp_end_y;
	} else {
		pan_data.x_end = xoffset + (disp_end_x - disp_start_x);
		pan_data.y_end = yoffset + (disp_end_y - disp_start_y);
		disp_data.x_end = disp_end_x;
		disp_data.y_end = disp_end_y;
	}

	if (osd_ext_hw.fb_gem[index].addr != fbmem ||
	    osd_ext_hw.fb_gem[index].width != w
	    || osd_ext_hw.fb_gem[index].height != yres_virtual) {
		osd_ext_hw.fb_gem[index].addr = fbmem;
		osd_ext_hw.fb_gem[index].width = w;
		osd_ext_hw.fb_gem[index].height = yres_virtual;

	if (color != osd_ext_hw.color_info[index]) {
		osd_ext_hw.color_info[index] = color;
		add_to_update_list(index, OSD_COLOR_MODE);
	}

	cpu_type = get_cpu_type();
	if (cpu_type == MESON_CPU_MAJOR_ID_AXG)
		osd_ext_update_phy_addr(0);

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
	else {
		if (fbmem == 0) {
			canvas_config(osd_ext_hw.fb_gem[index].canvas_idx,
				osd_hw->fb_gem[index].addr,
				osd_ext_hw.fb_gem[index].width,
				osd_ext_hw.fb_gem[index].height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else {
			canvas_config(osd_ext_hw.fb_gem[index].canvas_idx,
				osd_ext_hw.fb_gem[index].addr,
				osd_ext_hw.fb_gem[index].width,
				osd_ext_hw.fb_gem[index].height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		}
	}
#endif
	}

	/* osd blank only control by /sys/class/graphcis/fbx/blank */
#if 0
	if (osd_ext_hw.enable[index] == DISABLE) {
		osd_ext_hw.enable[index] = ENABLE;
		add_to_update_list(index, OSD_ENABLE);
	}
#endif
	if (memcmp(&pan_data, &osd_ext_hw.pandata[index],
		   sizeof(struct pandata_s)) != 0 ||
	    memcmp(&disp_data, &osd_ext_hw.dispdata[index],
		   sizeof(struct pandata_s)) != 0) {
		memcpy(&osd_ext_hw.pandata[index], &pan_data,
				sizeof(struct pandata_s));
		memcpy(&osd_ext_hw.dispdata[index], &disp_data,
				sizeof(struct pandata_s));
		add_to_update_list(index, DISP_GEOMETRY);
	}

	osd_ext_wait_vsync_hw();
}

void osd_ext_setpal_hw(u32 index,
		       unsigned int regno,
		       unsigned int red,
		       unsigned int green,
		       unsigned int blue,
		       unsigned int transp)
{

	if (regno < 256) {
		u32 pal;

		pal = ((red   & 0xff) << 24) |
		      ((green & 0xff) << 16) |
		      ((blue & 0xff) << 8) |
		      (transp & 0xff);
		osd_reg_write(VIU2_OSD1_COLOR_ADDR + REG_OFFSET * index, regno);
		osd_reg_write(VIU2_OSD1_COLOR + REG_OFFSET * index, pal);
	}
}

u32 osd_ext_get_order_hw(u32 index)
{
	return osd_ext_hw.order & 0x3;
}

void osd_ext_set_order_hw(u32 index, u32 order)
{
	if ((order != OSD_ORDER_01) && (order != OSD_ORDER_10))
		return;

	osd_ext_hw.order = order;
	add_to_update_list(index, OSD_CHANGE_ORDER);
	osd_ext_wait_vsync_hw();
}

void osd_ext_enable_hw(u32 index, int enable)
{
	osd_ext_hw.enable[index] = enable;
	add_to_update_list(index, OSD_ENABLE);

	osd_ext_wait_vsync_hw();
}

/* vpu free scale mode */
static void osd_ext_set_free_scale_enable_mode0(u32 index, u32 enable)
{
	static struct pandata_s save_disp_data = { 0, 0, 0, 0 };
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	int mode_changed = 0;

	if ((index == OSD1) &&
			(osd_ext_hw.free_scale_enable[index] != enable))
		mode_changed = 1;
#endif
#endif
	osd_log_info("osd%d free scale %s\n",
			index, enable ? "ENABLE" : "DISABLE");
	osd_ext_hw.free_scale_enable[index] = enable;

	if (index == OSD1) {
		if (enable) {
			osd_ext_vf_need_update = true;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			if ((osd_ext_hw.free_src_data[OSD1].x_end > 0)
			    && (osd_ext_hw.free_src_data[OSD1].x_end > 0)) {
				vf.width =
				    osd_ext_hw.free_src_data[index].x_end -
				    osd_ext_hw.free_src_data[index].x_start +
				    1;
				vf.height =
				    osd_ext_hw.free_src_data[index].y_end -
				    osd_ext_hw.free_src_data[index].y_start +
				    1;
			} else {
				vf.width = osd_ext_hw.free_scale_width[OSD1];
				vf.height = osd_ext_hw.free_scale_height[OSD1];
			}

			vf.type = (VIDTYPE_NO_VIDEO_ENABLE |
				   VIDTYPE_PROGRESSIVE |
				   VIDTYPE_VIU_FIELD |
				   VIDTYPE_VSCALE_DISABLE);
			vf.ratio_control = DISP_RATIO_FORCECONFIG |
				DISP_RATIO_NO_KEEPRATIO;

			if (osd_ext_vf_prov_init == 0) {
				vf_provider_init(&osd_ext_vf_prov,
						PROVIDER_NAME,
						&osd_ext_vf_provider,
						NULL);
				osd_ext_vf_prov_init = 1;
			}

			vf_reg_provider(&osd_ext_vf_prov);
			memcpy(&save_disp_data, &osd_ext_hw.dispdata[OSD1],
					sizeof(struct pandata_s));

			g_vf_visual_width = vf.width - 1 -
				osd_hw->dispdata[OSD1].x_start;
			g_vf_width = vf.width - 1;
			g_vf_height = vf.height - 1;
			osd_ext_hw.dispdata[OSD1].x_end =
				osd_ext_hw.dispdata[OSD1].x_start +
				vf.width - 1;
			osd_ext_hw.dispdata[OSD1].y_end =
				osd_ext_hw.dispdata[OSD1].y_start +
				vf.height - 1;
#endif
			add_to_update_list(OSD1, DISP_GEOMETRY);
			add_to_update_list(OSD1, OSD_COLOR_MODE);
		} else {
			osd_ext_vf_need_update = false;

			if (save_disp_data.x_end <= save_disp_data.x_start ||
			    save_disp_data.y_end <= save_disp_data.y_start)
				return;

			memcpy(&osd_ext_hw.dispdata[OSD1], &save_disp_data,
					sizeof(struct pandata_s));

			add_to_update_list(OSD1, DISP_GEOMETRY);
			add_to_update_list(OSD1, OSD_COLOR_MODE);
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			vf_unreg_provider(&osd_ext_vf_prov);
#endif

		}
	} else {
		add_to_update_list(OSD2, DISP_GEOMETRY);
		add_to_update_list(OSD2, OSD_COLOR_MODE);
	}

	osd_ext_enable_hw(osd_ext_hw.enable[index], index);
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	if (mode_changed) {
		/* extern void vf_ppmgr_reset(int type); */
		vf_ppmgr_reset(1);
	}
#endif
#endif
}

/* osd free scale mode */
static void osd_ext_set_free_scale_enable_mode1(u32 index, u32 enable)
{
	unsigned int h_enable = 0;
	unsigned int v_enable = 0;

	h_enable = (enable & 0xffff0000 ? 1 : 0);
	v_enable = (enable & 0xffff ? 1 : 0);
	osd_ext_hw.free_scale[index].h_enable = h_enable;
	osd_ext_hw.free_scale[index].v_enable = v_enable;

	if (h_enable || v_enable)
		osd_ext_hw.free_scale_enable[index] = 1;
	else if (!h_enable && !v_enable)
		osd_ext_hw.free_scale_enable[index] = 0;

	if (index == OSD1) {
		if (osd_ext_hw.free_scale_enable[index]) {
			add_to_update_list(index, OSD_COLOR_MODE);
			add_to_update_list(index, OSD_FREESCALE_COEF);
			add_to_update_list(index, DISP_GEOMETRY);
			add_to_update_list(index, DISP_FREESCALE_ENABLE);
		} else {
			add_to_update_list(index, OSD_COLOR_MODE);
			add_to_update_list(index, DISP_GEOMETRY);
			add_to_update_list(index, DISP_FREESCALE_ENABLE);
		}
	} else {
		add_to_update_list(OSD2, DISP_GEOMETRY);
		add_to_update_list(OSD2, OSD_COLOR_MODE);
	}
	osd_ext_enable_hw(osd_ext_hw.enable[index], index);
}

void osd_ext_set_free_scale_enable_hw(u32 index, u32 enable)
{
	if (osd_ext_hw.free_scale_mode[index])
		osd_ext_set_free_scale_enable_mode1(index, enable);
	else
		osd_ext_set_free_scale_enable_mode0(index, enable);
}

void osd_ext_get_free_scale_enable_hw(u32 index, u32 *free_scale_enable)
{
	*free_scale_enable = osd_ext_hw.free_scale_enable[index];
}

void osd_ext_get_free_scale_width_hw(u32 index, u32 *free_scale_width)
{
	*free_scale_width = osd_ext_hw.free_src_data[index].x_end -
		osd_ext_hw.free_src_data[index].x_start + 1;
}

void osd_ext_get_free_scale_height_hw(u32 index, u32 *free_scale_height)
{
	*free_scale_height = osd_ext_hw.free_src_data[index].y_end -
		osd_ext_hw.free_src_data[index].y_start + 1;

}

void osd_ext_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
				    s32 *y1)
{
	*x0 = osd_ext_hw.free_src_data[index].x_start;
	*y0 = osd_ext_hw.free_src_data[index].y_start;
	*x1 = osd_ext_hw.free_src_data[index].x_end;
	*y1 = osd_ext_hw.free_src_data[index].y_end;
}

void osd_ext_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_ext_hw.free_src_data[index].x_start = x0;
	osd_ext_hw.free_src_data[index].y_start = y0;
	osd_ext_hw.free_src_data[index].x_end = x1;
	osd_ext_hw.free_src_data[index].y_end = y1;
}

void osd_ext_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1,
			       s32 *y1)
{
	*x0 = osd_ext_hw.scaledata[index].x_start;
	*x1 = osd_ext_hw.scaledata[index].x_end;
	*y0 = osd_ext_hw.scaledata[index].y_start;
	*y1 = osd_ext_hw.scaledata[index].y_end;
}

void osd_ext_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_ext_hw.scaledata[index].x_start = x0;
	osd_ext_hw.scaledata[index].x_end = x1;
	osd_ext_hw.scaledata[index].y_start = y0;
	osd_ext_hw.scaledata[index].y_end = y1;
}

void osd_ext_set_free_scale_mode_hw(u32 index, u32 freescale_mode)
{
	osd_ext_hw.free_scale_mode[index] = freescale_mode;
}

void osd_ext_get_free_scale_mode_hw(u32 index, u32 *freescale_mode)
{
	*freescale_mode = osd_ext_hw.free_scale_mode[index];
}

void osd_ext_get_window_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	*x0 = osd_ext_hw.free_dst_data[index].x_start;
	*x1 = osd_ext_hw.free_dst_data[index].x_end;
	*y0 = osd_ext_hw.free_dst_data[index].y_start;
	*y1 = osd_ext_hw.free_dst_data[index].y_end;
}

void osd_ext_set_window_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_ext_hw.free_dst_data[index].x_start = x0;
	osd_ext_hw.free_dst_data[index].x_end = x1;
	osd_ext_hw.free_dst_data[index].y_start = y0;
	osd_ext_hw.free_dst_data[index].y_end = y1;
}

void osd_ext_set_debug_hw(u32 index, u32 debug_flag)
{
	u32 reg = 0;
	u32 offset = 0;
	struct pandata_s *pdata = NULL;

	if (debug_flag == 1) {
		pdata = &osd_ext_hw.pandata[index];
		osd_log_info("pandata[%d]:\n", index);
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &osd_ext_hw.dispdata[index];
		osd_log_info("dispdata[%d]\n", index);
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);

		pdata = &osd_ext_hw.scaledata[index];
		osd_log_info("scaledata[%d]\n", index);
		osd_log_info("\tx_start: 0x%08x, x_end: 0x%08x\n",
				pdata->x_start, pdata->x_end);
		osd_log_info("\ty_start: 0x%08x, y_end: 0x%08x\n",
				pdata->y_start, pdata->y_end);
	} else if (debug_flag == 2) {
		reg = VPU_VIU_VENC_MUX_CTRL;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VPP_MISC;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = VIU_OSD1_CTRL_STAT;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		if (index == 1)
			offset = REG_OFFSET;
		reg = offset + VIU2_OSD1_BLK0_CFG_W0;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU2_OSD1_BLK0_CFG_W1;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU2_OSD1_BLK0_CFG_W2;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU2_OSD1_BLK0_CFG_W3;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
		reg = offset + VIU2_OSD1_BLK0_CFG_W4;
		osd_log_info("reg[0x%x]: 0x%08x\n", reg, osd_reg_read(reg));
	} else {
		osd_log_err("argument error\n");
	}
}

void osd_ext_get_block_windows_hw(u32 index, u32 *windows)
{
	/* memcpy(windows, osd_ext_hw.block_windows[index],
	 *      sizeof(osd_ext_hw.block_windows[index]));
	 */
}

void osd_ext_set_block_windows_hw(u32 index, u32 *windows)
{
	/* memcpy(osd_ext_hw.block_windows[index], windows,
	 *      sizeof(osd_ext_hw.block_windows[index]));
	 * add_to_update_list(index, DISP_GEOMETRY);
	 * osd_ext_wait_vsync_hw();
	 */
}

void osd_ext_get_block_mode_hw(u32 index, u32 *mode)
{
	*mode = osd_ext_hw.block_mode[index];
}

void osd_ext_set_block_mode_hw(u32 index, u32 mode)
{
	osd_ext_hw.block_mode[index] = mode;
	add_to_update_list(index, DISP_GEOMETRY);
	osd_ext_wait_vsync_hw();
}

void osd_ext_set_2x_scale_hw(u32 index, u16 h_scale_enable, u16 v_scale_enable)
{
	osd_log_info("osd[%d] set scale, h_scale: %s, v_scale: %s\n",
		     index, h_scale_enable ? "ENABLE" : "DISABLE",
		     v_scale_enable ? "ENABLE" : "DISABLE");
	osd_log_info("osd[%d].scaledata: %d %d %d %d\n",
		     index,
		     osd_ext_hw.scaledata[index].x_start,
		     osd_ext_hw.scaledata[index].x_end,
		     osd_ext_hw.scaledata[index].y_start,
		     osd_ext_hw.scaledata[index].y_end);
	osd_log_info("osd[%d].pandata: %d %d %d %d\n",
		     index,
		     osd_ext_hw.pandata[index].x_start,
		     osd_ext_hw.pandata[index].x_end,
		     osd_ext_hw.pandata[index].y_start,
		     osd_ext_hw.pandata[index].y_end);

	osd_ext_hw.scale[index].h_enable = h_scale_enable;
	osd_ext_hw.scale[index].v_enable = v_scale_enable;
	add_to_update_list(index, DISP_SCALE_ENABLE);
	add_to_update_list(index, DISP_GEOMETRY);

	osd_ext_wait_vsync_hw();
}

void osd_ext_enable_3d_mode_hw(int index, int enable)
{
	spin_lock_irqsave(&osd_ext_lock, lock_flags);
	osd_ext_hw.mode_3d[index].enable = enable;
	spin_unlock_irqrestore(&osd_ext_lock, lock_flags);

	if (enable) {
		/* when disable 3d mode ,we should return to stardard state. */
		osd_ext_hw.mode_3d[index].left_right = OSD_LEFT;
		osd_ext_hw.mode_3d[index].l_start =
			osd_ext_hw.pandata[index].x_start;
		osd_ext_hw.mode_3d[index].l_end =
			(osd_ext_hw.pandata[index].x_end +
			 osd_ext_hw.pandata[index].x_start) >> 1;
		osd_ext_hw.mode_3d[index].r_start =
			osd_ext_hw.mode_3d[index].l_end + 1;
		osd_ext_hw.mode_3d[index].r_end =
			osd_ext_hw.pandata[index].x_end;
		osd_ext_hw.mode_3d[index].origin_scale.h_enable =
			osd_ext_hw.scale[index].h_enable;
		osd_ext_hw.mode_3d[index].origin_scale.v_enable =
			osd_ext_hw.scale[index].v_enable;
		osd_ext_set_2x_scale_hw(index, 1, 0);
	} else {
		osd_ext_set_2x_scale_hw(index,
			osd_ext_hw.mode_3d[index].origin_scale.h_enable,
			osd_ext_hw.mode_3d[index].origin_scale.v_enable);
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
void osd_ext_pan_display_fence(struct osd_fence_map_s *fence_map)
{
	s32 ret = 1;
	long diff_x, diff_y;
	u32 index = fence_map->fb_index;
	u32 xoffset = fence_map->xoffset;
	u32 yoffset = fence_map->yoffset;

	if (index >= 2)
		return;

	if (ext_timeline_created) { /* out fence created success. */
		ret = osd_ext_wait_buf_ready(fence_map);

		if (ret < 0)
			osd_log_dbg(MODULE_FENCE, "fence wait ret %d\n", ret);
	}

	if (ret) {
		if (xoffset != osd_ext_hw.pandata[index].x_start ||
		    yoffset != osd_ext_hw.pandata[index].y_start) {
			diff_x = xoffset - osd_ext_hw.pandata[index].x_start;
			diff_y = yoffset - osd_ext_hw.pandata[index].y_start;

			osd_ext_hw.pandata[index].x_start += diff_x;
			osd_ext_hw.pandata[index].x_end   += diff_x;
			osd_ext_hw.pandata[index].y_start += diff_y;
			osd_ext_hw.pandata[index].y_end   += diff_y;
			add_to_update_list(index, DISP_GEOMETRY);
			osd_ext_wait_vsync_hw();
		}
	}

	if (ext_timeline_created) {
		if (ret)
			sw_sync_timeline_inc(ext_timeline, 1);
		else
			osd_log_err("------NOT signal out_fence ERROR\n");
	}

	osd_log_dbg(MODULE_FENCE, "offset[%d-%d]x[%d-%d]y[%d-%d]\n",
		    xoffset, yoffset, osd_ext_hw.pandata[index].x_start,
		    osd_ext_hw.pandata[index].x_end,
		    osd_ext_hw.pandata[index].y_start,
		    osd_ext_hw.pandata[index].y_end);
}
#endif

void osd_ext_pan_display_hw(u32 index, unsigned int xoffset,
			    unsigned int yoffset)
{
	long diff_x, diff_y;

#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	if (index >= 1)
#else
	if (index >= 2)
#endif
		return;

	if (xoffset != osd_ext_hw.pandata[index].x_start ||
	    yoffset != osd_ext_hw.pandata[index].y_start) {
		diff_x = xoffset - osd_ext_hw.pandata[index].x_start;
		diff_y = yoffset - osd_ext_hw.pandata[index].y_start;

		osd_ext_hw.pandata[index].x_start += diff_x;
		osd_ext_hw.pandata[index].x_end += diff_x;
		osd_ext_hw.pandata[index].y_start += diff_y;
		osd_ext_hw.pandata[index].y_end += diff_y;
		add_to_update_list(index, DISP_GEOMETRY);

		osd_ext_wait_vsync_hw();

		osd_log_info("offset[%d-%d]x[%d-%d]y[%d-%d]\n",
			     xoffset, yoffset,
			     osd_ext_hw.pandata[index].x_start,
			     osd_ext_hw.pandata[index].x_end,
			     osd_ext_hw.pandata[index].y_start,
			     osd_ext_hw.pandata[index].y_end);
	}
}

void osd_ext_get_info(u32 index, u32 *addr, u32 *width, u32 *height)
{
	*addr = osd_ext_hw.fb_gem[index].addr;
	*width = osd_ext_hw.fb_gem[index].width;
	*height = osd_ext_hw.fb_gem[index].height;
}

static void osd1_update_disp_scale_enable(void)
{
	if (osd_ext_hw.scale[OSD1].h_enable)
		osd_reg_set_mask(VIU2_OSD1_BLK0_CFG_W0, 3 << 12);
	else
		osd_reg_clr_mask(VIU2_OSD1_BLK0_CFG_W0, 3 << 12);

	if (osd_ext_hw.scan_mode != SCAN_MODE_INTERLACE) {
		if (osd_ext_hw.scale[OSD1].v_enable)
			osd_reg_set_mask(VIU2_OSD1_BLK0_CFG_W0, 1 << 14);
		else
			osd_reg_clr_mask(VIU2_OSD1_BLK0_CFG_W0, 1 << 14);
	}
}

static void osd2_update_disp_scale_enable(void)
{
	if (osd_ext_hw.scale[OSD2].h_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
		osd_reg_clr_mask(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);
#else
		osd_reg_set_mask(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);
#endif
	} else
		osd_reg_clr_mask(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);

	if (osd_ext_hw.scan_mode != SCAN_MODE_INTERLACE) {
		if (osd_ext_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
			osd_reg_clr_mask(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
#else
			osd_reg_set_mask(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
#endif
		} else
			osd_reg_clr_mask(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
	}

}

static  void  osd1_update_disp_freescale_enable(void)
{
	int hf_phase_step, vf_phase_step;
	int src_w, src_h, dst_w, dst_h;
	int vsc_ini_rcv_num, vsc_ini_rpt_p0_num;
	int hsc_ini_rcv_num, hsc_ini_rpt_p0_num;

	int hf_bank_len = 4;
	int vf_bank_len = 4;

	hsc_ini_rcv_num = hf_bank_len;
	vsc_ini_rcv_num = vf_bank_len;
	hsc_ini_rpt_p0_num = (hf_bank_len / 2 - 1) > 0 ?
		(hf_bank_len / 2 - 1) : 0;
	vsc_ini_rpt_p0_num = (vf_bank_len / 2 - 1) > 0 ?
		(vf_bank_len / 2 - 1) : 0;

	src_w = osd_ext_hw.free_src_data[OSD1].x_end -
		osd_ext_hw.free_src_data[OSD1].x_start + 1;
	src_h = osd_ext_hw.free_src_data[OSD1].y_end -
		osd_ext_hw.free_src_data[OSD1].y_start + 1;
	dst_w = osd_ext_hw.free_dst_data[OSD1].x_end -
		osd_ext_hw.free_dst_data[OSD1].x_start + 1;
	hf_phase_step = ((src_w + 1) << 18) / dst_w;
	hf_phase_step = (hf_phase_step << 6);

	dst_h = osd_ext_hw.free_dst_data[OSD1].y_end -
		osd_ext_hw.free_dst_data[OSD1].y_start + 1;
	vf_phase_step = ((src_h + 1) << 20) /
		dst_h;
	vf_phase_step = (vf_phase_step << 4);

	osd_reg_set_bits(VPP2_OSD_SC_DUMMY_DATA, 0x00808000, 0, 32);

	if (osd_ext_hw.free_scale_enable[OSD1]) {
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, 1, 3, 1);
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, OSD1, 0, 2);
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, 0, 4, 8);
	} else
		osd_reg_clr_mask(VPP2_OSD_SC_CTRL0, 1 << 3);

	if (osd_ext_hw.free_scale_enable[OSD1]) {
		osd_reg_set_bits(VPP2_OSD_SCI_WH_M1,
			src_w, 16, 13);
		osd_reg_set_bits(VPP2_OSD_SCI_WH_M1,
			src_h, 0, 13);

		osd_reg_set_bits(VPP2_OSD_SCO_H_START_END,
			osd_ext_hw.free_dst_data[OSD1].x_start, 16, 12);
		osd_reg_set_bits(VPP2_OSD_SCO_H_START_END,
			osd_ext_hw.free_dst_data[OSD1].x_end, 0, 12);

		osd_reg_set_bits(VPP2_OSD_SCO_V_START_END,
			 osd_ext_hw.free_dst_data[OSD1].y_start,
			 16, 12);
		osd_reg_set_bits(VPP2_OSD_SCO_V_START_END,
			osd_ext_hw.free_dst_data[OSD1].y_end, 0, 12);
	}

	if (osd_ext_hw.free_scale[OSD1].v_enable) {
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0, vf_bank_len, 0, 3);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0,
				vsc_ini_rcv_num, 3, 3);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0,
				vsc_ini_rpt_p0_num, 8, 2);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0, 1, 24, 1);
	} else
		osd_reg_clr_mask(VPP2_OSD_VSC_CTRL0, 1 << 24);

	if (osd_ext_hw.free_scale[OSD1].h_enable) {
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0, hf_bank_len, 0, 3);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0,
				hsc_ini_rcv_num, 3, 3);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0,
				hsc_ini_rpt_p0_num, 8, 2);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0, 1, 22, 1);
	} else
		osd_reg_clr_mask(VPP2_OSD_HSC_CTRL0, 1 << 22);

	if (osd_ext_hw.free_scale_enable[OSD1]) {
		osd_reg_set_bits(VPP2_OSD_HSC_PHASE_STEP,
				hf_phase_step, 0, 28);
		osd_reg_set_bits(VPP2_OSD_HSC_INI_PHASE, 0, 0, 16);
		osd_reg_set_bits(VPP2_OSD_VSC_PHASE_STEP,
				vf_phase_step, 0, 28);
		osd_reg_set_bits(VPP2_OSD_VSC_INI_PHASE, 0, 0, 16);
	}

	remove_from_update_list(OSD1, DISP_FREESCALE_ENABLE);
}

static void osd1_update_coef(void)
{
	int hf_coef_idx = 0;
	int vf_coef_idx = 0;
	int *hf_coef, *vf_coef;
	int i = 0;
	int hf_coef_wren = 1;
	int vf_coef_wren = 1;

	if (vf_coef_idx == 0)
		vf_coef = filt_coef0;
	else if (vf_coef_idx == 1)
		vf_coef = filt_coef1;
	else if (vf_coef_idx == 2)
		vf_coef = filt_coef2;
	else
		vf_coef = filt_coef0;

	if (hf_coef_idx == 0)
		hf_coef = filt_coef0;
	else if (hf_coef_idx == 1)
		hf_coef = filt_coef1;
	else if (hf_coef_idx == 2)
		hf_coef = filt_coef2;
	else
		hf_coef = filt_coef0;


	if (vf_coef_wren) {
		osd_reg_set_bits(VPP2_OSD_SCALE_COEF_IDX,
				0x0000, 0, 9);
		for (i = 0; i < 33; i++)
			osd_reg_write(VPP2_OSD_SCALE_COEF,
					vf_coef[i]);
	}
	if (hf_coef_wren) {
		osd_reg_set_bits(VPP2_OSD_SCALE_COEF_IDX,
				0x0100, 0, 9);

		for (i = 0; i < 33; i++)
			osd_reg_write(VPP2_OSD_SCALE_COEF, hf_coef[i]);
	}
	remove_from_update_list(OSD1, OSD_FREESCALE_COEF);
}

static  void  osd2_update_disp_freescale_enable(void)
{
	int hf_phase_step, vf_phase_step;
	int src_w, src_h, dst_w, dst_h;
	int vsc_ini_rcv_num, vsc_ini_rpt_p0_num;
	int hsc_ini_rcv_num, hsc_ini_rpt_p0_num;

	int hf_bank_len = 4;
	int vf_bank_len = 4;

	hsc_ini_rcv_num = hf_bank_len;
	vsc_ini_rcv_num = vf_bank_len;
	hsc_ini_rpt_p0_num = (hf_bank_len / 2 - 1) > 0 ?
		(hf_bank_len / 2 - 1) : 0;
	vsc_ini_rpt_p0_num = (vf_bank_len / 2 - 1) > 0 ?
		(vf_bank_len / 2 - 1) : 0;

	src_w = osd_ext_hw.free_src_data[OSD2].x_end -
		osd_ext_hw.free_src_data[OSD2].x_start + 1;
	src_h = osd_ext_hw.free_src_data[OSD2].y_end -
		osd_ext_hw.free_src_data[OSD2].y_start + 1;
	dst_w = osd_ext_hw.free_dst_data[OSD2].x_end -
		osd_ext_hw.free_dst_data[OSD2].x_start + 1;
	hf_phase_step =
		((src_w + 1) << 18) / dst_w;
	hf_phase_step = (hf_phase_step << 6);

	dst_h = osd_ext_hw.free_dst_data[OSD2].y_end -
		osd_ext_hw.free_dst_data[OSD2].y_start + 1;
	vf_phase_step =
		((src_h + 1) << 20) / dst_h;
	vf_phase_step = (vf_phase_step << 4);

	osd_reg_set_bits(VPP2_OSD_SC_DUMMY_DATA, 0x00808000, 0, 32);

	if (osd_ext_hw.free_scale_enable[OSD2]) {
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, 1, 3, 1);
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, OSD2, 0, 2);
		osd_reg_set_bits(VPP2_OSD_SC_CTRL0, 0, 4, 8);
	} else
		osd_reg_clr_mask(VPP2_OSD_SC_CTRL0, 1 << 3);

	if (osd_ext_hw.free_scale_enable[OSD2]) {
		osd_reg_set_bits(VPP2_OSD_SCI_WH_M1,
			src_w, 16, 13);
		osd_reg_set_bits(VPP2_OSD_SCI_WH_M1,
			src_h, 0, 13);

		osd_reg_set_bits(VPP2_OSD_SCO_H_START_END,
			osd_ext_hw.free_dst_data[OSD2].x_start, 16, 12);
		osd_reg_set_bits(VPP2_OSD_SCO_H_START_END,
			osd_ext_hw.free_dst_data[OSD2].x_end, 0, 12);

		osd_reg_set_bits(VPP2_OSD_SCO_V_START_END,
			osd_ext_hw.free_dst_data[OSD2].y_start, 16, 12);
		osd_reg_set_bits(VPP2_OSD_SCO_V_START_END,
			osd_ext_hw.free_dst_data[OSD2].y_end, 0, 12);
	}

	if (osd_ext_hw.free_scale[OSD2].h_enable) {
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0, hf_bank_len, 0, 3);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0,
				hsc_ini_rcv_num, 3, 3);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0,
				hsc_ini_rpt_p0_num, 8, 2);
		osd_reg_set_bits(VPP2_OSD_HSC_CTRL0, 1, 22, 1);
	} else
		osd_reg_clr_mask(VPP2_OSD_HSC_CTRL0, 1 << 22);

	if (osd_ext_hw.free_scale[OSD2].v_enable) {
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0, vf_bank_len, 0, 3);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0,
				vsc_ini_rcv_num, 3, 3);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0,
				vsc_ini_rpt_p0_num, 8, 2);
		osd_reg_set_bits(VPP2_OSD_VSC_CTRL0, 1, 24, 1);
	} else
		osd_reg_clr_mask(VPP2_OSD_VSC_CTRL0, 1 << 24);

	if (osd_ext_hw.free_scale_enable[OSD2]) {
		osd_reg_set_bits(VPP2_OSD_HSC_PHASE_STEP,
				hf_phase_step, 0, 28);
		osd_reg_set_bits(VPP2_OSD_HSC_INI_PHASE, 0, 0, 16);

		osd_reg_set_bits(VPP2_OSD_VSC_PHASE_STEP,
				vf_phase_step, 0, 28);
		osd_reg_set_bits(VPP2_OSD_VSC_INI_PHASE, 0, 0, 16);
	}

	remove_from_update_list(OSD2, DISP_FREESCALE_ENABLE);
}

static void osd2_update_coef(void)
{
	int hf_coef_idx = 0;
	int vf_coef_idx = 0;
	int *hf_coef, *vf_coef;
	int i = 0;
	int hf_coef_wren = 1;
	int vf_coef_wren = 1;

	if (vf_coef_idx == 0)
		vf_coef = filt_coef0;
	else if (vf_coef_idx == 1)
		vf_coef = filt_coef1;
	else if (vf_coef_idx == 2)
		vf_coef = filt_coef2;
	else
		vf_coef = filt_coef0;

	if (hf_coef_idx == 0)
		hf_coef = filt_coef0;
	else if (hf_coef_idx == 1)
		hf_coef = filt_coef1;
	else if (hf_coef_idx == 2)
		hf_coef = filt_coef2;
	else
		hf_coef = filt_coef0;

	if (vf_coef_wren) {
		osd_reg_set_bits(VPP2_OSD_SCALE_COEF_IDX, 0x0000, 0, 9);

		for (i = 0; i < 33; i++)
			osd_reg_write(VPP2_OSD_SCALE_COEF, vf_coef[i]);
	}

	if (hf_coef_wren) {
		osd_reg_set_bits(VPP2_OSD_SCALE_COEF_IDX, 0x0100, 0, 9);

		for (i = 0; i < 33; i++)
			osd_reg_write(VPP2_OSD_SCALE_COEF, hf_coef[i]);
	}

	remove_from_update_list(OSD2, OSD_FREESCALE_COEF);
}

static void osd1_update_color_mode(void)
{
	u32 data32 = 0;

	if (osd_ext_hw.color_info[OSD1] != NULL) {
		data32 = (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= osd_reg_read(VIU2_OSD1_BLK0_CFG_W0) & 0x30007040;
		data32 |= osd_ext_hw.fb_gem[OSD1].canvas_idx << 16;

		/* if (!osd_ext_hw.rotate[OSD1].on_off) */
		data32 |= OSD_DATA_LITTLE_ENDIAN << 15;
		data32 |= osd_ext_hw.color_info[OSD1]->hw_colormat << 2;

		if (osd_ext_hw.color_info[OSD1]->color_index <
				COLOR_INDEX_YUV_422)
			data32 |= 1 << 7; /* rgb enable */

		/* osd_ext_blk_mode */
		data32 |= osd_ext_hw.color_info[OSD1]->hw_blkmode << 8;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W0, data32);
		osd_reg_write(VIU2_OSD1_BLK1_CFG_W0, data32);
		osd_reg_write(VIU2_OSD1_BLK2_CFG_W0, data32);
		osd_reg_write(VIU2_OSD1_BLK3_CFG_W0, data32);
	}

	remove_from_update_list(OSD1, OSD_COLOR_MODE);
}

static void osd2_update_color_mode(void)
{
	u32 data32 = 0;

	if (osd_ext_hw.color_info[OSD2] != NULL) {
		data32 = (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= osd_reg_read(VIU2_OSD2_BLK0_CFG_W0) & 0x30007040;
		data32 |= osd_ext_hw.fb_gem[OSD2].canvas_idx << 16;

		/* if (!osd_ext_hw.rotate[OSD1].on_off) */
		data32 |= OSD_DATA_LITTLE_ENDIAN << 15;

		data32 |= osd_ext_hw.color_info[OSD2]->hw_colormat << 2;

		if (osd_ext_hw.color_info[OSD2]->color_index <
				COLOR_INDEX_YUV_422)
			data32 |= 1 << 7; /* rgb enable */

		/* osd_ext_blk_mode */
		data32 |= osd_ext_hw.color_info[OSD2]->hw_blkmode << 8;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W0, data32);
	}
	remove_from_update_list(OSD2, OSD_COLOR_MODE);
}

static void osd1_update_enable(void)
{
	if (osd_ext_hw.free_scale_mode[OSD1]) {
		if (osd_ext_hw.enable[OSD1] == ENABLE) {
			osd_reg_set_mask(VPP2_MISC, VPP_OSD1_POSTBLEND);
			osd_reg_set_mask(VPP2_MISC, VPP_POSTBLEND_EN);
		} else
			osd_reg_clr_mask(VPP2_MISC, VPP_OSD1_POSTBLEND);
	} else {
		u32 video_enable = 0;

		video_enable |= osd_reg_read(VPP2_MISC) & VPP_VD1_PREBLEND;

		if (osd_ext_hw.enable[OSD1] == ENABLE) {
			if (osd_ext_hw.free_scale_enable[OSD1]) {
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD1_POSTBLEND);
				osd_reg_set_mask(VPP2_MISC, VPP_OSD1_PREBLEND);
				osd_reg_set_mask(VPP2_MISC, VPP_VD1_POSTBLEND);
				osd_reg_set_mask(VPP2_MISC, VPP_PREBLEND_EN);
			} else {
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD1_PREBLEND);

				if (!video_enable)
					osd_reg_clr_mask(VPP2_MISC,
							VPP_VD1_POSTBLEND);

				osd_reg_set_mask(VPP2_MISC, VPP_OSD1_POSTBLEND);
			}

		} else {
			if (osd_ext_hw.free_scale_enable[OSD1])
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD1_PREBLEND);
			else
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD1_POSTBLEND);
		}
	}

	remove_from_update_list(OSD1, OSD_ENABLE);
}

static void osd2_update_enable(void)
{
	if (osd_ext_hw.free_scale_mode[OSD2]) {
		if (osd_ext_hw.enable[OSD1] == ENABLE) {
			osd_reg_set_mask(VPP2_MISC, VPP_OSD2_POSTBLEND);
			osd_reg_set_mask(VPP2_MISC, VPP_POSTBLEND_EN);
		} else
			osd_reg_clr_mask(VPP2_MISC, VPP_OSD2_POSTBLEND);
	} else {
		u32 video_enable = 0;

		video_enable |= osd_reg_read(VPP2_MISC) & VPP_VD1_PREBLEND;

		if (osd_ext_hw.enable[OSD2] == ENABLE) {
			if (osd_ext_hw.free_scale_enable[OSD2]) {
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD2_POSTBLEND);
				osd_reg_set_mask(VPP2_MISC, VPP_OSD2_PREBLEND);
				osd_reg_set_mask(VPP2_MISC, VPP_VD1_POSTBLEND);
			} else {
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD2_PREBLEND);

				if (!video_enable)
					osd_reg_clr_mask(VPP2_MISC,
							VPP_VD1_POSTBLEND);

				osd_reg_set_mask(VPP2_MISC, VPP_OSD2_POSTBLEND);
			}

		} else {
			if (osd_ext_hw.free_scale_enable[OSD2])
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD2_PREBLEND);
			else
				osd_reg_clr_mask(VPP2_MISC, VPP_OSD2_POSTBLEND);
		}
	}

	remove_from_update_list(OSD2, OSD_ENABLE);
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
	osd_reg_write(VIU2_OSD1_TCOLOR_AG0, osd_ext_hw.color_key[OSD1]);
	remove_from_update_list(OSD1, OSD_COLOR_KEY);
}

static void osd2_update_color_key(void)
{
	osd_reg_write(VIU2_OSD2_TCOLOR_AG0, osd_ext_hw.color_key[OSD2]);
	remove_from_update_list(OSD2, OSD_COLOR_KEY);
}

static void osd1_update_color_key_enable(void)
{
	u32 data32;

	data32 = osd_reg_read(VIU2_OSD1_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_ext_hw.color_key_enable[OSD1] << 6);
	osd_reg_write(VIU2_OSD1_BLK0_CFG_W0, data32);
	osd_reg_write(VIU2_OSD1_BLK1_CFG_W0, data32);
	osd_reg_write(VIU2_OSD1_BLK2_CFG_W0, data32);
	osd_reg_write(VIU2_OSD1_BLK3_CFG_W0, data32);
	remove_from_update_list(OSD1, OSD_COLOR_KEY_ENABLE);
}

static void osd2_update_color_key_enable(void)
{
	u32 data32;

	data32 = osd_reg_read(VIU2_OSD2_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_ext_hw.color_key_enable[OSD2] << 6);
	osd_reg_write(VIU2_OSD2_BLK0_CFG_W0, data32);
	remove_from_update_list(OSD2, OSD_COLOR_KEY_ENABLE);
}

static void osd1_update_gbl_alpha(void)
{
	u32 data32 = osd_reg_read(VIU2_OSD1_CTRL_STAT);

	data32 &= ~(0x1ff << 12);
	data32 |= osd_ext_hw.gbl_alpha[OSD1] << 12;
	osd_reg_write(VIU2_OSD1_CTRL_STAT, data32);
	remove_from_update_list(OSD1, OSD_GBL_ALPHA);
}

static void osd2_update_gbl_alpha(void)
{
	u32 data32 = osd_reg_read(VIU2_OSD2_CTRL_STAT);

	data32 &= ~(0x1ff << 12);
	data32 |= osd_ext_hw.gbl_alpha[OSD2] << 12;
	osd_reg_write(VIU2_OSD2_CTRL_STAT, data32);
	remove_from_update_list(OSD2, OSD_GBL_ALPHA);
}

static void osd2_update_order(void)
{
	switch (osd_ext_hw.order) {
	case OSD_ORDER_01:
		osd_reg_clr_mask(VPP2_MISC,
				VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;

	case OSD_ORDER_10:
		osd_reg_set_mask(VPP2_MISC,
				VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;

	default:
		break;
	}

	remove_from_update_list(OSD2, OSD_CHANGE_ORDER);
}

static void osd1_update_order(void)
{
	switch (osd_ext_hw.order) {
	case OSD_ORDER_01:
		osd_reg_clr_mask(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;

	case OSD_ORDER_10:
		osd_reg_set_mask(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;

	default:
		break;
	}

	remove_from_update_list(OSD1, OSD_CHANGE_ORDER);
}


#if 0
static void osd_ext_block_update_disp_geometry(u32 index)
{
	u32 data32;
	u32 data_w1, data_w2, data_w3, data_w4;
	u32 coef[4][2] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
	u32 xoff, yoff;
	u32 i;

	switch (osd_ext_hw.block_mode[index] & HW_OSD_BLOCK_LAYOUT_MASK) {
	case HW_OSD_BLOCK_LAYOUT_HORIZONTAL:
		yoff = ((osd_ext_hw.pandata[index].y_end & 0x1fff) -
			(osd_ext_hw.pandata[index].y_start & 0x1fff) + 1) >> 2;
		data_w1 = (osd_ext_hw.pandata[index].x_start & 0x1fff) |
			(osd_ext_hw.pandata[index].x_end & 0x1fff) << 16;
		data_w3 = (osd_ext_hw.dispdata[index].x_start & 0xfff) |
			(osd_ext_hw.dispdata[index].x_end & 0xfff) << 16;

		for (i = 0; i < 4; i++) {
			if (i == 3) {
				data_w2 = ((osd_ext_hw.pandata[index].y_start +
							yoff * i) & 0x1fff)
					  | (osd_ext_hw.pandata[index].y_end &
							0x1fff) << 16;
				data_w4 = ((osd_ext_hw.dispdata[index].y_start +
							yoff * i) & 0xfff)
					  | (osd_ext_hw.dispdata[index].y_end &
							0xfff) << 16;
			} else {
				data_w2 = ((osd_ext_hw.pandata[index].y_start +
						yoff * i) & 0x1fff)
					  | ((osd_ext_hw.pandata[index].y_start
					+ yoff * (i + 1) - 1) & 0x1fff) << 16;
				data_w4 = ((osd_ext_hw.dispdata[index].y_start +
						yoff * i) & 0xfff)
					  | ((osd_ext_hw.dispdata[index].y_start
					+ yoff * (i + 1) - 1) & 0xfff) << 16;
			}

			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) |
					((((((data32 >> 16) & 0xfff) + 1) >>
					   1) - 1) << 16);
			}
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}

		break;

	case HW_OSD_BLOCK_LAYOUT_VERTICAL:
		xoff = ((osd_ext_hw.pandata[index].x_end & 0x1fff) -
			(osd_ext_hw.pandata[index].x_start & 0x1fff) + 1) >> 2;
		data_w2 = (osd_ext_hw.pandata[index].y_start & 0x1fff) |
			(osd_ext_hw.pandata[index].y_end & 0x1fff) << 16;
		data_w4 = (osd_ext_hw.dispdata[index].y_start & 0xfff) |
			(osd_ext_hw.dispdata[index].y_end & 0xfff) << 16;

		if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = data_w4;
			data_w4 = ((data32 & 0xfff) >> 1) |
				((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1)
				 << 16);
		}

		for (i = 0; i < 4; i++) {
			data_w1 = ((osd_ext_hw.pandata[index].x_start +
					xoff * i) & 0x1fff)
				  | ((osd_ext_hw.pandata[index].x_start +
					xoff * (i + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_ext_hw.dispdata[index].x_start +
					xoff * i) & 0xfff)
				  | ((osd_ext_hw.dispdata[index].x_start +
					xoff * (i + 1) - 1) & 0xfff) << 16;

			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}

		break;

	case HW_OSD_BLOCK_LAYOUT_GRID:
		xoff = ((osd_ext_hw.pandata[index].x_end & 0x1fff) -
			(osd_ext_hw.pandata[index].x_start & 0x1fff) + 1) >> 1;
		yoff = ((osd_ext_hw.pandata[index].y_end & 0x1fff) -
			(osd_ext_hw.pandata[index].y_start & 0x1fff) + 1) >> 1;

		for (i = 0; i < 4; i++) {
			data_w1 = ((osd_ext_hw.pandata[index].x_start +
					xoff * coef[i][0]) & 0x1fff)
				  | ((osd_ext_hw.pandata[index].x_start +
				xoff * (coef[i][0] + 1) - 1) & 0x1fff) << 16;
			data_w2 = ((osd_ext_hw.pandata[index].y_start +
					yoff * coef[i][1]) & 0x1fff)
				  | ((osd_ext_hw.pandata[index].y_start +
				yoff * (coef[i][1] + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_ext_hw.dispdata[index].x_start +
					xoff * coef[i][0]) & 0xfff)
				  | ((osd_ext_hw.dispdata[index].x_start +
				xoff * (coef[i][0] + 1) - 1) & 0xfff) << 16;
			data_w4 = ((osd_ext_hw.dispdata[index].y_start +
					yoff * coef[i][1]) & 0xfff)
				  | ((osd_ext_hw.dispdata[index].y_start +
				yoff * (coef[i][1] + 1) - 1) & 0xfff) << 16;

			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) |
					((((((data32 >> 16) & 0xfff) +
					1) >> 1) - 1) << 16);
			}

			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}

		break;

	case HW_OSD_BLOCK_LAYOUT_CUSTOMER:
		for (i = 0; i < 4; i++) {
			if (((osd_ext_hw.block_windows[index][i << 1] >> 16) &
				0x1fff) > osd_ext_hw.pandata[index].x_end) {
				osd_ext_hw.block_windows[index][i << 1] =
				    (osd_ext_hw.block_windows[index][i << 1] &
					0x1fff)
				    | ((osd_ext_hw.pandata[index].x_end &
					0x1fff) << 16);
			}

			data_w1 = osd_ext_hw.block_windows[index][i << 1] &
				0x1fff1fff;
			data_w2 = ((osd_ext_hw.pandata[index].y_start & 0x1fff)
			    + (osd_ext_hw.block_windows[index][(i << 1) + 1] &
				    0x1fff))
				| (((osd_ext_hw.pandata[index].y_start
				& 0x1fff) << 16) +
				(osd_ext_hw.block_windows[index][(i << 1) + 1]
				& 0x1fff0000));
			data_w3 = (osd_ext_hw.dispdata[index].x_start +
					(data_w1 & 0xfff))
				  | (((osd_ext_hw.dispdata[index].x_start &
					0xfff) << 16) + (data_w1 & 0xfff0000));
			data_w4 = (osd_ext_hw.dispdata[index].y_start +
				(osd_ext_hw.block_windows[index][(i << 1) + 1]
					& 0xfff))
				| (((osd_ext_hw.dispdata[index].y_start
					& 0xfff) << 16) +
				(osd_ext_hw.block_windows[index][(i << 1) + 1]
					& 0xfff0000));

			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) |
					((((((data32 >> 16) & 0xfff) + 1) >> 1)
					  - 1) << 16);
			}

			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1 + (i << 4),
					data_w1);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2 + (i << 4),
					data_w2);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W3 + (i << 4),
					data_w3);
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W4 + (i << 2),
					data_w4);
		}

		break;

	default:
		osd_log_err("ERROR block_mode: 0x%x\n",
				osd_ext_hw.block_mode[index]);
		break;
	}
}
#endif

static void osd1_update_disp_geometry(void)
{
	u32 dy0, sy0, sy1;
	u32 data32;

	/* enable osd multi block */
	if (osd_ext_hw.block_mode[OSD1]) {
		/* osd_ext_block_update_disp_geometry(OSD1);
		 * data32 = osd_reg_read(VIU2_OSD1_CTRL_STAT);
		 * data32 &= 0xfffffff0;
		 * data32 |= (osd_ext_hw.block_mode[OSD1] &
		 *		HW_OSD_BLOCK_ENABLE_MASK);
		 * osd_reg_write(VIU2_OSD1_CTRL_STAT, data32);
		 */
		osd_log_info(
			"osd1_update_disp_geometry: not support block mode\n");
	} else {
		data32 = (osd_ext_hw.dispdata[OSD1].x_start & 0xfff) |
			(osd_ext_hw.dispdata[OSD1].x_end & 0xfff) << 16;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W3, data32);
		if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE)
			data32 = ((osd_ext_hw.dispdata[OSD1].y_start >>
				1) & 0xfff)
				| ((((osd_ext_hw.dispdata[OSD1].y_end +
				1) >> 1) - 1) & 0xfff) << 16;
		else
			data32 = (osd_ext_hw.dispdata[OSD1].y_start &
				0xfff)
				| (osd_ext_hw.dispdata[OSD1].y_end &
					0xfff) << 16;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W4, data32);
		/* enable osd 2x scale */
		if (osd_ext_hw.scale[OSD1].h_enable ||
				osd_ext_hw.scale[OSD1].v_enable) {
			data32 = (osd_ext_hw.scaledata[OSD1].x_start & 0x1fff) |
			    (osd_ext_hw.scaledata[OSD1].x_end & 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 = ((osd_ext_hw.scaledata[OSD1].y_start +
			    osd_ext_hw.pandata[OSD1].y_start) & 0x1fff)
				| ((osd_ext_hw.scaledata[OSD1].y_end +
			    osd_ext_hw.pandata[OSD1].y_start) & 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2, data32);

			/* adjust display x-axis */
			if (osd_ext_hw.scale[OSD1].h_enable) {
				data32 = (osd_ext_hw.dispdata[OSD1].x_start
					& 0xfff)
					| ((osd_ext_hw.dispdata[OSD1].x_start +
					(osd_ext_hw.scaledata[OSD1].x_end -
					osd_ext_hw.scaledata[OSD1].x_start) *
					2 + 1) & 0xfff) << 16;
				osd_reg_write(VIU2_OSD1_BLK0_CFG_W3, data32);
			}

			/* adjust display y-axis */
			dy0 = osd_ext_hw.dispdata[OSD1].y_start;
			sy0 = osd_ext_hw.scaledata[OSD1].y_start;
			sy1 = osd_ext_hw.scaledata[OSD1].y_end;
			if (osd_ext_hw.scale[OSD1].v_enable) {
				if (osd_ext_hw.scan_mode ==
						SCAN_MODE_INTERLACE) {
					data32 = ((dy0 >> 1) & 0xfff)
						| (((((dy0 + (sy1 - sy0) * 2)
						+ 1) >> 1) - 1) & 0xfff) << 16;
				} else {
					data32 = (dy0 & 0xfff)
						| (((dy0 + (sy1 - sy0) * 2))
						& 0xfff) << 16;
				}

				osd_reg_write(VIU2_OSD1_BLK0_CFG_W4, data32);
			}
		} else if (osd_ext_hw.free_scale_enable[OSD1]
			   && (osd_ext_hw.free_src_data[OSD1].x_end > 0)
			   && (osd_ext_hw.free_src_data[OSD1].y_end > 0)) {
			/* enable osd free scale */
			data32 = (osd_ext_hw.free_src_data[OSD1].x_start
					& 0x1fff)
				| (osd_ext_hw.free_src_data[OSD1].x_end
					& 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 = ((osd_ext_hw.free_src_data[OSD1].y_start +
				   osd_ext_hw.pandata[OSD1].y_start) & 0x1fff)
				| ((osd_ext_hw.free_src_data[OSD1].y_end +
				   osd_ext_hw.pandata[OSD1].y_start)
				  & 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2, data32);
		} else {
			/* normal mode */
			data32 =
				(osd_ext_hw.pandata[OSD1].x_start & 0x1fff) |
				(osd_ext_hw.pandata[OSD1].x_end & 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 =
				(osd_ext_hw.pandata[OSD1].y_start & 0x1fff) |
				(osd_ext_hw.pandata[OSD1].y_end & 0x1fff) << 16;
			osd_reg_write(VIU2_OSD1_BLK0_CFG_W2, data32);
		}

		data32 = osd_reg_read(VIU2_OSD1_CTRL_STAT);
		data32 &= 0xfffffff0;
		data32 |= HW_OSD_BLOCK_ENABLE_0;
		osd_reg_write(VIU2_OSD1_CTRL_STAT, data32);
	}

	remove_from_update_list(OSD1, DISP_GEOMETRY);
}

static void osd2_update_disp_geometry(void)
{
	u32 data32;

	data32 = (osd_ext_hw.dispdata[OSD2].x_start & 0xfff) |
		 (osd_ext_hw.dispdata[OSD2].x_end & 0xfff) << 16;
	osd_reg_write(VIU2_OSD2_BLK0_CFG_W3, data32);

	if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
		data32 = ((osd_ext_hw.dispdata[OSD2].y_start >> 1) & 0xfff)
			| ((((osd_ext_hw.dispdata[OSD2].y_end + 1) >> 1) - 1)
					& 0xfff) << 16;
	} else
		data32 = (osd_ext_hw.dispdata[OSD2].y_start & 0xfff) |
			 (osd_ext_hw.dispdata[OSD2].y_end & 0xfff) << 16;

	osd_reg_write(VIU2_OSD2_BLK0_CFG_W4, data32);

	if (osd_ext_hw.scale[OSD2].h_enable ||
			osd_ext_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
		data32 = (osd_ext_hw.pandata[OSD2].x_start & 0x1fff) |
			 (osd_ext_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_ext_hw.pandata[OSD2].y_start & 0x1fff) |
			 (osd_ext_hw.pandata[OSD2].y_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W2, data32);
#else
		data32 =
			(osd_ext_hw.scaledata[OSD2].x_start & 0x1fff) |
			(osd_ext_hw.scaledata[OSD2].x_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = ((osd_ext_hw.scaledata[OSD2].y_start +
			   osd_ext_hw.pandata[OSD2].y_start) & 0x1fff)
			 | ((osd_ext_hw.scaledata[OSD2].y_end +
				osd_ext_hw.pandata[OSD2].y_start)
					 & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W2, data32);
#endif
	} else {
		data32 = (osd_ext_hw.pandata[OSD2].x_start & 0x1fff) |
			 (osd_ext_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_ext_hw.pandata[OSD2].y_start & 0x1fff) |
			 (osd_ext_hw.pandata[OSD2].y_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W2, data32);
	}

	remove_from_update_list(OSD2, DISP_GEOMETRY);
}

static void osd1_update_disp_3d_mode(void)
{
	/*step 1 . set pan data */
	u32 data32;

	if (osd_ext_hw.mode_3d[OSD1].left_right == OSD_LEFT) {
		data32 = (osd_ext_hw.mode_3d[OSD1].l_start & 0x1fff) |
			 (osd_ext_hw.mode_3d[OSD1].l_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_ext_hw.mode_3d[OSD1].r_start & 0x1fff) |
			 (osd_ext_hw.mode_3d[OSD1].r_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD1_BLK0_CFG_W1, data32);
	}

	osd_ext_hw.mode_3d[OSD1].left_right ^= 1;
}

static void osd2_update_disp_3d_mode(void)
{
	u32 data32;

	if (osd_ext_hw.mode_3d[OSD2].left_right == OSD_LEFT) {
		data32 = (osd_ext_hw.mode_3d[OSD2].l_start & 0x1fff) |
			 (osd_ext_hw.mode_3d[OSD2].l_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_ext_hw.mode_3d[OSD2].r_start & 0x1fff) |
			 (osd_ext_hw.mode_3d[OSD2].r_end & 0x1fff) << 16;
		osd_reg_write(VIU2_OSD2_BLK0_CFG_W1, data32);
	}

	osd_ext_hw.mode_3d[OSD2].left_right ^= 1;
}

void osd_ext_init_hw(u32 logo_loaded)
{
	u32 group, idx, data32;

	for (group = 0; group < HW_OSD_COUNT; group++)
		for (idx = 0; idx < HW_REG_INDEX_MAX; idx++)
			osd_ext_hw.reg[group][idx].update_func =
				hw_func_array[group][idx];

	osd_ext_hw.updated[OSD1] = 0;
	osd_ext_hw.updated[OSD2] = 0;

	/* here we will init default value ,these value only set once . */
	if (!logo_loaded) {
		data32 = 1;         /* Set DDR request priority to be urgent */
		data32 |= 4 << 5;   /* hold_fifo_lines */
		data32 |= 3 << 10;  /* burst_len_sel: 3=64 */
		data32 |= 32 << 12; /* fifo_depth_val: 32*8=256 */

		osd_reg_write(VIU2_OSD1_FIFO_CTRL_STAT, data32);
		osd_reg_write(VIU2_OSD2_FIFO_CTRL_STAT, data32);

		osd_reg_set_mask(VPP2_MISC, VPP_POSTBLEND_EN);
		osd_reg_clr_mask(VPP2_MISC, VPP_PREBLEND_EN);
		osd_reg_clr_mask(VPP2_MISC,
				VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND);
		data32 = 0x1 << 0;	/* osd_ext_blk_enable */
		data32 |= OSD_GLOBAL_ALPHA_DEF << 12;
		data32 |= (1 << 21);
		osd_reg_write(VIU2_OSD1_CTRL_STAT, data32);
		osd_reg_write(VIU2_OSD2_CTRL_STAT, data32);
	}

#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	osd_reg_set_mask(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_ext_hw.order = OSD_ORDER_10;
#else
	osd_reg_clr_mask(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_ext_hw.order = OSD_ORDER_01;
#endif

	osd_ext_hw.enable[OSD2] = osd_ext_hw.enable[OSD1] = DISABLE;
	osd_ext_hw.fb_gem[OSD1].canvas_idx = OSD3_CANVAS_INDEX;
	osd_ext_hw.fb_gem[OSD2].canvas_idx = OSD4_CANVAS_INDEX;
	osd_ext_hw.gbl_alpha[OSD1] = OSD_GLOBAL_ALPHA_DEF;
	osd_ext_hw.gbl_alpha[OSD2] = OSD_GLOBAL_ALPHA_DEF;
	osd_ext_hw.color_info[OSD1] = NULL;
	osd_ext_hw.color_info[OSD2] = NULL;
	/* TODO */
	/* vf.width = vf.height = 0; */
	osd_ext_hw.color_key[OSD1] = osd_ext_hw.color_key[OSD2] = 0xffffffff;
	osd_ext_hw.free_scale_enable[OSD1] = 0;
	osd_ext_hw.free_scale_enable[OSD2] = 0;
	osd_ext_hw.scale[OSD1].h_enable = osd_ext_hw.scale[OSD1].v_enable = 0;
	osd_ext_hw.scale[OSD2].h_enable = osd_ext_hw.scale[OSD2].v_enable = 0;
	osd_ext_hw.mode_3d[OSD2].enable = osd_ext_hw.mode_3d[OSD1].enable = 0;
	osd_ext_hw.block_mode[OSD1] = osd_ext_hw.block_mode[OSD2] = 0;
	/*
	 * osd_ext_hw.rotation_pandata[OSD1].x_start =
	 *	osd_ext_hw.rotation_pandata[OSD1].y_start = 0;
	 * osd_ext_hw.rotation_pandata[OSD2].x_start =
	 *	osd_ext_hw.rotation_pandata[OSD2].y_start = 0;
	 * memset(osd_ext_hw.rotate, 0, sizeof(struct osd_rotate_s));
	 */
	osd_get_hw_para(&osd_hw);

#ifdef CONFIG_AMLOGIC_MEDIA_FB_OSD_SYNC_FENCE
	INIT_LIST_HEAD(&ext_post_fence_list);
	mutex_init(&ext_post_fence_list_lock);
#endif

#ifdef FIQ_VSYNC
	osd_ext_hw.fiq_handle_item.handle = vsync_isr;
	osd_ext_hw.fiq_handle_item.key = (u32) vsync_isr;
	osd_ext_hw.fiq_handle_item.name = "osd_ext_vsync";

	if (register_fiq_bridge_handle(&osd_ext_hw.fiq_handle_item))
#else
	if (request_irq(INT_VIU2_VSYNC, &vsync_isr, IRQF_SHARED,
				"osd_ext_vsync", osd_ext_setup))
#endif
		osd_log_err("can't request irq for vsync\n");

#ifdef FIQ_VSYNC
	request_fiq(INT_VIU2_VSYNC, &osd_ext_fiq_isr);
#endif
}

#if defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
void osd_ext_cursor_hw(s16 x, s16 y, s16 xstart, s16 ystart, u32 osd_ext_w,
		       u32 osd_ext_h, int index)
{
	struct pandata_s disp_tmp;

	if (index != 1)
		return;

	memcpy(&disp_tmp, &osd_ext_hw.dispdata[OSD1], sizeof(struct pandata_s));

	if (osd_ext_hw.scale[OSD2].h_enable &&
			(osd_ext_hw.scaledata[OSD2].x_start > 0)
	    && (osd_ext_hw.scaledata[OSD2].x_end > 0)) {
		x = x * osd_ext_hw.scaledata[OSD2].x_end /
			osd_ext_hw.scaledata[OSD2].x_start;

		if (osd_ext_hw.scaledata[OSD2].x_end >
				osd_ext_hw.scaledata[OSD2].x_start) {
			disp_tmp.x_start =
				osd_ext_hw.dispdata[OSD1].x_start *
				osd_ext_hw.scaledata[OSD2].x_end /
				osd_ext_hw.scaledata[OSD2].x_start;
			disp_tmp.x_end =
				osd_ext_hw.dispdata[OSD1].x_end *
				osd_ext_hw.scaledata[OSD2].x_end /
				osd_ext_hw.scaledata[OSD2].x_start;
		}
	}

	if (osd_ext_hw.scale[OSD2].v_enable &&
			(osd_ext_hw.scaledata[OSD2].y_start > 0)
	    && (osd_ext_hw.scaledata[OSD2].y_end > 0)) {
		y = y * osd_ext_hw.scaledata[OSD2].y_end /
			osd_ext_hw.scaledata[OSD2].y_start;

		if (osd_ext_hw.scaledata[OSD2].y_end >
				osd_ext_hw.scaledata[OSD2].y_start) {
			disp_tmp.y_start =
				osd_ext_hw.dispdata[OSD1].y_start *
				osd_ext_hw.scaledata[OSD2].y_end /
				osd_ext_hw.scaledata[OSD2].y_start;
			disp_tmp.y_end =
				osd_ext_hw.dispdata[OSD1].y_end *
				osd_ext_hw.scaledata[OSD2].y_end /
				osd_ext_hw.scaledata[OSD2].y_start;
		}
	}

	x += xstart;
	y += ystart;
	/**
	 * Use pandata to show a partial cursor when it is at the edge
	 * because the registers can't have negative values and because we
	 * need to manually clip the cursor when it is past the edge.
	 * The edge is hardcoded to the OSD0 area.
	 */
	osd_ext_hw.dispdata[OSD2].x_start = x;
	osd_ext_hw.dispdata[OSD2].y_start = y;

	if (x < disp_tmp.x_start) {
		/* if negative position, set osd to 0,y and pan. */
		if ((disp_tmp.x_start - x) < osd_ext_w) {
			osd_ext_hw.pandata[OSD2].x_start = disp_tmp.x_start - x;
			osd_ext_hw.pandata[OSD2].x_end = osd_ext_w - 1;
		}

		osd_ext_hw.dispdata[OSD2].x_start = 0;
	} else {
		osd_ext_hw.pandata[OSD2].x_start = 0;

		if (x + osd_ext_w > disp_tmp.x_end) {
			/*
			 * if past positive edge,
			 * set osd to inside of the edge and pan.
			 */
			if (x < osd_ext_hw.dispdata[OSD1].x_end)
				osd_ext_hw.pandata[OSD2].x_end =
					disp_tmp.x_end - x;
		} else
			osd_ext_hw.pandata[OSD2].x_end = osd_ext_w - 1;
	}

	if (y < disp_tmp.y_start) {
		if ((disp_tmp.y_start - y) < osd_ext_h) {
			osd_ext_hw.pandata[OSD2].y_start = disp_tmp.y_start - y;
			osd_ext_hw.pandata[OSD2].y_end = osd_ext_h - 1;
		}

		osd_ext_hw.dispdata[OSD2].y_start = 0;
	} else {
		osd_ext_hw.pandata[OSD2].y_start = 0;

		if (y + osd_ext_h > disp_tmp.y_end) {
			if (y < disp_tmp.y_end)
				osd_ext_hw.pandata[OSD2].y_end =
					disp_tmp.y_end - y;
		} else
			osd_ext_hw.pandata[OSD2].y_end = osd_ext_h - 1;
	}

	osd_ext_hw.dispdata[OSD2].x_end =
		osd_ext_hw.dispdata[OSD2].x_start +
		osd_ext_hw.pandata[OSD2].x_end -
		osd_ext_hw.pandata[OSD2].x_start;
	osd_ext_hw.dispdata[OSD2].y_end =
		osd_ext_hw.dispdata[OSD2].y_start +
		osd_ext_hw.pandata[OSD2].y_end -
		osd_ext_hw.pandata[OSD2].y_start;
	add_to_update_list(OSD2, DISP_GEOMETRY);
}
#endif /* CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR */

void osd_ext_suspend_hw(void)
{
	osd_ext_hw.reg_status_save =
		osd_reg_read(VPP2_MISC) & OSD_RELATIVE_BITS;

	osd_reg_clr_mask(VPP2_MISC, OSD_RELATIVE_BITS);
	osd_log_info("osd_ext_suspended\n");
}

void osd_ext_resume_hw(void)
{
	osd_reg_set_mask(VPP2_MISC, osd_ext_hw.reg_status_save);
	osd_log_info("osd_ext_resumed\n");
}

void osd_ext_clone_pan(u32 index)
{
	s32 offset = 0;
	s32 height_osd0 = 0;
	s32 height_osd2 = 0;
	u32 py0, py1;

	height_osd0 = osd_hw->pandata[index].y_end -
		osd_hw->pandata[index].y_start + 1;
	height_osd2 = osd_ext_hw.pandata[index].y_end -
		osd_ext_hw.pandata[index].y_start + 1;

	py0 = osd_ext_hw.pandata[index].y_start;
	py1 = osd_ext_hw.pandata[index].y_end;
	if (osd_ext_hw.clone[index]) {
		if (py0 < height_osd0) {
			if (py0 >= height_osd2)
				offset -= py1 - py0 + 1;
			else
				offset = 0;
		} else {
			if (py0 < height_osd2)
				offset += py1 - py0 + 1;
			else
				offset = 0;
		}

		osd_ext_hw.pandata[index].y_start += offset;
		osd_ext_hw.pandata[index].y_end += offset;
		py0 = osd_ext_hw.pandata[index].y_start;

		if (osd_ext_hw.angle[index])
			osd_ext_clone_update_pan(py0 ? 1 : 0);

		add_to_update_list(index, DISP_GEOMETRY);
		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_get_angle_hw(u32 index, u32 *angle)
{
	osd_log_info("get osd_ext[%d]->angle: %d\n",
			index, osd_ext_hw.angle[index]);
	*angle = osd_ext_hw.angle[index];
}

void osd_ext_set_angle_hw(u32 index, u32 angle)
{
#ifndef OSD_EXT_GE2D_CLONE_SUPPORT
	osd_log_info("++ osd_clone depends on GE2D module!\n");
	return;
#endif

	if (angle > 4) {
		osd_log_err("++ invalid angle: %d\n", angle);
		return;
	}

	if (osd_ext_hw.clone[index] == 0) {
		osd_log_info("++ set osd_ext[%d]->angle: %d->%d\n", index,
			     osd_ext_hw.angle[index], angle);
		osd_ext_clone_set_angle(angle);
		osd_ext_hw.angle[index] = angle;
	} else if (!((osd_ext_hw.angle[index] == 0) || (angle == 0))) {
		osd_log_info("++ set osd_ext[%d]->angle: %d->%d\n", index,
			     osd_ext_hw.angle[index], angle);
		osd_ext_clone_set_angle(angle);
		osd_ext_hw.angle[index] = angle;
		osd_ext_clone_pan(index);
	}
}

void osd_ext_get_clone_hw(u32 index, u32 *clone)
{
	osd_log_info("get osd_ext[%d]->clone: %d\n",
			index, osd_ext_hw.clone[index]);
	*clone = osd_ext_hw.clone[index];
}

void osd_ext_set_clone_hw(u32 index, u32 clone)
{
	static const struct color_bit_define_s *color_info[HW_OSD_COUNT] = {};
	static struct pandata_s pandata[HW_OSD_COUNT] = {};

	osd_log_info("++ set osd_ext[%d]->clone: %d->%d\n", index,
		     osd_ext_hw.clone[index], clone);
	osd_ext_hw.clone[index] = clone;

	if (osd_ext_hw.clone[index]) {
		if (osd_ext_hw.angle[index]) {
			osd_ext_hw.color_info[index] =
				osd_hw->color_info[index];
			osd_ext_clone_task_start();
		} else {
			color_info[index] = osd_ext_hw.color_info[index];
			osd_ext_hw.color_info[index] =
				osd_hw->color_info[index];
			memcpy(&pandata, &osd_ext_hw.pandata[index],
					sizeof(struct pandata_s));
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			if (cpu_type != MESON_CPU_MAJOR_ID_AXG)
				canvas_update_addr(
					osd_ext_hw.fb_gem[index].canvas_idx,
					osd_hw->fb_gem[index].addr);
#endif
		}
	} else {
		if (osd_ext_hw.angle[index])
			osd_ext_clone_task_stop();
		else {
			color_info[index] = osd_ext_hw.color_info[index];
#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
			if (cpu_type != MESON_CPU_MAJOR_ID_AXG)
				canvas_update_addr(
					osd_ext_hw.fb_gem[index].canvas_idx,
					osd_ext_hw.fb_gem[index].addr);
#endif
			osd_ext_hw.color_info[index] = color_info[index];
			memcpy(&osd_ext_hw.pandata[index], &pandata,
					sizeof(struct pandata_s));
		}
	}
	add_to_update_list(index, OSD_COLOR_MODE);
}
