/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 NXP
 */


#ifndef __ELE_COMMON_H__
#define __ELE_COMMON_H__

#include "se_fw.h"

uint32_t plat_add_msg_crc(uint32_t *msg, uint32_t msg_len);
int imx_ele_msg_send_rcv(struct ele_mu_priv *priv);
void imx_se_free_tx_rx_buf(struct ele_mu_priv *priv);
int imx_se_alloc_tx_rx_buf(struct ele_mu_priv *priv);
int validate_rsp_hdr(struct ele_mu_priv *priv, unsigned int header,
		     uint8_t msg_id, uint8_t sz, bool is_base_api);
int plat_fill_cmd_msg_hdr(struct ele_mu_priv *priv,
			  struct mu_hdr *hdr,
			  uint8_t cmd,
			  uint32_t len,
			  bool is_base_api);
#ifdef CONFIG_IMX_ELE_TRNG
int ele_trng_init(struct device *dev);
#else
static inline int ele_trng_init(struct device *dev)
{
	return 0;
}
#endif

int ele_do_start_rng(struct device *dev);

#ifdef CONFIG_PM_SLEEP
int save_imem(struct device *dev);
int restore_imem(struct device *dev,
		 uint8_t *pool_name);
#endif

#endif /*__ELE_COMMON_H__ */
