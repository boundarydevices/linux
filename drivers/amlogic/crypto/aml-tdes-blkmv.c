/*
 * drivers/amlogic/crypto/aml-tdes-blkmv.c
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
#include <crypto/des.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include "aml-crypto-blkmv.h"

/* TDES flags */
#define TDES_FLAGS_MODE_MASK	0x03
#define TDES_FLAGS_ENCRYPT	BIT(0)
#define TDES_FLAGS_CBC		BIT(1)

#define TDES_FLAGS_INIT		BIT(8)
#define TDES_FLAGS_DMA		BIT(9)
#define TDES_FLAGS_BUSY		BIT(10)

#define AML_TDES_QUEUE_LENGTH	50
#define TDES_KEY_STARTING_IDX 0

unsigned char    ndma_tdes_table_block[NDMA_TABLE_SIZE + 32];
#define NDMA_TDES_TABLE_START (((unsigned long)ndma_tdes_table_block + 0x1f)\
		& (~0x1f))
unsigned long    ndma_tdes_table_ptr;

struct aml_tdes_dev;

struct aml_tdes_ctx {
	struct aml_tdes_dev *dd;

	int		keylen;
	u32		key[3*DES_KEY_SIZE / sizeof(u32)];

	u16		block_size;
	u32     key_idx;
};

struct aml_tdes_reqctx {
	unsigned long mode;
};

struct aml_tdes_dev {
	struct list_head	list;

	struct aml_tdes_ctx	*ctx;
	struct device		*dev;
	struct clk *clk;
	int	irq;
	u32 idx;

	unsigned long		flags;
	int	err;

	spinlock_t		lock;
	struct crypto_queue	queue;

	struct tasklet_struct	done_task;
	struct tasklet_struct	queue_task;

	struct ablkcipher_request	*req;
	size_t	total;

	struct scatterlist	*in_sg;
	size_t			in_offset;
	struct scatterlist	*out_sg;
	size_t			out_offset;

	size_t	buflen;
	size_t	dma_size;

	void	*buf_in;
	dma_addr_t	dma_addr_in;

	void	*buf_out;
	dma_addr_t	dma_addr_out;
	dma_addr_t	dma_descript_tab;
};

struct aml_tdes_drv {
	struct list_head	dev_list;
	spinlock_t		lock;
};

static struct aml_tdes_drv aml_tdes = {
	.dev_list = LIST_HEAD_INIT(aml_tdes.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_tdes.lock),
};

static void write_key_iv(unsigned long addr, unsigned long data_hi,
		unsigned long data_lo)
{
	unsigned long temp;

	temp = swap_ulong32(data_lo);
	cbus_wr(NDMA_TDES_KEY_LO, temp);
	temp = swap_ulong32(data_hi);
	cbus_wr(NDMA_TDES_KEY_HI, temp);

	/* write the key */
	cbus_wr(NDMA_TDES_CONTROL, ((1 << 31) | (addr << 0)));
}

static void write_modes(unsigned long idx, unsigned long cbc_en,
		unsigned long decrypt, unsigned long des_modes)
{
	/* Write the key */
	cbus_wr(NDMA_TDES_CONTROL, ((1 << 30)
				| (des_modes << 6)
				| (cbc_en << 5)
				| (decrypt << 4)
				| idx * 4 << 0));
}


static void set_tdes_key(u32 *key, u32 keylen, u32 idx, u32 dir)
{
	idx = (idx * 4);
	if (dir) {
		if (keylen == DES_KEY_SIZE * 2) {
			write_key_iv(idx + 0, *(key), *(key + 1));
			write_key_iv(idx + 1, *(key + 1 * 2),
					*(key + 1 * 2 + 1));
			write_key_iv(idx + 2, *(key), *(key + 1));
		} else {
			write_key_iv(idx + 0, *(key + 2 * 2),
					*(key + 2 * 2 + 1));
			write_key_iv(idx + 1, *(key + 1 * 2),
					*(key + 1 * 2 + 1));
			write_key_iv(idx + 2, *(key), *(key + 1));
		}
	} else {
		if (keylen == DES_KEY_SIZE * 2) {
			write_key_iv(idx + 0, *(key), *(key + 1));
			write_key_iv(idx + 1, *(key + 1 * 2),
					*(key + 1 * 2 + 1));
			write_key_iv(idx + 2, *(key), *(key + 1));
		} else {
			write_key_iv(idx + 0, *(key), *(key+1));
			write_key_iv(idx + 1, *(key + 1 * 2),
					*(key + 1 * 2 + 1));
			write_key_iv(idx + 2, *(key + 2 * 2),
					*(key + 2 * 2 + 1));
		}
	}
}

static void set_tdes_iv(u32 *iv, u32 idx)
{
	idx = (idx * 4);
	if (!iv)
		write_key_iv(idx + 3, 0x00000000, 0x00000000);
	else
		write_key_iv(idx + 3, *(iv), *(iv + 1));
}

static size_t aml_tdes_sg_copy(struct scatterlist **sg, size_t *offset,
		void *buf, size_t buflen, size_t total, int out)
{
	size_t count = 0, off = 0;

	while (buflen && total) {
		count = min((*sg)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return off;

		scatterwalk_map_and_copy(buf + off, *sg, *offset, count, out);

		off += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*sg)->length) {
			*sg = sg_next(*sg);
			if (*sg)
				*offset = 0;
			else
				total = 0;
		}
	}

	return off;
}

static struct aml_tdes_dev *aml_tdes_find_dev(struct aml_tdes_ctx *ctx)
{
	struct aml_tdes_dev *tdes_dd = NULL;
	struct aml_tdes_dev *tmp;

	spin_lock_bh(&aml_tdes.lock);
	if (!ctx->dd) {
		list_for_each_entry(tmp, &aml_tdes.dev_list, list) {
			tdes_dd = tmp;
			break;
		}
		ctx->dd = tdes_dd;
	} else {
		tdes_dd = ctx->dd;
	}

	spin_unlock_bh(&aml_tdes.lock);

	return tdes_dd;
}

static int aml_tdes_hw_init(struct aml_tdes_dev *dd)
{
	cbus_wr(AM_RING_OSC_REG0, 0x1);
	cbus_wr(NDMA_CNTL_REG1, cbus_rd(NDMA_CNTL_REG1) & 0xfffffffe);
	/* enable module clk */
	if (dd->clk)
		clk_prepare_enable(dd->clk);

	if (!(dd->flags & TDES_FLAGS_INIT)) {
		dd->flags |= TDES_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

static void aml_tdes_finish_req(struct aml_tdes_dev *dd, int err)
{
	struct ablkcipher_request *req = dd->req;

	dd->flags &= ~TDES_FLAGS_BUSY;
	if (req->base.complete)
		req->base.complete(&req->base, err);
}


static int aml_tdes_crypt_dma(struct aml_tdes_dev *dd,
		dma_addr_t dma_addr_in, dma_addr_t dma_addr_out,
		int length, unsigned long restart)
{
	unsigned long mode;
#if AML_CRYPTO_DEBUG
	int i = 0;
#endif
	dd->dma_size = length;

	dma_sync_single_for_device(dd->dev, dma_addr_in, length,
			DMA_TO_DEVICE);

	dd->flags |= TDES_FLAGS_DMA;

	mode = MODE_ECB;

	if (dd->flags & TDES_FLAGS_CBC)
		mode = MODE_CBC;

	ndma_add_descriptor_1d(
			ndma_tdes_table_ptr,  /* table location */
			1,                    /* irq */
			restart,              /* restart */
			7,                    /* pre_endian */
			7,                    /* post_endian */
			dd->ctx->key_idx,/* keytype + cryptodir */
			dd->dma_size,         /* bytes_to_move */
			INLINE_TYPE_TDES,     /* inlinetype */
			dma_addr_in,          /* src_addr */
			dma_addr_out);        /* dest_addr */

	dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
			NDMA_TABLE_SIZE, DMA_TO_DEVICE);
	ndma_set_table_position(TDES_THREAD_INDEX,
			dd->dma_descript_tab, NDMA_TABLE_SIZE);
	ndma_set_table_count(TDES_THREAD_INDEX, 1);

#if AML_CRYPTO_DEBUG
	for (i = 0x2270; i < 0x229d; i++)
		pr_err("reg(%4x) = 0x%8x\n", i, cbus_rd(i));

	for (i = 0; i < 8; i++) {
		pr_err("table addr(%d) = 0x%8lx\n", i,
			*(unsigned long *)(ndma_tdes_table_ptr + i * 4));
	}
#endif

	ndma_start(TDES_THREAD_INDEX);

	return 0;
}

static int aml_tdes_crypt_dma_start(struct aml_tdes_dev *dd,
		unsigned long restart)
{
	int err = 0;
	size_t count;
	dma_addr_t addr_in, addr_out;

	/* use cache buffers */
	count = aml_tdes_sg_copy(&dd->in_sg, &dd->in_offset,
			dd->buf_in, dd->buflen, dd->total, 0);
	addr_in = dd->dma_addr_in;
	addr_out = dd->dma_addr_out;
	dd->total -= count;

	err = aml_tdes_crypt_dma(dd, addr_in, addr_out, count, restart);

	return err;
}

static int aml_tdes_write_ctrl(struct aml_tdes_dev *dd)
{
	int err = 0;
	unsigned int tdes_mode = 0;
	unsigned int crypt = 0;

	err = aml_tdes_hw_init(dd);
	if (err)
		return err;

	if (dd->flags & TDES_FLAGS_CBC)
		tdes_mode = 1;
	else
		tdes_mode = 0;

	if (dd->flags & TDES_FLAGS_ENCRYPT)
		crypt = 0;
	else
		crypt = 1;

	write_modes(dd->ctx->key_idx,
			tdes_mode,          /* cbc_en */
			crypt,          /* decrypt */
			(crypt == 1) ? 5 : 2); /* des_modes */

	set_tdes_key(dd->ctx->key, dd->ctx->keylen,
			dd->ctx->key_idx, crypt);
	if (dd->flags & TDES_FLAGS_CBC)
		set_tdes_iv(dd->req->info, dd->ctx->key_idx);
	else
		set_tdes_iv(NULL, dd->ctx->key_idx);

	return err;
}

static int aml_tdes_handle_queue(struct aml_tdes_dev *dd,
		struct ablkcipher_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	struct aml_tdes_ctx *ctx;
	struct aml_tdes_reqctx *rctx;
	unsigned long flags;
	int err, ret = 0;

	spin_lock_irqsave(&dd->lock, flags);
	if (req)
		ret = ablkcipher_enqueue_request(&dd->queue, req);

	if (dd->flags & TDES_FLAGS_BUSY) {
		spin_unlock_irqrestore(&dd->lock, flags);
		return ret;
	}
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req)
		dd->flags |= TDES_FLAGS_BUSY;
	spin_unlock_irqrestore(&dd->lock, flags);

	if (!async_req)
		return ret;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);

	/* assign new request to device */
	dd->req = req;
	dd->total = req->nbytes;
	dd->in_offset = 0;
	dd->in_sg = req->src;
	dd->out_offset = 0;
	dd->out_sg = req->dst;

	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	rctx->mode &= TDES_FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~TDES_FLAGS_MODE_MASK) | rctx->mode;
	dd->ctx = ctx;
	ctx->dd = dd;

	err = aml_tdes_write_ctrl(dd);
	if (!err)
		err = aml_tdes_crypt_dma_start(dd, 1);

	if (err) {
		/* tdes_task will not finish it, so do it here */
		aml_tdes_finish_req(dd, err);
		tasklet_schedule(&dd->queue_task);
	}

	return ret;
}

static int aml_tdes_crypt_dma_stop(struct aml_tdes_dev *dd)
{
	int err = -EBUSY;
	size_t count;

	if (dd->flags & TDES_FLAGS_DMA) {
		err = 0;

		dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
				NDMA_TABLE_SIZE, DMA_FROM_DEVICE);
		dma_sync_single_for_device(dd->dev, dd->dma_addr_out,
				dd->dma_size, DMA_FROM_DEVICE);

		/* copy data */
		count = aml_tdes_sg_copy(&dd->out_sg, &dd->out_offset,
				dd->buf_out, dd->buflen, dd->dma_size, 1);

		if (count != dd->dma_size) {
			err = -EBUSY;
			pr_err("not all data converted: %u\n", count);
		}
		dd->flags &= ~TDES_FLAGS_DMA;
	}

	return err;
}


static int aml_tdes_buff_init(struct aml_tdes_dev *dd)
{
	int err = -ENOMEM;

	dd->buf_in = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buf_out = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(DES_BLOCK_SIZE - 1);

	if (!dd->buf_in || !dd->buf_out) {
		dev_err(dd->dev, "unable to alloc pages.\n");
		goto err_alloc;
	}

	/* MAP here */
	dd->dma_addr_in = dma_map_single(dd->dev, dd->buf_in,
			dd->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_in)) {
		dev_err(dd->dev, "dma %d bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_in;
	}

	dd->dma_addr_out = dma_map_single(dd->dev, dd->buf_out,
			dd->buflen, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_out)) {
		dev_err(dd->dev, "dma %d bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_out;
	}

	dd->dma_descript_tab = dma_map_single(dd->dev,
			(void *)ndma_tdes_table_ptr,
			NDMA_TABLE_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->dev, dd->dma_descript_tab)) {
		dev_err(dd->dev, "dma descriptor error\n");
		err = -EINVAL;
		goto err_map_descriptor;
	}

	return 0;

err_map_descriptor:
	dma_unmap_single(dd->dev, dd->dma_descript_tab, NDMA_TABLE_SIZE,
			DMA_TO_DEVICE);

err_map_out:
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
err_map_in:
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
err_alloc:
	if (err)
		pr_err("error: %d\n", err);
	return err;
}

static void aml_tdes_buff_cleanup(struct aml_tdes_dev *dd)
{
	dma_unmap_single(dd->dev, dd->dma_addr_out, dd->buflen,
			DMA_FROM_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
}

static int aml_tdes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(
			crypto_ablkcipher_reqtfm(req));
	struct aml_tdes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct aml_tdes_dev *dd;

	if (!IS_ALIGNED(req->nbytes, DES_BLOCK_SIZE)) {
		pr_err("request size is not exact amount of TDES blocks\n");
		return -EINVAL;
	}
	ctx->block_size = DES_BLOCK_SIZE;

	dd = aml_tdes_find_dev(ctx);
	if (!dd)
		return -ENODEV;

	ctx->key_idx = dd->idx;
	dd->idx = (dd->idx + 1) % 4;
	rctx->mode = mode;

	return aml_tdes_handle_queue(dd, req);
}

static int aml_des_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
		unsigned int keylen)
{
	u32 tmp[DES_EXPKEY_WORDS];
	int err;
	struct crypto_tfm *ctfm = crypto_ablkcipher_tfm(tfm);
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != DES_KEY_SIZE) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	err = des_ekey(tmp, key);
	if (err == 0 && (ctfm->crt_flags & CRYPTO_TFM_REQ_WEAK_KEY)) {
		ctfm->crt_flags |= CRYPTO_TFM_RES_WEAK_KEY;
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int aml_tdes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
		unsigned int keylen)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if ((keylen != 2*DES_KEY_SIZE) && (keylen != 3*DES_KEY_SIZE)) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

static int aml_tdes_ecb_encrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_ENCRYPT);
}

static int aml_tdes_ecb_decrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			0);
}

static int aml_tdes_cbc_encrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_ENCRYPT | TDES_FLAGS_CBC);
}

static int aml_tdes_cbc_decrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_CBC);
}

static int aml_tdes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_tdes_reqctx);

	return 0;
}

static void aml_tdes_cra_exit(struct crypto_tfm *tfm)
{
}

static struct crypto_alg tdes_algs[] = {
	{
		.cra_name        = "ecb(des)",
		.cra_driver_name = "ecb-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_cra_init,
		.cra_exit      =  aml_tdes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        =  "cbc(des)",
		.cra_driver_name =  "cbc-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_cra_init,
		.cra_exit      =  aml_tdes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.ivsize		=    DES_BLOCK_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_cbc_encrypt,
			.decrypt	=    aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(des3_ede)",
		.cra_driver_name = "ecb-tdes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_cra_init,
		.cra_exit       = aml_tdes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey		=    aml_tdes_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede)",
		.cra_driver_name = "cbc-tdes-aml",
		.cra_priority  = 100,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_cra_init,
		.cra_exit      = aml_tdes_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	}
};

static void aml_tdes_queue_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *)data;

	aml_tdes_handle_queue(dd, NULL);
}

static void aml_tdes_done_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *) data;
	int err;
#if AML_CRYPTO_DEBUG
	int i = 0;
#endif

	err = aml_tdes_crypt_dma_stop(dd);

#if AML_CRYPTO_DEBUG
	for (i = 0x2270; i < 0x228D; i++)
		pr_err("reg(%4x) = 0x%8x\n", i, cbus_rd(i));

	for (i = 0; i < 8; i++) {
		pr_err("table addr(%d) = 0x%8lx\n", i,
			*(unsigned long *)(ndma_tdes_table_ptr + i * 4));
	}
#endif
	err = dd->err ? : err;

	if (dd->total && !err) {
		if (!err)
			err = aml_tdes_crypt_dma_start(dd, 0);
		if (!err)
			return; /* DMA started. Not fininishing. */
	}

	aml_tdes_finish_req(dd, err);
	aml_tdes_handle_queue(dd, NULL);
}

static irqreturn_t aml_tdes_irq(int irq, void *dev_id)
{
	struct aml_tdes_dev *tdes_dd = dev_id;

	if (!(cbus_rd(NDMA_TABLE_ADD_REG) & (0xF << (TDES_THREAD_INDEX * 8)))
	   && (cbus_rd(NDMA_THREAD_REG) & (1 << (TDES_THREAD_INDEX + 8)))) {
		if (TDES_FLAGS_BUSY & tdes_dd->flags) {
			ndma_stop(TDES_THREAD_INDEX);
			tasklet_schedule(&tdes_dd->done_task);
			return IRQ_HANDLED;
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_NONE;
}

static void aml_tdes_unregister_algs(struct aml_tdes_dev *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tdes_algs); i++)
		crypto_unregister_alg(&tdes_algs[i]);
}

static int aml_tdes_register_algs(struct aml_tdes_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(tdes_algs); i++) {
		err = crypto_register_alg(&tdes_algs[i]);
		if (err)
			goto err_tdes_algs;
	}

	return 0;

err_tdes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&tdes_algs[j]);

	return err;
}

static int aml_tdes_probe(struct platform_device *pdev)
{
	struct aml_tdes_dev *tdes_dd;
	struct device *dev = &pdev->dev;
	struct resource *res_irq = 0;
	int err;

	ndma_tdes_table_ptr = NDMA_TDES_TABLE_START;

	tdes_dd = kzalloc(sizeof(struct aml_tdes_dev), GFP_KERNEL);
	if (tdes_dd == NULL) {
		err = -ENOMEM;
		goto tdes_dd_err;
	}

	tdes_dd->dev = dev;

	platform_set_drvdata(pdev, tdes_dd);
	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	tdes_dd->clk = devm_clk_get(&pdev->dev, "blkmv");

	INIT_LIST_HEAD(&tdes_dd->list);

	tasklet_init(&tdes_dd->done_task, aml_tdes_done_task,
			(unsigned long)tdes_dd);
	tasklet_init(&tdes_dd->queue_task, aml_tdes_queue_task,
			(unsigned long)tdes_dd);

	crypto_init_queue(&tdes_dd->queue, AML_TDES_QUEUE_LENGTH);

	tdes_dd->irq = res_irq->start;

	err = request_irq(tdes_dd->irq, aml_tdes_irq, IRQF_SHARED, "aml-tdes",
			tdes_dd);
	if (err) {
		dev_err(dev, "unable to request tdes irq.\n");
		goto tdes_irq_err;
	}

	tdes_dd->idx = 0;
	err = aml_tdes_hw_init(tdes_dd);
	if (err)
		goto err_tdes_buff;

	err = aml_tdes_buff_init(tdes_dd);
	if (err)
		goto err_tdes_buff;

	spin_lock(&aml_tdes.lock);
	list_add_tail(&tdes_dd->list, &aml_tdes.dev_list);
	spin_unlock(&aml_tdes.lock);

	err = aml_tdes_register_algs(tdes_dd);
	if (err)
		goto err_algs;

	dev_info(dev, "Aml TDES\n");

	return 0;

err_algs:
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);
	aml_tdes_buff_cleanup(tdes_dd);
err_tdes_buff:
	free_irq(tdes_dd->irq, tdes_dd);
tdes_irq_err:
	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);
	if (tdes_dd->clk) {
		clk_disable_unprepare(tdes_dd->clk);
		devm_clk_put(&pdev->dev, tdes_dd->clk);
	}
	kfree(tdes_dd);
	tdes_dd = NULL;
tdes_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_tdes_remove(struct platform_device *pdev)
{
	static struct aml_tdes_dev *tdes_dd;

	tdes_dd = platform_get_drvdata(pdev);
	if (!tdes_dd)
		return -ENODEV;
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);

	aml_tdes_unregister_algs(tdes_dd);

	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);

	if (tdes_dd->irq > 0)
		free_irq(tdes_dd->irq, tdes_dd);

	if (tdes_dd->clk) {
		clk_disable_unprepare(tdes_dd->clk);
		devm_clk_put(&pdev->dev, tdes_dd->clk);
	}
	kfree(tdes_dd);
	tdes_dd = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_tdes_dt_match[] = {
	{	.compatible = "amlogic,des,tdes",
	},
	{},
};
#else
#define aml_tdes_dt_match NULL
#endif

static struct platform_driver aml_tdes_driver = {
	.probe		= aml_tdes_probe,
	.remove		= aml_tdes_remove,
	.driver		= {
		.name	= "aml_tdes_blkmv",
		.owner	= THIS_MODULE,
		.of_match_table = aml_tdes_dt_match,
	},
};

module_platform_driver(aml_tdes_driver);

MODULE_DESCRIPTION("Aml TDES hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
