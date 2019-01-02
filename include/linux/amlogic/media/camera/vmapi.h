/*
 * include/linux/amlogic/media/camera/vmapi.h
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

#ifndef VM_API_INCLUDE_
#define VM_API_INCLUDE_

struct vm_output_para {
	int width;
	int height;
	int bytesperline;
	int v4l2_format;
	int index;
	int v4l2_memory;
	int zoom;     /* set -1 as invalid */
	int mirror;   /* set -1 as invalid */
	int angle;
	uintptr_t vaddr;/*unsigned*/
	unsigned int ext_canvas;
};
/*struct vm_output_para*/
struct videobuf_buffer;
struct vb2_buffer;

struct vm_init_s {
	size_t vm_buf_size;
	struct page *vm_pages;
	resource_size_t buffer_start;
	unsigned int vdin_id;
	unsigned int bt_path_count;
	bool isused;
	bool mem_alloc_succeed;
};
/*struct vm_init_s*/

int vm_fill_this_buffer(struct videobuf_buffer *vb, struct vm_output_para *para,
			struct vm_init_s *info);
int vm_fill_buffer(struct videobuf_buffer *vb, struct vm_output_para *para);

int vm_fill_buffer2(struct vb2_buffer *vb, struct vm_output_para *para);

#ifdef CONFIG_CMA
int vm_init_buf(size_t size);
int vm_init_resource(size_t size, struct vm_init_s *info);
void vm_deinit_buf(void);
void vm_deinit_resource(struct vm_init_s *info);
void vm_reserve_cma(void);
#endif

#endif /* VM_API_INCLUDE_ */
