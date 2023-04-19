/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/**
 * DOC: UAPI of GenieZone Hypervisor
 *
 * This file declares common data structure shared among user space,
 * kernel space, and GenieZone hypervisor.
 */
#ifndef __GZVM_H__
#define __GZVM_H__

#include <linux/const.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#define GZVM_CAP_VM_GPA_SIZE	0xa5
#define GZVM_CAP_PROTECTED_VM	0xffbadab1

/* sub-commands put in args[0] for GZVM_CAP_PROTECTED_VM */
#define GZVM_CAP_PVM_SET_PVMFW_GPA		0
#define GZVM_CAP_PVM_GET_PVMFW_SIZE		1
/* GZVM_CAP_PVM_SET_PROTECTED_VM only sets protected but not load pvmfw */
#define GZVM_CAP_PVM_SET_PROTECTED_VM		2

/*
 * Architecture specific registers are to be defined and ORed with
 * the arch identifier.
 */
#define GZVM_REG_ARCH_ARM64	FIELD_PREP(GENMASK_ULL(63, 56), 0x60)
#define GZVM_REG_ARCH_MASK	FIELD_PREP(GENMASK_ULL(63, 56), 0xff)
/*
 * Reg size = BIT((reg.id & GZVM_REG_SIZE_MASK) >> GZVM_REG_SIZE_SHIFT) bytes
 */
#define GZVM_REG_SIZE_SHIFT	52
#define GZVM_REG_SIZE_MASK	FIELD_PREP(GENMASK_ULL(63, 48), 0x00f0)

#define GZVM_REG_SIZE_U8	FIELD_PREP(GENMASK_ULL(63, 48), 0x0000)
#define GZVM_REG_SIZE_U16	FIELD_PREP(GENMASK_ULL(63, 48), 0x0010)
#define GZVM_REG_SIZE_U32	FIELD_PREP(GENMASK_ULL(63, 48), 0x0020)
#define GZVM_REG_SIZE_U64	FIELD_PREP(GENMASK_ULL(63, 48), 0x0030)
#define GZVM_REG_SIZE_U128	FIELD_PREP(GENMASK_ULL(63, 48), 0x0040)
#define GZVM_REG_SIZE_U256	FIELD_PREP(GENMASK_ULL(63, 48), 0x0050)
#define GZVM_REG_SIZE_U512	FIELD_PREP(GENMASK_ULL(63, 48), 0x0060)
#define GZVM_REG_SIZE_U1024	FIELD_PREP(GENMASK_ULL(63, 48), 0x0070)
#define GZVM_REG_SIZE_U2048	FIELD_PREP(GENMASK_ULL(63, 48), 0x0080)

#define GZVM_REG_TYPE_GENERAL2	FIELD_PREP(GENMASK(23, 16), 0x10)

/* GZVM ioctls */
#define GZVM_IOC_MAGIC			0x92	/* gz */

/* ioctls for /dev/gzvm fds */
#define GZVM_CREATE_VM             _IO(GZVM_IOC_MAGIC,   0x01) /* Returns a Geniezone VM fd */

/*
 * Check if the given capability is supported or not.
 * The argument is capability. Ex. GZVM_CAP_PROTECTED_VM or GZVM_CAP_VM_GPA_SIZE
 * return is 0 (supported, no error)
 * return is -EOPNOTSUPP (unsupported)
 * return is -EFAULT (failed to get the argument from userspace)
 */
#define GZVM_CHECK_EXTENSION       _IO(GZVM_IOC_MAGIC,   0x03)

/* ioctls for VM fds */
/* for GZVM_SET_MEMORY_REGION */
struct gzvm_memory_region {
	__u32 slot;
	__u32 flags;
	__u64 guest_phys_addr;
	__u64 memory_size; /* bytes */
};

#define GZVM_SET_MEMORY_REGION     _IOW(GZVM_IOC_MAGIC,  0x40, \
					struct gzvm_memory_region)
/*
 * GZVM_CREATE_VCPU receives as a parameter the vcpu slot,
 * and returns a vcpu fd.
 */
#define GZVM_CREATE_VCPU           _IO(GZVM_IOC_MAGIC,   0x41)

/**
 * struct gzvm_userspace_memory_region: gzvm userspace memory region descriptor
 * @slot: memory slot
 * @flags: describe the usage of userspace memory region
 * @guest_phys_addr: guest vm's physical address
 * @memory_size: memory size in bytes
 * @userspace_addr: start of the userspace allocated memory
 */
struct gzvm_userspace_memory_region {
	__u32 slot;
	__u32 flags;
	__u64 guest_phys_addr;
	__u64 memory_size;
	__u64 userspace_addr;
};

#define GZVM_SET_USER_MEMORY_REGION _IOW(GZVM_IOC_MAGIC, 0x46, \
					 struct gzvm_userspace_memory_region)

/*
 * ioctls for vcpu fds
 */
#define GZVM_RUN                   _IO(GZVM_IOC_MAGIC,   0x80)

/* VM exit reason */
enum {
	GZVM_EXIT_UNKNOWN = 0x92920000,
	GZVM_EXIT_MMIO = 0x92920001,
	GZVM_EXIT_HYPERCALL = 0x92920002,
	GZVM_EXIT_IRQ = 0x92920003,
	GZVM_EXIT_EXCEPTION = 0x92920004,
	GZVM_EXIT_DEBUG = 0x92920005,
	GZVM_EXIT_FAIL_ENTRY = 0x92920006,
	GZVM_EXIT_INTERNAL_ERROR = 0x92920007,
	GZVM_EXIT_SYSTEM_EVENT = 0x92920008,
	GZVM_EXIT_SHUTDOWN = 0x92920009,
	GZVM_EXIT_GZ = 0x9292000a,
};

/**
 * struct gzvm_vcpu_run: Same purpose as kvm_run, this struct is
 *			 shared between userspace, kernel and
 *			 GenieZone hypervisor
 * @exit_reason: The reason why gzvm_vcpu_run has stopped running the vCPU
 * @immediate_exit: Polled when the vcpu is scheduled.
 *                  If set, immediately returns -EINTR
 * @padding1: Reserved for future-proof and must be zero filled
 * @mmio: The nested struct in anonymous union. Handle mmio in host side
 * @fail_entry: The nested struct in anonymous union.
 *              Handle invalid entry address at the first run
 * @exception: The nested struct in anonymous union.
 *             Handle exception occurred in VM
 * @hypercall: The nested struct in anonymous union.
 *             Some hypercalls issued from VM must be handled
 * @internal: The nested struct in anonymous union. The errors from hypervisor
 * @system_event: The nested struct in anonymous union.
 *                VM's PSCI must be handled by host
 * @padding: Fix it to a reasonable size future-proof for keeping the same
 *           struct size when adding new variables in the union is needed
 *
 * Keep identical layout between the 3 modules
 */
struct gzvm_vcpu_run {
	/* to userspace */
	__u32 exit_reason;
	__u8 immediate_exit;
	__u8 padding1[3];
	/* union structure of collection of guest exit reason */
	union {
		/* GZVM_EXIT_MMIO */
		struct {
			/* From FAR_EL2 */
			/* The address guest tries to access */
			__u64 phys_addr;
			/* The value to be written (is_write is 1) or
			 * be filled by user for reads (is_write is 0)
			 */
			__u8 data[8];
			/* From ESR_EL2 as */
			/* The size of written data.
			 * Only the first `size` bytes of `data` are handled
			 */
			__u64 size;
			/* From ESR_EL2 */
			/* The register number where the data is stored */
			__u32 reg_nr;
			/* From ESR_EL2 */
			/* 1 for VM to perform a write or 0 for VM to perform a read */
			__u8 is_write;
		} mmio;
		/* GZVM_EXIT_FAIL_ENTRY */
		struct {
			/* The reason codes about hardware entry failure */
			__u64 hardware_entry_failure_reason;
			/* The current processor number via smp_processor_id() */
			__u32 cpu;
		} fail_entry;
		/* GZVM_EXIT_EXCEPTION */
		struct {
			/* Which exception vector */
			__u32 exception;
			/* Exception error codes */
			__u32 error_code;
		} exception;
		/* GZVM_EXIT_HYPERCALL */
		struct {
			/* The hypercall's arguments */
			__u64 args[8];	/* in-out */
		} hypercall;
		/* GZVM_EXIT_INTERNAL_ERROR */
		struct {
			/* The errors codes about GZVM_EXIT_INTERNAL_ERROR */
			__u32 suberror;
			/* The number of elements used in data[] */
			__u32 ndata;
			/* Keep the detailed information about GZVM_EXIT_SYSTEM_EVENT */
			__u64 data[16];
		} internal;
		/* GZVM_EXIT_SYSTEM_EVENT */
		struct {
#define GZVM_SYSTEM_EVENT_SHUTDOWN       1
#define GZVM_SYSTEM_EVENT_RESET          2
#define GZVM_SYSTEM_EVENT_CRASH          3
#define GZVM_SYSTEM_EVENT_WAKEUP         4
#define GZVM_SYSTEM_EVENT_SUSPEND        5
#define GZVM_SYSTEM_EVENT_SEV_TERM       6
#define GZVM_SYSTEM_EVENT_S2IDLE         7
			/* System event type.
			 * Ex. GZVM_SYSTEM_EVENT_SHUTDOWN or GZVM_SYSTEM_EVENT_RESET...etc.
			 */
			__u32 type;
			/* The number of elements used in data[] */
			__u32 ndata;
			/* Keep the detailed information about GZVM_EXIT_SYSTEM_EVENT */
			__u64 data[16];
		} system_event;
		char padding[256];
	};
};

/**
 * struct gzvm_enable_cap: The `capability support` on GenieZone hypervisor
 * @cap: `GZVM_CAP_ARM_PROTECTED_VM` or `GZVM_CAP_ARM_VM_IPA_SIZE`
 * @args: x3-x7 registers can be used for additional args
 */
struct gzvm_enable_cap {
	__u64 cap;
	__u64 args[5];
};

#define GZVM_ENABLE_CAP            _IOW(GZVM_IOC_MAGIC,  0xa3, \
					struct gzvm_enable_cap)

/* for GZVM_GET/SET_ONE_REG */
struct gzvm_one_reg {
	__u64 id;
	__u64 addr;
};

#define GZVM_GET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xab, \
					struct gzvm_one_reg)
#define GZVM_SET_ONE_REG	   _IOW(GZVM_IOC_MAGIC,  0xac, \
					struct gzvm_one_reg)

#define GZVM_REG_GENERIC	   0x0000000000000000ULL

#endif /* __GZVM_H__ */
