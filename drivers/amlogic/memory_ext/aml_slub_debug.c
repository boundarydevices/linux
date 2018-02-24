/*
 * drivers/amlogic/memory_ext/aml_slub_debug.c
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

#include <linux/amlogic/page_trace.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/mmzone.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define ADBG	pr_debug

static void show_obj_trace_around(void *obj)
{
	struct page *page;
	struct zone *zone;
	long pfn, i, start_pfn, end_pfn;

	pfn = virt_to_pfn(obj);
	if (!pfn_valid(pfn)) {
		pr_info("%s, invalid pfn:%lx, obj:%p\n", __func__, pfn, obj);
		return;
	}
	page = pfn_to_page(pfn);
	zone = page_zone(page);
	start_pfn = zone->zone_start_pfn;
	end_pfn =  zone->zone_start_pfn + zone->spanned_pages;
	for (i = pfn - 5; i < pfn + 5; i++) {
		if (i < start_pfn)
			i = start_pfn;
		if (i >= end_pfn)
			i = end_pfn - 1;
		page = pfn_to_page(i);
		pr_info("%s, obj:%p, page:%lx, allocator:%pf\n",
			__func__, obj, i, (void *)get_page_trace(page));
	}
}

static void dump_obj_trace(void *obj, struct kmem_cache *s)
{
	struct page *page;
	unsigned long trace, addr;
	int i, cnt, ip;
	int order;

	page = virt_to_head_page(obj);
	order = s->max.x >> 16;
	cnt = PAGE_SIZE * (1 << order) / s->size;
	pr_info("%s, obj:%p, page:%lx, s:%s %d, order:%d, cnt:%d, s_mem:%p\n",
		__func__, obj, page_to_pfn(page), s->name,
		s->size, order, cnt, page->s_mem);
	if (s->size >= PAGE_SIZE) {
		pr_info("%s, slab:%s, addr:%lx, trace:%pf\n",
			__func__, s->name, (unsigned long)obj, page->s_mem);
	} else {
		addr = (unsigned long)page_address(page);
		if (page->s_mem) {
			for (i = 0; i < cnt; i++) {
				ip = ((int *)page->s_mem)[i];
				trace = unpack_ip((struct page_trace *)&ip);
				pr_info("%s, addr:%lx, i:%d, trace:%pf\n",
					__func__, addr + i * s->size,
					i, (void *)trace);
			}
		}
	}
}

static int get_obj_index(void *obj, struct kmem_cache *s)
{
	struct page *page;
	unsigned long diff;
	int idx;

	page = virt_to_head_page(obj);
	diff = (unsigned long)obj - (unsigned long)page_address(page);
	idx = diff / s->size;

	ADBG("%s, obj:%p, page:%lx, diff:%lx, idx:%d, size:%d, s:%s\n",
		__func__, obj, page_to_pfn(page), diff, idx, s->size, s->name);

	return idx;
}

int aml_slub_check_object(struct kmem_cache *s, void *obj, void *new_obj)
{
	void *p;

	if (new_obj && (unsigned long)new_obj < PAGE_OFFSET) {
		p = obj + s->offset;
		pr_info("%s s:%s, obj:%p, offset:%x, bad new object:%p\n",
			__func__, s->name, obj, s->offset, obj);
		show_obj_trace_around(obj);
		dump_obj_trace(obj, s);
		show_data((unsigned long)p - 256,  512, "obj");
		dump_stack();
		return -EINVAL;
	}
	return 0;
}

int aml_check_kmemcache(struct kmem_cache_cpu *c, struct kmem_cache *s,
			void *object)
{
	if (c && (unsigned long)c < PAGE_OFFSET) {
		show_obj_trace_around(s);
		pr_err("%s, bad cpu cache:%p, c:%s\n", __func__, c, s->name);
		dump_stack();
		return -EINVAL;
	}

	if (object && (unsigned long)object < PAGE_OFFSET) {
		show_obj_trace_around(c);
		pr_err("%s, bad object:%p, c:%p, c:%s\n",
			__func__, object, c, s->name);
		dump_stack();
		return -EINVAL;
	}
	return 0;
}

void aml_get_slub_trace(struct kmem_cache *s, struct page *page,
			gfp_t flags, int order)
{
	int obj_count;
	void *p;

	if (!page)
		return;

	page->s_mem = NULL;
	/* allocate trace data */
	if (page && !(flags & __GFP_BDEV) && s->size < PAGE_SIZE) {
		obj_count = PAGE_SIZE * (1 << order) / s->size;
		ADBG("%s, s:%s, size:%d, osize:%d\n",
			__func__, s->name, s->size, s->object_size);
		ADBG("%s, order:%d, count:%d, flags:%x\n",
			__func__, order, obj_count, flags);
		p = kzalloc(obj_count * 4, GFP_KERNEL | __GFP_BDEV);
		page->s_mem = p;
		ADBG("%s, page:%lx,  s_mem:%p\n",
			__func__, page_to_pfn(page), page->s_mem);
	}
}

void aml_put_slub_trace(struct page *page, struct kmem_cache *s)
{
	if (!page)
		return;

	if (page->s_mem && s->size < PAGE_SIZE) {
		ADBG("%s, %d, free page:%lx, s_mem:%p\n",
			__func__, __LINE__, page_to_pfn(page), page->s_mem);
		kfree(page->s_mem);
	}
	page->s_mem = NULL;
}

void aml_slub_set_trace(struct kmem_cache *s, void *object)
{
	unsigned long trace;
	unsigned int *p, ip, idx;
	struct page *page;

	if (!object)
		return;
	page = virt_to_head_page(object);
	trace = find_back_trace();
	if (s->size >= PAGE_SIZE) {
		page->s_mem = (void *)trace;
		ADBG("%s, page:%lx, trace:%pf, s:%s, size:%d\n",
			__func__, page_to_pfn(page),
			page->s_mem, s->name, s->size);
	} else {
		ip = pack_ip(trace, 0, 0);
		idx = get_obj_index(object, s);
		if (page->s_mem) {
			p = page->s_mem;
			p[idx] = ip;
			ADBG("%s, page:%lx, trace:%pf\n",
				__func__, page_to_pfn(page), (void *)trace);
			ADBG("s:%s, size:%d, s_mem:%p, idx:%d, ip:%x\n",
				s->name, s->size, page->s_mem, idx, ip);
		}
	}
}
