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

#ifndef DM_CACHE_SHIM_UTILS_H
#define DM_CACHE_SHIM_UTILS_H

#include "dm-cache-policy.h"

struct shim_walk_map_ctx;

typedef void* (*cblock_to_hint_fn_t)(struct shim_walk_map_ctx *,
				     dm_cblock_t,
				     dm_oblock_t);

/*
 * For walk_mappings to work with a policy stack, every non-terminal policy
 * has to start its context with one of these.  There are no requirements for
 * the context used by the terminal policy.
 */
struct shim_walk_map_ctx {
	void *parent_ctx;
	policy_walk_fn parent_fn;
	struct dm_cache_policy *my_policy;
	char *child_hint_buf;
	cblock_to_hint_fn_t cblock_to_hint_fn;
	union {
		__le64 le64_buf;
		__le32 le32_buf;
		__le16 le16_buf;
	};
};

/*
 * Populate a shim (non-terminal) policy structure with functions that just
 * hand off to the child policy.  Caller can then override just those
 * functions of interest.
 */
void dm_cache_shim_utils_init_shim_policy(struct dm_cache_policy *p);

/*
 * Launch a "walk_mappings" leg using the context provided by our caller.
 * Typically used at the bottom of a policy stack, so caller can provide
 * the hint buffer.
 */
int dm_cache_shim_utils_walk_map_with_ctx(struct shim_walk_map_ctx *ctx);

/*
 * Initialize a context appropriately and Launch a "walk_mappings" leg.
 * Typically used to implement walk_mappings in shim policies. The
 * framework will call hint_fn at the appropriate point, and it should
 * return a pointer to the disk-ready hint for the given cblock.  The
 * leXX_bufs in the shim_walk_map_ctx structure can be used to store the
 * disk-ready hint if it will fit.
 */
int dm_cache_shim_utils_walk_map(struct dm_cache_policy *p,
				 policy_walk_fn fn,
				 void *context,
				 cblock_to_hint_fn_t hint_fn);

#endif /* DM_CACHE_SHIM_UTILS_H */
