/*
 * drivers/amlogic/media/gdc/src/fw_lib/acamera_gdc.c
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

//needed for gdc/gdc configuration
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

//data types and prototypes
#include "gdc_api.h"
#include "system_log.h"
#include "gdc_config.h"


/**
 *   Configure the output gdc configuration address/size
 *
 *   and buffer address/size; and resolution.
 *
 *   More than one gdc settings can be accessed by index to a gdc_config_t.
 *
 *   @param  gdc_settings - overall gdc settings and state
 *   @param  gdc_config_num - selects the current gdc config to be applied
 *
 *   @return 0 - success
 *	 -1 - fail.
 */
int gdc_init(struct gdc_settings *gdc_settings)
{

	gdc_settings->is_waiting_gdc = 0;
	gdc_settings->current_addr = gdc_settings->buffer_addr;

	if ((gdc_settings->gdc_config.output_width == 0)
			|| (gdc_settings->gdc_config.output_height == 0)) {
		LOG(LOG_ERR, "Wrong GDC output resolution.\n");
		return -1;
	}
	//stop gdc
	gdc_start_flag_write(0);
	//set the configuration address and size to the gdc block
	gdc_config_addr_write(gdc_settings->gdc_config.config_addr);
	gdc_config_size_write(gdc_settings->gdc_config.config_size);

	//set the gdc output resolution
	gdc_dataout_width_write(gdc_settings->gdc_config.output_width);
	gdc_dataout_height_write(gdc_settings->gdc_config.output_height);

	return 0;
}

/**
 *   This function stops the gdc block
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 */
void gdc_stop(struct gdc_settings *gdc_settings)
{
	gdc_settings->is_waiting_gdc = 0;
	gdc_start_flag_write(0);
}

/**
 *   This function starts the gdc block
 *
 *   Writing 0->1 transition is necessary for trigger
 *
 *   @param  gdc_settings - overall gdc settings and state
 *
 */
void gdc_start(struct gdc_settings *gdc_settings)
{
	gdc_start_flag_write(0); //do a stop for sync
	gdc_start_flag_write(1);
	gdc_settings->is_waiting_gdc = 1;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
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
		uint32_t y_base_addr, uint32_t uv_base_addr)
{
	uint32_t gdc_out_base_addr = gdc_settings->current_addr;
	uint32_t active_width = gdc_settings->gdc_config.output_width;
	uint32_t active_height = gdc_settings->gdc_config.output_height;
	uint32_t y_line_offset = active_width; //420 format
	uint32_t uv_line_offset = active_width; //420 format

	if (gdc_settings->is_waiting_gdc) {
		gdc_start_flag_write(0);
		LOG(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1);

		return -1;
	}

	LOG(LOG_DEBUG, "starting GDC process.\n");

	gdc_datain_width_write(active_width);
	gdc_datain_height_write(active_height);
	//input y plane
	gdc_data1in_addr_write(y_base_addr);
	gdc_data1in_line_offset_write(y_line_offset);

	//input uv plane
	gdc_data2in_addr_write(uv_base_addr);
	gdc_data2in_line_offset_write(uv_line_offset);

	//gdc y output
	gdc_data1out_addr_write(gdc_out_base_addr);
	gdc_data1out_line_offset_write(y_line_offset);

	//gdc uv output
	gdc_out_base_addr += active_height * y_line_offset;
	gdc_data2out_addr_write(gdc_out_base_addr);
	gdc_data2out_line_offset_write(uv_line_offset);

	gdc_start(gdc_settings);

	return 0;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
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
int gdc_process_yuv420p(struct gdc_settings *gdc_settings,
	uint32_t y_base_addr, uint32_t u_base_addr, uint32_t v_base_addr)
{
	struct gdc_config  *gc = &gdc_settings->gdc_config;
	uint32_t gdc_out_base_addr = gdc_settings->current_addr;
	uint32_t input_width = gc->input_width;
	uint32_t input_height = gc->input_height;
	uint32_t input_stride = gc->input_y_stride;
	uint32_t input_u_stride = gc->input_c_stride;
	uint32_t input_v_stride = gc->input_c_stride;

	LOG(LOG_DEBUG, "is_waiting_gdc=%d\n", gdc_settings->is_waiting_gdc);
	if (gdc_settings->is_waiting_gdc) {
		gdc_start_flag_write(0);
		LOG(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1);
		return -1;
	}

	/////
	LOG(LOG_DEBUG, "starting GDC process.\n");

	//already set in gdc_init
	//uint32_t output_width = gc->output_width;
	uint32_t output_height = gc->output_height;
	uint32_t output_stride = gc->output_y_stride;
	uint32_t output_u_stride = gc->output_c_stride;
	uint32_t output_v_stride = gc->output_c_stride;

	gdc_datain_width_write(input_width);
	gdc_datain_height_write(input_height);
	//input y plane
	gdc_data1in_addr_write(y_base_addr);
	gdc_data1in_line_offset_write(input_stride);

	//input u plane
	gdc_data2in_addr_write(u_base_addr);
	gdc_data2in_line_offset_write(input_u_stride);

	//input v plane
	gdc_data3in_addr_write(v_base_addr);
	gdc_data3in_line_offset_write(input_v_stride);

	//gdc y output
	gdc_data1out_addr_write(gdc_out_base_addr);
	gdc_data1out_line_offset_write(output_stride);

	//gdc u output
	gdc_out_base_addr += output_height * output_stride;
	gdc_data2out_addr_write(gdc_out_base_addr);
	gdc_data2out_line_offset_write(output_u_stride);

	//gdc v output
	gdc_out_base_addr += output_height * output_u_stride / 2;
	gdc_data3out_addr_write(gdc_out_base_addr);
	gdc_data3out_line_offset_write(output_v_stride);
	gdc_start(gdc_settings);

	return 0;
}

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
int gdc_get_frame(struct gdc_settings *gdc_settings)
{
	struct mgdc_fh_s *fh = gdc_settings->fh;
	uint32_t y;
	uint32_t y_offset;
	uint32_t uv;
	uint32_t uv_offset;

	if (!gdc_settings->is_waiting_gdc) {
		LOG(LOG_CRIT, "Unexpected interrupt from GDC.\n");
		return -1;
	}
	////

	wake_up_interruptible(&fh->irq_queue);

	//pass the frame buffer parameters if callback is available
	if (gdc_settings->get_frame_buffer) {
		y = gdc_data1out_addr_read();
		y_offset = gdc_data1out_line_offset_read();
		uv = gdc_data2out_addr_read();
		uv_offset = gdc_data2out_line_offset_read();

		gdc_settings->get_frame_buffer(y,
					uv, y_offset, uv_offset);
	}
	//done of the current frame and stop gdc block
	gdc_stop(gdc_settings);
	//spin_unlock_irqrestore(&gdev->slock, flags);
	return 0;
}
