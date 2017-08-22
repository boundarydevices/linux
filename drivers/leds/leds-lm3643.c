/*
* Simple driver for Texas Instruments LM3643 LED Flash driver chip
* Copyright (C) 2017 Boundary Devices Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
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

#define	REG_ENABLE			(0x1)
#define	REG_IVFM_MODE			(0x2)
#define REG_LED1_FLASH			(0x3)
#define REG_LED2_FLASH			(0x4)
#define REG_LED1_TORCH			(0x5)
#define REG_LED2_TORCH			(0x6)
#define REG_BOOST			(0x7)
#define REG_TIMING			(0x8)
#define REG_TEMP			(0x9)
#define	REG_FLAGS1			(0xA)
#define	REG_FLAGS2			(0xB)
#define	REG_DEVICE_ID			(0xC)
#define	REG_LAST_FLASH			(0xD)
#define	REG_MAX				(0xD)

#define	REG_ENABLE_LED1			(0x01)
#define	REG_ENABLE_LED2			(0x02)
#define	REG_ENABLE_MODE_STANDBY		(0x00)
#define	REG_ENABLE_MODE_IR		(0x04)
#define	REG_ENABLE_MODE_TORCH		(0x08)
#define	REG_ENABLE_MODE_FLASH		(0x0C)
#define	REG_ENABLE_TORCH_EN		(0x10)
#define	REG_ENABLE_STROBE_EN		(0x20)
#define	REG_ENABLE_STROBE_LEVEL		(0x00)
#define	REG_ENABLE_STROBE_EDGE		(0x40)
#define	REG_ENABLE_TX_EN		(0x80)

#define REG_LED1_FLASH_LED2_OVERRIDE	(0x80)
#define REG_LED1_TORCH_LED2_OVERRIDE	(0x80)

#define REG_TIMING_TORCH_MASK		(0x70)
#define REG_TIMING_FLASH_MASK		(0x0f)

/* Flash timeout values as defined in "9.6.8 Timing Configuration Register" */
int lm3643_flash_timeout_ms[] =
{10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200, 250, 300, 350, 400};

struct lm3643_platform_data {
	struct gpio_desc	*flash_gpio;
	struct gpio_desc	*hwen_gpio;
	struct gpio_desc	*strobe_gpio;
	struct gpio_desc	*torch_gpio;
};

struct lm3643_chip_data {
	struct device *dev;

	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;

	struct work_struct work_flash;
	struct work_struct work_torch;

	u8 br_flash;
	u8 br_torch;

	struct lm3643_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};

enum lm3643_mode {
	MODE_STANDBY = 0,
	MODE_IR,
	MODE_TORCH,
	MODE_FLASH
};

/* Chip control */
static int lm3643_control(struct lm3643_chip_data *chip, u8 value, int mode)
{
	int enable = 0;
	int ret = 0;

	/* Value 0 means off-state */
	if (!value)
		mode = MODE_STANDBY;

	switch (mode) {
	case MODE_TORCH:
		enable |= (REG_ENABLE_LED1 | REG_ENABLE_LED2 |
			   REG_ENABLE_MODE_TORCH);
		ret = regmap_write(chip->regmap, REG_LED1_TORCH,
				   (value - 1) | REG_LED1_TORCH_LED2_OVERRIDE);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write to LED1_TORCH\n");
			goto out;
		}
		break;
	case MODE_FLASH:
		enable |= (REG_ENABLE_LED1 | REG_ENABLE_LED2 |
			   REG_ENABLE_MODE_FLASH);
		ret = regmap_write(chip->regmap, REG_LED1_FLASH,
				   (value - 1) | REG_LED1_FLASH_LED2_OVERRIDE);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write to LED1_FLASH\n");
			goto out;
		}
		break;
	case MODE_STANDBY:
		break;
	case MODE_IR:
	default:
		return ret;
	}

	ret = regmap_write(chip->regmap, REG_ENABLE, enable);
out:
	return ret;
}

/* Torch */
static void lm3643_deferred_torch_brightness_set(struct work_struct *work)
{
	struct lm3643_chip_data *chip =
	    container_of(work, struct lm3643_chip_data, work_torch);

	mutex_lock(&chip->lock);
	lm3643_control(chip, chip->br_torch, MODE_TORCH);
	mutex_unlock(&chip->lock);
}

static void lm3643_torch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3643_chip_data *chip =
	    container_of(cdev, struct lm3643_chip_data, cdev_torch);

	chip->br_torch = brightness;
	schedule_work(&chip->work_torch);
}

/* Flash */
static void lm3643_deferred_strobe_brightness_set(struct work_struct *work)
{
	struct lm3643_chip_data *chip =
	    container_of(work, struct lm3643_chip_data, work_flash);

	mutex_lock(&chip->lock);
	lm3643_control(chip, chip->br_flash, MODE_FLASH);
	mutex_unlock(&chip->lock);
}

static void lm3643_strobe_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3643_chip_data *chip =
	    container_of(cdev, struct lm3643_chip_data, cdev_flash);

	chip->br_flash = brightness;
	schedule_work(&chip->work_flash);
}

static ssize_t lm3643_timeout_ms_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3643_chip_data *chip =
	    container_of(led_cdev, struct lm3643_chip_data, cdev_flash);
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, REG_TIMING, &val);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read regmap\n", __func__);
		return ret;
	}

	val &= REG_TIMING_FLASH_MASK;

	return scnprintf(buf, PAGE_SIZE, "%d\n", lm3643_flash_timeout_ms[val]);
}

static ssize_t lm3643_timeout_ms_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3643_chip_data *chip =
	    container_of(led_cdev, struct lm3643_chip_data, cdev_flash);
	unsigned int val;
	int ret;
	int i;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		dev_err(chip->dev, "%s: fail to change str to int\n", __func__);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(lm3643_flash_timeout_ms); i++) {
		if (lm3643_flash_timeout_ms[i] == val)
			break;
	}

	if (i >= ARRAY_SIZE(lm3643_flash_timeout_ms)) {
		ret = -EINVAL;
		return ret;
	}

	ret = regmap_update_bits(chip->regmap, REG_TIMING,
				 REG_TIMING_FLASH_MASK, i);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to update regmap\n", __func__);
		return ret;
	}

	return size;
}

static DEVICE_ATTR(timeout_ms, S_IRUGO | S_IWUSR, lm3643_timeout_ms_show,
		   lm3643_timeout_ms_store);

static struct attribute *lm3643_flash_attrs[] = {
	&dev_attr_timeout_ms.attr,
	NULL
};
ATTRIBUTE_GROUPS(lm3643_flash);

static const struct regmap_config lm3643_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

#ifdef CONFIG_OF
static struct lm3643_platform_data *lm3643_parse_dt(struct i2c_client *client)
{
	struct lm3643_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->flash_gpio = devm_gpiod_get_index(&client->dev, "flash", 0,
						 GPIOD_OUT_LOW);
	if (IS_ERR(pdata->flash_gpio)) {
		dev_err(&client->dev, "Couldn't find flash gpio\n");
		return ERR_PTR(-ENODATA);
	}

	pdata->hwen_gpio = devm_gpiod_get_index(&client->dev, "hwen", 0,
						GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->hwen_gpio)) {
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
static struct lm3643_platform_data *lm3643_parse_dt(struct i2c_client *client)
{
	return ERR_PTR(-ENOENT);
}
#endif

static int lm3643_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct lm3643_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3643_chip_data *chip;
	int err;

	if (pdata == NULL) {
		pdata = lm3643_parse_dt(client);
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
			    sizeof(struct lm3643_chip_data), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(client, &lm3643_regmap);
	if (IS_ERR(chip->regmap)) {
		err = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			err);
		return err;
	}

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	/* Init chip in standby mode */
	err = lm3643_control(chip, 0, MODE_STANDBY);
	if (err < 0)
		goto err_out;

	/* Flash */
	INIT_WORK(&chip->work_flash, lm3643_deferred_strobe_brightness_set);
	chip->cdev_flash.name = "flash";
	/*
	 * LM3643 datasheet section "9.3.1 Flash Mode" says:
	 * "The total allowed LED current during operation is 1.5A"
	 * This driver doesn't differentiate LED1/2, make sure value < 1.5A
	 */
	chip->cdev_flash.max_brightness = 64;
	chip->cdev_flash.brightness_set = lm3643_strobe_brightness_set;
	chip->cdev_flash.default_trigger = "flash";
	chip->cdev_flash.groups = lm3643_flash_groups,
	err = led_classdev_register((struct device *)
				    &client->dev, &chip->cdev_flash);
	if (err < 0) {
		dev_err(chip->dev, "failed to register flash\n");
		goto err_out;
	}

	/* Torch */
	INIT_WORK(&chip->work_torch, lm3643_deferred_torch_brightness_set);
	chip->cdev_torch.name = "torch";
	chip->cdev_torch.max_brightness = 128;
	chip->cdev_torch.brightness_set = lm3643_torch_brightness_set;
	chip->cdev_torch.default_trigger = "torch";
	err = led_classdev_register((struct device *)
				    &client->dev, &chip->cdev_torch);
	if (err < 0) {
		dev_err(chip->dev, "failed to register torch\n");
		goto err_create_torch_file;
	}

	dev_info(&client->dev, "LM3643 is initialized\n");
	return 0;

err_create_torch_file:
	led_classdev_unregister(&chip->cdev_flash);
err_out:
	return err;
}

static int lm3643_remove(struct i2c_client *client)
{
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	led_classdev_unregister(&chip->cdev_torch);
	flush_work(&chip->work_torch);
	led_classdev_unregister(&chip->cdev_flash);
	flush_work(&chip->work_flash);
	regmap_write(chip->regmap, REG_ENABLE, 0);
	gpiod_set_value(chip->pdata->hwen_gpio, 0);

	return 0;
}

static const struct i2c_device_id lm3643_id[] = {
	{"leds-lm3643", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3643_id);

static const struct of_device_id lm3643_dt_ids[] = {
	{ .compatible = "ti,lm3643" },
	{ /* sentinel */ }
};

static struct i2c_driver lm3643_i2c_driver = {
	.driver = {
		.name = "leds-lm3643",
		.of_match_table	= of_match_ptr(lm3643_dt_ids),
		.pm = NULL,
	},
	.id_table = lm3643_id,
	.probe = lm3643_probe,
	.remove = lm3643_remove,
};

module_i2c_driver(lm3643_i2c_driver);

MODULE_DESCRIPTION("Flash Lighting driver for TI LM3643");
MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_LICENSE("GPL v2");
