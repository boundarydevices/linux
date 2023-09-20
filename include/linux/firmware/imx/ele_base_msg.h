/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021-2023 NXP
 *
 * Header file for the ELE Base API(s).
 */

#ifndef ELE_BASE_MSG_H
#define ELE_BASE_MSG_H

#include <linux/types.h>

#define WORD_SZ				4
#define ELE_NONE_VAL			0x0

#define ELE_SUCCESS_IND			0xD6

#define ELE_GET_INFO_REQ		0xDA
#define ELE_GET_INFO_REQ_MSG_SZ		0x10
#define ELE_GET_INFO_RSP_MSG_SZ		0x08

#define ELE_GET_INFO_BUFF_SZ		0x100
#define ELE_GET_INFO_READ_SZ		0xA0
#define DEVICE_GET_INFO_SZ		0x100

#define GET_INFO_SOC_INFO_WORD_OFFSET	1
#define GET_INFO_UUID_WORD_OFFSET	3
#define GET_INFO_SL_NUM_MSB_WORD_OFF \
	(GET_INFO_UUID_WORD_OFFSET + 3)
#define GET_INFO_SL_NUM_LSB_WORD_OFF \
	(GET_INFO_UUID_WORD_OFFSET + 0)

#define ELE_PING_REQ			0x01
#define ELE_PING_REQ_SZ			0x04
#define ELE_PING_RSP_SZ			0x08

int ele_get_info(struct device *dev, phys_addr_t addr, u32 data_size);
int ele_ping(struct device *dev);

#endif
