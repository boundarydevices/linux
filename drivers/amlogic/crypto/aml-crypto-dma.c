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

u32 swap_ulong32(u32 val)
{
	u32 res = 0;

	res = ((val & 0xff) << 24) + (((val >> 8) & 0xff) << 16) +
		(((val >> 16) & 0xff) << 8) + ((val >> 24) & 0xff);
	return res;
}
EXPORT_SYMBOL_GPL(swap_ulong32);

void aml_write_crypto_reg(u32 addr, u32 data)
{
	if (cryptoreg)
		writel(data, cryptoreg + (addr << 2));
	else
		pr_err("crypto reg mapping is not initailized\n");
}
EXPORT_SYMBOL_GPL(aml_write_crypto_reg);

u32 aml_read_crypto_reg(u32 addr)
{
	if (!cryptoreg) {
		pr_err("crypto reg mapping is not initailized\n");
		return 0;
	}
	return readl(cryptoreg + (addr << 2));
}
EXPORT_SYMBOL_GPL(aml_read_crypto_reg);

void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg,
		u32 thread, u32 status)
{
	u32 i = 0;

	dbgp(0, "begin %s\n", msg);
	dbgp(0, "reg(%u) = 0x%8x\n", thread,
			aml_read_crypto_reg(thread));
	dbgp(0, "reg(%u) = 0x%8x\n", status,
			aml_read_crypto_reg(status));
	for (i = 0; i < nents; i++) {
		dbgp(0, "desc (%4x) (len) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.length);
		dbgp(0, "desc (%4x) (irq) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.irq);
		dbgp(0, "desc (%4x) (eoc) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.eoc);
		dbgp(0, "desc (%4x) (lop) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.loop);
		dbgp(0, "desc (%4x) (mod) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.mode);
		dbgp(0, "desc (%4x) (beg) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.begin);
		dbgp(0, "desc (%4x) (end) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.end);
		dbgp(0, "desc (%4x) (opm) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.op_mode);
		dbgp(0, "desc (%4x) (enc) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.enc_sha_only);
		dbgp(0, "desc (%4x) (blk) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.block);
		dbgp(0, "desc (%4x) (err) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.error);
		dbgp(0, "desc (%4x) (own) = 0x%8x\n", i,
				dsc[i].dsc_cfg.b.owner);
		dbgp(0, "desc (%4x) (src) = 0x%8x\n", i,
				dsc[i].src_addr);
		dbgp(0, "desc (%4x) (tgt) = 0x%8x\n", i,
				dsc[i].tgt_addr);
	}
	dbgp(0, "end %s\n", msg);
}
EXPORT_SYMBOL_GPL(aml_dma_debug);
