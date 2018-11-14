/*
 * include/linux/amlogic/page_trace.h
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

#ifndef __PAGE_TRACE_H__
#define __PAGE_TRACE_H__

#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/sections.h>
#include <linux/page-flags.h>

/*
 * bit map lay out for _ret_ip table
 *
 *  31   28 27 26  24 23             0
 * +------+---+----------------------+
 * |      |   |      |               |
 * +------+---+----------------------+
 *     |    |     |         |
 *     |    |     |         +-------- offset of ip in base address
 *     |    |     +------------------ MIGRATE_TYPE
 *     |    +------------------------ base address select
 *     |                              0: ip base is in kernel address
 *     |                              1: ip base is in module address
 *     +----------------------------- allocate order
 *
 * Note:
 *  offset in ip address is Logical shift right by 2 bits,
 *  because kernel code are always compiled by ARM instruction
 *  set, so pc is aligned by 2. There are 24 bytes used for store
 *  offset in kernel/module, plus these 2 shift bits, You must
 *  make sure your kernel image size is not larger than 2^26 = 64MB
 */
#define IP_ORDER_MASK		(0xf0000000)
#define IP_MODULE_BIT		(1 << 27)
#define IP_MIGRATE_MASK		(0x07000000)
#define IP_RANGE_MASK		(0x00ffffff)
 /* max order usually should not be 15 */
#define IP_INVALID		(0xf)

struct page;

/* this struct should not larger than 32 bit */
struct page_trace {
	unsigned int ret_ip       :24;
	unsigned int migrate_type : 3;
	unsigned int module_flag  : 1;
	unsigned int order        : 4;
};

#ifdef CONFIG_AMLOGIC_PAGE_TRACE
extern unsigned int cma_alloc_trace;
extern unsigned long unpack_ip(struct page_trace *trace);
extern unsigned int pack_ip(unsigned long ip, int order, gfp_t flag);
extern void set_page_trace(struct page *page, int order,
			   gfp_t gfp_flags, void *func);
extern void reset_page_trace(struct page *page, int order);
extern void page_trace_mem_init(void);
extern struct page_trace *find_page_base(struct page *page);
extern unsigned long find_back_trace(void);
extern unsigned long get_page_trace(struct page *page);
extern void show_data(unsigned long addr, int nbytes, const char *name);
#else
static inline unsigned long unpack_ip(struct page_trace *trace)
{
	return 0;
}
static inline void set_page_trace(struct page *page, int order, gfp_t gfp_flags)
{
}
static inline void reset_page_trace(struct page *page, int order)
{
}
static inline void page_trace_mem_init(void)
{
}
static inline struct page_trace *find_page_base(struct page *page)
{
	return NULL;
}
static unsigned long find_back_trace(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_SLUB_DEBUG
#include <linux/slub_def.h>
extern int aml_slub_check_object(struct kmem_cache *s, void *p, void *q);
extern void aml_get_slub_trace(struct kmem_cache *s, struct page *page,
				gfp_t flags, int order);
extern void aml_put_slub_trace(struct page *page, struct kmem_cache *s);
extern int aml_check_kmemcache(struct kmem_cache_cpu *c, struct kmem_cache *s,
			void *object);
extern void aml_slub_set_trace(struct kmem_cache *s, void *object);
#endif /* CONFIG_AMLOGIC_SLUB_DEBUG */

#ifdef CONFIG_KALLSYMS
extern const unsigned long kallsyms_addresses[] __weak;
extern const int kallsyms_offsets[] __weak;
extern const u8 kallsyms_names[] __weak;
extern const unsigned long kallsyms_num_syms
	__attribute__((weak, section(".rodata")));
extern const unsigned long kallsyms_relative_base
	__attribute__((weak, section(".rodata")));
extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;
#endif /* CONFIG_KALLSYMS */

#endif /* __PAGE_TRACE_H__ */
