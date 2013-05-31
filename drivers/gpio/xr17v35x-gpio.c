/*
 * xr17v35x-gpio.c  --  GPIO Driver for Exar xr17v35x
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
#include <linux/mfd/xr17v35x-reg.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

#define DRIVER_NAME "xr17v35x-gpio"
struct xr17v35x_gpio {
	const struct xr17v35x_access *access;
	void			*priv;
	struct device		*dev;
	struct gpio_chip	_gc;
	enum mpio_base		base;
	unsigned char		last_state;
	unsigned char		last_fall;
	unsigned char		irq_trig_fall;
	unsigned char		irq_trig_rise;
#ifdef CONFIG_GPIO_XR17V35X_IRQ
	struct mutex		irq_lock;
	int 			irq_base;
	unsigned char		irq_mask;
#endif
};

static inline struct xr17v35x_gpio *gpio_from_chip(struct gpio_chip *gc)
{
	return container_of(gc, struct xr17v35x_gpio, _gc);
}

#ifdef CONFIG_GPIO_XR17V35X_IRQ
void xr17v35x_gpio_update(struct xr17v35x_gpio *xr);
#endif

static s32 xr17v35x_gpio_get(struct gpio_chip *gc, u32 offset)
{
	struct xr17v35x_gpio *xr = gpio_from_chip(gc);
	s32 ret = xr->access->read(xr->priv, xr->base + XR_MPIOINT);
	if (ret < 0)
		return ret;
	if (!(ret & (1 << offset))) {
#ifdef CONFIG_GPIO_XR17V35X_IRQ
		/* change interrupt is NOT enabled */
		xr17v35x_gpio_update(xr);
#else
		ret = xr->access->read(xr->priv, xr->base + XR_MPIOLVL);
		if (ret < 0)
			return ret;
		xr->last_state = ret;
#endif
	}
	return (xr->last_state >> offset) & 1;
}

static s32 xr_gpio_dir_out(struct xr17v35x_gpio *xr, u32 offset, s32 value)
{
	s32 ret;
	unsigned mask = (1 << offset);
	xr->irq_trig_fall &= ~mask;
	xr->irq_trig_rise &= ~mask;
	ret = xr->access->modify(xr->priv, xr->base + XR_MPIOLVL, mask, (value & 1) << offset);
	if (ret >= 0)
		ret = xr->access->modify(xr->priv, xr->base + XR_MPIOSEL, mask, 0);
	if (ret < 0)
		return ret;
	return 0;
}

static void xr17v35x_gpio_set(struct gpio_chip *gc, u32 offset, s32 value)
{
	struct xr17v35x_gpio *xr = gpio_from_chip(gc);
	xr_gpio_dir_out(xr, offset, value);
}

static s32 xr17v35x_gpio_dir_in(struct gpio_chip *gc, u32 offset)
{
	s32 ret;
	struct xr17v35x_gpio *xr = gpio_from_chip(gc);
	ret =xr->access->modify(xr->priv, xr->base + XR_MPIOSEL, 0, 1 << offset);
	if (ret < 0)
		return ret;
	return 0;
}

static s32 xr17v35x_gpio_dir_out(struct gpio_chip *gc, u32 offset, s32 value)
{
	struct xr17v35x_gpio *xr = gpio_from_chip(gc);

	return xr_gpio_dir_out(xr, offset, value);
}

#ifdef CONFIG_GPIO_XR17V35X_IRQ
static int xr17v35x_gpio_to_irq(struct gpio_chip *gc, u32 offset)
{
	struct xr17v35x_gpio *xr = gpio_from_chip(gc);
	if ((xr->irq_base < 0)||(offset >= 8))
		return -EINVAL;
	return xr->irq_base + offset;
}

static void xr17v35x_irq_mask(struct irq_data *d)
{
	struct xr17v35x_gpio *xr = irq_data_get_irq_chip_data(d);
	xr->irq_mask |= 1 << (d->irq - xr->irq_base);
}

static void xr17v35x_irq_unmask(struct irq_data *d)
{
	struct xr17v35x_gpio *xr = irq_data_get_irq_chip_data(d);
	xr->irq_mask &= ~(1 << (d->irq - xr->irq_base));
}

static void xr17v35x_irq_bus_lock(struct irq_data *d)
{
	struct xr17v35x_gpio *xr = irq_data_get_irq_chip_data(d);
	mutex_lock(&xr->irq_lock);
}

static void xr17v35x_irq_bus_sync_unlock(struct irq_data *d)
{
	struct xr17v35x_gpio *xr = irq_data_get_irq_chip_data(d);
	unsigned int_mask;
	unsigned fall;

	fall = xr->irq_trig_fall & (~xr->irq_trig_rise | xr->last_state);
	int_mask = xr->irq_trig_fall | xr->irq_trig_rise;
	xr->last_fall = fall;
	xr->access->write(xr->priv, xr->base + XR_MPIOINV, fall);
	xr->access->modify(xr->priv, xr->base + XR_MPIOSEL, 0, int_mask);
	xr->access->write(xr->priv, xr->base + XR_MPIOINT, int_mask);
	if (int_mask)
		xr17v35x_gpio_update(xr);
	mutex_unlock(&xr->irq_lock);
}

static int xr17v35x_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct xr17v35x_gpio *xr = irq_data_get_irq_chip_data(d);
	unsigned gp = d->irq - xr->irq_base;
	unsigned mask = 1 << gp;

	if (type & IRQ_TYPE_LEVEL_MASK) {
		dev_err(xr->dev, "irq %d: unsupported type %d\n", d->irq, type);
		return -EINVAL;
	}
	if (type & IRQ_TYPE_EDGE_FALLING)
		xr->irq_trig_fall |= mask;
	else
		xr->irq_trig_fall &= ~mask;

	if (type & IRQ_TYPE_EDGE_RISING)
		xr->irq_trig_rise |= mask;
	else
		xr->irq_trig_rise &= ~mask;
	return 0;
}

static struct irq_chip xr17v35x_irq_chip = {
	.name			= "xr17v35x",
	.irq_mask		= xr17v35x_irq_mask,
	.irq_unmask		= xr17v35x_irq_unmask,
	.irq_bus_lock		= xr17v35x_irq_bus_lock,
	.irq_bus_sync_unlock	= xr17v35x_irq_bus_sync_unlock,
	.irq_set_type		= xr17v35x_irq_set_type,
};

void xr17v35x_gpio_update(struct xr17v35x_gpio *xr)
{
	unsigned state;
	unsigned change;
	unsigned pending;
	unsigned active;
	unsigned gp;
	unsigned fall;

	for (;;) {
		state = xr->access->read(xr->priv, xr->base + XR_MPIOLVL);
		if (state < 0)
			break;
		change = state ^ xr->last_state;
		xr->last_state = state;
		change &= ~xr->irq_mask;
		active = (state & xr->irq_trig_rise) | (~state & xr->irq_trig_fall);
		pending = change & active;

		while (pending) {
			gp = __ffs(pending);
			if (gp >= 8)
				break;
			handle_nested_irq(gp + xr->irq_base);
			pending &= ~(1 << gp);
		}

		fall = xr->irq_trig_fall & (~xr->irq_trig_rise | state);
		if (xr->last_fall == fall)
			break;
		xr->last_fall = fall;
		xr->access->write(xr->priv, xr->base + XR_MPIOINV, fall);
	}
}
void xr17v35x_gpio_int(struct xr17v35x_gpio *xr)
{
	xr17v35x_gpio_update(xr);
	xr17v35x_gpio_update(xr + 1);
}

void __devinit xr_setup_irqs(struct xr17v35x_gpio *xr, int irq_base)
{
	int gp;
	mutex_init(&xr->irq_lock);
	if (!irq_base)
		irq_base = MXC_BOARD_IRQ_START;
	xr->irq_base = irq_base;
	xr->_gc.to_irq = xr17v35x_gpio_to_irq;

	for (gp = 0; gp < xr->_gc.ngpio; gp++) {
		int ret;
		int irq = gp + xr->irq_base;

		pr_info("%s gp=%d, irq=%d\n", __func__, gp, irq);
		ret = irq_set_chip_data(irq, xr);
		if (ret < 0)
			pr_err("irq_set_chip_data %d\n", ret);
		irq_set_chip_and_handler(irq, &xr17v35x_irq_chip, handle_edge_irq);
		irq_set_nested_thread(irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(irq, IRQF_VALID);
#else
		irq_set_noprobe(irq);
#endif
	}
	pr_info("%s: irq_base=%i\n", __func__, xr->irq_base);
}

void __devexit xr_teardown_irqs(struct xr17v35x_gpio *xr)
{
	int gp;
	for (gp = 0; gp < xr->_gc.ngpio; gp++) {
		int irq = gp + xr->irq_base;
#ifdef CONFIG_ARM
		set_irq_flags(irq, 0);
#endif
		irq_set_chip_and_handler(irq, NULL, NULL);
		irq_set_chip_data(irq, NULL);
	}
}

#else
void xr_setup_irqs(struct xr17v35x_gpio *xr, int irq_base) {}
void xr_teardown_irqs(struct xr17v35x_gpio *xr) {}
#endif

static const char *labels[] = {
	"xr17v35x-gp0",
	"xr17v35x-gp1",
};

static const struct gpio_chip template_chip = {
	.owner			= THIS_MODULE,
	.direction_input	= xr17v35x_gpio_dir_in,
	.direction_output	= xr17v35x_gpio_dir_out,
	.get			= xr17v35x_gpio_get,
	.set			= xr17v35x_gpio_set,
	.ngpio			= 8,
};

static int __devinit xr17v35x_gpio_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	struct xr17v35x_gpio *xr, *xrbase;
	struct xr17v35x_gpio_platform_data *pdata = pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;
	xrbase = xr = kzalloc(sizeof(*xr) * 2, GFP_KERNEL);
	if (xr == NULL)
		return -ENOMEM;

	for (i = 0; i < 2; i++) {
		xr->base = (i == 0) ? xr_mpio0 : xr_mpio1;
		xr->access = pdata->access;
		xr->priv = pdata->priv;
		xr->dev			= &pdev->dev;
		xr->_gc = template_chip;
		xr->_gc.base = pdata->gpio_base;
		if (xr->_gc.base >= 0)
			xr->_gc.base += i * 8;
		xr->_gc.dev = &pdev->dev;
		xr->_gc.label = labels[i];
		xr->access->write(xr->priv, xr->base + XR_MPIOSEL, 0xff);
		xr->access->write(xr->priv, xr->base + XR_MPIO3T, 0x0);
		xr->access->write(xr->priv, xr->base + XR_MPIOOD, 0x0);
		xr->access->write(xr->priv, xr->base + XR_MPIOINV, 0);
		xr->access->write(xr->priv, xr->base + XR_MPIOLVL, 0);
		xr->access->write(xr->priv, xr->base + XR_MPIOINT, 0);

		ret = xr->access->read(xr->priv, xr->base + XR_MPIOLVL);
		if (ret >= 0)
			xr->last_state = ret;

#ifdef CONFIG_GPIO_XR17V35X_IRQ
		xr_setup_irqs(xr, (i == 0) ? pdata->irq_base : xrbase->irq_base + 8);
#endif
		ret = gpiochip_add(&xr->_gc);
		if (ret < 0) {
			dev_err(&pdev->dev, "Could not register gpiochip%d, %d\n", i, ret);
			if (i) {
				int err = gpiochip_remove(&xrbase->_gc);
				if (err < 0)
					dev_err(&pdev->dev, "gpiochip_remove err=%d\n", ret);
			}
			kfree(xrbase);
			return ret;
		}
		pr_info("%s: _gc.base=%i\n", __func__, xr->_gc.base);
		xr++;
	}
	platform_set_drvdata(pdev, xrbase);
#ifdef CONFIG_GPIO_XR17V35X_IRQ
	xrbase->access->register_gpio_interrupt(xrbase->priv,
			&xr17v35x_gpio_int, xrbase);
#endif
	return ret;
}

static int __devexit xr17v35x_gpio_remove(struct platform_device *pdev)
{
	struct xr17v35x_gpio *xr, *xrbase;
	int i;
	int ret;

	xr = xrbase = platform_get_drvdata(pdev);
	xr->access->register_gpio_interrupt(xr->priv, NULL, NULL);
	for (i = 0; i < 2; i++) {
		ret = gpiochip_remove(&xr->_gc);
		if (ret) {
			dev_err(&pdev->dev, "%s failed, %d\n",
					"gpiochip_remove()", ret);
			return ret;
		}
		xr_teardown_irqs(xr);
		xr++;
	}
	kfree(xrbase);
	return 0;
}

static struct platform_driver xr17v35x_gpio_driver = {
	.probe		= xr17v35x_gpio_probe,
	.remove		= __devexit_p(xr17v35x_gpio_remove),
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init xr17v35x_gpio_init(void)
{
	return platform_driver_register(&xr17v35x_gpio_driver);
}

static void __exit xr17v35x_gpio_exit(void)
{
	return platform_driver_unregister(&xr17v35x_gpio_driver);
}

module_init(xr17v35x_gpio_init);
module_exit(xr17v35x_gpio_exit);

MODULE_ALIAS("platform:xr17v35x-gpio");
MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("xr17v35x GPIO Device Driver");
MODULE_LICENSE("GPL");
