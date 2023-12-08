/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause*/
/*
 * Copyright 2019-2020, 2023 NXP
 */

#ifndef SECO_MU_IOCTL_H
#define SECO_MU_IOCTL_H

/* IOCTL definitions. */
struct seco_mu_ioctl_setup_iobuf {
	u8 *user_buf;
	u32 length;
	u32 flags;
	u64 seco_addr;
};

struct seco_mu_ioctl_shared_mem_cfg {
	u32 base_offset;
	u32 size;
};

struct seco_mu_ioctl_get_mu_info {
	u8 seco_mu_idx;
	u8 interrupt_idx;
	u8 tz;
	u8 did;
	u8 cmd_tag;
	u8 rsp_tag;
	u8 success_tag;
	u8 base_api_ver;
	u8 fw_api_ver;
};

struct seco_mu_ioctl_signed_message {
	u8 *message;
	u32 msg_size;
	u32 error_code;
};

struct seco_mu_ioctl_get_soc_info {
	u16 soc_id;
	u16 soc_rev;
};

#define SECO_MU_IO_FLAGS_IS_INPUT	(0x01u)
#define SECO_MU_IO_FLAGS_USE_SEC_MEM	(0x02u)
#define SECO_MU_IO_FLAGS_USE_SHORT_ADDR	(0x04u)
#define SECO_MU_IO_FLAGS_SHE_V2X	(0x08u)

#define SECO_MU_IOCTL			0x0A /* like MISC_MAJOR. */
#define SECO_MU_IOCTL_ENABLE_CMD_RCV	_IO(SECO_MU_IOCTL, 0x01)
#define SECO_MU_IOCTL_SHARED_BUF_CFG	_IOW(SECO_MU_IOCTL, 0x02, \
			struct seco_mu_ioctl_shared_mem_cfg)
#define SECO_MU_IOCTL_SETUP_IOBUF	_IOWR(SECO_MU_IOCTL, 0x03, \
			struct seco_mu_ioctl_setup_iobuf)
#define SECO_MU_IOCTL_GET_MU_INFO	_IOR(SECO_MU_IOCTL, 0x04, \
			struct seco_mu_ioctl_get_mu_info)
#define SECO_MU_IOCTL_SIGNED_MESSAGE	_IOWR(SECO_MU_IOCTL, 0x05, \
			struct seco_mu_ioctl_signed_message)
#define SECO_MU_IOCTL_GET_SOC_INFO	_IOR(SECO_MU_IOCTL, 0x06, \
			struct seco_mu_ioctl_get_soc_info)

#endif
