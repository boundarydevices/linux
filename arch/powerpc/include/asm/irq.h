#ifdef __KERNEL__
#ifndef _ASM_POWERPC_IRQ_H
#define _ASM_POWERPC_IRQ_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/irqdomain.h>
#include <linux/threads.h>
#include <linux/list.h>
#include <linux/radix-tree.h>

#include <asm/types.h>
#include <asm/atomic.h>


/* Define a way to iterate across irqs. */
#define for_each_irq(i) \
	for ((i) = 0; (i) < NR_IRQS; ++(i))

extern atomic_t ppc_n_lost_interrupts;

/* This number is used when no interrupt has been assigned */
#define NO_IRQ			(0)

/* This is a special irq number to return from get_irq() to tell that
 * no interrupt happened _and_ ignore it (don't count it as bad). Some
 * platforms like iSeries rely on that.
 */
#define NO_IRQ_IGNORE		((unsigned int)-1)

/* Total number of virq in the platform */
#define NR_IRQS		CONFIG_NR_IRQS

/* Same thing, used by the generic IRQ code */
#define NR_IRQS_LEGACY		NUM_ISA_INTERRUPTS

struct irq_data;
extern irq_hw_number_t irqd_to_hwirq(struct irq_data *d);
extern irq_hw_number_t virq_to_hw(unsigned int virq);
extern bool virq_is_host(unsigned int virq, struct irq_domain *host);

/**
 * irq_alloc_virt - Allocate virtual irq numbers
 * @host: host owning these new virtual irqs
 * @count: number of consecutive numbers to allocate
 * @hint: pass a hint number, the allocator will try to use a 1:1 mapping
 *
 * This is a low level function that is used internally by irq_create_mapping()
 * and that can be used by some irq controllers implementations for things
 * like allocating ranges of numbers for MSIs. The revmaps are left untouched.
 */
extern unsigned int irq_alloc_virt(struct irq_domain *host,
				   unsigned int count,
				   unsigned int hint);

/**
 * irq_free_virt - Free virtual irq numbers
 * @virq: virtual irq number of the first interrupt to free
 * @count: number of interrupts to free
 *
 * This function is the opposite of irq_alloc_virt. It will not clear reverse
 * maps, this should be done previously by unmap'ing the interrupt. In fact,
 * all interrupts covered by the range being freed should have been unmapped
 * prior to calling this.
 */
extern void irq_free_virt(unsigned int virq, unsigned int count);

/**
 * irq_early_init - Init irq remapping subsystem
 */
extern void irq_early_init(void);

static __inline__ int irq_canonicalize(int irq)
{
	return irq;
}

extern int distribute_irqs;

struct irqaction;
struct pt_regs;

#define __ARCH_HAS_DO_SOFTIRQ

#if defined(CONFIG_BOOKE) || defined(CONFIG_40x)
/*
 * Per-cpu stacks for handling critical, debug and machine check
 * level interrupts.
 */
extern struct thread_info *critirq_ctx[NR_CPUS];
extern struct thread_info *dbgirq_ctx[NR_CPUS];
extern struct thread_info *mcheckirq_ctx[NR_CPUS];
extern void exc_lvl_ctx_init(void);
#else
#define exc_lvl_ctx_init()
#endif

/*
 * Per-cpu stacks for handling hard and soft interrupts.
 */
extern struct thread_info *hardirq_ctx[NR_CPUS];
extern struct thread_info *softirq_ctx[NR_CPUS];

extern void irq_ctx_init(void);
extern void call_do_softirq(struct thread_info *tp);
extern int call_handle_irq(int irq, void *p1,
			   struct thread_info *tp, void *func);
extern void do_IRQ(struct pt_regs *regs);

#endif /* _ASM_IRQ_H */
#endif /* __KERNEL__ */
