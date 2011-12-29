/*
 * sc16is7xx-gpio.c  --  GPIO Driver for Dialog sc16is7xx
 *
 * Copyright(c) 2009 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd <dchen@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/mfd/sc16is7xx-reg.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

#define DRIVER_NAME "sc16is7xx-gpio"

struct sc16is7xx_gpio {
	struct sc16is7xx_access *access;
	struct device		*dev;
	struct gpio_chip	_gc;
	unsigned		last_state;
	unsigned char		irq_trig_fall;
	unsigned char		irq_trig_rise;
#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
	struct mutex		irq_lock;
	int 			irq_base;
	unsigned char		irq_mask;
#endif
};

static inline struct sc16is7xx_gpio *gpio_from_chip(struct gpio_chip *gc)
{
	return container_of(gc, struct sc16is7xx_gpio, _gc);
}

#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
void sc16is7xx_gpio_irq(struct sc16is7xx_gpio *sg, unsigned state);
#endif

static s32 sc16is7xx_gpio_get(struct gpio_chip *gc, u32 offset)
{
	struct sc16is7xx_gpio *sg = gpio_from_chip(gc);
	s32 ret = sg->access->read(sg->access, SC_IOINTENA);
	if (ret < 0)
		return ret;
	if (ret & (1 << offset)) {
		/* change interrupt is enabled, return last value */
		ret = sg->last_state;
	} else {
		ret = sg->access->read(sg->access, SC_IOSTATE_R);
		if (ret < 0)
			return ret;
#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
		sc16is7xx_gpio_irq(sg, ret);
#endif
	}
	return (ret >> offset) & 1;
}

static s32 sc_gpio_dir_out(struct sc16is7xx_gpio *sg, u32 offset, s32 value)
{
	s32 ret;
	unsigned mask = (1 << offset);
	sg->irq_trig_fall &= ~mask;
	sg->irq_trig_rise &= ~mask;
	ret = sg->access->modify(sg->access, SC_IOSTATE_W, mask, (value & 1) << offset);
	if (ret >= 0)
		ret = sg->access->modify(sg->access, SC_IODIR, 0, mask);
	if (ret >= 0)
		ret = sg->access->modify(sg->access, SC_IOCONTROL, (offset >= 4) ? 2 : 4, 1);
	if (ret < 0)
		return ret;
	return 0;
}

static void sc16is7xx_gpio_set(struct gpio_chip *gc, u32 offset, s32 value)
{
	struct sc16is7xx_gpio *sg = gpio_from_chip(gc);
	sc_gpio_dir_out(sg, offset, value);
}

static s32 sc16is7xx_gpio_dir_in(struct gpio_chip *gc, u32 offset)
{
	s32 ret;
	struct sc16is7xx_gpio *sg = gpio_from_chip(gc);
	ret =sg->access->modify(sg->access, SC_IODIR, 1 << offset, 0);
	if (ret >= 0)
		ret = sg->access->modify(sg->access, SC_IOCONTROL, (offset >= 4) ? 2 : 4, 1);
	if (ret < 0)
		return ret;
	return 0;
}

static s32 sc16is7xx_gpio_dir_out(struct gpio_chip *gc, u32 offset, s32 value)
{
	struct sc16is7xx_gpio *sg = gpio_from_chip(gc);
	return sc_gpio_dir_out(sg, offset, value);
}

#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
static int sc16is7xx_gpio_to_irq(struct gpio_chip *gc, u32 offset)
{
	struct sc16is7xx_gpio *sg = gpio_from_chip(gc);
	if ((sg->irq_base < 0)||(offset >= 8))
		return -EINVAL;
	return sg->irq_base + offset;
}

static void sc16is7xx_irq_mask(unsigned int irq)
{
	struct sc16is7xx_gpio *sg = get_irq_chip_data(irq);
	sg->irq_mask |= 1 << (irq - sg->irq_base);
}

static void sc16is7xx_irq_unmask(unsigned int irq)
{
	struct sc16is7xx_gpio *sg = get_irq_chip_data(irq);
	sg->irq_mask &= ~(1 << (irq - sg->irq_base));
}

static void sc16is7xx_irq_bus_lock(unsigned int irq)
{
	struct sc16is7xx_gpio *sg = get_irq_chip_data(irq);
	mutex_lock(&sg->irq_lock);
}

static void sc16is7xx_irq_bus_sync_unlock(unsigned int irq)
{
	struct sc16is7xx_gpio *sg = get_irq_chip_data(irq);
	unsigned int_mask;

	int_mask = sg->irq_trig_fall | sg->irq_trig_rise;
	sg->access->modify(sg->access, SC_IODIR, int_mask, 0);
	sg->access->write(sg->access, SC_IOINTENA, int_mask);
	if (int_mask) {
		int ret;
		int iostate;
		int mask = 0;
		if (int_mask & 0xf)
			mask |= 4;
		if (int_mask & 0xf0)
			mask |= 2;
		ret = sg->access->modify(sg->access, SC_IOCONTROL, mask, 1);
		iostate = sg->access->read(sg->access, SC_IOSTATE_R);
		if (iostate >= 0)
			sc16is7xx_gpio_irq(sg, iostate);
	}
	mutex_unlock(&sg->irq_lock);
}

static int sc16is7xx_irq_set_type(unsigned int irq, unsigned int type)
{
	struct sc16is7xx_gpio *sg = get_irq_chip_data(irq);
	unsigned gp = irq - sg->irq_base;
	unsigned mask = 1 << gp;

	type &= IRQ_TYPE_EDGE_BOTH;
	if (type && (type != IRQ_TYPE_EDGE_BOTH)) {
		dev_err(sg->dev, "irq %d: unsupported type %d\n", irq, type);
		return -EINVAL;
	}
	if (type & IRQ_TYPE_EDGE_FALLING)
		sg->irq_trig_fall |= mask;
	else
		sg->irq_trig_fall &= ~mask;

	if (type & IRQ_TYPE_EDGE_RISING)
		sg->irq_trig_rise |= mask;
	else
		sg->irq_trig_rise &= ~mask;
	return 0;
}

static struct irq_chip sc16is7xx_irq_chip = {
	.name			= "sc16is7xx",
	.mask			= sc16is7xx_irq_mask,
	.unmask			= sc16is7xx_irq_unmask,
	.bus_lock		= sc16is7xx_irq_bus_lock,
	.bus_sync_unlock	= sc16is7xx_irq_bus_sync_unlock,
	.set_type		= sc16is7xx_irq_set_type,
};

void sc16is7xx_gpio_irq(struct sc16is7xx_gpio *sg, unsigned state)
{
	unsigned change;
	unsigned pending;
	unsigned active;
	unsigned gp;

	change = state ^ sg->last_state;
	sg->last_state = state;
	change &= ~sg->irq_mask;
	active = (state & sg->irq_trig_rise) | (~state & sg->irq_trig_fall);
	pending = change & active;

	while (pending) {
		gp = __ffs(pending);
		if (gp >= 8)
			break;
		handle_nested_irq(gp + sg->irq_base);
		pending &= ~(1 << gp);
	}
}

void __devinit sc_setup_irqs(struct sc16is7xx_gpio *sg, int irq_base)
{
	int gp;
	mutex_init(&sg->irq_lock);
	sg->irq_base = irq_base;
	sg->_gc.to_irq = sc16is7xx_gpio_to_irq;
	sg->access->register_gpio_interrupt(sg->access,
			&sc16is7xx_gpio_irq, sg);

	for (gp = 0; gp < sg->_gc.ngpio; gp++) {
		int irq = gp + sg->irq_base;
		set_irq_chip_data(irq, sg);
		set_irq_chip_and_handler(irq, &sc16is7xx_irq_chip, handle_edge_irq);
		set_irq_nested_thread(irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(irq, IRQF_VALID);
#else
		set_irq_noprobe(irq);
#endif
	}
	pr_info("%s: irq_base=%i\n", __func__, sg->irq_base);
}

void __devexit sc_teardown_irqs(struct sc16is7xx_gpio *sg)
{
	int gp;
	for (gp = 0; gp < sg->_gc.ngpio; gp++) {
		int irq = gp + sg->irq_base;
#ifdef CONFIG_ARM
		set_irq_flags(irq, 0);
#endif
		set_irq_chip_and_handler(irq, NULL, NULL);
		set_irq_chip_data(irq, NULL);
	}
}

#else
void sc_setup_irqs(struct sc16is7xx_gpio *sg, int irq_base) {}
void sc_teardown_irqs(struct sc16is7xx_gpio *sg) {}
#endif

static int __devinit sc16is7xx_gpio_probe(struct platform_device *pdev)
{
	int ret;
	struct sc16is7xx_gpio *sg;
	struct sc16is7xx_gpio_platform_data *pdata = pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;
	sg = kzalloc(sizeof(*sg), GFP_KERNEL);
	if (sg == NULL)
		return -ENOMEM;
	sg->access = dev_get_drvdata(pdev->dev.parent);
	sg->dev			= &pdev->dev;
	sg->_gc.get		= sc16is7xx_gpio_get;
	sg->_gc.set		= sc16is7xx_gpio_set;
	sg->_gc.direction_input	= sc16is7xx_gpio_dir_in;
	sg->_gc.direction_output = sc16is7xx_gpio_dir_out;
	sg->_gc.base		= pdata->gpio_base;
	sg->_gc.ngpio		= 8;
	sg->_gc.can_sleep	= 1;
	sg->_gc.dev		= &pdev->dev;
	sg->_gc.owner		= THIS_MODULE;
	sg->_gc.label		= "sc16is7xx-gpio";
	sg->access->write(sg->access, SC_IODIR, 0);
	sg->access->write(sg->access, SC_IOSTATE_W, 0);
	sg->access->write(sg->access, SC_IOINTENA, 0);
	/* latch changes, modem control until configured */
	sg->access->write(sg->access, SC_IOCONTROL, 7);


	ret = sg->access->read(sg->access, SC_IOSTATE_R);
	if (ret >= 0)
		sg->last_state = ret;

	sc_setup_irqs(sg, pdata->irq_base);
	ret = gpiochip_add(&sg->_gc);
	if (ret >= 0) {
		pr_info("%s: _gc.base=%i\n", __func__, sg->_gc.base);
		platform_set_drvdata(pdev, sg);
		return ret;
	}
	dev_err(&pdev->dev, "Could not register gpiochip, %d\n", ret);
	kfree(sg);
	return ret;
}

static int __devexit sc16is7xx_gpio_remove(struct platform_device *pdev)
{
	struct sc16is7xx_gpio *sg = platform_get_drvdata(pdev);
	int ret = gpiochip_remove(&sg->_gc);
	if (ret) {
		dev_err(&pdev->dev, "%s failed, %d\n",
			"gpiochip_remove()", ret);
		return ret;
	}

	sg->access->register_gpio_interrupt(sg->access, NULL, NULL);
	sc_teardown_irqs(sg);
	kfree(sg);
	return 0;
}

static struct platform_driver sc16is7xx_gpio_driver = {
	.probe		= sc16is7xx_gpio_probe,
	.remove		= __devexit_p(sc16is7xx_gpio_remove),
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init sc16is7xx_gpio_init(void)
{
	return platform_driver_register(&sc16is7xx_gpio_driver);
}

static void __exit sc16is7xx_gpio_exit(void)
{
	return platform_driver_unregister(&sc16is7xx_gpio_driver);
}

module_init(sc16is7xx_gpio_init);
module_exit(sc16is7xx_gpio_exit);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("sc16is7xx GPIO Device Driver");
MODULE_LICENSE("GPL");
