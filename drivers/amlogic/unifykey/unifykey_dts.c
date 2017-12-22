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
#include "unifykey.h"

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

/* storages where key stored */
#define KEY_DEV_EFUSE		"efuse"
#define KEY_DEV_NORMAL		"normal"
#define KEY_DEV_SECURE		"secure"

/* compatibility for old names */
#define KEY_COMP_DEV_EFUSE		"efusekey"
#define KEY_COMP_DEV_NORMAL		"nandkey"
#define KEY_COMP_DEV_SECURE		"secureskey"

/* permision */
#define KEY_PERMIT_READ		"read"
#define KEY_PERMIT_WRITE	"write"
#define KEY_PERMIT_DEL		"del"

/* attribute */
#define KEY_ATTR_TRUE		"true"
#define KEY_ATTR_FALSE		"false"


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

struct key_item_t *unifykey_find_item_by_name(struct list_head *list,
		char *name)
{
	struct key_item_t *item;

	list_for_each_entry(item, list, node)
		if (!strncmp(item->name, name,
				((strlen(item->name) > strlen(name))
				? strlen(item->name) : strlen(name))))
			return item;

	return NULL;
}

struct key_item_t *unifykey_find_item_by_id(struct list_head *list, int id)
{
	struct key_item_t *item;

	list_for_each_entry(item, list, node)
		if (item->id == id)
			return item;

	return NULL;
}

int unifykey_count_key(struct list_head *list)
{
	int count = 0;
	struct list_head *node;

	list_for_each(node, list)
		count++;

	return count;
}

char unifykey_get_efuse_version(struct key_info_t *uk_info)
{
	char ver = 0;

	if (uk_info->efuse_version != -1)
		ver = (char)(uk_info->efuse_version);

	return ver;
}

static int unifykey_add_to_list(struct list_head *list, struct key_item_t *item)
{
	list_add(&(item->node), list);

	return 0;
}

static int unifykey_free_list(struct list_head *list)
{
	struct key_item_t *item;
	struct key_item_t *tmp;


	list_for_each_entry_safe(item, tmp, list, node) {
		list_del(&(item->node));
		kfree(item);
	}

	return 0;
}


int parse_extra_df_node_m8b(struct device_node *node, unsigned int *df)
{
	int ret =  -1;
	const char *propname;

	*df = KEY_M_MAX_DF;
	ret = of_property_read_string(node, "key-dataformat",
				&propname);
	if (ret < 0) {
		ret = -EINVAL;
		return ret;
	}

	if (propname) {
		if (strcmp(propname, "hexdata") == 0)
			*df = KEY_M_HEXDATA;
		else if (strcmp(propname, "hexascii") == 0)
			*df = KEY_M_HEXASCII;
		else if (strcmp(propname, "allascii") == 0)
			*df = KEY_M_ALLASCII;
		else
			*df = KEY_M_MAX_DF;
	}

	return 0;
}


static int unifykey_item_parse_dt(struct aml_unifykey_dev *ukdev,
	struct device_node *node, int id)
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
		if (strcmp(propname, KEY_DEV_EFUSE) == 0 ||
			strcmp(propname, KEY_COMP_DEV_EFUSE) == 0)
			temp_item->dev = KEY_M_EFUSE;
		else if (strcmp(propname, KEY_DEV_NORMAL) == 0 ||
			strcmp(propname, KEY_COMP_DEV_NORMAL) == 0)
			temp_item->dev = KEY_M_NORMAL;
		else if (strcmp(propname, KEY_DEV_SECURE) == 0 ||
			strcmp(propname, KEY_COMP_DEV_SECURE))
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

	if (ukdev->ops->parse_extra_df_node != NULL) {
		ret = ukdev->ops->parse_extra_df_node(node, &(temp_item->df));
		if (ret != 0)
			goto exit;
	}

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

	unifykey_add_to_list(&(ukdev->uk_header), temp_item);

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
	struct aml_unifykey_dev *ukdev = platform_get_drvdata(pdev);

	of_node_get(np);
	index = 0;
	for_each_child_of_node(np, child) {
		ret = unifykey_item_parse_dt(ukdev, child, index);
		if (!ret)
			index++;
	}
	pr_info("key unify fact unifykey-num is %d\n", index);

	return 0;
}

int unifykey_get_encrypt_type(struct key_info_t *uk_info)
{
	return uk_info->encrypt_type;
}


int unifykey_dt_create(struct platform_device *pdev)
{
	int ret =  -1;
	int key_num;
	struct aml_unifykey_dev *ukdev = platform_get_drvdata(pdev);

	/* do not care whether unifykey-encrypt really exists*/
	ukdev->uk_info.encrypt_type = -1;
	ret = of_property_read_u32(pdev->dev.of_node,
		"unifykey-encrypt",
		&(ukdev->uk_info.encrypt_type));

	ret = of_property_read_u32(pdev->dev.of_node,
		"unifykey-num", &key_num);
	if (ret) {
		pr_err("%s:%d,don't find to match unifykey-num\n",
			__func__,
			__LINE__);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "efuse-version",
		&(ukdev->uk_info.efuse_version));
	if (ret != 0) {
		pr_info("no efuse-version set, use default value: -1\n");
		ukdev->uk_info.efuse_version = -1;
		ret = 0;
	}

	pr_info("key unify config unifykey-num is %d\n", key_num);
	ukdev->uk_info.key_num = key_num;
	if (!(ukdev->uk_info.key_flag)) {
		unifykey_item_create(pdev, key_num);
		ukdev->uk_info.key_flag = 1;
	}

	return ret;
}

int unifykey_dt_release(struct platform_device *pdev)
{
	struct aml_unifykey_dev *ukdev = platform_get_drvdata(pdev);

	if (pdev->dev.of_node)
		of_node_put(pdev->dev.of_node);
	unifykey_free_list(&(ukdev->uk_header));
	ukdev->uk_info.key_flag = 0;
	return 0;
}



