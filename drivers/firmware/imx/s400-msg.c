// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 NXP
 * Author: Pankaj <pankaj.gupta@nxp.com>
	   Alice Guo <alice.guo@nxp.com>
 */

#include <linux/types.h>
#include <linux/firmware/imx/s400-api.h>
#include <linux/firmware/imx/s4_muap_ioctl.h>

#include "s4_muap.h"

/* Fill a command message header with a given command ID and length in bytes. */
static int plat_fill_cmd_msg_hdr(struct mu_hdr *hdr, uint8_t cmd, uint32_t len)
{
	struct imx_s400_api *priv = NULL;
	int err = 0;

	err = get_imx_s400_api(&priv);
	if (err) {
		pr_err("Error: iMX Sentinel MU is not probed successfully.\n");
		return err;
	}
	hdr->tag = priv->cmd_tag;
	hdr->ver = MESSAGING_VERSION_6;
	hdr->command = cmd;
	hdr->size = (uint8_t)(len / sizeof(uint32_t));

	return err;
};

static int imx_sentnl_msg_send_rcv(struct imx_s400_api *s400_api)
{
	unsigned int wait;
	int err = 0;

	mutex_lock(&s400_api->mu_cmd_lock);
	mutex_lock(&s400_api->mu_lock);

	err = mbox_send_message(s400_api->tx_chan, &s400_api->tx_msg);
	if (err < 0) {
		pr_err("Error: mbox_send_message failure.\n");
		mutex_unlock(&s400_api->mu_lock);
		return err;
	}
	mutex_unlock(&s400_api->mu_lock);

	wait = msecs_to_jiffies(1000);
	if (!wait_for_completion_timeout(&s400_api->done, wait)) {
		mutex_unlock(&s400_api->mu_cmd_lock);
		pr_err("Error: wait_for_completion timed out.\n");
		return -ETIMEDOUT;
	}

	mutex_unlock(&s400_api->mu_cmd_lock);
	return err;
}

static int read_otp_uniq_id(struct imx_s400_api *s400_api, u32 *value)
{
	unsigned int tag, command, size, ver, status;

	tag = MSG_TAG(s400_api->rx_msg.header);
	command = MSG_COMMAND(s400_api->rx_msg.header);
	size = MSG_SIZE(s400_api->rx_msg.header);
	ver = MSG_VER(s400_api->rx_msg.header);
	status = RES_STATUS(s400_api->rx_msg.data[0]);

	if (tag == 0xe1 && command == S400_READ_FUSE_REQ &&
	    size == 0x07 && ver == S400_VERSION && status == S400_SUCCESS_IND) {
		value[0] = s400_api->rx_msg.data[1];
		value[1] = s400_api->rx_msg.data[2];
		value[2] = s400_api->rx_msg.data[3];
		value[3] = s400_api->rx_msg.data[4];
		return 0;
	}

	return -EINVAL;
}

static int read_fuse_word(struct imx_s400_api *s400_api, u32 *value)
{
	unsigned int tag, command, size, ver, status;

	tag = MSG_TAG(s400_api->rx_msg.header);
	command = MSG_COMMAND(s400_api->rx_msg.header);
	size = MSG_SIZE(s400_api->rx_msg.header);
	ver = MSG_VER(s400_api->rx_msg.header);
	status = RES_STATUS(s400_api->rx_msg.data[0]);

	if (tag == 0xe1 && command == S400_READ_FUSE_REQ &&
	    size == 0x03 && ver == 0x06 && status == S400_SUCCESS_IND) {
		value[0] = s400_api->rx_msg.data[1];
		return 0;
	}

	return -EINVAL;
}

int read_common_fuse(uint16_t fuse_id, u32 *value)
{
	struct imx_s400_api *s400_api = NULL;
	int err = 0;

	err = get_imx_s400_api(&s400_api);
	if (err) {
		pr_err("Error: iMX Sentinel MU is not probed successfully.\n");
		return err;
	}
	err = plat_fill_cmd_msg_hdr((struct mu_hdr *)&s400_api->tx_msg.header, S400_READ_FUSE_REQ, 8);
	if (err) {
		pr_err("Error: plat_fill_cmd_msg_hdr failed.\n");
		return err;
	}

	s400_api->tx_msg.data[0] = fuse_id;
	err = imx_sentnl_msg_send_rcv(s400_api);
	if (err < 0)
		return err;

	switch (fuse_id) {
	case OTP_UNIQ_ID:
		err = read_otp_uniq_id(s400_api, value);
		break;
	default:
		err = read_fuse_word(s400_api, value);
		break;
	}

	return err;
}
EXPORT_SYMBOL_GPL(read_common_fuse);
