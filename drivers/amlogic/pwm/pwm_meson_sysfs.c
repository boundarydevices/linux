/*
 * drivers/amlogic/pwm/pwm_meson_sysfs.c
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

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm_meson.h>
#include "pwm_meson_util.h"

/**
 * pwm_constant_enable()
 *	- start a constant PWM output toggling
 *	  txl only support 8 channel constant output
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_constant_enable(struct aml_pwm_chip *chip, int index)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;

	if ((id < 0) && (id > 9)) {
		dev_err(aml_chip->chip.dev,
				"constant,index is not within the scope!\n");
		return -EINVAL;
	}

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		val = 1 << 28;
		break;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		val = 1 << 29;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"enable,index is not legal\n");
		return -EINVAL;
	break;
	}
	pwm_set_reg_bits(&aml_reg->miscr, val, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_constant_enable);


/**
 * pwm_constant_disable() - stop a constant PWM output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */

int pwm_constant_disable(struct aml_pwm_chip *chip, int index)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;
	unsigned int mask;

	if ((id < 0) && (id > 9)) {
		dev_err(aml_chip->chip.dev,
				"constant disable,index is not within the scope!\n");
		return -EINVAL;
	}

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		mask = 1 << 28;
		val = 0 << 28;
		break;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		mask = 1 << 29;
		val = 0 << 29;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"constant disable,index is not legal\n");
		return -EINVAL;

	break;
	}
	pwm_set_reg_bits(&aml_reg->miscr, mask, val);
	return 0;
}
EXPORT_SYMBOL_GPL(pwm_constant_disable);


static ssize_t pwm_constant_show(struct device *child,
			       struct device_attribute *attr, char *buf)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", chip->variant.constant);
}

static ssize_t pwm_constant_store(struct device *child,
				  struct device_attribute *attr,
				  const char *buf, size_t size)

{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child, "Can't parse pwm id,usage:[value index]\n");
		return -EINVAL;
	}
	if ((id < 0) && (id > 9)) {
		dev_err(chip->chip.dev,
				"constant,index is not within the scope!\n");
		return -EINVAL;
	}

	switch (val) {
	case 0:
		ret = pwm_constant_disable(chip, id);
		chip->variant.constant = 0;
		break;
	case 1:
		ret = pwm_constant_enable(chip, id);
		chip->variant.constant = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? : size;
}

/**
 * pwm_set_times() - set PWM times output toggling
 *					 set pwm a1 and pwm a2 timer together
 *					 and pwm a1 should be set first
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B,range from 1 to 15
 * @value: blink times to set,range from 1 to 255
 */

int pwm_set_times(struct aml_pwm_chip *chip,
						int index, int value)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int clear_val;
	unsigned int val;

	if (((val <= 0) && (val > 255)) || ((id < 0) && (id > 20))) {
		dev_err(aml_chip->chip.dev,
				"index or value is not within the scope!\n");
		return -EINVAL;
	}

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		clear_val = 0xff << 24;
		val = value << 24;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		clear_val = 0xff << 8;
		val = value << 8;
		break;
	case PWM_A2:
	case PWM_C2:
	case PWM_E2:
	case PWM_AO_A2:
	case PWM_AO_C2:
		clear_val = 0xff << 16;
		val = value << 16;
	case PWM_B2:
	case PWM_D2:
	case PWM_F2:
	case PWM_AO_B2:
	case PWM_AO_D2:
		clear_val = 0xff;
		val = value;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"times,index is not legal\n");
		return -EINVAL;

	break;
	}
	pwm_clear_reg_bits(&aml_reg->tr, clear_val);
	pwm_write_reg1(&aml_reg->tr, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_set_times);

static ssize_t pwm_times_show(struct device *child,
			       struct device_attribute *attr, char *buf)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", chip->variant.times);
}

static ssize_t pwm_times_store(struct device *child,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child,
		"Can't parse pwm id and value,usage:[value index]\n");
		return -EINVAL;
	}
	if (((val <= 0) && (val > 255)) || ((id < 0) && (id > 15))) {
		dev_err(chip->chip.dev,
		"index or value is not within the scope!\n");
		return -EINVAL;
	}

	ret = pwm_set_times(chip, id, val);
	chip->variant.times = val;

	return ret ? : size;
}

/**
 * pwm_blink_enable()
 *	- start a blink PWM output toggling
 *	  txl only support 8 channel blink output
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_blink_enable(struct aml_pwm_chip *chip, int index)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;

	if ((id < 0) && (id > 9)) {
		dev_err(aml_chip->chip.dev, "index is not within the scope!\n");
		return -EINVAL;
	}

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		val = 1 << 8;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		val = 1 << 9;
	default:
		dev_err(aml_chip->chip.dev,
				"blink enable,index is not legal\n");
		return -EINVAL;
	break;
	}
	pwm_set_reg_bits(&aml_reg->br, val, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_blink_enable);

/**
 * pwm_blink_disable() - stop a constant PWM output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_blink_disable(struct aml_pwm_chip *chip, int index)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int val;
	unsigned int mask;

	if ((id < 1) && (id > 9)) {
		dev_err(aml_chip->chip.dev, "index is not within the scope!\n");
		return -EINVAL;
	}

	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		mask = 1 << 8;
		val = 0 << 8;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		mask = 1 << 9;
		val = 0 << 9;
	default:
		dev_err(aml_chip->chip.dev,
				"blink enable,index is not legal\n");
		return -EINVAL;
	break;
	}
	pwm_set_reg_bits(&aml_reg->br, mask, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_blink_disable);

static ssize_t pwm_blink_enable_show(struct device *child,
			       struct device_attribute *attr, char *buf)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", chip->variant.blink_enable);
}

static ssize_t pwm_blink_enable_store(struct device *child,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child,
		"blink enable,Can't parse pwm id,usage:[value index]\n");
		return -EINVAL;
	}
	if ((id < 1) && (id > 7)) {
		dev_err(chip->chip.dev, "index is not within the scope!\n");
		return -EINVAL;
	}

	switch (val) {
	case 0:
		ret = pwm_blink_disable(chip, id);
		chip->variant.blink_enable = 0;
		break;
	case 1:
		ret = pwm_blink_enable(chip, id);
		chip->variant.blink_enable = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? : size;

}

/**
 * pwm_set_blink_times() - set PWM blink times output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 * @value: blink times to set,range from 1 to 15
 */
int pwm_set_blink_times(struct aml_pwm_chip *chip,
								int index,
								int value)
{
	struct aml_pwm_chip *aml_chip = chip;
	int id = index;
	struct pwm_aml_regs *aml_reg =
	(struct pwm_aml_regs *)pwm_id_to_reg(id, aml_chip);
	unsigned int clear_val;
	unsigned int val;

	if (((val <= 0) && (val > 15)) || ((id < 1) && (id > 9))) {
		dev_err(aml_chip->chip.dev,
		"value or index is not within the scope!\n");
		return -EINVAL;
	}
	switch (id) {
	case PWM_A:
	case PWM_C:
	case PWM_E:
	case PWM_AO_A:
	case PWM_AO_C:
		clear_val = 0xf;
		val = value;
	case PWM_B:
	case PWM_D:
	case PWM_F:
	case PWM_AO_B:
	case PWM_AO_D:
		clear_val = 0xf << 4;
		val = value << 4;
		break;
	default:
		dev_err(aml_chip->chip.dev,
				"times,index is not legal\n");
		return -EINVAL;
	break;
	}
	pwm_clear_reg_bits(&aml_reg->tr, clear_val);
	pwm_write_reg1(&aml_reg->tr, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_set_blink_times);

static ssize_t pwm_blink_times_show(struct device *child,
			       struct device_attribute *attr, char *buf)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", chip->variant.blink_times);
}

static ssize_t pwm_blink_times_store(struct device *child,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct aml_pwm_chip *chip =
		(struct aml_pwm_chip *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child,
		"Can't parse pwm id and value,usage:[value index]\n");
		return -EINVAL;
	}
	if (((val <= 0) && (val > 15)) || ((id < 0) && (id > 7))) {
		dev_err(chip->chip.dev,
		"value or index is not within the scope!\n");
		return -EINVAL;
	}

	ret = pwm_set_blink_times(chip, id, val);
	chip->variant.blink_times = val;

	return ret ? : size;
}


static DEVICE_ATTR(constant, 0644,
						pwm_constant_show,
						pwm_constant_store);
static DEVICE_ATTR(times, 0644,
						pwm_times_show,
						pwm_times_store);
static DEVICE_ATTR(blink_enable, 0644,
						pwm_blink_enable_show,
						pwm_blink_enable_store);
static DEVICE_ATTR(blink_times, 0644,
						pwm_blink_times_show,
						pwm_blink_times_store);

static struct attribute *pwm_attrs[] = {
		&dev_attr_constant.attr,
		&dev_attr_times.attr,
		&dev_attr_blink_enable.attr,
		&dev_attr_blink_times.attr,
		NULL,
};

static struct attribute_group pwm_attr_group = {
		.attrs = pwm_attrs,
};

int meson_pwm_sysfs_init(struct device *dev)
{
	int retval;

	retval = sysfs_create_group(&dev->kobj, &pwm_attr_group);
	return retval;
}
