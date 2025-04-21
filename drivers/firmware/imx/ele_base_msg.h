/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2024-2025 NXP
 *
 * Header file for the EdgeLock Enclave Base API(s).
 */

#ifndef ELE_BASE_MSG_H
#define ELE_BASE_MSG_H

#include <linux/device.h>
#include <linux/types.h>

#include "se_ctrl.h"

#define ELE_NONE_VAL			0x0

#define ELE_GET_INFO_REQ		0xDA
#define ELE_GET_INFO_REQ_MSG_SZ		0x10
#define ELE_GET_INFO_RSP_MSG_SZ		0x08

#define SOC_ID_MASK			0x0000FFFF

#define MAX_UID_SIZE                     (16)
#define DEV_GETINFO_ROM_PATCH_SHA_SZ     (32)
#define DEV_GETINFO_FW_SHA_SZ            (32)
#define DEV_GETINFO_OEM_SRKH_SZ          (64)
#define DEV_GETINFO_MIN_VER_MASK	0xFF
#define DEV_GETINFO_MAJ_VER_MASK	0xFF00
#define ELE_DEV_INFO_EXTRA_SZ		0x60

struct dev_info {
	u8  cmd;
	u8  ver;
	u16 length;
	u16 soc_id;
	u16 soc_rev;
	u16 lmda_val;
	u8  ssm_state;
	u8  dev_atts_api_ver;
	u8  uid[MAX_UID_SIZE];
	u8  sha_rom_patch[DEV_GETINFO_ROM_PATCH_SHA_SZ];
	u8  sha_fw[DEV_GETINFO_FW_SHA_SZ];
};

struct dev_addn_info {
	u8  oem_srkh[DEV_GETINFO_OEM_SRKH_SZ];
	u8  trng_state;
	u8  csal_state;
	u8  imem_state;
	u8  reserved2;
	u8  oem_pqc_srkh[DEV_GETINFO_OEM_SRKH_SZ];
	u8  reserved3[32];
};

struct ele_dev_info {
	struct dev_info d_info;
	struct dev_addn_info d_addn_info;
};

#define ELE_GET_INFO_BUFF_SZ		sizeof(struct ele_dev_info)

#define GET_SERIAL_NUM_FROM_UID(x, uid_word_sz) \
	(((u64)(((u32 *)(x))[(uid_word_sz) - 1]) << 32) | ((u32 *)(x))[0])

#define ELE_MAX_DBG_DMP_PKT		30
#define ELE_NON_DUMP_BUFFER_SZ		3
#define ELE_DEBUG_DUMP_REQ		0x21
#define ELE_DEBUG_DUMP_REQ_SZ		0x4
#define ELE_DEBUG_DUMP_RSP_SZ		0x5c

#define ELE_PING_REQ			0x01
#define ELE_PING_REQ_SZ			0x04
#define ELE_PING_RSP_SZ			0x08

#define ELE_SERVICE_SWAP_REQ		0xDF
#define ELE_SERVICE_SWAP_REQ_MSG_SZ	0x18
#define ELE_SERVICE_SWAP_RSP_MSG_SZ	0x0C
#define ELE_IMEM_SIZE			0x10000
#define ELE_IMEM_STATE_OK		0xCA
#define ELE_IMEM_STATE_BAD		0xFE
#define ELE_IMEM_STATE_WORD		0x27
#define ELE_IMEM_STATE_MASK		0x00ff0000
#define ELE_IMEM_EXPORT			0x1
#define ELE_IMEM_IMPORT			0x2

#define ELE_FW_GET_FWAUTH_REQ			0x02
#define ELE_FW_AUTH_REQ_SZ		0x10
#define ELE_FW_AUTH_RSP_MSG_SZ		0x08

#define ELE_FW_AUTH_REQ			0x02
#define ELE_FW_AUTH_REQ_SZ		0x10
#define ELE_FW_AUTH_RSP_MSG_SZ		0x08

#define ELE_START_RNG_REQ		0xA3
#define ELE_START_RNG_REQ_MSG_SZ	0x04
#define ELE_START_RNG_RSP_MSG_SZ	0x08

#define ELE_WRITE_FUSE			0xD6
#define ELE_WRITE_FUSE_REQ_MSG_SZ	12
#define ELE_WRITE_FUSE_RSP_MSG_SZ	12

#define ELE_READ_FUSE_REQ		0x97
#define ELE_READ_FUSE_REQ_MSG_SZ	0x08
#define ELE_READ_FUSE_RSP_MSG_SZ	0x0C
#define ELE_READ_FUSE_OTP_UNQ_ID_RSP_MSG_SZ \
					0x1C

#define ELE_VOLT_CHANGE_START_REQ			0x12
#define ELE_VOLT_CHANGE_FINISH_REQ			0x13
#define ELE_VOLT_CHANGE_REQ_MSG_SZ			0x4
#define ELE_VOLT_CHANGE_RSP_MSG_SZ			0x8

#define ELE_GET_STATE			0xB2
#define ELE_GET_STATE_REQ_SZ		0x04
#define ELE_GET_STATE_RSP_SZ		0x10

#define V2X_FW_IMG_DDR_ADDR		0x8b000000
#define ELE_V2X_FW_AUTH_REQ		0xB1
#define ELE_V2X_FW_AUTH_REQ_SZ		0x10
#define ELE_V2X_FW_AUTH_RSP_MSG_SZ	0x08

#define ELE_GET_FW_VERSION_REQ		0x9d
#define ELE_GET_FW_VERSION_REQ_SZ	0x04
#define ELE_GET_FW_VERSION_RSP_SZ	0x10

int ele_get_info(struct se_if_priv *priv, struct ele_dev_info *s_info);
int ele_fetch_soc_info(struct se_if_priv *priv, void *data);
int ele_ping(struct se_if_priv *priv);
int ele_service_swap(struct se_if_priv *priv,
		     phys_addr_t addr,
		     u32 addr_size, u16 flag);
int ele_fw_authenticate(struct se_if_priv *priv, phys_addr_t addr);
int ele_debug_dump(struct se_if_priv *priv);
int ele_start_rng(struct se_if_priv *priv);
int ele_write_fuse(struct se_if_priv *priv, u16 fuse_index,
		   u32 value, bool block);
int ele_voltage_change_req(struct se_if_priv *priv, bool start);
int read_common_fuse(struct se_if_priv *priv,
		     u16 fuse_id, u32 *value);
int ele_get_v2x_fw_state(struct se_if_priv *priv, uint32_t *state);
int ele_v2x_fw_authenticate(struct se_if_priv *priv, phys_addr_t addr);
int ele_get_fw_version(struct se_if_priv *priv, u32 *fw_ver_word,
		       u32 *commit_sha1);
#endif
