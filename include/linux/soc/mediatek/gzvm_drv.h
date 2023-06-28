/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GZVM_DRV_H__
#define __GZVM_DRV_H__

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
 * These are the definitions of APIs between GenieZone hypervisor and driver,
 * there's no need to be visible to uapi. Furthermore, we need GenieZone
 * specific error code in order to map to Linux errno
 */
#define NO_ERROR                (0)
#define ERR_NO_MEMORY           (-5)
#define ERR_NOT_SUPPORTED       (-24)
#define ERR_NOT_IMPLEMENTED     (-27)
#define ERR_FAULT               (-40)

int gzvm_err_to_errno(unsigned long err);

/* arch-dependant functions */
int gzvm_arch_probe(struct gzvm_version drv_version,
		    struct gzvm_version *hyp_version);

#endif /* __GZVM_DRV_H__ */
