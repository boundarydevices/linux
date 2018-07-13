/*
 * include/linux/amlogic/efuse.h
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

#ifndef __EFUSE_AMLOGIC_H
#define __EFUSE_AMLOGIC_H

#ifdef CONFIG_ARM64
struct efusekey_info {
	char keyname[32];
	unsigned int offset;
	unsigned int size;
};

extern int efusekeynum;

int efuse_getinfo(char *item, struct efusekey_info *info);
ssize_t efuse_user_attr_show(char *name, char *buf);
ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count);
ssize_t efuse_user_attr_read(char *name, char *buf);
#else
int efuse_read_intlItem(char *intl_item, char *buf, int size);
#endif
#endif
