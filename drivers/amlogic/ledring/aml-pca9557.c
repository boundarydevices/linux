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
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/input.h>
#include <linux/of_platform.h>

#define addr_led_reg0     0    //input data
#define addr_led_reg1     1    //output data
#define addr_led_reg2     2    //inversion
#define addr_led_reg3     3    //direction

#define MUTE_KEY    	  0x04
#define VOLUP_KEY    	  0x08
#define VOLDOWN_KEY    	  0x10
#define PAULSE_KEY    	  0x20
#define RELEASE_KEY       0x00

#define DEFAULT_SPEED     230  //default speed 230ms

#define LED_DEVICE_NAME             "ledring"
#define LED_CHAR_DEV_NAME           "aml-led"

#define CMD_LEDRING_ARG             0x100001
#define CMD_LED_MUTE_ARG            0x100002
#define CMD_LED_LISTENING_ARG       0x100003
#define LED_OFF                     0
#define LED_ON                      1
#define MAX_NAME_LEN                50

static struct kobject *ledring_kobj;
static struct i2c_client *g_client;

static int m_temp = -1;
static int m_index;
static int timeout_flag;
static int timeout;

static struct _key_led {
	struct input_dev *pca_input_dev;
	char dev_name[MAX_NAME_LEN];
	struct timer_list mtimer;
	struct work_struct list_work;
	int run_time;
	int key_num;
	int key_tmp_val;
	int key_last_val;
	int action_times;
	struct class *cls;
	int major;
	int mode;
} *key_led_des;

static struct _key_des {
	char name[MAX_NAME_LEN];
	unsigned int key_val;
	int pin;
	int irq;
} *exp_key;

static struct _style {
	int num;		/*the number of led*/
	int times;		/*1: stop after action once, 0: cycle action*/
	int speed; 		/*speed. N(ms)*/
	int timeout; 		/*timeout N(s)*/
	int style[12][12];	/*style data*/
} styleData = {
	6, 0, DEFAULT_SPEED, 0,
	{
	       /*led1  led2  led3  led4  led5  led6*/
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, /*state1*/
		{0x00, 0x00, 0x00, 0x00, 0x01, 0x00}, /*state2*/
		{0x00, 0x00, 0x00, 0x01, 0x00, 0x00}, /*state3*/
		{0x00, 0x00, 0x01, 0x00, 0x00, 0x00}, /*state4*/
		{0x00, 0x01, 0x00, 0x00, 0x00, 0x00}, /*state5*/
		{0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, /*state6*/
	},
};

static int leds_light(void);
static int leds_init(int mode);

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
	key_led_des->run_time = m_speed;
	schedule_work(&key_led_des->list_work);
	mod_timer(&key_led_des->mtimer,
			jiffies+key_led_des->run_time*HZ/1000);
	return count;
}

static ssize_t run_state_read(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	pr_info("close ledring: echo 0 > runflag\n");
	pr_info("open ledring:  echo 1 > runflag\n");
	return 0;
}
static ssize_t run_state_write(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	int runflag;
	ret = kstrtouint(buf, 10, &runflag);
	if (ret == 0) {
		if (runflag == 0) {
			del_timer(&key_led_des->mtimer);
			pr_info("stop running ledring.\n");
		} else if (runflag == 1) {
			pr_info("start running ledring.\n");
			schedule_work(&key_led_des->list_work);
			mod_timer(&key_led_des->mtimer,
				jiffies+key_led_des->run_time*HZ/1000);
		}
	} else {
		pr_info("set run state fail!\n");
	}
	return count;
}
static ssize_t speed_level_read(
		struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	pr_info("current speed level: %d ms\n", key_led_des->run_time);
	return 0;
}

static struct kobj_attribute
running_attribute =
	__ATTR(runflag, 0664,
	run_state_read,
	run_state_write);
static struct kobj_attribute
speed_attribute =
	__ATTR(speed, 0664,
	speed_level_read,
	speed_level_write);
static struct attribute
*attrs[] = {
	&speed_attribute.attr,
	&running_attribute.attr,
	NULL,
};

static struct attribute_group
attr_group = {
	.attrs = attrs,
};

static void mtimer_function(
	unsigned long data)
{
	schedule_work(&key_led_des->list_work);
	mod_timer(&key_led_des->mtimer,
		jiffies+key_led_des->run_time*HZ/1000);
}

static void list_work_func(
	struct work_struct *work)
{
	int ret;
	int key_val = 0;

	if (key_led_des->mode == 0) {
		if (timeout_flag == 0) {
			leds_light();
		} else {
			timeout--;
			if (timeout <= 0) {
				timeout = 0;
				del_timer(&key_led_des->mtimer);
				ret = i2c_smbus_write_byte_data(g_client,
					addr_led_reg1, 0);
				if (ret < 0)
					pr_err("led set reg1 fail!\n");
			} else
				leds_light();
		}
	} else if (key_led_des->mode == 1) {
		key_val = i2c_smbus_read_byte_data(g_client, addr_led_reg0) & 0x3C;
		if (key_led_des->key_tmp_val != key_val) {
			key_led_des->key_tmp_val = key_val;
			if (key_val != 0)
				key_led_des->key_last_val = key_val;
			switch (key_led_des->key_last_val) {
				case MUTE_KEY:
					if (key_val != 0) {
						pr_info("key \"%s\" down\n", exp_key[0].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[0].key_val,1);
					} else {
						pr_info("key \"%s\" up\n", exp_key[0].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[0].key_val,0);
					}
					input_event(key_led_des->pca_input_dev,EV_SYN,0,0);
				break;
				case VOLUP_KEY:
					if (key_val != 0) {
						pr_info("key \"%s\" down\n", exp_key[2].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[2].key_val,1);
					} else {
						pr_info("key \"%s\" up\n", exp_key[2].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[2].key_val,0);
					}
					input_event(key_led_des->pca_input_dev,EV_SYN,0,0);
				break;
				case VOLDOWN_KEY:
					if (key_val != 0) {
						pr_info("key \"%s\" down\n", exp_key[3].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[3].key_val,1);
					} else {
						pr_info("key \"%s\" up\n", exp_key[3].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[3].key_val,0);
					}
					input_event(key_led_des->pca_input_dev,EV_SYN,0,0);
				break;
				case PAULSE_KEY:
					if (key_val != 0) {
						pr_info("key \"%s\" down\n", exp_key[1].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[1].key_val,1);
					} else {
						pr_info("key \"%s\" up\n", exp_key[1].name);
						input_event(key_led_des->pca_input_dev,EV_KEY,exp_key[1].key_val,0);
					}
					input_event(key_led_des->pca_input_dev,EV_SYN,0,0);
				break;
				default:
					pr_info("invalid key, please check!\n");
				break;
			}
		}
	}
}

static int setup_timer_task(int mode)
{
	setup_timer(&key_led_des->mtimer, mtimer_function, mode);
	mod_timer(&key_led_des->mtimer, jiffies+key_led_des->run_time*HZ/1000);
	INIT_WORK(&key_led_des->list_work, list_work_func);
	return 0;
}

static int leds_init(int mode)
{
	int ret;
	if (mode == 0) {
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
	} else if (mode ==1) {
		ret = i2c_smbus_write_byte_data(g_client,
			addr_led_reg3, 0x3F);
		if (ret < 0) {
			pr_err("led init reg3 fail!\n");
			return -1;
		}
		ret = i2c_smbus_write_byte_data(g_client,
			addr_led_reg1, 0xC0);
		if (ret < 0) {
			pr_err("led init reg1 fail!\n");
			return -1;
		}
	} else
	pr_info("leds_init fail,%d mode is not support!\n", mode);

	return 0;
}

static int leds_light(void)
{
	int ret = 0;
	int i = 0;
	int temp = 0;

	for (i = 0; i < styleData.num; i++) {
		temp |= styleData.style[m_index][i]
			<< (styleData.num-i+1);
	}
	if (m_temp != temp) {
		ret = i2c_smbus_write_byte_data(g_client,
			addr_led_reg1, temp);
		if (ret < 0)
			pr_err("led lit fail!\n");
		m_temp = temp;
	}
	if (++m_index > styleData.num-1) {
                if (styleData.times != 0) {
			if (--key_led_des->action_times <= 0)
				del_timer(&key_led_des->mtimer);
		}
		m_index = 0;
	}
	return 0;
}

static long leds_ioctl(struct file *file,
			unsigned int cmd,
			unsigned long args)
{
	int ret, val;

	switch (cmd) {
	case CMD_LEDRING_ARG:
		m_index = 0;
		del_timer(&key_led_des->mtimer);
		ret = i2c_smbus_write_byte_data(g_client,
				addr_led_reg1, 0);
		if (ret < 0)
			pr_err("led set reg1 fail!\n");
		ret = copy_from_user(&styleData,
			(int *)args, sizeof(styleData));
		if (styleData.speed < 0) {
			pr_info("set speed level fail,use default speed!!\n");
			key_led_des->run_time = DEFAULT_SPEED;
		} else if (key_led_des->run_time != styleData.speed)
			key_led_des->run_time = styleData.speed;
		if (styleData.timeout != 0) {
			timeout_flag = 1;
			timeout = styleData.timeout*1000/key_led_des->run_time;
		} else
			timeout_flag = 0;
		if (styleData.times < 0) styleData.times = 0;
		key_led_des->action_times = styleData.times;
		schedule_work(&key_led_des->list_work);
		mod_timer(&key_led_des->mtimer, jiffies+key_led_des->run_time*HZ/1000);
	break;
	case CMD_LED_MUTE_ARG :
		val = i2c_smbus_read_byte_data(g_client, addr_led_reg1);
		if (args == LED_OFF) {
			ret = i2c_smbus_write_byte_data(g_client,
						addr_led_reg1, (1<<7) | val);
			if (ret < 0) {
				pr_err("led init reg1 fail!\n");
			}
		} else if (args == LED_ON) {
			ret = i2c_smbus_write_byte_data(g_client,
						addr_led_reg1, ~(1<<7) & val);
			if (ret < 0) {
				pr_err("led init reg1 fail!\n");
			}
		}
	break;
	case CMD_LED_LISTENING_ARG :
		val = i2c_smbus_read_byte_data(g_client, addr_led_reg1);
		if (args == LED_OFF) {
			ret = i2c_smbus_write_byte_data(g_client,
						addr_led_reg1, (1<<6) | val);
			if (ret < 0) {
				pr_err("led init reg1 fail!\n");
			}
		} else if (args == LED_ON) {
			ret = i2c_smbus_write_byte_data(g_client,
						addr_led_reg1, ~(1<<6) & val);
			if (ret < 0) {
				pr_err("led init reg1 fail!\n");
			}
		}
	break;
	default:
	break;
	}
	return 0;
}

static ssize_t leds_read(struct file *filp, char __user *buf,
				size_t count,loff_t *ppos)
{
	unsigned long ret;
	ret = copy_to_user(buf, &key_led_des->mode,
		sizeof(key_led_des->mode));
	return count;
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.read = leds_read,
	.compat_ioctl = leds_ioctl,
	.unlocked_ioctl = leds_ioctl,
};

static int ledring_parse_child_dt(const struct device *dev, int *mode)
{
	int ret, cnt;
	int m_mode = 0;
	const char* uname;

	if(dev->of_node) {
		ret = of_property_read_u32(dev->of_node, "mode", &m_mode);
		if (ret) {
			pr_err("does not have a valid mode property\n");
			return -EINVAL;
		}
		*mode = m_mode;
		ret = of_property_read_string(dev->of_node, "key_dev_name", &uname);
		if (ret) {
			pr_err("does not have a valid key_dev_name property\n");
			return -EINVAL;
		}
		strncpy(key_led_des->dev_name, uname, MAX_NAME_LEN);
		if (m_mode == 1) {
			ret = of_property_read_u32(dev->of_node, "key_num", &key_led_des->key_num);
			if (ret) {
				pr_err("failed to get key_num!\n");
				return -EINVAL;
			}
			exp_key  = kzalloc(sizeof(struct _key_des)*key_led_des->key_num, GFP_KERNEL);
				if (!exp_key) {
					pr_err("exp_key alloc mem failed!\n");
					return -ENOMEM;
			}
			for (cnt = 0; cnt < key_led_des->key_num; cnt++) {
				ret = of_property_read_string_index(dev->of_node,
					"key_name", cnt, &uname);
				if (ret < 0) {
					pr_err("invalid key name index[%d]\n",cnt);
					return  -EINVAL;
				}
				strncpy(exp_key[cnt].name, uname, MAX_NAME_LEN);
				ret = of_property_read_u32_index(dev->of_node,
					"key_value", cnt, &exp_key[cnt].key_val);
				if (ret < 0) {
					pr_err("invalid key code index[%d]\n",cnt);
					return  -EINVAL;
				}
			}
		}
		return 0;
	}
	return -EINVAL;
}

static int ledring_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	int ret, i;
	int try_times = 0;
	struct device *dev = &client->dev;

	g_client = client;
	pr_info("%s\n", __func__);
	key_led_des = devm_kzalloc(dev, sizeof(struct _key_led), GFP_KERNEL);
	if (!key_led_des)
		return -ENOMEM;

	ret = ledring_parse_child_dt(dev, &key_led_des->mode);
	if (ret < 0) {
		pr_err("%s,ledring_parse_child_dt fail!\n", __func__);
		return -EINVAL;
	}

	if (key_led_des->mode == 0) {
		ledring_kobj = kobject_create_and_add("ledring", kernel_kobj);
		if (!ledring_kobj)
			return -ENOMEM;
		ret = sysfs_create_group(ledring_kobj, &attr_group);
		if (ret)
			goto err1;
		key_led_des->run_time = DEFAULT_SPEED;
		for (i = 0; i < 3; i++) {
			ret = leds_init(key_led_des->mode);
			if (ret != 0) {
				if (++try_times >= 3)
					goto err1;
			} else
				break;
		}
	} else {
		for (i = 0; i < 3; i++) {
			ret = leds_init(key_led_des->mode);
			if (ret != 0) {
				if (++try_times >= 3)
					goto err3;
			} else
				break;
		}
		key_led_des->pca_input_dev = input_allocate_device();
		if (key_led_des->pca_input_dev == NULL) {
			pr_err("input_allocate_device err!\n");
			goto err3;
		}
		set_bit(EV_SYN,key_led_des->pca_input_dev->evbit);
		set_bit(EV_KEY,key_led_des->pca_input_dev->evbit);
		for (i=0; i<key_led_des->key_num; i++) {
			set_bit(exp_key[i].key_val,key_led_des->pca_input_dev->keybit);
		}
		key_led_des->pca_input_dev->name = key_led_des->dev_name;
		ret=input_register_device(key_led_des->pca_input_dev);
		if (ret != 0) {
			pr_err("input_register_device err!\n");
			goto err2;
		}
		key_led_des->run_time = 100; //200ms
		key_led_des->key_tmp_val = 0;
	}
	setup_timer_task(key_led_des->mode);
	key_led_des->major = register_chrdev(0, LED_CHAR_DEV_NAME, &led_fops);
	key_led_des->cls = class_create(THIS_MODULE, LED_DEVICE_NAME);
	device_create(key_led_des->cls, NULL, MKDEV(key_led_des->major, 0), NULL, LED_DEVICE_NAME);

	return 0;
err1:
	kobject_put(ledring_kobj);
	return -ENOMEM;
err2:
	input_free_device(key_led_des->pca_input_dev);
	return -ENOMEM;
err3:
	return -ENOMEM;
}

static int ledring_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);

	device_destroy(key_led_des->cls, MKDEV(key_led_des->major, 0));
	class_destroy(key_led_des->cls);
	unregister_chrdev(key_led_des->major, LED_CHAR_DEV_NAME);
	flush_work(&key_led_des->list_work);
	del_timer(&key_led_des->mtimer);

	if (key_led_des->mode == 0) {
		kobject_put(ledring_kobj);
	} else {
		input_unregister_device(key_led_des->pca_input_dev);
		input_free_device(key_led_des->pca_input_dev);
	}
	return 0;
}

static int ledring_resume(struct device *dev)
{
	pr_info("enter ledring_resume.\n");

	schedule_work(&key_led_des->list_work);
	mod_timer(&key_led_des->mtimer,
		jiffies+key_led_des->run_time*HZ/1000);

	return 0;
}

static int ledring_suspend(struct device *dev)
{
	int ret;

	pr_info("enter ledring_suspend.\n");
	del_timer(&key_led_des->mtimer);
	ret = i2c_smbus_write_byte_data(g_client,
			addr_led_reg1, 0);
	if (ret < 0)
		pr_err("led set reg1 fail!\n");

	return 0;
}

static const struct dev_pm_ops ledring_pm = {
	.suspend = ledring_suspend,
	.resume = ledring_resume,
};

static struct i2c_driver ledring_drv = {
	.driver = {
		.name = "aml_ledring",
		.owner = THIS_MODULE,
		.of_match_table = ledring_dt_ids,
		.pm = &ledring_pm,
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
