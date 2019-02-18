/*
 * drivers/amlogic/usb/dwc_otg/310/dwc_otg_os_dep.h
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

#ifndef _DWC_OS_DEP_H_
#define _DWC_OS_DEP_H_

/**
 * @file
 *
 * This file contains OS dependent structures.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/pci.h>

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
# include <linux/irq.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)
# include <linux/usb/ch9.h>
#else
# include <linux/usb_ch9.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# include <linux/usb/gadget.h>
#else
# include <linux/usb_gadget.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
# include <asm/irq.h>
#endif

#include <asm/unaligned.h>
#include <asm/sizes.h>
#include <asm/param.h>
#include <asm/io.h>

/** The OS page size */
#define DWC_OS_PAGE_SIZE	PAGE_SIZE

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
typedef int gfp_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
# define IRQF_SHARED SA_SHIRQ
#endif

typedef struct os_dependent {
	/** Base address returned from ioremap() */
	void *base;

	/** Register offset for Diagnostic API */
	uint32_t reg_offset;

	struct platform_device *pldev;

	/** Start address of a PCI region */
	resource_size_t rsrc_start;

	/** Length address of a PCI region */
	resource_size_t rsrc_len;
} os_dependent_t;

#ifdef __cplusplus
}
#endif

#endif /* _DWC_OS_DEP_H_ */
