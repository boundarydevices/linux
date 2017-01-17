/*
 * drivers/amlogic/media/common/codec_mm/codec_mm_keeper_priv.h
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

#ifndef CODEC_MM_KEEP_PRIV_HEADER
#define CODEC_MM_KEEP_PRIV_HEADER
int codec_mm_keeper_mgr_init(void);
int codec_mm_keeper_dump_info(void *buf, int size);
int codec_mm_keeper_free_all_keep(int force);

#endif
