// SPDX-License-Identifier: GPL-2.0
/*
 * Simple driver for Analog Devices ADP1650 LED Flash driver chip
 * Copyright (C) 2021 Boundary Devices LLC
 *
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define ADP1650_REG_VERSION		0x00
#define ADP1650_REG_TIMER_IOCFG		0x02
#define ADP1650_REG_CURRENT_SET		0x03
#define ADP1650_REG_OUTPUT_MODE		0x04
#define ADP1650_REG_FAULT		0x05
#define ADP1650_REG_CONTROL		0x06
#define ADP1650_REG_AD_MODE		0x07
#define ADP1650_REG_ADC			0x08
#define ADP1650_REG_BATT_LOW		0x09
#define ADP1650_REG_MAX			ADP1650_REG_BATT_LOW

/* ADP1650_REG_TIMER_IOCFG Bits and Masks */
#define ADP1650_FL_TIMER_ms(x)		((((x) - 100) / 100) & 0xF) /* Timer */
#define ADP1650_FL_TIMER_val2ms(x)	(((x & 0xF) * 100) + 100)
#define ADP1650_FL_TIMER_MIN		100
#define ADP1650_FL_TIMER_MAX		1600

/* ADP1650_REG_CURRENT_SET Bits and Masks  */
#define ADP1650_I_FL_mA(x)		((((x) - 300) / 50) << 3)
#define ADP1650_I_FL_MIN		300
#define ADP1650_I_FL_MAX		1500
#define ADP1650_I_TOR_mA(x)		((((x) - 25) / 25) & 0x7)
#define ADP1650_I_TOR_MIN		25
#define ADP1650_I_TOR_MAX		200

/* ADP1650_REG_OUTPUT_MODE Bits and Masks  */
#define ADP1650_OUTPUT_EN		(1 << 3)
#define ADP1650_STR_MODE_STBY		(0 << 0)
#define ADP1650_LED_MODE_VOUT		(1 << 0)
#define ADP1650_LED_MODE_ASSIST_LIGHT	(2 << 0)
#define ADP1650_LED_MODE_FLASH		(3 << 0)

struct adp1650_platform_data {
	struct gpio_desc	*enable_gpio;
	struct gpio_desc	*strobe_gpio;
	struct gpio_desc	*torch_gpio;
};

struct adp1650_chip_data {
	struct device *dev;

	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;

	struct work_struct work_flash;
	struct work_struct work_torch;

	u8 br_flash;
	u8 br_torch;

	struct adp1650_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};

enum adp1650_mode {
	MODE_STANDBY = 0,
	MODE_TORCH,
	MODE_FLASH
};

/* Chip control */
static int adp1650_control(struct adp1650_chip_data *chip, u8 value, int mode)
{
	int enable = 0;
	int max_current = 0;
	int ret = 0;

	/* Value 0 means off-state */
	if (!value)
		mode = MODE_STANDBY;

	switch (mode) {
	case MODE_TORCH:
		max_current = ADP1650_I_TOR_mA(value);
		enable = (ADP1650_OUTPUT_EN | ADP1650_LED_MODE_ASSIST_LIGHT);
		break;
	case MODE_FLASH:
		max_current = ADP1650_I_FL_mA(value);
		enable = (ADP1650_OUTPUT_EN | ADP1650_LED_MODE_FLASH);
		break;
	case MODE_STANDBY:
		break;
	default:
		dev_err(chip->dev, "Mode %x not supported!\n", mode);
		return -EINVAL;
	}

	ret = regmap_write(chip->regmap, ADP1650_REG_CURRENT_SET, max_current);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write to CURRENT_SET\n");
		goto out;
	}

	ret = regmap_write(chip->regmap, ADP1650_REG_OUTPUT_MODE, enable);
	if (ret < 0)
		dev_err(chip->dev, "Failed to write to OUTPUT_MODE\n");
out:
	return ret;
}

/* Torch */
static void adp1650_deferred_torch_brightness_set(struct work_struct *work)
{
	struct adp1650_chip_data *chip =
	    container_of(work, struct adp1650_chip_data, work_torch);

	mutex_lock(&chip->lock);
	adp1650_control(chip, chip->br_torch, MODE_TORCH);
	mutex_unlock(&chip->lock);
}

static void adp1650_torch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct adp1650_chip_data *chip =
	    container_of(cdev, struct adp1650_chip_data, cdev_torch);

	if (brightness && (brightness < ADP1650_I_TOR_MIN))
		brightness = ADP1650_I_TOR_MIN;

	chip->br_torch = brightness;
	schedule_work(&chip->work_torch);
}

/* Flash */
static void adp1650_deferred_strobe_brightness_set(struct work_struct *work)
{
	struct adp1650_chip_data *chip =
	    container_of(work, struct adp1650_chip_data, work_flash);

	mutex_lock(&chip->lock);
	adp1650_control(chip, chip->br_flash, MODE_FLASH);
	mutex_unlock(&chip->lock);
}

static void adp1650_strobe_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct adp1650_chip_data *chip =
	    container_of(cdev, struct adp1650_chip_data, cdev_flash);

	if (brightness && (brightness < ADP1650_I_FL_MIN))
		brightness = ADP1650_I_FL_MIN;

	chip->br_flash = brightness;
	schedule_work(&chip->work_flash);
}

static ssize_t adp1650_timeout_ms_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct adp1650_chip_data *chip =
	    container_of(led_cdev, struct adp1650_chip_data, cdev_flash);
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, ADP1650_REG_TIMER_IOCFG, &val);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read Timer reg\n", __func__);
		return ret;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", ADP1650_FL_TIMER_val2ms(val));
}

static ssize_t adp1650_timeout_ms_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct adp1650_chip_data *chip =
	    container_of(led_cdev, struct adp1650_chip_data, cdev_flash);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		dev_err(chip->dev, "%s: fail to change str to int\n", __func__);
		return ret;
	}

	/* Sanity check */
	if (val > ADP1650_FL_TIMER_MAX)
		return -EINVAL;
	if (val && (val < ADP1650_FL_TIMER_MIN))
		val = ADP1650_FL_TIMER_MIN;

	ret = regmap_write(chip->regmap, ADP1650_REG_TIMER_IOCFG,
			   ADP1650_FL_TIMER_ms(val));
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to set Timer reg\n", __func__);
		return ret;
	}

	return size;
}

static DEVICE_ATTR(timeout_ms, S_IRUGO | S_IWUSR, adp1650_timeout_ms_show,
		   adp1650_timeout_ms_store);

static struct attribute *adp1650_flash_attrs[] = {
	&dev_attr_timeout_ms.attr,
	NULL
};
ATTRIBUTE_GROUPS(adp1650_flash);

static const struct regmap_config adp1650_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ADP1650_REG_MAX,
};

#ifdef CONFIG_OF
static struct adp1650_platform_data *adp1650_parse_dt(struct i2c_client *client)
{
	struct adp1650_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->enable_gpio = devm_gpiod_get_index(&client->dev, "enable", 0,
						GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->enable_gpio)) {
		dev_err(&client->dev, "Couldn't find hwen gpio\n");
		return ERR_PTR(-ENODATA);
	}

	pdata->strobe_gpio = devm_gpiod_get_index(&client->dev, "strobe", 0,
						  GPIOD_OUT_LOW);
	if (IS_ERR(pdata->strobe_gpio)) {
		dev_err(&client->dev, "Couldn't find strobe gpio\n");
		return ERR_PTR(-ENODATA);
	}

	pdata->torch_gpio = devm_gpiod_get_index(&client->dev, "torch", 0,
						 GPIOD_OUT_LOW);
	if (IS_ERR(pdata->torch_gpio)) {
		dev_err(&client->dev, "Couldn't find torch gpio\n");
		return ERR_PTR(-ENODATA);
	}

	return pdata;
}
#else
static struct adp1650_platform_data *adp1650_parse_dt(struct i2c_client *client)
{
	return ERR_PTR(-ENOENT);
}
#endif

static int adp1650_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct adp1650_platform_data *pdata = dev_get_platdata(&client->dev);
	struct adp1650_chip_data *chip;
	int err;

	if (pdata == NULL) {
		pdata = adp1650_parse_dt(client);
		if (IS_ERR(pdata)) {
			dev_err(&client->dev, "Couldn't find platform data\n");
			return -ENODATA;
		}
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	chip = devm_kzalloc(&client->dev,
			    sizeof(struct adp1650_chip_data), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(client, &adp1650_regmap);
	if (IS_ERR(chip->regmap)) {
		err = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			err);
		return err;
	}

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	/* Init chip in standby mode */
	err = adp1650_control(chip, 0, MODE_STANDBY);
	if (err < 0)
		goto err_out;

	/* Flash */
	INIT_WORK(&chip->work_flash, adp1650_deferred_strobe_brightness_set);
	chip->cdev_flash.name = "flash";
	chip->cdev_flash.max_brightness = ADP1650_I_FL_MAX;
	chip->cdev_flash.brightness_set = adp1650_strobe_brightness_set;
	chip->cdev_flash.default_trigger = "flash";
	chip->cdev_flash.groups = adp1650_flash_groups,
	err = led_classdev_register((struct device *)
				    &client->dev, &chip->cdev_flash);
	if (err < 0) {
		dev_err(chip->dev, "failed to register flash\n");
		goto err_out;
	}

	/* Torch */
	INIT_WORK(&chip->work_torch, adp1650_deferred_torch_brightness_set);
	chip->cdev_torch.name = "torch";
	chip->cdev_torch.max_brightness = ADP1650_I_TOR_MAX;
	chip->cdev_torch.brightness_set = adp1650_torch_brightness_set;
	chip->cdev_torch.default_trigger = "torch";
	err = led_classdev_register((struct device *)
				    &client->dev, &chip->cdev_torch);
	if (err < 0) {
		dev_err(chip->dev, "failed to register torch\n");
		goto err_create_torch_file;
	}

	dev_info(&client->dev, "adp1650 is initialized\n");

	return 0;

err_create_torch_file:
	led_classdev_unregister(&chip->cdev_flash);
err_out:
	return err;
}

static void adp1650_remove(struct i2c_client *client)
{
	struct adp1650_chip_data *chip = i2c_get_clientdata(client);

	led_classdev_unregister(&chip->cdev_torch);
	flush_work(&chip->work_torch);
	led_classdev_unregister(&chip->cdev_flash);
	flush_work(&chip->work_flash);
	regmap_write(chip->regmap, ADP1650_REG_OUTPUT_MODE, 0);
	gpiod_set_value(chip->pdata->enable_gpio, 0);
}

static const struct i2c_device_id adp1650_id[] = {
	{"leds-adp1650", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, adp1650_id);

static const struct of_device_id adp1650_dt_ids[] = {
	{ .compatible = "adi,adp1650" },
	{ /* sentinel */ }
};

static struct i2c_driver adp1650_i2c_driver = {
	.driver = {
		.name = "leds-adp1650",
		.of_match_table	= of_match_ptr(adp1650_dt_ids),
		.pm = NULL,
	},
	.id_table = adp1650_id,
	.probe = adp1650_probe,
	.remove = adp1650_remove,
};

module_i2c_driver(adp1650_i2c_driver);

MODULE_DESCRIPTION("Flash Lighting driver for ADI ADP1650");
MODULE_AUTHOR("Boundary Devices <support@boundarydevices.com>");
MODULE_LICENSE("GPL v2");
