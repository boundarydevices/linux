/*
 * Keypad driver for Cottonwood Creek Technologies Hook Switch
 *
 * Copyright 2014 Boundary Devices
 *
 * Licensed under the BSD or GPL-2 or later.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/input.h>

#define CWC_HOOKSWITCH_I2C_BUS         2
#define CWC_HOOKSWITCH_I2C_ADDR        0x20
#define CWC_HOOKSWITCH_I2C_BITMASK     0x40
#define CWC_HOOKSWITCH_POLL_INTERVAL   250
#define CWC_HOOKSWITCH_KEY_INTERVAL    50

static struct task_struct *thread;
static int previous_offhook_state;
static int previous_led_state;
struct i2c_client *myclient;
static struct input_dev *fake_keyboard_dev;

static const char fake_input_name[] = "cwc_hookswitch";

MODULE_DEVICE_TABLE(i2c, cwc_hookswitch_id);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/cwc_hookswitch/offhook [ro]
 * /sys/devices/platform/cwc_hookswitch/leds [rw]
 */
static ssize_t hookswitch_show_offhook
	(struct device *dev,
	 struct device_attribute *attr,
	 char *buf)
{
	return sprintf(buf, "%d\n", previous_offhook_state);
}

static ssize_t hookswitch_store_offhook
	(struct device *dev,
	 struct device_attribute *attr,
	 const char *buf,
	 size_t count)
{
	return -EINVAL;
}

static DEVICE_ATTR(offhook, S_IRUGO,
		   hookswitch_show_offhook,
		   hookswitch_store_offhook);

static ssize_t hookswitch_show_leds
	(struct device *dev,
	 struct device_attribute *attr,
	 char *buf)
{
	return sprintf(buf, "%d\n", previous_led_state);
}

static ssize_t hookswitch_store_leds
        (struct device *dev,
         struct device_attribute *attr,
         const char *buf,
         size_t count)
{
	int state;
	if (sscanf(buf, "%u", &state) == 1) {
		if (2 > (unsigned)state) {
			if (state != previous_led_state) {
				i2c_smbus_write_byte_data
					(myclient, 1, state ? 0x20 : 0);
				previous_led_state = state;
			}
			return count;
		}
	}
	return -EINVAL;
}

static DEVICE_ATTR(leds, S_IWUSR | S_IRUGO,
		   hookswitch_show_leds,
		   hookswitch_store_leds);

static struct attribute *hookswitch_attrs[] = {
	&dev_attr_offhook.attr,
	&dev_attr_leds.attr,
	NULL,
};

static struct attribute_group hookswitch_attr_group = {
	.attrs = hookswitch_attrs,
};

static void cwc_read_hookstate(void)
{
	int data;
	int offhook;
	unsigned int key;

	if (myclient != NULL) {
		data = i2c_smbus_read_byte_data(myclient, 0);
		offhook = ((data & CWC_HOOKSWITCH_I2C_BITMASK) == 0) ? 0 : 1;
		if (previous_offhook_state >= 0) {
			if (offhook != previous_offhook_state) {
				pr_debug("hookswitch: raw: 0x%02x  offhook: %d\n",
					 data, offhook);

				if (offhook)
					key = KEY_KPLEFTPAREN;
				else
					key = KEY_KPRIGHTPAREN;

				input_report_key(fake_keyboard_dev,
						 KEY_LEFTCTRL, 1);
				input_report_key(fake_keyboard_dev,
						 KEY_LEFTALT, 1);
				input_report_key(fake_keyboard_dev,
						 key, 1);
				input_sync(fake_keyboard_dev);
				msleep(CWC_HOOKSWITCH_KEY_INTERVAL);
				input_report_key(fake_keyboard_dev,
						 KEY_LEFTCTRL, 0);
				input_report_key(fake_keyboard_dev,
						 KEY_LEFTALT, 0);
				input_report_key(fake_keyboard_dev,
						 key, 0);
				input_sync(fake_keyboard_dev);
			}
		}
		previous_offhook_state = offhook;
	}
}

static int cwc_poll_thread(void *data)
{
	int ret = 0;

	allow_signal(SIGINT);
	while (!kthread_should_stop()) {
		if (ret == -ERESTARTSYS) {
			pr_info("%s: interrupted\n", __func__);
			return -EINTR;
		}

		cwc_read_hookstate();
		msleep(CWC_HOOKSWITCH_POLL_INTERVAL);
	}
	return ret;
}

static int cwc_hookswitch_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;

	myclient = client;
	previous_offhook_state = -1;

	i2c_smbus_write_byte_data(myclient, 3, 0xd0);
	i2c_smbus_write_byte_data(myclient, 1, 0x0);
	fake_keyboard_dev = input_allocate_device();
	if (!fake_keyboard_dev) {
		pr_err("%s() : cannot allocate fake keyboard: ", __func__);
		return -ENOMEM;
	}

	set_bit(EV_KEY, fake_keyboard_dev->evbit);
	set_bit(KEY_LEFTCTRL, fake_keyboard_dev->keybit);
	set_bit(KEY_LEFTALT, fake_keyboard_dev->keybit);
	set_bit(KEY_KPLEFTPAREN, fake_keyboard_dev->keybit);
	set_bit(KEY_KPRIGHTPAREN, fake_keyboard_dev->keybit);

	fake_keyboard_dev->name = fake_input_name;
	fake_keyboard_dev->id.bustype = BUS_USB;
	fake_keyboard_dev->id.vendor = 0x0525;
	fake_keyboard_dev->id.product = 0xa4a0;
	fake_keyboard_dev->id.version = 0x0001;

	ret = input_register_device(fake_keyboard_dev);
	if (ret)
		pr_err(KERN_ERR "%s() : failed to register device\n", __func__);

	thread = kthread_run(cwc_poll_thread, NULL, "%s", KBUILD_MODNAME);
	if (IS_ERR(thread))
		ret = PTR_ERR(thread);

	ret = sysfs_create_group(&fake_keyboard_dev->dev.kobj, &hookswitch_attr_group);
	if (ret)
		dev_err(&fake_keyboard_dev->dev, "sysfs export error: %d\n", ret);

	return ret;
}

static int cwc_hookswitch_remove(struct i2c_client *client)
{
	sysfs_remove_group(&fake_keyboard_dev->dev.kobj, &hookswitch_attr_group);
	input_unregister_device(fake_keyboard_dev);

	if (!IS_ERR_OR_NULL(thread))
		kthread_stop(thread);

	return 0;
}

static const struct i2c_device_id cwc_hookswitch_id[] = {
	{"cwc_hookswitch", 0},
	{"cwc_xx", 0},
	{},
};

static struct i2c_driver cwc_hookswitch_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "cwc_hookswitch",
	},
	.probe  = cwc_hookswitch_probe,
	.remove = cwc_hookswitch_remove,
	.id_table = cwc_hookswitch_id,
};

static int cwc_hookswitch_init(void)
{
	return i2c_add_driver(&cwc_hookswitch_i2c_driver);
}

static void cwc_hookswitch_exit(void)
{
	i2c_del_driver(&cwc_hookswitch_i2c_driver);
}

module_init(cwc_hookswitch_init);
module_exit(cwc_hookswitch_exit);

MODULE_DESCRIPTION("CWC Tech Hook Switch");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:cwctech-keys");
