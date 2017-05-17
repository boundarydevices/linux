/*
 * drivers/amlogic/unifykey/unifykey_dts.c
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/amlogic/cpu_version.h>
#include "unifykey.h"

/* storages where key stored */
#define KEY_DEV_EFUSE		"efuse"
#define KEY_DEV_NORMAL		"normal"
#define KEY_DEV_SECURE		"secure"

/* permision */
#define KEY_PERMIT_READ		"read"
#define KEY_PERMIT_WRITE	"write"
#define KEY_PERMIT_DEL		"del"

/* attribute */
#define KEY_ATTR_TRUE		"true"
#define KEY_ATTR_FALSE		"false"

static struct key_info_t unify_key_info = {.key_num = 0, .key_flag = 0,
					   .encrypt_type = 0};
static struct key_item_t *unifykey_item;

int unifykey_item_verify_check(struct key_item_t *key_item)
{
	if (!key_item) {
		pr_err("%s:%d unify key item is invalid\n", __func__, __LINE__);
		return -1;
	}
	if (key_item->dev == KEY_M_UNKNOWN_DEV) {
		pr_err("%s:%d unify key item is invalid\n", __func__, __LINE__);
		return -1;
	}
	return 0;
}

struct key_item_t *unifykey_find_item_by_name(char *name)
{
	struct key_item_t *pre_item;

	pre_item = unifykey_item;
	while (pre_item) {
		if (!strncmp(pre_item->name, name,
				((strlen(pre_item->name) > strlen(name))
				? strlen(pre_item->name) : strlen(name))))
			return pre_item;
		pre_item = pre_item->next;
	}
	return NULL;
}

struct key_item_t *unifykey_find_item_by_id(int id)
{
	struct key_item_t *pre_item;

	pre_item = unifykey_item;
	while (pre_item) {
		if (pre_item->id == id)
			return pre_item;
		pre_item = pre_item->next;
	}
	return NULL;
}

int unifykey_count_key(void)
{
	int count = 0;
	struct key_item_t *pre_item;

	pre_item = unifykey_item;
	while (pre_item) {
		count++;
		pre_item = pre_item->next;
	}

	return count;
}

char unifykey_get_efuse_version(void)
{
	char ver = 0;

	if (unify_key_info.efuse_version != -1)
		ver = (char)unify_key_info.efuse_version;
	return ver;
}

static int unifykey_add_to_list(struct key_item_t *item)
{
	struct key_item_t *pre_item;

	if (unifykey_item == NULL) {
		unifykey_item = item;
	} else {
		pre_item = unifykey_item;
		while (pre_item->next != NULL)
			pre_item = pre_item->next;
		pre_item->next = item;
	}
	return 0;
}

static int unifykey_free_list(void)
{
	struct key_item_t *pre_item;

	pre_item = unifykey_item;
	while (pre_item) {
		unifykey_item = unifykey_item->next;
		kfree(pre_item);
		pre_item = unifykey_item;
	}
	return 0;
}

static int unifykey_item_parse_dt(struct device_node *node, int id)
{
	int count;
	int ret =  -1;
	const char *propname;
	struct key_item_t *temp_item = NULL;

	temp_item = kzalloc(sizeof(struct key_item_t), GFP_KERNEL);
	if (!temp_item) {
		ret = -ENOMEM;
		return ret;
	}
	propname = NULL;
	ret = of_property_read_string(node, "key-name", &propname);
	if (ret < 0) {
		pr_err("%s:%d,get key-name fail\n", __func__, __LINE__);
		ret = -EINVAL;
		goto exit;
	}
	if (propname) {
		count = strlen(propname);
		memset(temp_item->name, 0, KEY_UNIFY_NAME_LEN);
		if (count >= KEY_UNIFY_NAME_LEN)
			count = KEY_UNIFY_NAME_LEN-1;
		strncpy(temp_item->name, propname, count);
	}

	propname = NULL;
	ret = of_property_read_string(node, "key-device", &propname);
	if (ret < 0) {
		pr_err("%s:%d,get key-device fail\n", __func__, __LINE__);
		ret = -EINVAL;
		goto exit;
	}
	if (propname) {
		if (strcmp(propname, KEY_DEV_EFUSE) == 0)
			temp_item->dev = KEY_M_EFUSE;
		else if (strcmp(propname, KEY_DEV_NORMAL) == 0)
			temp_item->dev = KEY_M_NORMAL;
		else if (strcmp(propname, KEY_DEV_SECURE) == 0)
			temp_item->dev = KEY_M_SECURE;
		else
			temp_item->dev = KEY_M_UNKNOWN_DEV;
	}

	temp_item->permit = 0;
	if (of_property_match_string(node, "key-permit", KEY_PERMIT_READ) >= 0)
		temp_item->permit |= KEY_M_PERMIT_READ;
	if (of_property_match_string(node, "key-permit", KEY_PERMIT_WRITE) >= 0)
		temp_item->permit |= KEY_M_PERMIT_WRITE;
	if (of_property_match_string(node, "key-permit", KEY_PERMIT_DEL) >= 0)
		temp_item->permit |= KEY_M_PERMIT_DEL;
	temp_item->id = id;

	temp_item->attr = 0;
	ret = of_property_read_string(node, "key-encrypt", &propname);
	if (ret < 0)
		goto _next_attr;
	if (propname) {
		if (strcmp(propname, KEY_ATTR_TRUE) == 0)
			temp_item->attr = KEY_UNIFY_ATTR_ENCRYPT;
	}

_next_attr:
	/*todo, add new attribute here*/

	unifykey_add_to_list(temp_item);

	return 0;
exit:
	kfree(temp_item);
	return ret;
}



static int unifykey_item_create(struct platform_device *pdev, int num)
{
	int ret =  -1;
	int index;
	struct device_node *child;
	struct device_node *np = pdev->dev.of_node;

	of_node_get(np);
	index = 0;
	for_each_child_of_node(np, child) {
		ret = unifykey_item_parse_dt(child, index);
		if (!ret)
			index++;
	}
	pr_info("key unify fact unifykey-num is %d\n", index);

	return 0;
}

int unifykey_get_encrypt_type(void)
{
	return unify_key_info.encrypt_type;
}


int unifykey_dt_create(struct platform_device *pdev)
{
	int ret =  -1;
	int key_num;

	if (is_meson_m8b_cpu()) {
		/* do not care whether unifykey-encrypt really exists*/
		unify_key_info.encrypt_type = -1;
		ret = of_property_read_u32(pdev->dev.of_node,
			"unifykey-encrypt",
			&unify_key_info.encrypt_type);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"unifykey-num", &key_num);
	if (ret) {
		pr_err("%s:%d,don't find to match unifykey-num\n",
			__func__,
			__LINE__);
		return ret;
	}
	/* set default efuse version info */
	unify_key_info.efuse_version = -1;
	of_property_read_u32(pdev->dev.of_node, "efuse-version",
		&unify_key_info.efuse_version);

	pr_info("key unify config unifykey-num is %d\n", key_num);
	unify_key_info.key_num = key_num;
	if (!unify_key_info.key_flag) {
		unifykey_item_create(pdev, key_num);
		unify_key_info.key_flag = 1;
	}

	return ret;
}

int unifykey_dt_release(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		of_node_put(pdev->dev.of_node);
	unifykey_free_list();
	unify_key_info.key_flag = 0;
	return 0;
}



