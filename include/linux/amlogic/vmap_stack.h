/*
 * include/linux/amlogic/vmap_stack.h
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

#ifndef __VMAP_STACK_H__
#define __VMAP_STACK_H__

#define STACK_SHRINK_THRESHOLD		(PAGE_SIZE + 1024)
#define STACK_SHRINK_SLEEP		(HZ)
#ifdef CONFIG_64BIT
#define VM_STACK_AREA_SIZE		SZ_512M
#define VMAP_ADDR_START			VMALLOC_START
#define VMAP_ADDR_END			VMALLOC_END
#define VMAP_ALIGN			VM_STACK_AREA_SIZE
#else
/* currently support max 6144 tasks on 32bit */
#define VM_STACK_AREA_SIZE		(SZ_64M - SZ_16M)
#define VMAP_ADDR_START			MODULES_VADDR
#define VMAP_ADDR_END			MODULES_END
#define VMAP_ALIGN			SZ_64M
#endif

#define STACK_TOP_PAGE_OFF		(THREAD_SIZE - PAGE_SIZE)

#define MAX_TASKS			(VM_STACK_AREA_SIZE / THREAD_SIZE)

#define VMAP_PAGE_FLAG			(__GFP_ZERO | __GFP_HIGH |\
					 __GFP_ATOMIC | __GFP_REPEAT)

#define VMAP_CACHE_PAGE_ORDER		5
#define VMAP_CACHE_PAGE			(1 << VMAP_CACHE_PAGE_ORDER)
#define CACHE_MAINTAIN_DELAY		(HZ)

struct aml_vmap {
	spinlock_t vmap_lock;
	unsigned int start_bit;
	int cached_pages;
	struct vm_struct *root_vm;
	unsigned long *bitmap;
	struct list_head list;
	struct delayed_work mwork;
	spinlock_t page_lock;
};

extern int handle_vmap_fault(unsigned long addr,
			     unsigned int esr, struct pt_regs *regs);

extern bool on_vmap_stack(unsigned long sp, int cpu);
extern void  __setup_vmap_stack(unsigned long off);
extern void  update_vmap_stack(int diff);
extern int   get_vmap_stack_size(void);
extern int   is_vmap_addr(unsigned long addr);
extern void  aml_stack_free(struct task_struct *tsk);
extern void *aml_stack_alloc(int node, struct task_struct *tsk);
extern void  aml_account_task_stack(struct task_struct *tsk, int account);
#ifdef CONFIG_ARM
extern int   on_irq_stack(unsigned long sp, int cpu);
#endif
#endif /* __VMAP_STACK_H__ */
