/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GZVM_DRV_H__
#define __GZVM_DRV_H__

#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/gzvm.h>

/* GZVM version encode */
#define GZVM_DRV_MAJOR_VERSION		16
#define GZVM_DRV_MINOR_VERSION		0

struct gzvm_version {
	u32 major;
	u32 minor;
	u64 sub;	/* currently, used by hypervisor */
};

struct gzvm_driver {
	struct gzvm_version hyp_version;
	struct gzvm_version drv_version;
};

/*
 * For the normal physical address, the highest 12 bits should be zero, so we
 * can mask bit 62 ~ bit 52 to indicate the error physical address
 */
#define GZVM_PA_ERR_BAD (0x7ffULL << 52)

#define INVALID_VM_ID   0xffff

/*
 * These are the definitions of APIs between GenieZone hypervisor and driver,
 * there's no need to be visible to uapi. Furthermore, we need GenieZone
 * specific error code in order to map to Linux errno
 */
#define NO_ERROR                (0)
#define ERR_NO_MEMORY           (-5)
#define ERR_NOT_SUPPORTED       (-24)
#define ERR_NOT_IMPLEMENTED     (-27)
#define ERR_FAULT               (-40)

/*
 * The following data structures are for data transferring between driver and
 * hypervisor, and they're aligned with hypervisor definitions
 */
#define GZVM_MAX_MEM_REGION	10

/**
 * struct mem_region_addr_range: identical to ffa memory constituent
 * @address: the base IPA of the constituent memory region, aligned to 4 kiB
 * @pg_cnt: the number of 4 kiB pages in the constituent memory region
 * @reserved: reserved for 64bit alignment
 */
struct mem_region_addr_range {
	__u64 address;
	__u32 pg_cnt;
	__u32 reserved;
};

struct gzvm_memory_region_ranges {
	__u32 slot;
	__u32 constituent_cnt;
	__u64 total_pages;
	__u64 gpa;
	struct mem_region_addr_range constituents[];
};

/*
 * A reasonable and large enough limit for the maximum number of pages a
 * guest can use.
 */
#define GZVM_MEM_MAX_NR_PAGES		((1UL << 31) - 1)

/**
 * struct gzvm_memslot: VM's memory slot descriptor
 * @base_gfn: begin of guest page frame
 * @npages: number of pages this slot covers
 * @userspace_addr: corresponding userspace va
 * @vma: vma related to this userspace addr
 * @flags: define the usage of memory region. Ex. guest memory or
 * firmware protection
 * @slot_id: the id is used to identify the memory slot
 */
struct gzvm_memslot {
	u64 base_gfn;
	unsigned long npages;
	unsigned long userspace_addr;
	struct vm_area_struct *vma;
	u32 flags;
	u32 slot_id;
};

/**
 * struct gzvm: the following data structures are for data transferring between
 * driver and hypervisor, and they're aligned with hypervisor definitions.
 * @gzvm_drv: the data structure is used to keep driver's information
 * @mm: userspace tied to this vm
 * @memslot: VM's memory slot descriptor
 * @lock: lock for list_add
 * @vm_list: list head for vm list
 * @vm_id: vm id
 */
struct gzvm {
	struct gzvm_driver *gzvm_drv;
	struct mm_struct *mm;
	struct gzvm_memslot memslot[GZVM_MAX_MEM_REGION];
	struct mutex lock;
	struct list_head vm_list;
	u16 vm_id;
};

long gzvm_dev_ioctl_check_extension(struct gzvm *gzvm, unsigned long args);
int gzvm_dev_ioctl_create_vm(struct gzvm_driver *drv, unsigned long vm_type);

int gzvm_err_to_errno(unsigned long err);

void gzvm_destroy_all_vms(void);

/* arch-dependant functions */
int gzvm_arch_probe(struct gzvm_version drv_version,
		    struct gzvm_version *hyp_version);
int gzvm_arch_set_memregion(u16 vm_id, size_t buf_size,
			    phys_addr_t region);
int gzvm_arch_check_extension(struct gzvm *gzvm, __u64 cap, void __user *argp);
int gzvm_arch_create_vm(unsigned long vm_type);
int gzvm_arch_destroy_vm(u16 vm_id);
int gzvm_vm_ioctl_arch_enable_cap(struct gzvm *gzvm,
				  struct gzvm_enable_cap *cap,
				  void __user *argp);

int gzvm_gfn_to_hva_memslot(struct gzvm_memslot *memslot, u64 gfn,
			    u64 *hva_memslot);

#endif /* __GZVM_DRV_H__ */
