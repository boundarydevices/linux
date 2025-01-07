/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GZVM_DRV_H__
#define __GZVM_DRV_H__

#include <linux/eventfd.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/gzvm.h>
#include <linux/srcu.h>
#include <linux/rbtree.h>

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

	struct kobject *sysfs_root_dir;
	u32 demand_paging_batch_pages;
	u32 destroy_batch_pages;

	struct dentry *gzvm_debugfs_dir;
};

/*
 * For the normal physical address, the highest 12 bits should be zero, so we
 * can mask bit 62 ~ bit 52 to indicate the error physical address
 */
#define GZVM_PA_ERR_BAD (0x7ffULL << 52)

#define GZVM_VCPU_MMAP_SIZE  PAGE_SIZE
#define INVALID_VM_ID   0xffff

/*
 * These are the definitions of APIs between GenieZone hypervisor and driver,
 * there's no need to be visible to uapi. Furthermore, we need GenieZone
 * specific error code in order to map to Linux errno
 */
#define NO_ERROR                (0)
#define ERR_NO_MEMORY           (-5)
#define ERR_INVALID_ARGS        (-8)
#define ERR_NOT_SUPPORTED       (-24)
#define ERR_NOT_IMPLEMENTED     (-27)
#define ERR_BUSY                (-33)
#define ERR_FAULT               (-40)
#define GZVM_IRQFD_RESAMPLE_IRQ_SOURCE_ID       1

/*
 * The following data structures are for data transferring between driver and
 * hypervisor, and they're aligned with hypervisor definitions
 */
#define GZVM_MAX_VCPUS		 8
#define GZVM_MAX_MEM_REGION	10

#define GZVM_VCPU_RUN_MAP_SIZE			(PAGE_SIZE * 2)

#define GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE	(PMD_SIZE) /* 2MB */
#define GZVM_DRV_DEMAND_PAGING_BATCH_PAGES	\
	(GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE / PAGE_SIZE)
#define GZVM_DRV_DESTROY_PAGING_BATCH_PAGES	(128)

#define GZVM_MAX_DEBUGFS_DIR_NAME_SIZE  20
#define GZVM_MAX_DEBUGFS_VALUE_SIZE	20

enum gzvm_demand_paging_mode {
	GZVM_FULLY_POPULATED = 0,
	GZVM_DEMAND_PAGING = 1,
};

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

struct gzvm_vcpu {
	struct gzvm *gzvm;
	int vcpuid;
	/* lock of vcpu*/
	struct mutex lock;
	struct gzvm_vcpu_run *run;
	struct gzvm_vcpu_hwstate *hwstate;
	struct hrtimer gzvm_vtimer;
	struct {
		u32 vtimer_irq;
		u32 virtio_irq;
	} idle_events;
	struct rcuwait wait;
};

struct gzvm_pinned_page {
	struct rb_node node;
	struct page *page;
	u64 ipa;
};

struct gzvm_vm_stat {
	u64 protected_hyp_mem;
	u64 protected_shared_mem;
};

/**
 * struct gzvm: the following data structures are for data transferring between
 * driver and hypervisor, and they're aligned with hypervisor definitions.
 * @gzvm_drv: the data structure is used to keep driver's information
 * @vcpus: VM's cpu descriptors
 * @mm: userspace tied to this vm
 * @memslot: VM's memory slot descriptor
 * @lock: lock for list_add
 * @irqfds: the data structure is used to keep irqfds's information
 * @ioevents: list head for ioevents
 * @ioevent_lock: lock for ioevent list
 * @vm_list: list head for vm list
 * @vm_id: vm id
 * @irq_ack_notifier_list: list head for irq ack notifier
 * @irq_srcu: structure data for SRCU(sleepable rcu)
 * @irq_lock: lock for irq injection
 * @pinned_pages: use rb-tree to record pin/unpin page
 * @mem_lock: lock for memory operations
 * @mem_alloc_mode: memory allocation mode - fully allocated or demand paging
 * @demand_page_gran: demand page granularity: how much memory we allocate for
 * VM in a single page fault
 * @demand_page_buffer: the mailbox for transferring large portion pages
 * @demand_paging_lock: lock for preventing multiple cpu using the same demand
 * page mailbox at the same time
 * @stat: information for VM memory statistics
 * @debug_dir: debugfs directory node for VM memory statistics
 */
struct gzvm {
	struct gzvm_driver *gzvm_drv;
	struct gzvm_vcpu *vcpus[GZVM_MAX_VCPUS];
	struct mm_struct *mm;
	struct gzvm_memslot memslot[GZVM_MAX_MEM_REGION];
	struct mutex lock;

	struct {
		spinlock_t        lock;
		struct list_head  items;
		struct list_head  resampler_list;
		struct mutex      resampler_lock;
	} irqfds;

	struct list_head ioevents;
	struct mutex ioevent_lock;

	struct list_head vm_list;
	u16 vm_id;

	struct hlist_head irq_ack_notifier_list;
	struct srcu_struct irq_srcu;
	struct mutex irq_lock;
	u32 mem_alloc_mode;

	struct rb_root pinned_pages;
	struct mutex mem_lock;

	u32 demand_page_gran;
	u64 *demand_page_buffer;
	struct mutex  demand_paging_lock;

	struct gzvm_vm_stat stat;
	struct dentry *debug_dir;
};

long gzvm_dev_ioctl_check_extension(struct gzvm *gzvm, unsigned long args);
int gzvm_dev_ioctl_create_vm(struct gzvm_driver *drv, unsigned long vm_type);

int gzvm_err_to_errno(unsigned long err);

void gzvm_destroy_all_vms(void);

void gzvm_destroy_vcpus(struct gzvm *gzvm);

/* arch-dependant functions */
int gzvm_arch_probe(struct gzvm_version drv_version,
		    struct gzvm_version *hyp_version);
int gzvm_arch_query_hyp_batch_pages(struct gzvm_enable_cap *cap,
				    void __user *argp);
int gzvm_arch_query_destroy_batch_pages(struct gzvm_enable_cap *cap,
					void __user *argp);

int gzvm_arch_set_memregion(u16 vm_id, size_t buf_size,
			    phys_addr_t region);
int gzvm_arch_check_extension(struct gzvm *gzvm, __u64 cap, void __user *argp);
int gzvm_arch_create_vm(unsigned long vm_type);
int gzvm_arch_destroy_vm(u16 vm_id, u64 destroy_page_gran);
int gzvm_arch_map_guest(u16 vm_id, int memslot_id, u64 pfn, u64 gfn,
			u64 nr_pages);
int gzvm_arch_map_guest_block(u16 vm_id, int memslot_id, u64 gfn, u64 nr_pages);
int gzvm_arch_get_statistics(struct gzvm *gzvm);
int gzvm_vm_ioctl_arch_enable_cap(struct gzvm *gzvm,
				  struct gzvm_enable_cap *cap,
				  void __user *argp);

int gzvm_gfn_to_hva_memslot(struct gzvm_memslot *memslot, u64 gfn,
			    u64 *hva_memslot);
int gzvm_vm_populate_mem_region(struct gzvm *gzvm, int slot_id);
int gzvm_vm_allocate_guest_page(struct gzvm *gzvm, struct gzvm_memslot *slot,
				u64 gfn, u64 *pfn);

int gzvm_vm_ioctl_create_vcpu(struct gzvm *gzvm, u32 cpuid);
int gzvm_arch_vcpu_update_one_reg(struct gzvm_vcpu *vcpu, __u64 reg_id,
				  bool is_write, __u64 *data);
int gzvm_arch_drv_init(void);
int gzvm_arch_create_vcpu(u16 vm_id, int vcpuid, void *run);
int gzvm_arch_vcpu_run(struct gzvm_vcpu *vcpu, __u64 *exit_reason);
int gzvm_arch_destroy_vcpu(u16 vm_id, int vcpuid);
int gzvm_arch_inform_exit(u16 vm_id);

u64 gzvm_vcpu_arch_get_timer_delay_ns(struct gzvm_vcpu *vcpu);

void gzvm_vtimer_set(struct gzvm_vcpu *vcpu, u64 ns);
void gzvm_vtimer_release(struct gzvm_vcpu *vcpu);

int gzvm_find_memslot(struct gzvm *vm, u64 gpa);
int gzvm_handle_page_fault(struct gzvm_vcpu *vcpu);
bool gzvm_handle_guest_exception(struct gzvm_vcpu *vcpu);
int gzvm_handle_relinquish(struct gzvm_vcpu *vcpu, phys_addr_t ipa);
bool gzvm_handle_guest_hvc(struct gzvm_vcpu *vcpu);
bool gzvm_arch_handle_guest_hvc(struct gzvm_vcpu *vcpu);
int gzvm_handle_guest_idle(struct gzvm_vcpu *vcpu);
void gzvm_handle_guest_ipi(struct gzvm_vcpu *vcpu);
void gzvm_vcpu_wakeup_all(struct gzvm *gzvm);

int gzvm_arch_create_device(u16 vm_id, struct gzvm_create_device *gzvm_dev);
int gzvm_arch_inject_irq(struct gzvm *gzvm, unsigned int vcpu_idx,
			 u32 irq, bool level);

void gzvm_notify_acked_irq(struct gzvm *gzvm, unsigned int gsi);
int gzvm_irqfd(struct gzvm *gzvm, struct gzvm_irqfd *args);
int gzvm_drv_irqfd_init(void);
void gzvm_drv_irqfd_exit(void);
int gzvm_vm_irqfd_init(struct gzvm *gzvm);
void gzvm_vm_irqfd_release(struct gzvm *gzvm);

int gzvm_arch_memregion_purpose(struct gzvm *gzvm,
				struct gzvm_userspace_memory_region *mem);
int gzvm_arch_set_dtb_config(struct gzvm *gzvm, struct gzvm_dtb_config *args);

int gzvm_init_ioeventfd(struct gzvm *gzvm);
int gzvm_ioeventfd(struct gzvm *gzvm, struct gzvm_ioeventfd *args);
bool gzvm_ioevent_write(struct gzvm_vcpu *vcpu, __u64 addr, int len,
			const void *val);
void eventfd_ctx_do_read(struct eventfd_ctx *ctx, __u64 *cnt);
struct vm_area_struct *vma_lookup(struct mm_struct *mm, unsigned long addr);
void add_wait_queue_priority(struct wait_queue_head *wq_head,
			     struct wait_queue_entry *wq_entry);

#endif /* __GZVM_DRV_H__ */
