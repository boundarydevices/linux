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

/* #define CRYPTO_DEBUG */

 /* Reserved 4096 bytes and table is 12 bytes each */
#define MAX_NUM_TABLES 341

enum GXL_DMA_REG_OFFSETS {
	GXL_DMA_T0   = 0x00,
	GXL_DMA_T1   = 0x01,
	GXL_DMA_T2   = 0x02,
	GXL_DMA_T3   = 0x03,
	GXL_DMA_STS0 = 0x04,
	GXL_DMA_STS1 = 0x05,
	GXL_DMA_STS2 = 0x06,
	GXL_DMA_STS3 = 0x07,
	GXL_DMA_CFG  = 0x08,
};

enum TXLX_DMA_REG_OFFSETS {
	TXLX_DMA_T0   = 0x00,
	TXLX_DMA_T1   = 0x01,
	TXLX_DMA_T2   = 0x02,
	TXLX_DMA_T3   = 0x03,
	TXLX_DMA_T4   = 0x04,
	TXLX_DMA_T5   = 0x05,

	TXLX_DMA_STS0 = 0x08,
	TXLX_DMA_STS1 = 0x09,
	TXLX_DMA_STS2 = 0x0a,
	TXLX_DMA_STS3 = 0x0b,
	TXLX_DMA_STS4 = 0x0c,
	TXLX_DMA_STS5 = 0x0d,

	TXLX_DMA_CFG  = 0x10,
	TXLX_DMA_SEC  = 0x11,
};

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

#define DMA_FLAG_MAY_OCCUPY    BIT(0)
#define DMA_FLAG_TDES_IN_USE   BIT(1)
#define DMA_FLAG_AES_IN_USE    BIT(2)
#define DMA_FLAG_SHA_IN_USE    BIT(3)

struct aml_dma_dev {
	spinlock_t dma_lock;
	uint32_t thread;
	uint32_t status;
	int	irq;
	uint8_t dma_busy;
};

u32 swap_ulong32(u32 val);
void aml_write_crypto_reg(u32 addr, u32 data);
u32 aml_read_crypto_reg(u32 addr);
void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg,
		u32 thread, u32 status);

u32 get_dma_t0_offset(void);
u32 get_dma_sts0_offset(void);

extern void __iomem *cryptoreg;

extern int debug;
#ifndef CRYPTO_DEBUG
#define dbgp(level, fmt, arg...)
#else
#define dbgp(level, fmt, arg...)                 \
	do {                                            \
		if (likely(debug >= level))                         \
			pr_debug("%s: " fmt, __func__, ## arg);\
		else                                            \
			pr_info("%s: " fmt, __func__, ## arg); \
	} while (0)

#endif
#endif
