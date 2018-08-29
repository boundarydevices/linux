/*
 * drivers/amlogic/pinctrl/pinctrl-meson.c
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

/*
 * The available pins are organized in banks (A,B,C,D,E,X,Y,Z,AO,
 * BOOT,CARD for meson6, X,Y,DV,H,Z,AO,BOOT,CARD for meson8 and
 * X,Y,DV,H,AO,BOOT,CARD,DIF for meson8b) and each bank has a
 * variable number of pins.
 *
 * The AO bank is special because it belongs to the Always-On power
 * domain which can't be powered off; the bank also uses a set of
 * registers different from the other banks.
 *
 * For each pin controller there are 4 different register ranges that
 * control the following properties of the pins:
 * of the pins:
 *  1) pin muxing
 *  2) pull enable/disable
 *  3) pull up/down
 *  4) GPIO direction, output value, input value
 *
 * In some cases the register ranges for pull enable and pull
 * direction are the same and thus there are only 3 register ranges.
 *
 * For the pull and GPIO configuration every bank uses a contiguous
 * set of bits in the register sets described above; the same register
 * can be shared by more banks with different offsets.
 *
 * In addition to this there are some registers shared between all
 * banks that control the IRQ functionality. This feature is not
 * supported at the moment by the driver.
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../../pinctrl/pinctrl-utils.h"
#include "pinctrl-meson.h"

/**
 * meson_get_bank() - find the bank containing a given pin
 *
 * @pc:	the pinctrl instance
 * @pin:	the pin number
 * @bank:	the found bank
 *
 * Return:	0 on success, a negative value on error
 */
static int meson_get_bank(struct meson_pinctrl *pc, unsigned int pin,
			  struct meson_bank **bank)
{
	int i;

	for (i = 0; i < pc->data->num_banks; i++) {
		if (pin >= pc->data->banks[i].first &&
		    pin <= pc->data->banks[i].last) {
			*bank = &pc->data->banks[i];
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * meson_calc_reg_and_bit() - calculate register and bit for a pin
 *
 * @bank:	the bank containing the pin
 * @pin:	the pin number
 * @reg_type:	the type of register needed (pull-enable, pull, etc...)
 * @reg:	the computed register offset
 * @bit:	the computed bit
 */
static void meson_calc_reg_and_bit(struct meson_bank *bank, unsigned int pin,
				   enum meson_reg_type reg_type,
				   unsigned int *reg, unsigned int *bit)
{
	struct meson_reg_desc *desc = &bank->regs[reg_type];

	*reg = desc->reg * 4;
	*bit = desc->bit + pin - bank->first;
}

static int meson_get_groups_count(struct pinctrl_dev *pcdev)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	return pc->data->num_groups;
}

static const char *meson_get_group_name(struct pinctrl_dev *pcdev,
					unsigned int selector)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	return pc->data->groups[selector].name;
}

static int meson_get_group_pins(struct pinctrl_dev *pcdev,
	unsigned int selector,  const unsigned int **pins,
	unsigned int *num_pins)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	*pins = pc->data->groups[selector].pins;
	*num_pins = pc->data->groups[selector].num_pins;

	return 0;
}

static void meson_pin_dbg_show(struct pinctrl_dev *pcdev, struct seq_file *s,
			       unsigned int offset)
{
	seq_printf(s, " %s", dev_name(pcdev->dev));
}

static const struct pinctrl_ops meson_pctrl_ops = {
	.get_groups_count	= meson_get_groups_count,
	.get_group_name		= meson_get_group_name,
	.get_group_pins		= meson_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_all,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= meson_pin_dbg_show,
};

int meson_pmx_get_funcs_count(struct pinctrl_dev *pcdev)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	return pc->data->num_funcs;
}

const char *meson_pmx_get_func_name(struct pinctrl_dev *pcdev,
					   unsigned int selector)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	return pc->data->funcs[selector].name;
}

int meson_pmx_get_groups(struct pinctrl_dev *pcdev,
	unsigned int selector, const char * const **groups,
	unsigned int * const num_groups)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);

	*groups = pc->data->funcs[selector].groups;
	*num_groups = pc->data->funcs[selector].num_groups;

	return 0;
}

int meson_pinconf_common_set(struct meson_pinctrl *pc, unsigned int pin,
					enum pin_config_param param, u16 arg)
{
	struct meson_bank *bank;
	unsigned int reg, bit;
	int ret;

	ret = meson_get_bank(pc, pin, &bank);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		dev_dbg(pc->dev, "pin %u: disable bias\n", pin);

		meson_calc_reg_and_bit(bank, pin, REG_PULLEN,
					&reg, &bit);
		ret = regmap_update_bits(pc->reg_pullen, reg,
					 BIT(bit), 0);
		if (ret)
			return ret;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		dev_dbg(pc->dev, "pin %u: enable pull-up\n", pin);

		meson_calc_reg_and_bit(bank, pin, REG_PULLEN,
					   &reg, &bit);
		ret = regmap_update_bits(pc->reg_pullen, reg,
					 BIT(bit), BIT(bit));
		if (ret)
			return ret;

		meson_calc_reg_and_bit(bank, pin, REG_PULL, &reg, &bit);
		ret = regmap_update_bits(pc->reg_pull, reg,
					 BIT(bit), BIT(bit));
		if (ret)
			return ret;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		dev_dbg(pc->dev, "pin %u: enable pull-down\n", pin);

		meson_calc_reg_and_bit(bank, pin, REG_PULLEN,
					   &reg, &bit);
		ret = regmap_update_bits(pc->reg_pullen, reg,
					 BIT(bit), BIT(bit));
		if (ret)
			return ret;

		meson_calc_reg_and_bit(bank, pin, REG_PULL, &reg, &bit);
		ret = regmap_update_bits(pc->reg_pull, reg,
					 BIT(bit), 0);
		if (ret)
			return ret;
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		dev_dbg(pc->dev, "pin %u: enable input\n", pin);

		meson_calc_reg_and_bit(bank, pin, REG_DIR,
					   &reg, &bit);
		ret = regmap_update_bits(pc->reg_gpio, reg,
					 BIT(bit), BIT(bit));
		if (ret)
			return ret;
		break;
	case PIN_CONFIG_OUTPUT:
		dev_dbg(pc->dev, "pin %u: output %s\n", pin,
				arg ? "high" : "low");
		meson_calc_reg_and_bit(bank, pin, REG_DIR,
					   &reg, &bit);
		ret = regmap_update_bits(pc->reg_gpio, reg,
				 BIT(bit), 0);
		if (ret)
			return ret;
		meson_calc_reg_and_bit(bank, pin, REG_OUT,
					   &reg, &bit);
		ret = regmap_update_bits(pc->reg_gpio, reg,
				 BIT(bit), arg ? BIT(bit) : 0);
		if (ret)
			return ret;
	default:
		return -ENOTSUPP;
	}

	return 0;
}
static int meson_pinconf_set(struct pinctrl_dev *pcdev, unsigned int pin,
			     unsigned long *configs, unsigned int num_configs)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	enum pin_config_param param;
	int i, ret;
	u16 arg;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		ret = meson_pinconf_common_set(pc, pin, param, arg);
		if (ret)
			return ret;
	}

	return 0;
}

static int meson_pinconf_get_pull(struct meson_pinctrl *pc, unsigned int pin)
{
	struct meson_bank *bank;
	unsigned int reg, bit, val;
	int ret, conf;

	ret = meson_get_bank(pc, pin, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, pin, REG_PULLEN, &reg, &bit);

	ret = regmap_read(pc->reg_pullen, reg, &val);
	if (ret)
		return ret;

	if (!(val & BIT(bit))) {
		conf = PIN_CONFIG_BIAS_DISABLE;
	} else {
		meson_calc_reg_and_bit(bank, pin, REG_PULL, &reg, &bit);

		ret = regmap_read(pc->reg_pull, reg, &val);
		if (ret)
			return ret;

		if (val & BIT(bit))
			conf = PIN_CONFIG_BIAS_PULL_UP;
		else
			conf = PIN_CONFIG_BIAS_PULL_DOWN;
	}

	return conf;
}

static int meson_pinconf_get_pio(struct meson_pinctrl *pc, unsigned int pin,
					u16 *arg)
{
	struct meson_bank *bank;
	unsigned int reg, bit, val;
	int ret, conf;

	ret = meson_get_bank(pc, pin, &bank);
	if (ret)
		return ret;
	meson_calc_reg_and_bit(bank, pin, REG_DIR, &reg, &bit);

	ret = regmap_read(pc->reg_gpio, reg, &val);
	if (ret)
		return ret;
	if (val & BIT(bit)) {
		conf = PIN_CONFIG_INPUT_ENABLE;
		*arg = 1;
	} else {
		meson_calc_reg_and_bit(bank, pin, REG_OUT, &reg, &bit);

		ret = regmap_read(pc->reg_gpio, reg, &val);
		if (ret)
			return ret;

		if (val & BIT(bit))
			*arg = 1;
		else
			*arg = 0;
		conf = PIN_CONFIG_OUTPUT;
	}

	return conf;
}

int meson_pinconf_common_get(struct meson_pinctrl *pc, unsigned int pin,
				enum pin_config_param param, u16 *arg)
{
	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		if (meson_pinconf_get_pull(pc, pin) == param)
			*arg = 1;
		else
			return -EINVAL;
		break;
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		if (meson_pinconf_get_pio(pc, pin, arg) != param)
			return -EINVAL;
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static int meson_pinconf_get(struct pinctrl_dev *pcdev, unsigned int pin,
			     unsigned long *config)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	enum pin_config_param param = pinconf_to_config_param(*config);
	u16 arg;
	int ret;

	ret = meson_pinconf_common_get(pc, pin, param, &arg);
	if (ret)
		return ret;

	*config = pinconf_to_config_packed(param, arg);
	dev_dbg(pc->dev, "pinconf for pin %u is %lu\n", pin, *config);

	return 0;
}

static int meson_pinconf_group_set(struct pinctrl_dev *pcdev,
	unsigned int num_group, unsigned long *configs,
	unsigned int num_configs)
{
	struct meson_pinctrl *pc = pinctrl_dev_get_drvdata(pcdev);
	struct meson_pmx_group *group = &pc->data->groups[num_group];
	int i;

	dev_dbg(pc->dev, "set pinconf for group %s\n", group->name);

	for (i = 0; i < group->num_pins; i++) {
		meson_pinconf_set(pcdev, group->pins[i], configs,
				  num_configs);
	}

	return 0;
}

static const struct pinconf_ops meson_pinconf_ops = {
	.pin_config_get		= meson_pinconf_get,
	.pin_config_set		= meson_pinconf_set,
	.pin_config_group_set	= meson_pinconf_group_set,
	.is_generic		= true,
};

static int meson_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	unsigned int reg, bit;
	struct meson_bank *bank;
	int ret;

	ret = meson_get_bank(pc, gpio, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_DIR, &reg, &bit);

	return regmap_update_bits(pc->reg_gpio, reg, BIT(bit), BIT(bit));
}

static int meson_gpio_direction_output(struct gpio_chip *chip,
	unsigned int gpio, int value)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	unsigned int reg, bit;
	struct meson_bank *bank;
	int ret;

	ret = meson_get_bank(pc, gpio, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_OUT, &reg, &bit);
	ret = regmap_update_bits(pc->reg_gpio, reg, BIT(bit),
				  value ? BIT(bit) : 0);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_DIR, &reg, &bit);
	return regmap_update_bits(pc->reg_gpio, reg, BIT(bit), 0);
}

static void meson_gpio_set(struct gpio_chip *chip, unsigned int gpio,
	int value)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	unsigned int reg, bit;
	struct meson_bank *bank;
	int ret;

	ret = meson_get_bank(pc, gpio, &bank);
	if (ret)
		return;

	meson_calc_reg_and_bit(bank, gpio, REG_OUT, &reg, &bit);
	regmap_update_bits(pc->reg_gpio, reg, BIT(bit),
			   value ? BIT(bit) : 0);
}

static int meson_gpio_pull_set(struct gpio_chip *chip, unsigned int gpio,
	int value)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	unsigned int reg, bit;
	struct meson_bank *bank;
	int ret;

	if ((value != GPIOD_PULL_DIS) && (value != GPIOD_PULL_DOWN)
		&& (value != GPIOD_PULL_UP))
		return -EINVAL;

	ret = meson_get_bank(pc, gpio, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_PULLEN,
				&reg, &bit);
	ret = regmap_update_bits(pc->reg_pullen, reg,
				BIT(bit),
				(value == GPIOD_PULL_DIS) ? 0 : BIT(bit));
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_PULL, &reg, &bit);
	ret = regmap_update_bits(pc->reg_pull, reg,
				BIT(bit),
				(value == GPIOD_PULL_DOWN) ? 0 : BIT(bit));
	if (ret)
		return ret;

	return 0;
}

static int meson_gpio_get(struct gpio_chip *chip, unsigned int gpio)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	unsigned int reg, bit, val;
	struct meson_bank *bank;
	int ret;

	ret = meson_get_bank(pc, gpio, &bank);
	if (ret)
		return ret;

	meson_calc_reg_and_bit(bank, gpio, REG_IN, &reg, &bit);
	ret = regmap_read(pc->reg_gpio, reg, &val);
	if (ret)
		return ret;

	return !!(val & BIT(bit));
}

static int meson_gpio_to_irq(struct gpio_chip *chip, unsigned int gpio)
{
	struct meson_pinctrl *pc = gpiochip_get_data(chip);
	struct meson_bank *bank;
	struct irq_fwspec fwspec;
	int hwirq;

	if (meson_get_bank(pc, gpio, &bank))
		return -EINVAL;

	if (bank->irq < 0) {
		dev_warn(pc->dev, "no support irq for pin[%d]\n", gpio);
		return -EINVAL;
	}

	if (!pc->of_irq) {
		dev_err(pc->dev, "invalid device node of gpio INTC\n");
		return -EINVAL;
	}

	hwirq = gpio - bank->first + bank->irq;

	fwspec.fwnode = of_node_to_fwnode(pc->of_irq);
	fwspec.param_count = 2;
	fwspec.param[0] = hwirq;
	fwspec.param[1] = IRQ_TYPE_NONE;

	return irq_create_fwspec_mapping(&fwspec);
}

static int meson_gpiolib_register(struct meson_pinctrl *pc)
{
	int ret;

	pc->chip.label = pc->data->name;
	pc->chip.parent = pc->dev;
	pc->chip.request = gpiochip_generic_request;
	pc->chip.free = gpiochip_generic_free;
	pc->chip.direction_input = meson_gpio_direction_input;
	pc->chip.direction_output = meson_gpio_direction_output;
	pc->chip.get = meson_gpio_get;
	pc->chip.set = meson_gpio_set;
	pc->chip.to_irq = meson_gpio_to_irq;
	pc->chip.set_pull = meson_gpio_pull_set;
	pc->chip.base = -1;
	pc->chip.ngpio = pc->data->num_pins;
	pc->chip.can_sleep = false;
	pc->chip.of_node = pc->of_node;
	pc->chip.of_gpio_n_cells = 2;

	ret = gpiochip_add_data(&pc->chip, pc);
	if (ret) {
		dev_err(pc->dev, "can't add gpio chip %s\n",
			pc->data->name);
		goto fail;
	}

	ret = gpiochip_add_pin_range(&pc->chip, dev_name(pc->dev),
					0, 0, pc->chip.ngpio);
	if (ret) {
	dev_err(pc->dev, "can't add pin range\n");
	goto fail;
	}

	return 0;
fail:
	gpiochip_remove(&pc->chip);

	return ret;
}

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct regmap *meson_map_resource(struct meson_pinctrl *pc,
					 struct device_node *node, char *name)
{
	struct resource res;
	void __iomem *base;
	int i;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(pc->dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	meson_regmap_config.max_register = resource_size(&res) - 4;
	meson_regmap_config.name = devm_kasprintf(pc->dev, GFP_KERNEL,
						  "%s-%s", node->name,
						  name);
	if (!meson_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(pc->dev, base, &meson_regmap_config);
}

static int meson_pinctrl_parse_dt(struct meson_pinctrl *pc,
				  struct device_node *node)
{
	struct device_node *np;
	struct device_node *gpio_np = NULL;

	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;
		if (gpio_np) {
			dev_err(pc->dev, "multiple gpio nodes\n");
			return -EINVAL;
		}
		gpio_np = np;
	}

	if (!gpio_np) {
		dev_err(pc->dev, "no gpio node found\n");
		return -EINVAL;
	}

	pc->of_node = gpio_np;

	pc->of_irq = of_find_compatible_node(NULL,
					NULL, "amlogic,meson-gpio-intc");

	pc->reg_mux = meson_map_resource(pc, gpio_np, "mux");
	if (IS_ERR(pc->reg_mux)) {
		dev_err(pc->dev, "mux registers not found\n");
		return PTR_ERR(pc->reg_mux);
	}

	pc->reg_gpio = meson_map_resource(pc, gpio_np, "gpio");
	if (IS_ERR(pc->reg_gpio)) {
		dev_err(pc->dev, "gpio registers not found\n");
		return PTR_ERR(pc->reg_gpio);
	}

	pc->reg_pull = meson_map_resource(pc, gpio_np, "pull");
	/* Use gpio region if pull one is not present */
	if (IS_ERR(pc->reg_pull)) {
		if (PTR_ERR(pc->reg_pull) == -ENOENT) {
			pc->reg_pull = pc->reg_gpio;
		} else {
			dev_err(pc->dev, "pull registers not found\n");
			return PTR_ERR(pc->reg_pull);
		}
	}

	pc->reg_pullen = meson_map_resource(pc, gpio_np, "pull-enable");
	/* Use pull region if pull-enable one is not present */
	if (IS_ERR(pc->reg_pullen)) {
		if (PTR_ERR(pc->reg_pullen) == -ENOENT)
			pc->reg_pullen = pc->reg_pull;
		else {
			dev_err(pc->dev, "pull-enable registers not found\n");
			return PTR_ERR(pc->reg_pullen);
		}
	}

	pc->reg_drive = meson_map_resource(pc, gpio_np, "drive-strength");
	if (IS_ERR(pc->reg_drive)) {
		if (PTR_ERR(pc->reg_drive) == -ENOENT)
			pc->reg_drive = NULL;
		else {
			dev_err(pc->dev, "drive-strength registers not found\n");
			return PTR_ERR(pc->reg_drive);
		}
	}

	return 0;
}

int meson_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_pinctrl *pc;
	int ret;

	pc = devm_kzalloc(dev, sizeof(struct meson_pinctrl), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	pc->dev = dev;
	pc->data = (struct meson_pinctrl_data *) of_device_get_match_data(dev);

	ret = meson_pinctrl_parse_dt(pc, dev->of_node);
	if (ret)
		return ret;

	pc->desc.name		= "pinctrl-meson";
	pc->desc.owner		= THIS_MODULE;
	pc->desc.pctlops	= &meson_pctrl_ops;
	pc->desc.pmxops		= pc->data->pmx_ops;
	pc->desc.pins		= pc->data->pins;
	pc->desc.npins		= pc->data->num_pins;

	if (pc->data->pcf_ops)
		pc->desc.confops = pc->data->pcf_ops;
	else
		pc->desc.confops = &meson_pinconf_ops;

	if (pc->data->init)
		pc->data->init(pc);

	pc->pcdev = devm_pinctrl_register(pc->dev, &pc->desc, pc);
	if (IS_ERR(pc->pcdev)) {
		dev_err(pc->dev, "can't register pinctrl device");
		return PTR_ERR(pc->pcdev);
	}

	return meson_gpiolib_register(pc);
}
