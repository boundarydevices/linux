// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>

#define NT50358A_REG_VOP	0x00
#define NT50358A_REG_VON	0x01
#define NT50358A_REG_APPS	0x03

#define VOUT_MASK	0x1F

#define MIN_UV		4000000
#define STEP_UV		100000
#define MAX_UV		6000000
#define N_VOLTAGES	((MAX_UV - MIN_UV) / STEP_UV + 1)

#define DSV_OUT_POS	0
#define DSV_OUT_NEG	1
#define DSV_OUT_MAX	2

#define DSVP_ENABLE	BIT(0)
#define DSVN_ENABLE	BIT(1)
#define DSVALL_ENABLE	(DSVP_ENABLE | DSVN_ENABLE)

struct nt50358a_priv {
	struct device *dev;
	struct gpio_descs *enable_gpios;
	unsigned int enable_flag;
	unsigned int volt_sel[DSV_OUT_MAX];
};

static int nt50358a_set_voltage_sel(struct regulator_dev *rdev, unsigned int selector)
{
	struct nt50358a_priv *priv = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev), ret;

	if (priv->enable_flag & BIT(id)) {
		ret = regulator_set_voltage_sel_regmap(rdev, selector);
		if (ret)
			return ret;
	}

	priv->volt_sel[id] = selector;
	return 0;
}

static int nt50358a_get_voltage_sel(struct regulator_dev *rdev)
{
	struct nt50358a_priv *priv = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);

	if (priv->enable_flag & BIT(id))
		return regulator_get_voltage_sel_regmap(rdev);

	return priv->volt_sel[id];
}

static int nt50358a_enable(struct regulator_dev *rdev)
{
	struct nt50358a_priv *priv = rdev_get_drvdata(rdev);
	struct gpio_descs *gpios = priv->enable_gpios;
	int id = rdev_get_id(rdev), ret;

	if (!gpios || gpios->ndescs <= id) {
		dev_warn(&rdev->dev, "no dedicated gpio can control\n");
		goto bypass_gpio;
	}

	if (id == 0) {
		gpiod_set_value(gpios->desc[id], 1);
		if (gpios->ndescs > 1)
			gpiod_set_value(gpios->desc[id+1], 1);
	}

bypass_gpio:
	ret = regmap_write(rdev->regmap, rdev->desc->vsel_reg, priv->volt_sel[id]);
	if (ret)
		return ret;

	priv->enable_flag |= BIT(id);
	return 0;
}

static int nt50358a_disable(struct regulator_dev *rdev)
{
	struct nt50358a_priv *priv = rdev_get_drvdata(rdev);
	struct gpio_descs *gpios = priv->enable_gpios;
	int id = rdev_get_id(rdev);

	if (!gpios || gpios->ndescs <= id) {
		dev_warn(&rdev->dev, "no dedicated gpio can control\n");
		goto bypass_gpio;
	}

	if (id == 1) {
		gpiod_set_value(gpios->desc[id], 0);
		gpiod_set_value(gpios->desc[id-1], 0);
	}

bypass_gpio:
	priv->enable_flag &= ~BIT(id);
	return 0;
}

static int nt50358a_is_enabled(struct regulator_dev *rdev)
{
	struct nt50358a_priv *priv = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);

	return !!(priv->enable_flag & BIT(id));
}

static const struct regulator_ops nt50358a_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = nt50358a_set_voltage_sel,
	.get_voltage_sel = nt50358a_get_voltage_sel,
	.enable = nt50358a_enable,
	.disable = nt50358a_disable,
	.is_enabled = nt50358a_is_enabled,
};

static const struct regulator_desc nt50358a_regulator_descs[] = {
	{
		.name = "DSVP",
		.ops = &nt50358a_regulator_ops,
		.of_match = of_match_ptr("DSVP"),
		.type = REGULATOR_VOLTAGE,
		.id = DSV_OUT_POS,
		.min_uV = MIN_UV,
		.uV_step = STEP_UV,
		.n_voltages = N_VOLTAGES,
		.owner = THIS_MODULE,
		.vsel_reg = NT50358A_REG_VOP,
		.vsel_mask = VOUT_MASK,
	},
	{
		.name = "DSVN",
		.ops = &nt50358a_regulator_ops,
		.of_match = of_match_ptr("DSVN"),
		.type = REGULATOR_VOLTAGE,
		.id = DSV_OUT_NEG,
		.min_uV = MIN_UV,
		.uV_step = STEP_UV,
		.n_voltages = N_VOLTAGES,
		.owner = THIS_MODULE,
		.vsel_reg = NT50358A_REG_VON,
		.vsel_mask = VOUT_MASK,
	},
};

static const struct regmap_config nt50358a_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = NT50358A_REG_APPS,
};

static int nt50358a_probe(struct i2c_client *i2c)
{
	struct nt50358a_priv *priv;
	struct regmap *regmap;
	int i;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &i2c->dev;
	/* bootloader will on, driver only reconfigure enable to all output high */
	priv->enable_flag = DSVALL_ENABLE;

	regmap = devm_regmap_init_i2c(i2c, &nt50358a_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&i2c->dev, "Failed to init regmap\n");
		return PTR_ERR(regmap);
	}

	priv->enable_gpios = devm_gpiod_get_array_optional(&i2c->dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->enable_gpios)) {
		dev_err(&i2c->dev, "Failed to get gpios\n");
		return PTR_ERR(priv->enable_gpios);
	}

	for (i = 0; i < DSV_OUT_MAX; i++) {
		const struct regulator_desc *desc = nt50358a_regulator_descs + i;
		struct regulator_config config = { .dev = &i2c->dev, .driver_data = priv,
						   .regmap = regmap, };
		struct regulator_dev *rdev;
		unsigned int val;
		int ret;

		/* initialize volt_sel variable */
		ret = regmap_read(regmap, desc->vsel_reg, &val);
		if (ret)
			return ret;

		priv->volt_sel[i] = val & desc->vsel_mask;

		rdev = devm_regulator_register(&i2c->dev, desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&i2c->dev, "Failed to register [%d] regulator\n", i);
			return PTR_ERR(rdev);
		}
	}

	return 0;
}

static const struct of_device_id __maybe_unused nt50358a_of_id[] = {
	{ .compatible = "novatek,nt50358a", },
	{ },
};
MODULE_DEVICE_TABLE(of, nt50358a_of_id);

static struct i2c_driver nt50358a_driver = {
	.driver = {
		.name = "nt50358a",
		.of_match_table = of_match_ptr(nt50358a_of_id),
	},
	.probe_new = nt50358a_probe,
};
module_i2c_driver(nt50358a_driver);

MODULE_AUTHOR("Wenbin Liu <wenbin.liu@mediatek.com>");
MODULE_DESCRIPTION("Novatek NT50358A Display Bias Driver");
MODULE_LICENSE("GPL v2");
