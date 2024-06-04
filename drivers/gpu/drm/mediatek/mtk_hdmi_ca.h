/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#ifndef _HDMI_CA_H_
#define _HDMI_CA_H_

#include <linux/types.h>
#include <linux/uuid.h>

static const uuid_t HDCP_HDMI_TA_UUID =
UUID_INIT(0xeaf800b0, 0xda1b, 0x11e2,
	0xa2, 0x8f, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66);

enum HDMI_TA_SERVICE_CMD_T {
	HDMI_TA_WRITE_REG = 0,
	HDMI_TA_DPI1_WRITE_REG,
	HDMI_TA_INSTALL_HDCP_KEY,
	HDMI_TA_LOAD_HDCP_KEY,
	HDMI_TA_GET_HDCP_AKSV,
	HDMI_TA_HDCP_ENC_EN,
	HDMI_TA_HDCP_RST,
	HDMI_TA_VID_UNMUTE,
	HDMI_TA_AUD_UNMUTE,
	HDMI_TA_PROTECT_HDMIREG,
	HDMI_TA_LOAD_FIRMWARE,
	HDMI_TA_HDCP_FAIL,
	HDMI_TA_TEST_HDCP_VERSION,
	HDMI_TA_SET_LOG_LEVEL,
	HDMI_TA_HDCP_OFF,
	HDMI_TA_READ_STATUS,
	HDMI_TA_LOAD_RAM,
	HDMI_TA_SECURE_REG_ENTRY,
	HDMI_TA_GET_EFUSE,
	HDMI_TA_GET_HDCP_TYPE,
	HDMI_TA_GET_HDCP2_INFO,
	HDMI_TA_HDCP_ENC_STATUS,
	HDMI_TA_RESET_FIRMWARE,
};

enum TA_RETURN_HDMI_HDCP_STATE {
	TA_RETURN_HDCP_STATE_ENC_EN = 0,
	TA_RETURN_HDCP_STATE_ENC_FAIL,
	TA_RETURN_HDCP_STATE_ENC_UNKNOWN,
};

bool mtk_hdmi_hdcp_ca_create(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_close(struct mtk_hdmi *hdmi);
void mtk_hdmi_hdcp_ca_write_reg(struct mtk_hdmi *hdmi, unsigned int u4addr, unsigned int u4data);
void mtk_hdmi_hdcp_ca_write_hdcp_rst(struct mtk_hdmi *hdmi, unsigned int u4addr,
	unsigned int u4data);
void mtk_hdmi_hdcp_ca_write_ctrl(struct mtk_hdmi *hdmi, unsigned int u4addr, unsigned int u4data);
bool mtk_hdmi_hdcp_ca_get_aksv(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_load_hdcp_key(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_load_firmware(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_reset_firmware(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_init(struct mtk_hdmi *hdmi);
bool mtk_hdmi_hdcp_ca_deinit(struct mtk_hdmi *hdmi);

#endif
