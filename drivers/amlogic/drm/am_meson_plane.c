/*
 * drivers/amlogic/drm/am_meson_plane.c
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
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "osd.h"
#include "osd_drm.h"

#include "meson_plane.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "am_meson_fb.h"
#endif

#define to_am_osd_plane(x) container_of(x, struct am_osd_plane, base)

struct am_osd_plane {
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;

	u32 osd_idx;
};

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
		format = COLOR_INDEX_32_XRGB;
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
		DRM_ERROR("ERROR:am_meson_plane meet a scatter framebuffe.\nr");
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

int meson_plane_create(struct meson_drm *priv)
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

