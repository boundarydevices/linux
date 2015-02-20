/*
 *  max77823.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77823.h>
#include <linux/mfd/max77823-private.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell max77823_devs[] = {
	{
		.name = "sec-battery",
		.of_compatible = "samsung,sec-battery",
	},
	{
		.name = "max77823-charger",
		.of_compatible = "samsung,max77823-charger",
	},
	{
		.name = "max77823-fuelgauge",
		.of_compatible = "samsung,max77823-fuelgauge",
	},
};

static const u8 max77823_mask_reg[] = {
	[CHG_IRQ] = MAX77823_CHG_INT_MASK,
	[FUEL_IRQ] = MAX77823_FUEL_INT_MASK,
};

int max77823_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL(max77823_read_reg);

int max77823_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(max77823_bulk_read);

int max77823_read_word(struct i2c_client *i2c, u8 reg)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL(max77823_read_word);

int max77823_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0)
		pr_info("%s reg(0x%x), ret(%d)\n",
			__func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL(max77823_write_reg);

int max77823_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(max77823_bulk_write);

int max77823_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&max77823->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL(max77823_write_word);


int max77823_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77823->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&max77823->i2c_lock);
	return ret;
}
EXPORT_SYMBOL(max77823_update_reg);

static struct i2c_client *get_i2c(struct max77823_dev *max77823,
				enum max77823_irq_source src)
{
	switch (src) {
	case CHG_IRQ:
		return max77823->charger;
	case FUEL_IRQ:
		return max77823->fuelgauge;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct max77823_irq_data {
	int mask;
	enum max77823_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct max77823_irq_data max77823_irqs[] = {
	DECLARE_IRQ(MAX77823_CHG_IRQ_BYP_I,	CHG_IRQ, 1 << 0),
	DECLARE_IRQ(MAX77823_CHG_IRQ_BATP_I,	CHG_IRQ, 1 << 2),
	DECLARE_IRQ(MAX77823_CHG_IRQ_BAT_I,	CHG_IRQ, 1 << 3),
	DECLARE_IRQ(MAX77823_CHG_IRQ_CHG_I,	CHG_IRQ, 1 << 4),
	DECLARE_IRQ(MAX77823_CHG_IRQ_WCIN_I,	CHG_IRQ, 1 << 5),
	DECLARE_IRQ(MAX77823_CHG_IRQ_CHGIN_I,	CHG_IRQ, 1 << 6),
	DECLARE_IRQ(MAX77823_FG_IRQ_ALERT,	FUEL_IRQ, 1 << 1),
};

static void max77823_irq_ack(struct irq_data *data)
{
}

static void max77823_irq_lock(struct irq_data *data)
{
	struct max77823_dev *max77823 = irq_get_chip_data(data->irq);

	mutex_lock(&max77823->irqlock);
}

static void max77823_irq_sync_unlock(struct irq_data *data)
{
	struct max77823_dev *max77823 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < MAX77823_IRQ_GROUP_NR; i++) {
		u8 mask_reg = max77823_mask_reg[i];
		struct i2c_client *i2c = get_i2c(max77823, i);

		if (mask_reg == MAX77823_REG_INVALID ||
		    IS_ERR_OR_NULL(i2c))
			continue;
		max77823->irq_masks_cache[i] = max77823->irq_masks_cur[i];

		max77823_write_reg(i2c, max77823_mask_reg[i],
				max77823->irq_masks_cur[i]);
	}

	mutex_unlock(&max77823->irqlock);
}

static const inline struct max77823_irq_data *
irq_to_max77823_irq(struct max77823_dev *max77823, int irq)
{
	return &max77823_irqs[irq - max77823->irq_base];
}

static void max77823_irq_mask(struct irq_data *data)
{
	struct max77823_dev *max77823 = irq_get_chip_data(data->irq);
	const struct max77823_irq_data *irq_data =
		irq_to_max77823_irq(max77823, data->irq);

	if (irq_data->group >= MAX77823_IRQ_GROUP_NR)
		return;

	max77823->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void max77823_irq_unmask(struct irq_data *data)
{
	struct max77823_dev *max77823 = irq_get_chip_data(data->irq);
	const struct max77823_irq_data *irq_data =
	    irq_to_max77823_irq(max77823, data->irq);

	if (irq_data->group >= MAX77823_IRQ_GROUP_NR)
		return;

	max77823->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip max77823_irq_chip = {
	.name			= "max77823",
	.irq_ack                = max77823_irq_ack,
	.irq_bus_lock		= max77823_irq_lock,
	.irq_bus_sync_unlock	= max77823_irq_sync_unlock,
	.irq_mask		= max77823_irq_mask,
	.irq_unmask		= max77823_irq_unmask,
};

static irqreturn_t max77823_irq_thread(int irq, void *data)
{
	struct max77823_dev *max77823 = data;
	u8 irq_reg[MAX77823_IRQ_GROUP_NR] = {0};
	u8 irq_src;
	int ret;
	int i;
	u8 reg_data;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
		 gpio_get_value(max77823->irq_gpio));

	ret = max77823_read_reg(max77823->i2c,
				MAX77823_PMIC_INT, &irq_src);
	if (ret < 0) {
		dev_err(max77823->dev, "Failed to read interrupt source: %d\n",
			ret);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & MAX77823_IRQSRC_CHG) {
		/* CHG_IRQ */
		max77823_read_reg(max77823->charger,
			MAX77823_CHG_INT, &irq_reg[CHG_IRQ]);

		pr_info("%s: charger interrupt(0x%02x)\n",
				__func__, irq_reg[CHG_IRQ]);

		max77823_read_reg(max77823->charger,
			MAX77823_CHG_INT_MASK, &reg_data);
		pr_info("%s: charger interrupt mask(0x%02x)\n",
			__func__, reg_data);

		/* mask chgin to prevent chgin infinite interrupt
		 * chgin is unmasked chgin isr
		 */
		if (irq_reg[CHG_IRQ] &
				max77823_irqs[MAX77823_CHG_IRQ_CHGIN_I].mask) {
			max77823_read_reg(max77823->charger,
					  MAX77823_CHG_INT_MASK, &reg_data);
			reg_data |= (1 << 6);
			max77823_write_reg(max77823->charger,
					   MAX77823_CHG_INT_MASK, reg_data);
		}
	}

	if (irq_src & MAX77823_IRQSRC_FG) {
		pr_info("[%s] fuelgauge interrupt\n", __func__);
		pr_info("[%s]IRQ_BASE(%d), NESTED_IRQ(%d)\n",
			__func__, max77823->irq_base, max77823->irq_base + MAX77823_FG_IRQ_ALERT);
		handle_nested_irq(max77823->irq_base + MAX77823_FG_IRQ_ALERT);
		return IRQ_HANDLED;
	}

	pr_debug("%s: irq gpio post-state(0x%02x)\n", __func__,
		gpio_get_value(max77823->irq_gpio));

	/* Report */
	for (i = 0; i < MAX77823_IRQ_NR; i++) {
		if (irq_reg[max77823_irqs[i].group] & max77823_irqs[i].mask) {
			pr_info("[%s]IRQ_BASE(%d), NESTED_IRQ(%d)\n",
				__func__, max77823->irq_base, max77823->irq_base + i);
			handle_nested_irq(max77823->irq_base + i);
		}
	}

	return IRQ_HANDLED;
}

static int max77823_irq_init(struct max77823_dev *max77823)
{
	int i;
	int cur_irq;
	int ret;
	u8 i2c_data;

	if (!max77823->irq_gpio) {
		dev_warn(max77823->dev, "No interrupt specified.\n");
		max77823->irq_base = 0;
		return 0;
	}

	if (!max77823->irq_base) {
		dev_err(max77823->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&max77823->irqlock);

	max77823->irq = gpio_to_irq(max77823->irq_gpio);
	pr_info("%s irq=%d, irq->gpio=%d\n", __func__,
			max77823->irq, max77823->irq_gpio);

	ret = gpio_request(max77823->irq_gpio, "max77823_irq");
	if (ret) {
		dev_err(max77823->dev, "%s: failed requesting gpio %d\n",
			__func__, max77823->irq_gpio);
		return ret;
	}
	gpio_direction_input(max77823->irq_gpio);
	gpio_free(max77823->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77823_IRQ_GROUP_NR; i++) {

		max77823->irq_masks_cur[i] = 0x00;
		max77823->irq_masks_cache[i] = 0x00;

		if (max77823_mask_reg[i] == MAX77823_REG_INVALID)
			continue;
		if (i == CHG_IRQ)
			max77823_write_reg(max77823->charger, max77823_mask_reg[i], 0x9a);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77823_IRQ_NR; i++) {
		cur_irq = i + max77823->irq_base;
		irq_set_chip_data(cur_irq, max77823);
		irq_set_chip_and_handler(cur_irq, &max77823_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);
		irq_set_noprobe(cur_irq);
	}

	/* Unmask max77823 interrupt */
	ret = max77823_read_reg(max77823->i2c, MAX77823_PMIC_INT_MASK,
				&i2c_data);
	if (ret) {
		dev_err(max77823->dev, "%s: fail to read max77823 reg\n", __func__);
		return ret;
	}

	i2c_data &= ~(MAX77823_IRQSRC_CHG);	/* Unmask charger interrupt */
	i2c_data &= ~(MAX77823_IRQSRC_FG);	/* Unmask fuelgauge interrupt */

	max77823_write_reg(max77823->i2c, MAX77823_PMIC_INT_MASK,
			   i2c_data);

	pr_info("%s max77823_PMIC_REG_INTSRC_MASK=0x%02x\n",
		__func__, i2c_data);

	ret = request_threaded_irq(max77823->irq, NULL, max77823_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "max77823-irq", max77823);

	if (ret) {
		dev_err(max77823->dev, "Failed to request IRQ %d: %d\n",
			max77823->irq, ret);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_OF)
static int of_max77823_dt(struct device *dev, struct max77823_platform_data *pdata)
{
	struct device_node *np_max77823 = dev->of_node;

	if(!np_max77823)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_max77823, "max77823,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_max77823, "max77823,wakeup");

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	return 0;
}
#endif /* CONFIG_OF */

static int max77823_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct max77823_dev *max77823;
	struct max77823_platform_data *pdata;

	int ret;

	pr_info("[%s] START\n", __func__);

	max77823 = kzalloc(sizeof(struct max77823_dev), GFP_KERNEL);
	if (max77823 == NULL)
		return -ENOMEM;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct max77823_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev,
				"Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;
		}


		ret = of_max77823_dt(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get device of_node\n");
			goto err;
		}
		client->dev.platform_data = pdata;
	} else
		pdata = client->dev.platform_data;

	max77823->i2c = client;
	max77823->dev = &client->dev;
	max77823->irq = client->irq;

	if (pdata) {
		max77823->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77823_IRQ_NR, -1);
		pr_info("%s : IRQ_BASE = %d\n", __func__, pdata->irq_base);
		if (pdata->irq_base < 0) {
			pr_err("%s: irq_alloc_descs Fail ret(%d)\n",
			       __func__, pdata->irq_base);
			ret = -EINVAL;
		} else
			max77823->irq_base = pdata->irq_base;

		max77823->irq_gpio = pdata->irq_gpio;
		max77823->wakeup = pdata->wakeup;
#if 0
		gpio_tlmm_config(GPIO_CFG(max77823->irq_gpio, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_DISABLE);
#endif
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77823->i2c_lock);

	i2c_set_clientdata(client, max77823);

	max77823->charger = i2c_new_dummy(client->adapter, I2C_ADDR_CHARGER);
	i2c_set_clientdata(max77823->charger, max77823);

	max77823->fuelgauge = i2c_new_dummy(client->adapter, I2C_ADDR_FUELGAUGE);
	i2c_set_clientdata(max77823->fuelgauge, max77823);

	ret = max77823_irq_init(max77823);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77823->dev, -1, max77823_devs,
			      ARRAY_SIZE(max77823_devs), NULL,
			      max77823->irq_base, NULL);

	if (ret) {
		dev_err(&client->dev, "%s : failed to add devices\n", __func__);
		goto err_mfd;
	}

	device_init_wakeup(max77823->dev, pdata->wakeup);

	pr_info("[%s] END\n", __func__);

	return ret;

err_mfd:
	mfd_remove_devices(max77823->dev);
	free_irq(max77823->irq, max77823);
err_irq_init:
	i2c_unregister_device(max77823->charger);
	i2c_unregister_device(max77823->fuelgauge);
err:
	kfree(max77823);

	return ret;
}

static int max77823_remove(struct i2c_client *client)
{
	struct max77823_dev *max77823 = i2c_get_clientdata(client);

	mfd_remove_devices(max77823->dev);
	i2c_unregister_device(max77823->charger);
	i2c_unregister_device(max77823->fuelgauge);
	return 0;
}

static const struct i2c_device_id max77823_id[] = {
	{"max77823", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, max77823_id);

#if defined(CONFIG_OF)
static struct of_device_id max77823_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77823" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77823_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver max77823_driver = {
	.driver = {
		.name	= "max77823",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = max77823_i2c_dt_ids,
#endif
	},
	.probe	= max77823_probe,
	.remove	= max77823_remove,
	.id_table	= max77823_id,
};

static int __init max77823_init(void)
{
	pr_info("[%s] start \n", __func__);
	return i2c_add_driver(&max77823_driver);
}

static void __exit max77823_exit(void)
{
	i2c_del_driver(&max77823_driver);
}

module_init(max77823_init);
module_exit(max77823_exit);

MODULE_DESCRIPTION("Samsung MAX77823 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
