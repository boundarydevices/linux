/*
 * drivers/amlogic/unifykey/v7/key_storage/storage_util.h
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

#ifndef __STORAGE_UTIL_H_
#define __STORAGE_UTIL_H_

#include <linux/types.h>

void *storage_malloc(int32_t size);
void *storage_zalloc(int32_t size);
void storage_free(void *ptr);

#endif
