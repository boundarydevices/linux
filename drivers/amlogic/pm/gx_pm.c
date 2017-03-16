/*
 * drivers/amlogic/pm/gx_pm.c
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

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/psci.h>
#include <asm/compiler.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <asm/suspend.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <asm/cpuidle.h>
#include <uapi/linux/psci.h>
#include <linux/arm-smccc.h>

typedef unsigned long (psci_fn)(unsigned long, unsigned long,
				unsigned long, unsigned long);

static unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static u32 psci_get_version(void)
{
	return __invoke_psci_fn_smc(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
}


#undef pr_fmt
#define pr_fmt(fmt) "gxbb_pm: " fmt

static void __iomem *exit_reg;
static int max_idle_lvl;

/*
 *0x10000 : bit[16]=1:control cpu suspend to power down
 *cpu_suspend(0, meson_system_suspend);
 */

static void meson_gx_suspend(void)
{
	pr_info("enter meson_pm_suspend!\n");
	arm_cpuidle_suspend(max_idle_lvl);
/*	cpu_suspend(0, meson_system_suspend);
 */
	pr_info("... wake up\n");
}

static int meson_pm_prepare(void)
{
	pr_info("enter meson_pm_prepare!\n");
	return 0;
}

static int meson_gx_enter(suspend_state_t state)
{
	int ret = 0;

	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		meson_gx_suspend();
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static void meson_pm_finish(void)
{
	pr_info("enter meson_pm_finish!\n");
}
unsigned int get_resume_method(void)
{
	unsigned int val = 0;

	if (exit_reg)
		val = (readl(exit_reg) >> 28) & 0xf;
	return val;
}
EXPORT_SYMBOL(get_resume_method);

static const struct platform_suspend_ops meson_gx_ops = {
	.enter = meson_gx_enter,
	.prepare = meson_pm_prepare,
	.finish = meson_pm_finish,
	.valid = suspend_valid_only_mem,
};

static unsigned int suspend_reason;
ssize_t suspend_reason_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	unsigned int len;

	len = sprintf(buf, "%d\n", suspend_reason);

	return len;
}
ssize_t suspend_reason_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &suspend_reason);

	switch (ret) {
	case 1:
		__invoke_psci_fn_smc(0x82000042, suspend_reason, 0, 0);
		break;
	default:
		return -EINVAL;
	}
	return count;
}

DEVICE_ATTR(suspend_reason, 0664, suspend_reason_show, suspend_reason_store);

static int __init meson_pm_probe(struct platform_device *pdev)
{
	struct device_node *cpu_node;
	struct device_node *state_node;
	int count = 0;
	u32 ver = psci_get_version();

	pr_info("enter meson_pm_probe!\n");

	if (PSCI_VERSION_MAJOR(ver) == 0 && PSCI_VERSION_MINOR(ver) == 2) {
		cpu_node = of_get_cpu_node(0, NULL);
		if (!cpu_node) {
			pr_info("cpu node get fail!\n");
			return -1;
		}
		while ((state_node = of_parse_phandle(cpu_node,
					"cpu-idle-states", count))) {
			count++;
			of_node_put(state_node);
		}
		if (count)
			max_idle_lvl = count;
		else {
			max_idle_lvl = -1;
			pr_info("get system suspend level fail!\n");
			return -1;
		}
		pr_info("system suspend level: %d\n", max_idle_lvl);
		suspend_set_ops(&meson_gx_ops);
	}

	exit_reg = of_iomap(pdev->dev.of_node, 0);
	device_create_file(&pdev->dev, &dev_attr_suspend_reason);
	pr_info("meson_pm_probe done\n");
	return 0;
}

static int __exit meson_pm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id amlogic_pm_dt_match[] = {
	{.compatible = "amlogic, pm",
	 },
};

static struct platform_driver meson_pm_driver = {
	.driver = {
		   .name = "pm-meson",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_pm_dt_match,
		   },
	.remove = __exit_p(meson_pm_remove),
};

static int __init meson_pm_init(void)
{
	return platform_driver_probe(&meson_pm_driver, meson_pm_probe);
}

late_initcall(meson_pm_init);
