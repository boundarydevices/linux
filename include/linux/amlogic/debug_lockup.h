/*
 * include/linux/amlogic/debug_lockup.h
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

#ifndef __debug_lockup_h_
#define __debug_lockup_h_


void irq_trace_stop(unsigned long flag);
void irq_trace_start(unsigned long flag);
void pr_lockup_info(int cpu);
void lockup_hook(int cpu);
void isr_in_hook(unsigned int c, unsigned long long *t,
	unsigned int i, void *a);
void isr_out_hook(unsigned int cpu, unsigned long long tin, unsigned int irq);
void irq_trace_en(int en);
void sirq_in_hook(unsigned int cpu, unsigned long long *tin, void *p);
void sirq_out_hook(unsigned int cpu, unsigned long long tin, void *p);
void aml_wdt_disable_dbg(void);
void  notrace __arch_cpu_idle_enter(void);
void  notrace __arch_cpu_idle_exit(void);
#endif
