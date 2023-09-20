// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021-2023 NXP
 */

#include <linux/types.h>
#include <linux/completion.h>

#include <linux/firmware/imx/ele_base_msg.h>
#include <linux/firmware/imx/ele_mu_ioctl.h>

#include "ele_common.h"

int ele_get_info(struct device *dev, phys_addr_t addr, u32 data_size)
{
	struct ele_mu_priv *priv = dev_get_drvdata(dev);
	int ret;
	unsigned int status;

	ret = imx_se_alloc_tx_rx_buf(priv);
	if (ret)
		return ret;

	ret = plat_fill_cmd_msg_hdr(priv,
				    (struct mu_hdr *)&priv->tx_msg->header,
				    ELE_GET_INFO_REQ,
				    ELE_GET_INFO_REQ_MSG_SZ,
				    true);
	if (ret)
		goto exit;

	priv->tx_msg->data[0] = upper_32_bits(addr);
	priv->tx_msg->data[1] = lower_32_bits(addr);
	priv->tx_msg->data[2] = data_size;
	ret = imx_ele_msg_send_rcv(priv);
	if (ret < 0)
		goto exit;

	ret  = validate_rsp_hdr(priv,
				priv->rx_msg->header,
				ELE_GET_INFO_REQ,
				ELE_GET_INFO_RSP_MSG_SZ,
				true);
	if (ret)
		goto exit;

	status = RES_STATUS(priv->rx_msg->data[0]);
	if (status != priv->success_tag) {
		dev_err(dev, "Command Id[%d], Response Failure = 0x%x",
			ELE_GET_INFO_REQ, status);
		ret = -1;
	}

exit:
	imx_se_free_tx_rx_buf(priv);

	return ret;
}

int ele_ping(struct device *dev)
{
	struct ele_mu_priv *priv = dev_get_drvdata(dev);
	int ret;
	unsigned int status;

	ret = imx_se_alloc_tx_rx_buf(priv);
	if (ret)
		return ret;

	ret = plat_fill_cmd_msg_hdr(priv,
				    (struct mu_hdr *)&priv->tx_msg->header,
				    ELE_PING_REQ, ELE_PING_REQ_SZ,
				    true);
	if (ret) {
		pr_err("Error: plat_fill_cmd_msg_hdr failed.\n");
		goto exit;
	}

	ret = imx_ele_msg_send_rcv(priv);
	if (ret)
		goto exit;

	ret  = validate_rsp_hdr(priv,
				priv->rx_msg->header,
				ELE_PING_REQ,
				ELE_PING_RSP_SZ,
				true);
	if (ret)
		goto exit;

	status = RES_STATUS(priv->rx_msg->data[0]);
	if (status != priv->success_tag) {
		dev_err(dev, "Command Id[%d], Response Failure = 0x%x",
			ELE_PING_REQ, status);
		ret = -1;
	}
exit:
	imx_se_free_tx_rx_buf(priv);

	return ret;
}
