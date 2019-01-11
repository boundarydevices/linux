/*
 * drivers/amlogic/media/gdc/src/platform/system_gdc_io.c
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

#include "system_log.h"

#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/errno.h>
#include <linux/kernel.h>

static void *p_hw_base;

int32_t init_gdc_io(struct device_node *dn)
{
	p_hw_base = of_iomap(dn, 0);

	pr_info("reg base = %p\n", p_hw_base);
	if (!p_hw_base) {
		gdc_log(LOG_DEBUG, "failed to map register, %p\n", p_hw_base);
		return -1;
	}

	return 0;
}

void close_gdc_io(struct device_node *dn)
{
	gdc_log(LOG_DEBUG, "IO functionality has been closed");
}

uint32_t system_gdc_read_32(uint32_t addr)
{
	uint32_t result = 0;

	if (p_hw_base == NULL) {
		gdc_log(LOG_ERR, "Failed to base address %d\n", addr);
		return 0;
	}

	result = ioread32(p_hw_base + addr);
	gdc_log(LOG_DEBUG, "r [0x%04x]= %08x\n", addr, result);
	return result;
}

void system_gdc_write_32(uint32_t addr, uint32_t data)
{
	if (p_hw_base == NULL) {
		gdc_log(LOG_ERR, "Failed to write %d to addr %d\n", data, addr);
		return;
	}

	void *ptr = (void *)(p_hw_base + addr);

	iowrite32(data, ptr);
	gdc_log(LOG_DEBUG, "w [0x%04x]= %08x\n", addr, data);
}
