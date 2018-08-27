/*
 * drivers/amlogic/irblaster/meson-irblaster.c
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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "meson-irblaster.h"
#include <linux/amlogic/gpio-amlogic.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>

#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>


#define DEVICE_NAME "meson-irblaster"
#define DEIVE_COUNT 32

#define PS_SIZE 10

static dev_t am_irblaster_id;
static struct class *ir_blaster_class;
static struct device *ir_blaster_dev;
static struct cdev am_irblaster_device;
static struct blaster_window *irblaster_win;
static DEFINE_MUTEX(ir_blaster_file_mutex);
static int debug_enable;

struct irtx_dev {
	struct device *dev;
	struct task_struct	*thread;
#ifdef CONFIG_IRBLASTER_ENABLE_PIN
	int enable_pin;
#endif
};


#define IR_TX_EVENT_SIZE 4
#define IR_TX_BUFFER_SIZE 1024

struct tx_event {
	struct list_head list;
	int size;
	int buffer[IR_TX_BUFFER_SIZE];
};

DECLARE_KFIFO(irblaster_fifo, struct tx_event *, IR_TX_EVENT_SIZE);
static struct irtx_dev *tx_dev;

#define irblaster_dbg(format, arg...)     \
do {                                        \
	if (debug_enable)        \
		pr_info(format, ##arg);         \
} while (0)

static struct tx_event *irblaster_event_get(void)
{
	struct tx_event *ev = NULL;

	ev = devm_kzalloc(tx_dev->dev,
			sizeof(struct tx_event), GFP_KERNEL);
	irblaster_dbg("irblaster_event_get ev=0x%p\n", ev);
	return ev;
}

static void irblaster_event_put(struct tx_event *ev)
{
	irblaster_dbg("event_put ev=0x%p\n", ev);
	devm_kfree(tx_dev->dev, ev);
}

static int irblaster_send_bit(unsigned int hightime, unsigned int lowtime,
	unsigned int cycle)
{
	unsigned int count_delay;
	uint32_t val;
	int n = 0;
	int tb[3] = {
		1, 10, 100
	};
	/*
	 *MODULATOR_TB:
	 *	00:	system clock clk
	 *	01:	mpeg_xtal3_tick
	 *	10:	mpeg_1uS_tick
	 *	11:	mpeg_10uS_tick
	 * lowtime<1024,n=0,timebase=1us
	 * 1024<=lowtime<10240,n=1,timebase=10us
	 * AO_IR_BLASTER_ADDR2
	 * bit12: output level(or modulation enable/disable:1=enable)
	 * bit[11:10]: Timebase :
	 *			00=1us
	 *			01=10us
	 *			10=100us
	 *			11=Modulator clock
	 * bit[9:0]: Count of timebase units to delay
	 */
	count_delay = (((hightime + cycle/2) / cycle) - 1) & 0x3ff;
	val = (0x10000 | (1 << 12)) | (3 << 10) | (count_delay << 0);
	writel(val, irblaster_win->reg_base + AO_IR_BLASTER_ADDR2);
	/*
	 * lowtime<1024,n=0,timebase=1us
	 * 1024<=lowtime<10240,n=1,timebase=10us
	 * 10240<=lowtime,n=2,timebase=100us
	 */
	n = lowtime >> 10;
	if (n > 0 && n < 10)
		n = 1;
	else if (n >= 10)
		n = 2;
	lowtime = (lowtime + (tb[n] >> 1))/tb[n];
	count_delay = (lowtime-1) & 0x3ff;
	val = (0x10000 | (0 << 12)) |
		(n << 10) | (count_delay << 0);
	writel(val, irblaster_win->reg_base + AO_IR_BLASTER_ADDR2);
	return 0;
}

static void irblaster_send_all_frame(struct blaster_window *cw)
{
	int i, k;
	int exp = 0x00;
	unsigned int *pData;
	unsigned int consumerir_cycle;
	unsigned int high_ct, low_ct;
	unsigned long cnt;

	irblaster_dbg("cw->winnum = %d\n", cw->winnum);
	irblaster_dbg("cw->winarray = ");
	for (i = 0; i < cw->winnum; i++) {
		irblaster_dbg("%d,", cw->winarray[i]);
		if (i % 10 == 9)
			irblaster_dbg("\n");
	}
	irblaster_dbg("\n");
	consumerir_cycle = 1000 / (irblaster_win->consumerir_freqs / 1000);

	/*reset*/
	writel(readl(irblaster_win->reset_base +
		AO_RTI_GEN_CTNL_REG0) | (1 << 23),
		irblaster_win->reset_base + AO_RTI_GEN_CTNL_REG0);
	udelay(2);
	writel(readl(irblaster_win->reset_base +
		AO_RTI_GEN_CTNL_REG0) & ~(1 << 23),
		irblaster_win->reset_base + AO_RTI_GEN_CTNL_REG0);
	/*
	 *1.disable ir blaster
	 *2.set the modulator_tb = 2'10; mpeg_1uS_tick 1us
	 */
	writel((1 << 2) | (2 << 12) | (1<<2),
		irblaster_win->reg_base + AO_IR_BLASTER_ADDR0);
	/*
	 * 1. set mod_high_count = 13
	 * 2. set mod_low_count = 13
	 * 3. 60khz 8, 38k-13us, 12
	 */
	high_ct = consumerir_cycle * irblaster_win->consumerir_dutycycle/100;
	low_ct = consumerir_cycle - high_ct;
	writel(((high_ct - 1) << 16) | ((low_ct - 1) << 0),
		irblaster_win->reg_base + AO_IR_BLASTER_ADDR1);

	/* Setting this bit to 1 initializes the output to be high.*/
	writel(readl(irblaster_win->reg_base + AO_IR_BLASTER_ADDR0) & ~(1 << 2),
		irblaster_win->reg_base + AO_IR_BLASTER_ADDR0);
	/*enable irblaster_win*/
	writel(readl(irblaster_win->reg_base + AO_IR_BLASTER_ADDR0) | (1 << 0),
		irblaster_win->reg_base + AO_IR_BLASTER_ADDR0);
	k = cw->winnum;
#define SEND_BIT_NUM 64
	exp = cw->winnum / SEND_BIT_NUM;
	pData = cw->winarray;

	while (exp) {
		for (i = 0; i < SEND_BIT_NUM/2; i++) {
			irblaster_send_bit(*pData, *(pData+1),
				consumerir_cycle);
			pData += 2;
		}
		cnt = jiffies + msecs_to_jiffies(1000);
		while (!(readl(irblaster_win->reg_base + AO_IR_BLASTER_ADDR0)
			& (1<<24)) && time_is_after_eq_jiffies(cnt))
			;
		cnt = jiffies + msecs_to_jiffies(1000);
		while ((readl(irblaster_win->reg_base
			+ AO_IR_BLASTER_ADDR0)
			& (1<<26)) && time_is_after_eq_jiffies(cnt))
			;
		/*reset*/
		writel(readl(irblaster_win->reset_base +
			AO_RTI_GEN_CTNL_REG0) | (1 << 23),
			irblaster_win->reset_base + AO_RTI_GEN_CTNL_REG0);
		udelay(2);
		writel(readl(irblaster_win->reset_base +
			AO_RTI_GEN_CTNL_REG0) & ~(1 << 23),
			irblaster_win->reset_base + AO_RTI_GEN_CTNL_REG0);
		exp--;
	}
	exp = (cw->winnum % SEND_BIT_NUM) & (~(1));
	for (i = 0; i < exp; ) {
		irblaster_send_bit(*pData, *(pData+1), consumerir_cycle);
		pData += 2;
		i += 2;
	}

	irblaster_dbg("The all frame finished !!\n");

}


static int irblaster_tx_thread(void *data)
{
#ifdef CONFIG_IRBLASTER_ENABLE_PIN
	struct irtx_dev *dev = (struct irtx_dev *)data;
#endif
	struct tx_event *ev = NULL;
	int retval;
	int i;
	unsigned long cnt;

	while (!kthread_should_stop()) {
		irblaster_dbg("wakeup irblaster_tx_thread\n");
		retval = kfifo_len(&irblaster_fifo);
		if (retval <= 0) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);
			schedule();
			continue;
		}
		retval = kfifo_get(&irblaster_fifo, &ev);
		irblaster_dbg("retval=%d,ev=%p\n", retval, ev);
		if (retval) {
			irblaster_win->winnum = ev->size;
			for (i = 0; i < irblaster_win->winnum; i++)
				irblaster_win->winarray[i] = ev->buffer[i];
			irblaster_dbg("irblaster_send_all_frame.size=%d\n",
				ev->size);
#ifdef CONFIG_IRBLASTER_ENABLE_PIN
			gpio_direction_output(dev->enable_pin, 1);
#endif
			irblaster_send_all_frame(irblaster_win);
			irblaster_event_put(ev);
			cnt = jiffies + msecs_to_jiffies(1000);
			while (!(readl(irblaster_win->reg_base
				+ AO_IR_BLASTER_ADDR0) & (1<<24))
				&& time_is_after_eq_jiffies(cnt))
				;
			cnt = jiffies + msecs_to_jiffies(1000);
			while ((readl(irblaster_win->reg_base
				+ AO_IR_BLASTER_ADDR0) & (1<<26))
				&& time_is_after_eq_jiffies(cnt))
				;
#ifdef CONFIG_IRBLASTER_ENABLE_PIN
			gpio_direction_output(dev->enable_pin, 0);
#endif
		} else
			pr_err("kfifo_get fail\n");
	}

	return 0;
}

 /*
  * Function to set the irblaster Carrier Frequency,
  * The modulator is typically run between 32khz and 56khz.
  *
  * @param[in] pointer to irblaster structure.
  * @param[in] carrirer freqs value.
  * \return Reuturns 0 on success else return the error value.
  */
int set_irblaster_consumerir_freqs(struct blaster_window *irblaster_win,
		int consumerir_freqs)
{
	return	((irblaster_win->consumerir_freqs = consumerir_freqs) >= 32000
		 && (irblaster_win->consumerir_freqs <= 56000)) ? 0 : -1;
}

 /*
  * Function to get the irblaster_win cur Carrier Frequency.
  *
  * @param[in] pointer to irblaster_win structure.
  * return Reuturns freqs.
  */

static int  get_irblaster_consumerir_freqs(struct blaster_window *irblaster_win)
{
	return irblaster_win->consumerir_freqs;
}

static int  set_irblaster_duty_cycle(int duty_cycle)
{
	if (duty_cycle > 100 || duty_cycle < 0)
		return -1;
	irblaster_win->consumerir_dutycycle = duty_cycle;
	return 0;
}

static int aml_ir_blaster_open(struct inode *inode, struct file *file)
{
	irblaster_dbg("aml_ir_blaster_open()\n");
	return 0;
}

int irblaster_send(const char *buf, int len)
{
	/*alloc memory*/
	struct tx_event *ev;
	int i = 0, j = 0, m = 0, ret = 0;
	int val;
	char tone[PS_SIZE];
	struct irtx_dev *irdev = tx_dev;

	ev = irblaster_event_get();
	if (!ev) {
		pr_info("please wait for send\n");
		return -ENOMEM;
	}
	for (i = 0; i < len; i++) {
		if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			pr_info("val is %d", val);
			ev->buffer[m] = val * 10;
			j = 0;
			m++;
			if (m >= IR_TX_BUFFER_SIZE)
				break;
			continue;
		}
		tone[j] = buf[i];
		j++;

		if (j >= PS_SIZE) {
			irblaster_event_put(ev);
			pr_err("send timing value is out of range\n");
			return -ENOMEM;
		}
	}
	ev->size = m;
	/*to send cycle to,*/
	kfifo_put(&irblaster_fifo, (const struct tx_event *)ev);
	/*to wake up irblaster_tx_thread*/
	wake_up_process(irdev->thread);
	return 0;
}

static long aml_ir_blaster_ioctl(struct file *filp, unsigned int cmd,
				unsigned long args)
{

	int consumerir_freqs = 0, duty_cycle = 0;
	s32 r = 0;
	char *sendcode;
	void __user *argp = (void __user *)args;

	sendcode = kzalloc(MAX_PLUSE, GFP_KERNEL);
	if (!sendcode)
		return -ENOMEM;

	irblaster_dbg("aml_ir_blaster_ioctl()  0x%4x\n ", cmd);
	switch (cmd) {
	case CONSUMERIR_TRANSMIT:
		if (copy_from_user(sendcode, (char *)argp,
					strlen((char *)argp))) {
			kfree(sendcode);
			return -EFAULT;
		}
		pr_info("send code is %s\n", sendcode);
		r = irblaster_send(sendcode, strlen(argp));
		break;
	case GET_CARRIER:
		pr_info("in get freq\n");
		consumerir_freqs =
				get_irblaster_consumerir_freqs(irblaster_win);
		put_user(consumerir_freqs, (int *)argp);
		kfree(sendcode);
		return consumerir_freqs;
	case SET_CARRIER:
		pr_info("in set freq\n");
		get_user(consumerir_freqs, (int *)argp);
		r = set_irblaster_consumerir_freqs(irblaster_win,
				consumerir_freqs);
		break;
	case SET_DUTYCYCLE:
		pr_info("in set duty_cycle\n");
		if (copy_from_user(&duty_cycle, argp, sizeof(int))) {
			kfree(sendcode);
			return -EFAULT;
		}
		get_user(duty_cycle, (int *)argp);
		r = set_irblaster_duty_cycle(duty_cycle);
		break;

	default:
		r = -ENOIOCTLCMD;
		break;
	}

	kfree(sendcode);
	return r;
}
static int aml_ir_blaster_release(struct inode *inode, struct file *file)
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

static ssize_t store_carrier_freq(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	mutex_lock(&irblaster_win->lock);
	ret = kstrtoint(buf, 10, &irblaster_win->consumerir_freqs);
	if (ret) {
		pr_err("IR_OUT: Invalid input for carrier_freq\n");
		return ret;
	}
	irblaster_dbg("carrier_freq is %d\n", irblaster_win->consumerir_freqs);
	mutex_unlock(&irblaster_win->lock);
	return strlen(buf);
}

static ssize_t store_duty_cycle(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	mutex_lock(&irblaster_win->lock);
	ret = kstrtoint(buf, 10, &irblaster_win->consumerir_dutycycle);
	if (ret) {
		pr_err("IR_OUT: Invalid input for carrier_freq\n");
		return ret;
	}
	irblaster_dbg("duty_cycle is %d\n",
		irblaster_win->consumerir_dutycycle);
	mutex_unlock(&irblaster_win->lock);
	return strlen(buf);
}

static ssize_t show_log(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memset(buf, 0, PAGE_SIZE);
	return strlen(buf);
}

static ssize_t show_send_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memset(buf, 0, PAGE_SIZE);
	return strlen(buf);
}

static ssize_t store_send(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/*alloc memory*/
	struct tx_event *ev;
	int i = 0, j = 0, m = 0, ret = 0;
	int val;
	char tone[PS_SIZE];
	struct irtx_dev *irdev = dev_get_drvdata(dev);

	ev = irblaster_event_get();
	if (!ev) {
		pr_err("please wait for send\n");
		return -ENOMEM;
	}
	while (buf[i] != '\0') {
		if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			ev->buffer[m] = val * 10;
			j = 0;
			i++;
			m++;
			if (m >= IR_TX_BUFFER_SIZE)
				break;
			continue;
		}
		tone[j] = buf[i];
		i++;
		j++;
		if (j >= PS_SIZE) {
			pr_err("send timing value is out of range\n");
			return -ENOMEM;
		}
	}
	ev->size = m;
	/*to send cycle to,*/
	kfifo_put(&irblaster_fifo, (const struct tx_event *)ev);
	/*to wake up irblaster_tx_thread*/
	wake_up_process(irdev->thread);
	return count;
}

static DEVICE_ATTR(debug, 0644, show_debug, store_debug);
static DEVICE_ATTR(log, 0444, show_log, NULL);
static DEVICE_ATTR(sendvalue, 0444, show_send_value, NULL);
static DEVICE_ATTR(send, 0200, NULL, store_send);
static DEVICE_ATTR(carrier_freq, 0200, NULL, store_carrier_freq);
static DEVICE_ATTR(duty_cycle, 0200, NULL, store_duty_cycle);

static const struct file_operations aml_irblaster_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_ir_blaster_open,
	.compat_ioctl = aml_ir_blaster_ioctl,
	.unlocked_ioctl = aml_ir_blaster_ioctl,
	.release	= aml_ir_blaster_release,
};

static int  aml_ir_blaster_probe(struct platform_device *pdev)
{
	int r;
	struct irtx_dev *dev = NULL;
	struct pinctrl *p = NULL;
	struct resource *reg_mem = NULL;
	struct resource *reset_mem = NULL;
	void __iomem *reg_base = NULL;
	void __iomem *reset_base = NULL;

	pr_info("irblaster probe\n");
	dev = devm_kzalloc(&pdev->dev,
			sizeof(struct irtx_dev), GFP_KERNEL);
	if (!dev) {
		pr_info("faid to kzalloc  irtx_dev");
		return -ENOMEM;
	}

	irblaster_win = devm_kzalloc(&pdev->dev,
			sizeof(struct blaster_window), GFP_KERNEL);
	if (irblaster_win == NULL)
		return -1;

	irblaster_win->consumerir_freqs = 38000;
	irblaster_win->consumerir_dutycycle = 50;

	if (!pdev->dev.of_node) {
		pr_info("aml_irblaster: pdev->dev.of_node == NULL!\n");
		return -1;
	}

	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!IS_ERR_OR_NULL(reg_mem)) {
		reg_base = devm_ioremap_resource(&pdev->dev, reg_mem);
		if (IS_ERR(reg_base)) {
			dev_err(&pdev->dev, "reg: cannot obtain I/O memory region");
			return PTR_ERR(reg_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error");
		return PTR_ERR(reg_base);

	}
	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!IS_ERR_OR_NULL(reset_mem)) {
		reset_base = devm_ioremap_resource(&pdev->dev,
				reset_mem);
		if (IS_ERR(reset_base)) {
			dev_err(&pdev->dev, "reg: cannot obtain I/O memory region");
			return PTR_ERR(reset_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error");
		return PTR_ERR(reset_mem);
	}

	dev->dev = &pdev->dev;
	irblaster_win->reg_base = reg_base;
	irblaster_win->reset_base = reset_base;

	p = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(p)) {
		dev_err(&pdev->dev, "pinctrl error, %ld\n", PTR_ERR(p));
		return -1;
	}

	r = alloc_chrdev_region(&am_irblaster_id, 0, DEIVE_COUNT, DEVICE_NAME);
	if (r < 0) {
		pr_err("Can't register major for ir irblaster device\n");
		return r;
	}
	cdev_init(&am_irblaster_device, &aml_irblaster_fops);
	am_irblaster_device.owner = THIS_MODULE;
	cdev_add(&(am_irblaster_device), am_irblaster_id, DEIVE_COUNT);
	ir_blaster_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(ir_blaster_class)) {
		unregister_chrdev_region(am_irblaster_id, DEIVE_COUNT);
		pr_info("Can't create class for ir irblaster device\n");
		return -1;
	}
	ir_blaster_dev = device_create(ir_blaster_class, NULL,
					am_irblaster_id, dev,
				      "irblaster%d", 1);
	if (ir_blaster_dev == NULL) {
		pr_err("ir_blaster_dev create error\n");
		class_destroy(ir_blaster_class);
		return -EEXIST;
	}

	mutex_init(&irblaster_win->lock);
	device_create_file(ir_blaster_dev, &dev_attr_debug);
	device_create_file(ir_blaster_dev, &dev_attr_log);
	device_create_file(ir_blaster_dev, &dev_attr_sendvalue);
	device_create_file(ir_blaster_dev, &dev_attr_send);
	device_create_file(ir_blaster_dev, &dev_attr_duty_cycle);
	device_create_file(ir_blaster_dev, &dev_attr_carrier_freq);
	INIT_KFIFO(irblaster_fifo);
	dev->thread = kthread_run(irblaster_tx_thread, dev,
		 "ir-blaster-thread");

#ifdef CONFIG_IRBLASTER_ENABLE_PIN
	dev->enable_pin = of_get_named_gpio(pdev->dev.of_node, "enable_pin", 0);
	gpio_request(dev->enable_pin, "ir enable pin");
	gpio_direction_output(dev->enable_pin, 0);
#endif
	tx_dev = dev;
	return 0;
}

static int aml_ir_blaster_remove(struct platform_device *pdev)
{
	pr_info("remove IRBLASTER\n");
	device_remove_file(ir_blaster_dev, &dev_attr_debug);
	device_remove_file(ir_blaster_dev, &dev_attr_log);
	device_remove_file(ir_blaster_dev, &dev_attr_sendvalue);
	device_remove_file(ir_blaster_dev, &dev_attr_send);
	device_remove_file(ir_blaster_dev, &dev_attr_carrier_freq);
	device_remove_file(ir_blaster_dev, &dev_attr_duty_cycle);
	kfree(irblaster_win);
	cdev_del(&am_irblaster_device);
	device_destroy(ir_blaster_class, am_irblaster_id);
	class_destroy(ir_blaster_class);
	unregister_chrdev_region(am_irblaster_id, DEIVE_COUNT);
	return 0;
}
static const struct of_device_id ir_blaster_dt_match[] = {
	{
		.compatible	= "amlogic, meson_irblaster",
	},
	{},
};
static struct platform_driver aml_ir_blaster_driver = {
	.probe		= aml_ir_blaster_probe,
	.remove		= aml_ir_blaster_remove,
	.suspend	= NULL,
	.resume		= NULL,
	.driver = {
		.name = "meson-ir_blaster",
		.owner  = THIS_MODULE,
		.of_match_table = ir_blaster_dt_match,
	},
};

static int __init aml_ir_blaster_init(void)
{
	pr_info("BLASTER Driver Init\n");
	if (platform_driver_register(&aml_ir_blaster_driver)) {
		irblaster_dbg("failed to register aml_ir_irblaster_driver module\n");
		return -ENODEV;
	}
	pr_info("BLASTER Driver End\n");
	return 0;
}

static void __exit aml_ir_blaster_exit(void)
{
	pr_info("IRBLASTER Driver exit\n");
	platform_driver_unregister(&aml_ir_blaster_driver);
}
module_init(aml_ir_blaster_init);
module_exit(aml_ir_blaster_exit);

MODULE_AUTHOR("platform-beijing");
MODULE_DESCRIPTION("Irblaster Driver");
MODULE_LICENSE("GPL");
