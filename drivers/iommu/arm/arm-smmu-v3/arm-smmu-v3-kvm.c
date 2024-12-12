// SPDX-License-Identifier: GPL-2.0
/*
 * pKVM host driver for the Arm SMMUv3
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <asm/kvm_mmu.h>

#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <kvm/arm_smmu_v3.h>

#include "arm-smmu-v3.h"

extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);

static int kvm_arm_smmu_probe(struct platform_device *pdev)
{
	return -ENOSYS;
}

static void kvm_arm_smmu_remove(struct platform_device *pdev)
{
}

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v3", },
	{ },
};

static struct platform_driver kvm_arm_smmu_driver = {
	.driver = {
		.name = "kvm-arm-smmu-v3",
		.of_match_table = arm_smmu_of_match,
	},
	.remove = kvm_arm_smmu_remove,
};

static int kvm_arm_smmu_v3_init_drv(void)
{
	return platform_driver_probe(&kvm_arm_smmu_driver, kvm_arm_smmu_probe);
}

static void kvm_arm_smmu_v3_remove_drv(void)
{
	platform_driver_unregister(&kvm_arm_smmu_driver);
}

struct kvm_iommu_driver kvm_smmu_v3_ops = {
	.init_driver = kvm_arm_smmu_v3_init_drv,
	.remove_driver = kvm_arm_smmu_v3_remove_drv,
};

static int kvm_arm_smmu_v3_register(void)
{
	if (!is_protected_kvm_enabled())
		return 0;

	return kvm_iommu_register_driver(&kvm_smmu_v3_ops,
					kern_hyp_va(lm_alias(&kvm_nvhe_sym(smmu_ops))));
};

core_initcall(kvm_arm_smmu_v3_register);
