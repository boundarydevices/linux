/*
 * drivers/amlogic/media/common/vfm/vftrace.h
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

#ifndef VFTRACE_HANDLE_
#define VFTRACE_HANDLE_
#include <linux/amlogic/media/vfm/vframe.h>
void *vftrace_alloc_trace(const char *name, int get, int max);
void vftrace_free_trace(void *handle);
void vftrace_info_in(void *vhandle, struct vframe_s *vf);
void vftrace_dump_trace_infos(void *vhandle);

#endif
