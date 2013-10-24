/*
 * Copyright (C) 2012 Red Hat. All rights reserved.
 *
 * This file is released under the GPL.
 */

#include "dm-cache-policy-internal.h"
#include "dm.h"

#include <linux/module.h>
#include <linux/slab.h>

/*----------------------------------------------------------------*/

#define DM_MSG_PREFIX "cache-policy"

static DEFINE_SPINLOCK(register_lock);
static LIST_HEAD(register_list);

static struct dm_cache_policy_type *__find_policy(const char *name)
{
	struct dm_cache_policy_type *t;

	list_for_each_entry(t, &register_list, list)
		if (!strcmp(t->name, name))
			return t;

	return NULL;
}

static struct dm_cache_policy_type *__get_policy_once(const char *name)
{
	struct dm_cache_policy_type *t = __find_policy(name);

	if (t && !try_module_get(t->owner)) {
		DMWARN("couldn't get module %s", name);
		t = ERR_PTR(-EINVAL);
	}

	return t;
}

static struct dm_cache_policy_type *get_policy_once(const char *name)
{
	struct dm_cache_policy_type *t;

	spin_lock(&register_lock);
	t = __get_policy_once(name);
	spin_unlock(&register_lock);

	return t;
}

static struct dm_cache_policy_type *get_policy(const char *name)
{
	struct dm_cache_policy_type *t;

	t = get_policy_once(name);
	if (IS_ERR(t))
		return NULL;

	if (t)
		return t;

	request_module("dm-cache-%s", name);

	t = get_policy_once(name);
	if (IS_ERR(t))
		return NULL;

	return t;
}

static void put_policy(struct dm_cache_policy_type *t)
{
	module_put(t->owner);
}

int dm_cache_policy_register(struct dm_cache_policy_type *type)
{
	int r;

	if (type->hint_size > DM_CACHE_POLICY_MAX_HINT_SIZE) {
		DMWARN("hint size must be <= %llu but %llu was supplied.",
		       (unsigned long long) DM_CACHE_POLICY_MAX_HINT_SIZE,
		       (unsigned long long) type->hint_size);
		return -EINVAL;
	}

	spin_lock(&register_lock);
	if (__find_policy(type->name)) {
		DMWARN("attempt to register policy under duplicate name %s", type->name);
		r = -EINVAL;
	} else {
		list_add(&type->list, &register_list);
		r = 0;
	}
	spin_unlock(&register_lock);

	return r;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_register);

void dm_cache_policy_unregister(struct dm_cache_policy_type *type)
{
	spin_lock(&register_lock);
	list_del_init(&type->list);
	spin_unlock(&register_lock);
}
EXPORT_SYMBOL_GPL(dm_cache_policy_unregister);

struct dm_cache_policy *dm_cache_policy_create(const char *name,
					       dm_cblock_t cache_size,
					       sector_t origin_size,
					       sector_t cache_block_size)
{
	struct dm_cache_policy *p = NULL;
	struct dm_cache_policy_type *type;

	type = get_policy(name);
	if (!type) {
		DMWARN("unknown policy type");
		return ERR_PTR(-EINVAL);
	}

	p = type->create(cache_size, origin_size, cache_block_size);
	if (!p) {
		put_policy(type);
		return ERR_PTR(-ENOMEM);
	}
	p->private = type;

	return p;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_create);

void dm_cache_policy_destroy(struct dm_cache_policy *p)
{
	struct dm_cache_policy_type *t = p->private;

	p->destroy(p);
	put_policy(t);
}
EXPORT_SYMBOL_GPL(dm_cache_policy_destroy);

const char *dm_cache_policy_get_name(struct dm_cache_policy *p)
{
	struct dm_cache_policy_type *t = p->private;

	return t->name;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_get_name);

const unsigned *dm_cache_policy_get_version(struct dm_cache_policy *p)
{
	struct dm_cache_policy_type *t = p->private;

	return t->version;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_get_version);

size_t dm_cache_policy_get_hint_size(struct dm_cache_policy *p)
{
	struct dm_cache_policy_type *t = p->private;

	return t->hint_size;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_get_hint_size);

int dm_cache_policy_set_hint_size(struct dm_cache_policy *p, unsigned hint_size)
{
	struct dm_cache_policy_type *t = p->private;

	if (hint_size > DM_CACHE_POLICY_MAX_HINT_SIZE)
		return -EPERM;

	t->hint_size = hint_size;
	return 0;
}
EXPORT_SYMBOL_GPL(dm_cache_policy_set_hint_size);

/*----------------------------------------------------------------*/
