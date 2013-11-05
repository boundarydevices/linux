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

#include "dm-cache-policy-internal.h"
#include "dm-cache-shim-utils.h"
#include "dm-cache-stack-utils.h"
#include "dm-cache-policy.h"
#include "dm.h"

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>

#define DM_MSG_PREFIX "cache-stack-utils"

struct stack_root_policy {
	struct dm_cache_policy policy;
	struct dm_cache_policy_type type;
};

/*----------------------------------------------------------------*/

static void *stack_root_cblock_to_hint(struct shim_walk_map_ctx *ctx,
				       dm_cblock_t cblock, dm_oblock_t oblock)
{
	return ctx->child_hint_buf;
}

static int stack_root_walk_mappings(struct dm_cache_policy *p,
				    policy_walk_fn fn, void *context)
{
	struct shim_walk_map_ctx ctx;
	size_t hint_size;
	int r;

	ctx.parent_ctx = context;
	ctx.parent_fn = fn;
	ctx.my_policy = p;
	ctx.child_hint_buf = NULL;
	ctx.cblock_to_hint_fn = stack_root_cblock_to_hint;

	hint_size = dm_cache_policy_get_hint_size(p);
	if (hint_size) {
		ctx.child_hint_buf = kzalloc(hint_size, GFP_KERNEL);
		if (!ctx.child_hint_buf)
			return -ENOMEM;
	}

	r = dm_cache_shim_utils_walk_map_with_ctx(&ctx);

	kfree(ctx.child_hint_buf);

	return r;
}

static struct dm_cache_policy *stack_root_create(const char *policy_stack_str,
						 struct dm_cache_policy *head)
{
	struct stack_root_policy *p = kzalloc(sizeof(*p), GFP_KERNEL);
	struct dm_cache_policy *child;
	struct dm_cache_policy_type *t;
	const unsigned *version;
	const char *seg_name;
	size_t canonical_name_len, hint_size;
	int i;

	if (!p)
		return NULL;

	t = &p->type;
	dm_cache_shim_utils_init_shim_policy(&p->policy);
	p->policy.walk_mappings = stack_root_walk_mappings;
	p->policy.child = head;

	/*
	 * We compose the canonical name for this policy stack by removing
	 * any shim policies that do not have hint data.  This is intended
	 * to allow for a class of shim policies that can be inserted into,
	 * or removed from, the policy stack without causing the in-flash
	 * metadata to be invalidated.  The thought is to allow debug or
	 * tracing shims to be inserted or removed without dropping the cache.
	 * The composite version numbers of a policy stack do not include the
	 * versions of the hintless policies for the same reason.
	 */
	canonical_name_len = 0;
	for (child = head; child; child = child->child) {
		hint_size = dm_cache_policy_get_hint_size(child);

#if 0
		/* FIXME: avoids policy name in t->name, thus leaving an non-destroyable stack. */
		if (!hint_size && child->child)
			continue;
#endif

		t->hint_size += hint_size;

		seg_name = dm_cache_policy_get_name(child);
		canonical_name_len += strlen(seg_name) + (dm_cache_policy_is_shim(child) ? 1 : 0);

		if (canonical_name_len >= sizeof(t->name)) {
			DMWARN("policy stack string '%s' is too long",
			       policy_stack_str);
			kfree(p);
			return NULL;
		}

		strcat(t->name, seg_name);

		if (dm_cache_policy_is_shim(child)) {
			t->name[canonical_name_len - 1] = DM_CACHE_POLICY_STACK_DELIM;
			t->name[canonical_name_len] = '\0';
		}

		version = dm_cache_policy_get_version(child);

		for (i = 0; i < CACHE_POLICY_VERSION_SIZE; i++)
			t->version[i] += version[i];
	}

	p->policy.private = t;
	return &p->policy;
}

static void stack_root_destroy(struct dm_cache_policy *p)
{
	kfree(p);
}

/*----------------------------------------------------------------*/

int dm_cache_stack_utils_string_is_policy_stack(const char *string)
{
	const char *delim;

	/*
	 * A string specifies a policy stack instead of a policy if it
	 * contains a policy delimiter (+) anywhere but at the end.  The
	 * latter is needed to properly distinguish between policy stacks and
	 * individual shim policies, since this function is called on them
	 * when the policy stack is constructed from the specified string.
	 */
	delim = strchr(string, DM_CACHE_POLICY_STACK_DELIM);
	if (!delim || (delim[1] == '\0'))
		return false;

	return true;
}

static void __policy_destroy_stack(struct dm_cache_policy *head_p)
{
	struct dm_cache_policy *cur_p, *next_p;

	for (cur_p = head_p; cur_p; cur_p = next_p) {
		next_p = cur_p->child;
		dm_cache_policy_destroy(cur_p);
	}
}

struct dm_cache_policy *
dm_cache_stack_utils_policy_stack_create(const char *policy_stack_str,
					 dm_cblock_t cache_size,
					 sector_t origin_size,
					 sector_t cache_block_size)
{
	char policy_name_buf[CACHE_POLICY_NAME_SIZE];
	struct dm_cache_policy *p, *head_p, *next_p;
	char *policy_name, *delim, uninitialized_var(saved_char);
	int n, r = -EINVAL;

	n = strlcpy(policy_name_buf, policy_stack_str, sizeof(policy_name_buf));
	if (n >= sizeof(policy_name_buf)) {
		DMWARN("policy stack string is too long");
		return NULL;
	}

	policy_name = policy_name_buf;
	p = head_p = next_p = NULL;

	do {
		delim = strchr(policy_name, DM_CACHE_POLICY_STACK_DELIM);
		if (delim)
			*delim = '\0';

		next_p = dm_cache_policy_create(policy_name, cache_size,
						origin_size, cache_block_size);
		if (IS_ERR(next_p)) {
			r = PTR_ERR(next_p);
			goto cleanup;
		}

		next_p->child = NULL;
		if (p)
			p->child = next_p;
		else
			head_p = next_p;
		p = next_p;

		if (delim) {
			if (!dm_cache_policy_is_shim(next_p)) {
				DMERR("%s is not a shim policy", policy_name);
				goto cleanup;
			}

			*delim = DM_CACHE_POLICY_STACK_DELIM;
			policy_name = delim + 1;
		}
	} while (delim);

	if (head_p->child) {
		next_p = stack_root_create(policy_stack_str, head_p);
		if (!next_p)
			goto cleanup;

		head_p = next_p;
	}

	return head_p;

cleanup:
	__policy_destroy_stack(head_p);
	return ERR_PTR(r);
}

void dm_cache_stack_utils_policy_stack_destroy(struct dm_cache_policy *p)
{
	__policy_destroy_stack(p->child);
	stack_root_destroy(p);
}
