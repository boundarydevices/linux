/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "common.h"

static void __init imx6sx_init_machine(void)
{
	struct device *parent;

	mxc_arch_reset_init_dt();

	parent = imx_soc_device_init();
	if (parent == NULL)
		pr_warn("failed to initialize soc device\n");

	of_platform_populate(NULL, of_default_bus_match_table, NULL, parent);

	imx_anatop_init();
	imx6sx_pm_init();
}

static void __init imx6sx_init_irq(void)
{
	imx_init_revision_from_anatop();
	imx_init_l2cache();
	imx_src_init();
	imx_gpc_init();
	irqchip_init();
}

static const char *imx6sx_dt_compat[] __initconst = {
	"fsl,imx6sx",
	NULL,
};

static void __init imx6sx_opp_init(struct device *cpu_dev)
{
	struct device_node *np;

	np = of_find_node_by_path("/cpus/cpu@0");
	if (!np) {
		pr_warn("failed to find cpu0 node\n");
		return;
	}

	cpu_dev->of_node = np;
	if (of_init_opp_table(cpu_dev))
		pr_warn("failed to init OPP table\n");

	of_node_put(np);
}

static struct platform_device imx6sx_cpufreq_pdev = {
	.name = "imx6q-cpufreq",
};

static void __init imx6sx_init_late(void)
{
	if (IS_ENABLED(CONFIG_ARM_IMX6Q_CPUFREQ)) {
		imx6sx_opp_init(&imx6sx_cpufreq_pdev.dev);
		platform_device_register(&imx6sx_cpufreq_pdev);
	}
}

DT_MACHINE_START(IMX6SX, "Freescale i.MX6 SoloX (Device Tree)")
	.map_io		= debug_ll_io_init,
	.init_irq	= imx6sx_init_irq,
	.init_machine	= imx6sx_init_machine,
	.init_late	= imx6sx_init_late,
	.dt_compat	= imx6sx_dt_compat,
	.restart	= mxc_restart,
MACHINE_END
