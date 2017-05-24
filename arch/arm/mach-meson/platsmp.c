/*
 * arch/arm/mach-meson/platsmp.c
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

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <asm/smp_scu.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/cacheflush.h>
#include <asm/mach-types.h>
#include <linux/percpu.h>
#include "platsmp.h"
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <linux/amlogic/meson-secure.h>

static DEFINE_SPINLOCK(boot_lock);
static DEFINE_SPINLOCK(clockfw_lock);
static void __iomem *periph_membase;
static void __iomem *sram_membase;
static void __iomem *reg_map[3];
static unsigned int io_cbus_base;
static unsigned int io_aobus_base;


void meson_set_cpu_ctrl_addr(uint32_t cpu, const uint32_t addr)
{
	spin_lock(&clockfw_lock);

	if (meson_secure_enabled())
		meson_auxcoreboot_addr(cpu, addr);
	else
		writel(addr, (void *)(CPU1_CONTROL_ADDR_REG + ((cpu-1) << 2)));

	spin_unlock(&clockfw_lock);
}

void meson_set_cpu_ctrl_reg(int cpu, int is_on)
{
	uint32_t value = 0;

	spin_lock(&clockfw_lock);

	if (meson_secure_enabled()) {
		value = meson_read_corectrl();
		value = (value & ~(1U << cpu)) | (is_on << cpu);
		value |= 1;
		meson_modify_corectrl(value);
	} else {
		aml_set_reg32_bits(CPU_CONTROL_REG, is_on, cpu, 1);
		aml_set_reg32_bits(CPU_CONTROL_REG, 1, 0, 1);
	}

	spin_unlock(&clockfw_lock);
}

int meson_get_cpu_ctrl_addr(int cpu)
{
	if (meson_secure_enabled())
		return 0;
	else
		return readl((void *)(CPU1_CONTROL_ADDR_REG + ((cpu-1) << 2)));
}

void meson_set_cpu_power_ctrl(uint32_t cpu, int is_power_on)
{
	WARN_ON(cpu == 0);

	if (is_power_on) {
		/* SCU Power on CPU & CPU PWR_A9_CNTL0 CTRL_MODE bit.
		 * CTRL_MODE bit may write forward to SCU when cpu reset.
		 * So, we need clean it here to avoid the forward write happen.
		 */
		aml_set_reg32_bits(CPU_POWER_CTRL_REG,
			0x0, (cpu << 3), 2);
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL0, 0x0, 2*cpu + 16, 2);
		udelay(5);

#ifndef CONFIG_MESON_CPU_EMULATOR
		/* Reset enable*/
		aml_set_reg32_bits(P_HHI_SYS_CPU_CLK_CNTL, 1, (cpu + 24), 1);
		/* Power on*/
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_MEM_PD0,
			0, (32 - cpu * 4), 4);
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL1,
			0x0, ((cpu + 1) << 1), 2);

		udelay(10);
		while (!(readl((void *)(P_AO_RTI_PWR_A9_CNTL1)) &
			(1<<(cpu+16)))) {
			pr_err("wait power...0x%08x 0x%08x\n",
				readl((void *)(P_AO_RTI_PWR_A9_CNTL0)),
				readl((void *)(P_AO_RTI_PWR_A9_CNTL1)));
			udelay(10);
		};
		/* Isolation disable */
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL0, 0x0, cpu, 1);
		/* Reset disable */
		aml_set_reg32_bits(P_HHI_SYS_CPU_CLK_CNTL, 0, (cpu + 24), 1);
		aml_set_reg32_bits(CPU_POWER_CTRL_REG,
			0x0, (cpu << 3), 2);
#endif
	} else{
		aml_set_reg32_bits(CPU_POWER_CTRL_REG,
			0x3, (cpu << 3), 2);
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL0, 0x3, 2*cpu + 16, 2);

#ifndef CONFIG_MESON_CPU_EMULATOR
		/* Isolation enable */
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL0, 0x1, cpu, 1);
		udelay(10);
		/* Power off */
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_CNTL1,
			0x3, ((cpu + 1) << 1), 2);
		aml_set_reg32_bits(P_AO_RTI_PWR_A9_MEM_PD0,
			0xf, (32 - cpu * 4), 4);
#endif
	}
	dsb();
	dmb();

	pr_debug("----CPU %d\n", cpu);
	pr_debug("----CPU_POWER_CTRL_REG(%08x) = %08x\n",
		CPU_POWER_CTRL_REG,
		readl((void *)(CPU_POWER_CTRL_REG)));
	pr_debug("----P_AO_RTI_PWR_A9_CNTL0(%08x)    = %08x\n",
		P_AO_RTI_PWR_A9_CNTL0, readl((void *)(P_AO_RTI_PWR_A9_CNTL0)));
	pr_debug("----P_AO_RTI_PWR_A9_CNTL1(%08x)    = %08x\n",
		P_AO_RTI_PWR_A9_CNTL1, readl((void *)(P_AO_RTI_PWR_A9_CNTL1)));

}

static void write_pen_release(int val)
{
	pen_release = val;
	/*memory barrier*/
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static void meson_secondary_set(unsigned int cpu)
{
	meson_set_cpu_ctrl_addr(cpu,
		(const uint32_t)virt_to_phys(meson_secondary_startup));
	meson_set_cpu_ctrl_reg(cpu, 1);
	/*memory barrier*/
	smp_wmb();
	/*memory barrier*/
	mb();
}

static void meson_secondary_init(unsigned int cpu)
{
	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);
	/*memory barrier*/
	smp_wmb();
}

static int meson_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;
	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 */
	write_pen_release(cpu_logical_map(cpu));

	if (!meson_secure_enabled()) {
		meson_set_cpu_ctrl_addr(cpu,
			(const uint32_t)virt_to_phys(meson_secondary_startup));
		meson_set_cpu_power_ctrl(cpu, 1);
		timeout = jiffies + (10 * HZ);

		while (meson_get_cpu_ctrl_addr(cpu))
			;
		if (!time_before(jiffies, timeout))
			return -EPERM;
	}

	meson_secondary_set(cpu);
	dsb_sev();

/* smp_send_reschedule(cpu); */
	timeout = jiffies + (10 * HZ);
	while (time_before(jiffies, timeout)) {
		/*memory barrier*/
		smp_rmb();
		if (pen_release == -1)
			break;
		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);
	return pen_release != -1 ? -ETIME : 0;
}


static void __init meson_smp_prepare_cpus(unsigned int max_cpus)
{
	int i = 0;
	struct device_node *node;
	struct resource res;
	struct device_node *child;

	node = of_find_compatible_node(NULL, NULL, "amlogic,meson8b-cpuconfig");
	if (!node) {
		pr_err("Missing A5 CPU config node in the device tree\n");
		return;
	}

	periph_membase = of_iomap(node, 0);
	if (!periph_membase)
		pr_err("Couldn't map A5 CPU config registers\n");

	sram_membase = of_iomap(node, 1);
	if (!periph_membase)
		pr_err("Couldn't map A5 sram registers\n");

	node = of_find_compatible_node(NULL, NULL, "amlogic, iomap");
	if (!node) {
		pr_err("Missing A5 iomap node in the device tree\n");
		return;
	}

	for_each_child_of_node(node, child) {
		if (of_address_to_resource(child, 0, &res)) {
			pr_err("Missing iomap child node reg\n");
			return;
		}

		reg_map[i] = ioremap(res.start, resource_size(&res));
		i++;
	}
	io_cbus_base = (unsigned int)reg_map[0];
	io_aobus_base = (unsigned int)reg_map[2];


	/*
	 * Initialise the SCU and wake up the secondary core using
	 * wakeup_secondary().
	 */
	scu_enable((void __iomem *) periph_membase);
}


#ifdef CONFIG_HOTPLUG_CPU
int meson_cpu_kill(unsigned int cpu)
{
	unsigned int value;
	unsigned int offset = (cpu<<3);
	unsigned int cnt = 0;

	do {
		udelay(10);
		value = readl((void *)(CPU_POWER_CTRL_REG));
		cnt++;
		WARN_ON(cnt > 5000);
	} while ((value&(3<<offset)) != (3<<offset));

	udelay(10);
	meson_set_cpu_power_ctrl(cpu, 0);
	return 1;
}

void meson_cpu_die(unsigned int cpu)
{
	meson_set_cpu_ctrl_reg(cpu, 0);

	meson_cleanup();
	v7_exit_coherency_flush(louis);

	aml_set_reg32_bits(CPU_POWER_CTRL_REG, 0x3, (cpu << 3), 2);
	asm volatile(
		"dsb\n"
		"wfi\n"
	);
	WARN_ON(1);
}

int meson_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
#endif

const struct smp_operations meson_smp_ops __initconst = {
	.smp_prepare_cpus	= meson_smp_prepare_cpus,
	.smp_secondary_init	= meson_secondary_init,
	.smp_boot_secondary	= meson_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= meson_cpu_die,
	.cpu_kill = meson_cpu_kill,
	.cpu_disable = meson_cpu_disable,
#endif
};

CPU_METHOD_OF_DECLARE(meson8b_smp, "amlogic,meson8b-smp", &meson_smp_ops);
