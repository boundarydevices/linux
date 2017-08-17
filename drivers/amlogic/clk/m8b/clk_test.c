/*
 * drivers/amlogic/clk/m8b/clk_test.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <dt-bindings/clock/meson8b-clkc.h>

#include "clkc.h"
#include "meson8b.h"


static struct dentry *debugfs_root;

void usage(void)
{
	pr_info("\nclk_test:\n");
	pr_info("    echo get clk_name > /sys/kernel/debug/aml_clk/clk_test\n");
	pr_info("      -- get the clock rate of clk_name\n");
	pr_info("    echo set clk_name clkrate > /sys/kernel/debug/aml_clk/clk_test\n");
	pr_info("      -- set the clk_name's rate to clkrate\n");
}

struct clk *aml_get_clk_by_name(char *name)
{
	int idx;
	struct clk *cur_clk;

	for (idx = 0; idx < NR_CLKS; idx++) {
		if (!clks[idx]) {
			pr_debug("no such clk clks[%d]\n", idx);
			continue;
		}
		if (!strcmp(__clk_get_name(clks[idx]), name)) {
			cur_clk = clks[idx];
			return cur_clk;
		}
	}
	pr_debug("%s: can not find clk: %s\n", __func__, name);
	return NULL;
}

static ssize_t clk_test_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	usage();
	return 0;
}

static ssize_t clk_test_write(struct file *file, const char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	struct clk *cur_clk;
	unsigned long rate, old_rate, new_rate;
	char get_set[4];
	char clk_name[32];
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf)-1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%3s %31s %lu", get_set, clk_name, &rate);
	switch (ret) {
	case 1:
		pr_err("%s error usage!\n", __func__);
		usage();
		break;
	case 2:
		if (!strcmp(get_set, "get")) {
			cur_clk = aml_get_clk_by_name(clk_name);
			if (cur_clk != NULL) {
				pr_info("try to get %s's rate", clk_name);
				rate = clk_get_rate(cur_clk);
				pr_info("%s: %lu\n", clk_name, rate);
			} else
				pr_err("Fail! Can not find this clk %s\n",
					   clk_name);
		} else {
			pr_err("%s error usage!\n", __func__);
			usage();
		}
		break;
	case 3:
		if (!strcmp(get_set, "set")) {
			if (!rate)
				return -EINVAL;
			cur_clk = aml_get_clk_by_name(clk_name);
			if (cur_clk != NULL) {
				old_rate = clk_get_rate(cur_clk);
				if (old_rate == rate) {
					pr_info("cur rate is %lu, no need to set again!\n",
						rate);
					break;
				}
				pr_info("try to set %s's rate from %lu to %lu\n",
					clk_name, old_rate, rate);
				ret = clk_set_rate(cur_clk, rate);
				new_rate = clk_get_rate(cur_clk);
				if (ret) {
					pr_err("Fail! Can not set %s to %lu, cur rate: %lu\n",
						clk_name, rate, new_rate);
					break;
				}
				if (new_rate == old_rate)
					pr_err("Fail! Can not set %s to %lu, still rate: %lu\n",
						clk_name, rate, new_rate);
				else
					pr_info("success! %s cur rate: %lu\n",
						clk_name, new_rate);
			} else
				pr_err("Fail! Can not find this clk %s\n",
					clk_name);
		} else {
			pr_err("%s error usage!\n", __func__);
			usage();
		}

		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations clk_test_file_ops = {
	.open		= simple_open,
	.read		= clk_test_read,
	.write		= clk_test_write,
};

static int __init clk_test_init(void)
{
	debugfs_root = debugfs_create_dir("aml_clk", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return -1;
	}

	debugfs_create_file("clk_test", S_IFREG | 0444,
			    debugfs_root, NULL, &clk_test_file_ops);
	return 0;
}

static void __exit clk_test_exit(void)
{
}

module_init(clk_test_init);
module_exit(clk_test_exit);

MODULE_DESCRIPTION("AMLOGIC clk test driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cai Yun <yun.cai@amlogic.com>");
