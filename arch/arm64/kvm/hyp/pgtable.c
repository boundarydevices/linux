// SPDX-License-Identifier: GPL-2.0-only
/*
 * Stand-alone page-table allocator for hyp stage-1 and guest stage-2.
 * No bombay mix was harmed in the writing of this file.
 *
 * Copyright (C) 2020 Google LLC
 * Author: Will Deacon <will@kernel.org>
 */

#include <linux/bitfield.h>
#include <asm/kvm_pgtable.h>
#include <asm/stage2_pgtable.h>


#define KVM_PTE_LEAF_ATTR_S2_PERMS	(KVM_PTE_LEAF_ATTR_LO_S2_S2AP_R | \
					 KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W | \
					 KVM_PTE_LEAF_ATTR_HI_S2_XN)

#define KVM_INVALID_PTE_OWNER_MASK	GENMASK(9, 2)
#define KVM_MAX_OWNER_ID		FIELD_MAX(KVM_INVALID_PTE_OWNER_MASK)

struct kvm_pgtable_walk_data {
	struct kvm_pgtable_walker	*walker;

	const u64			start;
	u64				addr;
	const u64			end;
};

static bool kvm_pgtable_walk_skip_bbm_tlbi(const struct kvm_pgtable_visit_ctx *ctx)
{
	return unlikely(ctx->flags & KVM_PGTABLE_WALK_SKIP_BBM_TLBI);
}

static bool kvm_pgtable_walk_skip_cmo(const struct kvm_pgtable_visit_ctx *ctx)
{
	return unlikely(ctx->flags & KVM_PGTABLE_WALK_SKIP_CMO);
}

static bool kvm_phys_is_valid(u64 phys)
{
	return phys < BIT(id_aa64mmfr0_parange_to_phys_shift(ID_AA64MMFR0_EL1_PARANGE_MAX));
}

static bool kvm_block_mapping_supported(const struct kvm_pgtable_visit_ctx *ctx, u64 phys)
{
	u64 granule = kvm_granule_size(ctx->level);

	if (!kvm_level_supports_block_mapping(ctx->level))
		return false;

	if (granule > (ctx->end - ctx->addr))
		return false;

	if (kvm_phys_is_valid(phys) && !IS_ALIGNED(phys, granule))
		return false;

	return IS_ALIGNED(ctx->addr, granule);
}

static u32 kvm_pgtable_idx(struct kvm_pgtable_walk_data *data, u32 level)
{
	u64 shift = kvm_granule_shift(level);
	u64 mask = BIT(PAGE_SHIFT - 3) - 1;

	return (data->addr >> shift) & mask;
}

static u32 kvm_pgd_page_idx(struct kvm_pgtable *pgt, u64 addr)
{
	u64 shift = kvm_granule_shift(pgt->start_level - 1); /* May underflow */
	u64 mask = BIT(pgt->ia_bits) - 1;

	return (addr & mask) >> shift;
}

static u32 kvm_pgd_pages(u32 ia_bits, u32 start_level)
{
	struct kvm_pgtable pgt = {
		.ia_bits	= ia_bits,
		.start_level	= start_level,
	};

	return kvm_pgd_page_idx(&pgt, -1ULL) + 1;
}

static void kvm_clear_pte(kvm_pte_t *ptep)
{
	WRITE_ONCE(*ptep, 0);
}

static kvm_pte_t kvm_init_table_pte(kvm_pte_t *childp, struct kvm_pgtable_mm_ops *mm_ops)
{
	kvm_pte_t pte = kvm_phys_to_pte(mm_ops->virt_to_phys(childp));

	pte |= FIELD_PREP(KVM_PTE_TYPE, KVM_PTE_TYPE_TABLE);
	pte |= KVM_PTE_VALID;
	return pte;
}

static kvm_pte_t kvm_init_valid_leaf_pte(u64 pa, kvm_pte_t attr, u32 level)
{
	kvm_pte_t pte = kvm_phys_to_pte(pa);
	u64 type = (level == KVM_PGTABLE_MAX_LEVELS - 1) ? KVM_PTE_TYPE_PAGE :
							   KVM_PTE_TYPE_BLOCK;

	pte |= attr & (KVM_PTE_LEAF_ATTR_LO | KVM_PTE_LEAF_ATTR_HI);
	pte |= FIELD_PREP(KVM_PTE_TYPE, type);
	pte |= KVM_PTE_VALID;

	return pte;
}

static int kvm_pgtable_visitor_cb(struct kvm_pgtable_walk_data *data,
				  const struct kvm_pgtable_visit_ctx *ctx,
				  enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_walker *walker = data->walker;

	/* Ensure the appropriate lock is held (e.g. RCU lock for stage-2 MMU) */
	WARN_ON_ONCE(kvm_pgtable_walk_shared(ctx) && !kvm_pgtable_walk_lock_held());
	return walker->cb(ctx, visit);
}

static bool kvm_pgtable_walk_continue(const struct kvm_pgtable_walker *walker,
				      int r)
{
	/*
	 * Visitor callbacks return EAGAIN when the conditions that led to a
	 * fault are no longer reflected in the page tables due to a race to
	 * update a PTE. In the context of a fault handler this is interpreted
	 * as a signal to retry guest execution.
	 *
	 * Ignore the return code altogether for walkers outside a fault handler
	 * (e.g. write protecting a range of memory) and chug along with the
	 * page table walk.
	 */
	if (r == -EAGAIN)
		return !(walker->flags & KVM_PGTABLE_WALK_HANDLE_FAULT);

	return !r;
}

static int __kvm_pgtable_walk(struct kvm_pgtable_walk_data *data,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      struct kvm_pgtable_pte_ops *pte_ops,
			      kvm_pteref_t pgtable, u32 level);

static inline int __kvm_pgtable_visit(struct kvm_pgtable_walk_data *data,
				      struct kvm_pgtable_mm_ops *mm_ops,
				      struct kvm_pgtable_pte_ops *pte_ops,
				      kvm_pteref_t pteref, u32 level)
{
	enum kvm_pgtable_walk_flags flags = data->walker->flags;
	kvm_pte_t *ptep = kvm_dereference_pteref(data->walker, pteref);
	struct kvm_pgtable_visit_ctx ctx = {
		.ptep	= ptep,
		.old	= READ_ONCE(*ptep),
		.arg	= data->walker->arg,
		.mm_ops	= mm_ops,
		.start	= data->start,
		.pte_ops = pte_ops,
		.addr	= data->addr,
		.end	= data->end,
		.level	= level,
		.flags	= flags,
	};
	int ret = 0;
	bool reload = false;
	kvm_pteref_t childp;
	bool table = kvm_pte_table(ctx.old, level);

	if (table && (ctx.flags & KVM_PGTABLE_WALK_TABLE_PRE)) {
		ret = kvm_pgtable_visitor_cb(data, &ctx, KVM_PGTABLE_WALK_TABLE_PRE);
		reload = true;
	}

	if (!table && (ctx.flags & KVM_PGTABLE_WALK_LEAF)) {
		ret = kvm_pgtable_visitor_cb(data, &ctx, KVM_PGTABLE_WALK_LEAF);
		reload = true;
	}

	/*
	 * Reload the page table after invoking the walker callback for leaf
	 * entries or after pre-order traversal, to allow the walker to descend
	 * into a newly installed or replaced table.
	 */
	if (reload) {
		ctx.old = READ_ONCE(*ptep);
		table = kvm_pte_table(ctx.old, level);
	}

	if (!kvm_pgtable_walk_continue(data->walker, ret))
		goto out;

	if (!table) {
		data->addr = ALIGN_DOWN(data->addr, kvm_granule_size(level));
		data->addr += kvm_granule_size(level);
		goto out;
	}

	childp = (kvm_pteref_t)kvm_pte_follow(ctx.old, mm_ops);
	ret = __kvm_pgtable_walk(data, mm_ops, pte_ops, childp, level + 1);
	if (!kvm_pgtable_walk_continue(data->walker, ret))
		goto out;

	if (ctx.flags & KVM_PGTABLE_WALK_TABLE_POST)
		ret = kvm_pgtable_visitor_cb(data, &ctx, KVM_PGTABLE_WALK_TABLE_POST);

out:
	if (kvm_pgtable_walk_continue(data->walker, ret))
		return 0;

	return ret;
}

static int __kvm_pgtable_walk(struct kvm_pgtable_walk_data *data,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      struct kvm_pgtable_pte_ops *pte_ops,
			      kvm_pteref_t pgtable, u32 level)
{
	u32 idx;
	int ret = 0;

	if (WARN_ON_ONCE(level >= KVM_PGTABLE_MAX_LEVELS))
		return -EINVAL;

	for (idx = kvm_pgtable_idx(data, level); idx < PTRS_PER_PTE; ++idx) {
		kvm_pteref_t pteref = &pgtable[idx];

		if (data->addr >= data->end)
			break;

		ret = __kvm_pgtable_visit(data, mm_ops, pte_ops, pteref, level);
		if (ret)
			break;
	}

	return ret;
}

static int _kvm_pgtable_walk(struct kvm_pgtable *pgt, struct kvm_pgtable_walk_data *data)
{
	u32 idx;
	int ret = 0;
	u64 limit = BIT(pgt->ia_bits);

	if (data->addr > limit || data->end > limit)
		return -ERANGE;

	if (!pgt->pgd)
		return -EINVAL;

	for (idx = kvm_pgd_page_idx(pgt, data->addr); data->addr < data->end; ++idx) {
		kvm_pteref_t pteref = &pgt->pgd[idx * PTRS_PER_PTE];

		ret = __kvm_pgtable_walk(data, pgt->mm_ops, pgt->pte_ops,
					 pteref, pgt->start_level);
		if (ret)
			break;
	}

	return ret;
}

int kvm_pgtable_walk(struct kvm_pgtable *pgt, u64 addr, u64 size,
		     struct kvm_pgtable_walker *walker)
{
	struct kvm_pgtable_walk_data walk_data = {
		.start	= ALIGN_DOWN(addr, PAGE_SIZE),
		.addr	= ALIGN_DOWN(addr, PAGE_SIZE),
		.end	= PAGE_ALIGN(walk_data.addr + size),
		.walker	= walker,
	};
	int r;

	r = kvm_pgtable_walk_begin(walker);
	if (r)
		return r;

	r = _kvm_pgtable_walk(pgt, &walk_data);
	kvm_pgtable_walk_end(walker);

	return r;
}

struct leaf_walk_data {
	kvm_pte_t	pte;
	u32		level;
};

static int leaf_walker(const struct kvm_pgtable_visit_ctx *ctx,
		       enum kvm_pgtable_walk_flags visit)
{
	struct leaf_walk_data *data = ctx->arg;

	data->pte   = ctx->old;
	data->level = ctx->level;

	return 0;
}

int kvm_pgtable_get_leaf(struct kvm_pgtable *pgt, u64 addr,
			 kvm_pte_t *ptep, u32 *level)
{
	struct leaf_walk_data data;
	struct kvm_pgtable_walker walker = {
		.cb	= leaf_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &data,
	};
	int ret;

	ret = kvm_pgtable_walk(pgt, ALIGN_DOWN(addr, PAGE_SIZE),
			       PAGE_SIZE, &walker);
	if (!ret) {
		if (ptep)
			*ptep  = data.pte;
		if (level)
			*level = data.level;
	}

	return ret;
}

struct hyp_map_data {
	const u64			phys;
	kvm_pte_t			attr;
};

static int hyp_set_prot_attr(enum kvm_pgtable_prot prot, kvm_pte_t *ptep)
{
	u32 ap = (prot & KVM_PGTABLE_PROT_W) ? KVM_PTE_LEAF_ATTR_LO_S1_AP_RW :
					       KVM_PTE_LEAF_ATTR_LO_S1_AP_RO;
	bool device = prot & KVM_PGTABLE_PROT_DEVICE;
	u32 sh = KVM_PTE_LEAF_ATTR_LO_S1_SH_IS;
	bool nc = prot & KVM_PGTABLE_PROT_NC;
	kvm_pte_t attr;
	u32 mtype;

	if (!(prot & KVM_PGTABLE_PROT_R) || (device && nc) ||
			(prot & (KVM_PGTABLE_PROT_PXN | KVM_PGTABLE_PROT_UXN)))
		return -EINVAL;

	if (device)
		mtype = MT_DEVICE_nGnRnE;
	else if (nc)
		mtype = MT_NORMAL_NC;
	else
		mtype = MT_NORMAL;

	attr = FIELD_PREP(KVM_PTE_LEAF_ATTR_LO_S1_ATTRIDX, mtype);

	if (prot & KVM_PGTABLE_PROT_X) {
		if (prot & KVM_PGTABLE_PROT_W)
			return -EINVAL;

		if (device)
			return -EINVAL;

		if (IS_ENABLED(CONFIG_ARM64_BTI_KERNEL) && system_supports_bti())
			attr |= KVM_PTE_LEAF_ATTR_HI_S1_GP;
	} else {
		attr |= KVM_PTE_LEAF_ATTR_HI_S1_XN;
	}

	attr |= FIELD_PREP(KVM_PTE_LEAF_ATTR_LO_S1_AP, ap);
	attr |= FIELD_PREP(KVM_PTE_LEAF_ATTR_LO_S1_SH, sh);
	attr |= KVM_PTE_LEAF_ATTR_LO_S1_AF;
	attr |= prot & KVM_PTE_LEAF_ATTR_HI_SW;
	*ptep = attr;

	return 0;
}

enum kvm_pgtable_prot kvm_pgtable_hyp_pte_prot(kvm_pte_t pte)
{
	enum kvm_pgtable_prot prot = pte & KVM_PTE_LEAF_ATTR_HI_SW;
	u32 ap;

	if (!kvm_pte_valid(pte))
		return prot;

	if (!(pte & KVM_PTE_LEAF_ATTR_HI_S1_XN))
		prot |= KVM_PGTABLE_PROT_X;

	ap = FIELD_GET(KVM_PTE_LEAF_ATTR_LO_S1_AP, pte);
	if (ap == KVM_PTE_LEAF_ATTR_LO_S1_AP_RO)
		prot |= KVM_PGTABLE_PROT_R;
	else if (ap == KVM_PTE_LEAF_ATTR_LO_S1_AP_RW)
		prot |= KVM_PGTABLE_PROT_RW;

	return prot;
}

static bool hyp_map_walker_try_leaf(const struct kvm_pgtable_visit_ctx *ctx,
				    struct hyp_map_data *data)
{
	u64 phys = data->phys + (ctx->addr - ctx->start);
	kvm_pte_t new;

	if (!kvm_block_mapping_supported(ctx, phys))
		return false;

	new = kvm_init_valid_leaf_pte(phys, data->attr, ctx->level);
	if (ctx->old == new)
		return true;
	if (!kvm_pte_valid(ctx->old))
		ctx->mm_ops->get_page(ctx->ptep);
	else if (WARN_ON((ctx->old ^ new) & ~KVM_PTE_LEAF_ATTR_HI_SW))
		return false;

	smp_store_release(ctx->ptep, new);
	return true;
}

static int hyp_map_walker(const struct kvm_pgtable_visit_ctx *ctx,
			  enum kvm_pgtable_walk_flags visit)
{
	kvm_pte_t *childp, new;
	struct hyp_map_data *data = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;

	if (hyp_map_walker_try_leaf(ctx, data))
		return 0;

	if (WARN_ON(ctx->level == KVM_PGTABLE_MAX_LEVELS - 1))
		return -EINVAL;

	childp = (kvm_pte_t *)mm_ops->zalloc_page(NULL);
	if (!childp)
		return -ENOMEM;

	new = kvm_init_table_pte(childp, mm_ops);
	mm_ops->get_page(ctx->ptep);
	smp_store_release(ctx->ptep, new);

	return 0;
}

int kvm_pgtable_hyp_map(struct kvm_pgtable *pgt, u64 addr, u64 size, u64 phys,
			enum kvm_pgtable_prot prot)
{
	int ret;
	struct hyp_map_data map_data = {
		.phys	= ALIGN_DOWN(phys, PAGE_SIZE),
	};
	struct kvm_pgtable_walker walker = {
		.cb	= hyp_map_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &map_data,
	};

	ret = hyp_set_prot_attr(prot, &map_data.attr);
	if (ret)
		return ret;

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	dsb(ishst);
	isb();
	return ret;
}

static int hyp_unmap_walker(const struct kvm_pgtable_visit_ctx *ctx,
			    enum kvm_pgtable_walk_flags visit)
{
	kvm_pte_t *childp = NULL;
	u64 granule = kvm_granule_size(ctx->level);
	u64 *unmapped = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;

	if (!kvm_pte_valid(ctx->old))
		return -EINVAL;

	if (kvm_pte_table(ctx->old, ctx->level)) {
		childp = kvm_pte_follow(ctx->old, mm_ops);

		if (mm_ops->page_count(childp) != 1)
			return 0;

		kvm_clear_pte(ctx->ptep);
		dsb(ishst);
		__tlbi_level(vae2is, __TLBI_VADDR(ctx->addr, 0), 0);
	} else {
		if (ctx->end - ctx->addr < granule)
			return -EINVAL;

		kvm_clear_pte(ctx->ptep);
		dsb(ishst);
		__tlbi_level(vale2is, __TLBI_VADDR(ctx->addr, 0), ctx->level);
		*unmapped += granule;
	}

	dsb(ish);
	isb();
	mm_ops->put_page(ctx->ptep);

	if (childp)
		mm_ops->put_page(childp);

	return 0;
}

u64 kvm_pgtable_hyp_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	u64 unmapped = 0;
	struct kvm_pgtable_walker walker = {
		.cb	= hyp_unmap_walker,
		.arg	= &unmapped,
		.flags	= KVM_PGTABLE_WALK_LEAF | KVM_PGTABLE_WALK_TABLE_POST,
	};

	if (!pgt->mm_ops->page_count)
		return 0;

	kvm_pgtable_walk(pgt, addr, size, &walker);
	return unmapped;
}

int kvm_pgtable_hyp_init(struct kvm_pgtable *pgt, u32 va_bits,
			 struct kvm_pgtable_mm_ops *mm_ops)
{
	u64 levels = ARM64_HW_PGTABLE_LEVELS(va_bits);

	pgt->pgd = (kvm_pteref_t)mm_ops->zalloc_page(NULL);
	if (!pgt->pgd)
		return -ENOMEM;

	pgt->ia_bits		= va_bits;
	pgt->start_level	= KVM_PGTABLE_MAX_LEVELS - levels;
	pgt->mm_ops		= mm_ops;
	pgt->mmu		= NULL;
	pgt->pte_ops		= NULL;

	return 0;
}

static int hyp_free_walker(const struct kvm_pgtable_visit_ctx *ctx,
			   enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;

	if (!kvm_pte_valid(ctx->old))
		return 0;

	mm_ops->put_page(ctx->ptep);

	if (kvm_pte_table(ctx->old, ctx->level))
		mm_ops->put_page(kvm_pte_follow(ctx->old, mm_ops));

	return 0;
}

void kvm_pgtable_hyp_destroy(struct kvm_pgtable *pgt)
{
	struct kvm_pgtable_walker walker = {
		.cb	= hyp_free_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF | KVM_PGTABLE_WALK_TABLE_POST,
	};

	WARN_ON(kvm_pgtable_walk(pgt, 0, BIT(pgt->ia_bits), &walker));
	pgt->mm_ops->put_page(kvm_dereference_pteref(&walker, pgt->pgd));
	pgt->pgd = NULL;
}

struct stage2_map_data {
	const u64			phys;
	kvm_pte_t			attr;
	u64				annotation;

	kvm_pte_t			*anchor;
	kvm_pte_t			*childp;

	struct kvm_s2_mmu		*mmu;
	void				*memcache;

	/* Force mappings to page granularity */
	bool				force_pte;
};

u64 kvm_get_vtcr(u64 mmfr0, u64 mmfr1, u32 phys_shift)
{
	u64 vtcr = VTCR_EL2_FLAGS;
	u8 lvls;

	vtcr |= kvm_get_parange(mmfr0) << VTCR_EL2_PS_SHIFT;
	vtcr |= VTCR_EL2_T0SZ(phys_shift);
	/*
	 * Use a minimum 2 level page table to prevent splitting
	 * host PMD huge pages at stage2.
	 */
	lvls = stage2_pgtable_levels(phys_shift);
	if (lvls < 2)
		lvls = 2;
	vtcr |= VTCR_EL2_LVLS_TO_SL0(lvls);

#ifdef CONFIG_ARM64_HW_AFDBM
	/*
	 * Enable the Hardware Access Flag management, unconditionally
	 * on all CPUs. In systems that have asymmetric support for the feature
	 * this allows KVM to leverage hardware support on the subset of cores
	 * that implement the feature.
	 *
	 * The architecture requires VTCR_EL2.HA to be RES0 (thus ignored by
	 * hardware) on implementations that do not advertise support for the
	 * feature. As such, setting HA unconditionally is safe, unless you
	 * happen to be running on a design that has unadvertised support for
	 * HAFDBS. Here be dragons.
	 */
	if (!cpus_have_final_cap(ARM64_WORKAROUND_AMPERE_AC03_CPU_38))
		vtcr |= VTCR_EL2_HA;
#endif /* CONFIG_ARM64_HW_AFDBM */

	/* Set the vmid bits */
	vtcr |= (get_vmid_bits(mmfr1) == 16) ?
		VTCR_EL2_VS_16BIT :
		VTCR_EL2_VS_8BIT;

	return vtcr;
}

static bool stage2_has_fwb(struct kvm_pgtable *pgt)
{
	if (!cpus_have_const_cap(ARM64_HAS_STAGE2_FWB))
		return false;

	return !(pgt->flags & KVM_PGTABLE_S2_NOFWB);
}

void kvm_tlb_flush_vmid_range(struct kvm_s2_mmu *mmu,
				phys_addr_t addr, size_t size)
{
	unsigned long pages, inval_pages;

	if (!system_supports_tlb_range()) {
		kvm_call_hyp(__kvm_tlb_flush_vmid, mmu);
		return;
	}

	pages = size >> PAGE_SHIFT;
	while (pages > 0) {
		inval_pages = min(pages, MAX_TLBI_RANGE_PAGES);
		kvm_call_hyp(__kvm_tlb_flush_vmid_range, mmu, addr, inval_pages);

		addr += inval_pages << PAGE_SHIFT;
		pages -= inval_pages;
	}
}

#define KVM_S2_MEMATTR(pgt, attr) PAGE_S2_MEMATTR(attr, stage2_has_fwb(pgt))

static int stage2_set_prot_attr(struct kvm_pgtable *pgt, enum kvm_pgtable_prot prot,
		kvm_pte_t *ptep)
{
	u64 exec_type = KVM_PTE_LEAF_ATTR_HI_S2_XN_XN;
	bool device = prot & KVM_PGTABLE_PROT_DEVICE;
	u32 sh = KVM_PTE_LEAF_ATTR_LO_S2_SH_IS;
	bool nc = prot & KVM_PGTABLE_PROT_NC;
	enum kvm_pgtable_prot exec_prot;
	kvm_pte_t attr;

	if (device)
		attr = KVM_S2_MEMATTR(pgt, DEVICE_nGnRE);
	else if (nc)
		attr = KVM_S2_MEMATTR(pgt, NORMAL_NC);
	else
		attr = KVM_S2_MEMATTR(pgt, NORMAL);

	exec_prot = prot & (KVM_PGTABLE_PROT_X | KVM_PGTABLE_PROT_PXN | KVM_PGTABLE_PROT_UXN);
	switch(exec_prot) {
	case KVM_PGTABLE_PROT_X:
		goto set_ap;
	case KVM_PGTABLE_PROT_PXN:
		exec_type = KVM_PTE_LEAF_ATTR_HI_S2_XN_PXN;
		break;
	case KVM_PGTABLE_PROT_UXN:
		exec_type = KVM_PTE_LEAF_ATTR_HI_S2_XN_UXN;
		break;
	default:
		if (exec_prot)
			return -EINVAL;
	}
	attr |= FIELD_PREP(KVM_PTE_LEAF_ATTR_HI_S2_XN, exec_type);

set_ap:
	if (prot & KVM_PGTABLE_PROT_R)
		attr |= KVM_PTE_LEAF_ATTR_LO_S2_S2AP_R;

	if (prot & KVM_PGTABLE_PROT_W)
		attr |= KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W;

	attr |= FIELD_PREP(KVM_PTE_LEAF_ATTR_LO_S2_SH, sh);
	attr |= KVM_PTE_LEAF_ATTR_LO_S2_AF;
	attr |= prot & KVM_PTE_LEAF_ATTR_HI_SW;
	*ptep = attr;

	return 0;
}

enum kvm_pgtable_prot kvm_pgtable_stage2_pte_prot(kvm_pte_t pte)
{
	enum kvm_pgtable_prot prot = pte & KVM_PTE_LEAF_ATTR_HI_SW;

	if (!kvm_pte_valid(pte))
		return prot;

	if (pte & KVM_PTE_LEAF_ATTR_LO_S2_S2AP_R)
		prot |= KVM_PGTABLE_PROT_R;
	if (pte & KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W)
		prot |= KVM_PGTABLE_PROT_W;
	switch(FIELD_GET(KVM_PTE_LEAF_ATTR_HI_S2_XN, pte)) {
	case 0:
		prot |= KVM_PGTABLE_PROT_X;
		break;
	case KVM_PTE_LEAF_ATTR_HI_S2_XN_PXN:
		prot |= KVM_PGTABLE_PROT_PXN;
		break;
	case KVM_PTE_LEAF_ATTR_HI_S2_XN_UXN:
		prot |= KVM_PGTABLE_PROT_UXN;
		break;
	case KVM_PTE_LEAF_ATTR_HI_S2_XN_XN:
		break;
	default:
		WARN_ON(1);
	}

	return prot;
}

static bool stage2_pte_needs_update(struct kvm_pgtable *pgt,
				    kvm_pte_t old, kvm_pte_t new)
{
	/* Following filter logic applies only to guest stage-2 entries. */
	if (pgt->flags & KVM_PGTABLE_S2_IDMAP)
		return true;

	if (!kvm_pte_valid(old) || !kvm_pte_valid(new))
		return true;

	return ((old ^ new) & (~KVM_PTE_LEAF_ATTR_S2_PERMS));
}

static bool stage2_pte_is_locked(kvm_pte_t pte)
{
	return !kvm_pte_valid(pte) && (pte & KVM_INVALID_PTE_LOCKED);
}

static bool stage2_try_set_pte(const struct kvm_pgtable_visit_ctx *ctx, kvm_pte_t new)
{
	if (!kvm_pgtable_walk_shared(ctx)) {
		WRITE_ONCE(*ctx->ptep, new);
		return true;
	}

	return cmpxchg(ctx->ptep, ctx->old, new) == ctx->old;
}

/**
 * stage2_try_break_pte() - Invalidates a pte according to the
 *			    'break-before-make' requirements of the
 *			    architecture.
 *
 * @ctx: context of the visited pte.
 * @mmu: stage-2 mmu
 *
 * Returns: true if the pte was successfully broken.
 *
 * If the removed pte was valid, performs the necessary serialization and TLB
 * invalidation for the old value. For counted ptes, drops the reference count
 * on the containing table page.
 */
static bool stage2_try_break_pte(const struct kvm_pgtable_visit_ctx *ctx,
				 struct kvm_s2_mmu *mmu)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_pgtable_pte_ops *pte_ops = ctx->pte_ops;

	if (stage2_pte_is_locked(ctx->old)) {
		/*
		 * Should never occur if this walker has exclusive access to the
		 * page tables.
		 */
		WARN_ON(!kvm_pgtable_walk_shared(ctx));
		return false;
	}

	if (!stage2_try_set_pte(ctx, KVM_INVALID_PTE_LOCKED))
		return false;

	if (!kvm_pgtable_walk_skip_bbm_tlbi(ctx)) {
		/*
		 * Perform the appropriate TLB invalidation based on the
		 * evicted pte value (if any).
		 */
		if (kvm_pte_table(ctx->old, ctx->level)) {
			u64 size = kvm_granule_size(ctx->level);
			u64 addr = ALIGN_DOWN(ctx->addr, size);

			kvm_tlb_flush_vmid_range(mmu, addr, size);
		} else if (kvm_pte_valid(ctx->old)) {
			kvm_call_hyp(__kvm_tlb_flush_vmid_ipa, mmu,
				     ctx->addr, ctx->level);
		}
	}

	if (pte_ops->pte_is_counted_cb(ctx->old, ctx->level))
		mm_ops->put_page(ctx->ptep);

	return true;
}

static void stage2_make_pte(const struct kvm_pgtable_visit_ctx *ctx, kvm_pte_t new)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_pgtable_pte_ops *pte_ops = ctx->pte_ops;

	WARN_ON(!stage2_pte_is_locked(*ctx->ptep));

	if (pte_ops->pte_is_counted_cb(new, ctx->level))
		mm_ops->get_page(ctx->ptep);

	smp_store_release(ctx->ptep, new);
}

static bool stage2_unmap_defer_tlb_flush(struct kvm_pgtable *pgt)
{
	/*
	 * If FEAT_TLBIRANGE is implemented, defer the individual
	 * TLB invalidations until the entire walk is finished, and
	 * then use the range-based TLBI instructions to do the
	 * invalidations. Condition deferred TLB invalidation on the
	 * system supporting FWB as the optimization is entirely
	 * pointless when the unmap walker needs to perform CMOs.
	 */
	return system_supports_tlb_range() && stage2_has_fwb(pgt);
}

static void stage2_unmap_clear_pte(const struct kvm_pgtable_visit_ctx *ctx,
				   struct kvm_s2_mmu *mmu)
{
	struct kvm_pgtable *pgt = ctx->arg;
	if (kvm_pte_valid(ctx->old)) {
		kvm_clear_pte(ctx->ptep);

		if (kvm_pte_table(ctx->old, ctx->level)) {
			kvm_call_hyp(__kvm_tlb_flush_vmid_ipa, mmu, ctx->addr,
				     0);
		} else if (!stage2_unmap_defer_tlb_flush(pgt)) {
			kvm_call_hyp(__kvm_tlb_flush_vmid_ipa, mmu, ctx->addr,
				     ctx->level);
		}
	}
}

static void stage2_unmap_put_pte(const struct kvm_pgtable_visit_ctx *ctx,
				 struct kvm_s2_mmu *mmu,
				 struct kvm_pgtable_mm_ops *mm_ops)
{
	/*
	 * Clear the existing PTE, and perform break-before-make if it was
	 * valid. Depending on the system support, defer the TLB maintenance
	 * for the same until the entire unmap walk is completed.
	 */
	stage2_unmap_clear_pte(ctx, mmu);
	mm_ops->put_page(ctx->ptep);
}

static bool stage2_pte_cacheable(struct kvm_pgtable *pgt, kvm_pte_t pte)
{
	u64 memattr = pte & KVM_PTE_LEAF_ATTR_LO_S2_MEMATTR;
	return kvm_pte_valid(pte) && memattr == KVM_S2_MEMATTR(pgt, NORMAL);
}

static bool stage2_pte_executable(kvm_pte_t pte)
{
	kvm_pte_t xn = FIELD_GET(KVM_PTE_LEAF_ATTR_HI_S2_XN, pte);

	return kvm_pte_valid(pte) && xn != KVM_PTE_LEAF_ATTR_HI_S2_XN_XN;
}

static u64 stage2_map_walker_phys_addr(const struct kvm_pgtable_visit_ctx *ctx,
				       const struct stage2_map_data *data)
{
	u64 phys = data->phys;

	/*
	 * Stage-2 walks to update ownership data are communicated to the map
	 * walker using an invalid PA. Avoid offsetting an already invalid PA,
	 * which could overflow and make the address valid again.
	 */
	if (!kvm_phys_is_valid(phys))
		return phys;

	/*
	 * Otherwise, work out the correct PA based on how far the walk has
	 * gotten.
	 */
	return phys + (ctx->addr - ctx->start);
}

static bool stage2_leaf_mapping_allowed(const struct kvm_pgtable_visit_ctx *ctx,
					struct stage2_map_data *data)
{
	u64 phys = stage2_map_walker_phys_addr(ctx, data);

	if (data->force_pte && (ctx->level < (KVM_PGTABLE_MAX_LEVELS - 1)))
		return false;

	return kvm_block_mapping_supported(ctx, phys);
}

static int stage2_map_walker_try_leaf(const struct kvm_pgtable_visit_ctx *ctx,
				      struct stage2_map_data *data)
{
	kvm_pte_t new;
	u64 phys = stage2_map_walker_phys_addr(ctx, data);
	u64 granule = kvm_granule_size(ctx->level);
	struct kvm_pgtable *pgt = data->mmu->pgt;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_pgtable_pte_ops *pte_ops = pgt->pte_ops;
	bool old_is_counted;

	if (!stage2_leaf_mapping_allowed(ctx, data))
		return -E2BIG;

	if (kvm_phys_is_valid(phys))
		new = kvm_init_valid_leaf_pte(phys, data->attr, ctx->level);
	else
		new = data->annotation;

	old_is_counted = pte_ops->pte_is_counted_cb(ctx->old, ctx->level);
	if (old_is_counted) {
		/*
		 * Skip updating a guest PTE if we are trying to recreate the
		 * exact same mapping or change only the access permissions.
		 * Instead, the vCPU will exit one more time from the guest if
		 * still needed and then go through the path of relaxing
		 * permissions. This applies only to guest PTEs; Host PTEs
		 * are unconditionally updated. The host cannot livelock
		 * because the abort handler has done prior checks before
		 * calling here.
		 */
		if (!stage2_pte_needs_update(pgt, ctx->old, new))
			return -EAGAIN;
	}

	/* If we're only changing software bits, then store them and go! */
	if (!kvm_pgtable_walk_shared(ctx) &&
	    !((ctx->old ^ new) & ~KVM_PTE_LEAF_ATTR_HI_SW)) {
		if (old_is_counted !=
		    pte_ops->pte_is_counted_cb(new, ctx->level)) {
			if (old_is_counted)
				mm_ops->put_page(ctx->ptep);
			else
				mm_ops->get_page(ctx->ptep);
		}
		WRITE_ONCE(*ctx->ptep, new);
		return 0;
	}

	if (!stage2_try_break_pte(ctx, data->mmu))
		return -EAGAIN;

	/* Perform CMOs before installation of the guest stage-2 PTE */
	if (!kvm_pgtable_walk_skip_cmo(ctx) && mm_ops->dcache_clean_inval_poc &&
	    stage2_pte_cacheable(pgt, new))
		mm_ops->dcache_clean_inval_poc(kvm_pte_follow(new, mm_ops),
					       granule);

	if (!kvm_pgtable_walk_skip_cmo(ctx) && mm_ops->icache_inval_pou &&
	    stage2_pte_executable(new))
		mm_ops->icache_inval_pou(kvm_pte_follow(new, mm_ops), granule);

	stage2_make_pte(ctx, new);

	return 0;
}

static int stage2_map_walk_table_pre(const struct kvm_pgtable_visit_ctx *ctx,
				     struct stage2_map_data *data)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	kvm_pte_t *childp = kvm_pte_follow(ctx->old, mm_ops);
	int ret;

	if (!stage2_leaf_mapping_allowed(ctx, data))
		return 0;

	ret = stage2_map_walker_try_leaf(ctx, data);
	if (ret)
		return ret;

	mm_ops->free_unlinked_table(childp, ctx->level);
	return 0;
}

static void stage2_map_prefault_block(struct kvm_pgtable_pte_ops *pte_ops,
				      const struct kvm_pgtable_visit_ctx *ctx,
				      kvm_pte_t *ptep)
{
	kvm_pte_t block_pte = ctx->old;
	u64 pa, granule;
	bool counted;
	int i;

	if (!kvm_pte_valid(block_pte))
		return;

	pa = kvm_pte_to_phys(block_pte);
	granule = kvm_granule_size(ctx->level + 1);
	counted = pte_ops->pte_is_counted_cb(block_pte, ctx->level + 1);

	for (i = 0; i < PTRS_PER_PTE; ++i, ++ptep, pa += granule) {
		kvm_pte_t pte = kvm_init_valid_leaf_pte(
			pa, block_pte, ctx->level + 1);
		/*
		 * Skip ptes in the range being modified by the caller if we're
		 * installing last level entries. Otherwise, we need to
		 * temporarily put in a valid mapping to make sure the
		 * prefaulting logic is triggered on the next
		 * stage2_map_walk_leaf(). This adds an unnecessary TLBI as we'll
		 * presumably re-break the freshly installed block, but that
		 * should happen very infrequently.
		 */
		if ((ctx->level < (KVM_PGTABLE_MAX_LEVELS - 2)) ||
				(pa < ctx->addr) || (pa >= ctx->end)) {
			/* We can write non-atomically: ptep isn't yet live. */
			*ptep = pte;

			if (counted)
				ctx->mm_ops->get_page(ptep);
		}
	}
}

static int stage2_map_walk_leaf(const struct kvm_pgtable_visit_ctx *ctx,
				struct stage2_map_data *data)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_pgtable *pgt = data->mmu->pgt;
	struct kvm_pgtable_pte_ops *pte_ops = pgt->pte_ops;
	kvm_pte_t *childp, new;
	int ret;

	ret = stage2_map_walker_try_leaf(ctx, data);
	if (ret != -E2BIG)
		return ret;

	if (WARN_ON(ctx->level == KVM_PGTABLE_MAX_LEVELS - 1))
		return -EINVAL;

	if (!data->memcache)
		return -ENOMEM;

	childp = mm_ops->zalloc_page(data->memcache);
	if (!childp)
		return -ENOMEM;

	WARN_ON((pgt->flags & KVM_PGTABLE_S2_IDMAP) &&
		pte_ops->pte_is_counted_cb(ctx->old, ctx->level));

	if (pgt->flags & KVM_PGTABLE_S2_PREFAULT_BLOCK)
		stage2_map_prefault_block(pte_ops, ctx, childp);

	if (!stage2_try_break_pte(ctx, data->mmu)) {
		mm_ops->put_page(childp);
		return -EAGAIN;
	}

	/*
	 * If we've run into an existing block mapping then replace it with
	 * a table. Accesses beyond 'end' that fall within the new table
	 * will be mapped lazily.
	 */
	new = kvm_init_table_pte(childp, mm_ops);
	stage2_make_pte(ctx, new);
	return 0;
}

static void debug_check_table_before_coalescing(
	const struct kvm_pgtable_visit_ctx *ctx,
	struct stage2_map_data *data,
	kvm_pte_t *ptep, u64 pa)
{
#ifdef CONFIG_NVHE_EL2_DEBUG
	u64 granule = kvm_granule_size(ctx->level + 1);
	int i;

	for (i = 0; i < PTRS_PER_PTE; i++, ptep++, pa += granule) {
		kvm_pte_t pte = kvm_init_valid_leaf_pte(
			pa, data->attr, ctx->level + 1);
		WARN_ON(pte != *ptep);
	}
#endif
}

static int stage2_coalesce_walk_table_post(const struct kvm_pgtable_visit_ctx *ctx,
					   struct stage2_map_data *data)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	kvm_pte_t new, *childp = kvm_pte_follow(ctx->old, mm_ops);
	u64 size, addr;

	/*
	 * We don't want to coalesce during pkvm initialisation, before the
	 * overall structure of the host S2 table is created.
	 */
	if (!static_branch_likely(&kvm_protected_mode_initialized))
		return 0;

	/*
	 * If we installed a non-refcounted valid mapping, and the table has no
	 * other raised references, then we can immediately collapse to a block
	 * mapping.
	 */
	if (!kvm_phys_is_valid(data->phys) ||
	    !kvm_level_supports_block_mapping(ctx->level) ||
	    (mm_ops->page_count(childp) != 1))
		return 0;

	/*
	 * This should apply only to the host S2, which does not refcount its
	 * default memory and mmio mappings.
	 */
	WARN_ON(!(data->mmu->pgt->flags & KVM_PGTABLE_S2_IDMAP));

	size = kvm_granule_size(ctx->level);
	addr = ALIGN_DOWN(ctx->addr, size);

	debug_check_table_before_coalescing(ctx, data, childp, addr);

	new = kvm_init_valid_leaf_pte(addr, data->attr, ctx->level);

	/* Breaking must succeed, as this is not a shared walk. */
	WARN_ON(!stage2_try_break_pte(ctx, data->mmu));

	/* Host doesn't require CMOs. */
	WARN_ON(mm_ops->dcache_clean_inval_poc || mm_ops->icache_inval_pou);

	stage2_make_pte(ctx, new);

	/* Finally, free the unlinked table. */
	mm_ops->put_page(childp);

	return 0;
}

/*
 * The TABLE_PRE callback runs for table entries on the way down, looking
 * for table entries which we could conceivably replace with a block entry
 * for this mapping. If it finds one it replaces the entry and calls
 * kvm_pgtable_mm_ops::free_unlinked_table() to tear down the detached table.
 *
 * Otherwise, the LEAF callback performs the mapping at the existing leaves
 * instead.
 */
static int stage2_map_walker(const struct kvm_pgtable_visit_ctx *ctx,
			     enum kvm_pgtable_walk_flags visit)
{
	struct stage2_map_data *data = ctx->arg;

	switch (visit) {
	case KVM_PGTABLE_WALK_TABLE_PRE:
		return stage2_map_walk_table_pre(ctx, data);
	case KVM_PGTABLE_WALK_LEAF:
		return stage2_map_walk_leaf(ctx, data);
	case KVM_PGTABLE_WALK_TABLE_POST:
		return stage2_coalesce_walk_table_post(ctx, data);
	default:
		return -EINVAL;
	}
}

int kvm_pgtable_stage2_map(struct kvm_pgtable *pgt, u64 addr, u64 size,
			   u64 phys, enum kvm_pgtable_prot prot,
			   void *mc, enum kvm_pgtable_walk_flags flags)
{
	int ret;
	struct kvm_pgtable_pte_ops *pte_ops = pgt->pte_ops;
	struct stage2_map_data map_data = {
		.phys		= ALIGN_DOWN(phys, PAGE_SIZE),
		.mmu		= pgt->mmu,
		.memcache	= mc,
		.force_pte	= pte_ops->force_pte_cb &&
			pte_ops->force_pte_cb(addr, addr + size, prot),
	};
	struct kvm_pgtable_walker walker = {
		.cb		= stage2_map_walker,
		.flags		= flags |
				  KVM_PGTABLE_WALK_TABLE_PRE |
				  KVM_PGTABLE_WALK_LEAF |
				  KVM_PGTABLE_WALK_TABLE_POST,
		.arg		= &map_data,
	};

	if (pte_ops->force_pte_cb)
		map_data.force_pte = pte_ops->force_pte_cb(addr, addr + size, prot);

	if (WARN_ON((pgt->flags & KVM_PGTABLE_S2_IDMAP) && (addr != phys)))
		return -EINVAL;

	ret = stage2_set_prot_attr(pgt, prot, &map_data.attr);
	if (ret)
		return ret;

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	dsb(ishst);
	return ret;
}

int kvm_pgtable_stage2_annotate(struct kvm_pgtable *pgt, u64 addr, u64 size,
				void *mc, kvm_pte_t annotation)
{
	int ret;
	struct stage2_map_data map_data = {
		.phys		= KVM_PHYS_INVALID,
		.mmu		= pgt->mmu,
		.memcache	= mc,
		.force_pte	= true,
		.annotation	= annotation,
	};
	struct kvm_pgtable_walker walker = {
		.cb		= stage2_map_walker,
		.flags		= KVM_PGTABLE_WALK_TABLE_PRE |
				  KVM_PGTABLE_WALK_LEAF,
		.arg		= &map_data,
	};

	if (annotation & PTE_VALID)
		return -EINVAL;

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	return ret;
}

static int stage2_unmap_walker(const struct kvm_pgtable_visit_ctx *ctx,
			       enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable *pgt = ctx->arg;
	struct kvm_s2_mmu *mmu = pgt->mmu;
	struct kvm_pgtable_pte_ops *pte_ops = ctx->pte_ops;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	kvm_pte_t *childp = NULL;
	bool need_flush = false;

	if (!kvm_pte_valid(ctx->old)) {
		if (pte_ops->pte_is_counted_cb(ctx->old, ctx->level)) {
			kvm_clear_pte(ctx->ptep);
			mm_ops->put_page(ctx->ptep);
		}
		return 0;
	}

	if (kvm_pte_table(ctx->old, ctx->level)) {
		childp = kvm_pte_follow(ctx->old, mm_ops);

		if (mm_ops->page_count(childp) != 1)
			return 0;
	} else if (stage2_pte_cacheable(pgt, ctx->old)) {
		need_flush = !stage2_has_fwb(pgt);
	}

	/*
	 * This is similar to the map() path in that we unmap the entire
	 * block entry and rely on the remaining portions being faulted
	 * back lazily.
	 */
	if (pte_ops->pte_is_counted_cb(ctx->old, ctx->level))
		stage2_unmap_put_pte(ctx, mmu, mm_ops);
	else
		stage2_unmap_clear_pte(ctx, mmu);

	if (need_flush && mm_ops->dcache_clean_inval_poc)
		mm_ops->dcache_clean_inval_poc(kvm_pte_follow(ctx->old, mm_ops),
					       kvm_granule_size(ctx->level));

	if (childp)
		mm_ops->put_page(childp);

	return 0;
}

int kvm_pgtable_stage2_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	int ret;
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_unmap_walker,
		.arg	= pgt,
		.flags	= KVM_PGTABLE_WALK_LEAF | KVM_PGTABLE_WALK_TABLE_POST,
	};

	/*
	 * stage2_unmap_walker's TLBI logic is unsafe for the pKVM host stage-2
	 * table because a child table may have a refcount of 1 while still
	 * containing valid mappings. The use of __kvm_tlb_flush_vmid_ipa in
	 * stage2_unmap_clear_pte is then insufficient to invalidate all leaf
	 * mappings reachable from the child table. All other stage-2 tables
	 * hold a reference for every non-zero PTE, and are thus guaranteed to
	 * be completely empty when refcount is 1.
	 */
	if (WARN_ON(pgt->flags & KVM_PGTABLE_S2_IDMAP))
		return -EINVAL;

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	if (stage2_unmap_defer_tlb_flush(pgt))
		/* Perform the deferred TLB invalidations */
		kvm_tlb_flush_vmid_range(pgt->mmu, addr, size);

	return ret;
}

static int stage2_reclaim_leaf_walker(const struct kvm_pgtable_visit_ctx *ctx,
				      enum kvm_pgtable_walk_flags visit)
{
	struct stage2_map_data *data = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	kvm_pte_t *childp = kvm_pte_follow(ctx->old, mm_ops);
	u64 size, addr;

	/*
	 * If this table's refcount is not raised, we can safely discard it.
	 * Any mappings that it contains can be re-created on demand.
	 */
	if (!kvm_level_supports_block_mapping(ctx->level) ||
	    (mm_ops->page_count(childp) != 1))
		return 0;

	size = kvm_granule_size(ctx->level);
	addr = ALIGN_DOWN(ctx->addr, size);

	/* Unlink the table and flush TLBs. */
	kvm_clear_pte(ctx->ptep);
	kvm_tlb_flush_vmid_range(data->mmu, addr, size);

	/* Free the unlinked table, and drop its reference in the parent. */
	mm_ops->put_page(ctx->ptep);
	mm_ops->put_page(childp);

	return 0;
}

int kvm_pgtable_stage2_reclaim_leaves(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	struct stage2_map_data map_data = {
		.phys		= KVM_PHYS_INVALID,
		.mmu		= pgt->mmu,
	};
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_reclaim_leaf_walker,
		.arg	= &map_data,
		.flags	= KVM_PGTABLE_WALK_TABLE_POST,
	};

	return kvm_pgtable_walk(pgt, addr, size, &walker);
}

struct stage2_attr_data {
	kvm_pte_t			attr_set;
	kvm_pte_t			attr_clr;
	kvm_pte_t			pte;
	u32				level;
};

static int stage2_attr_walker(const struct kvm_pgtable_visit_ctx *ctx,
			      enum kvm_pgtable_walk_flags visit)
{
	kvm_pte_t pte = ctx->old;
	struct stage2_attr_data *data = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;

	if (!kvm_pte_valid(ctx->old))
		return -EAGAIN;

	data->level = ctx->level;
	data->pte = pte;
	pte &= ~data->attr_clr;
	pte |= data->attr_set;

	/*
	 * We may race with the CPU trying to set the access flag here,
	 * but worst-case the access flag update gets lost and will be
	 * set on the next access instead.
	 */
	if (data->pte != pte) {
		/*
		 * Invalidate instruction cache before updating the guest
		 * stage-2 PTE if we are going to add executable permission.
		 */
		if (mm_ops->icache_inval_pou &&
		    stage2_pte_executable(pte) && !stage2_pte_executable(ctx->old))
			mm_ops->icache_inval_pou(kvm_pte_follow(pte, mm_ops),
						  kvm_granule_size(ctx->level));

		if (!stage2_try_set_pte(ctx, pte))
			return -EAGAIN;
	}

	return 0;
}

static int stage2_update_leaf_attrs(struct kvm_pgtable *pgt, u64 addr,
				    u64 size, kvm_pte_t attr_set,
				    kvm_pte_t attr_clr, kvm_pte_t *orig_pte,
				    u32 *level, enum kvm_pgtable_walk_flags flags)
{
	int ret;
	kvm_pte_t attr_mask = KVM_PTE_LEAF_ATTR_LO | KVM_PTE_LEAF_ATTR_HI;
	struct stage2_attr_data data = {
		.attr_set	= attr_set & attr_mask,
		.attr_clr	= attr_clr & attr_mask,
	};
	struct kvm_pgtable_walker walker = {
		.cb		= stage2_attr_walker,
		.arg		= &data,
		.flags		= flags | KVM_PGTABLE_WALK_LEAF,
	};

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	if (ret)
		return ret;

	if (orig_pte)
		*orig_pte = data.pte;

	if (level)
		*level = data.level;
	return 0;
}

int kvm_pgtable_stage2_wrprotect(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	return stage2_update_leaf_attrs(pgt, addr, size, 0,
					KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W,
					NULL, NULL, 0);
}

kvm_pte_t kvm_pgtable_stage2_mkyoung(struct kvm_pgtable *pgt, u64 addr)
{
	kvm_pte_t pte = 0;
	int ret;

	ret = stage2_update_leaf_attrs(pgt, addr, 1, KVM_PTE_LEAF_ATTR_LO_S2_AF, 0,
				       &pte, NULL,
				       KVM_PGTABLE_WALK_HANDLE_FAULT |
				       KVM_PGTABLE_WALK_SHARED);
	if (!ret)
		dsb(ishst);

	return pte;
}

struct stage2_age_data {
	bool	mkold;
	bool	young;
};

static int stage2_age_walker(const struct kvm_pgtable_visit_ctx *ctx,
			     enum kvm_pgtable_walk_flags visit)
{
	kvm_pte_t new = ctx->old & ~KVM_PTE_LEAF_ATTR_LO_S2_AF;
	struct stage2_age_data *data = ctx->arg;

	if (!kvm_pte_valid(ctx->old) || new == ctx->old)
		return 0;

	data->young = true;

	/*
	 * stage2_age_walker() is always called while holding the MMU lock for
	 * write, so this will always succeed. Nonetheless, this deliberately
	 * follows the race detection pattern of the other stage-2 walkers in
	 * case the locking mechanics of the MMU notifiers is ever changed.
	 */
	if (data->mkold && !stage2_try_set_pte(ctx, new))
		return -EAGAIN;

	/*
	 * "But where's the TLBI?!", you scream.
	 * "Over in the core code", I sigh.
	 *
	 * See the '->clear_flush_young()' callback on the KVM mmu notifier.
	 */
	return 0;
}

bool kvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr,
					 u64 size, bool mkold)
{
	struct stage2_age_data data = {
		.mkold		= mkold,
	};
	struct kvm_pgtable_walker walker = {
		.cb		= stage2_age_walker,
		.arg		= &data,
		.flags		= KVM_PGTABLE_WALK_LEAF,
	};

	WARN_ON(kvm_pgtable_walk(pgt, addr, size, &walker));
	return data.young;
}

int kvm_pgtable_stage2_relax_perms(struct kvm_pgtable *pgt, u64 addr,
				   enum kvm_pgtable_prot prot)
{
	return __kvm_pgtable_stage2_relax_perms(pgt, addr, prot,
						KVM_PGTABLE_WALK_HANDLE_FAULT |
						KVM_PGTABLE_WALK_SHARED);
}

int __kvm_pgtable_stage2_relax_perms(struct kvm_pgtable *pgt, u64 addr,
				     enum kvm_pgtable_prot prot,
				     enum kvm_pgtable_walk_flags flags)
{
	int ret;
	u32 level;
	kvm_pte_t set = 0, clr = 0;

	if (prot & ~KVM_PGTABLE_PROT_RWX)
		return -EINVAL;

	if (prot & KVM_PGTABLE_PROT_R)
		set |= KVM_PTE_LEAF_ATTR_LO_S2_S2AP_R;

	if (prot & KVM_PGTABLE_PROT_W)
		set |= KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W;

	if (prot & KVM_PGTABLE_PROT_X)
		clr |= KVM_PTE_LEAF_ATTR_HI_S2_XN;

	ret = stage2_update_leaf_attrs(pgt, addr, 1, set, clr, NULL, &level, flags);
	if (!ret)
		kvm_call_hyp(__kvm_tlb_flush_vmid_ipa_nsh, pgt->mmu, addr, level);
	return ret;
}

static int stage2_flush_walker(const struct kvm_pgtable_visit_ctx *ctx,
			       enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable *pgt = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = pgt->mm_ops;

	if (!stage2_pte_cacheable(pgt, ctx->old))
		return 0;

	if (mm_ops->dcache_clean_inval_poc)
		mm_ops->dcache_clean_inval_poc(kvm_pte_follow(ctx->old, mm_ops),
					       kvm_granule_size(ctx->level));
	return 0;
}

int kvm_pgtable_stage2_flush(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_flush_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= pgt,
	};

	if (stage2_has_fwb(pgt))
		return 0;

	return kvm_pgtable_walk(pgt, addr, size, &walker);
}

kvm_pte_t *kvm_pgtable_stage2_create_unlinked(struct kvm_pgtable *pgt,
					      u64 phys, u32 level,
					      enum kvm_pgtable_prot prot,
					      void *mc, bool force_pte)
{
	struct stage2_map_data map_data = {
		.phys		= phys,
		.mmu		= pgt->mmu,
		.memcache	= mc,
		.force_pte	= force_pte,
	};
	struct kvm_pgtable_walker walker = {
		.cb		= stage2_map_walker,
		.flags		= KVM_PGTABLE_WALK_LEAF |
				  KVM_PGTABLE_WALK_SKIP_BBM_TLBI |
				  KVM_PGTABLE_WALK_SKIP_CMO,
		.arg		= &map_data,
	};
	/*
	 * The input address (.addr) is irrelevant for walking an
	 * unlinked table. Construct an ambiguous IA range to map
	 * kvm_granule_size(level) worth of memory.
	 */
	struct kvm_pgtable_walk_data data = {
		.walker	= &walker,
		.addr	= 0,
		.end	= kvm_granule_size(level),
	};
	struct kvm_pgtable_mm_ops *mm_ops = pgt->mm_ops;
	kvm_pte_t *pgtable;
	int ret;

	if (!IS_ALIGNED(phys, kvm_granule_size(level)))
		return ERR_PTR(-EINVAL);

	ret = stage2_set_prot_attr(pgt, prot, &map_data.attr);
	if (ret)
		return ERR_PTR(ret);

	pgtable = mm_ops->zalloc_page(mc);
	if (!pgtable)
		return ERR_PTR(-ENOMEM);

	ret = __kvm_pgtable_walk(&data, mm_ops, pgt->pte_ops,
				 (kvm_pteref_t)pgtable, level + 1);
	if (ret) {
		kvm_pgtable_stage2_free_unlinked(mm_ops, pgt->pte_ops,
						 pgtable, level);
		mm_ops->put_page(pgtable);
		return ERR_PTR(ret);
	}

	return pgtable;
}

/*
 * Get the number of page-tables needed to replace a block with a
 * fully populated tree up to the PTE entries. Note that @level is
 * interpreted as in "level @level entry".
 */
static int stage2_block_get_nr_page_tables(u32 level)
{
	switch (level) {
	case 1:
		return PTRS_PER_PTE + 1;
	case 2:
		return 1;
	case 3:
		return 0;
	default:
		WARN_ON_ONCE(level < KVM_PGTABLE_MIN_BLOCK_LEVEL ||
			     level >= KVM_PGTABLE_MAX_LEVELS);
		return -EINVAL;
	};
}

static int stage2_split_walker(const struct kvm_pgtable_visit_ctx *ctx,
			       enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_mmu_memory_cache *mc = ctx->arg;
	struct kvm_s2_mmu *mmu;
	kvm_pte_t pte = ctx->old, new, *childp;
	enum kvm_pgtable_prot prot;
	u32 level = ctx->level;
	bool force_pte;
	int nr_pages;
	u64 phys;

	/* No huge-pages exist at the last level */
	if (level == KVM_PGTABLE_MAX_LEVELS - 1)
		return 0;

	/* We only split valid block mappings */
	if (!kvm_pte_valid(pte))
		return 0;

	nr_pages = stage2_block_get_nr_page_tables(level);
	if (nr_pages < 0)
		return nr_pages;

	if (mc->nobjs >= nr_pages) {
		/* Build a tree mapped down to the PTE granularity. */
		force_pte = true;
	} else {
		/*
		 * Don't force PTEs, so create_unlinked() below does
		 * not populate the tree up to the PTE level. The
		 * consequence is that the call will require a single
		 * page of level 2 entries at level 1, or a single
		 * page of PTEs at level 2. If we are at level 1, the
		 * PTEs will be created recursively.
		 */
		force_pte = false;
		nr_pages = 1;
	}

	if (mc->nobjs < nr_pages)
		return -ENOMEM;

	mmu = container_of(mc, struct kvm_s2_mmu, split_page_cache);
	phys = kvm_pte_to_phys(pte);
	prot = kvm_pgtable_stage2_pte_prot(pte);

	childp = kvm_pgtable_stage2_create_unlinked(mmu->pgt, phys,
						    level, prot, mc, force_pte);
	if (IS_ERR(childp))
		return PTR_ERR(childp);

	if (!stage2_try_break_pte(ctx, mmu)) {
		kvm_pgtable_stage2_free_unlinked(mm_ops, ctx->pte_ops,
						 childp, level);
		mm_ops->put_page(childp);
		return -EAGAIN;
	}

	/*
	 * Note, the contents of the page table are guaranteed to be made
	 * visible before the new PTE is assigned because stage2_make_pte()
	 * writes the PTE using smp_store_release().
	 */
	new = kvm_init_table_pte(childp, mm_ops);
	stage2_make_pte(ctx, new);
	dsb(ishst);
	return 0;
}

int kvm_pgtable_stage2_split(struct kvm_pgtable *pgt, u64 addr, u64 size,
			     struct kvm_mmu_memory_cache *mc)
{
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_split_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= mc,
	};

	return kvm_pgtable_walk(pgt, addr, size, &walker);
}

int __kvm_pgtable_stage2_init(struct kvm_pgtable *pgt, struct kvm_s2_mmu *mmu,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      enum kvm_pgtable_stage2_flags flags,
			      struct kvm_pgtable_pte_ops *pte_ops)
{
	size_t pgd_sz;
	u64 vtcr = mmu->arch->vtcr;
	u32 ia_bits = VTCR_EL2_IPA(vtcr);
	u32 sl0 = FIELD_GET(VTCR_EL2_SL0_MASK, vtcr);
	u32 start_level = VTCR_EL2_TGRAN_SL0_BASE - sl0;

	pgd_sz = kvm_pgd_pages(ia_bits, start_level) * PAGE_SIZE;
	pgt->pgd = (kvm_pteref_t)mm_ops->zalloc_pages_exact(pgd_sz);
	if (!pgt->pgd)
		return -ENOMEM;

	pgt->ia_bits		= ia_bits;
	pgt->start_level	= start_level;
	pgt->mm_ops		= mm_ops;
	pgt->mmu		= mmu;
	pgt->flags		= flags;
	pgt->pte_ops		= pte_ops;

	/* Ensure zeroed PGD pages are visible to the hardware walker */
	dsb(ishst);
	return 0;
}

size_t kvm_pgtable_stage2_pgd_size(u64 vtcr)
{
	u32 ia_bits = VTCR_EL2_IPA(vtcr);
	u32 sl0 = FIELD_GET(VTCR_EL2_SL0_MASK, vtcr);
	u32 start_level = VTCR_EL2_TGRAN_SL0_BASE - sl0;

	return kvm_pgd_pages(ia_bits, start_level) * PAGE_SIZE;
}

static int stage2_free_walker(const struct kvm_pgtable_visit_ctx *ctx,
			      enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	struct kvm_pgtable_pte_ops *pte_ops = ctx->pte_ops;

	if (!pte_ops->pte_is_counted_cb(ctx->old, ctx->level))
		return 0;

	mm_ops->put_page(ctx->ptep);

	if (kvm_pte_table(ctx->old, ctx->level))
		mm_ops->put_page(kvm_pte_follow(ctx->old, mm_ops));

	return 0;
}

void kvm_pgtable_stage2_destroy(struct kvm_pgtable *pgt)
{
	size_t pgd_sz;
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_free_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF |
			  KVM_PGTABLE_WALK_TABLE_POST,
	};

	WARN_ON(kvm_pgtable_walk(pgt, 0, BIT(pgt->ia_bits), &walker));
	pgd_sz = kvm_pgd_pages(pgt->ia_bits, pgt->start_level) * PAGE_SIZE;
	pgt->mm_ops->free_pages_exact(kvm_dereference_pteref(&walker, pgt->pgd), pgd_sz);
	pgt->pgd = NULL;
}

void kvm_pgtable_stage2_free_unlinked(struct kvm_pgtable_mm_ops *mm_ops,
				      struct kvm_pgtable_pte_ops *pte_ops,
				      void *pgtable, u32 level)
{
	kvm_pteref_t ptep = (kvm_pteref_t)pgtable;
	struct kvm_pgtable_walker walker = {
		.cb	= stage2_free_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF |
			  KVM_PGTABLE_WALK_TABLE_POST,
	};
	struct kvm_pgtable_walk_data data = {
		.walker	= &walker,

		/*
		 * At this point the IPA really doesn't matter, as the page
		 * table being traversed has already been removed from the stage
		 * 2. Set an appropriate range to cover the entire page table.
		 */
		.addr	= 0,
		.end	= kvm_granule_size(level),
	};

	WARN_ON(__kvm_pgtable_walk(&data, mm_ops, pte_ops, ptep, level + 1));

	WARN_ON(mm_ops->page_count(pgtable) != 1);
	mm_ops->put_page(pgtable);
}

#ifdef CONFIG_NVHE_EL2_DEBUG
static int snapshot_walker(const struct kvm_pgtable_visit_ctx *ctx,
			   enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;
	void *copy_table, *original_addr;
	kvm_pte_t new = ctx->old;

	if (kvm_pte_table(ctx->old, ctx->level)) {
		copy_table = mm_ops->zalloc_page(ctx->arg);
		if (!copy_table)
			return -ENOMEM;

		original_addr = kvm_pte_follow(ctx->old, mm_ops);
		WARN_ON(!PAGE_ALIGNED(original_addr));

		memcpy(copy_table, original_addr, PAGE_SIZE);
		new = kvm_init_table_pte(copy_table, mm_ops);
	}

	*ctx->ptep = new;

	return 0;
}

int kvm_pgtable_stage2_snapshot(struct kvm_pgtable_snapshot *dest_pgt,
				struct kvm_pgtable *src_pgt,
				size_t pgd_len)
{
	struct kvm_pgtable *to_pgt = &dest_pgt->pgtable;
	struct kvm_pgtable_walker walker = {
		.cb	= snapshot_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF |
			  KVM_PGTABLE_WALK_TABLE_PRE,
		.arg	= &dest_pgt->mc,
	};

	if (!to_pgt->pgd || dest_pgt->pgd_pages < (pgd_len >> PAGE_SHIFT))
		return -EINVAL;

	/*
	 * Walk the destination pagetable with the original PGD entries from
	 * the source, then descend into the newly installed leafs.
	 */
	memcpy(to_pgt->pgd, src_pgt->pgd, pgd_len);

	return kvm_pgtable_walk(to_pgt, 0, BIT(to_pgt->ia_bits), &walker);
}
#endif /* CONFIG_NVHE_EL2_DEBUG */
