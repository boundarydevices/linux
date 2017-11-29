/*
 * drivers/amlogic/input/remote/sysfs.c
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
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/of_address.h>

#include "remote_meson.h"
#include <linux/amlogic/iomap.h>

static ssize_t protocol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	if (ENABLE_LEGACY_IR(chip->protocol))
		return sprintf(buf, "protocol=%s&%s (0x%x)\n",
			chip->ir_contr[LEGACY_IR_ID].proto_name,
			chip->ir_contr[MULTI_IR_ID].proto_name,
			chip->protocol);

	return sprintf(buf, "protocol=%s (0x%x)\n",
		chip->ir_contr[MULTI_IR_ID].proto_name,
		chip->protocol);
}

static ssize_t protocol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int protocol;
	unsigned long flags;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &protocol);
	if (ret != 0) {
		dev_err(chip->dev, "input parameter error\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->slock, flags);
	chip->protocol = protocol;
	chip->set_register_config(chip, chip->protocol);
	if (MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
				!MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		remote_raw_event_unregister(chip->r_dev); /*raw->no raw*/
		dev_info(chip->dev, "remote_raw_event_unregister\n");
	} else if (!MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
				MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		remote_raw_init();
		remote_raw_event_register(chip->r_dev); /*no raw->raw*/
	}
	chip->r_dev->rc_type = chip->protocol;
	spin_unlock_irqrestore(&chip->slock, flags);
	return count;
}

static ssize_t keymap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct ir_map_tab_list *map_tab;
	unsigned long flags;
	int i, len;

	spin_lock_irqsave(&chip->slock, flags);
	map_tab = seek_map_tab(chip, chip->sys_custom_code);
	if (!map_tab) {
		dev_err(chip->dev, "please set valid keymap name first\n");
		spin_unlock_irqrestore(&chip->slock, flags);
		return 0;
	}
	len = sprintf(buf, "custom_code=0x%x\n", map_tab->tab.custom_code);
	len += sprintf(buf+len, "custom_name=%s\n",  map_tab->tab.custom_name);
	len += sprintf(buf+len, "release_delay=%d\n",
						map_tab->tab.release_delay);
	len += sprintf(buf+len, "map_size=%d\n",  map_tab->tab.map_size);
	len += sprintf(buf+len, "fn_key_scancode = 0x%x\n",
				map_tab->tab.cursor_code.fn_key_scancode);
	len += sprintf(buf+len, "cursor_left_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_left_scancode);
	len += sprintf(buf+len, "cursor_right_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_right_scancode);
	len += sprintf(buf+len, "cursor_up_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_up_scancode);
	len += sprintf(buf+len, "cursor_down_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_down_scancode);
	len += sprintf(buf+len, "cursor_ok_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_ok_scancode);
	len += sprintf(buf+len, "keycode scancode\n");
	for (i = 0; i <  map_tab->tab.map_size; i++) {
		len += sprintf(buf+len, "%4d %4d\n",
			map_tab->tab.codemap[i].map.keycode,
			map_tab->tab.codemap[i].map.scancode);
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

static ssize_t keymap_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	int ret;
	int value;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		dev_err(chip->dev, "keymap_store input err\n");
		return -EINVAL;
	}
	chip->sys_custom_code = value;
	return count;
}

static ssize_t debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int debug_enable;

	debug_enable = remote_debug_get_enable();

	return sprintf(buf, "%d\n", debug_enable);
}

static ssize_t debug_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int debug_enable;
	int ret;

	ret = kstrtoint(buf, 0, &debug_enable);
	if (ret != 0)
		return -EINVAL;
	remote_debug_set_enable(debug_enable);
	return count;
}

int debug_log_printk(struct remote_dev *dev, const char *fmt)
{
	char *p;
	int len;

	len = strlen(fmt);
	if (dev->debug_current + len > dev->debug_buffer_size)
		dev->debug_current = 0;
	p = (char *)(dev->debug_buffer+dev->debug_current);
	strcpy(p, fmt);
	dev->debug_current += len;
	return 0;
}

static ssize_t debug_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	return sprintf(buf, r_dev->debug_buffer);
}

static ssize_t debug_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	if (buf[0] == 'c') {
		r_dev->debug_buffer[0] = 0;
		r_dev->debug_current = 0;
	}
	return count;
}

static ssize_t repeat_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", chip->repeat_enable);
}

static ssize_t repeat_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int val;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	chip->repeat_enable = val;
	return count;
}

static ssize_t map_tables_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct ir_map_tab_list *ir_map;
	unsigned long flags;
	int len = 0;
	int cnt = 0;

	spin_lock_irqsave(&chip->slock, flags);
	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		len += sprintf(buf+len, "%d. 0x%x,%s\n",
			cnt, ir_map->tab.custom_code, ir_map->tab.custom_name);
		cnt++;
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

DEVICE_ATTR_RW(repeat_enable);
DEVICE_ATTR_RW(protocol);
DEVICE_ATTR_RW(keymap);
DEVICE_ATTR_RW(debug_enable);
DEVICE_ATTR_RW(debug_log);
DEVICE_ATTR_RO(map_tables);

static struct attribute *remote_attrs[] = {
	&dev_attr_protocol.attr,
	&dev_attr_map_tables.attr,
	&dev_attr_keymap.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_repeat_enable.attr,
	&dev_attr_debug_log.attr,
	NULL,
};

ATTRIBUTE_GROUPS(remote);

static struct class remote_class = {
	.name		= "remote",
	.owner		= THIS_MODULE,
	.dev_groups = remote_groups,
};

int ir_sys_device_attribute_init(struct remote_chip *chip)
{
	struct device *dev;

	class_register(&remote_class);

	dev = device_create(&remote_class,  NULL,
					chip->chr_devno, chip, chip->dev_name);
	if (IS_ERR_OR_NULL(dev))
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_init);

void ir_sys_device_attribute_sys(struct remote_chip *chip)
{
	device_destroy(&remote_class, chip->chr_devno);
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_sys);
