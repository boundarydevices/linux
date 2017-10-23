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

/* this struct should not larger than 32 bit */
struct page_trace {
	unsigned int ret_ip	  :24;
	unsigned int migrate_type : 3;
	unsigned int module_flag  : 1;
	unsigned int order        : 4;
};

#ifdef CONFIG_AMLOGIC_PAGE_TRACE
extern unsigned long unpack_ip(struct page_trace *trace);
extern void set_page_trace(struct page *page, int order, gfp_t gfp_flags);
extern void reset_page_trace(struct page *page, int order);
extern void page_trace_mem_init(void);
extern struct page_trace *find_page_base(struct page *page);
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
#endif

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
