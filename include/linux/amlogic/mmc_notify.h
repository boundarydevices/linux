/*
 * include/linux/amlogic/mmc_notify.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __LINUX_AMLOGIC_MMC_NOTIFY_H
#define __LINUX_AMLOGIC_MMC_NOTIFY_H

#include <linux/notifier.h>


#define MMC_EVENT_UARTBOARD_IN		0x01
#define MMC_EVENT_UARTBOARD_OUT		0x02

#define MMC_EVENT_SDCARD_IN		0x04
#define MMC_EVENT_SDCARD_OUT		0x08

#define MMC_EVENT_JTAG_IN		0x10
#define MMC_EVENT_JTAG_OUT		0x20

#ifdef CONFIG_MMC_AML
extern int mmc_register_client(struct notifier_block *nb);
extern int mmc_unregister_client(struct notifier_block *nb);
extern int mmc_notifier_call_chain(unsigned long val, void *v);
#else /* !CONFIG_MMC_AML*/
static inline int mmc_register_client(struct notifier_block *nb)
{
	return 0;
}

static inline int mmc_unregister_client(struct notifier_block *nb)
{
	return 0;
};

static inline int mmc_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
};
#endif /* CONFIG_MMC_AML */

#endif
