/*
 * drivers/amlogic/media/common/codec_mm/codec_mm_priv.h
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

#ifndef AML_MM_HEADER
#define AML_MM_HEADER
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include<linux/printk.h>

#ifndef INFO_PREFIX
#define INFO_PREFIX "codec_mm"
#endif
/*
 *#define codec_print(KERN_LEVEL, args...) \
 *		printk(KERN_LEVEL INFO_PREFIX ":" args)
 *
 *
 *#define codec_info(args...) codec_print(KERN_INFO, args)
 *#define codec_err(args...)  codec_print(KERN_WARNING, args)
 *#define codec_waring(args...)  codec_print(KERN_ERR, args)
 *
 *
 *#ifdef pr_info
 *#undef pr_info
 *#undef pr_err
 *#undef pr_warn
 *#undef pr_warning
 *
 *#define pr_info(args...) codec_info(args)
 *#define pr_err(args...) codec_err(args)
 *#define pr_warn(args...) codec_waring(args)
 *#define pr_warning pr_warn
 *
 *#endif
 */

struct codec_mm_s {

	/*can be shared by many user */
	const char *owner[8];

	/*virtual buffer of this memory */
	char *vbuffer;

	void *mem_handle;	/*used for top level.alloc/free */
	void *from_ext;		/*alloced from pool */
	ulong phy_addr;		/*if phy continue or one page only */

	int buffer_size;

	int page_count;

	int align2n;

	/*if vbuffer is no cache set*/
	   /*AMPORTS_MEM_FLAGS_NOCACHED  to flags */
#define AMPORTS_MEM_FLAGS_NOCACHED (1<<0)
	/*phy continue,need dma*/
#define AMPORTS_MEM_FLAGS_DMA (1<<1)
	int flags;

#define AMPORTS_MEM_FLAGS_FROM_SYS 1
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES 2
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED 3
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA 4
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP 5
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES 6

	int from_flags;

	/*may can be shared on many user..
	 *decoder/di/ppmgr,
	 */
	atomic_t use_cnt;

	spinlock_t lock;
	char *pagemap;
	int pagemap_size;
	int alloced_page_num;
	int next_bit;
	struct list_head list;

};
extern void dma_clear_buffer(struct page *page, size_t size);
struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
	int align2n, int memflags);
void codec_mm_release(struct codec_mm_s *mem, const char *owner);
u32 get_codec_mm_profiles(void);

u32 codec_mm_get_sc_debug_mode(void);
u32 codec_mm_get_keep_debug_mode(void);

#endif /**/
