// SPDX-License-Identifier: GPL-2.0
/*
 * IOMMU operations for pKVM
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <nvhe/iommu.h>

/* Only one set of ops supported, similary to the kernel */
struct kvm_iommu_ops *kvm_iommu_ops;

int kvm_iommu_init(void)
{
	if (!kvm_iommu_ops || !kvm_iommu_ops->init)
		return -ENODEV;

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
