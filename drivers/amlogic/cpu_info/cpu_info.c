/*
 * drivers/amlogic/cpu_info/cpu_info.c
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

#define pr_fmt(fmt) "cpuinfo: " fmt

#include <linux/export.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
 #include <linux/module.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/secmon.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/arm-smccc.h>

static unsigned char cpuinfo_chip_id[16] = { 0 };

static noinline int fn_smc(u64 function_id,
			   u64 arg0,
			   u64 arg1,
			   u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
						(unsigned long)arg0,
						(unsigned long)arg1,
						(unsigned long)arg2,
						0, 0, 0, 0, &res);
	return res.a0;
}

static int cpuinfo_probe(struct platform_device *pdev)
{
	void __iomem *shm_out;
	struct device_node *np = pdev->dev.of_node;
	int cmd, ret;

	if (of_property_read_u32(np, "cpuinfo_cmd", &cmd))
		return -EINVAL;

	shm_out = get_secmon_sharemem_output_base();
	if (!shm_out) {
		pr_err("failed to allocate shared memory\n");
		return -ENOMEM;
	}

	sharemem_mutex_lock();
	ret = fn_smc(cmd, 2, 0, 0);
	if (ret == 0) {
		int version = *((unsigned int *)shm_out);

		if (version == 2)
			memcpy((void *)&cpuinfo_chip_id[0],
			       (void *)shm_out + 4,
			       16);
		else {
			/**
			 * Legacy 12-byte chip ID read out, transform data
			 * to expected order format.
			 */
			uint8_t *ch;
			int i;

			cpuinfo_chip_id[0] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_MAJOR);
			cpuinfo_chip_id[1] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_MINOR);
			cpuinfo_chip_id[2] = get_meson_cpu_version(
				MESON_CPU_VERSION_LVL_PACK);
			cpuinfo_chip_id[3] = 0;

			/* Transform into expected order for display */
			ch = (uint8_t *)(shm_out + 4);
			for (i = 0; i < 12; i++)
				cpuinfo_chip_id[i + 4] = ch[11 - i];
		}
	} else
		ret = -EPROTO;
	sharemem_mutex_unlock();

	return ret;
}

void cpuinfo_get_chipid(unsigned char *cid, unsigned int size)
{
	memcpy(&cid[0], cpuinfo_chip_id, size);
}

static const struct of_device_id cpuinfo_dt_match[] = {
	{ .compatible = "amlogic, cpuinfo" },
	{ /* sentinel */ },
};

static  struct platform_driver cpuinfo_platform_driver = {
	.probe		= cpuinfo_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "cpuinfo",
		.of_match_table	= cpuinfo_dt_match,
	},
};

static int __init meson_cpuinfo_init(void)
{
	return  platform_driver_register(&cpuinfo_platform_driver);
}
module_init(meson_cpuinfo_init);
