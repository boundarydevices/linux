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

static int s400_api_send_command(struct imx_s400_api *s400_api)
{
	int err;

	err = mbox_send_message(s400_api->tx_chan, &s400_api->tx_msg);
	if (err < 0)
		return err;

	return 0;
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
	unsigned int wait;
	struct imx_s400_api *s400_api = NULL;
	int err = -EINVAL;

	err = get_imx_s400_api(&s400_api);

	if (MSG_SIZE(s400_api->tx_msg.header) == 2) {
		err = s400_api_send_command(s400_api);
		if (err < 0)
			return err;

		wait = msecs_to_jiffies(1000);
		if (!wait_for_completion_timeout(&s400_api->done, wait))
			return -ETIMEDOUT;

		fuse_id = s400_api->tx_msg.data[0] & 0xff;
		switch (fuse_id) {
		case OTP_UNIQ_ID:
			err = read_otp_uniq_id(s400_api, value);
			break;
		default:
			err = read_fuse_word(s400_api, value);
			break;
		}
	}

	return err;
}
EXPORT_SYMBOL_GPL(read_common_fuse);

int imx_s400_api_call(struct imx_s400_api *s400_api, void *value)
{
	unsigned int tag, command, ver;
	int err;

	err = -EINVAL;
	tag = MSG_TAG(s400_api->tx_msg.header);
	command = MSG_COMMAND(s400_api->tx_msg.header);
	ver = MSG_VER(s400_api->tx_msg.header);

	if (tag == s400_api->cmd_tag && ver == S400_VERSION) {
		reinit_completion(&s400_api->done);

		switch (command) {
		case S400_READ_FUSE_REQ:
			err = read_common_fuse(OTP_UNIQ_ID, value);
			break;
		default:
			return -EINVAL;
		}
	}

	return err;
}
EXPORT_SYMBOL_GPL(imx_s400_api_call);
