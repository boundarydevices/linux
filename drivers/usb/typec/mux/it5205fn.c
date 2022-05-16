// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * ITE IT5205 Type-C USB alternate mode mux.
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

/* MUX power down register */
#define IT5205_REG_MUXPDR        0x10
#define IT5205_MUX_POWER_DOWN    BIT(0)

/* MUX control register */
#define IT5205_REG_MUXCR         0x11
#define IT5205_POLARITY_INVERTED BIT(4)
#define IT5205_DP_USB_CTRL_MASK  0x0f
#define IT5205_DP                0x0f
#define IT5205_DP_USB            0x03
#define IT5205_USB               0x07

struct it5205fn {
	struct i2c_client *client;
	struct typec_switch *sw;
	struct typec_mux *mux;
	struct mutex lock;
	u8 conf;
	struct regulator *type3v3;
};

static int it5205_read(struct it5205fn *it5205, uint8_t reg, u8 *data)
{
	int ret = 0;
	struct it5205fn *it = it5205;

	ret = i2c_smbus_read_byte_data(it->client, reg);
	if (data < 0) {
		dev_err(&it->client->dev, "Error I2C read :%d\n", ret);
		return ret;
	}
	*data = ret;
	return 0;
}

static int it5205_write(struct it5205fn *it5205, uint8_t reg, uint8_t val)
{
	return i2c_smbus_write_byte_data(it5205->client, reg, val);
}

static int it5205fn_set_conf(struct it5205fn *it, u8 new_conf)
{
	int ret = 0;

	if (it->conf == new_conf)
		return 0;

	ret = i2c_smbus_write_byte_data(it->client, IT5205_REG_MUXCR, new_conf);
	if (ret) {
		dev_err(&it->client->dev, "Error writing conf: %d\n", ret);
		return ret;
	}

	it->conf = new_conf;
	return 0;
}

static int it5205fn_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct it5205fn *it5205 = typec_switch_get_drvdata(sw);
	int ret;
	u8 new_conf = 0;
	u8 value = 0;

	dev_info(&it5205->client->dev, "%s orientation: %u\n",
		__func__, orientation);

	mutex_lock(&it5205->lock);
	it5205_read(it5205, IT5205_REG_MUXCR, &new_conf);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* enter sleep mode */
		new_conf = 0x0;
		break;
	case TYPEC_ORIENTATION_NORMAL:
		new_conf &= ~IT5205_POLARITY_INVERTED;
		break;
	case TYPEC_ORIENTATION_REVERSE:
		new_conf |= (IT5205_POLARITY_INVERTED);
		break;
	default:
		break;
	}
	ret = it5205fn_set_conf(it5205, new_conf);
	mutex_unlock(&it5205->lock);

	ret = it5205_read(it5205, IT5205_REG_MUXCR, &value);
	dev_info(&it5205->client->dev, "IT5205_REG_MUXCR = 0x%x\n", value);

	return 0;
}

static int it5205fn_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct it5205fn *it5205 = typec_mux_get_drvdata(mux);
	int ret = 0;
	u8 new_conf = 0;

	dev_info(&it5205->client->dev, "%s mode: %lu\n",
		__func__, state->mode);

	mutex_lock(&it5205->lock);

	it5205_read(it5205, IT5205_REG_MUXCR, &new_conf);
	new_conf &= ~IT5205_DP_USB_CTRL_MASK;

	switch (state->mode) {
	case TYPEC_STATE_SAFE:
		/* Do nothing */
		break;
	case TYPEC_STATE_USB:
		new_conf |= IT5205_USB;
		break;
	case TYPEC_DP_STATE_C:
	case TYPEC_DP_STATE_E:
		new_conf |= IT5205_DP;
		break;
	case TYPEC_DP_STATE_D:
	case TYPEC_DP_STATE_F:
		new_conf |= IT5205_DP_USB;
		break;
	default:
		break;
	}

	ret = it5205fn_set_conf(it5205, new_conf);
	mutex_unlock(&it5205->lock);

	return ret;
}

static int it5205fn_init(struct it5205fn *it5205)
{
	int ret = 0;
	struct it5205fn *it = it5205;
	u8 value = 0;

	/* bit[0]: mux power on, bit[7-1]: reserved. */
	ret = it5205_write(it, IT5205_REG_MUXPDR, 0);
	if (ret) {
		dev_err(&it->client->dev, "Error writing power on: %d\n", ret);
		return ret;
	}

	ret = it5205_read(it, IT5205_REG_MUXPDR, &value);
	dev_info(&it->client->dev, "IT5205_REG_MUXPDR = 0x%x\n", value);
	/*default set to USB mode = 0x07*/
	ret = it5205_write(it, IT5205_REG_MUXCR, 0x17);
	if (ret) {
		dev_err(&it->client->dev, "Error writing power on: %d\n", ret);
		return ret;
	}
	ret = it5205_read(it, IT5205_REG_MUXCR, &value);
	dev_info(&it->client->dev, "IT5205_REG_MUXCR = 0x%x\n", value);

	return ret;
}

static int it5205fn_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct it5205fn *it5205;
	struct typec_switch_desc sw_desc;
	struct typec_mux_desc mux_desc;
	int ret = 0;

	it5205 = devm_kzalloc(&client->dev, sizeof(*it5205), GFP_KERNEL);
	if (!it5205)
		return -ENOMEM;

	it5205->type3v3 = devm_regulator_get(dev, "type3v3");
	if (IS_ERR(it5205->type3v3)) {
		dev_err(dev, "failed to get type3v3\n");
		return -EPROBE_DEFER;
	}

	ret = regulator_set_voltage(it5205->type3v3, 3300000, 3300000);
	if (ret)
		dev_err(dev, "failed to set type3v3\n");

	ret = regulator_enable(it5205->type3v3);
	if (ret)
		dev_err(dev, "failed to enable type3v3\n");

	/*add some delay for it5205 power on ready*/
	mdelay(50);

	it5205->client = client;
	mutex_init(&it5205->lock);

	memset(&sw_desc, 0, sizeof(sw_desc));
	sw_desc.drvdata = it5205;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = it5205fn_switch_set;

	it5205->sw = typec_switch_register(dev, &sw_desc);

	if (IS_ERR(it5205->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(it5205->sw));
		return PTR_ERR(it5205->sw);
	}

	mux_desc.drvdata = it5205;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = it5205fn_mux_set;

	it5205->mux = typec_mux_register(dev, &mux_desc);
	
	if (IS_ERR(it5205->mux)) {
		typec_switch_unregister(it5205->sw);
		dev_err(dev, "Error registering typec mux: %ld\n",
			PTR_ERR(it5205->mux));
		return PTR_ERR(it5205->mux);
	}

	i2c_set_clientdata(client, it5205);
	ret = it5205fn_init(it5205);
	if (ret < 0) {
		typec_switch_unregister(it5205->sw);
		typec_mux_unregister(it5205->mux);
		dev_err(dev, "Error for IT5205FN init\n");
		return ret;
	}

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int it5205fn_remove(struct i2c_client *client)
{
	struct it5205fn *it5205 = i2c_get_clientdata(client);

	typec_switch_unregister(it5205->sw);
	return 0;
}

static const struct i2c_device_id it5205fn_table[] = {
	{ "it5205fn" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, it5205fn_table);

static const struct of_device_id mt_it5205_match_table[] = {
	{.compatible = "mediatek,it5205fn",},
	{},
};
MODULE_DEVICE_TABLE(of, mt_it5205_match_table);


static struct i2c_driver it5205fn_driver = {
	.driver = {
		.name = "it5205fn",
		.owner = THIS_MODULE,
		.of_match_table = mt_it5205_match_table,
	},
	.probe	= it5205fn_probe,
	.remove		= it5205fn_remove,
	.id_table	= it5205fn_table,
};

module_i2c_driver(it5205fn_driver);

MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("ITE IT5205FN Alternate mode driver");
MODULE_LICENSE("GPL");
