/*
 *  linux/arch/arm/include/asm/perf_event.h
 *
 *  Copyright (C) 2009 picoChip Designs Ltd, Jamie Iles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARM_PERF_EVENT_H__
#define __ARM_PERF_EVENT_H__

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/hrtimer.h>
#endif

#ifdef CONFIG_PERF_EVENTS
struct pt_regs;
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);
extern unsigned long perf_misc_flags(struct pt_regs *regs);
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif

#define perf_arch_fetch_caller_regs(regs, __ip) { \
	(regs)->ARM_pc = (__ip); \
	(regs)->ARM_fp = (unsigned long) __builtin_frame_address(0); \
	(regs)->ARM_sp = current_stack_pointer; \
	(regs)->ARM_cpsr = SVC_MODE; \
}

#ifdef CONFIG_AMLOGIC_MODIFY

extern void armv8pmu_handle_irq_ipi(void);

struct amlpmu_fixup_cpuinfo {
	int irq_num;

	int fix_done;

	unsigned long irq_cnt;
	unsigned long empty_irq_cnt;

	unsigned long irq_time;
	unsigned long empty_irq_time;

	unsigned long last_irq_cnt;
	unsigned long last_empty_irq_cnt;

	unsigned long last_irq_time;
	unsigned long last_empty_irq_time;
};

struct amlpmu_fixup_context {
	struct amlpmu_fixup_cpuinfo __percpu *cpuinfo;

	/* struct arm_pmu */
	void *dev;

	/* sys_cpu_status0 reg */
	unsigned int *sys_cpu_status0;

	/*
	 * In main pmu irq route wait for other cpu fix done may cause lockup,
	 * when lockup we disable main irq for a while.
	 * relax_timer will enable main irq again.
	 */
	struct hrtimer relax_timer;

	/* dts prop */
	unsigned int sys_cpu_status0_offset;
	unsigned int sys_cpu_status0_pmuirq_mask;
	unsigned int relax_timer_ns;
	unsigned int max_wait_cnt;
};
#endif

#endif /* __ARM_PERF_EVENT_H__ */
