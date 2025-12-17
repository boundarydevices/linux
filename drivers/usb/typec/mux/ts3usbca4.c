// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 *
 * TI TS3USBCA4 Type-C SBU mux.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_dp.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/tcpci.h>

#define TS3USBCA4_REG_G1	0xa
#define TS3USBCA4_REG_G2	0xb
#define TS3USBCA4_EN		BIT(1)
#define TS3USBCA4_FLIPSEL(x)	((x) & 0x1)
#define TS3USBCA4_SWSEL		0x3	// SBU to LnB

struct ts3usbca4 {
	struct i2c_client *client;
	struct typec_switch_dev *sw;
	struct mutex lock;
	struct regulator *mux3v3;
	bool mux3v3_enabled;
};

static int ts3usbca4_init(struct ts3usbca4 *ts3usbca4, uint8_t flip)
{
	int ret = 0;

	ret = i2c_smbus_write_byte_data(ts3usbca4->client, TS3USBCA4_REG_G1,
					TS3USBCA4_EN | TS3USBCA4_FLIPSEL(flip));
	if (ret)
		return ret;

	ret = i2c_smbus_write_byte_data(ts3usbca4->client, TS3USBCA4_REG_G2,
					TS3USBCA4_SWSEL);
	return ret;
}

static int ts3usbca4_switch_set(struct typec_switch_dev *sw,
			      enum typec_orientation orientation)
{
	struct ts3usbca4 *ts3usbca4 = typec_switch_get_drvdata(sw);
	int ret = 0;

	dev_info(&ts3usbca4->client->dev, "%s %d\n", __func__, orientation);

	mutex_lock(&ts3usbca4->lock);
	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		if (ts3usbca4->mux3v3_enabled) {
			ret = regulator_disable(ts3usbca4->mux3v3);
			ts3usbca4->mux3v3_enabled = false;
		}
		break;
	case TYPEC_ORIENTATION_NORMAL:
		if (!ts3usbca4->mux3v3_enabled) {
			ret = regulator_enable(ts3usbca4->mux3v3);
			usleep_range(100, 500);

			if (!ret)
				ret = ts3usbca4_init(ts3usbca4, 1);
			ts3usbca4->mux3v3_enabled = true;
		}
		break;
	case TYPEC_ORIENTATION_REVERSE:
		if (!ts3usbca4->mux3v3_enabled) {
			ret = regulator_enable(ts3usbca4->mux3v3);
			usleep_range(100, 500);

			if (!ret)
				ret = ts3usbca4_init(ts3usbca4, 0);
			ts3usbca4->mux3v3_enabled = true;
		}
		break;
	default:
		break;
	}
	mutex_unlock(&ts3usbca4->lock);

	return ret;
}
static int ts3usbca4_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ts3usbca4 *ts3usbca4;
	struct typec_switch_desc sw_desc = {};
	int ret = 0;

	ts3usbca4 = devm_kzalloc(&client->dev, sizeof(*ts3usbca4), GFP_KERNEL);
	if (!ts3usbca4)
		return -ENOMEM;

	ts3usbca4->mux3v3 = devm_regulator_get(dev, "mux3v3");
	if (IS_ERR(ts3usbca4->mux3v3)) {
		dev_info(dev, "failed to get mux3v3\n");
		return -EPROBE_DEFER;
	}

	if (regulator_enable(ts3usbca4->mux3v3))
		dev_info(dev, "failed to enable mux3v3\n");

	ts3usbca4->client = client;
	i2c_set_clientdata(client, ts3usbca4);
	usleep_range(100, 500);
	ret = ts3usbca4_init(ts3usbca4, 0);
	regulator_disable(ts3usbca4->mux3v3);
	ts3usbca4->mux3v3_enabled = false;
	if (ret < 0) {
		dev_info(dev, "Probe failed!\n");
		return ret;
	}

	mutex_init(&ts3usbca4->lock);

	sw_desc.drvdata = ts3usbca4;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = ts3usbca4_switch_set;

	ts3usbca4->sw = typec_switch_register(dev, &sw_desc);

	if (IS_ERR(ts3usbca4->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(ts3usbca4->sw));
		return PTR_ERR(ts3usbca4->sw);
	}

	dev_info(dev, "%s probe OK !\n", __func__);

	return ret;
}

static void ts3usbca4_remove(struct i2c_client *client)
{
	struct ts3usbca4 *ts3usbca4 = i2c_get_clientdata(client);

	typec_switch_unregister(ts3usbca4->sw);
}

static const struct i2c_device_id ts3usbca4_table[] = {
	{ "ts3usbca4" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ts3usbca4_table);

static const struct of_device_id mt_ts3usbca4_match_table[] = {
	{.compatible = "ti,ts3usbca4",},
	{},
};

static struct i2c_driver ts3usbca4_driver = {
	.driver = {
		.name = "ts3usbca4",
		.owner = THIS_MODULE,
		.of_match_table = mt_ts3usbca4_match_table,
	},
	.probe	= ts3usbca4_probe,
	.remove		= ts3usbca4_remove,
	.id_table	= ts3usbca4_table,
};

module_i2c_driver(ts3usbca4_driver);

MODULE_AUTHOR("Chris Chen <chris-qj.chen@mediatek.com>");
MODULE_DESCRIPTION("TI TS3USBCA4 SBU MUX driver");
MODULE_LICENSE("GPL");
