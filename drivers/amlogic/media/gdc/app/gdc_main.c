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
	struct gdc_cmd_s *gdc_cmd = (struct gdc_cmd_s *)param;

	gdc_get_frame(gdc_cmd);

	return IRQ_HANDLED;
}

int gdc_run(struct gdc_cmd_s *g)
{

	gdc_stop(g);

	gdc_log(LOG_INFO, "Done gdc load..\n");

	//initialise the gdc by the first configuration
	if (gdc_init(g) != 0) {
		gdc_log(LOG_ERR, "Failed to initialise GDC block");
		return -1;
	}

	gdc_log(LOG_INFO, "Done gdc config..\n");

	switch (g->gdc_config.format) {
	case NV12:
		gdc_process(g, g->y_base_addr, g->uv_base_addr);
	break;
	case YV12:
		gdc_process_yuv420p(g, g->y_base_addr,
				g->u_base_addr,
				g->v_base_addr);
	break;
	case Y_GREY:
		gdc_process_y_grey(g, g->y_base_addr);
	break;
	case YUV444_P:
		gdc_process_yuv444p(g, g->y_base_addr,
				g->u_base_addr,
				g->v_base_addr);
	break;
	case RGB444_P:
		gdc_process_rgb444p(g, g->y_base_addr,
				g->u_base_addr,
				g->v_base_addr);
	break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
	break;
	}
	gdc_log(LOG_DEBUG, "call gdc process\n");

	return 0;
}
