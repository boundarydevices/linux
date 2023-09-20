// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 NXP
 */

#include "ele_common.h"
#include "se_fw.h"

int imx_se_alloc_tx_rx_buf(struct ele_mu_priv *priv)
{
	int ret = 0;

	priv->tx_msg = devm_kzalloc(priv->dev,
				    sizeof(*priv->tx_msg),
				    GFP_KERNEL);
	if (!priv->tx_msg) {
		ret = -ENOMEM;
		dev_err(priv->dev, "Fail allocate mem for tx_msg.\n");
		return ret;
	}

	priv->rx_msg = devm_kzalloc(priv->dev,
				    sizeof(*priv->rx_msg),
				    GFP_KERNEL);

	if (!priv->rx_msg) {
		ret = -ENOMEM;
		dev_err(priv->dev, "Fail allocate mem for rx_msg.\n");
		return ret;
	}

	return ret;
}

void imx_se_free_tx_rx_buf(struct ele_mu_priv *priv)
{
	if (priv->tx_msg)
		devm_kfree(priv->dev, priv->tx_msg);

	if (priv->rx_msg)
		devm_kfree(priv->dev, priv->rx_msg);
}

int imx_ele_msg_send_rcv(struct ele_mu_priv *priv)
{
	unsigned int wait;
	int err;

	mutex_lock(&priv->mu_cmd_lock);
	mutex_lock(&priv->mu_lock);

	err = mbox_send_message(priv->tx_chan, priv->tx_msg);
	if (err < 0) {
		pr_err("Error: mbox_send_message failure.\n");
		mutex_unlock(&priv->mu_lock);
		mutex_unlock(&priv->mu_cmd_lock);
		return err;
	}
	err = 0;

	mutex_unlock(&priv->mu_lock);

	wait = msecs_to_jiffies(1000);
	if (!wait_for_completion_timeout(&priv->done, wait)) {
		pr_err("Error: wait_for_completion timed out.\n");
		err = -ETIMEDOUT;
	}

	mutex_unlock(&priv->mu_cmd_lock);

	return err;
}

/* Fill a command message header with a given command ID and length in bytes. */
int plat_fill_cmd_msg_hdr(struct ele_mu_priv *priv,
			  struct mu_hdr *hdr,
			  uint8_t cmd,
			  uint32_t len,
			  bool is_base_api)
{
	hdr->tag = priv->cmd_tag;
	hdr->ver = (is_base_api) ? priv->base_api_ver : priv->fw_api_ver;
	hdr->command = cmd;
	hdr->size = len >> 2;

	return 0;
}

int validate_rsp_hdr(struct ele_mu_priv *priv, unsigned int header,
		     uint8_t msg_id, uint8_t sz, bool is_base_api)
{
	unsigned int tag, command, size, ver;
	int ret = -EINVAL;

	tag = MSG_TAG(header);
	command = MSG_COMMAND(header);
	size = MSG_SIZE(header);
	ver = MSG_VER(header);

	do {
		if (tag != priv->rsp_tag) {
			dev_err(priv->dev,
				"MSG[0x%x] Hdr: Resp tag mismatch. (0x%x != 0x%x)",
				msg_id, tag, priv->rsp_tag);
			break;
		}

		if (command != msg_id) {
			dev_err(priv->dev,
				"MSG Header: Cmd id mismatch. (0x%x != 0x%x)",
				command, msg_id);
			break;
		}

		if (size != (sz >> 2)) {
			dev_err(priv->dev,
				"MSG[0x%x] Hdr: Cmd size mismatch. (0x%x != 0x%x)",
				msg_id, size, (sz >> 2));
			break;
		}

		if (is_base_api && (ver != priv->base_api_ver)) {
			dev_err(priv->dev,
				"MSG[0x%x] Hdr: Base API Vers mismatch. (0x%x != 0x%x)",
				msg_id, ver, priv->base_api_ver);
			break;
		} else if (!is_base_api && ver != priv->fw_api_ver) {
			dev_err(priv->dev,
				"MSG[0x%x] Hdr: FW API Vers mismatch. (0x%x != 0x%x)",
				msg_id, ver, priv->fw_api_ver);
			break;
		}

		ret = 0;

	} while (false);

	return ret;
}
