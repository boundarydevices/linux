/*
 * Copyright 2013 NetApp, Inc. All Rights Reserved, contribution by
 * Morgan Mears.
 *
 * Copyright 2013 Red Hat, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details
 *
 */

#include "dm-cache-policy.h"
#include "dm-cache-policy-internal.h"
#include "dm-cache-shim-utils.h"
#include "dm.h"

#include <linux/hash.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/delay.h>

#define DM_MSG_PREFIX "cache-policy-era"

typedef uint32_t era_t;
#define ERA_MAX_ERA UINT_MAX

struct era_policy {
	struct dm_cache_policy policy;

	struct mutex lock;	/* FIXME: spinlock? */

	dm_cblock_t cache_size;

	era_t *cb_to_era;

	era_t era_counter;

	/* Temporary store for unmap information during invalidation. */
	struct {
		unsigned long *bitset;
		dm_oblock_t *oblocks;
		unsigned long last_cblock;
	} invalidate;
};

/*----------------------------------------------------------------*/

static struct era_policy *to_era_policy(struct dm_cache_policy *p)
{
	return container_of(p, struct era_policy, policy);
}

static unsigned long *alloc_bitset(unsigned nr_entries)
{
	size_t s = sizeof(unsigned long) * dm_div_up(nr_entries, BITS_PER_LONG);
	return vzalloc(s);
}

static void free_bitset(unsigned long *bits)
{
	vfree(bits);
}

static dm_oblock_t *alloc_oblocks(unsigned nr_entries)
{
	size_t s = sizeof(dm_oblock_t) * nr_entries;
	return vmalloc(s);
}

static void free_oblocks(dm_oblock_t *blocks)
{
	vfree(blocks);
}

static void free_invalidate(struct era_policy *era)
{
	if (era->invalidate.oblocks) {
		free_oblocks(era->invalidate.oblocks);
		era->invalidate.oblocks = NULL;
	}

	if (era->invalidate.bitset) {
		free_bitset(era->invalidate.bitset);
		era->invalidate.bitset = NULL; /* Being checked for! */
	}
}

static int alloc_invalidate(struct era_policy *era)
{
	/* FIXME: memory consumption! */
	era->invalidate.oblocks = alloc_oblocks(from_cblock(era->cache_size));
	if (!era->invalidate.oblocks) {
		DMERR("failed to allocate original blocks unmap array");
		goto err;
	}

	era->invalidate.bitset = alloc_bitset(from_cblock(era->cache_size));
	if (!era->invalidate.bitset) {
		DMERR("failed to allocate cache blocks unmap bitset");
		goto err;
	}

	era->invalidate.last_cblock = 0;
	return 0;

err:
	free_invalidate(era);
	return -ENOMEM;
}


typedef int (*era_match_fn_t)(era_t, era_t);

static int incr_era_counter(struct era_policy *era, const char *curr_era_str,
			    era_match_fn_t dummy)
{
	era_t curr_era_counter;
	int r;

	/*
	 * If the era counter value provided by the user matches the current
	 * counter value while under lock, increment the counter (intention
	 * is to prevent races).  Rollover problems are avoided by locking
	 * the counter at a maximum value.  The application must take
	 * appropriate action on this error to preserve correction, but
	 * a properly behaved set of applications will never trigger it;
	 * the era counter is meant to increment less than once a second
	 * and is 32 bits.
	 */

	if (kstrtou32(curr_era_str, 10, &curr_era_counter))
		return -EINVAL;

	smp_rmb();
	if (era->era_counter != curr_era_counter)
		r = -ECANCELED;
	else if (era->era_counter >= ERA_MAX_ERA)
		r = -EOVERFLOW;
	else {
		era->era_counter++;
		smp_wmb();
		r = 0;
	}

	return r;
}

static void *era_cblock_to_hint(struct shim_walk_map_ctx *ctx,
				dm_cblock_t cblock, dm_oblock_t oblock)
{
	struct era_policy *era = to_era_policy(ctx->my_policy);
	era_t era_val;
	era_val = era->cb_to_era[from_cblock(cblock)];
	ctx->le32_buf = cpu_to_le32(era_val);
	return &ctx->le32_buf;
}

static int era_is_gt_value(era_t era, era_t value)
{
	return era > value;
}

static int era_is_gte_value(era_t era, era_t value)
{
	return era >= value;
}

static int era_is_lte_value(era_t era, era_t value)
{
	return era <= value;
}

static int era_is_lt_value(era_t era, era_t value)
{
	return era < value;
}

struct inval_oblocks_ctx {
	struct era_policy *era;
	era_match_fn_t era_match_fn;
	era_t test_era;
};

static int era_inval_oblocks(void *context, dm_cblock_t cblock,
			     dm_oblock_t oblock, void *unused)
{
	struct inval_oblocks_ctx *ctx = (struct inval_oblocks_ctx *)context;
	era_t act_era = ctx->era->cb_to_era[from_cblock(cblock)];

	if (ctx->era_match_fn(act_era, ctx->test_era)) {
		set_bit(from_cblock(cblock), ctx->era->invalidate.bitset);
		ctx->era->invalidate.oblocks[from_cblock(cblock)] = oblock;
	}

	return 0;
}

static int cond_unmap_by_era(struct era_policy *era, const char *test_era_str,
			     era_match_fn_t era_match_fn)
{
	struct shim_walk_map_ctx ctx;
	struct inval_oblocks_ctx io_ctx;
	era_t test_era;
	int r;

	if (era->invalidate.bitset) {
		DMERR("previous unmap request exists");
		return -EPERM;
	}

	/*
	 * Unmap blocks with eras matching the given era, according to the
	 * given matching function.
	 */

	if (kstrtou32(test_era_str, 10, &test_era))
		return -EINVAL;

	r = alloc_invalidate(era);
	if (r)
		return r;

	io_ctx.era = era;
	io_ctx.era_match_fn = era_match_fn;
	io_ctx.test_era = test_era;

	ctx.parent_ctx = &io_ctx;
	ctx.parent_fn = era_inval_oblocks;
	ctx.my_policy = &era->policy;
	ctx.child_hint_buf = NULL;
	ctx.cblock_to_hint_fn = NULL;

	mutex_lock(&era->lock);
	r = dm_cache_shim_utils_walk_map_with_ctx(&ctx);
	mutex_unlock(&era->lock);

	return r;
}

/*
 * Public interface, via the policy struct.  See dm-cache-policy.h for a
 * description of these.
 */

static void era_destroy(struct dm_cache_policy *p)
{
	struct era_policy *era = to_era_policy(p);

	free_invalidate(era);
	vfree(era->cb_to_era);
	kfree(era);
}

static int era_map(struct dm_cache_policy *p, dm_oblock_t oblock,
		   bool can_block, bool can_migrate, bool discarded_oblock,
		   struct bio *bio, struct policy_result *result)
{
	struct era_policy *era = to_era_policy(p);
	uint32_t cb_idx;
	int r;

	result->op = POLICY_MISS;

	if (can_block)
		mutex_lock(&era->lock);

	else if (!mutex_trylock(&era->lock))
		return -EWOULDBLOCK;

	/* Check for a mapping */
	r = policy_map(p->child, oblock, can_block, can_migrate,
		       discarded_oblock, bio, result);

	/* If we got a hit and this is a write, update the era for the block */
	if (!r && (bio_data_dir(bio) == WRITE) && (result->op == POLICY_HIT)) {
		cb_idx = from_cblock(result->cblock);
		BUG_ON(cb_idx >= from_cblock(era->cache_size));
		smp_rmb();
		era->cb_to_era[cb_idx] = era->era_counter;
	}

	mutex_unlock(&era->lock);

	return r;
}

static int era_load_mapping(struct dm_cache_policy *p,
			    dm_oblock_t oblock, dm_cblock_t cblock,
			    void *hint, bool hint_valid)
{
	struct era_policy *era = to_era_policy(p);
	struct dm_cache_policy *child;
	__le32 *le32_hint;
	era_t recovered_era;
	int r;

	child = era->policy.child;

	le32_hint = (__le32 *)hint;
	hint = &le32_hint[1];

	r = policy_load_mapping(child, oblock, cblock, hint, hint_valid);

	/* FIXME: recovered area valid on reload called from cache core invalidate mapping error path? */
	if (!r && hint_valid &&
	    (from_cblock(cblock) < from_cblock(era->cache_size))) {
		recovered_era = le32_to_cpu(*le32_hint);
		era->cb_to_era[from_cblock(cblock)] = recovered_era;

		/*
		 * Make sure the era counter starts higher than the highest
		 * persisted era.
		 */
		smp_rmb();
		if (recovered_era >= era->era_counter) {
			era->era_counter = recovered_era;
			if (era->era_counter < ERA_MAX_ERA)
				era->era_counter++;
			smp_wmb();
		}
	}

	return r;
}

static int era_walk_mappings(struct dm_cache_policy *p, policy_walk_fn fn,
			     void *context)
{
	return dm_cache_shim_utils_walk_map(p, fn, context, era_cblock_to_hint);
}

static void era_force_mapping(struct dm_cache_policy *p, dm_oblock_t old_oblock,
			      dm_oblock_t new_oblock)
{
	struct era_policy *era = to_era_policy(p);
	dm_cblock_t cblock;

	mutex_lock(&era->lock);

	if (!policy_lookup(p->child, old_oblock, &cblock)) {
		smp_rmb();
		era->cb_to_era[from_cblock(cblock)] = era->era_counter;
	}

	policy_force_mapping(p->child, old_oblock, new_oblock);

	mutex_unlock(&era->lock);
}

/* Find next block to invalidate. */
static int __find_invalidate_block(struct era_policy *era, dm_cblock_t *cblock)
{
	int bit = find_next_bit(era->invalidate.bitset, from_cblock(era->cache_size),
				era->invalidate.last_cblock);

	*cblock = to_cblock(bit);
	era->invalidate.last_cblock = bit;
	return bit < from_cblock(era->cache_size) ? 0 : -ENODATA;
}

static int era_invalidate_mapping(struct dm_cache_policy *p,
				  dm_oblock_t *oblock, dm_cblock_t *cblock)
{
	struct era_policy *era = to_era_policy(p);
	int r;

	if (!era->invalidate.bitset)
		return -ENODATA;

	r = __find_invalidate_block(era, cblock);
	if (r < 0)
		free_invalidate(era);
	else {
		BUG_ON(from_cblock(*cblock) >= from_cblock(era->cache_size));
		BUG_ON(!test_bit(from_cblock(*cblock), era->invalidate.bitset));
		clear_bit(from_cblock(*cblock), era->invalidate.bitset);
		*oblock = era->invalidate.oblocks[from_cblock(*cblock)];
		r = policy_invalidate_mapping(p->child, oblock, cblock);
	}

	return r;
}

struct config_value_handler {
	const char *cmd;
	int (*handler_fn)(struct era_policy *, const char *, era_match_fn_t);
	era_match_fn_t match_fn;
};

/* FIXME: is a delete unmap request needed or is reloading the mapping sufficient to achieve it? */
static int era_set_config_value(struct dm_cache_policy *p, const char *key,
				const char *value)
{
	struct era_policy *era = to_era_policy(p);
	struct config_value_handler *vh, value_handlers[] = {
		{ "increment_era_counter",                  incr_era_counter,  NULL },
		{ "unmap_blocks_from_later_eras",           cond_unmap_by_era, era_is_gt_value },
		{ "unmap_blocks_from_this_era_and_later",   cond_unmap_by_era, era_is_gte_value },
		{ "unmap_blocks_from_this_era_and_earlier", cond_unmap_by_era, era_is_lte_value },
		{ "unmap_blocks_from_earlier_eras",         cond_unmap_by_era, era_is_lt_value },
		{ NULL }
	};

	for (vh = value_handlers; vh->cmd; vh++) {
		if (!strcasecmp(key, vh->cmd))
			return vh->handler_fn(era, value, vh->match_fn);
	}

	return policy_set_config_value(p->child, key, value);
}

static int era_emit_config_values(struct dm_cache_policy *p, char *result,
				  unsigned maxlen)
{
	struct era_policy *era = to_era_policy(p);
	ssize_t sz = 0;

	smp_rmb();
	DMEMIT("era_counter %u ", era->era_counter);
	return policy_emit_config_values(p->child, result + sz, maxlen - sz);
}

/* Init the policy plugin interface function pointers. */
static void init_policy_functions(struct era_policy *era)
{
	dm_cache_shim_utils_init_shim_policy(&era->policy);
	era->policy.destroy = era_destroy;
	era->policy.map = era_map;
	era->policy.load_mapping = era_load_mapping;
	era->policy.walk_mappings = era_walk_mappings;
	era->policy.force_mapping = era_force_mapping;
	era->policy.invalidate_mapping = era_invalidate_mapping;
	era->policy.emit_config_values = era_emit_config_values;
	era->policy.set_config_value = era_set_config_value;
}

static struct dm_cache_policy *era_create(dm_cblock_t cache_size,
					  sector_t origin_size,
					  sector_t cache_block_size)
{
	struct era_policy *era = kzalloc(sizeof(*era), GFP_KERNEL);

	if (!era)
		return NULL;

	init_policy_functions(era);
	era->cache_size = cache_size;
	mutex_init(&era->lock);

	era->cb_to_era = vzalloc(from_cblock(era->cache_size) *
				 sizeof(*era->cb_to_era));
	if (era->cb_to_era) {
		era->era_counter = 1;
		return &era->policy;
	}

	kfree(era);
	return NULL;
}

/*----------------------------------------------------------------*/

static struct dm_cache_policy_type era_policy_type = {
	.name = "era",
	.version = {1, 0, 0},
	.hint_size = 4,
	.owner = THIS_MODULE,
	.create = era_create,
	.features = DM_CACHE_POLICY_SHIM
};

static int __init era_init(void)
{
	int r;

	r = dm_cache_policy_register(&era_policy_type);
	if (!r) {
		DMINFO("version %u.%u.%u loaded",
		       era_policy_type.version[0],
		       era_policy_type.version[1],
		       era_policy_type.version[2]);
		return 0;
	}

	DMERR("register failed %d", r);

	dm_cache_policy_unregister(&era_policy_type);
	return -ENOMEM;
}

static void __exit era_exit(void)
{
	dm_cache_policy_unregister(&era_policy_type);
}

module_init(era_init);
module_exit(era_exit);

MODULE_AUTHOR("Morgan Mears <dm-devel@redhat.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("era cache policy shim");
