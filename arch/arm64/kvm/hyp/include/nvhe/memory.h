/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __KVM_HYP_MEMORY_H
#define __KVM_HYP_MEMORY_H

#include <asm/kvm_mmu.h>
#include <asm/page.h>

#include <linux/types.h>
#include <nvhe/refcount.h>

/*
 * Bits 0-1 are reserved to track the memory ownership state of each page:
 *   00: The page is owned exclusively by the page-table owner.
 *   01: The page is owned by the page-table owner, but is shared
 *       with another entity.
 *   10: The page is shared with, but not owned by the page-table owner.
 *   11: Reserved for future use (lending).
 */
enum pkvm_page_state {
	PKVM_PAGE_OWNED			= 0ULL,
	PKVM_PAGE_SHARED_OWNED		= BIT(0),
	PKVM_PAGE_SHARED_BORROWED	= BIT(1),
	__PKVM_PAGE_RESERVED		= BIT(0) | BIT(1),

	/* Special non-meta state that only applies to host pages. Will not go in PTE SW bits. */
	PKVM_MODULE_OWNED_PAGE		= BIT(2),
	PKVM_NOPAGE			= BIT(3),

	/*
	 * Meta-states which aren't encoded directly in the PTE's SW bits (or
	 * the hyp_vmemmap entry for the host)
	 */
	PKVM_PAGE_RESTRICTED_PROT	= BIT(4),
	PKVM_MMIO			= BIT(5),
};
#define PKVM_PAGE_META_STATES_MASK	(~(BIT(0) | BIT(1)))

#define PKVM_PAGE_STATE_PROT_MASK	(KVM_PGTABLE_PROT_SW0 | KVM_PGTABLE_PROT_SW1)
static inline enum kvm_pgtable_prot pkvm_mkstate(enum kvm_pgtable_prot prot,
						 enum pkvm_page_state state)
{
	BUG_ON(state & PKVM_PAGE_META_STATES_MASK);
	prot &= ~PKVM_PAGE_STATE_PROT_MASK;
	prot |= FIELD_PREP(PKVM_PAGE_STATE_PROT_MASK, state);
	return prot;
}

static inline enum pkvm_page_state pkvm_getstate(enum kvm_pgtable_prot prot)
{
	return FIELD_GET(PKVM_PAGE_STATE_PROT_MASK, prot);
}

struct hyp_page {
	unsigned short refcount;
	u8 order;

	/* Host (non-meta) state. Guarded by the host stage-2 lock. */
	enum pkvm_page_state host_state : 8;
};

extern u64 __hyp_vmemmap;
#define hyp_vmemmap ((struct hyp_page *)__hyp_vmemmap)

#define __hyp_va(phys)	((void *)((phys_addr_t)(phys) - hyp_physvirt_offset))

static inline void *hyp_phys_to_virt(phys_addr_t phys)
{
	return __hyp_va(phys);
}

static inline phys_addr_t hyp_virt_to_phys(void *addr)
{
	return __hyp_pa(addr);
}

#define hyp_phys_to_pfn(phys)	((phys) >> PAGE_SHIFT)
#define hyp_pfn_to_phys(pfn)	((phys_addr_t)((pfn) << PAGE_SHIFT))
#define hyp_phys_to_page(phys)	(&hyp_vmemmap[hyp_phys_to_pfn(phys)])
#define hyp_virt_to_page(virt)	hyp_phys_to_page(__hyp_pa(virt))
#define hyp_virt_to_pfn(virt)	hyp_phys_to_pfn(__hyp_pa(virt))

#define hyp_page_to_pfn(page)	((struct hyp_page *)(page) - hyp_vmemmap)
#define hyp_page_to_phys(page)  hyp_pfn_to_phys((hyp_page_to_pfn(page)))
#define hyp_page_to_virt(page)	__hyp_va(hyp_page_to_phys(page))
#define hyp_page_to_pool(page)	(((struct hyp_page *)page)->pool)

/*
 * Refcounting wrappers for 'struct hyp_page'.
 */
static inline int hyp_page_count(void *addr)
{
	struct hyp_page *p = hyp_virt_to_page(addr);

	return hyp_refcount_get(p->refcount);
}

static inline void hyp_page_ref_inc(struct hyp_page *p)
{
	hyp_refcount_inc(p->refcount);
}

static inline void hyp_page_ref_dec(struct hyp_page *p)
{
	hyp_refcount_dec(p->refcount);
}

static inline int hyp_page_ref_dec_and_test(struct hyp_page *p)
{
	return hyp_refcount_dec(p->refcount) == 0;
}

static inline void hyp_set_page_refcounted(struct hyp_page *p)
{
	hyp_refcount_set(p->refcount, 1);
}
#endif /* __KVM_HYP_MEMORY_H */
