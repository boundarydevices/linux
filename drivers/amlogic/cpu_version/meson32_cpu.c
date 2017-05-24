/*
 * drivers/amlogic/cpu_version/meson32_cpu.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/meson-secure.h>

#define IO_REGS_BASE		0xFE000000
#define IO_BOOTROM_BASE	(IO_REGS_BASE + 0x010000) /*64K*/

static int meson_cpu_version[MESON_CPU_VERSION_LVL_MAX+1];
int __init meson_cpu_version_init(void)
{
	unsigned int version, ver;
	struct device_node *cpu_version;
	void __iomem  *assist_hw_rev0;
	void __iomem  *assist_hw_rev1;
	void __iomem  *assist_hw_rev2;
	unsigned int  *version_map;

	cpu_version = of_find_node_by_name(NULL, "cpu_version");
	if (!cpu_version) {
		pr_warn(" Not find cpu_version in dts. %s\n", __func__);
		return 0;
	}

	assist_hw_rev0 = of_iomap(cpu_version, 0);
	assist_hw_rev1 = of_iomap(cpu_version, 1);
	assist_hw_rev2 = of_iomap(cpu_version, 2);

	if (!assist_hw_rev0 || !assist_hw_rev1 || !assist_hw_rev2) {
		pr_warn("%s: iomap failed: %p %p %p\n", __func__,
			assist_hw_rev0, assist_hw_rev1, assist_hw_rev2);
		return 0;
	}

	meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR] = readl(assist_hw_rev0);

	if (meson_secure_enabled()) {
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC] =
			meson_read_socrev1();
	} else {
		version_map = (unsigned int *)assist_hw_rev2;
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC] = version_map[1];
	}

	version = readl(assist_hw_rev1);
	switch (version) {
	case 0x11111111:
		ver = 0xA;
		break;
	default:
		ver = 0xB;
		break;
	}
	meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR] = ver;
	pr_info("Meson chip version = Rev%X (%X:%X - %X:%X)\n", ver,
		meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_PACK],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC]
		);
	return 0;
}
EXPORT_SYMBOL(meson_cpu_version_init);

int get_meson_cpu_version(int level)
{
	if (level >= 0 && level <= MESON_CPU_VERSION_LVL_MAX)
		return meson_cpu_version[level];
	return 0;
}
EXPORT_SYMBOL(get_meson_cpu_version);
early_initcall(meson_cpu_version_init);
