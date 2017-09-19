/*
 * drivers/amlogic/pm/m8b_pm.c
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/fs.h>

#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/amlogic/iomap.h>
#include <linux/init.h>
#include <linux/of.h>
#include <asm/compiler.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <asm/suspend.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <../kernel/power/power.h>
#include <linux/of_reserved_mem.h>
#include <../mach-meson/platsmp.h>
#include <linux/amlogic/pm.h>

#ifdef CONFIG_MESON_TRUSTZONE
#include <mach/meson-secure.h>
#endif

#ifdef CONFIG_SUSPEND_WATCHDOG
#include <mach/watchdog.h>
#endif /* CONFIG_SUSPEND_WATCHDOG */

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif

//#define CONFIG_AO_TRIG_CLK 1
#ifdef CONFIG_AO_TRIG_CLK
#include "arc_trig_clk.h"
#endif

#define ON  1
#define OFF 0

//appf functions
#define APPF_INITIALIZE             0
#define APPF_POWER_DOWN_CPU         1
#define APPF_POWER_UP_CPUS          2
//appf flags
#define APPF_SAVE_PMU          (1<<0)
#define APPF_SAVE_TIMERS       (1<<1)
#define APPF_SAVE_VFP          (1<<2)
#define APPF_SAVE_DEBUG        (1<<3)
#define APPF_SAVE_L2           (1<<4)

/******************
 ***You need sync this param struct with arc_pwr.h is suspend firmware.
 ***1st word is used for arc output control: serial_disable.
 ***2nd word...
 ***
 ***If you need transfer more params,
 ***you need sync the struck define in arc_pwr.h
 *******************/
unsigned int arc_serial_disable;

static ulong suspend_firm_addr;
static ulong suspend_firm_size;
static suspend_state_t pm_state;

#if 0
static void ao_uart_change_buad(unsigned int reg, unsigned int clk_rate)
{
	aml_aobus_update_bits(reg, 0x7FFFFF, 0);
	aml_aobus_update_bits(reg, 0xffffff,
		(((clk_rate / (115200 * 4)) - 1) & 0x7fffff)|(1<<23));
}
#endif

static void wait_uart_empty(void)
{
	do {
		udelay(100);
	} while ((aml_read_aobus(AO_UART_STATUS) & (1 << 22)) == 0);
}

void clk_switch(int flag)
{
	if (flag) {
		//uart_rate_clk = clk_get_rate(clk81);
		wait_uart_empty();
		//gate on pll
		aml_cbus_update_bits(HHI_MPEG_CLK_CNTL, 1<<7, 1<<7);
		udelay(10);
		//switch to pll
		aml_cbus_update_bits(HHI_MPEG_CLK_CNTL, 1<<8, 1<<8);
		udelay(10);
//		ao_uart_change_buad(AO_UART_REG5,167*1000*1000);
	} else {
		//uart_rate_clk = clk_get_rate(clkxtal);
		wait_uart_empty();
		// gate off from pll
		aml_cbus_update_bits(HHI_MPEG_CLK_CNTL, 1<<8, 0);
		udelay(10);
		// switch to 24M
		aml_cbus_update_bits(HHI_MPEG_CLK_CNTL, 1<<7, 0);
		udelay(10);
//		ao_uart_change_buad(AO_UART_REG5,24*1000*1000);
	}
}
EXPORT_SYMBOL(clk_switch);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void meson_system_early_suspend(struct early_suspend *h)
{
	if (!early_suspend_flag)
		early_suspend_flag = 1;
}

static void meson_system_late_resume(struct early_suspend *h)
{
	if (early_suspend_flag)
		early_suspend_flag = 0;
}
#endif

unsigned int get_resume_method(void)
{
	return 0;
}
EXPORT_SYMBOL(get_resume_method);

int meson_power_suspend(ulong addr)
{
	static int test_flag;
	void (*pwrtest_entry)(unsigned int,
		unsigned int, unsigned int, unsigned int);

//	check_in_param();
	flush_cache_all();

	pwrtest_entry = (void (*)(unsigned int,
		unsigned int, unsigned int, unsigned int))addr;
	if (test_flag != 1234) {
		test_flag = 1234;
		pr_info("initial appf\n");
		pr_info("entry=0x%x 0x%x\n",
			*(unsigned int *)(addr), *(unsigned int *)(addr+4));
		pwrtest_entry(APPF_INITIALIZE,
			0, 0, IO_PL310_BASE & 0xffff0000);
	}
#ifdef CONFIG_SUSPEND_WATCHDOG
	DISABLE_SUSPEND_WATCHDOG;
#endif
	pr_info("power down cpu --\n");
	pwrtest_entry(APPF_POWER_DOWN_CPU, 0, 0,
		APPF_SAVE_PMU | APPF_SAVE_VFP | APPF_SAVE_L2
		| (IO_PL310_BASE & 0xffff0000));
#ifdef CONFIG_SUSPEND_WATCHDOG
	ENABLE_SUSPEND_WATCHDOG;
#endif
	return 0;
}

static void meson_pm_suspend(void)
{
	pr_info("enter meson_pm_suspend!\n");
#ifdef CONFIG_SUSPEND_WATCHDOG
	ENABLE_SUSPEND_WATCHDOG;
#endif

	clk_switch(OFF);
	pr_info("sleep ...\n");
	//switch A9 clock to xtal 24MHz
	aml_cbus_update_bits(HHI_SYS_CPU_CLK_CNTL, 1 << 7, 0);
	//disable sys pll
	aml_cbus_update_bits(HHI_SYS_PLL_CNTL, 1 << 30, 0);

#ifdef CONFIG_MESON_TRUSTZONE
	meson_suspend_firmware();
#else
	meson_power_suspend(0xc4d04400);
#endif
	//enable sys pll
	aml_cbus_update_bits(HHI_SYS_PLL_CNTL, 1 << 30, 1 << 30);
	pr_info("... wake up\n");

	if (aml_read_aobus(AO_RTC_ADDR1) & (1<<12)) {
	/* Woke from alarm, not power button.
	 *Set flag to inform key_input driver.
	 */
		aml_write_aobus(AO_RTI_STATUS_REG2, FLAG_WAKEUP_ALARM);
	}
	/* clear RTC interrupt*/
	aml_write_aobus(AO_RTC_ADDR1,
		aml_read_aobus(AO_RTC_ADDR1) | (0xf000));
	pr_info("RTCADD3=0x%x\n", aml_read_aobus(AO_RTC_ADDR3));
	if (aml_read_aobus(AO_RTC_ADDR3) | (1<<29)) {
		aml_write_aobus(AO_RTC_ADDR3,
			aml_read_aobus(AO_RTC_ADDR3) & (~(1<<29)));
		udelay(1000);
	}
	pr_info("RTCADD3=0x%x\n", aml_read_aobus(AO_RTC_ADDR3));
	/*wait_uart_empty();*/
	//a9 use pll
	aml_cbus_update_bits(HHI_SYS_CPU_CLK_CNTL, 1 << 7, 1 << 7);
	clk_switch(ON);

#ifdef CONFIG_AO_TRIG_CLK
	run_arc_program();
#endif
}

static int meson_pm_prepare(void)
{
	pr_info("enter meson_pm_prepare!\n");
	return 0;
}

static int meson_pm_enter(suspend_state_t state)
{
	int ret = 0;

	switch (state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		meson_pm_suspend();
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

static const struct platform_suspend_ops meson_pm_ops = {
	.enter        = meson_pm_enter,
	.prepare    = meson_pm_prepare,
	.finish       = meson_pm_finish,
	.valid        = suspend_valid_only_mem,
};

unsigned int is_pm_freeze_mode(void)
{
	if (pm_state == PM_SUSPEND_FREEZE)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_pm_freeze_mode);

static int frz_begin(void)
{
	pm_state = PM_SUSPEND_FREEZE;

	return 0;
}

static void frz_end(void)
{
	pm_state = PM_SUSPEND_ON;
}

static const struct platform_freeze_ops meson_m8b_frz_ops = {
	.begin = frz_begin,
	.end = frz_end,
};

static int __init meson_pm_probe(struct platform_device *pdev)
{
	pr_info("enter meson_pm_probe!\n");
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	early_suspend.suspend = meson_system_early_suspend;
	early_suspend.resume = meson_system_late_resume;
	register_early_suspend(&early_suspend);
#endif

	//ioremap_page_range(0xc4d00000,
	//0xc4d00000+suspend_firm_addr, suspend_firm_addr, vm_page_prot);

	suspend_set_ops(&meson_pm_ops);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (lgcy_early_suspend_init())
		return -1;
#endif
	freeze_set_ops(&meson_m8b_frz_ops);

	pr_info("meson_pm_probe done !\n");

#ifdef CONFIG_AO_TRIG_CLK
	return run_arc_program();
#else
	return 0;
#endif
}

static int __init rmem_pm_setup(struct reserved_mem *rmem)
{
	suspend_firm_addr = (ulong)rmem->base;
	suspend_firm_size = (ulong)rmem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(pm_meson, "amlogic, pm-m8b-reserve", rmem_pm_setup);

static int __exit meson_pm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	return 0;
}

static const struct of_device_id amlogic_pm_dt_match[] = {
	{	.compatible = "amlogic, pm-m8b",
	},
};

static struct platform_driver meson_pm_driver = {
	.driver = {
		.name     = "pm-meson",
		.owner     = THIS_MODULE,
		.of_match_table = amlogic_pm_dt_match,
	},
	.remove = __exit_p(meson_pm_remove),
};

static int __init meson_pm_init(void)
{
	pr_info("enter %s\n", __func__);
	return platform_driver_probe(&meson_pm_driver, meson_pm_probe);
}
late_initcall(meson_pm_init);
