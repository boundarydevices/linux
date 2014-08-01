/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*!
 * @file mx6_ramoops.c
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/pstore_ram.h>

unsigned long int ramoops_phys_addr;
unsigned long int ramoops_mem_size;
EXPORT_SYMBOL(ramoops_phys_addr);
EXPORT_SYMBOL(ramoops_mem_size);


static struct ramoops_platform_data ramoops_data;

static struct platform_device ramoops_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};
#if defined(CONFIG_OF)
static const struct of_device_id mxc_ramoops_dt_ids[] = {
	{ .compatible = "fsl,mxc_ramoops", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxc_ramoops_dt_ids);
#endif


static int mxc_ramoops_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;

	ramoops_data.mem_address = ramoops_phys_addr;
	ramoops_data.mem_size = ramoops_mem_size;
	ret = of_property_read_u32(np, "record_size", (u32 *)(&(ramoops_data.record_size)));
	if (ret < 0) {
		dev_dbg(&pdev->dev, "can not get record_size\n");
		goto err;
	}
	ret = of_property_read_u32(np, "console_size", (u32 *)(&(ramoops_data.console_size)));
	if (ret < 0) {
		dev_dbg(&pdev->dev, "can not get console_size\n");
		goto err;
	}
	ret = of_property_read_u32(np, "ftrace_size", (u32 *)(&(ramoops_data.ftrace_size)));
	if (ret < 0) {
		dev_dbg(&pdev->dev, "can not get ftrace_size\n");
		goto err;
	}

	ret = of_property_read_u32(np, "dump_oops", (u32 *)(&(ramoops_data.dump_oops)));
	if (ret < 0) {
		dev_dbg(&pdev->dev, "can not get dump_oops\n");
		goto err;
	}
	if ((0 == ramoops_data.mem_size) ||
		(ramoops_data.mem_size <
		(ramoops_data.record_size +
		 ramoops_data.console_size +
		 ramoops_data.ftrace_size))) {
		 dev_dbg(&pdev->dev, "memory reserve and config does not match!\n");
		goto err;
	}
	ret = platform_device_register(&ramoops_dev);
	if (ret) {
		printk(KERN_ERR "unable to register platform device\n");
		return -1;
	}
	platform_set_drvdata(pdev, NULL);
	printk(KERN_INFO "mxc_ramoops device success loaded\n");
	return 0;

err:
	return ret;
}

static int  mxc_ramoops_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mxc_ramoops_driver = {
	.probe	= mxc_ramoops_probe,
	.remove	= mxc_ramoops_remove,
	.driver = {
		.name	= "mxc_ramoops",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mxc_ramoops_dt_ids),
	},
};

module_platform_driver(mxc_ramoops_driver)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Pstore RAM Oops driver for  MX6 Platform");
