/*
 * drivers/amlogic/reboot/reboot_m8b.c
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
#include <asm/compiler.h>
#include <linux/kdebug.h>
#include <linux/amlogic/iomap.h>
#include <linux/cpuidle.h>
#include <asm/cpuidle.h>
#include <linux/amlogic/reboot_m8b.h>



#define WATCHDOG_ENABLE_BIT		(1<<19)
#define  DUAL_CORE_RESET			 (3<<24)
#define AO_RTI_STATUS_REG1			((0x00 << 10) | (0x01 << 2))
#define VENC_VDAC_SETTING			0x1b7e
#define WATCHDOG_RESET			0x2641
#define WATCHDOG_TC				0x2640

#ifdef AMLOGIC_M8B_REBOOT_UBOOT_SUSPEND
static int reboot_flag;
static int __init do_parse_args(char *line)
{
	if (strcmp(line, "uboot_suspend") == 0)
		reboot_flag = 1;
	pr_info("reboot_flag=%x\n", reboot_flag);
	return 1;
}
__setup("reboot_args=", do_parse_args);
#endif

static inline void arch_reset(char mode, const char *cmd)
{
	aml_write_cbus(VENC_VDAC_SETTING, 0xf);
	aml_write_cbus(WATCHDOG_RESET, 0);
	aml_write_cbus(WATCHDOG_TC,
		DUAL_CORE_RESET | WATCHDOG_ENABLE_BIT | 100);
	while (1)
		cpu_do_idle();
}


void meson_common_restart(char mode, const char *cmd)
{
	u32 reboot_reason = MESON_NORMAL_BOOT;

	if (cmd) {
		if (strcmp(cmd, "charging_reboot") == 0)
			reboot_reason = MESON_CHARGING_REBOOT;
		else if (strcmp(cmd, "recovery") == 0 ||
			strcmp(cmd, "factory_reset") == 0)
			reboot_reason = MESON_FACTORY_RESET_REBOOT;
		else if (strcmp(cmd, "update") == 0)
			reboot_reason = MESON_UPDATE_REBOOT;
		else if (strcmp(cmd, "report_crash") == 0)
			reboot_reason = MESON_CRASH_REBOOT;
		else if (strcmp(cmd, "factory_testl_reboot") == 0)
			reboot_reason = MESON_FACTORY_TEST_REBOOT;
		else if (strcmp(cmd, "switch_system") == 0)
			reboot_reason = MESON_SYSTEM_SWITCH_REBOOT;
		else if (strcmp(cmd, "safe_mode") == 0)
			reboot_reason = MESON_SAFE_REBOOT;
		else if (strcmp(cmd, "lock_system") == 0)
			reboot_reason = MESON_LOCK_REBOOT;
		else if (strcmp(cmd, "usb_burner_reboot") == 0)
			reboot_reason = MESON_USB_BURNER_REBOOT;
		else if (strcmp(cmd, "uboot_suspend") == 0)
			reboot_reason = MESON_UBOOT_SUSPEND;
	}
	aml_write_aobus(AO_RTI_STATUS_REG1, reboot_reason);
	pr_info("reboot_reason(0x%x) = 0x%x\n", AO_RTI_STATUS_REG1,
		aml_read_aobus(AO_RTI_STATUS_REG1));
	arch_reset(mode, cmd);
}

static void do_aml_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	pr_info("meson power off\n");
#ifdef AMLOGIC_M8B_REBOOT_UBOOT_SUSPEND
	if (reboot_flag)
		meson_common_restart('h', "uboot_suspend");
	else
#endif
		meson_common_restart('h', "charging_reboot");
}

static int aml_restart_probe(struct platform_device *pdev)
{
	arm_pm_restart = do_aml_restart;
	return 0;
}

static const struct of_device_id of_aml_restart_match[] = {
	{ .compatible = "aml, reboot_m8b", },
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

