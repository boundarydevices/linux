/*
 * include/linux/amlogic/unifykey/key_manage.h
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

#ifndef __KEYMANAGE1__
#define __KEYMANAGE1__

typedef int32_t (*store_key_ops)(uint8_t *buf,
					uint32_t len, uint32_t *actual_length);

#ifdef CONFIG_AMLOGIC_UNIFYKEY
void storage_ops_read(store_key_ops read);
void storage_ops_write(store_key_ops write);
#else
void storage_ops_read(store_key_ops read)
{
}

void storage_ops_write(store_key_ops read)
{
}
#endif /*CONFIG_KEY_MANAGE*/

void  *get_ukdev(void);

#endif /*__KEYMANAGE1__*/
