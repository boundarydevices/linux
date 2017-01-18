/*
 * Multiplexer subsystem
 *
 * Copyright (C) 2017 Axentia Technologies AB
 *
 * Author: Peter Rosin <peda@axentia.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "mux-core: " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mux.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

/*
 * The idle-as-is "state" is not an actual state that may be selected, it
 * only implies that the state should not be changed. So, use that state
 * as indication that the cached state of the multiplexer is unknown.
 */
#define MUX_CACHE_UNKNOWN MUX_IDLE_AS_IS

static struct class mux_class = {
	.name = "mux",
	.owner = THIS_MODULE,
};

static int __init mux_init(void)
{
	return class_register(&mux_class);
}

static DEFINE_IDA(mux_ida);

static void mux_chip_release(struct device *dev)
{
	struct mux_chip *mux_chip = to_mux_chip(dev);

	ida_simple_remove(&mux_ida, mux_chip->id);
	kfree(mux_chip);
}

static struct device_type mux_type = {
	.name = "mux-chip",
	.release = mux_chip_release,
};

struct mux_chip *mux_chip_alloc(struct device *dev,
				unsigned int controllers, size_t sizeof_priv)
{
	struct mux_chip *mux_chip;
	int i;

	if (WARN_ON(!dev || !controllers))
		return NULL;

	mux_chip = kzalloc(sizeof(*mux_chip) +
			   controllers * sizeof(*mux_chip->mux) +
			   sizeof_priv, GFP_KERNEL);
	if (!mux_chip)
		return NULL;

	mux_chip->mux = (struct mux_control *)(mux_chip + 1);
	mux_chip->dev.class = &mux_class;
	mux_chip->dev.type = &mux_type;
	mux_chip->dev.parent = dev;
	mux_chip->dev.of_node = dev->of_node;
	dev_set_drvdata(&mux_chip->dev, mux_chip);

	mux_chip->id = ida_simple_get(&mux_ida, 0, 0, GFP_KERNEL);
	if (mux_chip->id < 0) {
		pr_err("muxchipX failed to get a device id\n");
		kfree(mux_chip);
		return NULL;
	}
	dev_set_name(&mux_chip->dev, "muxchip%d", mux_chip->id);

	mux_chip->controllers = controllers;
	for (i = 0; i < controllers; ++i) {
		struct mux_control *mux = &mux_chip->mux[i];

		mux->chip = mux_chip;
		init_rwsem(&mux->lock);
		mux->cached_state = MUX_CACHE_UNKNOWN;
		mux->idle_state = MUX_IDLE_AS_IS;
	}

	device_initialize(&mux_chip->dev);

	return mux_chip;
}
EXPORT_SYMBOL_GPL(mux_chip_alloc);

static int mux_control_set(struct mux_control *mux, int state)
{
	int ret = mux->chip->ops->set(mux, state);

	mux->cached_state = ret < 0 ? MUX_CACHE_UNKNOWN : state;

	return ret;
}

int mux_chip_register(struct mux_chip *mux_chip)
{
	int i;
	int ret;

	for (i = 0; i < mux_chip->controllers; ++i) {
		struct mux_control *mux = &mux_chip->mux[i];

		if (mux->idle_state == mux->cached_state)
			continue;

		ret = mux_control_set(mux, mux->idle_state);
		if (ret < 0) {
			dev_err(&mux_chip->dev, "unable to set idle state\n");
			return ret;
		}
	}

	ret = device_add(&mux_chip->dev);
	if (ret < 0)
		dev_err(&mux_chip->dev,
			"device_add failed in mux_chip_register: %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(mux_chip_register);

void mux_chip_unregister(struct mux_chip *mux_chip)
{
	device_del(&mux_chip->dev);
}
EXPORT_SYMBOL_GPL(mux_chip_unregister);

void mux_chip_free(struct mux_chip *mux_chip)
{
	if (!mux_chip)
		return;

	put_device(&mux_chip->dev);
}
EXPORT_SYMBOL_GPL(mux_chip_free);

static void devm_mux_chip_release(struct device *dev, void *res)
{
	struct mux_chip *mux_chip = *(struct mux_chip **)res;

	mux_chip_free(mux_chip);
}

struct mux_chip *devm_mux_chip_alloc(struct device *dev,
				     unsigned int controllers,
				     size_t sizeof_priv)
{
	struct mux_chip **ptr, *mux_chip;

	ptr = devres_alloc(devm_mux_chip_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	mux_chip = mux_chip_alloc(dev, controllers, sizeof_priv);
	if (!mux_chip) {
		devres_free(ptr);
		return NULL;
	}

	*ptr = mux_chip;
	devres_add(dev, ptr);

	return mux_chip;
}
EXPORT_SYMBOL_GPL(devm_mux_chip_alloc);

static int devm_mux_chip_match(struct device *dev, void *res, void *data)
{
	struct mux_chip **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

void devm_mux_chip_free(struct device *dev, struct mux_chip *mux_chip)
{
	WARN_ON(devres_release(dev, devm_mux_chip_release,
			       devm_mux_chip_match, mux_chip));
}
EXPORT_SYMBOL_GPL(devm_mux_chip_free);

static void devm_mux_chip_reg_release(struct device *dev, void *res)
{
	struct mux_chip *mux_chip = *(struct mux_chip **)res;

	mux_chip_unregister(mux_chip);
}

int devm_mux_chip_register(struct device *dev,
			   struct mux_chip *mux_chip)
{
	struct mux_chip **ptr;
	int res;

	ptr = devres_alloc(devm_mux_chip_reg_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	res = mux_chip_register(mux_chip);
	if (res) {
		devres_free(ptr);
		return res;
	}

	*ptr = mux_chip;
	devres_add(dev, ptr);

	return res;
}
EXPORT_SYMBOL_GPL(devm_mux_chip_register);

void devm_mux_chip_unregister(struct device *dev, struct mux_chip *mux_chip)
{
	WARN_ON(devres_release(dev, devm_mux_chip_reg_release,
			       devm_mux_chip_match, mux_chip));
}
EXPORT_SYMBOL_GPL(devm_mux_chip_unregister);

int mux_control_select(struct mux_control *mux, int state)
{
	int ret;

	if (down_read_trylock(&mux->lock)) {
		if (mux->cached_state == state)
			return 0;

		/* Sigh, the mux needs updating... */
		up_read(&mux->lock);
	}

	/* ...or it's just contended. */
	down_write(&mux->lock);

	if (mux->cached_state == state) {
		/*
		 * Hmmm, someone else changed the mux to my liking.
		 * That makes me wonder how long I waited for nothing?
		 */
		downgrade_write(&mux->lock);
		return 0;
	}

	ret = mux_control_set(mux, state);
	if (ret < 0) {
		if (mux->idle_state != MUX_IDLE_AS_IS)
			mux_control_set(mux, mux->idle_state);

		up_write(&mux->lock);
		return ret;
	}

	downgrade_write(&mux->lock);

	return 1;
}
EXPORT_SYMBOL_GPL(mux_control_select);

int mux_control_deselect(struct mux_control *mux)
{
	int ret = 0;

	if (mux->idle_state != MUX_IDLE_AS_IS &&
	    mux->idle_state != mux->cached_state)
		ret = mux_control_set(mux, mux->idle_state);

	up_read(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mux_control_deselect);

static int of_dev_node_match(struct device *dev, const void *data)
{
	return dev->of_node == data;
}

static struct mux_chip *of_find_mux_chip_by_node(struct device_node *np)
{
	struct device *dev;

	dev = class_find_device(&mux_class, NULL, np, of_dev_node_match);

	return dev ? to_mux_chip(dev) : NULL;
}

struct mux_control *mux_control_get(struct device *dev, const char *mux_name)
{
	struct device_node *np = dev->of_node;
	struct of_phandle_args args;
	struct mux_chip *mux_chip;
	unsigned int controller;
	int index = 0;
	int ret;

	if (mux_name) {
		index = of_property_match_string(np, "mux-control-names",
						 mux_name);
		if (index < 0) {
			dev_err(dev, "mux controller '%s' not found\n",
				mux_name);
			return ERR_PTR(index);
		}
	}

	ret = of_parse_phandle_with_args(np,
					 "mux-controls", "#mux-control-cells",
					 index, &args);
	if (ret) {
		dev_err(dev, "%s: failed to get mux-control %s(%i)\n",
			np->full_name, mux_name ?: "", index);
		return ERR_PTR(ret);
	}

	mux_chip = of_find_mux_chip_by_node(args.np);
	of_node_put(args.np);
	if (!mux_chip)
		return ERR_PTR(-EPROBE_DEFER);

	if (args.args_count > 1 ||
	    (!args.args_count && (mux_chip->controllers > 1))) {
		dev_err(dev, "%s: wrong #mux-control-cells for %s\n",
			np->full_name, args.np->full_name);
		return ERR_PTR(-EINVAL);
	}

	controller = 0;
	if (args.args_count)
		controller = args.args[0];

	if (controller >= mux_chip->controllers) {
		dev_err(dev, "%s: bad mux controller %u specified in %s\n",
			np->full_name, controller, args.np->full_name);
		return ERR_PTR(-EINVAL);
	}

	get_device(&mux_chip->dev);
	return &mux_chip->mux[controller];
}
EXPORT_SYMBOL_GPL(mux_control_get);

void mux_control_put(struct mux_control *mux)
{
	put_device(&mux->chip->dev);
}
EXPORT_SYMBOL_GPL(mux_control_put);

static void devm_mux_control_release(struct device *dev, void *res)
{
	struct mux_control *mux = *(struct mux_control **)res;

	mux_control_put(mux);
}

struct mux_control *devm_mux_control_get(struct device *dev,
					 const char *mux_name)
{
	struct mux_control **ptr, *mux;

	ptr = devres_alloc(devm_mux_control_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mux = mux_control_get(dev, mux_name);
	if (IS_ERR(mux)) {
		devres_free(ptr);
		return mux;
	}

	*ptr = mux;
	devres_add(dev, ptr);

	return mux;
}
EXPORT_SYMBOL_GPL(devm_mux_control_get);

static int devm_mux_control_match(struct device *dev, void *res, void *data)
{
	struct mux_control **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

void devm_mux_control_put(struct device *dev, struct mux_control *mux)
{
	WARN_ON(devres_release(dev, devm_mux_control_release,
			       devm_mux_control_match, mux));
}
EXPORT_SYMBOL_GPL(devm_mux_control_put);

/*
 * Using subsys_initcall instead of module_init here to ensure - for the
 * non-modular case - that the subsystem is initialized when mux consumers
 * and mux controllers start to use it /without/ relying on link order.
 * For the modular case, the ordering is ensured with module dependencies.
 */
subsys_initcall(mux_init);

MODULE_DESCRIPTION("Multiplexer subsystem");
MODULE_AUTHOR("Peter Rosin <peda@axentia.se>");
MODULE_LICENSE("GPL v2");
