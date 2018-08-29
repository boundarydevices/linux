/*
 * drivers/amlogic/input/keyboard/pca9557_keypad.c
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
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/input.h>
#include <linux/of_platform.h>

#define PCA9557_INPUT_REG	0x00
#define PCA9557_OUTPUT_REG	0x01
#define PCA9557_POL_INV_REG	0x02
#define PCA9557_CONFIG_REG	0x03

#define DEFAULT_DELAY	200

#define DEVICE_NAME	"pca9557_keypad"

#define MAX_NAME_LEN	50

static struct pca9557_keypad {
	struct input_dev *input_dev;
	struct delayed_work work;
	int delay;
	int key_num;
	int key_tmp_val;
	int key_last_val;
	int major;
	int key_input_mask;
	struct i2c_client *i2c_client;
} *keypad_desc;

struct _key_des {
	char name[MAX_NAME_LEN];
	unsigned int key_val;
	int key_index_mask;
} *exp_key;

static void delay_work_func(
		struct work_struct *work)
{
	int i;
	int key_val = 0;
	unsigned long delay = msecs_to_jiffies(keypad_desc->delay);

	key_val = i2c_smbus_read_byte_data(keypad_desc->i2c_client,
		PCA9557_INPUT_REG) & keypad_desc->key_input_mask;
	if (keypad_desc->key_tmp_val != key_val) {
		keypad_desc->key_tmp_val = key_val;
		if (key_val != 0)
			keypad_desc->key_last_val = key_val;
		for (i = 0; i < keypad_desc->key_num; i++) {
			if (keypad_desc->key_last_val ==
				exp_key[i].key_index_mask) {
				if (key_val != 0) {
					pr_info("key \"%s\" down\n",
						exp_key[i].name);
					input_event(keypad_desc->input_dev,
						EV_KEY, exp_key[i].key_val, 1);
				} else {
					pr_info("key \"%s\" up\n",
						exp_key[i].name);
					input_event(keypad_desc->input_dev,
						EV_KEY, exp_key[i].key_val, 0);
				}
				input_event(keypad_desc->input_dev,
					EV_SYN, 0, 0);
				break;
			}

		}
	}
	schedule_delayed_work(&keypad_desc->work, delay);
}

static int pca9557_init_reg(struct i2c_client *client)
{

	int ret;

	ret = i2c_smbus_write_byte_data(client,
			PCA9557_POL_INV_REG, keypad_desc->key_input_mask);
	if (ret < 0) {
		pr_err("pca9557 init POL_INV reg fail!\n");
		return -1;
	}
	return 0;
}

static long pca9557_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long args)
{
	return 0;
}

static ssize_t pca9557_read(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations pca9557_fops = {
	.owner = THIS_MODULE,
	.read = pca9557_read,
	.compat_ioctl = pca9557_ioctl,
	.unlocked_ioctl = pca9557_ioctl,
};

static int pca9557_parse_child_dt(const struct device *dev)
{
	int ret, cnt;
	const char *uname;

	if (dev->of_node) {
		ret = of_property_read_u32(dev->of_node, "key_num",
			&keypad_desc->key_num);
		if (ret) {
			pr_err("failed to get key_num!\n");
			return -EINVAL;
		}
		exp_key = kcalloc(keypad_desc->key_num,
			sizeof(struct _key_des), GFP_KERNEL);

		for (cnt = 0; cnt < keypad_desc->key_num; cnt++) {
			ret = of_property_read_string_index(dev->of_node,
					"key_name", cnt, &uname);
			if (ret < 0) {
				pr_err("invalid key name index[%d]\n", cnt);
				return  -EINVAL;
			}
			strncpy(exp_key[cnt].name, uname, MAX_NAME_LEN);
			ret = of_property_read_u32_index(dev->of_node,
				"key_value", cnt, &exp_key[cnt].key_val);
			if (ret < 0) {
				pr_err("invalid key value index[%d]\n", cnt);
				return  -EINVAL;
			}
			ret = of_property_read_u32_index(dev->of_node,
				"key_index_mask", cnt,
				&exp_key[cnt].key_index_mask);
			if (ret < 0) {
				pr_err("invalid key index mask index[%d]\n",
					cnt);
				return  -EINVAL;
			}
		}
		ret = of_property_read_u32(dev->of_node, "key_input_mask",
			&keypad_desc->key_input_mask);
		if (ret) {
			pr_err("failed to get key_input_mask!\n");
			return -EINVAL;
		}
		return 0;
	}
	return -EINVAL;
}
static ssize_t delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "pca9557 schedule delay: %d\n", keypad_desc->delay);
	return ret;
}
static ssize_t delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 10, &keypad_desc->delay);
	if (ret != 0) {
		pr_info("set delay level fail, use default delay!!\n");
		keypad_desc->delay = DEFAULT_DELAY;
	}
	schedule_delayed_work(&keypad_desc->work,
		msecs_to_jiffies(keypad_desc->delay));
	return count;
}
DEVICE_ATTR_RW(delay);

static struct attribute *pca9557_attrs[] = {
	&dev_attr_delay.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pca9557);

static struct class pca9557_class = {
	.name		= "pca9557_keypad",
	.owner		= THIS_MODULE,
	.dev_groups = pca9557_groups,
};

static int pca9557_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	int ret, i;
	struct device *dev = &client->dev;

	pr_info("%s\n", __func__);
	keypad_desc = devm_kzalloc(dev, sizeof(struct pca9557_keypad),
		GFP_KERNEL);
	if (!keypad_desc)
		return -ENOMEM;
	keypad_desc->i2c_client = client;
	ret = pca9557_parse_child_dt(dev);
	if (ret < 0) {
		pr_err("%s,pca9557_parse_child_dt fail!\n", __func__);
		return -EINVAL;
	}

	ret = pca9557_init_reg(client);
	keypad_desc->input_dev = input_allocate_device();
	if (keypad_desc->input_dev == NULL) {
		pr_err("input_allocate_device err!\n");
		return -1;
	}
	set_bit(EV_SYN, keypad_desc->input_dev->evbit);
	set_bit(EV_KEY, keypad_desc->input_dev->evbit);
	for (i = 0; i < keypad_desc->key_num; i++)
		set_bit(exp_key[i].key_val, keypad_desc->input_dev->keybit);
	keypad_desc->input_dev->name = DEVICE_NAME;
	ret = input_register_device(keypad_desc->input_dev);
	if (ret != 0) {
		pr_err("input_register_device err!\n");
		return -1;
	}
	keypad_desc->delay = DEFAULT_DELAY;
	keypad_desc->key_tmp_val = 0;
	keypad_desc->major = register_chrdev(0, DEVICE_NAME, &pca9557_fops);
	class_register(&pca9557_class);
	device_create(&pca9557_class, NULL, MKDEV(keypad_desc->major, 0),
		NULL, DEVICE_NAME);

	INIT_DELAYED_WORK(&keypad_desc->work, delay_work_func);
	schedule_delayed_work(&keypad_desc->work, DEFAULT_DELAY);
	return 0;
}

static int pca9557_remove(struct i2c_client *client)
{
	device_destroy(&pca9557_class, MKDEV(keypad_desc->major, 0));
	cancel_delayed_work(&keypad_desc->work);

	input_unregister_device(keypad_desc->input_dev);
	input_free_device(keypad_desc->input_dev);
	return 0;
}
static const struct i2c_device_id pca9557_id[] = {
	{"pca9557_keypad", 0 },
	{}
};

static const struct of_device_id pca9557_dt_id[] = {
	{ .compatible = "amlogic,pca9557_keypad",},
	{},
};

static struct i2c_driver pca9557_drv = {
	.driver = {
		.name = "pca9557_keypad",
		.owner = THIS_MODULE,
		.of_match_table = pca9557_dt_id,
	},
	.probe = pca9557_probe,
	.remove = pca9557_remove,
	.id_table = pca9557_id,
};

static int __init pca9557_init(void)
{
	return i2c_add_driver(&pca9557_drv);
}

static void __exit pca9557_exit(void)
{
	i2c_del_driver(&pca9557_drv);
}

module_init(pca9557_init);
module_exit(pca9557_exit);
MODULE_AUTHOR("www.amlogic.com");
MODULE_DESCRIPTION("pca9557 driver for keypad");
MODULE_LICENSE("GPL");


