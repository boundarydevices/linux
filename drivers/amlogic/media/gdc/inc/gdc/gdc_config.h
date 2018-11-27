/*
 * drivers/amlogic/media/gdc/inc/gdc/gdc_config.h
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

#ifndef __GDC_CONFIG_H__
#define __GDC_CONFIG_H__

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include "system_gdc_io.h"
#include "gdc_api.h"

struct gdc_settings;
struct meson_gdc_dev_t {
	struct platform_device *pdev;
	void	*reg_base;
	struct clk	*clk_core;
	struct clk	*clk_axi;
	spinlock_t	slock;
	struct mutex d_mutext;
	struct completion d_com;
	int	 irq;
};

struct mgdc_fh_s {
	struct list_head list;
	wait_queue_head_t	irq_queue;
	struct meson_gdc_dev_t *gdev;
	char task_comm[32];
	struct ion_client   *ion_client;
	struct gdc_settings gs;
	uint32_t mmap_type;
	dma_addr_t i_paddr;
	dma_addr_t o_paddr;
	dma_addr_t c_paddr;
	void *i_kaddr;
	void *o_kaddr;
	void *c_kaddr;
	unsigned long i_len;
	unsigned long o_len;
	unsigned long c_len;
	struct gdc_dma_cfg y_dma_cfg;
	struct gdc_dma_cfg uv_dma_cfg;
};

irqreturn_t interrupt_handler_next(int irq, void *param);
// ----------------------------------- //
// Instance 'gdc' of module 'gdc_ip_config'
// ----------------------------------- //

#define GDC_BASE_ADDR (0x00L)
#define GDC_SIZE (0x100)

// ----------------------------------- //
// Group: ID
// ----------------------------------- //

// ----------------------------------- //
// Register: API
// ----------------------------------- //

#define GDC_ID_API_DEFAULT (0x0)
#define GDC_ID_API_DATASIZE (32)
#define GDC_ID_API_OFFSET (0x0)
#define GDC_ID_API_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_id_api_read(void)
{
	return system_gdc_read_32(0x00L);
}
// ----------------------------------- //
// Register: Product
// ----------------------------------- //

#define GDC_ID_PRODUCT_DEFAULT (0x0)
#define GDC_ID_PRODUCT_DATASIZE (32)
#define GDC_ID_PRODUCT_OFFSET (0x4)
#define GDC_ID_PRODUCT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_id_product_read(void)
{
	return system_gdc_read_32(0x04L);
}
// ----------------------------------- //
// Register: Version
// ----------------------------------- //

#define GDC_ID_VERSION_DEFAULT (0x0)
#define GDC_ID_VERSION_DATASIZE (32)
#define GDC_ID_VERSION_OFFSET (0x8)
#define GDC_ID_VERSION_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_id_version_read(void)
{
	return system_gdc_read_32(0x08L);
}
// ----------------------------------- //
// Register: Revision
// ----------------------------------- //

#define GDC_ID_REVISION_DEFAULT (0x0)
#define GDC_ID_REVISION_DATASIZE (32)
#define GDC_ID_REVISION_OFFSET (0xc)
#define GDC_ID_REVISION_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_id_revision_read(void)
{
	return system_gdc_read_32(0x0cL);
}
// ----------------------------------- //
// Group: GDC
// ----------------------------------- //
// ----------------------------------- //
// GDC controls
// ----------------------------------- //
// ----------------------------------- //
// Register: config addr
// ----------------------------------- //
// ----------------------------------- //
// Base address of configuration stream
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_CONFIG_ADDR_DEFAULT (0x0)
#define GDC_CONFIG_ADDR_DATASIZE (32)
#define GDC_CONFIG_ADDR_OFFSET (0x10)
#define GDC_CONFIG_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_config_addr_write(uint32_t data)
{
	system_gdc_write_32(0x10L, data);
}
static inline uint32_t gdc_config_addr_read(void)
{
	return system_gdc_read_32(0x10L);
}
// ----------------------------------- //
// Register: config size
// ----------------------------------- //
// ----------------------------------- //
// Size of the configuration stream	//
// (in bytes, 32 bit word granularity) //
// ----------------------------------- //

#define GDC_CONFIG_SIZE_DEFAULT (0x0)
#define GDC_CONFIG_SIZE_DATASIZE (32)
#define GDC_CONFIG_SIZE_OFFSET (0x14)
#define GDC_CONFIG_SIZE_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_config_size_write(uint32_t data)
{
	system_gdc_write_32(0x14L, data);
}
static inline uint32_t gdc_config_size_read(void)
{
	return system_gdc_read_32(0x14L);
}
// ----------------------------------- //
// Register: datain width
// ----------------------------------- //

// ----------------------------------- //
// Width of the input image (in pixels)
// ----------------------------------- //

#define GDC_DATAIN_WIDTH_DEFAULT (0x0)
#define GDC_DATAIN_WIDTH_DATASIZE (16)
#define GDC_DATAIN_WIDTH_OFFSET (0x20)
#define GDC_DATAIN_WIDTH_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_datain_width_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x20L);

	system_gdc_write_32(0x20L, ((curr & 0xffff0000) | data));
}
static inline uint16_t gdc_datain_width_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x20L) & 0xffff) >> 0);
}
// ----------------------------------- //
// Register: datain_height
// ----------------------------------- //

// ----------------------------------- //
// Height of the input image (in pixels)
// ----------------------------------- //

#define GDC_DATAIN_HEIGHT_DEFAULT (0x0)
#define GDC_DATAIN_HEIGHT_DATASIZE (16)
#define GDC_DATAIN_HEIGHT_OFFSET (0x24)
#define GDC_DATAIN_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_datain_height_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x24L);

	system_gdc_write_32(0x24L, ((curr & 0xffff0000) | data));
}
static inline uint16_t gdc_datain_height_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x24L) & 0xffff) >> 0);
}
// ----------------------------------- //
// Register: data1in addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 1st plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1IN_ADDR_DEFAULT (0x0)
#define GDC_DATA1IN_ADDR_DATASIZE (32)
#define GDC_DATA1IN_ADDR_OFFSET (0x28)
#define GDC_DATA1IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1in_addr_write(uint32_t data)
{
	system_gdc_write_32(0x28L, data);
}
static inline uint32_t gdc_data1in_addr_read(void)
{
	return system_gdc_read_32(0x28L);
}
// ----------------------------------- //
// Register: data1in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 1st plane in the input
// frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA1IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA1IN_LINE_OFFSET_OFFSET (0x2c)
#define GDC_DATA1IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1in_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x2cL, data);
}
static inline uint32_t gdc_data1in_line_offset_read(void)
{
	return system_gdc_read_32(0x2cL);
}
// ----------------------------------- //
// Register: data2in addr
// ----------------------------------- //

// ----------------------------------- //
// Address of the 2nd plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2IN_ADDR_DEFAULT (0x0)
#define GDC_DATA2IN_ADDR_DATASIZE (32)
#define GDC_DATA2IN_ADDR_OFFSET (0x30)
#define GDC_DATA2IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2in_addr_write(uint32_t data)
{
	system_gdc_write_32(0x30L, data);
}
static inline uint32_t gdc_data2in_addr_read(void)
{
	return system_gdc_read_32(0x30L);
}
// ----------------------------------- //
// Register: data2in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 2nd plane in the input
// frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA2IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA2IN_LINE_OFFSET_OFFSET (0x34)
#define GDC_DATA2IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2in_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x34L, data);
}
static inline uint32_t gdc_data2in_line_offset_read(void)
{
	return system_gdc_read_32(0x34L);
}
// ----------------------------------- //
// Register: data3in addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 3rd plane in the
// input frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3IN_ADDR_DEFAULT (0x0)
#define GDC_DATA3IN_ADDR_DATASIZE (32)
#define GDC_DATA3IN_ADDR_OFFSET (0x38)
#define GDC_DATA3IN_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3in_addr_write(uint32_t data)
{
	system_gdc_write_32(0x38L, data);
}
static inline uint32_t gdc_data3in_addr_read(void)
{
	return system_gdc_read_32(0x38L);
}
// ----------------------------------- //
// Register: data3in line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 3rd plane in the
// input frame buffer (in bytes,
// AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3IN_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA3IN_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA3IN_LINE_OFFSET_OFFSET (0x3c)
#define GDC_DATA3IN_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3in_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x3cL, data);
}
static inline uint32_t gdc_data3in_line_offset_read(void)
{
	return system_gdc_read_32(0x3cL);
}
// ----------------------------------- //
// Register: dataout width
// ----------------------------------- //

// ----------------------------------- //
// Width of the output image (in pixels)
// ----------------------------------- //

#define GDC_DATAOUT_WIDTH_DEFAULT (0x0)
#define GDC_DATAOUT_WIDTH_DATASIZE (16)
#define GDC_DATAOUT_WIDTH_OFFSET (0x40)
#define GDC_DATAOUT_WIDTH_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_dataout_width_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x40L);

	system_gdc_write_32(0x40L, ((curr & 0xffff0000) | data));
}
static inline uint16_t gdc_dataout_width_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x40L) & 0xffff) >> 0);
}
// ----------------------------------- //
// Register: dataout height
// ----------------------------------- //

// ----------------------------------- //
// Height of the output image (in pixels)
// ----------------------------------- //

#define GDC_DATAOUT_HEIGHT_DEFAULT (0x0)
#define GDC_DATAOUT_HEIGHT_DATASIZE (16)
#define GDC_DATAOUT_HEIGHT_OFFSET (0x44)
#define GDC_DATAOUT_HEIGHT_MASK (0xffff)

// args: data (16-bit)
static inline void gdc_dataout_height_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x44L);

	system_gdc_write_32(0x44L, ((curr & 0xffff0000) | data));
}
static inline uint16_t gdc_dataout_height_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x44L) & 0xffff) >> 0);
}
// ----------------------------------- //
// Register: data1out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 1st plane in the
// output frame buffer
// (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA1OUT_ADDR_DATASIZE (32)
#define GDC_DATA1OUT_ADDR_OFFSET (0x48)
#define GDC_DATA1OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1out_addr_write(uint32_t data)
{
	system_gdc_write_32(0x48L, data);
}
static inline uint32_t gdc_data1out_addr_read(void)
{
	return system_gdc_read_32(0x48L);
}
// ----------------------------------- //
// Register: data1out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 1st plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA1OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA1OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA1OUT_LINE_OFFSET_OFFSET (0x4c)
#define GDC_DATA1OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data1out_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x4cL, data);
}
static inline uint32_t gdc_data1out_line_offset_read(void)
{
	return system_gdc_read_32(0x4cL);
}
// ----------------------------------- //
// Register: data2out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 2nd plane in the
// output frame buffer  (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA2OUT_ADDR_DATASIZE (32)
#define GDC_DATA2OUT_ADDR_OFFSET (0x50)
#define GDC_DATA2OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2out_addr_write(uint32_t data)
{
	system_gdc_write_32(0x50L, data);
}
static inline uint32_t gdc_data2out_addr_read(void)
{
	return system_gdc_read_32(0x50L);
}
// ----------------------------------- //
// Register: data2out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent lines
// for the 2ndt plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA2OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA2OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA2OUT_LINE_OFFSET_OFFSET (0x54)
#define GDC_DATA2OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data2out_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x54L, data);
}
static inline uint32_t gdc_data2out_line_offset_read(void)
{
	return system_gdc_read_32(0x54L);
}
// ----------------------------------- //
// Register: data3out addr
// ----------------------------------- //

// ----------------------------------- //
// Base address of the 3rd plane in the
// output frame buffer  (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3OUT_ADDR_DEFAULT (0x0)
#define GDC_DATA3OUT_ADDR_DATASIZE (32)
#define GDC_DATA3OUT_ADDR_OFFSET (0x58)
#define GDC_DATA3OUT_ADDR_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3out_addr_write(uint32_t data)
{
	system_gdc_write_32(0x58L, data);
}
static inline uint32_t gdc_data3out_addr_read(void)
{
	return system_gdc_read_32(0x58L);
}
// ----------------------------------- //
// Register: data3out line offset
// ----------------------------------- //

// ----------------------------------- //
// Address difference between adjacent
// lines for the 3rd plane in the
// output frame buffer (in bytes, AXI word aligned)
// ----------------------------------- //

#define GDC_DATA3OUT_LINE_OFFSET_DEFAULT (0x0)
#define GDC_DATA3OUT_LINE_OFFSET_DATASIZE (32)
#define GDC_DATA3OUT_LINE_OFFSET_OFFSET (0x5c)
#define GDC_DATA3OUT_LINE_OFFSET_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_data3out_line_offset_write(uint32_t data)
{
	system_gdc_write_32(0x5cL, data);
}
static inline uint32_t gdc_data3out_line_offset_read(void)
{
	return system_gdc_read_32(0x5cL);
}
// ----------------------------------- //
// Register: status
// ----------------------------------- //

// ----------------------------------- //
// word with status fields:
// ----------------------------------- //

#define GDC_STATUS_DEFAULT (0x0)
#define GDC_STATUS_DATASIZE (32)
#define GDC_STATUS_OFFSET (0x60)
#define GDC_STATUS_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_status_read(void)
{
	return system_gdc_read_32(0x60L);
}
// ----------------------------------- //
// Register: busy
// ----------------------------------- //

// ----------------------------------- //
// Busy 1 = processing in progress,
//	  0 = ready for next image
// ----------------------------------- //

#define GDC_BUSY_DEFAULT (0x0)
#define GDC_BUSY_DATASIZE (1)
#define GDC_BUSY_OFFSET (0x60)
#define GDC_BUSY_MASK (0x1)

// args: data (1-bit)
static inline void gdc_busy_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);

	system_gdc_write_32(0x60L, ((data & 0x1) << 0) | (curr & 0xfffffffe));
}
static inline uint8_t gdc_busy_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x1) >> 0);
}
// ----------------------------------- //
// Register: error
// ----------------------------------- //

// ----------------------------------- //
// Error flag: last operation was finished with error (see bits 15:8)
// ----------------------------------- //

#define GDC_ERROR_DEFAULT (0x0)
#define GDC_ERROR_DATASIZE (1)
#define GDC_ERROR_OFFSET (0x60)
#define GDC_ERROR_MASK (0x2)

// args: data (1-bit)
static inline void gdc_error_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x2) >> 1);
}
// ----------------------------------- //
// Register: Reserved for future use 1
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_1_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_1_DATASIZE (6)
#define GDC_RESERVED_FOR_FUTURE_USE_1_OFFSET (0x60)
#define GDC_RESERVED_FOR_FUTURE_USE_1_MASK (0xfc)

// args: data (6-bit)
static inline void gdc_reserved_for_future_use_1_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x3f) << 2) | (curr & 0xffffff03);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_reserved_for_future_use_1_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0xfc) >> 2);
}
// ----------------------------------- //
// Register: configuration error
// ----------------------------------- //

// ----------------------------------- //
// Configuration error (wrong configuration stream)
// ----------------------------------- //

#define GDC_CONFIGURATION_ERROR_DEFAULT (0x0)
#define GDC_CONFIGURATION_ERROR_DATASIZE (1)
#define GDC_CONFIGURATION_ERROR_OFFSET (0x60)
#define GDC_CONFIGURATION_ERROR_MASK (0x100)

// args: data (1-bit)
static inline void gdc_configuration_error_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val =  ((data & 0x1) << 8) | (curr & 0xfffffeff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_configuration_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x100) >> 8);
}
// ----------------------------------- //
// Register: user abort
// ----------------------------------- //

// ----------------------------------- //
// User abort (stop/reset command)
// ----------------------------------- //

#define GDC_USER_ABORT_DEFAULT (0x0)
#define GDC_USER_ABORT_DATASIZE (1)
#define GDC_USER_ABORT_OFFSET (0x60)
#define GDC_USER_ABORT_MASK (0x200)

// args: data (1-bit)
static inline void gdc_user_abort_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 9) | (curr & 0xfffffdff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_user_abort_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x200) >> 9);
}
// ----------------------------------- //
// Register: AXI reader error
// ----------------------------------- //

// ----------------------------------- //
// AXI reader error (e.g. error code returned by fabric)
// ----------------------------------- //

#define GDC_AXI_READER_ERROR_DEFAULT (0x0)
#define GDC_AXI_READER_ERROR_DATASIZE (1)
#define GDC_AXI_READER_ERROR_OFFSET (0x60)
#define GDC_AXI_READER_ERROR_MASK (0x400)

// args: data (1-bit)
static inline void gdc_axi_reader_error_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 10) | (curr & 0xfffffbff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_axi_reader_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x400) >> 10);
}
// ----------------------------------- //
// Register: AXI writer error
// ----------------------------------- //

// ----------------------------------- //
// AXI writer error
// ----------------------------------- //

#define GDC_AXI_WRITER_ERROR_DEFAULT (0x0)
#define GDC_AXI_WRITER_ERROR_DATASIZE (1)
#define GDC_AXI_WRITER_ERROR_OFFSET (0x60)
#define GDC_AXI_WRITER_ERROR_MASK (0x800)

// args: data (1-bit)
static inline void gdc_axi_writer_error_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 11) | (curr & 0xfffff7ff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_axi_writer_error_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x800) >> 11);
}
// ----------------------------------- //
// Register: Unaligned access
// ----------------------------------- //

// ----------------------------------- //
// Unaligned access (address pointer is not aligned)
// ----------------------------------- //

#define GDC_UNALIGNED_ACCESS_DEFAULT (0x0)
#define GDC_UNALIGNED_ACCESS_DATASIZE (1)
#define GDC_UNALIGNED_ACCESS_OFFSET (0x60)
#define GDC_UNALIGNED_ACCESS_MASK (0x1000)

// args: data (1-bit)
static inline void gdc_unaligned_access_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 12) | (curr & 0xffffefff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_unaligned_access_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x1000) >> 12);
}
// ----------------------------------- //
// Register: Incompatible configuration
// ----------------------------------- //

// ----------------------------------- //
// Incompatible configuration (request
// of unimplemented mode of operation,
// e.g. unsupported image format,
// unsupported module mode in the configuration stream)
// ----------------------------------- //

#define GDC_INCOMPATIBLE_CONFIGURATION_DEFAULT (0x0)
#define GDC_INCOMPATIBLE_CONFIGURATION_DATASIZE (1)
#define GDC_INCOMPATIBLE_CONFIGURATION_OFFSET (0x60)
#define GDC_INCOMPATIBLE_CONFIGURATION_MASK (0x2000)

// args: data (1-bit)
static inline void gdc_incompatible_configuration_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val = ((data & 0x1) << 13) | (curr & 0xffffdfff);

	system_gdc_write_32(0x60L, val);
}
static inline uint8_t gdc_incompatible_configuration_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x60L) & 0x2000) >> 13);
}
// ----------------------------------- //
// Register: Reserved for future use 2
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_2_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_2_DATASIZE (18)
#define GDC_RESERVED_FOR_FUTURE_USE_2_OFFSET (0x60)
#define GDC_RESERVED_FOR_FUTURE_USE_2_MASK (0xffffc000)

// args: data (18-bit)
static inline void gdc_reserved_for_future_use_2_write(uint32_t data)
{
	uint32_t curr = system_gdc_read_32(0x60L);
	uint32_t val =  ((data & 0x3ffff) << 14) | (curr & 0x3fff);

	system_gdc_write_32(0x60L, val);
}
static inline uint32_t gdc_reserved_for_future_use_2_read(void)
{
	return (uint32_t)((system_gdc_read_32(0x60L) & 0xffffc000) >> 14);
}
// ----------------------------------- //
// Register: config
// ----------------------------------- //

#define GDC_CONFIG_DEFAULT (0x0)
#define GDC_CONFIG_DATASIZE (32)
#define GDC_CONFIG_OFFSET (0x64)
#define GDC_CONFIG_MASK (0xffffffff)

// args: data (32-bit)
static inline void gdc_config_write(uint32_t data)
{
	system_gdc_write_32(0x64L, data);
}
static inline uint32_t gdc_config_read(void)
{
	return system_gdc_read_32(0x64L);
}
// ----------------------------------- //
// Register: start flag
// ----------------------------------- //

// ----------------------------------- //
// Start flag: transition from 0 to 1
// latches the data on the configuration ports
// and starts the processing
// ----------------------------------- //

#define GDC_START_FLAG_DEFAULT (0x0)
#define GDC_START_FLAG_DATASIZE (1)
#define GDC_START_FLAG_OFFSET (0x64)
#define GDC_START_FLAG_MASK (0x1)

// args: data (1-bit)
static inline void gdc_start_flag_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x64L);
	uint32_t val = ((data & 0x1) << 0) | (curr & 0xfffffffe);

	system_gdc_write_32(0x64L, val);
}
static inline uint8_t gdc_start_flag_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x64L) & 0x1) >> 0);
}
// ----------------------------------- //
// Register: stop flag
// ----------------------------------- //

// ----------------------------------- //
// Stop/reset flag: 0 - normal operation,
// 1 means to initiate internal cleanup procedure
// to abandon the current frame
// and prepare for processing of the next frame.
// The busy flag in status word should be
// cleared at the end of this process
// ----------------------------------- //

#define GDC_STOP_FLAG_DEFAULT (0x0)
#define GDC_STOP_FLAG_DATASIZE (1)
#define GDC_STOP_FLAG_OFFSET (0x64)
#define GDC_STOP_FLAG_MASK (0x2)

// args: data (1-bit)
static inline void gdc_stop_flag_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x64L);
	uint32_t val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

	system_gdc_write_32(0x64L, val);
}
static inline uint8_t gdc_stop_flag_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x64L) & 0x2) >> 1);
}
// ----------------------------------- //
// Register: Reserved for future use 3
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_3_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_3_DATASIZE (30)
#define GDC_RESERVED_FOR_FUTURE_USE_3_OFFSET (0x64)
#define GDC_RESERVED_FOR_FUTURE_USE_3_MASK (0xfffffffc)

// args: data (30-bit)
static inline void gdc_reserved_for_future_use_3_write(uint32_t data)
{
	uint32_t curr = system_gdc_read_32(0x64L);
	uint32_t val =  ((data & 0x3fffffff) << 2) | (curr & 0x3);

	system_gdc_write_32(0x64L, val);
}
static inline uint32_t gdc_reserved_for_future_use_3_read(void)
{
	return (uint32_t)((system_gdc_read_32(0x64L) & 0xfffffffc) >> 2);
}
// ----------------------------------- //
// Register: Capability mask
// ----------------------------------- //

#define GDC_CAPABILITY_MASK_DEFAULT (0x0)
#define GDC_CAPABILITY_MASK_DATASIZE (32)
#define GDC_CAPABILITY_MASK_OFFSET (0x68)
#define GDC_CAPABILITY_MASK_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_capability_mask_read(void)
{
	return system_gdc_read_32(0x68L);
}
// ----------------------------------- //
// Register: Eight bit data suppoirted
// ----------------------------------- //

// ----------------------------------- //
// 8 bit data supported
// ----------------------------------- //

#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_DEFAULT (0x0)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_DATASIZE (1)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_OFFSET (0x68)
#define GDC_EIGHT_BIT_DATA_SUPPOIRTED_MASK (0x1)

// args: data (1-bit)
static inline void gdc_eight_bit_data_suppoirted_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 0) | (curr & 0xfffffffe);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_eight_bit_data_suppoirted_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x1) >> 0);
}
// ----------------------------------- //
// Register: Ten bit data supported
// ----------------------------------- //

// ----------------------------------- //
// 10 bit data supported
// ----------------------------------- //

#define GDC_TEN_BIT_DATA_SUPPORTED_DEFAULT (0x0)
#define GDC_TEN_BIT_DATA_SUPPORTED_DATASIZE (1)
#define GDC_TEN_BIT_DATA_SUPPORTED_OFFSET (0x68)
#define GDC_TEN_BIT_DATA_SUPPORTED_MASK (0x2)

// args: data (1-bit)
static inline void gdc_ten_bit_data_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 1) | (curr & 0xfffffffd);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_ten_bit_data_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x2) >> 1);
}
// ----------------------------------- //
// Register: Grayscale supported
// ----------------------------------- //

// ----------------------------------- //
// grayscale supported
// ----------------------------------- //

#define GDC_GRAYSCALE_SUPPORTED_DEFAULT (0x0)
#define GDC_GRAYSCALE_SUPPORTED_DATASIZE (1)
#define GDC_GRAYSCALE_SUPPORTED_OFFSET (0x68)
#define GDC_GRAYSCALE_SUPPORTED_MASK (0x4)

// args: data (1-bit)
static inline void gdc_grayscale_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 2) | (curr & 0xfffffffb);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_grayscale_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x4) >> 2);
}
// ----------------------------------- //
// Register: RGBA888 supported
// ----------------------------------- //

// ----------------------------------- //
// RGBA8:8:8/YUV4:4:4 mode supported
// ----------------------------------- //

#define GDC_RGBA888_SUPPORTED_DEFAULT (0x0)
#define GDC_RGBA888_SUPPORTED_DATASIZE (1)
#define GDC_RGBA888_SUPPORTED_OFFSET (0x68)
#define GDC_RGBA888_SUPPORTED_MASK (0x8)

// args: data (1-bit)
static inline void gdc_rgba888_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 3) | (curr & 0xfffffff7);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_rgba888_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x8) >> 3);
}
// ----------------------------------- //
// Register: RGB YUV444 planar supported
// ----------------------------------- //

// ----------------------------------- //
// RGB/YUV444 planar modes supported
// ----------------------------------- //

#define GDC_RGB_YUV444_PLANAR_SUPPORTED_DEFAULT (0x0)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_DATASIZE (1)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_OFFSET (0x68)
#define GDC_RGB_YUV444_PLANAR_SUPPORTED_MASK (0x10)

// args: data (1-bit)
static inline void gdc_rgb_yuv444_planar_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 4) | (curr & 0xffffffef);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_rgb_yuv444_planar_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x10) >> 4);
}
// ----------------------------------- //
// Register: YUV semiplanar supported
// ----------------------------------- //

// ----------------------------------- //
// YUV semiplanar modes supported
// ----------------------------------- //

#define GDC_YUV_SEMIPLANAR_SUPPORTED_DEFAULT (0x0)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_DATASIZE (1)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_OFFSET (0x68)
#define GDC_YUV_SEMIPLANAR_SUPPORTED_MASK (0x20)

// args: data (1-bit)
static inline void gdc_yuv_semiplanar_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 5) | (curr & 0xffffffdf);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_yuv_semiplanar_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x20) >> 5);
}
// ----------------------------------- //
// Register: YUV422 linear mode supported
// ----------------------------------- //

// ----------------------------------- //
// YUV4:2:2 linear mode supported (16 bit/pixel)
// ----------------------------------- //

#define GDC_YUV422_LINEAR_MODE_SUPPORTED_DEFAULT (0x0)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_DATASIZE (1)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_OFFSET (0x68)
#define GDC_YUV422_LINEAR_MODE_SUPPORTED_MASK (0x40)

// args: data (1-bit)
static inline void gdc_yuv422_linear_mode_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 6) | (curr & 0xffffffbf);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_yuv422_linear_mode_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x40) >> 6);
}
// ----------------------------------- //
// Register: RGB10_10_10 supported
// ----------------------------------- //

// ----------------------------------- //
// RGB10:10:10 mode supported
// ----------------------------------- //

#define GDC_RGB10_10_10_SUPPORTED_DEFAULT (0x0)
#define GDC_RGB10_10_10_SUPPORTED_DATASIZE (1)
#define GDC_RGB10_10_10_SUPPORTED_OFFSET (0x68)
#define GDC_RGB10_10_10_SUPPORTED_MASK (0x80)

// args: data (1-bit)
static inline void gdc_rgb10_10_10_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 7) | (curr & 0xffffff7f);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_rgb10_10_10_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x80) >> 7);
}
// ----------------------------------- //
// Register: Bicubic interpolation supported
// ----------------------------------- //

// ----------------------------------- //
// 4 tap bicubic interpolation supported
// ----------------------------------- //

#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_DEFAULT (0x0)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_DATASIZE (1)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_OFFSET (0x68)
#define GDC_BICUBIC_INTERPOLATION_SUPPORTED_MASK (0x100)

// args: data (1-bit)
static inline void gdc_bicubic_interpolation_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 8) | (curr & 0xfffffeff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_bicubic_interpolation_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x100) >> 8);
}
// ----------------------------------- //
// Register: Bilinear interpolation mode 1 supported
// ----------------------------------- //

// ----------------------------------- //
// bilinear interpolation mode 1 supported {for U,V components}
// ----------------------------------- //

#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_DEFAULT (0x0)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_DATASIZE (1)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_OFFSET (0x68)
#define GDC_BILINEAR_INTERPOLATION_MODE_1_SUPPORTED_MASK (0x200)

// args: data (1-bit)
static inline void
gdc_bilinear_interpolation_mode_1_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 9) | (curr & 0xfffffdff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_bilinear_interpolation_mode_1_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x200) >> 9);
}
// ----------------------------------- //
// Register: Bilinear interpolation mode 2 supported
// ----------------------------------- //

// ----------------------------------- //
// bilinear interpolation mode 2 supported {for U,V components}
// ----------------------------------- //

#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_DEFAULT (0x0)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_DATASIZE (1)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_OFFSET (0x68)
#define GDC_BILINEAR_INTERPOLATION_MODE_2_SUPPORTED_MASK (0x400)

// args: data (1-bit)
static inline void
gdc_bilinear_interpolation_mode_2_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1) << 10) | (curr & 0xfffffbff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_bilinear_interpolation_mode_2_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x400) >> 10);
}
// ----------------------------------- //
// Register: Output of interpolation coordinates supported
// ----------------------------------- //

// ----------------------------------- //
// output of interpolation coordinates is supported
// ----------------------------------- //

#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_DEFAULT (0x0)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_DATASIZE (1)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_OFFSET (0x68)
#define GDC_OUTPUT_OF_INTERPOLATION_COORDINATES_SUPPORTED_MASK (0x800)

// args: data (1-bit)
static inline void
gdc_output_of_interpolation_coordinates_supported_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x1) << 11) | (curr & 0xfffff7ff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t
gdc_output_of_interpolation_coordinates_supported_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x800) >> 11);
}
// ----------------------------------- //
// Register: Reserved for future use 4
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_4_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_4_DATASIZE (4)
#define GDC_RESERVED_FOR_FUTURE_USE_4_OFFSET (0x68)
#define GDC_RESERVED_FOR_FUTURE_USE_4_MASK (0xf000)

// args: data (4-bit)
static inline void gdc_reserved_for_future_use_4_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0xf) << 12) | (curr & 0xffff0fff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_reserved_for_future_use_4_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0xf000) >> 12);
}
// ----------------------------------- //
// Register: Size of output cache
// ----------------------------------- //

// ----------------------------------- //
// log2(size of output cache in lines)-5 (0 - 32lines, 1 - 64 lines etc)
// ----------------------------------- //

#define GDC_SIZE_OF_OUTPUT_CACHE_DEFAULT (0x0)
#define GDC_SIZE_OF_OUTPUT_CACHE_DATASIZE (3)
#define GDC_SIZE_OF_OUTPUT_CACHE_OFFSET (0x68)
#define GDC_SIZE_OF_OUTPUT_CACHE_MASK (0x70000)

// args: data (3-bit)
static inline void gdc_size_of_output_cache_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x7) << 16) | (curr & 0xfff8ffff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_size_of_output_cache_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x70000) >> 16);
}
// ----------------------------------- //
// Register: Size of tile cache
// ----------------------------------- //

// ----------------------------------- //
// log2(size of tile cache in 16x16 clusters)
// ----------------------------------- //

#define GDC_SIZE_OF_TILE_CACHE_DEFAULT (0x0)
#define GDC_SIZE_OF_TILE_CACHE_DATASIZE (5)
#define GDC_SIZE_OF_TILE_CACHE_OFFSET (0x68)
#define GDC_SIZE_OF_TILE_CACHE_MASK (0xf80000)

// args: data (5-bit)
static inline void gdc_size_of_tile_cache_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val =  ((data & 0x1f) << 19) | (curr & 0xff07ffff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_size_of_tile_cache_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0xf80000) >> 19);
}
// ----------------------------------- //
// Register: Nuimber of polyphase filter banks
// ----------------------------------- //

// ----------------------------------- //
// log2(number of polyphase filter banks)
// ----------------------------------- //

#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_DEFAULT (0x0)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_DATASIZE (3)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_OFFSET (0x68)
#define GDC_NUIMBER_OF_POLYPHASE_FILTER_BANKS_MASK (0x7000000)

// args: data (3-bit)
static inline void gdc_nuimber_of_polyphase_filter_banks_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x7) << 24) | (curr & 0xf8ffffff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_nuimber_of_polyphase_filter_banks_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x7000000) >> 24);
}
// ----------------------------------- //
// Register: AXI data width
// ----------------------------------- //

// ----------------------------------- //
// log2(AXI_DATA_WIDTH)-5
// ----------------------------------- //

#define GDC_AXI_DATA_WIDTH_DEFAULT (0x0)
#define GDC_AXI_DATA_WIDTH_DATASIZE (3)
#define GDC_AXI_DATA_WIDTH_OFFSET (0x68)
#define GDC_AXI_DATA_WIDTH_MASK (0x38000000)

// args: data (3-bit)
static inline void gdc_axi_data_width_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x7) << 27) | (curr & 0xc7ffffff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_axi_data_width_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0x38000000) >> 27);
}
// ----------------------------------- //
// Register: Reserved for future use 5
// ----------------------------------- //

#define GDC_RESERVED_FOR_FUTURE_USE_5_DEFAULT (0x0)
#define GDC_RESERVED_FOR_FUTURE_USE_5_DATASIZE (2)
#define GDC_RESERVED_FOR_FUTURE_USE_5_OFFSET (0x68)
#define GDC_RESERVED_FOR_FUTURE_USE_5_MASK (0xc0000000)

// args: data (2-bit)
static inline void gdc_reserved_for_future_use_5_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0x68L);
	uint32_t val = ((data & 0x3) << 30) | (curr & 0x3fffffff);

	system_gdc_write_32(0x68L, val);
}
static inline uint8_t gdc_reserved_for_future_use_5_read(void)
{
	return (uint8_t)((system_gdc_read_32(0x68L) & 0xc0000000) >> 30);
}
// ----------------------------------- //
// Register: default ch1
// ----------------------------------- //

// ----------------------------------- //
// Default value for 1st data channel (Y/R color)
// to fill missing pixels (when coordinated
// are out of bound). LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH1_DEFAULT (0x0)
#define GDC_DEFAULT_CH1_DATASIZE (12)
#define GDC_DEFAULT_CH1_OFFSET (0x70)
#define GDC_DEFAULT_CH1_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch1_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x70L);
	uint32_t  val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x70L, val);
}
static inline uint16_t gdc_default_ch1_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x70L) & 0xfff) >> 0);
}
// ----------------------------------- //
// Register: default ch2
// ----------------------------------- //

// ----------------------------------- //
// Default value for 2nd data channel
// (U/G color) to fill missing pixels
// (when coordinated are out of bound) LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH2_DEFAULT (0x0)
#define GDC_DEFAULT_CH2_DATASIZE (12)
#define GDC_DEFAULT_CH2_OFFSET (0x74)
#define GDC_DEFAULT_CH2_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch2_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x74L);
	uint32_t val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x74L, val);
}
static inline uint16_t gdc_default_ch2_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x74L) & 0xfff) >> 0);
}
// ----------------------------------- //
// Register: default ch3
// ----------------------------------- //

// ----------------------------------- //
// Default value for 3rd data channel
// (V/B color) to fill missing pixels
// (when coordinated are out of bound) LSB aligned
// ----------------------------------- //

#define GDC_DEFAULT_CH3_DEFAULT (0x0)
#define GDC_DEFAULT_CH3_DATASIZE (12)
#define GDC_DEFAULT_CH3_OFFSET (0x78)
#define GDC_DEFAULT_CH3_MASK (0xfff)

// args: data (12-bit)
static inline void gdc_default_ch3_write(uint16_t data)
{
	uint32_t curr = system_gdc_read_32(0x78L);
	uint32_t val = ((data & 0xfff) << 0) | (curr & 0xfffff000);

	system_gdc_write_32(0x78L, val);
}
static inline uint16_t gdc_default_ch3_read(void)
{
	return (uint16_t)((system_gdc_read_32(0x78L) & 0xfff) >> 0);
}
// ----------------------------------- //
// Group: GDC diagnostics
// ----------------------------------- //

// ----------------------------------- //
// Register: cfg_stall_count0
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configutation FIFO to tile reader
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_OFFSET (0x80)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT0_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_cfg_stall_count0_read(void)
{
	return system_gdc_read_32(0x80L);
}
// ----------------------------------- //
// Register: cfg_stall_count1
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configutation FIFO to CIM
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_OFFSET (0x84)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT1_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_cfg_stall_count1_read(void)
{
	return system_gdc_read_32(0x84L);
}
// ----------------------------------- //
// Register: cfg_stall_count2
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configutation FIFO to PIM
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_OFFSET (0x88)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT2_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_cfg_stall_count2_read(void)
{
	return system_gdc_read_32(0x88L);
}
// ----------------------------------- //
// Register: cfg_stall_count3
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configutation FIFO to write cache
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_OFFSET (0x8c)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT3_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_cfg_stall_count3_read(void)
{
	return system_gdc_read_32(0x8cL);
}
// ----------------------------------- //
// Register: cfg_stall_count4
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on stalls on configutation FIFO to tile writer
// ----------------------------------- //

#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_DATASIZE (32)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_OFFSET (0x90)
#define GDC_DIAGNOSTICS_CFG_STALL_COUNT4_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_cfg_stall_count4_read(void)
{
	return system_gdc_read_32(0x90L);
}
// ----------------------------------- //
// Register: int_read_stall_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on pixel interpolator read pixel stream
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_OFFSET (0x94)
#define GDC_DIAGNOSTICS_INT_READ_STALL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_int_read_stall_count_read(void)
{
	return system_gdc_read_32(0x94L);
}
// ----------------------------------- //
// Register: int_coord_stall_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on coordinate stream of pixel interpolator
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_OFFSET (0x98)
#define GDC_DIAGNOSTICS_INT_COORD_STALL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_int_coord_stall_count_read(void)
{
	return system_gdc_read_32(0x98L);
}
// ----------------------------------- //
// Register: int_write_wait_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on pixel interpolator output pixel stream
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_OFFSET (0x9c)
#define GDC_DIAGNOSTICS_INT_WRITE_WAIT_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_int_write_wait_count_read(void)
{
	return system_gdc_read_32(0x9cL);
}
// ----------------------------------- //
// Register: wrt_write_wait_count
// ----------------------------------- //

// ----------------------------------- //
// Cycles spent on waiting on sending word from write cache to tile writer
// ----------------------------------- //

#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_OFFSET (0xa0)
#define GDC_DIAGNOSTICS_WRT_WRITE_WAIT_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_wrt_write_wait_count_read(void)
{
	return system_gdc_read_32(0xa0L);
}
// ----------------------------------- //
// Register: int_dual_count
// ----------------------------------- //

// ----------------------------------- //
// Number of beats on output of tile writer
// interface where 2 pixels were interpolated.
// ----------------------------------- //

#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_DEFAULT (0x0)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_DATASIZE (32)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_OFFSET (0xa4)
#define GDC_DIAGNOSTICS_INT_DUAL_COUNT_MASK (0xffffffff)

// args: data (32-bit)
static inline uint32_t gdc_diagnostics_int_dual_count_read(void)
{
	return system_gdc_read_32(0xa4L);
}
// ----------------------------------------------------//
// Group: AXI Settings
// ----------------------------------------------------//
// ----------------------------------------------------//
// Register: config reader max arlen
// ----------------------------------------------------//
// ----------------------------------------------------//
// Maximum value to use for arlen (axi burst length).
// "0000"= max 1 transfer/burst
// upto "1111"= max 16 transfers/burst
// ----------------------------------------------------//

#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_MAX_ARLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_config_reader_max_arlen_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xa8L);
	uint32_t val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xa8L, val);
}
static inline uint8_t gdc_axi_settings_config_reader_max_arlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L) & 0xf) >> 0);
}
// ----------------------------------- //
// Register: config reader fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words space in fifo before AXI read burst(s) start
// (legal values = max_burst_length(max_arlen+1)
// to 2**fifo_aw, but workable value
// for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough space for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_config_reader_fifo_watermark_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xa8L);
	uint32_t val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xa8L, val);
}
static inline uint8_t gdc_axi_settings_config_reader_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L) & 0xff00) >> 8);
}
// ----------------------------------- //
// Register: config reader rxact maxostand
// ----------------------------------- //
// ----------------------------------- //
// Max outstanding read transactions (bursts) allowed.
// zero means no maximum(uses fifo size as max)
// ----------------------------------- //

#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_OFFSET (0xa8)
#define GDC_AXI_SETTINGS_CONFIG_READER_RXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_config_reader_rxact_maxostand_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xa8L);
	uint32_t val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xa8L, val);
}
static inline uint8_t gdc_axi_settings_config_reader_rxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xa8L) & 0xff0000) >> 16);
}
// ----------------------------------- //
// Register: tile reader max arlen
// ----------------------------------- //

// ----------------------------------- //
// Maximum value to use for arlen (axi burst length).
// "0000"= max 1 transfer/burst , upto "1111"= max 16 transfers/burst
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_MAX_ARLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_tile_reader_max_arlen_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xacL);
	uint32_t val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xacL, val);
}
static inline uint8_t gdc_axi_settings_tile_reader_max_arlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL) & 0xf) >> 0);
}
// ----------------------------------- //
// Register: tile reader fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words space in fifo before AXI read burst(s) start
// (legal values = max_burst_length(max_arlen+1) to 2**fifo_aw,
// but workable value for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough space for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_reader_fifo_watermark_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xacL);
	uint32_t val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xacL, val);
}
static inline uint8_t gdc_axi_settings_tile_reader_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL) & 0xff00) >> 8);
}
// ----------------------------------- //
// Register: tile reader rxact maxostand
// ----------------------------------- //

// ----------------------------------- //
// Max outstanding read transactions
// (bursts) allowed. zero means no maximum(uses fifo size as max).
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_OFFSET (0xac)
#define GDC_AXI_SETTINGS_TILE_READER_RXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_reader_rxact_maxostand_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xacL);
	uint32_t val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xacL, val);
}
static inline uint8_t gdc_axi_settings_tile_reader_rxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xacL) & 0xff0000) >> 16);
}
// ----------------------------------- //
// Register: tile writer max awlen
// ----------------------------------- //

// ----------------------------------- //
// Maximum value to use for awlen (axi burst length).
// "0000"= max 1 transfer/burst , upto "1111"= max 16 transfers/burst
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_DEFAULT (0xF)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_DATASIZE (4)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_MAX_AWLEN_MASK (0xf)

// args: data (4-bit)
static inline void gdc_axi_settings_tile_writer_max_awlen_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xb0L);
	uint32_t val = ((data & 0xf) << 0) | (curr & 0xfffffff0);

	system_gdc_write_32(0xb0L, val);
}
static inline uint8_t gdc_axi_settings_tile_writer_max_awlen_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L) & 0xf) >> 0);
}
// ----------------------------------- //
// Register: tile writer fifo watermark
// ----------------------------------- //

// ----------------------------------- //
// Number of words in fifo before AXI write burst(s) start
// (legal values = max_burst_length(max_awlen+1) to 2**fifo_aw,
// but workable value for your system are probably less!).
// Allowing n back to back bursts to generated
// if watermark is set to n*burst length.
// Burst(s) continue while fifo has enough for next burst.
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_DEFAULT (0x10)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_FIFO_WATERMARK_MASK (0xff00)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_writer_fifo_watermark_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xb0L);
	uint32_t val = ((data & 0xff) << 8) | (curr & 0xffff00ff);

	system_gdc_write_32(0xb0L, val);
}
static inline uint8_t
gdc_axi_settings_tile_writer_fifo_watermark_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L) & 0xff00) >> 8);
}
// ----------------------------------- //
// Register: tile writer wxact maxostand
// ----------------------------------- //

// ----------------------------------- //
// Max outstanding write transactions (bursts)
// allowed. zero means no maximum(uses internal limit of 2048)
// ----------------------------------- //

#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_DEFAULT (0x00)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_DATASIZE (8)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_OFFSET (0xb0)
#define GDC_AXI_SETTINGS_TILE_WRITER_WXACT_MAXOSTAND_MASK (0xff0000)

// args: data (8-bit)
static inline void
gdc_axi_settings_tile_writer_wxact_maxostand_write(uint8_t data)
{
	uint32_t curr = system_gdc_read_32(0xb0L);
	uint32_t val = ((data & 0xff) << 16) | (curr & 0xff00ffff);

	system_gdc_write_32(0xb0L, val);
}
static inline uint8_t gdc_axi_settings_tile_writer_wxact_maxostand_read(void)
{
	return (uint8_t)((system_gdc_read_32(0xb0L) & 0xff0000) >> 16);
}
// ----------------------------------- //
#endif //__GDC_CONFIG_H__
