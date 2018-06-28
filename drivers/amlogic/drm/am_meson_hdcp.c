/*
 * drivers/amlogic/drm/am_meson_hdcp.c
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

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/arm-smccc.h>

#include "am_meson_hdmi.h"
#include "am_meson_hdcp.h"

static int hdcp_topo_st = -1;
static int hdmitx_hdcp_opr(unsigned int val)
{
	struct arm_smccc_res res;

	if (val == 1) { /* HDCP14_ENABLE */
		arm_smccc_smc(0x82000010, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 2) { /* HDCP14_RESULT */
		arm_smccc_smc(0x82000011, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	if (val == 0) { /* HDCP14_INIT */
		arm_smccc_smc(0x82000012, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 3) { /* HDCP14_EN_ENCRYPT */
		arm_smccc_smc(0x82000013, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 4) { /* HDCP14_OFF */
		arm_smccc_smc(0x82000014, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 5) {	/* HDCP_MUX_22 */
		arm_smccc_smc(0x82000015, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 6) {	/* HDCP_MUX_14 */
		arm_smccc_smc(0x82000016, 0, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 7) { /* HDCP22_RESULT */
		arm_smccc_smc(0x82000017, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	if (val == 0xa) { /* HDCP14_KEY_LSTORE */
		arm_smccc_smc(0x8200001a, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	if (val == 0xb) { /* HDCP22_KEY_LSTORE */
		arm_smccc_smc(0x8200001b, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	if (val == 0xc) { /* HDCP22_KEY_SET_DUK */
		arm_smccc_smc(0x8200001c, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	if (val == 0xd) { /* HDCP22_SET_TOPO */
		arm_smccc_smc(0x82000083, hdcp_topo_st, 0, 0, 0, 0, 0, 0, &res);
	}
	if (val == 0xe) { /* HDCP22_GET_TOPO */
		arm_smccc_smc(0x82000084, 0, 0, 0, 0, 0, 0, 0, &res);
		return (unsigned int)((res.a0)&0xffffffff);
	}
	return -1;
}

static void get_hdcp_bstatus(void)
{
	int ret1 = 0;
	int ret2 = 0;

	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 1, 0, 1);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	ret1 = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_0);
	ret2 = hdmitx_rd_reg(HDMITX_DWC_HDCP_BSTATUS_1);
	hdmitx_set_reg_bits(HDMITX_DWC_A_KSVMEMCTRL, 0, 0, 1);
	DRM_INFO("BSTATUS0 = 0x%x   BSTATUS1 = 0x%x\n", ret1, ret2);
}

static void hdcp14_events_handle(unsigned long arg)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)arg;
	unsigned int bcaps_6_rp;
	static unsigned int st_flag = -1;

	bcaps_6_rp = !!(hdmitx_rd_reg(HDMITX_DWC_A_HDCPOBS3) & (1 << 6));
	if (st_flag != hdmitx_rd_reg(HDMITX_DWC_A_APIINTSTAT)) {
		st_flag = hdmitx_rd_reg(HDMITX_DWC_A_APIINTSTAT);
		DRM_INFO("hdcp14: instat: 0x%x\n", st_flag);
	}
	if (st_flag & (1 << 7)) {
		hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR, 1 << 7);
		hdmitx_hdcp_opr(3);
		get_hdcp_bstatus();
	}

	if (st_flag & (1 << 1)) {
		hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR, (1 << 1));
		hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x1);
		hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
		if (hdmitx_rd_reg(HDMITX_DWC_A_KSVMEMCTRL) & (1 << 1))
			;//hdcp_ksv_sha1_calc(hdev); todo
		else {
			DRM_INFO("hdcptx14: KSV List memory access denied\n");
			return;
		}
		hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x4);
	}

	if (am_hdmi->hdcp_try_times)
		mod_timer(&am_hdmi->hdcp_timer, jiffies + HZ / 100);
	else
		return;
	am_hdmi->hdcp_try_times--;
}

static void hdcp14_start_timer(struct am_hdmi_tx *am_hdmi)
{
	static int init_flag;

	if (!init_flag) {
		init_flag = 1;
		init_timer(&am_hdmi->hdcp_timer);
		am_hdmi->hdcp_timer.data = (ulong)am_hdmi;
		am_hdmi->hdcp_timer.function = hdcp14_events_handle;
		am_hdmi->hdcp_timer.expires = jiffies + HZ / 100;
		add_timer(&am_hdmi->hdcp_timer);
		am_hdmi->hdcp_try_times = 500;
		return;
	}
	am_hdmi->hdcp_try_times = 500;
	am_hdmi->hdcp_timer.expires = jiffies + HZ / 100;
	mod_timer(&am_hdmi->hdcp_timer, jiffies + HZ / 100);
}

static int am_hdcp14_enable(struct am_hdmi_tx *am_hdmi)
{
	am_hdmi->hdcp_mode = HDCP_MODE14;
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	hdmitx_hdcp_opr(6);
	hdmitx_hdcp_opr(1);
	hdcp14_start_timer(am_hdmi);
	return 0;
}

static int am_hdcp14_disable(struct am_hdmi_tx *am_hdmi)
{
	hdmitx_hdcp_opr(4);
	return 0;
}

static void set_pkf_duk_nonce(void)
{
	static int nonce_mode = 1; /* 1: use HW nonce   0: use SW nonce */

	/* Configure duk/pkf */
	hdmitx_hdcp_opr(0xc);
	if (nonce_mode == 1)
		hdmitx_wr_reg(HDMITX_TOP_SKP_CNTL_STAT, 0xf);
	else {
		hdmitx_wr_reg(HDMITX_TOP_SKP_CNTL_STAT, 0xe);
/* Configure nonce[127:0].
 * MSB must be written the last to assert nonce_vld signal.
 */
		hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x32107654);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xba98fedc);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0xcdef89ab);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x45670123);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_0,  0x76543210);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_1,  0xfedcba98);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_2,  0x89abcdef);
		hdmitx_wr_reg(HDMITX_TOP_NONCE_3,  0x01234567);
	}
	udelay(10);
}

static void am_sysfs_hdcp_event(struct drm_device *dev, unsigned int flag)
{
	char *envp1[2] = {  "HDCP22=1", NULL };
	char *envp0[2] = {  "HDCP22=0", NULL };

	DRM_INFO("generating hdcp22: %d\n  event\n", flag);
	if (flag)
		kobject_uevent_env(&dev->primary->kdev->kobj,
		KOBJ_CHANGE, envp1);
	else
		kobject_uevent_env(&dev->primary->kdev->kobj,
		KOBJ_CHANGE, envp0);
}

static int am_hdcp22_enable(struct am_hdmi_tx *am_hdmi)
{
	am_hdmi->hdcp_mode = HDCP_MODE22;
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	hdmitx_set_reg_bits(HDMITX_DWC_MC_CLKDIS, 1, 6, 1);
	udelay(5);
	hdmitx_set_reg_bits(HDMITX_DWC_HDCP22REG_CTRL, 3, 1, 2);
	hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 5, 1);
	udelay(10);
	hdmitx_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 5, 1);
	udelay(10);
	hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_MASK, 0);
	hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_MUTE, 0);
	set_pkf_duk_nonce();

	/*uevent to open hdcp_tx22*/
	am_sysfs_hdcp_event(am_hdmi->connector.dev, 1);
	return 0;
}

static int am_hdcp22_disable(struct am_hdmi_tx *am_hdmi)
{
	hdmitx_hdcp_opr(6);
	/*uevent to close hdcp_tx22*/
	am_sysfs_hdcp_event(am_hdmi->connector.dev, 0);
	return 0;
}

void am_hdcp_disable(struct am_hdmi_tx *am_hdmi)
{
	if (am_hdmi->hdcp_mode == HDCP_MODE22)
		am_hdcp22_disable(am_hdmi);
	else if (am_hdmi->hdcp_mode == HDCP_MODE14)
		am_hdcp14_disable(am_hdmi);
}
EXPORT_SYMBOL(am_hdcp_disable);

static int is_hdcp_hdmirx_supported(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_rx_type = 0x1;
	int st;

	/*if tx has hdcp22, then check if rx support hdcp22*/
	if (am_hdmi->hdcp_tx_type & 0x2) {
		hdmitx_ddc_hw_op(DDC_MUX_DDC);
		//mutex_lock(&am_hdmi->hdcp_mutex);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, HDCP_SLAVE);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, HDCP2_VERSION);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 0);
		mdelay(2);
		if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
			st = 0;
			DRM_INFO("ddc rd8b error 0x%02x 0x%02x\n",
				HDCP_SLAVE, HDCP2_VERSION);
		} else
			st = 1;
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
		if (hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI) & (1 << 2))
			hdcp_rx_type = 0x3;
		//mutex_unlock(&am_hdmi->hdcp_mutex);
	} else {
	/*if tx has hdcp14 or no key, then rx support hdcp14 acquiescently*/
		hdcp_rx_type = 0x1;
	}
	am_hdmi->hdcp_rx_type = hdcp_rx_type;

	DRM_INFO("hdmirx support hdcp14: %d\n", hdcp_rx_type & 0x1);
	DRM_INFO("hdmirx support hdcp22: %d\n", (hdcp_rx_type & 0x2) >> 1);
	return hdcp_rx_type;
}

int am_hdcp14_auth(struct am_hdmi_tx *am_hdmi)
{
	return hdmitx_hdcp_opr(0x2);
}

int am_hdcp22_auth(struct am_hdmi_tx *am_hdmi)
{
	return hdmitx_hdcp_opr(0x7);
}

/*firstly,check the hdmirx key
 *if hdmirx has hdcp22 key, start hdcp22. check auth status,
 *if failure,then start hdcp14
 *if hdmirx has hdcp14 key, start hdcp 14
 */
int am_hdcp_work(void *data)
{
	struct am_hdmi_tx *am_hdmi = data;
	struct drm_connector_state *state = am_hdmi->connector.state;
	int hdcp_fsm = 0;

	is_hdcp_hdmirx_supported(am_hdmi);
	if ((am_hdmi->hdcp_tx_type & 0x2) &&
		(am_hdmi->hdcp_rx_type & 0x2))
		hdcp_fsm = HDCP22_ENABLE;
	else
		hdcp_fsm = HDCP14_ENABLE;

	while (hdcp_fsm) {
		if (am_hdmi->hdcp_stop_flag)
			hdcp_fsm = HDCP_QUIT;

		switch (hdcp_fsm) {
		case HDCP22_ENABLE:
			am_hdcp22_enable(am_hdmi);
			DRM_INFO("hdcp22 work after 10s\n");
			/*this time is used to debug*/
			msleep_interruptible(10000);
			hdcp_fsm = HDCP22_AUTH;
			break;
		case HDCP22_AUTH:
			if (am_hdcp22_auth(am_hdmi))
				hdcp_fsm = HDCP22_SUCCESS;
			else
				hdcp_fsm = HDCP22_FAIL;
			break;
		case HDCP22_SUCCESS:
			state->content_protection =
				DRM_MODE_CONTENT_PROTECTION_ENABLED;
			DRM_DEBUG("hdcp22 is authenticated successfully\n");
			hdcp_fsm = HDCP22_AUTH;
			msleep_interruptible(200);
			break;
		case HDCP22_FAIL:
			am_hdcp22_disable(am_hdmi);
			state->content_protection =
				DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
			DRM_INFO("hdcp22 failure and start hdcp14\n");
			hdcp_fsm = HDCP14_ENABLE;
			msleep_interruptible(2000);
			break;
		case HDCP14_ENABLE:
			if ((am_hdmi->hdcp_tx_type & 0x1) == 0) {
				hdcp_fsm = HDCP_QUIT;
				break;
			}
			am_hdcp14_enable(am_hdmi);
			msleep_interruptible(500);
			hdcp_fsm = HDCP14_AUTH;
			break;
		case HDCP14_AUTH:
			if (am_hdcp14_auth(am_hdmi))
				hdcp_fsm = HDCP14_SUCCESS;
			else
				hdcp_fsm = HDCP14_FAIL;
			break;
		case HDCP14_SUCCESS:
			state->content_protection =
				DRM_MODE_CONTENT_PROTECTION_ENABLED;
			DRM_DEBUG("hdcp14 is authenticated successfully\n");
			hdcp_fsm = HDCP14_AUTH;
			msleep_interruptible(200);
			break;
		case HDCP14_FAIL:
			am_hdcp14_disable(am_hdmi);
			state->content_protection =
				DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
			DRM_DEBUG("hdcp14 failure\n");
			hdcp_fsm = HDCP_QUIT;
			break;
		case HDCP_QUIT:
		default:
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(am_hdcp_work);

int am_hdcp_init(struct am_hdmi_tx *am_hdmi)
{
	int ret;

	ret = drm_connector_attach_content_protection_property(
			&am_hdmi->connector);
	if (ret)
		return ret;
	return 0;
}
EXPORT_SYMBOL(am_hdcp_init);

/*bit0:hdcp14 bit 1:hdcp22*/
int is_hdcp_hdmitx_supported(struct am_hdmi_tx *am_hdmi)
{
	unsigned int hdcp_tx_type = 0;

	hdcp_tx_type |= hdmitx_hdcp_opr(0xa);
	hdcp_tx_type |= ((hdmitx_hdcp_opr(0xb)) << 1);
	am_hdmi->hdcp_tx_type = hdcp_tx_type;
	DRM_INFO("hdmitx support hdcp14: %d\n", hdcp_tx_type & 0x1);
	DRM_INFO("hdmitx support hdcp22: %d\n", (hdcp_tx_type & 0x2) >> 1);
	return hdcp_tx_type;
}
EXPORT_SYMBOL(is_hdcp_hdmitx_supported);
