/*
 * drivers/amlogic/reboot/reboot.c
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>

#include <asm/system_misc.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/reboot.h>
#include <asm/compiler.h>
#include <linux/kdebug.h>

static u32 psci_function_id_restart;
static u32 psci_function_id_poweroff;
static char *kernel_panic;
static u32 parse_reason(const char *cmd)
{
	u32 reboot_reason = MESON_NORMAL_BOOT;

	if (cmd) {
		if (strcmp(cmd, "recovery") == 0 ||
				strcmp(cmd, "factory_reset") == 0)
			reboot_reason = MESON_FACTORY_RESET_REBOOT;
		else if (strcmp(cmd, "update") == 0)
			reboot_reason = MESON_UPDATE_REBOOT;
		else if (strcmp(cmd, "fastboot") == 0)
			reboot_reason = MESON_FASTBOOT_REBOOT;
		else if (strcmp(cmd, "bootloader") == 0)
			reboot_reason = MESON_BOOTLOADER_REBOOT;
		else if (strcmp(cmd, "report_crash") == 0)
			reboot_reason = MESON_CRASH_REBOOT;
		else if (strcmp(cmd, "uboot_suspend") == 0)
			reboot_reason = MESON_UBOOT_SUSPEND;
	} else {
		if (kernel_panic) {
			if (strcmp(kernel_panic, "kernel_panic") == 0)
				reboot_reason = MESON_KERNEL_PANIC;
		}

	}

	pr_info("reboot reason %d\n", reboot_reason);
	return reboot_reason;
}
static noinline int __invoke_psci_fn_smc(u64 function_id, u64 arg0, u64 arg1,
					 u64 arg2)
{
	register long x0 asm("x0") = function_id;
	register long x1 asm("x1") = arg0;
	register long x2 asm("x2") = arg1;
	register long x3 asm("x3") = arg2;
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3));

	return function_id;
}
void meson_smc_restart(u64 function_id, u64 reboot_reason)
{
	__invoke_psci_fn_smc(function_id,
				reboot_reason, 0, 0);
}
void meson_common_restart(char mode, const char *cmd)
{
	u32 reboot_reason = parse_reason(cmd);

	if (psci_function_id_restart)
		meson_smc_restart((u64)psci_function_id_restart,
						(u64)reboot_reason);
}
static void do_aml_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	meson_common_restart(reboot_mode, cmd);
}

static void do_aml_poweroff(void)
{
	/* TODO: Add poweroff capability */
	__invoke_psci_fn_smc(0x82000042, 1, 0, 0);
	__invoke_psci_fn_smc(psci_function_id_poweroff,
				0, 0, 0);
}
static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	kernel_panic = "kernel_panic";

	return NOTIFY_DONE;
}

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

static int aml_restart_probe(struct platform_device *pdev)
{
	u32 id;
	int ret;

	if (!of_property_read_u32(pdev->dev.of_node, "sys_reset", &id)) {
		psci_function_id_restart = id;
		arm_pm_restart = do_aml_restart;
	}

	if (!of_property_read_u32(pdev->dev.of_node, "sys_poweroff", &id)) {
		psci_function_id_poweroff = id;
		pm_power_off = do_aml_poweroff;
	}

	ret = register_die_notifier(&panic_notifier);
	return ret;
}

static const struct of_device_id of_aml_restart_match[] = {
	{ .compatible = "aml, reboot", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aml_restart_match);

static struct platform_driver aml_restart_driver = {
	.probe = aml_restart_probe,
	.driver = {
		.name = "aml-restart",
		.of_match_table = of_match_ptr(of_aml_restart_match),
	},
};

static int __init aml_restart_init(void)
{
	return platform_driver_register(&aml_restart_driver);
}
device_initcall(aml_restart_init);
