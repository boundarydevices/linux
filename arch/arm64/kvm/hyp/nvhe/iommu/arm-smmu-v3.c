// SPDX-License-Identifier: GPL-2.0
/*
 * pKVM hyp driver for the Arm SMMUv3
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <asm/arm-smmu-v3-common.h>
#include <asm/kvm_hyp.h>
#include <kvm/arm_smmu_v3.h>
#include <linux/io-pgtable-arm.h>
#include <nvhe/alloc.h>
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>

#define ARM_SMMU_POLL_TIMEOUT_US	100000 /* 100ms arbitrary timeout */

size_t __ro_after_init kvm_hyp_arm_smmu_v3_count;
struct hyp_arm_smmu_v3_device *kvm_hyp_arm_smmu_v3_smmus;

#define for_each_smmu(smmu) \
	for ((smmu) = kvm_hyp_arm_smmu_v3_smmus; \
	     (smmu) != &kvm_hyp_arm_smmu_v3_smmus[kvm_hyp_arm_smmu_v3_count]; \
	     (smmu)++)

/*
 * Wait until @cond is true.
 * Return 0 on success, or -ETIMEDOUT
 */
#define smmu_wait(_cond)					\
({								\
	int __i = 0;						\
	int __ret = 0;						\
								\
	while (!(_cond)) {					\
		if (++__i > ARM_SMMU_POLL_TIMEOUT_US) {		\
			__ret = -ETIMEDOUT;			\
			break;					\
		}						\
		pkvm_udelay(1);					\
	}							\
	__ret;							\
})

#define smmu_wait_event(_smmu, _cond)				\
({								\
	if ((_smmu)->features & ARM_SMMU_FEAT_SEV) {		\
		while (!(_cond))				\
			wfe();					\
	}							\
	smmu_wait(_cond);					\
})

/*
 * SMMUv3 domain:
 * @domain: Pointer to the IOMMU domain.
 * @smmu: SMMU instance for this domain.
 * @type: Type of domain (S1, S2)
 * @pgt_lock: Lock for page table
 * @pgtable: io_pgtable instance for this domain
 */
struct hyp_arm_smmu_v3_domain {
	struct kvm_hyp_iommu_domain     *domain;
	struct hyp_arm_smmu_v3_device	*smmu;
	u32				type;
	hyp_spinlock_t			pgt_lock;
	struct io_pgtable		*pgtable;
};

static struct hyp_arm_smmu_v3_device *to_smmu(struct kvm_hyp_iommu *iommu)
{
	return container_of(iommu, struct hyp_arm_smmu_v3_device, iommu);
}

static int smmu_write_cr0(struct hyp_arm_smmu_v3_device *smmu, u32 val)
{
	writel_relaxed(val, smmu->base + ARM_SMMU_CR0);
	return smmu_wait(readl_relaxed(smmu->base + ARM_SMMU_CR0ACK) == val);
}

/* Transfer ownership of structures from host to hyp */
static int smmu_take_pages(u64 phys, size_t size)
{
	WARN_ON(!PAGE_ALIGNED(phys) || !PAGE_ALIGNED(size));
	return __pkvm_host_donate_hyp(phys >> PAGE_SHIFT, size >> PAGE_SHIFT);
}

static void smmu_reclaim_pages(u64 phys, size_t size)
{
	WARN_ON(!PAGE_ALIGNED(phys) || !PAGE_ALIGNED(size));
	WARN_ON(__pkvm_hyp_donate_host(phys >> PAGE_SHIFT, size >> PAGE_SHIFT));
}

#define Q_WRAP(smmu, reg)	((reg) & (1 << (smmu)->cmdq_log2size))
#define Q_IDX(smmu, reg)	((reg) & ((1 << (smmu)->cmdq_log2size) - 1))

static bool smmu_cmdq_full(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 cons = readl_relaxed(smmu->base + ARM_SMMU_CMDQ_CONS);

	return Q_IDX(smmu, smmu->cmdq_prod) == Q_IDX(smmu, cons) &&
	       Q_WRAP(smmu, smmu->cmdq_prod) != Q_WRAP(smmu, cons);
}

static bool smmu_cmdq_empty(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 cons = readl_relaxed(smmu->base + ARM_SMMU_CMDQ_CONS);

	return Q_IDX(smmu, smmu->cmdq_prod) == Q_IDX(smmu, cons) &&
	       Q_WRAP(smmu, smmu->cmdq_prod) == Q_WRAP(smmu, cons);
}

static int smmu_add_cmd(struct hyp_arm_smmu_v3_device *smmu,
			struct arm_smmu_cmdq_ent *ent)
{
	int i;
	int ret;
	u64 cmd[CMDQ_ENT_DWORDS] = {};
	int idx = Q_IDX(smmu, smmu->cmdq_prod);
	u64 *slot = smmu->cmdq_base + idx * CMDQ_ENT_DWORDS;

	if (smmu->iommu.power_is_off)
		return -EPIPE;

	ret = smmu_wait_event(smmu, !smmu_cmdq_full(smmu));
	if (ret)
		return ret;

	cmd[0] |= FIELD_PREP(CMDQ_0_OP, ent->opcode);

	switch (ent->opcode) {
	case CMDQ_OP_CFGI_ALL:
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_RANGE, 31);
		break;
	case CMDQ_OP_CFGI_CD:
		cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SSID, ent->cfgi.ssid);
		fallthrough;
	case CMDQ_OP_CFGI_STE:
		cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SID, ent->cfgi.sid);
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_LEAF, ent->cfgi.leaf);
		break;
	case CMDQ_OP_TLBI_NH_VA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_NUM, ent->tlbi.num);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_SCALE, ent->tlbi.scale);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TTL, ent->tlbi.ttl);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TG, ent->tlbi.tg);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_VA_MASK;
		break;
	case CMDQ_OP_TLBI_NSNH_ALL:
		break;
	case CMDQ_OP_TLBI_NH_ASID:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		fallthrough;
	case CMDQ_OP_TLBI_S12_VMALL:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		break;
	case CMDQ_OP_TLBI_S2_IPA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_NUM, ent->tlbi.num);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_SCALE, ent->tlbi.scale);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TTL, ent->tlbi.ttl);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TG, ent->tlbi.tg);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_IPA_MASK;
		break;
	case CMDQ_OP_CMD_SYNC:
		cmd[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_SEV);
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < CMDQ_ENT_DWORDS; i++)
		slot[i] = cpu_to_le64(cmd[i]);

	smmu->cmdq_prod++;
	writel(Q_IDX(smmu, smmu->cmdq_prod) | Q_WRAP(smmu, smmu->cmdq_prod),
	       smmu->base + ARM_SMMU_CMDQ_PROD);
	return 0;
}

static int smmu_sync_cmd(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CMD_SYNC,
	};

	ret = smmu_add_cmd(smmu, &cmd);
	if (ret)
		return ret;

	return smmu_wait_event(smmu, smmu_cmdq_empty(smmu));
}

static int smmu_send_cmd(struct hyp_arm_smmu_v3_device *smmu,
			 struct arm_smmu_cmdq_ent *cmd)
{
	int ret = smmu_add_cmd(smmu, cmd);

	if (ret)
		return ret;

	return smmu_sync_cmd(smmu);
}

static int smmu_sync_ste(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CFGI_STE,
		.cfgi.sid = sid,
		.cfgi.leaf = true,
	};

	return smmu_send_cmd(smmu, &cmd);
}

static int smmu_sync_cd(struct hyp_arm_smmu_v3_device *smmu, u32 sid, u32 ssid)
{
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CFGI_CD,
		.cfgi.sid	= sid,
		.cfgi.ssid	= ssid,
		.cfgi.leaf = true,
	};

	return smmu_send_cmd(smmu, &cmd);
}

static int smmu_alloc_l2_strtab(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	struct arm_smmu_strtab_l1 *l1_desc;
	dma_addr_t l2ptr_dma;
	struct arm_smmu_strtab_l2 *l2table;
	size_t l2_order = get_order(sizeof(struct arm_smmu_strtab_l2));
	int flags = 0;

	l1_desc = &cfg->l2.l1tab[arm_smmu_strtab_l1_idx(sid)];
	if (l1_desc->l2ptr)
		return 0;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		flags |= IOMMU_PAGE_NOCACHE;

	l2table = kvm_iommu_donate_pages(l2_order, flags);
	if (!l2table)
		return -ENOMEM;

	l2ptr_dma = hyp_virt_to_phys(l2table);

	if (l2ptr_dma & (~STRTAB_L1_DESC_L2PTR_MASK | ~PAGE_MASK)) {
		kvm_iommu_reclaim_pages(l2table, l2_order);
		return -EINVAL;
	}

	/* Ensure the empty stream table is visible before the descriptor write */
	wmb();

	arm_smmu_write_strtab_l1_desc(l1_desc, l2ptr_dma);
	return 0;
}

static struct arm_smmu_ste *
smmu_get_ste_ptr(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		struct arm_smmu_strtab_l1 *l1_desc =
					&cfg->l2.l1tab[arm_smmu_strtab_l1_idx(sid)];
		struct arm_smmu_strtab_l2 *l2ptr;

		if (arm_smmu_strtab_l1_idx(sid) > cfg->l2.num_l1_ents)
			return NULL;
		/* L2 should be allocated before calling this. */
		if (WARN_ON(!l1_desc->l2ptr))
			return NULL;

		l2ptr = hyp_phys_to_virt(l1_desc->l2ptr & STRTAB_L1_DESC_L2PTR_MASK);
		/* Two-level walk */
		return &l2ptr->stes[arm_smmu_strtab_l2_idx(sid)];
	}

	if (sid > cfg->linear.num_ents)
		return NULL;
	/* Simple linear lookup */
	return &cfg->linear.table[sid];
}

static struct arm_smmu_ste *
smmu_get_alloc_ste_ptr(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		int ret = smmu_alloc_l2_strtab(smmu, sid);

		if (ret) {
			WARN_ON(ret != -ENOMEM);
			return NULL;
		}
	}
	return smmu_get_ste_ptr(smmu, sid);
}

static u64 *smmu_get_cd_ptr(u64 *cdtab, u32 ssid)
{
	/* Only linear supported for now. */
	return cdtab + ssid * CTXDESC_CD_DWORDS;
}

static u64 *smmu_alloc_cd(struct hyp_arm_smmu_v3_device *smmu, u32 pasid_bits)
{
	u64 *cd_table;
	int flags = 0;
	u32 requested_order = get_order((1 << pasid_bits) *
					(CTXDESC_CD_DWORDS << 3));

	/*
	 * We support max of 64K linear tables only, this should be enough
	 * for 128 pasids
	 */
	if (WARN_ON(requested_order > 4))
		return NULL;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		flags |= IOMMU_PAGE_NOCACHE;

	cd_table = kvm_iommu_donate_pages(requested_order, flags);
	if (!cd_table)
		return NULL;
	return (u64 *)hyp_virt_to_phys(cd_table);
}

static void smmu_free_cd(u64 *cd_table, u32 pasid_bits)
{
	u32 order = get_order((1 << pasid_bits) *
			      (CTXDESC_CD_DWORDS << 3));

	kvm_iommu_reclaim_pages(cd_table, order);
}

static int smmu_init_registers(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 val, old;
	int ret;

	if (!(readl_relaxed(smmu->base + ARM_SMMU_GBPA) & GBPA_ABORT))
		return -EINVAL;

	/* Initialize all RW registers that will be read by the SMMU */
	ret = smmu_write_cr0(smmu, 0);
	if (ret)
		return ret;

	val = FIELD_PREP(CR1_TABLE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_TABLE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_TABLE_IC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_QUEUE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_IC, CR1_CACHE_WB);
	writel_relaxed(val, smmu->base + ARM_SMMU_CR1);
	writel_relaxed(CR2_PTM, smmu->base + ARM_SMMU_CR2);
	writel_relaxed(0, smmu->base + ARM_SMMU_IRQ_CTRL);

	val = readl_relaxed(smmu->base + ARM_SMMU_GERROR);
	old = readl_relaxed(smmu->base + ARM_SMMU_GERRORN);
	/* Service Failure Mode is fatal */
	if ((val ^ old) & GERROR_SFM_ERR)
		return -EIO;
	/* Clear pending errors */
	writel_relaxed(val, smmu->base + ARM_SMMU_GERRORN);

	return 0;
}

static int smmu_init_cmdq(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 cmdq_base;
	size_t cmdq_nr_entries, cmdq_size;
	int ret;
	enum kvm_pgtable_prot prot = PAGE_HYP;

	cmdq_base = readq_relaxed(smmu->base + ARM_SMMU_CMDQ_BASE);
	if (cmdq_base & ~(Q_BASE_RWA | Q_BASE_ADDR_MASK | Q_BASE_LOG2SIZE))
		return -EINVAL;

	smmu->cmdq_log2size = cmdq_base & Q_BASE_LOG2SIZE;
	cmdq_nr_entries = 1 << smmu->cmdq_log2size;
	cmdq_size = cmdq_nr_entries * CMDQ_ENT_DWORDS * 8;

	cmdq_base &= Q_BASE_ADDR_MASK;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		prot |= KVM_PGTABLE_PROT_NORMAL_NC;

	ret = ___pkvm_host_donate_hyp_prot(cmdq_base >> PAGE_SHIFT,
					   PAGE_ALIGN(cmdq_size) >> PAGE_SHIFT,
					   false, prot);
	if (ret)
		return ret;

	smmu->cmdq_base = hyp_phys_to_virt(cmdq_base);

	memset(smmu->cmdq_base, 0, cmdq_size);
	writel_relaxed(0, smmu->base + ARM_SMMU_CMDQ_PROD);
	writel_relaxed(0, smmu->base + ARM_SMMU_CMDQ_CONS);

	return 0;
}

/*
 * Event q support is optional and managed by the kernel,
 * However, it must set in a shared state so it can't be donated
 * to the hypervisor later.
 * This relies on the ARM_SMMU_EVTQ_BASE can't be changed after
 * de-privilege.
 */
static int smmu_init_evtq(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 evtq_base, evtq_pfn;
	size_t evtq_nr_entries, evtq_size, evtq_nr_pages;
	void *evtq_va, *evtq_end;
	size_t i;
	int ret;

	evtq_base = readq_relaxed(smmu->base + ARM_SMMU_EVTQ_BASE);
	if (!evtq_base)
		return 0;

	if (evtq_base & ~(Q_BASE_RWA | Q_BASE_ADDR_MASK | Q_BASE_LOG2SIZE))
		return -EINVAL;

	evtq_nr_entries = 1 << (evtq_base & Q_BASE_LOG2SIZE);
	evtq_size = evtq_nr_entries * EVTQ_ENT_DWORDS * 8;
	evtq_nr_pages = PAGE_ALIGN(evtq_size) >> PAGE_SHIFT;

	evtq_pfn = PAGE_ALIGN(evtq_base & Q_BASE_ADDR_MASK) >> PAGE_SHIFT;

	for (i = 0 ; i < evtq_nr_pages ; ++i) {
		ret = __pkvm_host_share_hyp(evtq_pfn + i);
		if (ret)
			return ret;
	}

	evtq_va = hyp_phys_to_virt(evtq_pfn << PAGE_SHIFT);
	evtq_end = hyp_phys_to_virt((evtq_pfn + evtq_nr_pages) << PAGE_SHIFT);

	return hyp_pin_shared_mem(evtq_va, evtq_end);
}

static int smmu_init_strtab(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	u64 strtab_base;
	size_t strtab_size;
	u32 strtab_cfg, fmt;
	int split, log2size;
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	enum kvm_pgtable_prot prot = PAGE_HYP;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		prot |= KVM_PGTABLE_PROT_NORMAL_NC;

	strtab_base = readq_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE);
	if (strtab_base & ~(STRTAB_BASE_ADDR_MASK | STRTAB_BASE_RA))
		return -EINVAL;

	strtab_cfg = readl_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE_CFG);
	if (strtab_cfg & ~(STRTAB_BASE_CFG_FMT | STRTAB_BASE_CFG_SPLIT |
			   STRTAB_BASE_CFG_LOG2SIZE))
		return -EINVAL;

	fmt = FIELD_GET(STRTAB_BASE_CFG_FMT, strtab_cfg);
	split = FIELD_GET(STRTAB_BASE_CFG_SPLIT, strtab_cfg);
	log2size = FIELD_GET(STRTAB_BASE_CFG_LOG2SIZE, strtab_cfg);
	strtab_base &= STRTAB_BASE_ADDR_MASK;

	switch (fmt) {
	case STRTAB_BASE_CFG_FMT_LINEAR:
		if (split)
			return -EINVAL;
		cfg->linear.num_ents = 1 << log2size;
		strtab_size = cfg->linear.num_ents * sizeof(struct arm_smmu_ste);
		cfg->linear.ste_dma = strtab_base;
		ret = ___pkvm_host_donate_hyp_prot(strtab_base >> PAGE_SHIFT,
						   PAGE_ALIGN(strtab_size) >> PAGE_SHIFT,
						   false, prot);
		if (ret)
			return -EINVAL;
		cfg->linear.table = hyp_phys_to_virt(strtab_base);
		/* Disable all STEs */
		memset(cfg->linear.table, 0, strtab_size);
		break;
	case STRTAB_BASE_CFG_FMT_2LVL:
		if (split != STRTAB_SPLIT)
			return -EINVAL;
		cfg->l2.num_l1_ents = 1 << max(0, log2size - split);
		strtab_size = cfg->l2.num_l1_ents * sizeof(struct arm_smmu_strtab_l1);
		cfg->l2.l1_dma = strtab_base;
		ret = ___pkvm_host_donate_hyp_prot(strtab_base >> PAGE_SHIFT,
						   PAGE_ALIGN(strtab_size) >> PAGE_SHIFT,
						   false, prot);
		if (ret)
			return -EINVAL;
		cfg->l2.l1tab = hyp_phys_to_virt(strtab_base);
		/* Disable all STEs */
		memset(cfg->l2.l1tab, 0, strtab_size);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int smmu_reset_device(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	struct arm_smmu_cmdq_ent cfgi_cmd = {
		.opcode = CMDQ_OP_CFGI_ALL,
	};
	struct arm_smmu_cmdq_ent tlbi_cmd = {
		.opcode = CMDQ_OP_TLBI_NSNH_ALL,
	};

	/* Invalidate all cached configs and TLBs */
	ret = smmu_write_cr0(smmu, CR0_CMDQEN);
	if (ret)
		return ret;

	ret = smmu_add_cmd(smmu, &cfgi_cmd);
	if (ret)
		goto err_disable_cmdq;

	ret = smmu_add_cmd(smmu, &tlbi_cmd);
	if (ret)
		goto err_disable_cmdq;

	ret = smmu_sync_cmd(smmu);
	if (ret)
		goto err_disable_cmdq;

	/* Enable translation */
	return smmu_write_cr0(smmu, CR0_SMMUEN | CR0_CMDQEN | CR0_ATSCHK);

err_disable_cmdq:
	return smmu_write_cr0(smmu, 0);
}

static int smmu_init_device(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;

	if (!PAGE_ALIGNED(smmu->mmio_addr | smmu->mmio_size))
		return -EINVAL;

	ret = ___pkvm_host_donate_hyp(smmu->mmio_addr >> PAGE_SHIFT,
				      smmu->mmio_size >> PAGE_SHIFT,
				      /* accept_mmio */ true);
	if (ret)
		return ret;

	smmu->base = hyp_phys_to_virt(smmu->mmio_addr);

	ret = smmu_init_registers(smmu);
	if (ret)
		return ret;

	ret = smmu_init_cmdq(smmu);
	if (ret)
		return ret;

	ret = smmu_init_evtq(smmu);
	if (ret)
		return ret;

	ret = smmu_init_strtab(smmu);
	if (ret)
		return ret;

	ret = smmu_reset_device(smmu);
	if (ret)
		return ret;

	return kvm_iommu_init_device(&smmu->iommu);
}

static int smmu_init(void)
{
	int ret;
	struct hyp_arm_smmu_v3_device *smmu;
	size_t smmu_arr_size = PAGE_ALIGN(sizeof(*kvm_hyp_arm_smmu_v3_smmus) *
					  kvm_hyp_arm_smmu_v3_count);
	phys_addr_t smmu_arr_phys;

	kvm_hyp_arm_smmu_v3_smmus = kern_hyp_va(kvm_hyp_arm_smmu_v3_smmus);

	smmu_arr_phys = hyp_virt_to_phys(kvm_hyp_arm_smmu_v3_smmus);

	ret = smmu_take_pages(smmu_arr_phys, smmu_arr_size);
	if (ret)
		return ret;

	for_each_smmu(smmu) {
		ret = smmu_init_device(smmu);
		if (ret)
			goto out_reclaim_smmu;
	}

	return 0;
out_reclaim_smmu:
	smmu_reclaim_pages(smmu_arr_phys, smmu_arr_size);
	return ret;
}

static struct kvm_hyp_iommu *smmu_id_to_iommu(pkvm_handle_t smmu_id)
{
	if (smmu_id >= kvm_hyp_arm_smmu_v3_count)
		return NULL;
	smmu_id = array_index_nospec(smmu_id, kvm_hyp_arm_smmu_v3_count);

	return &kvm_hyp_arm_smmu_v3_smmus[smmu_id].iommu;
}

static int smmu_alloc_domain(struct kvm_hyp_iommu_domain *domain, int type)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain;

	if (type >= KVM_ARM_SMMU_DOMAIN_MAX)
		return -EINVAL;

	smmu_domain = hyp_alloc(sizeof(*smmu_domain));
	if (!smmu_domain)
		return -ENOMEM;

	/*
	 * Can't do much without knowing the SMMUv3.
	 * Page table will be allocated at attach_dev, but can be
	 * freed from free domain.
	 */
	smmu_domain->domain = domain;
	smmu_domain->type = type;
	hyp_spin_lock_init(&smmu_domain->pgt_lock);
	domain->priv = (void *)smmu_domain;

	return 0;
}

static void smmu_free_domain(struct kvm_hyp_iommu_domain *domain)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	if (smmu_domain->pgtable)
		kvm_arm_io_pgtable_free(smmu_domain->pgtable);

	hyp_free(smmu_domain);
}

static void smmu_inv_domain(struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct kvm_hyp_iommu_domain *domain = smmu_domain->domain;
	struct hyp_arm_smmu_v3_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cmdq_ent cmd = {};

	if (smmu_domain->pgtable->cfg.fmt == ARM_64_LPAE_S2) {
		cmd.opcode = CMDQ_OP_TLBI_S12_VMALL;
		cmd.tlbi.vmid = domain->domain_id;
	} else {
		cmd.opcode = CMDQ_OP_TLBI_NH_ASID;
		cmd.tlbi.asid = domain->domain_id;
	}

	if (smmu->iommu.power_is_off)
		return;

	WARN_ON(smmu_send_cmd(smmu, &cmd));
}

static void smmu_tlb_flush_all(void *cookie)
{
	struct kvm_hyp_iommu_domain *domain = cookie;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct hyp_arm_smmu_v3_device *smmu = smmu_domain->smmu;

	kvm_iommu_lock(&smmu->iommu);
	smmu_inv_domain(smmu_domain);
	kvm_iommu_unlock(&smmu->iommu);
}

static int smmu_tlb_inv_range_smmu(struct hyp_arm_smmu_v3_device *smmu,
				   struct kvm_hyp_iommu_domain *domain,
				   struct arm_smmu_cmdq_ent *cmd,
				   unsigned long iova, size_t size, size_t granule)
{
	int ret = 0;
	unsigned long end = iova + size, num_pages = 0, tg = 0;
	size_t inv_range = granule;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	kvm_iommu_lock(&smmu->iommu);
	if (smmu->iommu.power_is_off)
		goto out_ret;

	/* Almost copy-paste from the kernel dirver. */
	if (smmu->features & ARM_SMMU_FEAT_RANGE_INV) {
		/* Get the leaf page size */
		tg = __ffs(smmu_domain->pgtable->cfg.pgsize_bitmap);

		num_pages = size >> tg;

		/* Convert page size of 12,14,16 (log2) to 1,2,3 */
		cmd->tlbi.tg = (tg - 10) / 2;

		/*
		 * Determine what level the granule is at. For non-leaf, both
		 * io-pgtable and SVA pass a nominal last-level granule because
		 * they don't know what level(s) actually apply, so ignore that
		 * and leave TTL=0. However for various errata reasons we still
		 * want to use a range command, so avoid the SVA corner case
		 * where both scale and num could be 0 as well.
		 */
		if (cmd->tlbi.leaf)
			cmd->tlbi.ttl = 4 - ((ilog2(granule) - 3) / (tg - 3));
		else if ((num_pages & CMDQ_TLBI_RANGE_NUM_MAX) == 1)
			num_pages++;
	}

	while (iova < end) {
		if (smmu->features & ARM_SMMU_FEAT_RANGE_INV) {
			/*
			 * On each iteration of the loop, the range is 5 bits
			 * worth of the aligned size remaining.
			 * The range in pages is:
			 *
			 * range = (num_pages & (0x1f << __ffs(num_pages)))
			 */
			unsigned long scale, num;

			/* Determine the power of 2 multiple number of pages */
			scale = __ffs(num_pages);
			cmd->tlbi.scale = scale;

			/* Determine how many chunks of 2^scale size we have */
			num = (num_pages >> scale) & CMDQ_TLBI_RANGE_NUM_MAX;
			cmd->tlbi.num = num - 1;

			/* range is num * 2^scale * pgsize */
			inv_range = num << (scale + tg);

			/* Clear out the lower order bits for the next iteration */
			num_pages -= num << scale;
		}
		cmd->tlbi.addr = iova;
		WARN_ON(smmu_add_cmd(smmu, cmd));
		BUG_ON(iova + inv_range < iova);
		iova += inv_range;
	}

	ret = smmu_sync_cmd(smmu);
out_ret:
	kvm_iommu_unlock(&smmu->iommu);
	return ret;
}

static void smmu_tlb_inv_range(struct kvm_hyp_iommu_domain *domain,
			       unsigned long iova, size_t size, size_t granule,
			       bool leaf)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	unsigned long end = iova + size;
	struct arm_smmu_cmdq_ent cmd;

	cmd.tlbi.leaf = leaf;
	if (smmu_domain->pgtable->cfg.fmt == ARM_64_LPAE_S2) {
		cmd.opcode = CMDQ_OP_TLBI_S2_IPA;
		cmd.tlbi.vmid = domain->domain_id;
	} else {
		cmd.opcode = CMDQ_OP_TLBI_NH_VA;
		cmd.tlbi.asid = domain->domain_id;
		cmd.tlbi.vmid = 0;
	}
	/*
	 * There are no mappings at high addresses since we don't use TTB1, so
	 * no overflow possible.
	 */
	BUG_ON(end < iova);
	WARN_ON(smmu_tlb_inv_range_smmu(smmu_domain->smmu, domain,
					&cmd, iova, size, granule));
}

static void smmu_tlb_flush_walk(unsigned long iova, size_t size,
				size_t granule, void *cookie)
{
	smmu_tlb_inv_range(cookie, iova, size, granule, false);
}

static void smmu_tlb_add_page(struct iommu_iotlb_gather *gather,
			      unsigned long iova, size_t granule,
			      void *cookie)
{
	if (gather)
		kvm_iommu_iotlb_gather_add_page(cookie, gather, iova, granule);
	else
		smmu_tlb_inv_range(cookie, iova, granule, granule, true);
}

static const struct iommu_flush_ops smmu_tlb_ops = {
	.tlb_flush_all	= smmu_tlb_flush_all,
	.tlb_flush_walk = smmu_tlb_flush_walk,
	.tlb_add_page	= smmu_tlb_add_page,
};

static void smmu_iotlb_sync(struct kvm_hyp_iommu_domain *domain,
			    struct iommu_iotlb_gather *gather)
{
	size_t size;

	if (!gather->pgsize)
		return;
	size = gather->end - gather->start + 1;
	smmu_tlb_inv_range(domain, gather->start, size,  gather->pgsize, true);
}

static int smmu_domain_config_s2(struct kvm_hyp_iommu_domain *domain,
				 struct arm_smmu_ste *ste)
{
	struct io_pgtable_cfg *cfg;
	u64 ts, sl, ic, oc, sh, tg, ps;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	cfg = &smmu_domain->pgtable->cfg;
	ps = cfg->arm_lpae_s2_cfg.vtcr.ps;
	tg = cfg->arm_lpae_s2_cfg.vtcr.tg;
	sh = cfg->arm_lpae_s2_cfg.vtcr.sh;
	oc = cfg->arm_lpae_s2_cfg.vtcr.orgn;
	ic = cfg->arm_lpae_s2_cfg.vtcr.irgn;
	sl = cfg->arm_lpae_s2_cfg.vtcr.sl;
	ts = cfg->arm_lpae_s2_cfg.vtcr.tsz;

	ste->data[0] = STRTAB_STE_0_V |
		FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S2_TRANS);
	ste->data[1] = FIELD_PREP(STRTAB_STE_1_SHCFG, STRTAB_STE_1_SHCFG_INCOMING);
	ste->data[2] = FIELD_PREP(STRTAB_STE_2_VTCR,
			FIELD_PREP(STRTAB_STE_2_VTCR_S2PS, ps) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2TG, tg) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2SH0, sh) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2OR0, oc) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2IR0, ic) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2SL0, sl) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2T0SZ, ts)) |
		 FIELD_PREP(STRTAB_STE_2_S2VMID, domain->domain_id) |
		 STRTAB_STE_2_S2AA64 | STRTAB_STE_2_S2R;
	ste->data[3] = cfg->arm_lpae_s2_cfg.vttbr & STRTAB_STE_3_S2TTB_MASK;

	return 0;
}

static u64 *smmu_domain_config_s1_ste(struct hyp_arm_smmu_v3_device *smmu,
				      u32 pasid_bits, struct arm_smmu_ste *ste)
{
	u64 *cd_table;

	cd_table = smmu_alloc_cd(smmu, pasid_bits);
	if (!cd_table)
		return NULL;

	ste->data[1] = FIELD_PREP(STRTAB_STE_1_S1DSS, STRTAB_STE_1_S1DSS_SSID0) |
		FIELD_PREP(STRTAB_STE_1_S1CIR, STRTAB_STE_1_S1C_CACHE_WBRA) |
		FIELD_PREP(STRTAB_STE_1_S1COR, STRTAB_STE_1_S1C_CACHE_WBRA) |
		FIELD_PREP(STRTAB_STE_1_S1CSH, ARM_SMMU_SH_ISH);
	ste->data[0] = ((u64)cd_table & STRTAB_STE_0_S1CTXPTR_MASK) |
		FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S1_TRANS) |
		FIELD_PREP(STRTAB_STE_0_S1CDMAX, pasid_bits) |
		FIELD_PREP(STRTAB_STE_0_S1FMT, STRTAB_STE_0_S1FMT_LINEAR) |
		STRTAB_STE_0_V;

	return cd_table;
}

/*
 * This function handles configuration for pasid and non-pasid domains
 * with the following assumptions:
 * - pasid 0 always attached first, this should be the typicall flow
 *   for the kernel where attach_dev is always called before set_dev_pasid.
 *   In that case only pasid 0 is allowed to allocate memory for the CD,
 *   and other pasids would expect to find the tabel.
 * - pasid 0 is detached last, also guaranteed from the kernel.
 */
static int smmu_domain_config_s1(struct hyp_arm_smmu_v3_device *smmu,
				 struct kvm_hyp_iommu_domain *domain,
				 u32 sid, u32 pasid, u32 pasid_bits,
				 struct arm_smmu_ste *ste)
{
	struct arm_smmu_ste *dst;
	u64 val;
	u64 *cd_entry, *cd_table;
	struct io_pgtable_cfg *cfg;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	cfg = &smmu_domain->pgtable->cfg;
	dst = smmu_get_ste_ptr(smmu, sid);
	val = dst->data[0];

	if (FIELD_GET(STRTAB_STE_0_CFG, val) == STRTAB_STE_0_CFG_S2_TRANS)
		return -EBUSY;

	if (pasid == 0) {
		cd_table = smmu_domain_config_s1_ste(smmu, pasid_bits, ste);
		if (!cd_table)
			return -ENOMEM;
	} else {
		u32 nr_entries;

		cd_table = (u64 *)(FIELD_GET(STRTAB_STE_0_S1CTXPTR_MASK, val) << 6);
		if (!cd_table)
			return -EINVAL;
		nr_entries = 1 << FIELD_GET(STRTAB_STE_0_S1CDMAX, val);
		if (pasid >= nr_entries)
			return -E2BIG;
	}

	/* Write CD. */
	cd_entry = smmu_get_cd_ptr(hyp_phys_to_virt((u64)cd_table), pasid);

	/* CD already used by another device. */
	if (cd_entry[0])
		return -EBUSY;

	cd_entry[1] = cpu_to_le64(cfg->arm_lpae_s1_cfg.ttbr & CTXDESC_CD_1_TTB0_MASK);
	cd_entry[2] = 0;
	cd_entry[3] = cpu_to_le64(cfg->arm_lpae_s1_cfg.mair);

	/* STE is live. */
	if (pasid)
		smmu_sync_cd(smmu, sid, pasid);
	val =  FIELD_PREP(CTXDESC_CD_0_TCR_T0SZ, cfg->arm_lpae_s1_cfg.tcr.tsz) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_TG0, cfg->arm_lpae_s1_cfg.tcr.tg) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_IRGN0, cfg->arm_lpae_s1_cfg.tcr.irgn) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_ORGN0, cfg->arm_lpae_s1_cfg.tcr.orgn) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_SH0, cfg->arm_lpae_s1_cfg.tcr.sh) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_IPS, cfg->arm_lpae_s1_cfg.tcr.ips) |
	       CTXDESC_CD_0_TCR_EPD1 | CTXDESC_CD_0_AA64 |
	       CTXDESC_CD_0_R | CTXDESC_CD_0_A |
	       CTXDESC_CD_0_ASET |
	       FIELD_PREP(CTXDESC_CD_0_ASID, domain->domain_id) |
	       CTXDESC_CD_0_V;
	WRITE_ONCE(cd_entry[0], cpu_to_le64(val));
	/* STE is live. */
	if (pasid)
		smmu_sync_cd(smmu, sid, pasid);
	return 0;
}

static int smmu_domain_finalise(struct hyp_arm_smmu_v3_device *smmu,
				struct kvm_hyp_iommu_domain *domain)
{
	int ret;
	struct io_pgtable_cfg cfg;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S1) {
		size_t ias = (smmu->features & ARM_SMMU_FEAT_VAX) ? 52 : 48;

		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S1,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = min_t(unsigned long, ias, VA_BITS),
			.oas = smmu->ias,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
			.tlb = &smmu_tlb_ops,
		};
	} else {
		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S2,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = smmu->ias,
			.oas = smmu->oas,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
			.tlb = &smmu_tlb_ops,
		};
	}

	hyp_spin_lock(&smmu_domain->pgt_lock);
	smmu_domain->pgtable = kvm_arm_io_pgtable_alloc(&cfg, domain, &ret);
	hyp_spin_unlock(&smmu_domain->pgt_lock);
	return ret;
}

static int smmu_attach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid, u32 pasid_bits)
{
	int i;
	int ret;
	struct arm_smmu_ste *dst;
	struct arm_smmu_ste ste = {};
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	kvm_iommu_lock(iommu);
	dst = smmu_get_alloc_ste_ptr(smmu, sid);
	if (!dst) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	if (smmu_domain->smmu && (smmu != smmu_domain->smmu)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	if (!smmu_domain->pgtable) {
		ret = smmu_domain_finalise(smmu, domain);
		if (ret)
			goto out_unlock;
	}

	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S2) {
		/* Device already attached or pasid for s2. */
		if (dst->data[0] || pasid) {
			ret = -EBUSY;
			goto out_unlock;
		}
		ret = smmu_domain_config_s2(domain, &ste);
	} else {
		/*
		 * Allocate and config CD, and update CD if possible.
		 */
		pasid_bits = min(pasid_bits, smmu->ssid_bits);
		ret = smmu_domain_config_s1(smmu, domain, sid, pasid,
					    pasid_bits, &ste);
	}
	smmu_domain->smmu = smmu;
	/* We don't update STEs for pasid domains. */
	if (ret || pasid)
		goto out_unlock;

	/*
	 * The SMMU may cache a disabled STE.
	 * Initialize all fields, sync, then enable it.
	 */
	for (i = 1; i < STRTAB_STE_DWORDS; i++)
		dst->data[i] = ste.data[i];

	ret = smmu_sync_ste(smmu, sid);
	if (ret)
		goto out_unlock;

	WRITE_ONCE(dst->data[0], ste.data[0]);
	ret = smmu_sync_ste(smmu, sid);
	WARN_ON(ret);
out_unlock:
	kvm_iommu_unlock(iommu);
	return ret;
}

static int smmu_detach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid)
{
	struct arm_smmu_ste *dst;
	int i, ret;
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	u32 pasid_bits = 0;
	u64 *cd_table, *cd;

	kvm_iommu_lock(iommu);
	dst = smmu_get_ste_ptr(smmu, sid);
	if (!dst) {
		ret = -ENODEV;
		goto out_unlock;
	}

	/*
	 * For stage-1:
	 * - The kernel has to detach pasid = 0 the last.
	 * - This will free the CD.
	 */
	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S1) {
		pasid_bits = FIELD_GET(STRTAB_STE_0_S1CDMAX, dst->data[0]);
		if (pasid >= (1 << pasid_bits)) {
			ret = -E2BIG;
			goto out_unlock;
		}
		cd_table = (u64 *)(dst->data[0] & STRTAB_STE_0_S1CTXPTR_MASK);
		if (WARN_ON(!cd_table)) {
			ret = -ENODEV;
			goto out_unlock;
		}

		cd_table = hyp_phys_to_virt((phys_addr_t)cd_table);
		if (pasid == 0) {
			int j;

			/* Ensure other pasids are detached. */
			for (j = 1 ; j < (1 << pasid_bits) ; ++j) {
				cd = smmu_get_cd_ptr(cd_table, j);
				if (cd[0] & CTXDESC_CD_0_V) {
					ret = -EINVAL;
					goto out_unlock;
				}
			}
		} else {
			cd = smmu_get_cd_ptr(cd_table, pasid);
			cd[0] = 0;
			smmu_sync_cd(smmu, sid, pasid);
			cd[1] = 0;
			cd[2] = 0;
			cd[3] = 0;
			ret = smmu_sync_cd(smmu, sid, pasid);
			goto out_unlock;
		}
	}
	/* For stage-2 and pasid = 0 */
	dst->data[0] = 0;
	ret = smmu_sync_ste(smmu, sid);
	if (ret)
		goto out_unlock;
	for (i = 1; i < STRTAB_STE_DWORDS; i++)
		dst->data[i] = 0;

	ret = smmu_sync_ste(smmu, sid);

	smmu_free_cd(cd_table, pasid_bits);

out_unlock:
	kvm_iommu_unlock(iommu);
	return ret;
}

/* Shared with the kernel driver in EL1 */
struct kvm_iommu_ops smmu_ops = {
	.init				= smmu_init,
	.get_iommu_by_id		= smmu_id_to_iommu,
	.alloc_domain			= smmu_alloc_domain,
	.free_domain			= smmu_free_domain,
	.iotlb_sync			= smmu_iotlb_sync,
	.attach_dev			= smmu_attach_dev,
	.detach_dev			= smmu_detach_dev,
};
