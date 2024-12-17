// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 NXP
 */

#include <linux/types.h>
#include <linux/completion.h>

#include "ele_common.h"
#include "v2x_base_msg.h"

#define FW_DBG_DUMP_FIXED_STR		"\nS40X: "

/*
 * v2x_start_rng() - prepare and send the command to start
 *                   initialization of the ELE RNG context
 *
 * returns:  0 on success.
 */
int v2x_start_rng(struct se_if_priv *priv)
{
	struct se_api_msg *tx_msg __free(kfree) = NULL;
	struct se_api_msg *rx_msg __free(kfree) = NULL;
	int ret = 0;

	if (!priv) {
		ret = -EINVAL;
		goto exit;
	}

	tx_msg = kzalloc(V2X_START_RNG_REQ_MSG_SZ, GFP_KERNEL);
	if (!tx_msg) {
		ret = -ENOMEM;
		goto exit;
	}

	rx_msg = kzalloc(V2X_START_RNG_RSP_MSG_SZ, GFP_KERNEL);
	if (!rx_msg) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = se_fill_cmd_msg_hdr(priv,
				  (struct se_msg_hdr *)&tx_msg->header,
				  V2X_START_RNG_REQ,
				  V2X_START_RNG_REQ_MSG_SZ,
				  true);
	if (ret)
		goto exit;

	ret = ele_msg_send_rcv(priv->priv_dev_ctx,
			       tx_msg,
			       V2X_START_RNG_REQ_MSG_SZ,
			       rx_msg,
			       V2X_START_RNG_RSP_MSG_SZ);
	if (ret < 0)
		goto exit;

	ret = se_val_rsp_hdr_n_status(priv,
				      rx_msg,
				      V2X_START_RNG_REQ,
				      V2X_START_RNG_RSP_MSG_SZ,
				      true);
	if (ret) {
		/* Initialization in progress for:
		 * P-TRNG at bit 0
		 * S-TRNG at bit 1
		 * Any of the bit is set, it in progress.
		 */
		if (rx_msg->data[1] & 0x3)
			goto exit;

		ret = -1;
	}
exit:
	return ret;
}

/*
 * v2x_pwr_state() - prepare and send the command to change
 *                   the power state of V2X-FW
 *
 * returns:  0 on success.
 */
int v2x_pwr_state(struct se_if_priv *priv, u16 action)
{
	struct se_api_msg *tx_msg __free(kfree) = NULL;
	struct se_api_msg *rx_msg __free(kfree) = NULL;
	int ret = 0;

	if (!priv) {
		ret = -EINVAL;
		goto exit;
	}

	tx_msg = kzalloc(V2X_PWR_STATE_MSG_SZ, GFP_KERNEL);
	if (!tx_msg) {
		ret = -ENOMEM;
		goto exit;
	}

	rx_msg = kzalloc(V2X_PWR_STATE_RSP_MSG_SZ, GFP_KERNEL);
	if (!rx_msg) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = se_fill_cmd_msg_hdr(priv,
				  (struct se_msg_hdr *)&tx_msg->header,
				  V2X_PWR_STATE,
				  V2X_PWR_STATE_MSG_SZ,
				  true);
	if (ret)
		goto exit;

	tx_msg->data[0] = action;

	ret = ele_msg_send_rcv(priv->priv_dev_ctx,
			       tx_msg,
			       V2X_PWR_STATE_MSG_SZ,
			       rx_msg,
			       V2X_PWR_STATE_RSP_MSG_SZ);
	if (ret < 0)
		goto exit;

	ret = se_val_rsp_hdr_n_status(priv,
				      rx_msg,
				      V2X_PWR_STATE,
				      V2X_PWR_STATE_RSP_MSG_SZ,
				      true);
	if (ret == -EPERM) {
		switch (rx_msg->data[0]) {
		case V2X_PERM_DENIED_FAIL_IND:
			dev_err(priv->dev,
				"TRNG is active or HSM/SHE session is remained open.");
			break;
		case V2X_INVAL_OPS_FAIL_IND:
			dev_err(priv->dev,
				"Invalid Action.");
			break;
		default:
			dev_err(priv->dev,
				"V2X Power Ops failed[0x%x].",
				RES_STATUS(rx_msg->data[0]));
		}
	}
exit:
	return ret;
}

int v2x_debug_dump(struct se_if_priv *priv)
{
	struct se_api_msg *tx_msg __free(kfree) = NULL;
	struct se_api_msg *rx_msg __free(kfree) = NULL;
	bool keep_logging;
	int msg_ex_cnt;
	int ret = 0;
	int i;

	if (!priv)
		return -EINVAL;

	tx_msg = kzalloc(V2X_DEBUG_DUMP_REQ_SZ, GFP_KERNEL);
	if (!tx_msg)
		return -ENOMEM;

	rx_msg = kzalloc(V2X_DEBUG_DUMP_RSP_SZ, GFP_KERNEL);
	if (!rx_msg)
		return -ENOMEM;

	ret = se_fill_cmd_msg_hdr(priv,
				  &tx_msg->header,
				  V2X_DEBUG_DUMP_REQ,
				  V2X_DEBUG_DUMP_REQ_SZ,
				  true);
	if (ret)
		return ret;

	tx_msg->header.tag = V2X_DEBUG_MU_MSG_CMD_TAG;
	tx_msg->header.ver = V2X_DEBUG_MU_MSG_VERS;
	tx_msg->data[0] = 0x1;
	msg_ex_cnt = 0;
	do {
		memset(rx_msg, 0x0, V2X_DEBUG_DUMP_RSP_SZ);

		ret = ele_msg_send_rcv(priv->priv_dev_ctx, tx_msg, V2X_DEBUG_DUMP_REQ_SZ,
				       rx_msg, V2X_DEBUG_DUMP_RSP_SZ);
		if (ret < 0)
			return ret;

		ret = se_val_rsp_hdr_n_status(priv, rx_msg, V2X_DEBUG_DUMP_REQ,
					      V2X_DEBUG_DUMP_RSP_SZ, true);
		if (ret) {
			dev_err(priv->dev, "Dump_Debug_Buffer Error: %x.", ret);
			break;
		}
		keep_logging = (rx_msg->header.size >= (V2X_DEBUG_DUMP_RSP_SZ >> 2) &&
				msg_ex_cnt < V2X_MAX_DBG_DMP_PKT);

		rx_msg->header.size -= 2;

		if (rx_msg->header.size > 4)
			rx_msg->header.size--;

		for (i = 0; i < rx_msg->header.size; i += 2)
			dev_info(priv->dev, "%s%02x_%02x: 0x%08x 0x%08x",
				 FW_DBG_DUMP_FIXED_STR,	msg_ex_cnt, i,
				 rx_msg->data[i + 1], rx_msg->data[i + 2]);

		msg_ex_cnt++;
	} while (keep_logging);

	return ret;
}
