/*
 * include/linux/amlogic/aml_sync_api.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

struct fence;

void *aml_sync_create_timeline(const char *tname);
int aml_sync_create_fence(void *timeline, unsigned int value);
int aml_sync_inc_timeline(void *timeline, unsigned int value);

struct fence *aml_sync_get_fence(int syncfile_fd);
int aml_sync_wait_fence(struct fence *syncfile, long timeout);
void aml_sync_put_fence(struct fence *syncfile);
