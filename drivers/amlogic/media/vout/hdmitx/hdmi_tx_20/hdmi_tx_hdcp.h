/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdmi_tx_hdcp.h
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

#ifndef __HDMI_TX_HDCP_H
#define __HDMI_TX_HDCP_H
/*
 * hdmi_tx_hdcp.c
 * version 1.0
 */

/* Notic: the HDCP key setting has been moved to uboot
 * On MBX project, it is too late for HDCP get from
 * other devices
 */

/* int task_tx_key_setting(unsigned force_wrong); */

int hdcp_ksv_valid(unsigned char *dat);

#endif

