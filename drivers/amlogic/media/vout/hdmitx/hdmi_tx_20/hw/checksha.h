/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/checksha.h
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

#ifndef __CHECKSHA_H__
#define __CHECKSHA_H__

#define FALSE 0
#define TRUE 1

#define HDCP_NULL 0
#define HDCP_KSVLIST_VALID 1
#define HDCP_KSVLIST_INVALID 2

#define KSV_SIZE 5
#define HDCP_HEAD 10
#define SHA_MAX_SIZE 20
#define KSV_MASK 0x7F
#define MAX_VAL_MASK 0xFFFFFFFF

#define SHA_CONST_VAL0 0x67452301
#define SHA_CONST_VAL1 0xEFCDAB89
#define SHA_CONST_VAL2 0x98BADCFE
#define SHA_CONST_VAL3 0x10325476
#define SHA_CONST_VAL4 0xC3D2E1F0

#define SHA_CONST_K0 0x5A827999
#define SHA_CONST_K1 0x6ED9EBA1
#define SHA_CONST_K2 0x8F1BBCDC
#define SHA_CONST_K3 0xCA62C1D6

struct shamsg_t {
	unsigned char shamsg_len[8];
	unsigned char shamsg_blk[64];
	int shamsg_idx;
	int shamsg_cmp;
	int shamsg_crp;
	unsigned int shamsg_dgt[5];
};

#define SHAMSG_SHIFT(bit, val) \
	((((val) << (bit)) & 0xFFFFFFFF) | ((val) >> (32-(bit))))

int calc_hdcp_ksv_valid(const unsigned char *dat, int size);

#endif /* __CHECKSHA_H__ */
