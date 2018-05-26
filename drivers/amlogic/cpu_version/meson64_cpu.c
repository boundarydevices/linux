/*
 * drivers/amlogic/cpu_version/meson64_cpu.c
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
static int meson_cpu_version[MESON_CPU_VERSION_LVL_MAX+1];
void __iomem  *assist_hw_rev;

int get_meson_cpu_version(int level)
{
	if (level >= 0 && level <= MESON_CPU_VERSION_LVL_MAX)
		return meson_cpu_version[level];
	return 0;
}
EXPORT_SYMBOL(get_meson_cpu_version);

/*
 * detect if a cpu id is big cpu
 */
int arch_big_cpu(int cpu)
{
	int type;
	struct device_node *cpu_version;

	cpu_version = of_find_node_by_name(NULL, "cpu_version");
	if (cpu_version)
		assist_hw_rev = of_iomap(cpu_version, 0);
	else
		return 0;

	type = readl(assist_hw_rev) >> 24;
	switch (type) {
	case MESON_CPU_MAJOR_ID_GXM:	/* 0 ~ 3 is faster cpu for GXM */
		if (cpu < 4)
			return 1;

	default:
		return 0;
	}
	return 0;
}
EXPORT_SYMBOL(arch_big_cpu);

static unsigned int cpu_version_init;
int __init meson_cpu_version_init(void)
{
	unsigned int ver;
	struct device_node *cpu_version;

	if (!cpu_version_init)
		cpu_version_init = 1;
	else
		return 0;

	cpu_version = of_find_node_by_name(NULL, "cpu_version");
	if (cpu_version)
		assist_hw_rev = of_iomap(cpu_version, 0);
	else
		return 0;

	meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR] =
		readl(assist_hw_rev) >> 24;

	ver = (readl(assist_hw_rev) >> 8) & 0xff;

	meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR] = ver;
	ver =  (readl(assist_hw_rev) >> 16) & 0xff;
	meson_cpu_version[MESON_CPU_VERSION_LVL_PACK] = ver;
	pr_info("Meson chip version = Rev%X (%X:%X - %X:%X)\n",
		meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MAJOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MINOR],
		meson_cpu_version[MESON_CPU_VERSION_LVL_PACK],
		meson_cpu_version[MESON_CPU_VERSION_LVL_MISC]
		);
	return 0;
}
early_initcall(meson_cpu_version_init);
