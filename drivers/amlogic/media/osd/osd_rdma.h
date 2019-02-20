/*
 * drivers/amlogic/media/osd/osd_rdma.h
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

#ifndef _OSD_RDMA_H_
#define _OSD_RDMA_H_

#include <linux/types.h>
#include "osd_io.h"
#include "osd_reg.h"

extern int rdma_watchdog_setting(int flag);

struct rdma_table_item {
	u32 addr;
	u32 val;
};

#define TABLE_SIZE	 PAGE_SIZE
#define MAX_TABLE_ITEM	 (TABLE_SIZE/sizeof(struct rdma_table_item_t))
#define OSD_RDMA_CHANNEL_INDEX	osd_rdma_handle
#define START_ADDR	(RDMA_AHB_START_ADDR_MAN+(OSD_RDMA_CHANNEL_INDEX<<1))
#define END_ADDR	(RDMA_AHB_END_ADDR_MAN+(OSD_RDMA_CHANNEL_INDEX<<1))

#define OSD_RDMA_FLAG_REG	VIU_OSD2_TCOLOR_AG3

#define OSD_RDMA_FLAG_REJECT	(0x99 << 0)
/* hw rdma own this flag, change it to zero when start rdma,
 *  change it to 0 when complete
 */

#define  OSD_RDMA_STATUS_IS_REJECT \
	(osd_reg_read(OSD_RDMA_FLAG_REG) & OSD_RDMA_FLAG_REJECT)

/* hw rdma op, set REJECT */
#define  OSD_RDMA_STATUS_MARK_TBL_RST \
	((osd_reg_read(OSD_RDMA_FLAG_REG) \
	& ~OSD_RDMA_FLAG_REJECT) | \
	(OSD_RDMA_FLAG_REJECT))

#define  OSD_RDMA_STATUS_MARK_TBL_DONE \
	((osd_reg_read(OSD_RDMA_FLAG_REG) \
	& ~OSD_RDMA_FLAG_REJECT)) \

/* cpu op, clear REJECT */
#define  OSD_RDMA_STATUS_CLEAR_REJECT \
	(osd_reg_write(OSD_RDMA_FLAG_REG, \
	(osd_reg_read(OSD_RDMA_FLAG_REG) & \
	~OSD_RDMA_FLAG_REJECT)))


extern int read_rdma_table(void);
extern int osd_rdma_enable(u32 enable);
extern int osd_rdma_reset_and_flush(u32 reset_bit);
extern int rdma_reset_tigger_flag;
extern int rdma_mgr_irq_request;
extern void osd_rdma_interrupt_done_clear(void);
extern int osd_rdma_uninit(void);
void set_reset_rdma_trigger_line(void);
void enable_line_n_rdma(void);
void enable_vsync_rdma(void);
#endif
