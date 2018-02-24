/*
 * include/linux/amlogic/aml_cma.h
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

#ifndef __AMLOGIC_CMA_H__
#define __AMLOGIC_CMA_H__

#include <linux/migrate_mode.h>
#include <linux/pagemap.h>

enum migrate_type {
	COMPACT_NORMAL,
	COMPACT_CMA,
	COMPACT_TO_CMA,
};

#define __GFP_NO_CMA    (__GFP_BDEV | __GFP_WRITE)

/* copy from mm/internal.h, must keep same as it */
struct compact_control {
	struct list_head freepages;	/* List of free pages to migrate to */
	struct list_head migratepages;	/* List of pages being migrated */
	unsigned long nr_freepages;	/* Number of isolated free pages */
	unsigned long nr_migratepages;	/* Number of pages to migrate */
	unsigned long free_pfn;		/* isolate_freepages search base */
	unsigned long migrate_pfn;	/* isolate_migratepages search base */
	unsigned long last_migrated_pfn;/* Not yet flushed page being freed */
	enum migrate_mode mode;		/* Async or sync migration mode */
	enum migrate_type page_type;
	bool ignore_skip_hint;		/* Scan blocks even if marked skip */
	bool ignore_block_suitable;	/* Scan blocks considered unsuitable */
	bool direct_compaction;		/* False from kcompactd or /proc/... */
	bool whole_zone;		/* Whole zone should/has been scanned */
	int order;			/* order a direct compactor needs */
	const gfp_t gfp_mask;		/* gfp mask of a direct compactor */
	const unsigned int alloc_flags;	/* alloc flags of a direct compactor */
	const int classzone_idx;	/* zone index of a direct compactor */
	struct zone *zone;
	bool contended;			/* Signal lock or sched contention */
};

static inline bool cma_forbidden_mask(gfp_t gfp_flags)
{
	if ((gfp_flags & __GFP_NO_CMA) || !(gfp_flags & __GFP_MOVABLE))
		return true;
	return false;
}

extern void cma_page_count_update(long size);
extern void aml_cma_alloc_pre_hook(int *a, int b);
extern void aml_cma_alloc_post_hook(int *a, int b, struct page *p);
extern void aml_cma_release_hook(int a, struct page *p);
extern struct page *get_cma_page(struct zone *zone, unsigned int order);
extern unsigned long compact_to_free_cma(struct zone *zone);
extern int cma_alloc_ref(void);
extern bool can_use_cma(gfp_t gfp_flags);
extern void get_cma_alloc_ref(void);
extern void put_cma_alloc_ref(void);
extern bool cma_page(struct page *page);
extern unsigned long get_cma_allocated(void);
extern unsigned long get_total_cmapages(void);
extern spinlock_t cma_iso_lock;
extern int aml_cma_alloc_range(unsigned long start, unsigned long end);

extern void aml_cma_free(unsigned long pfn, unsigned int nr_pages);

extern unsigned long reclaim_clean_pages_from_list(struct zone *zone,
		struct list_head *page_list);

unsigned long
isolate_freepages_range(struct compact_control *cc,
			unsigned long start_pfn, unsigned long end_pfn);
unsigned long
isolate_migratepages_range(struct compact_control *cc,
			   unsigned long low_pfn, unsigned long end_pfn);

struct page *compaction_cma_alloc(struct page *migratepage,
				  unsigned long data,
				  int **result);
#endif /* __AMLOGIC_CMA_H__ */
