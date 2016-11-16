/*
 * drivers/amlogic/input/keyboard/adc_keypad.c
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
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/amlogic/scpi_protocol.h>

#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/amlogic/saradc.h>
#include <linux/amlogic/adc_keypad.h>
#include <linux/of.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define POLL_PERIOD_WHEN_KEY_DOWN 10 /* unit msec */
#define POLL_PERIOD_WHEN_KEY_UP   50
#define KEY_JITTER_COUNT  2  /*  2 * POLL_PERIOD_WHEN_KEY_DOWN msec */

struct kp {
	struct input_dev *input;
	struct timer_list timer;
	unsigned int report_code;
	unsigned int code;
	unsigned int poll_period;
	int count;
	int config_major;
	char config_name[20];
	struct class *config_class;
	struct device *config_dev;
	int chan[SARADC_CHAN_NUM];
	int chan_num;
	struct adc_key *key;
	int key_num;
	struct work_struct work_update;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

static struct kp *gp_kp;
static int keypad_enable_flag = 1;
/* static int timer_count = 0; */

static int kp_search_key(struct kp *kp)
{
	struct adc_key *key;
	int value, i, j;

	for (i = 0; i < kp->chan_num; i++) {
		value = get_adc_sample(0, kp->chan[i]);
		if (value < 0)
			continue;
		key = kp->key;
		for (j = 0; j < kp->key_num; j++) {
			if ((key->chan == kp->chan[i])
			&& (value >= key->value - key->tolerance)
			&& (value <= key->value + key->tolerance)) {
				return key->code;
			}
			key++;
		}
	}

	return 0;
}

static void kp_work(struct kp *kp)
{
	int code = kp_search_key(kp);

	if (code)
		kp->poll_period = POLL_PERIOD_WHEN_KEY_DOWN;
	if ((!code) && (!kp->report_code)) {
		if (kp->poll_period < POLL_PERIOD_WHEN_KEY_UP)
			kp->poll_period++;
		return;
	} else if (code != kp->code) {
		kp->code = code;
		kp->count = 0;
	} else if (kp->count < KEY_JITTER_COUNT) {
		kp->count++;
		} else {
			if ((kp->report_code != code) && keypad_enable_flag) {
				if (!code) { /* key up */
				dev_info(&kp->input->dev,
					"key %d up\n", kp->report_code);
				input_report_key(kp->input, kp->report_code, 0);
				input_sync(kp->input);
			} else if (!kp->report_code) { /* key down */
				input_report_key(kp->input, code, 1);
				input_sync(kp->input);
			} else {
				dev_info(&kp->input->dev,
					"key %d up(f)\n", kp->report_code);
				input_report_key(kp->input, kp->report_code, 0);
				input_report_key(kp->input, code, 1);
				input_sync(kp->input);
			}
			kp->report_code = code;
		}
	}
}

static void update_work_func(struct work_struct *work)
{
	struct kp *kp = container_of(work, struct kp, work_update);

	kp_work(kp);
}

static void kp_timer_sr(unsigned long data)
{
	struct kp *kp = (struct kp *)data;

	schedule_work(&(kp->work_update));
	mod_timer(&kp->timer, jiffies+msecs_to_jiffies(kp->poll_period));
}

static int
adckpd_config_open(struct inode *inode, struct file *file)
{
	file->private_data = gp_kp;
	return 0;
}

static int
adckpd_config_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations keypad_fops = {
	.owner      = THIS_MODULE,
	.open       = adckpd_config_open,
	.release    = adckpd_config_release,
};

static int register_keypad_dev(struct kp  *kp)
{
	int ret = 0;

	strcpy(kp->config_name, "am_adc_kpd");
	ret = register_chrdev(0, kp->config_name, &keypad_fops);
	if (ret <= 0) {
		dev_info(&kp->input->dev, "register char device error\n");
		return  ret;
	}
	kp->config_major = ret;
	kp->config_class = class_create(THIS_MODULE, kp->config_name);
	kp->config_dev = device_create(kp->config_class,	NULL,
	MKDEV(kp->config_major, 0), NULL, kp->config_name);
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kp_early_suspend(struct early_suspend *h)
{
	struct kp *kp = container_of(h, struct kp, early_suspend);

	del_timer_sync(&kp->timer);
	cancel_work_sync(&kp->work_update);
}

static void kp_late_resume(struct early_suspend *h)
{
	struct kp *kp = container_of(h, struct kp, early_suspend);

	 mod_timer(&kp->timer, jiffies+msecs_to_jiffies(kp->poll_period));
}
#endif

/*
 *typedef enum
 *{
 *	POWER_WAKEUP_POWER,      /// only power key resume
 *	POWER_WAKEUP_ANY,        /// any key resume
 *	POWER_WAKEUP_NONE        /// no key can resume
 *}EN_FPP_POWER_KEYPAD_WAKEUP_METHOD;
 *
 *typedef enum
 *{
 *	KEYPAD_UNLOCK,///unlock, normal mode
 *	KEYPAD_LOCK /// lock, press key will be not useful
 *}EN_FPP_SYSTEM_KEYPAD_MODE;
 */

static char adc_key_mode_name[20] = "abcdef";
static char kernelkey_en_name[20] = "abcdef";

static void send_data_to_bl301(void)
{
	u32 val;

	if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_POWER")) {
		val = 0;
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_ANY")) {
		val = 1;
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	} else if (!strcmp(adc_key_mode_name, "POWER_WAKEUP_NONE")) {
		val = 2;
		scpi_send_usr_data(SCPI_CL_POWER, &val, sizeof(val));
	}

}

void kernel_keypad_enable_mode_enable(void)
{

	if (!strcmp(kernelkey_en_name, "KEYPAD_UNLOCK"))
		keypad_enable_flag = 1;
	else if (!strcmp(kernelkey_en_name, "KEYPAD_LOCK"))
		keypad_enable_flag = 0;
	else
		keypad_enable_flag = 1;
}

static int kp_probe(struct platform_device *pdev)
{
	struct kp *kp;

	struct input_dev *input_dev;
	int i, j, ret, key_size, name_len;
	int new_chan_flag;
	struct adc_key *key;
	struct adc_kp_platform_data *pdata = NULL;
	int *key_param = NULL;
	int state = 0;

	send_data_to_bl301();
	kernel_keypad_enable_mode_enable();

#ifdef CONFIG_OF
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "adc_key: pdev->dev.of_node == NULL!\n");
		state =  -EINVAL;
		goto get_key_node_fail;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "key_num", &key_size);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get key_num!\n");
		state =  -EINVAL;
		goto get_key_node_fail;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "name_len", &name_len);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get name_len!\n");
		name_len = 20;
	}
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		state = -EINVAL;
		goto get_key_node_fail;
	}
	pdata->key = kcalloc(key_size, sizeof(*(pdata->key)), GFP_KERNEL);
	if (!(pdata->key)) {
		dev_err(&pdev->dev, "platform key is required!\n");
		goto get_key_mem_fail;
	}
	pdata->key_num = key_size;
	for (i = 0; i < key_size; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
			 "key_name", i, &(pdata->key[i].name));
		if (ret < 0) {
			dev_err(&pdev->dev,
				 "adc_key: find key_name=%d finished\n", i);
			break;
		}
	}
	key_param =
		 kzalloc(4*(sizeof(*key_param))*(pdata->key_num), GFP_KERNEL);
	if (!key_param) {
		dev_err(&pdev->dev, "adc_key: key_param can not get mem\n");
		goto get_param_mem_fail;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		 "key_code", key_param, pdata->key_num);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get key_code!\n");
		goto get_key_param_failed;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		 "key_chan", key_param+pdata->key_num, pdata->key_num);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get key_chan!\n");
		goto get_key_param_failed;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		 "key_val", key_param+pdata->key_num*2, pdata->key_num);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get key_val!\n");
		goto get_key_param_failed;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		 "key_tolerance", key_param+pdata->key_num*3, pdata->key_num);
	if (ret) {
		dev_err(&pdev->dev, "adc_key: failed to get tolerance!\n");
		goto get_key_param_failed;
	}
	for (i = 0; i < pdata->key_num; i++) {
		pdata->key[i].code = *(key_param+i);
		pdata->key[i].chan = *(key_param+pdata->key_num+i);
		pdata->key[i].value = *(key_param+pdata->key_num*2+i);
		pdata->key[i].tolerance = *(key_param+pdata->key_num*3+i);
	}

#else
	pdata = pdev->dev.platform_data;
#endif
	kp = kzalloc(sizeof(struct kp), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!kp || !input_dev) {
		kfree(kp);
		input_free_device(input_dev);
		state = -ENOMEM;
		goto get_key_param_failed;
	}
	gp_kp = kp;
	platform_set_drvdata(pdev, pdata);
	kp->input = input_dev;
	kp->report_code = 0;
	kp->code = 0;
	kp->poll_period = POLL_PERIOD_WHEN_KEY_UP;
	kp->count = 0;
	INIT_WORK(&(kp->work_update), update_work_func);
	setup_timer(&kp->timer, kp_timer_sr, (unsigned long)kp);
	mod_timer(&kp->timer, jiffies+msecs_to_jiffies(100));
	/* setup input device */
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_REP, input_dev->evbit);

	kp->key = pdata->key;
	kp->key_num = pdata->key_num;

	key = pdata->key;
	kp->chan_num = 0;
	for (i = 0; i < kp->key_num; i++) {
		set_bit(key->code, input_dev->keybit);
		/* search the key chan */
		new_chan_flag = 1;
		for (j = 0; j < kp->chan_num; j++) {
			if (key->chan == kp->chan[j]) {
				new_chan_flag = 0;
				break;
			}
		}
		if (new_chan_flag) {
			kp->chan[kp->chan_num] = key->chan;
			kp->chan_num++;
		}
		dev_info(&pdev->dev,
			 "%s key(%d) registed.\n", key->name, key->code);
		key++;
	}

	input_dev->name = "adc_keypad";
	input_dev->phys = "adc_keypad/input0";
	input_dev->dev.parent = &pdev->dev;

	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	input_dev->rep[REP_DELAY] = 0xffffffff;
	input_dev->rep[REP_PERIOD] = 0xffffffff;

	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = 0x1ff;

	ret = input_register_device(kp->input);
	if (ret < 0) {
		dev_err(&pdev->dev,
			 "Unable to register keypad input device.\n");
		kfree(kp);
		input_free_device(input_dev);
		state = -EINVAL;
		goto get_key_param_failed;
	}
	register_keypad_dev(gp_kp);
	kfree(key_param);
	#ifdef CONFIG_HAS_EARLYSUSPEND
	kp->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	kp->early_suspend.suspend = kp_early_suspend;
	kp->early_suspend.resume = kp_late_resume;
	register_early_suspend(&kp->early_suspend);
	#endif
	return 0;

get_key_param_failed:
		kfree(key_param);
get_param_mem_fail:
		kfree(pdata->key);
get_key_mem_fail:
		kfree(pdata);
get_key_node_fail:
	return state;
}

static int kp_remove(struct platform_device *pdev)
	{
	struct adc_kp_platform_data *pdata = platform_get_drvdata(pdev);
	struct kp *kp = gp_kp;

	#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&kp->early_suspend);
	#endif
	del_timer_sync(&kp->timer);
	cancel_work_sync(&kp->work_update);
	input_unregister_device(kp->input);
	input_free_device(kp->input);
	unregister_chrdev(kp->config_major, kp->config_name);
	if (kp->config_class) {
		if (kp->config_dev)
			device_destroy(kp->config_class,
				 MKDEV(kp->config_major, 0));
		class_destroy(kp->config_class);
	}
	kfree(kp);
	#ifdef CONFIG_OF
	kfree(pdata->key);
	kfree(pdata);
	#endif
	gp_kp = NULL;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id key_dt_match[] = {
	{	.compatible = "amlogic, adc_keypad",},
	{},
};
#else
#define key_dt_match NULL
#endif

static struct platform_driver kp_driver = {
	.probe      = kp_probe,
	.remove     = kp_remove,
	.suspend    = NULL,
	.resume     = NULL,
	.driver     = {
		.name   = "m1-adckp",
		.of_match_table = key_dt_match,
	},
};

static int __init kp_init(void)
{
	return platform_driver_register(&kp_driver);
}

static void __exit kp_exit(void)
{
	platform_driver_unregister(&kp_driver);
}

module_init(kp_init);
module_exit(kp_exit);

/*
 * Note: If the driver is compiled as a module, the __setup() do nothing
 *       __setup() is defined in include/linux/init.h
 */
#ifndef MODULE

static int __init adc_key_mode_para_setup(char *s)
{
	if (s != NULL)
		sprintf(adc_key_mode_name, "%s", s);

	return 0;
}
__setup("adckeyswitch=", adc_key_mode_para_setup);

static int __init kernel_keypad_enable_setup(char *s)
{
	if (s != NULL)
		sprintf(kernelkey_en_name, "%s", s);

	return 0;
}
__setup("kernelkey_enable=", kernel_keypad_enable_setup);

#endif

MODULE_AUTHOR("Robin Zhu");
MODULE_DESCRIPTION("ADC Keypad Driver");
MODULE_LICENSE("GPL");
