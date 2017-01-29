/*
 * Multiplexer driver for Analog Devices ADG792A/G Triple 4:1 mux
 *
 * Copyright (C) 2017 Axentia Technologies AB
 *
 * Author: Peter Rosin <peda@axentia.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mux.h>

#define ADG792A_LDSW		BIT(0)
#define ADG792A_RESET		BIT(1)
#define ADG792A_DISABLE(mux)	(0x50 | (mux))
#define ADG792A_DISABLE_ALL	(0x5f)
#define ADG792A_MUX(mux, state)	(0xc0 | (((mux) + 1) << 2) | (state))
#define ADG792A_MUX_ALL(state)	(0xc0 | (state))

static int adg792a_set(struct mux_control *mux, int state)
{
	struct i2c_client *i2c = to_i2c_client(mux->chip->dev.parent);
	u8 cmd;

	if (mux->chip->controllers == 1) {
		/* parallel mux controller operation */
		if (state == MUX_IDLE_DISCONNECT)
			cmd = ADG792A_DISABLE_ALL;
		else
			cmd = ADG792A_MUX_ALL(state);
	} else {
		unsigned int controller = mux_control_get_index(mux);

		if (state == MUX_IDLE_DISCONNECT)
			cmd = ADG792A_DISABLE(controller);
		else
			cmd = ADG792A_MUX(controller, state);
	}

	return i2c_smbus_write_byte_data(i2c, cmd, ADG792A_LDSW);
}

static const struct mux_control_ops adg792a_ops = {
	.set = adg792a_set,
};

static int adg792a_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct mux_chip *mux_chip;
	u32 cells;
	int ret;
	int i;

	ret = of_property_read_u32(dev->of_node, "#mux-control-cells", &cells);
	if (ret < 0)
		return ret;
	if (cells >= 2)
		return -EINVAL;

	mux_chip = devm_mux_chip_alloc(dev, cells ? 3 : 1, 0);
	if (!mux_chip)
		return -ENOMEM;

	mux_chip->ops = &adg792a_ops;

	ret = i2c_smbus_write_byte_data(i2c, ADG792A_DISABLE_ALL,
					ADG792A_RESET | ADG792A_LDSW);
	if (ret < 0)
		return ret;

	for (i = 0; i < mux_chip->controllers; ++i) {
		struct mux_control *mux = &mux_chip->mux[i];
		s32 idle_state;

		mux->states = 4;

		ret = of_property_read_u32_index(dev->of_node, "idle-state", i,
						 (s32 *)&idle_state);
		if (ret < 0)
			continue;

		switch (idle_state) {
		case 0 ... 4:
		case MUX_IDLE_DISCONNECT:
			mux_chip->mux[i].idle_state = idle_state;
			break;
		case MUX_IDLE_AS_IS:
			break;
		default:
			dev_err(dev, "invalid idle-state %d\n", idle_state);
			return -EINVAL;
		}
	}

	ret = devm_mux_chip_register(dev, mux_chip);
	if (ret < 0)
		return ret;

	if (cells)
		dev_info(dev, "3x single pole quadruple throw muxes registered\n");
	else
		dev_info(dev, "triple pole quadruple throw mux registered\n");

	return 0;
}

static const struct i2c_device_id adg792a_id[] = {
	{ .name = "adg792a", },
	{ .name = "adg792g", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adg792a_id);

static const struct of_device_id adg792a_of_match[] = {
	{ .compatible = "adi,adg792a", },
	{ .compatible = "adi,adg792g", },
	{ }
};
MODULE_DEVICE_TABLE(of, adg792a_of_match);

static struct i2c_driver adg792a_driver = {
	.driver		= {
		.name		= "adg792a",
		.of_match_table = of_match_ptr(adg792a_of_match),
	},
	.probe		= adg792a_probe,
	.id_table	= adg792a_id,
};
module_i2c_driver(adg792a_driver);

MODULE_DESCRIPTION("Analog Devices ADG792A/G Triple 4:1 mux driver");
MODULE_AUTHOR("Peter Rosin <peda@axentia.se>");
MODULE_LICENSE("GPL v2");
