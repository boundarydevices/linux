/*
 * drivers/amlogic/crypto/aml-crypto-dma.h
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

#ifndef _AML_CRYPTO_H_
#define _AML_CRYPTO_H_
#include <linux/io.h>

#define AML_CRYPTO_DEBUG    0

 /* Reserved 4096 bytes and table is 12 bytes each */
#define MAX_NUM_TABLES 341

#define DMA_T0   0x00
#define DMA_T1   0x01
#define DMA_T2   0x02
#define DMA_T3   0x03
#define DMA_STS0 0x04
#define DMA_STS1 0x05
#define DMA_STS2 0x06
#define DMA_STS3 0x07
#define DMA_CFG  0x08

#define aml_write_reg(addr, data) \
	writel(data, (int *)addr)

#define aml_read_reg(addr) \
	readl(addr)

/* driver logic flags */
#define TDES_KEY_LENGTH 32
#define TDES_MIN_BLOCK_SIZE 8

#define OP_MODE_ECB 0
#define OP_MODE_CBC 1
#define OP_MODE_CTR 2

#define OP_MODE_SHA    0
#define OP_MODE_HMAC_I 1
#define OP_MODE_HMAC_O 2

#define MODE_DMA     0x0
#define MODE_KEY     0x1
#define MODE_MEMSET  0x2
/* 0x3 is skipped */
/* 0x4 is skipped */
#define MODE_SHA1    0x5
#define MODE_SHA256  0x6
#define MODE_SHA224  0x7
#define MODE_AES128  0x8
#define MODE_AES192  0x9
#define MODE_AES256  0xa
/* 0xb is skipped */
#define MODE_DES     0xc
/* 0xd is skipped */
#define MODE_TDES_2K 0xe
#define MODE_TDES_3K 0xf

/* Thread 2, 3 are for secure threads */
#define AES_THREAD_INDEX 0
#define TDES_THREAD_INDEX 0
#define SHA_THREAD_INDEX 0
#define HMAC_THREAD_INDEX 0

struct dma_dsc {
	union {
		uint32_t d32;
		struct {
			unsigned length:17;
			unsigned irq:1;
			unsigned eoc:1;
			unsigned loop:1;
			unsigned mode:4;
			unsigned begin:1;
			unsigned end:1;
			unsigned op_mode:2;
			unsigned enc_sha_only:1;
			unsigned block:1;
			unsigned error:1;
			unsigned owner:1;
		} b;
	} dsc_cfg;
	uint32_t src_addr;
	uint32_t tgt_addr;
};

extern void __iomem *cryptoreg_offset;
extern u32 secure_cryptoreg_offset;

u32 swap_ulong32(u32 val);
void aml_write_crypto_reg(u32 addr, u32 data);
u32 aml_read_crypto_reg(u32 addr);
void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg);
#endif
