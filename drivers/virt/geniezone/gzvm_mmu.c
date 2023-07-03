// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/soc/mediatek/gzvm_drv.h>

static int cmp_ppages(struct rb_node *node, const struct rb_node *parent)
{
	struct gzvm_pinned_page *a = container_of(node,
						  struct gzvm_pinned_page,
						  node);
	struct gzvm_pinned_page *b = container_of(parent,
						  struct gzvm_pinned_page,
						  node);

	if (a->ipa < b->ipa)
		return -1;
	if (a->ipa > b->ipa)
		return 1;
	return 0;
}

/* Invoker of this function is responsible for locking */
static int gzvm_insert_ppage(struct gzvm *vm, struct gzvm_pinned_page *ppage)
{
	if (rb_find_add(&ppage->node, &vm->pinned_pages, cmp_ppages))
		return -EEXIST;
	return 0;
}

static int pin_one_page(struct gzvm *vm, unsigned long hva, u64 gpa,
			struct page **out_page)
{
	unsigned int flags = FOLL_HWPOISON | FOLL_LONGTERM | FOLL_WRITE;
	struct gzvm_pinned_page *ppage = NULL;
	struct mm_struct *mm = current->mm;
	struct page *page = NULL;
	int ret;

	ppage = kmalloc(sizeof(*ppage), GFP_KERNEL_ACCOUNT);
	if (!ppage)
		return -ENOMEM;

	mmap_read_lock(mm);
	ret = pin_user_pages(hva, 1, flags, &page);
	mmap_read_unlock(mm);

	if (ret != 1 || !page) {
		kfree(ppage);
		return -EFAULT;
	}

	ppage->page = page;
	ppage->ipa = gpa;

	mutex_lock(&vm->mem_lock);
	ret = gzvm_insert_ppage(vm, ppage);

	/**
	 * The return of -EEXIST from gzvm_insert_ppage is considered an
	 * expected behavior in this context.
	 * This situation arises when two or more VCPUs are concurrently
	 * engaged in demand paging handling. The initial VCPU has already
	 * allocated and pinned a page, while the subsequent VCPU attempts
	 * to pin the same page again. As a result, we prompt the unpinning
	 * and release of the allocated structure, followed by a return 0.
	 */
	if (ret == -EEXIST) {
		kfree(ppage);
		unpin_user_pages(&page, 1);
		ret = 0;
	}
	mutex_unlock(&vm->mem_lock);
	*out_page = page;

	return ret;
}

int gzvm_vm_allocate_guest_page(struct gzvm *vm, struct gzvm_memslot *slot,
				u64 gfn, u64 *pfn)
{
	struct page *page = NULL;
	unsigned long hva;
	int ret;

	if (gzvm_gfn_to_hva_memslot(slot, gfn, (u64 *)&hva) != 0)
		return -EINVAL;

	ret = pin_one_page(vm, hva, PFN_PHYS(gfn), &page);
	if (ret != 0)
		return ret;

	if (page == NULL)
		return -EFAULT;
	/**
	 * As `pin_user_pages` already gets the page struct, we don't need to
	 * call other APIs to reduce function call overhead.
	 */
	*pfn = page_to_pfn(page);

	return 0;
}
