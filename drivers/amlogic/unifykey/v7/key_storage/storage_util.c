/*
 * drivers/amlogic/unifykey/v7/key_storage/storage_util.c
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

#include <linux/slab.h>
#include "storage_util.h"

/* memory pool for storage malloc*/

void *storage_malloc(int32_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

void *storage_zalloc(int32_t size)
{
	return kzalloc(size, GFP_KERNEL);
}

void storage_free(void *ptr)
{
	if (ptr != NULL)
		kfree(ptr);
}
