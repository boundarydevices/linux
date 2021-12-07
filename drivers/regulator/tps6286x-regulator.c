// SPDX-License-Identifier: GPL-2.0+
//
// tps6286x-regulator.c - TI TPS62864/6 step-down DC/DC converter
// Copyright (C) 2021 Boundary Devices LLC

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps6286x.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

/* Register definitions */
#define REG_VSET0		0
#define REG_VSET1		1
#define REG_CONTROL		3
#define REG_STATUS		5

/* Control register definitions */
#define SOFT_RESET		BIT(7)
#define FORCE_PWM_ENABLE	BIT(4)
#define OUT_DISCHARGE_ENABLE	BIT(3)
#define HICCUP_ENABLE		BIT(2)

/* Voltage registers definitions */
#define TPS6286X_BASE_VOLTAGE	400000
#define TPS6286X_N_VOLTAGES	256

/* tps 6286x chip information */
struct tps6286x_chip {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	int vsel_gpio;
	u8 voltage_reg_mask;
	int lru_index[2];
	int curr_vset_vsel[2];
	int curr_vset_id;
};

/*
 * find_voltage_set_register: Find new voltage configuration register
 * (VSET) id.
 * The finding of the new VSET register will be based on the LRU mechanism.
 * Each VSET register will have different voltage configured . This
 * Function will look if any of the VSET register have requested voltage set
 * or not.
 *     - If it is already there then it will make that register as most
 *       recently used and return as found so that caller need not to set
 *       the VSET register but need to set the proper gpios to select this
 *       VSET register.
 *     - If requested voltage is not found then it will use the least
 *       recently mechanism to get new VSET register for new configuration
 *       and will return not_found so that caller need to set new VSET
 *       register and then gpios (both).
 */
static bool find_voltage_set_register(struct tps6286x_chip *tps,
				      int req_vsel, int *vset_reg_id)
{
	int i;
	bool found = false;
	int new_vset_reg = tps->lru_index[1];
	int found_index = 1;

	for (i = 0; i < 2; ++i) {
		if (tps->curr_vset_vsel[tps->lru_index[i]] == req_vsel) {
			new_vset_reg = tps->lru_index[i];
			found_index = i;
			found = true;
			goto update_lru_index;
		}
	}

update_lru_index:
	for (i = found_index; i > 0; i--)
		tps->lru_index[i] = tps->lru_index[i - 1];

	tps->lru_index[0] = new_vset_reg;
	*vset_reg_id = new_vset_reg;
	return found;
}

static int tps6286x_dcdc_get_voltage_sel(struct regulator_dev *dev)
{
	struct tps6286x_chip *tps = rdev_get_drvdata(dev);
	int vsel;
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, REG_VSET0 + tps->curr_vset_id, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d read failed with err %d\n",
			__func__, REG_VSET0 + tps->curr_vset_id, ret);
		return ret;
	}
	vsel = (int)data & tps->voltage_reg_mask;

	return vsel;
}

static int tps6286x_dcdc_set_voltage_sel(struct regulator_dev *dev,
					 unsigned selector)
{
	struct tps6286x_chip *tps = rdev_get_drvdata(dev);
	int ret;
	bool found = false;
	int new_vset_id = tps->curr_vset_id;

	/*
	 * If gpios are available to select the VSET register then least
	 * recently used register for new configuration.
	 */
	if (gpio_is_valid(tps->vsel_gpio))
		found = find_voltage_set_register(tps, selector, &new_vset_id);

	if (!found) {
		ret = regmap_update_bits(tps->regmap, REG_VSET0 + new_vset_id,
					 tps->voltage_reg_mask, selector);
		if (ret < 0) {
			dev_err(tps->dev,
				"%s(): register %d update failed with err %d\n",
				 __func__, REG_VSET0 + new_vset_id, ret);
			return ret;
		}
		tps->curr_vset_id = new_vset_id;
		tps->curr_vset_vsel[new_vset_id] = selector;
	}

	/* Select proper VSET register vio gpios */
	if (gpio_is_valid(tps->vsel_gpio))
		gpio_set_value_cansleep(tps->vsel_gpio, new_vset_id);

	return 0;
}

static int tps6286x_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct tps6286x_chip *tps = rdev_get_drvdata(rdev);
	int val, ret;

	/* Enable force PWM mode in FAST mode only. */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = FORCE_PWM_ENABLE;
		break;

	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;

	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(tps->regmap, REG_CONTROL,
				 FORCE_PWM_ENABLE, val);
	if (ret < 0)
		dev_err(tps->dev, "%s(): register %d update failed, err %d\n",
			__func__, REG_CONTROL, ret);

	return ret;
}

static unsigned int tps6286x_get_mode(struct regulator_dev *rdev)
{
	struct tps6286x_chip *tps = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, REG_CONTROL, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d read failed, err %d\n",
			__func__, REG_CONTROL, ret);
		return ret;
	}
	return (data & FORCE_PWM_ENABLE) ?
		REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static const struct regulator_ops tps6286x_dcdc_ops = {
	.get_voltage_sel	= tps6286x_dcdc_get_voltage_sel,
	.set_voltage_sel	= tps6286x_dcdc_set_voltage_sel,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_mode		= tps6286x_set_mode,
	.get_mode		= tps6286x_get_mode,
};

static const struct regmap_config tps6286x_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= REG_STATUS,
	.cache_type		= REGCACHE_RBTREE,
};

static struct tps6286x_regulator_platform_data *
	of_get_tps6286x_platform_data(struct device *dev,
				      const struct regulator_desc *desc)
{
	struct tps6286x_regulator_platform_data *pdata;
	struct device_node *np = dev->of_node;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->reg_init_data = of_get_regulator_init_data(dev, dev->of_node,
							  desc);
	if (!pdata->reg_init_data) {
		dev_err(dev, "Not able to get OF regulator init data\n");
		return NULL;
	}

	pdata->vsel_gpio = of_get_named_gpio(np, "vsel-gpio", 0);

	if (of_find_property(np, "ti,vsel-state-high", NULL))
		pdata->vsel_def_state = 1;

	return pdata;
}

#if defined(CONFIG_OF)
static const struct of_device_id tps6286x_of_match[] = {
	{ .compatible = "ti,tps6286x" },
	{},
};
MODULE_DEVICE_TABLE(of, tps6286x_of_match);
#endif

static int tps6286x_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct regulator_config config = { };
	struct tps6286x_regulator_platform_data *pdata;
	struct regulator_dev *rdev;
	struct tps6286x_chip *tps;
	int ret;
	int i;
	int chip_id;

	pdata = dev_get_platdata(&client->dev);

	tps = devm_kzalloc(&client->dev, sizeof(*tps), GFP_KERNEL);
	if (!tps)
		return -ENOMEM;

	tps->desc.name = client->name;
	tps->desc.id = 0;
	tps->desc.ops = &tps6286x_dcdc_ops;
	tps->desc.type = REGULATOR_VOLTAGE;
	tps->desc.owner = THIS_MODULE;
	tps->desc.uV_step = 5000;

	if (client->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_device(of_match_ptr(tps6286x_of_match),
				&client->dev);
		if (!match) {
			dev_err(&client->dev, "Error: No device match found\n");
			return -ENODEV;
		}
		chip_id = (int)(long)match->data;
		if (!pdata)
			pdata = of_get_tps6286x_platform_data(&client->dev,
							      &tps->desc);
	} else if (id) {
		chip_id = id->driver_data;
	} else {
		dev_err(&client->dev, "No device tree match or id table match found\n");
		return -ENODEV;
	}

	if (!pdata) {
		dev_err(&client->dev, "%s(): Platform data not found\n",
						__func__);
		return -EIO;
	}

	tps->vsel_gpio = pdata->vsel_gpio;
	tps->desc.min_uV = TPS6286X_BASE_VOLTAGE;
	tps->desc.n_voltages = TPS6286X_N_VOLTAGES;
	tps->voltage_reg_mask = 0xFF;
	tps->dev = &client->dev;

	tps->regmap = devm_regmap_init_i2c(client, &tps6286x_regmap_config);
	if (IS_ERR(tps->regmap)) {
		ret = PTR_ERR(tps->regmap);
		dev_err(&client->dev,
			"%s(): regmap allocation failed with err %d\n",
			__func__, ret);
		return ret;
	}
	i2c_set_clientdata(client, tps);

	tps->curr_vset_id = pdata->vsel_def_state;
	tps->lru_index[0] = tps->curr_vset_id;

	if (gpio_is_valid(tps->vsel_gpio)) {
		int gpio_flags;
		gpio_flags = (pdata->vsel_def_state) ?
				GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		ret = devm_gpio_request_one(&client->dev, tps->vsel_gpio,
				gpio_flags, "tps6286x-vsel");
		if (ret) {
			dev_err(&client->dev,
				"%s(): Could not obtain vsel GPIO %d: %d\n",
				__func__, tps->vsel_gpio, ret);
			return ret;
		}

		/*
		 * Initialize the lru index with vset_reg id
		 * The index 0 will be most recently used and
		 * set with the tps->curr_vset_id */
		for (i = 0; i < 1; ++i)
			tps->lru_index[i] = i;
		tps->lru_index[0] = tps->curr_vset_id;
		tps->lru_index[tps->curr_vset_id] = 0;
	}

	config.dev = &client->dev;
	config.init_data = pdata->reg_init_data;
	config.driver_data = tps;
	config.of_node = client->dev.of_node;

	/* Register the regulators */
	rdev = devm_regulator_register(&client->dev, &tps->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(tps->dev,
			"%s(): regulator register failed with err %s\n",
			__func__, id->name);
		return PTR_ERR(rdev);
	}

	tps->rdev = rdev;

	return 0;
}

static const struct i2c_device_id tps6286x_id[] = {
	{.name = "tps6286x"},
	{},
};

MODULE_DEVICE_TABLE(i2c, tps6286x_id);

static struct i2c_driver tps6286x_i2c_driver = {
	.driver = {
		.name = "tps6286x",
		.of_match_table = of_match_ptr(tps6286x_of_match),
	},
	.probe = tps6286x_probe,
	.id_table = tps6286x_id,
};

static int __init tps6286x_init(void)
{
	return i2c_add_driver(&tps6286x_i2c_driver);
}
subsys_initcall(tps6286x_init);

static void __exit tps6286x_cleanup(void)
{
	i2c_del_driver(&tps6286x_i2c_driver);
}
module_exit(tps6286x_cleanup);

MODULE_AUTHOR("Boundary Devices <support@boundarydevices.com>");
MODULE_DESCRIPTION("TPS6236x voltage regulator driver");
MODULE_LICENSE("GPL v2");
