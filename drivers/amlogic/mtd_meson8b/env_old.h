/*
 * drivers/amlogic/mtd_old/env_old.h
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
#ifndef __AML_OLD_ENV_H__
#define __AML_OLD_ENV_H__


#include "aml_mtd.h"

#ifdef CONFIG_ENV_SIZE
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE	0x4000
#endif

#define ENV_SIZE (CONFIG_ENV_SIZE - (sizeof(uint32_t)))


#define NAND_MINI_PART_SIZE	(MINI_PART_SIZE)
#define ENV_NAND_SCAN_BLK	50

struct env_oobinfo_t {
    char	name[4];
    int16_t	ec;
    unsigned	timestamp: 15;
    unsigned	status_page: 1;
};


struct env_valid_node_t {
	int16_t  ec;
	int16_t phy_blk_addr;
	int16_t phy_page_addr;
	int timestamp;
#ifdef NAND_KEY_SAVE_MULTI_BLOCK
	int rd_flag;
	struct env_valid_node_t *next;
#endif
	int16_t env_status;
};

struct env_free_node_t {
	int16_t  ec;
	int16_t phy_blk_addr;
	int dirty_flag;
	struct env_free_node_t *next;
};

struct aml_nandenv_info_t {
	struct mtd_info *mtd;
	struct env_valid_node_t *env_valid_node;
	struct env_free_node_t *env_free_node;
	u_char env_valid;
	u_char env_init;
	u_char part_num_before_sys;
	struct aml_nand_bbt_info nand_bbt_info;
};

typedef struct environment_s {
        uint32_t        crc;            /* CRC32 over data bytes        */
        unsigned char   data[ENV_SIZE]; /* Environment data             */
} env_t;

#endif
