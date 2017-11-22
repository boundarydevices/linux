/*
 * drivers/amlogic/media/deinterlace/deinterlace_dbg.h
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
#ifndef _DI_DBG_H
#define _DI_DBG_H
#include "deinterlace.h"

void parse_cmd_params(char *buf_orig, char **parm);
void dump_di_pre_stru(struct di_pre_stru_s *di_pre_stru_p);
void dump_di_post_stru(struct di_post_stru_s *di_post_stru_p);
void dump_di_buf(struct di_buf_s *di_buf);
void dump_pool(struct queue_s *q);
void dump_vframe(vframe_t *vf);
void dump_di_reg(void);
void print_di_buf(struct di_buf_s *di_buf, int format);
void dump_pre_mif_state(void);
void dump_post_mif_reg(void);
void debug_device_files_add(struct device *dev);
void debug_device_files_del(struct device *dev);
#endif
