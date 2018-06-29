/*
 * drivers/amlogic/debug/debug_lockup.c
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

#include <linux/stacktrace.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/irqflags.h>
#include <linux/sched.h>
#include "../kernel/sched/sched.h"
#include <linux/moduleparam.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/uaccess.h>

/*isr trace*/
#define ns2ms			(1000 * 1000)
#define LONG_ISR		(500 * ns2ms)
#define LONG_SIRQ		(500 * ns2ms)
#define CHK_WINDOW		(1000 * ns2ms)
#define IRQ_CNT			256
#define CCCNT_WARN		1000
#define CPU			8
static unsigned long t_base[IRQ_CNT];
static unsigned long t_isr[IRQ_CNT];
static unsigned long t_total[IRQ_CNT];
static unsigned int cnt_total[IRQ_CNT];
static void *cpu_action[CPU];
static int cpu_irq[CPU] = {0};
static void *cpu_sirq[CPU] = {NULL};

/*irq disable trace*/
#define LONG_IRQDIS		(1000 * 1000000)	/*500 ms*/
#define OUT_WIN			(500 * 1000000)	/*500 ms*/
#define LONG_IDLE		(5000000000)	/*5 sec*/
#define ENTRY			10
static unsigned long		t_i_d[CPU];
static int			irq_flg;
static struct stack_trace	irq_trace[CPU];
static unsigned long		t_entrys[CPU][ENTRY];
static unsigned long		t_idle[CPU] = { 0 };
static unsigned long		t_d_out;

static unsigned long isr_thr = LONG_ISR;
core_param(isr_thr, isr_thr, ulong, 0644);
static unsigned long irq_dis_thr = LONG_IRQDIS;
core_param(irq_dis_thr, irq_dis_thr, ulong, 0644);
static unsigned long sirq_thr = LONG_SIRQ;
core_param(sirq_thr, sirq_thr, ulong, 0644);
static int irq_check_en;
core_param(irq_check_en, irq_check_en, int, 0644);
static int isr_check_en = 1;
core_param(isr_check_en, isr_check_en, int, 0644);
static unsigned long out_thr = OUT_WIN;
core_param(out_thr, out_thr, ulong, 0644);

void notrace isr_in_hook(unsigned int cpu, unsigned long *tin,
	unsigned int irq, void *act)
{
	if (irq >= IRQ_CNT || !isr_check_en || oops_in_progress)
		return;
	cpu_irq[cpu] = irq;
	cpu_action[cpu] = act;
	*tin = sched_clock();
	if (*tin >= CHK_WINDOW + t_base[irq]) {
		t_base[irq]	= *tin;
		t_isr[irq]	= 0;
		t_total[irq]	= 0;
		cnt_total[irq]	= 0;
	}
}

void notrace isr_out_hook(unsigned int cpu, unsigned long tin, unsigned int irq)
{
	unsigned long tout;
	unsigned int ratio = 0;

	if (!isr_check_en || oops_in_progress)
		return;
	if (irq >= IRQ_CNT || cpu_irq[cpu] <= 0)
		return;
	cpu_irq[cpu] = 0;
	tout = sched_clock();
	t_isr[irq] += (tout > tin) ? (tout-tin) : 0;
	t_total[irq] = (tout-t_base[irq]);
	cnt_total[irq]++;

	if (tout > isr_thr + tin)
		pr_err("ISR_Long___ERR. irq:%d  tout-tin:%ld ms\n",
			irq, (tout - tin) / ns2ms);

	if (t_total[irq] < CHK_WINDOW)
		return;

	ratio = t_isr[irq] * 100 / t_total[irq];
	if (ratio >= 35) {
		pr_err("IRQRatio___ERR.irq:%d ratio:%d\n", irq, (int)ratio);
		pr_err("t_isr:%d  t_total:%d, cnt:%d\n",
			(int)(t_isr[irq] / ns2ms),
			(int)(t_total[irq] / ns2ms),
			cnt_total[irq]);
	}
	t_base[irq] = sched_clock();
	t_isr[irq] = 0;
	t_total[irq] = 0;
	cnt_total[irq] = 0;
	cpu_action[cpu] = NULL;
}

void notrace sirq_in_hook(unsigned int cpu, unsigned long *tin, void *p)
{
	cpu_sirq[cpu] = p;
	*tin = sched_clock();
}
void notrace sirq_out_hook(unsigned int cpu, unsigned long tin, void *p)
{
	unsigned long tout = sched_clock();

	if (cpu_sirq[cpu] && tin && (tout > tin + sirq_thr) &&
		!oops_in_progress) {
		pr_err("SIRQLong___ERR. sirq:%p  tout-tin:%ld ms\n",
			p, (tout - tin) / ns2ms);
	}
	cpu_sirq[cpu] = NULL;
}

void notrace irq_trace_start(unsigned long flags)
{
	unsigned int cpu;
	int softirq = 0;

	if (!irq_flg  || !irq_check_en || oops_in_progress)
		return;
	if (arch_irqs_disabled_flags(flags))
		return;

	cpu = get_cpu();
	put_cpu();
	softirq =  task_thread_info(current)->preempt_count & SOFTIRQ_MASK;
	if ((t_idle[cpu] && !softirq) || t_i_d[cpu] || cpu_is_offline(cpu) ||
		(softirq_count() && !cpu_sirq[cpu]))
		return;

	memset(&irq_trace[cpu], 0, sizeof(irq_trace[cpu]));
	memset(&t_entrys[cpu][0], 0, sizeof(t_entrys[cpu][0])*ENTRY);
	irq_trace[cpu].entries = &t_entrys[cpu][0];
	irq_trace[cpu].max_entries = ENTRY;
	irq_trace[cpu].skip = 2;
	irq_trace[cpu].nr_entries = 0;
	t_i_d[cpu] = sched_clock();
	save_stack_trace(&irq_trace[cpu]);
}
EXPORT_SYMBOL(irq_trace_start);

void notrace irq_trace_stop(unsigned long flag)
{
	unsigned long t_i_e, t;
	unsigned int cpu;
	int softirq = 0;
	static int out_cnt;

	if (!irq_check_en ||  !irq_flg || oops_in_progress)
		return;

	if (arch_irqs_disabled_flags(flag))
		return;

	cpu = get_cpu();
	put_cpu();
	if (!t_i_d[cpu] ||
		!arch_irqs_disabled_flags(arch_local_save_flags())) {
		t_i_d[cpu] = 0;
		return;
	}

	t_i_e = sched_clock();
	if (t_i_e <  t_i_d[cpu]) {
		t_i_d[cpu] = 0;
		return;
	}

	t = (t_i_e - t_i_d[cpu]);
	softirq =  task_thread_info(current)->preempt_count & SOFTIRQ_MASK;

	if (!(t_idle[cpu] && !softirq) && (t > irq_dis_thr) && t_i_d[cpu] &&
		!(softirq_count() && !cpu_sirq[cpu])) {
		out_cnt++;
		if (t_i_e >= t_d_out + out_thr) {
			t_d_out = t_i_e;
			pr_err("\n\nDisIRQ___ERR:%ld ms <%ld %ld> %d:\n",
				t / ns2ms, t_i_e / ns2ms, t_i_d[cpu] / ns2ms,
				out_cnt);
			print_stack_trace(&irq_trace[cpu], 0);
			dump_stack();
		}
	}
	t_i_d[cpu] = 0;
}
EXPORT_SYMBOL(irq_trace_stop);

void __attribute__((weak)) lockup_hook(int cpu)
{
}

void  notrace arch_cpu_idle_enter(void)
{
	int cpu;

	if ((!irq_check_en ||  !irq_flg) && !isr_check_en)
		return;
	cpu = get_cpu();
	put_cpu();
	t_idle[cpu] = local_clock();
}
void  notrace arch_cpu_idle_exit(void)
{
	int cpu;

	if ((!irq_check_en ||  !irq_flg) && !isr_check_en)
		return;
	cpu = get_cpu();
	put_cpu();
	t_idle[cpu] = 0;
}

void pr_lockup_info(int c)
{
	int cpu;
	int virq = irq_check_en;
	int visr = isr_check_en;

	irq_flg = 0;
	irq_check_en = 0;
	isr_check_en = 0;
	console_loglevel = 7;
	pr_err("\n\n\nHARDLOCKUP____ERR.CPU[%d] <irqen:%d isren%d>START\n",
		c, virq, visr);
	for_each_online_cpu(cpu) {
		unsigned long t_cur = sched_clock();
		struct task_struct *p = (cpu_rq(cpu)->curr);
		int preempt = task_thread_info(p)->preempt_count;

		pr_err("\ndump_cpu[%d] irq:%3d preempt:%x  %s\n",
			cpu, cpu_irq[cpu], preempt,	 p->comm);
		if (preempt & HARDIRQ_MASK)
			pr_err("IRQ %pf, %x\n", cpu_action[cpu],
			(unsigned int)(preempt & HARDIRQ_MASK));
		if (preempt & SOFTIRQ_MASK)
			pr_err("SotIRQ %pf, %x\n", cpu_sirq[cpu],
			(unsigned int)(preempt & SOFTIRQ_MASK));

		if (t_i_d[cpu]) {
			pr_err("IRQ____ERR[%d]. <%ld %ld>.\n",
				cpu,  t_i_d[cpu] / ns2ms,
				(t_cur-t_i_d[cpu]) / ns2ms);
			print_stack_trace(&irq_trace[cpu], 0);
		}
		if (t_idle[cpu] && (t_idle[cpu] > LONG_IDLE + t_cur)) {
			pr_err("CPU[%d] IdleLong____ERR:%ld ms <%ld %ld>\n",
				cpu, (t_cur - t_idle[cpu]) / ns2ms,
				t_cur / ns2ms, t_idle[cpu] / ns2ms);
		}
		dump_cpu_task(cpu);
		lockup_hook(cpu);
	}
	pr_err("\nHARDLOCKUP____ERR.END\n\n");
}



static struct dentry *debug_lockup;
#define debug_fs(x)					\
static ssize_t x##_write(struct file *file, const char __user *userbuf,\
				   size_t count, loff_t *ppos)	\
{							\
	char buf[20];					\
	unsigned long val;					\
	int ret;						\
	count = min_t(size_t, count, (sizeof(buf)-1));		\
	if (copy_from_user(buf, userbuf, count))		\
		return -EFAULT;				\
	buf[count] = 0;					\
	ret = kstrtoul(buf, 0, &val);			\
	if (ret)					\
		return -1;				\
	x = (typeof(x))val;				\
	if (irq_check_en || isr_check_en)			\
		aml_wdt_disable_dbg();			\
	pr_info("%s:%ld\n", __func__, (unsigned long)x);	\
	return count;					\
}							\
static ssize_t x##_read(struct file *file, char __user *userbuf,	\
				 size_t count, loff_t *ppos)	\
{							\
	char buf[20];					\
	unsigned long val;					\
	ssize_t len;					\
	val = (unsigned long)x;				\
	len = snprintf(buf, sizeof(buf), "%ld\n", val);		\
	pr_info("%s:%ld\n", __func__, val);			\
	return simple_read_from_buffer(userbuf, count, ppos, buf, len);\
}							\
static const struct file_operations x##_debug_ops = {		\
	.open		= simple_open,			\
	.read		= x##_read,			\
	.write		= x##_write,			\
}
debug_fs(isr_thr);
debug_fs(irq_dis_thr);
debug_fs(sirq_thr);
debug_fs(irq_check_en);
debug_fs(isr_check_en);
debug_fs(out_thr);

static int __init debug_lockup_init(void)
{
	debug_lockup = debugfs_create_dir("lockup", NULL);
	if (IS_ERR_OR_NULL(debug_lockup)) {
		pr_warn("failed to create debug_lockup\n");
		debug_lockup = NULL;
		return -1;
	}
	debugfs_create_file("isr_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &isr_thr_debug_ops);
	debugfs_create_file("irq_dis_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &irq_dis_thr_debug_ops);
	debugfs_create_file("sirq_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &sirq_thr_debug_ops);
	debugfs_create_file("out_thr", S_IFREG | 0664,
			    debug_lockup, NULL, &out_thr_debug_ops);
	debugfs_create_file("isr_check_en", S_IFREG | 0664,
			    debug_lockup, NULL, &isr_check_en_debug_ops);
	debugfs_create_file("irq_check_en", S_IFREG | 0664,
			    debug_lockup, NULL, &irq_check_en_debug_ops);
	if (irq_check_en || isr_check_en)
		aml_wdt_disable_dbg();
	irq_flg = 1;
	return 0;
}
late_initcall(debug_lockup_init);

MODULE_DESCRIPTION("Amlogic debug lockup module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianxin Pan <jianxin.pan@amlogic.com>");
