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
