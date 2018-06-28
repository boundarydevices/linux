/*
 * drivers/amlogic/drm/am_meson_hdcp.h
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

#ifndef __AM_MESON_HDCP_H
#define __AM_MESON_HDCP_H

#define HDCP_SLAVE     0x3a
#define HDCP2_VERSION  0x50
#define HDCP_MODE14    1
#define HDCP_MODE22    2

#define HDCP_QUIT      0
#define HDCP14_ENABLE  1
#define HDCP14_AUTH    2
#define HDCP14_SUCCESS 3
#define HDCP14_FAIL    4
#define HDCP22_ENABLE  5
#define HDCP22_AUTH    6
#define HDCP22_SUCCESS 7
#define HDCP22_FAIL    8

int am_hdcp_init(struct am_hdmi_tx *am_hdmi);
int is_hdcp_hdmitx_supported(struct am_hdmi_tx *am_hdmi);
int am_hdcp_work(void *data);
void am_hdcp_disable(struct am_hdmi_tx *am_hdmi);

#endif
