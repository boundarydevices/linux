/*
 * drivers/amlogic/media/common/canvas/canvas_mgr.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/major.h>

#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>

/*ignore canvas 0,
 *because it hard for check and easy cleared by others.
 */
#define VALID_CANVAS(n) ((n) >= 0 && (n) < canvas_pool_canvas_num())

static struct canvas_pool global_pool;

struct canvas_pool *get_canvas_pool(void)
{
	return &global_pool;
}

static inline void cavas_pool_lock(struct canvas_pool *pool)
{
	/* /raw_local_save_flags(pool->fiq_flags); */
	/* /local_fiq_disable(); */
	spin_lock_irqsave(&pool->lock, pool->flags);

}

static inline void cavas_pool_unlock(struct canvas_pool *pool)
{
	spin_unlock_irqrestore(&pool->lock, pool->flags);
	/* /raw_local_irq_restore(pool->fiq_flags); */
}

static int
canvas_pool_map_alloc_canvas_in(struct canvas_pool *pool, const char *owner)
{
	int i;
	int index = -1;
	int start_index = pool->next_alloced_index;
	int looped = start_index <= 1;	/* only start on=0,1 not retry. */

	do {
		i = find_next_zero_bit(pool->canvas_map,
			pool->canvas_max,
			start_index);
		if (!VALID_CANVAS(i)) {
			if (!looped) {
				start_index = 1;
				looped = 1;
				continue;
			} else
				return -1;
		}
		cavas_pool_lock(pool);
		if (!test_and_set_bit(i, pool->canvas_map)) {
			index = i;
			pool->info[index].oldowner = owner;
			pool->info[index].owner = owner;
			pool->info[index].alloc_time = jiffies;
		}
		cavas_pool_unlock(pool);
	} while (index < 0);
	if (index > 0) {
		cavas_pool_lock(pool);
		if (index + 1 < pool->canvas_max)
			pool->next_alloced_index = index + 1;
		else
			pool->next_alloced_index = 1;	/* /ignore canvas 0; */
		pool->free_num--;
		cavas_pool_unlock(pool);
	}
	return index;
}

int canvas_pool_map_alloc_canvas(const char *owner)
{
	return canvas_pool_map_alloc_canvas_in(get_canvas_pool(), owner);
}

/*
 *num >=1 && <=4
 *when need canvas is continue;
 *like:
 *canvas = 0x04030201;
 */
static int
canvas_pool_map_alloc_canvas_continue_set(struct canvas_pool *pool,
					  const char *owner, u32 *canvas,
					  enum canvas_map_type_e type)
{
	int found_retry = 0;
	int found = 0;
	int canvas1, canvas2, canvas3, canvas4;
	int start = pool->next_alloced_index;
	u32 canvasval = 0;

	do {
		canvas1 = canvas2 = canvas3 = canvas4 = 0;
		switch (type) {
		case CANVAS_MAP_TYPE_4:
			canvas1 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas2 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas3 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas4 = canvas_pool_map_alloc_canvas_in(pool, owner);
			break;
		case CANVAS_MAP_TYPE_YUV:
			canvas2 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas3 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas4 = canvas_pool_map_alloc_canvas_in(pool, owner);
			break;
		case CANVAS_MAP_TYPE_NV21:
			canvas3 = canvas_pool_map_alloc_canvas_in(pool, owner);
			canvas4 = canvas_pool_map_alloc_canvas_in(pool, owner);
			break;
		case CANVAS_MAP_TYPE_1:
			canvas4 = canvas_pool_map_alloc_canvas_in(pool, owner);
			break;
		default:
			break;
		}
		switch (type) {
		case CANVAS_MAP_TYPE_4:
			found = (canvas1 > 0) && (canvas2 == canvas1 + 1)
				&& (canvas3 == canvas2 + 1)
				&& (canvas4 == canvas3 + 1);
			canvasval =
				canvas1 | (canvas2 << 8) | (canvas3 << 16) |
				(canvas4 << 24);
			break;
		case CANVAS_MAP_TYPE_YUV:
			found = (canvas2 > 0) && (canvas3 == canvas2 + 1)
				&& (canvas4 == canvas3 + 1);
			canvasval = canvas2 | (canvas3 << 8) | (canvas4 << 16);
			break;
		case CANVAS_MAP_TYPE_NV21:
			found = (canvas3 > 0) && (canvas4 == canvas3 + 1);
			canvasval = canvas3 | (canvas4 << 8) | (canvas4 << 16);
			break;
		case CANVAS_MAP_TYPE_1:
			found = (canvas4 > 0);
			canvasval = canvas4;
			break;
		default:
			break;
		}
		if (!found) {
			if (canvas1 > 0)
				canvas_pool_map_free_canvas(canvas1);
			if (canvas2 > 0)
				canvas_pool_map_free_canvas(canvas2);
			if (canvas3 > 0)
				canvas_pool_map_free_canvas(canvas3);
			if (canvas4 > 0)
				canvas_pool_map_free_canvas(canvas4);
			cavas_pool_lock(pool);
			if (start + 1 < pool->canvas_max - type)
				pool->next_alloced_index = start + 1;
			else {
				pool->next_alloced_index = 1;
				/* /ignore canvas 0; */
			}
			cavas_pool_unlock(pool);
			canvasval = 0;
		}

	} while (!found && (found_retry++ < pool->canvas_max));
	if (found) {
		*canvas = canvasval;
		return 0;
	}
	return -1;
}

static int canvas_pool_map_free_canvas_in(struct canvas_pool *pool, int index)
{
	if (!VALID_CANVAS(index))
		return -1;
	cavas_pool_lock(pool);
	if (!test_and_clear_bit(index, pool->canvas_map))
		pr_warn("canvas is free and freed again! index =%d\n", index);
	pool->info[index].oldowner = pool->info[index].owner;
	pool->info[index].owner = NULL;
	pool->info[index].alloc_time = 0;
	pool->free_num++;
	cavas_pool_unlock(pool);
	return 0;
}

int canvas_pool_map_free_canvas(int index)
{
	return canvas_pool_map_free_canvas_in(get_canvas_pool(), index);
}

int canvas_pool_canvas_num(void)
{
	return get_canvas_pool()->canvas_max;
}

static void canvas_pool_init(void)
{
	struct canvas_pool *pool = get_canvas_pool();

	memset(pool, 0, sizeof(struct canvas_pool));
	spin_lock_init(&pool->lock);
	pool->canvas_max = CANVAS_MAX_NUM;
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6TV)
		pool->canvas_max = 192;
	pool->next_alloced_index = pool->canvas_max / 2;
	/* /start at end part index. */
}

int
canvas_pool_register_const_canvas(int start_index, int end, const char *owner)
{
	int i;
	struct canvas_pool *pool = get_canvas_pool();

	for (i = 0; i < end - start_index + 1; i++) {
		int index = start_index + i;

		if (test_and_set_bit(index, pool->canvas_map))
			pr_warn("canvas have be used inddex=%d,old owner =%s,new owner =%s\n",
				index, pool->info[index].owner, owner);
		pool->info[index].owner = owner;
		pool->info[index].oldowner = NULL;
		pool->info[index].alloc_time = jiffies;
		pool->info[index].fixed_onwer = 1;
	}
	return 0;
}

int canvas_pool_get_canvas_info(int index, struct canvas_info *info)
{
	struct canvas_pool *pool = get_canvas_pool();

	if (!VALID_CANVAS(index))
		return -1;
	cavas_pool_lock(pool);
	*info = pool->info[index];
	cavas_pool_unlock(pool);
	return 0;
}

void canvas_pool_dump_canvas_info(void)
{
	int i, ret;
	struct canvas_info info;
	struct canvas_pool *pool = get_canvas_pool();

	for (i = 0; i < pool->canvas_max; i++) {
		ret = canvas_pool_get_canvas_info(i, &info);
		if (ret == 0 && test_bit(i, pool->canvas_map)) {
			const char *o1 = info.owner ? info.owner : "none";
			const char *o2 = info.oldowner ? info.oldowner : "none";

			pr_info("canvas[%x] used=%d owner =%s",
				(unsigned int)i,
				test_bit(i, pool->canvas_map), o1);
			pr_info("owner=%s,oldowner =%s,time=%ld,fixed=%d\n",
				o1, o2, info.alloc_time, info.fixed_onwer);
		}
	}
}

u32
canvas_pool_alloc_canvas_table(const char *owner, u32 *tab, int size,
	enum canvas_map_type_e type)
{
	int i;
	struct canvas_pool *pool = get_canvas_pool();
	int err = 0;
	int oldstart = pool->next_alloced_index;

	for (i = 0; i < size; i++) {
		u32 canvas = 0;

		err = canvas_pool_map_alloc_canvas_continue_set(pool, owner,
			&canvas, type);
		/*type equal num */
		if (err)
			break;
		tab[i] = canvas;
	}
	if (err) {
		pr_warn("%s: alloc canvas failed,on i=%d,size=%d\n", owner, i,
			size);
		if (i > 0)
			canvas_pool_free_canvas_table(tab, i);
		return err;
	}
	canvas_pool_dump_canvas_table(tab, size);
	pool->next_alloced_index = oldstart + 1;
	if (pool->next_alloced_index >= pool->canvas_max)
		pool->next_alloced_index = 1;
	return 0;
}

u32 canvas_pool_free_canvas_table(u32 *tab, int size)
{
	int i;
	int canvas1, canvas2, canvas3, canvas4;

	for (i = 0; i < size; i++) {
		canvas1 = tab[i] & 0xff;
		canvas2 = (tab[i] >> 8) & 0xff;
		canvas3 = (tab[i] >> 16) & 0xff;
		canvas4 = (tab[i] >> 24) & 0xff;
		if (canvas1 >= 0)
			canvas_pool_map_free_canvas(canvas1);
		if (canvas2 >= 0 && canvas2 != canvas1)
			canvas_pool_map_free_canvas(canvas2);
		if (canvas3 >= 0 && canvas3 != canvas2 && canvas3 != canvas1)
			canvas_pool_map_free_canvas(canvas3);
		if (canvas4 >= 0 && canvas4 != canvas3 && canvas4 != canvas2 &&
			canvas4 != canvas1)
			canvas_pool_map_free_canvas(canvas4);
	}
	memset(tab, 0, size * sizeof(u32));
	return 0;
}

u32 canvas_pool_dump_canvas_table(u32 *tab, int size)
{
	int i;
	int canvas1, canvas2, canvas3;

	for (i = 0; i < size; i++) {
		canvas1 = tab[i] & 0xff;
		canvas2 = (tab[i] >> 8) & 0xff;
		canvas3 = (tab[i] >> 16) & 0xff;
		pr_info("0x%08x:Y=%x,U=%x,V=%x\n", tab[i], (u32) canvas1,
			(u32) canvas2, (u32) canvas3);
	}
	return 0;
}

u32 canvas_pool_canvas_alloced(int index)
{
	struct canvas_pool *pool = get_canvas_pool();

	if (index < 0 || index >= pool->canvas_max)
		return 0;
	return !!test_bit(index, pool->canvas_map);
}

static ssize_t
canvas_pool_map_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	struct canvas_pool *pool = get_canvas_pool();
	int max = min(pool->next_dump_index + 64, pool->canvas_max);
	int ret;
	int i;
	ssize_t size = 0;
	struct canvas_info info = {NULL, 0, NULL, 0};
	struct canvas_s canvas;

	if (jiffies - pool->last_cat_map > 5 * HZ) {
		pool->next_dump_index = 0;
		/*timeout :not continue dump,dump from first. */
	}

	for (i = pool->next_dump_index;
		i < max && size < PAGE_SIZE - 256; i++) {

		const char *o1, *o2;

		ret = canvas_pool_get_canvas_info(i, &info);
		canvas_read(i, &canvas);
		o1 = info.owner ? info.owner : "none";
		o2 = info.oldowner ? info.oldowner : "none";
		size += snprintf(buf + size, PAGE_SIZE - size,
			"cavas[%02x],used=%d,fix=%d\town=%s,\toown=%s,\tt=%ld,",
			(unsigned int)i,
			test_bit(i, pool->canvas_map),
			info.fixed_onwer, o1, o2, info.alloc_time);
		size += snprintf(buf + size, PAGE_SIZE - size,
				"\taddr=%08x,\ts=%d X %d,\trap=%d,\tmod=%d\n",
				(unsigned int)canvas.addr,
				(int)canvas.width,
				(int)canvas.height,
				(int)canvas.wrap,
				(int)canvas.blkmode);
	}
	pool->next_dump_index = i;
	if (pool->next_dump_index >= pool->canvas_max)
		pool->next_dump_index = 0;
	pool->last_cat_map = jiffies;
	return size;
}

static ssize_t
canvas_pool_states_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;
	int i;
	struct canvas_pool *pool = get_canvas_pool();
	size += snprintf(buf + size, PAGE_SIZE - size, "canvas total num: %d\n",
			 pool->canvas_max);
	size +=
	    snprintf(buf + size, PAGE_SIZE - size,
		     "canvas next_dump_index: %d\n", pool->next_dump_index);
	size +=
	    snprintf(buf + size, PAGE_SIZE - size,
		     "canvas next_alloced_index: %d\n",
		     pool->next_alloced_index);
	size += snprintf(buf + size, PAGE_SIZE - size, "canvas map:\n");
	for (i = 0; i < CANVAS_MAX_NUM / BITS_PER_LONG; i++)
		size +=
		    snprintf(buf + size, PAGE_SIZE - size,
			     "% 3d - % 3d : %0lx\n",
			     i * BITS_PER_LONG,
			     (i + 1) * BITS_PER_LONG - 1,
			     (ulong) pool->canvas_map[i]);
	return size;
}

static ssize_t
canvas_pool_debug_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size += snprintf(buf + size, PAGE_SIZE - size,
		"help: echo n > canvas_pool_debug\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "1:next_dump_index=0\n");
	size +=
	    snprintf(buf + size, PAGE_SIZE - size, "2:next_alloced_index=0\n");
	return size;
}

static ssize_t
canvas_pool_debug_store(struct class *class,
			struct class_attribute *attr, const char *buf,
			size_t size)
{
	unsigned int val;
	ssize_t ret = 0;
	struct canvas_pool *pool = get_canvas_pool();

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	if (val == 1)
		pool->next_dump_index = 0;
	else if (val == 2)
		pool->next_alloced_index = 0;
	return size;
}

static struct class_attribute canvas_class_attrs[] = {
	__ATTR_RO(canvas_pool_map),
	__ATTR_RO(canvas_pool_states),
	__ATTR(canvas_pool_debug, 0664,
		canvas_pool_debug_show, canvas_pool_debug_store),
	__ATTR_NULL
};

int canvas_pool_get_static_canvas_by_name(const char *owner, u8 *tab, int size)
{
	int i, j;
	struct canvas_pool *pool = get_canvas_pool();
	struct canvas_info *info;

	j = 0;
	for (i = 0; i < pool->canvas_max && j < size; i++) {
		info = &pool->info[i];
		if (test_bit(i, pool->canvas_map) && info->fixed_onwer &&
			!strcmp(info->owner, owner)) {
			tab[j] = i;
			j++;
		}
	}
	if (j > 0)
		return j;

	pr_info("not found register static canvas for %s\n", owner);
	return 0;

}

static struct class canvas_class = {
	.name = "amcanvas",
	.class_attrs = canvas_class_attrs,
};

static void canvas_pool_config(void)
{
	canvas_pool_init();

	canvas_pool_register_const_canvas(0, 0x25, "amvdec");
	canvas_pool_register_const_canvas(0x26, 0x39, "vdin");
	canvas_pool_register_const_canvas(0x78, 0xbf, "amvdec");
	canvas_pool_register_const_canvas(0x60, 0x65, "display");
	canvas_pool_register_const_canvas(0x66, 0x6b, "display2");
	canvas_pool_register_const_canvas(0x70, 0x77, "ppmgr");
	canvas_pool_register_const_canvas(0xe4, 0xef, "encoder");
	canvas_pool_register_const_canvas(0x40, 0x48, "osd");
	canvas_pool_register_const_canvas(0xc0, 0xd7, "amlvideo2");
	/*please add static canvas later. */
}

int amcanvas_manager_init(void)
{
	int r;

	canvas_pool_config();
	/* canvas_pool_dump_canvas_info(); */
	r = class_register(&canvas_class);
	return r;
}

void amcanvas_manager_exit(void)
{
	class_unregister(&canvas_class);
}
