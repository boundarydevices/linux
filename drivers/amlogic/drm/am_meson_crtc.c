/*
 * drivers/amlogic/drm/am_meson_crtc.c
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

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_crtc.h"
#include "osd_drm.h"


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

char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode)
{
	int i;

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

struct am_vpp {
};

struct am_vout {
};

struct am_meson_crtc {
	struct drm_crtc base;
	struct meson_drm *priv;

	struct drm_pending_vblank_event *event;

	struct am_vpp vpp;
	struct am_vout vout;
};

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

	DRM_DEBUG_DRIVER("am_crtc_set_mode\n");

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

static bool am_meson_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	//DRM_INFO("am_meson_crtc_mode_fixup !!\n");

	return true;
}

void am_meson_crtc_enable(struct drm_crtc *crtc)
{
	enum vmode_e mode;
	int ret = 0;
	char *name;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;

	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable fail, unsupport mode:%s\n",
			adjusted_mode->name);
		return;
	}
	//DRM_INFO("meson_crtc_enable  %s\n", adjusted_mode->name);
	name = am_meson_crtc_get_voutmode(adjusted_mode);

	mode = validate_vmode(name);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret)
		DRM_ERROR("new mode %s set error\n", name);
	else
		DRM_INFO("new mode %s set ok\n", name);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
}

void am_meson_crtc_disable(struct drm_crtc *crtc)
{
	//DRM_INFO("meson_crtc_disable!!\n");
}

void am_meson_crtc_commit(struct drm_crtc *crtc)
{
	//DRM_INFO("am_meson_crtc_commit!!\n");
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

int meson_crtc_create(struct meson_drm *priv)
{
	struct am_meson_crtc *amcrtc;
	struct drm_crtc *crtc;
	int ret;

	DRM_DEBUG("amlogic meson_crtc_create\n");
	amcrtc = devm_kzalloc(priv->drm->dev, sizeof(*amcrtc),
				  GFP_KERNEL);
	if (!amcrtc)
		return -ENOMEM;

	amcrtc->priv = priv;
	crtc = &amcrtc->base;
	ret = drm_crtc_init_with_planes(priv->drm, crtc,
					priv->primary_plane, priv->cursor_plane,
					&am_meson_crtc_funcs, "amlogic vpu");
	if (ret) {
		dev_err(priv->drm->dev, "Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &am_crtc_helper_funcs);
	osd_drm_init();

	priv->crtc = crtc;
	return 0;
}

void meson_crtc_irq(struct meson_drm *priv)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(priv->crtc);

	osd_drm_vsync_isr_handler();
	am_meson_crtc_handle_vsync(amcrtc);
}

