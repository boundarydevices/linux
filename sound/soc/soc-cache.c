/*
 * soc-cache.c  --  ASoC register cache helpers
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <sound/soc.h>
#include <linux/bitmap.h>
#include <linux/rbtree.h>
#include <linux/export.h>

#include <trace/events/asoc.h>

static bool snd_soc_set_cache_val(void *base, unsigned int idx,
				  unsigned int val, unsigned int word_size)
{
	switch (word_size) {
	case 1: {
		u8 *cache = base;
		if (cache[idx] == val)
			return true;
		cache[idx] = val;
		break;
	}
	case 2: {
		u16 *cache = base;
		if (cache[idx] == val)
			return true;
		cache[idx] = val;
		break;
	}
	default:
		BUG();
	}
	return false;
}

static unsigned int snd_soc_get_cache_val(const void *base, unsigned int idx,
		unsigned int word_size)
{
	if (!base)
		return -1;

	switch (word_size) {
	case 1: {
		const u8 *cache = base;
		return cache[idx];
	}
	case 2: {
		const u16 *cache = base;
		return cache[idx];
	}
	default:
		BUG();
	}
	/* unreachable */
	return -1;
}

struct snd_soc_rbtree_node {
	struct rb_node node;
	unsigned int reg;
	unsigned int value;
	unsigned int defval;
} __attribute__ ((packed));

struct snd_soc_rbtree_ctx {
	struct rb_root root;
};

static struct snd_soc_rbtree_node *snd_soc_rbtree_lookup(
	struct rb_root *root, unsigned int reg)
{
	struct rb_node *node;
	struct snd_soc_rbtree_node *rbnode;

	node = root->rb_node;
	while (node) {
		rbnode = container_of(node, struct snd_soc_rbtree_node, node);
		if (rbnode->reg < reg)
			node = node->rb_left;
		else if (rbnode->reg > reg)
			node = node->rb_right;
		else
			return rbnode;
	}

	return NULL;
}

static int snd_soc_rbtree_insert(struct rb_root *root,
				 struct snd_soc_rbtree_node *rbnode)
{
	struct rb_node **new, *parent;
	struct snd_soc_rbtree_node *rbnode_tmp;

	parent = NULL;
	new = &root->rb_node;
	while (*new) {
		rbnode_tmp = container_of(*new, struct snd_soc_rbtree_node,
					  node);
		parent = *new;
		if (rbnode_tmp->reg < rbnode->reg)
			new = &((*new)->rb_left);
		else if (rbnode_tmp->reg > rbnode->reg)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* insert the node into the rbtree */
	rb_link_node(&rbnode->node, parent, new);
	rb_insert_color(&rbnode->node, root);

	return 1;
}

static int snd_soc_rbtree_cache_sync(struct snd_soc_codec *codec)
{
	struct snd_soc_rbtree_ctx *rbtree_ctx;
	struct rb_node *node;
	struct snd_soc_rbtree_node *rbnode;
	unsigned int val;
	int ret;

	rbtree_ctx = codec->reg_cache;
	for (node = rb_first(&rbtree_ctx->root); node; node = rb_next(node)) {
		rbnode = rb_entry(node, struct snd_soc_rbtree_node, node);
		if (rbnode->value == rbnode->defval)
			continue;
		WARN_ON(codec->writable_register &&
			codec->writable_register(codec, rbnode->reg));
		ret = snd_soc_cache_read(codec, rbnode->reg, &val);
		if (ret)
			return ret;
		codec->cache_bypass = 1;
		ret = snd_soc_write(codec, rbnode->reg, val);
		codec->cache_bypass = 0;
		if (ret)
			return ret;
		dev_dbg(codec->dev, "Synced register %#x, value = %#x\n",
			rbnode->reg, val);
	}

	return 0;
}

static int snd_soc_rbtree_cache_write(struct snd_soc_codec *codec,
				      unsigned int reg, unsigned int value)
{
	struct snd_soc_rbtree_ctx *rbtree_ctx;
	struct snd_soc_rbtree_node *rbnode;

	rbtree_ctx = codec->reg_cache;
	rbnode = snd_soc_rbtree_lookup(&rbtree_ctx->root, reg);
	if (rbnode) {
		if (rbnode->value == value)
			return 0;
		rbnode->value = value;
	} else {
		/* bail out early, no need to create the rbnode yet */
		if (!value)
			return 0;
		/*
		 * for uninitialized registers whose value is changed
		 * from the default zero, create an rbnode and insert
		 * it into the tree.
		 */
		rbnode = kzalloc(sizeof *rbnode, GFP_KERNEL);
		if (!rbnode)
			return -ENOMEM;
		rbnode->reg = reg;
		rbnode->value = value;
		snd_soc_rbtree_insert(&rbtree_ctx->root, rbnode);
	}

	return 0;
}

static int snd_soc_rbtree_cache_read(struct snd_soc_codec *codec,
				     unsigned int reg, unsigned int *value)
{
	struct snd_soc_rbtree_ctx *rbtree_ctx;
	struct snd_soc_rbtree_node *rbnode;

	rbtree_ctx = codec->reg_cache;
	rbnode = snd_soc_rbtree_lookup(&rbtree_ctx->root, reg);
	if (rbnode) {
		*value = rbnode->value;
	} else {
		/* uninitialized registers default to 0 */
		*value = 0;
	}

	return 0;
}

static int snd_soc_rbtree_cache_exit(struct snd_soc_codec *codec)
{
	struct rb_node *next;
	struct snd_soc_rbtree_ctx *rbtree_ctx;
	struct snd_soc_rbtree_node *rbtree_node;

	/* if we've already been called then just return */
	rbtree_ctx = codec->reg_cache;
	if (!rbtree_ctx)
		return 0;

	/* free up the rbtree */
	next = rb_first(&rbtree_ctx->root);
	while (next) {
		rbtree_node = rb_entry(next, struct snd_soc_rbtree_node, node);
		next = rb_next(&rbtree_node->node);
		rb_erase(&rbtree_node->node, &rbtree_ctx->root);
		kfree(rbtree_node);
	}

	/* release the resources */
	kfree(codec->reg_cache);
	codec->reg_cache = NULL;

	return 0;
}

static int snd_soc_rbtree_cache_init(struct snd_soc_codec *codec)
{
	struct snd_soc_rbtree_node *rbtree_node;
	struct snd_soc_rbtree_ctx *rbtree_ctx;
	unsigned int val;
	unsigned int word_size;
	int i;
	int ret;

	codec->reg_cache = kmalloc(sizeof *rbtree_ctx, GFP_KERNEL);
	if (!codec->reg_cache)
		return -ENOMEM;

	rbtree_ctx = codec->reg_cache;
	rbtree_ctx->root = RB_ROOT;

	if (!codec->reg_def_copy)
		return 0;

	/*
	 * populate the rbtree with the initialized registers.  All other
	 * registers will be inserted when they are first modified.
	 */
	word_size = codec->driver->reg_word_size;
	for (i = 0; i < codec->driver->reg_cache_size; ++i) {
		val = snd_soc_get_cache_val(codec->reg_def_copy, i, word_size);
		if (!val)
			continue;
		rbtree_node = kzalloc(sizeof *rbtree_node, GFP_KERNEL);
		if (!rbtree_node) {
			ret = -ENOMEM;
			snd_soc_cache_exit(codec);
			break;
		}
		rbtree_node->reg = i;
		rbtree_node->value = val;
		rbtree_node->defval = val;
		snd_soc_rbtree_insert(&rbtree_ctx->root, rbtree_node);
	}

	return 0;
}

static int snd_soc_flat_cache_sync(struct snd_soc_codec *codec)
{
	int i;
	int ret;
	const struct snd_soc_codec_driver *codec_drv;
	unsigned int val;

	codec_drv = codec->driver;
	for (i = 0; i < codec_drv->reg_cache_size; ++i) {
		ret = snd_soc_cache_read(codec, i, &val);
		if (ret)
			return ret;
		if (codec->reg_def_copy)
			if (snd_soc_get_cache_val(codec->reg_def_copy,
						  i, codec_drv->reg_word_size) == val)
				continue;

		WARN_ON(!snd_soc_codec_writable_register(codec, i));

		ret = snd_soc_write(codec, i, val);
		if (ret)
			return ret;
		dev_dbg(codec->dev, "Synced register %#x, value = %#x\n",
			i, val);
	}
	return 0;
}

static int snd_soc_flat_cache_write(struct snd_soc_codec *codec,
				    unsigned int reg, unsigned int value)
{
	snd_soc_set_cache_val(codec->reg_cache, reg, value,
			      codec->driver->reg_word_size);
	return 0;
}

static int snd_soc_flat_cache_read(struct snd_soc_codec *codec,
				   unsigned int reg, unsigned int *value)
{
	*value = snd_soc_get_cache_val(codec->reg_cache, reg,
				       codec->driver->reg_word_size);
	return 0;
}

static int snd_soc_flat_cache_exit(struct snd_soc_codec *codec)
{
	if (!codec->reg_cache)
		return 0;
	kfree(codec->reg_cache);
	codec->reg_cache = NULL;
	return 0;
}

static int snd_soc_flat_cache_init(struct snd_soc_codec *codec)
{
	if (codec->reg_def_copy)
		codec->reg_cache = kmemdup(codec->reg_def_copy,
					   codec->reg_size, GFP_KERNEL);
	else
		codec->reg_cache = kzalloc(codec->reg_size, GFP_KERNEL);
	if (!codec->reg_cache)
		return -ENOMEM;

	return 0;
}

/* an array of all supported compression types */
static const struct snd_soc_cache_ops cache_types[] = {
	/* Flat *must* be the first entry for fallback */
	{
		.id = SND_SOC_FLAT_COMPRESSION,
		.name = "flat",
		.init = snd_soc_flat_cache_init,
		.exit = snd_soc_flat_cache_exit,
		.read = snd_soc_flat_cache_read,
		.write = snd_soc_flat_cache_write,
		.sync = snd_soc_flat_cache_sync
	},
	{
		.id = SND_SOC_RBTREE_COMPRESSION,
		.name = "rbtree",
		.init = snd_soc_rbtree_cache_init,
		.exit = snd_soc_rbtree_cache_exit,
		.read = snd_soc_rbtree_cache_read,
		.write = snd_soc_rbtree_cache_write,
		.sync = snd_soc_rbtree_cache_sync
	}
};

int snd_soc_cache_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cache_types); ++i)
		if (cache_types[i].id == codec->compress_type)
			break;

	/* Fall back to flat compression */
	if (i == ARRAY_SIZE(cache_types)) {
		dev_warn(codec->dev, "Could not match compress type: %d\n",
			 codec->compress_type);
		i = 0;
	}

	mutex_init(&codec->cache_rw_mutex);
	codec->cache_ops = &cache_types[i];

	if (codec->cache_ops->init) {
		if (codec->cache_ops->name)
			dev_dbg(codec->dev, "Initializing %s cache for %s codec\n",
				codec->cache_ops->name, codec->name);
		return codec->cache_ops->init(codec);
	}
	return -ENOSYS;
}

/*
 * NOTE: keep in mind that this function might be called
 * multiple times.
 */
int snd_soc_cache_exit(struct snd_soc_codec *codec)
{
	if (codec->cache_ops && codec->cache_ops->exit) {
		if (codec->cache_ops->name)
			dev_dbg(codec->dev, "Destroying %s cache for %s codec\n",
				codec->cache_ops->name, codec->name);
		return codec->cache_ops->exit(codec);
	}
	return -ENOSYS;
}

/**
 * snd_soc_cache_read: Fetch the value of a given register from the cache.
 *
 * @codec: CODEC to configure.
 * @reg: The register index.
 * @value: The value to be returned.
 */
int snd_soc_cache_read(struct snd_soc_codec *codec,
		       unsigned int reg, unsigned int *value)
{
	int ret;

	mutex_lock(&codec->cache_rw_mutex);

	if (value && codec->cache_ops && codec->cache_ops->read) {
		ret = codec->cache_ops->read(codec, reg, value);
		mutex_unlock(&codec->cache_rw_mutex);
		return ret;
	}

	mutex_unlock(&codec->cache_rw_mutex);
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(snd_soc_cache_read);

/**
 * snd_soc_cache_write: Set the value of a given register in the cache.
 *
 * @codec: CODEC to configure.
 * @reg: The register index.
 * @value: The new register value.
 */
int snd_soc_cache_write(struct snd_soc_codec *codec,
			unsigned int reg, unsigned int value)
{
	int ret;

	mutex_lock(&codec->cache_rw_mutex);

	if (codec->cache_ops && codec->cache_ops->write) {
		ret = codec->cache_ops->write(codec, reg, value);
		mutex_unlock(&codec->cache_rw_mutex);
		return ret;
	}

	mutex_unlock(&codec->cache_rw_mutex);
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(snd_soc_cache_write);

/**
 * snd_soc_cache_sync: Sync the register cache with the hardware.
 *
 * @codec: CODEC to configure.
 *
 * Any registers that should not be synced should be marked as
 * volatile.  In general drivers can choose not to use the provided
 * syncing functionality if they so require.
 */
int snd_soc_cache_sync(struct snd_soc_codec *codec)
{
	int ret;
	const char *name;

	if (!codec->cache_sync) {
		return 0;
	}

	if (!codec->cache_ops || !codec->cache_ops->sync)
		return -ENOSYS;

	if (codec->cache_ops->name)
		name = codec->cache_ops->name;
	else
		name = "unknown";

	if (codec->cache_ops->name)
		dev_dbg(codec->dev, "Syncing %s cache for %s codec\n",
			codec->cache_ops->name, codec->name);
	trace_snd_soc_cache_sync(codec, name, "start");
	ret = codec->cache_ops->sync(codec);
	if (!ret)
		codec->cache_sync = 0;
	trace_snd_soc_cache_sync(codec, name, "end");
	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_cache_sync);

static int snd_soc_get_reg_access_index(struct snd_soc_codec *codec,
					unsigned int reg)
{
	const struct snd_soc_codec_driver *codec_drv;
	unsigned int min, max, index;

	codec_drv = codec->driver;
	min = 0;
	max = codec_drv->reg_access_size - 1;
	do {
		index = (min + max) / 2;
		if (codec_drv->reg_access_default[index].reg == reg)
			return index;
		if (codec_drv->reg_access_default[index].reg < reg)
			min = index + 1;
		else
			max = index;
	} while (min <= max);
	return -1;
}

int snd_soc_default_volatile_register(struct snd_soc_codec *codec,
				      unsigned int reg)
{
	int index;

	if (reg >= codec->driver->reg_cache_size)
		return 1;
	index = snd_soc_get_reg_access_index(codec, reg);
	if (index < 0)
		return 0;
	return codec->driver->reg_access_default[index].vol;
}
EXPORT_SYMBOL_GPL(snd_soc_default_volatile_register);

int snd_soc_default_readable_register(struct snd_soc_codec *codec,
				      unsigned int reg)
{
	int index;

	if (reg >= codec->driver->reg_cache_size)
		return 1;
	index = snd_soc_get_reg_access_index(codec, reg);
	if (index < 0)
		return 0;
	return codec->driver->reg_access_default[index].read;
}
EXPORT_SYMBOL_GPL(snd_soc_default_readable_register);

int snd_soc_default_writable_register(struct snd_soc_codec *codec,
				      unsigned int reg)
{
	int index;

	if (reg >= codec->driver->reg_cache_size)
		return 1;
	index = snd_soc_get_reg_access_index(codec, reg);
	if (index < 0)
		return 0;
	return codec->driver->reg_access_default[index].write;
}
EXPORT_SYMBOL_GPL(snd_soc_default_writable_register);
