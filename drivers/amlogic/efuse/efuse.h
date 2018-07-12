/*
 * drivers/amlogic/efuse/efuse.h
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

#ifndef __EFUSE_H
#define __EFUSE_H

/* #define EFUSE_DEBUG */
/*#define EFUSE_READ_ONLY			1*/

/* #define EFUSE_NONE_ID			0 */
#define EFUSE_VERSION_ID		1
#define EFUSE_LICENCE_ID		2

#define EFUSE_MAC_ID				3
#define EFUSE_MAC_WIFI_ID	4
#define EFUSE_MAC_BT_ID		5
/* #define EFUSE_HDCP_ID			6 */
#define EFUSE_USID_ID				7

/* #define EFUSE_RSA_KEY_ID		8 */
/* #define EFUSE_CUSTOMER_ID		9 */
/* #define EFUSE_MACHINEID_ID		10 */
#define EFUSE_NANDEXTCMD_ID		11

#define EFUSE_DWORDS            128  /* (EFUSE_BITS/32) */

#define EFUSE_BYTES            512  /* (EFUSE_BITS/8) */

#define EFUSE_INFO_GET			_IO('f', 0x40)

#define EFUSE_HAL_API_READ	0
#define EFUSE_HAL_API_WRITE 1
#define EFUSE_HAL_API_USER_MAX 3

#define AML_DATA_PROCESS            (0x820000FF)
#define AML_D_P_W_EFUSE_AMLOGIC     (0x20)
#define GXB_EFUSE_PATTERN_SIZE      (0x400)

#define ASSIST_HW_REV 0x1f53

extern int efuseinfo_num;

extern void __iomem *sharemem_input_base;
extern void __iomem *sharemem_output_base;
extern unsigned int efuse_read_cmd;
extern unsigned int efuse_write_cmd;
extern unsigned int efuse_get_max_cmd;

struct efuseinfo_item_t {
	char title[40];
	unsigned int id;
	loff_t offset;    /* write offset */
	unsigned int data_len;
};

struct efuseinfo_t {
	struct efuseinfo_item_t *efuseinfo_version;
	int size;
	int version;
};

struct efuse_platform_data {
	loff_t pos;
	size_t count;
	bool (*data_verify)(const char *usid);
};

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;		/* R/W */
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};

extern struct efuseinfo_t efuseinfo[];
#ifndef CONFIG_ARM64
int efuse_getinfo_byTitle(unsigned char *name, struct efuseinfo_item_t *info);
int check_if_efused(loff_t pos, size_t count);
int efuse_read_item(char *buf, size_t count, loff_t *ppos);
int efuse_write_item(char *buf, size_t count, loff_t *ppos);
extern int efuse_active_version;
extern struct clk *efuse_clk;
#else

ssize_t efuse_get_max(void);
ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos);
ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos);
unsigned long efuse_amlogic_set(char *buf, size_t count);

#endif

#endif
