/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 NXP
 */

#ifndef ELE_FW_API_H
#define ELE_FW_API_H

#include <linux/hw_random.h>

#define MESSAGING_VERSION_7		0x7

#define ELE_INIT_FW_REQ                 0x17
#define ELE_INIT_FW_REQ_SZ              0x04
#define ELE_INIT_FW_RSP_SZ              0x08


int ele_init_fw(struct device *dev);

#endif /* ELE_FW_API_H */
