/*
 * Arizona interrupt support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"

static int arizona_map_irq(struct arizona *arizona, int irq)
{
	if (irq < ARIZONA_AOD_IRQBASE)
		return arizona->int_base + irq;
	return arizona->aod_base + irq - ARIZONA_AOD_IRQBASE;
}

int arizona_request_irq(struct arizona *arizona, int irq, char *name,
			   irq_handler_t handler, void *data)
{
	irq = arizona_map_irq(arizona, irq);
	if (irq < 0)
		return irq;

	return request_threaded_irq(irq, NULL, handler, IRQF_ONESHOT,
				    name, data);
}
EXPORT_SYMBOL_GPL(arizona_request_irq);

void arizona_free_irq(struct arizona *arizona, int irq, void *data)
{
	irq = arizona_map_irq(arizona, irq);
	if (irq < 0)
		return;

	free_irq(irq, data);
}
EXPORT_SYMBOL_GPL(arizona_free_irq);

int arizona_set_irq_wake(struct arizona *arizona, int irq, int on)
{
	irq = arizona_map_irq(arizona, irq);
	if (irq < 0)
		return irq;

	return irq_set_irq_wake(irq, on);
}
EXPORT_SYMBOL_GPL(arizona_set_irq_wake);

static irqreturn_t arizona_boot_done(int irq, void *data)
{
	struct arizona *arizona = data;

	dev_dbg(arizona->dev, "Boot done\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_ctrlif_err(int irq, void *data)
{
	struct arizona *arizona = data;

	/*
	 * For pretty much all potential sources a register cache sync
	 * won't help, we've just got a software bug somewhere.
	 */
	dev_err(arizona->dev, "Control interface error\n");

	return IRQ_HANDLED;
}

int regmap_irq_chip_get_last_irq(struct regmap_irq_chip_data *data);

static irqreturn_t arizona_irq_thread(int irq, void *data)
{
	struct arizona *arizona = data;
	unsigned int val = 0;
	int ret;

	ret = pm_runtime_get_sync(arizona->dev);
	if (ret < 0) {
		unsigned st[5];
		dev_err(arizona->dev, "Failed to resume device: %d\n", ret);

		ret = regmap_read(arizona->regmap, ARIZONA_IRQ_PIN_STATUS, &val);
		pr_info("%s: pin status=%x\n", __func__, val);

		regmap_write(arizona->regmap, ARIZONA_INTERRUPT_CONTROL, 1);
		regmap_write(arizona->regmap, ARIZONA_IRQ2_CONTROL, 1);
		regmap_read(arizona->regmap, ARIZONA_AOD_IRQ1, &val);
		pr_info("%s: aod_irq1=%x\n", __func__, val);
		regmap_read(arizona->regmap, ARIZONA_INTERRUPT_STATUS_1, &st[0]);
		regmap_read(arizona->regmap, ARIZONA_INTERRUPT_STATUS_2, &st[1]);
		regmap_read(arizona->regmap, ARIZONA_INTERRUPT_STATUS_3, &st[2]);
		regmap_read(arizona->regmap, ARIZONA_INTERRUPT_STATUS_4, &st[3]);
		regmap_read(arizona->regmap, ARIZONA_INTERRUPT_STATUS_5, &st[4]);
		pr_info("%s: int status=%x %x %x %x %x\n", __func__,
				st[0], st[1], st[2], st[3], st[4]);
		return IRQ_NONE;
	}


	handle_nested_irq(arizona->virq[1]);
	if (arizona->pdata.irq_gpio) {
		val = gpio_get_value_cansleep(arizona->pdata.irq_gpio);
		if (arizona->pdata.irq_active_high)
				val ^= 1;
	}
	if (!val) {
		/* handle the AoD domain if int still pending */
		handle_nested_irq(arizona->virq[0]);
		if (arizona->pdata.irq_gpio) {
			val = gpio_get_value_cansleep(arizona->pdata.irq_gpio);
			if (arizona->pdata.irq_active_high)
					val ^= 1;
		}
		if (!val) {
			int irq = regmap_irq_chip_get_last_irq(arizona->irq_chip);
			if (!irq)
				irq = regmap_irq_chip_get_last_irq(arizona->aod_irq_chip);
			if (!irq) {
				dev_err(arizona->dev, "No interrupts processed, still pending\n");
				msleep(100);
			}
		}
	}

	pm_runtime_mark_last_busy(arizona->dev);
	pm_runtime_put_autosuspend(arizona->dev);

	return IRQ_HANDLED;
}

static void arizona_irq_enable(struct irq_data *data)
{
}

static void arizona_irq_disable(struct irq_data *data)
{
}

static struct irq_chip arizona_irq_chip = {
	.name			= "arizona",
	.irq_disable		= arizona_irq_disable,
	.irq_enable		= arizona_irq_enable,
};

int arizona_irq_init(struct arizona *arizona)
{
	int flags = IRQF_ONESHOT;
	int ret, i;
	const struct regmap_irq_chip *aod, *irq;
	bool ctrlif_error = true;
	int irq_base;

	switch (arizona->type) {
#ifdef CONFIG_MFD_WM5102
	case WM5102:
		aod = &wm5102_aod;
		irq = &wm5102_irq;

		ctrlif_error = false;
		break;
#endif
#ifdef CONFIG_MFD_WM5110
	case WM5110:
		aod = &wm5110_aod;
		irq = &wm5110_irq;

		ctrlif_error = false;
		break;
#endif
#ifdef CONFIG_MFD_WM8997
	case WM8997:
		aod = &wm8997_aod;
		irq = &wm8997_irq;

		ctrlif_error = false;
		break;
#endif
	default:
		BUG_ON("Unknown Arizona class device" == NULL);
		return -EINVAL;
	}

	if (arizona->pdata.irq_active_high) {
		ret = regmap_update_bits(arizona->regmap, ARIZONA_IRQ_CTRL_1,
					 ARIZONA_IRQ_POL, 0);
		if (ret != 0) {
			dev_err(arizona->dev, "Couldn't set IRQ polarity: %d\n",
				ret);
			goto err;
		}

		flags |= IRQF_TRIGGER_HIGH;
	} else {
		flags |= IRQF_TRIGGER_LOW;
	}

        /* set virtual IRQs */
	irq_base = arizona->pdata.irq_base;
	if (irq_base <= 0) {
		dev_err(arizona->dev, "No irq_base specified\n");
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(arizona->virq); i++)
		arizona->virq[i] = irq_base + i;

	ret = irq_alloc_descs(irq_base, 0, ARRAY_SIZE(arizona->virq), 0);
	if (ret < 0) {
		dev_err(arizona->dev, "Failed to allocate IRQs: %d\n", ret);
		return ret;
        }

	for (i = 0; i < ARRAY_SIZE(arizona->virq); i++) {
		irq_set_chip_data(arizona->virq[i], arizona);
		irq_set_chip_and_handler(arizona->virq[i], &arizona_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(arizona->virq[i], 1);

		/* ARM needs us to explicitly flag the IRQ as valid and
		 * will set them noprobe when we do so.
		 */
#ifdef CONFIG_ARM
		set_irq_flags(arizona->virq[i], IRQF_VALID);
#else
		irq_set_noprobe(arizona->virq[i]);
#endif
	}
	irq_base += ARRAY_SIZE(arizona->virq);

	arizona->aod_base = irq_base;
	ret = regmap_add_irq_chip(arizona->regmap,
				  arizona->virq[0],
				  IRQF_ONESHOT, irq_base, aod,
				  &arizona->aod_irq_chip);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to add AOD IRQs: %d\n", ret);
		goto err_domain;
	}
	irq_base += aod->num_irqs;

	arizona->int_base = irq_base;
	ret = regmap_add_irq_chip(arizona->regmap,
				  arizona->virq[1],
				  IRQF_ONESHOT, irq_base, irq,
				  &arizona->irq_chip);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to add IRQs: %d\n", ret);
		goto err_aod;
	}

	/* Make sure the boot done IRQ is unmasked for resumes */
	i = arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE);
	ret = request_threaded_irq(i, NULL, arizona_boot_done, IRQF_ONESHOT,
				   "Boot done", arizona);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request boot done %d: %d\n",
			arizona->irq, ret);
		goto err_boot_done;
	}

	/* Handle control interface errors in the core */
	if (ctrlif_error) {
		i = arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR);
		ret = request_threaded_irq(i, NULL, arizona_ctrlif_err,
					   IRQF_ONESHOT,
					   "Control interface error", arizona);
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to request CTRLIF_ERR %d: %d\n",
				arizona->irq, ret);
			goto err_ctrlif;
		}
	}

	/* Used to emulate edge trigger and to work around broken pinmux */
	if (arizona->pdata.irq_gpio) {
		if (gpio_to_irq(arizona->pdata.irq_gpio) != arizona->irq) {
			dev_warn(arizona->dev, "IRQ %d is not GPIO %d (%d)\n",
				 arizona->irq, arizona->pdata.irq_gpio,
				 gpio_to_irq(arizona->pdata.irq_gpio));
			arizona->irq = gpio_to_irq(arizona->pdata.irq_gpio);
		}

		ret = gpio_request_one(arizona->pdata.irq_gpio,
					    GPIOF_IN, "arizona IRQ");
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to request IRQ GPIO %d:: %d\n",
				arizona->pdata.irq_gpio, ret);
			arizona->pdata.irq_gpio = 0;
		}
	}

	ret = request_threaded_irq(arizona->irq, NULL, arizona_irq_thread,
				   flags, "arizona", arizona);

	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request IRQ %d: %d\n",
			arizona->irq, ret);
		goto err_main_irq;
	}

	return 0;

err_main_irq:
	if (arizona->pdata.irq_gpio)
		gpio_free(arizona->pdata.irq_gpio);
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR), arizona);
err_ctrlif:
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE), arizona);
err_boot_done:
	regmap_del_irq_chip(arizona->virq[1],
			    arizona->irq_chip);
err_aod:
	regmap_del_irq_chip(arizona->virq[0],
			    arizona->aod_irq_chip);
err_domain:
err:
	return ret;
}

int arizona_irq_exit(struct arizona *arizona)
{
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR), arizona);
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE), arizona);
	regmap_del_irq_chip(arizona->irq,
			    arizona->irq_chip);
	regmap_del_irq_chip(arizona->irq,
			    arizona->aod_irq_chip);
	if (arizona->pdata.irq_gpio)
		gpio_free(arizona->pdata.irq_gpio);
	free_irq(arizona->irq, arizona);

	return 0;
}

