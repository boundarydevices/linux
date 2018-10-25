/*
 * include/linux/amlogic/media/codec_mm/codec_mm_keeper.h
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

#ifndef CODEC_MM_KEEPER_HEADER
#define CODEC_MM_KEEPER_HEADER

#define MEM_TYPE_CODEC_MM			111
#define MEM_TYPE_CODEC_MM_SCATTER	222

/*
 *don't call in interrupt;
 */
int codec_mm_keeper_mask_keep_mem(void *mem_handle, int type);

int is_codec_mm_keeped(void *mem_handle);
/*
 *can call in irq
 */
int codec_mm_keeper_unmask_keeper(int keep_id, int delayms);

#endif
