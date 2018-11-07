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
#define VM_STACK_AREA_SIZE		SZ_512M

#define STACK_TOP_PAGE_OFF		(THREAD_SIZE - PAGE_SIZE)

#define MAX_TASKS			(VM_STACK_AREA_SIZE / THREAD_SIZE)

#define VMAP_PAGE_FLAG			(__GFP_ZERO | __GFP_HIGH |\
					 __GFP_ATOMIC | __GFP_REPEAT)

#define VMAP_CACHE_PAGE_ORDER		5
#define VMAP_CACHE_PAGE			(1 << VMAP_CACHE_PAGE_ORDER)
#define CACHE_MAINTAIN_DELAY		(HZ)

struct aml_vmap {
	unsigned int start_bit;
	int cached_pages;
	struct vm_struct *root_vm;
	unsigned long *bitmap;
	struct list_head list;
	spinlock_t vmap_lock;
	spinlock_t page_lock;
	struct delayed_work mwork;
};

extern int handle_vmap_fault(unsigned long addr,
			     unsigned int esr, struct pt_regs *regs);

extern DEFINE_PER_CPU(unsigned long [THREAD_SIZE/sizeof(long)], vmap_stack);
static inline bool on_vmap_stack(unsigned long sp, int cpu)
{
	/* variable names the same as kernel/stacktrace.c */
	unsigned long low = (unsigned long)per_cpu(vmap_stack, cpu);
	unsigned long high = low + THREAD_START_SP;

	return (low <= sp && sp <= high);
}

extern void  __setup_vmap_stack(unsigned long off);
extern void  update_vmap_stack(int diff);
extern int   get_vmap_stack_size(void);
extern void  aml_stack_free(struct task_struct *tsk);
extern void *aml_stack_alloc(int node, struct task_struct *tsk);
extern void  aml_account_task_stack(struct task_struct *tsk, int account);
#endif /* __VMAP_STACK_H__ */
