/*
 * include/linux/balloon_compaction.h
 *
 * Common interface definitions for making balloon pages movable by compaction.
 *
 * Despite being perfectly possible to perform ballooned pages migration, they
 * make a special corner case to compaction scans because balloon pages are not
 * enlisted at any LRU list like the other pages we do compact / migrate.
 *
 * As the page isolation scanning step a compaction thread does is a lockless
 * procedure (from a page standpoint), it might bring some racy situations while
 * performing balloon page compaction. In order to sort out these racy scenarios
 * and safely perform balloon's page compaction and migration we must, always,
 * ensure following these three simple rules:
 *
 *   i. when updating a balloon's page ->mapping element, strictly do it under
 *      the following lock order, independently of the far superior
 *      locking scheme (lru_lock, balloon_lock):
 *	    +-page_lock(page);
 *	      +--spin_lock_irq(&b_dev_info->pages_lock);
 *	            ... page->mapping updates here ...
 *
 *  ii. before isolating or dequeueing a balloon page from the balloon device
 *      pages list, the page reference counter must be raised by one and the
 *      extra refcount must be dropped when the page is enqueued back into
 *      the balloon device page list, thus a balloon page keeps its reference
 *      counter raised only while it is under our special handling;
 *
 * iii. after the lockless scan step have selected a potential balloon page for
 *      isolation, re-test the page->mapping flags and the page ref counter
 *      under the proper page lock, to ensure isolating a valid balloon page
 *      (not yet isolated, nor under release procedure)
 *
 * The functions provided by this interface are placed to help on coping with
 * the aforementioned balloon page corner case, as well as to ensure the simple
 * set of exposed rules are satisfied while we are dealing with balloon pages
 * compaction / migration.
 *
 * Copyright (C) 2012, Red Hat, Inc.  Rafael Aquini <aquini@redhat.com>
 */
#ifndef _LINUX_BALLOON_COMPACTION_H
#define _LINUX_BALLOON_COMPACTION_H
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/migrate.h>
#include <linux/gfp.h>
#include <linux/err.h>

/*
 * Balloon device information descriptor.
 * This struct is used to allow the common balloon compaction interface
 * procedures to find the proper balloon device holding memory pages they'll
 * have to cope for page compaction / migration, as well as it serves the
 * balloon driver as a page book-keeper for its registered balloon devices.
 */
struct balloon_dev_info {
	unsigned long isolated_pages;	/* # of isolated pages for migration */
	spinlock_t pages_lock;		/* Protection to pages list */
	struct list_head pages;		/* Pages enqueued & handled to Host */
	int (* migrate_page)(struct balloon_dev_info *, struct page *newpage,
			struct page *page, enum migrate_mode mode);
};

static inline void balloon_devinfo_init(struct balloon_dev_info *b_dev_info)
{
	b_dev_info->isolated_pages = 0;
	spin_lock_init(&b_dev_info->pages_lock);
	INIT_LIST_HEAD(&b_dev_info->pages);
	b_dev_info->migrate_page = NULL;
}

extern struct page *balloon_page_enqueue(struct balloon_dev_info *b_dev_info);
extern struct page *balloon_page_dequeue(struct balloon_dev_info *b_dev_info);

#ifdef CONFIG_BALLOON_COMPACTION
extern bool balloon_page_isolate(struct page *page);
extern void balloon_page_putback(struct page *page);

/*
 * balloon_page_insert - insert a page into the balloon's page list and make
 *		         the page->mapping assignment accordingly.
 * @page    : page to be assigned as a 'balloon page'
 * @mapping : allocated special 'balloon_mapping'
 * @head    : balloon's device page list head
 *
 * Caller must ensure the page is locked and the spin_lock protecting balloon
 * pages list is held before inserting a page into the balloon device.
 */
static inline void
balloon_page_insert(struct balloon_dev_info *balloon, struct page *page)
{
	__SetPageBalloon(page);
	set_page_private(page, (unsigned long)balloon);
	list_add(&page->lru, &balloon->pages);
}

/*
 * balloon_page_delete - delete a page from balloon's page list and clear
 *			 the page->mapping assignement accordingly.
 * @page    : page to be released from balloon's page list
 *
 * Caller must ensure the page is locked and the spin_lock protecting balloon
 * pages list is held before deleting a page from the balloon device.
 */
static inline void balloon_page_delete(struct page *page, bool isolated)
{
	__ClearPageBalloon(page);
	set_page_private(page, 0);
	if (!isolated)
		list_del(&page->lru);
}

int balloon_page_migrate(new_page_t get_new_page, free_page_t put_new_page,
		unsigned long private, struct page *page,
		int force, enum migrate_mode mode);

/*
 * balloon_page_device - get the b_dev_info descriptor for the balloon device
 *			 that enqueues the given page.
 */
static inline struct balloon_dev_info *balloon_page_device(struct page *page)
{
	return (struct balloon_dev_info *)page_private(page);
}

static inline gfp_t balloon_mapping_gfp_mask(void)
{
	return GFP_HIGHUSER_MOVABLE;
}

#else /* !CONFIG_BALLOON_COMPACTION */

static inline void *balloon_mapping_alloc(void *balloon_device,
				const struct address_space_operations *a_ops)
{
	return ERR_PTR(-EOPNOTSUPP);
}

static inline void balloon_mapping_free(struct address_space *balloon_mapping)
{
	return;
}

static inline void
balloon_page_insert(struct balloon_dev_info *balloon, struct page *page)
{
	__SetPageBalloon(page);
	list_add(&page->lru, &balloon->pages);
}

static inline void balloon_page_delete(struct page *page, bool isolated)
{
	__ClearPageBalloon(page);
	if (!isolated)
		list_del(&page->lru);
}

static inline int balloon_page_migrate(new_page_t get_new_page,
		free_page_t put_new_page, unsigned long private,
		struct page *page, int force, enum migrate_mode mode)
{
	return -EAGAIN;
}

static inline bool balloon_page_isolate(struct page *page)
{
	return false;
}

static inline void balloon_page_putback(struct page *page)
{
	return;
}

static inline gfp_t balloon_mapping_gfp_mask(void)
{
	return GFP_HIGHUSER;
}

#endif /* CONFIG_BALLOON_COMPACTION */
#endif /* _LINUX_BALLOON_COMPACTION_H */
