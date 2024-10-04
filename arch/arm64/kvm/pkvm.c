// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 - Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/io.h>
#include <linux/kmemleak.h>
#include <linux/kvm_host.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/sort.h>
#include <linux/stat.h>

#include <asm/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/setup.h>

#include <uapi/linux/mount.h>
#include <linux/init_syscalls.h>

#include "hyp_constants.h"
#include "kvm_ptdump.h"
#include "hyp_trace.h"

DEFINE_STATIC_KEY_FALSE(kvm_protected_mode_initialized);

static struct reserved_mem *pkvm_firmware_mem;
static phys_addr_t *pvmfw_base = &kvm_nvhe_sym(pvmfw_base);
static phys_addr_t *pvmfw_size = &kvm_nvhe_sym(pvmfw_size);

static struct pkvm_moveable_reg *moveable_regs = kvm_nvhe_sym(pkvm_moveable_regs);
static struct memblock_region *hyp_memory = kvm_nvhe_sym(hyp_memory);
static unsigned int *hyp_memblock_nr_ptr = &kvm_nvhe_sym(hyp_memblock_nr);

phys_addr_t hyp_mem_base;
phys_addr_t hyp_mem_size;

static int cmp_hyp_memblock(const void *p1, const void *p2)
{
	const struct memblock_region *r1 = p1;
	const struct memblock_region *r2 = p2;

	return r1->base < r2->base ? -1 : (r1->base > r2->base);
}

static void __init sort_memblock_regions(void)
{
	sort(hyp_memory,
	     *hyp_memblock_nr_ptr,
	     sizeof(struct memblock_region),
	     cmp_hyp_memblock,
	     NULL);
}

static int __init register_memblock_regions(void)
{
	struct memblock_region *reg;

	for_each_mem_region(reg) {
		if (*hyp_memblock_nr_ptr >= HYP_MEMBLOCK_REGIONS)
			return -ENOMEM;

		hyp_memory[*hyp_memblock_nr_ptr] = *reg;
		(*hyp_memblock_nr_ptr)++;
	}
	sort_memblock_regions();

	return 0;
}

static int cmp_moveable_reg(const void *p1, const void *p2)
{
	const struct pkvm_moveable_reg *r1 = p1;
	const struct pkvm_moveable_reg *r2 = p2;

	/*
	 * Moveable regions may overlap, so put the largest one first when start
	 * addresses are equal to allow a simpler walk from e.g.
	 * host_stage2_unmap_unmoveable_regs().
	 */
	if (r1->start < r2->start)
		return -1;
	else if (r1->start > r2->start)
		return 1;
	else if (r1->size > r2->size)
		return -1;
	else if (r1->size < r2->size)
		return 1;
	return 0;
}

static void __init sort_moveable_regs(void)
{
	sort(moveable_regs,
	     kvm_nvhe_sym(pkvm_moveable_regs_nr),
	     sizeof(struct pkvm_moveable_reg),
	     cmp_moveable_reg,
	     NULL);
}

static int __init register_moveable_regions(void)
{
	struct memblock_region *reg;
	struct device_node *np;
	int i = 0;

	for_each_mem_region(reg) {
		if (i >= PKVM_NR_MOVEABLE_REGS)
			return -ENOMEM;
		moveable_regs[i].start = reg->base;
		moveable_regs[i].size = reg->size;
		moveable_regs[i].type = PKVM_MREG_MEMORY;
		i++;
	}

	for_each_compatible_node(np, NULL, "pkvm,protected-region") {
		struct resource res;
		u64 start, size;
		int ret;

		if (i >= PKVM_NR_MOVEABLE_REGS)
			return -ENOMEM;

		ret = of_address_to_resource(np, 0, &res);
		if (ret)
			return ret;

		start = res.start;
		size = resource_size(&res);
		if (!PAGE_ALIGNED(start) || !PAGE_ALIGNED(size))
			return -EINVAL;

		moveable_regs[i].start = start;
		moveable_regs[i].size = size;
		moveable_regs[i].type = PKVM_MREG_PROTECTED_RANGE;
		i++;
	}

	kvm_nvhe_sym(pkvm_moveable_regs_nr) = i;
	sort_moveable_regs();

	return 0;
}

void __init kvm_hyp_reserve(void)
{
	u64 hyp_mem_pages = 0;
	int ret;

	if (!is_hyp_mode_available() || is_kernel_in_hyp_mode())
		return;

	if (kvm_get_mode() != KVM_MODE_PROTECTED)
		return;

	ret = register_memblock_regions();
	if (ret) {
		*hyp_memblock_nr_ptr = 0;
		kvm_err("Failed to register hyp memblocks: %d\n", ret);
		return;
	}

	ret = register_moveable_regions();
	if (ret) {
		*hyp_memblock_nr_ptr = 0;
		kvm_err("Failed to register pkvm moveable regions: %d\n", ret);
		return;
	}

	hyp_mem_pages += hyp_s1_pgtable_pages();
	hyp_mem_pages += host_s2_pgtable_pages();
	hyp_mem_pages += hyp_vm_table_pages();
	hyp_mem_pages += hyp_vmemmap_pages(STRUCT_HYP_PAGE_SIZE);
	hyp_mem_pages += hyp_ffa_proxy_pages();

	/*
	 * Try to allocate a PMD-aligned region to reduce TLB pressure once
	 * this is unmapped from the host stage-2, and fallback to PAGE_SIZE.
	 */
	hyp_mem_size = hyp_mem_pages << PAGE_SHIFT;
	hyp_mem_base = memblock_phys_alloc(ALIGN(hyp_mem_size, PMD_SIZE),
					   PMD_SIZE);
	if (!hyp_mem_base)
		hyp_mem_base = memblock_phys_alloc(hyp_mem_size, PAGE_SIZE);
	else
		hyp_mem_size = ALIGN(hyp_mem_size, PMD_SIZE);

	if (!hyp_mem_base) {
		kvm_err("Failed to reserve hyp memory\n");
		return;
	}

	kvm_info("Reserved %lld MiB at 0x%llx\n", hyp_mem_size >> 20,
		 hyp_mem_base);
}

static int __pkvm_create_hyp_vcpu(struct kvm *host_kvm, struct kvm_vcpu *host_vcpu, unsigned long idx)
{
	pkvm_handle_t handle = host_kvm->arch.pkvm.handle;
	struct kvm_hyp_req *hyp_reqs;
	int ret;

	init_hyp_stage2_memcache(&host_vcpu->arch.stage2_mc);

	/* Indexing of the vcpus to be sequential starting at 0. */
	if (WARN_ON(host_vcpu->vcpu_idx != idx))
		return -EINVAL;

	hyp_reqs = (struct kvm_hyp_req *)__get_free_page(GFP_KERNEL_ACCOUNT);
	if (!hyp_reqs)
		return -ENOMEM;

	ret = kvm_share_hyp(hyp_reqs, hyp_reqs + 1);
	if (ret)
		goto err_free_reqs;
	host_vcpu->arch.hyp_reqs = hyp_reqs;

	ret = kvm_call_refill_hyp_nvhe(__pkvm_init_vcpu,
				       handle, host_vcpu);
	if (!ret)
		return 0;

	kvm_unshare_hyp(hyp_reqs, hyp_reqs + 1);
err_free_reqs:
	free_page((unsigned long)hyp_reqs);
	host_vcpu->arch.hyp_reqs = NULL;

	return ret;
}

static void __pkvm_vcpu_hyp_created(struct kvm_vcpu *vcpu)
{
	if (kvm_vm_is_protected(vcpu->kvm))
		vcpu->arch.sve_state = NULL;
}

/*
 * Handle broken down huge pages which have not been reported to the
 * kvm_pinned_page.
 */
int pkvm_call_hyp_nvhe_ppage(struct kvm_pinned_page *ppage,
			     int (*call_hyp_nvhe)(u64 pfn, u64 gfn, u8 order, void* args),
			     void *args, bool unmap)
{
	size_t page_size, size = PAGE_SIZE << ppage->order;
	u64 pfn = page_to_pfn(ppage->page);
	u8 order = ppage->order;
	u64 gfn = ppage->ipa >> PAGE_SHIFT;

	/* We already know this huge-page has been broken down in the stage-2 */
	if (ppage->pins < (1 << order))
		order = 0;

	while (size) {
		int err = call_hyp_nvhe(pfn, gfn, order, args);

		switch (err) {
		/* The stage-2 huge page has been broken down */
		case -E2BIG:
			if (order)
				order = 0;
			else
				/* Something is really wrong ... */
				return -EINVAL;
			break;
		/* This has been unmapped already */
		case -ENOENT:
			/*
			 * We are not supposed to lose track of PAGE_SIZE pinned
			 * page.
			 */
			if (!ppage->order)
				return -EINVAL;

			fallthrough;
		case 0:
			page_size = PAGE_SIZE << order;
			gfn += 1 << order;
			pfn += 1 << order;

			if (page_size > size)
				return -EINVAL;

			/* If -ENOENT, pins was already dropped. */
			if (unmap && !err)
				ppage->pins -= 1 << order;

			if (!ppage->pins)
				return 0;

			size -= page_size;
			break;
		default:
			return err;
		}
	}

	return 0;
}

static int __reclaim_dying_guest_page_call(u64 pfn, u64 gfn, u8 order, void *args)
{
	struct kvm *host_kvm = args;

	return kvm_call_hyp_nvhe(__pkvm_reclaim_dying_guest_page,
				 host_kvm->arch.pkvm.handle,
				 pfn, gfn, order);
}

static void __pkvm_destroy_hyp_vm(struct kvm *host_kvm)
{
	struct mm_struct *mm = current->mm;
	struct kvm_pinned_page *ppage;
	struct kvm_vcpu *host_vcpu;
	unsigned long idx, ipa = 0;

	if (!host_kvm->arch.pkvm.handle)
		goto out_free;

	WARN_ON(kvm_call_hyp_nvhe(__pkvm_start_teardown_vm, host_kvm->arch.pkvm.handle));

	mt_for_each(&host_kvm->arch.pkvm.pinned_pages, ppage, ipa, ULONG_MAX) {
		WARN_ON(pkvm_call_hyp_nvhe_ppage(ppage,
						 __reclaim_dying_guest_page_call,
						 host_kvm, true));
		cond_resched();

		account_locked_vm(mm, 1, false);
		unpin_user_pages_dirty_lock(&ppage->page, 1, host_kvm->arch.pkvm.enabled);
		kfree(ppage);
	}
	mtree_destroy(&host_kvm->arch.pkvm.pinned_pages);

	WARN_ON(kvm_call_hyp_nvhe(__pkvm_finalize_teardown_vm, host_kvm->arch.pkvm.handle));

out_free:
	host_kvm->arch.pkvm.handle = 0;

	atomic64_sub(host_kvm->arch.pkvm.stage2_teardown_mc.nr_pages << PAGE_SHIFT,
		     &host_kvm->stat.protected_hyp_mem);
	atomic64_sub(host_kvm->arch.pkvm.stage2_teardown_mc.nr_pages << PAGE_SHIFT,
		     &host_kvm->stat.protected_pgtable_mem);
	free_hyp_memcache(&host_kvm->arch.pkvm.stage2_teardown_mc);

	kvm_for_each_vcpu(idx, host_vcpu, host_kvm) {
		struct kvm_hyp_req *hyp_reqs = host_vcpu->arch.hyp_reqs;

		if (!hyp_reqs)
			continue;

		kvm_unshare_hyp(hyp_reqs, hyp_reqs + 1);
		host_vcpu->arch.hyp_reqs = NULL;
		free_page((unsigned long)hyp_reqs);
	}
}

/*
 * Allocates and donates memory for hypervisor VM structs at EL2.
 *
 * Allocates space for the VM state, which includes the hyp vm as well as
 * the hyp vcpus.
 *
 * Stores an opaque handler in the kvm struct for future reference.
 *
 * Return 0 on success, negative error code on failure.
 */
static int __pkvm_create_hyp_vm(struct kvm *host_kvm)
{
	struct kvm_vcpu *host_vcpu;
	pkvm_handle_t handle;
	unsigned long idx;
	size_t pgd_sz;
	void *pgd;
	int ret;

	if (host_kvm->created_vcpus < 1)
		return -EINVAL;

	pgd_sz = kvm_pgtable_stage2_pgd_size(host_kvm->arch.vtcr);

	/*
	 * The PGD pages will be reclaimed using a hyp_memcache which implies
	 * page granularity. So, use alloc_pages_exact() to get individual
	 * refcounts.
	 */
	pgd = alloc_pages_exact(pgd_sz, GFP_KERNEL_ACCOUNT);
	if (!pgd)
		return -ENOMEM;
	atomic64_add(pgd_sz, &host_kvm->stat.protected_hyp_mem);

	init_hyp_stage2_memcache(&host_kvm->arch.pkvm.stage2_teardown_mc);

	/* Donate the VM memory to hyp and let hyp initialize it. */
	ret = kvm_call_refill_hyp_nvhe(__pkvm_init_vm, host_kvm, pgd);
	if (ret < 0)
		goto free_pgd;

	handle = ret;

	host_kvm->arch.pkvm.handle = handle;

	/* Donate memory for the vcpus at hyp and initialize it. */
	kvm_for_each_vcpu(idx, host_vcpu, host_kvm) {
		ret = __pkvm_create_hyp_vcpu(host_kvm, host_vcpu, idx);
		if (ret)
			goto destroy_vm;
		__pkvm_vcpu_hyp_created(host_vcpu);
	}

	atomic64_set(&host_kvm->stat.protected_pgtable_mem, pgd_sz);
	kvm_account_pgtable_pages(pgd, pgd_sz >> PAGE_SHIFT);

	return 0;

destroy_vm:
	__pkvm_destroy_hyp_vm(host_kvm);
	return ret;
free_pgd:
	free_pages_exact(pgd, pgd_sz);
	atomic64_sub(pgd_sz, &host_kvm->stat.protected_hyp_mem);

	return ret;
}

int pkvm_create_hyp_vm(struct kvm *host_kvm)
{
	int ret = 0;

	mutex_lock(&host_kvm->arch.config_lock);
	if (!host_kvm->arch.pkvm.handle)
		ret = __pkvm_create_hyp_vm(host_kvm);
	mutex_unlock(&host_kvm->arch.config_lock);

	return ret;
}

void pkvm_destroy_hyp_vm(struct kvm *host_kvm)
{
	mutex_lock(&host_kvm->arch.config_lock);
	__pkvm_destroy_hyp_vm(host_kvm);
	mutex_unlock(&host_kvm->arch.config_lock);
}

int pkvm_init_host_vm(struct kvm *host_kvm, unsigned long type)
{
	if (!(type & KVM_VM_TYPE_ARM_PROTECTED))
		return 0;

	if (!is_protected_kvm_enabled())
		return -EINVAL;

	host_kvm->arch.pkvm.pvmfw_load_addr = PVMFW_INVALID_LOAD_ADDR;
	host_kvm->arch.pkvm.enabled = true;
	return 0;
}

static void __init _kvm_host_prot_finalize(void *arg)
{
	int *err = arg;

	if (WARN_ON(kvm_call_hyp_nvhe(__pkvm_prot_finalize)))
		WRITE_ONCE(*err, -EINVAL);
}

static int __init pkvm_drop_host_privileges(void)
{
	int ret = 0;

	/*
	 * Flip the static key upfront as that may no longer be possible
	 * once the host stage 2 is installed.
	 */
	static_branch_enable(&kvm_protected_mode_initialized);
	on_each_cpu(_kvm_host_prot_finalize, &ret, 1);
	return ret;
}

static int __init pkvm_firmware_rmem_clear(void);

static int __init finalize_pkvm(void)
{
	int ret;

	if (!is_protected_kvm_enabled() || !is_kvm_arm_initialised()) {
		pkvm_firmware_rmem_clear();
		return 0;
	}

	/*
	 * Modules can play an essential part in the pKVM protection. All of
	 * them must properly load to enable protected VMs.
	 */
	if (pkvm_load_early_modules())
		pkvm_firmware_rmem_clear();

	ret = kvm_iommu_init_driver();
	if (ret) {
		pr_err("Failed to init KVM IOMMU driver: %d\n", ret);
		pkvm_firmware_rmem_clear();
	}

	/*
	 * Exclude HYP sections from kmemleak so that they don't get peeked
	 * at, which would end badly once inaccessible.
	 */
	kmemleak_free_part(__hyp_bss_start, __hyp_bss_end - __hyp_bss_start);
	kmemleak_free_part(__hyp_data_start, __hyp_data_end - __hyp_data_start);
	kmemleak_free_part(__hyp_rodata_start, __hyp_rodata_end - __hyp_rodata_start);
	kmemleak_free_part_phys(hyp_mem_base, hyp_mem_size);

	kvm_ptdump_host_register();

	ret = pkvm_drop_host_privileges();
	if (ret) {
		pr_err("Failed to finalize Hyp protection: %d\n", ret);
		BUG();
	}

	return 0;
}
device_initcall_sync(finalize_pkvm);

void pkvm_host_reclaim_page(struct kvm *host_kvm, phys_addr_t ipa)
{
	struct mm_struct *mm = current->mm;
	struct kvm_pinned_page *ppage;
	unsigned long index = ipa;

	write_lock(&host_kvm->mmu_lock);
	ppage = mt_find(&host_kvm->arch.pkvm.pinned_pages, &index,
			index + PAGE_SIZE - 1);
	if (ppage) {
		if (ppage->pins)
			ppage->pins--;
		else
			WARN_ON(1);

		if (!ppage->pins)
			mtree_erase(&host_kvm->arch.pkvm.pinned_pages, ipa);
	}
	write_unlock(&host_kvm->mmu_lock);

	WARN_ON(!ppage);
	if (!ppage || ppage->pins)
		return;

	account_locked_vm(mm, 1, false);
	unpin_user_pages_dirty_lock(&ppage->page, 1, host_kvm->arch.pkvm.enabled);
	kfree(ppage);
}

static int __init pkvm_firmware_rmem_err(struct reserved_mem *rmem,
					 const char *reason)
{
	phys_addr_t end = rmem->base + rmem->size;

	kvm_err("Ignoring pkvm guest firmware memory reservation [%pa - %pa]: %s\n",
		&rmem->base, &end, reason);
	return -EINVAL;
}

static int __init pkvm_firmware_rmem_init(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;

	if (pkvm_firmware_mem)
		return pkvm_firmware_rmem_err(rmem, "duplicate reservation");

	if (!of_get_flat_dt_prop(node, "no-map", NULL))
		return pkvm_firmware_rmem_err(rmem, "missing \"no-map\" property");

	if (of_get_flat_dt_prop(node, "reusable", NULL))
		return pkvm_firmware_rmem_err(rmem, "\"reusable\" property unsupported");

	if (!PAGE_ALIGNED(rmem->base))
		return pkvm_firmware_rmem_err(rmem, "base is not page-aligned");

	if (!PAGE_ALIGNED(rmem->size))
		return pkvm_firmware_rmem_err(rmem, "size is not page-aligned");

	*pvmfw_size = rmem->size;
	*pvmfw_base = rmem->base;
	pkvm_firmware_mem = rmem;
	return 0;
}
RESERVEDMEM_OF_DECLARE(pkvm_firmware, "linux,pkvm-guest-firmware-memory",
		       pkvm_firmware_rmem_init);

static int __init pkvm_firmware_rmem_clear(void)
{
	void *addr;
	phys_addr_t size;

	if (likely(!pkvm_firmware_mem))
		return 0;

	kvm_info("Clearing pKVM firmware memory\n");
	size = pkvm_firmware_mem->size;
	addr = memremap(pkvm_firmware_mem->base, size, MEMREMAP_WB);
	if (!addr)
		return -EINVAL;

	memset(addr, 0, size);
	dcache_clean_poc((unsigned long)addr, (unsigned long)addr + size);
	memunmap(addr);
	return 0;
}

static int pkvm_vm_ioctl_set_fw_ipa(struct kvm *kvm, u64 ipa)
{
	int ret = 0;

	if (!pkvm_firmware_mem)
		return -EINVAL;

	mutex_lock(&kvm->lock);
	if (kvm->arch.pkvm.handle) {
		ret = -EBUSY;
		goto out_unlock;
	}

	kvm->arch.pkvm.pvmfw_load_addr = ipa;
out_unlock:
	mutex_unlock(&kvm->lock);
	return ret;
}

static int pkvm_vm_ioctl_info(struct kvm *kvm,
			      struct kvm_protected_vm_info __user *info)
{
	struct kvm_protected_vm_info kinfo = {
		.firmware_size = pkvm_firmware_mem ?
				 pkvm_firmware_mem->size :
				 0,
	};

	return copy_to_user(info, &kinfo, sizeof(kinfo)) ? -EFAULT : 0;
}

int pkvm_vm_ioctl_enable_cap(struct kvm *kvm, struct kvm_enable_cap *cap)
{
	if (!kvm_vm_is_protected(kvm))
		return -EINVAL;

	if (cap->args[1] || cap->args[2] || cap->args[3])
		return -EINVAL;

	switch (cap->flags) {
	case KVM_CAP_ARM_PROTECTED_VM_FLAGS_SET_FW_IPA:
		return pkvm_vm_ioctl_set_fw_ipa(kvm, cap->args[0]);
	case KVM_CAP_ARM_PROTECTED_VM_FLAGS_INFO:
		return pkvm_vm_ioctl_info(kvm, (void __force __user *)cap->args[0]);
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_MODULES
static char early_pkvm_modules[COMMAND_LINE_SIZE] __initdata;

static int __init early_pkvm_modules_cfg(char *arg)
{
	/*
	 * Loading pKVM modules with kvm-arm.protected_modules is deprecated
	 * Use kvm-arm.protected_modules=<module1>,<module2>
	 */
	if (!arg)
		return -EINVAL;

	strscpy(early_pkvm_modules, arg, COMMAND_LINE_SIZE);

	return 0;
}
early_param("kvm-arm.protected_modules", early_pkvm_modules_cfg);

static void free_modprobe_argv(struct subprocess_info *info)
{
	kfree(info->argv);
}

/*
 * Heavily inspired by request_module(). The latest couldn't be reused though as
 * the feature can be disabled depending on umh configuration. Here some
 * security is enforced by making sure this can be called only when pKVM is
 * enabled, not yet completely initialized.
 */
static int __init __pkvm_request_early_module(char *module_name,
					      char *module_path)
{
	char *modprobe_path = CONFIG_MODPROBE_PATH;
	struct subprocess_info *info;
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
		NULL
	};
	static bool proc;
	char **argv;
	int idx = 0;

	if (!is_protected_kvm_enabled())
		return -EACCES;

	if (static_branch_likely(&kvm_protected_mode_initialized))
		return -EACCES;

	argv = kmalloc(sizeof(char *) * 7, GFP_KERNEL);
	if (!argv)
		return -ENOMEM;

	argv[idx++] = modprobe_path;
	argv[idx++] = "-q";
	if (*module_path != '\0') {
		argv[idx++] = "-d";
		argv[idx++] = module_path;
	}
	argv[idx++] = "--";
	argv[idx++] = module_name;
	argv[idx++] = NULL;

	info = call_usermodehelper_setup(modprobe_path, argv, envp, GFP_KERNEL,
					 NULL, free_modprobe_argv, NULL);
	if (!info)
		goto err;

	/* Even with CONFIG_STATIC_USERMODEHELPER we really want this path */
	info->path = modprobe_path;

	if (!proc) {
		wait_for_initramfs();
		if (init_mount("proc", "/proc", "proc",
			       MS_SILENT | MS_NOEXEC | MS_NOSUID, NULL))
			pr_warn("Couldn't mount /proc, pKVM module parameters will be ignored\n");

		proc = true;
	}

	return call_usermodehelper_exec(info, UMH_WAIT_PROC | UMH_KILLABLE);
err:
	kfree(argv);

	return -ENOMEM;
}

static int __init pkvm_request_early_module(char *module_name, char *module_path)
{
	int err = __pkvm_request_early_module(module_name, module_path);

	if (!err)
		return 0;

	/* Already tried the default path */
	if (*module_path == '\0')
		return err;

	pr_info("loading %s from %s failed, fallback to the default path\n",
		module_name, module_path);

	return __pkvm_request_early_module(module_name, "");
}

int __init pkvm_load_early_modules(void)
{
	char *token, *buf = early_pkvm_modules;
	char *module_path = CONFIG_PKVM_MODULE_PATH;
	int err;

	while (true) {
		token = strsep(&buf, ",");

		if (!token)
			break;

		if (*token) {
			err = pkvm_request_early_module(token, module_path);
			if (err) {
				pr_err("Failed to load pkvm module %s: %d\n",
				       token, err);
				return err;
			}
			/* Do it every iteration to iron out the dependencies. */
			flush_deferred_probe_now();
		}

		if (buf)
			*(buf - 1) = ',';
	}

	return 0;
}

#ifdef CONFIG_PROTECTED_NVHE_STACKTRACE
static LIST_HEAD(pkvm_modules);

static void pkvm_el2_mod_add(struct pkvm_el2_module *mod)
{
	INIT_LIST_HEAD(&mod->node);
	list_add(&mod->node, &pkvm_modules);
}

unsigned long pkvm_el2_mod_kern_va(unsigned long addr)
{
	struct pkvm_el2_module *mod;

	list_for_each_entry(mod, &pkvm_modules, node) {
		size_t len = (unsigned long)mod->sections.end -
			     (unsigned long)mod->sections.start;

		if (addr >= (unsigned long)mod->token &&
		    addr < (unsigned long)mod->token + len)
			return (unsigned long)mod->sections.start +
				(addr - mod->token);
	}

	return 0;
}
#else
static void pkvm_el2_mod_add(struct pkvm_el2_module *mod) { }
unsigned long pkvm_el2_mod_kern_va(unsigned long addr) { return 0; }
#endif

struct pkvm_mod_sec_mapping {
	struct pkvm_module_section *sec;
	enum kvm_pgtable_prot prot;
};

static void pkvm_unmap_module_pages(void *kern_va, void *hyp_va, size_t size)
{
	size_t offset;
	u64 pfn;

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		pfn = vmalloc_to_pfn(kern_va + offset);
		kvm_call_hyp_nvhe(__pkvm_unmap_module_page, pfn,
				  hyp_va + offset);
	}
}

static void pkvm_unmap_module_sections(struct pkvm_mod_sec_mapping *secs_map, void *hyp_va_base, int nr_secs)
{
	size_t offset, size;
	void *start;
	int i;

	for (i = 0; i < nr_secs; i++) {
		start = secs_map[i].sec->start;
		size = secs_map[i].sec->end - start;
		offset = start - secs_map[0].sec->start;
		pkvm_unmap_module_pages(start, hyp_va_base + offset, size);
	}
}

static int pkvm_map_module_section(struct pkvm_mod_sec_mapping *sec_map, void *hyp_va)
{
	size_t offset, size = sec_map->sec->end - sec_map->sec->start;
	int ret;
	u64 pfn;

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		pfn = vmalloc_to_pfn(sec_map->sec->start + offset);
		ret = kvm_call_hyp_nvhe(__pkvm_map_module_page, pfn,
					hyp_va + offset, sec_map->prot);
		if (ret) {
			pkvm_unmap_module_pages(sec_map->sec->start, hyp_va, offset);
			return ret;
		}
	}

	return 0;
}

static int pkvm_map_module_sections(struct pkvm_mod_sec_mapping *secs_map, void *hyp_va_base, int nr_secs)
{
	size_t offset;
	int i, ret;

	for (i = 0; i < nr_secs; i++) {
		offset = secs_map[i].sec->start - secs_map[0].sec->start;
		ret = pkvm_map_module_section(&secs_map[i], hyp_va_base + offset);
		if (ret) {
			pkvm_unmap_module_sections(secs_map, hyp_va_base, i);
			return ret;
		}
	}

	return 0;
}
static int __pkvm_cmp_mod_sec(const void *p1, const void *p2)
{
	struct pkvm_mod_sec_mapping const *s1 = p1;
	struct pkvm_mod_sec_mapping const *s2 = p2;

	return s1->sec->start < s2->sec->start ? -1 : s1->sec->start > s2->sec->start;
}

int __pkvm_load_el2_module(struct module *this, unsigned long *token)
{
	struct pkvm_el2_module *mod = &this->arch.hyp;
	struct pkvm_mod_sec_mapping secs_map[] = {
		{ &mod->text, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_X },
		{ &mod->bss, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_W },
		{ &mod->rodata, KVM_PGTABLE_PROT_R },
		{ &mod->event_ids, KVM_PGTABLE_PROT_R },
		{ &mod->data, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_W },
	};
	void *start, *end, *hyp_va;
	struct arm_smccc_res res;
	kvm_nvhe_reloc_t *endrel;
	int ret, i, secs_first;
	size_t offset, size;

	/* The pKVM hyp only allows loading before it is fully initialized */
	if (!is_protected_kvm_enabled() || is_pkvm_initialized())
		return -EOPNOTSUPP;

	for (i = 0; i < ARRAY_SIZE(secs_map); i++) {
		if (!PAGE_ALIGNED(secs_map[i].sec->start)) {
			kvm_err("EL2 sections are not page-aligned\n");
			return -EINVAL;
		}
	}

	if (!try_module_get(this)) {
		kvm_err("Kernel module has been unloaded\n");
		return -ENODEV;
	}

	/* Missing or empty module sections are placed first */
	sort(secs_map, ARRAY_SIZE(secs_map), sizeof(secs_map[0]), __pkvm_cmp_mod_sec, NULL);
	for (secs_first = 0; secs_first < ARRAY_SIZE(secs_map); secs_first++) {
		start = secs_map[secs_first].sec->start;
		if (start)
			break;
	}
	end = secs_map[ARRAY_SIZE(secs_map) - 1].sec->end;
	size = end - start;

	arm_smccc_1_1_hvc(KVM_HOST_SMCCC_FUNC(__pkvm_alloc_module_va),
			  size >> PAGE_SHIFT, &res);
	if (res.a0 != SMCCC_RET_SUCCESS || !res.a1) {
		kvm_err("Failed to allocate hypervisor VA space for EL2 module\n");
		module_put(this);
		return res.a0 == SMCCC_RET_SUCCESS ? -ENOMEM : -EPERM;
	}
	hyp_va = (void *)res.a1;

	/*
	 * The token can be used for other calls related to this module.
	 * Conveniently the only information needed is this addr so let's use it
	 * as an identifier.
	 */
	if (token)
		*token = (unsigned long)hyp_va;

	mod->token = (unsigned long)hyp_va;
	mod->sections.start = start;
	mod->sections.end = end;

	endrel = (void *)mod->relocs + mod->nr_relocs * sizeof(*endrel);
	kvm_apply_hyp_module_relocations(mod, mod->relocs, endrel);

	ret = hyp_trace_init_mod_events(mod->hyp_events,
					mod->event_ids.start,
					mod->nr_hyp_events);
	if (ret)
		kvm_err("Failed to init module events: %d\n", ret);

	/*
	 * Sadly we have also to disable kmemleak for EL1 sections: we can't
	 * reset created scan area and therefore we can't create a finer grain
	 * scan excluding only EL2 sections.
	 */
	if (this) {
		kmemleak_no_scan(this->mem[MOD_TEXT].base);
		kmemleak_no_scan(this->mem[MOD_DATA].base);
		kmemleak_no_scan(this->mem[MOD_RODATA].base);
	}

	ret = pkvm_map_module_sections(secs_map + secs_first, hyp_va,
				       ARRAY_SIZE(secs_map) - secs_first);
	if (ret) {
		kvm_err("Failed to map EL2 module page: %d\n", ret);
		module_put(this);
		return ret;
	}

	offset = (size_t)((void *)mod->init - start);
	ret = kvm_call_hyp_nvhe(__pkvm_init_module, hyp_va + offset);
	if (ret) {
		kvm_err("Failed to init EL2 module: %d\n", ret);
		pkvm_unmap_module_sections(secs_map, hyp_va, ARRAY_SIZE(secs_map));
		module_put(this);
		return ret;
	}

	pkvm_el2_mod_add(mod);

	return 0;
}
EXPORT_SYMBOL(__pkvm_load_el2_module);

int __pkvm_register_el2_call(unsigned long hfn_hyp_va)
{
	return kvm_call_hyp_nvhe(__pkvm_register_hcall, hfn_hyp_va);
}
EXPORT_SYMBOL(__pkvm_register_el2_call);
#endif /* CONFIG_MODULES */

int __pkvm_topup_hyp_alloc_mgt_gfp(unsigned long id, unsigned long nr_pages,
				   unsigned long sz_alloc, gfp_t gfp)
{
	struct kvm_hyp_memcache mc;
	int ret;

	init_hyp_memcache(&mc);

	ret = topup_hyp_memcache_gfp(&mc, nr_pages, get_order(sz_alloc), gfp);
	if (ret)
		return ret;

	ret = kvm_call_hyp_nvhe(__pkvm_hyp_alloc_mgt_refill, id,
				mc.head, mc.nr_pages);
	if (ret)
		free_hyp_memcache(&mc);

	return ret;
}
EXPORT_SYMBOL(__pkvm_topup_hyp_alloc_mgt_gfp);

int __pkvm_topup_hyp_alloc_mgt(unsigned long id, unsigned long nr_pages, unsigned long sz_alloc)
{
	return __pkvm_topup_hyp_alloc_mgt_gfp(id, nr_pages, sz_alloc, GFP_KERNEL);
}
EXPORT_SYMBOL(__pkvm_topup_hyp_alloc_mgt);

int __pkvm_topup_hyp_alloc(unsigned long nr_pages)
{
	return __pkvm_topup_hyp_alloc_mgt(HYP_ALLOC_MGT_HEAP_ID, nr_pages, PAGE_SIZE);
}
EXPORT_SYMBOL(__pkvm_topup_hyp_alloc);

unsigned long __pkvm_reclaim_hyp_alloc_mgt(unsigned long nr_pages)
{
	unsigned long ratelimit, last_reclaim, reclaimed = 0;
	struct kvm_hyp_memcache mc;
	struct arm_smccc_res res;

	init_hyp_memcache(&mc);

	do {
		/* Arbitrary upper bound to limit the time spent at EL2 */
		ratelimit = min(nr_pages, 16UL);

		arm_smccc_1_1_hvc(KVM_HOST_SMCCC_FUNC(__pkvm_hyp_alloc_mgt_reclaim),
				  ratelimit, &res);
		if (WARN_ON(res.a0 != SMCCC_RET_SUCCESS))
			break;

		mc.head = res.a1;
		last_reclaim = mc.nr_pages = res.a2;

		free_hyp_memcache(&mc);
		reclaimed += last_reclaim;

	} while (last_reclaim && (reclaimed < nr_pages));

	return reclaimed;
}
