/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clockchips.h>
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <asm/cpuidle.h>
#include <asm/proc-fns.h>
#include <mach/common.h>
#include <mach/cpuidle.h>

bool enable_wait_mode = true;

static int imx6q_enter_wait(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv, int index)
{
	int cpu = dev->cpu;

	if (enable_wait_mode) {
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu);

		imx6q_set_lpm(WAIT_UNCLOCKED);
		cpu_do_idle();
		imx6q_set_lpm(WAIT_CLOCKED);

		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu);
	} else {
		cpu_do_idle();
	}

	return index;
}

/*
 * For each cpu, setup the broadcast timer because local timer
 * stops for the states other than WFI.
 */
static void imx6q_setup_broadcast_timer(void *arg)
{
	int cpu = smp_processor_id();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ON, &cpu);
}

static struct cpuidle_driver imx6q_cpuidle_driver = {
	.name = "imx6q_cpuidle",
	.owner = THIS_MODULE,
	.en_core_tk_irqen = 1,
	.states = {
		/* WAIT */
		{
			.exit_latency = 5,
			.target_residency = 5,
			.flags = CPUIDLE_FLAG_TIME_VALID,
			.enter = imx6q_enter_wait,
			.name = "WAIT",
			.desc = "Clock off",
		},
	},
	.state_count = 1,
	.safe_state_index = 0,
};

static int __init early_enable_wait(char *p)
{
	if (memcmp(p, "on", 2) == 0) {
		enable_wait_mode = true;
		p += 2;
	} else if (memcmp(p, "off", 3) == 0) {
		enable_wait_mode = false;
		p += 3;
	}

	return 0;
}
early_param("enable_wait_mode", early_enable_wait);

int __init imx6q_cpuidle_init(void)
{
	pr_info("WAIT mode is %s!\n", enable_wait_mode ? "enabled" : "disabled");

	/* Set initial power mode */
	imx6q_set_lpm(WAIT_CLOCKED);

	/*
	 * Set cache lpm in wait mode bit
	 * for a reliable WAIT mode support
	 */
	imx6q_set_cache_lpm_in_wait(true);

	/* Configure the broadcast timer on each cpu */
	on_each_cpu(imx6q_setup_broadcast_timer, NULL, 1);

	return imx_cpuidle_init(&imx6q_cpuidle_driver);
}
