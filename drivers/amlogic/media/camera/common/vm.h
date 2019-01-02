/*
 * drivers/amlogic/media/camera/common/vm.h
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

#ifndef _VM_INCLUDE__
#define _VM_INCLUDE__

#include <linux/interrupt.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/io-mapping.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/camera/aml_cam_info.h>
#include <linux/amlogic/media/camera/vmapi.h>

/*************************************
 **
 **	macro define
 **
 *************************************/

#define VM_IOC_MAGIC  'P'
#define VM_IOC_2OSD0		_IOW(VM_IOC_MAGIC, 0x00, unsigned int)
#define VM_IOC_ENABLE_PP _IOW(VM_IOC_MAGIC, 0X01, unsigned int)
#define VM_IOC_CONFIG_FRAME  _IOW(VM_IOC_MAGIC, 0X02, unsigned int)
#define MAX_CANVAS_INDEX 12

#define VM_MAP_NAME_SIZE 100
#define VM_PROVIDER_NAME_SIZE 10

struct vm_device_s {
	unsigned int  index;
	char name[20];
	struct platform_device *pdev;
	int dump;
	char *dump_path;
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	/* struct class *cla; */
	struct cdev	cdev;
	struct device *dev;
	resource_size_t buffer_start;
	unsigned int buffer_size;
#ifdef CONFIG_CMA
	ulong cma_pool_size;
#endif
	struct vframe_receiver_s vm_vf_recv;
	struct task_struct *task;
	wait_queue_head_t frame_ready;
	int task_running;
	int vm_skip_count;
	int test_zoom;
	struct vm_output_para output_para;
	struct completion vb_start_sema;
	struct completion vb_done_sema;
	char vf_provider_name[VM_PROVIDER_NAME_SIZE];
	char vfm_map_id[VM_MAP_NAME_SIZE];
	char vfm_map_chain[VM_MAP_NAME_SIZE];
	int vm_canvas[MAX_CANVAS_INDEX];
};

struct display_frame_s {
	int frame_top;
	int frame_left;
	int frame_width;
	int frame_height;
	int content_top;
	int content_left;
	int content_width;
	int content_height;
};

int start_vm_task(struct vm_device_s *vdevp);
int start_simulate_task(void);

extern int get_vm_status(void);
extern void set_vm_status(int flag);

/* for vm device op. */
extern int init_vm_device(struct vm_device_s *vdevp,
		struct platform_device *pdev);
extern int uninit_vm_device(struct platform_device *plat_dev);

/* for vm device class op. */
extern struct class *init_vm_cls(void);

/* for thread of vm. */
extern int start_vpp_task(void);
extern void stop_vpp_task(void);

/* for vm private member. */
extern void set_vm_buf_info(resource_size_t start, unsigned int size);
extern void get_vm_buf_info(resource_size_t *start, unsigned int *size,
			    struct io_mapping **mapping);

/*  vm buffer op. */
extern int vm_buffer_init(struct vm_device_s *vdevp);
extern void vm_local_init(void);

extern void vm_deinit_resource(struct vm_init_s *info);

static DEFINE_MUTEX(vm_mutex);

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TV */
#if 1
#define CANVAS_WIDTH_ALIGN 32
#else
#define CANVAS_WIDTH_ALIGN 8
#endif

#define MAGIC_SG_MEM    0x17890714
#define MAGIC_DC_MEM    0x0733ac61
#define MAGIC_VMAL_MEM  0x18221223
#define MAGIC_RE_MEM    0x123039dc

#endif /* _VM_INCLUDE__ */
