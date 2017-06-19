/*
 * drivers/amlogic/media/common/codec_mm/configs/configs.c
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

#include <linux/amlogic/media/codec_mm/configs.h>
#include "configs_priv.h"
static struct mconfig_node root_node;
static struct mconfig_node *configs_get_root_node(void)
{
	return &root_node;
}

#ifdef DEBUG_CONFIG
#define config_debug(args...) pr_info(args)
#else
#define config_debug(args...)
#endif
int configs_inc_node_ref(
	struct mconfig_node *node)
{
	return atomic_inc_return(&node->ref_cnt);
}

int configs_dec_node_ref(
	struct mconfig_node *node)
{
	return atomic_dec_return(&node->ref_cnt);
}

static int configs_put_node(struct mconfig_node *node,
	int del_parent_ref, struct mconfig_node *root_node)
{
	struct mconfig_node *pnode;

	if (!node)
		return 0;
	configs_dec_node_ref(node);
	pnode = node->parent_node;
	if (!del_parent_ref)
		return 0;
	while ((pnode != NULL) && (pnode != root_node)) {
		configs_dec_node_ref(pnode);
		pnode = pnode->parent_node;
	}
	return 0;
}


static int configs_parser_first_node(
	const char *root_str,
	char *sub_str,
	int sub_size,
	int *is_end_node)
{
	int node_size;
	char *str;
	char *pstr;

	if (strlen(root_str) <= 0)
		return 0;
	str = strchr(root_str, '.');
	if (str != NULL) {
		node_size = str - root_str;
		*is_end_node = 0;
	} else {
		str = strchr(root_str, '=');
		if (str != NULL)
			node_size = str - root_str;
		else
			node_size = strlen(root_str);
		*is_end_node = 1;
	}
	if (node_size >= MAX_ITEM_NAME - 1)
		node_size = MAX_ITEM_NAME - 1;
	if (node_size > 0) {
		strncpy(sub_str, root_str, node_size);
		sub_str[node_size] = '\0';
		pstr = sub_str;
		while (pstr[0] != '\0' &&
			pstr[0] != ' ' &&
			pstr[0] != '\r' &&
			pstr[0] != '\n') {
			/*not space. */
			pstr++;
		}
		pstr[0] = '\0';
	} else
		sub_str[0] = '\0';
	return node_size;
}

static inline const char *configs_parser_str_valstr(
	const char *root_str)
{
	char *str;

	if (strlen(root_str) <= 0)
		return NULL;
	str = strchr(root_str, '=');
	if (str)
		str = str + 1;	/*del = */
	return str;
}

static int configs_parser_value_u32(const char *str, u32 *val)
{
	int ret = 0;
	const char *pstr;

	if (!str || strlen(str) <= 0)
		return 0;
	pstr = str;
	while (pstr[0] == ' ')
		pstr++;
	if (pstr[0] == '-') {
		ret = sscanf(pstr, "-%d", val);
		*val = -((int)*val);
	} else if (strstr(pstr, "0x"))
		ret = sscanf(pstr, "0x%x", val);
	else if (strstr(pstr, "0x"))
		ret = sscanf(pstr, "0x%x", val);
	else {
		ret = kstrtou32(pstr, 0, val);
		if (ret == 0)
			ret = 1;
	}
	return ret;
}

static int configs_parser_value_u64(const char *str, u64 *val)
{
	int ret = 0;
	const char *pstr;

	if (!str || strlen(str) <= 0)
		return 0;
	pstr = str;
	while (pstr[0] == ' ')
		pstr++;
	if (pstr[0] == '-') {
		ret = sscanf(pstr, "-%lld", val);
		*val = -((int)*val);
	} else if (strstr(pstr, "0x"))
		ret = sscanf(pstr, "0x%llx", val);
	else if (strstr(pstr, "0x"))
		ret = sscanf(pstr, "0x%llx", val);
	else {
		ret = kstrtou64(pstr, 0, val);
		if (ret == 0)
			ret = 1;
	}
	return ret;
}

static int configs_parser_value_bool(const char *str, bool *pbool)
{
	const char *pstr;

	if (!str || strlen(str) <= 0)
		return 0;
	pstr = str;
	while (pstr[0] == ' ')
		pstr++;
	if (str[0] == '1') {
		*pbool = true;
	} else if (str[0] == '0') {
		*pbool = false;
	} else if (!strcmp(str, "true") ||
			!strcmp(str, "TRUE") ||
			!strcmp(str, "true")) {
		*pbool = true;
	} else if (strcmp(str, "false") ||
			!strcmp(str, "FALSE") ||
			!strcmp(str, "False")) {
		*pbool = false;
	} else {
		return 0;
	}
	return 1;
}


static struct mconfig_node *configs_get_node_with_name(
	struct mconfig_node
	*rootnode, const char *node_name)
{
	struct mconfig_node *snode, *need;
	struct list_head *node_list, *next;

	need = NULL;
	if (!rootnode)
		rootnode = configs_get_root_node();
	mutex_lock(&rootnode->lock);
	if (list_empty(&rootnode->son_node_list)) {
		mutex_unlock(&rootnode->lock);
		return NULL;
	}
	list_for_each_safe(node_list, next, &rootnode->son_node_list) {
		snode = list_entry(node_list, struct mconfig_node, list);
		if (!strcmp(snode->name, node_name)) {
			need = snode;
			if (snode->active <= 0 ||
				configs_inc_node_ref(need) < 0)
				need = NULL;
			break;
		}
	}
	mutex_unlock(&rootnode->lock);
	return need;
}

int configs_init_new_node(struct mconfig_node *node,
	const char *name, int rw_flags)
{
	node->configs = NULL;
	node->configs_num = 0;
	node->son_node_num = 0;
	node->name = name;
	node->rw_flags = rw_flags;
	node->depth = 0;
	node->active = 1;
	node->prefix[0] = '\0';
	atomic_set(&node->ref_cnt, 0);
	mutex_init(&node->lock);
	INIT_LIST_HEAD(&node->list);
	INIT_LIST_HEAD(&node->son_node_list);
	return 0;
}
EXPORT_SYMBOL(configs_init_new_node);

int configs_register_node(struct mconfig_node *parent,
	struct mconfig_node *new_node)
{
	if (!parent)
		parent = configs_get_root_node();
	if (!parent || new_node == parent)
		return 0;	/*top node add. */
	if (parent->depth >= MAX_DEPTH)
		return -1;	/*too deep path */
	if (strlen(parent->prefix) + strlen(parent->name) >=
			MAX_PREFIX_NAME - 2) {
		pr_err("unsupport deep path %s.%s,len=%d > %d\n",
		       parent->prefix, parent->name,
		       (int)(strlen(parent->prefix) + strlen(parent->name) + 1),
		       MAX_PREFIX_NAME - 1);
		return -2;	/*unsuport deep path */
	}
	if (configs_get_node_with_name(parent, new_node->name) != NULL) {
		pr_err("have register same node[%s] on %s before\n",
			new_node->name, parent->name);
		return -3;
	}
	/*
	 *root.name.bb
	 *depth:0.1.2
	 *depth 0 &1 don't sed prefix.
	 *for ignore "root."
	 */
	new_node->depth = parent->depth + 1;
	if (new_node->depth >= 2) {
		if (parent->prefix[0]) {
			strcpy(new_node->prefix, parent->prefix);
			strcat(new_node->prefix, ".");
		}
		strcat(new_node->prefix, parent->name);
	}
	mutex_lock(&parent->lock);
	new_node->parent_node = parent;
	list_add_tail(&new_node->list, &parent->son_node_list);
	/*
	 *node parent not have write permissions,
	 *del the the node write permissions;
	 *same as read.
	 */
	new_node->rw_flags = parent->rw_flags & new_node->rw_flags;
	parent->son_node_num++;
	new_node->active = 1;
	mutex_unlock(&parent->lock);
	return 0;
}
EXPORT_SYMBOL(configs_register_node);

static struct mconfig_node *configs_get_node_path_end_node(
	struct mconfig_node *root_node, const char *path)
{
	char sub_node_name[128];
	struct mconfig_node *node, *start_node, *parent_node;
	int i, end;
	const char *next_path = path;

	if (!path)
		return NULL;
	end = 0;
	start_node = root_node;
	if (!root_node)
		start_node = configs_get_root_node();
	node = start_node;
	while (!end) {
		parent_node = node;
		i = configs_parser_first_node(next_path,
			sub_node_name, 128, &end);
		if (i <= 0 || sub_node_name[0] == '\0') {
			pr_err("can't find [%s] 's node!!\n", path);
			return NULL;
		}
		/*only no top node. */
		if (end && !root_node && node == start_node) {
			if (!strcmp(node->name, sub_node_name)) {
				/*need node is top node */
				break;
			}
		}
		config_debug("configs_parser_first_node{%s} end%d\n",
			sub_node_name, end);
		node = configs_get_node_with_name(parent_node,
			sub_node_name);
		if (node == NULL) {
			pr_err("can't find node:[%s], from:[%s], path=%s\n",
				sub_node_name, parent_node->name, path);
			return NULL;
		}
		next_path = next_path + i + 1;	/*media.vdec --> vdec */
		config_debug("get snode[%s] from node[%s], end=%d\n",
			node->name, parent_node->name, end);
	}
	return node;
}
EXPORT_SYMBOL(configs_get_node_path_end_node);

int configs_register_path_node(const char *path,
	struct mconfig_node *new_node)
{
	struct mconfig_node *parent = NULL;
	int ret;

	if (path && strlen(path) > 0) {
		parent = configs_get_node_path_end_node(NULL, path);
		if (!parent)
			return -1;
	}
	ret = configs_register_node(parent, new_node);
	configs_put_node(parent, 1, NULL);
	return ret;
}
EXPORT_SYMBOL(configs_register_path_node);

int configs_register_configs(struct mconfig_node *node,
	struct mconfig *configs, int num)
{
	if (!node)
		return -1;
	if (node->configs != NULL || node->configs_num > 0) {
		pr_err("node[%s] register config before!.\n", node->name);
		return -2;
	}
	if (0) {
		int i;

		for (i = 0; i < num; i++) {
			pr_info("init node:%s, config[i].data %lx-%lx\n",
				node->name,
				configs[i].ldata[0],
				configs[i].ldata[1]);
		}
	}
	mutex_lock(&node->lock);
	node->configs = configs;
	node->configs_num = num;
	mutex_unlock(&node->lock);
	return 0;
}
EXPORT_SYMBOL(configs_register_configs);

int configs_register_path_configs(const char *path,
	struct mconfig *configs, int num)
{
	struct mconfig_node *parent = NULL;
	int ret;

	if (path && strlen(path) > 0) {
		parent = configs_get_node_path_end_node(NULL, path);
		if (!parent)
			return -1;
		ret = configs_register_configs(parent, configs, num);
		configs_put_node(parent, 1, NULL);
		return ret;
	}
	return -1;
}
EXPORT_SYMBOL(configs_register_path_configs);

int configs_del_endnode(struct mconfig_node *parent,
	struct mconfig_node *node)
{
	struct mconfig_node *parent_node;

	parent_node = node->parent_node;
	node->active = 0;
	/*always set it to no active */
	if (atomic_read(&node->ref_cnt) != 0)
		return -1;
	/*always del configs. */
	node->configs = NULL;
	node->configs_num = 0;
	/*do't del node when have son node. */
	if (node->son_node_num > 0)
		return -2;
	if (parent) {
		mutex_lock(&parent->lock);
		list_del(&node->list);
		parent->son_node_num--;
		mutex_unlock(&parent->lock);
	}
	return 0;
}

static struct mconfig *configs_get_node_config(
	struct mconfig_node *node, char *name)
{
	int i;
	struct mconfig *config = NULL;

	mutex_lock(&node->lock);
	for (i = 0; i < node->configs_num; i++) {
		struct mconfig *val = &node->configs[i];

		if (val && !strcmp(val->item_name, name)) {
			configs_inc_node_ref(node);
			config = val;
			break;
		}
	}
	mutex_unlock(&node->lock);
	config_debug("get config[%s] from %s-end-%p\n",
		name, node->name, (void *)config);
	return config;
}

static int configs_config2str(struct mconfig *config,
	char *buf, int size)
{
	int ret = 0;

	if (size < 1)
		return 0;
	switch (config->type) {
	case CONFIG_TYPE_PBOOL:
		ret = snprintf(buf, size, "%s",
			config->pboolval[0] ? "true" : "false");
		break;
	case CONFIG_TYPE_PI32:
		ret = snprintf(buf, size, "%u", config->pival[0]);
		break;
	case CONFIG_TYPE_PU32:
		ret = snprintf(buf, size, "%d", config->pu32val[0]);
		break;
	case CONFIG_TYPE_PU64:
		ret = snprintf(buf, size, "%llx", config->pu64val[0]);
		break;
	case CONFIG_TYPE_PSTR:
	case CONFIG_TYPE_PCSTR:
		ret = snprintf(buf, size, "%s", config->str);
		break;
	case CONFIG_TYPE_BOOL:
		ret = snprintf(buf, size, "%s",
			config->pboolval ? "true" : "false");
	case CONFIG_TYPE_I32:
		ret = snprintf(buf, size, "%d", config->ival);
		break;
	case CONFIG_TYPE_U32:
		ret = snprintf(buf, size, "%u", config->u32val);
		break;
	case CONFIG_TYPE_U64:
		ret = snprintf(buf, size, "0x%llx", config->u64val);
		break;
	case CONFIG_TYPE_FUN:
		if (!config->f_get)
			ret = 0;
		else
			ret = config->f_get(config->item_name,
				config->id, buf, size);
		break;
	default:
		ret = -4;
	}
	if (ret <= 0) {
		pr_err("config2str error %s-type:%d,%lx-%lx,ret=%d\n",
		       config->item_name, config->type,
		       config->ldata[0], config->ldata[1], ret);
	} else if (ret >= size) {
		buf[size - 1] = '\0';
		ret = size - 1;
	}
	return ret;
}

static int configs_str2config(struct mconfig *config,
	const char *str)
{
	int ret = 0;
	u32 val;
	u64 val64;
	bool bval;

	if (!str || strlen(str) <= 0)
		return 0;
	switch (config->type) {
	case CONFIG_TYPE_PBOOL:
	case CONFIG_TYPE_BOOL:
		ret = configs_parser_value_bool(str, &bval);
		if (ret <= 0)
			break;
		if (config->type == CONFIG_TYPE_PBOOL)
			config->pboolval[0] = bval;
		else
			config->boolval = bval;
		break;
	case CONFIG_TYPE_PI32:
	case CONFIG_TYPE_PU32:
		ret = configs_parser_value_u32(str, (u32 *) &val);
		if (ret > 0)
			config->pu32val[0] = val;
		break;
	case CONFIG_TYPE_PU64:
		ret = configs_parser_value_u64(str, (u64 *) &val64);
		if (ret > 0)
			config->pu64val[0] = val64;
		break;
	case CONFIG_TYPE_PSTR:
		strncpy(config->str, str, config->size);
		ret = strlen(config->str);
		break;
	case CONFIG_TYPE_I32:
	case CONFIG_TYPE_U32:
		ret = configs_parser_value_u32(str, (u32 *) &val);
		if (ret > 0)
			config->u32val = val;
		break;
	case CONFIG_TYPE_U64:
		ret = configs_parser_value_u64(str, (u64 *) &val64);
		if (ret > 0)
			config->u64val = val64;
		break;
	case CONFIG_TYPE_FUN:
		if (!config->f_set)
			ret = -1;
		else
			ret = config->f_set(config->item_name,
					config->id,
					str, strlen(str));
		break;
	case CONFIG_TYPE_PCSTR:	/*can't set. */
	default:
		ret = -4;
	}
	return ret;
}

static char *configs_build_prefix(
		struct mconfig_node *node,
		const char *prefix,
		int mode,
		char *buf,
		int size)
{
	if (!buf)
		return "";
	buf[0] = '\0';
	if (mode & LIST_MODE_PATH_FULLPREFIX) {
		if (node->prefix[0]) {
			snprintf(buf, size, "%s",
				node->prefix);
		}
	} else if (mode & LIST_MODE_PATH_PREFIX) {
		if (prefix && prefix[0]) {
			snprintf(buf, size, "%s",
				prefix);
		}
	}
	if (buf[0] && buf[strlen(buf) - 1] != '.')
		strncat(buf, ".", size);
	return buf;
}

static int configs_list_node_configs_locked(
	struct mconfig_node *node,
	char *buf, int size, const char *prefix,
	int mode)
{
	int i;
	int ret;
	int pn_size = 0;
	struct mconfig *config = NULL;
	const char *cprefix = prefix;

	if (!buf || size < 8 || !node->configs)
		return 0;
	if (!cprefix)
		cprefix = "";
	for (i = 0; i < node->configs_num && (size - pn_size) > 8; i++) {
		config = &node->configs[i];
		ret = snprintf(buf + pn_size,
				size - pn_size,
				" %s%s.%s%s",
			     cprefix,
			     node->name,
			     config->item_name,
			     (node->rw_flags & CONFIG_FOR_R) ? "=" : "");
		if (ret > 0)
			pn_size += ret;
		if ((mode & LIST_MODE_VAL) &&
			((mode & CONFIG_FOR_T) ||
			!(CONFIG_FOR_T & node->rw_flags)) &&
			(node->rw_flags & CONFIG_FOR_R)) {
			ret = configs_config2str(config,
				buf + pn_size, size - pn_size);
			if (ret > 0) {
				if ((pn_size + ret) < size) {
					strcat(buf, "\n");
					ret++;
				}
				pn_size += ret;
			} else {
				if (ret == 0 && (pn_size + ret) < size) {
					strcat(buf, "\n");
					ret++;
					continue;
				}
				break;
			}
		} else {
			ret = snprintf(buf + pn_size, size - pn_size, "\n");
			if (ret > 0)
				pn_size += ret;
		}
	}
	return pn_size;
}

int configs_list_node_configs(struct mconfig_node *node,
	char *buf, int size,
	int mode)
{
	int ret;

	mutex_lock(&node->lock);
	ret = configs_list_node_configs_locked(node, buf,
		size, NULL, mode);
	mutex_unlock(&node->lock);
	return ret;
}

static int configs_list_nodes_in(struct mconfig_node *node,
	char *buf, int size, const char *prefix,
	int mode)
{
	int ret;
	int pn_size = 0;
	char cprefix[MAX_PREFIX_NAME + MAX_ITEM_NAME];
	char *c_prefix;
	char rw[4][4] = { "N", "r", "w", "rw" };

	if (!node || !buf || size <= 0)
		return 0;
	if (!(node->rw_flags & mode))
		return 0;/*LIST_MODE_LIST_RD/WD flags*/
	config_debug("start dump node %s...\n", node->name);
	c_prefix = configs_build_prefix(node,
				prefix,
				mode,
				cprefix,
				MAX_PREFIX_NAME + MAX_ITEM_NAME);
	mutex_lock(&node->lock);
	if (mode & LIST_MODE_NODE_INFO) {
		ret = snprintf(buf + pn_size, size - pn_size,
				"[NODE]%s%s:[%s/%d/%d/%d]\n",
			     c_prefix,
			     node->name,
			     rw[(node->rw_flags & 3)],
			     atomic_read(&node->ref_cnt),
			     node->configs_num,
			     node->depth);
		if (ret > 0)
			pn_size += ret;
	}
	if (mode & LIST_MODE_CONFIGS_VAL) {
		ret = configs_list_node_configs_locked(node, buf + pn_size,
			size - pn_size, c_prefix, mode);
		if (ret > 0)
			pn_size += ret;
	}
	if ((mode & LIST_MODE_SUB_NODES) && node->son_node_num > 0) {
		struct mconfig_node *snode;
		struct list_head *node_list, *next;

		if (mode & LIST_MODE_PATH_PREFIX) {
			if (node->depth > 0)/*not root.*/
				strncat(cprefix, node->name,
					MAX_PREFIX_NAME + MAX_ITEM_NAME);
		}
		list_for_each_safe(node_list, next, &node->son_node_list) {
			snode = list_entry(node_list,
					struct mconfig_node, list);
			if (snode) {
				ret = configs_list_nodes_in(snode,
					buf + pn_size, size - pn_size,
					cprefix, mode);
				if (ret > 0)
					pn_size += ret;
			}
		}
	}
	mutex_unlock(&node->lock);
	return pn_size;
}

int configs_list_nodes(struct mconfig_node *node,
	char *buf, int size, int mode)
{
	if (!node)
		node = configs_get_root_node();
	return configs_list_nodes_in(node, buf, size,
			NULL,
			mode);
}

int configs_list_path_nodes(const char *prefix,
	char *buf, int size, int mode)
{
	struct mconfig_node *node;
	int ret = 0;

	node = configs_get_node_path_end_node(NULL, prefix);
	if (node != NULL) {
		ret = configs_list_nodes(node, buf, size, mode);
		configs_put_node(node, 1, NULL);
	}
	return ret;
}

static int configs_get_node_path_config(
	struct mconfig_node *root_node,
	struct mconfig **config_ret,
	const char *path,
	struct mconfig_node **hold_node, int set)
{
	char sub_node_name[128];
	struct mconfig_node *node, *parent_node;
	struct mconfig *config = NULL;
	int i, end;
	const char *next_path = path;
	int err = 0;

	if (!path)
		return -EIO;
	end = 0;
	if (!root_node)
		root_node = configs_get_root_node();
	node = root_node;
	while (!end) {
		parent_node = node;
		i = configs_parser_first_node(next_path,
			sub_node_name, 128, &end);
		if (i <= 0 || sub_node_name[0] == '\0') {
			pr_err("can't find [%s] 's node!!\n", path);
			return -1;
		}
		config_debug("configs_parser_first_node{%s} end%d\n",
			sub_node_name, end);
		if (!end) {
			node = configs_get_node_with_name(parent_node,
				sub_node_name);
			if (node == NULL) {
				if (parent_node != root_node) {
					node = parent_node;
					goto out;
				}
				/*for reset refs. */
				pr_err("can't find node:[%s], from:[%s], path=%s\n",
				       sub_node_name, parent_node->name, path);
				err = -ENOENT;
				goto out;
			}
			next_path = next_path + i + 1;	/*media.vdec --> vdec */
			config_debug("get snode[%s] from node[%s], end=%d\n",
				node->name, parent_node->name, end);
		}
	}
	if (set && !(node->rw_flags & CONFIG_FOR_W)) {
		err = -ENOENT;
		goto out;
	}
	if (!set && !(node->rw_flags & CONFIG_FOR_R)) {
		err = -EPERM;
		goto out;
	}
	config = configs_get_node_config(node, sub_node_name);
	if (!config) {
		/*release node refs. */
		err = -EPERM;
	} else
		*hold_node = node;
 out:
	if (config == NULL) {
		if (node) {
			configs_put_node(node, 1, root_node);
			pr_err("can't find node %s's  config:%s\n",
				node->name, sub_node_name);
		} else
			pr_err("can't find node %s from %s\n",
			sub_node_name, root_node->name);
	} else {
		*config_ret = config;
		err = 0;
	}
	return err;
}

static int configs_setget_config_value(struct mconfig *config,
	const void *val_set, void *val_get, int size, int set)
{
	int s;
	int ret;
	void *dst;
	const void *src;

	if (set) {
		dst = config->buf_ptr;
		src = val_set;
	} else {
		dst = val_get;
		src = config->buf_ptr;
	}
	if (!dst || !src)
		return -1;
	switch (config->type) {
	case CONFIG_TYPE_PU32:
	case CONFIG_TYPE_PI32:
		if (size < sizeof(u32))
			return -2;
		((u32 *) dst)[0] = ((const u32 *)src)[0];
		ret = sizeof(u32);
		break;
	case CONFIG_TYPE_PU64:
		if (size < sizeof(u64))
			return -2;
		((u64 *) dst)[0] = ((const u64 *)src)[0];
		ret = sizeof(u64);
		break;
	case CONFIG_TYPE_PCSTR:
		if (set) {
			ret = -3;
			break;
		}
		/*rd same as CONFIG_TYPE_PSTR */
	case CONFIG_TYPE_PSTR:
		s = min_t(int, size - 1, config->size - 1);
		if (s > 0)
			strncpy(dst, src, s);
		else
			ret = -3;
		ret = s;
		break;
	case CONFIG_TYPE_U32:
	case CONFIG_TYPE_I32:
		if (size < sizeof(u32))
			return -2;
		if (set)
			config->u32val = ((const u32 *)src)[0];
		else
			((u32 *) dst)[0] = config->u32val;
		ret = sizeof(u32);
		break;
	case CONFIG_TYPE_U64:
		if (size < sizeof(u64))
			return -2;
		if (set)
			config->u64val = ((const u64 *)src)[0];
		else
			((u64 *) dst)[0] = config->u64val;
		ret = sizeof(u64);
		break;
	default:
		ret = -4;
	}
	return ret;
}

static int configs_setget_node_path_value(
	struct mconfig_node *topnode,
	const char *path,
	const void *val_set, void *val_get,
	int size, int set)
{
	struct mconfig_node *node;
	struct mconfig *config;
	int ret;

	ret = configs_get_node_path_config(topnode, &config, path, &node, set);
	if (ret != 0)
		return ret;
	if (set)
		pr_err("start set config val config=%s, val=%x\n",
		config->item_name, *(int *)val_set);
	ret = configs_setget_config_value(config, val_set,
		val_get, size, set);
	config_debug("setget config val config=%s, ret=%x end\n",
		config->item_name, ret);
	configs_put_node(node, 0, topnode);
	configs_put_node(node, 1, topnode);
	return ret;
}

int configs_set_node_path_str(struct mconfig_node *topnode,
	const char *path, const char *val)
{
	struct mconfig_node *node;
	struct mconfig *config;
	int ret = -1;

	if (!val || !path || strlen(path) <= 0 || strlen(val) <= 0)
		return -1;
	ret = configs_get_node_path_config(topnode, &config, path, &node, 1);
	if (ret != 0)
		return ret;
	config_debug("start set config val config=%s, %s\n",
		config->item_name, val);
	mutex_lock(&node->lock);
	ret = configs_str2config(config, val);
	mutex_unlock(&node->lock);
	configs_put_node(node, 0, topnode);
	configs_put_node(node, 1, topnode);
	if (ret > 0)
		ret = 0;/*set ok*/
	else if (ret == 0)
		ret = -1;/*val not changed.*/
	return ret;
}
int configs_set_node_nodepath_str(struct mconfig_node *topnode,
	const char *path, const char *val)
{
	if (topnode == NULL) {
		return configs_set_node_path_str(topnode,
			path, val);
	}
	if (strncmp(topnode->name, path, strlen(topnode->name))) {
		pr_err("nodepath(%s) must start from node=%s\n",
			path, topnode->name);
		return -1;
	}
	return configs_set_node_path_str(topnode,
			path + strlen(topnode->name) + 1, val);
}

int configs_set_prefix_path_str(const char *prefix,
	const char *path, const char *str)
{
	struct mconfig_node *topnode = NULL;
	int ret;

	if (prefix && strlen(prefix) > 0) {
		topnode = configs_get_node_path_end_node(NULL, prefix);
		if (!topnode) {
			pr_err("[0]can't get node from %s\n", prefix);
			return -1;
		}
	}
	ret = configs_set_node_path_str(topnode, path, str);
	configs_put_node(topnode, 1, NULL);
	return ret;
}

int configs_set_node_path_valonpath(
	struct mconfig_node *topnode,
	const char *path)
{
	return configs_set_node_path_str(topnode, path,
		configs_parser_str_valstr(path));
}

int configs_set_prefix_path_valonpath(const char *prefix,
	const char *path)
{
	struct mconfig_node *topnode = NULL;
	int ret;

	if (prefix && strlen(prefix) > 0) {
		topnode = configs_get_node_path_end_node(NULL, prefix);
		if (!topnode) {
			pr_err("can't get node from %s\n", prefix);
			return -1;
		}
	}
	ret = configs_set_node_path_str(topnode, path,
		configs_parser_str_valstr(path));
	configs_put_node(topnode, 1, NULL);
	return ret;
}

int configs_get_node_path_str(struct mconfig_node *topnode,
	const char *path, char *buf, int size)
{
	struct mconfig_node *node;
	struct mconfig *config;
	int ret = -1;

	if (!path || strlen(path) <= 0)
		return -1;
	ret = configs_get_node_path_config(topnode,
		&config, path, &node, 0);
	if (ret != 0)
		return ret;
	config_debug("startget config val config=%s\n",
		config->item_name);
	ret = configs_config2str(config, buf, size);
	configs_put_node(node, 0, topnode);
	configs_put_node(node, 1, topnode);
	return ret;
}
int configs_get_node_nodepath_str(struct mconfig_node *topnode,
	const char *path, char *buf, int size)
{
	if (topnode == NULL)
		return configs_get_node_path_str(topnode, path, buf, size);
	if (strncmp(topnode->name, path, strlen(topnode->name))) {
		pr_err("nodepath(%s) must start from node=%s\n",
			path, topnode->name);
		return -1;
	}
	return configs_get_node_path_str(topnode,
		path + strlen(topnode->name) + 1,
		buf, size);

}

static inline int configs_set_node_path_value(
	struct mconfig_node *topnode,
	const char *path, const void *val, int size)
{
	return configs_setget_node_path_value(topnode,
		path, val, NULL, size, 1);
}

static int configs_get_node_path_value(struct mconfig_node *topnode,
	const char *path, void *val, int size)
{
	return configs_setget_node_path_value(topnode,
		path, NULL, val, size, 0);
}

int configs_get_node_path_u32(struct mconfig_node *topnode,
	const char *path, u32 *val)
{
	return configs_get_node_path_value(topnode, path, val, sizeof(u32));
}

int configs_get_node_path_u64(struct mconfig_node *topnode,
	const char *path, u64 *val)
{
	return configs_get_node_path_value(topnode, path, val, sizeof(u64));
}

int configs_set_node_path_u32(struct mconfig_node *topnode,
	const char *path, u32 val)
{
	return configs_set_node_path_value(topnode, path, &val, sizeof(u32));
}

int configs_set_node_path_u64(struct mconfig_node *topnode,
	const char *path, u64 val)
{
	return configs_set_node_path_value(topnode, path, &val,
		sizeof(u64));
}

int configs_config_system_init(void)
{
	configs_init_new_node(&root_node, "root", CONFIG_FOR_RW);
	configs_register_node(NULL, &root_node);
	return 0;
}
