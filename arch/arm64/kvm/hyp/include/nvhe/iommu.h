/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ARM64_KVM_NVHE_IOMMU_H__
#define __ARM64_KVM_NVHE_IOMMU_H__

#include <asm/kvm_host.h>

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

struct kvm_iommu_ops {
	int (*init)(void);
};

int kvm_iommu_init(void);

#endif /* __ARM64_KVM_NVHE_IOMMU_H__ */
