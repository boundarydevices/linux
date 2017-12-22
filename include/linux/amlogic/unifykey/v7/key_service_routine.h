/*
 * include/linux/amlogic/unifykey/v7/key_service_routine.h
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

#ifndef __KEY_SERVICE_ROUTINE_H__
#define __KEY_SERVICE_ROUTINE_H__

#include <linux/types.h>

/*HDCP*/
/* Old definition */
#define HDCP_ENABLE		0x82000010
/* same as HDCP14_ENABLE/RESULT */
#define HDCP_RESULT		0x82000011
#define HDCP14_ENABLE		HDCP_ENABLE
#define HDCP14_RESULT		HDCP_RESULT
#define HDCP14_INIT		0x82000012
#define HDCP14_EN_ENCRYPT	0x82000013
#define HDCP14_OFF		0x82000014
#define HDCP_MUX_22		0x82000015
#define HDCP_MUX_14		0x82000016
#define HDCP22_RESULT		0x82000017
#define HDCP22_ESM_READ		0x82000018
#define HDCP22_ESM_WRITE	0x82000019
#define HDCP14_KEY_LSTORE	0x8200001a
#define HDCP22_KEY_LSTORE	0x8200001b
#define HDCP22_KEY_SET_DUK	0x8200001c
#define HDMIRX_RD_SEC_TOP	0x8200001d
#define HDMIRX_WR_SEC_TOP	0x8200001e
#define HDCP22_RX_ESM_READ	0x8200001f
#define HDCP22_RX_ESM_WRITE	0x8200002f
#define HDCP22_RX_SET_DUK_KEY	0x8200002e

/* MISC */
#define GET_SHARE_MEM_INPUT_BASE		0x82000020
#define GET_SHARE_MEM_OUTPUT_BASE		0x82000021
#define GET_REBOOT_REASON				0x82000022
#define GET_SHARE_STORAGE_IN_BASE		0x82000023
#define GET_SHARE_STORAGE_OUT_BASE		0x82000024
#define GET_SHARE_STORAGE_BLOCK_BASE	0x82000025
#define GET_SHARE_STORAGE_MESSAGE_BASE	0x82000026
#define GET_SHARE_STORAGE_BLOCK_SIZE		0x82000027
/* store which flash (nand or emmc) is storage device*/
#define SET_STORAGE_INFO				0x82000028
#define FREE_SHARE_STORAGE              0x82000029

/* EFUSE */
#define EFUSE_READ_USER		0x82000030
#define EFUSE_WRITE_USER		0x82000031
#define EFUSE_WRITE_PATTERN		0x82000032
#define EFUSE_MAX_USER			0x82000033
#define EFUSE_READ_TEE		0x82000034
#define EFUSE_WRITE_TEE		0x82000035

/*Audio Licence query*/
#define EFUSE_QUERY_LICENCE		(0x82000050)
	#define EFUSE_Q_L_DOLBY     (0x1)
	#define EFUSE_Q_L_DTS       (0x2)
	#define EFUSE_Q_L_MACV      (0x3)
	#define EFUSE_Q_L_DIVX      (0x4)
#define EFUSE_QUERY_LICENCE_VER (0x82000051)
	#define EFUSE_Q_L_VER       (0x60)

#define EFUSE_QUERY_MAX_CNT     (0x70)
#define EFUSE_Q_R_ENABLED       (0)  /* the query feature is enabled */
#define EFUSE_Q_R_DISABLED      (1)  /* the query feature is disabled */
#define EFUSE_Q_R_FAILED        (2)  /* the feature is forbidden to query */

/* Security Key*/
#define SECURITY_KEY_QUERY	0x82000060
#define SECURITY_KEY_READ	0x82000061
#define SECURITY_KEY_WRITE	0x82000062
#define SECURITY_KEY_TELL		0x82000063
#define SECURITY_KEY_VERIFY	0x82000064
#define SECURITY_KEY_STATUS	0x82000065
#define SECURITY_KEY_NOTIFY	0x82000066
#define SECURITY_KEY_LIST		0x82000067
#define SECURITY_KEY_REMOVE	0x82000068
#define SECURITY_KEY_NOTIFY_EX	0x82000069
#define SECURITY_KEY_SET_ENCTYPE	0x8200006A
#define SECURITY_KEY_GET_ENCTYPE	0x8200006B
#define SECURITY_KEY_VERSION		0x8200006C
#define SECURITY_KEY_STORAGE_TYPE	0x8200006D

uint64_t storage_service_routine(uint32_t fid,
			     uint64_t x1, void *in, void **out);

#endif
