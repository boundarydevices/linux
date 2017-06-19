/*
 * drivers/amlogic/media/common/codec_mm/configs/configs_priv.h
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

#ifndef AMLOGIC_MEDIA_CONFIG_HEADER_PRIV__
#define AMLOGIC_MEDIA_CONFIG_HEADER_PRIV__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
int configs_config_system_init(void);
int configs_inc_node_ref_locked(
	struct mconfig_node *node);
int configs_dec_node_ref_locked(
	struct mconfig_node *node);
int config_dump(void *buf, int size);
int configs_config_setstr(const char *buf);

#endif

