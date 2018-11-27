/*
 * drivers/amlogic/media/gdc/inc/api/gdc_api.h
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

#ifndef __GDC_API_H__
#define __GDC_API_H__

#include <linux/of_address.h>
#include <linux/dma-direction.h>

struct gdc_buf_cfg {
	uint32_t type;
	unsigned long len;
};

// each configuration addresses and size
struct gdc_config {
	uint32_t format;
	uint32_t config_addr;   //gdc config address
	uint32_t config_size;   //gdc config size in 32bit
	uint32_t input_width;  //gdc input width resolution
	uint32_t input_height; //gdc input height resolution
	uint32_t input_y_stride; //gdc input y stride resolution
	uint32_t input_c_stride; //gdc input uv stride
	uint32_t output_width;  //gdc output width resolution
	uint32_t output_height; //gdc output height resolution
	uint32_t output_y_stride; //gdc output y stride
	uint32_t output_c_stride; //gdc output uv stride
};

// overall gdc settings and state
struct gdc_settings {
	uint32_t magic;
	//writing/reading to gdc base address, currently not read by api
	uint32_t base_gdc;
	 //array of gdc configuration and sizes
	struct gdc_config gdc_config;
	//update this index for new config
	//int gdc_config_total;
	//start memory to write gdc output framse
	uint32_t buffer_addr;
	//size of memory output frames to determine
	//if it is enough and can do multiple write points
	uint32_t buffer_size;
	//current output address of gdc
	uint32_t current_addr;
	//set when expecting an interrupt from gdc
	int32_t is_waiting_gdc;

	int32_t in_fd;  //input buffer's share fd
	int32_t out_fd; //output buffer's share fd

	//input address for y and u, v planes
	uint32_t y_base_addr;
	union {
		uint32_t uv_base_addr;
		uint32_t u_base_addr;
	};
	uint32_t v_base_addr;
	//opaque address in ddr added with offset to
	//write the gdc config sequence
	void *ddr_mem;
	//when inititialised this callback will be called
	//to update frame buffer addresses and offsets
	void (*get_frame_buffer)(uint32_t y_base_addr,
			uint32_t uv_base_addr,
			uint32_t y_line_offset,
			uint32_t uv_line_offset);
	void *fh;
	int32_t y_base_fd;
	union {
		int32_t uv_base_fd;
		int32_t u_base_fd;
	};
	int32_t v_base_fd;
};

#define GDC_IOC_MAGIC  'G'
#define GDC_PROCESS	 _IOW(GDC_IOC_MAGIC, 0x00, struct gdc_settings)
#define GDC_PROCESS_NO_BLOCK	_IOW(GDC_IOC_MAGIC, 0x01, struct gdc_settings)
#define GDC_RUN	_IOW(GDC_IOC_MAGIC, 0x02, struct gdc_settings)
#define GDC_REQUEST_BUFF _IOW(GDC_IOC_MAGIC, 0x03, struct gdc_settings)
#define GDC_HANDLE _IOW(GDC_IOC_MAGIC, 0x04, struct gdc_settings)

enum {
	INPUT_BUFF_TYPE = 0x1000,
	OUTPUT_BUFF_TYPE,
	CONFIG_BUFF_TYPE,
	GDC_BUFF_TYPE_MAX
};

enum {
	NV12 = 1,
	YV12,
	Y_GREY,
	YUV444_P,
	RGB444_P,
	FMT_MAX
};

struct gdc_dma_cfg {
	int fd;
	void *dev;
	void *vaddr;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	enum dma_data_direction dir;
};

/**
 *   Configure the output gdc configuration
 *
 *   address/size and buffer address/size; and resolution.
 *
 *   More than one gdc settings can be accessed by index to a gdc_config_t.
 *
 *   @param  gdc_settings - overall gdc settings and state
 *   @param  gdc_config_num - selects the current gdc config to be applied
 *
 *   @return 0 - success
 *	 -1 - fail.
 */
int gdc_init(struct gdc_settings *gdc_settings);
/**
 *   This function stops the gdc block
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 */
void gdc_stop(struct gdc_settings *gdc_settings);

/**
 *   This function starts the gdc block
 *
 *   Writing 0->1 transition is necessary for trigger
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 */
void gdc_start(struct gdc_settings *gdc_settings);

/**
 *   This function points gdc to
 *
 *   its input resolution and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_settings - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  uv_base_addr - input UV base address
 *   @param  y_line_offset - input Y line buffer offset
 *   @param  uv_line_offset-  input UV line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process(struct gdc_settings *gdc_settings,
		uint32_t y_base_addr,
		uint32_t uv_base_addr);
int gdc_process_yuv420p(struct gdc_settings *gdc_settings,
		uint32_t y_base_addr,
		uint32_t u_base_addr,
		uint32_t v_base_addr);
int gdc_process_y_grey(struct gdc_settings *gdc_settings,
		uint32_t y_base_addr);
int gdc_process_yuv444p(struct gdc_settings *gdc_settings,
		uint32_t y_base_addr,
		uint32_t u_base_addr,
		uint32_t v_base_addr);
int gdc_process_rgb444p(struct gdc_settings *gdc_settings,
		uint32_t y_base_addr,
		uint32_t u_base_addr,
		uint32_t v_base_addr);

/**
 *   This function gets the GDC output frame addresses
 *
 *   and offsets and updates the frame buffer via callback
 *
 *   if it is available Shown ouputs to GDC are
 *
 *   Y and UV plane address and offsets
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 *   @return 0 - success
 *	 -1 - unexpected interrupt from GDC.
 */
int gdc_get_frame(struct gdc_settings *gdc_settings);

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_run(struct gdc_settings *g);

int32_t init_gdc_io(struct device_node *dn);

#endif
