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

struct host_arm_smmu_device {
	struct arm_smmu_device		smmu;
	pkvm_handle_t			id;
};

#define smmu_to_host(_smmu) \
	container_of(_smmu, struct host_arm_smmu_device, smmu);

static size_t				kvm_arm_smmu_cur;
static size_t				kvm_arm_smmu_count;
static struct hyp_arm_smmu_v3_device	*kvm_arm_smmu_array;

static int kvm_arm_smmu_probe(struct platform_device *pdev)
{
	int ret;
	size_t size;
	phys_addr_t ioaddr;
	struct resource *res;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	struct host_arm_smmu_device *host_smmu;
	struct hyp_arm_smmu_v3_device *hyp_smmu;

	if (kvm_arm_smmu_cur >= kvm_arm_smmu_count)
		return -ENOSPC;

	hyp_smmu = &kvm_arm_smmu_array[kvm_arm_smmu_cur];

	host_smmu = devm_kzalloc(dev, sizeof(*host_smmu), GFP_KERNEL);
	if (!host_smmu)
		return -ENOMEM;

	smmu = &host_smmu->smmu;
	smmu->dev = dev;

	ret = arm_smmu_fw_probe(pdev, smmu);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	size = resource_size(res);
	if (size < SZ_128K) {
		dev_err(dev, "unsupported MMIO region size (%pr)\n", res);
		return -EINVAL;
	}
	ioaddr = res->start;
	host_smmu->id = kvm_arm_smmu_cur;

	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);

	ret = arm_smmu_device_hw_probe(smmu);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, smmu);

	/* Hypervisor parameters */
	hyp_smmu->pgsize_bitmap = smmu->pgsize_bitmap;
	hyp_smmu->oas = smmu->oas;
	hyp_smmu->ias = smmu->ias;
	hyp_smmu->mmio_addr = ioaddr;
	hyp_smmu->mmio_size = size;
	kvm_arm_smmu_cur++;

	return arm_smmu_register_iommu(smmu, &kvm_arm_smmu_ops, ioaddr);
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

static int kvm_arm_smmu_array_alloc(void)
{
	int smmu_order;
	struct device_node *np;

	kvm_arm_smmu_count = 0;
	for_each_compatible_node(np, NULL, "arm,smmu-v3")
		kvm_arm_smmu_count++;

	if (!kvm_arm_smmu_count)
		return 0;

	/* Allocate the parameter list shared with the hypervisor */
	smmu_order = get_order(kvm_arm_smmu_count * sizeof(*kvm_arm_smmu_array));
	kvm_arm_smmu_array = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
						      smmu_order);
	if (!kvm_arm_smmu_array)
		return -ENOMEM;

	return 0;
}

static void kvm_arm_smmu_array_free(void)
{
	int order;

	order = get_order(kvm_arm_smmu_count * sizeof(*kvm_arm_smmu_array));
	free_pages((unsigned long)kvm_arm_smmu_array, order);
}

static int kvm_arm_smmu_v3_init_drv(void)
{
	int ret;

	/*
	 * Check whether any device owned by the host is behind an SMMU.
	 */
	ret = kvm_arm_smmu_array_alloc();
	if (ret || !kvm_arm_smmu_count)
		return ret;

	ret = platform_driver_probe(&kvm_arm_smmu_driver, kvm_arm_smmu_probe);
	if (ret)
		goto err_free;

	if (kvm_arm_smmu_cur != kvm_arm_smmu_count) {
		/* A device exists but failed to probe */
		ret = -EUNATCH;
		goto err_free;
	}

	/*
	 * These variables are stored in the nVHE image, and won't be accessible
	 * after KVM initialization. Ownership of kvm_arm_smmu_array will be
	 * transferred to the hypervisor as well.
	 */
	kvm_hyp_arm_smmu_v3_smmus = kvm_arm_smmu_array;
	kvm_hyp_arm_smmu_v3_count = kvm_arm_smmu_count;
	return 0;

err_free:
	kvm_arm_smmu_array_free();
	return ret;
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
