/*
 * drivers/amlogic/unifykey/unifykey.h
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

#ifndef __UNIFYKEY_MANAGE_H__
#define __UNIFYKEY_MANAGE_H__

#include <linux/cdev.h>
#include <linux/ioctl.h>

#define KEYUNIFY_ATTACH				_IO('f', 0x60)
#define KEYUNIFY_GET_INFO			_IO('f', 0x62)



/* need query! */

enum key_manager_dev_e {
		KEY_M_UNKNOWN_DEV = 0,
		KEY_M_EFUSE,
		KEY_M_NORMAL,/*include general key(nand key,emmc key)*/
		KEY_M_SECURE,
		KEY_M_MAX_DEV,
};

enum key_manager_permit_e {
		KEY_M_PERMIT_READ = (1<<0),
		KEY_M_PERMIT_WRITE = (1<<1),
		KEY_M_PERMIT_DEL    = (1<<2),
		KEY_M_PERMIT_MASK   = 0Xf,
};

enum key_manager_flag_e {
	KEY_M_FLAG_EMPTY = (0<<0),
	KEY_M_FLAG_EXSIT = (1<<0),
};
#define KEY_UNIFY_NAME_LEN	48
/* for ioctrl transfer parameters. */
struct key_item_info_t {
	unsigned int id;
	char name[KEY_UNIFY_NAME_LEN];
	unsigned int size;
	unsigned int permit;
	unsigned int flag;		/*bit 0: 1 exsit, 0-none;*/
	unsigned int reserve;
};

#define KEY_UNIFY_ATTR_ENCRYPT		(1 << 8)
#define KEY_UNIFY_ATTR_ENCRYPT_MASK	(KEY_UNIFY_ATTR_ENCRYPT)
#define KEY_UNIFY_ATTR_SECURE		(1 << 0)
#define KEY_UNIFY_ATTR_SECURE_MASK	(KEY_UNIFY_ATTR_SECURE)

struct key_item_t {
	char name[KEY_UNIFY_NAME_LEN];
	int id;
	unsigned int dev; /* key save in device //efuse, */
	/* unsigned int df; */
	unsigned int permit;
	int attr;
	int reserve;
	struct key_item_t *next;
};

struct key_info_t {
	int key_num;
	int efuse_version;
	int key_flag;
	int encrypt_type;
};

struct unifykey_dev_t {
	struct cdev cdev;
	unsigned int flags;
};

extern int unifykey_dt_create(struct platform_device *pdev);
extern int unifykey_dt_release(struct platform_device *pdev);
extern struct key_item_t *unifykey_find_item_by_name(char *name);
extern struct key_item_t *unifykey_find_item_by_id(int id);
extern int unifykey_item_verify_check(struct key_item_t *key_item);
extern int unifykey_count_key(void);
extern char unifykey_get_efuse_version(void);
extern int unifykey_get_encrypt_type(void);

#endif

