/*
 * drivers/amlogic/media/common/rdma/rdma.h
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

#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_

enum {
	VSYNC_RDMA,
	LINE_N_INT_RDMA,
};
void vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
extern int rdma_watchdog_setting(int flag);
int rdma_init2(void);
struct rdma_op_s *get_rdma_ops(int rdma_type);
void set_rdma_handle(int rdma_type, int handle);

#endif
