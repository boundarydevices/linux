// SPDX-License-Identifier: GPL-2.0
/*
 * IOMMU operations for pKVM
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>

/* Only one set of ops supported, similary to the kernel */
struct kvm_iommu_ops *kvm_iommu_ops;

/*
 * Common pool that can be used by IOMMU driver to allocate pages.
 */
static struct hyp_pool iommu_host_pool;

DECLARE_PER_CPU(struct kvm_hyp_req, host_hyp_reqs);

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

int kvm_iommu_init(void)
{
	int ret;

	if (!kvm_iommu_ops || !kvm_iommu_ops->init)
		return -ENODEV;

	ret = hyp_pool_init_empty(&iommu_host_pool, 64);
	if (ret)
		return ret;

	return kvm_iommu_ops->init();
}

int kvm_iommu_alloc_domain(pkvm_handle_t domain_id, int type)
{
	return -ENODEV;
}

int kvm_iommu_free_domain(pkvm_handle_t domain_id)
{
	return -ENODEV;
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
