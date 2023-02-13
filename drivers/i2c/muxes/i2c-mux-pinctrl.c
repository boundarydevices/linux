// SPDX-License-Identifier: GPL-2.0-only
/*
 * I2C multiplexer using pinctrl API
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include "../../pinctrl/core.h"


struct i2c_mux_pinctrl {
	unsigned cur_channel;
	unsigned num_channels;
	unsigned idle_channel;
	struct pinctrl *pinctrl;
	struct pinctrl_state *ps_idle;
	struct i2c_bus_recovery_info rinfo[];
};

static void i2c_mux_pinctrl_to_gpio(struct i2c_mux_pinctrl *mux, u32 chan)
{
	struct i2c_bus_recovery_info *rinfo;

	if (chan < mux->num_channels) {
		/* Move away from i2c function, so lines won't toggle */
		rinfo = &mux->rinfo[chan];
		if (rinfo->pins_gpio)
			pinctrl_commit_state(rinfo->pinctrl, rinfo->pins_gpio);
	}
}

static int i2c_mux_pinctrl_select(struct i2c_mux_core *muxc, u32 chan)
{
	struct i2c_mux_pinctrl *mux = i2c_mux_priv(muxc);
	struct i2c_bus_recovery_info *rinfo;

	if (chan == mux->cur_channel)
		return 0;
	if (chan >= mux->num_channels)
		return -EINVAL;

	i2c_mux_pinctrl_to_gpio(mux, mux->cur_channel);
	mux->cur_channel = chan;
	rinfo = &mux->rinfo[chan];
	if (rinfo->pins_gpio)
		muxc->parent->bus_recovery_info = rinfo;
	else
		muxc->parent->bus_recovery_info = NULL;
	return pinctrl_commit_state(rinfo->pinctrl, rinfo->pins_default);
}

static int i2c_mux_pinctrl_deselect(struct i2c_mux_core *muxc, u32 prev_chan)
{
	struct i2c_mux_pinctrl *mux = i2c_mux_priv(muxc);
	struct i2c_bus_recovery_info *rinfo;
	int chan;

	chan = mux->idle_channel;
	if (chan == mux->cur_channel)
		return 0;

	i2c_mux_pinctrl_to_gpio(mux, prev_chan);
	mux->cur_channel = chan;
	if (chan < mux->num_channels) {
		rinfo = &mux->rinfo[chan];

		if (rinfo->pins_gpio)
			muxc->parent->bus_recovery_info = rinfo;
		else
			muxc->parent->bus_recovery_info = NULL;
		return pinctrl_commit_state(rinfo->pinctrl, rinfo->pins_default);
	}
	muxc->parent->bus_recovery_info = NULL;
	if (mux->ps_idle)
		return pinctrl_commit_state(mux->pinctrl, mux->ps_idle);
	return 0;
}

int setup_base(struct device *dev, struct i2c_mux_pinctrl *mux, int num_names)
{
	struct device_node *np = dev->of_node;
	struct pinctrl_state *ps;
	const char *name;
	int idle;
	int ret, i;

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(np, "pinctrl-names", i,
						    &name);
		if (ret < 0) {
			dev_err(dev, "Cannot parse pinctrl-names: %d\n", ret);
			return -EINVAL;
		}
		idle = 0;
		if (!strcmp(name, "idle")) {
			idle = 1;
			if (i != num_names - 1) {
				dev_warn(dev, "idle state must be last\n");
				continue;
			}
			if (mux->idle_channel < mux->num_channels) {
				dev_warn(dev,
					"idle state already given to channel %d\n",
					mux->idle_channel);
				continue;
			}
		} else  if (i < mux->num_channels) {
			if (mux->rinfo[i].pins_default)
				continue;
		}

		ps = pinctrl_lookup_state(mux->pinctrl, name);
		if (IS_ERR(ps)) {
			ret = PTR_ERR(ps);
			dev_err(dev, "Cannot look up pinctrl state %s: %d\n",
				name, ret);
			return ret;
		}
		if (!idle) {
			mux->rinfo[i].pinctrl = mux->pinctrl;
			mux->rinfo[i].pins_default = ps;
			mux->rinfo[i].pins_gpio = NULL;
			continue;
		}
		mux->ps_idle = ps;
	}
	return 0;
}

static int setup_adap(struct i2c_adapter *adap, struct i2c_mux_core *muxc, int chan)
{
	struct i2c_mux_pinctrl *mux = i2c_mux_priv(muxc);
	struct i2c_bus_recovery_info *rinfo = &mux->rinfo[chan];
	struct device *dev = &adap->dev;
	struct pinctrl *pinctrl = devm_pinctrl_get(dev);

	if (!pinctrl || IS_ERR(pinctrl))
		return 0;

	if (device_property_read_bool(dev, "idle")) {
		if (mux->idle_channel < mux->num_channels) {
			dev_warn(dev, "idle already set to %d\n",
				mux->idle_channel);
		} else {
			mux->idle_channel = chan;
		}
	}
	rinfo->pinctrl = pinctrl;
	rinfo->recover_bus = i2c_generic_scl_recovery;
	adap->bus_recovery_info = rinfo;
	return i2c_init_recovery(adap);
}

static struct i2c_adapter *i2c_mux_pinctrl_root_adapter(
	struct pinctrl_state *state)
{
	struct i2c_adapter *root = NULL;
	struct pinctrl_setting *setting;
	struct i2c_adapter *pin_root;

	list_for_each_entry(setting, &state->settings, node) {
		pin_root = i2c_root_adapter(setting->pctldev->dev);
		if (!pin_root)
			return NULL;
		if (!root)
			root = pin_root;
		else if (root != pin_root)
			return NULL;
	}

	return root;
}

static struct i2c_adapter *i2c_mux_pinctrl_parent_adapter(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *parent_np;
	struct i2c_adapter *parent;

	parent_np = of_parse_phandle(np, "i2c-parent", 0);
	if (!parent_np) {
		dev_err(dev, "Cannot parse i2c-parent\n");
		return ERR_PTR(-ENODEV);
	}
	parent = of_find_i2c_adapter_by_node(parent_np);
	of_node_put(parent_np);
	if (!parent)
		return ERR_PTR(-EPROBE_DEFER);

	return parent;
}

static int i2c_mux_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct i2c_mux_core *muxc;
	struct i2c_mux_pinctrl *mux;
	struct i2c_adapter *parent;
	struct i2c_adapter *root;
	struct pinctrl *pinctrl;
	int num_names, i, ret;
	struct device_node *child;
	unsigned max_reg = 0;
	unsigned reg;
	unsigned num_channels;
	bool mux_locked;

	for_each_child_of_node(np, child) {
		ret = of_property_read_u32(child, "reg", &reg);
		if (ret)
			continue;
		if (max_reg < reg)
			max_reg = reg;
	}
	num_channels = max_reg + 1;


	parent = i2c_mux_pinctrl_parent_adapter(dev);
	if (IS_ERR(parent))
		return PTR_ERR(parent);

	muxc = i2c_mux_alloc(parent, dev, num_channels,
			     struct_size(mux, rinfo, num_channels),
			     0, i2c_mux_pinctrl_select, NULL);
	if (!muxc) {
		ret = -ENOMEM;
		goto err_put_parent;
	}
	mux = i2c_mux_priv(muxc);
	mux->idle_channel = -2;
	mux->cur_channel = -1;
	mux->num_channels = num_channels;

	platform_set_drvdata(pdev, muxc);

	for (i = 0; i < num_channels; i++) {
		struct i2c_adapter *adap = i2c_mux_add_adapter_start(muxc, i, 0, -1);

		if (IS_ERR(adap))
			return PTR_ERR(adap);
		ret = setup_adap(adap, muxc, i);
		i2c_mux_pinctrl_to_gpio(mux, i);
		mux->cur_channel = -1;
		if (ret)
			goto err_del_adapter;
	}

	pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(mux->pinctrl)) {
		mux->pinctrl = pinctrl;
		num_names = of_property_count_strings(np, "pinctrl-names");
		if (num_names > 0) {
			ret = setup_base(dev, mux, num_names);
			if (ret)
				goto err_del_adapter;
		}
	}
	if ((mux->idle_channel < mux->num_channels) || mux->ps_idle)
		muxc->deselect = i2c_mux_pinctrl_deselect;

	for (i = 0; i < num_channels; i++) {
		if (!mux->rinfo[i].pins_default) {
			ret = -EINVAL;
			goto err_del_adapter;
		}
	}

	mux_locked = true;
	root = i2c_root_adapter(&muxc->parent->dev);
	for (i = 0; i < num_channels; i++) {
		if (root != i2c_mux_pinctrl_root_adapter(mux->rinfo[i].pins_default)) {
			mux_locked = false;
			break;
		}
	}

	if (mux->ps_idle) {
		if (root != i2c_mux_pinctrl_root_adapter(mux->ps_idle))
			mux_locked = false;
	}
	if (mux_locked) {
		i2c_mux_add_adapters_set_locked(muxc, mux_locked);
		dev_info(dev, "mux-locked i2c mux\n");
	}
	for (i = 0; i < num_channels; i++) {
		struct i2c_adapter *adap = muxc->adapter[i];

		ret = i2c_mux_add_adapter_finish(adap);
		i2c_mux_pinctrl_to_gpio(mux, i);
		mux->cur_channel = -1;
		if (ret)
			goto err_del_adapter;
	}

	return 0;

err_del_adapter:
	i2c_mux_del_adapters(muxc);
err_put_parent:
	i2c_put_adapter(parent);

	return ret;
}

static int i2c_mux_pinctrl_remove(struct platform_device *pdev)
{
	struct i2c_mux_core *muxc = platform_get_drvdata(pdev);

	i2c_mux_del_adapters(muxc);
	i2c_put_adapter(muxc->parent);

	return 0;
}

static const struct of_device_id i2c_mux_pinctrl_of_match[] = {
	{ .compatible = "i2c-mux-pinctrl", },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_mux_pinctrl_of_match);

static struct platform_driver i2c_mux_pinctrl_driver = {
	.driver	= {
		.name	= "i2c-mux-pinctrl",
		.of_match_table = i2c_mux_pinctrl_of_match,
	},
	.probe	= i2c_mux_pinctrl_probe,
	.remove	= i2c_mux_pinctrl_remove,
};
module_platform_driver(i2c_mux_pinctrl_driver);

MODULE_DESCRIPTION("pinctrl-based I2C multiplexer driver");
MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:i2c-mux-pinctrl");
