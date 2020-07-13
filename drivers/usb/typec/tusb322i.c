// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 BayLibre, SAS.
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/usb_phy_generic.h>
#include <linux/usb/role.h>
#include <linux/usb/typec.h>
#include <linux/workqueue.h>

#define TUSB322I_IRQF	(IRQF_TRIGGER_LOW | IRQF_ONESHOT)

#define TUSB322I_REG_CSCR			0x9
#define   CSCR_ATTACHED_STATE_NOT_ATTACHED	0
#define   CSCR_ATTACHED_STATE_DFP		1
#define   CSCR_ATTACHED_STATE_UFP	    	2
#define   CSCR_ATTACHED_STATE_ACCESSORY		3

const struct regmap_config tusb322i_regmap_config = {
	.max_register = 0xA,
	.reg_bits = 8,
	.val_bits = 8,
};

enum tusb322i_fields {
	F_ATTACHED_STATE,
	F_INTERRUPT_STATUS,
	F_MAX_FIELDS
};

static const struct reg_field tusb322i_reg_fields[] = {
	[F_ATTACHED_STATE] = REG_FIELD(TUSB322I_REG_CSCR, 6, 7),
	[F_INTERRUPT_STATUS] = REG_FIELD(TUSB322I_REG_CSCR, 4, 4),
};

struct tusb322i_chip {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_field *regmap_fields[F_MAX_FIELDS];
	struct usb_role_switch *role_sw;
	struct typec_port *port;
	struct typec_capability cap;
	struct workqueue_struct *work_queue;
	struct work_struct check_cc_work;
	struct gpio_desc *irq_gpiod;
	int irq;
};

static int tusb322i_check_cc_state(struct tusb322i_chip *chip)
{
	unsigned int v;
	int ret;

	ret = regmap_field_read(chip->regmap_fields[F_ATTACHED_STATE], &v);
	if (ret) {
		dev_err(chip->dev, "Failed to read ATTACHED_STATE bits\n");
		return ret;
	}

	switch (v) {
	case CSCR_ATTACHED_STATE_NOT_ATTACHED:
	case CSCR_ATTACHED_STATE_DFP:
		usb_role_switch_set_role(chip->role_sw, USB_ROLE_HOST);
		typec_set_data_role(chip->port, TYPEC_HOST);
		break;

	case CSCR_ATTACHED_STATE_UFP:
		usb_role_switch_set_role(chip->role_sw, USB_ROLE_DEVICE);
		typec_set_data_role(chip->port, TYPEC_DEVICE);
		break;

	case CSCR_ATTACHED_STATE_ACCESSORY:
		break;

	default:
		WARN(1, "Reached un-reachable state: %d\n", v);
		break;
	}

	return 0;
}

static void check_cc_work_handler(struct work_struct *work)
{
	struct tusb322i_chip *chip =
		container_of(work, struct tusb322i_chip, check_cc_work);
	unsigned int v;
	int ret;

	ret = regmap_read(chip->regmap, TUSB322I_REG_CSCR, &v);
	if (ret) {
		dev_err(chip->dev, "Failed to read CSCR register\n");
		return;
	}

	ret = regmap_write(chip->regmap, TUSB322I_REG_CSCR, v);
	if (ret) {
		dev_warn(chip->dev, "Failed to update CSCR register\n");
	}

	tusb322i_check_cc_state(chip);
}

static irqreturn_t tusb322i_isr(int irq, void *dev_id)
{
	struct tusb322i_chip *chip = dev_id;

	queue_work(chip->work_queue, &chip->check_cc_work);

	return IRQ_HANDLED;
}

static int tusb322i_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct tusb322i_chip *chip;
	struct device *dev = &client->dev;
	int ret;
	int i;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;

	chip->regmap = devm_regmap_init_i2c(client, &tusb322i_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(dev, "Failed to initialize register map\n");
		return PTR_ERR(chip->regmap);
	}

	for (i = 0; i < F_MAX_FIELDS; i++) {
		chip->regmap_fields[i] =
			devm_regmap_field_alloc(dev, chip->regmap,
						tusb322i_reg_fields[i]);
		if (IS_ERR(chip->regmap_fields[i])) {
			dev_err(dev,
				"Failed to allocate regmap field %d\n", i);
			return PTR_ERR(chip->regmap_fields[i]);
		}
	}

	chip->irq_gpiod = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR(chip->irq_gpiod))
		return PTR_ERR(chip->irq_gpiod);

	chip->role_sw = usb_role_switch_get(dev);
	if (IS_ERR(chip->role_sw)) {
		dev_err(dev, "Failed to get role switch: %ld\n",
			PTR_ERR(chip->role_sw));
		return PTR_ERR(chip->role_sw);
	}

	chip->cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	chip->cap.type = TYPEC_PORT_DRP;
	chip->cap.data = TYPEC_PORT_DRD;
	chip->cap.revision = USB_TYPEC_REV_1_1;

	chip->port = typec_register_port(chip->dev, &chip->cap);
	if (IS_ERR(chip->port)) {
		ret = PTR_ERR(chip->port);
		dev_err(dev, "Failed to register typec port: %ld\n",
			PTR_ERR(chip->port));
		goto out_role_switch_put;
	}

	INIT_WORK(&chip->check_cc_work, check_cc_work_handler);
	chip->work_queue = create_singlethread_workqueue("tusb322i-cc-wq");
	if (!chip->work_queue) {
		ret = -ENOMEM;
		goto out_typec_unregister;
	}

	ret = tusb322i_check_cc_state(chip);
	if (ret)
		dev_warn(chip->dev, "Could not read CC state: %d\n", ret);

	i2c_set_clientdata(client, chip);

	chip->irq = gpiod_to_irq(chip->irq_gpiod);
	if (chip->irq < 0) {
		dev_err(dev, "failed to get IRQ\n");
		ret = chip->irq;
		goto out_typec_unregister;
	}

	ret = devm_request_threaded_irq(dev, chip->irq, NULL,
					tusb322i_isr, TUSB322I_IRQF,
					"tusb322i-irq", chip);
	if (ret < 0) {
		dev_err(dev, "failed to request ID IRQ\n");
		goto out_typec_unregister;
	}

	return 0;

out_typec_unregister:
	typec_unregister_port(chip->port);
out_role_switch_put:
	usb_role_switch_put(chip->role_sw);
	return ret;
}

static int tusb322i_remove(struct i2c_client *client)
{
	struct tusb322i_chip *chip = i2c_get_clientdata(client);

	destroy_workqueue(chip->work_queue);
	typec_unregister_port(chip->port);
	usb_role_switch_put(chip->role_sw);

	return 0;
}

static const struct of_device_id tusb322i_dt_match[] = {
	{ .compatible = "ti,tusb322i" },
	{},
};
MODULE_DEVICE_TABLE(of, tusb322i_dt_match);

static const struct i2c_device_id tusb322i_i2c_device_id[] = {
	{ "tusb322i", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, tusb322i_i2c_device_id);

static struct i2c_driver tusb322i_driver = {
	.driver = {
		   .name = "tusb322i",
		   .of_match_table = of_match_ptr(tusb322i_dt_match),
		   },
	.probe = tusb322i_probe,
	.remove = tusb322i_remove,
	.id_table = tusb322i_i2c_device_id,
};
module_i2c_driver(tusb322i_driver);

MODULE_AUTHOR("Fabien Parent <fparent@baylibre.com>");
MODULE_DESCRIPTION("TI TUSB322I Type-C Controller Driver");
MODULE_LICENSE("GPL");
