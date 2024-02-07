// SPDX-License-Identifier: GPL-2.0+
//
// Copyright 2016 Freescale Semiconductor, Inc.
// Copyright 2017 NXP

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/cpuhotplug.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched_clock.h>

#include "timer-of.h"

#define TPM_PARAM			0x4
#define TPM_PARAM_WIDTH_SHIFT		16
#define TPM_PARAM_WIDTH_MASK		(0xff << 16)
#define TPM_SC				0x10
#define TPM_SC_CMOD_INC_PER_CNT		(0x1 << 3)
#define TPM_SC_CMOD_DIV_DEFAULT		0x3
#define TPM_SC_CMOD_DIV_MAX		0x7
#define TPM_SC_TOF_MASK			(0x1 << 7)
#define TPM_CNT				0x14
#define TPM_MOD				0x18
#define TPM_STATUS			0x1c
#define TPM_STATUS_CH0F			BIT(0)
#define TPM_C0SC			0x20
#define TPM_C0SC_CHIE			BIT(6)
#define TPM_C0SC_MODE_SHIFT		2
#define TPM_C0SC_MODE_MASK		0x3c
#define TPM_C0SC_MODE_SW_COMPARE	0x4
#define TPM_C0SC_CHF_MASK		(0x1 << 7)
#define TPM_C0V				0x24

static int counter_width __ro_after_init;
//static void __iomem *timer_base __ro_after_init;

struct imx_tpm_clockevent {
	void __iomem *base;
	int counter_width;
	struct timer_of to_tpm;
};

struct imx_tpm_clockevent *global_tpm;

struct imx_tpm_clockevent* to_tpm_clkevt(struct clock_event_device *evt)
{
	struct timer_of *to_tpm;

	to_tpm = container_of(evt, struct timer_of, clkevt);
	return container_of(to_tpm, struct imx_tpm_clockevent, to_tpm);
}

static inline void tpm_timer_disable(struct imx_tpm_clockevent *tpm)
{
	unsigned int val;

	/* channel disable */
	val = readl(tpm->base + TPM_C0SC);
	val &= ~(TPM_C0SC_MODE_MASK | TPM_C0SC_CHIE);
	writel(val, tpm->base + TPM_C0SC);
}

static inline void tpm_timer_enable(struct imx_tpm_clockevent *tpm)
{
	unsigned int val;

	/* channel enabled in sw compare mode */
	val = readl(tpm->base + TPM_C0SC);
	val |= (TPM_C0SC_MODE_SW_COMPARE << TPM_C0SC_MODE_SHIFT) |
	       TPM_C0SC_CHIE;
	writel(val, tpm->base + TPM_C0SC);
}

static inline void tpm_irq_acknowledge(struct imx_tpm_clockevent *tpm)
{
	writel(TPM_STATUS_CH0F, tpm->base + TPM_STATUS);
}

static inline unsigned long tpm_read_counter(struct imx_tpm_clockevent *tpm)
{
	return readl(tpm->base + TPM_CNT);
}

#if defined(CONFIG_ARM)
static struct delay_timer tpm_delay_timer;

static unsigned long tpm_read_current_timer(void)
{
	return tpm_read_counter(global_tpm);
}
#endif

static u64 notrace tpm_read_sched_clock(void)
{
	return tpm_read_counter(global_tpm);
}

static int tpm_set_next_event(unsigned long delta,
				struct clock_event_device *evt)
{
	unsigned long next, prev, now;
	struct imx_tpm_clockevent *tpm = to_tpm_clkevt(evt);

	prev = tpm_read_counter(tpm);
	next = prev + delta;
	writel(next, tpm->base + TPM_C0V);
	now = tpm_read_counter(tpm);

	/*
	 * Need to wait CNT increase at least 1 cycle to make sure
	 * the C0V has been updated into HW.
	 */
	if ((next & 0xffffffff) != readl(tpm->base + TPM_C0V))
		while (now == tpm_read_counter(tpm))
			;

	/*
	 * NOTE: We observed in a very small probability, the bus fabric
	 * contention between GPU and A7 may results a few cycles delay
	 * of writing CNT registers which may cause the min_delta event got
	 * missed, so we need add a ETIME check here in case it happened.
	 */
	return (now - prev) >= delta ? -ETIME : 0;
}

static int tpm_set_state_oneshot(struct clock_event_device *evt)
{
	struct imx_tpm_clockevent *tpm = to_tpm_clkevt(evt);
	tpm_timer_enable(tpm);

	return 0;
}

static int tpm_set_state_shutdown(struct clock_event_device *evt)
{
	struct imx_tpm_clockevent *tpm = to_tpm_clkevt(evt);

	tpm_timer_disable(tpm);

	return 0;
}

static irqreturn_t tpm_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	struct imx_tpm_clockevent *tpm = to_tpm_clkevt(evt);

	tpm_irq_acknowledge(tpm);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static int __init tpm_clocksource_init(struct timer_of *to_tpm)
{
	struct imx_tpm_clockevent *tpm = container_of(to_tpm, struct imx_tpm_clockevent, to_tpm);

#if defined(CONFIG_ARM)
	tpm_delay_timer.read_current_timer = &tpm_read_current_timer;
	tpm_delay_timer.freq = timer_of_rate(to_tpm) >> 3;
	register_current_timer_delay(&tpm_delay_timer);
#endif

	sched_clock_register(tpm_read_sched_clock, counter_width,
			     timer_of_rate(to_tpm) >> 3);

	return clocksource_mmio_init(tpm->base + TPM_CNT,
				     "imx-tpm",
				     timer_of_rate(to_tpm) >> 3,
				     to_tpm->clkevt.rating,
				    counter_width,
				     clocksource_mmio_readl_up);
}

static void __init tpm_clockevent_init(struct timer_of *to_tpm)
{
	clockevents_config_and_register(&to_tpm->clkevt,
					timer_of_rate(to_tpm) >> 3,
					300,
					GENMASK(counter_width - 1,
					1));
}

static int tpm;
/* percpu timer */
static DEFINE_PER_CPU(struct imx_tpm_clockevent, tpm_percpu_timer);

static int imx_tpm_starting_cpu(unsigned int cpu)
{
        struct imx_tpm_clockevent *clkevt;
        clkevt = per_cpu_ptr(&tpm_percpu_timer, cpu);
        tpm_clockevent_init(&clkevt->to_tpm);
        irq_force_affinity(clkevt->to_tpm.clkevt.irq, cpumask_of(cpu));

        return 0;
}

static int imx_tpm_dying_cpu(unsigned int cpu)
{
        struct imx_tpm_clockevent *clkevt;
        clkevt = per_cpu_ptr(&tpm_percpu_timer, cpu);

        disable_percpu_irq(timer_of_irq(&clkevt->to_tpm));

        return 0;
}

static int __init tpm_timer_init(struct device_node *np)
{
	struct imx_tpm_clockevent *clkevt;
	void __iomem *timer_base;
	uint32_t rating = 0;
	struct clk *ipg;
	int ret;

	ipg = of_clk_get_by_name(np, "ipg");
	if (IS_ERR(ipg)) {
		pr_err("tpm: failed to get ipg clk\n");
		return -ENODEV;
	}
	/* enable clk before accessing registers */
	ret = clk_prepare_enable(ipg);
	if (ret) {
		pr_err("tpm: ipg clock enable failed (%d)\n", ret);
		clk_put(ipg);
		return ret;
	}

	clkevt = per_cpu_ptr(&tpm_percpu_timer, tpm);

	clkevt->to_tpm.flags = TIMER_OF_IRQ | TIMER_OF_BASE | TIMER_OF_CLOCK;

	if (tpm == 0)
		clkevt->to_tpm.clkevt = (struct clock_event_device) {
			.name			= "i.MX TPM Timer 0",
			.rating			= 200,
			.features		= CLOCK_EVT_FEAT_ONESHOT,
			.set_state_shutdown	= tpm_set_state_shutdown,
			.set_state_oneshot	= tpm_set_state_oneshot,
			.set_next_event		= tpm_set_next_event,
			.cpumask		= cpumask_of(tpm),
		};
	else
		clkevt->to_tpm.clkevt = (struct clock_event_device) {
			.name			= "i.MX TPM Timer 1",
			.rating			= 200,
			.features		= CLOCK_EVT_FEAT_ONESHOT,
			.set_state_shutdown	= tpm_set_state_shutdown,
			.set_state_oneshot	= tpm_set_state_oneshot,
			.set_next_event		= tpm_set_next_event,
			.cpumask		= cpumask_of(tpm),
		};

	clkevt->to_tpm.of_irq = (struct of_timer_irq) {
		.handler		= tpm_timer_interrupt,
		.flags			= IRQF_TIMER,
	};

	clkevt->to_tpm.of_clk = (struct of_timer_clk) {
		.name = "per",
	};

	ret = timer_of_init(np, &clkevt->to_tpm);
	if (ret)
		return ret;

	timer_base = timer_of_base(&clkevt->to_tpm);
	clkevt->base = timer_base;

	counter_width = (readl(timer_base + TPM_PARAM)
		& TPM_PARAM_WIDTH_MASK) >> TPM_PARAM_WIDTH_SHIFT;

	of_property_read_u32(np, "timer-rating", &rating);

	if (rating) {
		/* override timer rating if device tree property is found */
		clkevt->to_tpm.clkevt.rating = rating;
	} else {
		/* use rating 200 for 32-bit counter and 150 for 16-bit counter */
		clkevt->to_tpm.clkevt.rating = counter_width == 0x20 ? 200 : 150;
	}

	/*
	 * Initialize tpm module to a known state
	 * 1) Counter disabled
	 * 2) TPM counter operates in up counting mode
	 * 3) Timer Overflow Interrupt disabled
	 * 4) Channel0 disabled
	 * 5) DMA transfers disabled
	 */
	/* make sure counter is disabled */
	writel(0, timer_base + TPM_SC);
	/* TOF is W1C */
	writel(TPM_SC_TOF_MASK, timer_base + TPM_SC);
	writel(0, timer_base + TPM_CNT);
	/* CHF is W1C */
	writel(TPM_C0SC_CHF_MASK, timer_base + TPM_C0SC);

	/*
	 * increase per cnt,
	 * div 8 for 32-bit counter and div 128 for 16-bit counter
	 */
	writel(TPM_SC_CMOD_INC_PER_CNT |
		(counter_width == 0x20 ?
		TPM_SC_CMOD_DIV_DEFAULT : TPM_SC_CMOD_DIV_MAX),
		timer_base + TPM_SC);

	/* set MOD register to maximum for free running mode */
	writel(GENMASK(counter_width - 1, 0), timer_base + TPM_MOD);

	if (!tpm) {
		tpm++;
		global_tpm = clkevt;

		cpuhp_setup_state(CPUHP_AP_NXP_TIMER_STARTING,
				  "clockevents/nxp/tpm_timer:starting",
				  imx_tpm_starting_cpu, imx_tpm_dying_cpu);

		return tpm_clocksource_init(&clkevt->to_tpm);
	}
	else
		return 0;
}
#ifdef MODULE
static int tpm_timer_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	return tpm_timer_init(np);
}

static const struct of_device_id tpm_timer_match_table[] = {
	{ .compatible = "fsl,imx7ulp-tpm" },
	{ }
};
MODULE_DEVICE_TABLE(of, tpm_timer_match_table);

static struct platform_driver tpm_timer_driver = {
	.probe		= tpm_timer_probe,
	.driver		= {
		.name	= "tpm-timer",
		.of_match_table = tpm_timer_match_table,
	},
};
module_platform_driver(tpm_timer_driver);

#else
TIMER_OF_DECLARE(imx7ulp, "fsl,imx7ulp-tpm", tpm_timer_init);
#endif

MODULE_LICENSE("GPL");
