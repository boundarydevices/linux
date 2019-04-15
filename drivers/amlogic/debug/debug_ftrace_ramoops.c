/*
 * drivers/amlogic/debug/debug_ftrace_ramoops.c
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

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/ftrace.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/amlogic/debug_ftrace_ramoops.h>
#include <../../../fs/pstore/internal.h>
#include <linux/trace_clock.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>

static DEFINE_PER_CPU(int, en);

#define IRQ_D	1

unsigned int dump_iomap;
core_param(dump_iomap, dump_iomap, uint, 0664);

unsigned int ramoops_ftrace_en;
EXPORT_SYMBOL(ramoops_ftrace_en);

int ramoops_io_en;
EXPORT_SYMBOL(ramoops_io_en);
core_param(ramoops_io_en, ramoops_io_en, int, 0664);

const char *record_name[PSTORE_FLAG_IO_MAX] = {
	"NULL",
	"FUNC",
	"IO-R",
	"IO-W",
	"IO-R-E",
	"IO-W-E",
};

void notrace pstore_ftrace_save(struct pstore_ftrace_record *rec)
{
	int cpu = raw_smp_processor_id();

	if (unlikely(oops_in_progress) || unlikely(per_cpu(en, cpu)))
		return;
	per_cpu(en, cpu) = 1;
	pstore_ftrace_encode_cpu(rec, cpu);
	strlcpy(rec->comm, current->comm, sizeof(rec->comm) - 1);
	rec->pid = current->pid;
	rec->time = trace_clock_local();
	psinfo->write_buf(PSTORE_TYPE_FTRACE, 0, NULL, 0, (void *)rec,
			  0, sizeof(*rec), psinfo);
	per_cpu(en, cpu) = 0;
}
EXPORT_SYMBOL(pstore_ftrace_save);

static void notrace pstore_function_dump(struct pstore_ftrace_record *rec,
					 struct seq_file *s)
{
	unsigned long sec = 0, ms = 0;
	unsigned long long time = rec->time;

	do_div(time, 1000000);
	sec = (unsigned long)time / 1000;
	ms = (unsigned long)time % 1000;
	seq_printf(s, "[%04ld.%03ld@%d] <%5d-%s>  <%pf <- %pF>\n",
		   sec, ms, pstore_ftrace_decode_cpu(rec), rec->pid, rec->comm,
		   (void *)rec->ip, (void *)rec->parent_ip);
}

void notrace pstore_io_rw_dump(struct pstore_ftrace_record *rec,
			       struct seq_file *s)
{
	unsigned long sec = 0, ms = 0;
	unsigned long long time = rec->time;
	unsigned int cpu = pstore_ftrace_decode_cpu(rec);

	do_div(time, 1000000);
	sec = (unsigned long)time / 1000;
	ms = (unsigned long)time % 1000;
	seq_printf(s, "[%04ld.%03ld@%d] <%5d-%6s> <%6s %08lx-%8lx>  <%pf <- %pF>\n",
		   sec, ms, cpu, rec->pid, rec->comm, record_name[rec->flag],
		   rec->val1, (rec->flag == PSTORE_FLAG_IO_W) ? rec->val2 : 0,
		   (void *)rec->ip, (void *)rec->parent_ip);
}

void notrace pstore_ftrace_dump(struct pstore_ftrace_record *rec,
				struct seq_file *s)
{
	switch (rec->flag & PSTORE_FLAG_MASK) {
	case PSTORE_FLAG_FUNC:
		pstore_function_dump(rec, s);
		break;
	case PSTORE_FLAG_IO_R:
	case PSTORE_FLAG_IO_W:
	case PSTORE_FLAG_IO_W_END:
	case PSTORE_FLAG_IO_R_END:
		pstore_io_rw_dump(rec, s);
		break;
	default:
		seq_printf(s, "Unknown Msg:%x\n", rec->flag);
	}
}

void notrace pstore_io_save(unsigned long reg, unsigned long val,
			    unsigned long parant, unsigned int flag,
			    unsigned long *irq_flag)
{
	struct pstore_ftrace_record rec;

	if (!ramoops_ftrace_en || !ramoops_io_en)
		return;

	if ((flag == PSTORE_FLAG_IO_R || flag == PSTORE_FLAG_IO_W) && IRQ_D)
		local_irq_save(*irq_flag);

	rec.ip = CALLER_ADDR0;
	rec.parent_ip = parant;
	rec.flag = flag;
	rec.val1 = reg;
	rec.val2 = val;
	pstore_ftrace_save(&rec);

	if ((flag == PSTORE_FLAG_IO_R_END || flag == PSTORE_FLAG_IO_W_END) &&
		IRQ_D)
		local_irq_restore(*irq_flag);
}
EXPORT_SYMBOL(pstore_io_save);

