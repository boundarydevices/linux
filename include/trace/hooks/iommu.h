/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM iommu

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_IOMMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_IOMMU_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_RESTRICTED_HOOK(android_rvh_iommu_setup_dma_ops,
	TP_PROTO(struct device *dev),
	TP_ARGS(dev), 1);

struct iova_domain;
struct iova;

DECLARE_RESTRICTED_HOOK(android_rvh_iommu_alloc_insert_iova,
	TP_PROTO(struct iova_domain *iovad, unsigned long size,
		unsigned long limit_pfn, struct iova *new_iova,
		bool size_aligned, int *ret),
	TP_ARGS(iovad, size, limit_pfn, new_iova, size_aligned, ret),
	1);

DECLARE_RESTRICTED_HOOK(android_rvh_iommu_dma_info_to_prot,
	TP_PROTO(unsigned long attrs, int *prot),
	TP_ARGS(attrs, prot), 1);

DECLARE_HOOK(android_vh_iommu_iovad_alloc_iova,
	TP_PROTO(struct device *dev, struct iova_domain *iovad, dma_addr_t iova, size_t size),
	TP_ARGS(dev, iovad, iova, size));

DECLARE_HOOK(android_vh_iommu_iovad_free_iova,
	TP_PROTO(struct iova_domain *iovad, dma_addr_t iova, size_t size),
	TP_ARGS(iovad, iova, size));

DECLARE_HOOK(android_vh_adjust_alloc_flags,
	TP_PROTO(unsigned int order, gfp_t *alloc_flags),
	TP_ARGS(order, alloc_flags));

DECLARE_RESTRICTED_HOOK(android_rvh_iommu_iovad_init_alloc_algo,
	TP_PROTO(struct device *dev, struct iova_domain *iovad),
	TP_ARGS(dev, iovad), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_iommu_limit_align_shift,
	TP_PROTO(struct iova_domain *iovad, unsigned long size,
		unsigned long *shift),
	TP_ARGS(iovad, size, shift), 1);

#endif /* _TRACE_HOOK_IOMMU_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
