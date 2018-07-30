/*
 * drivers/amlogic/input/avin_detect/avin_detect.c
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
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <uapi/linux/input.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "avin_detect.h"
#include <linux/gpio.h>

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

#undef pr_fmt
#define pr_fmt(fmt)    "avin-detect: " fmt

#define DEBUG_DEF  1
#define INPUT_REPORT_SWITCH 0
#define LOOP_DETECT_TIMES 3

#define MAX_AVIN_DEVICE_NUM  3
#define AVIN_NAME  "avin_detect"
#define AVIN_NAME_CH1  "avin_detect_ch1"
#define AVIN_NAME_CH2  "avin_detect_ch2"
#define AVIN_NAME_CH3  "avin_detect_ch3"
#define ABS_AVIN_1 0
#define ABS_AVIN_2 1
#define ABS_AVIN_3 2

static char *avin_name_ch[3] = {AVIN_NAME_CH1, AVIN_NAME_CH2, AVIN_NAME_CH3};
static char avin_ch[3] = {AVIN_CHANNEL1, AVIN_CHANNEL2, AVIN_CHANNEL3};

static DECLARE_WAIT_QUEUE_HEAD(avin_waitq);

static inline void avin_disable_irq(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc->depth)
		disable_irq_nosync(irq);
}

static inline void avin_enable_irq(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc->depth)
		return;

	enable_irq(irq);
}

static irqreturn_t avin_detect_handler(int irq, void *data)
{
	int i;
	struct avin_det_s *avdev = (struct avin_det_s *)data;

	for (i = 0; i <= avdev->dts_param.dts_device_num; i++) {
		if (irq == avdev->hw_res.irq_num[i])
			break;
		else if (i == avdev->dts_param.dts_device_num)
			return IRQ_HANDLED;
	}

	if (avdev->code_variable.loop_detect_times[i]++
		== LOOP_DETECT_TIMES) {
		avdev->code_variable.irq_falling_times[
			i * avdev->dts_param.dts_detect_times +
			avdev->code_variable.detect_channel_times[i]]++;
		avdev->code_variable.pin_mask_irq_flag[i] = 1;
		/*avdev->code_variable.loop_detect_times[i] = 0;*/
		schedule_work(&(avdev->work_struct_maskirq));
	}
	return IRQ_HANDLED;
}

/* must open irq >100ms later,then into timer handler */
static void avin_timer_sr(unsigned long data)
{
	int i;
	struct avin_det_s *avdev = (struct avin_det_s *)data;

	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		if (avdev->code_variable.detect_channel_times[i] <
			(avdev->dts_param.dts_detect_times-1)) {
			avdev->code_variable.detect_channel_times[i]++;
			if (avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times +
				avdev->code_variable.detect_channel_times[
					i]-1] != 0) {
				avdev->code_variable.loop_detect_times[i] = 0;
				/* avin_enable_irq(avdev->hw_res.irq_num[i]); */
			}
			avin_enable_irq(avdev->hw_res.irq_num[i]);
			if (avdev->code_variable.detect_channel_times[
				i] == 1) {
				avdev->code_variable.irq_falling_times[
				(i+1) * avdev->dts_param.dts_detect_times
				- 1] = 0;
			} else if (avdev->code_variable.detect_channel_times[i]
			== (avdev->dts_param.dts_detect_times-1)) {
				schedule_work(&(avdev->work_struct_update));
			}
		} else {
			avdev->code_variable.detect_channel_times[i] = 0;
			avdev->code_variable.loop_detect_times[i] = 0;
			avin_enable_irq(avdev->hw_res.irq_num[i]);
		}
	}
	mod_timer(&avdev->timer,
	jiffies+msecs_to_jiffies(avdev->dts_param.dts_interval_length));
}

static void kp_work_channel1(struct avin_det_s *avdev)
{
	int i, j;

	mutex_lock(&avdev->lock);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		for (j = 0; j < (avdev->dts_param.dts_detect_times-1); j++) {
			if (avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times + j] == 0)
				avdev->code_variable.actual_into_irq_times[i]++;

			avdev->code_variable.irq_falling_times[
				i * avdev->dts_param.dts_detect_times + j] = 0;
		}

		if (avdev->code_variable.actual_into_irq_times[i] >=
			((avdev->dts_param.dts_detect_times - 1)
			- avdev->dts_param.dts_fault_tolerance)) {
			if (avdev->code_variable.ch_current_status[i]
				!= AVIN_STATUS_OUT) {
				avdev->code_variable.ch_current_status[i]
					= AVIN_STATUS_OUT;
				#if INPUT_REPORT_SWITCH
				input_report_abs(avdev->input_dev,
				 ABS_AVIN_1, AVIN_STATUS_OUT);
				input_sync(avdev->input_dev);
				#endif
				avdev->code_variable.report_data_s[i].channel
				= avin_ch[i];
				avdev->code_variable.report_data_s[i].status
					= AVIN_STATUS_OUT;
				avdev->code_variable.report_data_flag = 1;
				wake_up_interruptible(&avin_waitq);
				#if DEBUG_DEF
				pr_info("avin ch%d current_status out!\n", i);
				#endif
			}
		} else if (avdev->code_variable.actual_into_irq_times[i] <=
			avdev->dts_param.dts_fault_tolerance) {
			if (avdev->code_variable.ch_current_status[i]
				!= AVIN_STATUS_IN) {
				avdev->code_variable.ch_current_status[i]
					= AVIN_STATUS_IN;
				#if INPUT_REPORT_SWITCH
				input_report_abs(avdev->input_dev,
				 ABS_AVIN_1, AVIN_STATUS_IN);
				input_sync(avdev->input_dev);
				#endif
				avdev->code_variable.report_data_s[i].channel
				= avin_ch[i];
				avdev->code_variable.report_data_s[i].status
					= AVIN_STATUS_IN;
				avdev->code_variable.report_data_flag = 1;
				wake_up_interruptible(&avin_waitq);
				#if DEBUG_DEF
				pr_info("avin ch%d current_status in!\n", i);
				#endif
			}
		} else {
			/*keep current status*/
		}
	}
	memset(avdev->code_variable.actual_into_irq_times, 0,
		sizeof(avdev->code_variable.actual_into_irq_times[0]) *
		avdev->dts_param.dts_device_num);

	mutex_unlock(&avdev->lock);
}

static void update_work_update_status(struct work_struct *work)
{
	struct avin_det_s *avin_data =
	container_of(work, struct avin_det_s, work_struct_update);

	kp_work_channel1(avin_data);
}

static void update_work_maskirq(struct work_struct *work)
{
	int i;
	struct avin_det_s *avdev =
	container_of(work, struct avin_det_s, work_struct_maskirq);

	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		if (avdev->code_variable.pin_mask_irq_flag[i] == 1) {
			avin_disable_irq(avdev->hw_res.irq_num[i]);
			avdev->code_variable.pin_mask_irq_flag[i] = 0;
		}
	}
}

static int aml_sysavin_dts_parse(struct platform_device *pdev)
{
	int ret;
	int i;
	int state;
	int value;
	struct avin_det_s *avdev;

	avdev = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
	"avin_device_num", &value);
	avdev->dts_param.dts_device_num = value;
	if (ret) {
		pr_info("Failed to get dts_device_num.\n");
		goto get_avin_param_failed;
	} else {
		if (avdev->dts_param.dts_device_num == 0) {
			pr_info("avin device num is 0\n");
			goto get_avin_param_failed;
		} else if (avdev->dts_param.dts_device_num >
		MAX_AVIN_DEVICE_NUM) {
			pr_info("avin device num is > MAX NUM\n");
			goto get_avin_param_failed;
		}
	}

	ret = of_property_read_u32(pdev->dev.of_node,
	"detect_interval_length", &value);
	if (ret) {
		pr_info("Failed to get dts_interval_length.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_interval_length = value;

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_detect_times", &value);
	if (ret) {
		pr_info("Failed to get dts_detect_times.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_detect_times = value + 1;

	ret = of_property_read_u32(pdev->dev.of_node,
	"set_fault_tolerance", &value);
	if (ret) {
		pr_info("Failed to get dts_fault_tolerance.\n");
		goto get_avin_param_failed;
	}
	avdev->dts_param.dts_fault_tolerance = value;

	/* request resource of pin */
	avdev->hw_res.pin =
		devm_kzalloc(&pdev->dev, (sizeof(struct gpio_desc *)
		* avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->hw_res.pin) {
		state = -ENOMEM;
		goto get_avin_param_failed;
	}
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		avdev->hw_res.pin[i] = devm_gpiod_get_index(&pdev->dev,
				NULL, i, GPIOD_IN);

		if (IS_ERR_OR_NULL(avdev->hw_res.pin[i])) {
			state = -EINVAL;
			goto get_avin_param_failed;
		}

		gpiod_set_pull(avdev->hw_res.pin[i], GPIOD_PULL_DIS);
	}

	/* request resource of irq num */
	avdev->hw_res.irq_num =
		devm_kzalloc(&pdev->dev, (sizeof(avdev->hw_res.irq_num[0])
		* avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->hw_res.irq_num) {
		state = -ENOMEM;
		goto get_avin_param_failed;
	}

	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		avdev->hw_res.irq_num[i] = gpiod_to_irq(avdev->hw_res.pin[i]);

	return 0;

get_avin_param_failed:
	return -EINVAL;
}

static int avin_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct avin_det_s *avindev;

	avindev = container_of(inode->i_cdev, struct avin_det_s, avin_cdev);
	file->private_data = avindev;
	return ret;
}

static ssize_t avin_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	unsigned long ret;
	struct avin_det_s *avin_data = (struct avin_det_s *)file->private_data;

	/*wait_event_interruptible(avin_waitq, avin_data->report_data_flag);*/
	ret = copy_to_user(buf,
		(void *)(avin_data->code_variable.report_data_s),
		sizeof(avin_data->code_variable.report_data_s[0])
		* avin_data->dts_param.dts_device_num);
	avin_data->code_variable.report_data_flag = 0;
	return 0;
}

static int avin_config_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static unsigned int avin_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct avin_det_s *avin_data = (struct avin_det_s *)file->private_data;

	poll_wait(file, &avin_waitq, wait);

	if (avin_data->code_variable.report_data_flag)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct file_operations avin_fops = {
	.owner      = THIS_MODULE,
	.open       = avin_open,
	.read       = avin_read,
	.poll       = avin_poll,
	.release    = avin_config_release,
};

static int register_avin_dev(struct avin_det_s *avin_data)
{
	int ret = 0;

	ret = alloc_chrdev_region(&avin_data->avin_devno,
		0, 1, "avin_detect_region");
	if (ret < 0) {
		pr_err("avin: failed to allocate major number\n");
		return -ENODEV;
	}

	/* connect the file operations with cdev */
	cdev_init(&avin_data->avin_cdev, &avin_fops);
	avin_data->avin_cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&avin_data->avin_cdev, avin_data->avin_devno, 1);
	if (ret) {
		pr_err("avin: failed to add device\n");
		return -ENODEV;
	}

	strcpy(avin_data->config_name, "avin_detect");
	avin_data->config_class = class_create(THIS_MODULE,
		avin_data->config_name);
	avin_data->config_dev = device_create(avin_data->config_class, NULL,
		avin_data->avin_devno, NULL, avin_data->config_name);
	if (IS_ERR(avin_data->config_dev)) {
		pr_err("avin: failed to create device node\n");
		ret = PTR_ERR(avin_data->config_dev);
		return ret;
	}

	return ret;
}

static int init_resource(struct avin_det_s *avdev)
{
	int i, j;

	INIT_WORK(&(avdev->work_struct_update),  update_work_update_status);
	INIT_WORK(&(avdev->work_struct_maskirq), update_work_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		for (j = 0; j < avdev->dts_param.dts_detect_times; j++)
			avdev->code_variable.irq_falling_times[
			i * avdev->dts_param.dts_detect_times + j] = 0;

		avdev->code_variable.loop_detect_times[i] = 0;
	}

	/* set timer */
	setup_timer(&avdev->timer, avin_timer_sr, (unsigned long)avdev);
	mod_timer(&avdev->timer, jiffies+msecs_to_jiffies(2000));

	return 0;
}

static int request_mem_resource(struct platform_device *pdev)
{
	int i;
	struct avin_det_s *avdev;

	avdev = platform_get_drvdata(pdev);

	avdev->code_variable.pin_mask_irq_flag =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.pin_mask_irq_flag[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.pin_mask_irq_flag)
		return -ENOMEM;

	avdev->code_variable.loop_detect_times =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.loop_detect_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.loop_detect_times)
		return -ENOMEM;

	avdev->code_variable.detect_channel_times =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.detect_channel_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.detect_channel_times)
		return -ENOMEM;

	avdev->code_variable.report_data_s =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.report_data_s[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.report_data_s)
		return -ENOMEM;

	avdev->code_variable.irq_falling_times =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.irq_falling_times[0]) *
		avdev->dts_param.dts_device_num
		* (avdev->dts_param.dts_detect_times)), GFP_KERNEL);
	if (!avdev->code_variable.irq_falling_times)
		return -ENOMEM;

	avdev->code_variable.actual_into_irq_times =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.actual_into_irq_times[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.actual_into_irq_times)
		return -ENOMEM;

	avdev->code_variable.ch_current_status =
		devm_kzalloc(&pdev->dev,
		(sizeof(avdev->code_variable.ch_current_status[0]) *
		avdev->dts_param.dts_device_num), GFP_KERNEL);
	if (!avdev->code_variable.ch_current_status)
		return -ENOMEM;

	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		avdev->code_variable.ch_current_status[i] = AVIN_STATUS_UNKNOWN;

	return 0;
}

int avin_detect_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	struct avin_det_s *avdev = NULL;

	avdev = devm_kzalloc(&pdev->dev,
			sizeof(struct avin_det_s), GFP_KERNEL);
	if (!avdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, avdev);

	ret = aml_sysavin_dts_parse(pdev);
	if (ret)
		return ret;

	ret = request_mem_resource(pdev);
	if (ret)
		return ret;

	init_resource(avdev);

	/* request irq num*/
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		ret = devm_request_irq(&pdev->dev, avdev->hw_res.irq_num[i],
		avin_detect_handler, IRQF_TRIGGER_FALLING,
		avin_name_ch[i], (void *)avdev);
		if (ret) {
			pr_info("Unable to request irq resource.\n");
			return -EINVAL;
		}
	}

	mutex_init(&avdev->lock);

	/* register input device */
	avdev->input_dev = input_allocate_device();
	if (avdev->input_dev == 0)
		return -ENOMEM;

	set_bit(EV_ABS, avdev->input_dev->evbit);
	input_set_abs_params(avdev->input_dev,
	 ABS_AVIN_1, 0, 2, 0, 0);
	input_set_abs_params(avdev->input_dev,
	 ABS_AVIN_2, 0, 2, 0, 0);
	avdev->input_dev->name = AVIN_NAME;
    /*avdev->input_dev->phys = "gpio_keypad/input0";*/
	avdev->input_dev->dev.parent = &pdev->dev;
	avdev->input_dev->id.bustype = BUS_ISA;
	avdev->input_dev->id.vendor  = 0x5f5f;
	avdev->input_dev->id.product = 0x6f6f;
	avdev->input_dev->id.version = 0x7f7f;

	ret = input_register_device(avdev->input_dev);
	if (ret < 0) {
		pr_info("Unable to register avin input device.\n");
		input_free_device(avdev->input_dev);
		return -EINVAL;
	}

	/* register char device */
	ret = register_avin_dev(avdev);
	if (ret)
		return ret;

	return 0;
}

static int avin_detect_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int i;
	struct avin_det_s *avdev = platform_get_drvdata(pdev);

	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_struct_update);
	cancel_work_sync(&avdev->work_struct_maskirq);
	for (i = 0; i < avdev->dts_param.dts_device_num; i++) {
		avin_disable_irq(avdev->hw_res.irq_num[i]);
		avdev->code_variable.irq_falling_times[i] = 0;
		avdev->code_variable.detect_channel_times[i] = 0;
		avdev->code_variable.loop_detect_times[i] = 0;
	}
	pr_info("avin_detect_suspend ok.\n");
	return 0;
}

static int avin_detect_resume(struct platform_device *pdev)
{
	int i;
	struct avin_det_s *avdev = platform_get_drvdata(pdev);

	for (i = 0; i < avdev->dts_param.dts_device_num; i++)
		avin_enable_irq(avdev->hw_res.irq_num[i]);
	init_resource(avdev);
	pr_info("avin_detect_resume ok.\n");
	return 0;
}

int avin_detect_remove(struct platform_device *pdev)
{
	struct avin_det_s *avdev = platform_get_drvdata(pdev);

	input_unregister_device(avdev->input_dev);
	input_free_device(avdev->input_dev);
	cdev_del(&avdev->avin_cdev);
	del_timer_sync(&avdev->timer);
	cancel_work_sync(&avdev->work_struct_update);
	cancel_work_sync(&avdev->work_struct_maskirq);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id avin_dt_match[] = {
	{	.compatible = "amlogic, avin_detect",
	},
	{},
};
#else
#define avin_dt_match NULL
#endif

static struct platform_driver avin_driver = {
	.probe      = avin_detect_probe,
	.remove     = avin_detect_remove,
	.suspend    = avin_detect_suspend,
	.resume     = avin_detect_resume,
	.driver     = {
		.name   = "avin_detect",
		.of_match_table = avin_dt_match,
	},
};

module_platform_driver(avin_driver);

MODULE_DESCRIPTION("Meson AVIN Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
