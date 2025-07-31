/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __SSPM_LOGGER_H__
#define __SSPM_LOGGER_H__

#include <linux/types.h>

extern int scmi_plt_id;

ssize_t sspm_log_read(char __user *data, size_t len);
unsigned int sspm_log_poll(void);
unsigned int __init sspm_logger_init(phys_addr_t start, phys_addr_t limit);
int __init sspm_logger_init_done(void);
#endif
