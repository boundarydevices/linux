// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/err.h>
#include <linux/tee_drv.h>

#include "mtk_hdmirx.h"
#include "hdmi_rx2_8051.h"
#include "hdmi_rx2_hw.h"
#include "vga_table.h"
#include "hdmi_rx2_hal.h"
#include "hdmi_rx2_hdcp.h"
#include "hdmi_rx2_ctrl.h"
#include "hdmi_rx2_dvi.h"
#include "hdmi_rx2_aud_task.h"

void
hdcp2_load_prom_to_sram(struct MTK_HDMI *myhdmi)
{
	u32 i = 0;
	u32 length;

	length = sizeof(u1HDCP22Rom);
	for (i = 0; i < length; i++) {
		w_reg(HDMI20_PROM_ADDR, i);
		w_reg(HDMI20_PROM_DATA, u1HDCP22Rom[i]);
	}
}

void
hdcp2_load_pram_to_sram(struct MTK_HDMI *myhdmi)
{
	u32 i = 0;
	u32 length;

	length = sizeof(u1HDCP22Ram);
	for (i = 0; i < length; i++) {
		w_reg(HDMI20_PRAM_ADDR, i);
		w_reg(HDMI20_PRAM_DATA, u1HDCP22Ram[i]);
	}
}

void
hdcp2_check_prom(struct MTK_HDMI *myhdmi)
{
	u32 i = 0, temp;
	u32 length;
	u32 result;

	length = sizeof(u1HDCP22Rom);
	result = 0;

	for (i = 0; i < length; i++) {
		udelay(1);
		w_reg(HDMI20_PROM_ADDR, i);
		udelay(1);
		temp = r_reg(HDMI20_PROM_DATA);
		if (temp != u1HDCP22Rom[i]) {
			result++;
			RX_DEF_LOG("[rx]prom_err: 0x%x,0x%x,%d\n",
			temp, u1HDCP22Rom[i], result);
		}
	}

	RX_DEF_LOG("[rx]check prom result: %d\n", result);
	if (result != 0)
		RX_DEF_LOG("[rx]prom have err\n");
}

void
hdcp2_check_pram(struct MTK_HDMI *myhdmi)
{
	u32 i = 0, temp;
	u32 length;
	u32 result;

	length = sizeof(u1HDCP22Ram);
	result = 0;

	for (i = 0; i < length; i++) {
		udelay(1);
		w_reg(HDMI20_PRAM_ADDR, i);
		udelay(1);
		temp = r_reg(HDMI20_PRAM_DATA);
		if (temp != u1HDCP22Ram[i]) {
			result++;
			RX_DEF_LOG("[rx]pram_err: 0x%x,0x%x,%d,%d\n",
			temp, u1HDCP22Ram[i], i, result);
		}
	}

	RX_DEF_LOG("[rx]check pram result: %d\n", result);
	if (result != 0)
		RX_DEF_LOG("[rx]pram have err\n");
}

#ifdef HDMIRX_OPTEE_EN
#define TA_HDMIRX_CMD  1
#define TA_HDMIRX_INSTALL_HDCP_KEY 2
#define TA_HDMIRX_KEY_STATUS 3

#define TEEC_PAYLOAD_REF_COUNT 4
#define HDMI_OP_ENC_KEY_14	0
#define HDMI_OP_DEC_KEY_14	1
#define HDMI_OP_ENC_KEY_22	2
#define HDMI_OP_DEC_KEY_22	3

const uint8_t hdmirx_uuid[] = {
	0x37, 0x90, 0xde, 0x2c,
	0x9d, 0x0a, 0x44, 0x8e,
	0x9e, 0xdf, 0x97, 0xe8,
	0x15, 0x14, 0x90, 0x13};

static int hdmirx_optee_dev_match(struct tee_ioctl_version_data *t,
	const void *v)
{
	if (t->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
	return 0;
}

int optee_hdcp_open(struct MTK_HDMI *myhdmi)
{
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];
	int rc, ret = 0;

	/* 0: open context */
	myhdmi->hdcp_ctx = tee_client_open_context(NULL,
		hdmirx_optee_dev_match,
				NULL, NULL);
	if (IS_ERR(myhdmi->hdcp_ctx)) {
		RX_DEF_LOG("[RX]open_context failed err %ld",
			PTR_ERR(myhdmi->hdcp_ctx));
		ret = -1;
		goto out;
	}
	RX_DEF_LOG("[RX]open_context succeed! tee_ctx = 0x%p\n",
		myhdmi->hdcp_ctx);

	/* 1: open session */
	memset(&myhdmi->hdcp_osarg, 0, sizeof(myhdmi->hdcp_osarg));
	myhdmi->hdcp_osarg.num_params = TEEC_PAYLOAD_REF_COUNT;
	myhdmi->hdcp_osarg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	memcpy(myhdmi->hdcp_osarg.uuid, hdmirx_uuid, sizeof(hdmirx_uuid));
	memset(params, 0, sizeof(params));
	rc = tee_client_open_session(myhdmi->hdcp_ctx,
		&myhdmi->hdcp_osarg, params);

	if (rc || myhdmi->hdcp_osarg.ret) {
		RX_DEF_LOG("[RX]open_session failed err %d, ret=%d",
			rc, myhdmi->hdcp_osarg.ret);
		ret = -1;
		goto close_session;
	}
	RX_DEF_LOG("[RX]open_session succeed! session = %lx\n",
		(unsigned long)myhdmi->hdcp_osarg.session);

	return 0;

close_session:
	/* 3: close session */
	rc = tee_client_close_session(myhdmi->hdcp_ctx,
		myhdmi->hdcp_osarg.session);
	if (rc != 0)
		RX_DEF_LOG("close_session failed err %d", rc);
	/* 5: close context */
	tee_client_close_context(myhdmi->hdcp_ctx);
out:
	return ret;
}

int optee_hdcp_key(struct MTK_HDMI *myhdmi, u32 op)
{
	int rc, ret = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];

	if ((myhdmi->hdcp_osarg.session == 0) ||
		(myhdmi->hdcp_ctx == NULL)) {
		RX_DEF_LOG("[RX]TEE NULL\n");
		return 0;
	}

	memset(&arg, 0, sizeof(arg));
	arg.num_params = TEEC_PAYLOAD_REF_COUNT;
	arg.session = myhdmi->hdcp_osarg.session;
	arg.func = TA_HDMIRX_CMD;

	memset(params, 0, sizeof(params));
	params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	params[0].u.value.a = op;

	rc = tee_client_invoke_func(myhdmi->hdcp_ctx, &arg, params);
	if (rc) {
		RX_DEF_LOG("[RX]%s(): rc = %d\n", __func__, rc);
		ret = -1;
	}

	return ret;
}

int optee_key_status(struct MTK_HDMI *myhdmi, u32 *key_status)
{
	int rc;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param params[TEEC_PAYLOAD_REF_COUNT];

	if (key_status == NULL)
		return -1;

	*key_status = 0;

	if ((myhdmi->hdcp_osarg.session == 0) ||
		(myhdmi->hdcp_ctx == NULL)) {
		RX_DEF_LOG("[RX]TEE NULL\n");
		return -1;
	}

	memset(&arg, 0, sizeof(arg));
	arg.num_params = TEEC_PAYLOAD_REF_COUNT;
	arg.session = myhdmi->hdcp_osarg.session;
	arg.func = TA_HDMIRX_KEY_STATUS;

	memset(params, 0, sizeof(params));
	params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	rc = tee_client_invoke_func(myhdmi->hdcp_ctx, &arg, params);
	if (rc) {
		RX_DEF_LOG("[RX]%s(): rc = %d\n", __func__, rc);
		return -1;
	}
	*key_status = params[0].u.value.a;

	return 0;
}
#endif

void
hdcp2_load_hdcp_key(struct MTK_HDMI *myhdmi)
{
	/* switch to ARM, ARM access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 0, REG_PRAM_8051_CNTL_SEL);
	/* direct addr */
	w_fld(DIG_PHY_CONTROL_2, 1, REG_APLL_CNTL_SEL);
	hdcp2_load_prom_to_sram(myhdmi);
	hdcp2_load_pram_to_sram(myhdmi);
	/* switch to 8051, 8051 access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 1, REG_PRAM_8051_CNTL_SEL);
	w_fld(DIG_PHY_CONTROL_2, 0, REG_APLL_CNTL_SEL);
}

void
hdcp2_check_hdcp_rom(struct MTK_HDMI *myhdmi)
{
	/* switch to ARM, ARM access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 0, REG_PRAM_8051_CNTL_SEL);
	w_fld(DIG_PHY_CONTROL_2, 1, REG_APLL_CNTL_SEL);
	hdcp2_check_prom(myhdmi);
	/* switch to 8051, 8051 access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 1, REG_PRAM_8051_CNTL_SEL);
	w_fld(DIG_PHY_CONTROL_2, 0, REG_APLL_CNTL_SEL);
}

void
hdcp2_check_hdcp_ram(struct MTK_HDMI *myhdmi)
{
	/* switch to ARM, ARM access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 0, REG_PRAM_8051_CNTL_SEL);
	w_fld(DIG_PHY_CONTROL_2, 1, REG_APLL_CNTL_SEL);
	hdcp2_check_pram(myhdmi);
	/* switch to 8051, 8051 access prom and pram */
	w_fld(DIG_PHY_CONTROL_2, 1, REG_PRAM_8051_CNTL_SEL);
	w_fld(DIG_PHY_CONTROL_2, 0, REG_APLL_CNTL_SEL);
}

void
hdcp2_reset_sram_address(struct MTK_HDMI *myhdmi)
{
	w_reg(HDCP22_KEY0_ADDR, 0x0);
	w_reg(HDCP22_KEY1_ADDR, 0x0);
	w_reg(HDCP14_KEY_ADDR, 0x0);
	w_reg(HDMI20_PROM_ADDR, 0x0);
	w_reg(HDMI20_PRAM_ADDR, 0x0);
}

void
hdcp2_reset_sram_addr_hdcp1x_key(struct MTK_HDMI *myhdmi)
{
	w_reg(HDCP14_KEY_ADDR, 0x0);
}
