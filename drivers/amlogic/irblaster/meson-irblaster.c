/*
 * drivers/amlogic/irblaster/irblaster.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include "meson-irblaster.h"

#undef pr_fmt
#define pr_fmt(fmt) "irblaster: " fmt
#define BLASTER_NAME				"meson-irblaster"

static int debug_enable;
static DEFINE_MUTEX(blaster_mutex);

#define irblaster_dbg(format, arg...)    \
do {                                     \
	if (debug_enable)	\
		pr_info(format, ##arg);  \
} while (0)

static int write_to_fifo(struct aml_irblaster_dev *dev,
		unsigned int hightime, unsigned int lowtime)
{
	unsigned int count_delay;
	unsigned int cycle = 1000 / (dev->carrier_freqs / 1000);
	uint32_t val;
	int n = 0;
	int tb[3] = {
		1, 10, 100
	};
	/*
	hightime: modulator signal.
	MODULATOR_TB:
		00:	system clock
		01:	mpeg_xtal3_tick
		10:	mpeg_1uS_tick
		11:	mpeg_10uS_tick

	AO_IR_BLASTER_ADDR2
	bit12: output level(or modulation enable/disable:1=enable)
	bit[11:10]: Timebase :
				00=1us
				01=10us
				10=100us
				11=Modulator clock
	bit[9:0]: Count of timebase units to delay
	*/

	count_delay = (((hightime + cycle/2) / cycle) - 1) & COUNT_DELAY_MASK;
	val = (BLASTER_WRITE_FIFO | BLASTER_MODULATION_ENABLE |
		BLASTER_TIMEBASE_MODULATION_CLOCK | (count_delay << 0));
	writel(val, dev->reg_base + AO_IR_BLASTER_ADDR2);

	/*
	lowtime<1024,n=0,timebase=1us
	1024<=lowtime<10240,n=1,timebase=10us
	10240<=lowtime,n=2,timebase=100us
	*/
	n = lowtime >> 10;
	if (n > 0 && n < 10)
		n = 1;
	else if (n >= 10)
		n = 2;
	lowtime = (lowtime + (tb[n] >> 1))/tb[n];
	count_delay = (lowtime-1) & COUNT_DELAY_MASK;
	val = (BLASTER_WRITE_FIFO & (~BLASTER_MODULATION_ENABLE)) |
		(n << TIMEBASE_SHIFT) | (count_delay << 0);
	writel(val, dev->reg_base + AO_IR_BLASTER_ADDR2);

	return 0;
}

static void send_all_data(struct aml_irblaster_dev *dev)
{
	int i;
	unsigned int *pdata = NULL;

	irblaster_dbg("dev->count = %d\n", dev->count);
	pdata = &dev->buffer[dev->count];
	for (i = 0; (i < 120) &&
			(dev->count < dev->buffer_size);) {
		write_to_fifo(dev, *pdata, *(pdata + 1));
		pdata += 2;
		dev->count += 2;
		i += 2;
	}
	if (dev->count >= dev->buffer_size) {
		irblaster_dbg("The all datas finished!\n");
		//complete(&dev->blaster_completion);
	}
}

static irqreturn_t meson_blaster_interrupt(int irq, void *dev_id)
{
	struct aml_irblaster_dev *dev = dev_id;

	irblaster_dbg("meson_blaster_interrupt !!\n");
	complete(&dev->blaster_completion);
	/*clear pending bit*/
	writel(readl(dev->reg_base + AO_IR_BLASTER_ADDR3) &
			(~BLASTER_FIFO_THD_PENDING),
				dev->reg_base + AO_IR_BLASTER_ADDR3);
	schedule_work(&dev->blaster_work);
	return IRQ_HANDLED;
}

int set_carrier_freqs(struct aml_irblaster_dev *dev, int carrier_freqs)
{
	if (carrier_freqs > MAX_FREQ || carrier_freqs < LIMIT_FREQ) {
		pr_info("freqs[%d,%d]\n", LIMIT_FREQ, MAX_FREQ);
		return -1;
	}
	dev->carrier_freqs = carrier_freqs;
	return 0;
}

static int  get_carrier_freqs(struct aml_irblaster_dev *dev)
{
	return dev->carrier_freqs;
}

static int  set_duty_cycle(struct aml_irblaster_dev *dev, int duty_cycle)
{
	if (duty_cycle > MAX_DUTY || duty_cycle < LIMIT_DUTY) {
		pr_info("duty_cycle[%d,%d]\n", LIMIT_DUTY, MAX_DUTY);
		return -1;
	}
	dev->duty_cycle = duty_cycle;
	return 0;
}

static int aml_irblaster_open(struct inode *inode, struct file *file)
{
	struct aml_irblaster_dev *dev = NULL;

	dev = container_of(inode->i_cdev,
				struct aml_irblaster_dev, blaster_cdev);
	file->private_data = dev;
	return 0;
}

static int send(struct aml_irblaster_dev *dev, const char *buf, int len)
{
	int i = 0, j = 0, m = 0, ret = 0;
	int val;
	char tone[PS_SIZE];
	unsigned int sum_time = 0;

	for (i = 0; i < len; i++) {
		if (buf[i] == '\0') {
			break;
		} else if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			if (ret) {
				pr_err("Invalid tone\n");
				return ret;
			}
			dev->buffer[m] = val*10;
			sum_time = sum_time + val*10;
			j = 0;
			m++;
			if (m >= MAX_PLUSE)
				break;
			continue;
		}
		tone[j] = buf[i];
		j++;
		if (j >= PS_SIZE) {
			pr_err("send timing value is out of range\n");
			return -ENOMEM;
		}
	}
	dev->buffer_size = m;
	dev->count = 0;
	send_all_data(dev);
	ret = wait_for_completion_interruptible_timeout(
		&dev->blaster_completion, msecs_to_jiffies(sum_time / 1000));
	if (ret)
		pr_err("failed to send all data\n");
	return ret;
}

static long aml_irblaster_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{
	int carrier_freqs = 0, duty_cycle = 0;
	static int psize;
	s32 r = 0;
	void __user *argp = (void __user *)args;
	char *sendcode = NULL;
	struct aml_irblaster_dev *dev = filp->private_data;

	irblaster_dbg("aml_irblaster_ioctl()  0x%4x\n ", cmd);
	switch (cmd) {
	case CONSUMERIR_TRANSMIT:
		psize = psize ? psize:MAX_PLUSE;
		sendcode = kcalloc(psize, sizeof(char), GFP_KERNEL);
		if (sendcode == NULL) {
			pr_err("can't get sendcode memory\n");
			return -ENOMEM;
		}
		if (copy_from_user(sendcode, (char *)argp,
					psize))
			return -EFAULT;
		pr_info("send code is %s\n", sendcode);
		mutex_lock(&blaster_mutex);
		r = send(dev, sendcode, psize);
		mutex_unlock(&blaster_mutex);
		kfree(sendcode);
		break;
	case GET_CARRIER:
		pr_info("in get freq\n");
		carrier_freqs = get_carrier_freqs(dev);
		put_user(carrier_freqs, (int *)argp);
		return carrier_freqs;
	case SET_CARRIER:
		pr_info("in set freq\n");
		if (get_user(carrier_freqs, (int *)argp) < 0)
			return -EFAULT;
		mutex_lock(&blaster_mutex);
		r = set_carrier_freqs(dev, carrier_freqs);
		mutex_unlock(&blaster_mutex);
		break;
	case SET_DUTYCYCLE:
		pr_info("in set duty_cycle\n");
		if (copy_from_user(&duty_cycle, argp, sizeof(int)))
			return -EFAULT;
		get_user(duty_cycle, (int *)argp);
		mutex_lock(&blaster_mutex);
		r = set_duty_cycle(dev, duty_cycle);
		mutex_unlock(&blaster_mutex);
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	return r;
}
static int aml_irblaster_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t show_debug(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (debug_enable)
		sprintf(buf, "debug=enable\n");
	else
		sprintf(buf, "debug=disable\n");
	return strlen(buf);
}

static ssize_t store_debug(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, "enable", 1)) {
		debug_enable = 1;
		pr_info("enable debug\n");
	} else if (!strncmp(buf, "disable", 1)) {
		debug_enable = 0;
		pr_info("disable debug\n");
	}
	return strlen(buf);
}

static ssize_t show_carrier_freq(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aml_irblaster_dev *irblaster_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", irblaster_dev->carrier_freqs);
}

static ssize_t store_carrier_freq(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, val;
	struct aml_irblaster_dev *irblaster_dev = dev_get_drvdata(dev);

	mutex_lock(&blaster_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input for carrier_freq\n");
		mutex_unlock(&blaster_mutex);
		return ret;
	}
	ret = set_carrier_freqs(irblaster_dev, val);
	mutex_unlock(&blaster_mutex);
	if (ret)
		return ret;
	pr_info("carrier_freq is %d\n", irblaster_dev->carrier_freqs);
	return count;
}

static ssize_t show_duty_cycle(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aml_irblaster_dev *irblaster_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", irblaster_dev->duty_cycle);
}

static ssize_t store_duty_cycle(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, val;
	struct aml_irblaster_dev *irblaster_dev = dev_get_drvdata(dev);

	mutex_lock(&blaster_mutex);
	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input for duty_cycle\n");
		mutex_unlock(&blaster_mutex);
		return ret;
	}
	ret = set_duty_cycle(irblaster_dev, val);
	mutex_unlock(&blaster_mutex);
	if (ret)
		return ret;
	pr_info("duty_cycle is %d\n", irblaster_dev->duty_cycle);
	return count;
}

static ssize_t store_send(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aml_irblaster_dev *irblaster_dev = dev_get_drvdata(dev);
	int i = 0, j = 0, m = 0, ret = 0;
	int val;
	char tone[PS_SIZE];
	unsigned int sum_time = 0;

	mutex_lock(&blaster_mutex);
	while (buf[i] != '\0') {
		if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			if (ret) {
				pr_err("Invalid tone\n");
				mutex_unlock(&blaster_mutex);
				return ret;
			}
			irblaster_dev->buffer[m] = val*10;
			sum_time = sum_time + val*10;
			j = 0;
			i++;
			m++;
			if (m >= MAX_PLUSE)
				break;
			continue;
		}
		tone[j] = buf[i];
		i++;
		j++;
		if (j >= PS_SIZE) {
			pr_err("send timing value is out of range\n");
			mutex_unlock(&blaster_mutex);
			return -ENOMEM;
		}
	}

	irblaster_dev->buffer_size = m;
	irblaster_dbg("irblaster_dev->buffer_size = %d\n",
					irblaster_dev->buffer_size);
	irblaster_dbg("irblaster_dev->buffer =\n ");
	for (i = 0; i < irblaster_dev->buffer_size; i++)
		irblaster_dbg("%d\n", irblaster_dev->buffer[i]);
	irblaster_dbg("sum_time = %d\n", sum_time);
	irblaster_dev->count = 0;
	init_completion(&irblaster_dev->blaster_completion);
	send_all_data(irblaster_dev);
	ret = wait_for_completion_interruptible_timeout(
			&irblaster_dev->blaster_completion,
			msecs_to_jiffies(sum_time / 1000));
	if (ret)
		pr_err("failed to send all data\n");
	mutex_unlock(&blaster_mutex);
	return count;
}

static DEVICE_ATTR(debug, 0644, show_debug, store_debug);
static DEVICE_ATTR(send, 0200, NULL, store_send);
static DEVICE_ATTR(carrier_freq, 0644,
				show_carrier_freq, store_carrier_freq);
static DEVICE_ATTR(duty_cycle, 0644,
				show_duty_cycle, store_duty_cycle);

static struct attribute *irblaster_attributes[] = {
	&dev_attr_debug.attr,
	&dev_attr_send.attr,
	&dev_attr_carrier_freq.attr,
	&dev_attr_duty_cycle.attr,
	NULL,
};

static struct attribute_group irblaster_attribute_group = {
	.attrs      = irblaster_attributes,
};

const struct attribute_group *irblaster_attribute_groups[] = {
	&irblaster_attribute_group,
	NULL,
};

static const struct file_operations blaster_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_irblaster_open,
	.compat_ioctl = aml_irblaster_ioctl,
	.unlocked_ioctl = aml_irblaster_ioctl,
	.release	= aml_irblaster_release,
};

static void do_blaster_send(struct work_struct *work)
{
	struct aml_irblaster_dev *dev = container_of(work,
			struct aml_irblaster_dev, blaster_work);

	if (dev->count < dev->buffer_size)
		send_all_data(dev);
}

static void blaster_initialize(struct aml_irblaster_dev *dev)
{
	unsigned int carrier_cycle = 1000 / (dev->carrier_freqs / 1000);
	unsigned int high_ct, low_ct;

	/*
	 *1. disable ir blaster
	 *2. set the modulator_tb = 2'10; mpeg_1uS_tick 1us
	 *3. set initializes the output to be high
	 */
	writel((~BLASTER_ENABLE) & (BLASTER_MODULATOR_TB_1US_TICK |
		BLASTER_INIT_HIGH), dev->reg_base + AO_IR_BLASTER_ADDR0);
	/*
	 *1. set mod_high_count = 13
	 *2. set mod_low_count = 13
	 *3. 60khz-8us, 38k-13us
	 */
	high_ct = carrier_cycle * dev->duty_cycle / 100;
	low_ct = carrier_cycle - high_ct;
	writel((BLASTER_MODULATION_LOW_COUNT(low_ct - 1) |
		BLASTER_MODULATION_HIGH_COUNT(high_ct - 1)),
			dev->reg_base + AO_IR_BLASTER_ADDR1);
	/*mask initialize output to be high*/
	writel(readl(dev->reg_base + AO_IR_BLASTER_ADDR0) &
			~BLASTER_INIT_HIGH,
			dev->reg_base + AO_IR_BLASTER_ADDR0);
	/*
	 *1. set fifo irq enable
	 *2. set fifo irq threshold
	 */
	writel(BLASTER_FIFO_IRQ_ENABLE |
		BLASTER_FIFO_IRQ_THRESHOLD(8),
		dev->reg_base + AO_IR_BLASTER_ADDR3);
	/*enable irblaster*/
	writel(readl(dev->reg_base + AO_IR_BLASTER_ADDR0) |
		BLASTER_ENABLE,
		dev->reg_base + AO_IR_BLASTER_ADDR0);
}

static int  aml_irblaster_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct aml_irblaster_dev *irblaster_dev = NULL;
	struct pinctrl *irblaster_pinctrl = NULL;
	struct resource *reg_mem = NULL;
	struct resource *res_irq = NULL;
	struct resource *reset_mem = NULL;
	void __iomem *reg_base = NULL;
	void __iomem *reset_base = NULL;

	pr_info("probe\n");
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "pdev->dev.of_node == NULL!\n");
		return -1;
	}

	irblaster_pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(irblaster_pinctrl)) {
		dev_err(&pdev->dev, "pinctrl error.\n");
		return PTR_ERR(irblaster_pinctrl);
	}

	irblaster_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct aml_irblaster_dev), GFP_KERNEL);
	if (!irblaster_dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, irblaster_dev);
	irblaster_dev->dev = &pdev->dev;

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq)
		return -ENODEV;
	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!IS_ERR_OR_NULL(reg_mem)) {
		reg_base = devm_ioremap_resource(&pdev->dev, reg_mem);
		if (IS_ERR(reg_base)) {
			dev_err(&pdev->dev, "reg0: cannot obtain I/O memory region.\n");
			return PTR_ERR(reg_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error.\n");
		return PTR_ERR(reg_base);
	}
	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!IS_ERR_OR_NULL(reset_mem)) {
		reset_base = devm_ioremap_resource(&pdev->dev,
					reset_mem);
		if (IS_ERR(reset_base)) {
			dev_err(&pdev->dev, "reg1: cannot obtain I/O memory region.\n");
			return PTR_ERR(reset_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error.\n");
		return PTR_ERR(reset_mem);
	}

	irblaster_dev->carrier_freqs = DEFAULT_CARRIER_FREQ;
	irblaster_dev->duty_cycle = DEFAULT_DUTY_CYCLE;
	irblaster_dev->reg_base = reg_base;
	irblaster_dev->reset_base = reset_base;
	irblaster_dev->irq = res_irq->start;

	ret = alloc_chrdev_region(&irblaster_dev->blaster_cdev.dev, 0,
				BLASTER_DEVICE_COUNT,
				BLASTER_NAME);
	if (ret < 0) {
		pr_err("Failed to alloc cdev  major  device\n");
		return ret;
	}
	cdev_init(&irblaster_dev->blaster_cdev, &blaster_fops);
	irblaster_dev->blaster_cdev.owner = THIS_MODULE;
	ret = cdev_add(&irblaster_dev->blaster_cdev,
			irblaster_dev->blaster_cdev.dev,
			BLASTER_DEVICE_COUNT);
	if (ret)
		goto err0;
	irblaster_dev->blaster_class = class_create(THIS_MODULE,
						BLASTER_NAME);
	if (IS_ERR(irblaster_dev->blaster_class)) {
		pr_err("Failed to class create\n");
		ret = PTR_ERR(irblaster_dev->blaster_class);
		goto err1;
	}

	irblaster_dev->dev = device_create_with_groups(
						irblaster_dev->blaster_class,
						NULL,
						irblaster_dev->blaster_cdev.dev,
						irblaster_dev,
						irblaster_attribute_groups,
						"irblaster%d", 1);
	if (IS_ERR(irblaster_dev->dev)) {
		pr_err("Failed to device create\n");
		ret = PTR_ERR(irblaster_dev->dev);
		goto err2;
	}
	init_completion(&irblaster_dev->blaster_completion);
	INIT_WORK(&irblaster_dev->blaster_work, do_blaster_send);
	ret = devm_request_irq(&pdev->dev, irblaster_dev->irq,
				meson_blaster_interrupt,
				IRQF_TRIGGER_RISING,
				dev_name(&pdev->dev),
				irblaster_dev);
	if (ret) {
		pr_err("Failed to request irq number\n");
		goto err2;
	}
	/*initial blaster*/
	blaster_initialize(irblaster_dev);

	return 0;

err2:
	class_destroy(irblaster_dev->blaster_class);
err1:
	cdev_del(&irblaster_dev->blaster_cdev);
err0:
	unregister_chrdev_region(irblaster_dev->blaster_cdev.dev,
				BLASTER_DEVICE_COUNT);

	return ret;
}

static int aml_irblaster_remove(struct platform_device *pdev)
{
	struct aml_irblaster_dev *irblaster_dev = platform_get_drvdata(pdev);

	device_destroy(irblaster_dev->blaster_class,
			irblaster_dev->blaster_cdev.dev);
	class_destroy(irblaster_dev->blaster_class);
	cdev_del(&irblaster_dev->blaster_cdev);
	unregister_chrdev_region(irblaster_dev->blaster_cdev.dev,
				BLASTER_DEVICE_COUNT);
	return 0;
}

static const struct of_device_id irblaster_dt_match[] = {
	{
		.compatible	= "amlogic, meson_irblaster",
	},
	{},
};
MODULE_DEVICE_TABLE(of, irblaster_dt_match);

static struct platform_driver aml_irblaster_driver = {
	.probe		= aml_irblaster_probe,
	.remove		= aml_irblaster_remove,
	.driver = {
		.name = "meson_irblaster",
		.owner  = THIS_MODULE,
		.of_match_table = irblaster_dt_match,
	},
};

static int __init aml_irblaster_init(void)
{
	return platform_driver_register(&aml_irblaster_driver);
}

static void __exit aml_irblaster_exit(void)
{
	platform_driver_unregister(&aml_irblaster_driver);
}

module_init(aml_irblaster_init);
module_exit(aml_irblaster_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic Meson ir blaster driver");
MODULE_LICENSE("GPL");
