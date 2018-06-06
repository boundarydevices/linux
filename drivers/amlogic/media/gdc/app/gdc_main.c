/*
 * drivers/amlogic/media/gdc/app/gdc_main.c
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

#include "system_log.h"

#include <linux/interrupt.h>
#include <linux/kernel.h>
//gdc api functions
#include "gdc_api.h"

irqreturn_t interrupt_handler_next(int irq, void *param)
{
	//handle the start of frame with gdc_process
	struct gdc_settings *gdc_settings = (struct gdc_settings *)param;

	gdc_get_frame(gdc_settings);

	return IRQ_HANDLED;
}

int gdc_run(struct gdc_settings *g)
{

	gdc_stop(g);

	LOG(LOG_INFO, "Done gdc load..\n");

	//initialise the gdc by the first configuration
	if (gdc_init(g) != 0) {
		LOG(LOG_ERR, "Failed to initialise GDC block");
		return -1;
	}

	LOG(LOG_INFO, "Done gdc config..\n");

	//start gdc process with input address for y and uv planes
	if (g->gdc_config.format == NV12) {
		gdc_process(g,
				(uint32_t)g->y_base_addr,
				(uint32_t)g->uv_base_addr);
	} else {
		gdc_process_yuv420p(g, (uint32_t)g->y_base_addr,
				(uint32_t)g->u_base_addr,
				(uint32_t)g->v_base_addr);
	}
	LOG(LOG_DEBUG, "call gdc process\n");

	return 0;
}
