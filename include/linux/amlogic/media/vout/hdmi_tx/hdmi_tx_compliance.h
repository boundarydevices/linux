/*
 * include/linux/amlogic/media/vout/hdmi_tx/hdmi_tx_compliance.h
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

#ifndef __HDMI_TX_COMPLIANCE_H
#define __HDMI_TX_COMPLIANCE_H

#include "hdmi_info_global.h"
#include "hdmi_tx_module.h"

void hdmitx_special_handler_video(struct hdmitx_dev *hdmitx_device);
void hdmitx_special_handler_audio(struct hdmitx_dev *hdmitx_device);

#endif

