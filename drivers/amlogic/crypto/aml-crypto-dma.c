/*
 * drivers/amlogic/crypto/aml-crypto-dma.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/internal/hash.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include "aml-crypto-dma.h"
#if 1
void __iomem *cryptoreg_offset;
u32 swap_ulong32(u32 val)
{
	u32 res = 0;

	res = ((val & 0xff) << 24) + (((val >> 8) & 0xff) << 16) +
		(((val >> 16) & 0xff) << 8) + ((val >> 24) & 0xff);
	return res;
}
void aml_write_crypto_reg(u32 addr, u32 data)
{
	writel(data, cryptoreg_offset + (addr << 2));
}

u32 aml_read_crypto_reg(u32 addr)
{
	return readl(cryptoreg_offset + (addr << 2));
}
#endif
void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg)
{
#if AML_CRYPTO_DEBUG
	uint32_t i = 0;

	pr_err("begin %s\n", msg);
	for (i = 0; i < 2; i++)
		pr_err("reg(%lu) = 0x%8x\n", (uintptr_t)(DMA_T0 + i),
				aml_read_crypto_reg(DMA_T0 + i));
	for (i = 0; i < 2; i++)
		pr_err("reg(%lu) = 0x%8x\n", (uintptr_t)(DMA_STS0 + i),
				aml_read_crypto_reg(DMA_STS0 + i));
	pr_err("reg(%lu) = 0x%8x\n", (uintptr_t)(DMA_CFG),
			aml_read_crypto_reg(DMA_CFG));
	for (i = 0; i < nents; i++) {
		pr_err("desc (%4x) (len) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.length);
		pr_err("desc (%4x) (irq) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.irq);
		pr_err("desc (%4x) (eoc) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.eoc);
		pr_err("desc (%4x) (lop) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.loop);
		pr_err("desc (%4x) (mod) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.mode);
		pr_err("desc (%4x) (beg) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.begin);
		pr_err("desc (%4x) (end) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.end);
		pr_err("desc (%4x) (opm) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.op_mode);
		pr_err("desc (%4x) (enc) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.enc_sha_only);
		pr_err("desc (%4x) (blk) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.block);
		pr_err("desc (%4x) (err) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.error);
		pr_err("desc (%4x) (own) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.owner);
		pr_err("desc (%4x) (src) = 0x%8x\n", i,
				dsc[i].src_addr);
		pr_err("desc (%4x) (tgt) = 0x%8x\n", i,
				dsc[i].tgt_addr);
	}
	pr_err("end %s\n", msg);
#endif
}
