/*
 * drivers/amlogic/ledring/ledring.c
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
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>

#define addr_led_reg0     0    //input data
#define addr_led_reg1     1    //output data
#define addr_led_reg2     2    //inversion
#define addr_led_reg3     3    //direction

#define DEFAULT_SPEED     230  //default speed 230ms

#define DEVICE_NAME       "ledring"
#define CHAR_DEV_NAME     "aml_ledring"

#define CMD_LEDRING_ARG   0x100001

static int major;
static struct class *cls;
static struct timer_list mtimer;
static struct work_struct ledring_work;
static int ledring_speed;
static struct kobject *ledring_kobj;
static struct i2c_client *g_client;
static int m_index;
static int tmp1_rgb_style, tmp2_rgb_style, tmp3_rgb_style;
static int tmp4_rgb_style, tmp5_rgb_style, tmp6_rgb_style;
static int timeout_flag;
static int timeout;

static struct _style {
	int num;
	int speed;
	int timeout;
	int style[6][6];
} styleData = {
	6, DEFAULT_SPEED, 0,
	{
	       /*rgb1 rgb2 rgb3 rgb4 rgb5 rgb6*/
		{0x80, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x40, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x20, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x10, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x08, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x04, 0x0, 0x0, 0x0, 0x0, 0x0},
	},
};

static int leds_light(void);
static int leds_init(void);

static const struct i2c_device_id ledring_id[] = {
	{"aml_pca9557"},
	{}
};
MODULE_DEVICE_TABLE(i2c, ledring_id);

static const struct of_device_id ledring_dt_ids[] = {
	{
		.compatible = "aml, ledring",
		.data = (void *)NULL
	},
	{},
};
MODULE_DEVICE_TABLE(of, ledring_dt_ids);

static ssize_t speed_level_write(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	int m_speed;

	ret = kstrtouint(buf, 10, &m_speed);
	if (ret == 0) {
		pr_info("set speed level: %d ms\n", m_speed);
	} else {
		pr_info("set speed level fail, use default speed!!\n");
		m_speed = DEFAULT_SPEED;
	}
	ledring_speed = m_speed;
	schedule_work(&ledring_work);
	mod_timer(&mtimer, jiffies+ledring_speed*HZ/1000);
	return count;
}

static ssize_t speed_level_read(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	pr_info("current speed level: %d ms\n", ledring_speed);
	return 0;
}

static struct kobj_attribute
speed_attribute =
	__ATTR(speed, 0664,
	speed_level_read,
	speed_level_write);
static struct attribute
*attrs[] = {
	&speed_attribute.attr,
	NULL,
};

static struct attribute_group
attr_group = {
	.attrs = attrs,
};

static void mtimer_function(
	unsigned long data)
{
	schedule_work(&ledring_work);
	mod_timer(&mtimer, jiffies+ledring_speed*HZ/1000);
}

static void ledring_work_func(
	struct work_struct *work)
{
	int ret;

	if (timeout_flag == 0) {
		leds_light();
	} else {
		timeout--;
		if (timeout <= 0) {
			timeout = 0;
			del_timer(&mtimer);
			ret = i2c_smbus_write_byte_data(g_client,
					addr_led_reg1, 0);
			if (ret < 0)
				pr_err("led set reg1 fail!\n");
		} else
			leds_light();
	}
}

static void setup_timer_task(void)
{
	setup_timer(&mtimer, mtimer_function, 0);
	mod_timer(&mtimer, jiffies+ledring_speed*HZ/1000);
	INIT_WORK(&ledring_work, ledring_work_func);
}

static int leds_init(void)
{
	int ret;

	ret = i2c_smbus_write_byte_data(g_client,
	addr_led_reg3, 0);
	if (ret < 0) {
		pr_err("led init reg3 fail!\n");
		return -1;
	}
	ret = i2c_smbus_write_byte_data(g_client,
		addr_led_reg1, 0);
	if (ret < 0) {
		pr_err("led init reg1 fail!\n");
		return -1;
	}
	return 0;
}

static int leds_light(void)
{
	int ret;

	if ((tmp1_rgb_style != styleData.style[m_index][0]) ||
		(tmp2_rgb_style != styleData.style[m_index][1]) ||
		(tmp3_rgb_style != styleData.style[m_index][2]) ||
		(tmp4_rgb_style != styleData.style[m_index][3]) ||
		(tmp5_rgb_style != styleData.style[m_index][4]) ||
		(tmp6_rgb_style != styleData.style[m_index][5])) {
		ret = i2c_smbus_write_byte_data(g_client,
			addr_led_reg1, styleData.style[m_index][0]);
		if (ret < 0)
			pr_err("led lit fail!\n");
		tmp1_rgb_style = styleData.style[m_index][0];
		tmp2_rgb_style = styleData.style[m_index][1];
		tmp3_rgb_style = styleData.style[m_index][2];
		tmp4_rgb_style = styleData.style[m_index][3];
		tmp5_rgb_style = styleData.style[m_index][4];
		tmp6_rgb_style = styleData.style[m_index][5];
	}

	if (++m_index > 5)
		m_index = 0;

	return 0;
}

static long ledring_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long args)
{
	int ret;

	switch (cmd) {

	case CMD_LEDRING_ARG:
		m_index = 0;
		del_timer(&mtimer);
		ret = i2c_smbus_write_byte_data(g_client,
				addr_led_reg1, 0);
		if (ret < 0)
			pr_err("led set reg1 fail!\n");

		ret = copy_from_user(&styleData,
			(int *)args, sizeof(styleData));
		if (styleData.speed < 0) {
			pr_info("set speed level fail,use default speed!!\n");
			ledring_speed = DEFAULT_SPEED;
		} else if (ledring_speed != styleData.speed)
			ledring_speed = styleData.speed;

		if (styleData.timeout != 0) {
			timeout_flag = 1;
			timeout = styleData.timeout*1000/ledring_speed;
		} else
			timeout_flag = 0;
		schedule_work(&ledring_work);
		mod_timer(&mtimer, jiffies+ledring_speed*HZ/1000);
	break;
	default:
	break;
	}

	return 0;
}

static const struct file_operations ledring_fops = {
	.owner = THIS_MODULE,
	//.unlocked_ioctl=ledring_ioctl,
	.compat_ioctl = ledring_ioctl,
};

static int ledring_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	int ret;

	pr_info("%s\n", __func__);

	ledring_kobj = kobject_create_and_add("ledring",
					kernel_kobj);
	if (!ledring_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(ledring_kobj, &attr_group);
	if (ret)
		goto err;

	g_client = client;

	ledring_speed = DEFAULT_SPEED;

	ret = leds_init();
	if (ret < 0)
		goto err;

	setup_timer_task();

	major = register_chrdev(major, CHAR_DEV_NAME,
					&ledring_fops);
	cls = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(cls, NULL, MKDEV(major, 0), NULL,
				DEVICE_NAME);
	return 0;
err:
	kobject_put(ledring_kobj);
	return -ENOMEM;
}

static int ledring_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(major, CHAR_DEV_NAME);

	flush_work(&ledring_work);
	del_timer(&mtimer);
	kobject_put(ledring_kobj);
	return 0;
}

static struct i2c_driver ledring_drv = {
	.driver = {
		.name = "aml_ledring",
		.owner = THIS_MODULE,
		.of_match_table = ledring_dt_ids,
	},
	.probe = ledring_probe,
	.remove = ledring_remove,
	.id_table = ledring_id,
};

static int __init ledring_init(void)
{
	return i2c_add_driver(&ledring_drv);
}

static void __exit ledring_exit(void)
{
	i2c_del_driver(&ledring_drv);
}

arch_initcall(ledring_init);
module_exit(ledring_exit);
MODULE_AUTHOR("renjun.xu <renjun.xu@amlogic.com>");
MODULE_DESCRIPTION("i2c driver for ledring");
MODULE_LICENSE("GPL");
