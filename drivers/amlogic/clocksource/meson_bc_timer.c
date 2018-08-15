/*
 * drivers/amlogic/clocksource/meson_bc_timer.c
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
#define pr_fmt(fmt) "meson_bc_timer: " fmt

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

#define TIMER_DI_RESOLUTION_BIT         6
#define TIMER_CH_RESOLUTION_BIT         4
#define TIMER_BG_RESOLUTION_BIT         2
#define TIMER_AF_RESOLUTION_BIT         0

#define TIMER_DI_ENABLE_BIT         19
#define TIMER_CH_ENABLE_BIT         18
#define TIMER_BG_ENABLE_BIT         17
#define TIMER_AF_ENABLE_BIT         16

#define TIMER_DI_MODE_BIT         15
#define TIMER_CH_MODE_BIT         14
#define TIMER_BG_MODE_BIT         13
#define TIMER_AF_MODE_BIT         12

#define TIMER_RESOLUTION_1us            0
#define TIMER_RESOLUTION_10us           1
#define TIMER_RESOLUTION_100us          2
#define TIMER_RESOLUTION_1ms            3

void __iomem *timer_ctrl_base;

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

/********** Clock Event Device, Timer-ABCD/FGHI *********/

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
static struct meson_clock bc_clock;
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id);
static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev);

static DEFINE_SPINLOCK(time_lock);

static int meson_set_next_event(unsigned long evt,
				struct clock_event_device *dev)
{
	struct meson_clock *clk =  &bc_clock;
	/* use a big number to clear previous trigger cleanly */
	aml_set_reg32_mask(clk->reg, evt & 0xffff);
	/* then set next event */
	aml_set_reg32_bits(clk->reg, evt, 0, 16);
	return 0;
}

static int meson_clkevt_shutdown(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;

	spin_lock(&time_lock);
	/* pr_info("Disable timer %p %s\n",dev,dev->name); */
	aml_set_reg32_bits(clk->mux_reg, 0, clk->bit_enable, 1);
	spin_unlock(&time_lock);
	return 0;
}

static int meson_clkevt_set_periodic(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;

	spin_lock(&time_lock);
	aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_mode, 1);
	aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
	/* pr_info("Periodic timer %s!,mux_reg=%x\n",*/
	/* dev->name,readl_relaxed(clk->mux_reg)); */
	spin_unlock(&time_lock);
	return 0;
}

static int meson_clkevt_set_oneshot(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;

	spin_lock(&time_lock);
	aml_set_reg32_bits(clk->mux_reg, 0, clk->bit_mode, 1);
	aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
	/* pr_info("One shot timer %s!mux_reg=%x\n",*/
	/* dev->name,readl_relaxed(clk->mux_reg)); */
	spin_unlock(&time_lock);
	return 0;
}

static int meson_clkevt_resume(struct clock_event_device *dev)
{
	struct meson_clock *clk = &bc_clock;

	spin_lock(&time_lock);
	/* pr_info("Resume timer%s\n", dev->name); */
	aml_set_reg32_bits(clk->mux_reg, 1, clk->bit_enable, 1);
	spin_unlock(&time_lock);
	return 0;
}

/* Clock event timer interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	if (evt == NULL || evt->event_handler == NULL) {
		WARN_ONCE(evt == NULL || evt->event_handler == NULL,
			"%p %s %p %d",
			evt, evt?evt->name:NULL,
			evt?evt->event_handler:NULL, irq);
		return IRQ_HANDLED;
	}
	evt->event_handler(evt);
	return IRQ_HANDLED;

}

static struct clock_event_device meson_clockevent = {
	.cpumask = cpu_all_mask,
	.set_next_event = meson_set_next_event,
	.set_state_shutdown = meson_clkevt_shutdown,
	.set_state_periodic = meson_clkevt_set_periodic,
	.set_state_oneshot = meson_clkevt_set_oneshot,
	.tick_resume = meson_clkevt_resume,
};

/*
 * This sets up the system timers, clock source and clock event.
 */
int clockevent_init_and_register(struct device_node *np)
{
	struct device_node *timer;
	struct meson_clock *mclk = &bc_clock;

	timer = np;
	if (!timer) {
		pr_info(" * missing timer phandle\n");
		return -1;
	}
	if (of_property_read_string(timer, "timer_name",
				&meson_clockevent.name))
		return -1;

	if (of_property_read_u32(timer, "clockevent-rating",
				&meson_clockevent.rating))
		return -1;

	if (of_property_read_u32(timer, "clockevent-shift",
				&meson_clockevent.shift))
		return -1;

	if (of_property_read_u32(timer, "clockevent-features",
				&meson_clockevent.features))
		return -1;

	if (of_property_read_u32(timer, "bit_enable", &mclk->bit_enable))
		return -1;

	if (of_property_read_u32(timer, "bit_mode", &mclk->bit_mode))
		return -1;

	if (of_property_read_u32(timer, "bit_resolution",
				&mclk->bit_resolution))
		return -1;

	mclk->mux_reg = timer_ctrl_base;
	mclk->reg = of_iomap(timer, 1);
	pr_info("mclk->mux_reg =%p,mclk->reg =%p\n", mclk->mux_reg, mclk->reg);
	mclk->irq.irq = irq_of_parse_and_map(timer, 0);

	aml_set_reg32_mask(mclk->mux_reg,
		((1 << mclk->bit_mode)
		|(TIMER_RESOLUTION_1us << mclk->bit_resolution)));

	meson_clockevent.mult = div_sc(1000000, NSEC_PER_SEC, 20);
	meson_clockevent.max_delta_ns =
		clockevent_delta2ns(0xfffe, &meson_clockevent);
	meson_clockevent.min_delta_ns =
		clockevent_delta2ns(1, &meson_clockevent);
	mclk->irq.dev_id = &meson_clockevent;
	mclk->irq.handler = meson_timer_interrupt;
	mclk->irq.name = meson_clockevent.name;
	mclk->irq.flags =
		IRQF_TIMER | IRQF_IRQPOLL|IRQF_TRIGGER_RISING;
	/* Set up the IRQ handler */
	meson_clockevent.irq = mclk->irq.irq;
	clockevents_register_device(&meson_clockevent);
	setup_irq(mclk->irq.irq, &mclk->irq);
	return 0;
}

/*meson broadcast timer*/
static int __init meson_timer_init(struct device_node *np)
{
	timer_ctrl_base = of_iomap(np, 0);
	if (clockevent_init_and_register(np) < 0)
		pr_err("%s err\n", __func__);

	return 0;
}
CLOCKSOURCE_OF_DECLARE(meson_timer, "arm, meson-bc-timer", meson_timer_init);
