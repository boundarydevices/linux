/*
 * drivers/amlogic/drm/am_meson_vpu.c
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
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/component.h>
#ifdef CONFIG_DRM_MESON_USE_ION
#include <ion/ion_priv.h>
#endif

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#include "osd.h"
#include "osd_drm.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "am_meson_fb.h"
#endif
#include "am_meson_vpu.h"

/*
 * Video Processing Unit
 *
 * VPU Handles the Global Video Processing, it includes management of the
 * clocks gates, blocks reset lines and power domains.
 *
 * What is missing :
 * - Full reset of entire video processing HW blocks
 * - Scaling and setup of the VPU clock
 * - Bus clock gates
 * - Powering up video processing HW blocks
 * - Powering Up HDMI controller and PHY
 */
struct am_osd_plane {
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;

	u32 osd_idx;
};
#define to_am_osd_plane(x) container_of(x, struct am_osd_plane, base)

struct am_meson_crtc {
	struct drm_crtc base;
	struct device *dev;
	struct drm_device *drm_dev;

	struct meson_drm *priv;

	struct drm_pending_vblank_event *event;

	unsigned int irq;

	struct dentry *crtc_debugfs_dir;
};

#define to_am_meson_crtc(x) container_of(x, struct am_meson_crtc, base)

struct am_vout_mode {
	char name[DRM_DISPLAY_MODE_LEN];
	enum vmode_e mode;
	int width, height, vrefresh;
	unsigned int flags;
};

static struct am_vout_mode am_vout_modes[] = {
	{ "1080p60hz", VMODE_HDMI, 1920, 1080, 60, 0},
	{ "1080p30hz", VMODE_HDMI, 1920, 1080, 30, 0},
	{ "1080p50hz", VMODE_HDMI, 1920, 1080, 50, 0},
	{ "1080p25hz", VMODE_HDMI, 1920, 1080, 25, 0},
	{ "1080p24hz", VMODE_HDMI, 1920, 1080, 24, 0},
	{ "2160p30hz", VMODE_HDMI, 3840, 2160, 30, 0},
	{ "2160p60hz", VMODE_HDMI, 3840, 2160, 60, 0},
	{ "2160p50hz", VMODE_HDMI, 3840, 2160, 50, 0},
	{ "2160p25hz", VMODE_HDMI, 3840, 2160, 25, 0},
	{ "2160p24hz", VMODE_HDMI, 3840, 2160, 24, 0},
	{ "1080i60hz", VMODE_HDMI, 1920, 1080, 60, DRM_MODE_FLAG_INTERLACE},
	{ "1080i50hz", VMODE_HDMI, 1920, 1080, 50, DRM_MODE_FLAG_INTERLACE},
	{ "720p60hz", VMODE_HDMI, 1280, 720, 60, 0},
	{ "720p50hz", VMODE_HDMI, 1280, 720, 50, 0},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
	{ "480i60hz", VMODE_HDMI, 720, 480, 60, DRM_MODE_FLAG_INTERLACE},
	{ "576p50hz", VMODE_HDMI, 720, 576, 50, 0},
	{ "576i50hz", VMODE_HDMI, 720, 576, 50, DRM_MODE_FLAG_INTERLACE},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
};


static struct osd_device_data_s osd_gxbb = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXBB,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_gxl = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXL,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_gxm = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXM,
	.osd_ver = OSD_NORMAL,
	.afbc_type = MESON_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0xfff,
	.dummy_data = 0x00202000,/* dummy data is different */
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_txl = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TXL,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_txlx = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TXLX,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 1,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_axg = {
	.cpu_id = __MESON_CPU_MAJOR_ID_AXG,
	.osd_ver = OSD_SIMPLE,
	.afbc_type = NO_AFBC,
	.osd_count = 1,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 0,
	.has_dolby_vision = 0,
	 /* use iomap its self, no rdma, no canvas, no freescale */
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0x400,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_g12a = {
	.cpu_id = __MESON_CPU_MAJOR_ID_G12A,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 3,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
};

static struct osd_device_data_s osd_meson_dev;

int am_meson_crtc_dts_info_set(const void *dt_match_data)
{
	struct osd_device_data_s *osd_meson;

	osd_meson = (struct osd_device_data_s *)dt_match_data;
	if (osd_meson)
		memcpy(&osd_meson_dev, osd_meson,
			sizeof(struct osd_device_data_s));
	else {
		DRM_ERROR("%s data NOT match\n", __func__);
		return -1;
	}

	return 0;
}


static const struct drm_plane_funcs am_osd_plane_funs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
};

int am_osd_begin_display(
	struct drm_plane *plane,
	struct drm_plane_state *new_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("%s osd %d.\n", __func__, osd_plane->osd_idx);
	return 0;
}

void am_osd_end_display(
	struct drm_plane *plane,
	struct drm_plane_state *old_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("%s osd %d.\n", __func__, osd_plane->osd_idx);
}

void am_osd_do_display(
	struct drm_plane *plane,
	struct drm_plane_state *old_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	struct meson_drm *drv = osd_plane->drv;
	struct osd_plane_map_s plane_map;
#ifdef CONFIG_DRM_MESON_USE_ION
	struct am_meson_fb *meson_fb;
#else
	struct drm_gem_cma_object *gem;
#endif
	int format = DRM_FORMAT_ARGB8888;
	dma_addr_t phyaddr;
	unsigned long flags;

	//DRM_INFO("%s osd %d.\n", __func__, osd_plane->osd_idx);

	switch (fb->pixel_format) {
	case DRM_FORMAT_XRGB8888:
		/*
		 *force convert to ARGB8888 format,
		 *because overlay layer needs to display
		 */
		format = COLOR_INDEX_32_XRGB;//COLOR_INDEX_32_ARGB;
		break;
	case DRM_FORMAT_XBGR8888:
		format = COLOR_INDEX_32_XBGR;
		break;
	case DRM_FORMAT_RGBX8888:
		format = COLOR_INDEX_32_RGBX;
		break;
	case DRM_FORMAT_BGRX8888:
		format = COLOR_INDEX_32_BGRX;
		break;
	case DRM_FORMAT_ARGB8888:
		format = COLOR_INDEX_32_ARGB;
		break;
	case DRM_FORMAT_ABGR8888:
		format = COLOR_INDEX_32_ABGR;
		break;
	case DRM_FORMAT_RGBA8888:
		format = COLOR_INDEX_32_RGBA;
		break;
	case DRM_FORMAT_BGRA8888:
		format = COLOR_INDEX_32_BGRA;
		break;
	case DRM_FORMAT_RGB888:
		format = COLOR_INDEX_24_RGB;
		break;
	case DRM_FORMAT_RGB565:
		format = COLOR_INDEX_16_565;
		break;
	case DRM_FORMAT_ARGB1555:
		format = COLOR_INDEX_16_1555_A;
		break;
	case DRM_FORMAT_ARGB4444:
		format = COLOR_INDEX_16_4444_A;
		break;
	default:
		DRM_INFO("unsupport fb->pixel_format=%x\n", fb->pixel_format);
		break;
	};

	spin_lock_irqsave(&drv->drm->event_lock, flags);

#ifdef CONFIG_DRM_MESON_USE_ION
	meson_fb = container_of(fb, struct am_meson_fb, base);
	phyaddr = am_meson_gem_object_get_phyaddr(drv, meson_fb->bufp);
	if (meson_fb->bufp->bscatter)
		DRM_ERROR("ERROR:am_meson_plane meet a scatter framebuffer.\n");
#else
	/* Update Canvas with buffer address */
	gem = drm_fb_cma_get_gem_obj(fb, 0);
	phyaddr = gem->paddr;
#endif

	/* setup osd display parameters */
	plane_map.plane_index = osd_plane->osd_idx;
	plane_map.zorder = state->zpos;
	plane_map.phy_addr = phyaddr;
	plane_map.enable = 1;
	plane_map.format = format;
	plane_map.byte_stride = fb->pitches[0];

	plane_map.src_x = state->src_x;
	plane_map.src_y = state->src_y;
	plane_map.src_w = (state->src_w >> 16) & 0xffff;
	plane_map.src_h = (state->src_h >> 16) & 0xffff;

	plane_map.dst_x = state->crtc_x;
	plane_map.dst_y = state->crtc_y;
	plane_map.dst_w = state->crtc_w;
	plane_map.dst_h = state->crtc_h;
	#if 0
	DRM_INFO("flags:%d pixel_format:%d,zpos=%d\n",
				fb->flags, fb->pixel_format, state->zpos);
	DRM_INFO("plane index=%d, type=%d\n", plane->index, plane->type);
	#endif
	osd_drm_plane_page_flip(&plane_map);

	spin_unlock_irqrestore(&drv->drm->event_lock, flags);
}

int am_osd_check(struct drm_plane *plane, struct drm_plane_state *state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("%s osd %d.\n", __func__, osd_plane->osd_idx);
	return 0;
}

void am_osd_blank(struct drm_plane *plane, struct drm_plane_state *old_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("%s osd %d.\n", __func__, osd_plane->osd_idx);
}

static const struct drm_plane_helper_funcs am_osd_helper_funcs = {
	.prepare_fb = am_osd_begin_display,
	.cleanup_fb = am_osd_end_display,
	.atomic_update	= am_osd_do_display,
	.atomic_check	= am_osd_check,
	.atomic_disable	= am_osd_blank,
};

static const uint32_t supported_drm_formats[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGB565,
};

struct am_osd_plane *am_osd_plane_create(struct meson_drm *priv, u32 type)
{
	struct am_osd_plane *osd_plane;
	struct drm_plane *plane;
	char *plane_name = NULL;

	osd_plane = devm_kzalloc(priv->drm->dev, sizeof(*osd_plane),
				   GFP_KERNEL);
	if (!osd_plane)
		return 0;

	osd_plane->drv = priv;
	plane = &osd_plane->base;

	if (type == DRM_PLANE_TYPE_PRIMARY) {
		osd_plane->osd_idx = 0;
		plane_name = "osd-0";
	} else if (type == DRM_PLANE_TYPE_CURSOR) {
		osd_plane->osd_idx = 1;
		plane_name = "osd-1";
	}

	drm_universal_plane_init(priv->drm, plane, 0xFF,
				 &am_osd_plane_funs,
				 supported_drm_formats,
				 ARRAY_SIZE(supported_drm_formats),
				 type, plane_name);

	drm_plane_helper_add(plane, &am_osd_helper_funcs);
	osd_drm_debugfs_add(&(osd_plane->plane_debugfs_dir),
		plane_name, osd_plane->osd_idx);
	return osd_plane;
}

int am_meson_plane_create(struct meson_drm *priv)
{
	struct am_osd_plane *plane;

	DRM_DEBUG("%s. enter\n", __func__);
	/*crate primary plane*/
	plane = am_osd_plane_create(priv, DRM_PLANE_TYPE_PRIMARY);
	if (plane == NULL)
		return -ENOMEM;

	priv->primary_plane = &(plane->base);

	/*crate cursor plane*/
	plane = am_osd_plane_create(priv, DRM_PLANE_TYPE_CURSOR);
	if (plane == NULL)
		return -ENOMEM;

	priv->cursor_plane = &(plane->base);

	return 0;
}

char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode)
{
	int i;

	if (!strcmp(mode->name, "panel"))
		return "panel";

	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if ((am_vout_modes[i].width == mode->hdisplay)
			&& (am_vout_modes[i].height == mode->vdisplay)
			&& (am_vout_modes[i].vrefresh == mode->vrefresh)
			&& (am_vout_modes[i].flags ==
				(mode->flags&DRM_MODE_FLAG_INTERLACE)))
			return am_vout_modes[i].name;
	}
	return NULL;
}

void am_meson_crtc_handle_vsync(struct am_meson_crtc *amcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc;

	crtc = &amcrtc->base;
	drm_crtc_handle_vblank(crtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (amcrtc->event) {
		drm_crtc_send_vblank_event(crtc, amcrtc->event);
		drm_crtc_vblank_put(crtc);
		amcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

int am_meson_crtc_set_mode(struct drm_mode_set *set)
{
	struct am_meson_crtc *amcrtc;
	int ret;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	amcrtc = to_am_meson_crtc(set->crtc);
	ret = drm_atomic_helper_set_config(set);

	return ret;
}

static const struct drm_crtc_funcs am_meson_crtc_funcs = {
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.destroy		= drm_crtc_cleanup,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= drm_atomic_helper_crtc_reset,
	.set_config             = am_meson_crtc_set_mode,
};

static int am_meson_crtc_loader_protect(struct drm_crtc *crtc, bool on)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	DRM_INFO("%s  %d\n", __func__, on);

	if (on) {
		enable_irq(amcrtc->irq);
		drm_crtc_vblank_on(crtc);
	} else {
		disable_irq(amcrtc->irq);
		drm_crtc_vblank_off(crtc);
	}

	return 0;
}

static int am_meson_crtc_enable_vblank(struct drm_crtc *crtc)
{
	return 0;
}

static void am_meson_crtc_disable_vblank(struct drm_crtc *crtc)
{
}

static const struct meson_crtc_funcs meson_private_crtc_funcs = {
	.loader_protect = am_meson_crtc_loader_protect,
	.enable_vblank = am_meson_crtc_enable_vblank,
	.disable_vblank = am_meson_crtc_disable_vblank,
};

static bool am_meson_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	//DRM_INFO("%s !!\n", __func__);

	return true;
}

void am_meson_crtc_enable(struct drm_crtc *crtc)
{
	char *name;
	enum vmode_e mode;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	DRM_INFO("%s\n", __func__);
	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable fail, unsupport mode:%s\n",
			adjusted_mode->name);
		return;
	}
	DRM_INFO("%s: %s\n", __func__, adjusted_mode->name);
	name = am_meson_crtc_get_voutmode(adjusted_mode);
	mode = validate_vmode(name);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}

	set_vout_init(mode);
	update_vout_viu();

	enable_irq(amcrtc->irq);
}

void am_meson_crtc_disable(struct drm_crtc *crtc)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	DRM_INFO("%s\n", __func__);
	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}

	disable_irq(amcrtc->irq);
}

void am_meson_crtc_commit(struct drm_crtc *crtc)
{
	//DRM_INFO("%s\n", __func__);
}

void am_meson_crtc_atomic_begin(struct drm_crtc *crtc,
			     struct drm_crtc_state *old_crtc_state)
{
	struct am_meson_crtc *amcrtc;
	unsigned long flags;

	amcrtc = to_am_meson_crtc(crtc);

	if (crtc->state->event) {
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		amcrtc->event = crtc->state->event;
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		crtc->state->event = NULL;
	}
}

void am_meson_crtc_atomic_flush(struct drm_crtc *crtc,
			     struct drm_crtc_state *old_crtc_state)
{
}

static const struct drm_crtc_helper_funcs am_crtc_helper_funcs = {
	.enable			= am_meson_crtc_enable,
	.disable			= am_meson_crtc_disable,
	.commit			= am_meson_crtc_commit,
	.mode_fixup		= am_meson_crtc_mode_fixup,
	.atomic_begin	= am_meson_crtc_atomic_begin,
	.atomic_flush		= am_meson_crtc_atomic_flush,
};

int am_meson_crtc_create(struct am_meson_crtc *amcrtc)
{
	struct meson_drm *priv = amcrtc->priv;
	struct drm_crtc *crtc = &amcrtc->base;
	int ret;

	DRM_INFO("%s\n", __func__);
	ret = drm_crtc_init_with_planes(priv->drm, crtc,
					priv->primary_plane, priv->cursor_plane,
					&am_meson_crtc_funcs, "amlogic vpu");
	if (ret) {
		dev_err(amcrtc->dev, "Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &am_crtc_helper_funcs);
	osd_drm_init(&osd_meson_dev);

	priv->crtc = crtc;
	return 0;
}

void am_meson_crtc_irq(struct meson_drm *priv)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(priv->crtc);

	osd_drm_vsync_isr_handler();
	am_meson_crtc_handle_vsync(amcrtc);
}

static irqreturn_t am_meson_vpu_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct meson_drm *priv = dev->dev_private;

	am_meson_crtc_irq(priv);

	return IRQ_HANDLED;
}

static int am_meson_vpu_bind(struct device *dev,
				struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm_dev = data;
	struct meson_drm *private = drm_dev->dev_private;
	struct am_meson_crtc *amcrtc;

	int ret, irq;

	/* Allocate crtc struct */
	DRM_DEBUG("%s\n", __func__);
	amcrtc = devm_kzalloc(dev, sizeof(*amcrtc),
				  GFP_KERNEL);
	if (!amcrtc)
		return -ENOMEM;

	amcrtc->priv = private;
	amcrtc->dev = dev;
	amcrtc->drm_dev = drm_dev;

	dev_set_drvdata(dev, amcrtc);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "cannot find irq for vpu\n");
		return irq;
	}
	amcrtc->irq = (unsigned int)irq;

	ret = devm_request_irq(dev, amcrtc->irq, am_meson_vpu_irq,
		IRQF_SHARED, dev_name(dev), drm_dev);
	if (ret)
		return ret;
	/* IRQ is initially disabled; it gets enabled in crtc_enable */
	disable_irq(amcrtc->irq);

	ret = am_meson_plane_create(private);
	if (ret)
		return ret;

	ret = am_meson_crtc_create(amcrtc);
	if (ret)
		return ret;

	am_meson_register_crtc_funcs(private->crtc, &meson_private_crtc_funcs);

	return 0;
}

static void am_meson_vpu_unbind(struct device *dev,
				struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct meson_drm *private = drm_dev->dev_private;

	am_meson_unregister_crtc_funcs(private->crtc);
	osd_drm_debugfs_exit();
}

static const struct component_ops am_meson_vpu_component_ops = {
	.bind = am_meson_vpu_bind,
	.unbind = am_meson_vpu_unbind,
};

static const struct of_device_id am_meson_vpu_driver_dt_match[] = {
	{ .compatible = "amlogic,meson-gxbb-vpu",
	 .data = &osd_gxbb, },
	{ .compatible = "amlogic,meson-gxl-vpu",
	 .data = &osd_gxl, },
	{ .compatible = "amlogic,meson-gxm-vpu",
	 .data = &osd_gxm, },
	{ .compatible = "amlogic,meson-txl-vpu",
	 .data = &osd_txl, },
	{ .compatible = "amlogic,meson-txlx-vpu",
	 .data = &osd_txlx, },
	{ .compatible = "amlogic,meson-axg-vpu",
	 .data = &osd_axg, },
	{ .compatible = "amlogic,meson-g12a-vpu",
	 .data = &osd_g12a, },
	{},
};
MODULE_DEVICE_TABLE(of, am_meson_vpu_driver_dt_match);

static int am_meson_vpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const void *vpu_data;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "can't find vpu devices\n");
		return -ENODEV;
	}

	vpu_data = of_device_get_match_data(dev);
	if (vpu_data) {
		ret = am_meson_crtc_dts_info_set(vpu_data);
		if (ret < 0)
			return -ENODEV;
	} else {
		dev_err(dev, "%s NOT match\n", __func__);
		return -ENODEV;
	}

	return component_add(dev, &am_meson_vpu_component_ops);
}

static int am_meson_vpu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_vpu_component_ops);

	return 0;
}

static struct platform_driver am_meson_vpu_platform_driver = {
	.probe = am_meson_vpu_probe,
	.remove = am_meson_vpu_remove,
	.driver = {
		.name = "meson-vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(am_meson_vpu_driver_dt_match),
	},
};

module_platform_driver(am_meson_vpu_platform_driver);

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm VPU driver");
MODULE_LICENSE("GPL");
