/*
 * drivers/amlogic/clocksource/meson_timer.c
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
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <asm/memory.h>
#include <linux/sched_clock.h>
#include <linux/of.h>
#include <asm/smp_plat.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/cpu.h>

#undef pr_fmt
#define pr_fmt(fmt) "meson_timer: " fmt

/***********************************************************************
 * System timer
 **********************************************************************/
#define TIMER_E_RESOLUTION_BIT         8
#define TIMER_E_ENABLE_BIT        20
#define TIMER_E_RESOLUTION_MASK       (7UL << TIMER_E_RESOLUTION_BIT)
#define TIMER_E_RESOLUTION_SYS           0
#define TIMER_E_RESOLUTION_1us           1
#define TIMER_E_RESOLUTION_10us          2
#define TIMER_E_RESOLUTION_100us         3
#define TIMER_E_RESOLUTION_1ms           4

#define TIMER_RESOLUTION_1us            0
#define TIMER_RESOLUTION_10us           1
#define TIMER_RESOLUTION_100us          2
#define TIMER_RESOLUTION_1ms            3

struct meson_clock {
	struct irqaction	irq;
	const char *name;	/*A,B,C,D,F,G,H,I*/
	int	bit_enable;
	int	bit_mode;
	int	bit_resolution;
	void __iomem *mux_reg;
	void __iomem *reg;
	unsigned int init_flag;
};


static DEFINE_PER_CPU(struct clock_event_device, percpu_clockevent);
static DEFINE_PER_CPU(struct meson_clock, percpu_mesonclock);

static void __iomem *timer_ctrl_base;
static void __iomem *clocksrc_base;

static inline void aml_set_reg32_mask(void __iomem *_reg, const uint32_t _mask)
{
	uint32_t _val;

	_val = readl_relaxed(_reg) | _mask;

	writel_relaxed(_val, _reg);
}

static inline void aml_write_reg32(void __iomem *_reg, const uint32_t _value)
{
	writel_relaxed(_value, _reg);
}

static inline void aml_clr_reg32_mask(void __iomem *_reg, const uint32_t _mask)
{
	writel_relaxed((readl_relaxed(_reg) & (~(_mask))), _reg);
}

static inline void
aml_set_reg32_bits(void __iomem *_reg, const uint32_t _value,
		const uint32_t _start, const uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
					~(((1L << (_len))-1) << (_start))) |
				(((_value)&((1L<<(_len))-1)) << (_start))),
				_reg);
}

static cycle_t cycle_read_timerE(struct clocksource *cs)
{
	return (cycle_t) readl_relaxed(clocksrc_base);
}
static unsigned long cycle_read_timerE1(void)
{
	return (unsigned long) readl_relaxed(clocksrc_base);
}
static struct clocksource clocksource_timer_e = {
	.name   = "Timer-E",
	.rating = 300,
	.read   = cycle_read_timerE,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};
static u64 notrace meson8_read_sched_clock(void)
{
	return (u64)readl_relaxed(clocksrc_base);
}
static void __init meson_clocksource_init(void)
{
	aml_clr_reg32_mask(timer_ctrl_base, TIMER_E_RESOLUTION_MASK);
	aml_set_reg32_mask(timer_ctrl_base,
			TIMER_E_RESOLUTION_1us << TIMER_E_RESOLUTION_BIT);

	clocksource_timer_e.shift = 22;
	clocksource_timer_e.mult = 4194304000u;
	clocksource_register_khz(&clocksource_timer_e, 1000);
	sched_clock_register(meson8_read_sched_clock, 32, USEC_PER_SEC);
}

static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev)
{
	int cpuidx = smp_processor_id();
	struct meson_clock *clk =  &per_cpu(percpu_mesonclock, cpuidx);
	/* use a big number to clear previous trigger cleanly */
	aml_set_reg32_mask(clk->reg, evt & 0xffff);
	/* then set next event */
	aml_set_reg32_bits(clk->reg, evt, 0, 16);
	return 0;
}

/* Clock event timer interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	if (evt == NULL || evt->event_handler == NULL) {
		WARN_ONCE(evt == NULL || evt->event_handler == NULL,
			"%p %s %p %d", evt, evt?evt->name:NULL,
			evt?evt->event_handler:NULL, irq);
		return IRQ_HANDLED;
	}
	evt->event_handler(evt);
	return IRQ_HANDLED;

}

void local_timer_setup_data(int cpuidx)
{
		phandle phandle =  -1;
		u32 hwid = 0;
		struct device_node *cpu, *cpus, *timer;
		struct meson_clock *mclk = &per_cpu(percpu_mesonclock, cpuidx);
		struct clock_event_device *clock_evt =
			&per_cpu(percpu_clockevent, cpuidx);
		cpus = of_find_node_by_path("/cpus");
		if (!cpus)
			return;
		for_each_child_of_node(cpus, cpu) {
			if (hwid == cpuidx)
				break;
			hwid++;
		}
		if (hwid != cpuidx)
			cpu = of_get_next_child(cpus, NULL);

		if (of_property_read_u32(cpu, "timer", &phandle)) {
			pr_info(" * missing timer property\n");
			return;
		}
		timer = of_find_node_by_phandle(phandle);
		if (!timer) {
			pr_info(" * %s missing timer phandle\n",
				cpu->full_name);
			return;
		}
		if (of_property_read_string(timer, "timer_name",
			&clock_evt->name))
			return;

		if (of_property_read_u32(timer, "clockevent-rating",
			&clock_evt->rating))
			return;

		if (of_property_read_u32(timer, "clockevent-shift",
			&clock_evt->shift))
			return;

		if (of_property_read_u32(timer, "clockevent-features",
			&clock_evt->features))
			return;

		if (of_property_read_u32(timer, "bit_enable",
			&mclk->bit_enable))
			return;

		if (of_property_read_u32(timer, "bit_mode",
			&mclk->bit_mode))
			return;

		if (of_property_read_u32(timer, "bit_resolution",
			&mclk->bit_resolution))
			return;

		mclk->mux_reg = timer_ctrl_base;
		mclk->reg = of_iomap(timer, 0);
		pr_info("%s mclk->mux_reg =%p,mclk->reg =%p\n",
				clock_evt->name, mclk->mux_reg, mclk->reg);
		mclk->irq.irq = irq_of_parse_and_map(timer, 0);

}
int  clockevent_local_timer(unsigned int cpu)
	{
		int cpuidx = smp_processor_id();

		struct meson_clock *mclk = &per_cpu(percpu_mesonclock, cpuidx);
		struct clock_event_device *clock_evt =
			&per_cpu(percpu_clockevent, cpuidx);

		aml_set_reg32_mask(mclk->mux_reg,
			((1 << mclk->bit_mode)
			|(TIMER_RESOLUTION_1us << mclk->bit_resolution)));
		aml_write_reg32(mclk->reg, 9999);

		clock_evt->mult = div_sc(1000000, NSEC_PER_SEC, 20);
		clock_evt->max_delta_ns =
			clockevent_delta2ns(0xfffe, clock_evt);
		clock_evt->min_delta_ns = clockevent_delta2ns(1, clock_evt);
		clock_evt->set_next_event = meson_set_next_event;
		clock_evt->cpumask = cpumask_of(cpuidx);
		clock_evt->irq = mclk->irq.irq;
		mclk->irq.dev_id = clock_evt;
		mclk->irq.handler = meson_timer_interrupt;
		mclk->irq.name = clock_evt->name;
		mclk->irq.flags =
			IRQF_TIMER | IRQF_IRQPOLL | IRQF_TRIGGER_RISING;
		clockevents_register_device(clock_evt);
		setup_irq(mclk->irq.irq, &mclk->irq);
		if (cpuidx)
			irq_force_affinity(mclk->irq.irq, cpumask_of(cpuidx));
		enable_percpu_irq(mclk->irq.irq, 0);
		aml_set_reg32_mask(mclk->mux_reg, (1 << mclk->bit_enable));
		return 0;
	}

int  meson_local_timer_stop(unsigned int cpuidx)
{
	struct meson_clock *clk;
	struct clock_event_device *clock_evt =
		&per_cpu(percpu_clockevent, cpuidx);
	if (!clock_evt) {
		pr_err("meson_local_timer_stop: null evt!\n");
		return 0;
	}
	if (cpuidx == 0)
		BUG();
	clk = &per_cpu(percpu_mesonclock, cpuidx);

	aml_clr_reg32_mask(clk->mux_reg, (1 << clk->bit_enable));
	disable_percpu_irq(clk->irq.irq);
	return 0;
}

static int __init meson_timer_init(struct device_node *np)
{
	int i;
	static struct delay_timer aml_delay_timer;

	timer_ctrl_base = of_iomap(np, 0);
	clocksrc_base = of_iomap(np, 1);
	meson_clocksource_init();
	for (i = 0; i < nr_cpu_ids; i++)
		local_timer_setup_data(i);
	cpuhp_setup_state(CPUHP_AP_ARM_ARCH_TIMER_STARTING,
				"AP_ARM_ARCH_TIMER_STARTING",
				clockevent_local_timer, meson_local_timer_stop);

	aml_delay_timer.read_current_timer = &cycle_read_timerE1;
	aml_delay_timer.freq = USEC_PER_SEC;
	register_current_timer_delay(&aml_delay_timer);
	return 0;
}
CLOCKSOURCE_OF_DECLARE(meson_timer, "arm, meson-timer", meson_timer_init);
