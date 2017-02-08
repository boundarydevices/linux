/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hdcp22/host_driver_linux_if.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _HOST_LIB_DRIVER_LINUX_IF_H_
#define _HOST_LIB_DRIVER_LINUX_IF_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#include "ESMHostTypes.h"
#include "ESMHostLibDriverErrors.h"

/* ESM_HLD_IOCTL_LOAD_CODE */
struct esm_hld_ioctl_load_code {
	uint8_t *code;
	uint32_t code_size;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_load_code {
	compat_uptr_t code;
	uint32_t code_size;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_CODE_PHYS_ADDR */
struct esm_hld_ioctl_get_code_phys_addr {
	uint32_t returned_phys_addr;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_DATA_PHYS_ADDR */
struct esm_hld_ioctl_get_data_phys_addr {
	uint32_t returned_phys_addr;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_GET_DATA_SIZE */
struct esm_hld_ioctl_get_data_size {
	uint32_t returned_data_size;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_HPI_READ */
struct esm_hld_ioctl_hpi_read {
	uint32_t offset;
	uint32_t returned_data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_HPI_WRITE */
struct esm_hld_ioctl_hpi_write {
	uint32_t offset;
	uint32_t data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_READ */
struct esm_hld_ioctl_data_read {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t *dest_buf;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_data_read {
	uint32_t offset;
	uint32_t nbytes;
	compat_uptr_t dest_buf;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_WRITE */
struct esm_hld_ioctl_data_write {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t *src_buf;
	ESM_STATUS returned_status;
};

struct compact_esm_hld_ioctl_data_write {
	uint32_t offset;
	uint32_t nbytes;
	compat_uptr_t src_buf;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_DATA_SET */
struct esm_hld_ioctl_data_set {
	uint32_t offset;
	uint32_t nbytes;
	uint8_t data;
	ESM_STATUS returned_status;
};

/* ESM_HLD_IOCTL_ESM_OPEN */
struct esm_hld_ioctl_esm_open {
	uint32_t hpi_base;
	uint32_t code_base;
	uint32_t code_size;
	uint32_t data_base;
	uint32_t data_size;
	ESM_STATUS returned_status;
};

/* IOCTL commands */
#define ESM_HLD_IOC_MAGIC  'E'
#define ESM_HLD_IOCTL_LOAD_CODE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1000, struct esm_hld_ioctl_load_code)
#define ESM_HLD_IOCTL_GET_CODE_PHYS_ADDR \
	_IOR(ESM_HLD_IOC_MAGIC, 1001, struct esm_hld_ioctl_get_code_phys_addr)
#define ESM_HLD_IOCTL_GET_DATA_PHYS_ADDR \
	_IOR(ESM_HLD_IOC_MAGIC, 1002, struct esm_hld_ioctl_get_data_phys_addr)
#define ESM_HLD_IOCTL_GET_DATA_SIZE \
	_IOR(ESM_HLD_IOC_MAGIC, 1003, struct esm_hld_ioctl_get_data_size)
#define ESM_HLD_IOCTL_HPI_READ \
	_IOWR(ESM_HLD_IOC_MAGIC, 1004, struct esm_hld_ioctl_hpi_read)
#define ESM_HLD_IOCTL_HPI_WRITE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1005, struct esm_hld_ioctl_hpi_write)
#define ESM_HLD_IOCTL_DATA_READ \
	_IOWR(ESM_HLD_IOC_MAGIC, 1006, struct esm_hld_ioctl_data_read)
#define ESM_HLD_IOCTL_DATA_WRITE \
	_IOWR(ESM_HLD_IOC_MAGIC, 1007, struct esm_hld_ioctl_data_write)
#define ESM_HLD_IOCTL_DATA_SET \
	_IOWR(ESM_HLD_IOC_MAGIC, 1008, struct esm_hld_ioctl_data_set)
#define ESM_HLD_IOCTL_ESM_OPEN \
	_IOWR(ESM_HLD_IOC_MAGIC, 1009, struct esm_hld_ioctl_esm_open)
#define ESM_HLD_IOCTL_LOAD_CODE32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1000, struct compact_esm_hld_ioctl_load_code)
#define ESM_HLD_IOCTL_DATA_READ32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1006, struct compact_esm_hld_ioctl_data_read)
#define ESM_HLD_IOCTL_DATA_WRITE32 \
	_IOWR(ESM_HLD_IOC_MAGIC, 1007, struct compact_esm_hld_ioctl_data_write)

#endif /* _HOST_LIB_DRIVER_LINUX_IF_H_ */
