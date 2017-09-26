/*
 * drivers/amlogic/drm/am_meson_hdmi.h
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
#ifndef __AM_MESON_HDMI_H
#define __AM_MESON_HDMI_H

#include "meson_drv.h"

struct am_hdmi_tx {
	struct drm_encoder	encoder;
	struct drm_connector	connector;
	struct meson_drm	*priv;
};

int am_hdmi_connector_create(struct meson_drm *priv);

#endif

