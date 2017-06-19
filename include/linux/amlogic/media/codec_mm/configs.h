/*
 * include/linux/amlogic/media/codec_mm/configs.h
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

#ifndef AMLOGIC_MEDIA_CONFIG_HEADER__
#define AMLOGIC_MEDIA_CONFIG_HEADER__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/amlogic/media/codec_mm/configs_api.h>

#define MAX_DEPTH 8

typedef int (*get_fun)(const char *trigger, int id, char *buf, int size);

typedef int (*set_fun)(const char *trigger, int id, const char *buf, int size);

struct mconfig {
#define CONFIG_TYPE_PI32 1
#define CONFIG_TYPE_PU32 2
#define CONFIG_TYPE_PU64 4
#define CONFIG_TYPE_PBOOL 5

#define CONFIG_TYPE_PSTR 7
#define CONFIG_TYPE_PCSTR 8
#define CONFIG_TYPE_I32 21
#define CONFIG_TYPE_U32 22
#define CONFIG_TYPE_U64 24
#define CONFIG_TYPE_BOOL 25

#define CONFIG_TYPE_FUN 100
	int type;
	int id;
	const char *item_name;
	union {
		void *buf_ptr;
		char *str;
		const char *cstr;

		int *pival;
		u32 *pu32val;
		u64 *pu64val;

		int ival;
		u32 u32val;
		u32 u32mval[2];
		u64 u64val;
		bool boolval;
		bool *pboolval;
		char b[8];
		long ldata[2];
		struct {
			get_fun f_get;	/*used 0 */
			set_fun f_set;	/*used 1 */
		};
	};
	int size;
};

struct mconfig_node {
	char prefix[MAX_PREFIX_NAME];
	const char *name;
	int active;
	int depth;
	struct mconfig_node *parent_node;
	struct list_head list;
	int son_node_num;
	struct list_head son_node_list;
	struct mutex lock;
	int configs_num;
	struct mconfig *configs;
	atomic_t ref_cnt;
	 /*
	  *rw_flags:
	  * CONFIG_FOR_R: support read.
	  * CONFIG_FOR_W: support write.
	  * CONFIG_FOR_T: CONFIG FOR TRIGGER;
	  */
#define CONFIG_FOR_R 1
#define CONFIG_FOR_W 2
#define CONFIG_FOR_T 4
#define CONFIG_FOR_RW (CONFIG_FOR_R | CONFIG_FOR_W)
	int rw_flags;
};

#define MC_U32(n, v) \
	{.type = CONFIG_TYPE_U32, .item_name = (n), .u32val = (v)}

#define MC_I32(n, v) \
	{.type = CONFIG_TYPE_I32, .item_name = (n), .ival = (v)}

#define MC_U64(n, v) \
	{.type = CONFIG_TYPE_U32, .item_name = (n), .u64val = (v)}
#define MC_BOOL(n, v) \
	{.type = CONFIG_TYPE_BOOL, .item_name = (n), .boolval = (v)}

#define MC_STR(n, v) \
	{.type = CONFIG_TYPE_PSTR, .item_name = (n), .str = (v)}

#define MC_CSTR(n, v) \
	{.type = CONFIG_TYPE_PCSTR, .item_name = (n), .cstr = (v)}

#define MC_PI32(n, v) \
	{.type = CONFIG_TYPE_PI32, .item_name = (n), .pival = (v)}

#define MC_PU32(n, v) \
	{.type = CONFIG_TYPE_PU32, .item_name = (n), .pu32val = (v)}

#define MC_PU64(n, v) \
	{.type = CONFIG_TYPE_PU64, .item_name = (n), .pu64val = (v)}

#define MC_PBOOL(n, v) \
	{.type = CONFIG_TYPE_PBOOL, .item_name = (n), .pboolval = (v)}

#define MC_FUN(n, fget, fset) \
	{.type = CONFIG_TYPE_FUN, .item_name = (n),\
	.f_get = (fget), .f_set = (fset)}

#define MC_FUN_ID(n, fget, fset, cid) \
	{.type = CONFIG_TYPE_FUN, .item_name = (n),\
	.f_get = (fget), .f_set = (fset), .id = (cid)}


#define REG_CONFIGS(node, configs) \
	configs_register_configs(node, configs,\
	sizeof(configs)/sizeof(struct mconfig))

#define REG_PATH_CONFIGS(path, configs) \
		configs_register_path_configs(path, configs,\
		sizeof(configs)/sizeof(struct mconfig))

#define INIT_REG_NODE_CONFIGS(prefix, node, name, config, rw)\
	do {\
		configs_init_new_node(node, name, rw);\
		REG_CONFIGS(node, config);\
		configs_register_path_node(prefix, node);\
	} while (0)

int configs_inc_node_ref(struct mconfig_node *node);
int configs_dec_node_ref(struct mconfig_node *node);

int configs_init_new_node(
	struct mconfig_node *node, const char *name,
	int rw_flags);

int configs_register_node(
	struct mconfig_node *parent,
	struct mconfig_node *new_node);
int configs_register_path_node(
	const char *path, struct mconfig_node *new_node);

int configs_register_configs(
	struct mconfig_node *node, struct mconfig *configs,
	int num);
int configs_register_path_configs(
	const char *path, struct mconfig *configs,
	int num);

int configs_del_endnode(
	struct mconfig_node *parent, struct mconfig_node *node);
int configs_get_node_path_u32(
	struct mconfig_node *topnode, const char *path,
	u32 *val);
int configs_get_node_path_u64(
	struct mconfig_node *topnode, const char *path,
	u64 *val);
int configs_get_node_path_2u32(
	struct mconfig_node *topnode, const char *path,
	u32 *val);
int configs_get_node_path_buf(
	struct mconfig_node *topnode, const char *path,
	char *buf, int size);
int configs_set_node_path_u32(
	struct mconfig_node *topnode, const char *path,
	u32 val);
int configs_set_node_path_u64(
	struct mconfig_node *topnode, const char *path,
	u64 val);
int configs_set_node_path_2u32(
	struct mconfig_node *topnode, const char *path,
	u32 val1, u32 val2);

int configs_set_node_path_str(
	struct mconfig_node *topnode, const char *path,
	const char *val);
int configs_set_node_path_valonpath(
	struct mconfig_node *topnode,
	const char *path);

static inline int configs_set_path_str(
	const char *path, const char *val)
{
	return configs_set_node_path_str(NULL, path, val);
}

int configs_set_node_path_str(
	struct mconfig_node *topnode, const char *path,
	const char *val);

int configs_set_node_nodepath_str(
	struct mconfig_node *topnode,
	const char *path, const char *val);

static inline int configs_set_path_valonpath(
	const char *path)
{
	return configs_set_node_path_valonpath(NULL, path);
}

int configs_set_prefix_path_valonpath(
	const char *prfix, const char *pathval);
int configs_set_prefix_path_str(
	const char *prfix, const char *path,
	const char *str);

int configs_get_node_path_str(
	struct mconfig_node *topnode, const char *path,
	char *buf, int size);

int configs_get_node_nodepath_str(
	struct mconfig_node *topnode,
	const char *path, char *buf, int size);

#define LIST_MODE_LIST_RD			(CONFIG_FOR_R)
#define LIST_MODE_LIST_WD			(CONFIG_FOR_W)
#define LIST_MODE_LIST_RW (LIST_MODE_LIST_RD | LIST_MODE_LIST_WD)
#define LIST_MODE_VAL				(1<<8)
#define LIST_MODE_CONFIGS			(1<<9)
#define LIST_MODE_CONFIGS_VAL	(LIST_MODE_VAL | LIST_MODE_CONFIGS)
#define LIST_MODE_NODE_INFO		(1<<10)
#define LIST_MODE_PATH_NODE		(1<<11)
#define LIST_MODE_PATH_PREFIX	(1<<12)
#define LIST_MODE_PATH_FULLPREFIX   (1<<13)
#define LIST_MODE_SUB_NODES		(1<<15)

#define LIST_MODE_NODE_PATH_CONFIGS \
	(LIST_MODE_LIST_RW | LIST_MODE_PATH_PREFIX | LIST_MODE_CONFIGS)
#define LIST_MODE_NODE_CMD_VAL \
		(LIST_MODE_NODE_PATH_CONFIGS | LIST_MODE_VAL)
#define LIST_MODE_NODE_CMD_ALL \
	(LIST_MODE_NODE_CMD_VAL | LIST_MODE_SUB_NODES | LIST_MODE_NODE_INFO)
#define LIST_MODE_NODE_CMDVAL_ALL \
	(LIST_MODE_NODE_CMD_ALL | LIST_MODE_VAL)
#define LIST_MODE_FULL_CMA_ALL \
	(LIST_MODE_NODE_CMD_ALL | LIST_MODE_PATH_FULLPREFIX)
#define LIST_MODE_FULL_CMDVAL_ALL \
	(LIST_MODE_FULL_CMA_ALL | LIST_MODE_VAL)

int configs_list_node_configs(struct mconfig_node *node, char *buf, int size,
							  int mode);
int configs_list_nodes(struct mconfig_node *node, char *buf, int size,
					   int mode);
int configs_list_path_nodes(const char *prefix, char *buf, int size, int mode);

#endif
