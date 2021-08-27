// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2021 Boundary Devices LLC
//
// Battery charger driver for MAXIM 77975/77976 charger/power-supply.

#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>

/* TOP_INT */
#define	MAX77975_TOP_INT			0x03

/* CHG_INT */
#define	MAX77975_CHG_INT			0x10

/* CHG_INT_OK */
#define	MAX77975_CHG_INT_OK			0x12
#define	MAX77975_CHG_INT_OK_CHGIN		BIT(6)

/* CHG_DTLS_01 */
#define MAX77975_CHG_DTLS_01			0x14
#define MAX77975_CHG_DTLS_MASK			GENMASK(3, 0)

/* CHG_CNFG_00 register */
#define MAX77975_CHG_CNFG_00			0x16
#define MAX77975_CHARGER_ENABLED		0x05
#define MAX77975_CHARGER_DISABLED		0x04
#define MAX77975_CHARGER_CHG_EN_MASK		GENMASK(3, 0)

/* CHG_CNFG_02 register */
#define MAX77975_CHG_CNFG_02			0x18
#define MAX77975_FAST_ILIM_MAX			5500000
#define MAX77975_FAST_ILIM_MASK			GENMASK(6, 0)
#define MAX77975_FAST_ILIM_UA_TO_VAL(x)		(((x - 100000)/50000) + 1)
#define MAX77975_FAST_ILIM_VAL_TO_UA(x)		(100000 + ((x - 1)*50000))

/* CHG_CNFG_04 register */
#define MAX77975_CHG_CNFG_04			0x1A
#define MAX77975_VCHG_MAX			4460000
#define MAX77975_VCHG_MASK			GENMASK(4, 0)
#define MAX77975_VCHG_UV_TO_VAL(x)		((x - 4150000)/10000)
#define MAX77975_VCHG_VAL_TO_UV(x)		(4150000 + (x*10000))

/* CHG_CNFG_06 register */
#define MAX77975_CHG_CNFG_06			0x1C
#define MAX77975_CHGPROT_MASK			GENMASK(3, 2)

/* CHG_CNFG_09 register */
#define MAX77975_CHG_CNFG_09			0x1F
#define MAX77975_ILIM_MAX			3200000
#define MAX77975_ILIM_MASK			GENMASK(5, 0)
#define MAX77975_ILIM_UA_TO_VAL(x)		(((x - 100000)/50000) + 1)
#define MAX77975_ILIM_VAL_TO_UA(x)		(100000 + ((x - 1)*50000))

/* CHG_CNFG_12 register */
#define MAX77975_CHG_CNFG_12			0x22
#define MAX77975_VCHGIN_MIN_SHIFT(_val)		((_val) << 4)
#define MAX77975_VCHGIN_MIN_MASK		GENMASK(5, 4)

/* STAT_CNFG register */
#define MAX77975_STAT_CNFG			0x24

struct max77975_charger_data {
	struct regmap *map;
	struct device *dev;
	struct power_supply *psy;
	struct gpio_desc *gpio_suspend;
	struct gpio_desc *gpio_disbatt;
	bool is_charging;
};

static enum power_supply_property max77975_charger_properties[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static const unsigned int max77975_charger_vchgin_min_table[] = {
	4500000, 4600000, 4700000, 4850000
};

static int max77975_charger_set_vchgin_min(struct max77975_charger_data *chg,
					   unsigned int val)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(max77975_charger_vchgin_min_table); i++) {
		if (val == max77975_charger_vchgin_min_table[i]) {
			ret = regmap_update_bits(chg->map,
					MAX77975_CHG_CNFG_12,
					MAX77975_VCHGIN_MIN_MASK,
					MAX77975_VCHGIN_MIN_SHIFT(i));
			if (ret)
				return ret;

			return 0;
		}
	}

	return -EINVAL;
}

static int max77975_charger_unlock(struct max77975_charger_data *chg)
{
	return regmap_update_bits(chg->map, MAX77975_CHG_CNFG_06,
				  MAX77975_CHGPROT_MASK, MAX77975_CHGPROT_MASK);
}

static int max77975_charger_is_enabled(struct max77975_charger_data *chg)
{
	int reg;

	regmap_read(chg->map, MAX77975_CHG_CNFG_00, &reg);

	return (reg & MAX77975_CHARGER_CHG_EN_MASK) == MAX77975_CHARGER_ENABLED;
}

static int max77975_charger_is_chgin_valid(struct max77975_charger_data *chg)
{
	int reg;

	regmap_read(chg->map, MAX77975_CHG_INT_OK, &reg);

	return (reg & MAX77975_CHG_INT_OK_CHGIN);
}

static int max77975_charger_enable(struct max77975_charger_data *chg)
{
	int ret = 0;

	if (!chg->is_charging) {
		max77975_charger_unlock(chg);
		ret = regmap_update_bits(chg->map,
					 MAX77975_CHG_CNFG_00,
					 MAX77975_CHARGER_CHG_EN_MASK,
					 MAX77975_CHARGER_ENABLED);
		if (ret)
			dev_err(chg->dev, "unable to enable: %d\n", ret);
		chg->is_charging = true;
		power_supply_changed(chg->psy);
	}

	return ret;
}

static int max77975_charger_disable(struct max77975_charger_data *chg)
{
	int ret = 0;

	if (chg->is_charging) {
		max77975_charger_unlock(chg);
		ret = regmap_update_bits(chg->map,
					 MAX77975_CHG_CNFG_00,
					 MAX77975_CHARGER_CHG_EN_MASK,
					 MAX77975_CHARGER_DISABLED);
		if (ret)
			dev_err(chg->dev, "unable to disable: %d\n", ret);
		chg->is_charging = false;
		power_supply_changed(chg->psy);
	}

	return ret;
}

static int max77975_charger_get_charger_state(struct max77975_charger_data *chg)
{
	int state;
	int reg;

	regmap_read(chg->map, MAX77975_CHG_DTLS_01, &reg);
	dev_dbg(chg->dev, "get charger state CHG_DTLS: 0x%02x\n", reg);
	switch (reg & MAX77975_CHG_DTLS_MASK) {
	case 0x0:
	case 0x1:
	case 0x2:
		state = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x3:
	case 0x4:
		state = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x5:
	case 0x6:
	case 0x7:
		state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		state = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return state;
}

static int max77975_charger_get_charge_current(struct max77975_charger_data *chg)
{
	int reg;

	regmap_read(chg->map, MAX77975_CHG_CNFG_02, &reg);
	dev_dbg(chg->dev, "get charger state CHG_CNFG_02: 0x%02x\n", reg);
	reg &= MAX77975_FAST_ILIM_MASK;
	dev_dbg(chg->dev, "get charge current: %duA\n",
		MAX77975_FAST_ILIM_VAL_TO_UA(reg));

	return MAX77975_FAST_ILIM_VAL_TO_UA(reg);
}

static int max77975_charger_set_charge_current(struct max77975_charger_data *chg,
					       int val)
{
	if (val > MAX77975_FAST_ILIM_MAX)
		return -EINVAL;

	return regmap_update_bits(chg->map,
				  MAX77975_CHG_CNFG_02,
				  MAX77975_FAST_ILIM_MASK,
				  MAX77975_FAST_ILIM_UA_TO_VAL(val));
}

static int max77975_charger_get_input_current(struct max77975_charger_data *chg)
{
	int reg;

	regmap_read(chg->map, MAX77975_CHG_CNFG_09, &reg);
	dev_dbg(chg->dev, "get charger state CHG_CNFG_09: 0x%02x\n", reg);
	reg &= MAX77975_ILIM_MASK;
	dev_dbg(chg->dev, "get charge current: %duA\n",
		MAX77975_ILIM_VAL_TO_UA(reg));

	return MAX77975_ILIM_VAL_TO_UA(reg);
}

static int max77975_charger_set_input_current(struct max77975_charger_data *chg,
					      int val)
{
	if (val > MAX77975_ILIM_MAX)
		return -EINVAL;

	return regmap_update_bits(chg->map,
				  MAX77975_CHG_CNFG_09,
				  MAX77975_ILIM_MASK,
				  MAX77975_ILIM_UA_TO_VAL(val));
}

static irqreturn_t max77975_charger_irq(int irq, void *data)
{
	struct max77975_charger_data *chg = data;
	int reg, reg2;

	regmap_read(chg->map, MAX77975_TOP_INT, &reg);
	regmap_read(chg->map, MAX77975_CHG_INT, &reg2);
	dev_dbg(chg->dev, "%s TODO TOP 0x%02x CHG 0x%02x\n",
		__func__, reg & 0xFF, reg2 & 0xFF);

	return IRQ_HANDLED;
}

static int max77975_charger_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct max77975_charger_data *chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		if (max77975_charger_is_chgin_valid(chg))
			val->intval = POWER_SUPPLY_TYPE_USB;
		break;

	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = max77975_charger_is_enabled(chg);
		break;

	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77975_charger_get_charger_state(chg);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = max77975_charger_get_charge_current(chg);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		val->intval = max77975_charger_get_input_current(chg);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (max77975_charger_get_charger_state(chg) ==
					POWER_SUPPLY_STATUS_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max77975_charger_set_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 const union power_supply_propval *val)
{
	struct max77975_charger_data *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;

	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (val->intval && max77975_charger_is_chgin_valid(chg))
			max77975_charger_enable(chg);
		else
			max77975_charger_disable(chg);
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = max77975_charger_set_charge_current(chg, val->intval);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		ret = max77975_charger_set_input_current(chg, val->intval);
		break;

	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static const struct power_supply_desc max77975_supply_desc = {
	.name		= "max77975",
	.type		= POWER_SUPPLY_TYPE_USB,
	.get_property	= max77975_charger_get_property,
	.set_property	= max77975_charger_set_property,
	.properties	= max77975_charger_properties,
	.num_properties	= ARRAY_SIZE(max77975_charger_properties),
};

static const struct regmap_config max77975_regmap_config = {
	.name		= "max77975",
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= MAX77975_STAT_CNFG,
};

static int max77975_charger_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct power_supply_config pscfg = {};
	struct max77975_charger_data *chg;
	struct device *dev;
	unsigned int prop;
	int ret;

	dev = &client->dev;

	chg = devm_kzalloc(dev, sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	i2c_set_clientdata(client, chg);

	chg->dev = dev;
	chg->is_charging = false;
	chg->map = devm_regmap_init_i2c(client, &max77975_regmap_config);
	if (IS_ERR(chg->map)) {
		dev_err(dev, "Unable to initialise I2C Regmap\n");
		return PTR_ERR(chg->map);
	}

	chg->gpio_suspend = devm_gpiod_get_optional(dev, "suspend",
						    GPIOD_OUT_LOW);
	if (IS_ERR(chg->gpio_suspend)) {
		if (PTR_ERR(chg->gpio_suspend) != -EPROBE_DEFER)
			dev_err(dev, "Suspend GPIO request failed\n");
		return PTR_ERR(chg->gpio_suspend);
	}

	chg->gpio_disbatt = devm_gpiod_get_optional(dev, "disbatt",
						    GPIOD_OUT_LOW);
	if (IS_ERR(chg->gpio_disbatt)) {
		if (PTR_ERR(chg->gpio_disbatt) != -EPROBE_DEFER)
			dev_err(dev, "Disable GPIO request failed\n");
		return PTR_ERR(chg->gpio_disbatt);
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					max77975_charger_irq,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"max77975-irq", chg);
	if (ret)
		return ret;

	pscfg.of_node = dev->of_node;
	pscfg.drv_data = chg;
	chg->psy = devm_power_supply_register(dev, &max77975_supply_desc, &pscfg);
	if (IS_ERR(chg->psy))
		return PTR_ERR(chg->psy);

	max77975_charger_unlock(chg);

	ret = of_property_read_u32(dev->of_node,
				  "input-voltage-min-microvolt", &prop);
	if (ret == 0) {
		ret = max77975_charger_set_vchgin_min(chg, prop);
		if (ret)
			return ret;
	}

	ret = of_property_read_u32(dev->of_node,
				  "input-current-limit-microamp", &prop);
	if (ret == 0) {
		ret = max77975_charger_set_input_current(chg, prop);
		if (ret)
			return ret;
	}

	dev_info(dev, "probe successful!\n");

	return 0;
}

static void max77975_charger_remove(struct i2c_client *client)
{
	struct max77975_charger_data *chg = i2c_get_clientdata(client);

	max77975_charger_disable(chg);
}

static const struct of_device_id max77975_match_ids[] = {
	{ .compatible = "maxim,max77975", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, max77975_match_ids);

static struct i2c_driver max77975_charger_i2c_driver = {
	.driver = {
		.name = "max77975-charger",
		.of_match_table = max77975_match_ids
	},
	.probe = max77975_charger_probe,
	.remove = max77975_charger_remove,
};
module_i2c_driver(max77975_charger_i2c_driver);

MODULE_DESCRIPTION("MAXIM 77975/77976 charger driver");
MODULE_AUTHOR("Boundary Devices <support@boundarydevices.com>");
MODULE_LICENSE("GPL v2");
