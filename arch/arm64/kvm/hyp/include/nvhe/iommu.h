/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ARM64_KVM_NVHE_IOMMU_H__
#define __ARM64_KVM_NVHE_IOMMU_H__

#include <asm/kvm_host.h>

#include <kvm/iommu.h>

#include <nvhe/alloc_mgt.h>

/* Hypercall handlers */
int kvm_iommu_alloc_domain(pkvm_handle_t domain_id, int type);
int kvm_iommu_free_domain(pkvm_handle_t domain_id);
int kvm_iommu_attach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid, u32 pasid_bits);
int kvm_iommu_detach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid);
size_t kvm_iommu_map_pages(pkvm_handle_t domain_id,
			   unsigned long iova, phys_addr_t paddr, size_t pgsize,
			   size_t pgcount, int prot);
size_t kvm_iommu_unmap_pages(pkvm_handle_t domain_id, unsigned long iova,
			     size_t pgsize, size_t pgcount);
phys_addr_t kvm_iommu_iova_to_phys(pkvm_handle_t domain_id, unsigned long iova);

/* Flags for memory allocation for IOMMU drivers */
#define IOMMU_PAGE_NOCACHE				BIT(0)
void *kvm_iommu_donate_pages(u8 order, int flags);
void kvm_iommu_reclaim_pages(void *p, u8 order);

#define kvm_iommu_donate_page()		kvm_iommu_donate_pages(0, 0)
#define kvm_iommu_donate_page_nc()	kvm_iommu_donate_pages(0, IOMMU_PAGE_NOCACHE)
#define kvm_iommu_reclaim_page(p)	kvm_iommu_reclaim_pages(p, 0)

struct kvm_iommu_ops {
	int (*init)(void);
	int (*alloc_domain)(struct kvm_hyp_iommu_domain *domain, int type);
	void (*free_domain)(struct kvm_hyp_iommu_domain *domain);
};

int kvm_iommu_init(void);

extern struct hyp_mgt_allocator_ops kvm_iommu_allocator_ops;

#endif /* __ARM64_KVM_NVHE_IOMMU_H__ */
