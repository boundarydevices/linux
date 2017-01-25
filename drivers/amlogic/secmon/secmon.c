/*
 * drivers/amlogic/secmon/secmon.c
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

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <asm/compiler.h>
#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#endif
#undef pr_fmt
#define pr_fmt(fmt) "secmon: " fmt

static void __iomem *sharemem_in_base;
static void __iomem *sharemem_out_base;
static long phy_in_base;
static long phy_out_base;
#ifdef CONFIG_ARM64
#define IN_SIZE	0x1000
#else
 #define IN_SIZE	0x8000
#endif
 #define OUT_SIZE 0x1000
static DEFINE_MUTEX(sharemem_mutex);
#ifdef CONFIG_ARM64
static long get_sharemem_info(unsigned int function_id)
{
	asm volatile(
		__asmeq("%0", "x0")
		"smc	#0\n"
		: "+r" (function_id));

	return function_id;
}
#else
static long get_sharemem_info(unsigned int function_id)
{
	register long r0 asm("r0") = function_id;
	asm volatile(
		__asmeq("%0", "r0")
		__asmeq("%1", "r0")
		__SMC(0)
			: "=r" (r0)
			: "r"(r0));

	return r0;
}
#endif
static int secmon_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int id;
	int ret;

	if (!of_property_read_u32(np, "in_base_func", &id))
		phy_in_base = get_sharemem_info(id);

	if (!of_property_read_u32(np, "out_base_func", &id))
		phy_out_base = get_sharemem_info(id);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		pr_info("probe done\n");

	return ret;
}

static const struct of_device_id secmon_dt_match[] = {
	{ .compatible = "amlogic, secmon" },
	{ /* sentinel */ },
};

static  struct platform_driver secmon_platform_driver = {
	.probe		= secmon_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "secmon",
		.of_match_table	= secmon_dt_match,
	},
};

int __init meson_secmon_init(void)
{
	return  platform_driver_register(&secmon_platform_driver);
}
module_init(meson_secmon_init);

void sharemem_mutex_lock(void)
{
	mutex_lock(&sharemem_mutex);
}
void sharemem_mutex_unlock(void)
{
	mutex_unlock(&sharemem_mutex);
}

void __iomem *get_secmon_sharemem_input_base(void)
{
	return sharemem_in_base;
}
void __iomem *get_secmon_sharemem_output_base(void)
{
	return sharemem_out_base;
}

static int secmon_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	sharemem_in_base = ioremap_cache(phy_in_base, IN_SIZE);
	if (!sharemem_in_base) {
		pr_info("secmon share mem in buffer remap fail!\n");
		return -ENOMEM;
	}
	sharemem_out_base = ioremap_cache(phy_out_base, OUT_SIZE);
	if (!sharemem_out_base) {
		pr_info("secmon share mem out buffer remap fail!\n");
		return -ENOMEM;
	}
	pr_info("share in base: 0x%lx, share out base: 0x%lx\n",
			(long)sharemem_in_base, (long)sharemem_out_base);
	return 0;
}
static const struct reserved_mem_ops rmem_secmon_ops = {
	.device_init = secmon_mem_device_init,
};
static int __init secmon_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_secmon_ops;
	pr_debug("share mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(secmonmem, "amlogic, aml_secmon_memory",
					   secmon_mem_setup);
