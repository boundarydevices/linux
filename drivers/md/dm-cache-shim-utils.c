/*
 * Copyright 2013 NetApp, Inc. All Rights Reserved, contribution by
 * Morgan Mears.
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

#include <linux/module.h>

#define DM_MSG_PREFIX "cache-shim-utils"

/*----------------------------------------------------------------*/

static int shim_nested_walk_apply(void *context, dm_cblock_t cblock,
				  dm_oblock_t oblock, void *hint)
{
	struct shim_walk_map_ctx *ctx = context;
	struct dm_cache_policy *p;
	int child_hint_size;
	void *my_hint;

	/* Save off our child's hint */
	if (ctx->child_hint_buf) {
		p = ctx->my_policy;
		child_hint_size = dm_cache_policy_get_hint_size(p->child);
		if (child_hint_size && hint)
			memcpy(&ctx->child_hint_buf[0], hint, child_hint_size);
	}

	/* Provide my hint or NULL up the stack */
	my_hint = ctx->cblock_to_hint_fn ?
		ctx->cblock_to_hint_fn(ctx, cblock, oblock) : NULL;

	/* Reverse recurse, unless short-circuted */
	return (ctx->parent_fn) ?
		(*ctx->parent_fn)(ctx->parent_ctx, cblock, oblock, my_hint) : 0;
}

/*----------------------------------------------------------------*/

/*
 * Public interface, via the policy struct.  See dm-cache-policy.h for a
 * description of these.
 */

static void shim_destroy(struct dm_cache_policy *p)
{
	kfree(p);
}

static int shim_map(struct dm_cache_policy *p, dm_oblock_t oblock,
		    bool can_block, bool can_migrate, bool discarded_oblock,
		    struct bio *bio, struct policy_result *result)
{
	return policy_map(p->child, oblock, can_block, can_migrate,
			  discarded_oblock, bio, result);
}

static int shim_lookup(struct dm_cache_policy *p, dm_oblock_t oblock,
		       dm_cblock_t *cblock)
{
	return policy_lookup(p->child, oblock, cblock);
}

static void shim_set_dirty(struct dm_cache_policy *p, dm_oblock_t oblock)
{
	policy_set_dirty(p->child, oblock);
}

static void shim_clear_dirty(struct dm_cache_policy *p, dm_oblock_t oblock)
{
	policy_clear_dirty(p->child, oblock);
}

static int shim_load_mapping(struct dm_cache_policy *p,
			     dm_oblock_t oblock, dm_cblock_t cblock,
			     void *hint, bool hint_valid)
{
	return policy_load_mapping(p->child, oblock, cblock, hint, hint_valid);
}

static int shim_walk_mappings(struct dm_cache_policy *p, policy_walk_fn fn,
			      void *context)
{
	struct shim_walk_map_ctx my_ctx, *parent_ctx;
	int my_hint_size;

	parent_ctx = (struct shim_walk_map_ctx *)context;
	my_hint_size = dm_cache_policy_get_hint_size(p);

	my_ctx.parent_ctx = parent_ctx;
	my_ctx.parent_fn = fn;
	my_ctx.my_policy = p;
	my_ctx.child_hint_buf = (parent_ctx->child_hint_buf) ?
		&parent_ctx->child_hint_buf[my_hint_size] : NULL;
	my_ctx.cblock_to_hint_fn = NULL;

	return policy_walk_mappings(p->child, shim_nested_walk_apply, &my_ctx);
}

static void shim_remove_mapping(struct dm_cache_policy *p, dm_oblock_t oblock)
{
	policy_remove_mapping(p->child, oblock);
}

static int shim_writeback_work(struct dm_cache_policy *p, dm_oblock_t *oblock,
			       dm_cblock_t *cblock)
{
	return policy_writeback_work(p->child, oblock, cblock);
}

static void shim_force_mapping(struct dm_cache_policy *p,
			       dm_oblock_t current_oblock,
			       dm_oblock_t new_oblock)
{
	policy_force_mapping(p->child, current_oblock, new_oblock);
}

static dm_cblock_t shim_residency(struct dm_cache_policy *p)
{
	return policy_residency(p->child);
}

static void shim_tick(struct dm_cache_policy *p)
{
	policy_tick(p->child);
}

static int shim_set_config_value(struct dm_cache_policy *p,
				 const char *key, const char *value)
{
	return policy_set_config_value(p->child, key, value);
}

static int shim_emit_config_values(struct dm_cache_policy *p, char *result,
				   unsigned maxlen)
{
	return policy_emit_config_values(p->child, result, maxlen);
}

void dm_cache_shim_utils_init_shim_policy(struct dm_cache_policy *p)
{
	p->destroy = shim_destroy;
	p->map = shim_map;
	p->lookup = shim_lookup;
	p->set_dirty = shim_set_dirty;
	p->clear_dirty = shim_clear_dirty;
	p->load_mapping = shim_load_mapping;
	p->walk_mappings = shim_walk_mappings;
	p->remove_mapping = shim_remove_mapping;
	p->writeback_work = shim_writeback_work;
	p->force_mapping = shim_force_mapping;
	p->residency = shim_residency;
	p->tick = shim_tick;
	p->emit_config_values = shim_emit_config_values;
	p->set_config_value = shim_set_config_value;
}
EXPORT_SYMBOL_GPL(dm_cache_shim_utils_init_shim_policy);

int dm_cache_shim_utils_walk_map_with_ctx(struct shim_walk_map_ctx *ctx)
{
	struct dm_cache_policy *p = ctx->my_policy;

	/*
	 * Used by the stack root policy in its walk_mappings implementation,
	 * to provide the top-level context that contains the buffer used to
	 * consolidate hint data from all of the shims and the terminal policy.
	 */
	return policy_walk_mappings(p->child, shim_nested_walk_apply, ctx);
}
EXPORT_SYMBOL_GPL(dm_cache_shim_utils_walk_map_with_ctx);

int dm_cache_shim_utils_walk_map(struct dm_cache_policy *p, policy_walk_fn fn,
				 void *context, cblock_to_hint_fn_t hint_fn)
{
	struct shim_walk_map_ctx my_ctx, *parent_ctx;
	int my_hint_size;

	/*
	 * Used by shim policies for their walk_mappings implementations.
	 * Handles packing up the hint data, in conjunction with
	 * shim_nested_walk_apply.
	 */
	parent_ctx = (struct shim_walk_map_ctx *)context;
	my_hint_size = dm_cache_policy_get_hint_size(p);

	my_ctx.parent_ctx = parent_ctx;
	my_ctx.parent_fn = fn;
	my_ctx.my_policy = p;
	my_ctx.child_hint_buf = (parent_ctx && parent_ctx->child_hint_buf) ?
		&parent_ctx->child_hint_buf[my_hint_size] : NULL;
	my_ctx.cblock_to_hint_fn = hint_fn;

	return policy_walk_mappings(p->child, shim_nested_walk_apply, &my_ctx);
}
EXPORT_SYMBOL_GPL(dm_cache_shim_utils_walk_map);
