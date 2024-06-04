// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek HDMI CEC Driver
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include "optee_private.h"
#include "mtk_hdmi_common.h"
#include "mtk_hdmi_ca.h"
#include "mtk_hdmi_hdcp.h"

#define HDMI_CA_LOG(fmt, arg...) \
	pr_debug("[HDMI][CA] %s,%d "fmt, __func__, __LINE__, ##arg)

#define HDMI_CA_FUNC()	\
	pr_debug("[HDMI][CA] %s\n", __func__)

static int hdmi_hdcp_match(struct tee_ioctl_version_data *ver, const void *data)
{
	/* Currently this driver only support GP Complaint OPTEE based fTPM TA */
	if ((ver->impl_id == TEE_IMPL_ID_OPTEE) &&
		(ver->gen_caps & TEE_GEN_CAP_GP))
		return 1;
	else
		return 0;
}

bool mtk_hdmi_hdcp_ca_create(struct mtk_hdmi *hdmi)
{
	int tee_ret;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session) {
		pr_notice("%s ca_hdmi_session already exists\n", __func__);
		return -EPERM;
	}

	hdcp->tee_ctx = tee_client_open_context(NULL, hdmi_hdcp_match, NULL, NULL);
	if (IS_ERR(hdcp->tee_ctx)) {
		pr_notice("%s ctx error\n", __func__);
		return -ENODEV;
	}

	memset(&hdcp->tee_session, 0, sizeof(struct tee_ioctl_open_session_arg));

	export_uuid(hdcp->tee_session.uuid, &HDCP_HDMI_TA_UUID);
	hdcp->tee_session.clnt_login = TEE_IOCTL_LOGIN_REE_KERNEL;
	hdcp->tee_session.num_params = 0;

	tee_ret = tee_client_open_session(hdcp->tee_ctx, &hdcp->tee_session, NULL);
	if (tee_ret < 0 || hdcp->tee_session.ret != 0) {
		HDMI_CA_LOG("Create ca_hdmi_session Error: %d\n", tee_ret);
		return false;
	}

	HDMI_CA_LOG("Create ca_hdmi_session ok: %d\n", tee_ret);

	return true;
}

bool mtk_hdmi_hdcp_ca_close(struct mtk_hdmi *hdmi)
{
	int tee_ret = 0;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		pr_notice("%s ca_hdmi_session does not exist\n", __func__);
		return -EPERM;
	}

	tee_ret = tee_client_close_session(hdcp->tee_ctx, hdcp->tee_session.session);

	if (tee_ret < 0 || hdcp->tee_session.ret != 0) {
		HDMI_CA_LOG("Close ca_hdmi_session Error: %d\n", tee_ret);
		return false;
	}

	hdcp->tee_session.session = 0;
	HDMI_CA_LOG("Close ca_hdmi_session ok: %d\n", tee_ret);

	return true;
}

void mtk_hdmi_hdcp_ca_write_reg(struct mtk_hdmi *hdmi, unsigned int u4addr, unsigned int u4data)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return;
	}

	inv_arg.func = HDMI_TA_WRITE_REG;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 2;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = u4addr & 0xFFF;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = u4data;
	param[1].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS)
		HDMI_CA_LOG("[CA]%s HDMI_TA_WRITE_REG err:%X\n", __func__, tee_ret);
}

void mtk_hdmi_hdcp_ca_write_hdcp_rst(struct mtk_hdmi *hdmi, unsigned int u4addr,
	unsigned int u4data)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	inv_arg.func = HDMI_TA_HDCP_RST;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 2;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = u4addr;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = u4data;
	param[1].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS)
		HDMI_CA_LOG("[CA]%s HDMI_TA_HDCP_RST err:%X\n", __func__, tee_ret);
}

void mtk_hdmi_hdcp_ca_write_ctrl(struct mtk_hdmi *hdmi, unsigned int u4addr, unsigned int u4data)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return;
	}

	inv_arg.func = HDMI_TA_WRITE_REG;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 2;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = u4addr;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = u4data;
	param[1].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS)
		HDMI_CA_LOG("[CA]%s HDMI_TA_WRITE_REG err:%X\n", __func__, tee_ret);
}

bool mtk_hdmi_hdcp_ca_get_aksv(struct mtk_hdmi *hdmi)
{
	int tee_ret = 0;
	unsigned char *ptr;
	unsigned char i;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[1];
	struct tee_shm *tee_shm;
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;
	unsigned char *pdata = hdcp->ca_hdcp_aksv;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return false;
	}

	tee_shm = tee_shm_alloc(hdcp->tee_ctx, HDCP_AKSV_COUNT, TEE_SHM_MAPPED);

	if (IS_ERR(tee_shm)) {
		HDMI_CA_LOG("[CA] AKSV kmalloc failure\n");
		return false;
	}

	inv_arg.func = HDMI_TA_GET_HDCP_AKSV;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[0].u.memref.shm_offs = 0;
	param[0].u.memref.size = HDCP_AKSV_COUNT;
	param[0].u.memref.shm = tee_shm;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);
	if (tee_ret != TEEC_SUCCESS)
		HDMI_CA_LOG("[CA]%s HDMI_TA_GET_HDCP_AKSV err:%X\n", __func__, tee_ret);

	ptr = (unsigned char *)param[0].u.memref.shm->kaddr;
	for (i = 0; i < HDCP_AKSV_COUNT; i++)
		pdata[i] = ptr[i];

	HDMI_CA_LOG("[CA]hdcp aksv : %x %x %x %x %x\n",
		   pdata[0], pdata[1], pdata[2], pdata[3], pdata[4]);

	tee_shm_free(tee_shm);
	return true;
}

bool mtk_hdmi_hdcp_ca_load_hdcp_key(struct mtk_hdmi *hdmi)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return false;
	}

	inv_arg.func = HDMI_TA_LOAD_HDCP_KEY;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = 0;
	param[0].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS) {
		HDMI_CA_LOG("[CA]%s HDMI_TA_LOAD_HDCP_KEY err:%X\n", __func__, tee_ret);
		return false;
	}

	return true;
}

bool mtk_hdmi_hdcp_ca_load_firmware(struct mtk_hdmi *hdmi)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return false;
	}

	inv_arg.func = HDMI_TA_LOAD_FIRMWARE;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = 0;
	param[0].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS) {
		HDMI_CA_LOG("[CA]%s HDMI_TA_LOAD_FIRMWARE err:%X\n", __func__, tee_ret);
		return false;
	}

	return true;
}

bool mtk_hdmi_hdcp_ca_reset_firmware(struct mtk_hdmi *hdmi)
{
	int tee_ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[2];
	struct mtk_hdmi_hdcp *hdcp = hdmi->hdcp;

	if (hdcp->tee_session.session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return false;
	}

	inv_arg.func = HDMI_TA_RESET_FIRMWARE;
	inv_arg.session = hdcp->tee_session.session;
	inv_arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = 0;
	param[0].u.value.b = 0;

	tee_ret = tee_client_invoke_func(hdcp->tee_ctx, &inv_arg, param);

	if (tee_ret != TEEC_SUCCESS) {
		HDMI_CA_LOG("[CA]%s HDMI_TA_RESET_FIRMWARE err:%X\n", __func__, tee_ret);
		return false;
	}

	return true;
}

bool mtk_hdmi_hdcp_ca_init(struct mtk_hdmi *hdmi)
{
	return mtk_hdmi_hdcp_ca_create(hdmi);
}

bool mtk_hdmi_hdcp_ca_deinit(struct mtk_hdmi *hdmi)
{
	return mtk_hdmi_hdcp_ca_close(hdmi);
}
