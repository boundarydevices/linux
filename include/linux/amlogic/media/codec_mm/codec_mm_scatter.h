/*
 * include/linux/amlogic/media/codec_mm/codec_mm_scatter.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef CODEC_MM_SCATTER_HEADER
#define CODEC_MM_SCATTER_HEADER

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>

/*#define PHY_ADDR_NEED_64BITS*/
#ifdef PHY_ADDR_NEED_64BITS
#define phy_addr_type u64
#else /* */
#define phy_addr_type u32
#endif
#define page_sid_type u16

#define PAGE_INDEX(page) (page >> PAGE_SHIFT)

struct codec_mm_scatter {
	void *manager;
	phy_addr_type *pages_list;
	int page_max_cnt;
	int page_cnt;		/*page num */
	int page_tail;		/*last page in list */
	int page_used;
	spinlock_t lock;
	atomic_t user_cnt;
	unsigned long tofree_jiffies;
	struct mutex mutex;
	struct list_head list;	/*hold list. */
};

int codec_mm_scatter_free_all_pages(struct codec_mm_scatter *mms);

int codec_mm_scatter_free_tail_pages(struct codec_mm_scatter *mms,
	int start_free_id);
int codec_mm_scatter_free_tail_pages_fast(struct codec_mm_scatter *mms,
	int start_free_id);
int codec_mm_scatter_free_tail_pages_fast(struct codec_mm_scatter *mms,
	int start_free_id);

struct codec_mm_scatter *codec_mm_scatter_alloc(int max_page,
	int page_num, int istvp);
int codec_mm_scatter_alloc_want_pages(struct codec_mm_scatter *mms,
			int want_pages);
int codec_mm_scatter_size(int is_tvp);
int codec_mm_scatter_mgt_delay_free_swith(int on, int delay_ms,
	int wait_size_M, int istvp);
int codec_mm_dump_scatter(struct codec_mm_scatter *mms, void *buf, int size);
int codec_mm_scatter_dec_owner_user(void *sc_mm, int delay_ms);

#endif
