/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HDMI_RX2_HDCP_H_
#define _HDMI_RX2_HDCP_H_

#include "mtk_hdmirx.h"

void hdcp2_load_hdcp_key(struct MTK_HDMI *myhdmi);
void hdcp2_reset_sram_address(struct MTK_HDMI *myhdmi);
void hdcp2_reset_sram_addr_hdcp1x_key(struct MTK_HDMI *myhdmi);
#ifdef HDMIRX_OPTEE_EN
int optee_hdcp_open(struct MTK_HDMI *myhdmi);
int optee_key_status(struct MTK_HDMI *myhdmi, u32 *key_status);
#endif

#endif
