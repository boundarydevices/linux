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

#ifndef DM_CACHE_STACK_UTILS_H
#define DM_CACHE_STACK_UTILS_H

#include "dm-cache-policy.h"

#define DM_CACHE_POLICY_STACK_DELIM '+'

int dm_cache_stack_utils_string_is_policy_stack(const char *string);

struct dm_cache_policy *dm_cache_stack_utils_policy_stack_create(
				const char *policy_stack_string,
				dm_cblock_t cache_size,
				sector_t origin_size,
				sector_t cache_block_size);

void dm_cache_stack_utils_policy_stack_destroy(struct dm_cache_policy *p);

#endif /* DM_CACHE_STACK_UTILS_H */
