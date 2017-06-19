/*
 * include/linux/amlogic/media/canvas/canvas_mgr.h
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

#ifndef CANVAS_MGR_HEADER_
#define CANVAS_MGR_HEADER_

#include <linux/types.h>

#define CANVAS_MAX_NUM 256

struct canvas_info {
	const char *owner;
	unsigned long alloc_time;
	const char *oldowner;
	int fixed_onwer;
};

struct canvas_pool {
	unsigned long flags;
	unsigned long fiq_flags;
	spinlock_t lock;
	int canvas_max;
	int free_num;
	struct canvas_info info[CANVAS_MAX_NUM];
	unsigned long canvas_map[(CANVAS_MAX_NUM / BITS_PER_LONG) + 1];
	int next_alloced_index;
	int next_dump_index;
	unsigned long last_cat_map;
};

#define canvas_0(v) ((v) & 0xff)
#define canvas_1(v) (((v)>>8) & 0xff)
#define canvas_2(v) (((v) >> 16) & 0xff)
#define canvas_3(v) (((v) >> 24) & 0xff)

#define canvasY(v) canvas_0(v)
#define canvasU(v) canvas_1(v)
#define canvasV(v) canvas_2(v)
#define canvasUV(v) canvas_1(v)
#define canvasADDR(y, u, v) ((y) | ((u) << 8) | ((v) << 16))

enum canvas_map_type_e {
	/*don't changed this value,*/
	CANVAS_MAP_TYPE_1 = 1,	/*one canvas in u32:saved:0x00000001 */
	CANVAS_MAP_TYPE_NV21 = 2,	/*one canvas in u32:like:0x00020201 */
	CANVAS_MAP_TYPE_YUV = 3,	/*three canvas in u32:like:0x00030201 */
	CANVAS_MAP_TYPE_4 = 4,	/*for canvas in u32:like:0x04030201 */
	CANVAS_MAP_TYPE_MAX,
};

int canvas_pool_map_alloc_canvas(const char *owner);
int canvas_pool_map_free_canvas(int index);
int canvas_pool_register_const_canvas(int start_index, int num,
	const char *owner);

int canvas_pool_get_canvas_info(int index, struct canvas_info *info);
int canvas_pool_canvas_num(void);

u32 canvas_pool_alloc_canvas_table(const char *owner, u32 *tab, int size,
	enum canvas_map_type_e type);

u32 canvas_pool_free_canvas_table(u32 *tab, int size);
u32 canvas_pool_dump_canvas_table(u32 *tab, int size);

u32 canvas_pool_canvas_alloced(int index);

int canvas_pool_get_static_canvas_by_name(const char *owner, u8 *tab,
	int size);

#endif
