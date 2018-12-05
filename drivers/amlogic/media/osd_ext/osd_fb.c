/*
 * drivers/amlogic/media/osd_ext/osd_fb.c
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
#include <linux/version.h>
#include <linux/compat.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>


/* Local Headers */
#include <osd/osd.h>
#include <osd/osd_log.h>
#include <osd/osd_sync.h>
#include <osd/osd_io.h>
#include "osd_hw.h"
#include "osd_fb.h"

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif
#ifdef CONFIG_SCREEN_ON_EARLY
static int early_resume_flag;
#endif

int int_viu2_vsync = -ENXIO;
static struct osd_fb_dev_s *gp_fbdev_list[OSD_COUNT] = {};
static struct reserved_mem fb_rmem;
static phys_addr_t fb_rmem_paddr[2];
static void __iomem *fb_rmem_vaddr[2];
static u32 fb_rmem_size[2];

static DEFINE_MUTEX(dbg_mutex);

static void osddev_ext_setup(struct osd_fb_dev_s *fbdev)
{
	mutex_lock(&fbdev->lock);
	osd_ext_setup(&fbdev->osd_ctl,
		      fbdev->fb_info->var.xoffset,
		      fbdev->fb_info->var.yoffset,
		      fbdev->fb_info->var.xres,
		      fbdev->fb_info->var.yres,
		      fbdev->fb_info->var.xres_virtual,
		      fbdev->fb_info->var.yres_virtual,
		      fbdev->osd_ctl.disp_start_x,
		      fbdev->osd_ctl.disp_start_y,
		      fbdev->osd_ctl.disp_end_x,
		      fbdev->osd_ctl.disp_end_y,
		      fbdev->fb_mem_paddr,
		      fbdev->color,
		      fbdev->fb_info->node - 2);
	mutex_unlock(&fbdev->lock);
}

static void osddev_ext_update_disp_axis(struct osd_fb_dev_s *fbdev,
					int  mode_change)
{
	osd_ext_update_disp_axis_hw(fbdev->fb_info->node - 2,
				    fbdev->osd_ctl.disp_start_x,
				    fbdev->osd_ctl.disp_end_x,
				    fbdev->osd_ctl.disp_start_y,
				    fbdev->osd_ctl.disp_end_y,
				    fbdev->fb_info->var.xoffset,
				    fbdev->fb_info->var.yoffset,
				    mode_change);
}

static int osddev_ext_setcolreg(unsigned int regno, u16 red,
				u16 green, u16 blue,
				u16 transp, struct osd_fb_dev_s *fbdev)
{
	struct fb_info *info = fbdev->fb_info;

	if ((fbdev->color->color_index == COLOR_INDEX_02_PAL4) ||
	    (fbdev->color->color_index == COLOR_INDEX_04_PAL16) ||
	    (fbdev->color->color_index == COLOR_INDEX_08_PAL256)) {
		mutex_lock(&fbdev->lock);
		osd_ext_setpal_hw(fbdev->fb_info->node - 2,
				regno, red, green, blue, transp);
		mutex_lock(&fbdev->lock);
	}
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v, r, g, b, a;

		if (regno >= 16)
			return 1;
		r = red    >> (16 - info->var.red.length);
		g = green  >> (16 - info->var.green.length);
		b = blue   >> (16 - info->var.blue.length);
		a = transp >> (16 - info->var.transp.length);
		v = (r << info->var.red.offset)   |
		    (g << info->var.green.offset) |
		    (b << info->var.blue.offset)  |
		    (a << info->var.transp.offset);
		((u32 *)(info->pseudo_palette))[regno] = v;
	}
	return 0;
}

static const struct color_bit_define_s *
_find_color_format(struct fb_var_screeninfo *var)
{
	u32 upper_margin, lower_margin, i, level;
	const struct color_bit_define_s *found = NULL;
	const struct color_bit_define_s *color = NULL;

	level = (var->bits_per_pixel - 1) / 8;
	switch (level) {
	case 0:
		upper_margin = COLOR_INDEX_08_PAL256;
		lower_margin = COLOR_INDEX_02_PAL4;
		break;
	case 1:
		upper_margin = COLOR_INDEX_16_565;
		lower_margin = COLOR_INDEX_16_655;
		break;
	case 2:
		upper_margin = COLOR_INDEX_24_RGB;
		lower_margin = COLOR_INDEX_24_6666_A;
		break;
	case 3:
		upper_margin = COLOR_INDEX_32_ARGB;
		lower_margin = COLOR_INDEX_32_BGRA;
		break;
	case 4:
		upper_margin = COLOR_INDEX_YUV_422;
		lower_margin = COLOR_INDEX_YUV_422;
		break;
	default:
		return NULL;
	}
	/*
	 * if not provide color component length
	 * then we find the first depth match.
	 */
	if ((var->red.length == 0) || (var->green.length == 0)
	    || (var->blue.length == 0) ||
	    var->bits_per_pixel != (var->red.length + var->green.length +
		    var->blue.length + var->transp.length)) {
		osd_log_dbg(MODULE_BASE, "not provide color length, use default color\n");
		found = &default_color_format_array[upper_margin];
	} else {
		for (i = upper_margin; i >= lower_margin; i--) {
			color = &default_color_format_array[i];
			if ((color->red_length == var->red.length) &&
			    (color->green_length == var->green.length) &&
			    (color->blue_length == var->blue.length) &&
			    (color->transp_length == var->transp.length) &&
			    (color->transp_offset == var->transp.offset) &&
			    (color->green_offset == var->green.offset) &&
			    (color->blue_offset == var->blue.offset) &&
			    (color->red_offset == var->red.offset)) {
				found = color;
				break;
			}
			color--;
		}
	}
	return found;
}

static void __init _fbdev_set_default(struct osd_fb_dev_s *fbdev, int index)
{
	/* setup default value */
	fbdev->fb_info->var = fb_def_var[index];
	fbdev->fb_info->fix = fb_def_fix;
	fbdev->color = _find_color_format(&fbdev->fb_info->var);
}

static int osd_ext_check_var(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct fb_fix_screeninfo *fix;
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)info->par;
	const struct color_bit_define_s *color_format_pt;

	fix = &info->fix;
	color_format_pt = _find_color_format(var);
	if (color_format_pt == NULL || color_format_pt->color_index == 0)
		return -EFAULT;
	osd_log_dbg(MODULE_BASE, "select color format :index %d, bpp %d\n",
		    color_format_pt->color_index,
		    color_format_pt->bpp);
	fbdev->color = color_format_pt;
	var->red.offset = color_format_pt->red_offset;
	var->red.length = color_format_pt->red_length;
	var->red.msb_right = color_format_pt->red_msb_right;
	var->green.offset  = color_format_pt->green_offset;
	var->green.length  = color_format_pt->green_length;
	var->green.msb_right = color_format_pt->green_msb_right;
	var->blue.offset   = color_format_pt->blue_offset;
	var->blue.length   = color_format_pt->blue_length;
	var->blue.msb_right = color_format_pt->blue_msb_right;
	var->transp.offset = color_format_pt->transp_offset;
	var->transp.length = color_format_pt->transp_length;
	var->transp.msb_right = color_format_pt->transp_msb_right;
	var->bits_per_pixel = color_format_pt->bpp;
	osd_log_dbg(MODULE_BASE, "rgba(L/O):%d/%d-%d/%d-%d/%d-%d/%d\n",
		    var->red.length, var->red.offset,
		    var->green.length, var->green.offset,
		    var->blue.length, var->blue.offset,
		    var->transp.length, var->transp.offset);
	fix->visual = color_format_pt->color_type;
	/* adjust memory length. */
	fix->line_length = var->xres_virtual * var->bits_per_pixel / 8;
	if (var->xres_virtual * var->yres_virtual * var->bits_per_pixel / 8 >
			fbdev->fb_len) {
		osd_log_err("no enough memory for %d*%d*%d\n", var->xres,
			var->yres, var->bits_per_pixel);
		return  -ENOMEM;
	}
	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;
	var->left_margin = var->right_margin = 0;
	var->upper_margin = var->lower_margin = 0;
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;
	return 0;
}

static int osd_ext_set_par(struct fb_info *info)
{
	const struct vinfo_s *vinfo = NULL;
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)info->par;
	struct osd_ctl_s *osd_ext_ctrl = &fbdev->osd_ctl;
	u32  virt_end_x, virt_end_y;

#ifdef CONFIG_AMLOGIC_VOUT2
	vinfo = get_current_vinfo2();
	if (!vinfo) {
		osd_log_err("current vinfo NULL\n");
		return -1;
	}
#endif

	if (vinfo == NULL)
		return -1;

	virt_end_x = osd_ext_ctrl->disp_start_x + info->var.xres;
	virt_end_y = osd_ext_ctrl->disp_start_y + info->var.yres;

	if (virt_end_x > vinfo->width)
		osd_ext_ctrl->disp_end_x = vinfo->width - 1;
	else
		osd_ext_ctrl->disp_end_x = virt_end_x - 1;
	if (virt_end_y  > vinfo->height)
		osd_ext_ctrl->disp_end_y = vinfo->height - 1;
	else
		osd_ext_ctrl->disp_end_y = virt_end_y - 1;
	osddev_ext_setup((struct osd_fb_dev_s *)info->par);
	return  0;
}

static int osd_ext_setcolreg(unsigned int regno, unsigned int red,
		unsigned int green, unsigned int blue,
		unsigned int transp, struct fb_info *info)
{
	return osddev_ext_setcolreg(regno, red, green, blue,
				    transp, (struct osd_fb_dev_s *)info->par);
}

static int osd_ext_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int count, index, r;
	u16 *red, *green, *blue, *transp;
	u16 trans = 0xffff;

	red     = cmap->red;
	green   = cmap->green;
	blue    = cmap->blue;
	transp  = cmap->transp;
	index   = cmap->start;

	for (count = 0; count < cmap->len; count++) {
		if (transp)
			trans = *transp++;
		r = osddev_ext_setcolreg(index++, *red++, *green++, *blue++,
				trans, (struct osd_fb_dev_s *)info->par);
		if (r != 0)
			return r;
	}

	return 0;
}

static int osd_ext_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)info->par;
	void __user *argp = (void __user *)arg;
	u32 src_colorkey;/* 16 bit or 24 bit */
	u32 srckey_enable;
	u32 gbl_alpha;
	u32 osd_ext_order;
	s32 osd_ext_axis[4] = {0};
	s32 osd_ext_dst_axis[4] = {0};
	u32 block_windows[8] = {0};
	u32 block_mode;
	unsigned long ret;
	struct fb_sync_request_s sync_request;

	switch (cmd) {
	case  FBIOPUT_OSD_SRCKEY_ENABLE:
		ret = copy_from_user(&srckey_enable, argp, sizeof(u32));
		break;
	case  FBIOPUT_OSD_SRCCOLORKEY:
		ret = copy_from_user(&src_colorkey, argp, sizeof(u32));
		break;
	case FBIOPUT_OSD_SET_GBL_ALPHA:
		ret = copy_from_user(&gbl_alpha, argp, sizeof(u32));
		break;
	case FBIOPUT_OSD_SCALE_AXIS:
		ret = copy_from_user(&osd_ext_axis, argp, 4 * sizeof(s32));
		break;
	case FBIOPUT_OSD_SYNC_ADD:
		ret = copy_from_user(&sync_request, argp,
				sizeof(struct fb_sync_request_s));
		break;
	case FBIO_WAITFORVSYNC:
	case FBIOGET_OSD_SCALE_AXIS:
	case FBIOPUT_OSD_ORDER:
	case FBIOGET_OSD_ORDER:
	case FBIOGET_OSD_GET_GBL_ALPHA:
	case FBIOPUT_OSD_2X_SCALE:
	case FBIOPUT_OSD_ENABLE_3D_MODE:
	case FBIOPUT_OSD_FREE_SCALE_ENABLE:
	case FBIOPUT_OSD_FREE_SCALE_MODE:
	case FBIOPUT_OSD_FREE_SCALE_WIDTH:
	case FBIOPUT_OSD_FREE_SCALE_HEIGHT:
	case FBIOGET_OSD_BLOCK_WINDOWS:
	case FBIOGET_OSD_BLOCK_MODE:
	case FBIOGET_OSD_FREE_SCALE_AXIS:
	case FBIOGET_OSD_WINDOW_AXIS:
	case FBIOPUT_OSD_ROTATE_ON:
	case FBIOPUT_OSD_ROTATE_ANGLE:
		break;
	case FBIOPUT_OSD_BLOCK_MODE:
		ret = copy_from_user(&block_mode, argp, sizeof(u32));
		break;
	case FBIOPUT_OSD_BLOCK_WINDOWS:
		ret = copy_from_user(&block_windows, argp, 8 * sizeof(u32));
		break;
	case FBIOPUT_OSD_FREE_SCALE_AXIS:
		ret = copy_from_user(&osd_ext_axis, argp, 4 * sizeof(s32));
		break;
	case FBIOPUT_OSD_WINDOW_AXIS:
		ret = copy_from_user(&osd_ext_dst_axis, argp, 4 * sizeof(s32));
		break;
	default:
		osd_log_err("command not supported\n ");
		return -1;
	}
	mutex_lock(&fbdev->lock);

	switch (cmd) {
	case FBIOPUT_OSD_ORDER:
		osd_ext_set_order_hw(info->node - 2, arg);
		break;
	case FBIOGET_OSD_ORDER:
		osd_ext_order = osd_ext_get_order_hw(info->node - 2);
		ret = copy_to_user(argp, &osd_ext_order, sizeof(u32));
		break;
	case FBIOPUT_OSD_FREE_SCALE_ENABLE:
		osd_ext_set_free_scale_enable_hw(info->node - 2, arg);
		break;
	case FBIOPUT_OSD_FREE_SCALE_MODE:
		osd_ext_set_free_scale_mode_hw(info->node - 2, arg);
		break;
	case FBIOPUT_OSD_ENABLE_3D_MODE:
		osd_ext_enable_3d_mode_hw(info->node - 2, arg);
		break;
	case FBIOPUT_OSD_2X_SCALE:
		/*
		 * higher 16 bit h_scale_enable,
		 * lower 16 bit v_scale_enable
		 */
		osd_ext_set_2x_scale_hw(info->node - 2, arg & 0xffff0000 ?
				1 : 0, arg & 0xffff ? 1 : 0);
		break;
	case FBIOPUT_OSD_ROTATE_ON:
		break;
	case FBIOPUT_OSD_ROTATE_ANGLE:
		break;
	case FBIOPUT_OSD_SRCCOLORKEY:
		switch (fbdev->color->color_index) {
		case COLOR_INDEX_16_655:
		case COLOR_INDEX_16_844:
		case COLOR_INDEX_16_565:
		case COLOR_INDEX_24_888_B:
		case COLOR_INDEX_24_RGB:
		case COLOR_INDEX_YUV_422:
			osd_log_dbg(MODULE_BASE,
				"set osd color key 0x%x\n", src_colorkey);
			fbdev->color_key = src_colorkey;
			osd_ext_set_color_key_hw(info->node - 2,
				fbdev->color->color_index, src_colorkey);
			break;
		default:
			break;
		}
		break;
	case FBIOPUT_OSD_SRCKEY_ENABLE:
		switch (fbdev->color->color_index) {
		case COLOR_INDEX_16_655:
		case COLOR_INDEX_16_844:
		case COLOR_INDEX_16_565:
		case COLOR_INDEX_24_888_B:
		case COLOR_INDEX_24_RGB:
		case COLOR_INDEX_YUV_422:
			osd_log_dbg(MODULE_BASE, "set osd color key %s\n",
					srckey_enable ? "enable" : "disable");
			if (srckey_enable != 0) {
				fbdev->enable_key_flag |= KEYCOLOR_FLAG_TARGET;
				if (!(fbdev->enable_key_flag &
						KEYCOLOR_FLAG_ONHOLD)) {
					fbdev->enable_key_flag |=
						KEYCOLOR_FLAG_CURRENT;
				    osd_ext_srckey_enable_hw(info->node - 2, 1);
				}
			} else {
				osd_ext_srckey_enable_hw(info->node - 2, 0);
				fbdev->enable_key_flag &=
					~(KEYCOLOR_FLAG_TARGET |
						KEYCOLOR_FLAG_CURRENT);
			}
			break;
		default:
			break;
		}
		break;
	case FBIOPUT_OSD_SET_GBL_ALPHA:
		osd_ext_set_gbl_alpha_hw(info->node - 2, gbl_alpha);
		break;
	case  FBIOGET_OSD_GET_GBL_ALPHA:
		gbl_alpha = osd_ext_get_gbl_alpha_hw(info->node - 2);
		ret = copy_to_user(argp, &gbl_alpha, sizeof(u32));
		break;

	case FBIOGET_OSD_SCALE_AXIS:
		osd_ext_get_scale_axis_hw(info->node - 2,
				&osd_ext_axis[0], &osd_ext_axis[1],
				&osd_ext_axis[2], &osd_ext_axis[3]);
		ret = copy_to_user(argp, &osd_ext_axis, 4 * sizeof(s32));
		break;
	case FBIOPUT_OSD_SCALE_AXIS:
		osd_ext_set_scale_axis_hw(info->node - 2,
				osd_ext_axis[0], osd_ext_axis[1],
				osd_ext_axis[2], osd_ext_axis[3]);
		break;
	case FBIOGET_OSD_BLOCK_WINDOWS:
		osd_ext_get_block_windows_hw(info->node - 2, block_windows);
		ret = copy_to_user(argp, &block_windows, 8 * sizeof(u32));
		break;
	case FBIOPUT_OSD_BLOCK_WINDOWS:
		osd_ext_set_block_windows_hw(info->node - 2, block_windows);
		break;
	case FBIOPUT_OSD_BLOCK_MODE:
		osd_ext_set_block_mode_hw(info->node - 2, block_mode);
		break;
	case FBIOGET_OSD_BLOCK_MODE:
		osd_ext_get_block_mode_hw(info->node - 2, &block_mode);
		ret = copy_to_user(argp, &block_mode, sizeof(u32));
		break;
	case FBIOGET_OSD_FREE_SCALE_AXIS:
		osd_ext_get_free_scale_axis_hw(info->node - 2,
				&osd_ext_axis[0], &osd_ext_axis[1],
				&osd_ext_axis[2], &osd_ext_axis[3]);
		ret = copy_to_user(argp, &osd_ext_axis, 4 * sizeof(s32));
		break;
	case FBIOGET_OSD_WINDOW_AXIS:
		osd_ext_get_window_axis_hw(info->node - 2,
				&osd_ext_dst_axis[0], &osd_ext_dst_axis[1],
				&osd_ext_dst_axis[2], &osd_ext_dst_axis[3]);
		ret = copy_to_user(argp, &osd_ext_dst_axis, 4 * sizeof(s32));
		break;
	case FBIOPUT_OSD_FREE_SCALE_AXIS:
		osd_ext_set_free_scale_axis_hw(info->node - 2,
				osd_ext_axis[0], osd_ext_axis[1],
				osd_ext_axis[2], osd_ext_axis[3]);
		break;
	case FBIOPUT_OSD_WINDOW_AXIS:
		osd_ext_set_window_axis_hw(info->node - 2,
				osd_ext_dst_axis[0], osd_ext_dst_axis[1],
				osd_ext_dst_axis[2], osd_ext_dst_axis[3]);
		break;
	case FBIOPUT_OSD_SYNC_ADD:
		sync_request.out_fen_fd = osd_ext_sync_request(info->node - 2,
				info->var.yres,
				sync_request.xoffset,
				sync_request.yoffset,
				sync_request.in_fen_fd);
		ret = copy_to_user(argp, &sync_request,
				sizeof(struct fb_sync_request_s));
		if (sync_request.out_fen_fd < 0) /* fence create fail. */
			ret = -1;
		break;
	case FBIO_WAITFORVSYNC:
		osd_ext_wait_vsync_event();
		ret = copy_to_user(argp, &ret, sizeof(u32));
		break;
	default:
		break;
	}

	mutex_unlock(&fbdev->lock);

	return  0;
}

#ifdef CONFIG_COMPAT
static int osd_ext_compat_ioctl(struct fb_info *info,
		unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = osd_ext_ioctl(info, cmd, arg);

	return ret;
}
#endif

static int osd_ext_open(struct fb_info *info, int arg)
{
	return 0;
}

static int
osd_ext_blank(int blank_mode, struct fb_info *info)
{
	osd_ext_enable_hw(info->node - 2, (blank_mode != 0) ? 0 : 1);

	return 0;
}

static int osd_ext_pan_display(struct fb_var_screeninfo *var,
			       struct fb_info *fbi)
{

	osd_ext_pan_display_hw(fbi->node - 2, var->xoffset, var->yoffset);
	osd_log_info("osd_ext_pan_display:=>osd%d\n", fbi->node);
	return 0;
}

static int osd_ext_sync(struct fb_info *info)
{
	return 0;
}

/* fb_ops structures */
static struct fb_ops osd_ext_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var   = osd_ext_check_var,
	.fb_set_par     = osd_ext_set_par,
	.fb_setcolreg   = osd_ext_setcolreg,
	.fb_setcmap     = osd_ext_setcmap,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
#ifdef CONFIG_FB_SOFT_CURSOR
	.fb_cursor      = soft_cursor,
#elif defined(CONFIG_AMLOGIC_MEDIA_FB_OSD2_CURSOR)
	.fb_cursor      = NULL,
#endif
	.fb_ioctl       = osd_ext_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = osd_ext_compat_ioctl,
#endif
	.fb_open        = osd_ext_open,
	.fb_blank       = osd_ext_blank,
	.fb_pan_display = osd_ext_pan_display,
	.fb_sync        = osd_ext_sync,
};

static void set_default_display_axis(struct fb_var_screeninfo *var,
		struct osd_ctl_s *osd_ext_ctrl, const struct vinfo_s *vinfo)
{
	u32 virt_end_x = osd_ext_ctrl->disp_start_x + var->xres;
	u32 virt_end_y = osd_ext_ctrl->disp_start_y + var->yres;

	if (virt_end_x > vinfo->width)
		osd_ext_ctrl->disp_end_x = vinfo->width - 1; /* screen axis */
	else
		osd_ext_ctrl->disp_end_x = virt_end_x - 1; /* screen axis */
	if (virt_end_y > vinfo->height)
		osd_ext_ctrl->disp_end_y = vinfo->height - 1;
	else
		osd_ext_ctrl->disp_end_y = virt_end_y - 1; /* screen axis */
}

int osd_ext_notify_callback(struct notifier_block *block, unsigned long cmd,
		void *para)
{
	const struct vinfo_s *vinfo = NULL;
	struct osd_fb_dev_s *fb_dev;
	int  i, blank;
	struct disp_rect_s *disp_rect;

#ifdef CONFIG_AMLOGIC_VOUT2
	vinfo = get_current_vinfo2();
	osd_log_info("tv_server:vmode=%s\n", vinfo->name);
#endif
	if (vinfo == NULL)
		return -1;

	if (vinfo->mode == VMODE_INIT_NULL)
		return 1;

	switch (cmd) {
	case  VOUT_EVENT_MODE_CHANGE:
		osd_log_info("recevie change mode  message\n");
		for (i = 0; i < OSD_COUNT; i++) {
			fb_dev = gp_fbdev_list[i];
			if (fb_dev == NULL)
				continue;
			set_default_display_axis(&fb_dev->fb_info->var,
					&fb_dev->osd_ctl, vinfo);
			console_lock();
			osddev_ext_update_disp_axis(fb_dev, 1);
			console_unlock();
		}
		break;

	case VOUT_EVENT_OSD_BLANK:
		blank = *(int *)para;
		for (i = 0; i < OSD_COUNT; i++) {
			fb_dev = gp_fbdev_list[i];
			if (fb_dev == NULL)
				continue;
			console_lock();
			osd_ext_blank(blank, fb_dev->fb_info);
			console_unlock();
		}
		break;
	case   VOUT_EVENT_OSD_DISP_AXIS:
		disp_rect = (struct disp_rect_s *)para;

		for (i = 0; i < OSD_COUNT; i++) {
			if (!disp_rect)
				break;
			fb_dev = gp_fbdev_list[i];
			/*
			 * if (fb_dev->preblend_enable)
			 *	break;
			 */
			fb_dev->osd_ctl.disp_start_x = disp_rect->x;
			fb_dev->osd_ctl.disp_start_y = disp_rect->y;
			osd_log_info("set disp axis: x:%d y:%d w:%d h:%d\n",
				     disp_rect->x, disp_rect->y,
				     disp_rect->w, disp_rect->h);
			if (disp_rect->x + disp_rect->w > vinfo->width)
				fb_dev->osd_ctl.disp_end_x = vinfo->width - 1;
			else
				fb_dev->osd_ctl.disp_end_x =
					fb_dev->osd_ctl.disp_start_x +
					disp_rect->w - 1;
			if (disp_rect->y + disp_rect->h > vinfo->height)
				fb_dev->osd_ctl.disp_end_y = vinfo->height - 1;
			else
				fb_dev->osd_ctl.disp_end_y =
					fb_dev->osd_ctl.disp_start_y +
					disp_rect->h - 1;
			disp_rect++;
			osd_log_info("new disp axis: x0:%d y0:%d x1:%d y1:%d\n",
				     fb_dev->osd_ctl.disp_start_x,
				     fb_dev->osd_ctl.disp_start_y,
				     fb_dev->osd_ctl.disp_end_x,
				     fb_dev->osd_ctl.disp_end_y);
			console_lock();
			osddev_ext_update_disp_axis(fb_dev, 0);
			console_unlock();
		}

		break;
	}
	return 0;
}

static struct notifier_block osd_ext_notifier_nb = {
	.notifier_call	= osd_ext_notify_callback,
};

static ssize_t store_enable_3d(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	fbdev->enable_3d = res;
	osd_ext_enable_3d_mode_hw(fb_info->node - 2, fbdev->enable_3d);

	return count;
}

static ssize_t show_enable_3d(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	return snprintf(buf, PAGE_SIZE, "3d_enable:[0x%x]\n", fbdev->enable_3d);
}

static ssize_t store_color_key(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 16, &res);
	switch (fbdev->color->color_index) {
	case COLOR_INDEX_16_655:
	case COLOR_INDEX_16_844:
	case COLOR_INDEX_16_565:
	case COLOR_INDEX_24_888_B:
	case COLOR_INDEX_24_RGB:
	case COLOR_INDEX_YUV_422:
		fbdev->color_key = res;
		osd_ext_set_color_key_hw(fb_info->node - 2,
				fbdev->color->color_index, fbdev->color_key);
		break;
	default:
		break;
	}
	return count;
}

static ssize_t show_color_key(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	return snprintf(buf, PAGE_SIZE, "0x%x\n", fbdev->color_key);
}

static ssize_t store_enable_key(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	mutex_lock(&fbdev->lock);
	if (res != 0) {
		fbdev->enable_key_flag |= KEYCOLOR_FLAG_TARGET;
		if (!(fbdev->enable_key_flag & KEYCOLOR_FLAG_ONHOLD)) {
			osd_ext_srckey_enable_hw(fb_info->node - 2, 1);
			fbdev->enable_key_flag |= KEYCOLOR_FLAG_CURRENT;
		}
	} else {
		fbdev->enable_key_flag &=
			~(KEYCOLOR_FLAG_TARGET | KEYCOLOR_FLAG_CURRENT);
		osd_ext_srckey_enable_hw(fb_info->node - 2, 0);
	}
	mutex_unlock(&fbdev->lock);

	return count;
}

static ssize_t show_enable_key(struct device *device,
			       struct device_attribute *attr,
			       char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	return snprintf(buf, PAGE_SIZE,
			(fbdev->enable_key_flag & KEYCOLOR_FLAG_TARGET) ?
			"1\n" : "0\n");
}

static ssize_t store_enable_key_onhold(struct device *device,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	mutex_lock(&fbdev->lock);
	if (res != 0) {
		/* hold all the calls to enable color key */
		fbdev->enable_key_flag |= KEYCOLOR_FLAG_ONHOLD;
		fbdev->enable_key_flag &= ~KEYCOLOR_FLAG_CURRENT;
		osd_ext_srckey_enable_hw(fb_info->node - 2, 0);

	} else {
		fbdev->enable_key_flag &= ~KEYCOLOR_FLAG_ONHOLD;
		/*
		 * if target and current mistach
		 * then recover the pending key settings
		 */
		if (fbdev->enable_key_flag & KEYCOLOR_FLAG_TARGET) {
			if ((fbdev->enable_key_flag &
						KEYCOLOR_FLAG_CURRENT) == 0) {
				fbdev->enable_key_flag |= KEYCOLOR_FLAG_CURRENT;
				osd_ext_srckey_enable_hw(fb_info->node - 2, 1);
			}
		} else {
			if (fbdev->enable_key_flag & KEYCOLOR_FLAG_CURRENT) {
				fbdev->enable_key_flag &=
					~KEYCOLOR_FLAG_CURRENT;
				osd_ext_srckey_enable_hw(fb_info->node - 2, 0);
			}
		}
	}
	mutex_unlock(&fbdev->lock);

	return count;
}

static ssize_t show_enable_key_onhold(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	return snprintf(buf, PAGE_SIZE,
			(fbdev->enable_key_flag & KEYCOLOR_FLAG_ONHOLD) ?
			"1\n" : "0\n");
}

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token)
				|| !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

static ssize_t show_free_scale_axis(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int x, y, w, h;

	osd_ext_get_free_scale_axis_hw(fb_info->node - 2, &x, &y, &w, &h);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", x, y, w, h);
}

static ssize_t store_free_scale_axis(struct device *device,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int parsed[4];

	if (likely(parse_para(buf, 4, parsed) == 4))
		osd_ext_set_free_scale_axis_hw(fb_info->node - 2,
				parsed[0], parsed[1], parsed[2], parsed[3]);
	else
		osd_log_err("set free scale axis error\n");

	return count;
}

static ssize_t show_scale_axis(struct device *device,
			       struct device_attribute *attr,
			       char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int x0, y0, x1, y1;

	osd_ext_get_scale_axis_hw(fb_info->node - 2, &x0, &y0, &x1, &y1);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", x0, y0, x1, y1);
}

static ssize_t store_scale_axis(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int parsed[4];

	if (likely(parse_para(buf, 4, parsed) == 4))
		osd_ext_set_scale_axis_hw(fb_info->node - 2,
				parsed[0], parsed[1], parsed[2], parsed[3]);
	else
		osd_log_err("set scale axis error\n");

	return count;
}


static ssize_t show_scale_width(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_width = 0;

	osd_ext_get_free_scale_width_hw(fb_info->node - 2, &free_scale_width);
	return snprintf(buf, PAGE_SIZE, "free_scale_width:[0x%x]\n",
			free_scale_width);
}

static ssize_t show_scale_height(struct device *device,
				 struct device_attribute *attr,
				 char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_height = 0;

	osd_ext_get_free_scale_height_hw(fb_info->node - 2, &free_scale_height);
	return snprintf(buf, PAGE_SIZE, "free_scale_height:[0x%x]\n",
			free_scale_height);
}

static ssize_t store_free_scale(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_enable = 0;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	free_scale_enable = res;
	osd_ext_set_free_scale_enable_hw(fb_info->node - 2, free_scale_enable);

	return count;
}

static ssize_t show_free_scale(struct device *device,
			       struct device_attribute *attr,
			       char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_enable = 0;

	osd_ext_get_free_scale_enable_hw(fb_info->node - 2, &free_scale_enable);
	return snprintf(buf, PAGE_SIZE, "free_scale_enable:[0x%x]\n",
			free_scale_enable);
}

static ssize_t show_scale(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	return snprintf(buf, PAGE_SIZE, "scale:[0x%x]\n", fbdev->scale);
}

static ssize_t store_scale(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);

	fbdev->scale = res;
	osd_ext_set_2x_scale_hw(fb_info->node - 2,
			fbdev->scale & 0xffff0000 ? 1 : 0,
			fbdev->scale & 0xffff ? 1 : 0);

	return count;
}


static ssize_t show_debug(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	char help[] = "Usage:\n"
		      " echo val > debug ; show osd pan/display/scale value\n"
		      "	echo reg > debug ; Show osd register value\n";

	return snprintf(buf, sizeof(help), "%s", help);
}

static ssize_t store_debug(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int debug_flag = 0;

	if (!buf)
		return count;

	if (strncmp(buf, "val", 3) == 0)
		debug_flag = 1;
	else if (strncmp(buf, "reg", 3) == 0)
		debug_flag = 2;
	osd_ext_set_debug_hw(fb_info->node - 2, debug_flag);


	return count;
}

static ssize_t store_order(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	fbdev->order = res;
	osd_ext_set_order_hw(fb_info->node - 2, fbdev->order);

	return count;
}

static ssize_t show_order(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	struct osd_fb_dev_s *fbdev = (struct osd_fb_dev_s *)fb_info->par;

	fbdev->order = osd_ext_get_order_hw(fb_info->node - 2);

	return snprintf(buf, PAGE_SIZE, "order:[0x%x]\n", fbdev->order);
}

static ssize_t show_block_windows(struct device *device,
				  struct device_attribute *attr,
				  char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 wins[8];

	osd_ext_get_block_windows_hw(fb_info->node - 2, wins);

	return snprintf(buf, PAGE_SIZE,
			"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			wins[0], wins[1], wins[2], wins[3],
			wins[4], wins[5], wins[6], wins[7]);
}

static ssize_t store_block_windows(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int parsed[8];

	if (likely(parse_para(buf, 8, parsed) == 8))
		osd_ext_set_block_windows_hw(fb_info->node - 2, parsed);
	else
		osd_log_err("set block windows error\n");

	return count;
}

static ssize_t show_block_mode(struct device *device,
			       struct device_attribute *attr,
			       char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 mode;

	osd_ext_get_block_mode_hw(fb_info->node - 2, &mode);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", mode);
}

static ssize_t store_block_mode(struct device *device,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 mode;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	mode = res;
	osd_ext_set_block_mode_hw(fb_info->node - 2, mode);

	return count;
}

static ssize_t store_freescale_mode(struct device *device,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_mode = 0;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	free_scale_mode = res;
	osd_ext_set_free_scale_mode_hw(fb_info->node - 2, free_scale_mode);

	return count;
}

static ssize_t show_freescale_mode(struct device *device,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	unsigned int free_scale_mode = 0;

	osd_ext_get_free_scale_mode_hw(fb_info->node - 2, &free_scale_mode);

	return snprintf(buf, PAGE_SIZE, "free_scale_mode:%s\n",
			free_scale_mode ? "new" : "default");
}

static ssize_t show_window_axis(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	int x0, y0, x1, y1;

	osd_ext_get_window_axis_hw(fb_info->node - 2, &x0, &y0, &x1, &y1);

	return snprintf(buf, PAGE_SIZE,
			"window axis is [%d %d %d %d]\n",
			x0, y0, x1, y1);
}

static ssize_t store_window_axis(struct device *device,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	s32 parsed[4];

	if (likely(parse_para(buf, 4, parsed) == 4))
		osd_ext_set_window_axis_hw(fb_info->node - 2,
				parsed[0], parsed[1], parsed[2], parsed[3]);
	else
		osd_log_err("set window axis error\n");

	return count;
}

static ssize_t show_clone(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 clone;

	osd_ext_get_clone_hw(fb_info->node - 2, &clone);

	return snprintf(buf, PAGE_SIZE, "osd_ext->clone: [0x%x]\n", clone);
}

static ssize_t store_clone(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 clone;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	clone = res;
	osd_ext_set_clone_hw(fb_info->node - 2, clone);

	return count;
}

static ssize_t show_angle(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 angle;

	osd_ext_get_angle_hw(fb_info->node - 2, &angle);

	return snprintf(buf, PAGE_SIZE, "osd_ext->angle: [0x%x]\n", angle);
}

static ssize_t store_angle(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);
	u32 angle;
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	angle = res;
	osd_ext_set_angle_hw(fb_info->node - 2, angle);

	return count;
}

static struct device_attribute osd_ext_attrs[] = {
	__ATTR(scale, 0664,
			show_scale, store_scale),
	__ATTR(order, 0664,
			show_order, store_order),
	__ATTR(enable_3d, 0644,
			show_enable_3d, store_enable_3d),
	__ATTR(free_scale, 0664,
			show_free_scale, store_free_scale),
	__ATTR(scale_axis, 0644,
			show_scale_axis, store_scale_axis),
	__ATTR(scale_width, 0644,
			show_scale_width, NULL),
	__ATTR(scale_height, 0644,
			show_scale_height, NULL),
	__ATTR(color_key, 0644,
			show_color_key, store_color_key),
	__ATTR(enable_key, 0644,
			show_enable_key, store_enable_key),
	__ATTR(enable_key_onhold, 0644,
			show_enable_key_onhold, store_enable_key_onhold),
	__ATTR(block_windows, 0644,
			show_block_windows, store_block_windows),
	__ATTR(block_mode, 0644,
			show_block_mode, store_block_mode),
	__ATTR(free_scale_axis, 0644,
			show_free_scale_axis, store_free_scale_axis),
	__ATTR(debug, 0644,
			show_debug, store_debug),
	__ATTR(window_axis, 0644,
			show_window_axis, store_window_axis),
	__ATTR(freescale_mode, 0644,
			show_freescale_mode, store_freescale_mode),
	__ATTR(clone, 0664,
			show_clone, store_clone),
	__ATTR(angle, 0644,
			show_angle, store_angle),
};

#ifdef CONFIG_PM
static int osd_ext_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (early_suspend_flag)
		return 0;
#endif
	osd_ext_suspend_hw();
	return 0;
}

static int osd_ext_resume(struct platform_device *dev)
{
#ifdef CONFIG_SCREEN_ON_EARLY
	if (early_resume_flag) {
		early_resume_flag = 0;
		return 0;
	}
#endif
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (early_suspend_flag)
		return 0;
#endif
	osd_ext_resume_hw();
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void osd_ext_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;
	osd_ext_suspend_hw();
	early_suspend_flag = 1;
}

static void osd_ext_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;
	early_suspend_flag = 0;
	osd_ext_resume_hw();
}
#endif

#ifdef CONFIG_SCREEN_ON_EARLY
void osd_ext_resume_early(void)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	osd_ext_resume_hw();
	early_suspend_flag = 0;
#endif
	early_resume_flag = 1;
}
EXPORT_SYMBOL(osd_ext_resume_early);
#endif

/* static struct resource memobj; */
static int
osd_ext_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fb_info *fbi = NULL;
	const struct vinfo_s *vinfo = NULL;
	struct fb_var_screeninfo *var;
	struct fb_fix_screeninfo *fix;
	/* struct resource *mem; */
	int  index, Bpp;
	int i;
	struct osd_fb_dev_s *fbdev = NULL;
	u32 memsize[2];
	int osd_ext_memory = 0;
	/* 0:don't need osd_ext memory,1:need osd_ext memory */

#ifdef CONFIG_AMLOGIC_VOUT2
	/* register vout2 client */
	vout2_register_client(&osd_ext_notifier_nb);
#endif

	/* get interrupt resource */
	int_viu2_vsync = platform_get_irq_byname(pdev, "viu2-vsync");
	if (int_viu2_vsync == -ENXIO) {
		osd_log_err("cannot get viu2 irq resource\n");
		goto failed1;
	} else
		osd_log_info("viu2 vysnc irq: %d\n", int_viu2_vsync);

	ret = osd_io_remap();
	if (!ret) {
		osd_log_err("osd_io_remap failed\n");
		goto failed1;
	}

	/* get buffer size from dt */
	ret = of_property_read_u32_array(pdev->dev.of_node,
			"mem_size", memsize, 2);
	if (ret) {
		osd_log_err("not found mem_size from dt\n");
		osd_ext_memory = 0;
	} else {
		osd_log_dbg(MODULE_BASE,
			"mem_size: 0x%x, 0x%x\n", memsize[0], memsize[1]);
		fb_rmem_size[0] = memsize[0];
		fb_rmem_paddr[0] = fb_rmem.base;
		if ((OSD_COUNT == 2) && ((memsize[0] + memsize[1]) <=
					fb_rmem.size)) {
			fb_rmem_size[1] = memsize[1];
			fb_rmem_paddr[1] = fb_rmem.base + memsize[0];
		}
		osd_ext_memory = 1;
	}

	/* init reserved memory */
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0) {
		osd_log_info("not found reserved memory\n");
		osd_ext_memory = 0;
	}
#ifdef CONFIG_AMLOGIC_VOUT2
	set_current_vmode2(VMODE_INIT_NULL);
#endif
	osd_ext_init_hw(0);

#ifdef CONFIG_AMLOGIC_VOUT2
	vinfo = get_current_vinfo2();
	osd_log_info("%s vinfo:%p\n", __func__, vinfo);
#endif
	for (index = 0; index < OSD_COUNT; index++) {
		fbi = framebuffer_alloc(sizeof(struct osd_fb_dev_s),
				&pdev->dev);
		if (!fbi) {
			ret = -ENOMEM;
			goto failed1;
		}

		fbdev = (struct osd_fb_dev_s *)fbi->par;
		fbdev->fb_info = fbi;
		fbdev->dev = pdev;

		mutex_init(&fbdev->lock);
		var = &fbi->var;
		fix = &fbi->fix;
		gp_fbdev_list[index] = fbdev;
		if (osd_ext_memory) {
			fbdev->fb_len = fb_rmem_size[index];
			fbdev->fb_mem_paddr = fb_rmem_paddr[index];
			fbdev->fb_mem_vaddr = fb_rmem_vaddr[index];
			if (!fbdev->fb_mem_vaddr) {
				osd_log_err("failed to ioremap frame buffer\n");
				ret = -ENOMEM;
				goto failed1;
			}
		} else
			fbdev->fb_mem_paddr = 0;
		osd_log_info("Frame buffer memory assigned at");
		osd_log_info("    phy:0x%08x, vir:0x%p, size=%dK\n",
			     fbdev->fb_mem_paddr,
			     fbdev->fb_mem_vaddr,
			     fbdev->fb_len >> 10);
		if (index == DEV_OSD0) {
			ret = of_property_read_u32_array(pdev->dev.of_node,
					"display_size_default",
					&var_screeninfo[0], 5);
			if (ret)
				osd_log_info("not found default size\n");
			else {
				int bpp = var_screeninfo[4];

				fb_def_var[index].xres = var_screeninfo[0];
				fb_def_var[index].yres = var_screeninfo[1];
				fb_def_var[index].xres_virtual =
					var_screeninfo[2];
				fb_def_var[index].yres_virtual =
					var_screeninfo[3];
				/* logo always use double buffer */
				fb_def_var[index].bits_per_pixel = bpp;
				osd_log_info("init fbdev bpp is :%d\n",
					fb_def_var[index].bits_per_pixel);
				if (fb_def_var[index].bits_per_pixel > 32)
					fb_def_var[index].bits_per_pixel = 32;
			}
		}

		_fbdev_set_default(fbdev, index);
		if (fbdev->color == NULL) {
			osd_log_err("fbdev->color NULL\n");
			ret = -ENOENT;
			goto failed1;
		}

		if (osd_ext_memory) {
			Bpp = (fbdev->color->color_index > 8 ?
					(fbdev->color->color_index > 16 ?
					(fbdev->color->color_index > 24 ?
					 4 : 3) : 2) : 1);
			fix->line_length = var->xres_virtual * Bpp;
			fix->smem_start = fbdev->fb_mem_paddr;
			fix->smem_len = fbdev->fb_len;
		}

		if (fb_alloc_cmap(&fbi->cmap, 16, 0) != 0) {
			osd_log_err("unable to allocate color map memory\n");
			ret = -ENOMEM;
			goto failed2;
		}

		fbi->pseudo_palette = kmalloc(sizeof(u32) * 16, GFP_KERNEL);
		if (!(fbi->pseudo_palette)) {
			osd_log_err("unable to allocate pseudo palette mem\n");
			ret = -ENOMEM;
			goto failed2;
		}
		memset(fbi->pseudo_palette, 0, sizeof(u32) * 16);

		fbi->fbops = &osd_ext_ops;

		if (osd_ext_memory) {
			fbi->screen_base = (char __iomem *)fbdev->fb_mem_vaddr;
			fbi->screen_size = fix->smem_len;
		}

		if (vinfo)
			set_default_display_axis(&fbdev->fb_info->var,
					&fbdev->osd_ctl, vinfo);
		osd_ext_check_var(var, fbi);
		register_framebuffer(fbi);

		osddev_ext_setup(fbdev);
		for (i = 0; i < ARRAY_SIZE(osd_ext_attrs); i++)
			ret = device_create_file(fbi->dev, &osd_ext_attrs[i]);
	}

	index = 0;
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	early_suspend.suspend = osd_ext_early_suspend;
	early_suspend.resume = osd_ext_late_resume;
	register_early_suspend(&early_suspend);
#endif

	osd_log_info("osd-ext probe ok\n");
	return 0;

failed2:
	fb_dealloc_cmap(&fbi->cmap);
failed1:
	osd_log_err("osd-ext module probe failed.\n");
	return ret;
}

static int
osd_ext_remove(struct platform_device *pdev)
{
	struct fb_info *fbi;
	int i = 0;

	osd_log_info("osd_ext_remove.\n");
	if (!pdev)
		return -ENODEV;

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&early_suspend);
#endif

	vout_unregister_client(&osd_ext_notifier_nb);

	for (i = 0; i < OSD_COUNT; i++) {
		int j;

		if (gp_fbdev_list[i]) {
			struct osd_fb_dev_s *fbdev = gp_fbdev_list[i];

			fbi = fbdev->fb_info;
			for (j = 0; j < ARRAY_SIZE(osd_ext_attrs); j++)
				device_remove_file(fbi->dev, &osd_ext_attrs[j]);
			iounmap(fbdev->fb_mem_vaddr);
			kfree(fbi->pseudo_palette);
			fb_dealloc_cmap(&fbi->cmap);
			unregister_framebuffer(fbi);
			framebuffer_release(fbi);
		}
	}
	return 0;
}

/* Process kernel command line parameters */
static int __init
osd_ext_setup_attribute(char *options)
{
	char *this_opt = NULL;
	int r = 0;
	unsigned long res;

	if (!options || !*options)
		goto exit;

	while (!r && (this_opt = strsep(&options, ",")) != NULL) {
		if (!strncmp(this_opt, "xres:", 5)) {
			r = kstrtol(this_opt + 5, 0, &res);
			fb_def_var[0].xres = fb_def_var[0].xres_virtual = res;
		} else if (!strncmp(this_opt, "yres:", 5)) {
			r = kstrtol(this_opt + 5, 0, &res);
			fb_def_var[0].yres = fb_def_var[0].yres_virtual = res;
		} else {
			osd_log_err("invalid option\n");
			r = -1;
		}
	}
exit:
	return r;
}

static int rmem_fb_device_init(struct reserved_mem *rmem, struct device *dev)
{
	int i = 0;

	for (i = 0; i < OSD_COUNT; i++) {
		if ((fb_rmem_paddr[i] > 0) && (fb_rmem_size[i] > 0)) {
			fb_rmem_vaddr[i] = ioremap_wc(fb_rmem_paddr[i],
					fb_rmem_size[i]);
			if (!fb_rmem_vaddr[i])
				osd_log_err("fb [%d] ioremap error", i);
		}
	}

	return 0;
}

static const struct reserved_mem_ops rmem_fb_ops = {
	.device_init = rmem_fb_device_init,
};

static int __init rmem_fb_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE;
	phys_addr_t mask = align - 1;

	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of region\n");
		return -EINVAL;
	}
	fb_rmem.base = rmem->base;
	fb_rmem.size = rmem->size;
	rmem->ops = &rmem_fb_ops;

	osd_log_info("Reserved memory: created fb at 0x%p, size %ld MiB\n",
		     (void *)rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}
RESERVEDMEM_OF_DECLARE(fb, "amlogic, fb-ext-memory", rmem_fb_setup);

/****************************************/
static const struct of_device_id meson_fbext_dt_match[] = {
	{.compatible = "amlogic, meson-fb-ext",},
	{},
};

static struct platform_driver
	osd_ext_driver = {
	.probe      = osd_ext_probe,
	.remove     = osd_ext_remove,
#ifdef CONFIG_PM
	.suspend    = osd_ext_suspend,
	.resume     = osd_ext_resume,
#endif
	.driver     = {
		.name   = "meson-fb-ext",
		.of_match_table = meson_fbext_dt_match,
	}
};

static int __init osd_ext_init_module(void)
{
	char *option;

	osd_log_info("%s\n", __func__);
	if (fb_get_options("meson-fb-ext", &option))
		return -ENODEV;
	osd_ext_setup_attribute(option);
	if (platform_driver_register(&osd_ext_driver)) {
		osd_log_err("failed to register osd ext driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit osd_ext_exit_module(void)
{
	osd_log_info("%s\n", __func__);

	platform_driver_unregister(&osd_ext_driver);
}

/****************************************/
module_init(osd_ext_init_module);
module_exit(osd_ext_exit_module);

MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
MODULE_DESCRIPTION("OSD EXT Module");
MODULE_LICENSE("GPL");
