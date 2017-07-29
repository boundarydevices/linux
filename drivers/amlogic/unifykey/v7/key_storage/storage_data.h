/*
 * drivers/amlogic/unifykey/v7/key_storage/storage_data.h
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

#ifndef __STORAGE_DATA_H_
#define __STORAGE_DATA_H_

#include <linux/types.h>
#include <linux/list.h>
#include "storage_def.h"

static LIST_HEAD(nodelist);
static int32_t aesfrom = -1;
static int32_t aesfrom_outer = -1;
//static int32_t version;
static uint8_t *storage_shar_in_block;
static uint8_t *storage_shar_out_block;
static uint8_t *storage_share_block;
static uint8_t *internal_storage_block;

#endif
