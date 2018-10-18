/*
 * drivers/amlogic/irqchip/irq-meson-gpio-double-edge.c
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

#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqchip.h>

#ifndef NO_IRQ
#define NO_IRQ ((unsigned int)(-1))
#endif

#define DRIVER_NAME "GPIO-INTC"
#define MESON_GPIO_BIND_PARENT_IRQ_NUM_MAX 2

#define REG_EDGE_POL   0x0
#define REG_PIN_03_SEL 0x4
#define REG_PIN_47_SEL 0x8
#define REG_FILTER_SEL 0xc

#define REG_EDGE_POL_MASK(x) (BIT(x) | BIT(16 + x))
#define REG_EDGE_SET(x)       BIT(x)
#define REG_POL_SET(x)        BIT(16+x)
/**
 * struct gpio_parent_irq - describe the parent irq for gpio
 *
 * @virq: virtual interrupt number of parent irq
 * @owner: hwirq for gpio
 */
struct gpio_parent_irq {
	int virq;
	unsigned int owner;
};

struct meson_gpio_irq_data {
	unsigned int nr_hwirq;
};

struct meson_gpio_intc {
	unsigned char nr_gicirq;
	spinlock_t lock;
	void __iomem *base;
	struct irq_domain *irqdomain;
	struct gpio_parent_irq *parent_irqs;
	const struct meson_gpio_irq_data *data;
};

static const struct meson_gpio_irq_data meson8_data = {
	.nr_hwirq = 134,
};

static const struct meson_gpio_irq_data meson8b_data = {
	.nr_hwirq = 119,
};

static const struct meson_gpio_irq_data gxbb_data = {
	.nr_hwirq = 133,
};

static const struct meson_gpio_irq_data gxl_data = {
	.nr_hwirq = 110,
};

static const struct meson_gpio_irq_data axg_data = {
	.nr_hwirq = 100,
};

static const struct meson_gpio_irq_data txlx_data = {
	.nr_hwirq = 119,
};

static const struct meson_gpio_irq_data g12a_data = {
	.nr_hwirq = 100,
};

static const struct meson_gpio_irq_data txl_data = {
	.nr_hwirq = 93,
};

static const struct meson_gpio_irq_data tl1_data = {
	.nr_hwirq = 102,
};

static const struct of_device_id meson_gpio_irq_matches[] = {
	{ .compatible = "amlogic,meson8-gpio-intc", .data = &meson8_data },
	{ .compatible = "amlogic,meson8b-gpio-intc", .data = &meson8b_data },
	{ .compatible = "amlogic,meson-gxbb-gpio-intc", .data = &gxbb_data },
	{ .compatible = "amlogic,meson-gxl-gpio-intc", .data = &gxl_data },
	{ .compatible = "amlogic,meson-axg-gpio-intc", .data = &axg_data },
	{ .compatible = "amlogic,meson-txlx-gpio-intc", .data = &txlx_data },
	{ .compatible = "amlogic,meson-g12a-gpio-intc", .data = &g12a_data },
	{ .compatible = "amlogic,meson-txl-gpio-intc", .data = &txl_data },
	{ .compatible = "amlogic,meson-tl1-gpio-intc", .data = &tl1_data },
	{ }
};

static void meson_reg_update_bits(void __iomem *base, unsigned int regoff,
	unsigned int mask, const unsigned int val)
{
	unsigned int orig;

	orig = readl_relaxed(base + regoff);
	orig = orig & (~mask);
	orig |= val;
	writel_relaxed(orig, base + regoff);
}

static int meson_gpio_irq_find_by_parent_virq(struct irq_data *irqd)
{
	struct irq_desc *desc = irq_data_to_desc(irqd);
	struct meson_gpio_intc *intc = irq_desc_get_handler_data(desc);
	int idx;

	for (idx = 0; idx < intc->nr_gicirq; idx++)
		if (intc->parent_irqs[idx].virq == irqd->irq)
			return idx;

	return -EINVAL;
}

static int meson_gpio_parent_irq_find_by_hwirq(struct irq_data *irqd,
	int *data, int dlen)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	int nr = 0;
	int idx;

	for (idx = 0; idx < intc->nr_gicirq; idx++) {
		if (nr >= dlen)
			break;

		if (intc->parent_irqs[idx].owner == irqd->hwirq)
			data[nr++] = idx;
	}

	return nr;
}

/*
 * NOP functions
 */
static void noop(struct irq_data *irqd) { }

static void meson_gpio_parent_irq_unmask(int virq)
{
	struct irq_data *parent_data;

	parent_data = irq_get_irq_data(virq);

	/*enable the interrupt line of gpio in GIC controller*/
	parent_data->chip->irq_unmask(parent_data);
}
static void meson_gpio_parent_irq_mask(int virq)
{
	struct irq_data *parent_data;

	parent_data = irq_get_irq_data(virq);

	/*disable the interrupt line of gpio in GIC controller*/
	parent_data->chip->irq_mask(parent_data);
}

static int meson_gpio_parent_irq_request(struct irq_data *irqd,
	unsigned int type)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	struct irq_data *parent_data;
	unsigned int idx;

	for (idx = 0; idx < intc->nr_gicirq; idx++) {
		if (intc->parent_irqs[idx].owner == NO_IRQ) {
			intc->parent_irqs[idx].owner = irqd->hwirq;
			break;
		}
	}

	if (idx == intc->nr_gicirq) {
		pr_warn("%s: no more gpio irqs available\n", DRIVER_NAME);
		return -EINVAL;
	}

	parent_data = irq_get_irq_data(intc->parent_irqs[idx].virq);

	/*set trigger type of gpio in GIC controller*/
	if (type & IRQ_TYPE_EDGE_BOTH)
		parent_data->chip->irq_set_type(parent_data,
			IRQ_TYPE_EDGE_RISING);
	else
		parent_data->chip->irq_set_type(parent_data,
			IRQ_TYPE_LEVEL_HIGH);

	pr_info("%s: gpio virq[%d] connect to GIC hwirq[%ld]\n", DRIVER_NAME,
		irqd->irq, parent_data->hwirq);

	return idx;
}

static int meson_gpio_parent_irq_release(struct irq_data *irqd)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	int idx[MESON_GPIO_BIND_PARENT_IRQ_NUM_MAX];
	int nr;

	nr = meson_gpio_parent_irq_find_by_hwirq(irqd, idx, ARRAY_SIZE(idx));
	while (nr--)
		intc->parent_irqs[idx[nr]].owner = NO_IRQ;

	return 0;
}

static void meson_gpio_irq_regs_config(struct irq_data *irqd,
	int idx, unsigned int type)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	unsigned int val = 0;
	unsigned long flags;
	int regoff;
	int shift;

	spin_lock_irqsave(&intc->lock, flags);

	if (type & IRQ_TYPE_EDGE_BOTH)
		val |= REG_EDGE_SET(idx);

	if (type & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_LEVEL_LOW))
		val |= REG_POL_SET(idx);

	meson_reg_update_bits(intc->base, REG_EDGE_POL,
			REG_EDGE_POL_MASK(idx), val);

	/*set the  filter registers*/
	shift = idx << 2;
	meson_reg_update_bits(intc->base, REG_FILTER_SEL,
			0x7 << shift, 0x7 << shift);

	/*set pin select register*/
	shift = (idx << 3) % 32;
	regoff = (idx < 4) ? REG_PIN_03_SEL : REG_PIN_47_SEL;
	meson_reg_update_bits(intc->base, regoff,
			0xff << shift, irqd->hwirq << shift);

	spin_unlock_irqrestore(&intc->lock, flags);
}

static void meson_gpio_irq_enable(struct irq_data *irqd)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	int idx[MESON_GPIO_BIND_PARENT_IRQ_NUM_MAX];
	unsigned long flags;
	int nr;

	spin_lock_irqsave(&intc->lock, flags);

	nr = meson_gpio_parent_irq_find_by_hwirq(irqd,
			idx, ARRAY_SIZE(idx));

	while (nr--)
		meson_gpio_parent_irq_unmask(intc->parent_irqs[idx[nr]].virq);

	spin_unlock_irqrestore(&intc->lock, flags);
}

static void meson_gpio_irq_disable(struct irq_data *irqd)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	int idx[MESON_GPIO_BIND_PARENT_IRQ_NUM_MAX];
	unsigned long flags;
	int nr;

	spin_lock_irqsave(&intc->lock, flags);

	nr = meson_gpio_parent_irq_find_by_hwirq(irqd, idx, ARRAY_SIZE(idx));
	while (nr--)
		meson_gpio_parent_irq_mask(intc->parent_irqs[idx[nr]].virq);

	spin_unlock_irqrestore(&intc->lock, flags);
}

/**
 *free gpio irq when free_irq() is called, and another pin can use it again.
 */
static void meson_gpio_irq_shutdown(struct irq_data *irqd)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	unsigned long flags;

	spin_lock_irqsave(&intc->lock, flags);
	meson_gpio_parent_irq_release(irqd);
	spin_unlock_irqrestore(&intc->lock, flags);

	meson_gpio_irq_disable(irqd);
}

static int meson_gpio_irq_type(struct irq_data *irqd, unsigned int type)
{
	struct meson_gpio_intc *intc = irq_data_get_irq_chip_data(irqd);
	unsigned long flags;
	int nr_parent_irq;
	int idx;

	type = type & IRQ_TYPE_SENSE_MASK;
	nr_parent_irq = (type == IRQ_TYPE_EDGE_BOTH) ? 2 : 1;

	while (nr_parent_irq--) {
		if (type == IRQ_TYPE_EDGE_BOTH)
			type = IRQ_TYPE_EDGE_FALLING;

		spin_lock_irqsave(&intc->lock, flags);
		idx = meson_gpio_parent_irq_request(irqd, type);
		spin_unlock_irqrestore(&intc->lock, flags);
		if (idx < 0) {
			meson_gpio_irq_shutdown(irqd);
			return -EINVAL;
		}

		meson_gpio_irq_regs_config(irqd, idx, type);

		if (nr_parent_irq > 0)
			type = IRQ_TYPE_EDGE_RISING;
	}

	return 0;
}

static struct irq_chip meson_gpio_intc_chip = {
	.name		= "meson-gpio-irqchip",
	.irq_enable	= meson_gpio_irq_enable,
	.irq_disable	= meson_gpio_irq_disable,
	.irq_set_type	= meson_gpio_irq_type,
	.irq_mask	= noop,
	.irq_unmask	= noop,
	.irq_shutdown	= meson_gpio_irq_shutdown,
};

static int meson_gpio_intc_domain_map(struct irq_domain *d, unsigned int virq,
			 irq_hw_number_t hwirq)
{
	struct meson_gpio_intc *intc = d->host_data;

	irq_set_chip_and_handler(virq,
		&meson_gpio_intc_chip, handle_simple_irq);
	irq_set_chip_data(virq, intc);

	return 0;
}

static const struct irq_domain_ops meson_gpio_intc_domain_ops = {
	.map = meson_gpio_intc_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

static void meson_gpio_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct irq_data *parent_data = irq_desc_get_irq_data(desc);
	struct meson_gpio_intc *intc = irq_desc_get_handler_data(desc);
	int idx;

	chained_irq_enter(chip, desc);

	idx = meson_gpio_irq_find_by_parent_virq(parent_data);
	if (idx >= 0 && intc->parent_irqs[idx].owner != NO_IRQ)
		generic_handle_irq(irq_find_mapping(intc->irqdomain,
			intc->parent_irqs[idx].owner));

	chained_irq_exit(chip, desc);
}

static int __init meson_gpio_intc_init(struct device_node *node,
	struct device_node *parent)
{
	struct meson_gpio_intc *intc;
	const struct of_device_id *match;
	struct irq_fwspec fwspec;
	int parent_hwirq;
	int parent_virq;
	int ret;
	int idx;

	intc = kzalloc(sizeof(struct meson_gpio_intc), GFP_KERNEL);
	if (!intc)
		return -ENOMEM;

	match = of_match_node(meson_gpio_irq_matches, node);
	if (!match) {
		ret = -ENODEV;
		pr_err("%s: fail to match device node\n", DRIVER_NAME);
		goto alloc_err1;
	}
	intc->data = match->data;

	ret = of_property_count_elems_of_size(node,
				"amlogic,channel-interrupts", sizeof(u32));
	if (ret <= 0) {
		pr_err("%s: fail to get the number of elements\n", DRIVER_NAME);
		ret = -EINVAL;
		goto alloc_err1;
	}
	intc->nr_gicirq = ret;

	intc->parent_irqs = kcalloc(intc->nr_gicirq,
			sizeof(struct gpio_parent_irq), GFP_KERNEL);
	if (!intc->parent_irqs) {
		ret = -ENOMEM;
		goto alloc_err1;
	}

	spin_lock_init(&intc->lock);

	intc->base = of_iomap(node, 0);
	if (IS_ERR_OR_NULL(intc->base)) {
		ret = -ENOMEM;
		goto alloc_err2;
	}

	intc->irqdomain = irq_domain_add_linear(node, intc->data->nr_hwirq,
			&meson_gpio_intc_domain_ops, intc);
	if (IS_ERR_OR_NULL(intc->irqdomain)) {
		ret = -ENOMEM;
		goto iomap_err;
	}

	for (idx = 0; idx < intc->nr_gicirq; idx++) {
		ret = of_property_read_u32_index(node,
				"amlogic,channel-interrupts", idx,
				&parent_hwirq);
		if (ret < 0) {
			pr_err("%s: fail to read property value\n",
					DRIVER_NAME);
			goto iomap_err;
		}

		fwspec.fwnode = of_node_to_fwnode(parent);
		fwspec.param_count = 3;
		fwspec.param[0] = 0;
		fwspec.param[1] = parent_hwirq;
		fwspec.param[2] = IRQ_TYPE_EDGE_RISING;

		parent_virq = irq_create_fwspec_mapping(&fwspec);

		intc->parent_irqs[idx].virq = parent_virq;
		intc->parent_irqs[idx].owner = NO_IRQ;
		irq_set_handler_data(parent_virq, intc);
		irq_set_chained_handler(parent_virq, meson_gpio_irq_handler);

		/*disable the interrupt line of gpio in GIC controller*/
		meson_gpio_parent_irq_mask(parent_virq);
	}

	pr_info("%s: support to detect double-edge trigger signal\n",
			DRIVER_NAME);

	return 0;

iomap_err:
	iounmap(intc->base);

alloc_err2:
	kfree(intc->parent_irqs);

alloc_err1:
	kfree(intc);

	return ret;
}

/*
 * if you want to use the Meson GPIO IRQ which support the
 * double-edge detection, please set the compatible property
 * to "amlogic,meson-gpio-intc-ext" in dts
 */
IRQCHIP_DECLARE(meson_gpio, "amlogic,meson-gpio-intc-ext",
		meson_gpio_intc_init);
