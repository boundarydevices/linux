/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ARM64_KVM_NVHE_IOMMU_H__
#define __ARM64_KVM_NVHE_IOMMU_H__

#include <asm/kvm_host.h>

struct kvm_iommu_ops {
	int (*init)(void);
};

int kvm_iommu_init(void);

#endif /* __ARM64_KVM_NVHE_IOMMU_H__ */
