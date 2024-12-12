// SPDX-License-Identifier: GPL-2.0
/*
 * IOMMU operations for pKVM
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <kvm/iommu.h>

#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>

/* Only one set of ops supported, similary to the kernel */
struct kvm_iommu_ops *kvm_iommu_ops;
void **kvm_hyp_iommu_domains;

/*
 * Common pool that can be used by IOMMU driver to allocate pages.
 */
static struct hyp_pool iommu_host_pool;

DECLARE_PER_CPU(struct kvm_hyp_req, host_hyp_reqs);

/* Protects domains in kvm_hyp_iommu_domains */
static DEFINE_HYP_SPINLOCK(kvm_iommu_domain_lock);

static int kvm_iommu_refill(struct kvm_hyp_memcache *host_mc)
{
	if (!kvm_iommu_ops)
		return -EINVAL;

	return refill_hyp_pool(&iommu_host_pool, host_mc);
}

static void kvm_iommu_reclaim(struct kvm_hyp_memcache *host_mc, int target)
{
	if (!kvm_iommu_ops)
		return;

	reclaim_hyp_pool(&iommu_host_pool, host_mc, target);
}

static int kvm_iommu_reclaimable(void)
{
	if (!kvm_iommu_ops)
		return 0;

	return hyp_pool_free_pages(&iommu_host_pool);
}

struct hyp_mgt_allocator_ops kvm_iommu_allocator_ops = {
	.refill = kvm_iommu_refill,
	.reclaim = kvm_iommu_reclaim,
	.reclaimable = kvm_iommu_reclaimable,
};

void *kvm_iommu_donate_pages(u8 order, int flags)
{
	void *p;
	struct kvm_hyp_req *req = this_cpu_ptr(&host_hyp_reqs);
	int ret;

	p = hyp_alloc_pages(&iommu_host_pool, order);
	if (p) {
		/*
		 * If page request is non-cacheable remap it as such
		 * as all pages in the pool are mapped before hand and
		 * assumed to be cacheable.
		 */
		if (flags & IOMMU_PAGE_NOCACHE) {
			ret = pkvm_remap_range(p, 1 << order, true);
			if (ret) {
				hyp_put_page(&iommu_host_pool, p);
				return NULL;
			}
		}
		return p;
	}

	req->type = KVM_HYP_REQ_TYPE_MEM;
	req->mem.dest = REQ_MEM_DEST_HYP_IOMMU;
	req->mem.sz_alloc = (1 << order) * PAGE_SIZE;
	req->mem.nr_pages = 1;
	return NULL;
}

void kvm_iommu_reclaim_pages(void *p, u8 order)
{
	/*
	 * Remap all pages to cacheable, as we don't know, may be use a flag
	 * in the vmemmap or trust the driver to pass the cacheability same
	 * as the allocation on free?
	 */
	pkvm_remap_range(p, 1 << order, false);
	hyp_put_page(&iommu_host_pool, p);
}

static struct kvm_hyp_iommu_domain *
handle_to_domain(pkvm_handle_t domain_id)
{
	int idx;
	struct kvm_hyp_iommu_domain *domains;

	if (domain_id >= KVM_IOMMU_MAX_DOMAINS)
		return NULL;
	domain_id = array_index_nospec(domain_id, KVM_IOMMU_MAX_DOMAINS);

	idx = domain_id / KVM_IOMMU_DOMAINS_PER_PAGE;
	domains = (struct kvm_hyp_iommu_domain *)READ_ONCE(kvm_hyp_iommu_domains[idx]);
	if (!domains) {
		domains = kvm_iommu_donate_page();
		if (!domains)
			return NULL;
		/*
		 * handle_to_domain() does not have to be called under a lock,
		 * but even though we allocate a leaf in all cases, it's only
		 * really a valid thing to do under alloc_domain(), which uses a
		 * lock. Races are therefore a host bug and we don't need to be
		 * delicate about it.
		 */
		if (WARN_ON(cmpxchg64_relaxed(&kvm_hyp_iommu_domains[idx], 0,
					      (void *)domains) != 0)) {
			kvm_iommu_reclaim_page(domains);
			return NULL;
		}
	}
	return &domains[domain_id % KVM_IOMMU_DOMAINS_PER_PAGE];
}

int kvm_iommu_init(void)
{
	int ret;
	u64 domain_root_pfn = __hyp_pa(kvm_hyp_iommu_domains) >> PAGE_SHIFT;

	if (!kvm_iommu_ops ||
	    !kvm_iommu_ops->init ||
	    !kvm_iommu_ops->alloc_domain ||
	    !kvm_iommu_ops->free_domain)
		return -ENODEV;

	ret = hyp_pool_init_empty(&iommu_host_pool, 64);
	if (ret)
		return ret;

	ret = __pkvm_host_donate_hyp(domain_root_pfn,
				     KVM_IOMMU_DOMAINS_ROOT_ORDER_NR);
	if (ret)
		return ret;

	ret = kvm_iommu_ops->init();
	if (ret)
		goto out_reclaim_domain;

	return ret;

out_reclaim_domain:
	__pkvm_hyp_donate_host(domain_root_pfn, KVM_IOMMU_DOMAINS_ROOT_ORDER_NR);
	return ret;
}

int kvm_iommu_alloc_domain(pkvm_handle_t domain_id, int type)
{
	int ret = -EINVAL;
	struct kvm_hyp_iommu_domain *domain;

	domain = handle_to_domain(domain_id);
	if (!domain)
		return -ENOMEM;

	hyp_spin_lock(&kvm_iommu_domain_lock);
	if (atomic_read(&domain->refs))
		goto out_unlock;

	domain->domain_id = domain_id;
	ret = kvm_iommu_ops->alloc_domain(domain, type);
	if (ret)
		goto out_unlock;

	atomic_set_release(&domain->refs, 1);
out_unlock:
	hyp_spin_unlock(&kvm_iommu_domain_lock);
	return ret;
}

int kvm_iommu_free_domain(pkvm_handle_t domain_id)
{
	int ret = 0;
	struct kvm_hyp_iommu_domain *domain;

	domain = handle_to_domain(domain_id);
	if (!domain)
		return -EINVAL;

	hyp_spin_lock(&kvm_iommu_domain_lock);
	if (WARN_ON(atomic_cmpxchg_acquire(&domain->refs, 1, 0) != 1)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	kvm_iommu_ops->free_domain(domain);

	memset(domain, 0, sizeof(*domain));

out_unlock:
	hyp_spin_unlock(&kvm_iommu_domain_lock);

	return ret;
}

int kvm_iommu_attach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid, u32 pasid_bits)
{
	return -ENODEV;
}

int kvm_iommu_detach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid)
{
	return -ENODEV;
}

size_t kvm_iommu_map_pages(pkvm_handle_t domain_id,
			   unsigned long iova, phys_addr_t paddr, size_t pgsize,
			   size_t pgcount, int prot)
{
	return 0;
}

size_t kvm_iommu_unmap_pages(pkvm_handle_t domain_id, unsigned long iova,
			     size_t pgsize, size_t pgcount)
{
	return 0;
}

phys_addr_t kvm_iommu_iova_to_phys(pkvm_handle_t domain_id, unsigned long iova)
{
	return 0;
}
