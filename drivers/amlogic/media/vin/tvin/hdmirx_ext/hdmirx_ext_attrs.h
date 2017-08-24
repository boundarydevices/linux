/*
 * drivers/amlogic/media/vin/tvin/hdmirx_ext/hdmirx_ext_attrs.h
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

#ifndef __HDMIRX_EXT_DEVICE_ATTRS_H__
#define __HDMIRX_EXT_DEVICE_ATTRS_H__

extern int hdmirx_ext_create_device_attrs(struct device *pdevice);
extern void hdmirx_ext_destroy_device_attrs(struct device *pdevice);

#endif
