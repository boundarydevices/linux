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
/* query hypervisor supported block-based demand page */
#define GZVM_CAP_BLOCK_BASED_DEMAND_PAGING	0x9201
#define GZVM_CAP_ENABLE_DEMAND_PAGING	0x9202
#define GZVM_CAP_ENABLE_IDLE		0x9203
#define GZVM_CAP_QUERY_HYP_BATCH_PAGES	0x9204
#define GZVM_CAP_QUERY_DESTROY_BATCH_PAGES	0x9205

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

/* for GZVM_IRQ_LINE, irq field index values */
#define GZVM_IRQ_VCPU_MASK		0xff
#define GZVM_IRQ_LINE_TYPE		GENMASK(27, 24)
#define GZVM_IRQ_LINE_VCPU		GENMASK(23, 16)
#define GZVM_IRQ_LINE_VCPU2		GENMASK(31, 28)
#define GZVM_IRQ_LINE_NUM		GENMASK(15, 0)

/* irq_type field */
#define GZVM_IRQ_TYPE_CPU		0
#define GZVM_IRQ_TYPE_SPI		1
#define GZVM_IRQ_TYPE_PPI		2

/* out-of-kernel GIC cpu interrupt injection irq_number field */
#define GZVM_IRQ_CPU_IRQ		0
#define GZVM_IRQ_CPU_FIQ		1

struct gzvm_irq_level {
	union {
		__u32 irq;
		__s32 status;
	};
	__u32 level;
};

#define GZVM_IRQ_LINE              _IOW(GZVM_IOC_MAGIC,  0x61, \
					struct gzvm_irq_level)

enum gzvm_device_type {
	GZVM_DEV_TYPE_ARM_VGIC_V3_DIST = 0,
	GZVM_DEV_TYPE_ARM_VGIC_V3_REDIST = 1,
	GZVM_DEV_TYPE_MAX,
};

/**
 * struct gzvm_create_device: For GZVM_CREATE_DEVICE.
 * @dev_type: Device type.
 * @id: Device id.
 * @flags: Bypass to hypervisor to handle them and these are flags of virtual
 *         devices.
 * @dev_addr: Device ipa address in VM's view.
 * @dev_reg_size: Device register range size.
 * @attr_addr: If user -> kernel, this is user virtual address of device
 *             specific attributes (if needed). If kernel->hypervisor,
 *             this is ipa.
 * @attr_size: This attr_size is the buffer size in bytes of each attribute
 *             needed from various devices. The attribute here refers to the
 *             additional data passed from VMM(e.g. Crosvm) to GenieZone
 *             hypervisor when virtual devices were to be created. Thus,
 *             we need attr_addr and attr_size in the gzvm_create_device
 *             structure to keep track of the attribute mentioned.
 *
 * Store information needed to create device.
 */
struct gzvm_create_device {
	__u32 dev_type;
	__u32 id;
	__u64 flags;
	__u64 dev_addr;
	__u64 dev_reg_size;
	__u64 attr_addr;
	__u64 attr_size;
};

#define GZVM_CREATE_DEVICE	   _IOWR(GZVM_IOC_MAGIC,  0xe0, \
					struct gzvm_create_device)

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
	GZVM_EXIT_IDLE = 0x9292000b,
	GZVM_EXIT_IPI = 0x9292000d,
};

/* exception definitions of GZVM_EXIT_EXCEPTION */
enum {
	GZVM_EXCEPTION_UNKNOWN = 0x0,
	GZVM_EXCEPTION_PAGE_FAULT = 0x1,
};

/* hypercall definitions of GZVM_EXIT_HYPERCALL */
enum {
	GZVM_HVC_PTP = 0x86000001,
	GZVM_HVC_MEM_RELINQUISH = 0xc6000009,
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
			/* Fault GPA (guest physical address or IPA in ARM) */
			__u64 fault_gpa;
			/* Future-proof reservation and reset to zero in hypervisor.
			 * Fill up to the union size, 256 bytes.
			 */
			__u64 reserved[30];
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

#define GZVM_IRQFD_FLAG_DEASSIGN	BIT(0)
/*
 * GZVM_IRQFD_FLAG_RESAMPLE indicates resamplefd is valid and specifies
 * the irqfd to operate in resampling mode for level triggered interrupt
 * emulation.
 */
#define GZVM_IRQFD_FLAG_RESAMPLE	BIT(1)

/**
 * struct gzvm_irqfd: gzvm irqfd descriptor
 * @fd: File descriptor.
 * @gsi: Used for level IRQ fast-path.
 * @flags: FLAG_DEASSIGN or FLAG_RESAMPLE.
 * @resamplefd: The file descriptor of the resampler.
 * @pad: Reserved for future-proof.
 */
struct gzvm_irqfd {
	__u32 fd;
	__u32 gsi;
	__u32 flags;
	__u32 resamplefd;
	__u8  pad[16];
};

#define GZVM_IRQFD	_IOW(GZVM_IOC_MAGIC, 0x76, struct gzvm_irqfd)

enum {
	gzvm_ioeventfd_flag_nr_datamatch = 0,
	gzvm_ioeventfd_flag_nr_pio = 1,
	gzvm_ioeventfd_flag_nr_deassign = 2,
	gzvm_ioeventfd_flag_nr_max,
};

#define GZVM_IOEVENTFD_FLAG_DATAMATCH	(1 << gzvm_ioeventfd_flag_nr_datamatch)
#define GZVM_IOEVENTFD_FLAG_PIO		(1 << gzvm_ioeventfd_flag_nr_pio)
#define GZVM_IOEVENTFD_FLAG_DEASSIGN	(1 << gzvm_ioeventfd_flag_nr_deassign)
#define GZVM_IOEVENTFD_VALID_FLAG_MASK	((1 << gzvm_ioeventfd_flag_nr_max) - 1)

struct gzvm_ioeventfd {
	__u64 datamatch;
	/* private: legal pio/mmio address */
	__u64 addr;
	/* private: 1, 2, 4, or 8 bytes; or 0 to ignore length */
	__u32 len;
	__s32 fd;
	__u32 flags;
	__u8  pad[36];
};

#define GZVM_IOEVENTFD	_IOW(GZVM_IOC_MAGIC, 0x79, struct gzvm_ioeventfd)

/**
 * struct gzvm_dtb_config: store address and size of dtb passed from userspace
 *
 * @dtb_addr: dtb address set by VMM (guset memory)
 * @dtb_size: dtb size
 */
struct gzvm_dtb_config {
	__u64 dtb_addr;
	__u64 dtb_size;
};

#define GZVM_SET_DTB_CONFIG       _IOW(GZVM_IOC_MAGIC, 0xff, \
				       struct gzvm_dtb_config)

#endif /* __GZVM_H__ */
