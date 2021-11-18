// SPDX-License-Identifier: GPL-2.0-only
/*
 * Backlight driver for Rohm BD2606 Devices
 *
 * Copyright 2016 ArcticSand, Inc.
 * Author : Brian Dodge <bdodge@arcticsand.com>
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

enum bd2606_chip_id {
	BD2606
};

/* registers */
#define BD2606_REG_LEDACNT	0x00	/* LEDACNT Register */
#define BD2606_REG_LEDBCNT	0x01	/* LEDBCNT Register */
#define BD2606_REG_LEDCCNT	0x02	/* LEDCCNT Register */
#define BD2606_REG_LEDPWRCNT 0x03	/* LEDPWRCNT Register */


#define BD2606_LEDPWRCNT_MASK  0x7F  /* Mask for LED PWR Control register */
#define BD2606_LED_MASK		0x3F	/*   LED string enables mask */
#define BD2606_LED_BITS		0x06	/*   Bits of LED string enables */
#define BD2606_LED_LEDA1	0x01
#define BD2606_LED_LEDA2	0x02
#define BD2606_LED_LEDB1	0x04
#define BD2606_LED_LEDB2	0x08
#define BD2606_LED_LEDC1	0x10
#define BD2606_LED_LEDC2	0x20


#define MAX_BRIGHTNESS		63   /* 64 steps */
#define INIT_BRIGHT		10

struct bd2606 {
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	const char *name;
	u16 default_brightness;

	u8	leden, leden_curr;
};

static int bd2606_write(struct bd2606 *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static u8 bd2606_read(struct bd2606 *lp, u8 reg)
{
	return i2c_smbus_read_byte_data(lp->client, reg);
}

static int bd2606_update_field(struct bd2606 *lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = bd2606_read(lp, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return bd2606_write(lp, reg, tmp);
}

static int bd2606_set_brightness(struct bd2606 *lp, u32 brightness)
{
	int ret;
	u8 val;

	/* lower nibble of brightness goes in upper nibble of LSB register */
	val = (brightness & 0x3F); //<< BD2606_WLED_ISET_LSB_SHIFT;

	ret = bd2606_write(lp, BD2606_REG_LEDACNT, val);
	ret = bd2606_write(lp, BD2606_REG_LEDBCNT, val);
	ret = bd2606_write(lp, BD2606_REG_LEDCCNT, val);
	return ret;
}

static int bd2606_bl_update_status(struct backlight_device *bl)
{
	struct bd2606 *lp = bl_get_data(bl);
	u32 brightness = bl->props.brightness;
	int leden;
	int ret;

	leden = brightness ? lp->leden : 0;
	if (lp->leden_curr != leden) {
		lp->leden_curr = leden;
		pr_debug("%s: leden=0x%02x\n", __func__, leden);
		bd2606_update_field(lp, BD2606_REG_LEDPWRCNT,
			BD2606_LED_MASK, leden);
	}
	ret = bd2606_set_brightness(lp, brightness);
	return ret;
}

static const struct backlight_ops bd2606_bl_ops = {
	.update_status = bd2606_bl_update_status,
};

static int bd2606_backlight_register(struct bd2606 *lp)
{
	struct backlight_properties *props;
	const char *name = lp->name ? : "rohm_bl";

	props = devm_kzalloc(lp->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->type = BACKLIGHT_PLATFORM;
	props->max_brightness = MAX_BRIGHTNESS;

	if (lp->default_brightness > props->max_brightness)
		lp->default_brightness = props->max_brightness;

	props->brightness = lp->default_brightness;

	lp->bl = devm_backlight_device_register(lp->dev, name, lp->dev, lp,
				       &bd2606_bl_ops, props);
	return PTR_ERR_OR_ZERO(lp->bl);
}

static void bd2606_parse_dt(struct bd2606 *lp)
{
	struct device *dev = lp->dev;
	struct device_node *node = dev->of_node;
	u32 prog_val, num_entry, entry, sources[BD2606_LED_BITS];
	int ret;

	/* device tree entry isn't required, defaults are OK */
	if (!node)
		return;

	ret = of_property_read_string(node, "label", &lp->name);
	if (ret < 0)
		lp->name = NULL;

	ret = of_property_read_u32(node, "default-brightness", &prog_val);
	if (ret == 0)
		lp->default_brightness = prog_val;


	ret = of_property_count_u32_elems(node, "led-sources");
	if (ret < 0) {
		lp->leden = BD2606_LED_MASK; /* all on is default */
	} else {
		num_entry = ret;
		if (num_entry > BD2606_LED_BITS)
			num_entry = BD2606_LED_BITS;

		ret = of_property_read_u32_array(node, "led-sources", sources,
					num_entry);
		if (ret < 0) {
			dev_err(dev, "led-sources node is invalid.\n");
			return;
		}

		lp->leden = 0;

		/* for each enable in source, set bit in led enable */
		for (entry = 0; entry < num_entry; entry++) {
			u8 onbit = 1 << sources[entry];

			lp->leden |= onbit;
		}
	}
}

static int bd2606_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct bd2606 *lp;
	int ret;

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(*lp), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;

	/* Setup defaults based on power-on defaults */
	lp->name = NULL;
	lp->default_brightness = INIT_BRIGHT;

	lp->leden = BD2606_LED_MASK;

	if (IS_ENABLED(CONFIG_OF))
		bd2606_parse_dt(lp);

	i2c_set_clientdata(cl, lp);

	/* constrain settings to what is possible */
	if (lp->default_brightness > MAX_BRIGHTNESS)
		lp->default_brightness = MAX_BRIGHTNESS;

	/* disable LEDs */
	bd2606_update_field(lp, BD2606_REG_LEDPWRCNT,
		BD2606_LED_MASK, 0);

	ret = bd2606_backlight_register(lp);
	if (ret)
		goto probe_register_err;

	backlight_update_status(lp->bl);

	return 0;

probe_register_err:
	dev_err(lp->dev,
		"failed to register BD2606 backlight.\n");
	return ret;
}

static void bd2606_remove(struct i2c_client *cl)
{
	struct bd2606 *lp = i2c_get_clientdata(cl);

	/* disable all strings (ignore errors) */
	bd2606_write(lp, BD2606_REG_LEDPWRCNT, 0x00);

	lp->bl->props.brightness = 0;

	backlight_update_status(lp->bl);
}

static const struct of_device_id bd2606_dt_ids[] = {
	{ .compatible = "rohm,bd2606" },
	{ }
};
MODULE_DEVICE_TABLE(of, bd2606_dt_ids);

static const struct i2c_device_id bd2606_ids[] = {
	{"bd2606", BD2606},
	{ }
};
MODULE_DEVICE_TABLE(i2c, bd2606_ids);

static struct i2c_driver bd2606_driver = {
	.driver = {
		.name = "bd2606_bl",
		.of_match_table = of_match_ptr(bd2606_dt_ids),
	},
	.probe = bd2606_probe,
	.remove = bd2606_remove,
	.id_table = bd2606_ids,
};
module_i2c_driver(bd2606_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("David Zhang <david.zhang@averydennison.com>");
MODULE_DESCRIPTION("BD2606 Backlight driver");
